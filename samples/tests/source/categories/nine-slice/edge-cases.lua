-- Edge cases: frames cut from a sheet whose gaps are magenta, so any texel read outside a piece shows as a pink seam. They draw with linear filtering at fractional places and border scales, tiled at sizes that are not a multiple of their tiles, smaller than their own borders, and from a sheet of twice the resolution at half the border scale, which must look the same. The layout of each case is checked to cover its rectangle exactly with quads that stay inside their pieces.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local Results = require('harness.results')
local Test = require('harness.test')

local EdgeCases = haylen.class('EdgeCases', Test)

EdgeCases.borderScale = 2.7
EdgeCases.view = {1500, 860}
EdgeCases.tiny = {{24, 18}, {12, 60}, {90, 10}, {6, 6}}
EdgeCases.code = [[
local frame = graphics2d.newNineSlice(sheet, {pieces = pieces, fill = 'tile'})
graphics2d.drawNineSlice(frame, {10.3, 20.6, 203.3, 131.9}, nil, nil, 2.7)
local quads = frame:layout({0, 0, 24, 18}, 2.7)  -- Borders shrink until they meet.]]

-- Builds a sheet of three by three pieces of `size` texels with magenta gaps of `gap` texels: a blue center and yellow borders that darken toward their outer side, so stretched and tiled pieces look different. Returns the texture and the nine regions row by row.
function EdgeCases.sheet(size, gap)
    local width = size * 3 + gap * 2
    local bytes = {}
    for y = 0, width - 1 do
        for x = 0, width - 1 do
            local column, row = x // (size + gap), y // (size + gap)
            local u, v = x % (size + gap), y % (size + gap)
            if u >= size or v >= size then
                bytes[#bytes + 1] = string.char(255, 0, 255, 255)
            elseif column == 1 and row == 1 then
                bytes[#bytes + 1] = string.char(70, 140, 220, 255)
            else
                local across = (column == 1) and (row == 0 and size - 1 - v or v) or (column == 0 and size - 1 - u or u)
                local shade = math.floor(140 + 110 * across / (size - 1))
                bytes[#bytes + 1] = string.char(shade, math.floor(shade * 0.8), 40, 255)
            end
        end
    end
    local pieces = {}
    for row = 0, 2 do
        for column = 0, 2 do
            pieces[#pieces + 1] = {column * (size + gap), row * (size + gap), size, size}
        end
    end
    return graphics.newTexture(width, width, {pixels = table.concat(bytes), filter = 'linear'}), pieces
end

function EdgeCases:enter()
    local texture, pieces = EdgeCases.sheet(8, 2)
    local sharp, sharpPieces = EdgeCases.sheet(16, 4)
    self.pieces = pieces
    self.stretched = graphics2d.newNineSlice(texture, {pieces = pieces})
    self.tiled = graphics2d.newNineSlice(texture, {pieces = pieces, fill = 'tile'})
    self.sharp = graphics2d.newNineSlice(sharp, {pieces = sharpPieces})
    self.time = 0
    self.results = Results(self.entry.code)
    self:check()
    self:frame{code = EdgeCases.code, view = EdgeCases.view, hint = 'No pink seam may show anywhere, the tiled frame ends every edge on a whole or partial tile without slivers, the small frames shrink their borders until they meet, and the two lower frames look alike.'}
    self.camera.position = {EdgeCases.view[1] / 2, EdgeCases.view[2] / 2}
    self.layout = m.rect(0, 0, EdgeCases.view[1], EdgeCases.view[2])
end

-- Lays out each case and checks that its quads cover the rectangle exactly and read only the pixels of their own piece.
function EdgeCases:check()
    local scale = EdgeCases.borderScale
    local cases = {
        {key = 'stretch', name = 'Stretched frame', slice = self.stretched, rect = {0.3, 0.6, 151.7, 77.2}},
        {key = 'tile', name = 'Tiled frame', slice = self.tiled, rect = {0.3, 0.6, 203.3, 131.9}},
        {key = 'small', name = 'Frame smaller than its borders', slice = self.tiled, rect = {0, 0, 24, 18}},
    }
    for _, case in ipairs(cases) do
        local covered, inside = 0, true
        local quads = case.slice:layout(case.rect, scale)
        for _, quad in ipairs(quads) do
            covered = covered + quad.area.width * quad.area.height
            inside = inside and self:withinPiece(quad.source)
        end
        local area = case.rect[3] * case.rect[4]
        local exact = math.abs(covered - area) <= area * 0.0001
        self.results:set(case.key, exact and inside and 'pass' or 'fail', case.name, string.format('%d quads cover %.2f of %.2f square units%s.', #quads, covered, area, inside and ' inside their pieces' or ', and one reads outside its piece'))
    end

    local corner = self.stretched:layout({0, 0, 24, 18}, scale)[1].area
    local meets = corner.width == 12 and corner.height == 9
    self.results:set('meet', meets and 'pass' or 'fail', 'Shrunk borders', string.format('The top-left corner of a 24 x 18 frame with borders of %.1f is %g x %g.', 8 * scale, corner.width, corner.height))
end

function EdgeCases:withinPiece(source)
    for _, piece in ipairs(self.pieces) do
        local x, y, width, height = piece[1], piece[2], piece[3], piece[4]
        if source.x >= x - 0.001 and source.y >= y - 0.001 and source.x + source.width <= x + width + 0.001 and source.y + source.height <= y + height + 0.001 then
            return true
        end
    end
    return false
end

function EdgeCases:update(dt)
    EdgeCases.super.update(self, dt)
    self.time = self.time + dt
    self:status(self.results:summary())
end

-- Every frame drifts by a fraction of a pixel, so its edges cross texel boundaries while it draws.
function EdgeCases:draw()
    local layout = self.layout
    local scale = EdgeCases.borderScale
    local drift = (self.time * 0.37) % 1
    local column = 480
    local x, y = 30 + drift, 30 + drift

    graphics2d.drawNineSlice(self.stretched, {x, y, 151.7 + 40 * math.sin(self.time), 77.2 + 30 * math.cos(self.time * 0.8)}, nil, nil, scale)
    Test.caption('Stretched at a border scale of 2.7', 30, y + 130, {color = Test.ink})

    graphics2d.drawNineSlice(self.tiled, {x + column, y, 203.3 + 50 * math.sin(self.time * 0.7), 131.9}, nil, nil, scale)
    Test.caption('Tiled at sizes off the tile grid', 30 + column, y + 150, {color = Test.ink})

    local left = x + column * 2
    for _, size in ipairs(EdgeCases.tiny) do
        graphics2d.drawNineSlice(self.tiled, {left, y, size[1], size[2]}, nil, nil, scale)
        left = left + size[1] + 24
    end
    Test.caption('Smaller than their borders', 30 + column * 2, y + 150, {color = Test.ink})

    local lower = y + 210
    graphics2d.drawNineSlice(self.stretched, {x, lower, 240, 120}, nil, nil, scale)
    graphics2d.drawNineSlice(self.sharp, {x + 270, lower, 240, 120}, nil, nil, scale / 2)
    Test.caption('A sheet of 8 texel pieces and one of 16 at half the border scale', 30, lower + 130, {color = Test.ink})

    self.results:draw(layout, lower + 180)
end

return EdgeCases
