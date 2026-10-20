local s = 0.0
for iter = 1, 240 do
  for i = 1, 200000 do s = s + i * 1.5 - i / 3.0 + (i % 7) end
end
print(string.format("%.3f", s))
