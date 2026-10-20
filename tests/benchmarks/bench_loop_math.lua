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

-- Loop math with constant-derived multipliers
local pi = 3.14159265358979
local two_pi = 2 * pi
local factor = two_pi + 1
bench("loop_math", function()
  local sum = 0.0
  for i = 1, 20000000 do
    sum = sum + i * factor
  end
end)
