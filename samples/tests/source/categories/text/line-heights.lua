-- Line heights: each line of natural text takes its own height, the ascent and descent of its fonts grown by the ink of glyphs that reach past them, such as the stacked marks of Burmese, Khmer, Tibetan, Thai and Devanagari, the double accents of Vietnamese capitals and emoji, while every line of uniform text takes the tallest ascent and descent of all its lines, so its baselines keep one step. The bands behind the text show the box of every line and its baseline.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local LineHeights = haylen.class('LineHeights', TextTest)

LineHeights.code = [[
local family = graphics.newFontFamily({regular = fira, fallbacks = {myanmar, khmer, tibetan, thai, devanagari, emoji}})
local natural = graphics2d.newRichText(lines, {family = family, size = 24, lineSpacing = 1.2})
local uniform = graphics2d.newRichText(lines, {family = family, size = 24, lineSpacing = 1.2, lineHeight = 'uniform'})
for _, line in ipairs(natural:frame().lines) do graphics2d.drawRect(line.rect, band) end
graphics2d.drawText(family, rows, x, y, {size = 24, lineHeight = 'uniform'})]]

LineHeights.lines = {
    {'Vietnamese', 'ẪỄỖ Tiếng Việt'},
    {'Burmese', 'မြန်မာစာ ကြွေ့ ပ္ပိုင်'},
    {'Khmer', 'ខ្ញុំស្រឡាញ់ ក្ស៊ី ង្គ្រៀ'},
    {'Tibetan', 'བསྒྲུབས་ སྐྱོ་ ཧཱུྃ'},
    {'Thai', 'ที่นี่ ปี่ ฐู้'},
    {'Hindi', 'र्द्ध्र्य क्षत्रिय हृदय'},
    {'Sizes', '[size=40]Big[/size] and small 🎉🚀'},
}
LineHeights.size = 24
LineHeights.labelWidth = 150
LineHeights.padding = 16
LineHeights.caption = 40
LineHeights.columnGap = 48
LineHeights.bandColors = {'#FF2A3350', '#FF323C5E'}
LineHeights.baselineColor = '#A0F2B23A'

function LineHeights:init(entry)
    LineHeights.super.init(self, entry)
    self.spacing = 1.2
    self.bands = true
    self.family = graphics.newFontFamily({
        regular = fonts.load('fira'),
        fallbacks = {fonts.load('myanmar'), fonts.load('khmer'), fonts.load('tibetan'), fonts.load('thai'), fonts.load('devanagari'), fonts.load('emoji')},
    })
    local rows = {}
    for index, line in ipairs(LineHeights.lines) do
        rows[index] = line[2]
    end
    local markup = table.concat(rows, '\n')
    self.columns = {
        {caption = 'Natural: each line takes its own height', text = graphics2d.newRichText(markup, {family = self.family, size = LineHeights.size, lineSpacing = self.spacing, color = Test.ink})},
        {caption = 'Uniform: every line takes the tallest height', text = graphics2d.newRichText(markup, {family = self.family, size = LineHeights.size, lineSpacing = self.spacing, lineHeight = 'uniform', color = Test.ink})},
    }
    self:measure()
end

function LineHeights:enter()
    self:frame{
        code = LineHeights.code,
        page = {1500, 720},
        hint = 'Drag the line spacing slider to spread the lines apart, and turn the bands off to see the text alone. Each band is the box of one line, and its gold line is the baseline.',
        controls = {
            ui.formField{label = 'Line spacing', ui.slider{id = 'spacing', min = 1, max = 1.4, step = 0.1, value = self.spacing, showValue = true, decimals = 1, onChange = function(event)
                self.spacing = event.value
                self:measure()
            end}},
            ui.toggle{id = 'bands', text = 'Show the line boxes', checked = self.bands, onChange = function(event) self.bands = event.checked end},
        },
        focus = 'spacing',
    }
end

-- Reads the lines of both texts whenever their spacing changes, so the frames draw from what this keeps.
function LineHeights:measure()
    for _, column in ipairs(self.columns) do
        column.text.lineSpacing = self.spacing
        local _, height = column.text:size()
        column.height = height
        column.lines = {}
        for index, line in ipairs(column.text:frame().lines) do
            column.lines[index] = {y = line.rect.y, height = line.rect.height, baseline = line.baseline}
        end
    end
    local shortest, tallest = math.huge, 0
    for _, line in ipairs(self.columns[1].lines) do
        shortest = math.min(shortest, line.height)
        tallest = math.max(tallest, line.height)
    end
    self.shortest = shortest
    self.tallest = tallest
    self.uniform = self.columns[2].lines[1].height
end

function LineHeights:update(dt)
    LineHeights.super.update(self, dt)
    self:status(string.format('Natural lines are %.0f to %.0f units tall, and every uniform line is %.0f.', self.shortest, self.tallest, self.uniform))
end

-- Draws one column: its caption, its box, a band for the box of every line with its baseline, the name of the script of each line and the text.
function LineHeights:column(column, x, width, height)
    local padding = LineHeights.padding
    local labelAscent = graphics2d.defaultFont():ascent(22)
    local top = LineHeights.caption + padding
    TextTest.label(column.caption, x, 0)
    TextTest.box({x, LineHeights.caption, width, height + padding * 2})
    for index, line in ipairs(column.lines) do
        local y = top + line.y
        if self.bands then
            graphics2d.drawRect({x + padding, y, width - padding * 2, line.height}, LineHeights.bandColors[index % 2 + 1], {layer = -5})
            graphics2d.drawLine(x + padding, top + line.baseline, x + width - padding, top + line.baseline, 1, LineHeights.baselineColor, {layer = -4})
        end
        TextTest.label(LineHeights.lines[index][1], x + padding * 2, top + line.baseline - labelAscent, {size = 22})
    end
    column.text:draw(x + padding * 2 + LineHeights.labelWidth, top)
end

function LineHeights:draw(area)
    local width = (self.layout.width - LineHeights.columnGap) / 2
    local height = math.max(self.columns[1].height, self.columns[2].height)
    self:column(self.columns[1], 0, width, height)
    self:column(self.columns[2], width + LineHeights.columnGap, width, height)
end

return LineHeights
