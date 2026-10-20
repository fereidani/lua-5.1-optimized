local t = {}
local acc = 0
for iter = 1, 2400 do
  for i = 1, 20000 do t[i] = i * 2 end
  for i = 1, 20000 do acc = acc + t[i] end
end
print(acc)
