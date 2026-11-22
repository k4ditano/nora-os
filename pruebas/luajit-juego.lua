-- carga tipica de un juego: mover N particulas con rebote, muchas veces
local ffi = require("ffi")
local N, PASOS = 2000, 600   -- 600 fotogramas = 10 s de juego a 60 fps
local function con_ffi()
  local p = ffi.new("struct { float x, y, vx, vy; }[?]", N)
  for i = 0, N - 1 do p[i].x, p[i].y, p[i].vx, p[i].vy = i % 640, i % 480, (i % 7) - 3, (i % 5) - 2 end
  for _ = 1, PASOS do
    for i = 0, N - 1 do
      local q = p[i]
      q.x = q.x + q.vx; q.y = q.y + q.vy
      if q.x < 0 or q.x > 640 then q.vx = -q.vx end
      if q.y < 0 or q.y > 480 then q.vy = -q.vy end
    end
  end
end
local function con_tablas()
  local p = {}
  for i = 1, N do p[i] = {x = i % 640, y = i % 480, vx = (i % 7) - 3, vy = (i % 5) - 2} end
  for _ = 1, PASOS do
    for i = 1, N do
      local q = p[i]
      q.x = q.x + q.vx; q.y = q.y + q.vy
      if q.x < 0 or q.x > 640 then q.vx = -q.vx end
      if q.y < 0 or q.y > 480 then q.vy = -q.vy end
    end
  end
end
local function mide(nombre, f)
  local t = os.clock(); f(); local s = os.clock() - t
  print(string.format("%-26s %.3f s  (%.2f ms por fotograma)", nombre, s, s * 1000 / PASOS))
end
mide("FFI    con JIT", con_ffi); mide("tablas con JIT", con_tablas)
jit.off(); jit.flush()
mide("FFI    sin JIT", con_ffi); mide("tablas sin JIT", con_tablas)
