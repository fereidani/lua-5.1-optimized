-- Building many short strings by concatenation (cache keys, path joining).
local acc = 0
for iter = 1, 200 do
  for i = 1, 20000 do
    local k = "user:" .. i .. ":profile"
    acc = acc + #k
  end
end
print(acc)
