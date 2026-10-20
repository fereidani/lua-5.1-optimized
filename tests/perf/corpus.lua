-- Realistic text corpus, built once from this repository's own C sources.
-- All paths are relative to the repository root; run via tests/perf/compare.py.
local M = {}
local files = {
  "lapi.c","lvm.c","lgc.c","ltable.c","lstrlib.c","lparser.c","lcode.c",
  "ldo.c","lobject.c","lstate.c","ldebug.c","lauxlib.c","lbaselib.c",
}
local parts = {}
for _, f in ipairs(files) do
  local h = io.open("src/" .. f, "r")
  if h then parts[#parts+1] = h:read("*a"); h:close() end
end
M.text = table.concat(parts)
-- words[] is built on demand: benchmarks that do not need it should not pay
-- for the tokenizing pass.
function M.getwords()
  if not M.words then
    local w = {}
    for x in M.text:gmatch("[%a_][%w_]*") do w[#w+1] = x end
    M.words = w
  end
  return M.words
end
return M
