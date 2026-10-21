-- The courtyard most lighting tests stand in: a floor of stone slabs with wooden crates and round bushes from the painted tiles of the project, the occluders that cast their shadows, and the lamps that show where the lights of a test shine from.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')

local Room = haylen.class('Room')

-- The tiles of 128 pixels in the painted tileset, and the share of a tile that a crate or a bush fills.
Room.tileset = 'tiled/tiles/terrain.png'
Room.sources = {floor = {530, 2, 128, 128}, bricks = {662, 2, 128, 128}, crate = {530, 266, 128, 128}, bush = {266, 266, 128, 128}}
Room.fill = {crate = 110 / 128, bush = 100 / 128}
Room.slab = 160

-- Crates and bushes in world units around the origin, the size of what shows of them.
Room.props = {
    {kind = 'crate', x = -520, y = -120, size = 120},
    {kind = 'crate', x = -440, y = 200, size = 96},
    {kind = 'bush', x = -180, y = -220, size = 110},
    {kind = 'crate', x = 60, y = 260, size = 140},
    {kind = 'bush', x = 240, y = -60, size = 130},
    {kind = 'crate', x = 520, y = -200, size = 110},
    {kind = 'bush', x = 600, y = 220, size = 100},
}

-- Returns the outline of a prop, clockwise on screen.
function Room.outline(prop)
    local half = prop.size / 2
    if prop.kind == 'crate' then
        return {prop.x - half, prop.y - half, prop.x + half, prop.y - half, prop.x + half, prop.y + half, prop.x - half, prop.y + half}
    end
    local points = {}
    for index = 0, 23 do
        local angle = index / 24 * math.pi * 2
        points[#points + 1] = prop.x + math.cos(angle) * half
        points[#points + 1] = prop.y + math.sin(angle) * half
    end
    return points
end

function Room:init()
    self.texture = assets.texture(Room.tileset, {filter = 'linear'})
    local slabs = graphics2d.newSpriteBatch(self.texture)
    local size = Room.slab
    for row = -10, 9 do
        for column = -16, 15 do
            slabs:add({x = column * size, y = row * size, width = size, height = size, pivotX = 0, pivotY = 0, source = Room.sources.floor})
        end
    end
    self.floor = slabs:bake()

    -- The props shade their own sides, so only what lies behind them falls in shadow.
    self.occluders = {}
    for index, prop in ipairs(Room.props) do
        self.occluders[index] = lighting2d.newOccluder({points = Room.outline(prop), cull = 'counterClockwise'})
    end
end

-- Draws the floor on layer 0 and the props on layer 1 with their occluders.
function Room:draw()
    graphics2d.drawStatic(self.floor)
    for index, prop in ipairs(Room.props) do
        local size = prop.size / Room.fill[prop.kind]
        graphics2d.draw(self.texture, prop.x, prop.y, {source = Room.sources[prop.kind], width = size, height = size, layer = 1})
        graphics2d.drawOccluder(self.occluders[index])
    end
end

-- Returns the bricks of the tileset as a texture of their own that repeats along a wall.
function Room.bricks()
    local tiles = assets.load(Room.tileset, 'image')
    return graphics.newTexture(tiles:crop(Room.sources.bricks), {filter = 'linear', wrap = 'repeat'})
end

-- Draws a wall of bricks along the points, a flat list of x and y pairs, with the outline of the props.
function Room.drawWall(bricks, points, width, layer)
    graphics2d.drawStroke(points, {width = width + 10, color = '#FF1E1B33', join = 'round', cap = 'round', layer = layer})
    graphics2d.drawStroke(points, {width = width, texture = bricks, textureMode = 'tile', textureLength = width * 2, join = 'round', cap = 'round', layer = layer + 1})
end

-- Draws a lamp where a light shines from: a glass orb of the color of the light with a soft halo, which glows whatever the light around it.
function Room.drawLamp(x, y, color, size)
    size = size or 30
    graphics2d.draw(graphics2d.lightTexture(), x, y, {width = size * 5, height = size * 5, color = m.color(color):withAlpha(0.45), layer = 4, emission = 0.8})
    graphics2d.drawCircle(x, y, size * 0.56, '#FF1E1B33', {layer = 5, unshaded = true})
    graphics2d.drawCircle(x, y, size * 0.48, color, {layer = 5, emission = 2})
    graphics2d.drawCircle(x, y, size * 0.3, '#FFFFFFFF', {layer = 5, emission = 1.2})
    graphics2d.drawCircle(x - size * 0.18, y - size * 0.18, size * 0.1, '#FFFFFFFF', {layer = 5, unshaded = true})
end

return Room
