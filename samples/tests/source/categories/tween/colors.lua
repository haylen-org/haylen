-- Colors in RGB and HSV: two swatches travel between the same two colors, one through RGB and one through hue, and their trails paint the way each takes over the last seconds.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Colors = haylen.class('Colors', TweenTest)

local kPairs = {
    {id = 'red-blue', text = 'Red to blue', from = '#FFFF3030', to = '#FF3040FF'},
    {id = 'yellow-purple', text = 'Yellow to purple', from = '#FFFFE040', to = '#FF9030FF'},
    {id = 'green-magenta', text = 'Green to magenta', from = '#FF30E060', to = '#FFFF30C0'},
}
local kTrail = 90
local kRowHeight = 170
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
        hint = 'Pick a pair of colors. The top trail blends red, green and blue, and the bottom one turns the hue around the color wheel.',
        code = kCode,
        controls = {ui.radioGroup{id = 'pair', items = items, selected = kPairs[1].id, onChange = function(event) self:play(event.value) end}},
        focus = 'pair',
    })
    self:play(kPairs[1].id)
end

function Colors:play(id)
    for _, pair in ipairs(kPairs) do
        if pair.id == id then
            self.pair = pair
            tween.killTarget(self.rgb)
            tween.killTarget(self.hsv)
            self.rgb.color, self.hsv.color = m.color(pair.from), m.color(pair.from)
            self.trails.rgb:clear()
            self.trails.hsv:clear()
            local options = {owner = self, ease = 'sineInOut', loopMode = 'yoyo', repeatCount = -1, repeatDelay = 0.4}
            self.handle = tween.to(self.rgb, 1.5, {color = pair.to}, options)
            options.colorSpace = 'hsv'
            tween.to(self.hsv, 1.5, {color = pair.to}, options)
        end
    end
end

function Colors:update(dt)
    Colors.super.update(self, dt)
    self.trails.rgb:push(m.color(self.rgb.color))
    self.trails.hsv:push(m.color(self.hsv.color))
    self:status(string.format('%s, %.0f%% of the way.', self.pair.text, self.handle.progress * 100))
end

-- A row: its name, the swatch of the color now, and the trail of the colors it took, from the oldest on the left to the newest on the right.
function Colors:drawRow(title, caption, color, trail, y, area)
    local left, right = 380, area.width - 60
    graphics2d.drawText(nil, title, 60, y + 46, {size = 34, color = Test.ink, anchor = {0, 1}})
    graphics2d.drawText(nil, caption, 60, y + 56, {size = 18, color = Test.muted, maxWidth = 150})
    graphics2d.drawShape({220, y, 120, 120}, {radius = 22, color = color, borderWidth = 4, borderColor = '#FF1E1B33'})

    local width = (right - left) / kTrail
    graphics2d.pushMask(function()
        graphics2d.drawShape({left, y + 20, right - left, 80}, {radius = 16})
    end)
    local start = kTrail - trail.size
    for index = 1, trail.size do
        graphics2d.drawRect({left + (start + index - 1) * width, y + 20, width + 1, 80}, trail:get(index))
    end
    graphics2d.popMask()
    graphics2d.drawShape({left - 2, y + 18, right - left + 4, 84}, {radius = 18, color = '#00000000', borderWidth = 4, borderColor = '#FF1E1B33', layer = 1})
    graphics2d.drawText(nil, 'Earlier', left, y + 112, {size = 17, color = Test.muted})
    graphics2d.drawText(nil, 'Now', right, y + 112, {size = 17, color = Test.muted, anchor = {1, 0}})
end

function Colors:draw(area)
    local top = (area.height - kRowHeight * 2 - 40) / 2
    self:drawRow('RGB', 'Mixes the channels', self.rgb.color, self.trails.rgb, top, area)
    self:drawRow('HSV', 'Turns the hue', self.hsv.color, self.trails.hsv, top + kRowHeight + 40, area)
end

return Colors
