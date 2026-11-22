/* nora: el nucleo de Nora OS para la RG35XX.
 *
 * Abre la ventana de la GPU, pinta con OpenGL ES 2.0, lee los botones y corre
 * una app escrita en LuaJIT con la API nora.*. Si la app falla, el nucleo no
 * se cae: enseña el error y espera.
 *
 *   nora carpeta-de-la-app        (con un app.lua dentro)
 *   nora fichero.lua
 *
 * La app define (todas opcionales):
 *   app.entrar()            al empezar
 *   app.actualiza(dt)       cada fotograma, dt en segundos
 *   app.pinta(t)            cada fotograma, t = segundos desde que empezo
 *   app.boton(nombre)       al pulsar: "A" "B" "X" "Y" "L" "R" "L2" "R2"
 *   app.suelta(nombre)      al soltar   "SELECT" "START" "MENU"
 *                                       "ARRIBA" "ABAJO" "IZQUIERDA" "DERECHA"
 * SELECT + START sale siempre (devuelve el control al menu de GarlicOS).
 */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/input.h>
/* los nombres "SOUTH/EAST..." no estan en las cabeceras del kernel 3.10 */
#ifndef BTN_SOUTH
#define BTN_SOUTH BTN_A
#define BTN_EAST BTN_B
#define BTN_NORTH BTN_X
#define BTN_WEST BTN_Y
#endif
#ifndef BTN_DPAD_UP
#define BTN_DPAD_UP 0x220
#define BTN_DPAD_DOWN 0x221
#define BTN_DPAD_LEFT 0x222
#define BTN_DPAD_RIGHT 0x223
#endif
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "ventana.h"
#include "pinta.h"

#define VERSION "0.1"

