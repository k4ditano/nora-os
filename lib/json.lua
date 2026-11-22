-- json: leer y escribir JSON en Lua puro, para el protocolo con el PC.
--   local json = require "json"
--   json.lee('{"a":[1,2]}')  -> {a = {1, 2}}     (nil, error si no es JSON)
--   json.escribe({a = 1})    -> '{"a":1}'
-- null se lee como json.nulo. Una tabla vacia se escribe como [] (lista).
local json = {nulo = setmetatable({}, {__tostring = function() return "null" end})}

local byte, sub, char, find = string.byte, string.sub, string.char, string.find
local concat, floor = table.concat, math.floor

---------------------------------------------------------------- leer
local function utf8(c)
  if c < 0x80 then return char(c)
  elseif c < 0x800 then return char(0xc0 + floor(c / 64), 0x80 + c % 64)
  elseif c < 0x10000 then return char(0xe0 + floor(c / 4096), 0x80 + floor(c / 64) % 64, 0x80 + c % 64) end
  return char(0xf0 + floor(c / 262144), 0x80 + floor(c / 4096) % 64, 0x80 + floor(c / 64) % 64, 0x80 + c % 64)
end
local ESCAPE = {['"'] = '"', ["\\"] = "\\", ["/"] = "/", b = "\b", f = "\f", n = "\n", r = "\r", t = "\t"}

local valor
local function blanco(s, i)
  local _, j = find(s, "^[ \t\r\n]*", i)
  return j + 1
end
local function error_en(s, i, que) error(string.format("JSON: %s en la posición %d", que, i), 0) end

local function cadena(s, i)                  -- s[i] es la comilla
  local partes, j = {}, i + 1
  while true do
    local a, b = find(s, '["\\]', j)
    if not a then error_en(s, i, "cadena sin cerrar") end
    partes[#partes + 1] = sub(s, j, a - 1)
    if sub(s, a, a) == '"' then return concat(partes), a + 1 end
    local e = sub(s, a + 1, a + 1)
    if e == "u" then
      local c = tonumber(sub(s, a + 2, a + 5), 16)
      if not c then error_en(s, a, "\\u mal escrito") end
      j = a + 6
      if c >= 0xd800 and c < 0xdc00 and sub(s, j, j + 1) == "\\u" then   -- par sustituto
        local c2 = tonumber(sub(s, j + 2, j + 5), 16)
        if c2 and c2 >= 0xdc00 and c2 < 0xe000 then
          c = 0x10000 + (c - 0xd800) * 1024 + (c2 - 0xdc00)
          j = j + 6
        end
      end
      partes[#partes + 1] = utf8(c)
    else
      partes[#partes + 1] = ESCAPE[e] or error_en(s, a, "escape raro")
      j = a + 2
    end
  end
end

valor = function(s, i)
  i = blanco(s, i)
  local c = sub(s, i, i)
  if c == "{" then
    local t = {}
    i = blanco(s, i + 1)
    if sub(s, i, i) == "}" then return t, i + 1 end
    while true do
      if sub(s, i, i) ~= '"' then error_en(s, i, "se esperaba una clave") end
      local k
      k, i = cadena(s, i)
      i = blanco(s, i)
      if sub(s, i, i) ~= ":" then error_en(s, i, "se esperaba ':'") end
      t[k], i = valor(s, i + 1)
      i = blanco(s, i)
      c = sub(s, i, i)
      if c == "}" then return t, i + 1 end
      if c ~= "," then error_en(s, i, "se esperaba ',' o '}'") end
      i = blanco(s, i + 1)
    end
  elseif c == "[" then
    local t, n = {}, 0
    i = blanco(s, i + 1)
    if sub(s, i, i) == "]" then return t, i + 1 end
    while true do
      n = n + 1
      t[n], i = valor(s, i)
      i = blanco(s, i)
      c = sub(s, i, i)
      if c == "]" then return t, i + 1 end
      if c ~= "," then error_en(s, i, "se esperaba ',' o ']'") end
      i = i + 1
    end
  elseif c == '"' then return cadena(s, i)
  elseif sub(s, i, i + 3) == "true" then return true, i + 4
  elseif sub(s, i, i + 4) == "false" then return false, i + 5
  elseif sub(s, i, i + 3) == "null" then return json.nulo, i + 4
  end
  local a, b = find(s, "^-?%d+%.?%d*[eE]?[-+]?%d*", i)
  if not a then error_en(s, i, "valor desconocido") end
  return tonumber(sub(s, a, b)), b + 1
end

function json.lee(s)
  if type(s) ~= "string" then return nil, "JSON: no es texto" end
  local ok, v, i = pcall(valor, s, 1)
  if not ok then return nil, v end
  if blanco(s, i) <= #s then return nil, "JSON: sobra texto en la posición " .. i end
  return v
end

---------------------------------------------------------------- escribir
local ESC = {['"'] = '\\"', ["\\"] = "\\\\", ["\b"] = "\\b", ["\f"] = "\\f", ["\n"] = "\\n", ["\r"] = "\\r", ["\t"] = "\\t"}
local function esc(s)
  return '"' .. s:gsub('[%c"\\]', function(c) return ESC[c] or string.format("\\u%04x", byte(c)) end) .. '"'
end

local escribe
escribe = function(v, sal)
  local t = type(v)
  if v == nil or v == json.nulo then sal[#sal + 1] = "null"
  elseif t == "boolean" then sal[#sal + 1] = tostring(v)
  elseif t == "number" then
    if v ~= v or v == math.huge or v == -math.huge then sal[#sal + 1] = "null"
    elseif v == floor(v) and v > -1e15 and v < 1e15 then sal[#sal + 1] = string.format("%d", v)
    else sal[#sal + 1] = string.format("%.14g", v) end
  elseif t == "string" then sal[#sal + 1] = esc(v)
  elseif t == "table" then
    local n = #v
    if n > 0 or next(v) == nil then
      sal[#sal + 1] = "["
      for i = 1, n do
        if i > 1 then sal[#sal + 1] = "," end
        escribe(v[i], sal)
      end
      sal[#sal + 1] = "]"
    else
      sal[#sal + 1] = "{"
      local primero = true
      for k, x in pairs(v) do
        if not primero then sal[#sal + 1] = "," end
        primero = false
        sal[#sal + 1] = esc(tostring(k))
        sal[#sal + 1] = ":"
        escribe(x, sal)
      end
      sal[#sal + 1] = "}"
    end
  else error("JSON: no sé escribir un " .. t, 0) end
end

function json.escribe(v)
  local sal = {}
  escribe(v, sal)
  return concat(sal)
end

return json
