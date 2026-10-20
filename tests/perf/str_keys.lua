-- Word frequency count over real text: string creation plus string-keyed
-- table lookups. The most common real-world Lua string workload.
local C = dofile("tests/perf/corpus.lua")
local words = C.getwords()
local total = 0
for iter = 1, 900 do
  local freq = {}
  for i = 1, #words do
    local w = words[i]
    freq[w] = (freq[w] or 0) + 1
  end
  local c = 0
  for k, v in pairs(freq) do c = c + v end
  total = total + c
end
print(total)