static volatile int seguir = 1;
static void parar(int s) { (void)s; seguir = 0; }
static double ahora(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

/* ---------------- el menu de GarlicOS (mientras exista) ---------------- */
static int menu_pid;
static void menu(int sig)
{
    if (!menu_pid) {
        DIR *d = opendir("/proc");
        struct dirent *e;
        while (d && (e = readdir(d)) && !menu_pid) {
            char ruta[64], nombre[32] = "";
            snprintf(ruta, sizeof ruta, "/proc/%s/comm", e->d_name);
            FILE *f = fopen(ruta, "r");
            if (!f) continue;
            if (fgets(nombre, sizeof nombre, f)) nombre[strcspn(nombre, "\n")] = 0;
            fclose(f);
            if (!strcmp(nombre, "main")) menu_pid = atoi(e->d_name);
        }
        if (d) closedir(d);
    }
    if (menu_pid) kill(menu_pid, sig);
}

/* ---------------- botones ---------------- */
static int entradas[8], nentradas;
static void abrir_entradas(void)
{
    DIR *d = opendir("/dev/input");
    struct dirent *e;
    while (d && (e = readdir(d)) && nentradas < 8) {
        if (strncmp(e->d_name, "event", 5)) continue;
        char ruta[64];
        snprintf(ruta, sizeof ruta, "/dev/input/%s", e->d_name);
        int fd = open(ruta, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        ioctl(fd, EVIOCGRAB, 1);           /* los botones, solo para Nora */
        entradas[nentradas++] = fd;
    }
    if (d) closedir(d);
}
static const char *nombre_boton(int code)
{
    switch (code) {
    case BTN_SOUTH: return "A";
    case BTN_EAST: return "B";
    case BTN_NORTH: return "X";
    case BTN_WEST: return "Y";
    case BTN_TL: return "L";
    case BTN_TR: return "R";
    case BTN_TL2: return "L2";
    case BTN_TR2: return "R2";
    case BTN_SELECT: case KEY_ESC: return "SELECT";
    case BTN_START: case KEY_ENTER: return "START";
    case BTN_MODE: case KEY_MENU: case KEY_HOME: return "MENU";
    case KEY_UP: case BTN_DPAD_UP: return "ARRIBA";
    case KEY_DOWN: case BTN_DPAD_DOWN: return "ABAJO";
    case KEY_LEFT: case BTN_DPAD_LEFT: return "IZQUIERDA";
    case KEY_RIGHT: case BTN_DPAD_RIGHT: return "DERECHA";
    }
    return 0;
}

/* ---------------- estado para la API ---------------- */
static color_t fondo;
static double t0;
static lua_State *L;
static char error_app[1024];
static int sel_pulsado, start_pulsado;

static uint32_t color_arg(lua_State *l, int i, uint32_t def)
{
    if (lua_isnumber(l, i)) return (uint32_t)lua_tonumber(l, i);
    if (lua_isstring(l, i)) {
        const char *s = lua_tostring(l, i);
        if (*s == '#') s++;
        return (uint32_t)strtoul(s, 0, 16);
    }
    return def;
}
static color_t color_args(lua_State *l, int i)
{
    return color_hex(color_arg(l, i, 0xffffff), (float)luaL_optnumber(l, i + 1, 1.0));
}

static int l_fondo(lua_State *l) { fondo = color_hex(color_arg(l, 1, 0), 1); return 0; }
static int l_caja(lua_State *l)
{
    pinta_caja(luaL_checknumber(l, 1), luaL_checknumber(l, 2), luaL_checknumber(l, 3), luaL_checknumber(l, 4),
               luaL_optnumber(l, 5, 0), color_args(l, 6), 0);
    return 0;
}
static int l_sombra(lua_State *l)   /* x y w h radio difuminado color alfa */
{
    pinta_caja(luaL_checknumber(l, 1), luaL_checknumber(l, 2), luaL_checknumber(l, 3), luaL_checknumber(l, 4),
               luaL_optnumber(l, 5, 0), color_args(l, 7), luaL_optnumber(l, 6, 8));
    return 0;
}
static int l_circulo(lua_State *l)
{
    float r = luaL_checknumber(l, 3);
    pinta_caja(luaL_checknumber(l, 1) - r, luaL_checknumber(l, 2) - r, 2 * r, 2 * r, r, color_args(l, 4), 0);
    return 0;
}
static int fuente_arg(lua_State *l, int i)
{
    int f = pinta_fuente(luaL_optstring(l, i, "peq"));
    return f < 0 ? 0 : f;
}
/* nora.texto(s, x, y, fuente, color, alfa, alinea) -> ancho */
static int l_texto(lua_State *l)
{
    const char *s = luaL_checkstring(l, 1);
    float x = luaL_checknumber(l, 2), y = luaL_checknumber(l, 3);
    int f = fuente_arg(l, 4);
    color_t c = color_args(l, 5);
    const char *al = luaL_optstring(l, 7, "izq");
    float w = pinta_ancho(f, s);
    if (!strcmp(al, "centro")) x -= w / 2;
    else if (!strcmp(al, "der")) x -= w;
    lua_pushnumber(l, pinta_texto(f, x, y, s, c));
    return 1;
}
static int l_ancho(lua_State *l) { lua_pushnumber(l, pinta_ancho(fuente_arg(l, 2), luaL_checkstring(l, 1))); return 1; }
static int l_alto(lua_State *l) { lua_pushnumber(l, pinta_alto_fuente(fuente_arg(l, 1))); return 1; }
static int l_ahora(lua_State *l) { lua_pushnumber(l, ahora() - t0); return 1; }
static int l_suena(lua_State *l)
{
    const char *wav = luaL_checkstring(l, 1);
    const char *rep = getenv("NORA_SUENA");
    if (!rep) rep = "/tmp/suena";
    if (fork() == 0) {
        execl(rep, "suena", wav, (char *)0);
        _exit(1);
    }
    return 0;
}
static int l_log(lua_State *l)
{
    int n = lua_gettop(l);
    fprintf(stderr, "app:");
    for (int i = 1; i <= n; i++) {
        lua_getglobal(l, "tostring");
        lua_pushvalue(l, i);
        lua_call(l, 1, 1);
        fprintf(stderr, " %s", lua_tostring(l, -1));
        lua_pop(l, 1);
    }
    fprintf(stderr, "\n");
    return 0;
}
static int l_salir(lua_State *l) { (void)l; seguir = 0; return 0; }

static const luaL_Reg API[] = {
    {"fondo", l_fondo}, {"caja", l_caja}, {"sombra", l_sombra}, {"circulo", l_circulo},
    {"texto", l_texto}, {"ancho", l_ancho}, {"alto", l_alto}, {"ahora", l_ahora},
    {"suena", l_suena}, {"log", l_log}, {"salir", l_salir}, {0, 0}};

/* llama a app.<nombre>(args...) si existe; los errores se guardan, no tumban */
static int traza(lua_State *l)
{
    luaL_traceback(l, l, lua_tostring(l, 1), 1);
    return 1;
}
static void llama(const char *nombre, int nargs)
{
    if (error_app[0]) { lua_pop(L, nargs); return; }
    lua_getglobal(L, "app");
    if (!lua_istable(L, -1)) { lua_pop(L, 1 + nargs); return; }
    lua_getfield(L, -1, nombre);
    lua_remove(L, -2);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1 + nargs); return; }
    lua_insert(L, -1 - nargs);
    lua_pushcfunction(L, traza);
    lua_insert(L, -2 - nargs);
    int base = lua_gettop(L) - nargs - 1;
    if (lua_pcall(L, nargs, 0, base)) {
        snprintf(error_app, sizeof error_app, "%s", lua_tostring(L, -1));
        fprintf(stderr, "nora: error en app.%s: %s\n", nombre, error_app);
        lua_pop(L, 1);
    }
    lua_remove(L, base);
}

