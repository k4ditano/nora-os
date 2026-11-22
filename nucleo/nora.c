/* nora: el nucleo de Nora OS para la RG35XX.
 *
 * Abre la ventana de la GPU, pinta con OpenGL ES 2.0, lee los botones y corre
 * dos "mundos" de LuaJIT separados:
 *   - la APP abierta (el lanzador "inicio" es una app mas), y
 *   - el SISTEMA (barra de estado, menu rapido), que se pinta encima de todo.
 * Si una app falla, el nucleo y el sistema siguen: se enseña el error.
 *
 *   nora [id-de-app | carpeta]      (por defecto "inicio")
 *   NORA_APPS=/mnt/mmc/nora/apps    donde estan las apps (una carpeta por app)
 *   NORA_LIB=/mnt/mmc/nora/lib      modulos Lua comunes (json...), por defecto <apps>/../lib
 *
 * El PC (nora-puente) se conecta por TCP a :7070, un JSON por linea. El nucleo
 * manda el saludo y los latidos; cada linea que llega va al sistema:
 * app.mensaje(linea). El sistema guarda los datos para las apps con
 * nora.guardar(clave, texto) y cada app los lee con nora.dato(clave).
 *
 * Una app (app.lua) define, todas opcionales:
 *   app.entrar()  app.actualiza(dt)  app.pinta(t)  app.boton(b)  app.suelta(b)
 *   app.barra = true    para que el sistema pinte la barra de estado encima
 * El sistema (sistema/app.lua) recibe app.pinta(t, barra, id_app) y
 * app.boton(b) -> true si se lo queda; app.abierto() -> true con el menu abierto.
 * Botones: A B X Y L R L2 R2 SELECT START MENU ARRIBA ABAJO IZQUIERDA DERECHA.
 * MENU va siempre al sistema. SELECT + START sale de Nora.
 */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/input.h>
#include <linux/rtc.h>
#include <sound/asound.h>
#ifndef BTN_SOUTH
#define BTN_SOUTH BTN_A
#define BTN_EAST BTN_B
#define BTN_NORTH BTN_X
#define BTN_WEST BTN_Y
#endif
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "ventana.h"
#include "pinta.h"
#include "sonido.h"
#include "enlace.h"

#define VERSION "0.4"

static volatile int seguir = 1;
static void parar(int s) { (void)s; seguir = 0; }
static double ahora(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static const char *apps_dir(void)
{
    const char *d = getenv("NORA_APPS");
    return d ? d : "/mnt/mmc/nora/apps";
}
static const char *lib_dir(void)
{
    static char d[512];
    const char *e = getenv("NORA_LIB");
    if (e) return e;
    if (!d[0]) snprintf(d, sizeof d, "%s/../lib", apps_dir());
    return d;
}

/* ---------------- el menu de GarlicOS (mientras exista) ---------------- */
static int menu_pid;
static void menu_garlic(int sig)
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

/* ---------------- hardware ---------------- */
static int lee_int(const char *ruta, int def)
{
    FILE *f = fopen(ruta, "r");
    int v = def;
    if (f) { if (fscanf(f, "%d", &v) != 1) v = def; fclose(f); }
    return v;
}
static int lee_txt(const char *ruta, char *b, int n)
{
    FILE *f = fopen(ruta, "r");
    b[0] = 0;
    if (!f) return -1;
    if (fgets(b, n, f)) b[strcspn(b, "\n")] = 0;
    fclose(f);
    return 0;
}
static void escribe_int(const char *ruta, int v)
{
    FILE *f = fopen(ruta, "w");
    if (f) { fprintf(f, "%d", v); fclose(f); }
}
#define BRILLO "/sys/class/backlight/backlight.2/"
/* volumen: el control 14 del codec ("DAC PA Volume", 0..40) */
#define VOL_NUMID 14
#define VOL_MAX 40
static int volumen(int nuevo)        /* nuevo < 0: solo leer */
{
    int fd = open("/dev/snd/controlC0", O_RDWR), v = -1;
    if (fd < 0) return -1;
    struct snd_ctl_elem_value e;
    memset(&e, 0, sizeof e);
    e.id.numid = VOL_NUMID;
    if (nuevo >= 0) {
        e.value.integer.value[0] = nuevo > VOL_MAX ? VOL_MAX : nuevo;
        ioctl(fd, SNDRV_CTL_IOCTL_ELEM_WRITE, &e);
    }
    memset(&e, 0, sizeof e);
    e.id.numid = VOL_NUMID;
    if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_READ, &e) == 0) v = e.value.integer.value[0];
    close(fd);
    return v;
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
    case KEY_UP: return "ARRIBA";
    case KEY_DOWN: return "ABAJO";
    case KEY_LEFT: return "IZQUIERDA";
    case KEY_RIGHT: return "DERECHA";
    case KEY_POWER: return "ENCENDIDO";
    }
    return 0;
}

