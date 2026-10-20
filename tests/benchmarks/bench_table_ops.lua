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

-- Table creation + summation
bench("table_ops", function()
  local t = {}
  for i = 1, 2000000 do t[i] = i * i end
  local sum = 0
  for i = 1, 2000000 do sum = sum + t[i] end
end)
