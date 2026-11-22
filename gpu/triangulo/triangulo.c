/* triangulo: primera prueba de la GPU (PowerVR SGX544) de la RG35XX.
 *
 * Dibuja con OpenGL ES 2.0 en un pbuffer (sin ventana: asi no depende del
 * display class dc_owl), lee la imagen y la copia a /dev/fb0. Mide los fps.
 * Es un ejecutable de bionic (Android 4.4): se arranca con arranque.S.
 *
 *   triangulo [segundos]
 */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdarg.h>
#include <stddef.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

/* de bionic, declaradas a mano para no arrastrar cabeceras de glibc */
int vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
long write(int fd, const void *b, size_t n);
int open(const char *p, int f, ...);
int close(int fd);
int usleep(unsigned long us);
int atoi(const char *s);
float sinf(float x);
float cosf(float x);
void *memcpy(void *d, const void *s, size_t n);
int strcmp(const char *a, const char *b);
int kill(int pid, int sig);
void *opendir(const char *p);
int closedir(void *d);
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
    clock_gettime(1 /* CLOCK_MONOTONIC */, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

/* congelar el menu de GarlicOS ("main") para que no pinte encima */
static int menu = 0;
static void congelar_menu(int sig)
{
    char ruta[64], nombre[32];
    for (int pid = 2; pid < 4000 && !menu; pid++) {
        int n;
        {
            /* snprintf sin variadicos raros */
            char *p = ruta;
            const char *a = "/proc/";
            while (*a) *p++ = *a++;
            char num[12]; int k = 0, v = pid;
            do { num[k++] = '0' + v % 10; v /= 10; } while (v);
            while (k) *p++ = num[--k];
            a = "/comm";
            while (*a) *p++ = *a++;
            *p = 0;
        }
        int fd = open(ruta, O_RDONLY);
        if (fd < 0) continue;
        n = 0;
        extern long read(int, void *, size_t);
        n = read(fd, nombre, sizeof nombre - 1);
        close(fd);
        if (n > 0) {
            nombre[n] = 0;
            if (nombre[n - 1] == '\n') nombre[n - 1] = 0;
            if (!strcmp(nombre, "main")) menu = pid;
        }
    }
    if (menu) kill(menu, sig);
}

static GLuint shader(GLenum tipo, const char *src)
{
    GLuint s = glCreateShader(tipo);
    glShaderSource(s, 1, &src, 0);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof log, 0, log);
        di("shader: %s\n", log);
    }
    return s;
}

static const char *VS =
    "attribute vec2 pos; attribute vec3 col; uniform mediump float t; varying vec3 v;\n"
    "void main(){ float c=cos(t), s=sin(t);\n"
    "  vec2 p = vec2(c*pos.x - s*pos.y, s*pos.x + c*pos.y);\n"
    "  p.x *= 0.75; gl_Position = vec4(p, 0.0, 1.0); v = col; }\n";
static const char *FS =
    "precision mediump float; varying vec3 v; uniform float t;\n"
    "void main(){ gl_FragColor = vec4(v * (0.8 + 0.2*sin(t*3.0)), 1.0); }\n";

