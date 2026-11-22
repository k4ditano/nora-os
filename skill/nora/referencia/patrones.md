# Patrones para apps y juegos de Nora

## Esqueleto de juego con estados

```lua
local W, H = nora.W, nora.H
local estado = "menu"            -- "menu" | "juego" | "pausa" | "fin"
local jugador, enemigos, puntos

local function empezar()
  jugador = {x = W / 2, y = H / 2, vx = 0, vy = 0}
  enemigos, puntos = {}, 0
  estado = "juego"
end

function app.boton(b)
  if b == "B" and estado ~= "pausa" then nora.volver(); return end
  if estado == "menu" or estado == "fin" then
    if b == "A" or b == "START" then empezar() end
  elseif estado == "juego" then
    if b == "START" then estado = "pausa" end
    if b == "A" then --[[ saltar, disparar ]] end
  elseif estado == "pausa" then
    if b == "START" or b == "A" then estado = "juego" elseif b == "B" then estado = "menu" end
  end
end

function app.actualiza(dt)
  if estado ~= "juego" then return end
  dt = math.min(dt, 1 / 20)      -- si un fotograma tarda mucho, que nada atraviese paredes
  -- mover, chocar, puntuar...
end

function app.pinta(t)
  nora.fondo(0x0e1017)
  -- dibujar mundo, luego marcador, luego la capa del estado (menu / pausa / fin) encima
end
```

## Movimiento

- **Velocidades en píxeles por segundo**, siempre multiplicadas por `dt`: `x = x + vx * dt`.
- Cruceta mantenida con `nora.pulsado` dentro de `actualiza`; acciones sueltas en `app.boton`.
- Diagonal normalizada: si `dx ~= 0 and dy ~= 0` multiplica por `0.7071`.
- Gravedad y salto: `vy = vy + 1400 * dt; y = y + vy * dt`; al pulsar A en el suelo `vy = -560`.
- Suavizar (cámara, menús): `x = x + (destino - x) * math.min(1, dt * 10)`.

## Choques

```lua
local function circulos(a, b, r) return (a.x - b.x)^2 + (a.y - b.y)^2 < r * r end
local function cajas(a, b)        -- {x, y, w, h}, esquina arriba-izquierda
  return a.x < b.x + b.w and b.x < a.x + a.w and a.y < b.y + b.h and b.y < a.y + a.h
end
```
Al borrar de una lista mientras se recorre, recórrela **al revés**: `for i = #l, 1, -1 do … table.remove(l, i) end`.

## Rejillas (puzles, Snake, tetris, mapas)

```lua
local C = 24                                   -- tamaño de celda: 640/24 ≈ 26 columnas, 480/24 = 20 filas
local cols, filas = math.floor(W / C), math.floor(H / C)
local mapa = {}                                -- mapa[f][c]
for f = 1, filas do mapa[f] = {} for c = 1, cols do mapa[f][c] = 0 end end
-- dibujar: nora.caja((c - 1) * C, (f - 1) * C, C - 2, C - 2, 4, color)
```
Juegos por turnos o de paso fijo (Snake): acumula tiempo y avanza cada `paso` segundos:
```lua
acum = acum + dt
while acum >= paso do acum = acum - paso; avanzar() end
```

## Cámara y mundos más grandes que la pantalla

Guarda `camx, camy`, dibuja cada cosa en `x - camx, y - camy`, y no dibujes lo que
queda fuera (`if sx > -64 and sx < W + 64 then …`).

## Menús y listas

```lua
local sel = 1
function app.boton(b)
  if b == "ABAJO" then sel = sel % #items + 1; nora.suena(SONIDOS .. "/mover.wav")
  elseif b == "ARRIBA" then sel = (sel - 2) % #items + 1; nora.suena(SONIDOS .. "/mover.wav")
  elseif b == "A" then items[sel].accion() end
end
```
Elemento elegido: caja con más luz y un borde (`nora.caja` dos veces: la de fuera 3 px mayor y blanca).
Listas largas: `nora.recorte(x, y, w, h)` + desplazamiento suavizado hacia el elegido.

## Aspecto Nora

- Fondo oscuro azulado `0x0e1017`; tarjetas `0x161a24`–`0x1d2230` con radio 14–20.
- Texto principal `0xf3f4f6`, secundario `0x9ca3af`, tenue `0x6b7280`.
- Color de Nora `0x8b7cf6` (violeta); acentos `0xffd166` (amarillo), `0x34d399` (verde), `0xf87171` (rojo), `0x60a5fa` (azul), `0xf59e0b` (naranja).
- Márgenes de 24–32 px; título en `"tit"` arriba a la izquierda (x=28, y=36) o centrado.
- Pistas de botones abajo en `"peq"`: `"A jugar · B salir"`.
- Animaciones cortas (0,15–0,3 s) con salida suave: `local function suave(p) p = math.max(0, math.min(1, p)); return 1 - (1 - p)^3 end`.

## Rendimiento (ARM Cortex-A9 a 1 GHz, GPU PowerVR SGX544)

- Miles de cajas e imágenes por fotograma van bien; lo caro es Lua haciendo mucho por cosa.
- No crees tablas nuevas en cada fotograma si puedes reutilizarlas (el recolector de basura da tirones).
- Carga imágenes y sonidos al principio, nunca dentro de `pinta`.
- Seguidas, las imágenes de una misma textura se dibujan de una vez: agrupa por imagen (todas las rocas, luego todas las estrellas) y usa hojas de sprites.
- `string.format` y concatenar en `pinta` está bien para unos pocos textos; no para cientos.

## Imágenes: de dónde sacarlas

- **SVG escrito a mano**: lo más rápido para naves, iconos, formas, personajes sencillos. Usa `viewBox`, formas (`path`, `circle`, `rect`, `polygon`), `fill`/`stroke`. Nada de `<text>`.
- **PNG generado con Python (Pillow)**: pixel art, texturas con ruido, hojas de sprites. Dibuja pequeño (16–32 px) y carga con `{pixel = true}` para escalar nítido.
- **Herramienta de imágenes del agente** si la hay: genera la imagen y recórtala/redimensiónala con Pillow antes de meterla en la app (máximo 2048 px; mejor pequeñas).

## Sonidos: de dónde sacarlos

Genéralos con Python (`wave` + `struct`): ondas cuadradas/seno con envolvente que cae
dan efectos de videojuego (monedas, saltos, golpes). 22050 Hz, mono, 16 bits, < 1 s.
```python
import wave, struct, math
fr = 22050; w = wave.open("sonidos/salto.wav", "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(fr)
n = int(fr * 0.15); datos = b""
for i in range(n):
    f = 300 + 600 * i / n                     # sube de tono
    datos += struct.pack("<h", int(8000 * (1 - i / n) * (1 if math.sin(2 * math.pi * f * i / fr) > 0 else -1)))
w.writeframes(datos); w.close()
```
