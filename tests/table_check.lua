-- Node chaining: collisions, node relocation on insert, rehash, deletion,
-- traversal order and dead keys. Compared byte-for-byte against stock 5.1.5.
local out = {}
local function o(...) out[#out+1] = table.concat({...}, "|") end

-- dense integer keys forced into the hash part (negative + sparse)
local t = {}
for i = -200, -1 do t[i] = i * 2 end
for i = 1, 300, 7 do t[i] = i end
local sum, cnt = 0, 0
for k, v in pairs(t) do sum = sum + v; cnt = cnt + 1 end
o("ints", cnt, sum, tostring(t[-100]), tostring(t[8]), tostring(t[7]))

-- string keys with heavy collision pressure, then deletion and reinsertion
local s = {}
for i = 1, 2000 do s["key" .. i] = i end
for i = 1, 2000, 2 do s["key" .. i] = nil end
local n, tot = 0, 0
for k, v in pairs(s) do n = n + 1; tot = tot + v end
o("strings", n, tot, tostring(s.key2), tostring(s.key1))
for i = 1, 2000, 2 do s["key" .. i] = -i end
n, tot = 0, 0
for k, v in pairs(s) do n = n + 1; tot = tot + v end
o("reinsert", n, tot, tostring(s.key1), tostring(s.key3))

-- mixed key types in one table, exercising every mainposition branch
local m = {}
local kt, kf = {}, function() end
m[1.5] = "float"; m["str"] = "string"; m[true] = "bool"; m[kt] = "table"
m[kf] = "func"; m[2^40] = "big"; m[-0.0] = "negzero"
local seen = {}
for k, v in pairs(m) do seen[#seen+1] = v end
table.sort(seen)
o("mixed", table.concat(seen, ","), tostring(m[1.5]), tostring(m[kt]), tostring(m[0.0]))

-- forced rehash growth then shrink, interleaved with traversal
local g = {}
for round = 1, 6 do
  for i = 1, 400 do g["k"..round.."_"..i] = i end
  local c = 0; for _ in pairs(g) do c = c + 1 end
  o("grow"..round, c)
  for i = 1, 400, 3 do g["k"..round.."_"..i] = nil end
end

-- next() explicitly, including resuming from a known key
local nx = {a=1, b=2, c=3, d=4}
local keys = {}
local k, v = next(nx)
while k do keys[#keys+1] = k; k, v = next(nx, k) end
table.sort(keys)
o("next", table.concat(keys, ","), #keys)

-- collisions between removed and live keys (dead-key path in findindex)
local d = {}
for i = 1, 50 do d["d"..i] = i end
local order = {}
for kk in pairs(d) do order[#order+1] = kk end
for i = 1, #order, 2 do d[order[i]] = nil end
collectgarbage("collect")
local c2 = 0; for _ in pairs(d) do c2 = c2 + 1 end
o("deadkeys", c2)

-- table.remove/insert churn and length
local arr = {}
for i = 1, 100 do table.insert(arr, i) end
for i = 1, 50 do table.remove(arr, 1) end
o("array", #arr, arr[1], arr[#arr])

-- weak table interaction with the chain
local w = setmetatable({}, {__mode = "k"})
for i = 1, 100 do w[{}] = i end
collectgarbage("collect")
local wc = 0; for _ in pairs(w) do wc = wc + 1 end
o("weak-collected", wc == 0 and "all" or "some")

-- integer keys at the int boundaries: the array-part probe computes key - 1,
-- which must not overflow for INT_MIN (UBSan catches the regression)
local ib = {10, 20, 30}
ib[-2^31] = "min"; ib[2^31 - 1] = "max"; ib[2^31] = "over"
o("int-bounds", ib[-2^31], ib[2^31 - 1], ib[2^31], tostring(ib[0]), ib[1], ib[3])

print(table.concat(out, "\n"))
