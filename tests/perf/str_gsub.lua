-- Template substitution over real text.
local C = dofile("tests/perf/corpus.lua")
local text = C.text:sub(1, 60000)
local acc = 0
for iter = 1, 700 do
  local out = text:gsub("[%a_][%w_]*", function(w)
    if #w > 6 then return w:upper() end
    return w
  end)
  acc = acc + #out
end
print(acc)
