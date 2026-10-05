-- Slices of trimmed atlas frames: an atlas packer cuts the transparent margin off a frame and records the offset of what it kept, while the bounds of a slice stay in the coordinates of the whole frame. The slice must read the same pixels as the frame it belongs to, here a framed panel trimmed out of a canvas with a margin of 16 and packed next to a checkered frame that any wrong offset reads.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Results = require('harness.results')
local Test = require('harness.test')

local TrimmedSlices = haylen.class('TrimmedSlices', Test)

TrimmedSlices.preview = 2
TrimmedSlices.code = [[
local atlas = assets.load('nine-slice/cards.json', 'atlas')
local slice = atlas:slice('card')  -- Bounds 16, 16, 96 x 96 on a frame trimmed by 16 on every side.
local frame = atlas:frame('card')  -- The pixels the slice must read: frame.source.]]

function TrimmedSlices:enter()
    self.atlas = assets.load('nine-slice/cards.json', 'atlas', {filter = 'linear'})
    self.fromAtlas = self.atlas:slice('card')
    self.frameOfCard = self.atlas:frame('card')
    local source = self.frameOfCard.source
    self.byHand = graphics2d.newNineSlice(self.atlas.texture, {source = {source.x, source.y, source.width, source.height}, borders = {28, 28, 28, 28}})
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

-- Draws the atlas with the outline of every frame and of the slice, the panel from the slice of the atlas and the panel built from the frame.
function TrimmedSlices:draw(area)
    local scale = TrimmedSlices.preview
    local texture = self.atlas.texture
    local x, y = 30, 40
    graphics2d.draw(texture, x, y, {pivotX = 0, pivotY = 0, scaleX = scale, scaleY = scale})
    for _, name in ipairs(self.atlas:frameNames()) do
        local source = self.atlas:source(name)
        graphics2d.drawRectOutline({x + source.x * scale, y + source.y * scale, source.width * scale, source.height * scale}, 2, Test.accent, {layer = 1})
    end
    local first, last = self.fromAtlas.pieces[1], self.fromAtlas.pieces[9]
    local bottom = last.y + last.height
    graphics2d.drawRectOutline({x + first.x * scale, y + first.y * scale, (last.x + last.width - first.x) * scale, (bottom - first.y) * scale}, 3, Test.warm, {layer = 2})
    Test.caption('The atlas, with its frames in blue and the slice in orange', x, y + math.max(texture.height, bottom) * scale + 16, {size = 22})

    local left = x + texture.width * scale + 60
    local width = (area.width - left - 60) / 2
    local height = math.min(260, area.height - 200)
    graphics2d.drawNineSlice(self.fromAtlas, {left, y, width, height})
    Test.caption('From "atlas:slice"', left, y + height + 16, {size = 22, color = Test.ink})
    graphics2d.drawNineSlice(self.byHand, {left + width + 30, y, width, height})
    Test.caption('Built from the frame', left + width + 30, y + height + 16, {size = 22, color = Test.ink})

    self.results:draw(area, y + height + 70)
end

return TrimmedSlices
