-- The landscape behind the screen and sprite effect tests, laid out over a world of 1920 by 1080 around the origin: a sky, a sun bright enough to bloom, drifting clouds, mountains, hills, trees and grass, all below layer 0.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local ParticleTest = require('categories.particles.particle-test')

local Scenery = haylen.class('Scenery')

Scenery.ground = 400
Scenery.cloudSpeed = 24
Scenery.trees = {{-820, 330, 2.4}, {-640, 360, 1.9}, {660, 340, 2.6}, {860, 370, 2}}

function Scenery:init()
    local linear = {filter = 'linear'}
    self.clouds = assets.texture('camera/clouds.png', linear)
    self.mountains = assets.texture('camera/mountains.png', linear)
    self.hills = assets.texture('camera/hills.png', linear)
    self.grass = assets.texture('camera/grass.png', linear)
    self.tree = assets.texture('sprites/images/tree.png', linear)
    self.time = 0
end

function Scenery:update(dt)
    self.time = self.time + dt
end

-- Draws the landscape across the whole visible area, which grows past the world of 1920 by 1080 when the play area has another shape.
function Scenery:draw()
    local area = graphics2d.canvasBounds()
    ParticleTest.backdrop({0.3, 0.48, 0.8}, {0.88, 0.8, 0.68})
    graphics2d.drawCircle(480, -300, 150, '#50FFF0C0', {layer = -9})
    graphics2d.drawCircle(480, -300, 96, '#FFFFFAE6', {layer = -9})

    -- The clouds repeat every 1920 units and drift to the right, so copies start at the one that covers the left edge.
    local drift = self.time * Scenery.cloudSpeed % 1920
    for x = area.x - (area.x + 960 - drift) % 1920, area:right(), 1920 do
        graphics2d.draw(self.clouds, x, -540, {width = 1920, height = 520, pivotX = 0, pivotY = 0, color = '#C0FFFFFF', layer = -8})
    end
    local left, width = math.min(-960, area.x), math.max(1920, area.width)
    graphics2d.draw(self.mountains, left, -260, {width = width, height = 800, pivotX = 0, pivotY = 0, layer = -7})
    graphics2d.draw(self.hills, left, 0, {width = width, height = 540, pivotX = 0, pivotY = 0, layer = -6})
    for _, tree in ipairs(Scenery.trees) do
        graphics2d.draw(self.tree, tree[1], tree[2], {scaleX = tree[3], scaleY = tree[3], pivotY = 1, layer = -5})
    end
    graphics2d.drawRect({area.x, Scenery.ground, area.width, area:bottom() - Scenery.ground}, '#FF2F5A2C', {layer = -4})
    for x = left, left + width, 384 do
        graphics2d.draw(self.grass, x, Scenery.ground + 10, {width = 384, height = 96, pivotX = 0, pivotY = 1, layer = -3})
    end
end

return Scenery
