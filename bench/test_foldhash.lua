-- Basic functionality test
print("=== Lua foldhash integration test ===")

-- String interning
local a = "hello world"
local b = "hello world"
assert(a == b, "string equality broken")
print("  string interning: OK")

-- Table with string keys
local t = {}
for i = 1, 10000 do
    t["key_" .. i] = i
end
assert(t["key_5000"] == 5000, "table lookup broken")
assert(t["key_10000"] == 10000, "table lookup broken")
print("  table string keys: OK (10000 keys)")

-- Table with number keys
local tn = {}
for i = 1, 10000 do
    tn[i] = i * i
end
assert(tn[100] == 10000, "number key lookup broken")
print("  table number keys: OK")

-- Collision stress test
local tc = {}
for i = 1, 50000 do
    tc["prefix_" .. i .. "_suffix_" .. (i*7)] = i
end
local count = 0
for k, v in pairs(tc) do
    count = count + 1
end
assert(count == 50000, "collision test failed: expected 50000, got " .. count)
print("  collision stress: OK (50000 keys)")

-- Short strings
assert(type("") == "string")
assert(type("a") == "string")
assert(type("ab") == "string")
print("  short strings: OK")

-- Long strings
local long = string.rep("x", 10000)
assert(#long == 10000)
assert(long == string.rep("x", 10000))
print("  long strings: OK (10000 chars)")

-- Metatables
local mt = { __index = function(t, k) return k end }
setmetatable(t, mt)
assert(t.nonexistent == "nonexistent", "metatable broken")
print("  metatables: OK")

-- String library
assert(string.len("hello") == 5)
assert(string.sub("hello", 1, 3) == "hel")
assert(string.find("hello world", "world") == 7)
print("  string library: OK")

-- Performance test: hash-heavy table operations
local t0 = os.clock()
local pt = {}
for i = 1, 200000 do
    pt["k" .. i] = i
end
for i = 1, 200000 do
    local v = pt["k" .. i]
    assert(v == i)
end
local elapsed = os.clock() - t0
print(string.format("  200K insert+lookup: %.3fs", elapsed))

print("\nAll tests passed!")
