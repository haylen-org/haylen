-- Bitmap fonts: Haylen Pixel as a ".fnt" bitmap font in the text format with white glyphs the text color tints, the same font in the binary format with gold glyphs that keep their colors, and LCD digits cut from an image of equal cells as a grid font. Every size is a whole number of screen pixels per pixel of the font, so the strokes keep one width.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Bitmap = haylen.class('Bitmap', TextTest)

Bitmap.code = [[
local pixel = assets.font('text/haylen-pixel.fnt', {filter = 'nearest'})
graphics2d.drawText(pixel, 'PRESS START to play!', x, y, {size = pixel.nativeSize * 3, pixelSnap = true})
graphics2d.drawText(pixel, 'Shadow', x, y, {size = pixel.nativeSize * 5, shadowOffset = {5, 5}, shadowColor = '#FF141626'})
local lcd = assets.load('text/lcd-digits.png', 'gridFont', {characters = '0123456789:.-', cellWidth = 12, cellHeight = 20, advance = 13, baseline = 19})
graphics2d.drawText(lcd, '12:04.5', x, y, {size = 60})]]

Bitmap.line = 'PRESS START to play!'
Bitmap.story = 'Bitmap fonts wrap at a width like any other font, and keep every pixel square.'
Bitmap.lcdFrame = '#FF0B1A10'
Bitmap.words = {{'Red', '#FFFF6A6A'}, {'Green', '#FF6FDCA0'}, {'Blue', '#FF7FCBF2'}, {'Shadow', '#FFFFF3D6'}}

function Bitmap:init(entry)
    Bitmap.super.init(self, entry)
    self.pixel = fonts.load('pixel')
    self.gold = fonts.load('pixelGold')
    self.lcd = fonts.load('lcd')
    self.pixelFamily = fonts.family('pixel')
end

function Bitmap:enter()
    self:frame{code = Bitmap.code, hint = 'The white lines draw each pixel of the font one to four screen pixels wide, and the gold font keeps the colors of its image unless a tint multiplies them. Bitmap fonts have no distance field, so they take no outline or glow, and their shadow is the silhouette of their glyphs.'}
end

function Bitmap:update(dt)
    Bitmap.super.update(self, dt)
    local field = self.pixel.distanceField or self.gold.distanceField or self.lcd.distanceField
    self:status(string.format('Both bitmap fonts are drawn for size %d and the grid font for size %d, and %s.', self.pixel.nativeSize, self.lcd.nativeSize, field and 'one of them has a distance field' or 'none of them has a distance field, so outlines and glows do not apply'))
end

-- Draws text in a bitmap font at a whole multiple of its pixels, from a point on a whole pixel.
function Bitmap:crisp(font, text, x, y, multiple, options)
    options = options or {}
    options.size = self:pixelSize(font, multiple)
    options.pixelSnap = true
    graphics2d.drawText(font, text, x, y, options)
    return options.size
end

function Bitmap:textFormat(x, width, height)
    local pixel = self.pixel
    TextTest.label('The text format file "haylen-pixel.fnt"', x, 0, {size = 28, color = Test.accent})
    local lines = 0
    for count = 1, 4 do
        lines = lines + self:screenPixelSize(pixel, count) * 1.3
    end
    local five, story = self:pixelSize(pixel, 5), self:pixelSize(pixel, 3)
    local plateHeight = five * 1.9
    local _, storyHeight = graphics2d.measureText(pixel, Bitmap.story, {size = story, maxWidth = width, lineSpacing = 1.3})
    local gap = (height - 52 - lines - plateHeight - storyHeight) / 2

    local y = 52
    for count = 1, 4 do
        local size = self:screenPixelSize(pixel, count)
        graphics2d.drawText(pixel, Bitmap.line, x, y, {size = size, pixelSnap = true, color = Test.ink})
        y = y + size * 1.3
    end

    -- The colored words sit on a lighter plate, where the dark shadow one pixel of the font away shows.
    y = y + gap
    graphics2d.drawShape({x, y, width, plateHeight}, {radius = 14, color = '#FF3A4462', borderWidth = 2, borderColor = '#FF4E5878'})
    local unit = five / pixel.nativeSize
    local wordX, wordY = x + unit * 4, y + (plateHeight - five) / 2
    for _, word in ipairs(Bitmap.words) do
        self:crisp(pixel, word[1], wordX, wordY, 5, {color = word[2], shadowOffset = {unit, unit}, shadowColor = '#FF141626'})
        wordX = wordX + graphics2d.measureText(pixel, word[1], {size = five}) + unit * 5
    end

    y = y + plateHeight + gap
    self:crisp(pixel, Bitmap.story, x, y, 3, {color = Test.ink, maxWidth = width, lineSpacing = 1.3})
end

-- The gold font and the LCD panel share the right column, spaced over its height.
function Bitmap:binaryFormat(x, width, height)
    local gold, pixel, lcd = self.gold, self.pixel, self.lcd
    local six, four, three = self:pixelSize(gold, 6), self:pixelSize(gold, 4), self:pixelSize(pixel, 3)
    local large, small = self:pixelSize(lcd, 3), self:pixelSize(lcd, 2)
    local padding = 28
    local panelHeight = large * 2 + small + padding * 4
    local gap = (height - 52 - six - four - three - 48 - panelHeight) / 3

    TextTest.label('The binary format file "haylen-pixel-gold.fnt"', x, 0, {size = 28, color = Test.accent})
    local y = 52
    self:crisp(gold, 'GAME OVER', x, y, 6)
    y = y + six + gap
    self:crisp(gold, 'High score', x, y, 4)
    self:crisp(gold, 'Tinted red', x + width * 0.5, y, 4, {color = '#FFFF8A8A'})
    y = y + four + gap * 0.6
    local startX, startY = self:onPixel(x, y)
    graphics2d.drawRichText('[b]Bold[/b] and [i]italic[/i] are synthesized', startX, startY, {family = self.pixelFamily, size = three, color = Test.ink})
    y = y + three + gap * 1.4

    TextTest.label('A grid font of LCD digits in cells of 12 by 20', x, y, {size = 28, color = Test.accent})
    y = y + 48
    local time = haylen.elapsed()
    local panelWidth = math.min(width, graphics2d.measureText(lcd, '-0000000.0', {size = large}) + padding * 3)
    graphics2d.drawShape({x, y, panelWidth, panelHeight}, {radius = 18, color = Bitmap.lcdFrame, borderWidth = 6, borderColor = '#FF2C3A33'})
    graphics2d.drawShape({x + 10, y + 10, panelWidth - 20, panelHeight - 20}, {radius = 10, color = '#FF071208', borderWidth = 2, borderColor = '#FF1B3022'})
    local textX, textY = x + padding * 1.5, y + padding
    graphics2d.drawText(lcd, string.format('%02d:%04.1f', math.floor(time / 60), time % 60), textX, textY, {size = large, pixelSnap = true})
    textY = textY + large + padding
    graphics2d.drawText(lcd, string.format('-%07.1f', 1024 + time * 3.7), textX, textY, {size = large, pixelSnap = true})
    textY = textY + large + padding
    graphics2d.drawText(lcd, '88:88.8', textX, textY, {size = small, pixelSnap = true})
end

function Bitmap:draw(area)
    local layout = self.layout
    local column = (layout.width - 80) / 2
    self:textFormat(0, column, layout.height)
    self:binaryFormat(column + 80, column, layout.height)
end

return Bitmap
