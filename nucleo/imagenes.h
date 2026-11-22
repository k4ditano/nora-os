/* imagenes: leer PNG/JPG/BMP/GIF/SVG a RGBA y escribir PNG (para las capturas) */
#ifndef NORA_IMAGENES_H
#define NORA_IMAGENES_H

/* carga un archivo a RGBA (malloc; liberar con free). Un .svg se dibuja al
 * tamano pedido (ancho/alto <= 0: el suyo; uno solo: guardando la proporcion);
 * los demas se cargan a su tamano. NULL y error en *err si no se puede. */
unsigned char *imagen_cargar(const char *ruta, int ancho, int alto, int *w, int *h, const char **err);
/* RGBA -> PNG en memoria (malloc); *n bytes */
unsigned char *imagen_png(const unsigned char *rgba, int w, int h, int *n);

#endif
