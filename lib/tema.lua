-- tema: los colores, el fondo, los sonidos, la música y el estilo de Nora.
--
--   local T = require "tema"
--   function app.pinta(t)
--     T.revisa()                      -- si el usuario cambia de tema, se nota al momento
--     T.pinta_fondo()                 -- color de fondo (y la imagen del tema, si tiene)
--     nora.caja(20, 20, 200, 80, T.radio(16), T.panel)
--     nora.texto("Hola", 36, 40, "med", T.texto)
--   end
--
-- Un tema es una carpeta en la tarjeta, <apps>/../temas/<id>/, con tema.lua
-- (ver temas/noche/tema.lua, que trae todos los campos) y, si quiere, su fondo,
-- sus sonidos y su música. Lo que un tema no diga, lo pone el tema de base.
local T = {}

local BASE = {
  nombre = "Noche",
  colores = {
    fondo = 0x0e1017,        -- la pantalla
    panel = 0x1a1d29,        -- tarjetas, cajas
    panel2 = 0x2a2e3e,       -- tarjeta elegida o encima de otra
    riel = 0x2b2f3f,         -- pistas de deslizadores, círculos de las pistas de botones
    texto = 0xf3f4f8,        -- texto principal
    suave = 0xc9ccd8,        -- texto secundario que aún se tiene que leer bien
    tenue = 0x8b90a5,        -- texto de apoyo, pistas
    acento = 0x8b7cf6,       -- el color de Nora: elegido, anillos, botones principales
    sobre_acento = 0x0e1017, -- lo que va encima del acento (texto de botones, lo elegido relleno)
    sobre_app = 0x0e1017,    -- el icono encima del color de cada app (oscuro también en temas claros)
    bien = 0x34d399, aviso = 0xfbbf24, mal = 0xf87171, info = 0x60a5fa,
    velo = 0x0e1017,         -- lo que oscurece la app cuando el sistema se pone encima
    sombra = 0x000000,
  },
  fondo = nil,               -- imagen de fondo (PNG/SVG, 640x480) relativa al tema
  fondo_alfa = 1,
  sonidos = {},              -- mover, abrir, volver, menu, aviso, pasar-izq, pasar-der: WAV del tema
  musica = "sistema",        -- la música del menú: "sistema" (la de siempre), un WAV del tema o false
  musica_volumen = 0.35,
  estilo = {
    radio = 1,               -- multiplica el radio de las esquinas (0 = cuadradas)
    inicio = "cuadricula",   -- "cuadricula" | "lista" | "carrusel"
    icono = "circulo",       -- fondo de los iconos de las apps: "circulo" | "cuadrado" | "nada"
    sombras = true,          -- sombras suaves bajo los paneles
    anillo = "borde",        -- cómo se marca lo elegido en el inicio: "borde" | "relleno"
    color_iconos = "app",    -- color del fondo de los iconos: "app" (el de cada app) | "acento" (todos el del tema)
  },
}

local SONIDOS = os.getenv("NORA_SONIDOS") or "/mnt/mmc/nora/sonidos"
T.carpeta_temas = nora.apps .. "/../temas"

local function mezcla(base, encima)
  local r = {}
  for k, v in pairs(base) do r[k] = v end
  if type(encima) == "table" then
    for k, v in pairs(encima) do
      if type(v) == "table" and type(r[k]) == "table" then r[k] = mezcla(r[k], v) else r[k] = v end
    end
  end
  return r
end

local function lee_activo()
  local f = io.open(T.carpeta_temas .. "/activo")
  local id = f and f:read("*l")
  if f then f:close() end
  return id and id:match("^[%w_%-]+$") or "noche"
end

-- preferencias del usuario por encima del tema (las pone Ajustes): temas/preferencias.json
local json = require "json"
local function lee_prefs()
  local f = io.open(T.carpeta_temas .. "/preferencias.json")
  local p = f and json.lee(f:read("*a") or "")
  if f then f:close() end
  return type(p) == "table" and p or {}
end
T.prefs = lee_prefs()

local visto = -1        -- versión de "tema.activo" que se ha cargado
local imagen_fondo, ruta_fondo

