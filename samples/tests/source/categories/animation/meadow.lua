-- The meadow under the crowd and the walker: the painted grass of the Tiled tests with a few flowers, as a tile layer over a rectangle of the world.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local Meadow = haylen.class('Meadow')

Meadow.grass = {2, 2, 128, 128}
Meadow.flowers = {134, 2, 128, 128}
Meadow.flowerShare = 0.07

-- Covers `rect` of the world with cells of `cell` units, and more cells around it so a view a little wider than the rectangle shows grass too.
function Meadow:init(rect, cell)
    self.layer = graphics2d.newTileLayer(assets.texture('tiled/tiles/terrain.png', {filter = 'linear'}), {cellWidth = cell, cellHeight = cell})
    self.layer:defineFrame(1, {source = Meadow.grass, width = cell, height = cell})
    self.layer:defineFrame(2, {source = Meadow.flowers, width = cell, height = cell})
    local random = m.random(5)
    for row = math.floor(rect.y / cell) - 3, math.ceil((rect.y + rect.height) / cell) + 3 do
        for column = math.floor(rect.x / cell) - 3, math.ceil((rect.x + rect.width) / cell) + 3 do
            self.layer:setCell(column, row, random:chance(Meadow.flowerShare) and 2 or 1)
        end
    end
end

function Meadow:draw()
    self.layer:draw({layer = -30})
end

return Meadow
