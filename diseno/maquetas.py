#!/usr/bin/env python3
"""Maquetas de Nora OS: pantallas de la consola (640x480, como las pinta) y el
extra de Bahia (a 2x). Mismas fuentes que la consola (Inter) e iconos de
Font Awesome sacados de una Nerd Font."""
import math, os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

AQUI = os.path.dirname(os.path.abspath(__file__))
INTER = os.path.expanduser(os.environ.get("NORA_INTER", "~/.local/share/fonts/inter/"))
ICON = "/usr/share/fonts/TTF/FantasqueSansMNerdFont-Regular.ttf"
MONO = "/usr/share/fonts/TTF/MesloLGLDZNerdFontPropo-Regular.ttf"

def F(peso, tam):
    return ImageFont.truetype(INTER + {"r": "Inter-Regular.ttf", "m": "Inter-Medium.ttf", "s": "Inter-SemiBold.ttf",
                                       "b": "Inter-Bold.ttf"}[peso], tam)
def I(tam): return ImageFont.truetype(ICON, tam)

# iconos (Font Awesome 4 dentro de la Nerd Font)
IC = dict(gauge="", smile="", shield="", bell="", music="", cog="",
          gamepad="", plus="", vol="", sun="", wifi="", usb="",
          bat="", cam="", power="", refresh="", check="", times="",
          folder="", send="", desktop="", moon="", terminal="", home="",
          download="", trash="", bolt="", plug="", microchip="", eye="",
          chev="", cloud="", keyboard="", clock="", info="")

# paleta de la consola (la misma que usos.c)
FONDO, PANEL, TENUE, CLARO, RIEL = "#0e1017", "#1a1d29", "#8b90a5", "#f3f4f8", "#2b2f3f"
VERDE, AMBAR, ROJO, NORA = "#34d399", "#fbbf24", "#f87171", "#8b7cf6"
CLAUDE, CODEX = "#d97757", "#10a37f"

def lienzo(w=640, h=480, c=FONDO):
    im = Image.new("RGB", (w, h), c)
    return im, ImageDraw.Draw(im)

def der(d, x, y, s, f, c):
    d.text((x - d.textlength(s, font=f), y), s, font=f, fill=c)

def centro(d, cx, y, s, f, c):
    d.text((cx - d.textlength(s, font=f) / 2, y), s, font=f, fill=c)

def barra_estado(d, hora="10:37", enlace="usb", bat=82):
    d.text((20, 9), hora, font=F("s", 16), fill=CLARO)
    x = 620
    der(d, x, 9, f"{bat}%", F("m", 15), CLARO); x -= d.textlength(f"{bat}%", font=F("m", 15)) + 10
    der(d, x, 8, IC["bat"], I(18), CLARO); x -= 36
    der(d, x, 8, IC["vol"], I(18), CLARO); x -= 30
    # enlace con el PC: pildora verde
    t = "PC"
    w = d.textlength(t, font=F("s", 13)) + 34
    d.rounded_rectangle((x - w, 7, x, 29), 11, fill="#16352b")
    d.text((x - w + 9, 9), IC["usb" if enlace == "usb" else "wifi"], font=I(15), fill=VERDE)
    d.text((x - w + 26, 10), t, font=F("s", 13), fill=VERDE)

def pista(d, partes):
    """barra de ayuda de abajo: [('A','abrir'), ...]"""
    x = 20
    for tecla, que in partes:
        d.ellipse((x, 448, x + 22, 470), fill=RIEL)
        centro(d, x + 11, 451, tecla, F("b", 12), CLARO)
        d.text((x + 28, 450), que, font=F("m", 14), fill=TENUE)
        x += 28 + d.textlength(que, font=F("m", 14)) + 22

def guardar(im, nombre):
    im.save(os.path.join(AQUI, nombre))
    print("ok", nombre)

# ------------------------------------------------------------------ consola

