-- A cave made from a seed as a distance field: a room inside rounded walls, rocks that grow from its walls, floor and ceiling, and rocks that float in it. The distance of a point to the nearest rock is what climbing creatures read: `normal` points away from the rock, `project` puts a point at a height over it, `clear` tells whether a straight leap fits, and the rocks draw as polygons.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local Cave = haylen.class('Cave')

Cave.wallRadius = 90
Cave.blend = 14
Cave.outlinePoints = 72
Cave.colors = {rock = '#FF4C4361', edge = '#FF8A7AA8', air = '#FF121722', glow = '#FF1E2738'}

-- The room spans `width` by `height` around the origin.
function Cave:init(seed, width, height)
    local random = m.random(seed)
    self.halfWidth, self.halfHeight = width / 2, height / 2
    self.rocks = {}
    for _ = 1, random:integer(6, 9) do
        local side = random:integer(1, 4)
        local along = random:range(-0.85, 0.85)
        local radius = random:range(50, 100)
        local x, y
        if side == 1 or side == 2 then
            x, y = along * self.halfWidth, (side == 1 and -1 or 1) * (self.halfHeight + radius * 0.25)
        else
            x, y = (side == 3 and -1 or 1) * (self.halfWidth + radius * 0.25), along * self.halfHeight
        end
        self:addRock(random, x, y, radius)
    end
    -- Floating rocks keep room around them, so a creature always fits between a rock and the walls.
    for _ = 1, 40 do
        if #self.rocks >= 12 then
            break
        end
        local x, y = random:range(-self.halfWidth + 200, self.halfWidth - 200), random:range(-self.halfHeight + 160, self.halfHeight - 160)
        local radius = random:range(40, 70)
        if self:distance(x, y) > radius + 140 then
            self:addRock(random, x, y, radius)
        end
    end
end

-- Adds a rock whose radius waves around its center with a few slow waves, so its outline is smooth and its distance stays close to the true one.
function Cave:addRock(random, x, y, radius)
    local waves = {}
    for order = 2, 5 do
        waves[#waves + 1] = {order, random:range(0, 0.16) / order * 2, random:range(0, math.pi * 2)}
    end
    local rock = {x = x, y = y, radius = radius, waves = waves}
    rock.points = {}
    for index = 1, Cave.outlinePoints do
        local angle = (index - 1) / Cave.outlinePoints * math.pi * 2
        local reach = Cave.rockRadius(rock, angle)
        rock.points[index] = {x + math.cos(angle) * reach, y + math.sin(angle) * reach}
    end
    self.rocks[#self.rocks + 1] = rock
end

function Cave.rockRadius(rock, angle)
    local scale = 1
    for _, wave in ipairs(rock.waves) do
        scale = scale + wave[2] * math.sin(wave[1] * angle + wave[3])
    end
    return rock.radius * scale
end

-- Joins two distances with a rounded fillet, so a creature turns smoothly where a rock meets a wall.
function Cave.blendMin(a, b)
    local h = m.clamp(0.5 + 0.5 * (b - a) / Cave.blend, 0, 1)
    return b + (a - b) * h - Cave.blend * h * (1 - h)
end

-- Returns how far a point is from the nearest rock or wall, negative inside them.
function Cave:distance(x, y)
    local inset = Cave.wallRadius
    local qx, qy = math.abs(x) - (self.halfWidth - inset), math.abs(y) - (self.halfHeight - inset)
    local outside = math.sqrt(math.max(qx, 0) ^ 2 + math.max(qy, 0) ^ 2)
    local distance = -(outside + math.min(math.max(qx, qy), 0) - inset)
    for _, rock in ipairs(self.rocks) do
        local dx, dy = x - rock.x, y - rock.y
        distance = Cave.blendMin(distance, math.sqrt(dx * dx + dy * dy) - Cave.rockRadius(rock, math.atan(dy, dx)))
    end
    return distance
end

-- Returns the direction away from the nearest rock.
function Cave:normal(x, y)
    local step = 1.5
    local nx = self:distance(x + step, y) - self:distance(x - step, y)
    local ny = self:distance(x, y + step) - self:distance(x, y - step)
    local length = math.sqrt(nx * nx + ny * ny)
    if length == 0 then
        return 0, -1
    end
    return nx / length, ny / length
end

-- Moves a point along the normal until it sits `height` over the nearest rock.
function Cave:project(x, y, height)
    for _ = 1, 3 do
        local distance = self:distance(x, y) - height
        local nx, ny = self:normal(x, y)
        x, y = x - nx * distance, y - ny * distance
    end
    return x, y
end

-- Returns whether the straight line between two points keeps `margin` away from every rock.
function Cave:clear(x1, y1, x2, y2, margin)
    local dx, dy = x2 - x1, y2 - y1
    local steps = math.max(1, math.ceil(math.sqrt(dx * dx + dy * dy) / 10))
    for step = 1, steps - 1 do
        local t = step / steps
        if self:distance(x1 + dx * t, y1 + dy * t) < margin then
            return false
        end
    end
    return true
end

-- Draws the walls, the open room and the rocks, whose parts that reach into the walls stay hidden behind the mask of the room.
function Cave:draw()
    local width, height = self.halfWidth * 2, self.halfHeight * 2
    local room = {-self.halfWidth, -self.halfHeight, width, height}
    graphics2d.drawRect({-width, -height, width * 2, height * 2}, Cave.colors.rock)
    graphics2d.pushMask(function()
        graphics2d.drawShape(room, {radius = Cave.wallRadius, color = '#FFFFFFFF'})
    end, {layer = 1})
    graphics2d.drawShape(room, {radius = Cave.wallRadius, color = Cave.colors.air, borderWidth = 4, borderColor = Cave.colors.edge})
    graphics2d.drawShape({-self.halfWidth + 60, -self.halfHeight + 60, width - 120, height - 120}, {radius = Cave.wallRadius, color = Cave.colors.glow, softness = 120})
    for _, rock in ipairs(self.rocks) do
        graphics2d.drawPolygon(rock.points, Cave.colors.rock)
        graphics2d.drawPolyline(rock.points, 4, Cave.colors.edge, true)
    end
    graphics2d.popMask()
end

return Cave
