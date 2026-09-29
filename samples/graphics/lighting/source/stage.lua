-- The room most tests stand in: a stone floor, crates and pillars, with the occluders that cast their shadows.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')

local Stage = haylen.class('Stage')

-- Crates are squares and pillars circles, in world units around the origin.
Stage.props = {
    {kind = 'crate', x = -520, y = -120, size = 90},
    {kind = 'crate', x = -440, y = 200, size = 70},
    {kind = 'pillar', x = -180, y = -220, size = 46},
    {kind = 'crate', x = 60, y = 260, size = 110},
    {kind = 'pillar', x = 240, y = -60, size = 56},
    {kind = 'crate', x = 520, y = -200, size = 80},
    {kind = 'pillar', x = 600, y = 220, size = 40},
}

-- Returns the outline of a prop, clockwise on screen.
function Stage.outline(prop)
    local half = prop.size / 2
    if prop.kind == 'crate' then
        return {prop.x - half, prop.y - half, prop.x + half, prop.y - half, prop.x + half, prop.y + half, prop.x - half, prop.y + half}
    end
    local points = {}
    for index = 0, 15 do
        local angle = index / 16 * math.pi * 2
        points[#points + 1] = prop.x + math.cos(angle) * half
        points[#points + 1] = prop.y + math.sin(angle) * half
    end
    return points
end

function Stage:init()
    local tiles = graphics2d.newSpriteBatch(graphics.whiteTexture())
    for row = -15, 14 do
        for column = -24, 23 do
            local shade = 0.52 + ((row * 7 + column * 13) % 5) * 0.025
            tiles:add({x = column * 64, y = row * 64, width = 62, height = 62, pivotX = 0, pivotY = 0, color = {shade, shade * 0.96, shade * 0.9, 1}})
        end
    end
    self.floor = tiles:bake()

    -- The props shade their own sides, so only what lies behind them falls in shadow.
    self.occluders = {}
    for index, prop in ipairs(Stage.props) do
        self.occluders[index] = lighting2d.newOccluder({points = Stage.outline(prop), cull = 'counterClockwise'})
    end
end

-- Draws the floor on layer 0 and the props on layer 1, with their occluders when the canvas is lit.
function Stage:draw(lit)
    graphics2d.drawStatic(self.floor)
    for index, prop in ipairs(Stage.props) do
        self:drawProp(prop)
        if lit then
            graphics2d.drawOccluder(self.occluders[index])
        end
    end
end

function Stage:drawProp(prop)
    local half = prop.size / 2
    if prop.kind == 'crate' then
        graphics2d.drawRect({prop.x - half, prop.y - half, prop.size, prop.size}, '#FF8A5A34', {layer = 1})
        graphics2d.drawRectOutline({prop.x - half, prop.y - half, prop.size, prop.size}, 8, '#FF5E3A1E', {layer = 1})
        graphics2d.drawLine(prop.x - half, prop.y - half, prop.x + half, prop.y + half, 8, '#FF5E3A1E', {layer = 1})
        return
    end
    graphics2d.drawCircle(prop.x, prop.y, half, '#FF8C8C94', {layer = 1})
    graphics2d.drawRing(prop.x, prop.y, half - 5, 10, '#FF5C5C66', {layer = 1})
end

return Stage
