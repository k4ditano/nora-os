/* enlace: el servidor TCP de Nora (:7070, un JSON por linea) al que se
 * conecta nora-puente desde el PC. Un solo cliente: si llega otro, sustituye
 * al anterior. Todo sin bloquear, se atiende una vez por fotograma. */
#ifndef ENLACE_H
#define ENLACE_H

#define ENLACE_PUERTO 7070

int enlace_iniciar(int puerto);
/* atiende la red: acepta, lee y manda latidos. Por cada linea recibida llama
 * a linea(texto). Devuelve 1 si se acaba de conectar un cliente, -1 si se
 * acaba de ir, 0 si nada cambia. */
int enlace_paso(double ahora, void (*linea)(const char *));
int enlace_conectado(void);
const char *enlace_quien(void);          /* ip del PC conectado ("" si no hay) */
/* manda una linea (se le pone el \n). Devuelve -1 si no hay cliente. */
int enlace_enviar(const char *linea);
void enlace_cerrar(void);

#endif
