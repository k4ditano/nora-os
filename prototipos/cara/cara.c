/* cara: una cara sencilla de prueba para la pantalla de la RG35XX.
 *
 * Pinta en /dev/fb0 sin depender de nada mas. Parpadea sola, mira a los lados
 * y cambia de expresion leyendo /tmp/cara (una palabra):
 *     feliz | triste | sorpresa | dormida
 *     echo sorpresa > /tmp/cara
 * Sale al pulsar cualquier boton o con kill.
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

#define W 640
#define H 480

static unsigned int buf[W * H];          /* lo dibujamos aqui y luego se copia */
static volatile int seguir = 1;

static void parar(int s) { (void)s; seguir = 0; }

static double ahora(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static void fondo(unsigned int c)
{
    for (int i = 0; i < W * H; i++) buf[i] = c;
}

/* elipse rellena, centro (cx,cy), radios rx,ry */
static void elipse(double cx, double cy, double rx, double ry, unsigned int c)
{
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    int y0 = (int)(cy - ry), y1 = (int)(cy + ry);
    for (int y = y0; y <= y1; y++) {
        if (y < 0 || y >= H) continue;
        double dy = (y - cy) / ry;
        double k = 1 - dy * dy;
        if (k < 0) continue;
        double dx = rx * sqrt(k);
        int x0 = (int)(cx - dx), x1 = (int)(cx + dx);
        if (x0 < 0) x0 = 0;
        if (x1 >= W) x1 = W - 1;
        for (int x = x0; x <= x1; x++) buf[y * W + x] = c;
    }
}

/* arco grueso: parabola y = cy + curva*(1 - t^2) entre cx-ancho y cx+ancho */
static void boca(double cx, double cy, double ancho, double curva, double grosor, unsigned int c)
{
    for (double x = -ancho; x <= ancho; x += 1) {
        double t = x / ancho;
        double y = cy + curva * (1 - t * t);
        elipse(cx + x, y, grosor, grosor, c);
    }
}

/* hay algun boton pulsado en cualquier /dev/input/event* */
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
        ioctl(fd, EVIOCGRAB, 1);   /* botones solo para la cara: el menu no los ve ni los encola */
        fds[nfds++] = fd;
    }
    closedir(d);
}
/* devuelve el codigo de la ultima tecla pulsada (0 si ninguna) y lo apunta */
static int pulsado(void)
{
    struct input_event ev;
    int tecla = 0;
    for (int i = 0; i < nfds; i++)
        while (read(fds[i], &ev, sizeof ev) == sizeof ev)
            if (ev.type == EV_KEY && ev.value == 1) {
                tecla = ev.code;
                fprintf(stderr, "tecla %d\n", tecla);
            }
    return tecla;
}

/* lanza "suena fichero" sin esperar a que acabe */
static void sonar(const char *wav)
{
    if (fork() == 0) {
        execl("/tmp/suena", "suena", wav, (char *)0);
        _exit(1);
    }
}

/* El menu de GarlicOS (proceso "main") repinta la pantalla sin parar:
 * se congela con SIGSTOP mientras esta la cara y se descongela al salir. */
static int menu_pid = 0;
static void pausar_menu(void)
{
    DIR *d = opendir("/proc");
    struct dirent *e;
    if (!d) return;
    while ((e = readdir(d))) {
        int pid = atoi(e->d_name);
        if (pid <= 1) continue;
        char ruta[64], nombre[32] = "";
        snprintf(ruta, sizeof ruta, "/proc/%d/comm", pid);
        FILE *f = fopen(ruta, "r");
        if (!f) continue;
        if (fgets(nombre, sizeof nombre, f)) nombre[strcspn(nombre, "\n")] = 0;
        fclose(f);
        if (!strcmp(nombre, "main")) { menu_pid = pid; break; }
    }
    closedir(d);
    if (menu_pid) kill(menu_pid, SIGSTOP);
}
/* si el menu se reinicio (otro pid), volver a congelarlo */
static void vigilar_menu(void)
{
    if (menu_pid && kill(menu_pid, 0) == 0) { kill(menu_pid, SIGSTOP); return; }
    menu_pid = 0;
    pausar_menu();
}
static void soltar_menu(void)
{
    if (menu_pid) kill(menu_pid, SIGCONT);
}

