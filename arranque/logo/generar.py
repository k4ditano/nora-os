#!/usr/bin/env python3
"""El logo de arranque de Nora en los dos formatos que hacen falta:
  - boot_logo.bmp.gz: lo enseña el arranque de Anbernic al encender (en /misc).
    Se copia la cabecera del BMP original de la consola (640x480x24, 138 bytes)
    y solo se cambian los pixeles, para que el cargador lo lea seguro.
  - arranque.565: el mismo en crudo RGB565, para volcarlo a /dev/fb0 en cuanto
    empieza arranque.sh, mientras se levanta la GPU."""
import gzip, os
from PIL import Image, ImageDraw, ImageFont

AQUI = os.path.dirname(os.path.abspath(__file__))
INTER = os.path.expanduser(os.environ.get("NORA_INTER", "~/.local/share/fonts/inter/"))
FONDO, NORA, CLARO = (14, 16, 23), (139, 124, 246), (243, 244, 248)

im = Image.new("RGB", (640, 480), FONDO)
d = ImageDraw.Draw(im)
cx, cy, r = 320, 195, 62
d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=NORA)
e = r * 0.16
for lx in (-0.34, 0.34):
    d.rounded_rectangle((cx + lx * r - e, cy - r * 0.28 - e * 1.6, cx + lx * r + e, cy - r * 0.28 + e * 1.6), e, fill=FONDO)
d.arc((cx - r * 0.42, cy - r * 0.15, cx + r * 0.42, cy + r * 0.5), 20, 160, fill=FONDO, width=int(r * 0.1))
f = ImageFont.truetype(INTER + "Inter-Bold.ttf", 52)
d.text((cx - d.textlength("nora", font=f) / 2, 278), "nora", font=f, fill=CLARO)
im.save(os.path.join(AQUI, "logo-nora.png"))

# BMP con la cabecera original: filas de abajo arriba, en BGR
cab = gzip.open(os.path.join(AQUI, "boot_logo-garlic-original.bmp.gz")).read()[:138]
pix = bytearray()
for y in range(479, -1, -1):
    for x in range(640):
        r_, g, b = im.getpixel((x, y))
        pix += bytes((b, g, r_))
with gzip.open(os.path.join(AQUI, "boot_logo.bmp.gz"), "wb", 9) as o:
    o.write(cab + bytes(pix))

# RGB565 en crudo, de arriba abajo, little endian
raw = bytearray()
for y in range(480):
    for x in range(640):
        r_, g, b = im.getpixel((x, y))
        v = ((r_ >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        raw += bytes((v & 255, v >> 8))
open(os.path.join(AQUI, "arranque.565"), "wb").write(raw)
print("logo generado:", os.path.getsize(os.path.join(AQUI, "boot_logo.bmp.gz")), "bytes el bmp.gz")
