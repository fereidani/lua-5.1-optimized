-- Real application: a pure-Lua SHA-2 implementation hashing a text corpus.
-- Numeric/bitwise heavy with medium string handling; representative of the
-- "crypto or codec in pure Lua" workload.
local sha = dofile("tests/perf/lib_sha2.lua")
local C = dofile("tests/perf/corpus.lua")
local block = C.text:sub(1, 40000)
local last
for iter = 1, 12 do
  last = sha.sha256(block)
end
print(last)
