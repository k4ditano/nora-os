-- Usos: cuanto te queda de Claude y Codex. Primera app de Nora en LuaJIT.
-- Lee /tmp/usos.txt (lo manda el PC: nora-puente / usos-pc.py), una ficha por
-- bloque; ◀ ▶ pasa de una a otra con transicion y sonido.
--   F <tab> titulo <tab> subtitulo <tab> nota <tab> color hex
--   B <tab> nombre <tab> % usado (-1 = no se sabe) <tab> detalle
--   T <tab> nombre <tab> valor
local FONDO, PANEL, TENUE, CLARO, RIEL = 0x0e1017, 0x1a1d29, 0x8b90a5, 0xf3f4f8, 0x2b2f3f
local DATOS = os.getenv("NORA_USOS") or "/tmp/usos.txt"
local SONIDOS = os.getenv("NORA_SONIDOS") or "/tmp/nora/sonidos"

local fichas = {}
local actual, desde, t_cambio, dir = 1, nil, -10, 1
local ult_lectura = -10

-- el PC manda Latin-1; el nucleo pinta UTF-8
local function utf8(s)
  return (s:gsub("[\128-\255]", function(c)
    local b = c:byte()
    return string.char(0xc0 + math.floor(b / 64), 0x80 + b % 64)
  end))
end

