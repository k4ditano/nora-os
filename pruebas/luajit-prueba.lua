-- prueba de LuaJIT en la RG35XX: version, JIT, FFI y velocidad
local ffi = require("ffi")
print(jit.version, jit.arch, "JIT:", jit.status())
ffi.cdef[[ int getpid(void); int printf(const char *fmt, ...); double sqrt(double); ]]
ffi.C.printf("FFI -> printf de bionic, pid %d\n", ffi.C.getpid())
print("FFI -> sqrt(2) =", ffi.C.sqrt(2))

local function fib(n) if n < 2 then return n end return fib(n-1) + fib(n-2) end
local function bucle()            -- numeros de coma flotante, como un juego
  local s = 0
  for i = 1, 5e6 do s = s + math.sin(i) * 0.5 end
  return s
end
local function mide(nombre, f)
  local t = os.clock(); local r = f(); print(string.format("%-22s %.3f s", nombre, os.clock() - t))
end
mide("fib(30) con JIT", function() return fib(30) end)
mide("5M senos con JIT", bucle)
jit.off()
mide("fib(30) sin JIT", function() return fib(30) end)
mide("5M senos sin JIT", bucle)
