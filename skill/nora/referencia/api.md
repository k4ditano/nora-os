# La API de Nora (núcleo 0.5)

Todo está en la tabla global `nora`. La app rellena la tabla global `app` con sus
funciones; el núcleo las llama. LuaJIT 2.1 (Lua 5.1 + `bit`, `ffi`, `jit`), con
las librerías estándar (`math`, `string`, `table`, `io`, `os`).

## La app: lo que llama el núcleo

| Función | Cuándo |
|---|---|
| `app.entrar()` | al abrirse la app, después de ejecutar `app.lua` entero (que se ejecuta de nuevo en cada apertura y recarga) |
| `app.actualiza(dt)` | cada fotograma, antes de pintar; `dt` = segundos desde el anterior (~0,016) |
| `app.pinta(t)` | cada fotograma; `t` = segundos desde que se abrió la app. Aquí solo se dibuja |
| `app.boton(b)` | se pulsa un botón (ver *Botones*) |
| `app.suelta(b)` | se suelta |
| `app.barra = true` | (campo, no función) el sistema dibuja encima la barra de estado (hora, batería; ~40 px de alto). Por defecto no |

- Si algo da error, la app se para y sale una pantalla roja con el error y la traza;
  **B vuelve al inicio**. Desde el PC, `nora-puente instalar` y `log` lo enseñan.
- **B debe volver al inicio**: `if b == "B" then nora.volver() end` (es la costumbre
  de todas las apps; la única excepción razonable es usar B para "atrás" dentro de
  un submenú y volver desde el nivel de arriba).
- MENU lo tiene el sistema (menú rápido); la app no lo recibe.
- SELECT+START a la vez cierra Nora entera (para emergencias): no lo uses en juegos.

## Pantalla

- `nora.W`, `nora.H` → 640, 480. El origen (0,0) es arriba a la izquierda; x a la derecha, y hacia abajo.
- 60 fotogramas por segundo como mucho.
- La pantalla es de 16 bits (RGB565) y la GPU trama (dithering) los colores: degradados suaves y colores muy oscuros se ven a escalones o con un tramado fino.
- Va sincronizada con el refresco de la pantalla (~60 Hz): sin cortes. `nora.fps()` → fotogramas del último segundo y ms que cuesta pasar uno a la pantalla (para medir si una app va justa).
- Colores: número `0xRRGGBB` o texto `"#rrggbb"`. Casi todas las funciones aceptan
  detrás un `alfa` de 0 a 1 (por defecto 1).

## Dibujo

| Función | Qué hace |
|---|---|
| `nora.fondo(color)` | color de fondo de la pantalla (se borra con él cada fotograma) |
| `nora.caja(x, y, w, h [, radio, color, alfa])` | rectángulo, con esquinas redondeadas de `radio` px; bordes suaves |
| `nora.circulo(cx, cy, r [, color, alfa])` | círculo relleno |
| `nora.sombra(x, y, w, h [, radio, difuminado, color, alfa])` | sombra suave de una caja (difuminado en px, por defecto 8). Ponla antes que la caja |
| `nora.linea(x1, y1, x2, y2 [, grosor, color, alfa])` | línea de `grosor` px (por defecto 2), en cualquier ángulo |
| `nora.texto(s, x, y [, fuente, color, alfa, alinea])` → ancho | texto; `y` es la parte de arriba de la línea. `alinea`: `"izq"` (defecto), `"centro"`, `"der"` (respecto a `x`) |
| `nora.ancho(s [, fuente])` → px | lo que mide un texto |
| `nora.alto(fuente)` → px | alto de línea de una fuente |
| `nora.icono(nombre, cx, cy [, color, alfa])` → bool | icono centrado en (cx, cy), siempre de 38 px (no se escala); `false` si no existe. Para otros tamaños, una imagen SVG |
| `nora.recorte(x, y, w, h)` / `nora.recorte()` | a partir de aquí solo se pinta dentro de ese rectángulo (listas con scroll); sin argumentos se quita. Se quita solo al acabar el fotograma |

**Fuentes** (Inter): `"peq"` 24 px de línea, `"med"` 38, `"tit"` 54 (negrita), `"gra"` 113 (números grandes).
**Solo caracteres Latin-1**: español completo (á é í ó ú ñ ¿ ¡ ü º ª €…), sin emojis ni
símbolos raros. "…", "–", "—", comillas tipográficas y "•" se cambian solos por "...", "-", comillas normales y "·".
Lo que no exista se ve como "?". Para símbolos, usa iconos o imágenes.

**Iconos**: `gauge smile shield bell music cog gamepad keyboard home cam moon power vol sun
wifi usb bat bolt plus folder terminal star heart clock chart info check times play pause
next prev cube puzzle rocket image code chev desktop send`.

## Imágenes (PNG, JPG, BMP, GIF y SVG)

```lua
local NAVE = assert(nora.imagen("img/nave.svg", {ancho = 64}))   -- una vez, fuera de pinta
function app.pinta(t)
  nora.dibuja(NAVE, x, y, {centro = true, giro = angulo})
end
```

