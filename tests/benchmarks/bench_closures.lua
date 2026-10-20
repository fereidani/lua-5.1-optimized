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

-- Closure creation + invocation stress
bench("closure_stress", function()
  local sum = 0
  for i = 1, 200000 do
    local val = i
    local f = function() return val end
    sum = sum + f()
  end
end)
