-- Measuring text: `measureText` sizes a block, `font:layout` returns the quad of every glyph, `ascent` and `lineHeight` place baselines, `font:glyph` gives the metrics of single characters, `font:shape` the advances kerning shortens, and rich text measures its block and lays out its lines, links and images.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Measuring = haylen.class('Measuring', TextTest)

Measuring.code = [[
local width, height = graphics2d.measureText(font, 'Voyage to Tiny Island', {size = 96})
local layout = font:layout('Voyage to Tiny Island', {size = 96})  -- The quad of every glyph is in `layout.quads`.
local glyph = font:glyph('g')  -- It holds its `source`, `offset` and `advance`.
local kerning = font:shape('AV')[1].advance - font:glyph('A').advance
local blockWidth, blockHeight = graphics2d.measureRichText(markup, {family = family, size = 40, maxWidth = 640})]]

Measuring.size = 96
Measuring.text = 'Voyage to Tiny Island'
Measuring.rich = '[b]Quest[/b] complete: [color=gold]12 coins[/color] [img=text/coin.png height=34]\nand a [url=map]map[/url] of the [i]northern reef[/i].'
Measuring.richOptions = {size = 40, maxWidth = 640}
Measuring.glyphSize = 200
Measuring.kerningSize = 100
Measuring.rightWidth = 820
-- The captions of a large glyph start below the deepest descender at its size.
Measuring.captionGap = 96
Measuring.blue = '#FF4C7DFF'
Measuring.red = '#FFFF6A6A'
Measuring.green = '#FF3DBE7A'
Measuring.orange = '#FFF2B23A'

function Measuring:init(entry)
    Measuring.super.init(self, entry)
    self.font = fonts.load('crimson')
    self.family = fonts.family('crimson')
    self.richOptions = {family = self.family, size = Measuring.richOptions.size, maxWidth = Measuring.richOptions.maxWidth, color = Test.ink}
    self.richText = graphics2d.newRichText(Measuring.rich, self.richOptions)
    self.layoutOfLine = self.font:layout(Measuring.text, {size = Measuring.size})
    self.lineWidth, self.lineHeight = graphics2d.measureText(self.font, Measuring.text, {size = Measuring.size})
    self.kerning = {self:kerned('AV'), self:kerned('To')}
    self.lineX, self.lineY = 0, 0
end

-- The rich text keeps its block and the layout of its lines, which stay the same while the page draws it.
function Measuring:resize(area)
    Measuring.super.resize(self, area)
    self.richWidth, self.richHeight = graphics2d.measureRichText(Measuring.rich, self.richOptions)
    self.richFrame = self.richText:frame()
end

function Measuring:enter()
    self:frame{code = Measuring.code, hint = 'Move the pointer over the measured line to pick a glyph from the quads of "font:layout". Blue boxes are measured sizes, red lines are baselines, green boxes are glyph quads and orange bars are advances.'}
end

-- Returns how much kerning shortens the advance of the first letter of a pair.
function Measuring:kerned(pair)
    return self.font:shape(pair)[1].advance - self.font:glyph(pair:sub(1, 1)).advance
end

-- Picks the glyph quad under the pointer, from the same layout the line draws with.
function Measuring:update(dt)
    Measuring.super.update(self, dt)
    self.richText:update(dt)
    if not self.stage then
        return
    end
    local x, y = self:toStage(input.mousePosition())
    x, y = x - self.lineX, y - self.lineY
    self.picked = nil
    for index, quad in ipairs(self.layoutOfLine.quads) do
        if x >= quad.position.x and x < quad.position.x + quad.size.x and y >= quad.position.y and y < quad.position.y + quad.size.y then
            self.picked = index
            self:status(string.format('The pointer is over glyph %d, whose quad starts at %.1f, %.1f and measures %.1f by %.1f on atlas page %d.', index, quad.position.x, quad.position.y, quad.size.x, quad.size.y, quad.page))
        end
    end
    if self.picked == nil then
        self:status('The pointer is over no glyph of the measured line.')
    end
end

