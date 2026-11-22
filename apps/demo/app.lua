-- demo: prueba la API del nucleo. Sombras, cajas, texto, animacion y botones.
local FONDO, PANEL, CLARO, TENUE, NORA = 0x0e1017, 0x1a1d29, 0xf3f4f8, 0x8b90a5, 0x8b7cf6
local colores = {0x34d399, 0xfbbf24, 0xf87171, 0x60a5fa, 0xf472b6, 0x8b7cf6}

local pulsados = {}      -- nombre -> momento en que se pulso
local ultimo = "pulsa un boton"
local sel = 1            -- tarjeta elegida con la cruceta
local x_sel = 0          -- posicion animada del marco

function app.entrar()
  nora.log("demo arrancada, nucleo", nora.version, nora.W .. "x" .. nora.H)
end

function app.boton(b)
  pulsados[b] = nora.ahora()
  ultimo = b
  if b == "B" and pulsados.L then nora.volver() end   -- B sola se usa para probar; L + B vuelve
  if b == "IZQUIERDA" then sel = math.max(1, sel - 1) end
  if b == "DERECHA" then sel = math.min(3, sel + 1) end
end

function app.suelta(b) pulsados[b] = nil end

function app.actualiza(dt)
  -- el marco persigue a la tarjeta elegida (muelle amortiguado)
  local objetivo = (sel - 1) * 200
  x_sel = x_sel + (objetivo - x_sel) * math.min(1, dt * 12)
end

function app.pinta(t)
  nora.fondo(FONDO)

  -- motas de fondo
  for i = 1, 24 do
    local fx = (i * 0.618 + t * 0.02 * (1 + i % 3)) % 1
    local fy = (i * 0.377 + t * 0.03 * (1 + i % 4)) % 1
    nora.circulo(fx * 640, 480 - fy * 480, 2 + i % 3, colores[1 + i % #colores], 0.25 * math.sin(math.pi * fy))
  end

  nora.texto("Nora", 28, 20, "tit", CLARO)
  nora.texto("núcleo " .. nora.version .. " · OpenGL ES en la PowerVR SGX544", 30, 70, "peq", TENUE)

  -- resplandor de la elegida (detras de las tarjetas)
  local mx = 30 + x_sel
  nora.sombra(mx - 6, 114 + 6 * math.sin(t * 2 + sel), 192, 172, 26, 12, NORA, 0.45 + 0.2 * math.sin(t * 4))
  -- tres tarjetas con sombra
  for i = 1, 3 do
    local x, y = 30 + (i - 1) * 200, 120 + 6 * math.sin(t * 2 + i)
    nora.sombra(x, y + 8, 180, 160, 20, 18, 0x000000, 0.55)
    nora.caja(x, y, 180, 160, 20, PANEL)
    nora.circulo(x + 46, y + 50, 26, colores[i])
    nora.texto(({"Usos", "Cara", "Juegos"})[i], x + 20, y + 92, "med", CLARO)
    nora.texto(({"7 % semanal", "se ríe con A", "en LuaJIT"})[i], x + 20, y + 128, "peq", TENUE)
  end

  -- los botones que estan pulsados ahora
  local x = 30
  for _, b in ipairs({"A", "B", "X", "Y", "L", "R", "L2", "R2", "SELECT", "START", "MENU"}) do
    local w = nora.ancho(b, "peq") + 18
    local p = pulsados[b]
    nora.caja(x, 330, w, 34, 17, p and NORA or 0x2b2f3f)
    nora.texto(b, x + w / 2, 337, "peq", p and 0xffffff or TENUE, 1, "centro")
    x = x + w + 6
  end
  nora.texto("Último: " .. ultimo, 30, 384, "med", CLARO)
  nora.texto("L + B para volver", 30, 426, "peq", TENUE)

  -- reloj grande, para ver la fluidez
  nora.texto(string.format("%05.2f", t % 100), 610, 395, "gra", 0x2b2f3f, 1, "der")
end
