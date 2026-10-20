-- Bench 5: Mixed workload
-- Realistic workload mixing tables, strings, closures, and coroutines.
-- Tests overall generational GC performance.

local N = 100000

local function process_data(idx)
  -- Create a table (young)
  local t = {
    id = idx,
    name = "item_" .. tostring(idx),
    data = { 1, 2, 3, idx },
  }
  -- Create a closure (young)
  local get_name = function()
    return t.name
  end
  -- Do some string work (young strings)
  local desc = get_name() .. " with " .. #t.data .. " elements"
  return desc
end

local function run_bench(mode_name)
  collectgarbage("collect")
  collectgarbage(mode_name)
  collectgarbage("collect")

  local mem_start = collectgarbage("count")
  local t_start = os.clock()

  -- Keep some objects alive (long-lived)
  local cache = {}
  for i = 1, 100 do cache[i] = { persistent = true } end

  local count = 0
  for i = 1, N do
    local desc = process_data(i)
    count = count + #desc
    -- Periodically create short-lived coroutines
    if i % 1000 == 0 then
      local co = coroutine.create(function()
        local x = { i, i + 1 }
        coroutine.yield(x[1])
      end)
      local ok, val = coroutine.resume(co)
      count = count + (val or 0)
    end
  end

  local t_end = os.clock()
  collectgarbage("collect")
  local mem_end = collectgarbage("count")

  return t_end - t_start, mem_start, mem_end, count
end

print("=== Bench 5: Mixed Workload ===")
print(string.format("Iterations: %d", N))

local gen_time, gen_m1, gen_m2, gen_count = run_bench("generational")
print(string.format("[Generational] Time: %.4fs  Checksum: %d  Mem: %.1fKB -> %.1fKB",
      gen_time, gen_count, gen_m1, gen_m2))

local inc_time, inc_m1, inc_m2, inc_count = run_bench("incremental")
print(string.format("[Incremental]  Time: %.4fs  Checksum: %d  Mem: %.1fKB -> %.1fKB",
      inc_time, inc_count, inc_m1, inc_m2))

local speedup = inc_time / gen_time
print(string.format("Speedup: %.2fx (gen/inc)", speedup))
