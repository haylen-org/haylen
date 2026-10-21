-- A sequence diagram of the last messages between two sides, such as a client and a server, which the network tests of the category draw beside their checks.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')

local Exchange = haylen.class('Exchange')

Exchange.limit = 6
-- The rows of the messages share the height of the stage, from this height up to the most.
Exchange.rowHeight = {40, 72}
-- A binary message shows its size and this many of its first bytes.
Exchange.shownBytes = 6
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

-- Returns `data` as it is when it is printable text, and otherwise its size and its first bytes in hexadecimal.
function Exchange.printable(data)
    if data:match('^[%g ]*$') then
        return data
    end
    local bytes = {}
    for index = 1, math.min(#data, Exchange.shownBytes) do
        bytes[index] = string.format('%02X', data:byte(index))
    end
    return string.format('%d bytes, %s%s', #data, table.concat(bytes, ' '), #data > Exchange.shownBytes and ' …' or '')
end

-- Draws the two sides as named pills over their lifelines from `top` to `bottom`, and every message as an arrow from its side to the other, with its text over it.
function Exchange:draw(left, right, top, bottom)
    local xs = {left + 90, right - 90}
    local rowHeight = math.max(Exchange.rowHeight[1], math.min(Exchange.rowHeight[2], (bottom - top - 72) / Exchange.limit))
    for side = 1, 2 do
        local name = self.sides[side]
        local width = graphics2d.measureText(nil, name, {size = 20}) + 32
        graphics2d.drawShape({xs[side] - width / 2, top, width, 32}, {radius = 16, color = Exchange.colors[side], layer = 2})
        graphics2d.drawText(nil, name, xs[side], top + 16, {size = 20, color = '#FF101418', anchor = {0.5, 0.5}, layer = 3})
        graphics2d.drawShape({xs[side] - 1.5, top + 32, 3, bottom - top - 32}, {radius = 1.5, color = Test.line})
    end

    local y = top + 32
    for _, message in ipairs(self.messages) do
        y = y + rowHeight
        local from, to = xs[message.from], xs[3 - message.from]
        local direction = to > from and 1 or -1
        local color = Exchange.colors[message.from]
        graphics2d.drawLine(from, y, to - direction * 10, y, 3, color, {layer = 1})
        graphics2d.drawPolygon({{to, y}, {to - direction * 14, y - 7}, {to - direction * 14, y + 7}}, color, {layer = 1})
        graphics2d.drawText(nil, message.text, (from + to) / 2, y - 5, {size = 16, color = Test.ink, anchor = {0.5, 1}, maxWidth = math.abs(to - from) - 24, wrap = false, minScale = 0.7, layer = 2})
    end
end

return Exchange
