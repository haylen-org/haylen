-- Scale and tint: the wooden frame with its borders scaled from half to twice their size, and a near white frame painted to take a tint, multiplied by colors, with a large frame that takes the scale and the tint of the panel.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local ScaleTint = haylen.class('ScaleTint', Test)

ScaleTint.scales = {0.5, 0.75, 1, 1.25}
-- The ink of the text on the panel that a tinted frame holds.
ScaleTint.ink = '#FF3A2A1E'
-- Each tint with the cream of the panel its frame holds, as see-through as the tint.
ScaleTint.tints = {
    {id = 'white', text = 'None', color = '#FFFFFFFF', panel = '#FFF8F0DC'},
    {id = 'gold', text = 'Gold', color = '#FFFFC93C', panel = '#FFF8F0DC'},
    {id = 'sea', text = 'Sea', color = '#FF2FB3C4', panel = '#FFF8F0DC'},
    {id = 'leaf', text = 'Leaf', color = '#FF7BC74D', panel = '#FFF8F0DC'},
    {id = 'ghost', text = 'Ghost', color = '#70FFFFFF', panel = '#70F8F0DC'},
}
ScaleTint.code = [[
local woodTexture = assets.texture('nine-slice/parchment.png', {filter = 'linear'})
local whiteTexture = assets.texture('nine-slice/frame-white.png', {filter = 'linear'})
local wood = graphics2d.newNineSlice(woodTexture, {borders = {50, 50, 50, 50}})
local white = graphics2d.newNineSlice(whiteTexture, {borders = {46, 46, 46, 46}})
graphics2d.drawNineSlice(wood, {x, y, 300, 180}, '#FFFFFFFF', {borderScale = 0.5})  -- Borders at half their size.
graphics2d.drawNineSlice(white, {x, y, 300, 180}, '#FF2FB3C4', {borderScale = 2})  -- Tinted, borders twice as large.]]

function ScaleTint:enter()
    self.wood = graphics2d.newNineSlice(assets.texture('nine-slice/parchment.png', {filter = 'linear'}), {borders = {50, 50, 50, 50}})
    self.white = graphics2d.newNineSlice(assets.texture('nine-slice/frame-white.png', {filter = 'linear'}), {borders = {46, 46, 46, 46}})
    self.scale, self.tint = 1, ScaleTint.tints[3]
    local items = {}
    for index, tint in ipairs(ScaleTint.tints) do
        items[index] = {id = tint.id, text = tint.text}
    end
    self:frame{
        code = ScaleTint.code,
        hint = 'Borders that do not fit the rectangle shrink to fit it.',
        controls = {
            ui.formField{label = 'Border scale', ui.slider{id = 'scale', min = 0.25, max = 3, value = 1, step = 0.05, showValue = true, onChange = function(event) self.scale = event.value end}},
            ui.formField{label = 'Tint of the large frame', ui.segmentedControl{id = 'tint', items = items, selected = 'sea', onChange = function(event)
                for _, tint in ipairs(ScaleTint.tints) do
                    if tint.id == event.value then
                        self.tint = tint
                    end
                end
            end}},
        },
        focus = 'scale',
    }
end

function ScaleTint:update(dt)
    ScaleTint.super.update(self, dt)
    self:status(string.format('The large frame draws its borders at %.2f times their size, tinted "%s".', self.scale, self.tint.text))
end

-- Draws a tinted frame around a cream panel, so the tint reads as the color of the frame, and returns the rectangle of the panel.
function ScaleTint:drawTinted(rect, tint, scale)
    graphics2d.drawNineSlice(self.white, rect, tint.color, {borderScale = scale})
    local inset = math.min(46 * scale, rect[3] / 2, rect[4] / 2) - 4
    local panel = {rect[1] + inset, rect[2] + inset, rect[3] - inset * 2, rect[4] - inset * 2}
    if panel[3] > 0 and panel[4] > 0 then
        graphics2d.drawShape(panel, {radius = 8, color = tint.panel, layer = 1})
    end
    return panel
end

-- The wooden frames at four border scales fill the left half in a grid of two by two. The large frame on the right takes the scale and the tint of the panel, over the four tints, each frame holding its name.
function ScaleTint:draw(area)
    local margin, gap = 30, 24
    local half = (area.width - margin * 2 - gap) / 2
    local cellWidth, cellHeight = (half - gap) / 2, (area.height - margin * 2 - gap) / 2
    local frameHeight = cellHeight - 44
    for index, scale in ipairs(ScaleTint.scales) do
        local x, y = margin + (index - 1) % 2 * (cellWidth + gap), margin + (index - 1) // 2 * (cellHeight + gap)
        graphics2d.drawNineSlice(self.wood, {x, y, cellWidth, frameHeight}, nil, {borderScale = scale})
        Test.caption('Border scale ' .. scale, x + cellWidth / 2, y + frameHeight + 10, {size = 22, color = Test.ink, anchor = {0.5, 0}})
    end

    local left = margin + half + gap
    local tintHeight = math.min(110, area.height * 0.24)
    local large = {left, margin, half, area.height - margin * 2 - tintHeight - gap}
    local panel = self:drawTinted(large, self.tint, self.scale)
    if panel[3] > 120 and panel[4] > 60 then
        graphics2d.drawText(nil, self.tint.text, panel[1] + panel[3] / 2, panel[2] + panel[4] / 2 - 18, {size = 34, color = ScaleTint.ink, anchor = {0.5, 0.5}, layer = 2})
        graphics2d.drawText(nil, string.format('Borders at %.2f times their size', self.scale), panel[1] + panel[3] / 2, panel[2] + panel[4] / 2 + 22, {size = 20, color = ScaleTint.ink, anchor = {0.5, 0.5}, maxWidth = panel[3] - 20, align = 'center', layer = 2})
    end

    local tints = #ScaleTint.tints - 1
    local tintWidth = (half - gap * (tints - 1)) / tints
    local tintTop = area.height - margin - tintHeight
    for index = 2, #ScaleTint.tints do
        local tint = ScaleTint.tints[index]
        local x = left + (index - 2) * (tintWidth + gap)
        local inner = self:drawTinted({x, tintTop, tintWidth, tintHeight}, tint, 0.6)
        graphics2d.drawText(nil, tint.text, inner[1] + inner[3] / 2, inner[2] + inner[4] / 2, {size = 22, color = ScaleTint.ink, anchor = {0.5, 0.5}, layer = 2})
    end
end

return ScaleTint
