-- Stretch and tile: a wooden frame cut by borders into nine regions, drawn at a size that grows and shrinks, once with its edges and center stretched and once with them repeated at their pixel size. The stripes of the middle repeat across its tiles without a seam, so stretching shows as wider stripes and tiling as more of them.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')

local StretchTile = haylen.class('StretchTile', Test)

StretchTile.border = 50
StretchTile.code = [[
local texture = assets.texture('nine-slice/panel.png', {filter = 'linear'})
local stretched = graphics2d.newNineSlice(texture, {borders = {50, 50, 50, 50}})
local tiled = graphics2d.newNineSlice(texture, {borders = {50, 50, 50, 50}, fill = 'tile'})
graphics2d.drawNineSlice(stretched, {x, y, width, height})  -- The corners keep their size.
graphics2d.drawNineSlice(tiled, {x, y, width, height})]]

function StretchTile:enter()
    local border = StretchTile.border
    self.texture = assets.texture('nine-slice/panel.png', {filter = 'linear'})
    self.stretched = graphics2d.newNineSlice(self.texture, {borders = {border, border, border, border}})
    self.tiled = graphics2d.newNineSlice(self.texture, {borders = {border, border, border, border}, fill = 'tile'})
    self.grow = {amount = 0}
    self.growing = tween.to(self.grow, 2.4, {amount = 1}, {owner = self, loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut'})
    self:frame{
        code = StretchTile.code,
        hint = 'The stripes of the middle show the difference: stretched they widen, tiled they repeat.',
        controls = {ui.toggle{id = 'animate', text = 'Animate', checked = true, onChange = function(event)
            if event.checked then
                self.growing:resume()
            else
                self.growing:pause()
            end
        end}},
        focus = 'animate',
    }
end

function StretchTile:update(dt)
    StretchTile.super.update(self, dt)
    if self.frameSize then
        self:status(string.format('Both frames are %.0f by %.0f units, with borders of %d pixels.', self.frameSize[1], self.frameSize[2], StretchTile.border))
    end
end

-- Draws the source image with lines where the borders cut it, scaled to `size`.
function StretchTile:drawSource(x, y, size)
    local scale = size / self.texture.width
    graphics2d.draw(self.texture, x, y, {pivotX = 0, pivotY = 0, scaleX = scale, scaleY = scale})
    local cut = StretchTile.border * scale
    for _, offset in ipairs({cut, size - cut}) do
        graphics2d.drawLine(x + offset, y - 10, x + offset, y + size + 10, 2, Test.red, {layer = 1})
        graphics2d.drawLine(x - 10, y + offset, x + size + 10, y + offset, 2, Test.red, {layer = 1})
    end
    Test.caption('The source, cut 50 pixels in', x + size / 2, y + size + 22, {size = 24, color = Test.ink, anchor = {0.5, 0}})
end

function StretchTile:draw(area)
    local margin, captionRoom = 30, 50
    local source = math.min(256, area.height - margin * 2 - captionRoom - 20)
    self:drawSource(margin, margin + 10, source)
    local left = margin + source + 60
    local room = (area.width - left - margin - 40) / 2
    local smallest = 160
    local width = smallest + (room - smallest) * self.grow.amount
    local height = smallest + (area.height - margin * 2 - captionRoom - smallest) * self.grow.amount
    self.frameSize = {width, height}
    graphics2d.drawNineSlice(self.stretched, {left, margin, width, height})
    graphics2d.drawNineSlice(self.tiled, {left + room + 40, margin, width, height})
    Test.caption('Fill "stretch"', left + width / 2, margin + height + 14, {size = 26, color = Test.ink, anchor = {0.5, 0}})
    Test.caption('Fill "tile"', left + room + 40 + width / 2, margin + height + 14, {size = 26, color = Test.ink, anchor = {0.5, 0}})
end

return StretchTile