`nora.imagen(ruta [, opciones])` → imagen, o `nil, error`
- `ruta` relativa a la carpeta de la app (o absoluta).
- `opciones`: `ancho`, `alto` (solo SVG: el tamaño al que se dibuja; con uno solo se
  guarda la proporción; sin ninguno, el tamaño del SVG), `pixel = true` (al escalar,
  píxeles nítidos en vez de suavizados: para pixel art).
- Devuelve un objeto con `.w` y `.h` (también `.ancho`, `.alto`).
- Cada imagen se carga **una sola vez** por app (si la pides otra vez con las mismas
  opciones te da la misma), pero cargar es lento: hazlo al principio de `app.lua`.
- Máximo 2048×2048. Un SVG se convierte en píxeles al cargarlo: si lo necesitas a
  dos tamaños muy distintos, cárgalo dos veces con `ancho` distinto (escalar un SVG
  pequeño lo emborrona).
- SVG: formas, trazos, degradados, transformaciones. **Sin texto** (`<text>` no sale), sin filtros ni imágenes incrustadas.

`nora.dibuja(img, x, y [, opciones])`
- Sin opciones: la imagen a su tamaño con la esquina de arriba a la izquierda en (x, y).
- `centro = true`: (x, y) es el centro (lo normal en juegos). O `ox`, `oy` (0..1): el punto de la imagen que cae en (x, y).
- `w`, `h` (o `ancho`, `alto`): tamaño en pantalla. `escala`: multiplica el tamaño.
- `giro`: radianes, en sentido horario, alrededor del punto de origen (`centro`/`ox,oy`).
- `recorte = {x, y, w, h}`: solo ese trozo de la imagen, en píxeles de la imagen (hojas de sprites).
- `espejo = true` (voltear horizontal), `voltea = true` (vertical).
- `alfa` (0..1) y `color` (tinte: multiplica; blanco = sin cambio).

Hoja de sprites de 4 fotogramas de 32×32 en fila:
```lua
local HOJA = nora.imagen("img/andar.png", {pixel = true})
local f = math.floor(t * 10) % 4
nora.dibuja(HOJA, x, y, {recorte = {f * 32, 0, 32, 32}, escala = 3, centro = true, espejo = mira_izq})
```

## Botones

Nombres: `"A" "B" "X" "Y" "L" "R" "L2" "R2" "SELECT" "START" "ARRIBA" "ABAJO" "IZQUIERDA" "DERECHA"`
(y `"MENU"`, que es del sistema). La cruceta llega como botones.

- `app.boton(b)` / `app.suelta(b)`: al pulsar / soltar. Para menús y acciones sueltas (saltar, disparar).
- `nora.pulsado(b)` → bool: ¿está pulsado ahora? Para moverse mientras se mantiene (en `app.actualiza`).
- Se puede pulsar más de uno a la vez (diagonales, correr+saltar).

## Sonido

- `nora.suena(wav [, volumen])`: efecto, se mezcla con lo que suene. `volumen` 0..1.
- `nora.musica(wav [, volumen])`: música de fondo en bucle (una sola; cambia con fundido). `nora.musica()` la para. Es de la app que la pone: al cambiar de app se funde sola (la del menú no suena en tu juego); ponla en `app.entrar` si tu app tiene la suya.
- Rutas relativas a la carpeta de la app. Los sonidos del sistema, siempre así:
  `local SONIDOS = os.getenv("NORA_SONIDOS")` y luego `nora.suena(SONIDOS .. "/mover.wav")`.
  Hay: `abrir mover menu volver pasar-izq pasar-der aviso risa` (`.wav`).
- WAV PCM de 16 bits, mono o estéreo, a cualquier frecuencia. Cada WAV se lee una vez y queda en memoria (cambiar el archivo con la app abierta no cambia el sonido hasta reiniciar Nora): usa nombres nuevos si lo cambias.

## Guardar partidas

- `nora.salvar(nombre, texto)` → true | nil, error: guarda texto en la tarjeta, para esta app. `nora.salvar(nombre, nil)` lo borra.
- `nora.cargar(nombre)` → texto | nil.
- `nombre` sin `/`. Para tablas, con `json` (ver abajo):
  ```lua
  local json = require "json"
  nora.salvar("partida", json.escribe({nivel = 3, vidas = 2}))
  local p = json.lee(nora.cargar("partida") or "") or {nivel = 1, vidas = 3}
  ```
- Se escribe de golpe (nunca queda medio archivo). Sobrevive a apagar, a reinstalar la app y a `instalar --limpiar`; desde el PC se borra con `nora-puente borrar-datos <id>`.

## Tiempo, azar y otros

