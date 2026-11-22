#!/usr/bin/env python3
"""Genera atlas.h: las letras Inter (suavizadas, alfa de 8 bits) empaquetadas en
UNA textura para la GPU, con la posicion y metricas de cada glifo. Caracteres
Latin-1 (tildes, ñ, ×, ·, €): el nucleo pasa el UTF-8 de Lua a Latin-1."""
import os
from PIL import Image, ImageDraw, ImageFont

D = os.path.expanduser(os.environ.get("NORA_INTER", "~/.local/share/fonts/inter/"))
LATIN = list(range(32, 127)) + list(range(160, 256))
FUENTES = [  # nombre en la API, fichero, tamaño, caracteres
    ("peq", "Inter-Medium.ttf", 19, LATIN),
    ("med", "Inter-SemiBold.ttf", 30, LATIN),
    ("tit", "Inter-Bold.ttf", 44, LATIN),
    ("gra", "Inter-Bold.ttf", 92, [ord(c) for c in " 0123456789%?-.:+/"]),
]
ANCHO = 1024
aqui = os.path.dirname(os.path.abspath(__file__))

glifos = []   # (fuente, codigo, adv, x0, y0, w, h, imagen)
metricas = []
for i, (nom, fic, tam, chars) in enumerate(FUENTES):
    f = ImageFont.truetype(D + fic, tam)
    asc, desc = f.getmetrics()
    metricas.append((nom, asc + desc, asc))
    for c in chars:
        ch = chr(c)
        bb = f.getbbox(ch)
        w, h = max(0, bb[2] - bb[0]), max(0, bb[3] - bb[1])
        im = None
        if w and h:
            im = Image.new("L", (w, h), 0)
            ImageDraw.Draw(im).text((-bb[0], -bb[1]), ch, font=f, fill=255)
        glifos.append((i, c, round(f.getlength(ch)), bb[0], bb[1], w, h, im))

# empaquetado por estantes, de mas alto a mas bajo, con 1 px de margen
orden = sorted(range(len(glifos)), key=lambda k: -glifos[k][6])
x = y = alto_estante = 0
pos = {}
for k in orden:
    w, h = glifos[k][5], glifos[k][6]
    if not w:
        pos[k] = (0, 0)
        continue
    if x + w + 1 > ANCHO:
        x, y = 0, y + alto_estante + 1
        alto_estante = 0
    pos[k] = (x, y)
    x += w + 1
    alto_estante = max(alto_estante, h)
alto = 1
while alto < y + alto_estante + 1:
    alto *= 2
atlas = Image.new("L", (ANCHO, alto), 0)
for k, g in enumerate(glifos):
    if g[7] is not None:
        atlas.paste(g[7], pos[k])

out = ["/* generado por atlas.py: no editar */",
       f"#define ATLAS_W {ANCHO}", f"#define ATLAS_H {alto}", f"#define ATLAS_FUENTES {len(FUENTES)}",
       "typedef struct { short adv, x, y, w, h, u, v; } glifo_t;",
       "typedef struct { const char *nombre; int alto, base; } fuente_t;",
       "static const fuente_t FUENTES[ATLAS_FUENTES] = {" +
       ", ".join(f'{{"{n}", {a}, {b}}}' for n, a, b in metricas) + "};",
       "static const glifo_t GLIFOS[ATLAS_FUENTES][256] = {"]
for i in range(len(FUENTES)):
    tabla = {g[1]: (g[2], g[3], g[4], g[5], g[6], *pos[k]) for k, g in enumerate(glifos) if g[0] == i}
    filas = ["{%d,%d,%d,%d,%d,%d,%d}" % tabla.get(c, (0, 0, 0, 0, 0, 0, 0)) for c in range(256)]
    out.append("  {" + ",".join(filas) + "},")
out.append("};")
out.append("static const unsigned char ATLAS[] = {" + ",".join(map(str, atlas.tobytes())) + "};")
open(os.path.join(aqui, "atlas.h"), "w").write("\n".join(out) + "\n")
atlas.save(os.path.join(aqui, "atlas-vista.png"))
print(f"atlas {ANCHO}x{alto}, {len(glifos)} glifos")
