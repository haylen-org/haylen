-- Slices of trimmed atlas frames: an atlas packer cuts the transparent margin off a frame and records the offset of what it kept, while the bounds of a slice stay in the coordinates of the whole frame. The slice must read the same pixels as the frame it belongs to, here a wooden panel trimmed out of a canvas with a margin of 16 and packed next to a checkered frame that any wrong offset reads.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Results = require('harness.results')
local Test = require('harness.test')

local TrimmedSlices = haylen.class('TrimmedSlices', Test)

TrimmedSlices.preview = 2
-- The panels draw their borders larger than the 19 pixels of the small frame, so they keep its proportions at their size.
TrimmedSlices.borderScale = 2.5
TrimmedSlices.code = [[
local atlas = assets.load('nine-slice/cards.json', 'atlas')
local slice = atlas:slice('card')  -- Bounds 16, 16, 96 by 96 on a frame trimmed by 16 on every side.
local frame = atlas:frame('card')  -- The pixels the slice must read: frame.source.]]

function TrimmedSlices:enter()
    self.atlas = assets.load('nine-slice/cards.json', 'atlas', {filter = 'linear'})
    self.fromAtlas = self.atlas:slice('card')
    self.frameOfCard = self.atlas:frame('card')
    local source = self.frameOfCard.source
    self.byHand = graphics2d.newNineSlice(self.atlas.texture, {source = {source.x, source.y, source.width, source.height}, borders = {19, 19, 19, 19}})
    self.results = Results(self.entry.code)
    self:check()
    self:frame{code = TrimmedSlices.code, hint = 'Both panels must look the same. A slice that ignores the trim offset reads the checkered frame next to the panel.'}
end

-- Compares the first and the last piece of the slice with the rectangle of the frame in the texture.
function TrimmedSlices:check()
    local frame = self.frameOfCard
    local source, offset = frame.source, frame.offset
    local first, last = self.fromAtlas.pieces[1], self.fromAtlas.pieces[9]
    local starts = first.x == source.x and first.y == source.y
    self.results:set('start', starts and 'pass' or 'fail', 'Top-left piece', string.format('Starts at %g, %g of the texture, and the frame starts at %g, %g with a trim offset of %g, %g.', first.x, first.y, source.x, source.y, offset.x, offset.y))
    local right, bottom = last.x + last.width, last.y + last.height
    local ends = right == source.x + source.width and bottom == source.y + source.height
    self.results:set('end', ends and 'pass' or 'fail', 'Bottom-right piece', string.format('Ends at %g, %g of the texture, and the frame ends at %g, %g.', right, bottom, source.x + source.width, source.y + source.height))
end

function TrimmedSlices:update(dt)
    TrimmedSlices.super.update(self, dt)
    self:status(self.results:summary())
end

-- Draws the atlas with the outline of every frame and of the slice, the panel from the slice of the atlas and the panel built from the frame, in two columns of the same margins.
function TrimmedSlices:draw(area)
    local margin = 40
    local scale = TrimmedSlices.preview
    local texture = self.atlas.texture
    local x, y = margin, margin
    local atlasWidth = texture.width * scale
    graphics2d.drawShape({x - 12, y - 12, atlasWidth + 24, texture.height * scale + 24}, {radius = 14, color = '#FF151A2A'})
    graphics2d.draw(texture, x, y, {pivotX = 0, pivotY = 0, scaleX = scale, scaleY = scale, layer = 1})
    for _, name in ipairs(self.atlas:frameNames()) do
        local source = self.atlas:source(name)
        graphics2d.drawRectOutline({x + source.x * scale, y + source.y * scale, source.width * scale, source.height * scale}, 2, Test.accent, {layer = 2})
    end
    local first, last = self.fromAtlas.pieces[1], self.fromAtlas.pieces[9]
    local bottom = last.y + last.height
    graphics2d.drawRectOutline({x + first.x * scale, y + first.y * scale, (last.x + last.width - first.x) * scale, (bottom - first.y) * scale}, 3, Test.warm, {layer = 3})
    Test.caption('The atlas: its frames in blue, and the slice in orange.', x, y + texture.height * scale + 28, {size = 22, color = Test.ink, maxWidth = atlasWidth})

    local left = x + atlasWidth + 60
    local gap = 30
    local width = (area.width - left - margin - gap) / 2
    local height = math.min(260, area.height - 260)
    graphics2d.drawNineSlice(self.fromAtlas, {left, y, width, height}, nil, {borderScale = TrimmedSlices.borderScale})
    Test.caption('From "atlas:slice"', left + width / 2, y + height + 14, {size = 24, color = Test.ink, anchor = {0.5, 0}})
    graphics2d.drawNineSlice(self.byHand, {left + width + gap, y, width, height}, nil, {borderScale = TrimmedSlices.borderScale})
    Test.caption('Built from the frame', left + width + gap + width / 2, y + height + 14, {size = 24, color = Test.ink, anchor = {0.5, 0}})

    self.results:draw(area, y + height + 70)
end

return TrimmedSlices
