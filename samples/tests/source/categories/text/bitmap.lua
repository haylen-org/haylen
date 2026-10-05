-- Bitmap fonts: Haylen Pixel as a ".fnt" bitmap font in the text format with white glyphs the text color tints, the same font in the binary format with gold glyphs that keep their colors, and LCD digits cut from an image of equal cells as a grid font.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local fonts = require('categories.text.fonts')

local Bitmap = haylen.class('Bitmap', Test)

Bitmap.line = 'PRESS START to play!'

function Bitmap:init(entry)
    Bitmap.super.init(self, entry)
    self.pixel = fonts.load('pixel')
    self.gold = fonts.load('pixelGold')
    self.lcd = fonts.load('lcd')
    self.pixelFamily = fonts.family('pixel')
end

function Bitmap:enter()
    self:frame{hint = 'Bitmap fonts draw their own images pixel for pixel at their native size and scale with nearest filtering at whole multiples of it. They have no distance field, so they take no outline or glow, and their shadow is the silhouette of their glyphs.'}
end

function Bitmap:update(dt)
    Bitmap.super.update(self, dt)
    self:status(string.format('Text format native size %d   Binary format native size %d   Grid font native size %d   Distance field "%s"', self.pixel.nativeSize, self.gold.nativeSize, self.lcd.nativeSize, self.pixel.distanceField))
end

function Bitmap:draw(area)
    local pixel, gold, lcd = self.pixel, self.gold, self.lcd
    local native = pixel.nativeSize

    local x, y = 24, 24
    Test.caption('The file "haylen_pixel.fnt", text format, at 1, 2, 3 and 4 times its native size', x, y, {size = 22, color = Test.accent})
    y = y + 34
    for scale = 1, 4 do
        graphics2d.drawText(pixel, Bitmap.line, x, y, {size = native * scale})
        y = y + native * scale * 1.4
    end
    graphics2d.drawText(pixel, 'Red', x, y, {size = native * 5, color = '#FFFF6A6A'})
    graphics2d.drawText(pixel, 'Green', x + 180, y, {size = native * 5, color = '#FF6FDCA0'})
    graphics2d.drawText(pixel, 'Blue', x + 420, y, {size = native * 5, color = '#FF7FCBF2'})
    graphics2d.drawText(pixel, 'Shadow', x + 620, y, {size = native * 5, shadowOffset = {5, 5}, shadowColor = '#FF4C7DFF'})

    x, y = area.width * 0.52, 24
    Test.caption('The file "haylen_pixel_gold.fnt", binary format, colors kept', x, y, {size = 22, color = Test.accent})
    graphics2d.drawText(gold, 'GAME OVER', x, y + 40, {size = native * 6})
    graphics2d.drawText(gold, 'High score', x, y + 130, {size = native * 4})
    graphics2d.drawText(gold, 'Tinted', x + 420, y + 130, {size = native * 4, color = '#FF80C0FF'})
    graphics2d.drawRichText('[b]Bold[/b] and [i]italic[/i] are synthesized', x, y + 210, {family = self.pixelFamily, size = native * 3})

    local panelY = area.height * 0.5
    Test.caption('The image "lcd_digits.png" as a grid font of 12 by 20 cells', x, panelY, {size = 22, color = Test.accent})
    graphics2d.drawRect({x, panelY + 36, 560, 270}, '#FF0B1A10')
    local time = haylen.elapsed()
    graphics2d.drawText(lcd, string.format('%02d:%04.1f', math.floor(time / 60), time % 60), x + 30, panelY + 56, {size = 60})
    graphics2d.drawText(lcd, string.format('-%07.1f', 1024 + time * 3.7), x + 30, panelY + 136, {size = 60})
    graphics2d.drawText(lcd, '88:88.8', x + 30, panelY + 226, {size = 40})
    Test.caption('Three times and twice the cell size', x + 300, panelY + 240, {size = 22})
end

return Bitmap
