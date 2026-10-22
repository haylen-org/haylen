-- A log of what happens while an audio test runs, drawn on a card: a title with the legend of its colors, the newest entries at the bottom with the time each one happened, and a line under them that says what brings the next one.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local AudioTest = require('categories.audio.audio-test')
local Test = require('harness.test')

local EventLog = haylen.class('EventLog')

EventLog.rowHeight = 40
EventLog.legendStyle = {size = 18, color = Test.muted, anchor = {1, 0.5}, layer = 1}
EventLog.hintStyle = {size = 19, color = Test.muted, maxWidth = 0, layer = 1}

-- Options: `title`, `hint`, the line under the entries, and `legend`, a list of `{color, text}` pairs.
function EventLog:init(options)
    self.title = options.title
    self.hint = options.hint
    self.legend = options.legend or {}
    self.entries = collections.newRingBuffer(24)
end

-- Adds an entry that happened `time` seconds after the test opened.
function EventLog:add(time, text, color)
    self.entries:push({time = AudioTest.clock(time), text = text, color = color})
end

function EventLog:draw(x, y, width, height)
    graphics2d.drawShape({x, y, width, height}, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    Test.caption(self.title, x + 24, y + 18, {size = 24, color = Test.ink, layer = 1})
    local right = x + width - 24
    for index = #self.legend, 1, -1 do
        local color, text = self.legend[index][1], self.legend[index][2]
        local textWidth = graphics2d.measureText(nil, text, EventLog.legendStyle)
        graphics2d.drawText(nil, text, right, y + 34, EventLog.legendStyle)
        graphics2d.drawCircle(right - textWidth - 16, y + 34, 7, color, {layer = 1})
        right = right - textWidth - 52
    end
    graphics2d.drawLine(x + 24, y + 66, x + width - 24, y + 66, 1, Test.line, {layer = 1})

    -- The hint keeps the bottom of the card, and the newest entries that fit fill the room above it.
    EventLog.hintStyle.maxWidth = width - 48
    local _, hintHeight = graphics2d.measureText(nil, self.hint, EventLog.hintStyle)
    local footer = y + height - hintHeight - 36
    graphics2d.drawLine(x + 24, footer, x + width - 24, footer, 1, Test.line, {layer = 1})
    graphics2d.drawText(nil, self.hint, x + 24, footer + 18, EventLog.hintStyle)

    local entries = self.entries:values()
    local top = y + 80
    local count = math.min(#entries, math.floor((footer - 8 - top) / EventLog.rowHeight))
    for index = 1, count do
        local entry = entries[#entries - count + index]
        local rowY = top + (index - 1) * EventLog.rowHeight
        Test.caption(entry.time, x + 24, rowY, {size = 19, layer = 1})
        graphics2d.drawCircle(x + 96, rowY + 12, 6, entry.color, {layer = 1})
        graphics2d.drawText(nil, entry.text, x + 116, rowY, {size = 20, color = Test.ink, maxWidth = width - 140, wrap = false, minScale = 0.7, layer = 1})
    end
end

return EventLog
