-- sistema: lo que Nora pinta encima de cualquier app. La barra de estado (si
-- la app la pide con app.barra = true) y el menu rapido, que sube desde abajo
-- al pulsar MENU. Corre en su propio mundo de Lua: si una app falla, esto sigue.
local CLARO, TENUE, PANEL, RIEL = 0xf3f4f8, 0x8b90a5, 0x20232f, 0x2b2f3f
local VERDE, AMBAR, ROJO, NORA = 0x34d399, 0xfbbf24, 0xf87171, 0x8b7cf6
local SONIDOS = os.getenv("NORA_SONIDOS") or "/tmp/nora/sonidos"

local abierto, t_abre, t_cierra = false, -10, -10
local fila, col = 1, 1                     -- fila 1 volumen, 2 brillo, 3 botones
local estado = {t = -10}
local BOTONES = {{"home", "Inicio", "inicio"}, {"cog", "Ajustes", "ajustes"}, {"power", "Salir", "salir"}}

local function suave(p) p = math.max(0, math.min(1, p)); return 1 - (1 - p) ^ 3 end
local function suena(n) nora.suena(SONIDOS .. "/" .. n .. ".wav") end

local function refresca(t)                 -- leer el hardware cada 2 s, no cada fotograma
  if t - estado.t < 2 then return end
  estado.t = t
  estado.bat, estado.cargando = nora.bateria()
  estado.enlace = nora.enlace()
  estado.vol = nora.volumen()
  estado.brillo = nora.brillo()
end

local function barra(t)
  nora.texto(os.date("%H:%M"), 20, 8, "peq", CLARO)
  local x = 620
  local txt = estado.bat >= 0 and (estado.bat .. "%") or "--"
  x = x - nora.texto(txt, x, 8, "peq", CLARO, 1, "der") - 18
  nora.icono(estado.cargando and "bolt" or "bat", x, 20, estado.bat < 20 and ROJO or CLARO, 0.9)
  x = x - 36
  nora.icono("vol", x, 20, CLARO, estado.vol > 0 and 0.9 or 0.35)
  x = x - 28
  if estado.enlace then                    -- pildora verde: conectada al PC
    local t2 = estado.enlace == "cable" and "PC" or "Wi-Fi"
    local w = nora.ancho(t2, "peq") + 46
    nora.caja(x - w, 6, w, 26, 13, 0x16352b)
    nora.icono(estado.enlace == "cable" and "usb" or "wifi", x - w + 18, 19, VERDE, 1)
    nora.texto(t2, x - w + 36, 8, "peq", VERDE)
  end
end

local function deslizador(x, y, w, v, color, elegido)
  nora.caja(x, y, w, 10, 5, RIEL)
  nora.caja(x, y, math.max(10, w * v), 10, 5, color)
  if elegido then nora.sombra(x + w * v - 13, y - 8, 26, 26, 13, 6, color, 0.6) end
  nora.circulo(x + w * v, y + 5, 11, CLARO)
end

local function menu(t)
  local p = abierto and suave((t - t_abre) / 0.28) or 1 - suave((t - t_cierra) / 0.22)
  if p <= 0 then return end
  nora.caja(-2, -2, 644, 484, 0, 0x0e1017, 0.55 * p)        -- oscurece la app
  local y = 480 - 240 * p
  nora.sombra(20, y + 6, 600, 260, 26, 20, 0x000000, 0.6 * p)
  nora.caja(20, y, 600, 260, 26, PANEL)
  nora.caja(300, y + 10, 40, 5, 3, 0x4a4f63)
  local filas = {{"vol", estado.vol, VERDE}, {"sun", estado.brillo, AMBAR}}
  for i, f in ipairs(filas) do
    local fy = y + 40 + (i - 1) * 46
    if fila == i then nora.caja(30, fy - 16, 580, 42, 14, 0x2a2e3e) end
    nora.icono(f[1], 58, fy + 5, CLARO, fila == i and 1 or 0.7)
    deslizador(90, fy, 490, f[2], f[3], fila == i)
  end
  for i, b in ipairs(BOTONES) do
    local bx, by = 40 + (i - 1) * 190, y + 140
    local elegido = fila == 3 and col == i
    if elegido then nora.sombra(bx - 4, by - 4, 178, 88, 22, 8, NORA, 0.5) end
    nora.caja(bx, by, 170, 80, 18, elegido and 0x343850 or 0x1a1d29)
    nora.icono(b[1], bx + 85, by + 28, CLARO, 1)
    nora.texto(b[2], bx + 85, by + 46, "peq", CLARO, 1, "centro")
  end
end

function app.entrar() refresca(0) end

function app.pinta(t, con_barra, id)
  refresca(t)
  if con_barra then barra(t) end
  menu(t)
end

function app.abierto() return abierto end

function app.boton(b)
  if b == "MENU" or (abierto and b == "B") then
    abierto = not abierto
    if abierto then t_abre = nora.ahora(); fila, col = 1, 1; estado.t = -10; suena("menu")
    else t_cierra = nora.ahora(); suena("volver") end
    return true
  end
  if not abierto then return false end
  if b == "ARRIBA" then fila = math.max(1, fila - 1); suena("mover")
  elseif b == "ABAJO" then fila = math.min(3, fila + 1); suena("mover")
  elseif b == "IZQUIERDA" or b == "DERECHA" then
    local d = b == "DERECHA" and 1 or -1
    if fila == 1 then estado.vol = nora.volumen(math.max(0, math.min(1, estado.vol + d * 0.1))); suena("mover")
    elseif fila == 2 then estado.brillo = nora.brillo(math.max(0.02, math.min(1, estado.brillo + d * 0.1)))
    else col = math.max(1, math.min(#BOTONES, col + d)); suena("mover") end
  elseif b == "A" and fila == 3 then
    local accion = BOTONES[col][3]
    abierto = false; t_cierra = nora.ahora()
    suena("abrir")
    if accion == "salir" then nora.salir() else nora.abrir(accion) end
  end
  return true
end
