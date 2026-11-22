/* pinta: el dibujo de Nora sobre OpenGL ES 2.0, en pixeles (0,0 arriba a la
 * izquierda). Cajas redondeadas por campo de distancia (bordes suaves a
 * cualquier tamaño; con "difuminado" son sombras) y texto desde un atlas.
 * Todo se acumula en tandas y se manda a la GPU de una vez. */
#ifndef NORA_PINTA_H
#define NORA_PINTA_H
#include <stdint.h>

typedef struct { float r, g, b, a; } color_t;
color_t color_hex(uint32_t rgb, float alfa);

int pinta_iniciar(int ancho, int alto);
void pinta_empezar(color_t fondo);
/* caja redondeada; difuminado 0 = borde nitido (con antialias), >0 = sombra */
void pinta_caja(float x, float y, float w, float h, float radio, color_t c, float difuminado);
/* fuente: 0 peq, 1 med, 2 tit, 3 gra; devuelve el ancho en pixeles */
float pinta_texto(int fuente, float x, float y, const char *utf8, color_t c);
float pinta_ancho(int fuente, const char *utf8);
int pinta_alto_fuente(int fuente);
int pinta_icono(const char *nombre, float cx, float cy, color_t c);   /* centrado; -1 si no existe */
int pinta_fuente(const char *nombre);   /* "peq" "med" "tit" "gra" -> indice, -1 si no */
void pinta_terminar(void);              /* manda lo que quede */
int pinta_dibujados(void);              /* tandas mandadas en el ultimo fotograma */

#endif
