-- inicio: el lanzador de Nora. Apps y carpetas, en el estilo del tema
-- (cuadrícula, lista o carrusel). A abre, B sale de una carpeta, Y ordena.
--
-- Carpetas: una app puede decir en su info.lua en cuál va (carpeta = "Juegos");
-- lo que ordene el usuario (modo ordenar) manda y se guarda en datos/inicio/orden.
--
-- Modo ordenar (Y): A coge o suelta lo elegido y la cruceta lo mueve; X da las
-- opciones (meter en carpeta, sacar, carpeta nueva, renombrar, deshacer); B sale.
app.barra = true
local json = require "json"
local T = require "tema"
local teclado = require "teclado"
local OCULTAS = {inicio = true, sistema = true}

local todas = {}           -- id -> {id, nombre, icono, color, orden, carpeta}
local elementos = {}       -- lo de arriba: {app = id} | {carpeta = nombre, apps = {id...}}
local dentro = nil         -- la carpeta abierta (un elemento de `elementos`)
local sel, sel_fuera = 1, 1
local scroll, scroll_ver = 0, 0              -- desplazamiento (filas o tarjetas) y el animado
local ax, ay = 0, 0                          -- anillo animado (cuadrícula)
local ordenando, cogido = false, false
local menu = nil                             -- {titulo, opciones = {{texto, fn}}, sel}
local t_vista = 0                            -- cuando se entró en la vista (para la cascada)
local resumen, version_usos, sin_leer = "", -1, 0

local function suave(p) p = math.max(0, math.min(1, p)); return 1 - (1 - p) ^ 3 end

