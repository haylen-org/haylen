-- The ground band along the bottom of the visible area, its flowers, the grip at its left end and, in window mode, a sky behind everything.
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local viewport = require('haylen.viewport')

local config = require('config')

local stage = {}
stage.__index = stage

local tileWidth = 16 * config.pixel
local skyTop = '#FF5B8FD6'
local skyBottom = '#FFBFE3F0'
local hills = {
    {color = '#FFA3CFD0', height = 90, wave = 0.005, phase = 0},
    {color = '#FF74B48E', height = 48, wave = 0.012, phase = 2},
}

-- Flowers and stones sit at fixed places of the design space, so they stay put when the window changes its width.
local function scatter(art)
    local random = m.random(11)
    local decorations = {}
    local x = -4000
    while x < 6000 do
        decorations[#decorations + 1] = {x = x, source = art.decorations.all:frame(random:integer(1, 4))}
        x = x + random:range(40, 140)
    end
    return decorations
end

function stage.new(art)
    local self = setmetatable({art = art, decorations = scatter(art)}, stage)
    self:layout()
    return self
end

-- Places everything against the visible area of this frame, which follows the window in both modes.
function stage:layout()
    local visible = viewport.visibleRect()
    local pixel = config.pixel
    self.visible = visible
    self.groundTop = visible:bottom() - config.groundHeight
    self.band = m.rect(visible.x, self.groundTop + 2 * pixel, visible.width, config.groundHeight - 2 * pixel)
    self.grip = m.rect(visible.x + 12, self.groundTop - 12 * pixel, 10 * pixel, 16 * pixel)
    self.lane = {left = visible.x + config.hero.marginLeft, right = visible:right() - config.hero.marginRight, ground = self.groundTop + 3 * pixel}
end

-- Returns whether a press at `x`, `y` grabs the window: on the ground band or on the grip.
function stage:grabs(x, y)
    return self.band:contains({x, y}) or self.grip:contains({x, y})
end

function stage:collectRegions(regions)
    regions[#regions + 1] = self.band
    regions[#regions + 1] = self.grip
end

function stage:drawBackdrop(time)
    local visible = self.visible
    local left, right, top, bottom = visible.x, visible:right(), visible.y, visible:bottom()
    graphics2d.drawMesh(nil, {
        {x = left, y = top, color = skyTop},
        {x = right, y = top, color = skyTop},
        {x = right, y = bottom, color = skyBottom},
        {x = left, y = bottom, color = skyBottom},
    }, {1, 2, 3, 1, 3, 4}, {layer = config.layer.backdrop})

    local span = visible.width + 200
    for index = 1, 3 do
        local x = left - 100 + (index * 530 + time * (8 + index * 3)) % span
        local y = top + (bottom - top) * (0.12 + index * 0.1)
        graphics2d.draw(self.art.cloud, x, y, {scaleX = config.pixel, scaleY = config.pixel, color = '#E6FFFFFF', layer = config.layer.backdrop})
    end

    -- Two rows of hills step their outlines on the pixel grid of the art.
    local step = 4 * config.pixel
    local first = math.floor(left / step) * step
    for _, hill in ipairs(hills) do
        local points = {}
        local previous
        for x = first, right + step, step do
            local y = math.floor((self.groundTop - hill.height * (0.6 + 0.4 * math.sin(x * hill.wave + hill.phase))) / config.pixel) * config.pixel
            if previous == nil then
                points[#points + 1] = {x, y}
            elseif y ~= previous then
                points[#points + 1] = {x, previous}
                points[#points + 1] = {x, y}
            end
            previous = y
        end
        points[#points + 1] = {right + step, previous}
        points[#points + 1] = {right + step, self.groundTop + step}
        points[#points + 1] = {first, self.groundTop + step}
        graphics2d.drawPolygon(points, hill.color, {layer = config.layer.backdrop})
    end
end

function stage:draw(windowMode, time)
    if windowMode then
        self:drawBackdrop(time)
    end

    local visible = self.visible
    local tiles = {}
    for x = math.floor(visible.x / tileWidth) * tileWidth, visible:right(), tileWidth do
        tiles[#tiles + 1] = {x = x, y = self.groundTop, width = tileWidth, height = config.groundHeight, pivotX = 0, pivotY = 0}
    end
    graphics2d.drawBatch(self.art.ground, tiles, {layer = config.layer.ground})

    local flowers = {}
    local size = 8 * config.pixel
    for _, decoration in ipairs(self.decorations) do
        if decoration.x > visible.x - size and decoration.x < visible:right() then
            flowers[#flowers + 1] = {x = decoration.x, y = self.lane.ground, width = size, height = size, pivotY = 1, source = decoration.source}
        end
    end
    graphics2d.drawBatch(self.art.decorations.texture, flowers, {layer = config.layer.props})

    graphics2d.draw(self.art.grip, self.grip.x, self.grip.y, {pivotX = 0, pivotY = 0, width = self.grip.width, height = self.grip.height, layer = config.layer.props})
end

return stage
