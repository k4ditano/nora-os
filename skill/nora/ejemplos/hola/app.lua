-- Hola: la app más pequeña. Desde el PC:
--   nora-puente vigilar skill/nora/ejemplos/hola
-- y cada vez que guardes este archivo, la consola la recarga.
local x, y, vx, vy = 320, 240, 140, 110
local COLOR = 0xf59e0b

function app.boton(b)
  if b == "B" then nora.volver() end
  if b == "A" then vx, vy = -vx, -vy; nora.suena(nora.carpeta .. "/../../sonidos/mover.wav") end
end

function app.actualiza(dt)
  x, y = x + vx * dt, y + vy * dt
  if x < 30 or x > nora.W - 30 then vx = -vx end
  if y < 70 or y > nora.H - 30 then vy = -vy end
end

function app.pinta(t)
  nora.texto("¡Hola desde el PC!", 320, 40, "tit", 0xf3f4f6, 1, "centro")
  nora.circulo(x, y, 26, COLOR)
  nora.texto("A: rebota · B: volver", 320, 440, "peq", 0x9ca3af, 1, "centro")
end
