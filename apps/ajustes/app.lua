-- Ajustes: volumen, brillo, tema, estilo del inicio, música del menú y el estado
-- de la consola. Cruceta arriba/abajo elige, izquierda/derecha cambia, B vuelve.
-- El tema se aplica al momento en todo el sistema.
app.barra = true
local T = require "tema"
local sel, sy = 1, 0
local vol, brillo, bat, cargando, enlace = 0, 0, 0, false, nil
local t_lee = -10
local temas = T.lista()

local ESTILOS = {false, "cuadricula", "lista", "carrusel"}
local NOMBRE_ESTILO = {cuadricula = "Cuadrícula", lista = "Lista", carrusel = "Carrusel"}

local FILAS = {
  {icono = "vol", nombre = "Volumen", desliza = true, color = "bien"},
  {icono = "sun", nombre = "Brillo", desliza = true, color = "aviso"},
  {icono = "image", nombre = "Tema", elige = true},
  {icono = "home", nombre = "Estilo del inicio", elige = true},
  {icono = "music", nombre = "Música del menú", elige = true},
  {icono = "usb", nombre = "Conexión con el PC"},
  {icono = "bat", nombre = "Batería"},
  {icono = "info", nombre = "Sistema"},
}
local ALTO, Y0 = 44, 96

local function indice_tema()
  for i, t in ipairs(temas) do if t.id == T.id then return i end end
  return 1
end

local function valor(i)
  if i == 3 then return T.nombre
  elseif i == 4 then
    local p = T.prefs.inicio
    return p and NOMBRE_ESTILO[p] or ("Del tema (" .. (NOMBRE_ESTILO[T.estilo_del_tema] or "?") .. ")")
  elseif i == 5 then return T.prefs.musica == false and "No" or (T.def.musica and "Sí" or "Sí (el tema no trae)")
  elseif i == 6 then return enlace == "cable" and "Por cable USB" or enlace == "wifi" and "Por Wi-Fi" or "Sin conexión"
  elseif i == 7 then return bat .. " %" .. (cargando and " · cargando" or "")
  elseif i == 8 then return "Nora " .. nora.version .. " · PowerVR SGX544" end
end

local function cambia(d)
  if sel == 1 then vol = nora.volumen(math.max(0, math.min(1, vol + d * 0.1))); T.suena("mover")
  elseif sel == 2 then brillo = nora.brillo(math.max(0.02, math.min(1, brillo + d * 0.1)))
  elseif sel == 3 and #temas > 1 then
    local i = (indice_tema() - 1 + d) % #temas + 1
    T.poner(temas[i].id)
    T.suena("mover")
    T.musica()                       -- la música del tema nuevo, para oírla
  elseif sel == 4 then
    local actual = 1
    for i, e in ipairs(ESTILOS) do if e == (T.prefs.inicio or false) then actual = i end end
    local e = ESTILOS[(actual - 1 + d) % #ESTILOS + 1]
    T.preferir("inicio", e or nil)
    T.suena("mover")
  elseif sel == 5 then
    T.preferir("musica", T.prefs.musica == false and nil or false)
    T.suena("mover")
    T.musica()
  end
end

function app.entrar() T.musica() end   -- en Ajustes suena la del tema: así se prueba

function app.boton(b)
  if b == "B" then nora.volver(); return end
  if b == "ARRIBA" and sel > 1 then sel = sel - 1; T.suena("mover") end
  if b == "ABAJO" and sel < #FILAS then sel = sel + 1; T.suena("mover") end
  if b == "IZQUIERDA" then cambia(-1) elseif b == "DERECHA" or b == "A" then cambia(1) end
end

function app.actualiza(dt)
  local objetivo = Y0 + (sel - 1) * ALTO
  if sy == 0 then sy = objetivo end
  sy = sy + (objetivo - sy) * math.min(1, dt * 16)
end

function app.pinta(t)
  if T.revisa() then temas = T.lista() end
  if t - t_lee > 1 then
    t_lee = t
    vol, brillo = nora.volumen(), nora.brillo()
    bat, cargando = nora.bateria()
    enlace = nora.enlace()
  end
  T.pinta_fondo()
  nora.texto("Ajustes", 28, 40, "tit", T.texto)
  nora.caja(20, sy - 6, 600, ALTO - 2, T.radio(14), T.panel)
  for i, f in ipairs(FILAS) do
    local y = Y0 + (i - 1) * ALTO
    local elegida = i == sel
    nora.icono(f.icono, 48, y + 15, T.texto, elegida and 1 or 0.6)
    nora.texto(f.nombre, 76, y + 3, "peq", T.texto)
    if f.desliza then
      local v = i == 1 and vol or brillo
      nora.caja(330, y + 10, 230, 10, T.radio(5), T.riel)
      nora.caja(330, y + 10, math.max(10, 230 * v), 10, T.radio(5), T[f.color])
      nora.circulo(330 + 230 * v, y + 15, 11, T.texto)
      nora.texto(tostring(math.floor(v * 100 + 0.5)), 604, y + 3, "peq", T.tenue, 1, "der")
    elseif f.elige then
      local txt = valor(i)
      local w = nora.ancho(txt, "peq")
      nora.texto(txt, 580, y + 3, "peq", elegida and T.texto or T.suave, 1, "der")
      if elegida then
        nora.texto("<", 580 - w - 16, y + 3, "peq", T.acento, 1, "der")
        nora.texto(">", 598, y + 3, "peq", T.acento, 1, "der")
      end
    else
      nora.texto(valor(i), 604, y + 3, "peq", T.tenue, 1, "der")
    end
  end
  nora.texto("Cruceta: elegir y cambiar · B volver", 28, 446, "peq", T.tenue)
end
