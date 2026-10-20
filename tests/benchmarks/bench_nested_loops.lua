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

-- Nested loops with constant multipliers
local factor = 3
local offset = 7
bench("nested_loops", function()
  local sum = 0
  for i = 1, 1000 do
    for j = 1, 1000 do
      sum = sum + i * factor + j * offset
    end
  end
end)
