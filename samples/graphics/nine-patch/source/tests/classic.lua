-- Stretch and tile: a framed image cut by borders into nine regions, drawn at a size that grows and shrinks, once with its edges and center stretched and once with them repeated at their pixel size.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Classic = haylen.class('Classic', sample.Test)

local kBorder = 28
local kPreview = 2
local kCode = [[
local texture = assets.texture('frames/panel.png')
local stretched = graphics2d.newNineSlice(texture, {borders = {28, 28, 28, 28}})
local tiled = graphics2d.newNineSlice(texture, {borders = {28, 28, 28, 28}, fill = 'tile'})
graphics2d.drawNineSlice(stretched, {x, y, width, height})  -- Corners keep their size.]]

function Classic:enter()
    self.texture = sample.texture('frames/panel.png')
    self.stretched = graphics2d.newNineSlice(self.texture, {borders = {kBorder, kBorder, kBorder, kBorder}})
    self.tiled = graphics2d.newNineSlice(self.texture, {borders = {kBorder, kBorder, kBorder, kBorder}, fill = 'tile'})
    self.grow = {amount = 0}
    self.growing = tween.to(self.grow, 2.4, {amount = 1}, {owner = self, loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut'})
    self:frame({
        hint = 'The stripes of the center show the difference: stretched they widen, tiled they repeat.',
        code = kCode,
        controls = {ui.toggle{id = 'animate', text = 'Animate', checked = true, onChange = function(event)
            if event.checked then
                self.growing:resume()
            else
                self.growing:pause()
            end
        end}},
        focus = 'animate',
    })
end

function Classic:update(dt)
    Classic.super.update(self, dt)
    if self.frameSize then
        self:status(string.format('Frames %.0f x %.0f   borders %s   fills %s and %s', self.frameSize[1], self.frameSize[2], table.concat(self.stretched.borders, ', '), self.stretched.fill, self.tiled.fill))
    end
end

-- Draws the source image enlarged with lines where the borders cut it.
function Classic:drawSource(x, y)
    local size = self.texture.width * kPreview
    graphics2d.draw(self.texture, x, y, {pivotX = 0, pivotY = 0, scaleX = kPreview, scaleY = kPreview})
    local cut = kBorder * kPreview
    for _, offset in ipairs({cut, size - cut}) do
        graphics2d.drawLine(x + offset, y - 10, x + offset, y + size + 10, 2, sample.red, {layer = 1})
        graphics2d.drawLine(x - 10, y + offset, x + size + 10, y + offset, 2, sample.red, {layer = 1})
    end
    graphics2d.drawText(nil, 'Source, borders 28', x + size / 2, y + size + 36, {size = 22, color = sample.muted, anchor = {0.5, 0.5}})
end

function Classic:draw(area)
    self:drawSource(30, 40)
    local left = 30 + self.texture.width * kPreview + 50
    local room = (area.width - left - 30) / 2 - 20
    local width = 150 + (room - 150) * self.grow.amount
    local height = 150 + (area.height - 130 - 150) * self.grow.amount
    self.frameSize = {width, height}
    graphics2d.drawNineSlice(self.stretched, {left, 40, width, height})
    graphics2d.drawNineSlice(self.tiled, {left + room + 40, 40, width, height})
    graphics2d.drawText(nil, 'Fill "stretch"', left, area.height - 50, {size = 26, color = sample.ink})
    graphics2d.drawText(nil, 'Fill "tile"', left + room + 40, area.height - 50, {size = 26, color = sample.ink})
end

return Classic