-- Draws the line with its measured box and its baseline. The quads of its glyphs stay faint, and the one under the pointer stands out.
function Measuring:measuredLine(x, y)
    local font, size, layout = self.font, Measuring.size, self.layoutOfLine
    local width, height = self.lineWidth, self.lineHeight
    graphics2d.drawRectOutline({x, y, width, height}, 3, Measuring.blue)
    graphics2d.drawText(font, Measuring.text, x, y, {size = size, color = Test.ink})
    local baseline = y + font:ascent(size)
    graphics2d.drawLine(x - 20, baseline, x + width + 20, baseline, 2, Measuring.red)
    for index, quad in ipairs(layout.quads) do
        local picked = index == self.picked
        graphics2d.drawRectOutline({x + quad.position.x, y + quad.position.y, quad.size.x, quad.size.y}, picked and 4 or 1, picked and '#FFFFE070' or '#503DBE7A', {layer = 1})
    end
    TextTest.label(string.format('Size from "measureText" %.0f by %.0f, %d quads on %d line, ascent %.1f, line height %.1f', width, height, #layout.quads, layout.lineCount, font:ascent(size), font:lineHeight(size)), x, y + height + 14, {size = 24, maxWidth = width})
end

-- Draws one character large with its source quad, its offset from the pen on the baseline and its advance.
function Measuring:glyphMetrics(character, x, baseline)
    local font, size = self.font, Measuring.glyphSize
    local glyph = font:glyph(character)
    local scale = size / font.nativeSize
    graphics2d.drawText(font, character, x, baseline - font:ascent(size), {size = size, color = Test.ink})
    local left, top = x + glyph.offset.x * scale, baseline + glyph.offset.y * scale
    graphics2d.drawRectOutline({left, top, glyph.source.width * scale, glyph.source.height * scale}, 2, Measuring.green, {layer = 1})
    graphics2d.drawLine(x - 30, baseline, x + glyph.advance * scale + 30, baseline, 2, Measuring.red, {layer = 1})
    graphics2d.drawCircle(x, baseline, 6, '#FFFFFFFF', {layer = 1})
    graphics2d.drawLine(x, baseline + 24, x + glyph.advance * scale, baseline + 24, 4, Measuring.orange)
    local captionY = baseline + Measuring.captionGap
    TextTest.label(string.format('Glyph "%s" at size %d', character, font.nativeSize), x - 30, captionY, {size = 24, color = Test.accent})
    TextTest.label(string.format('Offset %.1f, %.1f, size %.0f by %.0f', glyph.offset.x, glyph.offset.y, glyph.source.width, glyph.source.height), x - 30, captionY + 32, {size = 24})
    TextTest.label(string.format('Advance %.1f', glyph.advance), x - 30, captionY + 64, {size = 24})
end

-- Draws a pair with the kerning of its font and again with each letter at its own advance.
function Measuring:kerningPairs(x, y)
    local font, size = self.font, Measuring.kerningSize
    local scale = size / font.nativeSize
    TextTest.label(string.format('Kerning from "font:shape": "AV" by %.2f and "To" by %.2f at size %d', self.kerning[1], self.kerning[2], font.nativeSize), x, y, {size = 24})
    local column = Measuring.rightWidth / 2
    TextTest.label('With kerning', x, y + 40, {size = 24, color = Test.accent})
    TextTest.label('Without kerning', x + column, y + 40, {size = 24, color = Test.accent})
    local top = y + 72
    graphics2d.drawText(font, 'AV To', x, top, {size = size, color = Test.ink})
    local pen = x + column
    for character in ('AV To'):gmatch('.') do
        graphics2d.drawText(font, character, pen, top, {size = size, color = Test.ink})
        pen = pen + font:glyph(character).advance * scale
    end
end

-- Draws the rich text with the block "measureRichText" returns, the box of each line, and its link and image.
function Measuring:richBlock(x, y)
    local width, height, laid = self.richWidth, self.richHeight, self.richFrame
    graphics2d.drawRectOutline({x, y, width, height}, 3, Measuring.blue)
    for _, line in ipairs(laid.lines) do
        graphics2d.drawRect({x + line.rect.x, y + line.rect.y, line.rect.width, line.rect.height}, '#283DBE7A', {layer = -1})
    end
    self.richText:draw(x, y)
    for _, link in ipairs(laid.links) do
        graphics2d.drawRectOutline({x + link.rect.x, y + link.rect.y, link.rect.width, link.rect.height}, 2, Measuring.orange, {layer = 1})
    end
    for _, image in ipairs(laid.images) do
        graphics2d.drawRectOutline({x + image.rect.x, y + image.rect.y, image.rect.width, image.rect.height}, 2, Measuring.green, {layer = 1})
    end
    TextTest.label(string.format('The block from "measureRichText" is %.0f by %.0f, as wide as its maximum width, while the shaded boxes of its %d lines end with their text. It holds %d glyphs, %d link and %d image.', width, height, laid.lineCount, #laid.glyphs, #laid.links, #laid.images), x, y + height + 14, {size = 24, maxWidth = Measuring.rightWidth})
end

function Measuring:draw(area)
    local layout = self.layout
    local right = layout.width - Measuring.rightWidth
    self:measuredLine(self.lineX, self.lineY)
    local baseline = layout.height - Measuring.captionGap - 96
    self:glyphMetrics('g', 40, baseline)
    self:glyphMetrics('A', 480, baseline)
    self:kerningPairs(right, 0)
    self:richBlock(right, layout.height * 0.5)
end

return Measuring
