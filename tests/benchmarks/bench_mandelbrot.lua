local function bench(name, fn)
  local elapsed = 0
  local runs = 0
  while elapsed < 3.0 do
    local t0 = os.clock()
    fn()
    local t1 = os.clock()
    elapsed = elapsed + (t1 - t0)
    runs = runs + 1
  end
  local avg = elapsed / runs
  print(string.format("%s: %.4fs avg (%d runs, %.4fs total)", name, avg, runs, elapsed))
  return avg
end

-- Mandelbrot set
local function mandelbrot(max_iter)
  local h, w = 200, 300
  local count = 0
  local xmin, xmax = -2.0, 1.0
  local ymin, ymax = -1.5, 1.5
  local dx = (xmax - xmin) / w
  local dy = (ymax - ymin) / h
  for py = 0, h-1 do
    local ci = ymin + py * dy
    for px = 0, w-1 do
      local cr = xmin + px * dx
      local zr, zi = 0.0, 0.0
      local iter = 0
      while iter < max_iter do
        local zr2 = zr * zr
        local zi2 = zi * zi
        if zr2 + zi2 > 4.0 then break end
        zi = 2.0 * zr * zi + ci
        zr = zr2 - zi2 + cr
        iter = iter + 1
      end
      if iter == max_iter then count = count + 1 end
    end
  end
  return count
end
bench("mandelbrot", function()
  mandelbrot(100)
end)
