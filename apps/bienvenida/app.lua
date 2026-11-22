-- Bienvenida: lo primero que se ve al estrenar Nora. Elegir tema, conectar con
-- el PC y cuatro trucos. Sale una vez (datos/bienvenida/hecho); el inicio la abre.
local T = require "tema"
local W, H = nora.W, nora.H
local paso, t_paso = 1, 0
local temas, sel_tema = {}, 1
local PASOS = 4

local function suave(p) p = math.max(0, math.min(1, p)); return 1 - (1 - p) ^ 3 end
local function cambia_paso(n)
  paso, t_paso = math.max(1, math.min(PASOS, n)), nora.ahora()
end

function app.entrar()
  temas = T.lista()
  for i, t in ipairs(temas) do
    if t.id == T.id then sel_tema = i end
    local ok, def = pcall(dofile, T.carpeta_temas .. "/" .. t.id .. "/tema.lua")   -- sus colores, para la muestra
    local c = ok and type(def) == "table" and type(def.colores) == "table" and def.colores or {}
    t.c = {fondo = c.fondo or 0x0e1017, panel = c.panel or 0x1a1d29, acento = c.acento or 0x8b7cf6,
           texto = c.texto or 0xf3f4f8, tenue = c.tenue or 0x8b90a5}
    t.radio = ok and type(def) == "table" and type(def.estilo) == "table" and def.estilo.radio or 1
  end
  t_paso = nora.ahora()
  T.musica()
end

local function terminar()
  nora.salvar("hecho", os.date("%Y-%m-%d %H:%M"))
  T.suena("abrir")
  nora.volver()
end

function app.boton(b)
  if paso == 2 and (b == "IZQUIERDA" or b == "DERECHA" or b == "ARRIBA" or b == "ABAJO") and #temas > 0 then
    local d = (b == "DERECHA" or b == "ABAJO") and 1 or -1
    sel_tema = (sel_tema - 1 + d) % #temas + 1
    T.poner(temas[sel_tema].id)
    T.suena("mover")
    T.musica()
    return
  end
  if b == "A" or b == "START" then
    if paso == PASOS then terminar() else T.suena("abrir"); cambia_paso(paso + 1) end
  elseif b == "B" and paso > 1 then T.suena("volver"); cambia_paso(paso - 1)
  elseif b == "SELECT" then terminar() end                -- saltársela
end

local function centrado(s, y, f, c, a) nora.texto(s, W / 2, y, f, c, a or 1, "centro") end

local function puntos()
  for i = 1, PASOS do
    local w = i == paso and 26 or 10
    local x = W / 2 - (PASOS * 18) / 2 + (i - 1) * 18 + (i > paso and 16 or 0)
    nora.caja(x, 440, w, 10, T.radio(5), i == paso and T.acento or T.riel)
  end
end

local function pista(txt) centrado(txt, 404, "peq", T.tenue) end

function app.pinta(t)
  T.revisa()
  T.pinta_fondo()
  local e = suave((nora.ahora() - t_paso) / 0.45)
  local dy = 16 * (1 - e)
  if paso == 1 then
    nora.circulo(W / 2, 150 + dy, 64 + 3 * math.sin(t * 2), T.acento, e)
    nora.icono("smile", W / 2, 150 + dy, T.sobre_acento, e)
    centrado("Hola, soy Nora", 240 + dy, "tit", T.texto, e)
    centrado("Tu consola, a tu manera. Vamos a dejarla lista.", 308 + dy, "peq", T.suave, e)
    pista("A empezar  ·  SELECT saltar")
  elseif paso == 2 then
    centrado("Elige cómo se ve", 40 + dy, "tit", T.texto, e)
    centrado("Se cambia al momento; luego también en Ajustes", 104 + dy, "peq", T.tenue, e)
    local n = #temas
    for i, tm in ipairs(temas) do
      local x = W / 2 + (i - (n + 1) / 2) * 190 - 80
      local y = 160 + dy
      local elegido = i == sel_tema
      if elegido then nora.caja(x - 4, y - 4, 168, 188, T.radio(22), T.acento, e) end
      -- una muestra del tema: su fondo, una tarjeta, su color y sus letras
      local c, r = tm.c, tm.radio
      nora.caja(x, y, 160, 180, 18 * r, c.fondo, e)
      nora.caja(x + 14, y + 16, 132, 70, 14 * r, c.panel, e)
      nora.circulo(x + 46, y + 51, 18, c.acento, e)
      nora.caja(x + 74, y + 40, 58, 8, 4 * r, c.texto, e)
      nora.caja(x + 74, y + 56, 40, 8, 4 * r, c.tenue, e)
      nora.texto(tm.nombre, x + 80, y + 104, "med", c.texto, e, "centro")
      if elegido then nora.icono("check", x + 80, y + 154, c.acento, e) end
    end
    pista("Cruceta elegir  ·  A seguir  ·  B atrás")
  elseif paso == 3 then
    local c = nora.conectado()
    centrado("Conéctala con tu PC", 40 + dy, "tit", T.texto, e)
    nora.caja(60, 120 + dy, 520, 220, T.radio(20), T.panel, e)
    nora.icono("usb", 104, 168 + dy, T.acento, e)
    nora.texto("Instala nora-puente en el PC", 140, 150 + dy, "peq", T.texto, e)
    nora.texto("github.com/k4ditano/nora-puente", 140, 180 + dy, "peq", T.tenue, e)
    nora.icono("send", 104, 240 + dy, T.acento, e)
    nora.texto("Conecta el cable USB-C: hora, avisos,", 140, 222 + dy, "peq", T.texto, e)
    nora.texto("apps y juegos desde el PC", 140, 252 + dy, "peq", T.texto, e)
    nora.circulo(104, 304 + dy, 10, c and T.bien or T.riel, e)
    nora.texto(c and "Conectada con el PC" or "Sin PC todavía (puedes hacerlo luego)", 140, 292 + dy, "peq",
               c and T.bien or T.tenue, e)
    pista("A seguir  ·  B atrás")
  else
    centrado("Cuatro trucos", 40 + dy, "tit", T.texto, e)
    local trucos = {{"cog", "MENU", "menú rápido: volumen, brillo, apagar"},
                    {"home", "B", "vuelve al inicio desde cualquier app"},
                    {"folder", "Y", "en el inicio, ordena apps y carpetas"},
                    {"power", "Encendido", "toque: reposo  ·  mantener: apagar"}}
    for i, tr in ipairs(trucos) do
      local y = 120 + (i - 1) * 66 + dy
      nora.caja(40, y, 560, 56, T.radio(16), T.panel, e)
      nora.icono(tr[1], 76, y + 28, T.acento, e)
      nora.texto(tr[2], 108, y + 14, "peq", T.texto, e)
      nora.texto(tr[3], 108 + nora.ancho(tr[2], "peq") + 14, y + 14, "peq", T.tenue, e)
    end
    pista("A empezar a usar Nora")
  end
  puntos()
end
