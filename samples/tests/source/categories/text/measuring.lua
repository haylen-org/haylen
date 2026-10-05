-- Measuring text: `measureText` sizes a block, `font:layout` returns the quad of every glyph, `ascent` and `lineHeight` place baselines, `font:glyph` gives the metrics of single characters, `font:shape` the advances kerning shortens, and rich text measures and lays out its own blocks.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Measuring = haylen.class('Measuring', TextTest)

Measuring.size = 96
Measuring.text = 'Voyage to Tiny Island'
Measuring.rich = '[b]Quest[/b] complete: [color=gold]12 coins[/color] [img=text/coin.png height=34]\nand a [url=map]map[/url] of the [i]northern reef[/i].'
Measuring.lineX, Measuring.lineY = 20, 10
Measuring.richX, Measuring.richY = 1000, 420

function Measuring:init(entry)
    Measuring.super.init(self, entry)
    self.font = fonts.load('crimson')
    self.family = fonts.family('crimson')
    self.richText = graphics2d.newRichText(Measuring.rich, {family = self.family, size = 40, maxWidth = 640})
    self.layoutOfLine = self.font:layout(Measuring.text, {size = Measuring.size})
    self.kerning = {self:kerned('AV'), self:kerned('To')}
end

function Measuring:enter()
    self:frame{hint = 'Move the pointer over the measured line to pick a glyph from the quads of "font:layout". Blue boxes are measured sizes, red lines are baselines, green boxes are glyph quads and orange marks the advance.'}
end

-- Returns how much kerning shortens the advance of the first letter of a pair.
function Measuring:kerned(pair)
    return self.font:shape(pair)[1].advance - self.font:glyph(pair:sub(1, 1)).advance
end

-- Picks the glyph quad under the pointer, from the same layout the line draws with.
function Measuring:update(dt)
    Measuring.super.update(self, dt)
    if not self.stage then
        return
    end
    local x, y = self:toStage(input.mousePosition())
    x, y = x - Measuring.lineX, y - Measuring.lineY
    self.picked = nil
    for index, quad in ipairs(self.layoutOfLine.quads) do
        if x >= quad.position.x and x < quad.position.x + quad.size.x and y >= quad.position.y and y < quad.position.y + quad.size.y then
            self.picked = index
            self:status(string.format('Glyph %d: quad at %.1f, %.1f, %.1f x %.1f on page %d', index, quad.position.x, quad.position.y, quad.size.x, quad.size.y, quad.page))
        end
    end
    if self.picked == nil then
        self:status('The pointer is over no glyph')
    end
end

-- Draws the line with its measured box, its baseline and the quad of every glyph.
function Measuring:measuredLine(x, y)
    local font, size, layout = self.font, Measuring.size, self.layoutOfLine
    local width, height = graphics2d.measureText(font, Measuring.text, {size = size})
    graphics2d.drawRectOutline({x, y, width, height}, 3, '#FF4C7DFF')
    graphics2d.drawText(font, Measuring.text, x, y, {size = size})
    local baseline = y + font:ascent(size)
    graphics2d.drawLine(x - 20, baseline, x + width + 20, baseline, 2, '#FFFF6A6A')
    for index, quad in ipairs(layout.quads) do
        local picked = index == self.picked
        graphics2d.drawRectOutline({x + quad.position.x, y + quad.position.y, quad.size.x, quad.size.y}, picked and 4 or 1, picked and '#FFFFE070' or '#A03DBE7A', {layer = 1})
    end
    Test.caption(string.format('Size from "measureText" %.0f x %.0f, %d quads, %d line, ascent %.1f, line height %.1f', width, height, #layout.quads, layout.lineCount, font:ascent(size), font:lineHeight(size)), x, y + height + 12, {size = 22})
end

-- Draws one character large with its source quad, its offset from the pen on the baseline and its advance.
function Measuring:glyphMetrics(character, x, baseline)
    local font = self.font
    local glyph = font:glyph(character)
    local scale = 220 / font.nativeSize
    graphics2d.drawText(font, character, x, baseline - font:ascent(220), {size = 220})
    local left, top = x + glyph.offset.x * scale, baseline + glyph.offset.y * scale
    graphics2d.drawRectOutline({left, top, glyph.source.width * scale, glyph.source.height * scale}, 2, '#FF3DBE7A', {layer = 1})
    graphics2d.drawLine(x - 30, baseline, x + glyph.advance * scale + 30, baseline, 2, '#FFFF6A6A', {layer = 1})
    graphics2d.drawCircle(x, baseline, 6, '#FFFFFFFF', {layer = 1})
    graphics2d.drawLine(x, baseline + 24, x + glyph.advance * scale, baseline + 24, 4, '#FFF2B23A')
    Test.caption(string.format('Glyph "%s" at size %d: offset %.1f, %.1f', character, font.nativeSize, glyph.offset.x, glyph.offset.y), x - 30, baseline + 104, {size = 22})
    Test.caption(string.format('Size %.0f x %.0f, advance %.1f', glyph.source.width, glyph.source.height, glyph.advance), x - 30, baseline + 132, {size = 22})
end

function Measuring:draw(area)
    self:measuredLine(Measuring.lineX, Measuring.lineY)
    self:glyphMetrics('g', 60, 440)
    self:glyphMetrics('A', 480, 440)

    local x = Measuring.richX
    Test.caption(string.format('The method "font:shape" kerns "AV" by %.2f and "To" by %.2f at size %d', self.kerning[1], self.kerning[2], self.font.nativeSize), x, 170, {size = 22})
    graphics2d.drawText(self.font, 'AV To', x, 200, {size = 110})

    local richY = Measuring.richY
    local width, height = graphics2d.measureRichText(Measuring.rich, {family = self.family, size = 40, maxWidth = 640})
    graphics2d.drawRectOutline({x, richY, width, height}, 3, '#FF4C7DFF')
    self.richText:draw(x, richY)
    local laid = self.richText:frame()
    for _, link in ipairs(laid.links) do
        graphics2d.drawRectOutline({x + link.rect.x, richY + link.rect.y, link.rect.width, link.rect.height}, 2, '#FFF2B23A', {layer = 1})
    end
    for _, image in ipairs(laid.images) do
        graphics2d.drawRectOutline({x + image.rect.x, richY + image.rect.y, image.rect.width, image.rect.height}, 2, '#FF3DBE7A', {layer = 1})
    end
    Test.caption(string.format('Size from "measureRichText" %.0f x %.0f, %d glyphs, %d lines, %d link, %d image', width, height, #laid.glyphs, laid.lineCount, #laid.links, #laid.images), x, richY + height + 12, {size = 22})
end

return Measuring
