-- Ajustes: volumen, brillo y el estado de la consola. ◀ ▶ cambian, B vuelve.
app.barra = true
local CLARO, TENUE, PANEL, RIEL = 0xf3f4f8, 0x8b90a5, 0x1a1d29, 0x2b2f3f
local SONIDOS = os.getenv("NORA_SONIDOS") or "/tmp/nora/sonidos"
local sel, sy = 1, 0
local vol, brillo, bat, cargando, enlace = 0, 0, 0, false, nil
local t_lee = -10

local function suena(n) nora.suena(SONIDOS .. "/" .. n .. ".wav") end
local FILAS = {
  {icono = "vol", nombre = "Volumen", desliza = true, color = 0x34d399},
  {icono = "sun", nombre = "Brillo", desliza = true, color = 0xfbbf24},
  {icono = "usb", nombre = "Conexión con el PC"},
  {icono = "bat", nombre = "Batería"},
  {icono = "info", nombre = "Sistema"},
}

local function valor(i)
  if i == 3 then return enlace == "cable" and "Por cable USB" or enlace == "wifi" and "Por Wi-Fi" or "Sin conexión"
  elseif i == 4 then return bat .. " %" .. (cargando and " · cargando" or "")
  elseif i == 5 then return "Nora " .. nora.version .. " · PowerVR SGX544" end
end

function app.boton(b)
  if b == "B" then nora.volver(); return end
  if b == "ARRIBA" and sel > 1 then sel = sel - 1; suena("mover") end
  if b == "ABAJO" and sel < #FILAS then sel = sel + 1; suena("mover") end
  if b == "IZQUIERDA" or b == "DERECHA" then
    local d = b == "DERECHA" and 0.1 or -0.1
    if sel == 1 then vol = nora.volumen(math.max(0, math.min(1, vol + d))); suena("mover")
    elseif sel == 2 then brillo = nora.brillo(math.max(0.02, math.min(1, brillo + d))) end
  end
end

function app.actualiza(dt)
  local objetivo = 98 + (sel - 1) * 60
  if sy == 0 then sy = objetivo end
  sy = sy + (objetivo - sy) * math.min(1, dt * 16)
end

function app.pinta(t)
  if t - t_lee > 1 then
    t_lee = t
    vol, brillo = nora.volumen(), nora.brillo()
    bat, cargando = nora.bateria()
    enlace = nora.enlace()
  end
  nora.fondo(0x0e1017)
  nora.texto("Ajustes", 28, 46, "tit", CLARO)
  nora.caja(20, sy - 8, 600, 52, 14, PANEL)
  for i, f in ipairs(FILAS) do
    local y = 98 + (i - 1) * 60
    local elegida = i == sel
    nora.icono(f.icono, 48, y + 18, CLARO, elegida and 1 or 0.6)
    nora.texto(f.nombre, 76, y + 6, "peq", CLARO)
    if f.desliza then
      local v = i == 1 and vol or brillo
      nora.caja(330, y + 13, 230, 10, 5, RIEL)
      nora.caja(330, y + 13, math.max(10, 230 * v), 10, 5, f.color)
      nora.circulo(330 + 230 * v, y + 18, 11, CLARO)
      nora.texto(tostring(math.floor(v * 100 + 0.5)), 604, y + 6, "peq", TENUE, 1, "der")
    else
      nora.texto(valor(i), 604, y + 6, "peq", TENUE, 1, "der")
    end
  end
  nora.texto("Cruceta: elegir y cambiar · B volver", 28, 446, "peq", TENUE)
end
