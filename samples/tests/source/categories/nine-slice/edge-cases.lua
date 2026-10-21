-- Edge cases: small wooden frames cut from a sheet whose gaps are magenta, so any texel read outside a piece shows as a pink seam. They draw with linear filtering at fractional places and border scales, tiled at sizes that are not a multiple of their tiles, smaller than their own borders, and from a sheet of twice the resolution at half the border scale, which must look the same. The layout of each case is checked to cover its rectangle exactly with quads that stay inside their pieces.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local Results = require('harness.results')
local Test = require('harness.test')

local EdgeCases = haylen.class('EdgeCases', Test)

EdgeCases.borderScale = 2.7
EdgeCases.tiny = {{24, 18}, {12, 60}, {90, 10}, {6, 6}}
EdgeCases.code = [[
local frame = graphics2d.newNineSlice(sheet, {pieces = pieces, fill = 'tile'})
graphics2d.drawNineSlice(frame, {10.3, 20.6, 203.3, 131.9}, nil, {borderScale = 2.7})
local quads = frame:layout({0, 0, 24, 18}, 2.7)  -- Borders shrink until they meet.]]

-- Builds a sheet of three by three pieces of `size` texels with magenta gaps of `gap` texels: a cream middle and borders of wood that darken toward their outer side, with a dark outline on that side, so stretched and tiled pieces look different. Returns the texture and the nine regions row by row.
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
                bytes[#bytes + 1] = string.char(243, 230, 196, 255)
            else
                local across = (column == 1) and (row == 0 and size - 1 - v or v) or (column == 0 and size - 1 - u or u)
                if across < size / 8 then
                    bytes[#bytes + 1] = string.char(30, 27, 51, 255)
                else
                    local shade = across / (size - 1)
                    bytes[#bytes + 1] = string.char(math.floor(120 + 90 * shade), math.floor(70 + 70 * shade), math.floor(36 + 40 * shade), 255)
                end
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
    self.resultsCamera = graphics2d.newCamera()
    self.resultsCamera.anchor = 'topLeft'
    self.tinyCamera = graphics2d.newCamera()
    self.tinyCamera.anchor = 'topLeft'
    self:frame{code = EdgeCases.code, hint = 'No pink seam may show anywhere, the tiled frame ends every edge on a whole or partial tile, the small frames shrink their borders until they meet, and the two lower frames look alike.'}
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

-- Draws a case in a card with its title and what it must show, and returns the rectangle left for its frames.
function EdgeCases.card(rect, title, text)
    graphics2d.drawShape(rect, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    graphics2d.drawText(nil, title, rect[1] + 20, rect[2] + 18, {size = 26, color = Test.ink, layer = 2})
    graphics2d.drawText(nil, text, rect[1] + 20, rect[2] + 56, {size = 20, color = Test.muted, maxWidth = rect[3] - 40, layer = 2})
    return {rect[1] + 20, rect[2] + 112, rect[3] - 40, rect[4] - 132}
end

-- Looks at a part of the stage through a camera of its own whose view starts at the corner of `rect`, so what follows draws there at `zoom` times its size.
function EdgeCases:look(camera, rect, zoom)
    camera.zoom = {zoom, zoom}
    camera.viewport = m.rect(self.stage.x + rect[1], self.stage.y + rect[2], rect[3], rect[4])
    graphics2d.beginWorld(camera)
end

-- Every frame drifts by a fraction of a pixel, so its edges cross texel boundaries while it draws.
function EdgeCases:draw(area)
    local scale = EdgeCases.borderScale
    local drift = (self.time * 0.37) % 1
    local margin, gap = 24, 24
    local width = (area.width - margin * 2 - gap * 2) / 3
    local height = math.max(260, (area.height - margin * 2 - gap) * 0.5)

    local room = EdgeCases.card({margin, margin, width, height}, 'Stretched', 'At a border scale of 2.7 and a size that changes, at fractions of a pixel.')
    graphics2d.drawNineSlice(self.stretched, {room[1] + drift, room[2] + drift, room[3] * (0.75 + 0.25 * math.sin(self.time)), room[4] * (0.8 + 0.2 * math.cos(self.time * 0.8))}, nil, {borderScale = scale})

    room = EdgeCases.card({margin + width + gap, margin, width, height}, 'Tiled', 'At sizes off the grid of its tiles, ending each edge on a part of a tile.')
    graphics2d.drawNineSlice(self.tiled, {room[1] + drift, room[2] + drift, room[3] * (0.8 + 0.2 * math.sin(self.time * 0.7)), room[4]}, nil, {borderScale = scale})

    local tinyRoom = EdgeCases.card({margin + (width + gap) * 2, margin, width, height}, 'Smaller than their borders', 'The borders shrink together until they meet, shown magnified.')

    local lower = margin + height + gap
    local lowerHeight = area.height - lower - margin
    room = EdgeCases.card({margin, lower, width, lowerHeight}, 'Two resolutions', 'A sheet of 8 texel pieces, and one of 16 at half the border scale, which must look alike.')
    local frameWidth = (room[3] - 24) / 2
    local frameHeight = math.min(110, room[4])
    graphics2d.drawNineSlice(self.stretched, {room[1] + drift, room[2] + drift, frameWidth, frameHeight}, nil, {borderScale = scale})
    graphics2d.drawNineSlice(self.sharp, {room[1] + frameWidth + 24 + drift, room[2] + drift, frameWidth, frameHeight}, nil, {borderScale = scale / 2})

    local checks = {margin + width + gap, lower, width * 2 + gap, lowerHeight}
    graphics2d.drawShape(checks, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})

    -- The small frames draw magnified, so their shrunk borders show, and the checks draw in their card, each through a camera of its own.
    local spacing = 12
    local span, tallest = (#EdgeCases.tiny - 1) * spacing + 2, 2
    for _, size in ipairs(EdgeCases.tiny) do
        span, tallest = span + size[1], math.max(tallest, size[2] + 2)
    end
    local zoom = math.min(3, tinyRoom[3] / span, tinyRoom[4] / tallest)
    local viewWidth, viewHeight = span * zoom, tallest * zoom
    self:look(self.tinyCamera, {tinyRoom[1] + (tinyRoom[3] - viewWidth) / 2, tinyRoom[2] + (tinyRoom[4] - viewHeight) / 2, viewWidth, viewHeight}, zoom)
    local left = drift
    for _, size in ipairs(EdgeCases.tiny) do
        graphics2d.drawNineSlice(self.tiled, {left, drift, size[1], size[2]}, nil, {borderScale = scale})
        left = left + size[1] + spacing
    end
    self:look(self.resultsCamera, checks, 1)
    self.results:draw(m.rect(0, 0, checks[3], checks[4]), 12)
end

return EdgeCases
