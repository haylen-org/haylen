-- Alignment: each line of a wrapped block aligns left, center, right or fills the width, and the anchor places the block around its position. Rich text aligns paragraph by paragraph.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Alignment = haylen.class('Alignment', TextTest)

Alignment.text = 'The ferry leaves the harbor at noon, and the keeper lights the beacon when the sun goes down over the western cliffs.'
Alignment.width = 400
Alignment.aligns = {'left', 'center', 'right', 'fill'}
Alignment.anchors = {{{0, 0}, '0, 0'}, {{0.5, 0.5}, '0.5, 0.5'}, {{1, 1}, '1, 1'}}

function Alignment:init(entry)
    Alignment.super.init(self, entry)
    self.font = fonts.load('crimson')
    self.rich = graphics2d.newRichText('[left]Left paragraph.[/left]\n[center]A centered paragraph.[/center]\n[right]And one at the right.[/right]\n[p align=fill]A filled paragraph wraps its lines to both edges of the block, like the pages of a book.[/p]', {family = fonts.family('crimson'), size = 30, maxWidth = 500})
end

function Alignment:enter()
    self:frame{hint = 'Fill stretches the spaces of every wrapped line to both edges and leaves the last line of a paragraph at the left. The crosses mark the position each anchored block is drawn at, and the last block also turns around it.'}
end

function Alignment.cross(x, y)
    graphics2d.drawLine(x - 16, y, x + 16, y, 3, '#FFFF6A6A', {layer = 1})
    graphics2d.drawLine(x, y - 16, x, y + 16, 3, '#FFFF6A6A', {layer = 1})
end

function Alignment:draw(area)
    local font, width = self.font, Alignment.width
    for index, align in ipairs(Alignment.aligns) do
        local x = (index - 1) * (width + 50)
        local _, height = graphics2d.measureText(font, Alignment.text, {size = 30, maxWidth = width, align = align})
        Test.caption('Align "' .. align .. '"', x, 0, {size = 22})
        graphics2d.drawRect({x, 34, width, height}, '#FF1F2638')
        graphics2d.drawText(font, Alignment.text, x, 34, {size = 30, maxWidth = width, align = align})
    end

    local y = 390
    for index, anchor in ipairs(Alignment.anchors) do
        local x = 120 + (index - 1) * 360
        Test.caption('Anchor ' .. anchor[2], x - 100, y - 60, {size = 22})
        graphics2d.drawText(font, 'Anchored', x, y + 60, {size = 44, anchor = anchor[1], color = '#FFFFE070'})
        Alignment.cross(x, y + 60)
    end
    local x = 1150
    Test.caption('Anchor 0.5, 0.5, rotation 0.3', x - 110, y - 60, {size = 22})
    graphics2d.drawText(font, 'Turned', x, y + 60, {size = 44, anchor = {0.5, 0.5}, rotation = 0.3, color = '#FF7FCBF2'})
    Alignment.cross(x, y + 60)

    Test.caption('Rich text paragraphs', 1340, y - 60, {size = 22})
    self.rich:draw(1340, y - 20)
end

return Alignment
