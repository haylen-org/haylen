-- Outline, shadow and glow: the text shader draws them from the distance field of the font, which reaches as far as the spread the font was baked with. Lilita One bakes with a spread of 16 at 64 for thick effects.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local SdfEffects = haylen.class('SdfEffects', TextTest)

SdfEffects.size = 112

function SdfEffects:init(entry)
    SdfEffects.super.init(self, entry)
    self.outline = 8
    self.blur = 8
    self.glow = 14
    self.font = fonts.load('lilita')
    self.family = fonts.family('lilita')
    self:refresh()
end

function SdfEffects:slider(id, label, max)
    return ui.formField{label = label, ui.slider{id = id, min = 0, max = max, step = 1, value = self[id], showValue = true, decimals = 0, onChange = function(event)
        self[id] = event.value
        self:refresh()
    end}}
end

function SdfEffects:enter()
    self:frame{
        hint = 'Drag the sliders, or focus them and press left and right. An effect grows until it reaches the spread of the distance field, as the status line shows in distance units, where 0.5 spans the whole spread.',
        controls = {self:slider('outline', 'Outline width', 24), self:slider('blur', 'Shadow blur', 20), self:slider('glow', 'Glow reach', 28)},
        focus = 'outline',
    }
end

-- The glow is a tag of rich text, so its markup follows the slider.
function SdfEffects:refresh()
    local size = SdfEffects.size
    self.glowText = graphics2d.newRichText(string.format('[glow=%d color=#FFFF8A00]Glow[/glow]', self.glow), {family = self.family, size = size, color = '#FFFFF3B0'})
    self.allText = graphics2d.newRichText(string.format('[outline=%d color=#FF3B1A00][shadow=8,10 color=#C0000000 blur=%d][glow=%d color=#FF40C4FF]Island[/glow][/shadow][/outline]', math.min(self.outline, 10), self.blur, self.glow), {family = self.family, size = size})
end

function SdfEffects:update(dt)
    SdfEffects.super.update(self, dt)
    self.glowText:update(dt)
    self.allText:update(dt)
    local font, size = self.font, SdfEffects.size
    self:status(string.format('Outline %.3f   Blur %.3f   Glow %.3f in distance units at size %d', font:toDistance(self.outline, size), font:toDistance(self.blur, size), font:toDistance(self.glow, size), size))
end

function SdfEffects:draw(area)
    local layout, font, size = self.layout, self.font, SdfEffects.size
    local left, right = 40, layout.width / 2 + 20
    local top = 20
    graphics2d.pushClip(layout)
    graphics2d.drawRect(layout, '#FF1A2030')
    for index = 0, 12 do
        graphics2d.drawLine(index * 120, layout.height, index * 120 + 300, 0, 30, '#FF222A3E')
    end

    Test.caption(string.format('Option "outlineWidth" at %g', self.outline), left, top, {size = 22})
    graphics2d.drawText(font, 'Outline', left, top + 30, {size = size, color = '#FFFFD166', outlineWidth = self.outline, outlineColor = '#FF8A1E1E'})
    Test.caption(string.format('Options "shadowOffset" at 10, 12 and "shadowBlur" at %g', self.blur), right, top, {size = 22})
    graphics2d.drawText(font, 'Shadow', right, top + 30, {size = size, color = '#FFE8EAF2', shadowOffset = {10, 12}, shadowColor = '#E0000000', shadowBlur = self.blur})

    local middle = top + 30 + size * 1.5
    Test.caption(string.format('The tag "[glow=%g]" in rich text', self.glow), left, middle, {size = 22})
    self.glowText:draw(left, middle + 30)
    Test.caption('All three, with the outline under 10', right, middle, {size = 22})
    self.allText:draw(right, middle + 30)

    local bottom = middle + 30 + size * 1.5
    Test.caption('A thin outline keeps small text readable over a busy picture', left, bottom, {size = 22})
    graphics2d.drawText(font, 'Wood 12   Stone 4   Night 3', left, bottom + 30, {size = 40, outlineWidth = 3, outlineColor = '#FF000000'})
    graphics2d.drawText(font, 'Wood 12   Stone 4   Night 3', right, bottom + 30, {size = 40})
    graphics2d.popClip()
end

return SdfEffects
