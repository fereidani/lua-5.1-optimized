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

-- Quicksort
local function quicksort(t, lo, hi)
  if lo >= hi then return end
  local pivot = t[math.floor((lo+hi)/2)]
  local i, j = lo, hi
  while i <= j do
    while t[i] < pivot do i = i + 1 end
    while t[j] > pivot do j = j - 1 end
    if i <= j then
      t[i], t[j] = t[j], t[i]
      i = i + 1; j = j - 1
    end
  end
  quicksort(t, lo, j)
  quicksort(t, i, hi)
end
-- Pre-build the array once
math.randomseed(42)
local data = {}
for i = 1, 50000 do data[i] = math.random(1, 50000) end
-- Copy function to avoid bias from table creation
local function copy_table(src)
  local dst = {}
  for i = 1, #src do dst[i] = src[i] end
  return dst
end
bench("quicksort", function()
  local t = copy_table(data)
  quicksort(t, 1, #t)
end)
