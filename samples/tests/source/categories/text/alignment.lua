-- Alignment: each line of a wrapped block aligns left, center, right or fills the width, and the anchor places the block around its position. Rich text aligns paragraph by paragraph.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Alignment = haylen.class('Alignment', TextTest)

Alignment.code = [[
local font = assets.font('fonts/crimson-text-regular.ttf')
graphics2d.drawText(font, story, x, y, {size = 30, maxWidth = 400, align = 'fill'})
graphics2d.drawText(font, 'Anchored', x, y, {size = 44, anchor = {1, 1}})
graphics2d.drawText(font, 'Turned', x, y, {size = 44, anchor = {0.5, 0.5}, rotation = 0.3})
local rich = graphics2d.newRichText('[center]A centered paragraph.[/center]', {family = family, size = 30, maxWidth = 820})]]

Alignment.text = 'The ferry leaves the harbor at noon, and the keeper lights the beacon when the sun goes down over the western cliffs.'
Alignment.aligns = {{'left', 'Left'}, {'center', 'Centered'}, {'right', 'Right'}, {'fill', 'Filled to both edges'}}
Alignment.anchors = {{{0, 0}, 'Anchor at the top left'}, {{0.5, 0.5}, 'Anchor at the center'}, {{1, 1}, 'Anchor at the bottom right'}, {{0.5, 0.5}, 'Centered and turned', 0.3}}
Alignment.paragraphs = '[left]A paragraph at the left.[/left]\n[center]A centered paragraph.[/center]\n[right]And one at the right.[/right]'
Alignment.filled = '[p align=fill]A filled paragraph stretches the spaces of every wrapped line to both edges of the block, like the pages of a printed book, and leaves its last line at the start.[/p]'
Alignment.size = 30
Alignment.padding = 18
Alignment.gap = 36
Alignment.captionHeight = 42

function Alignment:init(entry)
    Alignment.super.init(self, entry)
    self.font = fonts.load('crimson')
    self.family = fonts.family('crimson')
end

function Alignment:enter()
    self:frame{code = Alignment.code, page = {1800, 700}, hint = 'Fill stretches the spaces of every wrapped line to both edges and leaves the last line of a paragraph at its start. The red marks show the position each anchored block is drawn at, and the last block also turns around it.'}
end

-- Lays the boxes out again for the width of the page, and keeps the heights the rows take.
function Alignment:resize(area)
    Alignment.super.resize(self, area)
    local layout, padding = self.layout, Alignment.padding
    self.boxWidth = (layout.width - Alignment.gap * 3) / 4
    local _, height = graphics2d.measureText(self.font, Alignment.text, {size = Alignment.size, maxWidth = self.boxWidth - padding * 2})
    self.boxHeight = height + padding * 2

    local halfWidth = (layout.width - Alignment.gap) / 2
    local options = {family = self.family, size = Alignment.size, maxWidth = halfWidth - padding * 2, color = Test.ink}
    self.paragraphs = graphics2d.newRichText(Alignment.paragraphs, options)
    self.filled = graphics2d.newRichText(Alignment.filled, options)
    local _, first = self.paragraphs:size()
    local _, second = self.filled:size()
    self.richWidth = halfWidth
    self.richHeight = math.max(first, second) + padding * 2
end

function Alignment.mark(x, y)
    graphics2d.drawLine(x - 14, y, x + 14, y, 2, '#C0FF6A6A', {layer = -1})
    graphics2d.drawLine(x, y - 14, x, y + 14, 2, '#C0FF6A6A', {layer = -1})
    graphics2d.drawCircle(x, y, 5, '#FFFF6A6A', {layer = 3})
end

function Alignment:draw(area)
    local layout, padding, caption = self.layout, Alignment.padding, Alignment.captionHeight
    local anchorHeight = 120
    local gap = (layout.height - caption * 3 - self.boxHeight - anchorHeight - self.richHeight) / 2

    local y = 0
    for index, align in ipairs(Alignment.aligns) do
        local x = (index - 1) * (self.boxWidth + Alignment.gap)
        TextTest.label(align[2], x + self.boxWidth / 2, y, {anchor = {0.5, 0}})
        TextTest.box({x, y + caption, self.boxWidth, self.boxHeight})
        graphics2d.drawText(self.font, Alignment.text, x + padding, y + caption + padding, {size = Alignment.size, maxWidth = self.boxWidth - padding * 2, align = align[1], color = Test.ink})
    end

    -- Each anchored block stands centered in its cell, so the mark shows where its anchor point lies on it.
    y = y + caption + self.boxHeight + gap
    local cell = layout.width / #Alignment.anchors
    for index, anchor in ipairs(Alignment.anchors) do
        local centerX, centerY = (index - 0.5) * cell, y + caption + anchorHeight / 2
        local width, height = graphics2d.measureText(self.font, 'Anchored', {size = 44})
        local x = centerX + (anchor[1][1] - 0.5) * width
        local markY = centerY + (anchor[1][2] - 0.5) * height
        TextTest.label(anchor[2], centerX, y, {anchor = {0.5, 0}})
        graphics2d.drawText(self.font, anchor[3] and 'Turned' or 'Anchored', x, markY, {size = 44, anchor = anchor[1], rotation = anchor[3] or 0, color = anchor[3] and '#FF7FCBF2' or '#FFFFE070'})
        Alignment.mark(x, markY)
    end

    y = y + caption + anchorHeight + gap
    local right = self.richWidth + Alignment.gap
    TextTest.label('Rich text paragraphs with their own alignment', 0, y)
    TextTest.box({0, y + caption, self.richWidth, self.richHeight})
    self.paragraphs:draw(padding, y + caption + padding)
    TextTest.label('A rich text paragraph filled to both edges', right, y)
    TextTest.box({right, y + caption, self.richWidth, self.richHeight})
    self.filled:draw(right + padding, y + caption + padding)
end

return Alignment
