-- The cart: the dishes the person picked from one restaurant, with their options and quantities. Every change emits "basketChanged" on the event bus.
local events = require('haylen.events')

local Catalog = require('data.catalog')

local Basket = {}

Basket.restaurant = nil
Basket.lines = {}
Basket.serviceRate = 0.05
Basket.next = 0

-- Adds a dish with its size, extras and quantity. A dish of another restaurant returns false, and the caller asks whether to start a new cart.
function Basket.add(dish, choice)
    if Basket.restaurant and Basket.restaurant ~= dish.restaurant then
        return false
    end
    Basket.restaurant = dish.restaurant
    local size = choice and choice.size or Catalog.sizes[1]
    local extras = choice and choice.extras or {}
    local unit = dish.price + size.extra
    local notes = {}
    if size.extra > 0 then
        notes[#notes + 1] = size.text
    end
    for _, extra in ipairs(extras) do
        unit = unit + extra.price
        notes[#notes + 1] = extra.text
    end
    local note = table.concat(notes, ', ')
    for _, line in ipairs(Basket.lines) do
        if line.dish == dish.id and line.note == note then
            line.quantity = line.quantity + (choice and choice.quantity or 1)
            Basket.changed()
            return true
        end
    end
    Basket.next = Basket.next + 1
    Basket.lines[#Basket.lines + 1] = {
        id = 'line-' .. Basket.next,
        dish = dish.id,
        name = dish.name,
        image = dish.image,
        unit = unit,
        note = note,
        quantity = choice and choice.quantity or 1,
    }
    Basket.changed()
    return true
end

function Basket.setQuantity(id, quantity)
    for index, line in ipairs(Basket.lines) do
        if line.id == id then
            if quantity <= 0 then
                table.remove(Basket.lines, index)
            else
                line.quantity = quantity
            end
            break
        end
    end
    if #Basket.lines == 0 then
        Basket.restaurant = nil
    end
    Basket.changed()
end

function Basket.clear()
    Basket.lines = {}
    Basket.restaurant = nil
    Basket.changed()
end

-- Fills the texts a cell of the cart shows, and tells the screens the cart changed.
function Basket.changed()
    for _, line in ipairs(Basket.lines) do
        line.total = Catalog.money(line.unit * line.quantity)
        line.caption = line.note ~= '' and line.note or Catalog.money(line.unit) .. ' each'
    end
    events.emit('basketChanged')
end

function Basket.count()
    local count = 0
    for _, line in ipairs(Basket.lines) do
        count = count + line.quantity
    end
    return count
end

function Basket.subtotal()
    local total = 0
    for _, line in ipairs(Basket.lines) do
        total = total + line.unit * line.quantity
    end
    return total
end

-- Returns the subtotal, the delivery fee, the service fee and the total with a tip and a faster delivery.
function Basket.totals(tip, priority)
    local subtotal = Basket.subtotal()
    local restaurant = Basket.restaurant and Catalog.restaurant(Basket.restaurant)
    local delivery = (restaurant and restaurant.fee or 0) + (priority and 2.99 or 0)
    local service = math.floor(subtotal * Basket.serviceRate * 100 + 0.5) / 100
    local tipAmount = math.floor(subtotal * (tip or 0) * 100 + 0.5) / 100
    return {subtotal = subtotal, delivery = delivery, service = service, tip = tipAmount, total = subtotal + delivery + service + tipAmount}
end

return Basket
