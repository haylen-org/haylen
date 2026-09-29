-- Colors in RGB and HSV: two swatches travel between the same two colors, one through RGB and one through hue, and their trails paint the way each takes.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Colors = haylen.class('Colors', sample.Test)

local kPairs = {
    {id = 'red-blue', text = 'Red to blue', from = '#FFFF3030', to = '#FF3040FF'},
    {id = 'yellow-purple', text = 'Yellow to purple', from = '#FFFFE040', to = '#FF9030FF'},
    {id = 'green-magenta', text = 'Green to magenta', from = '#FF30E060', to = '#FFFF30C0'},
}
local kTrail = 90
local kCode = [[
tween.to(rgb, 1.5, {color = '#FF3040FF'}, {loopMode = 'yoyo', repeatCount = -1})
tween.to(hsv, 1.5, {color = '#FF3040FF'}, {loopMode = 'yoyo', repeatCount = -1, colorSpace = 'hsv'})]]

function Colors:enter()
    local items = {}
    for index, pair in ipairs(kPairs) do
        items[index] = {id = pair.id, text = pair.text}
    end
    self.rgb = {color = m.color(kPairs[1].from)}
    self.hsv = {color = m.color(kPairs[1].from)}
    self.trails = {rgb = collections.newRingBuffer(kTrail), hsv = collections.newRingBuffer(kTrail)}
    self:frame({
        hint = 'Pick a pair of colors. The top trail blends red, green and blue, the bottom one turns the hue around the color wheel.',
        code = kCode,
        controls = {ui.radioGroup{id = 'pair', items = items, selected = kPairs[1].id, onChange = function(event) self:play(event.value) end}},
        focus = 'pair',
    })
    self:play(kPairs[1].id)
end

function Colors:play(id)
    for _, pair in ipairs(kPairs) do
        if pair.id == id then
            tween.killTarget(self.rgb)
            tween.killTarget(self.hsv)
            self.rgb.color, self.hsv.color = m.color(pair.from), m.color(pair.from)
            self.trails.rgb:clear()
            self.trails.hsv:clear()
            local options = {owner = self, ease = 'sineInOut', loopMode = 'yoyo', repeatCount = -1, repeatDelay = 0.4}
            tween.to(self.rgb, 1.5, {color = pair.to}, options)
            options.colorSpace = 'hsv'
            tween.to(self.hsv, 1.5, {color = pair.to}, options)
        end
    end
end

function Colors:update(dt)
    Colors.super.update(self, dt)
    self.trails.rgb:push(m.color(self.rgb.color))
    self.trails.hsv:push(m.color(self.hsv.color))
    self:status('rgb ' .. self.rgb.color:toHex() .. '   hsv ' .. self.hsv.color:toHex())
end

function Colors:drawRow(label, color, trail, y, area)
    local width = (area.width - 420) / kTrail
    graphics2d.drawText(nil, label, 40, y + 90, {size = 30, color = sample.ink, anchor = {0, 0.5}})
    graphics2d.drawRect({160, y + 20, 140, 140}, color)
    for index, value in ipairs(trail:values()) do
        graphics2d.drawRect({340 + (index - 1) * width, y + 40, width + 1, 100}, value)
    end
end

function Colors:draw(area)
    local half = area.height / 2
    self:drawRow('RGB', self.rgb.color, self.trails.rgb, half / 2 - 90, area)
    self:drawRow('HSV', self.hsv.color, self.trails.hsv, half + half / 2 - 90, area)
end

return Colors
