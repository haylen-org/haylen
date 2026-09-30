-- Scale and tint: the same frame with its borders scaled from half to twice their size and multiplied by colors, and a large frame that takes the scale and the tint of the panel.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local ScaleTint = haylen.class('ScaleTint', sample.Test)

local kScales = {0.5, 1, 1.5, 2}
local kTints = {
    {id = 'white', text = 'None', color = '#FFFFFFFF'},
    {id = 'gold', text = 'Gold', color = '#FFFFD166'},
    {id = 'sea', text = 'Sea', color = '#FF7FCBF2'},
    {id = 'ghost', text = 'Ghost', color = '#80FFFFFF'},
}
local kCode = [[
graphics2d.drawNineSlice(frame, {x, y, 300, 180}, '#FFFFFFFF', nil, 0.5)  -- Borders at half their size.
graphics2d.drawNineSlice(frame, {x, y, 300, 180}, '#FF7FCBF2', {layer = 1}, 2)  -- Tinted, borders twice as large.]]

function ScaleTint:enter()
    self.frameSlice = graphics2d.newNineSlice(sample.texture('frames/panel.png'), {borders = {28, 28, 28, 28}})
    self.scale, self.tint = 1, kTints[1].color
    local items = {}
    for index, tint in ipairs(kTints) do
        items[index] = {id = tint.id, text = tint.text}
    end
    self:frame({
        hint = 'Borders that do not fit the rectangle shrink to fit it.',
        code = kCode,
        controls = {
            ui.formField{label = 'Border scale', ui.slider{id = 'scale', min = 0.25, max = 3, value = 1, step = 0.05, showValue = true, onChange = function(event) self.scale = event.value end}},
            ui.formField{label = 'Tint', ui.segmentedControl{id = 'tint', items = items, selected = 'white', onChange = function(event)
                for _, tint in ipairs(kTints) do
                    if tint.id == event.value then
                        self.tint = tint.color
                    end
                end
            end}},
        },
        focus = 'scale',
    })
end

function ScaleTint:update(dt)
    ScaleTint.super.update(self, dt)
    self:status(string.format('border scale %.2f   tint %s', self.scale, self.tint))
end

function ScaleTint:draw(area)
    local width = (area.width - 50) / #kScales - 20
    for index, scale in ipairs(kScales) do
        local x = 30 + (index - 1) * (width + 20)
        graphics2d.drawNineSlice(self.frameSlice, {x, 30, width, 150}, kTints[index].color, nil, scale)
        graphics2d.drawText(nil, 'scale ' .. scale .. ', ' .. kTints[index].text:lower(), x + width / 2, 206, {size = 22, color = sample.muted, anchor = {0.5, 0.5}})
    end
    graphics2d.drawNineSlice(self.frameSlice, {30, 250, area.width - 60, area.height - 280}, self.tint, nil, self.scale)
end

return ScaleTint