static void pinta_error(void)
{
    pinta_empezar(color_hex(0x2a0f14, 1));
    pinta_caja(24, 24, 592, 432, 20, color_hex(0x3d1820, 1), 0);
    pinta_texto(1, 48, 44, "La app ha fallado", color_hex(0xf87171, 1));
    /* el error en lineas de 60 caracteres */
    char linea[64];
    const char *p = error_app;
    float y = 100;
    while (*p && y < 410) {
        int n = 0;
        while (p[n] && p[n] != '\n' && n < 60) n++;
        memcpy(linea, p, n);
        linea[n] = 0;
        pinta_texto(0, 48, y, linea, color_hex(0xf3d1d6, 1));
        p += n;
        if (*p == '\n') p++;
        y += 26;
    }
    pinta_texto(0, 48, 420, "SELECT + START para salir", color_hex(0xb07a84, 1));
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "uso: nora carpeta-de-app | fichero.lua\n");
        return 2;
    }
    signal(SIGINT, parar);
    signal(SIGTERM, parar);
    signal(SIGCHLD, SIG_IGN);
    setvbuf(stderr, 0, _IONBF, 0);

    /* la app: carpeta con app.lua, o un .lua suelto */
    char ruta[512], carpeta[512];
    struct stat st;
    if (stat(argv[1], &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(carpeta, sizeof carpeta, "%s", argv[1]);
        snprintf(ruta, sizeof ruta, "%s/app.lua", argv[1]);
    } else {
        snprintf(ruta, sizeof ruta, "%s", argv[1]);
        snprintf(carpeta, sizeof carpeta, "%s", argv[1]);
        char *b = strrchr(carpeta, '/');
        if (b) *b = 0; else strcpy(carpeta, ".");
    }

    if (ventana_abrir()) return 1;
    int W = ventana_ancho(), H = ventana_alto();
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, 0, 0)) { fprintf(stderr, "nora: eglInitialize 0x%x\n", eglGetError()); return 1; }
    const EGLint atr[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                          EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5, EGL_NONE};
    EGLConfig cfgs[64], cfg = 0;
    EGLint n = 0;
    eglChooseConfig(dpy, atr, cfgs, 64, &n);
    for (int i = 0; i < n && !cfg; i++) {   /* la 565 exacta, que casa con la ventana */
        EGLint r, g, b;
        eglGetConfigAttrib(dpy, cfgs[i], EGL_RED_SIZE, &r);
        eglGetConfigAttrib(dpy, cfgs[i], EGL_GREEN_SIZE, &g);
        eglGetConfigAttrib(dpy, cfgs[i], EGL_BLUE_SIZE, &b);
        if (r == 5 && g == 6 && b == 5) cfg = cfgs[i];
    }
    if (!cfg) { if (!n) { fprintf(stderr, "nora: sin config EGL\n"); return 1; } cfg = cfgs[0]; }
    EGLSurface sup = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)ventana_nativa(), 0);
    const EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ca);
    if (sup == EGL_NO_SURFACE || !eglMakeCurrent(dpy, sup, sup, ctx)) {
        fprintf(stderr, "nora: sin superficie EGL 0x%x\n", eglGetError());
        return 1;
    }
    eglSwapInterval(dpy, 0);
    if (pinta_iniciar(W, H)) fprintf(stderr, "nora: aviso, GL dio error al iniciar\n");
    fprintf(stderr, "nora %s: %s, %dx%d, vsync %s\n", VERSION, glGetString(GL_RENDERER), W, H,
            ventana_tiene_vsync() ? "si" : "no (se limita a 60 fps)");

    /* LuaJIT con la API */
    L = luaL_newstate();
    luaL_openlibs(L);
    luaL_register(L, "nora", API);
    lua_pushnumber(L, W); lua_setfield(L, -2, "W");
    lua_pushnumber(L, H); lua_setfield(L, -2, "H");
    lua_pushstring(L, VERSION); lua_setfield(L, -2, "version");
    lua_pushstring(L, carpeta); lua_setfield(L, -2, "carpeta");
    lua_pop(L, 1);
    lua_getglobal(L, "package");          /* require() busca primero en la carpeta de la app */
    lua_getfield(L, -1, "path");
    lua_pushfstring(L, "%s/?.lua;%s", carpeta, lua_tostring(L, -1));
    lua_setfield(L, -3, "path");
    lua_pop(L, 2);
    lua_newtable(L);
    lua_setglobal(L, "app");
    lua_pushcfunction(L, traza);
    if (luaL_loadfile(L, ruta) || lua_pcall(L, 0, 0, -2)) {
        snprintf(error_app, sizeof error_app, "%s", lua_tostring(L, -1));
        fprintf(stderr, "nora: no carga %s: %s\n", ruta, error_app);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    menu(SIGSTOP);
    abrir_entradas();
    t0 = ahora();
    fondo = color_hex(0x0e1017, 1);
    llama("entrar", 0);

    double antes = ahora(), t_fps = antes, objetivo = 1.0 / 60;
    int cuadros = 0, mostrar_fps = getenv("NORA_FPS") != 0;
    char fps_txt[64] = "";
    while (seguir) {
        struct input_event ev;
        for (int i = 0; i < nentradas; i++)
            while (read(entradas[i], &ev, sizeof ev) == sizeof ev) {
                if (ev.type != EV_KEY || ev.value == 2) continue;   /* 2 = autorepeticion */
                const char *b = nombre_boton(ev.code);
                char otro[24];
                if (!b) { snprintf(otro, sizeof otro, "codigo_%d", ev.code); b = otro; }
                if (!strcmp(b, "SELECT")) sel_pulsado = ev.value;
                if (!strcmp(b, "START")) start_pulsado = ev.value;
                if (sel_pulsado && start_pulsado) seguir = 0;
                lua_pushstring(L, b);
                llama(ev.value ? "boton" : "suelta", 1);
            }
        double t = ahora(), dt = t - antes;
        antes = t;
        if (error_app[0]) pinta_error();
        else {
            lua_pushnumber(L, dt);
            llama("actualiza", 1);
            pinta_empezar(fondo);
            lua_pushnumber(L, t - t0);
            llama("pinta", 1);
        }
        if (mostrar_fps) pinta_texto(0, 8, H - 26, fps_txt, color_hex(0x34d399, 0.9f));
        pinta_terminar();
        eglSwapBuffers(dpy, sup);
        cuadros++;
        if (t - t_fps >= 1) {
            snprintf(fps_txt, sizeof fps_txt, "%d fps  copia %.1f ms  tandas %d", cuadros, ventana_ms_copia(),
                     pinta_dibujados());
            cuadros = 0;
            t_fps = t;
        }
        if (!ventana_tiene_vsync()) {      /* sin vsync: no pasar de 60 */
            double resto = objetivo - (ahora() - t);
            if (resto > 0) usleep((useconds_t)(resto * 1e6));
        }
    }

    ventana_cerrar();
    menu(SIGCONT);
    lua_close(L);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(dpy);
    return 0;
}
