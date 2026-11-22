/* usos: fichas con lo que queda de Claude y Codex en la pantalla de la RG35XX.
 *
 * Lee /tmp/usos.txt (Latin-1, lo escribe el PC cada minuto) y pinta una ficha
 * por bloque. Con la cruceta (izquierda / derecha) se pasa de una a otra.
 *
 *   F <tab> titulo <tab> subtitulo <tab> nota al pie <tab> color (hex)
 *   B <tab> nombre <tab> % usado (-1 = no se sabe) <tab> detalle
 *   T <tab> nombre <tab> valor                      (fila de texto)
 *
 * Congela el menu de GarlicOS mientras esta y se queda con los botones.
 * Sale con kill (o con SELECT+START).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/input.h>

#include "fuentes.h"

#define W 640
#define H 480

static unsigned int buf[W * H];
static volatile int seguir = 1;
static void parar(int s) { (void)s; seguir = 0; }

static double ahora(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

/* ---------- dibujo ---------- */

static inline void mezcla(int x, int y, unsigned int c, int a)
{
    if (x < 0 || y < 0 || x >= W || y >= H || a <= 0) return;
    unsigned int *p = &buf[y * W + x];
    if (a >= 255) { *p = c; return; }
    unsigned int d = *p;
    unsigned r = (((c >> 16) & 255) * a + ((d >> 16) & 255) * (255 - a)) / 255;
    unsigned g = (((c >> 8) & 255) * a + ((d >> 8) & 255) * (255 - a)) / 255;
    unsigned b = ((c & 255) * a + (d & 255) * (255 - a)) / 255;
    *p = r << 16 | g << 8 | b;
}

/* rectangulo redondeado relleno, con el borde suavizado y opacidad (0..255) */
static void caja_a(double x, double y, double w, double h, double r, unsigned int c, int op)
{
    for (int py = (int)y; py < (int)(y + h); py++) {
        if (py < 0 || py >= H) continue;
        for (int px = (int)x; px < (int)(x + w); px++) {
            if (px < 0 || px >= W) continue;
            double cx = px + 0.5, cy = py + 0.5;
            double qx = cx < x + r ? x + r - cx : (cx > x + w - r ? cx - (x + w - r) : 0);
            double qy = cy < y + r ? y + r - cy : (cy > y + h - r ? cy - (y + h - r) : 0);
            int a;
            if (qx == 0 && qy == 0) {                  /* dentro: solo los bordes rectos */
                double d = fmin(fmin(cx - x, x + w - cx), fmin(cy - y, y + h - cy));
                a = d >= 1 ? 255 : (int)(255 * d);
            } else {
                double d = sqrt(qx * qx + qy * qy) - r;
                a = d <= -0.5 ? 255 : d >= 0.5 ? 0 : (int)(255 * (0.5 - d));
            }
            mezcla(px, py, c, a * op / 255);
        }
    }
}

static void caja(double x, double y, double w, double h, double r, unsigned int c)
{
    caja_a(x, y, w, h, r, c, 255);
}
static void circulo(double cx, double cy, double r, unsigned int c)
{
    caja(cx - r, cy - r, 2 * r, 2 * r, r, c);
}
static void circulo_a(double cx, double cy, double r, unsigned int c, int op)
{
    caja_a(cx - r, cy - r, 2 * r, 2 * r, r, c, op);
}
static unsigned int aclarar(unsigned int c, double f)   /* f 0..1 hacia blanco */
{
    unsigned r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    r += (255 - r) * f; g += (255 - g) * f; b += (255 - b) * f;
    return r << 16 | g << 8 | b;
}
static double suave(double p)                            /* ease-out cubico, recortado */
{
    if (p <= 0) return 0;
    if (p >= 1) return 1;
    return 1 - pow(1 - p, 3);
}

/* lanza "suena fichero" sin esperar */
static void sonar(const char *wav)
{
    if (fork() == 0) {
        execl("/tmp/suena", "suena", wav, (char *)0);
        _exit(1);
    }
}