- `nora.ahora()` → segundos desde que arrancó Nora (con decimales).
- `math.random` ya viene sembrado con la hora.
- `os.time()`, `os.date("%H:%M")`: hora real (en hora de Madrid; el PC la pone en hora).
- `nora.log(...)`: escribe en el registro; desde el PC se ve con `nora-puente log`. Para depurar.
- `nora.volver()`: vuelve al inicio. `nora.abrir(id)`: abre otra app.
- `nora.app()` → id de la app abierta. `nora.carpeta` → carpeta de esta app. `nora.version` → "0.5".
- `nora.bateria()` → porcentaje, cargando (bool).
- `nora.brillo([0..1])`, `nora.volumen([0..1])` → valor actual (con argumento, lo cambian).
- `nora.conectado()` → true, ip | false: si el PC (nora-puente) está conectado.
- `nora.dato(clave)` → texto, versión: datos que manda el PC (p. ej. `"usos.fichas"`).
- `require "modulo"` busca primero en la carpeta de la app y luego en la librería común.

## Librería común

- `json` (`require "json"`): `json.lee(texto)` → valor | nil, error; `json.escribe(valor)` → texto; `json.nulo`.
- `tema` (`require "tema"`): los colores, sonidos y estilo del tema que ha elegido el usuario (abajo).
- `teclado` (`require "teclado"`): escribir texto con la cruceta (abajo).

## El tema del usuario (`require "tema"`)

El usuario elige un tema en Ajustes (colores, fondo, sonidos, música, esquinas, estilo del
inicio). Las apps del sistema lo siguen siempre; **una app normal debería seguirlo** (sus
pantallas de menú, textos y paneles), y un juego puede tener su propio aspecto en la partida
pero conviene que sus menús lo sigan.

```lua
local T = require "tema"
function app.pinta(t)
  T.revisa()                                   -- primero: si cambia el tema, se nota al momento
  T.pinta_fondo()                              -- el color de fondo y la imagen del tema
  T.sombrea(20, 80, 600, 120, 20, 16)          -- sombra (solo si el tema usa sombras)
  nora.caja(20, 80, 600, 120, T.radio(20), T.panel)
  nora.texto("Hola", 40, 100, "med", T.texto)
  nora.texto("subtítulo", 40, 140, "peq", T.tenue)
end
function app.boton(b) T.suena("mover") end     -- los sonidos con la voz del tema
```

- Colores: `T.fondo T.panel T.panel2 T.riel T.texto T.suave T.tenue T.acento T.sobre_acento T.sobre_app T.bien T.aviso T.mal T.info T.velo T.sombra`.
  `panel` para tarjetas, `panel2` para lo elegido o lo que va encima, `riel` para pistas de
  deslizadores y círculos pequeños, `sobre_acento` para lo que va encima de un color (texto
  de un botón de color, icono sobre su círculo). Los temas pueden ser **claros u oscuros**: no
  supongas que el fondo es oscuro; usa siempre la pareja (fondo/panel con texto/tenue).
- `T.radio(r)`: el radio de esquina según el tema (algunos son cuadrados). Úsalo en `nora.caja`.
- `T.sombrea(x, y, w, h, radio, difuminado [, alfa])`: sombra suave, solo si el tema las quiere.
- `T.suena(nombre [, volumen])`: `"mover" "abrir" "volver" "menu" "aviso" "pasar-izq" "pasar-der"`.
- `T.musica()`: pone la música del menú del tema (o ninguna, si el tema o el usuario no quieren).
- `T.color_app(color)`: el color para el icono de una app (algunos temas los ponen todos del color del tema).
- `T.estilo.radio`, `T.estilo.sombras`, `T.estilo.inicio`, `T.estilo.icono`: por si la app quiere adaptarse más.
- `T.nombre`, `T.id`: el tema puesto. `T.lista()`, `T.poner(id)`: los temas y cambiar de tema (lo usa Ajustes).
- Para crear un tema nuevo, ver [temas.md](temas.md).

## Teclado en pantalla (`require "teclado"`)

```lua
local teclado = require "teclado"
teclado.abrir("Tu nombre", "", function(texto) if texto then nombre = texto end end)  -- nil = cancelado
function app.boton(b) if teclado.boton(b) then return end --[[ lo tuyo ]] end
function app.pinta(t) --[[ lo tuyo ]]; teclado.pinta(t) end                        -- lo último
```
Cruceta para moverse, A escribe, X borra, Y mayúsculas, START acepta, B cancela. Hasta 24 caracteres; incluye ñ, tildes, ¿ ¡.

## Una app

```
mi-juego/
  app.lua          obligatorio
  info.lua         return {nombre = "Mi juego", icono = "gamepad", color = 0x8b7cf6, orden = 50, carpeta = "Juegos"}
  img/…            imágenes (opcional)
  sonidos/…        WAV (opcional)
  otro.lua         módulos propios: require "otro"
```
`info.lua`: `nombre` (lo que sale en el inicio), `icono` (de la lista de iconos), `color` (del icono), `orden` (posición en el inicio; menor = antes), `carpeta` (opcional: la carpeta del inicio donde cae la primera vez; usa nombres que ya existan si encajan: `"Juegos"`, `"Herramientas"`, `"Ejemplos"`). El usuario puede moverla luego a otra (modo ordenar del inicio, con Y), y eso manda.
El id de la app (su carpeta) es letras, números, `-` y `_`.
