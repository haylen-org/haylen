-- TrueType and OpenType sizes: both kinds render from a signed distance field baked once, so every size stays sharp, and a zoomed word shows it from 8 to 320 units.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Sizes = haylen.class('Sizes', TextTest)

Sizes.code = [[
local serif = assets.font('fonts/crimson-text-regular.ttf')
graphics2d.drawText(serif, 'Black sphinx', 64, y, {size = 12})
y = y + serif:lineHeight(12) + 6
local baseline = top + (height + serif:capHeight(320)) / 2  -- Centers the capitals.
graphics2d.drawText(serif, 'Aa', x, baseline - serif:ascent(320), {size = 320, anchor = {0.5, 0}})]]

Sizes.sizes = {12, 16, 20, 24, 32, 48, 64, 96}
Sizes.line = 'Black sphinx'
Sizes.word = 'Aa'
Sizes.numberSize = 24
Sizes.numberWidth = 44
Sizes.titleHeight = 52
Sizes.maxZoom = 320
-- The widths of the two columns of lines, each wide enough for its font at 96, and of the panel of the zoomed words.
Sizes.columns = {560, 620, 420}

function Sizes:init(entry)
    Sizes.super.init(self, entry)
    self.zoomSize = 120
    self.pulse = true
    self.serif = fonts.load('crimson')
    self.sans = fonts.load('fira')
    self.labelFont = graphics2d.defaultFont()
end

function Sizes:enter()
    self:frame{
        code = Sizes.code,
        page = {1680, 560},
        hint = 'Drag the zoom slider, or focus it and press left and right. With pulse on, the word breathes between 8 and 320 units, and no size needs an atlas of its own.',
        controls = {
            ui.formField{label = 'Zoom', ui.slider{id = 'zoom', min = 8, max = 320, value = self.zoomSize, step = 1, showValue = true, decimals = 0, onChange = function(event)
                self.zoomSize = event.value
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
        self.zoomSize = 164 + 156 * math.sin(haylen.elapsed() * 0.8)
    end
    local serif, sans = self.serif, self.sans
    self:status(string.format('Crimson Text bakes its glyphs once at size %d into %s and Fira Sans at size %d into %s, and the word draws at size %d.', serif.nativeSize, Sizes.pages(serif.pageCount), sans.nativeSize, Sizes.pages(sans.pageCount), math.floor(self.zoomSize + 0.5)))
end

function Sizes.pages(count)
    return count == 1 and 'one atlas page' or string.format('%d atlas pages', count)
end

-- Draws one column of lines in a font, each with its size right-aligned on its baseline in front of it, spreading the room the page has past the lines between them.
function Sizes:column(font, title, x, height)
    TextTest.label(title, x, 0, {size = 28, color = Test.accent})
    local lines = 0
    for _, size in ipairs(Sizes.sizes) do
        lines = lines + font:lineHeight(size)
    end
    local gap = math.min(24, (height - Sizes.titleHeight - lines) / (#Sizes.sizes - 1))
    local labelFont, numberSize = self.labelFont, Sizes.numberSize
    local y = Sizes.titleHeight
    for _, size in ipairs(Sizes.sizes) do
        local baseline = y + font:ascent(size)
        TextTest.label(tostring(size), x + Sizes.numberWidth, baseline - labelFont:ascent(numberSize), {size = numberSize, anchor = {1, 0}})
        graphics2d.drawText(font, Sizes.line, x + Sizes.numberWidth + 20, y, {size = size, color = Test.ink})
        y = y + font:lineHeight(size) + gap
    end
end

-- The zoomed word of each font stands on a baseline that centers its capitals at the largest size in its half of the panel.
function Sizes:zoomed(font, x, top, height, color)
    local size = self.zoomSize
    local baseline = top + (height + font:capHeight(Sizes.maxZoom)) / 2
    graphics2d.drawText(font, Sizes.word, x, baseline - font:ascent(size), {size = size, anchor = {0.5, 0}, color = color})
end

function Sizes:draw(area)
    local layout = self.layout
    local widths = Sizes.columns
    local gap = (layout.width - widths[1] - widths[2] - widths[3]) / 2
    self:column(self.serif, 'Crimson Text, TrueType (".ttf")', 0, layout.height)
    self:column(self.sans, 'Fira Sans, OpenType (".otf")', widths[1] + gap, layout.height)

    local panelX = layout.width - widths[3]
    TextTest.box({panelX, 0, widths[3], layout.height})
    local half = layout.height / 2
    self:zoomed(self.serif, panelX + widths[3] / 2, 0, half, '#FFFFE070')
    self:zoomed(self.sans, panelX + widths[3] / 2, half, half, '#FF7FCBF2')
end

return Sizes