/* texto: devuelve el ancho */
static int ancho_texto(const fuente_t *f, const char *s)
{
    int w = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) w += f->g[*p].adv;
    return w;
}
static int texto(const fuente_t *f, int x, int y, const char *s, unsigned int c)
{
    int x0 = x;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        const glifo_t *g = &f->g[*p];
        const unsigned char *px = f->px + g->off;
        for (int gy = 0; gy < g->h; gy++)
            for (int gx = 0; gx < g->w; gx++)
                mezcla(x + g->x + gx, y + g->y + gy, c, px[gy * g->w + gx]);
        x += g->adv;
    }
    return x - x0;
}
static void texto_der(const fuente_t *f, int xder, int y, const char *s, unsigned int c)
{
    texto(f, xder - ancho_texto(f, s), y, s, c);
}

/* ---------- datos ---------- */

#define MAXF 6
#define MAXB 4
#define MAXT 6
typedef struct { char nombre[48]; int pct; char detalle[64]; } barra_t;
typedef struct { char nombre[48]; char valor[48]; } fila_t;
typedef struct {
    char titulo[48], sub[64], nota[80];
    unsigned int color;
    int nb, nt;
    barra_t b[MAXB];
    fila_t t[MAXT];
} ficha_t;

static ficha_t fichas[MAXF];
static int nfichas = 0;

static char *campo(char **s)
{
    char *r = *s;
    if (!r) return "";
    char *t = strchr(r, '\t');
    if (t) { *t = 0; *s = t + 1; } else *s = 0;
    return r;
}

static void leer_datos(void)
{
    FILE *f = fopen("/tmp/usos.txt", "r");
    if (!f) return;
    ficha_t nuevas[MAXF];
    int n = -1;
    char linea[256];
    while (fgets(linea, sizeof linea, f)) {
        linea[strcspn(linea, "\r\n")] = 0;
        char *s = linea;
        char *tipo = campo(&s);
        if (!strcmp(tipo, "F") && n < MAXF - 1) {
            ficha_t *k = &nuevas[++n];
            memset(k, 0, sizeof *k);
            snprintf(k->titulo, sizeof k->titulo, "%s", campo(&s));
            snprintf(k->sub, sizeof k->sub, "%s", campo(&s));
            snprintf(k->nota, sizeof k->nota, "%s", campo(&s));
            k->color = strtoul(campo(&s), 0, 16);
        } else if (!strcmp(tipo, "B") && n >= 0 && nuevas[n].nb < MAXB) {
            barra_t *b = &nuevas[n].b[nuevas[n].nb++];
            snprintf(b->nombre, sizeof b->nombre, "%s", campo(&s));
            b->pct = atoi(campo(&s));
            snprintf(b->detalle, sizeof b->detalle, "%s", campo(&s));
        } else if (!strcmp(tipo, "T") && n >= 0 && nuevas[n].nt < MAXT) {
            fila_t *t = &nuevas[n].t[nuevas[n].nt++];
            snprintf(t->nombre, sizeof t->nombre, "%s", campo(&s));
            snprintf(t->valor, sizeof t->valor, "%s", campo(&s));
        }
    }
    fclose(f);
    if (n >= 0) { memcpy(fichas, nuevas, sizeof(ficha_t) * (n + 1)); nfichas = n + 1; }
}

/* ---------- la ficha ---------- */

static const unsigned int FONDO = 0x0e1017, PANEL = 0x1a1d29, TENUE = 0x8b90a5,
                          CLARO = 0xf3f4f8, RIEL = 0x2b2f3f;

static unsigned int color_uso(int pct)
{
    if (pct < 0) return TENUE;
    if (pct < 60) return 0x34d399;
    if (pct < 85) return 0xfbbf24;
    return 0xf87171;
}

