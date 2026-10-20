-- luaV_finishget reimplements the __index half of luaV_gettable. Everything
-- here is compared byte-for-byte against stock Lua 5.1.5.
local out = {}
local function e(...) out[#out+1] = table.concat({...}, "|") end
local function p(...) local r = {pcall(...)} local s = {}
  for i,v in ipairs(r) do s[i] = tostring(v) end; e(unpack(s)) end

-- __index as table, one level
local base = {a = 1, shared = "B"}
local obj = setmetatable({own = 2}, {__index = base})
e("t1", tostring(obj.own), tostring(obj.a), tostring(obj.shared), tostring(obj.missing))

-- __index chain, several levels deep
local l1 = {deep = "L1"}
local l2 = setmetatable({}, {__index = l1})
local l3 = setmetatable({}, {__index = l2})
local l4 = setmetatable({}, {__index = l3})
e("t2", tostring(l4.deep), tostring(l4.nope))

-- __index as function
local calls = 0
local fobj = setmetatable({}, {__index = function(t, k) calls = calls + 1; return "F:" .. tostring(k) end})
e("t3", tostring(fobj.x), tostring(fobj.y), tostring(calls))

-- function after a table hop
local mid = setmetatable({}, {__index = function(t, k) return "VIA:" .. k end})
local top = setmetatable({}, {__index = mid})
e("t4", tostring(top.zz))

-- method dispatch through __index (the SELF opcode path)
local V = {}
V.__index = V
function V.new(x) return setmetatable({x = x}, V) end
function V:get() return self.x end
function V:twice() return self:get() * 2 end
local v = V.new(21)
e("t5", tostring(v:get()), tostring(v:twice()))

-- __index on a non-table receiver
local smt = {__index = function(s, k) return "STR:" .. k end}
p(function() local s = "abc" return s.someField end)
e("t6", tostring(("abc"):len()))

-- errors: indexing nil / number / boolean
p(function() local x = nil; return x.field end)
p(function() local x = 42; return x.field end)
p(function() local x = true; return x.field end)
p(function() local x = nil; return x[1] end)

-- __index loop must be cut off, not hang
local a1, a2 = {}, {}
setmetatable(a1, {__index = a2})
setmetatable(a2, {__index = a1})
p(function() return a1.never end)

-- self-referential __index
local sr = {}
setmetatable(sr, {__index = sr})
p(function() return sr.never end)

-- rawget must bypass all of it
e("t7", tostring(rawget(obj, "a")), tostring(rawget(obj, "own")))

-- integer and mixed keys through __index
local ibase = {[1] = "one", [2] = "two"}
local iobj = setmetatable({}, {__index = ibase})
e("t8", tostring(iobj[1]), tostring(iobj[2]), tostring(iobj[3]), tostring(iobj[1.0]))

-- __index that itself errors
p(function() local q = setmetatable({}, {__index = function() error("inner") end}) return q.k end)

-- __index interacting with rawset and later reads
local m = setmetatable({}, {__index = base})
e("t9", tostring(m.a)); rawset(m, "a", 99); e("t9b", tostring(m.a))

-- metatable removed at runtime
local rm = setmetatable({}, {__index = base})
e("t10", tostring(rm.a)); setmetatable(rm, nil); e("t10b", tostring(rm.a))

-- __index added at runtime to a previously bare table
local late = {}
e("t11", tostring(late.a)); setmetatable(late, {__index = base}); e("t11b", tostring(late.a))

-- an __index function at the very end of a long chain. luaV_gettable calls it
-- from its MAXTAGLOOP-th (100th) table and errors one table later; finishget,
-- which picks up after the caller's first lookup, must cut off at the same
-- depth for both the GETTABLE and SELF paths
for _, depth in ipairs({98, 99, 100}) do
  local o = setmetatable({}, {__index = function() return "end" end})
  for i = 1, depth do o = setmetatable({}, {__index = o}) end
  p(function() return o.x end)
  p(function() return o:x() end)
end

print(table.concat(out, "\n"))