static void leer_estado(char *estado, size_t n)
{
    FILE *f = fopen("/tmp/cara", "r");
    if (!f) return;
    char s[32] = "";
    if (fgets(s, sizeof s, f)) {
        s[strcspn(s, " \r\n")] = 0;
        if (s[0]) snprintf(estado, n, "%s", s);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    int segundos = argc > 1 ? atoi(argv[1]) : 0;   /* 0 = para siempre (sale con kill) */
    int tecla_risa = argc > 2 ? atoi(argv[2]) : -1; /* -1 = se rie con cualquier boton */
    setvbuf(stderr, 0, _IONBF, 0);
    signal(SIGINT, parar);
    signal(SIGCHLD, SIG_IGN);                       /* que los "suena" no queden zombis */
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
        fprintf(stderr, "esperaba 640x480 a 16 o 32 bits, hay %ux%ux%u\n", vi.xres, vi.yres, vi.bits_per_pixel);
        return 1;
    }
    long paginas = vi.yres_virtual / vi.yres;
    if (paginas < 1) paginas = 1;
    long tam = (long)fi.line_length * vi.yres_virtual;
    unsigned char *fb = mmap(0, tam, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) { perror("mmap"); return 1; }

    pausar_menu();
    abrir_entradas();
    usleep(300000);
    pulsado();                      /* tirar lo que hubiera en cola (el boton que la lanzo) */

    const unsigned int FONDO = 0x1b1d2b, CARA = 0xffd166, OJO = 0x1b1d2b,
                       BRILLO = 0xffffff, MEJILLA = 0xff8fa3, ZZZ = 0x8aa0ff;
    char estado[32] = "feliz";
    double t0 = ahora(), prox_parpadeo = t0 + 2, ult_estado = 0, risa_hasta = 0;

    while (seguir) {
        double t = ahora();
        if (segundos && t - t0 > segundos) break;
        int tecla = pulsado();
        if (tecla && (tecla_risa < 0 || tecla == tecla_risa)) {
            risa_hasta = t + 1.6;
            sonar("/tmp/risa.wav");
        }
        int riendo = t < risa_hasta;
        if (t - ult_estado > 0.5) { leer_estado(estado, sizeof estado); vigilar_menu(); ult_estado = t; }

        int dormida = !strcmp(estado, "dormida");
        int triste = !strcmp(estado, "triste");
        int sorpresa = !strcmp(estado, "sorpresa");

        /* parpadeo: 0.15 s cada 2-5 s */
        double abierto = 1;
        if (t > prox_parpadeo) {
            double p = (t - prox_parpadeo) / 0.15;
            if (p >= 1) prox_parpadeo = t + 2 + (rand() % 3000) / 1000.0;
            else abierto = fabs(1 - 2 * p);
        }
        if (dormida) abierto = 0.08;

        double mira = dormida ? 0 : 18 * sin(t * 0.7);   /* mirar a los lados */
        double flota = riendo ? 10 * sin(t * 28)            /* se sacude de risa */
                              : 6 * sin(t * 1.6);           /* respirar */

        fondo(FONDO);
        double cx = W / 2, cy = H / 2 + flota;
        elipse(cx, cy, 220, 200, CARA);

        double ry = (sorpresa ? 52 : 42) * abierto;
        for (int lado = -1; lado <= 1; lado += 2) {
            double ex = cx + lado * 80 + mira, ey = cy - 45;
            if (riendo) {                                  /* ojos ^ ^ */
                boca(cx + lado * 80, ey + 10, 32, -26, 7, OJO);
                elipse(cx + lado * 140, cy + 40, 34, 18, MEJILLA);
                continue;
            }
            elipse(ex, ey, sorpresa ? 32 : 26, ry, OJO);
            if (abierto > 0.5) elipse(ex + 8, ey - ry * 0.4, 7, 7, BRILLO);
            if (!triste && !dormida) elipse(cx + lado * 140, cy + 40, 30, 16, MEJILLA);
        }

        if (riendo) {                                      /* boca abierta con lengua */
            double ab = 45 + 12 * fabs(sin(t * 14));
            elipse(cx, cy + 70, 85, ab, OJO);
            for (int y = (int)(cy + 70 - ab - 1); y < cy + 70; y++)   /* tapar la mitad de arriba */
                if (y >= 0 && y < H)
                    for (int x = (int)cx - 90; x <= cx + 90; x++) buf[y * W + x] = CARA;
            elipse(cx, cy + 70 + ab * 0.6, 40, ab * 0.3, 0xe85d75);
        } else if (sorpresa) elipse(cx, cy + 85, 26, 32, OJO);
        else if (dormida) boca(cx, cy + 85, 30, 0, 6, OJO);
        else if (triste) boca(cx, cy + 110, 60, -30, 7, OJO);
        else boca(cx, cy + 70, 70, 35 + 5 * sin(t * 1.6), 7, OJO);

        if (dormida) {                                     /* zzz que suben */
            for (int i = 0; i < 3; i++) {
                double f = fmod(t * 0.4 + i / 3.0, 1);
                double zx = cx + 170 + f * 60, zy = cy - 120 - f * 120, s = 8 + i * 5;
                boca(zx, zy - s, s, 0, 3, ZZZ);
                boca(zx, zy + s, s, 0, 3, ZZZ);
                for (double k = -s; k <= s; k += 1) elipse(zx - k, zy + k, 3, 3, ZZZ);
            }
        }

        /* copiar a todas las paginas: asi da igual cual este mirando el menu */
        if (bpp == 16) {                                   /* RGB565 */
            static unsigned short fila[W];
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    unsigned int c = buf[y * W + x];
                    fila[x] = ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f);
                }
                for (long p = 0; p < paginas; p++)
                    memcpy(fb + (p * H + y) * fi.line_length, fila, W * 2);
            }
        } else
            for (long p = 0; p < paginas; p++)
                for (int y = 0; y < H; y++)
                    memcpy(fb + (p * H + y) * fi.line_length, buf + y * W, W * 4);

        usleep(33000);   /* ~30 fps */
    }

    memset(fb, 0, tam);
    soltar_menu();
    munmap(fb, tam);
    close(fd);
    return 0;
}
