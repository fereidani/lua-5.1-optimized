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

-- String concatenation via table.concat
bench("string_concat", function()
  local parts = {}
  for i = 1, 100000 do parts[i] = tostring(i) end
  local s = table.concat(parts, ",")
end)
