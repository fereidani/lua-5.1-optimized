local acc = 0
for iter = 1, 600 do
  for i = 1, 20000 do
    local n = i
    local f = function(x) return x + n end
    acc = acc + f(1)
  end
end
print(acc)