def logo(d, cx, cy, r, c=NORA):
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=c)
    e = r * 0.16
    for lx in (-0.34, 0.34):
        d.rounded_rectangle((cx + lx * r - e, cy - r * 0.28 - e * 1.6, cx + lx * r + e, cy - r * 0.28 + e * 1.6), e, fill=FONDO)
    d.arc((cx - r * 0.42, cy - r * 0.15, cx + r * 0.42, cy + r * 0.5), 20, 160, fill=FONDO, width=max(3, int(r * 0.1)))

def arranque():
    im, d = lienzo()
    logo(d, 320, 185, 62)
    centro(d, 320, 268, "nora", F("b", 52), CLARO)
    centro(d, 320, 336, "Conectando con el PC…", F("m", 17), TENUE)
    for i, a in enumerate((255, 150, 70)):
        c = tuple(int(int(NORA[k:k + 2], 16) * a / 255 + 14 * (1 - a / 255)) for k in (1, 3, 5))
        d.ellipse((300 + i * 16, 372, 310 + i * 16, 382), fill=c)
    centro(d, 320, 448, "Mantén SELECT al encender para arrancar GarlicOS", F("r", 13), "#555a70")
    guardar(im, "c1-arranque.png")

APPS = [("Usos", "gauge", CLAUDE), ("Cara", "smile", AMBAR), ("Permisos", "shield", VERDE), ("Avisos", "bell", "#60a5fa"),
        ("Música", "music", "#f472b6"), ("Escritorio", "keyboard", NORA), ("Juegos", "gamepad", "#a3e635"), ("Ajustes", "cog", TENUE)]