int main(int argc, char **argv)
{
    int segundos = argc > 1 ? atoi(argv[1]) : 10;

    int fb = open("/dev/fb0", O_RDWR);
    struct fb_var_screeninfo vi;
    struct fb_fix_screeninfo fi;
    if (fb < 0 || ioctl(fb, FBIOGET_VSCREENINFO, &vi) || ioctl(fb, FBIOGET_FSCREENINFO, &fi)) {
        di("sin framebuffer\n");
        return 1;
    }
    int W = vi.xres, H = vi.yres, bpp = vi.bits_per_pixel;
    int paginas = vi.yres_virtual / vi.yres;
    long tam = (long)fi.line_length * vi.yres_virtual;
    unsigned char *pant = mmap(0, tam, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    di("pantalla %dx%d a %d bits, %d paginas\n", W, H, bpp, paginas);

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint maj = 0, min = 0;
    if (!eglInitialize(dpy, &maj, &min)) {
        di("eglInitialize fallo: 0x%x\n", eglGetError());
        return 1;
    }
    di("EGL %d.%d  %s  %s\n", maj, min, eglQueryString(dpy, EGL_VENDOR), eglQueryString(dpy, EGL_VERSION));

    const EGLint atr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                          EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5, EGL_NONE};
    EGLConfig cfg;
    EGLint n = 0;
    if (!eglChooseConfig(dpy, atr, &cfg, 1, &n) || n < 1) {
        di("sin configuracion EGL: 0x%x\n", eglGetError());
        return 1;
    }
    EGLint r, g, b, a;
    eglGetConfigAttrib(dpy, cfg, EGL_RED_SIZE, &r);
    eglGetConfigAttrib(dpy, cfg, EGL_GREEN_SIZE, &g);
    eglGetConfigAttrib(dpy, cfg, EGL_BLUE_SIZE, &b);
    eglGetConfigAttrib(dpy, cfg, EGL_ALPHA_SIZE, &a);
    di("config RGBA %d%d%d%d\n", r, g, b, a);

    const EGLint pb[] = {EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE};
    EGLSurface sup = eglCreatePbufferSurface(dpy, cfg, pb);
    const EGLint ctxa[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxa);
    if (sup == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, sup, sup, ctx)) {
        di("no hay superficie/contexto: 0x%x\n", eglGetError());
        return 1;
    }
    di("GL: %s | %s | %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

    GLint fmt = 0, tipo = 0;
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &fmt);
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &tipo);
    int lee565 = (fmt == GL_RGB && tipo == GL_UNSIGNED_SHORT_5_6_5);
    di("lectura nativa: fmt 0x%x tipo 0x%x %s\n", fmt, tipo, lee565 ? "(565 directo)" : "(RGBA + conversion)");

    GLuint prog = glCreateProgram();
    glAttachShader(prog, shader(GL_VERTEX_SHADER, VS));
    glAttachShader(prog, shader(GL_FRAGMENT_SHADER, FS));
    glBindAttribLocation(prog, 0, "pos");
    glBindAttribLocation(prog, 1, "col");
    glLinkProgram(prog);
    GLint enl = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &enl);
    if (!enl) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof log, 0, log);
        di("enlazar: %s\n", log);
    }
    di("programa enlazado=%d\n", enl);
    glUseProgram(prog);
    GLint ut = glGetUniformLocation(prog, "t");
    static const GLfloat vtx[] = {0, 0.8f, 1, 0.3f, 0.3f, -0.7f, -0.5f, 0.3f, 1, 0.4f, 0.7f, -0.5f, 0.3f, 0.5f, 1};
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * 4, vtx);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * 4, vtx + 2);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glViewport(0, 0, W, H);

    static unsigned short fila565[1280 * 1024];
    static unsigned int rgba[1280 * 1024];
    congelar_menu(SIGSTOP);

    double t0 = ahora(), tgpu = 0, tlee = 0;
    int cuadros = 0;
    while (ahora() - t0 < segundos) {
        float t = ahora() - t0;
        double a0 = ahora();
        glClearColor(0.06f + 0.04f * sinf(t), 0.07f, 0.12f + 0.05f * cosf(t * 0.7f), 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUniform1f(ut, t);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glFinish();
        if (cuadros == 0) {
            unsigned char px[4];
            glReadPixels(W / 2, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
            di("primer cuadro: error GL 0x%x, centro RGBA %d %d %d %d\n", glGetError(), px[0], px[1], px[2], px[3]);
        }
        double a1 = ahora();
        /* la imagen de GL va de abajo arriba: se copia girada */
        if (lee565 && bpp == 16) {
            glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, fila565);
            for (int y = 0; y < H; y++)
                for (int p = 0; p < paginas; p++)
                    memcpy(pant + ((long)p * H + y) * fi.line_length, fila565 + (H - 1 - y) * W, W * 2);
        } else {
            glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
            for (int y = 0; y < H; y++) {
                unsigned int *src = rgba + (H - 1 - y) * W;
                if (bpp == 16) {
                    unsigned short *d = (unsigned short *)(pant + (long)y * fi.line_length);
                    for (int x = 0; x < W; x++) {
                        unsigned int c = src[x];   /* bytes R,G,B,A en memoria */
                        d[x] = ((c & 0xf8) << 8) | ((c >> 5) & 0x7e0) | ((c >> 19) & 0x1f);
                    }
                    for (int p = 1; p < paginas; p++)
                        memcpy(pant + ((long)p * H + y) * fi.line_length, d, W * 2);
                }
            }
        }
        tgpu += a1 - a0;
        tlee += ahora() - a1;
        cuadros++;
    }
    double total = ahora() - t0;
    di("%d cuadros en %.1f s = %.1f fps | dibujar %.2f ms | leer+copiar %.2f ms\n", cuadros, total,
       cuadros / total, 1000 * tgpu / cuadros, 1000 * tlee / cuadros);
    congelar_menu(SIGCONT);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(dpy);
    return 0;
}
