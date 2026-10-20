-- Bench 1: Short-lived objects
-- Creates millions of temporary tables that are immediately discarded.
-- Tests young-generation collection efficiency.

local N = 2000000

local function run_bench(mode_name)
  collectgarbage("collect")
  collectgarbage(mode_name)
  collectgarbage("collect")

  local mem_start = collectgarbage("count")
  local t_start = os.clock()

  for i = 1, N do
    local t = { x = i, y = i * 2, z = "hello" }
  end

  local t_end = os.clock()
  collectgarbage("collect")
  local mem_end = collectgarbage("count")

  return t_end - t_start, mem_start, mem_end
end

print("=== Bench 1: Short-lived Objects ===")
print(string.format("Iterations: %d", N))

-- Run generational first
local gen_time, gen_mem1, gen_mem2 = run_bench("generational")
print(string.format("[Generational] Time: %.4fs  Mem before: %.1fKB  Mem after GC: %.1fKB",
      gen_time, gen_mem1, gen_mem2))

-- Run incremental
local inc_time, inc_mem1, inc_mem2 = run_bench("incremental")
print(string.format("[Incremental]  Time: %.4fs  Mem before: %.1fKB  Mem after GC: %.1fKB",
      inc_time, inc_mem1, inc_mem2))

local speedup = inc_time / gen_time
print(string.format("Speedup: %.2fx (gen/inc)", speedup))
