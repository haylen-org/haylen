-- A cave made from a seed as a distance field: a room inside rounded walls, rocks that grow from its walls, floor and ceiling, and rocks that float in it. The distance of a point to the nearest rock is what climbing creatures read: `normal` points away from the rock, `project` puts a point at a height over it, `walk` moves a point along the surface, `clear` tells whether a straight leap fits, and the rocks draw as meshes of a painted stone texture over the creatures, so a foot that reaches into the stone hides in it.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local Cave = haylen.class('Cave')

Cave.wallRadius = 90
Cave.blend = 14
Cave.outlinePoints = 72
Cave.walkStep = 32
Cave.colors = {edge = '#FF1E1B33', rim = '#FF6E5A86', air = '#FF121722', glow = '#FF262F45'}
-- The world units one copy of the stone texture covers, and the layer the stone draws at, over the legs of the creatures and under their bodies.
Cave.textureSize = 420
Cave.layer = 4

-- The room spans `width` by `height` around the origin.
function Cave:init(seed, width, height)
    local random = m.random(seed)
    self.halfWidth, self.halfHeight = width / 2, height / 2
    self.stone = assets.texture('procedural/cave-rock.png', {filter = 'linear', wrap = 'repeat'})
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
    -- The outline is a flat list for its stroke, and the fill a fan of triangles around the center, which every point of a rock sees.
    rock.outline = {}
    rock.vertices = {Cave.vertex(x, y)}
    rock.indices = {}
    for index = 1, Cave.outlinePoints do
        local angle = (index - 1) / Cave.outlinePoints * math.pi * 2
        local reach = Cave.rockRadius(rock, angle)
        local px, py = x + math.cos(angle) * reach, y + math.sin(angle) * reach
        rock.outline[#rock.outline + 1] = px
        rock.outline[#rock.outline + 1] = py
        rock.vertices[index + 1] = Cave.vertex(px, py)
        local nextIndex = index % Cave.outlinePoints + 2
        rock.indices[#rock.indices + 1] = 1
        rock.indices[#rock.indices + 1] = index + 1
        rock.indices[#rock.indices + 1] = nextIndex
    end
    self.rocks[#self.rocks + 1] = rock
end

-- A vertex of the stone, whose texture repeats every `textureSize` units of the world.
function Cave.vertex(x, y)
    return {x = x, y = y, u = x / Cave.textureSize, v = y / Cave.textureSize}
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

-- Walks from a point along the surface under it by `distance`, forward along the direction that has the rock on its right for a positive distance, and returns where it ends `height` over the rock, so a point ahead of a creature stays on the rock it stands on even past a bump or a corner.
function Cave:walk(x, y, distance, height)
    local steps = math.max(1, math.ceil(math.abs(distance) / Cave.walkStep))
    local step = distance / steps
    x, y = self:project(x, y, height)
    for _ = 1, steps do
        local nx, ny = self:normal(x, y)
        x, y = self:project(x - ny * step, y + nx * step, height)
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

-- Draws the air of the room under everything, and over the legs of the creatures, which hides a leg where it reaches into the rock, the stone of the walls around the room and of the rocks inside it. Every outline draws before every fill of stone, so outlines show only along the edge of the stone and never inside where rocks meet each other or a wall.
function Cave:draw()
    local width, height = self.halfWidth * 2, self.halfHeight * 2
    local room = {-self.halfWidth, -self.halfHeight, width, height}
    local colors = Cave.colors
    graphics2d.drawShape(room, {radius = Cave.wallRadius, color = colors.air, layer = -20})
    graphics2d.drawShape({-self.halfWidth + 60, -self.halfHeight + 60, width - 120, height - 120}, {radius = Cave.wallRadius, color = colors.glow, softness = 160, layer = -19})

    local bounds = graphics2d.canvasBounds()
    local left, top, right, bottom = bounds.x, bounds.y, bounds.x + bounds.width, bounds.y + bounds.height
    graphics2d.pushMask(function()
        graphics2d.drawShape(room, {radius = Cave.wallRadius, color = '#FFFFFFFF'})
    end, {mode = 'outside', layer = Cave.layer})
    graphics2d.drawMesh(self.stone, {Cave.vertex(left, top), Cave.vertex(right, top), Cave.vertex(right, bottom), Cave.vertex(left, bottom)}, {1, 2, 3, 1, 3, 4})
    graphics2d.popMask()

    graphics2d.pushMask(function()
        graphics2d.drawShape(room, {radius = Cave.wallRadius, color = '#FFFFFFFF'})
    end, {layer = Cave.layer + 1})
    graphics2d.drawShape(room, {radius = Cave.wallRadius, color = '#00000000', borderWidth = 7, borderColor = colors.edge})
    for _, rock in ipairs(self.rocks) do
        graphics2d.drawStroke(rock.outline, {width = 14, color = colors.edge, closed = true, join = 'round', layer = 1})
    end
    for _, rock in ipairs(self.rocks) do
        graphics2d.drawMesh(self.stone, rock.vertices, rock.indices, {layer = 2})
    end
    graphics2d.popMask()
end

return Cave
