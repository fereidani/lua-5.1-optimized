-- Real application: rxi/json.lua (a widely used pure-Lua JSON library).
-- Encode + decode a realistic API-shaped document. Heavy on string creation,
-- string-keyed table access and concatenation -- exactly where the string hash
-- would show up if it mattered.
local json = dofile("tests/perf/lib_json.lua")
local C = dofile("tests/perf/corpus.lua")
local words = C.getwords()

local doc = { users = {}, meta = { version = "1.4.2", generated = "2026-08-12" } }
for i = 1, 400 do
  doc.users[i] = {
    id = i,
    login = words[(i * 3) % #words + 1],
    email = words[(i * 5) % #words + 1] .. "@example.com",
    active = (i % 3) ~= 0,
    score = i * 1.5,
    tags = { words[(i * 7) % #words + 1], words[(i * 11) % #words + 1] },
    profile = { city = words[(i * 13) % #words + 1], zip = 10000 + i },
  }
end

local n = 0
for iter = 1, 100 do
  local s = json.encode(doc)
  local back = json.decode(s)
  n = n + #back.users
end
print(n)
