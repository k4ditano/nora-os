-- Noche: el tema de siempre de Nora. Sirve de plantilla: trae todos los campos.
-- Un tema puede dejar fuera lo que quiera; lo que falte sale de este.
return {
  nombre = "Noche",
  colores = {
    fondo = 0x0e1017,        -- la pantalla
    panel = 0x1a1d29,        -- tarjetas, cajas
    panel2 = 0x2a2e3e,       -- tarjeta elegida o encima de otra
    riel = 0x2b2f3f,         -- pistas de deslizadores, círculos de las pistas de botones
    texto = 0xf3f4f8,        -- texto principal
    suave = 0xc9ccd8,        -- texto secundario que se tiene que leer bien
    tenue = 0x8b90a5,        -- texto de apoyo, pistas
    acento = 0x8b7cf6,       -- el color del tema: lo elegido, anillos, botones principales
    sobre_acento = 0x0e1017, -- lo que va encima del acento (texto de botones, lo elegido relleno)
    sobre_app = 0x0e1017,    -- el icono encima del color de cada app (oscuro también en temas claros)
    bien = 0x34d399, aviso = 0xfbbf24, mal = 0xf87171, info = 0x60a5fa,
    velo = 0x0e1017,         -- lo que oscurece la app cuando el sistema se pone encima
    sombra = 0x000000,
  },
  fondo = nil,               -- imagen de fondo, relativa a esta carpeta (PNG o SVG; se estira a 640x480)
  fondo_alfa = 1,
  sonidos = {},              -- {mover = "mover.wav", abrir = ..., volver = ..., menu = ..., aviso = ...}
  musica = "sistema",        -- "sistema" (la lo-fi de siempre), un WAV de esta carpeta, o false (sin música)
  musica_volumen = 0.35,
  estilo = {
    radio = 1,               -- multiplica el radio de las esquinas (0 = cuadradas)
    inicio = "cuadricula",   -- "cuadricula" | "lista" | "carrusel"
    icono = "circulo",       -- fondo de los iconos de las apps: "circulo" | "cuadrado" | "nada"
    sombras = true,          -- sombras suaves bajo los paneles
    anillo = "borde",        -- cómo se marca lo elegido en el inicio: "borde" | "relleno"
    color_iconos = "app",    -- "app" (el color de cada app) | "acento" (todos el del tema)
  },
}
