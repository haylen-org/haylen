-- The landscape behind the screen and sprite effect tests, laid out over a world of 1920 by 1080 around the origin: a sky, a painted sun bright enough to bloom, drifting clouds, mountains, hills, trees, grass and the earth under it, all below layer 0. At night the sky darkens, the moon and the stars come out and the land takes the blue of the night.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local ParticleTest = require('categories.particles.particle-test')

local Scenery = haylen.class('Scenery')

Scenery.ground = 400
Scenery.cloudSpeed = 24
Scenery.trees = {{-820, 330, 1.2}, {-640, 360, 0.95}, {660, 340, 1.3}, {860, 370, 1}}
Scenery.sun = {480, -290}
Scenery.earth = '#FF813F1E'
-- The strip of earth starts behind the grass, so the edge between its grass and its earth shows just under the tufts.
Scenery.earthHeight = 240
Scenery.earthTop = Scenery.ground - 90
-- The land multiplies its colors by this blue at night.
Scenery.nightTint = '#FF5C6A9E'
Scenery.starCount = 70

function Scenery:init()
    local linear = {filter = 'linear'}
    self.clouds = assets.texture('camera/clouds.png', linear)
    self.mountains = assets.texture('camera/mountains.png', linear)
    self.hills = assets.texture('camera/hills.png', linear)
    self.grass = assets.texture('camera/grass.png', linear)
    self.earthStrip = assets.texture('camera/ground.png', linear)
    self.tree = assets.texture('sprites/images/tree.png', linear)
    self.sunImage = assets.texture('effects/sun.png', linear)
    self.moonImage = assets.texture('effects/moon.png', linear)
    self.time = 0
    -- The stars keep their places, picked once from a small generator of their own.
    self.stars = {}
    local seed = 17
    for index = 1, Scenery.starCount do
        seed = (seed * 1103515245 + 12345) % 2147483648
        local x = seed / 2147483648
        seed = (seed * 1103515245 + 12345) % 2147483648
        local y = seed / 2147483648
        self.stars[index] = {x = -1100 + x * 2200, y = -540 + y * 520, size = 1.5 + (index % 4) * 0.9, phase = index * 1.7}
    end
end

function Scenery:update(dt)
    self.time = self.time + dt
end

-- Draws the landscape across the whole visible area, which grows past the world of 1920 by 1080 when the play area has another shape.
function Scenery:draw(night)
    local area = graphics2d.canvasBounds()
    local tint = night and Scenery.nightTint or nil
    if night then
        ParticleTest.backdrop({0.04, 0.05, 0.13}, {0.16, 0.17, 0.32})
        self:drawStars()
        graphics2d.drawShape({Scenery.sun[1] - 190, Scenery.sun[2] - 190, 380, 380}, {kind = 'ellipse', color = '#40B8C4FF', softness = 160, layer = -9})
        graphics2d.draw(self.moonImage, Scenery.sun[1], Scenery.sun[2], {width = 170, height = 170, layer = -9})
    else
        ParticleTest.backdrop({0.3, 0.48, 0.8}, {0.88, 0.8, 0.68})
        graphics2d.drawShape({Scenery.sun[1] - 230, Scenery.sun[2] - 230, 460, 460}, {kind = 'ellipse', color = '#90FFE9A0', softness = 220, layer = -9})
        graphics2d.draw(self.sunImage, Scenery.sun[1], Scenery.sun[2], {width = 300, height = 300, rotation = self.time * 0.05, layer = -9})
    end

    -- The clouds repeat every 1920 units and drift to the right, so copies start at the one that covers the left edge.
    local drift = self.time * Scenery.cloudSpeed % 1920
    local cloudColor = night and '#FF6C76A8' or nil
    for x = area.x - (area.x + 960 - drift) % 1920, area:right(), 1920 do
        graphics2d.draw(self.clouds, x, -540, {width = 1920, height = 520, pivotX = 0, pivotY = 0, color = cloudColor, layer = -8})
    end
    local left, width = math.min(-960, area.x), math.max(1920, area.width)
    graphics2d.draw(self.mountains, left, -260, {width = width, height = 800, pivotX = 0, pivotY = 0, color = tint, layer = -7})
    graphics2d.draw(self.hills, left, 0, {width = width, height = 540, pivotX = 0, pivotY = 0, color = tint, layer = -6})
    for _, tree in ipairs(Scenery.trees) do
        graphics2d.draw(self.tree, tree[1], tree[2], {scaleX = tree[3], scaleY = tree[3], pivotY = 1, color = tint, layer = -5})
    end

    local stripWidth = Scenery.earthHeight * self.earthStrip.width / self.earthStrip.height
    for x = area.x - (area.x % stripWidth), area:right(), stripWidth do
        graphics2d.draw(self.earthStrip, x, Scenery.earthTop, {width = stripWidth, height = Scenery.earthHeight, pivotX = 0, pivotY = 0, color = tint, layer = -4})
    end
    local below = Scenery.earthTop + Scenery.earthHeight - 1
    graphics2d.drawRect({area.x, below, area.width, math.max(0, area:bottom() - below)}, night and '#FF2E1A13' or Scenery.earth, {layer = -4})
    for x = left, left + width, 384 do
        graphics2d.draw(self.grass, x, Scenery.ground + 10, {width = 384, height = 170, pivotX = 0, pivotY = 1, color = tint, layer = -3})
    end
end

-- The stars twinkle, each at a pace of its own.
function Scenery:drawStars()
    for _, star in ipairs(self.stars) do
        local glow = 0.55 + 0.45 * math.sin(self.time * 1.6 + star.phase)
        graphics2d.drawCircle(star.x, star.y, star.size, {1, 0.97, 0.86, glow}, {layer = -9})
    end
end

return Scenery
