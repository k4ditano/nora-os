/* pulsada: dice si un boton esta pulsado AHORA MISMO (sale con 0 si lo esta).
 * Lo usa el arranque para "SELECT al encender = GarlicOS".
 *   pulsada 314        (codigo evdev: 314 = BTN_SELECT, 315 = BTN_START)
 * Estatico (glibc): corre en la raiz de las apps de GarlicOS y en la real. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/input.h>

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    int codigo = atoi(argv[1]);
    if (codigo < 0 || codigo > KEY_MAX) return 2;
    DIR *d = opendir("/dev/input");
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (strncmp(e->d_name, "event", 5)) continue;
        char ruta[64];
        snprintf(ruta, sizeof ruta, "/dev/input/%s", e->d_name);
        int fd = open(ruta, O_RDONLY);
        if (fd < 0) continue;
        unsigned char teclas[KEY_MAX / 8 + 1];
        memset(teclas, 0, sizeof teclas);
        int ok = ioctl(fd, EVIOCGKEY(sizeof teclas), teclas) >= 0;
        close(fd);
        if (ok && (teclas[codigo / 8] & (1 << (codigo % 8)))) return 0;
    }
    return 1;
}
