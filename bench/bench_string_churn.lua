-- Bench 3: String churn
-- Heavy string concatenation creating many temporary strings.
-- Tests string-specific GC performance.

local N = 500000

local function run_bench(mode_name)
  collectgarbage("collect")
  collectgarbage(mode_name)
  collectgarbage("collect")

  local mem_start = collectgarbage("count")
  local t_start = os.clock()

  local result = ""
  for i = 1, N do
    result = "prefix_" .. tostring(i) .. "_suffix"
  end

  local t_end = os.clock()
  collectgarbage("collect")
  local mem_end = collectgarbage("count")

  return t_end - t_start, mem_start, mem_end
end

print("=== Bench 3: String Churn ===")
print(string.format("Iterations: %d", N))

local gen_time, gen_m1, gen_m2 = run_bench("generational")
print(string.format("[Generational] Time: %.4fs  Mem: %.1fKB -> %.1fKB",
      gen_time, gen_m1, gen_m2))

local inc_time, inc_m1, inc_m2 = run_bench("incremental")
print(string.format("[Incremental]  Time: %.4fs  Mem: %.1fKB -> %.1fKB",
      inc_time, inc_m1, inc_m2))

local speedup = inc_time / gen_time
print(string.format("Speedup: %.2fx (gen/inc)", speedup))