def inicio(sel=0, im=None):
    if im is None:
        im, d = lienzo()
    d = ImageDraw.Draw(im)
    barra_estado(d)
    d.text((28, 52), "Buenos días", font=F("b", 30), fill=CLARO)
    d.text((28, 92), "Claude te deja un 7 % esta semana · 2 avisos", font=F("m", 15), fill=TENUE)
    tw, th, gx, gy = 132, 132, 18, 16
    x0, y0 = (640 - (4 * tw + 3 * gx)) / 2, 138
    for i, (nom, ic, col) in enumerate(APPS):
        x, y = x0 + (i % 4) * (tw + gx), y0 + (i // 4) * (th + gy)
        if i == sel:
            d.rounded_rectangle((x - 5, y - 5, x + tw + 5, y + th + 5), 26, outline=CLARO, width=3)
        d.rounded_rectangle((x, y, x + tw, y + th), 22, fill=PANEL)
        d.ellipse((x + tw / 2 - 30, y + 20, x + tw / 2 + 30, y + 80), fill=col)
        centro(d, x + tw / 2, y + 33, IC[ic], I(32), FONDO)
        centro(d, x + tw / 2, y + 94, nom, F("s", 16), CLARO)
        if nom == "Avisos":
            d.ellipse((x + tw - 34, y + 12, x + tw - 12, y + 34), fill=ROJO)
            centro(d, x + tw - 23, y + 14, "2", F("b", 13), "white")
    pista(d, [("A", "abrir"), ("B", "atrás"), ("M", "ajustes rápidos")])
    return im

def aviso():
    im = inicio(sel=2)
    capa = Image.new("RGB", im.size, FONDO)
    im = Image.blend(im, capa, 0.45)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((40, 44, 600, 160), 22, fill="#242838", outline="#363b50", width=1)
    d.ellipse((62, 70, 118, 126), fill=CLAUDE)
    centro(d, 90, 84, "\uf069", I(28), "white")
    d.text((136, 66), "Claude Code ha terminado", font=F("s", 20), fill=CLARO)
    d.text((136, 96), "nora-os · 3 archivos cambiados, tests en verde", font=F("m", 15), fill=TENUE)
    d.text((136, 124), "A ver en el PC   ·   B cerrar", font=F("m", 14), fill="#6b7088")
    der(d, 582, 66, "ahora", F("m", 13), "#6b7088")
    guardar(im, "c3-aviso.png")

def permiso():
    im, d = lienzo()
    barra_estado(d)
    d.ellipse((40, 56, 92, 108), fill="#3a2f12")
    centro(d, 66, 68, IC["shield"], I(28), AMBAR)
    d.text((108, 56), "Claude Code pide permiso", font=F("b", 24), fill=CLARO)
    d.text((108, 90), "en la terminal «nora-os» · hace 4 s", font=F("m", 15), fill=TENUE)
    d.rounded_rectangle((40, 130, 600, 250), 16, fill=PANEL)
    d.text((60, 146), "Quiere ejecutar:", font=F("m", 14), fill=TENUE)
    d.text((60, 172), "$ git push origin main", font=ImageFont.truetype(MONO, 22), fill="#e6e8f0")
    d.text((60, 212), "~/Proyectos/nora-os  ·  Bash", font=F("m", 14), fill="#6b7088")
    for i, (t, txt, col, fondo) in enumerate((("A", "Permitir", VERDE, "#123528"), ("B", "Rechazar", ROJO, "#3a1a1d"))):
        x = 40 + i * 290
        d.rounded_rectangle((x, 276, x + 270, 360), 20, fill=fondo, outline=col, width=2)
        d.ellipse((x + 22, 300, x + 58, 336), fill=col)
        centro(d, x + 40, 306, t, F("b", 18), FONDO)
        d.text((x + 74, 302), txt, font=F("b", 26), fill=col)
    d.text((40, 384), "Y  permitir siempre en esta sesión", font=F("m", 15), fill=TENUE)
    d.text((40, 410), "X  ver el diff completo", font=F("m", 15), fill=TENUE)
    pista(d, [("A", "permitir"), ("B", "rechazar"), ("Y", "siempre"), ("X", "detalles")])
    guardar(im, "c4-permiso.png")

def deslizador(d, x, y, w, v, col):
    d.rounded_rectangle((x, y, x + w, y + 10), 5, fill=RIEL)
    d.rounded_rectangle((x, y, x + w * v, y + 10), 5, fill=col)
    d.ellipse((x + w * v - 11, y - 6, x + w * v + 11, y + 16), fill=CLARO)

def ajustes():
    im, d = lienzo()
    barra_estado(d)
    d.text((28, 50), "Ajustes", font=F("b", 28), fill=CLARO)
    filas = [("vol", "Volumen", 0.7, VERDE), ("sun", "Brillo", 0.55, AMBAR),
             ("wifi", "Wi-Fi", "Casa · conectada", None), ("usb", "Conexión con el PC", "Por cable · 3 ms", None),
             ("bat", "Batería", "82 % · 4 h 10 min", None), ("info", "Sistema", "Nora 0.1 · GarlicOS debajo", None)]
    y = 98
    for i, (ic, nom, val, col) in enumerate(filas):
        if i == 0:
            d.rounded_rectangle((20, y - 6, 620, y + 46), 14, fill=PANEL)
        d.text((36, y + 6), IC[ic], font=I(20), fill=TENUE if i else CLARO)
        d.text((72, y + 6), nom, font=F("s", 18), fill=CLARO)
        if isinstance(val, float):
            deslizador(d, 330, y + 15, 230, val, col)
            der(d, 604, y + 8, f"{int(val * 100)}", F("m", 15), TENUE)
        else:
            der(d, 580, y + 8, val, F("m", 15), TENUE)
            d.text((590, y + 7), IC["chev"], font=I(16), fill="#4a4f63")
        y += 56
    pista(d, [("◀▶", "cambiar"), ("B", "volver")])
    guardar(im, "c5-ajustes.png")

def menu_rapido():
    base = Image.open(os.path.join(AQUI, "ref-usos-real.png")).convert("RGB")
    im = Image.blend(base, Image.new("RGB", base.size, FONDO), 0.55)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((20, 250, 620, 470), 26, fill="#20232f", outline="#353a4e")
    d.rounded_rectangle((300, 260, 340, 265), 3, fill="#4a4f63")
    for i, (ic, v, col) in enumerate((("vol", 0.7, VERDE), ("sun", 0.55, AMBAR))):
        y = 290 + i * 46
        d.text((44, y - 6), IC[ic], font=I(20), fill=CLARO)
        deslizador(d, 86, y + 2, 500, v, col)
    for i, (ic, t) in enumerate((("home", "Inicio"), ("cam", "Captura"), ("moon", "Pantalla"), ("power", "Apagar"))):
        x = 40 + i * 142
        d.rounded_rectangle((x, 380, x + 128, 452), 18, fill=PANEL, outline=CLARO if i == 0 else PANEL, width=2)
        centro(d, x + 64, 390, IC[ic], I(24), CLARO)
        centro(d, x + 64, 424, t, F("s", 14), CLARO)
    guardar(im, "c6-menu-rapido.png")

def cara():
    im, d = lienzo(c="#1b1d2b")
    cx, cy = 320, 245
    d.ellipse((cx - 220, cy - 200, cx + 220, cy + 200), fill="#ffd166")
    for s in (-1, 1):
        d.arc((cx + s * 80 - 32, cy - 72, cx + s * 80 + 32, cy - 16), 200, 340, fill="#1b1d2b", width=12)
        d.ellipse((cx + s * 140 - 34, cy + 22, cx + s * 140 + 34, cy + 58), fill="#ff8fa3")
    d.chord((cx - 85, cy + 20, cx + 85, cy + 130), 0, 180, fill="#1b1d2b")
    d.ellipse((cx - 40, cy + 92, cx + 40, cy + 126), fill="#e85d75")
    d.text((470, 60), "ja", font=F("b", 34), fill="#ffd166")
    d.text((520, 100), "ja", font=F("b", 26), fill="#ffd166")
    guardar(im, "c7-cara.png")

# ------------------------------------------------------------------ Bahia (2x)

S = 2
def vidrio(fondo_box, w, h):
    """recorte borroso del escritorio real como fondo"""
    esc = Image.open(os.path.join(AQUI, "ref-escritorio.png")).convert("RGB")
    rec = esc.crop(fondo_box).resize((w, h)).filter(ImageFilter.GaussianBlur(28))
    return rec

def barra_bahia():
    W, Hh = 560 * S, 38 * S
    esc = Image.open(os.path.join(AQUI, "ref-escritorio.png")).convert("RGB")
    im = esc.crop((1920 - 560, 0, 1920, 38)).resize((W, Hh)).filter(ImageFilter.GaussianBlur(6))
    im = Image.blend(im, Image.new("RGB", im.size, "#efe9e4"), 0.35)
    d = ImageDraw.Draw(im)
    x = 18 * S
    tinta = "#1d1d1f"
    # el extra de Nora, pulsado
    w = 74 * S
    d.rounded_rectangle((x - 6 * S, 5 * S, x + w, 33 * S), 7 * S, fill="#ffffff99" if False else "#d9d3cf")
    r = 8 * S
    cxl, cyl = x + 10 * S, 19 * S
    d.ellipse((cxl - r, cyl - r, cxl + r, cyl + r), fill=tinta)
    for lx in (-3, 3):
        d.rounded_rectangle((cxl + lx * S - S, cyl - 4 * S, cxl + lx * S + S, cyl), S, fill="#d9d3cf")
    d.ellipse((cxl + 7 * S, cyl + 4 * S, cxl + 11 * S, cyl + 8 * S), fill="#22c55e")
    d.text((x + 24 * S, 9 * S), "82%", font=F("m", 14 * S), fill=tinta)
    x += w + 18 * S
    d.text((x, 9 * S), "\uf069", font=I(14 * S), fill=tinta); x += 22 * S
    for t, f in (("21%", F("m", 14 * S)), (IC["wifi"], I(15 * S)), ("\U0001F50D", None)):
        if f is None:
            d.text((x, 9 * S), "", font=I(15 * S), fill=tinta); x += 34 * S; continue
        d.text((x, 9 * S), t, font=f, fill=tinta)
        x += d.textlength(t, font=f) + 22 * S
    d.text((x, 9 * S), "", font=I(16 * S), fill=tinta); x += 40 * S
    d.text((x, 9 * S), "dom 22 nov  10:37", font=F("m", 14 * S), fill=tinta)
    guardar(im, "b1-barra.png")

def panel_bahia():
    W, Hh = 380 * S, 812 * S
    m = 16 * S
    total = (W + 2 * m, Hh + 2 * m)
    im = vidrio((1300, 20, 1920, 1080), *total)
    # sombra + hoja de vidrio
    sombra = Image.new("L", total, 0)
    ImageDraw.Draw(sombra).rounded_rectangle((m, m + 6 * S, m + W, m + Hh + 6 * S), 22 * S, fill=110)
    sombra = sombra.filter(ImageFilter.GaussianBlur(14 * S))
    im = Image.composite(Image.new("RGB", total, "#000000"), im, sombra.point(lambda v: v // 2))
    hoja = Image.new("RGBA", total, (0, 0, 0, 0))
    hd = ImageDraw.Draw(hoja)
    hd.rounded_rectangle((m, m, m + W, m + Hh), 22 * S, fill=(246, 244, 242, 205), outline=(255, 255, 255, 170), width=S)
    im = Image.alpha_composite(im.convert("RGBA"), hoja)
    d = ImageDraw.Draw(im)
    tinta, gris, linea = "#1d1d1f", "#6e6e73", "#d6d3d0"
    X, Y = m + 18 * S, m + 18 * S
    # cabecera
    r = 19 * S
    d.ellipse((X, Y, X + 2 * r, Y + 2 * r), fill="#26233a")
    for lx in (-6, 6):
        d.rounded_rectangle((X + r + lx * S - 2 * S, Y + 11 * S, X + r + lx * S + 2 * S, Y + 19 * S), 2 * S, fill="#c4b5fd")
    d.arc((X + r - 9 * S, Y + 15 * S, X + r + 9 * S, Y + 29 * S), 20, 160, fill="#c4b5fd", width=2 * S)
    d.text((X + 2 * r + 12 * S, Y + 1 * S), "Nora", font=F("b", 17 * S), fill=tinta)
    d.text((X + 2 * r + 12 * S, Y + 22 * S), "RG35XX · por cable · 3 ms", font=F("m", 12 * S), fill=gris)
    px = m + W - 18 * S
    t = "Conectada"
    pw = d.textlength(t, font=F("s", 11 * S)) + 24 * S
    d.rounded_rectangle((px - pw, Y + 8 * S, px, Y + 30 * S), 11 * S, fill="#d8f3e4")
    d.ellipse((px - pw + 8 * S, Y + 16 * S, px - pw + 14 * S, Y + 22 * S), fill="#16a34a")
    d.text((px - pw + 18 * S, Y + 12 * S), t, font=F("s", 11 * S), fill="#15803d")
    Y += 54 * S
    # pantalla en vivo
    pw_, ph_ = W - 36 * S, int((W - 36 * S) * 0.75)
    viva = Image.open(os.path.join(AQUI, "ref-usos-real.png")).convert("RGB").resize((pw_, ph_))
    masc = Image.new("L", (pw_, ph_), 0)
    ImageDraw.Draw(masc).rounded_rectangle((0, 0, pw_, ph_), 14 * S, fill=255)
    im.paste(viva, (X, Y), masc)
    d.rounded_rectangle((X + pw_ - 92 * S, Y + 10 * S, X + pw_ - 10 * S, Y + 32 * S), 11 * S, fill="#000000")
    d.ellipse((X + pw_ - 84 * S, Y + 18 * S, X + pw_ - 78 * S, Y + 24 * S), fill="#ef4444")
    d.text((X + pw_ - 72 * S, Y + 13 * S), "en vivo", font=F("s", 11 * S), fill="white")
    Y += ph_ + 10 * S
    d.text((X, Y), "Usos  ·  ficha 1 de 3", font=F("m", 12 * S), fill=gris)
    der(d, m + W - 18 * S, Y, "◀  ▶", F("s", 12 * S), tinta)
    Y += 30 * S
    # deslizadores
    for ic, v in (("vol", 0.7), ("sun", 0.55)):
        d.text((X, Y - 4 * S), IC[ic], font=I(15 * S), fill=tinta)
        x0, x1 = X + 30 * S, m + W - 18 * S
        d.rounded_rectangle((x0, Y, x1, Y + 22 * S), 11 * S, fill="#cdc8c4")
        d.rounded_rectangle((x0, Y, x0 + (x1 - x0) * v, Y + 22 * S), 11 * S, fill="#ffffff")
        d.ellipse((x0 + (x1 - x0) * v - 20 * S, Y + 1 * S, x0 + (x1 - x0) * v, Y + 21 * S), fill="#ffffff", outline=linea)
        Y += 34 * S
    Y += 4 * S
    # botones rapidos
    bw = (W - 36 * S - 3 * 8 * S) / 4
    for i, (ic, t) in enumerate((("cam", "Captura"), ("moon", "Pantalla"), ("send", "Aviso"), ("refresh", "Reiniciar"))):
        x = X + i * (bw + 8 * S)
        d.rounded_rectangle((x, Y, x + bw, Y + 64 * S), 14 * S, fill="#ffffff")
        centro(d, x + bw / 2, Y + 10 * S, IC[ic], I(18 * S), tinta)
        centro(d, x + bw / 2, Y + 40 * S, t, F("m", 11 * S), tinta)
    Y += 78 * S
    d.line((X, Y, m + W - 18 * S, Y), fill=linea, width=S)
    Y += 12 * S
    d.text((X, Y), "Apps en Nora", font=F("s", 13 * S), fill=tinta)
    der(d, m + W - 18 * S, Y, "4 de 8", F("m", 12 * S), gris)
    Y += 28 * S
    filas = [("gauge", "Usos", "En inicio · v0.3", CLAUDE, "act"), ("smile", "Cara", "v0.2", AMBAR, "act"),
             ("shield", "Permisos de Claude", "v0.1 · necesita el hook", VERDE, "act"),
             ("music", "Música", "Sin instalar · 12 KB", "#f472b6", "inst")]
    for ic, nom, sub, col, est in filas:
        d.rounded_rectangle((X, Y, X + 30 * S, Y + 30 * S), 8 * S, fill=col)
        centro(d, X + 15 * S, Y + 6 * S, IC[ic], I(15 * S), "white")
        d.text((X + 42 * S, Y), nom, font=F("s", 13 * S), fill=tinta)
        d.text((X + 42 * S, Y + 17 * S), sub, font=F("m", 11 * S), fill=gris)
        xr = m + W - 18 * S
        if est == "act":
            d.rounded_rectangle((xr - 40 * S, Y + 5 * S, xr, Y + 27 * S), 11 * S, fill="#34c759")
            d.ellipse((xr - 21 * S, Y + 7 * S, xr - 3 * S, Y + 25 * S), fill="white")
        else:
            t = "Instalar"
            tw = d.textlength(t, font=F("s", 12 * S)) + 22 * S
            d.rounded_rectangle((xr - tw, Y + 4 * S, xr, Y + 28 * S), 12 * S, fill="#007aff")
            d.text((xr - tw + 11 * S, Y + 8 * S), t, font=F("s", 12 * S), fill="white")
        Y += 42 * S
    d.text((X + 22 * S, Y), "Añadir app desde una carpeta…", font=F("m", 13 * S), fill="#007aff")
    d.text((X, Y + 1 * S), IC["plus"], font=I(13 * S), fill="#007aff")
    Y += 30 * S
    d.line((X, Y, m + W - 18 * S, Y), fill=linea, width=S)
    Y += 12 * S
    d.text((X, Y), IC["bat"], font=I(14 * S), fill=gris)
    d.text((X + 24 * S, Y), "82 %  ·  Nora 0.1", font=F("m", 12 * S), fill=gris)
    der(d, m + W - 18 * S, Y, "Ajustes de Nora…", F("m", 12 * S), tinta)
    guardar(im.convert("RGB"), "b2-panel.png")

if __name__ == "__main__":
    arranque()
    guardar(inicio(), "c2-inicio.png")
    aviso(); permiso(); ajustes(); menu_rapido(); cara()
    barra_bahia(); panel_bahia()
