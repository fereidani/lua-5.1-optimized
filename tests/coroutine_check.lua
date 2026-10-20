-- Coroutines exercise the one control path this fork changed that nothing else
-- tests: the yield return inside vmfetch, and a hook flag that is cached per
-- luaV_execute invocation while hooks in 5.1 are per-thread. Compared
-- byte-for-byte against stock Lua 5.1.5.
local out = {}
local function o(...)
  local t = {}
  for i = 1, select("#", ...) do t[i] = tostring((select(i, ...))) end
  out[#out+1] = table.concat(t, "|")
end

-- basic create/resume/yield/status, values in both directions
local co = coroutine.create(function(a, b)
  o("in", a, b, coroutine.status(coroutine.running()))
  local c, d = coroutine.yield(a + b, a - b)
  o("resumed", c, d)
  local e = coroutine.yield(c * d)
  o("resumed2", e)
  return "done", e
end)
o("status0", coroutine.status(co))
o("r1", coroutine.resume(co, 10, 4))
o("status1", coroutine.status(co))
o("r2", coroutine.resume(co, 3, 7))
o("r3", coroutine.resume(co, "last"))
o("status2", coroutine.status(co))
o("r4", coroutine.resume(co))

-- wrap, including the error it raises when dead
local w = coroutine.wrap(function(x) local y = coroutine.yield(x * 2); return y .. "!" end)
o("w1", w(21))
o("w2", w("hi"))
o("w3", pcall(w))

-- errors raised inside a coroutine, raw and under pcall
local ec = coroutine.create(function() error("inner boom") end)
o("err", coroutine.resume(ec))
o("errstatus", coroutine.status(ec))
local ec2 = coroutine.create(function()
  local ok, e = pcall(function() error({code = 42}) end)
  o("caught-inside", ok, type(e), e.code)
  coroutine.yield("after-pcall")
  error("second")
end)
o("e1", coroutine.resume(ec2))
o("e2", coroutine.resume(ec2))

-- yielding across a C boundary must still be refused
local cb = coroutine.create(function()
  local t = {1, 2, 3}
  table.foreach(t, function() coroutine.yield() end)
end)
o("cboundary", coroutine.resume(cb))
local cb2 = coroutine.create(function() return pcall(function() coroutine.yield() end) end)
o("cboundary2", coroutine.resume(cb2))

-- nested coroutines, and resuming one from inside another
local inner = coroutine.create(function() coroutine.yield("inner-y"); return "inner-r" end)
local outer = coroutine.create(function()
  o("nested1", coroutine.resume(inner))
  coroutine.yield("outer-y")
  o("nested2", coroutine.resume(inner))
  return "outer-r"
end)
o("n1", coroutine.resume(outer))
o("n2", coroutine.resume(outer))

-- a hook installed on a coroutine, not the main thread
local hooked = coroutine.create(function()
  local s = 0
  for i = 1, 20 do s = s + i end
  coroutine.yield(s)
  return s * 2
end)
local counts = 0
debug.sethook(hooked, function() counts = counts + 1 end, "", 5)
o("h1", coroutine.resume(hooked))
o("h2", coroutine.resume(hooked))
o("hook-fired", counts > 0)
o("hook-mask", type(debug.gethook(hooked)))

-- a count hook on the main thread while coroutines run underneath it
local mainCount = 0
debug.sethook(function() mainCount = mainCount + 1 end, "", 3)
local c3 = coroutine.wrap(function() for i = 1, 30 do coroutine.yield(i) end end)
local acc = 0
for i = 1, 30 do acc = acc + c3() end
debug.sethook()
o("mainhook", acc, mainCount > 0)

-- coroutine.running inside and outside
o("running-main", type(coroutine.running()))
local rc = coroutine.create(function() return coroutine.running() ~= nil end)
o("running-co", coroutine.resume(rc))

-- values and varargs across yields, plus string building inside a coroutine
local vc = coroutine.create(function(...)
  local n = select("#", ...)
  coroutine.yield(n, ...)
  local s = ""
  for i = 1, 40 do s = s .. i .. "," end
  return #s
end)
o("va1", coroutine.resume(vc, "a", nil, "c", 4))
o("va2", coroutine.resume(vc))

-- garbage collection with suspended coroutines alive
local held = {}
for i = 1, 40 do
  held[i] = coroutine.create(function() coroutine.yield(i); return i end)
  coroutine.resume(held[i])
end
collectgarbage("collect")
local sum = 0
for i = 1, 40 do local ok, v = coroutine.resume(held[i]); sum = sum + v end
o("gc-coroutines", sum)

print(table.concat(out, "\n"))
