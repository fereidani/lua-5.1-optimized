local obj = {x = 1, y = 2, z = 3, name = "obj", nested = {a = 1, b = 2}}
local acc = 0
for iter = 1, 540 do
  for i = 1, 40000 do
    acc = acc + obj.x + obj.y + obj.z + obj.nested.a + obj.nested.b
  end
end
print(acc)
