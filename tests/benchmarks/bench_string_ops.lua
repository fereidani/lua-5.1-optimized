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

-- String find stress
local s = string.rep("abc", 1000)
bench("string_find", function()
  local total = 0
  for i = 1, 50000 do
    local p = string.find(s, "cab", 1)
    if p then total = total + p end
  end
end)
