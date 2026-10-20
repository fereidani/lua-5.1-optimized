-- game_sim.lua (approx 10-20 KB base; duplicate sections for more size)
local Entity = {}
Entity.__index = Entity

function Entity:new(x, y, vx, vy)
  local self = setmetatable({}, Entity)
  self.x, self.y = x or 0, y or 0
  self.vx, self.vy = vx or 0, vy or 0
  self.health = 100
  return self
end

function Entity:update(dt)
  self.x = self.x + self.vx * dt
  self.y = self.y + self.vy * dt
  if self.health < 0 then self.dead = true end
end

local World = {}
World.__index = World

function World:new()
  local self = setmetatable({}, World)
  self.entities = {}
  self.particles = {}
  return self
end

function World:addEntity(e)
  table.insert(self.entities, e)
end

function World:update(dt)
  for i = #self.entities, 1, -1 do
    local e = self.entities[i]
    e:update(dt)
    if e.dead then table.remove(self.entities, i) end
  end

  -- Particle simulation (more bytecode)
  for i = 1, 200 do
    table.insert(self.particles, {x=math.random(800), y=math.random(600), life=60})
  end
  for i = #self.particles, 1, -1 do
    self.particles[i].life = self.particles[i].life - 1
    if self.particles[i].life <= 0 then table.remove(self.particles, i) end
  end
end

-- Collision and other heavy functions
local function checkCollisions(world)
  for i, e1 in ipairs(world.entities) do
    for j = i+1, #world.entities do
      local e2 = world.entities[j]
      local dx = e1.x - e2.x
      local dy = e1.y - e2.y
      if dx*dx + dy*dy < 400 then
        e1.health = e1.health - 10
        e2.health = e2.health - 10
      end
    end
  end
end

-- Main simulation loop (repeatable for size)
local world = World:new()
for i = 1, 500 do
  world:addEntity(Entity:new(math.random(800), math.random(600), math.random(-50,50), math.random(-50,50)))
end

local dt = 0.016
for frame = 1, 1000 do
  world:update(dt)
  checkCollisions(world)
  if frame % 100 == 0 then
    print("Frame", frame, "Entities:", #world.entities, "Particles:", #world.particles)
  end
end

print("Simulation finished with", #world.entities, "entities left")
