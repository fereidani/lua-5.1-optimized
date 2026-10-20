local acc = 0
for iter = 1, 300 do
  local t = {}
  for i = 1, 20000 do t[i] = (i * 7919) % 20011 end
  table.sort(t)
  acc = acc + t[1] + t[#t]
end
print(acc)
