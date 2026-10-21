-- Nine pieces: a wooden frame made of nine separate regions of a sheet, listed row by row from the top-left corner, stretched and tiled at a size that grows and shrinks. The pieces sit apart in the sheet with transparent gaps between them, which never show.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Pieces = haylen.class('Pieces', Test)

-- The sheet holds corners of 75 pixels around a middle of 234, with gaps of 16 between the pieces.
Pieces.border = 75
Pieces.middle = 234
Pieces.gap = 16
Pieces.filters = {{id = 'linear', text = 'Linear'}, {id = 'nearest', text = 'Nearest'}}
Pieces.code = [[
local spans = {{0, 75}, {91, 234}, {341, 75}}  -- Where each column and row of pieces starts, and its size.
local pieces = {}
for _, row in ipairs(spans) do
    for _, column in ipairs(spans) do pieces[#pieces + 1] = {column[1], row[1], column[2], row[2]} end
end
local frame = graphics2d.newNineSlice(assets.texture('nine-slice/pieces.png', {filter = 'linear'}), {pieces = pieces})]]

function Pieces:enter()
    self:build('linear')
    self.grow = {amount = 0}
    tween.to(self.grow, 2.4, {amount = 1}, {owner = self, loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut'})
    self:frame{
        code = Pieces.code,
        hint = 'The gaps between the pieces never show, with either filter, since each region is read on its own.',
        controls = {ui.formField{label = 'Filter', ui.segmentedControl{id = 'filter', items = Pieces.filters, selected = 'linear', onChange = function(event) self:build(event.value) end}}},
        focus = 'filter',
    }
end

-- Cuts the sheet loaded with a filter into the nine pieces, stretched and tiled.
function Pieces:build(filter)
    self.filter = filter
    self.texture = assets.texture('nine-slice/pieces.png', {filter = filter})
    local spans = {{0, Pieces.border}, {Pieces.border + Pieces.gap, Pieces.middle}, {Pieces.border + Pieces.middle + Pieces.gap * 2, Pieces.border}}
    local pieces = {}
    for _, row in ipairs(spans) do
        for _, column in ipairs(spans) do
            pieces[#pieces + 1] = {column[1], row[1], column[2], row[2]}
        end
    end
    self.stretched = graphics2d.newNineSlice(self.texture, {pieces = pieces})
    self.tiled = graphics2d.newNineSlice(self.texture, {pieces = pieces, fill = 'tile'})
end

function Pieces:update(dt)
    Pieces.super.update(self, dt)
    local center = self.stretched.pieces[5]
    local borders = self.stretched.borders
    self:status(string.format('%d pieces with borders of %g, %g, %g and %g pixels around a middle of %.0f by %.0f, read with the "%s" filter.', #self.stretched.pieces, borders[1], borders[2], borders[3], borders[4], center.width, center.height, self.filter))
end

-- Draws the sheet with an outline and the number of every piece, on a dark plate so the gaps show as gaps.
function Pieces:drawSheet(x, y, size)
    local scale = size / self.texture.width
    graphics2d.drawShape({x - 8, y - 8, size + 16, size + 16}, {radius = 12, color = '#FF151A2A'})
    graphics2d.draw(self.texture, x, y, {pivotX = 0, pivotY = 0, scaleX = scale, scaleY = scale, layer = 1})
    for index, piece in ipairs(self.stretched.pieces) do
        local rect = {x + piece.x * scale, y + piece.y * scale, piece.width * scale, piece.height * scale}
        graphics2d.drawRectOutline(rect, 2, Test.warm, {layer = 2})
        local cx, cy = rect[1] + rect[3] / 2, rect[2] + rect[4] / 2
        graphics2d.drawShape({cx - 15, cy - 15, 30, 30}, {kind = 'ellipse', color = Test.warm, borderWidth = 2, borderColor = '#FF15172A', layer = 3})
        graphics2d.drawText(nil, tostring(index), cx, cy, {size = 18, color = '#FF15172A', anchor = {0.5, 0.5}, layer = 4})
    end
    Test.caption('The sheet of nine pieces', x + size / 2, y + size + 22, {size = 24, color = Test.ink, anchor = {0.5, 0}})
end

function Pieces:draw(area)
    local margin, captionRoom = 30, 50
    local size = math.min(300, area.height - margin * 2 - captionRoom - 20)
    self:drawSheet(margin + 8, margin + 18, size)
    local left = margin + size + 76
    local room = (area.width - left - margin - 40) / 2
    local smallest = 170
    local width = smallest + (room - smallest) * self.grow.amount
    local height = smallest + (area.height - margin * 2 - captionRoom - smallest) * self.grow.amount
    local scale = 0.7
    graphics2d.drawNineSlice(self.stretched, {left, margin, width, height}, nil, {borderScale = scale})
    graphics2d.drawNineSlice(self.tiled, {left + room + 40, margin, width, height}, nil, {borderScale = scale})
    Test.caption('Fill "stretch"', left + width / 2, margin + height + 14, {size = 26, color = Test.ink, anchor = {0.5, 0}})
    Test.caption('Fill "tile"', left + room + 40 + width / 2, margin + height + 14, {size = 26, color = Test.ink, anchor = {0.5, 0}})
end

return Pieces
