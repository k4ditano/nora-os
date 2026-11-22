-- sistema: lo que Nora pinta encima de cualquier app. La barra de estado (si
-- la app la pide con app.barra = true) y el menu rapido, que sube desde abajo
-- al pulsar MENU, y los avisos, que caen desde arriba encima de todo (6 s; B los
-- quita, A hace su accion en el PC). Corre en su propio mundo de Lua: si una app
-- falla, esto sigue.
-- Tambien atiende al PC (nora-puente): el nucleo le pasa cada linea JSON que
-- llega en app.mensaje(linea).
local json = require "json"
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
  estado.puente = nora.conectado()
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
  if estado.enlace then                    -- pildora: verde si nora-puente esta conectado
    local t2 = estado.enlace == "cable" and "PC" or "Wi-Fi"
    local w = nora.ancho(t2, "peq") + 46
    local col = estado.puente and VERDE or TENUE
    nora.caja(x - w, 6, w, 26, 13, estado.puente and 0x16352b or 0x262a38)
    nora.icono(estado.enlace == "cable" and "usb" or "wifi", x - w + 18, 19, col, 1)
    nora.texto(t2, x - w + 36, 8, "peq", col)
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

---------------------------------------------------------------- avisos
local DURA = 6                             -- segundos que se queda un aviso
local cola, actual, t_aviso, t_fuera = {}, nil, -10, nil
local historial, sin_leer, n_aviso = {}, 0, 0

