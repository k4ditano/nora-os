#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>
int main(int argc, char **argv) {
    struct stat st;
    int fd = open("/no/existe", O_RDONLY);
    printf("hola desde bionic: argc=%d fd=%d errno=%d (%s)\n", argc, fd, errno, strerror(errno));
    stat("/system/bin/linker", &st);
    fprintf(stderr, "stderr ok, linker mide %ld bytes, sqrt(2)=%f\n", (long)st.st_size, sqrt(2.0));
    return 0;
}
