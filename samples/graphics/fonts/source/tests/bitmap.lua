-- Bitmap fonts: Haylen Pixel as a BMFont in the text format with white glyphs the text color tints, the same font in the binary format with gold glyphs that keep their colors, and LCD digits cut from an image of equal cells as a grid font.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local fonts = require('fonts')
local sample = require('sample')

local Bitmap = haylen.class('Bitmap', sample.Test)

Bitmap.hints = 'Bitmap fonts draw their own images pixel for pixel at their native size and scale with nearest filtering at whole multiples of it. They have no distance field, so they take no outline or glow, and their shadow is the silhouette of their glyphs.'

local kLine = 'PRESS START to play!'

function Bitmap:update(dt)
    local pixel, gold, lcd = fonts.get('pixel'), fonts.get('pixelGold'), fonts.get('lcd')
    self:setStatus(string.format('text BMFont native size %d, binary BMFont %d, grid font %d, distance field %s', pixel.nativeSize, gold.nativeSize, lcd.nativeSize, tostring(pixel.distanceField)))
end

function Bitmap:render()
    local stage = self:stage()
    if stage == nil then
        return
    end
    local pixel, gold, lcd = fonts.get('pixel'), fonts.get('pixelGold'), fonts.get('lcd')
    local native = pixel.nativeSize
    graphics2d.beginScreen()

    local x, y = stage.x, stage.y
    sample.caption('haylen_pixel.fnt, text format, at 1, 2, 3 and 4 times its native size', x, y, {color = '#FF8FB0FF'})
    y = y + 34
    for scale = 1, 4 do
        graphics2d.drawText(pixel, kLine, x, y, {size = native * scale})
        y = y + native * scale * 1.4
    end
    graphics2d.drawText(pixel, 'Red', x, y, {size = native * 5, color = '#FFFF6A6A'})
    graphics2d.drawText(pixel, 'Green', x + 180, y, {size = native * 5, color = '#FF6FDCA0'})
    graphics2d.drawText(pixel, 'Blue', x + 420, y, {size = native * 5, color = '#FF7FCBF2'})
    graphics2d.drawText(pixel, 'Shadow', x + 620, y, {size = native * 5, shadowOffset = {5, 5}, shadowColor = '#FF4C7DFF'})

    x, y = stage.x + stage.width * 0.52, stage.y
    sample.caption('haylen_pixel_gold.fnt, binary format, colors kept', x, y, {color = '#FF8FB0FF'})
    graphics2d.drawText(gold, 'GAME OVER', x, y + 40, {size = native * 6})
    graphics2d.drawText(gold, 'High score', x, y + 130, {size = native * 4})
    graphics2d.drawText(gold, 'tinted', x + 420, y + 130, {size = native * 4, color = '#FF80C0FF'})
    graphics2d.drawRichText('[b]Bold[/b] and [i]italic[/i] are synthesized', x, y + 210, {family = fonts.family('pixel'), size = native * 3})

    local panelY = stage.y + stage.height * 0.55
    sample.caption('lcd_digits.png as a grid font of 12 by 20 cells', x, panelY, {color = '#FF8FB0FF'})
    graphics2d.drawRect({x, panelY + 36, 560, 270}, '#FF0B1A10')
    local time = haylen.time()
    graphics2d.drawText(lcd, string.format('%02d:%04.1f', math.floor(time / 60), time % 60), x + 30, panelY + 56, {size = 60})
    graphics2d.drawText(lcd, string.format('-%07.1f', 1024 + time * 3.7), x + 30, panelY + 136, {size = 60})
    graphics2d.drawText(lcd, '88:88.8', x + 30, panelY + 226, {size = 40})
    sample.caption('three times and twice the cell size', x + 300, panelY + 240)
end

return Bitmap