---------------------------------------------------------------- las apps y su orden
local function guardar()
  local l = {}
  for _, e in ipairs(elementos) do
    l[#l + 1] = e.app and {app = e.app} or {carpeta = e.carpeta, apps = e.apps}
  end
  nora.salvar("orden", json.escribe(l))
end

local function busca_carpeta(nombre)
  for _, e in ipairs(elementos) do
    if e.carpeta and e.carpeta:lower() == nombre:lower() then return e end
  end
end

local function cargar()
  todas = {}
  local nuevas = {}
  for _, id in ipairs(nora.lista(nora.apps)) do
    if not OCULTAS[id] and nora.es_carpeta(nora.apps .. "/" .. id) then
      local ok, info = pcall(dofile, nora.apps .. "/" .. id .. "/info.lua")
      info = ok and type(info) == "table" and info or {}
      todas[id] = {id = id, nombre = info.nombre or id, icono = info.icono or "cube", color = info.color,
                   orden = info.orden or 50, carpeta = type(info.carpeta) == "string" and info.carpeta or nil}
    end
  end
  -- lo que guardó el usuario, sin lo que ya no existe
  local guardado = json.lee(nora.cargar("orden") or "") or {}
  local visto = {}
  elementos = {}
  for _, e in ipairs(type(guardado) == "table" and guardado or {}) do
    if type(e) == "table" and e.app and todas[e.app] and not visto[e.app] then
      visto[e.app] = true
      elementos[#elementos + 1] = {app = e.app}
    elseif type(e) == "table" and type(e.carpeta) == "string" then
      local c = {carpeta = e.carpeta, apps = {}}
      for _, id in ipairs(type(e.apps) == "table" and e.apps or {}) do
        if todas[id] and not visto[id] then visto[id] = true; c.apps[#c.apps + 1] = id end
      end
      if #c.apps > 0 then elementos[#elementos + 1] = c end
    end
  end
  -- las que no estaban: donde diga su info.lua, en su orden
  for id, a in pairs(todas) do if not visto[id] then nuevas[#nuevas + 1] = a end end
  table.sort(nuevas, function(a, b) return a.orden < b.orden or (a.orden == b.orden and a.nombre < b.nombre) end)
  for _, a in ipairs(nuevas) do
    if a.carpeta then
      local c = busca_carpeta(a.carpeta)
      if not c then c = {carpeta = a.carpeta, apps = {}}; elementos[#elementos + 1] = c end
      c.apps[#c.apps + 1] = a.id
    else
      elementos[#elementos + 1] = {app = a.id}
    end
  end
end

-- lo que se ve ahora: la lista de arriba o la de la carpeta abierta
local function vista()
  if dentro then
    local l = {}
    for _, id in ipairs(dentro.apps) do l[#l + 1] = {app = id} end
    return l
  end
  return elementos
end
local function lista_real() return dentro and dentro.apps or elementos end
local function cuantas() local n = 0; for _ in pairs(todas) do n = n + 1 end; return n end

local function leer_resumen()
  local s, v = nora.dato("usos.fichas")
  if v == version_usos and resumen ~= "" then return end
  version_usos = v
  local peor
  local fichas = s and json.lee(s)
  if type(fichas) == "table" and type(fichas[1]) == "table" and type(fichas[1].barras) == "table" then
    for _, b in ipairs(fichas[1].barras) do
      local pct = tonumber(b.pct)
      if pct and pct >= 0 and (not peor or pct > peor) then peor = pct end
    end
  end
  local n = cuantas()
  resumen = peor and string.format("Claude te deja un %d %% · %d apps", 100 - peor, n) or (n .. " apps")
end

local function saludo()
  local h = tonumber(os.date("%H"))
  return h < 6 and "Buenas noches" or h < 14 and "Buenos días" or h < 21 and "Buenas tardes" or "Buenas noches"
end

---------------------------------------------------------------- abrir, entrar, salir de carpetas
local function entrar_en(c)
  dentro, sel_fuera, sel, scroll, t_vista = c, sel, 1, 0, nora.ahora()
  nora.guardar("inicio.dentro", c.carpeta)
end
local function salir_de_carpeta()
  local c = dentro
  dentro, t_vista = nil, nora.ahora()
  nora.guardar("inicio.dentro", nil)
  sel = 1
  for i, e in ipairs(elementos) do if e == c then sel = i end end
  if not c then sel = sel_fuera end
end

function app.entrar()
  cargar(); leer_resumen()
  T.musica()
  t_vista = nora.ahora()
  -- si se abrió una app desde una carpeta, se vuelve a ella
  local en = nora.dato("inicio.dentro")
  local c = en and busca_carpeta(en)
  if c then dentro = c end
  local s = tonumber((nora.dato(dentro and "inicio.sel_dentro" or "inicio.sel"))) or 1
  sel = math.max(1, math.min(#vista(), s))
  if dentro then sel_fuera = tonumber((nora.dato("inicio.sel"))) or 1 end
end

local function abrir(e)
  if e.carpeta then T.suena("abrir"); entrar_en(e); return end
  nora.guardar("inicio.sel", tostring(dentro and sel_fuera or sel))
  nora.guardar("inicio.sel_dentro", dentro and tostring(sel) or nil)
  T.suena("abrir")
  nora.abrir(e.app)
end

---------------------------------------------------------------- ordenar
local function mover(de, a)          -- en la lista que se ve
  local l = lista_real()
  if a < 1 or a > #l or a == de then return false end
  table.insert(l, a, table.remove(l, de))
  return true
end

local function meter_en(id, c)       -- saca la app de donde esté y la mete en la carpeta c
  if dentro then table.remove(dentro.apps, sel) else table.remove(elementos, sel) end
  c.apps[#c.apps + 1] = id
  if dentro and #dentro.apps == 0 then
    for i, e in ipairs(elementos) do if e == dentro then table.remove(elementos, i) end end
    dentro = nil
  end
  sel = math.max(1, math.min(sel, #lista_real()))
  guardar()
end

local function opciones_de(e)
  local o = {}
  if e.app then
    local id = e.app
    for _, c in ipairs(elementos) do
      if c.carpeta and c ~= dentro then
        o[#o + 1] = {"Meter en «" .. c.carpeta .. "»", function() meter_en(id, c) end}
      end
    end
    o[#o + 1] = {"Carpeta nueva con esta app", function()
      teclado.abrir("Nombre de la carpeta nueva", todas[id].carpeta or "", function(nombre)
        if not nombre then return end
        local c = busca_carpeta(nombre)
        if not c then
          c = {carpeta = nombre, apps = {}}
          table.insert(elementos, dentro and #elementos + 1 or sel, c)
          if not dentro then sel = sel + 1 end      -- la app se ha corrido un sitio
        end
        meter_en(id, c)
        if not dentro then for i, e in ipairs(elementos) do if e == c then sel = i end end end
      end)
    end}
    if dentro then
      o[#o + 1] = {"Sacar de la carpeta", function()
        table.remove(dentro.apps, sel)
        local pos = 1
        for i, c in ipairs(elementos) do if c == dentro then pos = i end end
        table.insert(elementos, pos + 1, {app = id})
        if #dentro.apps == 0 then table.remove(elementos, pos); pos = pos - 1 end
        dentro = nil
        sel = pos + 1
        guardar()
      end}
    end
  else
    local c = e
    o[#o + 1] = {"Abrir", function() entrar_en(c) end}
    o[#o + 1] = {"Cambiar el nombre", function()
      teclado.abrir("Nombre de la carpeta", c.carpeta, function(nombre)
        if nombre then c.carpeta = nombre; guardar() end
      end)
    end}
    o[#o + 1] = {"Deshacer la carpeta (las apps salen)", function()
      local pos = sel
      table.remove(elementos, pos)
      for i, id in ipairs(c.apps) do table.insert(elementos, pos + i - 1, {app = id}) end
      guardar()
    end}
  end
  o[#o + 1] = {"Cancelar", function() end}
  return o
end

---------------------------------------------------------------- botones
local function columnas()
  local e = T.estilo.inicio
  return e == "cuadricula" and 4 or 1
end

function app.boton(b)
  if teclado.boton(b) then return end
  if menu then
    if b == "ABAJO" then menu.sel = menu.sel % #menu.opciones + 1; T.suena("mover")
    elseif b == "ARRIBA" then menu.sel = (menu.sel - 2) % #menu.opciones + 1; T.suena("mover")
    elseif b == "A" then local fn = menu.opciones[menu.sel][2]; menu = nil; T.suena("abrir"); fn()
    elseif b == "B" then menu = nil; T.suena("volver") end
    return
  end
  local l = vista()
  local n, cols = #l, columnas()
  local estilo = T.estilo.inicio
  local paso = {DERECHA = 1, IZQUIERDA = -1, ABAJO = cols, ARRIBA = -cols}
  if estilo == "lista" then paso = {ABAJO = 1, ARRIBA = -1, DERECHA = 4, IZQUIERDA = -4}
  elseif estilo == "carrusel" then paso = {DERECHA = 1, IZQUIERDA = -1, ABAJO = 1, ARRIBA = -1} end
  if paso[b] and n > 0 then
    local nuevo = math.max(1, math.min(n, sel + paso[b]))
    if nuevo ~= sel then
      if cogido then
        if mover(sel, nuevo) then sel = nuevo; guardar() end
      else
        sel = nuevo
      end
      T.suena("mover")
    end
    return
  end
  if b == "Y" then
    ordenando, cogido = not ordenando, false
    T.suena(ordenando and "menu" or "volver")
    return
  end
  if b == "B" then
    if cogido then cogido = false; T.suena("volver")
    elseif dentro then salir_de_carpeta(); T.suena("volver")
    elseif ordenando then ordenando = false; T.suena("volver") end
    return
  end
  local e = l[sel]
  if not e then return end
  if b == "A" then
    if ordenando then cogido = not cogido; T.suena(cogido and "abrir" or "volver")
    else abrir(e) end
  elseif b == "X" and ordenando then
    cogido = false
    local real = dentro and e or elementos[sel]
    menu = {titulo = real.carpeta or todas[real.app].nombre, opciones = opciones_de(real), sel = 1}
    T.suena("menu")
  end
end

---------------------------------------------------------------- dibujo
local Y0, Y1 = 132, 436          -- la zona de las apps (debajo, las pistas de botones)

local function fondo_icono(cx, cy, r, color, alfa)
  local f = T.estilo.icono
  if f == "circulo" then nora.circulo(cx, cy, r, color, alfa)
  elseif f == "cuadrado" then nora.caja(cx - r, cy - r, 2 * r, 2 * r, T.radio(r * 0.4), color, alfa) end
end

-- el icono de un elemento: el de la app, o una carpeta con sus apps en pequeño
local function pinta_icono(e, cx, cy, r, alfa, sobre)
  if e.app then
    local a = todas[e.app]
    local col = T.color_app(a.color)
    if T.estilo.icono == "nada" then nora.icono(a.icono, cx, cy, sobre or col, alfa)
    else fondo_icono(cx, cy, r, col, alfa); nora.icono(a.icono, cx, cy, sobre or (T.estilo.color_iconos == "acento" and T.sobre_acento or T.sobre_app), alfa) end
    return
  end
  nora.caja(cx - r - 4, cy - r - 4, 2 * r + 8, 2 * r + 8, T.radio(r * 0.5), T.riel, alfa)
  for i = 1, math.min(4, #e.apps) do
    local a = todas[e.apps[i]]
    local mx = cx + (i % 2 == 1 and -1 or 1) * r * 0.45
    local my = cy + (i <= 2 and -1 or 1) * r * 0.45
    fondo_icono(mx, my, r * 0.36, T.color_app(a.color), alfa)
  end
end

local function nombre_de(e) return e.app and todas[e.app].nombre or e.carpeta end
local function sub_de(e) return e.carpeta and (#e.apps .. (#e.apps == 1 and " app" or " apps")) or nil end

local function insignia(e, x, y, alfa)          -- avisos sin ver, en el icono de Avisos
  if not (e.app == "avisos" and sin_leer > 0) then return end
  local txt = sin_leer > 9 and "9+" or tostring(sin_leer)
  local w = math.max(26, nora.ancho(txt, "peq") + 14)
  nora.caja(x - w / 2, y, w, 26, 13, T.mal, alfa)
  nora.texto(txt, x, y + 2, "peq", 0xffffff, alfa, "centro")
end

local function temblor(t, i) return (ordenando and not cogido) and math.sin(t * 18 + i) * 1.5 or 0 end

-- cuadrícula de 4: tarjetas de 132, con scroll por filas
local function pinta_cuadricula(l, t, ve)
  local tw, gx, gy = 132, 18, 16
  local x0 = (640 - (4 * tw + 3 * gx)) / 2
  local fila = math.floor((sel - 1) / 4)
  if fila < scroll then scroll = fila elseif fila > scroll + 1 then scroll = fila - 1 end
  local function pos(i) return x0 + ((i - 1) % 4) * (tw + gx), Y0 + 8 + (math.floor((i - 1) / 4) - scroll_ver) * (tw + gy) end
  local sx, sy = pos(sel)
  if ax == 0 then ax, ay = sx, sy end
  local k = math.min(1, ve * 16)
  ax, ay = ax + (sx - ax) * k, ay + (sy - ay) * k
  if T.estilo.anillo == "borde" and #l > 0 then
    T.sombrea(ax - 6, ay - 6, 144, 144, 28, 10, 0.25)
    nora.caja(ax - 5, ay - 5, 142, 142, T.radio(26), cogido and T.aviso or (ordenando and T.acento or T.texto))
    nora.caja(ax - 2, ay - 2, 136, 136, T.radio(24), T.fondo)
  end
  for i, e in ipairs(l) do
    local x, y = pos(i)
    if y > Y0 - tw and y < Y1 then
      local entra = suave((nora.ahora() - t_vista - 0.04 * i) / 0.35)
      y = y + 16 * (1 - entra) + temblor(t, i)
      local elegido = i == sel
      local relleno = elegido and T.estilo.anillo == "relleno"
      nora.caja(x, y, tw, tw, T.radio(22), relleno and T.acento or T.panel, entra)
      pinta_icono(e, x + 66, y + 50, 30 + (elegido and 2 * math.sin(t * 4) or 0), entra, relleno and T.panel or nil)
      local nombre = nombre_de(e)
      nora.texto(nombre, x + 66, y + 92, "peq", relleno and T.sobre_acento or T.texto, entra, "centro")
      insignia(e, x + tw - 10, y - 6, entra)
    end
  end
end

-- lista: filas de 60, con scroll
local function pinta_lista(l, t, ve)
  local fh, gap = 60, 8
  local visibles = math.floor((Y1 - Y0) / (fh + gap))
  if sel - 1 < scroll then scroll = sel - 1 elseif sel > scroll + visibles then scroll = sel - visibles end
  for i, e in ipairs(l) do
    local y = Y0 + 4 + (i - 1 - scroll_ver) * (fh + gap)
    if y > Y0 - fh and y < Y1 then
      local entra = suave((nora.ahora() - t_vista - 0.04 * i) / 0.3)
      local x = 24 + 20 * (1 - entra) + temblor(t, i)
      local elegido = i == sel
      local relleno = elegido and T.estilo.anillo == "relleno"
      if elegido and not relleno then nora.caja(x - 3, y - 3, 598, fh + 6, T.radio(18), cogido and T.aviso or T.acento, entra) end
      nora.caja(x, y, 592, fh, T.radio(16), relleno and (cogido and T.aviso or T.acento) or T.panel, entra)
      pinta_icono(e, x + 36, y + fh / 2, 20, entra, relleno and T.panel or nil)
      nora.texto(nombre_de(e), x + 72, y + 16, "peq", relleno and T.sobre_acento or T.texto, entra)
      local sub = sub_de(e)
      if sub then nora.texto(sub, x + 572, y + 16, "peq", relleno and T.sobre_acento or T.tenue, entra, "der") end
      insignia(e, x + 572 - (sub and 90 or 20), y + 17, entra)
    end
  end
end

-- carrusel: tarjetas grandes, la elegida en el centro
local function pinta_carrusel(l, t, ve)
  local cw, ch, gap = 210, 244, 30
  scroll = sel - 1
  for i, e in ipairs(l) do
    local d = (i - 1) - scroll_ver
    if math.abs(d) < 3 then
      local escala = 1 - math.min(1, math.abs(d)) * 0.18
      local w, h = cw * escala, ch * escala
      local cx = 320 + d * (cw * 0.82 + gap)
      local y = Y0 + 12 + (ch - h) / 2 + temblor(t, i)
      local alfa = math.max(0, 1 - math.abs(d) * 0.35) * suave((nora.ahora() - t_vista) / 0.3)
      local elegido = i == sel
      local relleno = elegido and T.estilo.anillo == "relleno"
      if elegido then
        T.sombrea(cx - w / 2, y + 8, w, h, 28, 22, 0.35 * alfa)
        if not relleno then nora.caja(cx - w / 2 - 4, y - 4, w + 8, h + 8, T.radio(30), cogido and T.aviso or T.acento, alfa) end
      end
      nora.caja(cx - w / 2, y, w, h, T.radio(26), relleno and T.acento or T.panel, alfa)
      pinta_icono(e, cx, y + h * 0.4, 56 * escala, alfa, relleno and T.panel or nil)
      nora.texto(nombre_de(e), cx, y + h * 0.72, escala > 0.95 and "med" or "peq",
                 relleno and T.sobre_acento or T.texto, alfa, "centro")
      local sub = sub_de(e)
      if sub and escala > 0.95 then nora.texto(sub, cx, y + h * 0.72 + 40, "peq", T.tenue, alfa, "centro") end
      insignia(e, cx + w / 2 - 14, y - 8, alfa)
    end
  end
  if #l > 1 then
    nora.texto(sel .. " / " .. #l, 320, Y1 - 22, "peq", T.tenue, 1, "centro")
  end
end

local function pistas(l)
  local p
  if menu then p = {{"A", "elegir"}, {"B", "cancelar"}}
  elseif ordenando then
    p = cogido and {{"A", "soltar"}, {"B", "dejar"}} or {{"A", "coger"}, {"X", "opciones"}, {"Y", "terminar"}}
  else
    p = {{"A", "abrir"}}
    if dentro then p[#p + 1] = {"B", "volver"} end
    p[#p + 1] = {"Y", "ordenar"}
    p[#p + 1] = {"M", "menú"}
  end
  local x = 20
  for _, q in ipairs(p) do
    nora.circulo(x + 11, 459, 11, T.riel)
    nora.texto(q[1], x + 11, 449, "peq", T.texto, 1, "centro")
    x = x + 28 + nora.texto(q[2], x + 28, 448, "peq", T.tenue) + 22
  end
end

local function pinta_menu(t)
  if not menu then return end
  nora.caja(-2, -2, 644, 484, 0, T.velo, 0.55)
  local h = 70 + #menu.opciones * 46
  local x, y, w = 70, (480 - h) / 2, 500
  T.sombrea(x, y + 8, w, h, 24, 20, 0.5)
  nora.caja(x, y, w, h, T.radio(24), T.panel)
  nora.texto(menu.titulo, x + 28, y + 20, "med", T.texto)
  for i, o in ipairs(menu.opciones) do
    local oy = y + 66 + (i - 1) * 46
    if i == menu.sel then nora.caja(x + 14, oy - 6, w - 28, 42, T.radio(14), T.acento) end
    nora.texto(o[1], x + 32, oy, "peq", i == menu.sel and T.sobre_acento or T.texto)
  end
end

local ultimo_t = 0
function app.pinta(t)
  if T.revisa() then t_vista = nora.ahora(); scroll, scroll_ver, ax = 0, 0, 0; T.musica() end
  local ve = math.min(0.1, math.max(0, t - ultimo_t)); ultimo_t = t
  leer_resumen()
  sin_leer = tonumber((nora.dato("avisos.sin_leer"))) or 0
  T.pinta_fondo()
  local l = vista()
  sel = math.max(1, math.min(sel, math.max(1, #l)))
  scroll_ver = scroll_ver + (scroll - scroll_ver) * math.min(1, ve * 12)
  -- cabecera
  local e = suave(t / 0.5)
  if dentro then
    nora.icono("folder", 44, 80, T.acento)
    nora.texto(dentro.carpeta, 74, 56, "tit", T.texto)
    nora.texto(#dentro.apps .. (#dentro.apps == 1 and " app" or " apps"), 30, 104, "peq", T.tenue)
  else
    nora.texto(saludo(), 28, 52 + 10 * (1 - e), "tit", T.texto, e)
    nora.texto(ordenando and "Ordenando: A coge una app o carpeta, X para más" or resumen, 30, 104, "peq",
               ordenando and T.acento or T.tenue, e)
  end
  if #l == 0 then
    nora.texto("No hay apps en " .. nora.apps, 320, 260, "peq", T.tenue, 1, "centro")
  else
    nora.recorte(0, Y0 - 8, 640, Y1 - Y0 + 8)
    local estilo = T.estilo.inicio
    if estilo == "lista" then pinta_lista(l, t, ve)
    elseif estilo == "carrusel" then pinta_carrusel(l, t, ve)
    else pinta_cuadricula(l, t, ve) end
    nora.recorte()
  end
  pistas(l)
  pinta_menu(t)
  teclado.pinta(t)
end
