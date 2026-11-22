/* imagenes: ver imagenes.h. stb_image / stb_image_write (dominio publico) y
 * nanosvg (zlib), en terceros/cabeceras. */
#include "imagenes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS          /* bionic de Android 4.4 no tiene TLS de compilador */
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_NO_STDIO_LARGE
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

static int es_svg(const char *ruta)
{
    size_t n = strlen(ruta);
    return n > 4 && !strcasecmp(ruta + n - 4, ".svg");
}

unsigned char *imagen_cargar(const char *ruta, int ancho, int alto, int *w, int *h, const char **err)
{
    *err = 0;
    if (!es_svg(ruta)) {
        int c;
        unsigned char *p = stbi_load(ruta, w, h, &c, 4);
        if (!p) *err = stbi_failure_reason();
        return p;
    }
    NSVGimage *svg = nsvgParseFromFile(ruta, "px", 96);
    if (!svg) { *err = "no se puede leer el SVG"; return 0; }
    if (svg->width <= 0 || svg->height <= 0) { nsvgDelete(svg); *err = "SVG sin tamano"; return 0; }
    float e;
    if (ancho > 0 && alto <= 0) e = ancho / svg->width;
    else if (alto > 0 && ancho <= 0) e = alto / svg->height;
    else if (ancho > 0) e = fminf(ancho / svg->width, alto / svg->height);
    else e = 1;
    *w = ancho > 0 ? ancho : (int)ceilf(svg->width * e);
    *h = alto > 0 ? alto : (int)ceilf(svg->height * e);
    if (*w > 2048 || *h > 2048) { nsvgDelete(svg); *err = "demasiado grande (2048 px como mucho)"; return 0; }
    unsigned char *p = calloc((size_t)*w * *h, 4);
    NSVGrasterizer *r = nsvgCreateRasterizer();
    if (!p || !r) { free(p); nsvgDelete(svg); *err = "sin memoria"; return 0; }
    /* centrado si la proporcion no casa */
    float dx = (*w - svg->width * e) / 2, dy = (*h - svg->height * e) / 2;
    nsvgRasterize(r, svg, dx, dy, e, p, *w, *h, *w * 4);
    nsvgDeleteRasterizer(r);
    nsvgDelete(svg);
    return p;
}

unsigned char *imagen_png(const unsigned char *rgba, int w, int h, int *n)
{
    stbi_write_png_compression_level = 6;
    return stbi_write_png_to_mem(rgba, w * 4, w, h, 4, n);
}
