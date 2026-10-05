-- TrueType and OpenType sizes: both kinds render from a signed distance field baked once, so every size stays sharp, and a zoomed word shows it from 8 to 320 units.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Sizes = haylen.class('Sizes', TextTest)

Sizes.sizes = {12, 16, 20, 24, 32, 48, 64, 96}
Sizes.line = 'Black sphinx'
Sizes.word = 'Aa'

function Sizes:init(entry)
    Sizes.super.init(self, entry)
    self.zoom = 120
    self.pulse = true
    self.serif = fonts.load('crimson')
    self.sans = fonts.load('fira')
end

function Sizes:enter()
    self:frame{
        hint = 'Drag the zoom slider, or focus it and press left and right. With pulse on, the word breathes between 8 and 320 units, and no size needs an atlas of its own.',
        controls = {
            ui.formField{label = 'Zoom', ui.slider{id = 'zoom', min = 8, max = 320, value = self.zoom, step = 1, showValue = true, decimals = 0, onChange = function(event)
                self.zoom = event.value
                self.pulse = false
                self:set('pulse', {checked = false})
            end}},
            ui.toggle{id = 'pulse', text = 'Pulse', checked = self.pulse, onChange = function(event) self.pulse = event.checked end},
        },
        focus = 'zoom',
    }
end

function Sizes:update(dt)
    Sizes.super.update(self, dt)
    if self.pulse then
        self.zoom = 164 + 156 * math.sin(haylen.elapsed() * 0.8)
    end
    local serif, sans = self.serif, self.sans
    self:status(string.format('TrueType bake size %d, atlas pages %d   OpenType bake size %d, atlas pages %d   Zoom %.0f', serif.nativeSize, serif.pageCount, sans.nativeSize, sans.pageCount, self.zoom))
end

-- Draws one column of lines in a font, each after its size, and returns where the column ends.
function Sizes.column(font, title, x, y)
    Test.caption(title, x, y, {size = 26, color = Test.accent})
    y = y + 44
    for _, size in ipairs(Sizes.sizes) do
        Test.caption(tostring(size), x, y + font:ascent(size) - 18)
        graphics2d.drawText(font, Sizes.line, x + 56, y, {size = size})
        y = y + font:lineHeight(size) + 6
    end
    return y
end

function Sizes:draw(area)
    local layout = self.layout
    local half = layout.width / 2
    graphics2d.pushClip(layout)
    local bottom = Sizes.column(self.serif, 'Crimson Text, TrueType (".ttf")', 0, 0)
    Sizes.column(self.sans, 'Fira Sans, OpenType with CFF outlines (".otf")', half, 0)
    local y = bottom + 20
    graphics2d.drawLine(0, y, layout.width, y, 2, Test.line)
    graphics2d.drawText(self.serif, Sizes.word, half / 2, y + 20, {size = self.zoom, anchor = {0.5, 0}, color = '#FFFFE070'})
    graphics2d.drawText(self.sans, Sizes.word, half * 1.5, y + 20, {size = self.zoom, anchor = {0.5, 0}, color = '#FF7FCBF2'})
    graphics2d.popClip()
end

return Sizes