/* dx: desplazamiento horizontal; t: reloj; ent: segundos desde que entro la ficha */
static void pintar_ficha(const ficha_t *k, int dx, double t, double ent)
{
    int x = 24 + dx, y = 20, w = W - 48;
    if (x > W || x + w < 0) return;
    caja(x, y, w, 410, 22, PANEL);

    /* motas que flotan dentro de la ficha, del color de la marca */
    for (int i = 0; i < 14; i++) {
        double fx = fmod(i * 0.618 + t * (0.010 + 0.004 * (i % 3)), 1.0);
        double fy = fmod(i * 0.377 + t * (0.020 + 0.006 * (i % 4)), 1.0);
        double mx = x + 30 + fx * (w - 60) + 8 * sin(t * 0.9 + i);
        double my = y + 400 - fy * 380;
        double vida = sin(M_PI * fy);                     /* aparecen y se van */
        circulo_a(mx, my, 2 + i % 3, k->color, (int)(40 * vida));
    }

    /* filo de color que respira */
    int resp = 170 + (int)(85 * (0.5 + 0.5 * sin(t * 2.2)));
    caja_a(x, y + 24, 6, 410 - 48, 3, k->color, resp);

    texto(&F_MED, x + 28, y + 22, k->titulo, CLARO);
    texto(&F_PEQ, x + 28, y + 60, k->sub, TENUE);

    int cy = y + 98;
    if (k->nb > 0) {
        /* en grande, lo que QUEDA de la ventana mas apurada, contando desde 0 */
        const barra_t *b = &k->b[0];
        for (int i = 1; i < k->nb; i++)
            if (k->b[i].pct > b->pct) b = &k->b[i];
        char num[16];
        unsigned int col = color_uso(b->pct);
        if (b->pct >= 0) {
            double e = suave((ent - 0.10) / 0.9);
            snprintf(num, sizeof num, "%d%%", (int)((100 - b->pct) * e + 0.5));
            if (b->pct >= 85) col = aclarar(col, 0.25 * (0.5 + 0.5 * sin(t * 4)));   /* late */
        } else
            snprintf(num, sizeof num, "?");
        int sube = (int)(18 * (1 - suave(ent / 0.5)));     /* entra subiendo */
        int wn = texto(&F_GRA, x + 24, cy - 6 + sube, num, col);
        char et[80];
        snprintf(et, sizeof et, "te queda (%s)", b->nombre);
        for (char *c = et; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
        texto(&F_PEQ, x + 36 + wn, cy + 40 + sube, et, CLARO);
        texto(&F_PEQ, x + 36 + wn, cy + 66 + sube, b->detalle, TENUE);
        cy += 118;
        for (int i = 0; i < k->nb; i++) {
            const barra_t *bi = &k->b[i];
            char p[16];
            if (bi->pct >= 0) snprintf(p, sizeof p, "%d%% usado", bi->pct);
            else snprintf(p, sizeof p, "sin datos");
            texto(&F_PEQ, x + 28, cy, bi->nombre, CLARO);
            texto_der(&F_PEQ, x + w - 28, cy, p, TENUE);
            int bw = w - 56, bx = x + 28, by = cy + 30;
            caja(bx, by, bw, 12, 6, RIEL);
            if (bi->pct > 0) {
                double e = suave((ent - 0.25 - 0.15 * i) / 0.8);   /* se llenan en cascada */
                double lleno = bw * (bi->pct > 100 ? 100 : bi->pct) / 100.0 * e;
                if (lleno >= 12) {
                    unsigned int cb = color_uso(bi->pct);
                    caja(bx, by, lleno, 12, 6, cb);
                    /* brillo que recorre la barra */
                    double pos = fmod(t * 0.45 + i * 0.37, 1.6) - 0.3;
                    double hx = bx + pos * lleno;
                    for (int px = (int)(hx - 30); px < hx + 30; px++) {
                        if (px < bx + 6 || px > bx + lleno - 6) continue;
                        int al = (int)(110 * (1 - fabs(px - hx) / 30));
                        for (int py = by + 2; py < by + 10; py++) mezcla(px, py, 0xffffff, al);
                    }
                }
            } else if (bi->pct < 0) {                      /* sin datos: un punto que va y viene */
                double pos = 0.5 + 0.5 * sin(t * 1.5 + i);
                circulo_a(bx + 6 + pos * (bw - 12), by + 6, 5, TENUE, 160);
            }
            if (bi != b) texto(&F_PEQ, x + 28, cy + 48, bi->detalle, TENUE);
            cy += bi != b ? 78 : 56;
        }
    }
    for (int i = 0; i < k->nt; i++) {
        int sube = (int)(16 * (1 - suave((ent - 0.08 * i) / 0.5)));
        texto(&F_MED, x + 28, cy + sube, k->t[i].nombre, CLARO);
        texto_der(&F_MED, x + w - 28, cy + sube, k->t[i].valor, k->color);
        cy += 50;
    }
    /* puntito "en vivo" junto a la nota */
    int ny = y + 410 - 36;
    circulo_a(x + 34, ny + 11, 4, 0x34d399, 120 + (int)(135 * (0.5 + 0.5 * sin(t * 3))));
    texto(&F_PEQ, x + 46, ny, k->nota, TENUE);
}

static void pintar_puntos(double pos)   /* pos: ficha actual, con decimales en la transicion */
{
    int total = nfichas, sep = 24;
    int x0 = W / 2 - (total - 1) * sep / 2;
    for (int i = 0; i < total; i++) {
        double cerca = 1 - fmin(1, fabs(pos - i));        /* 1 en la actual */
        double ancho = 8 + 14 * cerca;
        unsigned int c = cerca > 0.5 ? CLARO : 0x4a4f63;
        caja(x0 + i * sep - ancho / 2, 451, ancho, 8, 4, c);
    }
}

/* ---------- entrada y menu ---------- */

static int fds[8], nfds = 0;
static void abrir_entradas(void)
{
    DIR *d = opendir("/dev/input");
    struct dirent *e;
    if (!d) return;
    while ((e = readdir(d)) && nfds < 8) {
        if (strncmp(e->d_name, "event", 5)) continue;
        char ruta[64];
        snprintf(ruta, sizeof ruta, "/dev/input/%s", e->d_name);
        int fd = open(ruta, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        ioctl(fd, EVIOCGRAB, 1);
        fds[nfds++] = fd;
    }
    closedir(d);
}

static int menu_pid = 0;
static int buscar_menu(void)
{
    DIR *d = opendir("/proc");
    struct dirent *e;
    int r = 0;
    if (!d) return 0;
    while (!r && (e = readdir(d))) {
        int pid = atoi(e->d_name);
        if (pid <= 1) continue;
        char ruta[64], nombre[32] = "";
        snprintf(ruta, sizeof ruta, "/proc/%d/comm", pid);
        FILE *f = fopen(ruta, "r");
        if (!f) continue;
        if (fgets(nombre, sizeof nombre, f)) nombre[strcspn(nombre, "\n")] = 0;
        fclose(f);
        if (!strcmp(nombre, "main")) r = pid;
    }
    closedir(d);
    return r;
}
static void vigilar_menu(void)
{
    if (!menu_pid || kill(menu_pid, 0) != 0) menu_pid = buscar_menu();
    if (menu_pid) kill(menu_pid, SIGSTOP);
}

int main(void)
{
    setvbuf(stderr, 0, _IONBF, 0);
    signal(SIGINT, parar);
    signal(SIGTERM, parar);

    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) { perror("fb0"); return 1; }
    struct fb_var_screeninfo vi;
    struct fb_fix_screeninfo fi;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) < 0 || ioctl(fd, FBIOGET_FSCREENINFO, &fi) < 0) {
        perror("ioctl");
        return 1;
    }
    int bpp = vi.bits_per_pixel;
    if ((bpp != 32 && bpp != 16) || vi.xres != W || vi.yres != H) {
        fprintf(stderr, "esperaba 640x480 a 16 o 32 bits, hay %ux%ux%u\n", vi.xres, vi.yres, bpp);
        return 1;
    }
    long paginas = vi.yres_virtual / vi.yres;
    if (paginas < 1) paginas = 1;
    long tam = (long)fi.line_length * vi.yres_virtual;
    unsigned char *fb = mmap(0, tam, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) { perror("mmap"); return 1; }

    vigilar_menu();
    abrir_entradas();

    int actual = 0;
    double desde = -1, t_anim = 0, ult = 0;   /* animacion: de "desde" a "actual" */
    int sel = 0, start = 0;
    double t_entrada = ahora();
    signal(SIGCHLD, SIG_IGN);

    while (seguir) {
        double t = ahora();
        if (t - ult > 1) { leer_datos(); vigilar_menu(); ult = t; }

        struct input_event ev;
        int paso = 0;
        for (int i = 0; i < nfds; i++)
            while (read(fds[i], &ev, sizeof ev) == sizeof ev) {
                if (ev.type == EV_KEY) {
                    if (ev.value == 1) fprintf(stderr, "tecla %d\n", ev.code);
                    if (ev.code == KEY_LEFT && ev.value == 1) paso = -1;
                    if (ev.code == KEY_RIGHT && ev.value == 1) paso = 1;
                    if (ev.code == BTN_SELECT || ev.code == KEY_ESC) sel = ev.value != 0;
                    if (ev.code == BTN_START || ev.code == KEY_ENTER) start = ev.value != 0;
                } else if (ev.type == EV_ABS && (ev.code == ABS_HAT0X || ev.code == ABS_X)) {
                    if (ev.code == ABS_HAT0X && ev.value) paso = ev.value < 0 ? -1 : 1;
                }
            }
        if (sel && start) break;
        if (paso && nfichas > 1) {
            desde = actual;
            actual = (actual + paso + nfichas) % nfichas;
            t_anim = t;
            t_entrada = t;
            sonar(paso > 0 ? "/tmp/pasar-der.wav" : "/tmp/pasar-izq.wav");
            /* direccion de la animacion: el signo de paso */
            if (paso < 0) desde = -desde - 1;   /* codificado: negativo = hacia la izquierda */
        }

        double p = desde == -1 ? 1 : (t - t_anim) / 0.28;
        if (p >= 1 && desde != -1) { desde = -1; p = 1; }

        for (int i = 0; i < W * H; i++) buf[i] = FONDO;
        if (nfichas == 0) {
            texto(&F_MED, 150, 210, "Esperando datos del PC...", TENUE);
        } else if (desde == -1) {
            pintar_ficha(&fichas[actual], 0, t, t - t_entrada);
        } else {
            int izq = desde < 0;
            int vieja = izq ? (int)(-desde - 1) : (int)desde;
            double e = 1 - pow(1 - p, 3);              /* ease-out */
            int off = (int)(W * e);
            int dir = izq ? 1 : -1;
            pintar_ficha(&fichas[vieja], dir * off, t, 10);
            pintar_ficha(&fichas[actual], dir * off - dir * W, t, t - t_entrada);
        }
        if (nfichas) {
            double pos = actual;
            if (desde != -1) {
                int izq = desde < 0;
                int vieja = izq ? (int)(-desde - 1) : (int)desde;
                pos = vieja + (actual - vieja) * suave(p);
            }
            pintar_puntos(pos);
        }

        if (bpp == 16) {
            static unsigned short fila[W];
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    unsigned int c = buf[y * W + x];
                    fila[x] = ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f);
                }
                for (long pg = 0; pg < paginas; pg++)
                    memcpy(fb + (pg * H + y) * fi.line_length, fila, W * 2);
            }
        } else
            for (long pg = 0; pg < paginas; pg++)
                for (int y = 0; y < H; y++)
                    memcpy(fb + (pg * H + y) * fi.line_length, buf + y * W, W * 4);
        usleep(8000);
    }

    memset(fb, 0, tam);
    if (menu_pid) kill(menu_pid, SIGCONT);
    return 0;
}
