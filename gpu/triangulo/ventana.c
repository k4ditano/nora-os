/* ventana: la GPU de la RG35XX pintando "en ventana" (eglCreateWindowSurface).
 *
 * En Android la ventana (ANativeWindow) la da SurfaceFlinger; aqui no hay, asi
 * que la hacemos nosotros, como libhybris: sus buffers salen de gralloc
 * (RGB565, el formato de la pantalla, y con cache de CPU para leerlos rapido)
 * y al "encolar" cada fotograma se copia a una pagina libre de /dev/fb0 y se
 * hace pan (triple buffer: sin parpadeo). Ejecutable de bionic (arranque.S).
 *
 *   ventana [segundos]
 */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

int vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
long write(int fd, const void *b, size_t n);
long read(int fd, void *b, size_t n);
int open(const char *p, int f, ...);
int close(int fd);
int atoi(const char *s);
float sinf(float x);
float cosf(float x);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int strcmp(const char *a, const char *b);
int kill(int pid, int sig);
int clock_gettime(int clk, struct timespec *t);

static void di(const char *fmt, ...)
{
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    write(1, b, n > 0 ? n : 0);
}
static double ahora(void)
{
    struct timespec t;
    clock_gettime(1, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

/* ---------------- HAL de Android (cabeceras escritas a mano, KitKat) ---------------- */

typedef struct hw_module_t hw_module_t;
typedef struct hw_device_t hw_device_t;
typedef struct {
    int (*open)(const hw_module_t *m, const char *id, hw_device_t **dev);
} hw_module_methods_t;
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
int hw_get_module(const char *id, const hw_module_t **m);   /* de nuestra libhardware */

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

/* ---------------- nuestra ventana ---------------- */

#define NBUF 3
static struct {
    ANativeWindow win;
    ANativeWindowBuffer buf[NBUF];
    int libre[NBUF];
    int siguiente;
    gralloc_module_t *gr;
    alloc_device_t *ad;
    int W, H, fb, pagina, paginas;
    unsigned char *pant;
    struct fb_var_screeninfo vi;
    struct fb_fix_screeninfo fi;
    double tcopia;
    int encolados;
} V;

static void nada(void *p) { (void)p; }

static int w_intervalo(ANativeWindow *w, int i) { return 0; }
static int w_query(const ANativeWindow *w, int que, int *v)
{
    switch (que) {
    case 0: case 6: *v = V.W; return 0;            /* WIDTH, DEFAULT_WIDTH */
    case 1: case 7: *v = V.H; return 0;            /* HEIGHT, DEFAULT_HEIGHT */
    case 2: *v = HAL_RGB_565; return 0;            /* FORMAT */
    case 3: *v = 1; return 0;                      /* MIN_UNDEQUEUED_BUFFERS */
    case 4: *v = 0; return 0;                      /* QUEUES_TO_WINDOW_COMPOSER */
    case 5: *v = 0; return 0;                      /* CONCRETE_TYPE: framebuffer */
    case 10: *v = USO_HW_RENDER | USO_HW_TEXTURE; return 0;   /* CONSUMER_USAGE_BITS */
    default: *v = 0; return 0;
    }
}
static int w_perform(ANativeWindow *w, int op, ...) { return 0; }

static int w_dequeue(ANativeWindow *w, ANativeWindowBuffer **b, int *fence)
{
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
    return -16;   /* EBUSY: no deberia pasar con un solo hilo */
}
static int w_dequeue_viejo(ANativeWindow *w, ANativeWindowBuffer **b) { return w_dequeue(w, b, 0); }
static int w_lock_viejo(ANativeWindow *w, ANativeWindowBuffer *b) { return 0; }

static int indice(ANativeWindowBuffer *b) { return (int)(b - V.buf); }

static int w_queue(ANativeWindow *w, ANativeWindowBuffer *b, int fence)
{
    double t0 = ahora();
    void *src = 0;
    if (fence >= 0) close(fence);   /* lock() ya espera a que la GPU acabe */
    /* lock con lectura de CPU: gralloc espera a la GPU y nos da la direccion */
    if (V.gr->lock(V.gr, b->handle, USO_SW_READ_OFTEN, 0, 0, V.W, V.H, &src) == 0 && src) {
        V.pagina = (V.pagina + 1) % V.paginas;
        unsigned char *dst = V.pant + (long)V.pagina * V.H * V.fi.line_length;
        if (b->stride * 2 == (int)V.fi.line_length)
            memcpy(dst, src, (size_t)V.fi.line_length * V.H);
        else
            for (int y = 0; y < V.H; y++)
                memcpy(dst + (long)y * V.fi.line_length, (unsigned char *)src + (long)y * b->stride * 2, V.W * 2);
        V.gr->unlock(V.gr, b->handle);
        V.vi.yoffset = V.pagina * V.H;
        ioctl(V.fb, FBIOPAN_DISPLAY, &V.vi);
    }
    V.libre[indice(b)] = 1;
    V.tcopia += ahora() - t0;
    V.encolados++;
    return 0;
}
static int w_queue_viejo(ANativeWindow *w, ANativeWindowBuffer *b) { return w_queue(w, b, -1); }
static int w_cancel(ANativeWindow *w, ANativeWindowBuffer *b, int fence)
{
    if (fence >= 0) close(fence);
    V.libre[indice(b)] = 1;
    return 0;
}
static int w_cancel_viejo(ANativeWindow *w, ANativeWindowBuffer *b) { return w_cancel(w, b, -1); }

static int crear_ventana(void)
{
    V.fb = open("/dev/fb0", O_RDWR);
    if (V.fb < 0 || ioctl(V.fb, FBIOGET_VSCREENINFO, &V.vi) || ioctl(V.fb, FBIOGET_FSCREENINFO, &V.fi)) return -1;
    V.W = V.vi.xres;
    V.H = V.vi.yres;
    V.paginas = V.vi.yres_virtual / V.vi.yres;
    if (V.vi.bits_per_pixel != 16) { di("la pantalla no esta en 16 bits\n"); return -1; }
    V.pant = mmap(0, (size_t)V.fi.line_length * V.vi.yres_virtual, PROT_READ | PROT_WRITE, MAP_SHARED, V.fb, 0);

    if (hw_get_module("gralloc", (const hw_module_t **)&V.gr)) { di("sin gralloc\n"); return -1; }
    if (V.gr->common.methods->open(&V.gr->common, "gpu0", (hw_device_t **)&V.ad)) { di("gralloc: sin gpu0\n"); return -1; }
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
        int r = V.ad->alloc(V.ad, V.W, V.H, b->format, b->usage, &b->handle, &b->stride);
        if (r) { di("gralloc alloc %d fallo: %d\n", i, r); return -1; }
        V.libre[i] = 1;
    }
    di("ventana %dx%d, %d buffers de gralloc (stride %d), %d paginas de pantalla\n", V.W, V.H, NBUF,
       V.buf[0].stride, V.paginas);

    ANativeWindow *w = &V.win;
    w->common.magic = MAGIA('_', 'w', 'n', 'd');
    w->common.version = sizeof *w;
    w->common.incRef = w->common.decRef = nada;
    *(int *)&w->minSwapInterval = 0;
    *(int *)&w->maxSwapInterval = 1;
    *(float *)&w->xdpi = *(float *)&w->ydpi = 160;
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

/* ---------------- el menu de GarlicOS, congelado ---------------- */
static int menu = 0;
static void menu_senal(int sig)
{
    char ruta[32], nombre[32];
    for (int pid = 2; pid < 4000 && !menu; pid++) {
        char *p = ruta, num[12];
        int k = 0, v = pid;
        for (const char *a = "/proc/"; *a;) *p++ = *a++;
        do { num[k++] = '0' + v % 10; v /= 10; } while (v);
        while (k) *p++ = num[--k];
        for (const char *a = "/comm"; *a;) *p++ = *a++;
        *p = 0;
        int fd = open(ruta, O_RDONLY);
        if (fd < 0) continue;
        int n = read(fd, nombre, sizeof nombre - 1);
        close(fd);
        if (n > 0) {
            nombre[n] = 0;
            if (nombre[n - 1] == '\n') nombre[n - 1] = 0;
            if (!strcmp(nombre, "main")) menu = pid;
        }
    }
    if (menu) kill(menu, sig);
}

/* ---------------- la escena ---------------- */
static GLuint shader(GLenum tipo, const char *src)
{
    GLuint s = glCreateShader(tipo);
    glShaderSource(s, 1, &src, 0);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char l[512]; glGetShaderInfoLog(s, sizeof l, 0, l); di("shader: %s\n", l); }
    return s;
}
static const char *VS =
    "attribute vec2 pos; attribute vec3 col; uniform mediump float t; varying vec3 v;\n"
    "void main(){ float c=cos(t), s=sin(t);\n"
    "  vec2 p = vec2(c*pos.x - s*pos.y, s*pos.x + c*pos.y);\n"
    "  p.x *= 0.75; gl_Position = vec4(p, 0.0, 1.0); v = col; }\n";
static const char *FS =
    "precision mediump float; varying vec3 v; uniform mediump float t;\n"
    "void main(){ gl_FragColor = vec4(v * (0.8 + 0.2*sin(t*3.0)), 1.0); }\n";

int main(int argc, char **argv)
{
    int segundos = argc > 1 ? atoi(argv[1]) : 10;
    if (crear_ventana()) return 1;

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, 0, 0)) { di("eglInitialize: 0x%x\n", eglGetError()); return 1; }
    const EGLint atr[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                          EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5, EGL_ALPHA_SIZE, 0,
                          EGL_NATIVE_VISUAL_ID, HAL_RGB_565, EGL_NONE};
    /* eglChooseConfig ordena poniendo primero las de mas bits: buscamos a mano
       la de 565 exacto, que casa con los buffers RGB565 de la ventana */
    EGLConfig cfgs[64], cfg = 0;
    EGLint n = 0, r = 0, g = 0, b = 0;
    if (!eglChooseConfig(dpy, atr, cfgs, 64, &n) || n < 1) { di("sin config de ventana: 0x%x\n", eglGetError()); return 1; }
    for (int i = 0; i < n && !cfg; i++) {
        eglGetConfigAttrib(dpy, cfgs[i], EGL_RED_SIZE, &r);
        eglGetConfigAttrib(dpy, cfgs[i], EGL_GREEN_SIZE, &g);
        eglGetConfigAttrib(dpy, cfgs[i], EGL_BLUE_SIZE, &b);
        if (r == 5 && g == 6 && b == 5) cfg = cfgs[i];
    }
    if (!cfg) { cfg = cfgs[0]; di("no hay config 565, uso la primera\n"); }
    di("config RGB %d%d%d\n", r, g, b);
    EGLSurface sup = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)&V.win, 0);
    if (sup == EGL_NO_SURFACE) { di("eglCreateWindowSurface: 0x%x\n", eglGetError()); return 1; }
    const EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ca);
    if (!eglMakeCurrent(dpy, sup, sup, ctx)) { di("eglMakeCurrent: 0x%x\n", eglGetError()); return 1; }
    eglSwapInterval(dpy, 0);
    di("GL: %s\n", glGetString(GL_RENDERER));

    GLuint prog = glCreateProgram();
    glAttachShader(prog, shader(GL_VERTEX_SHADER, VS));
    glAttachShader(prog, shader(GL_FRAGMENT_SHADER, FS));
    glBindAttribLocation(prog, 0, "pos");
    glBindAttribLocation(prog, 1, "col");
    glLinkProgram(prog);
    glUseProgram(prog);
    GLint ut = glGetUniformLocation(prog, "t");
    static const GLfloat vtx[] = {0, 0.8f, 1, 0.3f, 0.3f, -0.7f, -0.5f, 0.3f, 1, 0.4f, 0.7f, -0.5f, 0.3f, 0.5f, 1};
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * 4, vtx);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * 4, vtx + 2);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glViewport(0, 0, V.W, V.H);

    menu_senal(SIGSTOP);
    double t0 = ahora();
    int cuadros = 0;
    while (ahora() - t0 < segundos) {
        float t = ahora() - t0;
        glClearColor(0.06f + 0.04f * sinf(t), 0.07f, 0.12f + 0.05f * cosf(t * 0.7f), 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUniform1f(ut, t);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        eglSwapBuffers(dpy, sup);
        cuadros++;
    }
    double total = ahora() - t0;
    di("%d cuadros en %.1f s = %.1f fps | copia a pantalla %.2f ms por cuadro (%d encolados)\n", cuadros, total,
       cuadros / total, V.encolados ? 1000 * V.tcopia / V.encolados : 0.0, V.encolados);
    V.vi.yoffset = 0;
    ioctl(V.fb, FBIOPAN_DISPLAY, &V.vi);
    menu_senal(SIGCONT);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(dpy);
    return 0;
}
