-- Every conversion must be byte-identical to string.format("%.14g", n),
-- which is exactly what stock lua_number2str produces.
local fmt, tostr = string.format, tostring
local bad, checked = 0, 0
local function chk(n)
  checked = checked + 1
  local a, b = tostr(n), fmt("%.14g", n)
  if a ~= b then bad = bad + 1; if bad <= 10 then print("MISMATCH", fmt("%.17g", n), a, b) end end
end
for i = -300000, 300000 do chk(i) end
for _, n in ipairs{0, -0.0, 1/0, -1/0, 0.5, 1/3, 1e13, 99999999999999, 100000000000000,
                   -99999999999999, -100000000000000, 1e14, 1e15, 1e300, -1e-300, 2^53,
                   -2^53, 2^53+2, 4503599627370496, 1e100, 123456789012345,
                   0.1, 3.14159265358979, -7.5, 2^31, 2^32, 2^63, -2^63} do chk(n) end
local x = 1
for i = 1, 320 do chk(x); chk(-x); x = x * 1.7 end
local y = 1
for i = 1, 200 do chk(y); chk(-y); y = y / 3 end
math.randomseed(12345)
for i = 1, 400000 do
  local r = math.random()
  chk(r); chk(r * 1e15); chk(math.floor(r * 1e15)); chk(-math.floor(r * 1e9))
end
chk(0/0)
print(("checked %d values, %d mismatches"):format(checked, bad))