-- carga el tema `id` (o el activo) en T
function T.cargar(id)
  id = id or lee_activo()
  local dir = T.carpeta_temas .. "/" .. id
  local ok, def = pcall(dofile, dir .. "/tema.lua")
  if not ok or type(def) ~= "table" then
    if ok == false then nora.log("tema", id, "no carga:", def) end
    def, id, dir = {}, "noche", T.carpeta_temas .. "/noche"
  end
  local t = mezcla(BASE, def)
  T.id, T.nombre, T.dir, T.def = id, t.nombre, dir, t
  for k, v in pairs(t.colores) do T[k] = v end
  T.prefs = lee_prefs()
  T.estilo = t.estilo
  T.estilo_del_tema = t.estilo.inicio
  if T.prefs.inicio == "cuadricula" or T.prefs.inicio == "lista" or T.prefs.inicio == "carrusel" then
    T.estilo.inicio = T.prefs.inicio
  end
  T.sonidos = t.sonidos
  imagen_fondo, ruta_fondo = nil, t.fondo and (dir .. "/" .. t.fondo) or nil
end

-- ¿ha cambiado el tema? (lo pone Ajustes con T.poner). Llámalo al principio de app.pinta.
function T.revisa()
  local _, v = nora.dato("tema.activo")
  if v == visto then return false end
  visto = v
  local id = nora.dato("tema.activo")
  T.cargar(id)
  return true
end

-- cambia el tema de todo el sistema (y lo recuerda al reiniciar)
function T.poner(id)
  local f = io.open(T.carpeta_temas .. "/activo", "w")
  if f then f:write(id, "\n"); f:close() end
  nora.guardar("tema.activo", id)
  T.revisa()
end

-- cambia una preferencia (inicio = "cuadricula"|"lista"|"carrusel"|nil, musica = false|nil) para todo el sistema
function T.preferir(clave, valor)
  local p = lee_prefs()
  p[clave] = valor
  local f = io.open(T.carpeta_temas .. "/preferencias.json", "w")
  if f then f:write(json.escribe(p)); f:close() end
  nora.guardar("tema.activo", T.id)        -- que todos lo vuelvan a leer
  T.revisa()
end

-- los temas que hay: {{id=, nombre=}, ...}
function T.lista()
  local l = {}
  for _, id in ipairs(nora.lista(T.carpeta_temas)) do
    if nora.es_carpeta(T.carpeta_temas .. "/" .. id) then
      local ok, def = pcall(dofile, T.carpeta_temas .. "/" .. id .. "/tema.lua")
      if ok and type(def) == "table" then l[#l + 1] = {id = id, nombre = def.nombre or id} end
    end
  end
  return l
end

-- el color del icono de una app según el tema
function T.color_app(c) return T.estilo.color_iconos == "acento" and T.acento or (c or T.acento) end

-- radio de esquina según el estilo del tema
function T.radio(r) return r * T.estilo.radio end

-- el fondo de la pantalla: color y, si el tema la tiene, su imagen
function T.pinta_fondo()
  nora.fondo(T.fondo)
  if ruta_fondo and imagen_fondo == nil then
    imagen_fondo = nora.imagen(ruta_fondo, {ancho = nora.W, alto = nora.H}) or false
  end
  if imagen_fondo then nora.dibuja(imagen_fondo, 0, 0, {w = nora.W, h = nora.H, alfa = T.def.fondo_alfa}) end
end

-- sombra suave bajo un panel, si el tema las quiere
function T.sombrea(x, y, w, h, r, dif, alfa)
  if T.estilo.sombras then nora.sombra(x, y, w, h, T.radio(r), dif, T.sombra, alfa) end
end

-- un sonido del sistema con la voz del tema: "mover", "abrir", "volver", "menu", "aviso"...
function T.suena(nombre, volumen)
  local propio = T.sonidos[nombre]
  nora.suena(propio and (T.dir .. "/" .. propio) or (SONIDOS .. "/" .. nombre .. ".wav"), volumen)
end

-- la música del menú del tema (o ninguna)
function T.musica()
  local m = T.def.musica
  if T.prefs.musica == false then m = false end
  if m == "sistema" then nora.musica(SONIDOS .. "/musica-menu.wav", T.def.musica_volumen)
  elseif m then nora.musica(T.dir .. "/" .. m, T.def.musica_volumen)
  else nora.musica() end
end

T.cargar()
local _, v = nora.dato("tema.activo")
visto = v
return T
