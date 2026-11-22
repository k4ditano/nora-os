-- teclado: escribir texto con la cruceta, en un panel encima de la app.
--
--   local teclado = require "teclado"
--   teclado.abrir("Nombre de la carpeta", "Juegos", function(texto) ... end)  -- texto = nil si se cancela
--   function app.boton(b) if teclado.boton(b) then return end ... end
--   function app.pinta(t) ...; teclado.pinta(t) end                          -- lo último
--
-- Cruceta: moverse · A: escribir la tecla · X: borrar · Y: mayúsculas · START: aceptar · B: cancelar
local T = require "tema"
local K = {}

local FILAS = {
  {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
  {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
  {"a", "s", "d", "f", "g", "h", "j", "k", "l", "ñ"},
  {"z", "x", "c", "v", "b", "n", "m", ",", ".", "-"},
  {"á", "é", "í", "ó", "ú", "ü", "¿", "?", "¡", "!"},
  {"espacio", "borrar", "aceptar"},
}
local abierto, titulo, texto, al_terminar, f, c, mayus, t_abre = false, "", "", nil, 2, 1, true, 0
local MAX = 24

local function mayuscula(s)
  local m = {["ñ"] = "Ñ", ["á"] = "Á", ["é"] = "É", ["í"] = "Í", ["ó"] = "Ó", ["ú"] = "Ú", ["ü"] = "Ü"}
  return m[s] or s:upper()
end
-- quita el último carácter (UTF-8)
local function sin_ultimo(s)
  local i = #s
  while i > 0 and s:byte(i) >= 0x80 and s:byte(i) < 0xc0 do i = i - 1 end
  return s:sub(1, i - 1)
end
local function largo(s) local _, n = s:gsub("[^\128-\191]", ""); return n end

function K.abierto() return abierto end

function K.abrir(t, inicial, fn)
  abierto, titulo, texto, al_terminar = true, t or "", inicial or "", fn
  f, c, mayus, t_abre = 2, 1, texto == "", nora.ahora()
end

local function cerrar(resultado)
  abierto = false
  if al_terminar then al_terminar(resultado) end
end

local function pulsa()
  local k = FILAS[f][c]
  if k == "espacio" then if largo(texto) < MAX then texto = texto .. " " end
  elseif k == "borrar" then texto = sin_ultimo(texto)
  elseif k == "aceptar" then
    local limpio = texto:gsub("^%s+", ""):gsub("%s+$", "")
    T.suena(limpio ~= "" and "abrir" or "volver")
    cerrar(limpio ~= "" and limpio or nil)
    return
  elseif largo(texto) < MAX then
    texto = texto .. (mayus and mayuscula(k) or k)
    mayus = false
  end
  T.suena("mover")
end

-- true si el teclado se ha quedado el botón
function K.boton(b)
  if not abierto then return false end
  local n = #FILAS[f]
  if b == "DERECHA" then c = c % n + 1
  elseif b == "IZQUIERDA" then c = (c - 2) % n + 1
  elseif b == "ABAJO" or b == "ARRIBA" then
    local x = (c - 0.5) / n                               -- misma columna, a lo ancho
    f = b == "ABAJO" and f % #FILAS + 1 or (f - 2) % #FILAS + 1
    c = math.max(1, math.min(#FILAS[f], math.floor(x * #FILAS[f]) + 1))
  elseif b == "A" then pulsa(); return true
  elseif b == "X" then texto = sin_ultimo(texto)
  elseif b == "Y" then mayus = not mayus
  elseif b == "START" then f, c = #FILAS, 3; pulsa(); return true
  elseif b == "B" then T.suena("volver"); cerrar(nil); return true
  else return true end
  T.suena("mover")
  return true
end

function K.pinta(t)
  if not abierto then return end
  local p = math.min(1, (nora.ahora() - t_abre) / 0.2)
  p = 1 - (1 - p) ^ 3
  nora.caja(-2, -2, 644, 484, 0, T.velo, 0.6 * p)
  local x, w, h = 20, 600, 424
  local y = 480 - (h + 20) * p
  T.sombrea(x, y + 8, w, h, 24, 20, 0.5 * p)
  nora.caja(x, y, w, h, T.radio(24), T.panel)
  nora.texto(titulo, x + 24, y + 14, "peq", T.tenue)
  nora.caja(x + 20, y + 44, w - 40, 50, T.radio(14), T.fondo)
  local tw = nora.texto(texto, x + 36, y + 50, "med", T.texto)
  if math.floor(t * 2) % 2 == 0 then nora.caja(x + 38 + tw, y + 56, 3, 30, 0, T.acento) end
  local ky = y + 108
  for i, fila in ipairs(FILAS) do
    local n = #fila
    local kw = (w - 40 - (n - 1) * 6) / n
    for j, k in ipairs(fila) do
      local kx = x + 20 + (j - 1) * (kw + 6)
      local elegida = i == f and j == c
      local especial = #k > 2
      nora.caja(kx, ky, kw, 38, T.radio(10), elegida and T.acento or (especial and T.panel2 or T.riel))
      local txt = (k == "espacio" and "Espacio") or (k == "borrar" and "Borrar") or (k == "aceptar" and "Aceptar")
                  or (mayus and mayuscula(k) or k)
      nora.texto(txt, kx + kw / 2, ky + 6, "peq", elegida and T.sobre_acento or T.texto, 1, "centro")
    end
    ky = ky + 44
  end
  nora.texto("A escribir · X borrar · Y mayús. · START listo · B cancelar", x + w / 2, y + h - 36, "peq",
             T.tenue, 1, "centro")
end

return K
