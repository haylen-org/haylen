-- Nine pieces: a frame made of nine separate regions of a sheet, listed row by row from the top-left corner, stretched and tiled at a size that grows and shrinks.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Pieces = haylen.class('Pieces', Test)

Pieces.piece = 40
Pieces.gap = 10
Pieces.preview = 2
Pieces.filters = {{id = 'nearest', text = 'Nearest'}, {id = 'linear', text = 'Linear'}}
Pieces.code = [[
local pieces = {}  -- Nine rectangles, row by row from the top-left corner.
for row = 0, 2 do for column = 0, 2 do pieces[#pieces + 1] = {column * 50, row * 50, 40, 40} end end
local frame = graphics2d.newNineSlice(assets.texture('nine-slice/pieces.png', {filter = 'nearest'}), {pieces = pieces})
log.info(#frame.pieces, table.concat(frame.borders, ', '))  -- 9  40, 40, 40, 40]]

function Pieces:enter()
    self:build('nearest')
    self.grow = {amount = 0}
    tween.to(self.grow, 2.4, {amount = 1}, {owner = self, loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut'})
    self:frame{
        code = Pieces.code,
        hint = 'The dark gaps between the pieces never show, with either filter, since each region is read on its own.',
        controls = {ui.formField{label = 'Filter', ui.segmentedControl{id = 'filter', items = Pieces.filters, selected = 'nearest', onChange = function(event) self:build(event.value) end}}},
        focus = 'filter',
    }
end

-- Cuts the sheet loaded with a filter into the nine pieces, stretched and tiled.
function Pieces:build(filter)
    self.filter = filter
    self.texture = assets.texture('nine-slice/pieces.png', {filter = filter})
    local pieces = {}
    for row = 0, 2 do
        for column = 0, 2 do
            pieces[#pieces + 1] = {column * (Pieces.piece + Pieces.gap), row * (Pieces.piece + Pieces.gap), Pieces.piece, Pieces.piece}
        end
    end
    self.stretched = graphics2d.newNineSlice(self.texture, {pieces = pieces})
    self.tiled = graphics2d.newNineSlice(self.texture, {pieces = pieces, fill = 'tile'})
end

function Pieces:update(dt)
    Pieces.super.update(self, dt)
    local center = self.stretched.pieces[5]
    self:status(string.format('Pieces %d   Borders %s   Center %.0f x %.0f   Filter "%s"', #self.stretched.pieces, table.concat(self.stretched.borders, ', '), center.width, center.height, self.filter))
end

function Pieces:draw(area)
    local x, y, scale = 30, 40, Pieces.preview
    graphics2d.draw(self.texture, x, y, {pivotX = 0, pivotY = 0, scaleX = scale, scaleY = scale})
    for index, piece in ipairs(self.stretched.pieces) do
        graphics2d.drawRectOutline({x + piece.x * scale, y + piece.y * scale, piece.width * scale, piece.height * scale}, 2, Test.warm, {layer = 1})
        graphics2d.drawText(nil, tostring(index), x + piece.x * scale + 8, y + piece.y * scale + 6, {size = 22, color = '#FF1B1E2B', layer = 2})
    end

    local left = x + self.texture.width * scale + 50
    local room = (area.width - left - 30) / 2 - 20
    local width = 130 + (room - 130) * self.grow.amount
    local height = 130 + (area.height - 130 - 130) * self.grow.amount
    graphics2d.drawNineSlice(self.stretched, {left, 40, width, height})
    graphics2d.drawNineSlice(self.tiled, {left + room + 40, 40, width, height})
    Test.caption('Fill "stretch"', left, area.height - 50, {size = 26, color = Test.ink})
    Test.caption('Fill "tile"', left + room + 40, area.height - 50, {size = 26, color = Test.ink})
end

return Pieces
