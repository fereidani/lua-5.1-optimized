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

-- Pi via Leibniz formula
local function calc_pi(n)
  local pi = 0.0
  local sign = 1.0
  for i = 0, n-1 do
    pi = pi + sign / (2*i + 1)
    sign = -sign
  end
  return pi * 4
end
bench("pi", function()
  calc_pi(10000000)
end)
