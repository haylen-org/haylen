-- Scale and tint: the same frame with its borders scaled from half to twice their size and multiplied by colors, and a large frame that takes the scale and the tint of the panel.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local ScaleTint = haylen.class('ScaleTint', Test)

ScaleTint.scales = {0.5, 1, 1.5, 2}
ScaleTint.tints = {
    {id = 'white', text = 'None', color = '#FFFFFFFF'},
    {id = 'gold', text = 'Gold', color = '#FFFFD166'},
    {id = 'sea', text = 'Sea', color = '#FF7FCBF2'},
    {id = 'ghost', text = 'Ghost', color = '#80FFFFFF'},
}
ScaleTint.code = [[
graphics2d.drawNineSlice(frame, {x, y, 300, 180}, '#FFFFFFFF', nil, 0.5)  -- Borders at half their size.
graphics2d.drawNineSlice(frame, {x, y, 300, 180}, '#FF7FCBF2', {layer = 1}, 2)  -- Tinted, borders twice as large.]]

function ScaleTint:enter()
    self.slice = graphics2d.newNineSlice(assets.texture('nine-slice/panel.png', {filter = 'linear'}), {borders = {28, 28, 28, 28}})
    self.scale, self.tint = 1, ScaleTint.tints[1].color
    local items = {}
    for index, tint in ipairs(ScaleTint.tints) do
        items[index] = {id = tint.id, text = tint.text}
    end
    self:frame{
        code = ScaleTint.code,
        hint = 'Borders that do not fit the rectangle shrink to fit it.',
        controls = {
            ui.formField{label = 'Border scale', ui.slider{id = 'scale', min = 0.25, max = 3, value = 1, step = 0.05, showValue = true, onChange = function(event) self.scale = event.value end}},
            ui.formField{label = 'Tint', ui.segmentedControl{id = 'tint', items = items, selected = 'white', onChange = function(event)
                for _, tint in ipairs(ScaleTint.tints) do
                    if tint.id == event.value then
                        self.tint = tint.color
                    end
                end
            end}},
        },
        focus = 'scale',
    }
end

function ScaleTint:update(dt)
    ScaleTint.super.update(self, dt)
    self:status(string.format('Border scale %.2f   Tint "%s"', self.scale, self.tint))
end

function ScaleTint:draw(area)
    local width = (area.width - 50) / #ScaleTint.scales - 20
    for index, scale in ipairs(ScaleTint.scales) do
        local x = 30 + (index - 1) * (width + 20)
        graphics2d.drawNineSlice(self.slice, {x, 30, width, 150}, ScaleTint.tints[index].color, nil, scale)
        Test.caption('Scale ' .. scale .. ', ' .. ScaleTint.tints[index].text:lower(), x + width / 2, 206, {size = 22, anchor = {0.5, 0.5}})
    end
    graphics2d.drawNineSlice(self.slice, {30, 250, area.width - 60, area.height - 280}, self.tint, nil, self.scale)
end

return ScaleTint
