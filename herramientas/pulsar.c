/* pulsar: pulsa un boton del mando de la consola como si fuera de verdad
 * (escribe el evento en el dispositivo del mando; Nora lo recibe aunque lo
 * tenga agarrado). Para probar desde el PC, y para manejar la consola de lejos.
 *   pulsar A [B X Y L R L2 R2 SELECT START MENU ARRIBA ABAJO IZQUIERDA DERECHA]...
 * Estatico (glibc), como pulsada. */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#ifndef BTN_SOUTH
#define BTN_SOUTH BTN_A
#define BTN_EAST BTN_B
#define BTN_NORTH BTN_X
#define BTN_WEST BTN_Y
#endif

static int mando(void)                     /* el "RG35XX Gamepad" */
{
    DIR *d = opendir("/dev/input");
    struct dirent *e;
    int fd = -1;
    while (d && (e = readdir(d)) && fd < 0) {
        if (strncmp(e->d_name, "event", 5)) continue;
        char ruta[64], nombre[64] = "";
        snprintf(ruta, sizeof ruta, "/dev/input/%s", e->d_name);
        int f = open(ruta, O_WRONLY);
        if (f < 0) continue;
        ioctl(f, EVIOCGNAME(sizeof nombre), nombre);
        if (strstr(nombre, "Gamepad")) fd = f; else close(f);
    }
    if (d) closedir(d);
    return fd;
}

static void evento(int fd, int tipo, int codigo, int valor)
{
    struct input_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = tipo; ev.code = codigo; ev.value = valor;
    if (write(fd, &ev, sizeof ev) != sizeof ev) perror("pulsar");
    memset(&ev, 0, sizeof ev);
    ev.type = EV_SYN; ev.code = SYN_REPORT;
    if (write(fd, &ev, sizeof ev) != sizeof ev) perror("pulsar");
}

int main(int argc, char **argv)
{
    static const struct { const char *n; int c; } B[] = {
        {"A", BTN_SOUTH}, {"B", BTN_EAST}, {"X", BTN_NORTH}, {"Y", BTN_WEST}, {"L", BTN_TL}, {"R", BTN_TR},
        {"L2", BTN_TL2}, {"R2", BTN_TR2}, {"SELECT", BTN_SELECT}, {"START", BTN_START}, {"MENU", BTN_MODE}};
    int fd = mando();
    if (fd < 0) { fprintf(stderr, "pulsar: no encuentro el mando\n"); return 1; }
    for (int i = 1; i < argc; i++) {
        const char *b = argv[i];
        int eje = -1, valor = 0;
        if (!strcmp(b, "ARRIBA")) { eje = ABS_HAT0Y; valor = -1; }
        else if (!strcmp(b, "ABAJO")) { eje = ABS_HAT0Y; valor = 1; }
        else if (!strcmp(b, "IZQUIERDA")) { eje = ABS_HAT0X; valor = -1; }
        else if (!strcmp(b, "DERECHA")) { eje = ABS_HAT0X; valor = 1; }
        if (eje >= 0) {
            evento(fd, EV_ABS, eje, valor); usleep(80000); evento(fd, EV_ABS, eje, 0);
        } else {
            int c = -1;
            for (unsigned k = 0; k < sizeof B / sizeof *B; k++) if (!strcmp(b, B[k].n)) c = B[k].c;
            if (c < 0) { fprintf(stderr, "pulsar: no conozco «%s»\n", b); continue; }
            evento(fd, EV_KEY, c, 1); usleep(80000); evento(fd, EV_KEY, c, 0);
        }
        usleep(150000);
    }
    close(fd);
    return 0;
}
