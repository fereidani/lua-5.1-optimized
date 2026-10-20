local acc = 0
for iter = 1, 320 do
  local n = 100000
  local flags = {}
  for i = 2, n do flags[i] = true end
  for i = 2, 316 do
    if flags[i] then for j = i*i, n, i do flags[j] = false end end
  end
  local c = 0
  for i = 2, n do if flags[i] then c = c + 1 end end
  acc = acc + c
end
print(acc)
