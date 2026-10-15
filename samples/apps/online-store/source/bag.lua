-- The shopping bag: the products with their color, size and quantity, and a promo code. Every change emits "bagChanged" on the event bus.
local events = require('haylen.events')

local Store = require('data.store')

local Bag = {}

Bag.lines = {}
Bag.next = 0
Bag.codes = {WELCOME10 = 0.1, SPRING20 = 0.2}
Bag.freeShippingFrom = 75

function Bag.add(product, colorId, size, quantity)
    for _, line in ipairs(Bag.lines) do
        if line.product == product.id and line.color == colorId and line.size == size then
            line.quantity = line.quantity + quantity
            Bag.changed()
            return
        end
    end
    Bag.next = Bag.next + 1
    local color = Store.color(colorId)
    Bag.lines[#Bag.lines + 1] = {
        id = 'line-' .. Bag.next,
        type = 'line',
        product = product.id,
        name = product.name,
        image = product.image,
        backdrop = product.backdrop,
        tint = color.value,
        color = colorId,
        size = size,
        options = size == 'One size' and color.name or color.name .. ' · Size ' .. size,
        unit = product.price,
        quantity = quantity,
    }
    Bag.changed()
end

function Bag.setQuantity(id, quantity)
    for index, line in ipairs(Bag.lines) do
        if line.id == id then
            if quantity <= 0 then
                table.remove(Bag.lines, index)
            else
                line.quantity = quantity
            end
            break
        end
    end
    Bag.changed()
end

function Bag.clear()
    Bag.lines = {}
    Bag.code = nil
    Bag.changed()
end

-- Applies a promo code and returns whether the store knows it.
function Bag.apply(code)
    code = string.upper(code)
    if Bag.codes[code] then
        Bag.code = code
        Bag.changed()
        return true
    end
    return false
end

function Bag.changed()
    for _, line in ipairs(Bag.lines) do
        line.total = Store.money(line.unit * line.quantity)
    end
    events.emit('bagChanged')
end

function Bag.count()
    local count = 0
    for _, line in ipairs(Bag.lines) do
        count = count + line.quantity
    end
    return count
end

-- Returns the subtotal, the discount of the code, the shipping and the total, with the express shipping when asked.
function Bag.totals(express)
    local subtotal = 0
    for _, line in ipairs(Bag.lines) do
        subtotal = subtotal + line.unit * line.quantity
    end
    local discount = Bag.code and math.floor(subtotal * Bag.codes[Bag.code] * 100 + 0.5) / 100 or 0
    local shipping = express and 9.99 or ((subtotal - discount >= Bag.freeShippingFrom or subtotal == 0) and 0 or 5.99)
    return {subtotal = subtotal, discount = discount, shipping = shipping, total = subtotal - discount + shipping}
end

return Bag
