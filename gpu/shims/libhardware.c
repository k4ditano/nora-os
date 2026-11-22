/* libhardware de mentira para la RG35XX: lo que piden las librerias de la
 * SGX544 de la LeMaker Guitar y el Android 4.4 de la consola no trae.
 *
 *  - hw_get_module / hw_get_module_by_class: carga <dir>/<id>.<variante>.so
 *  - atof, rand, stpcpy: el libc (bionic 4.4) de la consola no las exporta.
 *    Se precarga con LD_PRELOAD para que las encuentre cualquier libreria.
 *
 * Se enlaza contra el bionic de la consola (no glibc): sin errno, sin TLS.
 */
#include <stddef.h>

int snprintf(char *s, size_t n, const char *fmt, ...);
int strcmp(const char *a, const char *b);
double strtod(const char *s, char **end);
long lrand48(void);
void *dlopen(const char *f, int flags);
void *dlsym(void *h, const char *s);
int dlclose(void *h);
const char *dlerror(void);
char *getenv(const char *n);
int access(const char *p, int modo);
int __android_log_print(int prio, const char *tag, const char *fmt, ...);

#define RTLD_NOW 0          /* bionic: RTLD_NOW = 0 */
#define ENOENT 2
#define EINVAL 22

typedef struct hw_module_t {
    unsigned int tag;
    unsigned short module_api_version, hal_api_version;
    const char *id, *name, *author;
    void *methods;
    void *dso;
    unsigned int reserved[32 - 7];
} hw_module_t;

static const char *dirs[] = { 0 /* $NORA_HW */, "/tmp/gpu/hw", "/system/vendor/lib/hw", "/system/lib/hw" };
static const char *variantes[] = { "S500", "gs705a", "atm7059", "default" };

int hw_get_module_by_class(const char *class_id, const char *inst, const hw_module_t **module)
{
    char nombre[64], ruta[256];
    if (!class_id || !module) return -EINVAL;
    if (inst) snprintf(nombre, sizeof nombre, "%s.%s", class_id, inst);
    else snprintf(nombre, sizeof nombre, "%s", class_id);
    dirs[0] = getenv("NORA_HW");
    for (unsigned d = 0; d < sizeof dirs / sizeof *dirs; d++) {
        if (!dirs[d]) continue;
        for (unsigned v = 0; v < sizeof variantes / sizeof *variantes; v++) {
            snprintf(ruta, sizeof ruta, "%s/%s.%s.so", dirs[d], nombre, variantes[v]);
            if (access(ruta, 4 /* R_OK */) != 0) continue;
            void *h = dlopen(ruta, RTLD_NOW);
            if (!h) {
                __android_log_print(6, "nora-hw", "no carga %s: %s", ruta, dlerror());
                continue;
            }
            hw_module_t *m = dlsym(h, "HMI");
            if (!m || strcmp(m->id, class_id)) {
                dlclose(h);
                continue;
            }
            m->dso = h;
            *module = m;
            __android_log_print(4, "nora-hw", "modulo %s <- %s", nombre, ruta);
            return 0;
        }
    }
    return -ENOENT;
}

int hw_get_module(const char *id, const hw_module_t **module)
{
    return hw_get_module_by_class(id, 0, module);
}

/* lo que falta en el libc */
double atof(const char *s) { return strtod(s, 0); }
int rand(void) { return (int)(lrand48() & 0x7fffffff); }
char *stpcpy(char *d, const char *s)
{
    while ((*d = *s++)) d++;
    return d;
}
int getpagesize(void) { return 4096; }

/* dlopen precargado: libIMGegl busca sus librerias GL en rutas fijas de Android
 * (/system/vendor/lib/egl/...) y /system es de solo lectura. Se reescribe la
 * ruta a $NORA_GL (por defecto /tmp/gpu/lib) y se llama al dlopen de verdad,
 * el del enlazador, que se saca de libdl.so. */
size_t strlen(const char *s);
int strncmp(const char *a, const char *b, size_t n);
static void *(*dlopen_real)(const char *, int);
void *dlopen(const char *f, int flags)
{
    static const char pre[] = "/system/vendor/lib/egl/";
    char ruta[256];
    if (!dlopen_real) {
        /* RTLD_DEFAULT (bionic 4.4: 0xffffffff) busca desde la cabeza de la
           lista de librerias, que es la libdl falsa del enlazador: su dlopen */
        dlopen_real = (void *(*)(const char *, int))dlsym((void *)0xffffffff, "dlopen");
        if (!dlopen_real || dlopen_real == dlopen) {
            __android_log_print(6, "nora-hw", "no encuentro el dlopen del enlazador");
            return 0;
        }
    }
    if (f && !strncmp(f, pre, sizeof pre - 1)) {
        const char *d = getenv("NORA_GL");
        snprintf(ruta, sizeof ruta, "%s/%s", d ? d : "/tmp/gpu/lib", f + sizeof pre - 1);
        f = ruta;
    }
    return dlopen_real(f, flags);
}
