/* ventana: la superficie de la GPU para EGL en la RG35XX.
 * Un ANativeWindow propio (en Android lo da SurfaceFlinger) con buffers de
 * gralloc RGB565 con cache de CPU; cada fotograma terminado se copia a una
 * pagina libre de /dev/fb0 y se cambia de pagina en el refresco (vsync). */
#ifndef NORA_VENTANA_H
#define NORA_VENTANA_H

int ventana_abrir(void);              /* 0 = bien */
void *ventana_nativa(void);           /* para eglCreateWindowSurface */
int ventana_ancho(void);
int ventana_alto(void);
int ventana_tiene_vsync(void);
double ventana_ms_copia(void);        /* media de lo que cuesta pasar un fotograma a la pantalla */
void ventana_cerrar(void);            /* vuelve a la pagina 0 */

#endif
