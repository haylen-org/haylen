-- A sequence diagram of the last messages between two sides, such as a client and a server, which the network tests of the category draw under their checks.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')

local Exchange = haylen.class('Exchange')

Exchange.limit = 6
Exchange.rowHeight = 30
Exchange.colors = {Test.accent, Test.green}

-- Takes the names of the left and the right side.
function Exchange:init(left, right)
    self.sides = {left, right}
    self.messages = {}
end

-- Adds a message that side 1 sends to side 2, or side 2 to side 1.
function Exchange:add(from, text)
    self.messages[#self.messages + 1] = {from = from, text = text}
    if #self.messages > Exchange.limit then
        table.remove(self.messages, 1)
    end
end

function Exchange:clear()
    self.messages = {}
end

-- Returns `data` as it is when it is printable text, and as hexadecimal bytes otherwise.
function Exchange.printable(data)
    if data:match('^[%g ]*$') then
        return data
    end
    return (data:gsub('.', function(byte) return string.format('%02X ', byte:byte()) end))
end

function Exchange:height()
    return 40 + Exchange.limit * Exchange.rowHeight
end

function Exchange:draw(left, right, top)
    local xs = {left + 90, right - 90}
    local bottom = top + self:height()
    for side = 1, 2 do
        Test.caption(self.sides[side], xs[side], top, {anchor = {0.5, 0}, color = Test.ink})
        graphics2d.drawLine(xs[side], top + 32, xs[side], bottom, 2, Test.line)
    end

    local y = top + 32
    for _, message in ipairs(self.messages) do
        y = y + Exchange.rowHeight
        local from, to = xs[message.from], xs[3 - message.from]
        local direction = to > from and 1 or -1
        local color = Exchange.colors[message.from]
        graphics2d.drawLine(from, y, to - direction * 10, y, 2, color, {layer = 1})
        graphics2d.drawPolygon({{to, y}, {to - direction * 12, y - 6}, {to - direction * 12, y + 6}}, color, {layer = 1})
        Test.caption(message.text, (from + to) / 2, y - 3, {size = 16, color = Test.ink, anchor = {0.5, 1}})
    end
end

return Exchange
