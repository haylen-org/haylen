-- The courtyard most lighting tests stand in: a floor of painted flagstones with wooden crates and round bushes from the painted tiles of the project, the occluders that cast their shadows, and the lamps that show where the lights of a test shine from.
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
-- The painted flagstones repeat every this many world units.
Room.floor = 'lighting/courtyard.png'
Room.floorTile = 560
Room.outlineColor = m.color('#FF1E1B33')
Room.ironColor = m.color('#FF4E4A66')
Room.white = m.color('#FFFFFFFF')

-- Crates and bushes in world units around the origin, the size of what shows of them.
Room.props = {
    {kind = 'crate', x = -520, y = -120, size = 120},
    {kind = 'crate', x = -440, y = 200, size = 96},
    {kind = 'bush', x = -180, y = -220, size = 110},
    {kind = 'crate', x = 60, y = 260, size = 140},
    {kind = 'bush', x = 300, y = -110, size = 130},
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
    self.courtyard = assets.texture(Room.floor, {filter = 'linear', wrap = 'repeat', mipmaps = true})

    -- The props shade their own sides, so only what lies behind them falls in shadow.
    self.occluders = {}
    for index, prop in ipairs(Room.props) do
        self.occluders[index] = lighting2d.newOccluder({points = Room.outline(prop), cull = 'counterClockwise'})
    end
end

-- Draws the flagstones over the whole canvas on layer 0, as one sprite whose source repeats the painted floor, with the draw options of `order`.
function Room:drawFloor(order)
    local area = graphics2d.canvasBounds()
    local size = Room.floorTile
    local left, top = math.floor(area.x / size) * size, math.floor(area.y / size) * size
    local width, height = area:right() - left, area:bottom() - top
    local options = order or {}
    options.source = {0, 0, self.courtyard.width * width / size, self.courtyard.height * height / size}
    options.width, options.height, options.pivotX, options.pivotY = width, height, 0, 0
    graphics2d.draw(self.courtyard, left, top, options)
end

-- Draws the floor on layer 0 and the props on layer 1 with their occluders.
function Room:draw()
    self:drawFloor()
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

-- Draws a wall of bricks along the points, a flat list of x and y pairs, with square ends and the outline of the props.
function Room.drawWall(bricks, points, width, layer)
    graphics2d.drawStroke(points, {width = width + 10, color = Room.outlineColor, cap = 'square', layer = layer})
    graphics2d.drawStroke(points, {width = width, texture = bricks, textureMode = 'tile', textureLength = width * 2, cap = 'square', layer = layer + 1})
end

-- Draws a wooden door seen from above between its hinge and its end: a thick plank with its grain along it, two iron bands across it and the iron hinge it turns on.
function Room.drawDoor(hingeX, hingeY, endX, endY, thickness, layer)
    local length = math.sqrt((endX - hingeX) ^ 2 + (endY - hingeY) ^ 2)
    local angle = math.atan(endY - hingeY, endX - hingeX)
    local dx, dy = math.cos(angle), math.sin(angle)
    local centerX, centerY = (hingeX + endX) / 2, (hingeY + endY) / 2
    graphics2d.drawShape({centerX - length / 2, centerY - thickness / 2, length, thickness}, {radius = 6, rotation = angle, color = '#FFB0743F', borderWidth = 5, borderColor = Room.outlineColor, layer = layer})
    graphics2d.drawShape({centerX - length / 2 + 8, centerY - thickness / 2 + 7, length - 16, 5}, {radius = 2.5, rotation = angle, color = '#FFD39A5C', layer = layer + 1})
    for _, share in ipairs({0.28, 0.78}) do
        local x, y = hingeX + dx * length * share, hingeY + dy * length * share
        graphics2d.drawShape({x - 6, y - thickness / 2 - 2, 12, thickness + 4}, {radius = 3, rotation = angle, color = Room.ironColor, borderWidth = 3, borderColor = Room.outlineColor, layer = layer + 2})
    end
    graphics2d.drawCircle(hingeX, hingeY, thickness * 0.6, Room.outlineColor, {layer = layer + 2})
    graphics2d.drawCircle(hingeX, hingeY, thickness * 0.42, Room.ironColor, {layer = layer + 3})
    graphics2d.drawCircle(hingeX - 3, hingeY - 3, thickness * 0.14, '#FF8A86A8', {layer = layer + 3})
end

-- Draws a lamp where a light shines from: a lantern seen from above, its glass of the color of the light in a rim of dark iron, with a soft halo around it. The lantern keeps its colors whatever the light, so it never washes out under its own light or a stronger one.
function Room.drawLamp(x, y, color, size)
    size = size or 30
    local glass = m.color(color)
    local order = {layer = 5, unshaded = true}
    graphics2d.draw(graphics2d.lightTexture(), x, y, {width = size * 5, height = size * 5, color = glass:withAlpha(0.45), layer = 4, emission = 0.8})
    graphics2d.drawCircle(x, y, size * 0.62, Room.outlineColor, order)
    graphics2d.drawCircle(x, y, size * 0.52, Room.ironColor, order)
    graphics2d.drawCircle(x, y, size * 0.4, glass:lerp(Room.outlineColor, 0.25), order)
    graphics2d.drawCircle(x - size * 0.03, y - size * 0.03, size * 0.33, glass, order)
    graphics2d.drawCircle(x - size * 0.05, y - size * 0.05, size * 0.17, glass:lerp(Room.white, 0.7), order)
    graphics2d.drawCircle(x - size * 0.2, y - size * 0.2, size * 0.07, Room.white, order)
end

return Room
