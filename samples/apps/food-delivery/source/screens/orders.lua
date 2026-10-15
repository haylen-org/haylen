-- The orders tab: the orders on their way with a button to track them, and the past orders with a button to order again.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Orders = require('orders')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local OrderList = haylen.class('OrderList', Page)

function OrderList:enter()
    self:mountGui(Shell.frame('orders', ui.column{grow = 1, gap = 0,
        ui.row{gap = 12, padding = {14, 16, 8, 20}, ui.label{text = 'Orders', font = 'title', grow = 1}, Widgets.cartButton(self)},
        ui.collection{id = 'orders', grow = 1, gap = 4, padding = {0, 12, 24, 12}, maxWidth = 980, rememberFocus = true,
            types = {
                section = {template = ui.panel{padding = {14, 8, 6, 8}, style = Shell.flatBar, ui.sectionTitle{part = 'title', bind = {text = 'title'}}}, sticky = true, interactive = false},
                order = Widgets.orderRow(),
            },
            onClick = function(event)
                self:act(event.cell.item)
            end,
        },
    }))
    Widgets.showCount(self.gui)
    self.list = self.gui:collection('orders')
    events.on('orderChanged', function()
        self:showOrders()
    end, {owner = self})
    self:showOrders()
end

function OrderList:showOrders()
    local items = {}
    if #Orders.active > 0 then
        items[#items + 1] = {id = 'active', type = 'section', title = 'On the way'}
        for _, order in ipairs(Orders.active) do
            items[#items + 1] = order
        end
    end
    items[#items + 1] = {id = 'past', type = 'section', title = 'Past orders'}
    for _, order in ipairs(Orders.past) do
        items[#items + 1] = order
    end
    self.list:setItems(items)
end

-- Tracks an order on its way, or opens the restaurant of a past order to order again.
function OrderList:act(id)
    local order = Orders.find(id)
    if order.action == 'Track' then
        Shell.open(require('screens.tracking'), {order = order.id})
    else
        Shell.open(require('screens.restaurant'), {restaurant = order.restaurant})
    end
end

return OrderList
