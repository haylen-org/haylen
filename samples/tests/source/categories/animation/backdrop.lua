-- The side view behind the fighter and the puppets: a sky that pales toward the horizon with drifting clouds, the hills and the ground strip of the camera tests, laid across the whole play area so the top of the grass meets the line the characters stand on. A far ledge of the same ground can stand between the hills and the near ground, and the strips scroll by an offset, the far ones slower, so a character that walks in place moves through the land.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local linear = {filter = 'linear'}

local Backdrop = haylen.class('Backdrop')

-- The ground strip is 1076 by 256 pixels, and characters stand this share of its height below its top, on the grass.
Backdrop.groundSize = {1076, 256}
Backdrop.footing = 70 / 256
Backdrop.hillsSize = {1096, 538}
Backdrop.cloudsSize = {1024, 322}
Backdrop.drift = 14
Backdrop.soil = '#FF813E1F'
Backdrop.haze = '#50DCEBF5'

function Backdrop:init()
    local sky = graphics.newImage(4, 256)
    sky:fillGradient({angle = math.pi / 2, stops = {{0, '#FF5FB4EC'}, {0.75, '#FFAEDDF6'}, {1, '#FFE4F5FB'}}})
    self.sky = graphics.newTexture(sky, linear)
    self.clouds = assets.texture('camera/clouds.png', linear)
    self.hills = assets.texture('camera/hills.png', linear)
    self.ground = assets.texture('camera/ground.png', linear)
end

-- Fits a world of `size` around the origin into the play area of a test and puts the line `ground` at `share` of the height the view shows, so a tall play area shows more sky and a wide one more land.
function Backdrop.frame(test, size, ground, share)
    local area = test.area
    local zoom = math.min(area.width / size[1], area.height / size[2])
    local width, height = area.width / zoom, area.height / zoom
    test.camera.zoom = {zoom, zoom}
    test.camera.position = {-width / 2, ground - height * share}
end

-- Draws the view over `bounds`, the area the canvas shows, with the characters standing on the line `ground` of a strip `height` units tall, scrolled by `offset`. The optional `ledge`, a table with its own `ground` and `height`, adds a far ground paled by the haze, which scrolls slower.
function Backdrop:draw(bounds, ground, height, offset, ledge)
    offset = offset or 0
    local top = ground - height * Backdrop.footing
    local horizon = ledge and ledge.ground - ledge.height * Backdrop.footing or top
    graphics2d.draw(self.sky, bounds.x, bounds.y, {width = bounds.width, height = horizon - bounds.y + height * 0.5, pivotX = 0, pivotY = 0, layer = -31})

    -- Bands of clouds rise from behind the hills to the top of the view, each shifted and drifting a little faster than the one below it.
    local cloudsHeight = height * 0.6
    local cloudsWidth = cloudsHeight * Backdrop.cloudsSize[1] / Backdrop.cloudsSize[2]
    local band, bottom = 0, horizon - height * 0.3
    while bottom > bounds.y do
        local shift = band * cloudsWidth * 0.37 + offset * 0.1 - haylen.elapsed() * Backdrop.drift * (1 + band * 0.3)
        for x = bounds.x - (bounds.x + shift) % cloudsWidth, bounds:right(), cloudsWidth do
            graphics2d.draw(self.clouds, x, bottom, {width = cloudsWidth + 1, height = cloudsHeight, pivotX = 0, pivotY = 1, color = '#E6FFFFFF', layer = -30})
        end
        band, bottom = band + 1, bottom - height * 1.6
    end

    local hillsHeight = height * 1.5
    local hillsWidth = hillsHeight * Backdrop.hillsSize[1] / Backdrop.hillsSize[2]
    for x = bounds.x - (bounds.x + offset * 0.25) % hillsWidth, bounds:right(), hillsWidth do
        graphics2d.draw(self.hills, x, horizon + height * 0.4, {width = hillsWidth + 1, height = hillsHeight, pivotX = 0, pivotY = 1, layer = -29})
    end
    if ledge then
        self:strip(bounds, ledge.ground, ledge.height, offset * 0.6, Backdrop.haze, -28)
    end
    self:strip(bounds, ground, height, offset, nil, -27)
end

function Backdrop:strip(bounds, ground, height, offset, haze, layer)
    local top = ground - height * Backdrop.footing
    local width = height * Backdrop.groundSize[1] / Backdrop.groundSize[2]
    for x = bounds.x - (bounds.x + offset) % width, bounds:right(), width do
        graphics2d.draw(self.ground, x, top, {width = width + 1, height = height, pivotX = 0, pivotY = 0, flash = haze, layer = layer})
    end
    if bounds:bottom() > top + height - 1 then
        graphics2d.drawRect({bounds.x, top + height - 1, bounds.width, bounds:bottom() - top - height + 1}, Backdrop.soil, {layer = layer})
    end
end

-- Draws the soft shadow of a character on the ground, under the layer the character draws in.
function Backdrop.shadow(x, y, width, layer)
    graphics2d.drawShape({x - width / 2, y - width * 0.09, width, width * 0.18}, {kind = 'ellipse', color = '#55203018', softness = width * 0.08, layer = layer})
end

return Backdrop
