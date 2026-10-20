-- Debug hooks are what debuggers and profilers rely on, and a VM change that
-- breaks them fails silently. Everything here is compared byte-for-byte
-- against stock Lua 5.1.5.
local out = {}
local function log(...) out[#out+1] = table.concat({...}, " ") end

-- line hook over a mix of control flow
local function work(n)
  local s = 0
  for i = 1, n do
    if i % 2 == 0 then s = s + i else s = s - i end
  end
  local t = {}
  for i = 1, 3 do t[#t+1] = i * 2 end
  return s, #t
end

local lines = {}
debug.sethook(function(ev, ln) lines[#lines+1] = ev .. ":" .. tostring(ln) end, "l")
work(5)
debug.sethook()
log("line events", #lines, table.concat(lines, ","))

-- count hook at several intervals
for _, n in ipairs{1, 2, 7, 50} do
  local c = 0
  debug.sethook(function() c = c + 1 end, "", n)
  work(20)
  debug.sethook()
  log("count", n, c)
end

-- call/return hooks
local ev = {}
debug.sethook(function(e) ev[#ev+1] = e end, "cr")
work(3)
debug.sethook()
log("callret", #ev, table.concat(ev, ","))

-- hook that inspects the running frame
local info = {}
debug.sethook(function(e, ln)
  local d = debug.getinfo(2, "Sl")
  if d then info[#info+1] = (d.short_src or "?") .. ":" .. tostring(d.currentline) end
end, "l")
work(2)
debug.sethook()
log("getinfo", #info, info[1] or "none", info[#info] or "none")

-- hook set from inside a C-called function, and hook removal from a hook
local fired = 0
debug.sethook(function() fired = fired + 1; if fired == 3 then debug.sethook() end end, "", 5)
work(100)
debug.sethook()
log("selfclearing", fired)

-- mask reporting
debug.sethook(function() end, "lcr", 11)
local f, m, c = debug.gethook()
log("gethook", type(f), m, c)
debug.sethook()
log("gethook-after", tostring(debug.gethook()))

-- pcall/error interaction under a hook
debug.sethook(function() end, "l")
local ok, err = pcall(function() error("boom") end)
debug.sethook()
log("pcall-under-hook", tostring(ok), tostring(err))

print(table.concat(out, "\n"))
