-- The class of the walkers of the test "DEV-002". Change the speed, the color of the ring under the feet or the size and save the file while the test runs: every walker takes the new methods at once and keeps its place, because the class keeps its identity.
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

-- Walks straight and turns back where its ring would leave the area at the sides and the bottom, or its head at the top.
function Walker:step(dt, area)
    local radius = self:radius()
    local left, right, top, bottom = radius * 1.4, area.width - radius * 1.4, radius * 5, area.height - radius
    self.x = self.x + math.cos(self.heading) * self:speed() * dt
    self.y = self.y + math.sin(self.heading) * self:speed() * dt
    if self.x < left or self.x > right then
        self.heading = math.pi - self.heading
        self.x = math.max(left, math.min(self.x, right))
    end
    if self.y < top or self.y > bottom then
        self.heading = -self.heading
        self.y = math.max(top, math.min(self.y, bottom))
    end
end

return Walker
