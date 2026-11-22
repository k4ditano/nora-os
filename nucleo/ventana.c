/* ventana: ver ventana.h. Cabeceras del HAL de Android 4.4 escritas a mano. */
#include "ventana.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC _IOW('F', 0x20, uint32_t)
#endif

/* ---- HAL ---- */
typedef struct hw_module_t hw_module_t;
typedef struct hw_device_t hw_device_t;
typedef struct { int (*open)(const hw_module_t *m, const char *id, hw_device_t **dev); } hw_module_methods_t;
struct hw_module_t {
    uint32_t tag;
    uint16_t module_api_version, hal_api_version;
    const char *id, *name, *author;
    hw_module_methods_t *methods;
    void *dso;
    uint32_t reserved[32 - 7];
};
struct hw_device_t {
    uint32_t tag, version;
    hw_module_t *module;
    uint32_t reserved[12];
    int (*close)(hw_device_t *d);
};
typedef const void *buffer_handle_t;
typedef struct {
    hw_module_t common;
    int (*registerBuffer)(const void *m, buffer_handle_t h);
    int (*unregisterBuffer)(const void *m, buffer_handle_t h);
    int (*lock)(const void *m, buffer_handle_t h, int usage, int l, int t, int w, int hh, void **vaddr);
    int (*unlock)(const void *m, buffer_handle_t h);
    int (*perform)(const void *m, int op, ...);
    void *lock_ycbcr;
    void *reserved_proc[6];
} gralloc_module_t;
typedef struct alloc_device_t {
    hw_device_t common;
    int (*alloc)(struct alloc_device_t *d, int w, int h, int fmt, int usage, buffer_handle_t *hnd, int *stride);
    int (*free)(struct alloc_device_t *d, buffer_handle_t h);
    void (*dump)(struct alloc_device_t *d, char *buf, int len);
    void *reserved_proc[7];
} alloc_device_t;
int hw_get_module(const char *id, const hw_module_t **m);   /* nuestra libhardware */

#define USO_SW_READ_OFTEN 0x3
#define USO_HW_TEXTURE 0x100
#define USO_HW_RENDER 0x200
#define HAL_RGB_565 4

typedef struct {
    int magic, version;
    void *reserved[4];
    void (*incRef)(void *);
    void (*decRef)(void *);
} native_base_t;
typedef struct ANativeWindowBuffer {
    native_base_t common;
    int width, height, stride, format, usage;
    void *reserved[2];
    buffer_handle_t handle;
    void *reserved_proc[8];
} ANativeWindowBuffer;
typedef struct ANativeWindow ANativeWindow;
struct ANativeWindow {
    native_base_t common;
    uint32_t flags;
    int minSwapInterval, maxSwapInterval;
    float xdpi, ydpi;
    intptr_t oem[4];
    int (*setSwapInterval)(ANativeWindow *, int);
    int (*dequeueBuffer_DEPRECATED)(ANativeWindow *, ANativeWindowBuffer **);
    int (*lockBuffer_DEPRECATED)(ANativeWindow *, ANativeWindowBuffer *);
    int (*queueBuffer_DEPRECATED)(ANativeWindow *, ANativeWindowBuffer *);
    int (*query)(const ANativeWindow *, int, int *);
    int (*perform)(ANativeWindow *, int, ...);
    int (*cancelBuffer_DEPRECATED)(ANativeWindow *, ANativeWindowBuffer *);
    int (*dequeueBuffer)(ANativeWindow *, ANativeWindowBuffer **, int *);
    int (*queueBuffer)(ANativeWindow *, ANativeWindowBuffer *, int);
    int (*cancelBuffer)(ANativeWindow *, ANativeWindowBuffer *, int);
};
#define MAGIA(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

