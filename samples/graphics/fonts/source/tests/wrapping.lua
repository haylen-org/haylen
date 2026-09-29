-- Wrapping: lines wrap after spaces at the maximum width, between any two Chinese or Japanese characters, which need no spaces, and inside a word only when the word alone is wider than a line. Line spacing sets the distance between lines.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local fonts = require('fonts')
local sample = require('sample')

local Wrapping = haylen.class('Wrapping', sample.Test)

Wrapping.hints = 'Drag the sliders, or focus them and press left and right. The dashed line is the maximum width, and with sweep on it moves by itself.'
Wrapping.focus = 'width'

local kEnglish = 'On the third night the keeper climbed the lighthouse stairs, lit the great lamp and watched the ships find the harbor.\nA new paragraph starts after a line break.'
local kJapanese = '灯台守は三日目の夜に階段を上り、大きなランプに火をともして、船が港へ帰るのを見守った。'
local kLong = 'Pneumonoultramicroscopicsilicovolcanoconiosis is a word that breaks only because it must.'

function Wrapping:init(entry)
    Wrapping.super.init(self, entry)
    self.width = 520
    self.spacing = 1.2
    self.sweep = true
end

function Wrapping:controls()
    return {
        ui.label{text = 'Maximum width'},
        ui.slider{id = 'width', min = 160, max = 640, step = 10, value = self.width, showValue = true, decimals = 0, onChange = function(event)
            self.width = event.value
            self.sweep = false
            event.document:set('sweep', {checked = false})
        end},
        ui.label{text = 'Line spacing'},
        ui.slider{id = 'spacing', min = 0.8, max = 2, step = 0.1, value = self.spacing, showValue = true, decimals = 1, onChange = function(event) self.spacing = event.value end},
        ui.toggle{id = 'sweep', text = 'Sweep the width', checked = self.sweep, onChange = function(event) self.sweep = event.checked end},
    }
end

function Wrapping:update(dt)
    if self.sweep then
        self.width = 400 + 240 * math.sin(haylen.elapsed() * 0.7)
    end
    local style = {size = 34, maxWidth = self.width, lineSpacing = self.spacing}
    local english = fonts.get('crimson'):layout(kEnglish, style).lineCount
    local japanese = fonts.get('cjk'):layout(kJapanese, style).lineCount
    self:setStatus(string.format('width %.0f, line spacing %.1f, %d lines of English and %d of Japanese', self.width, self.spacing, english, japanese))
end

local function guide(x, top, bottom)
    for y = top, bottom, 24 do
        graphics2d.drawLine(x, y, x, math.min(y + 12, bottom), 2, '#FFFF6A6A')
    end
end

function Wrapping:render()
    local stage = self:stage()
    if stage == nil then
        return
    end
    local left, right = stage.x, stage.x + stage.width / 2 + 10
    local style = {size = 34, maxWidth = self.width, lineSpacing = self.spacing}
    graphics2d.beginScreen()
    sample.caption('English wraps after spaces', left, stage.y)
    graphics2d.drawText(fonts.get('crimson'), kEnglish, left, stage.y + 34, style)
    guide(left + self.width, stage.y + 30, stage.y + 520)
    sample.caption('Japanese breaks between characters', right, stage.y)
    graphics2d.drawText(fonts.get('cjk'), kJapanese, right, stage.y + 34, style)
    guide(right + self.width, stage.y + 30, stage.y + 520)

    local y = stage.y + 540
    sample.caption('a word wider than the line breaks inside itself', left, y)
    graphics2d.drawText(fonts.get('crimson'), kLong, left, y + 34, {size = 34, maxWidth = math.min(self.width, 300), lineSpacing = self.spacing})
    guide(left + math.min(self.width, 300), y + 30, stage:bottom())
end

return Wrapping
