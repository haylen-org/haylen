-- Wrapping: lines wrap after spaces at the maximum width, between any two Chinese or Japanese characters, which need no spaces, and inside a word only when the word alone is wider than a line. Line spacing sets the distance between lines.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Wrapping = haylen.class('Wrapping', TextTest)

Wrapping.english = 'On the third night the keeper climbed the lighthouse stairs, lit the great lamp and watched the ships find the harbor.\nA new paragraph starts after a line break.'
Wrapping.japanese = '灯台守は三日目の夜に階段を上り、大きなランプに火をともして、船が港へ帰るのを見守った。'
Wrapping.long = 'Pneumonoultramicroscopicsilicovolcanoconiosis is a word that breaks only because it must.'

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
        hint = 'Drag the sliders, or focus them and press left and right. The dashed line is the maximum width, and with sweep on it moves by itself.',
        controls = {
            ui.formField{label = 'Maximum width', ui.slider{id = 'width', min = 160, max = 640, step = 10, value = self.width, showValue = true, decimals = 0, onChange = function(event)
                self.width = event.value
                self.sweep = false
                self:set('sweep', {checked = false})
            end}},
            ui.formField{label = 'Line spacing', ui.slider{id = 'spacing', min = 0.8, max = 2, step = 0.1, value = self.spacing, showValue = true, decimals = 1, onChange = function(event) self.spacing = event.value end}},
            ui.toggle{id = 'sweep', text = 'Sweep the width', checked = self.sweep, onChange = function(event) self.sweep = event.checked end},
        },
        focus = 'width',
    }
end

function Wrapping:update(dt)
    Wrapping.super.update(self, dt)
    if self.sweep then
        self.width = 400 + 240 * math.sin(haylen.elapsed() * 0.7)
    end
    local style = {size = 34, maxWidth = self.width, lineSpacing = self.spacing}
    local english = self.latin:layout(Wrapping.english, style).lineCount
    local japanese = self.cjk:layout(Wrapping.japanese, style).lineCount
    self:status(string.format('Width %.0f   Line spacing %.1f   English lines %d   Japanese lines %d', self.width, self.spacing, english, japanese))
end

function Wrapping.guide(x, top, bottom)
    for y = top, bottom, 24 do
        graphics2d.drawLine(x, y, x, math.min(y + 12, bottom), 2, '#FFFF6A6A', {layer = 1})
    end
end

function Wrapping:draw(area)
    local layout = self.layout
    local left, right = 0, layout.width / 2 + 10
    local style = {size = 34, maxWidth = self.width, lineSpacing = self.spacing}
    Test.caption('English wraps after spaces', left, 0, {size = 22})
    graphics2d.drawText(self.latin, Wrapping.english, left, 34, style)
    Wrapping.guide(left + self.width, 30, 520)
    Test.caption('Japanese breaks between characters', right, 0, {size = 22})
    graphics2d.drawText(self.cjk, Wrapping.japanese, right, 34, style)
    Wrapping.guide(right + self.width, 30, 520)

    local y = 560
    local width = math.min(self.width, 300)
    Test.caption('A word wider than the line breaks inside itself', left, y, {size = 22})
    graphics2d.drawText(self.latin, Wrapping.long, left, y + 34, {size = 34, maxWidth = width, lineSpacing = self.spacing})
    Wrapping.guide(left + width, y + 30, layout.height)
end

return Wrapping
