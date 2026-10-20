-- Bench 4: Closure churn
-- Creates and discards many closures with upvalues.
-- Tests closure-heavy workloads.

local N = 500000

local function make_adder(val)
  local offset = val * 2
  return function()
    return val + offset
  end
end

local function run_bench(mode_name)
  collectgarbage("collect")
  collectgarbage(mode_name)
  collectgarbage("collect")

  local mem_start = collectgarbage("count")
  local t_start = os.clock()

  local sum = 0
  for i = 1, N do
    local f = make_adder(i)
    sum = sum + f()
  end

  local t_end = os.clock()
  collectgarbage("collect")
  local mem_end = collectgarbage("count")

  return t_end - t_start, mem_start, mem_end, sum
end

print("=== Bench 4: Closure Churn ===")
print(string.format("Iterations: %d", N))

local gen_time, gen_m1, gen_m2, gen_sum = run_bench("generational")
print(string.format("[Generational] Time: %.4fs  Sum: %d  Mem: %.1fKB -> %.1fKB",
      gen_time, gen_sum, gen_m1, gen_m2))

local inc_time, inc_m1, inc_m2, inc_sum = run_bench("incremental")
print(string.format("[Incremental]  Time: %.4fs  Sum: %d  Mem: %.1fKB -> %.1fKB",
      inc_time, inc_sum, inc_m1, inc_m2))

local speedup = inc_time / gen_time
print(string.format("Speedup: %.2fx (gen/inc)", speedup))
