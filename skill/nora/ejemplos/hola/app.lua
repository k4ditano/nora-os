-- Hola: la app más pequeña. Sigue el tema que tenga puesto el usuario.
-- Desde el PC:  nora-puente vigilar skill/nora/ejemplos/hola
-- y cada vez que guardes este archivo, la consola la recarga.
local T = require "tema"
local x, y, vx, vy = 320, 240, 140, 110

function app.boton(b)
  if b == "B" then nora.volver() end
  if b == "A" then vx, vy = -vx, -vy; T.suena("mover") end
end

function app.actualiza(dt)
  x, y = x + vx * dt, y + vy * dt
  if x < 30 or x > nora.W - 30 then vx = -vx end
  if y < 110 or y > nora.H - 70 then vy = -vy end
end

function app.pinta(t)
  T.revisa()
  T.pinta_fondo()
  nora.texto("¡Hola desde el PC!", 320, 40, "tit", T.texto, 1, "centro")
  nora.circulo(x, y, 26, T.acento)
  nora.texto("A: rebota · B: volver", 320, 440, "peq", T.tenue, 1, "centro")
end
