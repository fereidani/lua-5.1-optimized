local function bench(name, fn)
  local elapsed = 0
  local runs = 0
  while elapsed < 3.0 do
    local t0 = os.clock()
    fn()
    local t1 = os.clock()
    elapsed = elapsed + (t1 - t0)
    runs = runs + 1
  end
  local avg = elapsed / runs
  print(string.format("%s: %.4fs avg (%d runs, %.4fs total)", name, avg, runs, elapsed))
  return avg
end

-- Tight numeric loop with constant-foldable arithmetic
local a = 1; local b = 2; local c = 3
local d = a + b + c
bench("sum_loop", function()
  local sum = 0
  for i = 1, 100000000 do
    sum = sum + d
  end
end)
