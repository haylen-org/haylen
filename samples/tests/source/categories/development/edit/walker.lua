-- The class of the walkers of the test "DEV-002". Change the speed, the color or the size and save the file while the test runs: every walker takes the new methods at once and keeps its place, because the class keeps its identity.
local haylen = require('haylen')

local Walker = haylen.class('Walker')

function Walker:init(x, y, heading)
    self.x, self.y, self.heading = x, y, heading
end

function Walker:speed()
    return 140
end

function Walker:color()
    return '#FF6FDCA0'
end

function Walker:radius()
    return 18
end

-- Walks straight and turns back at the edges of the area.
function Walker:step(dt, area)
    self.x = self.x + math.cos(self.heading) * self:speed() * dt
    self.y = self.y + math.sin(self.heading) * self:speed() * dt
    if self.x < 0 or self.x > area.width then
        self.heading = math.pi - self.heading
        self.x = math.max(0, math.min(self.x, area.width))
    end
    if self.y < 0 or self.y > area.height then
        self.heading = -self.heading
        self.y = math.max(0, math.min(self.y, area.height))
    end
end

return Walker
