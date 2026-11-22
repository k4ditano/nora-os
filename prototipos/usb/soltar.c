/* soltar: lanza un programa totalmente desenganchado de quien lo llama
 * (nueva sesion, sin terminal, padre = init), para que GarlicOS no lo mate
 * al cerrar la app que lo lanzo.   soltar programa [args...] */
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    if (fork() > 0) return 0;          /* el padre vuelve enseguida */
    setsid();
    signal(SIGHUP, SIG_IGN);
    if (fork() > 0) _exit(0);          /* doble fork: ni lider de sesion */
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); }
    execv(argv[1], argv + 1);
    _exit(127);
}
