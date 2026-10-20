-- Bench 2: Tree building and partial replacement
-- Builds a large binary tree, then replaces half the nodes repeatedly.
-- Tests mixed short/long-lived object patterns.

local DEPTH = 16
local REPLACEMENTS = 5

local function make_node(val, left, right)
  return { val = val, left = left, right = right }
end

local function build_tree(depth, val)
  if depth == 0 then return nil end
  return make_node(val, build_tree(depth - 1, val * 2), build_tree(depth - 1, val * 2 + 1))
end

local function replace_half(node, depth)
  if node == nil or depth == 0 then return node end
  -- Replace one side with a new subtree
  if depth % 2 == 0 then
    return make_node(node.val + 1, build_tree(3, node.val * 10), node.right)
  else
    return make_node(node.val + 1, node.left, build_tree(3, node.val * 10 + 1))
  end
end

local function count_nodes(node)
  if node == nil then return 0 end
  return 1 + count_nodes(node.left) + count_nodes(node.right)
end

local function run_bench(mode_name)
  collectgarbage("collect")
  collectgarbage(mode_name)
  collectgarbage("collect")

  local mem_start = collectgarbage("count")
  local t_start = os.clock()

  local tree = build_tree(DEPTH, 1)

  for i = 1, REPLACEMENTS do
    tree = replace_half(tree, DEPTH)
  end

  local nodes = count_nodes(tree)
  local t_end = os.clock()
  collectgarbage("collect")
  local mem_end = collectgarbage("count")

  return t_end - t_start, mem_start, mem_end, nodes
end

print("=== Bench 2: Tree Build & Replace ===")
print(string.format("Tree depth: %d, Replacements: %d", DEPTH, REPLACEMENTS))

local gen_time, gen_m1, gen_m2, gen_nodes = run_bench("generational")
print(string.format("[Generational] Time: %.4fs  Nodes: %d  Mem: %.1fKB -> %.1fKB",
      gen_time, gen_nodes, gen_m1, gen_m2))

local inc_time, inc_m1, inc_m2, inc_nodes = run_bench("incremental")
print(string.format("[Incremental]  Time: %.4fs  Nodes: %d  Mem: %.1fKB -> %.1fKB",
      inc_time, inc_nodes, inc_m1, inc_m2))

local speedup = inc_time / gen_time
print(string.format("Speedup: %.2fx (gen/inc)", speedup))
