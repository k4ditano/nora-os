/* enlace: el servidor TCP de Nora para nora-puente. Ver enlace.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "enlace.h"

#define LATIDO 2.0          /* cada cuanto se manda un latido */
#define SILENCIO 6.0        /* sin oir nada en este tiempo, el PC se da por ido */
#define MAX_LINEA (4 << 20) /* una linea (una app en base64) no pasa de 4 MB */
#define MAX_SALIDA (8 << 20)

static int escucha = -1, cli = -1;
static char quien[64];
static double t_oido, t_latido;
static char *ent, *sal;                  /* lo recibido sin linea completa, lo pendiente de mandar */
static int nent, cap_ent, nsal, cap_sal;

static int crece(char **b, int *cap, int hace_falta, int max)
{
    if (hace_falta <= *cap) return 0;
    if (hace_falta > max) return -1;
    int n = *cap ? *cap : 4096;
    while (n < hace_falta) n *= 2;
    char *nb = realloc(*b, n);
    if (!nb) return -1;
    *b = nb;
    *cap = n;
    return 0;
}

int enlace_iniciar(int puerto)
{
    escucha = socket(AF_INET, SOCK_STREAM, 0);
    if (escucha < 0) { perror("nora: enlace socket"); return -1; }
    int uno = 1;
    setsockopt(escucha, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(puerto);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(escucha, (struct sockaddr *)&a, sizeof a) || listen(escucha, 2)) {
        perror("nora: enlace bind/listen");
        close(escucha);
        escucha = -1;
        return -1;
    }
    fcntl(escucha, F_SETFL, O_NONBLOCK);
    fprintf(stderr, "nora: esperando a nora-puente en :%d\n", puerto);
    return 0;
}

static void soltar_cliente(const char *porque)
{
    if (cli < 0) return;
    fprintf(stderr, "nora: el PC %s se va (%s)\n", quien, porque);
    close(cli);
    cli = -1;
    quien[0] = 0;
    nent = nsal = 0;
}

int enlace_conectado(void) { return cli >= 0; }
const char *enlace_quien(void) { return quien; }

static void vaciar(void)
{
    while (cli >= 0 && nsal > 0) {
        int n = send(cli, sal, nsal, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (n > 0) {
            memmove(sal, sal + n, nsal - n);
            nsal -= n;
        } else {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return;
            soltar_cliente("no se le puede escribir");
        }
    }
}

int enlace_enviar(const char *linea)
{
    if (cli < 0) return -1;
    int n = strlen(linea);
    if (crece(&sal, &cap_sal, nsal + n + 1, MAX_SALIDA)) {
        soltar_cliente("no lee lo que se le manda");
        return -1;
    }
    memcpy(sal + nsal, linea, n);
    sal[nsal + n] = '\n';
    nsal += n + 1;
    vaciar();
    return 0;
}

int enlace_paso(double ahora, void (*linea)(const char *))
{
    int cambio = 0;
    if (escucha < 0) return 0;
    struct sockaddr_in a;
    socklen_t la = sizeof a;
    int nuevo = accept(escucha, (struct sockaddr *)&a, &la);
    if (nuevo >= 0) {
        if (cli >= 0) soltar_cliente("llega otro");
        cli = nuevo;
        fcntl(cli, F_SETFL, O_NONBLOCK);
        int uno = 1;
        setsockopt(cli, IPPROTO_TCP, TCP_NODELAY, &uno, sizeof uno);
        snprintf(quien, sizeof quien, "%s", inet_ntoa(a.sin_addr));
        t_oido = t_latido = ahora;
        nent = nsal = 0;
        fprintf(stderr, "nora: conectado el PC %s\n", quien);
        cambio = 1;
    }
    if (cli < 0) return cambio;

    for (;;) {                                     /* leer todo lo que haya */
        if (crece(&ent, &cap_ent, nent + 16384, MAX_LINEA + 16384)) { soltar_cliente("linea demasiado larga"); return -1; }
        int n = recv(cli, ent + nent, cap_ent - nent, MSG_DONTWAIT);
        if (n > 0) { nent += n; t_oido = ahora; continue; }
        if (n == 0) { soltar_cliente("ha cerrado"); return -1; }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
        soltar_cliente("error al leer");
        return -1;
    }
    int ini = 0;                                    /* repartir las lineas completas */
    for (int i = 0; i < nent && cli >= 0; i++) {
        if (ent[i] != '\n') continue;
        ent[i] = 0;
        if (i > ini && ent[i - 1] == '\r') ent[i - 1] = 0;
        if (ent[ini]) linea(ent + ini);
        ini = i + 1;
    }
    if (cli < 0) return -1;                         /* se fue mientras se atendia */
    if (ini) { memmove(ent, ent + ini, nent - ini); nent -= ini; }
    if (nent > MAX_LINEA) { soltar_cliente("linea demasiado larga"); return -1; }

    if (ahora - t_oido > SILENCIO) { soltar_cliente("no da señales"); return -1; }
    if (ahora - t_latido >= LATIDO) {
        t_latido = ahora;
        enlace_enviar("{\"tipo\":\"latido\"}");
    }
    vaciar();
    return cli < 0 ? -1 : cambio;
}

void enlace_cerrar(void)
{
    soltar_cliente("Nora se cierra");
    if (escucha >= 0) close(escucha);
    escucha = -1;
}
