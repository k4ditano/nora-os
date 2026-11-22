/* ventana: ver ventana.h. Cabeceras del HAL de Android 4.4 escritas a mano. */
#include "ventana.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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
        if (V.vsync) {
            uint32_t pantalla = 0;
            ioctl(V.fb, FBIO_WAITFORVSYNC, &pantalla);
        }
        V.pagina = pag;
        V.vi.yoffset = pag * V.H;
        ioctl(V.fb, FBIOPAN_DISPLAY, &V.vi);
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
    if (V.vi.bits_per_pixel != 16) {
        fprintf(stderr, "nora: la pantalla no esta en 16 bits (%d)\n", V.vi.bits_per_pixel);
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
    if (V.fb >= 0) {
        V.vi.yoffset = 0;
        ioctl(V.fb, FBIOPAN_DISPLAY, &V.vi);
    }
}
