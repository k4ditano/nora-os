-- Retro: fósforo verde de terminal antigua, esquinas casi rectas, el inicio en
-- lista, pitidos propios y una melodía de 8 bits.
return {
  nombre = "Retro",
  colores = {
    fondo = 0x07140b, panel = 0x0d2415, panel2 = 0x1a4a2a, riel = 0x143820,
    texto = 0x9cff9c, suave = 0x7ee07e, tenue = 0x4f9a5f,
    acento = 0x39ff6a, sobre_acento = 0x07140b,
    bien = 0x39ff6a, aviso = 0xd8ff3a, mal = 0xff6a4a, info = 0x5affd8,
    velo = 0x020804, sombra = 0x000000,
  },
  sonidos = {mover = "mover.wav", abrir = "abrir.wav", volver = "volver.wav", menu = "menu.wav", aviso = "aviso.wav"},
  musica = "musica.wav",
  musica_volumen = 0.25,
  estilo = {radio = 0.15, inicio = "lista", icono = "cuadrado", sombras = false, anillo = "relleno", color_iconos = "acento"},
}
