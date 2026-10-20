-- Compile Lua source repeatedly. The lexer interns every identifier and
-- string literal, so this is end-to-end "load a real program" cost.
local src = {}
for i = 1, 400 do  -- each handler wrapped in do..end to stay under the 200-local limit
  src[#src+1] = string.format(
    "do local function handler_%d(request_%d, response_%d)\n" ..
    "  local payload_%d = request_%d.body_field_%d\n" ..
    "  response_%d.status_code_%d = payload_%d and 200 or 404\n" ..
    "  return response_%d\n" ..
    "end end\n", i,i,i, i,i,i, i,i,i, i)
end
local text = table.concat(src)
local n = 0
for iter = 1, 1000 do
  local f = assert(loadstring(text))
  n = n + 1
end
print(n)
