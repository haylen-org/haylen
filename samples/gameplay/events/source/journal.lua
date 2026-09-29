-- A log of the newest lines, each stamped with the frame it happened on, drawn in the stage with the newest line at the bottom.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Journal = haylen.class('Journal')

local kInk = '#FFE8EAF2'
local kFrame = '#FF7A8099'

function Journal:init(capacity)
    self.lines = collections.newRingBuffer(capacity or 40)
end

function Journal:add(text, color)
    self.lines:push({frame = haylen.frame(), text = text, color = color or kInk})
end

function Journal:clear()
    self.lines:clear()
end

-- Draws as many of the newest lines as fit in `height`, at `size` units per line. The stage clips lines that are too long.
function Journal:draw(x, y, height, size)
    local lineHeight = size * 1.4
    local lines = self.lines:values()
    local count = math.min(#lines, math.floor(height / lineHeight))
    for index = 1, count do
        local line = lines[#lines - count + index]
        local top = y + (index - 1) * lineHeight
        graphics2d.drawText(nil, tostring(line.frame), x + size * 3.2, top, {size = size, color = kFrame, anchor = {1, 0}})
        graphics2d.drawText(nil, line.text, x + size * 4, top, {size = size, color = line.color})
    end
end

return Journal
