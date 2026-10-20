-- Serialize and re-parse a nested table, JSON-ish. Real application shape:
-- many medium strings created and immediately interned.
local C = dofile("tests/perf/corpus.lua")
local words = C.getwords()
local rec = {}
for i = 1, 3000 do
  rec[i] = { name = words[(i % #words) + 1], id = i, tag = words[((i*7) % #words) + 1] }
end
local acc = 0
for iter = 1, 600 do
  local buf = {}
  for i = 1, #rec do
    local r = rec[i]
    buf[#buf+1] = '{"name":"' .. r.name .. '","id":' .. r.id .. ',"tag":"' .. r.tag .. '"}'
  end
  local doc = table.concat(buf, ",")
  local cnt = 0
  for name in doc:gmatch('"name":"([^"]*)"') do cnt = cnt + #name end
  acc = acc + cnt
end
print(acc)
