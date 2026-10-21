-- The village of the street scenes of the lighting tests: the painted cottages with windows whose panes glow at night, the sun and the moon, the neon sign and the wall torch of `content/lighting/scenery.png`, a sky drawn from a gradient and the hills and ground of the camera tests.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Town = haylen.class('Town')

Town.atlas = 'lighting/scenery.json'

-- The glass of every window and the top of the chimney of each house, in pixels of its frame, whose bottom edge stands on the ground.
Town.houses = {
    cottage = {windows = {{124, 332, 68, 72}, {311, 152, 72, 70}, {489, 332, 66, 71}}, chimney = {188, 10}},
    ['stone-house'] = {windows = {{108, 369, 49, 127}, {207, 166, 61, 125}, {319, 369, 49, 127}}, chimney = {360, 10}},
}

-- The lamp post of the props tileset, with the glass of its lantern, in pixels of its tile.
Town.lampTiles = 'tiled/tiles/props.png'
Town.postSource = {266, 2, 128, 256}
Town.lantern = {110, 137}

Town.glass = {night = '#FFFFC870', day = '#FF33465E'}

function Town:init()
    self.scenery = assets.load(Town.atlas, 'atlas', {filter = 'linear'})
    self.lamps = assets.texture(Town.lampTiles, {filter = 'linear'})
    self.hills = assets.texture('camera/hills.png', {filter = 'linear', wrap = 'repeat'})
    self.ground = assets.texture('camera/ground.png', {filter = 'linear', wrap = 'repeat'})

    -- A white sky that fades out toward the horizon, which the color of the hour tints over the color of the horizon.
    local fade = graphics.newImage(4, 256)
    fade:fillGradient({angle = math.pi / 2, stops = {{0, '#FFFFFFFF'}, {1, '#00FFFFFF'}}})
    self.fade = graphics.newTexture(fade, {filter = 'linear'})
end

-- Returns the size of a frame of the atlas.
function Town:size(name)
    local source = self.scenery:source(name)
    return source.width, source.height
end

-- Draws a piece of the atlas with its options, at its pivot.
function Town:piece(name, x, y, scale, options)
    local width, height = self:size(name)
    options.source = self.scenery:source(name)
    options.width, options.height = width * scale, height * scale
    graphics2d.draw(self.scenery.texture, x, y, options)
end

-- Draws a house standing on `ground` with its middle at `x`, its window panes behind its frames on the layer below it, lit by `glow` from 0 by day to 1 at night, and its walls multiplied by `tint`, such as a dark blue for a rainy night.
function Town:house(name, x, ground, scale, layer, glow, tint)
    local width, height = self:size(name)
    local left, top = x - width * scale / 2, ground - height * scale
    local color = glow > 0 and Town.glass.night or Town.glass.day
    for _, window in ipairs(Town.houses[name].windows) do
        graphics2d.drawRect({left + window[1] * scale - 2, top + window[2] * scale - 2, window[3] * scale + 4, window[4] * scale + 4}, color, {layer = layer, emission = glow})
    end
    self:piece(name, x, ground, scale, {pivotY = 1, layer = layer + 1, color = tint})
end

-- Returns where the chimney of a house drawn by `house` ends, in world units.
function Town:chimney(name, x, ground, scale)
    local width, height = self:size(name)
    local chimney = Town.houses[name].chimney
    return x - width * scale / 2 + chimney[1] * scale, ground - height * scale + chimney[2] * scale
end

-- Draws a lamp post standing on `ground`, whose lantern glows by `glow`, and returns where its lantern is.
function Town:lampPost(x, ground, height, layer, glow)
    local scale = height / Town.postSource[4]
    graphics2d.draw(self.lamps, x, ground, {source = Town.postSource, width = Town.postSource[3] * scale, height = height, pivotY = 1, layer = layer})
    local lanternX = x + (Town.lantern[1] - Town.postSource[3] / 2) * scale
    local lanternY = ground - height + Town.lantern[2] * scale
    if glow > 0 then
        graphics2d.drawCircle(lanternX, lanternY, 7 * scale, '#B0FFE2A0', {layer = layer + 1, emission = 1.6 * glow})
        graphics2d.draw(graphics2d.lightTexture(), lanternX, lanternY, {width = 120 * scale, height = 120 * scale, color = '#70FFC870', layer = layer + 1, emission = 0.8 * glow})
    end
    return lanternX, lanternY
end

-- Fills the canvas with a sky that runs from `zenith` at the top to `horizon` at `bottom`, which keeps its colors whatever the light.
function Town:sky(zenith, horizon, bottom)
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect({area.x, area.y, area.width, bottom - area.y}, horizon, {layer = -20, unshaded = true})
    graphics2d.draw(self.fade, area.x, area.y, {width = area.width, height = bottom - area.y, pivotX = 0, pivotY = 0, color = zenith, layer = -19, unshaded = true})
end

-- Repeats a strip of scenery across the canvas with its bottom edge on `bottom` and the height `height`.
function Town.strip(texture, bottom, height, layer, color)
    local area = graphics2d.canvasBounds()
    local width = texture.width * height / texture.height
    local first = math.floor(area.x / width) * width
    graphics2d.draw(texture, first, bottom - height, {source = {0, 0, texture.width * math.ceil((area:right() - first) / width), texture.height}, width = width * math.ceil((area:right() - first) / width), height = height, pivotX = 0, pivotY = 0, layer = layer, color = color})
end

-- Draws the hills behind the village and the ground it stands on, down to the bottom of the canvas.
function Town:land(horizon, ground)
    local area = graphics2d.canvasBounds()
    Town.strip(self.hills, ground + 40, ground + 40 - horizon, -10)
    Town.strip(self.ground, area:bottom() + 10, area:bottom() + 10 - ground + 60, 0)
end

return Town
