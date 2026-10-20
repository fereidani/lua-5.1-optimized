-- Substring extraction: each string.sub result is a fresh interned string.
-- Models field splitting in a CSV or log parser.
local C = dofile("tests/perf/corpus.lua")
local text = C.text
local acc = 0
for iter = 1, 300 do
  local i = 1
  local len = #text
  while i < len - 24 do
    local s = text:sub(i, i + 11)
    acc = acc + #s
    i = i + 7
  end
end
print(acc)
