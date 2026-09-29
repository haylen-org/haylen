-- Measuring text: measureText sizes a block, font:layout returns the quad of every glyph, ascent and lineHeight place baselines, font:glyph and font:kerning give the metrics of single characters, and rich text measures and lays out its own blocks.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local fonts = require('fonts')
local sample = require('sample')

local Measuring = haylen.class('Measuring', sample.Test)

Measuring.hints = 'Move the pointer over the measured line to pick a glyph from the quads of font:layout. Blue boxes are measured sizes, red lines are baselines, green boxes are glyph quads and orange marks the advance.'

local kSize = 96
local kText = 'Voyage to Tiny Island'
local kRich = '[b]Quest[/b] complete: [color=gold]12 coins[/color] [img=images/coin.png height=34]\nand a [url=map]map[/url] of the [i]northern reef[/i].'

function Measuring:init(entry)
    Measuring.super.init(self, entry)
    self.rich = graphics2d.newRichText(kRich, {family = fonts.family('crimson'), size = 40, maxWidth = 640})
end

-- The origin of the measured line on the stage.
local function lineOrigin(stage)
    return stage.x + 20, stage.y + 10
end

-- Picks the glyph quad under the pointer, from the same layout the line draws with.
function Measuring:update(dt)
    local stage = self:stage()
    if stage == nil then
        return
    end
    local x, y = lineOrigin(stage)
    local mouseX, mouseY = input.mousePosition()
    self.picked = nil
    for index, quad in ipairs(fonts.get('crimson'):layout(kText, {size = kSize}).quads) do
        local left, top = x + quad.position.x, y + quad.position.y
        if mouseX >= left and mouseX < left + quad.size.x and mouseY >= top and mouseY < top + quad.size.y then
            self.picked = index
            self:setStatus(string.format('glyph %d: quad at %.1f, %.1f, %.1f x %.1f on page %d', index, quad.position.x, quad.position.y, quad.size.x, quad.size.y, quad.page))
        end
    end
    if self.picked == nil then
        self:setStatus('the pointer is over no glyph')
    end
end

-- Draws the line with its measured box, its baseline and the quad of every glyph.
function Measuring:measuredLine(font, x, y)
    local width, height = graphics2d.measureText(font, kText, {size = kSize})
    local layout = font:layout(kText, {size = kSize})
    graphics2d.drawRectOutline({x, y, width, height}, 3, '#FF4C7DFF')
    graphics2d.drawText(font, kText, x, y, {size = kSize})
    local baseline = y + font:ascent(kSize)
    graphics2d.drawLine(x - 20, baseline, x + width + 20, baseline, 2, '#FFFF6A6A')
    for index, quad in ipairs(layout.quads) do
        local picked = index == self.picked
        graphics2d.drawRectOutline({x + quad.position.x, y + quad.position.y, quad.size.x, quad.size.y}, picked and 4 or 1, picked and '#FFFFE070' or '#A03DBE7A')
    end
    sample.caption(string.format('measureText %.0f x %.0f, %d quads, %d line, ascent %.1f, line height %.1f', width, height, #layout.quads, layout.lineCount, font:ascent(kSize), font:lineHeight(kSize)), x, y + height + 12)
end

-- Draws one character large with its source quad, its offset from the pen on the baseline and its advance.
function Measuring:glyphMetrics(font, character, x, baseline)
    local glyph = font:glyph(character)
    local scale = 220 / font.nativeSize
    graphics2d.drawText(font, character, x, baseline - font:ascent(220), {size = 220})
    local left, top = x + glyph.offset.x * scale, baseline + glyph.offset.y * scale
    graphics2d.drawRectOutline({left, top, glyph.source.width * scale, glyph.source.height * scale}, 2, '#FF3DBE7A')
    graphics2d.drawLine(x - 30, baseline, x + glyph.advance * scale + 30, baseline, 2, '#FFFF6A6A')
    graphics2d.drawCircle(x, baseline, 6, '#FFFFFFFF')
    graphics2d.drawLine(x, baseline + 24, x + glyph.advance * scale, baseline + 24, 4, '#FFF2B23A')
    sample.caption(string.format("'%s' at size %d: offset %.1f, %.1f", character, font.nativeSize, glyph.offset.x, glyph.offset.y), x - 30, baseline + 44)
    sample.caption(string.format("size %.0f x %.0f, advance %.1f", glyph.source.width, glyph.source.height, glyph.advance), x - 30, baseline + 72)
end

function Measuring:render()
    local stage = self:stage()
    if stage == nil then
        return
    end
    local font = fonts.get('crimson')
    graphics2d.beginScreen()
    self:measuredLine(font, lineOrigin(stage))

    local baseline = stage.y + 440
    self:glyphMetrics(font, 'g', stage.x + 60, baseline)
    self:glyphMetrics(font, 'A', stage.x + 480, baseline)

    local x = stage.x + 1000
    sample.caption(string.format("font:kerning('A', 'V') = %.2f and ('T', 'o') = %.2f at size %d", font:kerning('A', 'V'), font:kerning('T', 'o'), font.nativeSize), x, stage.y + 170)
    graphics2d.drawText(font, 'AV To', x, stage.y + 200, {size = 110})

    local richX, richY = x, stage.y + 420
    local width, height = graphics2d.measureRichText(kRich, {family = fonts.family('crimson'), size = 40, maxWidth = 640})
    graphics2d.drawRectOutline({richX, richY, width, height}, 3, '#FF4C7DFF')
    self.rich:draw(richX, richY)
    local laid = self.rich:layout()
    for _, link in ipairs(laid.links) do
        graphics2d.drawRectOutline({richX + link.rect.x, richY + link.rect.y, link.rect.width, link.rect.height}, 2, '#FFF2B23A')
    end
    for _, image in ipairs(laid.images) do
        graphics2d.drawRectOutline({richX + image.rect.x, richY + image.rect.y, image.rect.width, image.rect.height}, 2, '#FF3DBE7A')
    end
    sample.caption(string.format('measureRichText %.0f x %.0f, %d glyphs, %d lines, %d link, %d image', width, height, #laid.glyphs, laid.lineCount, #laid.links, #laid.images), richX, richY + height + 12)
end

return Measuring
