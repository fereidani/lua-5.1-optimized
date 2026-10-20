-- Accumulating one large string: every concatenation copies the whole string
-- built so far and interns the result, so this measures long-string copying,
-- hashing and garbage collection rather than the VM's dispatch loop.
local acc = 0
for iter = 1, 200 do
  local s = ""
  for i = 1, 2500 do
    s = s .. "abcdefghijklmnopqrstuvwxyz0123456789"
  end
  acc = acc + #s
end
print(acc)
