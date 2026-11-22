-- Avisos: los que han llegado (del PC o del sistema), el mas nuevo arriba. El
-- sistema los guarda en el dato "avisos.lista"; al entrar aqui se dan por leidos.
-- ▲ ▼ se mueven, A manda al PC la accion del aviso (si tiene), B vuelve.
app.barra = true
local json = require "json"
local CLARO, TENUE, PANEL, NORA = 0xf3f4f8, 0x8b90a5, 0x1a1d29, 0x8b7cf6
local SONIDOS = os.getenv("NORA_SONIDOS") or "/tmp/nora/sonidos"
local lista, version, sel, scroll = {}, -1, 1, 0
local ALTO, Y0, VISTAS = 76, 92, 4

local function suena(n) nora.suena(SONIDOS .. "/" .. n .. ".wav") end
local function suave(p) p = math.max(0, math.min(1, p)); return 1 - (1 - p) ^ 3 end

local function corta(s, fuente, w)
  if nora.ancho(s, fuente) <= w then return s end
  while #s > 0 and nora.ancho(s .. "…", fuente) > w do
    s = s:sub(1, -2):gsub("[\128-\191]+$", ""):gsub("[\192-\255]$", "")
  end
  return s .. "…"
end

local function leer()
  local s, v = nora.dato("avisos.lista")
  if v == version then return end
  version = v
  local l = s and json.lee(s)
  lista = type(l) == "table" and l or {}
  sel = math.max(1, math.min(sel, #lista))
end

function app.entrar()
  leer()
  nora.guardar("avisos.sin_leer", "0")
end

function app.boton(b)
  if b == "B" then nora.volver(); return end
  if b == "ARRIBA" and sel > 1 then sel = sel - 1; suena("mover") end
  if b == "ABAJO" and sel < #lista then sel = sel + 1; suena("mover") end
  if b == "A" and lista[sel] and lista[sel].accion then
    nora.enviar(json.escribe({tipo = "accion", nombre = lista[sel].accion, aviso = lista[sel].id}))
    suena("abrir")
  end
end

function app.actualiza(dt)
  local objetivo = math.max(0, math.min(sel - 2, #lista - VISTAS)) * ALTO
  scroll = scroll + (objetivo - scroll) * math.min(1, dt * 14)
end

function app.pinta(t)
  leer()
  nora.fondo(0x0e1017)
  local e = suave(t / 0.4)
  nora.texto("Avisos", 28, 40 + 8 * (1 - e), "tit", CLARO, e)
  if #lista == 0 then
    nora.icono("bell", 320, 230, TENUE, 0.6)
    nora.texto("No ha llegado ningún aviso", 320, 262, "peq", TENUE, 1, "centro")
    return
  end
  for i, a in ipairs(lista) do
    local y = Y0 + (i - 1) * ALTO - scroll
    if y > 40 and y < 440 then
      local elegido = i == sel
      nora.caja(24, y, 592, ALTO - 10, 18, elegido and 0x2a2e3e or PANEL)
      if elegido then nora.caja(24, y + 14, 4, ALTO - 38, 2, NORA) end
      nora.circulo(62, y + 33, 18, NORA, 0.2)
      nora.icono("bell", 62, y + 33, NORA, 1)
      local cab = (a.app and a.app ~= "" and (a.app .. "  ·  ") or "") .. (a.hora or "")
      nora.texto(cab, 600, y + 8, "peq", TENUE, 1, "der")
      nora.texto(corta(tostring(a.titulo or ""), "med", 600 - 92 - nora.ancho(cab, "peq") - 16), 92, y + 6, "med", CLARO)
      nora.texto(corta(tostring(a.texto or ""), "peq", 500), 92, y + 38, "peq", 0xc9ccd8)
    end
  end
  local x = 20
  local pistas = lista[sel] and lista[sel].accion and {{"A", "abrir en el PC"}, {"B", "volver"}} or {{"B", "volver"}}
  for _, p in ipairs(pistas) do
    nora.circulo(x + 11, 459, 11, 0x2b2f3f)
    nora.texto(p[1], x + 11, 449, "peq", CLARO, 1, "centro")
    x = x + 28 + nora.texto(p[2], x + 28, 448, "peq", TENUE) + 22
  end
end
