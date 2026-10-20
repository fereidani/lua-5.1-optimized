-- Tokenize real source text repeatedly. Every gmatch capture allocates and
-- interns a string: the identifier-length hashing path that dominates lexers,
-- template engines and config parsers.
local C = dofile("tests/perf/corpus.lua")
local text = C.text
local n = 0
for iter = 1, 220 do
  for w in text:gmatch("[%a_][%w_]*") do n = n + #w end
end
print(n)
