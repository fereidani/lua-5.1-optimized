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

-- Ackermann function
local function ack(m, n)
  if m == 0 then return n + 1 end
  if n == 0 then return ack(m-1, 1) end
  return ack(m-1, ack(m, n-1))
end
bench("ackermann", function()
  ack(3, 8)
end)
