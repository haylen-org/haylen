-- The meadow the camera tests look at: a baked grass floor with a grid, a pond, trees and rocks, all drawn with primitives.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local World = haylen.class('World')

World.bounds = m.rect(-2400, -1400, 4800, 2800)

-- Visibility bits of the draws, so a minimap can leave the details out.
World.terrain = 1
World.details = 2

function World:init()
    local bounds = World.bounds
    local tiles = graphics2d.newSpriteBatch(graphics.whiteTexture())
    for row = 0, bounds.height // 160 - 1 do
        for column = 0, bounds.width // 160 - 1 do
            local shade = (row + column) % 2 == 0 and {0.32, 0.55, 0.3, 1} or {0.36, 0.6, 0.33, 1}
            tiles:add({x = bounds.x + column * 160, y = bounds.y + row * 160, width = 160, height = 160, pivotX = 0, pivotY = 0, color = shade})
        end
    end
    self.floor = tiles:bake()

    local random = m.random(12)
    self.trees, self.rocks = {}, {}
    for _ = 1, 60 do
        local x, y = random:range(bounds.x + 100, bounds:right() - 100), random:range(bounds.y + 100, bounds:bottom() - 100)
        if math.abs(x - 600) > 420 or math.abs(y + 300) > 300 then
            self.trees[#self.trees + 1] = {x = x, y = y, size = random:range(40, 80)}
        end
    end
    for _ = 1, 30 do
        self.rocks[#self.rocks + 1] = {x = random:range(bounds.x, bounds:right()), y = random:range(bounds.y, bounds:bottom()), size = random:range(16, 34)}
    end
end

-- Draws the world into the active canvas. Terrain goes on layer 0 and details on layer 2, so players can stand on layer 1 between them.
function World:draw()
    local bounds = World.bounds
    graphics2d.drawStatic(self.floor, 0, 0, {visibility = World.terrain})
    graphics2d.drawRectOutline(bounds, 24, '#FF2A4A2A', {visibility = World.terrain})
    graphics2d.drawCircle(600, -300, 320, '#FF3A70B0', {visibility = World.terrain})
    graphics2d.drawRing(600, -300, 320, 16, '#FF8AC0E0', {visibility = World.terrain})
    graphics2d.drawRect({-2400, 380, 4800, 90}, '#FFB09A70', {visibility = World.terrain})
    for _, rock in ipairs(self.rocks) do
        graphics2d.drawCircle(rock.x, rock.y, rock.size, '#FF8A8A92', {layer = 2, visibility = World.details}, 7)
    end
    for _, tree in ipairs(self.trees) do
        graphics2d.drawCircle(tree.x + 10, tree.y + 14, tree.size, '#50000000', {layer = 2, visibility = World.details})
        graphics2d.drawCircle(tree.x, tree.y, tree.size, '#FF2E6E34', {layer = 2, visibility = World.details})
        graphics2d.drawCircle(tree.x - tree.size * 0.25, tree.y - tree.size * 0.25, tree.size * 0.45, '#FF4A9A4A', {layer = 2, visibility = World.details})
    end
end

return World