local function leer()
  local f = io.open(DATOS, "rb")
  if not f then return end
  local nuevas, k = {}, nil
  for linea in f:lines() do
    local c = {}
    for campo in (utf8(linea) .. "\t"):gmatch("([^\t]*)\t") do c[#c + 1] = campo end
    if c[1] == "F" then
      k = {titulo = c[2] or "", sub = c[3] or "", nota = c[4] or "", color = tonumber(c[5] or "", 16) or TENUE,
           barras = {}, filas = {}}
      nuevas[#nuevas + 1] = k
    elseif c[1] == "B" and k then
      k.barras[#k.barras + 1] = {nombre = c[2] or "", pct = tonumber(c[3]) or -1, detalle = c[4] or ""}
    elseif c[1] == "T" and k then
      k.filas[#k.filas + 1] = {nombre = c[2] or "", valor = c[3] or ""}
    end
  end
  f:close()
  if #nuevas > 0 then fichas = nuevas; actual = math.min(actual, #fichas) end
end

local function color_uso(p)
  if p < 0 then return TENUE elseif p < 60 then return 0x34d399 elseif p < 85 then return 0xfbbf24 end
  return 0xf87171
end
local function suave(p) p = math.max(0, math.min(1, p)); return 1 - (1 - p) ^ 3 end
local function aclarar(c, f)
  local r, g, b = math.floor(c / 65536), math.floor(c / 256) % 256, c % 256
  return math.floor(r + (255 - r) * f) * 65536 + math.floor(g + (255 - g) * f) * 256 + math.floor(b + (255 - b) * f)
end

local function pinta_ficha(k, dx, t, ent)
  local x, y, w = 24 + dx, 20, 592
  if x > 640 or x + w < 0 then return end
  nora.sombra(x, y + 10, w, 410, 22, 22, 0x000000, 0.6)
  nora.caja(x, y, w, 410, 22, PANEL)
  -- motas del color de la marca
  for i = 0, 13 do
    local fx = (i * 0.618 + t * (0.010 + 0.004 * (i % 3))) % 1
    local fy = (i * 0.377 + t * (0.020 + 0.006 * (i % 4))) % 1
    nora.circulo(x + 30 + fx * (w - 60) + 8 * math.sin(t * 0.9 + i), y + 400 - fy * 380, 2 + i % 3, k.color,
                 0.16 * math.sin(math.pi * fy))
  end
  nora.caja(x, y + 24, 6, 362, 3, k.color, 0.65 + 0.35 * (0.5 + 0.5 * math.sin(t * 2.2)))   -- filo que respira
  nora.texto(k.titulo, x + 28, y + 22, "med", CLARO)
  nora.texto(k.sub, x + 28, y + 60, "peq", TENUE)

  local cy = y + 98
  if #k.barras > 0 then
    local b = k.barras[1]                       -- en grande, la ventana mas apurada
    for _, bi in ipairs(k.barras) do if bi.pct > b.pct then b = bi end end
    local col, num = color_uso(b.pct), "?"
    if b.pct >= 0 then
      num = string.format("%d%%", math.floor((100 - b.pct) * suave((ent - 0.1) / 0.9) + 0.5))
      if b.pct >= 85 then col = aclarar(col, 0.25 * (0.5 + 0.5 * math.sin(t * 4))) end   -- late
    end
    local sube = 18 * (1 - suave(ent / 0.5))
    local wn = nora.texto(num, x + 24, cy - 6 + sube, "gra", col)
    nora.texto("te queda (" .. b.nombre:lower() .. ")", x + 36 + wn, cy + 40 + sube, "peq", CLARO)
    nora.texto(b.detalle, x + 36 + wn, cy + 66 + sube, "peq", TENUE)
    cy = cy + 118
    for i, bi in ipairs(k.barras) do
      nora.texto(bi.nombre, x + 28, cy, "peq", CLARO)
      nora.texto(bi.pct >= 0 and (bi.pct .. "% usado") or "sin datos", x + w - 28, cy, "peq", TENUE, 1, "der")
      local bx, by, bw = x + 28, cy + 30, w - 56
      nora.caja(bx, by, bw, 12, 6, RIEL)
      if bi.pct > 0 then
        local lleno = bw * math.min(bi.pct, 100) / 100 * suave((ent - 0.25 - 0.15 * (i - 1)) / 0.8)
        if lleno >= 12 then
          nora.caja(bx, by, lleno, 12, 6, color_uso(bi.pct))
          -- brillo que recorre la barra
          local pos = ((t * 0.45 + i * 0.37) % 1.6) - 0.3
          local hx = bx + pos * lleno
          local a, b2 = math.max(bx + 4, hx - 26), math.min(bx + lleno - 4, hx + 26)
          if b2 > a then nora.sombra(a + 6, by + 3, b2 - a - 12, 6, 3, 6, 0xffffff, 0.45) end
        end
      elseif bi.pct < 0 then                   -- sin datos: una bolita que va y viene
        nora.circulo(bx + 6 + (0.5 + 0.5 * math.sin(t * 1.5 + i)) * (bw - 12), by + 6, 5, TENUE, 0.6)
      end
      if bi ~= b then nora.texto(bi.detalle, x + 28, cy + 48, "peq", TENUE) end
      cy = cy + (bi ~= b and 78 or 56)
    end
  end
  for i, f in ipairs(k.filas) do
    local sube = 16 * (1 - suave((ent - 0.08 * (i - 1)) / 0.5))
    nora.texto(f.nombre, x + 28, cy + sube, "med", CLARO)
    nora.texto(f.valor, x + w - 28, cy + sube, "med", k.color, 1, "der")
    cy = cy + 50
  end
  nora.circulo(x + 34, y + 385, 4, 0x34d399, 0.5 + 0.5 * math.sin(t * 3))   -- en vivo
  nora.texto(k.nota, x + 46, y + 374, "peq", TENUE)
end

function app.entrar() leer(); t_cambio = nora.ahora() end

function app.boton(b)
  if b == "B" then nora.volver(); return end
  if #fichas < 2 then return end
  local paso = (b == "DERECHA" or b == "R") and 1 or (b == "IZQUIERDA" or b == "L") and -1 or 0
  if paso == 0 then return end
  desde, dir = actual, paso
  actual = (actual - 1 + paso) % #fichas + 1
  t_cambio = nora.ahora()
  nora.suena(SONIDOS .. (paso > 0 and "/pasar-der.wav" or "/pasar-izq.wav"))
end

function app.pinta(t)
  if t - ult_lectura > 1 then leer(); ult_lectura = t end
  nora.fondo(FONDO)
  if #fichas == 0 then
    nora.texto("Esperando datos del PC…", 320, 220, "med", TENUE, 1, "centro")
    return
  end
  local p = (t - t_cambio) / 0.32
  if desde and p < 1 then
    local off = 640 * suave(p)
    pinta_ficha(fichas[desde], -dir * off, t, 10)
    pinta_ficha(fichas[actual], -dir * off + dir * 640, t, t - t_cambio)
  else
    desde = nil
    pinta_ficha(fichas[actual], 0, t, t - t_cambio)
  end
  -- puntos: la pildora se desliza hacia la ficha nueva
  local pos = actual
  if desde then pos = desde + (actual - desde) * suave(p) end
  local x0 = 320 - (#fichas - 1) * 12
  for i = 1, #fichas do
    local cerca = 1 - math.min(1, math.abs(pos - i))
    local ancho = 8 + 14 * cerca
    nora.caja(x0 + (i - 1) * 24 - ancho / 2, 451, ancho, 8, 4, cerca > 0.5 and CLARO or 0x4a4f63)
  end
end
