-- A landscape seen from the side behind the sprite tests that stand things on the ground: a sky, drifting clouds, mountains, hills and a strip of grass over soil, from the painted strips of `content/camera/`.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Landscape = haylen.class('Landscape')

Landscape.textureOptions = {filter = 'linear', mipmaps = true}
Landscape.sky = {{0, '#FF4FA8EC'}, {0.7, '#FFA8DCF8'}, {1, '#FFE2F4FF'}}
-- The row of the ground image that things stand on, in its pixels, and the color of the soil under the image.
Landscape.surface = 80
Landscape.soil = '#FF80401E'
Landscape.layer = -100
Landscape.cloudSpeed = 14
Landscape.cloudBands = 6
Landscape.smallestCloud = 40

function Landscape:init()
    local sky = graphics.newImage(4, 256)
    sky:fillGradient({angle = math.pi / 2, stops = Landscape.sky})
    self.skyTexture = graphics.newTexture(sky, {filter = 'linear'})
    self.clouds = assets.texture('camera/clouds.png', Landscape.textureOptions)
    self.mountains = assets.texture('camera/mountains.png', Landscape.textureOptions)
    self.hills = assets.texture('camera/hills.png', Landscape.textureOptions)
    self.ground = assets.texture('camera/ground.png', Landscape.textureOptions)
end

-- Repeats a strip across `width` units at `height` units tall, with its bottom at `bottom` and moved left by `shift` units.
function Landscape.strip(texture, width, bottom, height, shift, layer)
    local tile = texture.width * height / texture.height
    local x = -(shift % tile)
    while x < width do
        graphics2d.draw(texture, x, bottom, {width = tile + 1, height = height, pivotX = 0, pivotY = 1, layer = layer})
        x = x + tile
    end
end

-- Draws the landscape over `area` with the ground that things stand on at `groundY`, the strip of grass `scale` units per pixel of its image, the clouds drifted by `time` seconds and the ground moved left by `travel` units, with the hills and mountains behind it moving slower, below every draw of a layer above `Landscape.layer`.
function Landscape:draw(area, groundY, scale, time, travel)
    local layer, width = Landscape.layer, area.width
    local distant = math.min(groundY, math.max(width, area.height) * 0.45)
    travel = travel or 0
    graphics2d.draw(self.skyTexture, 0, 0, {width = width, height = groundY, pivotX = 0, pivotY = 0, layer = layer})
    local cloudBottom, cloudHeight = groundY - distant * 0.5, distant * 0.4
    local drift = time * Landscape.cloudSpeed + travel * 0.05
    Landscape.strip(self.clouds, width, cloudBottom, cloudHeight, drift, layer + 1)
    -- A sky taller than the landscape holds more bands of clouds, smaller and slower the higher they float.
    for band = 1, Landscape.cloudBands do
        if cloudBottom - cloudHeight < cloudHeight * 0.5 or cloudHeight * 0.8 < Landscape.smallestCloud then
            break
        end
        cloudBottom, cloudHeight = cloudBottom - cloudHeight * 1.3, cloudHeight * 0.8
        Landscape.strip(self.clouds, width, cloudBottom, cloudHeight, drift * 0.7 ^ band + band * 211, layer + 1)
    end
    Landscape.strip(self.mountains, width, groundY, distant * 0.62, width * 0.3 + travel * 0.15, layer + 2)
    Landscape.strip(self.hills, width, groundY + 8, distant * 0.62, travel * 0.4, layer + 3)

    local top = groundY - Landscape.surface * scale
    local height = self.ground.height * scale
    graphics2d.drawRect({0, top + height - 2, width, area.height - top - height + 2}, Landscape.soil, {layer = layer + 4})
    Landscape.strip(self.ground, width, top + height, height, travel, layer + 5)
end

return Landscape
