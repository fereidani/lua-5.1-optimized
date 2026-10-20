local V = {}
V.__index = V
function V.new(x, y) return setmetatable({x = x, y = y}, V) end
function V:dot(o) return self.x * o.x + self.y * o.y end
function V:add(o) return V.new(self.x + o.x, self.y + o.y) end
local a, b = V.new(1, 2), V.new(3, 4)
local acc = 0
for iter = 1, 180 do
  for i = 1, 30000 do
    local c = a:add(b)
    acc = acc + c:dot(a)
  end
end
print(acc)
