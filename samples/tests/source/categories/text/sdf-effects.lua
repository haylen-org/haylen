-- Outline, shadow and glow: the text shader draws them from the distance field of the font, which reaches as far as the spread the font was baked with. Lilita One bakes with a spread of 16 at 64 for thick effects, and a painted landscape shows how a thin outline keeps small text readable over a busy picture.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local SdfEffects = haylen.class('SdfEffects', TextTest)

SdfEffects.code = [[
local font = assets.font('fonts/lilita-one-regular.ttf', {bakeSize = 64, spread = 16})
graphics2d.drawText(font, 'Outline', x, y, {size = 112, outlineWidth = 8, outlineColor = '#FF8A1E1E'})
graphics2d.drawText(font, 'Shadow', x, y, {size = 112, shadowOffset = {10, 12}, shadowBlur = 8})
local glow = graphics2d.newRichText('[glow=14 color=#FFFF8A00]Glow[/glow]', {family = family, size = 112})
graphics2d.drawText(font, 'Wood 12', x, y, {size = 48, outlineWidth = 3, outlineColor = '#FF000000'})]]

SdfEffects.size = 112
SdfEffects.hud = 'Wood 12   Stone 4   Night 3'
SdfEffects.backdrop = '#FF161B28'

function SdfEffects:init(entry)
    SdfEffects.super.init(self, entry)
    self.outline = 8
    self.blur = 8
    self.glow = 14
    self.font = fonts.load('lilita')
    self.family = fonts.family('lilita')
    self.scenery = {
        sky = assets.texture('tiled/images/sky.png', {filter = 'linear'}),
        mountains = assets.texture('camera/mountains.png', {filter = 'linear'}),
        hills = assets.texture('camera/hills.png', {filter = 'linear'}),
        grass = assets.texture('camera/grass.png', {filter = 'linear'}),
    }
    self:refresh()
end

function SdfEffects:slider(id, label, max)
    return ui.formField{label = label, ui.slider{id = id, min = 0, max = max, step = 1, value = self[id], showValue = true, decimals = 0, onChange = function(event)
        self[id] = event.value
        self:refresh()
    end}}
end

function SdfEffects:enter()
    self:frame{
        code = SdfEffects.code,
        hint = 'Drag the sliders, or focus them and press left and right. An effect grows until it reaches the spread of the distance field, as the status line shows, and over the landscape only the outlined line stays easy to read.',
        controls = {self:slider('outline', 'Outline width', 24), self:slider('blur', 'Shadow blur', 20), self:slider('glow', 'Glow reach', 28)},
        focus = 'outline',
    }
end

-- The glow is a tag of rich text, so its markup follows the slider.
function SdfEffects:refresh()
    local size = SdfEffects.size
    self.glowText = graphics2d.newRichText(string.format('[glow=%d color=#FFFF8A00]Glow[/glow]', self.glow), {family = self.family, size = size, color = '#FFFFF3B0'})
    self.allText = graphics2d.newRichText(string.format('[outline=%d color=#FF3B1A00][shadow=8,10 color=#C0000000 blur=%d][glow=%d color=#FF40C4FF]Island[/glow][/shadow][/outline]', math.min(self.outline, 10), self.blur, self.glow), {family = self.family, size = size})
end

function SdfEffects:update(dt)
    SdfEffects.super.update(self, dt)
    self.glowText:update(dt)
    self.allText:update(dt)
    local font, size = self.font, SdfEffects.size
    self:status(string.format('In distance units, where 0.5 spans the whole spread of the field, the outline reaches %.3f, the blur %.3f and the glow %.3f.', font:toDistance(self.outline, size), font:toDistance(self.blur, size), font:toDistance(self.glow, size)))
end

-- Draws the four effects in two rows, each word under its label.
function SdfEffects:effects(left, right, top, rowHeight)
    local font, size = self.font, SdfEffects.size
    local wordTop = top + 44
    TextTest.label(string.format('Outline %d units wide', self.outline), left, top)
    graphics2d.drawText(font, 'Outline', left, wordTop, {size = size, color = '#FFFFD166', outlineWidth = self.outline, outlineColor = '#FF8A1E1E'})
    TextTest.label(string.format('Shadow 10, 12 units away, blurred by %d', self.blur), right, top)
    graphics2d.drawText(font, 'Shadow', right, wordTop, {size = size, color = '#FFE8EAF2', shadowOffset = {10, 12}, shadowColor = '#E0000000', shadowBlur = self.blur})

    top = top + rowHeight
    wordTop = top + 44
    TextTest.label(string.format('Glow reaching %d units, from rich text', self.glow), left, top)
    self.glowText:draw(left, wordTop)
    TextTest.label('All three together', right, top)
    self.allText:draw(right, wordTop)
end

-- Paints a dusk landscape from the painted strips of the package across the band, behind the small text.
function SdfEffects:landscape(band)
    local scenery = self.scenery
    graphics2d.draw(scenery.sky, band.x, band.y, {width = band.width, height = band.height, pivotX = 0, pivotY = 0})
    local function strip(texture, bottom, height)
        local width = texture.width * height / texture.height
        for x = band.x, band.x + band.width, width do
            graphics2d.draw(texture, x, bottom, {width = width, height = height, pivotX = 0, pivotY = 1})
        end
    end
    strip(scenery.mountains, band.y + band.height * 0.82, band.height * 0.62)
    strip(scenery.hills, band.y + band.height * 1.02, band.height * 0.72)
    strip(scenery.grass, band.y + band.height + 4, band.height * 0.34)
end

-- Draws a label on a dark plate, so it reads over the landscape.
function SdfEffects.plate(text, x, y)
    local width = graphics2d.measureText(nil, text, {size = 24})
    graphics2d.drawShape({x, y, width + 36, 40}, {radius = 20, color = '#D0101622', layer = 1})
    TextTest.label(text, x + 18, y + 6, {size = 24, color = Test.ink})
end

function SdfEffects:draw(area)
    local layout = self.layout
    TextTest.fill(SdfEffects.backdrop)
    local band = {x = 0, y = layout.height * 0.6, width = layout.width, height = layout.height * 0.4}
    local right = layout.width / 2 + 24
    self:effects(24, right, 0, band.y / 2)

    graphics2d.pushMask(function()
        graphics2d.drawShape({band.x, band.y, band.width, band.height}, {radius = 18})
    end)
    self:landscape(band)
    local hudY = band.y + band.height * 0.42
    SdfEffects.plate('With an outline 3 units wide', 24, band.y + 20)
    graphics2d.drawText(self.font, SdfEffects.hud, 24, hudY, {size = 48, outlineWidth = 3, outlineColor = '#FF000000'})
    SdfEffects.plate('Without an outline', right, band.y + 20)
    graphics2d.drawText(self.font, SdfEffects.hud, right, hudY, {size = 48})
    graphics2d.popMask()
    graphics2d.drawShape({band.x, band.y, band.width, band.height}, {radius = 18, color = '#00000000', borderWidth = 2, borderColor = TextTest.boxBorder, layer = 3})
end

return SdfEffects
