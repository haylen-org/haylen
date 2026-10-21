-- The orders of the person. It is an autoload, so an order moves on through its stages on every screen: the warehouse packs it, the carrier ships it, the courier brings it and it arrives at the door. Every change of stage emits "orderChanged" and shows a notice.
local events = require('haylen.events')

local Random = require('random')
local Store = require('data.store')

local Orders = {}

Orders.list = {}
Orders.placed = 0
Orders.stages = {
    {id = 'ordered', title = 'Ordered', message = 'We have your order.', seconds = 5},
    {id = 'packed', title = 'Packed', message = 'Your order is packed at the warehouse.', seconds = 6},
    {id = 'shipped', title = 'Shipped', message = 'The carrier has your parcel.', seconds = 6},
    {id = 'outForDelivery', title = 'Out for delivery', message = 'The courier is on the way to you.', seconds = 8},
    {id = 'delivered', title = 'Delivered', message = 'Your parcel is at your door.'},
}
Orders.months = {'September', 'August', 'July', 'June'}

function Orders:start()
    local random = Random(91)
    for index = 1, 18 do
        local lines, total = {}, 0
        for line = 1, random:integer(1, 4) do
            local product = Store.products[random:integer(1, #Store.products)]
            lines[line] = {id = 'past-' .. index .. '-' .. line, type = 'line', name = product.name, options = Store.color(product.colors[1]).name, image = product.image, backdrop = product.backdrop, tint = product.tint, quantity = 1, total = Store.money(product.price)}
            total = total + product.price
        end
        local order = {
            id = 'past-' .. index,
            type = 'order',
            number = 'Order ' .. (52000 - index * 37),
            lines = lines,
            stage = #Orders.stages,
            total = total,
            date = string.format('%s %d', Orders.months[(index - 1) // 5 + 1], random:integer(1, 28)),
        }
        Orders.describe(order)
        self.list[index] = order
    end
end

-- Places the bag as a new order and returns it.
function Orders.place(lines, totals, express)
    Orders.placed = Orders.placed + 1
    local copies = {}
    for index, line in ipairs(lines) do
        copies[index] = {id = 'order-' .. Orders.placed .. '-' .. index, type = 'line', name = line.name, options = line.options, image = line.image, backdrop = line.backdrop, tint = line.tint, quantity = line.quantity, total = line.total}
    end
    local order = {
        id = 'order-' .. Orders.placed,
        type = 'order',
        number = 'Order ' .. (52010 + Orders.placed),
        lines = copies,
        total = totals.total,
        totals = totals,
        express = express,
        date = 'Today',
        stage = 1,
        elapsed = 0,
    }
    Orders.describe(order)
    table.insert(Orders.list, 1, order)
    events.emit('orderChanged', order)
    return order
end

function Orders.find(id)
    for _, order in ipairs(Orders.list) do
        if order.id == id then
            return order
        end
    end
end

-- Seconds from the order to the start of a stage.
function Orders.stageStart(stage)
    local start = 0
    for index = 1, stage - 1 do
        start = start + Orders.stages[index].seconds
    end
    return start
end

-- Fills the texts and the picture a row of the orders shows.
function Orders.describe(order)
    local count = 0
    for _, line in ipairs(order.lines) do
        count = count + line.quantity
    end
    local first = order.lines[1]
    order.image, order.backdrop, order.tint = first.image, first.backdrop, first.tint
    order.title = order.number
    order.caption = string.format('%s · %d %s · %s', order.date, count, count == 1 and 'item' or 'items', Store.money(order.total))
    order.status = Orders.stages[order.stage].title
    order.tone = order.stage == #Orders.stages and 'success' or 'neutral'
end

function Orders:update(dt)
    for _, order in ipairs(self.list) do
        if order.elapsed and order.stage < #Orders.stages then
            order.elapsed = order.elapsed + dt
            local stage = order.stage
            while stage < #Orders.stages and order.elapsed >= Orders.stageStart(stage + 1) do
                stage = stage + 1
            end
            if stage ~= order.stage then
                order.stage = stage
                Orders.describe(order)
                events.emit('orderChanged', order)
                events.emit('notice', order.number .. ': ' .. Orders.stages[stage].message)
            end
        end
    end
end

return Orders