-- corta un texto en hasta `n` lineas de `w` px (por palabras; la ultima con «…»)
local function lineas(s, fuente, w, n)
  local out, linea = {}, ""
  for palabra in s:gsub("%s+", " "):gmatch("%S+") do
    local prueba = linea == "" and palabra or (linea .. " " .. palabra)
    if nora.ancho(prueba, fuente) <= w then linea = prueba
    else
      if linea ~= "" then out[#out + 1] = linea end
      linea = palabra
      if #out == n then break end
    end
  end
  if #out < n and linea ~= "" then out[#out + 1] = linea; linea = "" end
  local ultima = out[#out]
  if ultima and (linea ~= "" or nora.ancho(ultima, fuente) > w) then
    while #ultima > 0 and nora.ancho(ultima .. "…", fuente) > w do
      ultima = ultima:sub(1, -2):gsub("[\128-\191]+$", ""):gsub("[\192-\255]$", "")
    end
    out[#out] = ultima .. "…"
  end
  return out
end

local function guarda_avisos()             -- para la app Avisos y la insignia del inicio
  nora.guardar("avisos.lista", json.escribe(historial))
  nora.guardar("avisos.sin_leer", tostring(sin_leer))
end

local function siguiente()
  actual = table.remove(cola, 1)
  t_fuera = nil
  if actual then t_aviso = nora.ahora(); suena("aviso") end
end

local function quitar_aviso(t)
  if actual and not t_fuera then t_fuera = t or nora.ahora() end
end

-- un aviso nuevo: {titulo, texto, app, accion}; lo puede mandar el PC o el propio sistema
local function avisar(a)
  n_aviso = n_aviso + 1
  local av = {id = a.id or n_aviso, titulo = a.titulo ~= "" and a.titulo or "Aviso", texto = a.texto or "",
              app = a.app or "", accion = a.accion, hora = os.date("%H:%M")}
  table.insert(historial, 1, av)
  while #historial > 50 do table.remove(historial) end
  sin_leer = sin_leer + 1
  guarda_avisos()
  cola[#cola + 1] = av
  while #cola > 5 do table.remove(cola, 1) end   -- una avalancha: solo los ultimos
  if not actual then siguiente() end
end

local function pinta_aviso(t)
  if not actual then return end
  if not t_fuera and t - t_aviso > DURA then quitar_aviso(t) end
  local p = suave((t - t_aviso) / 0.32)
  if t_fuera then
    p = p * (1 - suave((t - t_fuera) / 0.22))
    if t - t_fuera > 0.24 then siguiente(); return end
  end
  if p <= 0 then return end
  nora.caja(-2, -2, 644, 484, 0, 0x0e1017, 0.35 * p)        -- oscurece lo de debajo
  local x, w, h = 20, 600, 104
  local y = -h - 20 + (h + 32) * p
  nora.sombra(x, y + 8, w, h, 22, 20, 0x000000, 0.55 * p)
  nora.caja(x, y, w, h, 22, PANEL)
  nora.caja(x + 1, y + 1, w - 2, h - 2, 21, 0x262a38, 0.6)
  nora.circulo(x + 46, y + 44, 22, NORA, 0.22)
  nora.icono("bell", x + 46, y + 44, NORA, 1)
  local tx, tw = x + 84, w - 84 - 24
  local cab = actual.app ~= "" and (actual.app .. "  ·  " .. actual.hora) or actual.hora
  nora.texto(cab, tx, y + 12, "peq", TENUE)
  nora.texto(lineas(actual.titulo, "med", tw, 1)[1] or "", tx, y + 34, "med", CLARO)
  local l = lineas(actual.texto, "peq", tw, 1)
  if l[1] then nora.texto(l[1], tx, y + 68, "peq", 0xc9ccd8) end
  -- lo que queda de los 6 s, como una linea que se acaba
  if not t_fuera then
    local resta = math.max(0, 1 - (t - t_aviso) / DURA)
    nora.caja(x + 22, y + h - 6, (w - 44) * resta, 3, 2, NORA, 0.5)
  end
  if #cola > 0 then nora.texto("+" .. #cola, x + w - 24, y + 12, "peq", TENUE, 1, "der") end
end

---------------------------------------------------------------- el PC
local function manda(t) nora.enviar(json.escribe(t)) end
local function avisa_ajuste()            -- el PC se entera de lo que cambia aqui
  manda({tipo = "ajuste", volumen = math.floor(estado.vol * 100 + 0.5), brillo = math.floor(estado.brillo * 100 + 0.5)})
end

local ATIENDE = {}
function ATIENDE.dato(m)                 -- {tipo=dato, app=, nombre=, valor=}: para que la app lo lea
  if type(m.nombre) ~= "string" then return end
  local clave = (type(m.app) == "string" and m.app ~= "" and (m.app .. ".") or "") .. m.nombre
  nora.guardar(clave, m.valor ~= nil and m.valor ~= json.nulo and json.escribe(m.valor) or nil)
end
function ATIENDE.hora(m)                 -- {tipo=hora, epoch=segundos UTC}
  if type(m.epoch) == "number" and math.abs(m.epoch - os.time()) > 2 then
    nora.log("hora del PC:", m.epoch, "(iba", m.epoch - os.time(), "s atrasada)")
    nora.poner_hora(m.epoch)
  end
end
function ATIENDE.aviso(m)                -- {tipo=aviso, titulo=, texto=, app=, accion=, id=}
  local function txt(v) return type(v) == "string" and v or "" end
  avisar({titulo = txt(m.titulo), texto = txt(m.texto), app = txt(m.app),
          accion = type(m.accion) == "string" and m.accion ~= "" and m.accion or nil, id = m.id})
end
function ATIENDE.ajuste(m)               -- {tipo=ajuste, volumen=0..100, brillo=0..100}
  if type(m.volumen) == "number" then estado.vol = nora.volumen(math.max(0, math.min(1, m.volumen / 100))) end
  if type(m.brillo) == "number" then estado.brillo = nora.brillo(math.max(0.02, math.min(1, m.brillo / 100))) end
  avisa_ajuste()                          -- contesta con lo que ha quedado de verdad
end

-- el PC se entera de que cambia la app abierta o la bateria
local contado = {}
local nombres = {}
local function nombre_app(id)
  if nombres[id] == nil then
    local ok, info = pcall(dofile, nora.apps .. "/" .. id .. "/info.lua")
    nombres[id] = ok and type(info) == "table" and info.nombre or (id == "inicio" and "Inicio" or id)
  end
  return nombres[id]
end
local function avisa_estado(id)
  if not estado.puente then return end
  if contado.app == id and contado.bat == estado.bat and contado.cargando == estado.cargando then return end
  contado = {app = id, bat = estado.bat, cargando = estado.cargando}
  manda({tipo = "estado", app = id, nombre_app = nombre_app(id), bateria = estado.bat, cargando = estado.cargando})
end

function app.mensaje(linea)
  local m, err = json.lee(linea)
  if type(m) ~= "table" then nora.log("mensaje del PC que no se entiende:", err or linea); return end
  local f = ATIENDE[m.tipo]
  if not f then nora.log("mensaje del PC sin atender:", m.tipo); return end
  local ok, e = pcall(f, m)             -- un mensaje raro no debe tumbar el sistema
  if not ok then nora.log("fallo atendiendo", m.tipo, e) end
end

function app.enlace(conectado)
  estado.puente = conectado
  contado = {}
  if conectado then
    estado.vol, estado.brillo = nora.volumen(), nora.brillo()
    avisa_ajuste()
  end
end

function app.pinta(t, con_barra, id)
  refresca(t)
  avisa_estado(id)
  if con_barra then barra(t) end
  menu(t)
  pinta_aviso(t)
end

-- con el menu abierto, todo es del sistema; con un aviso, A y B (el resto sigue yendo a la app)
function app.abierto() return abierto or (actual ~= nil and not t_fuera) end

function app.boton(b)
  if not abierto and actual and not t_fuera and b ~= "MENU" then
    if b == "B" then quitar_aviso(); suena("volver"); return true end
    if b == "A" then
      if actual.accion then manda({tipo = "accion", nombre = actual.accion, aviso = actual.id}); suena("abrir")
      else suena("volver") end
      quitar_aviso()
      return true
    end
    return false
  end
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
    if fila == 1 then estado.vol = nora.volumen(math.max(0, math.min(1, estado.vol + d * 0.1))); suena("mover"); avisa_ajuste()
    elseif fila == 2 then estado.brillo = nora.brillo(math.max(0.02, math.min(1, estado.brillo + d * 0.1))); avisa_ajuste()
    else col = math.max(1, math.min(#BOTONES, col + d)); suena("mover") end
  elseif b == "A" and fila == 3 then
    local accion = BOTONES[col][3]
    abierto = false; t_cierra = nora.ahora()
    suena("abrir")
    if accion == "salir" then nora.salir() else nora.abrir(accion) end
  end
  return true
end
