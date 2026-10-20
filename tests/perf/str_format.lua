-- string.format templating, as in a logger or report generator.
local acc = 0
for iter = 1, 150 do
  for i = 1, 15000 do
    local line = string.format("[%s] %-8s id=%d value=%.3f", "INFO", "worker", i, i * 1.5)
    acc = acc + #line
  end
end
print(acc)
