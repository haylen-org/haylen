-- Wrapping: lines wrap after spaces at the maximum width, between any two Chinese or Japanese characters, which need no spaces, and inside a word only when the word alone is wider than a line. Line spacing sets the distance between lines.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Wrapping = haylen.class('Wrapping', TextTest)

Wrapping.code = [[
graphics2d.drawText(latin, story, x, y, {size = 30, maxWidth = 520, lineSpacing = 1.2})
graphics2d.drawText(cjk, japanese, x, y, {size = 30, maxWidth = 520, lineSpacing = 1.2})
graphics2d.drawText(latin, longWord, x, y, {size = 30, maxWidth = 300})
local lines = latin:layout(story, {size = 30, maxWidth = 520}).lineCount]]

Wrapping.english = 'On the third night the keeper climbed the lighthouse stairs, lit the great lamp and watched the ships find the harbor.\nA new paragraph starts after a line break.'
Wrapping.japanese = '灯台守は三日目の夜に階段を上り、大きなランプに火をともして、船が港へ帰るのを見守った。'
Wrapping.long = 'Pneumonoultramicroscopicsilicovolcanoconiosis is a word that breaks only because it must.'
Wrapping.size = 30
Wrapping.maxWidth = 560
Wrapping.longWidth = 300
Wrapping.columnGap = 50
Wrapping.textTop = 64
Wrapping.padding = 16

function Wrapping:init(entry)
    Wrapping.super.init(self, entry)
    self.width = 520
    self.spacing = 1.2
    self.sweep = true
    self.latin = fonts.load('crimson')
    self.cjk = fonts.load('cjk')
end

function Wrapping:enter()
    self:frame{
        code = Wrapping.code,
        page = {Wrapping.maxWidth * 2 + Wrapping.longWidth + Wrapping.columnGap * 2 + Wrapping.padding * 2, 640},
        hint = 'Drag the sliders, or focus them and press left and right. The dashed lines mark the maximum width, and with sweep on it moves by itself.',
        controls = {
            ui.formField{label = 'Maximum width', ui.slider{id = 'width', min = 320, max = Wrapping.maxWidth, step = 10, value = self.width, showValue = true, decimals = 0, onChange = function(event)
                self.width = event.value
                self.sweep = false
                self:set('sweep', {checked = false})
            end}},
            ui.formField{label = 'Line spacing', ui.slider{id = 'spacing', min = 0.9, max = 1.5, step = 0.1, value = self.spacing, showValue = true, decimals = 1, onChange = function(event) self.spacing = event.value end}},
            ui.toggle{id = 'sweep', text = 'Sweep the width', checked = self.sweep, onChange = function(event) self.sweep = event.checked end},
        },
        focus = 'width',
    }
end

function Wrapping:update(dt)
    Wrapping.super.update(self, dt)
    if self.sweep then
        self.width = 440 + 120 * math.sin(haylen.elapsed() * 0.7)
    end
    local style = {size = Wrapping.size, maxWidth = self.width, lineSpacing = self.spacing}
    local english = self.latin:layout(Wrapping.english, style).lineCount
    local japanese = self.cjk:layout(Wrapping.japanese, style).lineCount
    self:status(string.format('At a width of %.0f and a line spacing of %.1f, the English text takes %d lines and the Japanese text %d.', self.width, self.spacing, english, japanese))
end

function Wrapping.guide(x, top, bottom)
    for y = top, bottom, 24 do
        graphics2d.drawLine(x, y, x, math.min(y + 12, bottom), 2, '#C0FF6A6A', {layer = 1})
    end
end

-- Draws one column: its caption, the box of the widest the column gets, its text and the dashed line at its maximum width, down to the bottom of the box.
function Wrapping:column(caption, font, text, x, width, widest)
    local layout, padding = self.layout, Wrapping.padding
    TextTest.label(caption, x, 0)
    TextTest.box({x - padding, Wrapping.textTop - padding, widest + padding * 2, layout.height - Wrapping.textTop + padding})
    graphics2d.drawText(font, text, x, Wrapping.textTop, {size = Wrapping.size, maxWidth = width, lineSpacing = self.spacing, color = Test.ink})
    Wrapping.guide(x + width, Wrapping.textTop - 6, layout.height - 6)
end

function Wrapping:draw(area)
    local padding = Wrapping.padding
    local columns = Wrapping.maxWidth * 2 + Wrapping.longWidth + Wrapping.columnGap * 2
    local gap = Wrapping.columnGap + (self.layout.width - columns - padding * 2) / 2
    local japaneseX = padding + Wrapping.maxWidth + gap
    local longX = japaneseX + Wrapping.maxWidth + gap
    self:column('English wraps after spaces', self.latin, Wrapping.english, padding, self.width, Wrapping.maxWidth)
    self:column('Japanese breaks between characters', self.cjk, Wrapping.japanese, japaneseX, self.width, Wrapping.maxWidth)
    self:column('A long word breaks inside', self.latin, Wrapping.long, longX, math.min(self.width, Wrapping.longWidth), Wrapping.longWidth)
end

return Wrapping
