# Crear un tema para Nora

Un tema es una carpeta con `tema.lua` y, si quiere, su fondo, sus sonidos y su música.
Se instala desde el PC y el usuario lo elige en Ajustes (o se pone desde el PC).

```sh
nora-puente tema ~/Proyectos/nora-temas/oceano --poner   # instalar y ponerlo
nora-puente temas                                         # los que hay, y cuál está puesto
nora-puente poner-tema noche                              # volver al de siempre
nora-puente captura /tmp/tema.png                         # mirarlo (abre antes el inicio: nora-puente abrir inicio)
```

## tema.lua

Todo es opcional: lo que falte sale del tema Noche (el de siempre). Plantilla completa:

```lua
return {
  nombre = "Océano",
  colores = {
    fondo = 0x0b1d2a,        -- la pantalla
    panel = 0x12304a,        -- tarjetas, cajas
    panel2 = 0x1d4566,       -- lo elegido, lo que va encima de un panel
    riel = 0x1a3c5a,         -- pistas de deslizadores, círculos de las pistas de botones
    texto = 0xe8f4ff,        -- texto principal
    suave = 0xbcd6ea,        -- texto secundario que se tiene que leer
    tenue = 0x7fa3bf,        -- texto de apoyo
    acento = 0x38bdf8,       -- el color del tema
    sobre_acento = 0x0b1d2a, -- lo que va encima del acento (texto de botones, lo elegido relleno)
    sobre_app = 0x0b1d2a,    -- el icono encima del color de cada app (mejor oscuro, también en temas claros)
    bien = 0x34d399, aviso = 0xfbbf24, mal = 0xf87171, info = 0x60a5fa,
    velo = 0x05101a,         -- oscurece la app cuando el sistema se pone encima (menú, avisos)
    sombra = 0x000000,
  },
  fondo = "fondo.svg",       -- imagen de fondo (PNG o SVG), se estira a 640x480
  fondo_alfa = 1,
  sonidos = {mover = "mover.wav", abrir = "abrir.wav", volver = "volver.wav", menu = "menu.wav", aviso = "aviso.wav"},
  musica = "musica.wav",     -- "sistema" (la lo-fi de siempre), un WAV de la carpeta, o false
  musica_volumen = 0.3,
  estilo = {
    radio = 1,               -- multiplica las esquinas: 0 cuadradas, 1 normal, 1.4 muy redondas
    inicio = "cuadricula",   -- "cuadricula" | "lista" | "carrusel"
    icono = "circulo",       -- fondo de los iconos de las apps: "circulo" | "cuadrado" | "nada"
    sombras = true,
    anillo = "borde",        -- lo elegido en el inicio: "borde" (un marco) | "relleno" (la tarjeta del color del tema)
    color_iconos = "app",    -- "app" (cada app su color) | "acento" (todas del color del tema)
  },
}
```

## Consejos

- **Contraste**: `texto` y `tenue` se tienen que leer sobre `fondo`, `panel` y `panel2`;
  `sobre_acento` sobre `acento` y sobre los colores de las apps (en temas claros, `sobre_acento`
  claro; en oscuros, oscuro). Compruébalo con capturas del inicio, Ajustes y el menú rápido (MENU).
- **Fondo**: que no compita con el texto (poco contraste, detalles abajo o en los bordes). Un SVG
  pesa poco y queda nítido; sin `<text>`. La pantalla es de 16 bits: degradados suaves se ven a escalones.
- **Sonidos**: WAV de 16 bits, cortos (< 0,3 s los de moverse). Si falta uno, suena el del sistema.
- **Música**: WAV en bucle; 22050 Hz mono para que no pese (8 s ≈ 350 KB). Se puede generar con Python.
- Ids de tema: letras, números, `-`, `_`. `noche` es el de base y no se puede borrar.
- Los temas de serie están en el repo de Nora OS, en `temas/` (noche, arena, retro): buenos ejemplos.
