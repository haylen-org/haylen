-- Scale and tint: the wooden frame with its borders scaled from half to twice their size, and a near white frame painted to take a tint, multiplied by colors, with a large frame that takes the scale and the tint of the panel.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local ScaleTint = haylen.class('ScaleTint', Test)

ScaleTint.scales = {0.5, 1, 1.5, 2}
ScaleTint.tints = {
    {id = 'white', text = 'None', color = '#FFFFFFFF'},
    {id = 'gold', text = 'Gold', color = '#FFFFC93C'},
    {id = 'sea', text = 'Sea', color = '#FF2FB3C4'},
    {id = 'leaf', text = 'Leaf', color = '#FF7BC74D'},
    {id = 'ghost', text = 'Ghost', color = '#70FFFFFF'},
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

function ScaleTint:draw(area)
    local margin, gap = 30, 24
    local width = (area.width - margin * 2 - gap * (#ScaleTint.scales - 1)) / #ScaleTint.scales
    local height = math.min(150, area.height * 0.24)
    for index, scale in ipairs(ScaleTint.scales) do
        local x = margin + (index - 1) * (width + gap)
        graphics2d.drawNineSlice(self.wood, {x, margin, width, height}, nil, {borderScale = scale})
        Test.caption('Border scale ' .. scale, x + width / 2, margin + height + 10, {size = 22, color = Test.ink, anchor = {0.5, 0}})
    end
    local tints = #ScaleTint.tints - 1
    local tintWidth = (area.width - margin * 2 - gap * (tints - 1)) / tints
    local tintTop = margin + height + 56
    for index = 2, #ScaleTint.tints do
        local tint = ScaleTint.tints[index]
        local x = margin + (index - 2) * (tintWidth + gap)
        graphics2d.drawNineSlice(self.white, {x, tintTop, tintWidth, height * 0.8}, tint.color, {borderScale = 0.6})
        Test.caption(tint.text, x + tintWidth / 2, tintTop + height * 0.8 + 10, {size = 22, color = Test.ink, anchor = {0.5, 0}})
    end
    local top = tintTop + height * 0.8 + 56
    graphics2d.drawNineSlice(self.white, {margin, top, area.width - margin * 2, area.height - top - margin}, self.tint.color, {borderScale = self.scale})
end

return ScaleTint
