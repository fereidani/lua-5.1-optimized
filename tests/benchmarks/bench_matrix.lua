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

-- Matrix multiplication
local function mat_mul(A, B, n)
  local C = {}
  for i = 1, n do
    C[i] = {}
    for j = 1, n do
      local s = 0
      for k = 1, n do s = s + A[i][k] * B[k][j] end
      C[i][j] = s
    end
  end
  return C
end
math.randomseed(42)
local N = 80
local A, B = {}, {}
for i = 1, N do
  A[i], B[i] = {}, {}
  for j = 1, N do
    A[i][j] = math.random() * 10
    B[i][j] = math.random() * 10
  end
end
bench("matmul", function()
  mat_mul(A, B, N)
end)
