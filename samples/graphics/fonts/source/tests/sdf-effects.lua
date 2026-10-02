-- Outline, shadow and glow: the text shader draws them from the distance field of the font, which reaches as far as the spread the font was baked with. Lilita One bakes with a spread of 16 at 64 for thick effects.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local fonts = require('fonts')
local sample = require('sample')

local SdfEffects = haylen.class('SdfEffects', sample.Test)

SdfEffects.hints = 'Drag the sliders, or focus them and press left and right. An effect grows until it reaches the spread of the distance field, as the status line shows in distance units, where 0.5 spans the whole spread.'
SdfEffects.focus = 'outline'

local kSize = 112

function SdfEffects:init(entry)
    SdfEffects.super.init(self, entry)
    self.outline = 8
    self.blur = 8
    self.glow = 14
end

function SdfEffects:slider(id, label, max)
    return ui.column{gap = 4,
        ui.label{text = label},
        ui.slider{id = id, min = 0, max = max, step = 1, value = self[id], showValue = true, decimals = 0, onChange = function(event)
            self[id] = event.value
            self:refresh()
        end},
    }
end

function SdfEffects:controls()
    return {self:slider('outline', 'Outline width', 24), self:slider('blur', 'Shadow blur', 20), self:slider('glow', 'Glow reach', 28)}
end

-- The glow is a tag of rich text, so its markup follows the slider.
function SdfEffects:refresh()
    local family = fonts.family('lilita')
    self.glowText = graphics2d.newRichText(string.format('[glow=%d color=#FFFF8A00]Glow[/glow]', self.glow), {family = family, size = kSize, color = '#FFFFF3B0'})
    self.allText = graphics2d.newRichText(string.format('[outline=%d color=#FF3B1A00][shadow=8,10 color=#C0000000 blur=%d][glow=%d color=#FF40C4FF]Island[/glow][/shadow][/outline]', math.min(self.outline, 10), self.blur, self.glow), {family = family, size = kSize})
    local font = fonts.get('lilita')
    self:setStatus(string.format('Outline %.3f, blur %.3f, glow %.3f in distance units at size %d', font:toDistance(self.outline, kSize), font:toDistance(self.blur, kSize), font:toDistance(self.glow, kSize), kSize))
end

function SdfEffects:started()
    self:refresh()
end

function SdfEffects:update(dt)
    self.glowText:update(dt)
    self.allText:update(dt)
end

function SdfEffects:render()
    local stage = self:stage()
    if stage == nil then
        return
    end
    local font = fonts.get('lilita')
    local left, right = stage.x + 40, stage.x + stage.width / 2 + 20
    local top = stage.y + 20
    graphics2d.beginScreen()
    graphics2d.drawRect({stage.x, stage.y, stage.width, stage.height}, '#FF1A2030')
    for index = 0, 12 do
        graphics2d.drawLine(stage.x + index * 120, stage:bottom(), stage.x + index * 120 + 300, stage.y, 30, '#FF222A3E')
    end

    sample.caption(string.format('Option "outlineWidth" at %g', self.outline), left, top)
    graphics2d.drawText(font, 'Outline', left, top + 30, {size = kSize, color = '#FFFFD166', outlineWidth = self.outline, outlineColor = '#FF8A1E1E'})
    sample.caption(string.format('Options "shadowOffset" at 10, 12 and "shadowBlur" at %g', self.blur), right, top)
    graphics2d.drawText(font, 'Shadow', right, top + 30, {size = kSize, color = '#FFE8EAF2', shadowOffset = {10, 12}, shadowColor = '#E0000000', shadowBlur = self.blur})

    local middle = top + 30 + kSize * 1.5
    sample.caption(string.format('The tag "[glow=%g]" in rich text', self.glow), left, middle)
    self.glowText:draw(left, middle + 30)
    sample.caption('All three, with the outline under 10', right, middle)
    self.allText:draw(right, middle + 30)

    local bottom = middle + 30 + kSize * 1.5
    sample.caption('A thin outline keeps small text readable over a busy picture', left, bottom)
    graphics2d.drawText(font, 'Wood 12   Stone 4   Night 3', left, bottom + 30, {size = 40, outlineWidth = 3, outlineColor = '#FF000000'})
    graphics2d.drawText(font, 'Wood 12   Stone 4   Night 3', right, bottom + 30, {size = 40})
end

return SdfEffects
