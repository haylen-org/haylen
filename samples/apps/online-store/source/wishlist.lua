-- The products the person saved with the heart, newest first. Every change emits "wishlistChanged" on the event bus.
local events = require('haylen.events')

local Wishlist = {}

Wishlist.list = {}
Wishlist.ids = {}

function Wishlist.has(product)
    return Wishlist.ids[product.id] == true
end

function Wishlist.toggle(product)
    if Wishlist.has(product) then
        Wishlist.ids[product.id] = nil
        for index, other in ipairs(Wishlist.list) do
            if other == product then
                table.remove(Wishlist.list, index)
                break
            end
        end
    else
        Wishlist.ids[product.id] = true
        table.insert(Wishlist.list, 1, product)
    end
    events.emit('wishlistChanged', product)
    return Wishlist.has(product)
end

return Wishlist
