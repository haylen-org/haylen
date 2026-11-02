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
    self:frame{code = TrimmedSlices.code, hint = 'Both panels must look the same. A slice that ignores the trim offset reads the checkered frame next to the panel.', controls = {self.results:view('Checks')}}
    self.results:show(self.gui)
end

-- Compares the first and the last piece of the slice with the rectangle of the frame in the texture.
function TrimmedSlices:check()
    local frame = self.frameOfCard
    local source, offset = frame.source, frame.offset
    local first, last = self.fromAtlas.pieces[1], self.fromAtlas.pieces[9]
    local starts = first.x == source.x and first.y == source.y
    self.results:set('start', starts and 'pass' or 'fail', 'Top-left piece', string.format('It starts at %g, %g of the texture, where the frame starts after a trim of %g pixels.', first.x, first.y, offset.x))
    local right, bottom = last.x + last.width, last.y + last.height
    local ends = right == source.x + source.width and bottom == source.y + source.height
    self.results:set('end', ends and 'pass' or 'fail', 'Bottom-right piece', string.format('It ends at %g, %g of the texture, where the frame ends.', right, bottom))
end

function TrimmedSlices:update(dt)
    TrimmedSlices.super.update(self, dt)
    self:status(self.results:summary())
end

-- Draws a line of the legend: a stroke of the color of an outline and what it marks.
function TrimmedSlices.legend(color, text, x, y, width)
    graphics2d.drawShape({x, y + 9, 34, 6}, {radius = 3, color = color})
    Test.caption(text, x + 48, y, {size = 22, color = Test.ink, maxWidth = width - 48})
end

-- Draws the atlas on the left with the outline of every frame, and the slice just outside the frame it shares its bounds with, over a legend, and on the right the panel from the slice of the atlas over the panel built from the frame.
function TrimmedSlices:draw(area)
    local margin = 40
    local scale = TrimmedSlices.preview
    local texture = self.atlas.texture
    local x, y = margin, margin
    local atlasWidth = texture.width * scale
    graphics2d.drawShape({x - 14, y - 14, atlasWidth + 28, texture.height * scale + 28}, {radius = 14, color = '#FF151A2A'})
    graphics2d.draw(texture, x, y, {pivotX = 0, pivotY = 0, scaleX = scale, scaleY = scale, layer = 1})
    for _, name in ipairs(self.atlas:frameNames()) do
        local source = self.atlas:source(name)
        graphics2d.drawRectOutline({x + source.x * scale, y + source.y * scale, source.width * scale, source.height * scale}, 2, Test.accent, {layer = 2})
    end
    local first, last = self.fromAtlas.pieces[1], self.fromAtlas.pieces[9]
    local slice = {x + first.x * scale - 6, y + first.y * scale - 6, (last.x + last.width - first.x) * scale + 12, (last.y + last.height - first.y) * scale + 12}
    graphics2d.drawRectOutline(slice, 3, Test.warm, {layer = 3})
    local legendTop = y + texture.height * scale + 40
    TrimmedSlices.legend(Test.accent, 'The frames the packer trimmed', x, legendTop, atlasWidth)
    TrimmedSlices.legend(Test.warm, 'The slice "card", on the same pixels', x, legendTop + 64, atlasWidth)

    local left = x + atlasWidth + 70
    local width = area.width - left - margin
    local height = (area.height - margin * 2 - 100) / 2
    graphics2d.drawNineSlice(self.fromAtlas, {left, y, width, height}, nil, {borderScale = TrimmedSlices.borderScale})
    Test.caption('From "atlas:slice"', left + width / 2, y + height + 12, {size = 24, color = Test.ink, anchor = {0.5, 0}})
    local lower = y + height + 60
    graphics2d.drawNineSlice(self.byHand, {left, lower, width, height}, nil, {borderScale = TrimmedSlices.borderScale})
    Test.caption('Built from the frame', left + width / 2, lower + height + 12, {size = 24, color = Test.ink, anchor = {0.5, 0}})
end

return TrimmedSlices
