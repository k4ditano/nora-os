#!/usr/bin/env python3
"""Genera fuentes.h: tipografias Inter suavizadas (alfa de 8 bits) para pintar
en el framebuffer de la RG35XX sin FreeType. Caracteres en Latin-1 (tildes, ñ, ×, ·)."""
from PIL import Image, ImageDraw, ImageFont
import os
D = os.path.expanduser(os.environ.get("NORA_INTER", "~/.local/share/fonts/inter/"))
LATIN = [c for c in range(32, 127)] + [c for c in range(160, 256)]
FUENTES = [  # nombre, fichero, tamaño, caracteres
    ("peq", "Inter-Medium.ttf", 19, LATIN),
    ("med", "Inter-SemiBold.ttf", 30, LATIN),
    ("gra", "Inter-Bold.ttf", 92, [ord(c) for c in " 0123456789%?-"]),
]
out = ["/* generado por fuentes.py: no editar */", "typedef struct { short adv, x, y, w, h; int off; } glifo_t;",
       "typedef struct { int alto, base; const glifo_t *g; const unsigned char *px; } fuente_t;"]
for nom, fic, tam, chars in FUENTES:
    f = ImageFont.truetype(D + fic, tam)
    asc, desc = f.getmetrics()
    glifos, px = {}, bytearray()
    for c in chars:
        ch = chr(c)
        adv = round(f.getlength(ch))
        bb = f.getbbox(ch)
        w, h = max(0, bb[2] - bb[0]), max(0, bb[3] - bb[1])
        if w and h:
            im = Image.new("L", (w, h), 0)
            ImageDraw.Draw(im).text((-bb[0], -bb[1]), ch, font=f, fill=255)
            glifos[c] = (adv, bb[0], bb[1], w, h, len(px))
            px += im.tobytes()
        else:
            glifos[c] = (adv, 0, 0, 0, 0, 0)
    out.append(f"static const unsigned char {nom}_px[] = {{" + ",".join(map(str, px or b"\0")) + "};")
    filas = [f"{{{a},{x},{y},{w},{h},{o}}}" if (c in glifos and (a, x, y, w, h, o) == glifos[c]) else "{0,0,0,0,0,0}"
             for c in range(256) for (a, x, y, w, h, o) in [glifos.get(c, (0, 0, 0, 0, 0, 0))]]
    out.append(f"static const glifo_t {nom}_g[256] = {{" + ",".join(filas) + "};")
    out.append(f"static const fuente_t F_{nom.upper()} = {{ {asc + desc}, {asc}, {nom}_g, {nom}_px }};")
open("fuentes.h", "w").write("\n".join(out) + "\n")
print("ok", sum(len(x) for x in out) // 1024, "KB de cabecera")
