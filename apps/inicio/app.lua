-- inicio: el lanzador de Nora. Las apps de NORA_APPS en cuadricula; la cruceta
-- elige, A abre. Cada app puede traer info.lua: return {nombre=, icono=, color=}
app.barra = true
local CLARO, TENUE, PANEL = 0xf3f4f8, 0x8b90a5, 0x1a1d29
local SONIDOS = os.getenv("NORA_SONIDOS") or "/tmp/nora/sonidos"
local OCULTAS = {inicio = true, sistema = true}

local apps, sel, sx, sy = {}, 1, 0, 0     -- sx, sy: posicion animada del anillo
local resumen = ""

local function suena(n) nora.suena(SONIDOS .. "/" .. n .. ".wav") end
local function suave(p) p = math.max(0, math.min(1, p)); return 1 - (1 - p) ^ 3 end

local function cargar()
  apps = {}
  for _, id in ipairs(nora.lista(nora.apps)) do
    if not OCULTAS[id] then
      local ok, info = pcall(dofile, nora.apps .. "/" .. id .. "/info.lua")
      info = ok and type(info) == "table" and info or {}
      apps[#apps + 1] = {id = id, nombre = info.nombre or id, icono = info.icono or "cube",
                         color = info.color or 0x8b7cf6, orden = info.orden or 50}
    end
  end
  table.sort(apps, function(a, b) return a.orden < b.orden or (a.orden == b.orden and a.nombre < b.nombre) end)
end

-- una linea con lo mas urgente: lo que queda de Claude, si el PC lo ha mandado
local json = require "json"
local version_usos = -1
local function leer_resumen()
  local s, v = nora.dato("usos.fichas")
  if v == version_usos and resumen ~= "" then return end
  version_usos = v
  local peor
  local fichas = s and json.lee(s)
  if type(fichas) == "table" and type(fichas[1]) == "table" and type(fichas[1].barras) == "table" then
    for _, b in ipairs(fichas[1].barras) do                 -- la primera ficha es Claude
      local pct = tonumber(b.pct)
      if pct and pct >= 0 and (not peor or pct > peor) then peor = pct end
    end
  end
  resumen = peor and string.format("Claude te deja un %d %% · %d apps", 100 - peor, #apps) or (#apps .. " apps")
end

local function saludo()
  local h = tonumber(os.date("%H"))
  return h < 6 and "Buenas noches" or h < 14 and "Buenos días" or h < 21 and "Buenas tardes" or "Buenas noches"
end

function app.entrar()
  cargar(); leer_resumen()
  nora.musica(SONIDOS .. "/musica-menu.wav", 0.35)   -- lo-fi de fondo (Estudio IA), en bucle
end

local function posicion(i)
  local tw, gx, gy = 132, 18, 16
  local x0 = (640 - (4 * tw + 3 * gx)) / 2
  return x0 + ((i - 1) % 4) * (tw + gx), 138 + math.floor((i - 1) / 4) * (tw + gy)
end

function app.boton(b)
  local n = #apps
  if n == 0 then return end
  local antes = sel
  if b == "DERECHA" then sel = math.min(n, sel + 1)
  elseif b == "IZQUIERDA" then sel = math.max(1, sel - 1)
  elseif b == "ABAJO" then sel = math.min(n, sel + 4)
  elseif b == "ARRIBA" then sel = math.max(1, sel - 4)
  elseif b == "A" then suena("abrir"); nora.abrir(apps[sel].id); return end
  if sel ~= antes then suena("mover") end
end

function app.actualiza(dt)
  local x, y = posicion(sel)
  if sx == 0 then sx, sy = x, y end
  local k = math.min(1, dt * 16)
  sx, sy = sx + (x - sx) * k, sy + (y - sy) * k
end

function app.pinta(t)
  leer_resumen()
  nora.fondo(0x0e1017)
  local e = suave(t / 0.5)
  nora.texto(saludo(), 28, 52 + 10 * (1 - e), "tit", CLARO, e)
  nora.texto(resumen, 30, 104, "peq", TENUE, e)
  if #apps == 0 then
    nora.texto("No hay apps en " .. nora.apps, 320, 260, "peq", TENUE, 1, "centro")
    return
  end
  -- anillo de la elegida, que se desliza
  nora.sombra(sx - 6, sy - 6, 144, 144, 28, 10, 0x8b7cf6, 0.35 + 0.1 * math.sin(t * 3))
  nora.caja(sx - 5, sy - 5, 142, 142, 26, CLARO)
  nora.caja(sx - 2, sy - 2, 136, 136, 24, 0x0e1017)
  for i, a in ipairs(apps) do
    local x, y = posicion(i)
    local entra = suave((t - 0.05 * i) / 0.4)              -- entran en cascada
    y = y + 20 * (1 - entra)
    local s = i == sel and 1 or 0
    nora.caja(x, y, 132, 132, 22, PANEL, entra)
    nora.circulo(x + 66, y + 50, 30 + 2 * s * math.sin(t * 4), a.color, entra)
    nora.icono(a.icono, x + 66, y + 50, 0x0e1017, entra)
    nora.texto(a.nombre, x + 66, y + 92, "peq", CLARO, entra, "centro")
  end
  -- pista de botones
  local x = 20
  for _, p in ipairs({{"A", "abrir"}, {"M", "menú rápido"}}) do
    nora.circulo(x + 11, 459, 11, 0x2b2f3f)
    nora.texto(p[1], x + 11, 449, "peq", CLARO, 1, "centro")
    x = x + 28 + nora.texto(p[2], x + 28, 448, "peq", TENUE) + 22
  end
end
