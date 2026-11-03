-- Edge cases: the painted wooden frame cut into nine pieces apart in a sheet whose gaps are magenta, so any texel read outside a piece shows as a pink seam. The frames draw with linear filtering at fractional places and border scales, tiled at sizes that are not a multiple of their tiles, smaller than their own borders, and from the same frame painted at a higher resolution at a smaller border scale, which must look the same. The layout of each case is checked to cover its rectangle exactly with quads that stay inside their pieces.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local Results = require('harness.results')
local Test = require('harness.test')

local EdgeCases = haylen.class('EdgeCases', Test)

-- The frame of 96 pixels with borders of 19 in the atlas of the trimmed slices, and the same frame painted at 256 pixels with borders of 50.
EdgeCases.small = {path = 'nine-slice/cards.png', source = {2, 2, 96, 96}, border = 19, gap = 2}
EdgeCases.large = {path = 'nine-slice/parchment.png', source = {0, 0, 256, 256}, border = 50, gap = 4}
EdgeCases.borderScale = 1.15
EdgeCases.tiny = {{36, 28}, {22, 40}, {80, 18}, {12, 12}}
EdgeCases.code = [[
local frame = graphics2d.newNineSlice(sheet, {pieces = pieces, fill = 'tile'})
graphics2d.drawNineSlice(frame, {10.3, 20.6, 203.3, 131.9}, nil, {borderScale = 1.15})
local quads = frame:layout({0, 0, 24, 18}, 1.15)  -- Borders shrink until they meet.]]

