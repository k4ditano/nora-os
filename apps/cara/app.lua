-- Cara: la mascota de Nora. Respira, parpadea, mira a los lados y se rie con A.
-- X cambia de expresion (feliz, sorpresa, triste, dormida). B vuelve.
local FONDO, CARA, OJO, MEJILLA = 0x1b1d2b, 0xffd166, 0x1b1d2b, 0xff8fa3
local SONIDOS = os.getenv("NORA_SONIDOS") or "/tmp/nora/sonidos"
local EXPR = {"feliz", "sorpresa", "triste", "dormida"}
local expr, risa_hasta, prox_parpadeo = 1, 0, 2

function app.boton(b)
  if b == "B" then nora.volver()
  elseif b == "A" then risa_hasta = nora.ahora() + 1.6; nora.suena(SONIDOS .. "/risa.wav")
  elseif b == "X" then expr = expr % #EXPR + 1 end
end

-- arco grueso hecho de circulitos: y = cy + curva * (1 - u^2)
local function arco(cx, cy, ancho, curva, grosor, color)
  for x = -ancho, ancho, 3 do
    local u = x / ancho
    nora.circulo(cx + x, cy + curva * (1 - u * u), grosor, color)
  end
end

function app.pinta(t)
  local ahora = nora.ahora()
  local riendo, e = ahora < risa_hasta, EXPR[expr]
  nora.fondo(FONDO)
  local abierto = 1
  if t > prox_parpadeo then                      -- parpadeo de 0,15 s cada 2-5 s
    local p = (t - prox_parpadeo) / 0.15
    if p >= 1 then prox_parpadeo = t + 2 + math.random() * 3 else abierto = math.abs(1 - 2 * p) end
  end
  if e == "dormida" then abierto = 0.08 end
  local mira = e == "dormida" and 0 or 18 * math.sin(t * 0.7)
  local cx = 320
  local cy = 245 + (riendo and 10 * math.sin(t * 28) or 6 * math.sin(t * 1.6))

  nora.sombra(cx - 220, cy - 190, 440, 400, 200, 30, 0x000000, 0.5)
  nora.caja(cx - 220, cy - 200, 440, 400, 200, CARA)
  for lado = -1, 1, 2 do
    local ex, ey = cx + lado * 80 + mira, cy - 45
    if riendo then
      arco(cx + lado * 80, ey + 10, 30, -24, 6, OJO)                  -- ojos ^ ^
    else
      local ry = (e == "sorpresa" and 52 or 42) * abierto
      local rx = e == "sorpresa" and 32 or 26
      nora.caja(ex - rx, ey - ry, 2 * rx, 2 * ry, math.min(rx, ry), OJO)
      if abierto > 0.5 then nora.circulo(ex + 8, ey - ry * 0.4, 7, 0xffffff) end
    end
    if e ~= "triste" and e ~= "dormida" or riendo then
      nora.sombra(cx + lado * 140 - 30, cy + 26, 60, 28, 14, 6, MEJILLA, 0.9)
    end
  end
  if riendo then
    local ab = 45 + 12 * math.abs(math.sin(t * 14))
    nora.caja(cx - 85, cy + 70 - ab, 170, 2 * ab, ab, OJO)
    nora.caja(cx - 90, cy + 70 - ab - 2, 180, ab + 2, 0, CARA)           -- tapa la mitad de arriba
    nora.caja(cx - 40, cy + 70 + ab * 0.3, 80, ab * 0.6, ab * 0.3, 0xe85d75)
  elseif e == "sorpresa" then nora.caja(cx - 26, cy + 53, 52, 64, 26, OJO)
  elseif e == "dormida" then arco(cx, cy + 85, 30, 0, 6, OJO)
  elseif e == "triste" then arco(cx, cy + 110, 60, -30, 7, OJO)
  else arco(cx, cy + 70, 70, 35 + 5 * math.sin(t * 1.6), 7, OJO) end
  if e == "dormida" then
    for i = 0, 2 do
      local f = (t * 0.4 + i / 3) % 1
      nora.texto("z", cx + 170 + f * 60, cy - 150 - f * 120, "tit", 0x8aa0ff, 1 - f)
    end
  end
  nora.texto("A reír · X cara · B volver", 320, 446, "peq", 0x6b6f86, 1, "centro")
end
