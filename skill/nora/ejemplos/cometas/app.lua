-- Cometas: recoge estrellas y esquiva rocas. Ejemplo de juego para Nora:
-- imagenes SVG y PNG, cruceta mantenida (nora.pulsado), sonido propio,
-- record guardado en la tarjeta (nora.salvar/cargar) y estados menu/juego/fin.
local W, H = nora.W, nora.H
local SONIDOS = os.getenv("NORA_SONIDOS")

-- imagenes: se cargan una vez, al abrir la app (rutas relativas a esta carpeta)
local NAVE = assert(nora.imagen("img/nave.svg", {ancho = 56}))
local ESTRELLA = assert(nora.imagen("img/estrella.svg", {ancho = 36}))
local ROCA = assert(nora.imagen("img/roca.png", {pixel = true}))   -- 24x24, se escala sin suavizar

local estado = "menu"              -- "menu" | "juego" | "fin"
local nave, cosas, puntos, vidas, t_juego, t_sale, golpe
local record = tonumber(nora.cargar("record") or "") or 0
local fondo_estrellas = {}
for i = 1, 60 do fondo_estrellas[i] = {x = math.random(W), y = math.random(H), v = 20 + math.random(60)} end

local function empezar()
  nave = {x = W / 2, y = H - 70}
  cosas, puntos, vidas, t_juego, t_sale, golpe = {}, 0, 3, 0, 0, 0
  estado = "juego"
end

local function choca(a, b, r) return (a.x - b.x) ^ 2 + (a.y - b.y) ^ 2 < r * r end

function app.boton(b)
  if b == "B" then nora.volver(); return end
  if estado ~= "juego" and (b == "A" or b == "START") then
    empezar(); nora.suena(SONIDOS .. "/abrir.wav")
  end
end

function app.actualiza(dt)
  for _, e in ipairs(fondo_estrellas) do
    e.y = e.y + e.v * dt
    if e.y > H then e.y, e.x = 0, math.random(W) end
  end
  if estado ~= "juego" then return end
  t_juego = t_juego + dt
  -- la nave: cruceta mantenida, con un poco de inercia
  local v = 320
  if nora.pulsado("IZQUIERDA") then nave.x = nave.x - v * dt end
  if nora.pulsado("DERECHA") then nave.x = nave.x + v * dt end
  if nora.pulsado("ARRIBA") then nave.y = nave.y - v * dt end
  if nora.pulsado("ABAJO") then nave.y = nave.y + v * dt end
  nave.x = math.max(28, math.min(W - 28, nave.x))
  nave.y = math.max(80, math.min(H - 30, nave.y))
  -- cada vez salen mas cosas, y mas rapido
  t_sale = t_sale - dt
  if t_sale <= 0 then
    t_sale = math.max(0.25, 0.9 - t_juego * 0.01)
    local roca = math.random() < 0.55
    cosas[#cosas + 1] = {x = math.random(30, W - 30), y = -30, roca = roca,
                         v = 120 + math.random(80) + t_juego * 3, giro = math.random() * 6, vg = math.random() * 4 - 2}
  end
  golpe = math.max(0, golpe - dt)
  for i = #cosas, 1, -1 do
    local c = cosas[i]
    c.y, c.giro = c.y + c.v * dt, c.giro + c.vg * dt
    if c.y > H + 40 then table.remove(cosas, i)
    elseif choca(c, nave, c.roca and 34 or 38) then
      table.remove(cosas, i)
      if not c.roca then
        puntos = puntos + 1
        nora.suena("sonidos/moneda.wav")              -- relativa a la carpeta de la app
      elseif golpe == 0 then
        vidas, golpe = vidas - 1, 1.2
        nora.suena(SONIDOS .. "/volver.wav")
        if vidas == 0 then
          estado = "fin"
          if puntos > record then record = puntos; nora.salvar("record", tostring(record)) end
          nora.log("partida acabada:", puntos, "puntos, record", record)
        end
      end
    end
  end
end

local function centrado(s, y, fuente, color, alfa) nora.texto(s, W / 2, y, fuente, color, alfa or 1, "centro") end

function app.pinta(t)
  nora.fondo(0x0b0d1a)
  for _, e in ipairs(fondo_estrellas) do nora.caja(e.x, e.y, 2, 2, 1, 0xffffff, e.v / 100) end
  if estado == "menu" then
    nora.dibuja(NAVE, W / 2, 200 + math.sin(t * 2) * 8, {centro = true, escala = 1.6})
    centrado("Cometas", 50, "tit", 0xf3f4f6)
    centrado("Recoge estrellas, esquiva rocas", 290, "peq", 0x9ca3af)
    centrado("A para empezar", 360, "med", 0xffd166, 0.6 + 0.4 * math.sin(t * 4))
    if record > 0 then centrado("Record: " .. record, 420, "peq", 0x8b7cf6) end
    return
  end
  for _, c in ipairs(cosas) do
    if c.roca then nora.dibuja(ROCA, c.x, c.y, {centro = true, w = 52, h = 52, giro = c.giro})
    else nora.dibuja(ESTRELLA, c.x, c.y, {centro = true, giro = math.sin(t * 3 + c.x) * 0.3}) end
  end
  local parpadea = golpe > 0 and math.floor(t * 12) % 2 == 0
  if estado == "juego" and not parpadea then nora.dibuja(NAVE, nave.x, nave.y, {centro = true}) end
  nora.texto(tostring(puntos), 20, 14, "med", 0xffd166)
  for i = 1, vidas do nora.icono("heart", W - 30 * i, 34, 0xf87171) end
  if estado == "fin" then
    nora.caja(120, 150, 400, 190, 20, 0x161a2e, 0.95)
    centrado("Fin", 170, "tit", 0xf3f4f6)
    centrado(puntos .. " estrellas  ·  record " .. record, 240, "peq", 0x9ca3af)
    centrado("A otra vez  ·  B salir", 290, "peq", 0xffd166)
  end
end
