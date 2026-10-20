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

-- Sieve of Eratosthenes
local function sieve(n)
  local flags = {}
  for i = 1, n do flags[i] = true end
  flags[1] = false
  for i = 2, math.floor(math.sqrt(n)) do
    if flags[i] then
      for j = i*i, n, i do flags[j] = false end
    end
  end
  local count = 0
  for i = 1, n do if flags[i] then count = count + 1 end end
  return count
end
bench("sieve", function()
  sieve(1000000)
end)