#define NBUF 3
static struct {
    ANativeWindow win;
    ANativeWindowBuffer buf[NBUF];
    int libre[NBUF], siguiente;
    gralloc_module_t *gr;
    alloc_device_t *ad;
    int W, H, fb, pagina, paginas, vsync;
    int irq_fd, irq_de;          /* la interrupcion del controlador de pantalla (asoc_de): una por refresco */
    int pendiente;               /* pagina pedida y aun no puesta (-1: ninguna) */
    long irq_al_pedir;
    unsigned char *pant;
    size_t tam;
    struct fb_var_screeninfo vi;
    struct fb_fix_screeninfo fi;
    double tcopia;
    long copias;
} V;

static double ahora(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
/* /proc/interrupts: veces que ha saltado la interrupcion de la pantalla (-1 si no se sabe) */
static long cuenta_refrescos(void)
{
    static char b[8192];
    if (V.irq_fd < 0) return -1;
    int n = pread(V.irq_fd, b, sizeof b - 1, 0);
    if (n <= 0) return -1;
    b[n] = 0;
    char clave[16];
    int k = snprintf(clave, sizeof clave, "\n%3d:", V.irq_de);
    char *p = strstr(b, clave);
    if (!p) return -1;
    p += k;
    long total = 0;
    for (int c = 0; c < 8; c++) {           /* una columna por CPU */
        char *fin;
        long v = strtol(p, &fin, 10);
        if (fin == p) break;
        total += v;
        p = fin;
    }
    return total;
}
/* busca la interrupcion "asoc_de" (el DE de Actions avisa en cada refresco) */
static void buscar_refresco(void)
{
    V.irq_fd = open("/proc/interrupts", O_RDONLY);
    if (V.irq_fd < 0) return;
    static char b[8192];
    int n = pread(V.irq_fd, b, sizeof b - 1, 0);
    b[n > 0 ? n : 0] = 0;
    char *p = strstr(b, "asoc_de");
    if (!p) { close(V.irq_fd); V.irq_fd = -1; return; }
    while (p > b && p[-1] != '\n') p--;
    V.irq_de = atoi(p);
    /* ¿late? (si no, no sirve de reloj) */
    long a = cuenta_refrescos();
    usleep(40000);
    if (cuenta_refrescos() <= a) { close(V.irq_fd); V.irq_fd = -1; }
}
/* espera al siguiente refresco (como mucho 50 ms) */
static void espera_refresco(long desde)
{
    for (int i = 0; i < 100 && cuenta_refrescos() == desde; i++) usleep(500);
}

static void nada(void *p) { (void)p; }
static int w_intervalo(ANativeWindow *w, int i) { (void)w; (void)i; return 0; }
static int w_query(const ANativeWindow *w, int que, int *v)
{
    (void)w;
    switch (que) {
    case 0: case 6: *v = V.W; break;               /* WIDTH, DEFAULT_WIDTH */
    case 1: case 7: *v = V.H; break;               /* HEIGHT, DEFAULT_HEIGHT */
    case 2: *v = HAL_RGB_565; break;               /* FORMAT */
    case 3: *v = 1; break;                         /* MIN_UNDEQUEUED_BUFFERS */
    case 10: *v = USO_HW_RENDER | USO_HW_TEXTURE; break;
    default: *v = 0;
    }
    return 0;
}
static int w_perform(ANativeWindow *w, int op, ...) { (void)w; (void)op; return 0; }
static int indice(ANativeWindowBuffer *b) { return (int)(b - V.buf); }

static int w_dequeue(ANativeWindow *w, ANativeWindowBuffer **b, int *fence)
{
    (void)w;
    for (int k = 0; k < NBUF; k++) {
        int i = (V.siguiente + k) % NBUF;
        if (V.libre[i]) {
            V.libre[i] = 0;
            V.siguiente = (i + 1) % NBUF;
            *b = &V.buf[i];
            if (fence) *fence = -1;
            return 0;
        }
    }
    return -16;
}
static int w_dequeue_viejo(ANativeWindow *w, ANativeWindowBuffer **b) { return w_dequeue(w, b, 0); }
static int w_lock_viejo(ANativeWindow *w, ANativeWindowBuffer *b) { (void)w; (void)b; return 0; }

static int w_queue(ANativeWindow *w, ANativeWindowBuffer *b, int fence)
{
    (void)w;
    double t0 = ahora();
    void *src = 0;
    if (fence >= 0) close(fence);       /* lock() ya espera a la GPU */
    if (V.gr->lock(V.gr, b->handle, USO_SW_READ_OFTEN, 0, 0, V.W, V.H, &src) == 0 && src) {
        /* La pagina pedida no se pone hasta el siguiente refresco: hasta entonces se sigue
         * viendo la anterior. No se escribe nunca en la que se ve ni en la pedida. */
        if (V.irq_fd >= 0 && V.pendiente >= 0) {
            if (cuenta_refrescos() == V.irq_al_pedir) espera_refresco(V.irq_al_pedir);
            V.pagina = V.pendiente;
            V.pendiente = -1;
        }
        int pag = (V.pagina + 1) % V.paginas;           /* una pagina que NO se ve */
        unsigned char *dst = V.pant + (size_t)pag * V.H * V.fi.line_length;
        if (b->stride * 2 == (int)V.fi.line_length)
            memcpy(dst, src, (size_t)V.fi.line_length * V.H);
        else
            for (int y = 0; y < V.H; y++)
                memcpy(dst + (size_t)y * V.fi.line_length, (unsigned char *)src + (size_t)y * b->stride * 2, V.W * 2);
        V.gr->unlock(V.gr, b->handle);
        V.tcopia += ahora() - t0;
        V.copias++;
        if (V.vsync && V.irq_fd < 0) {
            uint32_t pantalla = 0;
            ioctl(V.fb, FBIO_WAITFORVSYNC, &pantalla);
        }
        V.vi.yoffset = pag * V.H;
        ioctl(V.fb, FBIOPAN_DISPLAY, &V.vi);
        if (V.irq_fd >= 0) {
            V.pendiente = pag;
            V.irq_al_pedir = cuenta_refrescos();
        } else
            V.pagina = pag;
    }
    V.libre[indice(b)] = 1;
    return 0;
}
static int w_queue_viejo(ANativeWindow *w, ANativeWindowBuffer *b) { return w_queue(w, b, -1); }
static int w_cancel(ANativeWindow *w, ANativeWindowBuffer *b, int fence)
{
    (void)w;
    if (fence >= 0) close(fence);
    V.libre[indice(b)] = 1;
    return 0;
}
static int w_cancel_viejo(ANativeWindow *w, ANativeWindowBuffer *b) { return w_cancel(w, b, -1); }

int ventana_abrir(void)
{
    V.fb = open("/dev/fb0", O_RDWR);
    if (V.fb < 0 || ioctl(V.fb, FBIOGET_VSCREENINFO, &V.vi) || ioctl(V.fb, FBIOGET_FSCREENINFO, &V.fi)) {
        fprintf(stderr, "nora: sin /dev/fb0\n");
        return -1;
    }
    /* Al encender, la pantalla esta en 32 bits (es el menu de GarlicOS el que la
     * pasa a 16). Nora la pone en RGB565 con tres paginas: copiar cuesta la mitad. */
    if (V.vi.bits_per_pixel != 16 || V.vi.yres_virtual < 3 * V.vi.yres) {
        fprintf(stderr, "nora: pantalla en %d bits y %d paginas: la paso a RGB565 con 3\n",
                V.vi.bits_per_pixel, V.vi.yres_virtual / V.vi.yres);
        V.vi.bits_per_pixel = 16;
        V.vi.red.offset = 11;  V.vi.red.length = 5;
        V.vi.green.offset = 5; V.vi.green.length = 6;
        V.vi.blue.offset = 0;  V.vi.blue.length = 5;
        V.vi.transp.offset = 0; V.vi.transp.length = 0;
        V.vi.xres_virtual = V.vi.xres;
        V.vi.yres_virtual = 3 * V.vi.yres;
        V.vi.xoffset = V.vi.yoffset = 0;
        V.vi.activate = FB_ACTIVATE_NOW;
        if (ioctl(V.fb, FBIOPUT_VSCREENINFO, &V.vi) || ioctl(V.fb, FBIOGET_VSCREENINFO, &V.vi) ||
            ioctl(V.fb, FBIOGET_FSCREENINFO, &V.fi)) {
            fprintf(stderr, "nora: no puedo cambiar el modo de la pantalla\n");
            return -1;
        }
    }
    if (V.vi.bits_per_pixel != 16) {
        fprintf(stderr, "nora: la pantalla sigue en %d bits\n", V.vi.bits_per_pixel);
        return -1;
    }
    V.W = V.vi.xres;
    V.H = V.vi.yres;
    V.paginas = V.vi.yres_virtual / V.vi.yres;
    if (V.paginas < 2) {
        fprintf(stderr, "nora: la pantalla solo tiene %d pagina\n", V.paginas);
        return -1;
    }
    V.tam = (size_t)V.fi.line_length * V.vi.yres_virtual;
    V.pant = mmap(0, V.tam, PROT_READ | PROT_WRITE, MAP_SHARED, V.fb, 0);
    if (V.pant == MAP_FAILED) return -1;
    V.pagina = V.vi.yoffset / V.H;
    uint32_t p = 0;
    V.vsync = ioctl(V.fb, FBIO_WAITFORVSYNC, &p) == 0;
    V.pendiente = -1;
    buscar_refresco();
    if (V.irq_fd >= 0) {
        V.vsync = 1;
        fprintf(stderr, "nora: refresco por la interrupcion %d (asoc_de)\n", V.irq_de);
    }

    if (hw_get_module("gralloc", (const hw_module_t **)&V.gr)) { fprintf(stderr, "nora: sin gralloc\n"); return -1; }
    if (V.gr->common.methods->open(&V.gr->common, "gpu0", (hw_device_t **)&V.ad)) {
        fprintf(stderr, "nora: gralloc sin gpu0\n");
        return -1;
    }
    for (int i = 0; i < NBUF; i++) {
        ANativeWindowBuffer *b = &V.buf[i];
        memset(b, 0, sizeof *b);
        b->common.magic = MAGIA('_', 'b', 'f', 'r');
        b->common.version = sizeof *b;
        b->common.incRef = b->common.decRef = nada;
        b->width = V.W;
        b->height = V.H;
        b->format = HAL_RGB_565;
        b->usage = USO_HW_RENDER | USO_HW_TEXTURE | USO_SW_READ_OFTEN;
        if (V.ad->alloc(V.ad, V.W, V.H, b->format, b->usage, &b->handle, &b->stride)) {
            fprintf(stderr, "nora: gralloc no da el buffer %d\n", i);
            return -1;
        }
        V.libre[i] = 1;
    }
    ANativeWindow *w = &V.win;
    w->common.magic = MAGIA('_', 'w', 'n', 'd');
    w->common.version = sizeof *w;
    w->common.incRef = w->common.decRef = nada;
    w->maxSwapInterval = 1;
    w->xdpi = w->ydpi = 228;   /* 3,5" a 640x480 */
    w->setSwapInterval = w_intervalo;
    w->dequeueBuffer_DEPRECATED = w_dequeue_viejo;
    w->lockBuffer_DEPRECATED = w_lock_viejo;
    w->queueBuffer_DEPRECATED = w_queue_viejo;
    w->query = w_query;
    w->perform = w_perform;
    w->cancelBuffer_DEPRECATED = w_cancel_viejo;
    w->dequeueBuffer = w_dequeue;
    w->queueBuffer = w_queue;
    w->cancelBuffer = w_cancel;
    return 0;
}

void *ventana_nativa(void) { return &V.win; }
int ventana_ancho(void) { return V.W; }
int ventana_alto(void) { return V.H; }
int ventana_tiene_vsync(void) { return V.vsync; }
double ventana_ms_copia(void) { return V.copias ? 1000 * V.tcopia / V.copias : 0; }
void ventana_cerrar(void)
{
    if (V.irq_fd >= 0) close(V.irq_fd);
    if (V.fb >= 0) {
        V.vi.yoffset = 0;
        ioctl(V.fb, FBIOPAN_DISPLAY, &V.vi);
    }
}
