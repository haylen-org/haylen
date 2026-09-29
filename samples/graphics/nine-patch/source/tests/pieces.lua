-- Nine pieces: a frame made of nine separate regions of a sheet, listed row by row from the top-left corner, stretched and tiled at a size that grows and shrinks.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')

local sample = require('sample')

local Pieces = haylen.class('Pieces', sample.Test)

local kPiece, kGap, kPreview = 40, 10, 2
local kCode = [[
local pieces = {}  -- nine rectangles, row by row from the top-left corner
for row = 0, 2 do for column = 0, 2 do pieces[#pieces + 1] = {column * 50, row * 50, 40, 40} end end
local frame = graphics2d.newNineSlice(assets.texture('frames/pieces.png', {filter = 'nearest'}), {pieces = pieces})
print(#frame.pieces, table.concat(frame.borders, ', '))  -- 9  40, 40, 40, 40]]

function Pieces:enter()
    self.texture = sample.texture('frames/pieces.png', 'nearest')
    local pieces = {}
    for row = 0, 2 do
        for column = 0, 2 do
            pieces[#pieces + 1] = {column * (kPiece + kGap), row * (kPiece + kGap), kPiece, kPiece}
        end
    end
    self.stretched = graphics2d.newNineSlice(self.texture, {pieces = pieces})
    self.tiled = graphics2d.newNineSlice(self.texture, {pieces = pieces, fill = 'tile'})
    self.grow = {amount = 0}
    tween.to(self.grow, 2.4, {amount = 1}, {owner = self, loop = 'yoyo', repeatCount = -1, ease = 'sine_in_out'})
    self:frame({hint = 'The gaps between the pieces never show, since each region is read on its own.', code = kCode})
end

function Pieces:update(dt)
    Pieces.super.update(self, dt)
    local center = self.stretched.pieces[5]
    self:status(string.format('%d pieces   borders %s   center %.0f x %.0f', #self.stretched.pieces, table.concat(self.stretched.borders, ', '), center.width, center.height))
end

function Pieces:draw(area)
    local x, y = 30, 40
    graphics2d.draw(self.texture, x, y, {pivotX = 0, pivotY = 0, scaleX = kPreview, scaleY = kPreview})
    for index, piece in ipairs(self.stretched.pieces) do
        graphics2d.drawRectOutline({x + piece.x * kPreview, y + piece.y * kPreview, piece.width * kPreview, piece.height * kPreview}, 2, sample.warm, {layer = 1})
        graphics2d.drawText(nil, tostring(index), x + piece.x * kPreview + 8, y + piece.y * kPreview + 6, {size = 22, color = '#FF1B1E2B', layer = 2})
    end

    local left = x + self.texture.width * kPreview + 50
    local room = (area.width - left - 30) / 2 - 20
    local width = 130 + (room - 130) * self.grow.amount
    local height = 130 + (area.height - 130 - 130) * self.grow.amount
    graphics2d.drawNineSlice(self.stretched, {left, 40, width, height})
    graphics2d.drawNineSlice(self.tiled, {left + room + 40, 40, width, height})
    graphics2d.drawText(nil, "fill = 'stretch'", left, area.height - 50, {size = 26, color = sample.ink})
    graphics2d.drawText(nil, "fill = 'tile'", left + room + 40, area.height - 50, {size = 26, color = sample.ink})
end

return Pieces