/* ---------------- los mundos de Lua ---------------- */
typedef struct {
    lua_State *L;
    char id[64], carpeta[512], error[1024];
} mundo_t;
static mundo_t app, sis;
static color_t fondo;
static double t0, t_app;
static char pendiente[512];          /* app que hay que abrir en cuanto se pueda */

static uint32_t color_arg(lua_State *l, int i, uint32_t def)
{
    if (lua_type(l, i) == LUA_TNUMBER) return (uint32_t)lua_tonumber(l, i);
    if (lua_type(l, i) == LUA_TSTRING) {
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
static int fuente_arg(lua_State *l, int i)
{
    int f = pinta_fuente(luaL_optstring(l, i, "peq"));
    return f < 0 ? 0 : f;
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
static int l_texto(lua_State *l)    /* s x y fuente color alfa alinea -> ancho */
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
static int l_icono(lua_State *l)    /* nombre cx cy color alfa */
{
    lua_pushboolean(l, pinta_icono(luaL_checkstring(l, 1), luaL_checknumber(l, 2), luaL_checknumber(l, 3),
                                   color_args(l, 4)) == 0);
    return 1;
}
static int l_ancho(lua_State *l) { lua_pushnumber(l, pinta_ancho(fuente_arg(l, 2), luaL_checkstring(l, 1))); return 1; }
static int l_alto(lua_State *l) { lua_pushnumber(l, pinta_alto_fuente(fuente_arg(l, 1))); return 1; }
static int l_ahora(lua_State *l) { lua_pushnumber(l, ahora() - t0); return 1; }
static int hay_mezclador;
static int l_suena(lua_State *l)    /* wav [volumen 0..1] */
{
    const char *wav = luaL_checkstring(l, 1);
    if (hay_mezclador) { sonido_efecto(wav, (float)luaL_optnumber(l, 2, 1)); return 0; }
    const char *rep = getenv("NORA_SUENA");            /* sin mezclador: un proceso por sonido */
    if (!rep) rep = "/tmp/nora/suena";
    if (fork() == 0) {
        execl(rep, "suena", wav, (char *)0);
        _exit(1);
    }
    return 0;
}
static int l_musica(lua_State *l)   /* wav [volumen 0..1]; sin wav: parar con fundido */
{
    sonido_musica(luaL_optstring(l, 1, 0), (float)luaL_optnumber(l, 2, 0.5));
    return 0;
}
static int l_log(lua_State *l)
{
    fprintf(stderr, "lua:");
    for (int i = 1; i <= lua_gettop(l); i++) {
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
static int l_lista(lua_State *l)    /* nombres de una carpeta, ordenados */
{
    DIR *d = opendir(luaL_checkstring(l, 1));
    lua_newtable(l);
    if (!d) return 1;
    struct dirent *e;
    int n = 0;
    while ((e = readdir(d)))
        if (e->d_name[0] != '.') {
            lua_pushstring(l, e->d_name);
            lua_rawseti(l, -2, ++n);
        }
    closedir(d);
    lua_getglobal(l, "table");
    lua_getfield(l, -1, "sort");
    lua_pushvalue(l, -3);
    lua_call(l, 1, 0);
    lua_pop(l, 1);
    return 1;
}
static int l_abrir(lua_State *l) { snprintf(pendiente, sizeof pendiente, "%s", luaL_checkstring(l, 1)); return 0; }
static int l_volver(lua_State *l) { (void)l; snprintf(pendiente, sizeof pendiente, "inicio"); return 0; }
static int l_app(lua_State *l) { lua_pushstring(l, app.id); return 1; }
static int l_bateria(lua_State *l)
{
    char st[32];
    lua_pushnumber(l, lee_int("/sys/class/power_supply/battery/capacity", -1));
    lee_txt("/sys/class/power_supply/battery/status", st, sizeof st);
    lua_pushboolean(l, !strcmp(st, "Charging") || !strcmp(st, "Full"));
    return 2;
}
static int l_enlace(lua_State *l)   /* "cable" | "wifi" | nil */
{
    char s[16];
    if (lee_txt("/sys/class/net/rndis0/operstate", s, sizeof s) == 0 && !strcmp(s, "up")) lua_pushstring(l, "cable");
    else if (lee_txt("/sys/class/net/wlan0/operstate", s, sizeof s) == 0 && !strcmp(s, "up")) lua_pushstring(l, "wifi");
    else lua_pushnil(l);
    return 1;
}
static int l_brillo(lua_State *l)   /* [0..1] -> 0..1 */
{
    int max = lee_int(BRILLO "max_brightness", 1024);
    if (lua_isnumber(l, 1)) {
        double v = lua_tonumber(l, 1);
        v = v < 0 ? 0 : v > 1 ? 1 : v;
        escribe_int(BRILLO "brightness", 8 + (int)(v * (max - 8)));
    }
    lua_pushnumber(l, (lee_int(BRILLO "brightness", max) - 8) / (double)(max - 8));
    return 1;
}
static int l_volumen(lua_State *l)  /* [0..1] -> 0..1 */
{
    int v = volumen(lua_isnumber(l, 1) ? (int)(lua_tonumber(l, 1) * VOL_MAX + 0.5) : -1);
    lua_pushnumber(l, v < 0 ? 0 : v / (double)VOL_MAX);
    return 1;
}

/* ---------------- datos del PC, compartidos entre los dos mundos ---------------- */
typedef struct dato { char *clave, *valor; int version; struct dato *sig; } dato_t;
static dato_t *datos;
static int version_datos;
static dato_t *busca_dato(const char *clave)
{
    for (dato_t *d = datos; d; d = d->sig)
        if (!strcmp(d->clave, clave)) return d;
    return 0;
}
static int l_guardar(lua_State *l)  /* clave texto|nil */
{
    const char *clave = luaL_checkstring(l, 1);
    dato_t *d = busca_dato(clave);
    if (!d) {
        d = calloc(1, sizeof *d);
        d->clave = strdup(clave);
        d->sig = datos;
        datos = d;
    }
    free(d->valor);
    d->valor = lua_isnoneornil(l, 2) ? 0 : strdup(luaL_checkstring(l, 2));
    d->version = ++version_datos;
    return 0;
}
static int l_dato(lua_State *l)     /* clave -> texto|nil, version (cambia con cada guardar) */
{
    dato_t *d = busca_dato(luaL_checkstring(l, 1));
    if (!d || !d->valor) { lua_pushnil(l); lua_pushnumber(l, d ? d->version : 0); return 2; }
    lua_pushstring(l, d->valor);
    lua_pushnumber(l, d->version);
    return 2;
}
static int l_enviar(lua_State *l) { lua_pushboolean(l, enlace_enviar(luaL_checkstring(l, 1)) == 0); return 1; }
static int l_conectado(lua_State *l)    /* -> true, ip | false */
{
    if (!enlace_conectado()) { lua_pushboolean(l, 0); return 1; }
    lua_pushboolean(l, 1);
    lua_pushstring(l, enlace_quien());
    return 2;
}
static int l_poner_hora(lua_State *l)   /* segundos desde 1970 (UTC): reloj del sistema y RTC */
{
    struct timeval tv = {(time_t)luaL_checknumber(l, 1), 0};
    int ok = settimeofday(&tv, 0) == 0;
    int fd = open("/dev/rtc0", O_WRONLY);
    if (fd >= 0) {
        struct tm g;
        gmtime_r(&tv.tv_sec, &g);
        struct rtc_time r;
        memset(&r, 0, sizeof r);
        r.tm_sec = g.tm_sec; r.tm_min = g.tm_min; r.tm_hour = g.tm_hour;
        r.tm_mday = g.tm_mday; r.tm_mon = g.tm_mon; r.tm_year = g.tm_year;
        if (ioctl(fd, RTC_SET_TIME, &r)) perror("nora: RTC_SET_TIME");
        close(fd);
    }
    if (!ok) perror("nora: settimeofday");
    lua_pushboolean(l, ok);
    return 1;
}

/* ---- archivos: para instalar apps desde el PC ---- */
static int l_de64(lua_State *l)     /* base64 -> bytes */
{
    size_t n;
    const unsigned char *s = (const unsigned char *)luaL_checklstring(l, 1, &n);
    static signed char tabla[256];
    if (!tabla['B']) {
        memset(tabla, -1, sizeof tabla);
        const char *a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; i++) tabla[(unsigned char)a[i]] = (signed char)i;
        tabla['-'] = 62; tabla['_'] = 63;
    }
    luaL_Buffer b;
    luaL_buffinit(l, &b);
    unsigned v = 0;
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
        int c = tabla[s[i]];
        if (c < 0) continue;              /* '=', saltos de linea... */
        v = (v << 6) | (unsigned)c;
        bits += 6;
        if (bits >= 8) { bits -= 8; luaL_addchar(&b, (char)((v >> bits) & 0xff)); }
    }
    luaL_pushresult(&b);
    return 1;
}
static void crear_carpetas(const char *ruta)   /* mkdir -p de la carpeta de ruta */
{
    char c[512];
    snprintf(c, sizeof c, "%s", ruta);
    for (char *p = c + 1; *p; p++)
        if (*p == '/') { *p = 0; mkdir(c, 0755); *p = '/'; }
}
static int l_escribir(lua_State *l) /* escribe entero (a .tmp y renombra: nadie lee medio archivo) */
{
    const char *ruta = luaL_checkstring(l, 1);
    size_t n;
    const char *datos = luaL_checklstring(l, 2, &n);
    char tmp[540];
    crear_carpetas(ruta);
    snprintf(tmp, sizeof tmp, "%s.tmp", ruta);
    FILE *f = fopen(tmp, "wb");
    int ok = f && fwrite(datos, 1, n, f) == n;
    if (f && fclose(f)) ok = 0;
    if (ok && rename(tmp, ruta)) ok = 0;
    if (!ok) { lua_pushnil(l); lua_pushstring(l, strerror(errno)); unlink(tmp); return 2; }
    lua_pushboolean(l, 1);
    return 1;
}
static int l_borrar(lua_State *l)   /* un archivo o una carpeta vacia */
{
    const char *ruta = luaL_checkstring(l, 1);
    lua_pushboolean(l, unlink(ruta) == 0 || rmdir(ruta) == 0);
    return 1;
}
static int l_es_carpeta(lua_State *l)
{
    struct stat st;
    lua_pushboolean(l, stat(luaL_checkstring(l, 1), &st) == 0 && S_ISDIR(st.st_mode));
    return 1;
}

static const luaL_Reg API[] = {
    {"fondo", l_fondo}, {"caja", l_caja}, {"sombra", l_sombra}, {"circulo", l_circulo}, {"texto", l_texto},
    {"icono", l_icono}, {"ancho", l_ancho}, {"alto", l_alto}, {"ahora", l_ahora}, {"suena", l_suena},
    {"musica", l_musica}, {"log", l_log}, {"salir", l_salir}, {"lista", l_lista}, {"abrir", l_abrir}, {"volver", l_volver},
    {"app", l_app}, {"bateria", l_bateria}, {"enlace", l_enlace}, {"brillo", l_brillo},
    {"volumen", l_volumen}, {"guardar", l_guardar}, {"dato", l_dato}, {"enviar", l_enviar},
    {"conectado", l_conectado}, {"poner_hora", l_poner_hora}, {"de64", l_de64}, {"escribir", l_escribir},
    {"borrar", l_borrar}, {"es_carpeta", l_es_carpeta}, {0, 0}};

static int traza(lua_State *l)
{
    luaL_traceback(l, l, lua_tostring(l, 1), 1);
    return 1;
}

/* llama a app.<nombre>(args) en un mundo; devuelve cuantos resultados deja (0 o nres) */
static int llama(mundo_t *m, const char *nombre, int nargs, int nres)
{
    lua_State *L = m->L;
    if (!L || m->error[0]) { if (L) lua_pop(L, nargs); return 0; }
    lua_getglobal(L, "app");
    if (lua_istable(L, -1)) lua_getfield(L, -1, nombre); else lua_pushnil(L);
    lua_remove(L, -2);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1 + nargs); return 0; }
    lua_insert(L, -1 - nargs);
    lua_pushcfunction(L, traza);
    lua_insert(L, -2 - nargs);
    int base = lua_gettop(L) - nargs - 1;
    if (lua_pcall(L, nargs, nres, base)) {
        snprintf(m->error, sizeof m->error, "%s", lua_tostring(L, -1));
        fprintf(stderr, "nora: error en %s, app.%s: %s\n", m->id, nombre, m->error);
        lua_pop(L, 1);
        lua_remove(L, base);
        return 0;
    }
    lua_remove(L, base);
    return nres;
}
static int llama_bool(mundo_t *m, const char *nombre, int nargs)
{
    if (!llama(m, nombre, nargs, 1)) return 0;
    int v = lua_toboolean(m->L, -1);
    lua_pop(m->L, 1);
    return v;
}
static int campo_bool(mundo_t *m, const char *campo)
{
    if (!m->L) return 0;
    lua_getglobal(m->L, "app");
    int v = 0;
    if (lua_istable(m->L, -1)) {
        lua_getfield(m->L, -1, campo);
        v = lua_toboolean(m->L, -1);
        lua_pop(m->L, 1);
    }
    lua_pop(m->L, 1);
    return v;
}

static void mundo_cerrar(mundo_t *m)
{
    if (m->L) lua_close(m->L);
    memset(m, 0, sizeof *m);
}
/* abre una app: "id" (en NORA_APPS) o una ruta a carpeta/fichero .lua */
static int mundo_abrir(mundo_t *m, const char *quien)
{
    char ruta[600];
    struct stat st;
    mundo_cerrar(m);
    if (strchr(quien, '/')) {
        if (stat(quien, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(m->carpeta, sizeof m->carpeta, "%s", quien);
            snprintf(ruta, sizeof ruta, "%s/app.lua", quien);
        } else {
            snprintf(ruta, sizeof ruta, "%s", quien);
            snprintf(m->carpeta, sizeof m->carpeta, "%s", quien);
            *strrchr(m->carpeta, '/') = 0;
        }
        const char *b = strrchr(m->carpeta, '/');
        snprintf(m->id, sizeof m->id, "%s", b ? b + 1 : m->carpeta);
    } else {
        snprintf(m->id, sizeof m->id, "%s", quien);
        snprintf(m->carpeta, sizeof m->carpeta, "%s/%s", apps_dir(), quien);
        snprintf(ruta, sizeof ruta, "%s/app.lua", m->carpeta);
    }
    lua_State *L = m->L = luaL_newstate();
    luaL_openlibs(L);
    luaL_register(L, "nora", API);
    lua_pushnumber(L, ventana_ancho()); lua_setfield(L, -2, "W");
    lua_pushnumber(L, ventana_alto()); lua_setfield(L, -2, "H");
    lua_pushstring(L, VERSION); lua_setfield(L, -2, "version");
    lua_pushstring(L, m->carpeta); lua_setfield(L, -2, "carpeta");
    lua_pushstring(L, apps_dir()); lua_setfield(L, -2, "apps");
    lua_pop(L, 1);
    lua_getglobal(L, "package");          /* require() busca primero en la carpeta de la app */
    lua_getfield(L, -1, "path");
    lua_pushfstring(L, "%s/?.lua;%s/?.lua;%s", m->carpeta, lib_dir(), lua_tostring(L, -1));
    lua_setfield(L, -3, "path");
    lua_pop(L, 2);
    lua_newtable(L);
    lua_setglobal(L, "app");
    lua_pushcfunction(L, traza);
    if (luaL_loadfile(L, ruta) || lua_pcall(L, 0, 0, -2)) {
        snprintf(m->error, sizeof m->error, "%s", lua_tostring(L, -1));
        fprintf(stderr, "nora: no carga %s: %s\n", ruta, m->error);
        lua_pop(L, 2);
        return -1;
    }
    lua_pop(L, 1);
    fprintf(stderr, "nora: abierta %s\n", m->id);
    return 0;
}

static void pinta_error(mundo_t *m)
{
    pinta_empezar(color_hex(0x2a0f14, 1));
    pinta_caja(24, 24, 592, 432, 20, color_hex(0x3d1820, 1), 0);
    char t[96];
    snprintf(t, sizeof t, "«%s» ha fallado", m->id);
    pinta_texto(1, 48, 44, t, color_hex(0xf87171, 1));
    char linea[64];
    const char *p = m->error;
    float y = 100;
    while (*p && y < 400) {
        int n = 0;
        while (p[n] && p[n] != '\n' && n < 60) n++;
        memcpy(linea, p, n);
        linea[n] = 0;
        pinta_texto(0, 48, y, linea, color_hex(0xf3d1d6, 1));
        p += n;
        if (*p == '\n') p++;
        y += 26;
    }
    pinta_texto(0, 48, 416, "B para volver al inicio", color_hex(0xb07a84, 1));
}

/* ---------------- entrada ---------------- */
static int sel_pulsado, start_pulsado, sis_tiene;   /* sis_tiene: el sistema se quedo la pulsacion */
static void entrada_boton(const char *b, int pulsado)
{
    if (!strcmp(b, "SELECT")) sel_pulsado = pulsado;
    if (!strcmp(b, "START")) start_pulsado = pulsado;
    if (sel_pulsado && start_pulsado) seguir = 0;
    if (pendiente[0]) return;                       /* cambiando de app */
    /* MENU y, con el menu abierto, todo: al sistema */
    if (pulsado && sis.L && (!strcmp(b, "MENU") || llama_bool(&sis, "abierto", 0))) {
        lua_pushstring(sis.L, b);
        sis_tiene = llama_bool(&sis, "boton", 1);
        if (sis_tiene) return;
    }
    if (!pulsado && sis_tiene) { sis_tiene = 0; return; }
    if (app.error[0]) {                             /* la app fallo: B vuelve al inicio */
        if (pulsado && !strcmp(b, "B") && strcmp(app.id, "inicio")) snprintf(pendiente, sizeof pendiente, "inicio");
        return;
    }
    lua_pushstring(app.L, b);
    llama(&app, pulsado ? "boton" : "suelta", 1, 0);
}
/* la cruceta del "RG35XX Gamepad" llega como eje (ABS_HAT0X/Y), y L2/R2 como
 * ejes analogicos (ABS_Z/ABS_RZ, 0..255): todo se traduce a botones */
static void entrada_eje(int code, int valor)
{
    static int hat[2], gat[2];
    if (code == ABS_HAT0X || code == ABS_HAT0Y) {
        int i = code == ABS_HAT0Y;
        const char *menos = i ? "ARRIBA" : "IZQUIERDA", *mas = i ? "ABAJO" : "DERECHA";
        int v = valor < 0 ? -1 : valor > 0 ? 1 : 0;
        if (v == hat[i]) return;
        if (hat[i] < 0) entrada_boton(menos, 0);
        if (hat[i] > 0) entrada_boton(mas, 0);
        hat[i] = v;
        if (v < 0) entrada_boton(menos, 1);
        if (v > 0) entrada_boton(mas, 1);
    } else if (code == ABS_Z || code == ABS_RZ) {
        int i = code == ABS_RZ, p = valor > 127;
        if (p != gat[i]) { gat[i] = p; entrada_boton(i ? "R2" : "L2", p); }
    }
}

/* ---------------- el PC ---------------- */
static void recibir(const char *linea)
{
    if (strstr(linea, "\"tipo\":\"latido\"")) return;    /* el latido solo mantiene viva la conexion */
    if (!sis.L) return;
    lua_pushstring(sis.L, linea);
    llama(&sis, "mensaje", 1, 0);
}
static void saludar(void)
{
    char st[32], m[256];
    lee_txt("/sys/class/power_supply/battery/status", st, sizeof st);
    snprintf(m, sizeof m,
             "{\"tipo\":\"hola\",\"version\":\"%s\",\"bateria\":%d,\"cargando\":%s,\"app\":\"%s\",\"ancho\":%d,\"alto\":%d}",
             VERSION, lee_int("/sys/class/power_supply/battery/capacity", -1),
             !strcmp(st, "Charging") || !strcmp(st, "Full") ? "true" : "false", app.id, ventana_ancho(), ventana_alto());
    enlace_enviar(m);
}

int main(int argc, char **argv)
{
    const char *primera = argc > 1 ? argv[1] : "inicio";
    signal(SIGINT, parar);
    signal(SIGTERM, parar);
    signal(SIGCHLD, SIG_IGN);
    setvbuf(stderr, 0, _IONBF, 0);

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
    fprintf(stderr, "nora %s: %s, %dx%d, apps en %s, vsync %s\n", VERSION, glGetString(GL_RENDERER), W, H,
            apps_dir(), ventana_tiene_vsync() ? "si" : "no (se limita a 60 fps)");

    hay_mezclador = sonido_iniciar() == 0;
    if (!hay_mezclador) fprintf(stderr, "nora: sin mezclador de sonido (cada efecto, un proceso)\n");
    t0 = ahora();
    char ruta_sis[600];
    snprintf(ruta_sis, sizeof ruta_sis, "%s/sistema/app.lua", apps_dir());
    if (access(ruta_sis, R_OK) == 0) {
        mundo_abrir(&sis, ruta_sis);
        llama(&sis, "entrar", 0, 0);
    }
    mundo_abrir(&app, primera);
    t_app = ahora();
    fondo = color_hex(0x0e1017, 1);
    llama(&app, "entrar", 0, 0);

    menu_garlic(SIGSTOP);
    abrir_entradas();
    enlace_iniciar(ENLACE_PUERTO);

    /* fundido entre apps: fase 1 oscurece, se cambia, fase 2 aclara */
    int fase = 0;
    double t_fase = 0;
    const double SALE = 0.16, ENTRA = 0.24;
    double antes = ahora(), t_fps = antes;
    int cuadros = 0, mostrar_fps = getenv("NORA_FPS") != 0;
    char fps_txt[64] = "";
    while (seguir) {
        struct input_event ev;
        for (int i = 0; i < nentradas; i++)
            while (read(entradas[i], &ev, sizeof ev) == sizeof ev) {
                if (ev.type == EV_ABS) { entrada_eje(ev.code, ev.value); continue; }
                if (ev.type != EV_KEY || ev.value == 2) continue;
                const char *b = nombre_boton(ev.code);
                if (!b) { if (ev.value) fprintf(stderr, "nora: boton sin nombre, codigo %d\n", ev.code); continue; }
                entrada_boton(b, ev.value);
            }
        double t = ahora(), dt = t - antes;
        antes = t;

        int cambio = enlace_paso(t, recibir);
        if (cambio) {
            if (cambio > 0) saludar();
            if (sis.L) { lua_pushboolean(sis.L, cambio > 0); llama(&sis, "enlace", 1, 0); }
        }

        if (pendiente[0] && fase == 0) { fase = 1; t_fase = t; }
        if (fase == 1 && t - t_fase >= SALE) {
            char quien[512];
            snprintf(quien, sizeof quien, "%s", pendiente);
            mundo_abrir(&app, quien);
            pendiente[0] = 0;
            fondo = color_hex(0x0e1017, 1);
            t_app = t;
            llama(&app, "entrar", 0, 0);
            fase = 2;
            t_fase = t;
        }
        if (fase == 2 && t - t_fase >= ENTRA) fase = 0;

        if (app.error[0]) pinta_error(&app);
        else {
            lua_pushnumber(app.L, dt);
            llama(&app, "actualiza", 1, 0);
            pinta_empezar(fondo);
            lua_pushnumber(app.L, t - t_app);
            llama(&app, "pinta", 1, 0);
        }
        if (sis.L) {
            lua_pushnumber(sis.L, t - t0);
            lua_pushboolean(sis.L, campo_bool(&app, "barra"));
            lua_pushstring(sis.L, app.id);
            llama(&sis, "pinta", 3, 0);
        }
        if (fase) {
            double p = fase == 1 ? (t - t_fase) / SALE : 1 - (t - t_fase) / ENTRA;
            if (p > 1) p = 1;
            if (p < 0) p = 0;
            pinta_caja(-2, -2, W + 4, H + 4, 0, color_hex(0x0e1017, (float)p), 0);
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
        if (!ventana_tiene_vsync()) {
            double resto = 1.0 / 60 - (ahora() - t);
            if (resto > 0) usleep((useconds_t)(resto * 1e6));
        }
    }

    enlace_cerrar();
    sonido_cerrar();
    ventana_cerrar();
    menu_garlic(SIGCONT);
    mundo_cerrar(&app);
    mundo_cerrar(&sis);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(dpy);
    return 0;
}
