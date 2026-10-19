-- A point that walks over the surface of a cave at a fixed height, around rocks, up walls and across ceilings, toward a target or wandering when it has none. Each step moves it along the surface and puts it back at its height, so it rounds outer corners and inner corners alike. It reports `stuck` when it stands at the point of its surface nearest a target that lies on another surface.
local haylen = require('haylen')
local m = require('haylen.math')

local Crawler = haylen.class('Crawler')

Crawler.arrival = 10
Crawler.turnRate = 10

function Crawler:init(cave, x, y, height, speed, seed)
    self.cave, self.height, self.speed = cave, height, speed
    self.random = m.random(seed)
    self.x, self.y = cave:project(x, y, height)
    self.nx, self.ny = cave:normal(self.x, self.y)
    self.dir = 1
    self.wanderTime = 0
    self.moved = 0
end

-- Returns the direction of travel along the surface.
function Crawler:tangent()
    return -self.ny * self.dir, self.nx * self.dir
end

-- Places the crawler on the surface under a point at once, such as when a leap lands.
function Crawler:land(x, y)
    self.x, self.y = self.cave:project(x, y, self.height)
    self.nx, self.ny = self.cave:normal(self.x, self.y)
end

-- Walks toward the point of the surface nearest the target, or wanders and turns around now and then without one.
function Crawler:update(dt, targetX, targetY)
    local cave = self.cave
    local nx, ny = cave:normal(self.x, self.y)
    local speed = self.speed
    self.stuck = false
    if targetX then
        local goalX, goalY = cave:project(targetX, targetY, self.height)
        local along = -ny * (goalX - self.x) + nx * (goalY - self.y)
        local away = math.sqrt((goalX - self.x) ^ 2 + (goalY - self.y) ^ 2)
        if math.abs(along) > 4 then
            self.dir = along > 0 and 1 or -1
        end
        -- Slowing near the nearest point keeps it from rocking back and forth over it.
        speed = speed * m.clamp(math.abs(along) / 40, 0, 1)
        self.stuck = away > Crawler.arrival * 4 and math.abs(along) < 6
        self.arrived = away <= Crawler.arrival
    else
        self.arrived = false
        self.wanderTime = self.wanderTime - dt
        if self.wanderTime <= 0 then
            self.wanderTime = self.random:range(3, 7)
            if self.random:chance(0.4) then
                self.dir = -self.dir
            end
        end
    end

    local step = speed * dt
    self.x, self.y = cave:project(self.x - ny * self.dir * step, self.y + nx * self.dir * step, self.height)
    self.moved = step
    nx, ny = cave:normal(self.x, self.y)
    -- The normal that bodies draw with turns smoothly, so a sharp inner corner never snaps them around.
    local blend = m.dampFactor(Crawler.turnRate, dt)
    self.nx, self.ny = self.nx + (nx - self.nx) * blend, self.ny + (ny - self.ny) * blend
    local length = math.sqrt(self.nx * self.nx + self.ny * self.ny)
    self.nx, self.ny = self.nx / length, self.ny / length
end

return Crawler
