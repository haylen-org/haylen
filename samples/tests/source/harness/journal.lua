-- A log of the newest lines that a test draws on its stage, each stamped with the frame it happened on, with the newest line at the bottom.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')

local Journal = haylen.class('Journal')

function Journal:init(capacity)
    self.lines = collections.newRingBuffer(capacity or 40)
end

function Journal:add(text, color)
    self.lines:push({frame = haylen.frameIndex(), text = text, color = color or Test.ink})
end

function Journal:clear()
    self.lines:clear()
end

-- Draws as many of the newest lines as fit in `height`, at `size` units per line.
function Journal:draw(x, y, height, size)
    local lineHeight = size * 1.4
    local lines = self.lines:values()
    local count = math.min(#lines, math.floor(height / lineHeight))
    for index = 1, count do
        local line = lines[#lines - count + index]
        local top = y + (index - 1) * lineHeight
        graphics2d.drawText(nil, tostring(line.frame), x + size * 3.2, top, {size = size, color = Test.muted, anchor = {1, 0}, layer = 2})
        graphics2d.drawText(nil, line.text, x + size * 4, top, {size = size, color = line.color, layer = 2})
    end
end

return Journal
