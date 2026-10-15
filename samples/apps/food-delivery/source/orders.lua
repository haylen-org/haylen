-- The orders of the person, past and active. It is an autoload, so an order moves on through its stages on every screen: the restaurant confirms it, cooks it, a courier picks it up and rides the route on the city map to the door. Every change of stage emits "orderChanged" and shows a notice.
local events = require('haylen.events')

local Account = require('data.account')
local Catalog = require('data.catalog')
local City = require('city')
local Random = require('random')

local Orders = {}

Orders.city = City(11)
Orders.active = {}
Orders.past = {}
Orders.placed = 0
-- Seconds of each stage before the courier rides, short so the whole order plays out in about a minute.
Orders.stages = {
    {id = 'confirmed', title = 'Order confirmed', message = 'The restaurant has your order.', seconds = 5},
    {id = 'preparing', title = 'Preparing your food', message = 'The kitchen is cooking your order.', seconds = 12},
    {id = 'pickup', title = 'Courier at the restaurant', message = 'Your courier is picking up the bag.', seconds = 5},
    {id = 'onTheWay', title = 'On the way', message = 'Your courier is riding to you.'},
    {id = 'delivered', title = 'Delivered', message = 'Enjoy your meal.'},
}
-- Map units a courier rides per second.
Orders.speed = 120
Orders.couriers = {
    {name = 'Theo Martins', vehicle = 'Red scooter', plate = 'FD 2048', rating = '4.9'},
    {name = 'Ines Kowalski', vehicle = 'Cargo bike', plate = 'Bike 17', rating = '4.8'},
    {name = 'Amara Okafor', vehicle = 'Blue scooter', plate = 'FD 7731', rating = '5.0'},
    {name = 'Luca Bianchi', vehicle = 'Electric bike', plate = 'Bike 42', rating = '4.7'},
}
Orders.months = {'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'}

function Orders:start()
    local random = Random(29)
    for index = 1, 36 do
        local restaurant = Catalog.restaurants[random:integer(1, #Catalog.restaurants)]
        local count = random:integer(1, 5)
        local total = random:integer(14, 72) + 0.5
        local cancelled = random:chance(0.08)
        self.past[index] = {
            id = 'past-' .. index,
            type = 'order',
            restaurant = restaurant.id,
            name = restaurant.name,
            image = restaurant.image,
            summary = string.format('%s · %d %s · %s', Orders.months[12 - (index // 4) % 12], count, count == 1 and 'item' or 'items', Catalog.money(total)),
            status = cancelled and 'Cancelled' or 'Delivered',
            tone = cancelled and 'danger' or 'success',
            action = 'Reorder',
        }
    end
end

-- Places the cart as a new order and returns it. The route runs from the corner of the restaurant to the corner of the address.
function Orders.place(lines, restaurant, totals, addressId, payment)
    Orders.placed = Orders.placed + 1
    local address = Account.findAddress(addressId)
    local route = Orders.city:route(restaurant.block[1], restaurant.block[2], address.corner[1], address.corner[2])
    local count = 0
    for _, line in ipairs(lines) do
        count = count + line.quantity
    end
    local order = {
        id = 'order-' .. Orders.placed,
        type = 'order',
        number = 4100 + Orders.placed * 7,
        restaurant = restaurant.id,
        name = restaurant.name,
        image = restaurant.image,
        lines = lines,
        totals = totals,
        count = count,
        address = address,
        payment = payment,
        route = route,
        length = City.routeLength(route),
        courier = Orders.couriers[(Orders.placed - 1) % #Orders.couriers + 1],
        elapsed = 0,
        stage = 1,
        ridden = 0,
        action = 'Track',
    }
    order.rideSeconds = order.length / Orders.speed
    Orders.describe(order)
    table.insert(Orders.active, 1, order)
    events.emit('orderChanged', order)
    return order
end

function Orders.find(id)
    for _, list in ipairs({Orders.active, Orders.past}) do
        for _, order in ipairs(list) do
            if order.id == id then
                return order
            end
        end
    end
end

-- Returns the minutes left until the door, counting a minute of the app as a second of the order.
function Orders.minutesLeft(order)
    local left = 0
    for index = order.stage, #Orders.stages - 2 do
        left = left + Orders.stages[index].seconds
    end
    if order.stage <= 3 then
        left = left - (order.elapsed - Orders.stageStart(order, order.stage)) + order.rideSeconds
    elseif order.stage == 4 then
        left = (order.length - order.ridden) / Orders.speed
    end
    return math.max(1, math.ceil(left / 2))
end

function Orders.stageStart(order, stage)
    local start = 0
    for index = 1, stage - 1 do
        start = start + (Orders.stages[index].seconds or order.rideSeconds)
    end
    return start
end

-- Fills the texts a row of the orders list shows.
function Orders.describe(order)
    local stage = Orders.stages[order.stage]
    order.summary = string.format('Order %d · %d %s · %s', order.number, order.count, order.count == 1 and 'item' or 'items', Catalog.money(order.totals.total))
    order.status = stage.title
    order.tone = order.stage == #Orders.stages and 'success' or 'accent'
end

function Orders:update(dt)
    for index = #self.active, 1, -1 do
        local order = self.active[index]
        order.elapsed = order.elapsed + dt
        local stage = order.stage
        while stage < #Orders.stages and order.elapsed >= Orders.stageStart(order, stage + 1) do
            stage = stage + 1
        end
        if Orders.stages[order.stage].id == 'onTheWay' or Orders.stages[stage].id == 'onTheWay' then
            order.ridden = math.min(order.length, math.max(0, order.elapsed - Orders.stageStart(order, 4)) * Orders.speed)
        end
        if stage ~= order.stage then
            order.stage = stage
            Orders.describe(order)
            if stage == #Orders.stages then
                order.action = 'Reorder'
                table.remove(self.active, index)
                table.insert(self.past, 1, order)
            end
            events.emit('orderChanged', order)
            events.emit('notice', Orders.stages[stage].title .. '. ' .. Orders.stages[stage].message)
        end
    end
end

return Orders
