-- Alignment: each line of a wrapped block aligns left, center, right or fills the width, and the anchor places the block around its position. Rich text aligns paragraph by paragraph.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local fonts = require('fonts')
local sample = require('sample')

local Alignment = haylen.class('Alignment', sample.Test)

Alignment.hints = 'Fill stretches the spaces of every wrapped line to both edges and leaves the last line of a paragraph at the left. The crosses mark the position each anchored block is drawn at, and the last block also turns around it.'

local kText = 'The ferry leaves the harbor at noon, and the keeper lights the beacon when the sun goes down over the western cliffs.'
local kWidth = 400
local kAligns = {'left', 'center', 'right', 'fill'}
local kAnchors = {{{0, 0}, '0, 0'}, {{0.5, 0.5}, '0.5, 0.5'}, {{1, 1}, '1, 1'}}

function Alignment:init(entry)
    Alignment.super.init(self, entry)
    self.rich = graphics2d.newRichText('[left]Left paragraph.[/left]\n[center]A centered paragraph.[/center]\n[right]And one at the right.[/right]\n[p align=fill]A filled paragraph wraps its lines to both edges of the block, like the pages of a book.[/p]', {family = fonts.family('crimson'), size = 30, maxWidth = 500})
end

local function cross(x, y)
    graphics2d.drawLine(x - 16, y, x + 16, y, 3, '#FFFF6A6A')
    graphics2d.drawLine(x, y - 16, x, y + 16, 3, '#FFFF6A6A')
end

function Alignment:render()
    local stage = self:stage()
    if stage == nil then
        return
    end
    local font = fonts.get('crimson')
    graphics2d.beginScreen()
    for index, align in ipairs(kAligns) do
        local x = stage.x + (index - 1) * (kWidth + 50)
        local _, height = graphics2d.measureText(font, kText, {size = 30, maxWidth = kWidth, align = align})
        sample.caption("align = '" .. align .. "'", x, stage.y)
        graphics2d.drawRect({x, stage.y + 34, kWidth, height}, '#FF1F2638')
        graphics2d.drawText(font, kText, x, stage.y + 34, {size = 30, maxWidth = kWidth, align = align})
    end

    local y = stage.y + 330
    for index, anchor in ipairs(kAnchors) do
        local x = stage.x + 120 + (index - 1) * 360
        sample.caption('anchor ' .. anchor[2], x - 100, y - 60)
        graphics2d.drawText(font, 'Anchored', x, y + 60, {size = 44, anchor = anchor[1], color = '#FFFFE070'})
        cross(x, y + 60)
    end
    local x = stage.x + 1150
    sample.caption('anchor 0.5, 0.5, rotation 0.3', x - 110, y - 60)
    graphics2d.drawText(font, 'Turned', x, y + 60, {size = 44, anchor = {0.5, 0.5}, rotation = 0.3, color = '#FF7FCBF2'})
    cross(x, y + 60)

    sample.caption('rich text paragraphs', stage.x + 1340, y - 60)
    self.rich:draw(stage.x + 1340, y - 20)
end

return Alignment