-- Cuts a painted frame into its nine pieces, set apart in a sheet with magenta gaps, and returns the texture and the nine regions row by row.
function EdgeCases.sheet(frame)
    local painted = assets.load(frame.path, 'image')
    local border, gap, source = frame.border, frame.gap, frame.source
    local middle = source[3] - border * 2
    local spans = {{0, border}, {border, middle}, {border + middle, border}}
    local size = source[3] + gap * 2
    local image = graphics.newImage(size, size, '#FFFF00FF')
    local pieces = {}
    for row = 1, 3 do
        for column = 1, 3 do
            local x, y = spans[column][1] + (column - 1) * gap, spans[row][1] + (row - 1) * gap
            image:blit(painted, x, y, {source = {source[1] + spans[column][1], source[2] + spans[row][1], spans[column][2], spans[row][2]}})
            pieces[#pieces + 1] = {x, y, spans[column][2], spans[row][2]}
        end
    end
    -- The sheet keeps no mipmaps, whose smaller copies would average the magenta gaps into the pieces, so a pink seam shows only what the sampling of a piece reads.
    image:bleed()
    return graphics.newTexture(image, {filter = 'linear', mipmaps = false}), pieces
end

function EdgeCases:enter()
    local texture, pieces = EdgeCases.sheet(EdgeCases.small)
    local large, largePieces = EdgeCases.sheet(EdgeCases.large)
    self.pieces = pieces
    self.stretched = graphics2d.newNineSlice(texture, {pieces = pieces})
    self.tiled = graphics2d.newNineSlice(texture, {pieces = pieces, fill = 'tile'})
    self.sharp = graphics2d.newNineSlice(large, {pieces = largePieces})
    self.time = 0
    self.results = Results(self.entry.code)
    self:check()
    self.tinyCamera = graphics2d.newCamera()
    self.tinyCamera.anchor = 'topLeft'
    self:frame{
        code = EdgeCases.code,
        hint = 'No pink seam may show anywhere, the tiled frame ends every edge on a whole or partial tile, the small frames shrink their borders until they meet, and the two frames of the last card look alike.',
        controls = {self.results:view('Checks')},
    }
    self.results:show(self.gui)
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
        local detail = exact and inside and string.format('Its %d quads cover the frame exactly and read only their own pieces.', #quads) or string.format('Its %d quads cover %.1f of %.1f square units%s.', #quads, covered, area, inside and '' or ', and one reads outside its piece')
        self.results:set(case.key, exact and inside and 'pass' or 'fail', case.name, detail)
    end

    local corner = self.stretched:layout({0, 0, 24, 18}, scale)[1].area
    local meets = corner.width == 12 and corner.height == 9
    self.results:set('meet', meets and 'pass' or 'fail', 'Shrunk borders', string.format('The corner of a frame of 24 by 18 with borders of %.1f shrinks to %g by %g.', EdgeCases.small.border * scale, corner.width, corner.height))
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

-- Draws a case in a card with its title and a line of what it must show, and returns the rectangle left for its frames.
function EdgeCases.card(rect, title, text)
    graphics2d.drawShape(rect, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    graphics2d.drawText(nil, title, rect[1] + 20, rect[2] + 16, {size = 26, color = Test.ink, layer = 2})
    graphics2d.drawText(nil, text, rect[1] + 20, rect[2] + 50, {size = 20, color = Test.muted, maxWidth = rect[3] - 40, wrap = false, minScale = 0.8, layer = 2})
    return {rect[1] + 20, rect[2] + 88, rect[3] - 40, rect[4] - 106}
end

-- Looks at a part of the stage through a camera of its own whose view starts at the corner of `rect`, so what follows draws there at `zoom` times its size.
function EdgeCases:look(camera, rect, zoom)
    camera.zoom = {zoom, zoom}
    camera.viewport = m.rect(self.stage.x + rect[1], self.stage.y + rect[2], rect[3], rect[4])
    graphics2d.beginWorld(camera)
end

-- The four cases fill the play area in cards of two by two. Every frame drifts by a fraction of a pixel, so its edges cross texel boundaries while it draws.
function EdgeCases:draw(area)
    local scale = EdgeCases.borderScale
    local drift = (self.time * 0.37) % 1
    local margin, gap = 24, 24
    local width = (area.width - margin * 2 - gap) / 2
    local height = (area.height - margin * 2 - gap) / 2
    local lower = margin + height + gap

    local room = EdgeCases.card({margin, margin, width, height}, 'Stretched', 'A border scale of 1.15 at fractions of a pixel.')
    graphics2d.drawNineSlice(self.stretched, {room[1] + drift, room[2] + drift, room[3] * (0.7 + 0.3 * math.sin(self.time)), room[4] * (0.75 + 0.25 * math.cos(self.time * 0.8))}, nil, {borderScale = scale})

    room = EdgeCases.card({margin + width + gap, margin, width, height}, 'Tiled', 'Every edge ends on a whole or partial tile.')
    graphics2d.drawNineSlice(self.tiled, {room[1] + drift, room[2] + drift, room[3] * (0.75 + 0.25 * math.sin(self.time * 0.7)), room[4]}, nil, {borderScale = scale})

    room = EdgeCases.card({margin + width + gap, lower, width, height}, 'Two resolutions', 'Painted at 96 and 256 pixels, they look alike.')
    local frameWidth = (room[3] - 24) / 2
    local small, large = EdgeCases.small, EdgeCases.large
    graphics2d.drawNineSlice(self.stretched, {room[1] + drift, room[2] + drift, frameWidth, room[4]}, nil, {borderScale = scale})
    graphics2d.drawNineSlice(self.sharp, {room[1] + frameWidth + 24 + drift, room[2] + drift, frameWidth, room[4]}, nil, {borderScale = scale * small.border / large.border})

    -- The small frames draw magnified through a camera of their own, so their shrunk borders show.
    local tinyRoom = EdgeCases.card({margin, lower, width, height}, 'Smaller than their borders', 'The borders shrink until they meet, magnified.')
    local spacing = 16
    local span, tallest = (#EdgeCases.tiny - 1) * spacing + 2, 2
    for _, size in ipairs(EdgeCases.tiny) do
        span, tallest = span + size[1], math.max(tallest, size[2] + 2)
    end
    local zoom = math.min(3, tinyRoom[3] / span, tinyRoom[4] / tallest)
    local viewWidth, viewHeight = span * zoom, tallest * zoom
    self:look(self.tinyCamera, {tinyRoom[1] + (tinyRoom[3] - viewWidth) / 2, tinyRoom[2] + (tinyRoom[4] - viewHeight) / 2, viewWidth, viewHeight}, zoom)
    local left = drift
    for _, size in ipairs(EdgeCases.tiny) do
        graphics2d.drawNineSlice(self.tiled, {left, (tallest - size[2]) / 2 + drift, size[1], size[2]}, nil, {borderScale = scale})
        left = left + size[1] + spacing
    end
end

return EdgeCases
