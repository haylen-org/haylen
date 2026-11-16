-- The orders tab: the list of orders and the open order. A wide window shows them side by side, with a note in place of the order until one opens, and a phone shows them as one stack, where the order slides over the list. The split lives in this screen, so it keeps the open order while another tab shows.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')

local OrdersTab = haylen.class('OrdersTab', Page)

-- What the side of the order shows while no order is open.
local Pick = haylen.class('Pick', ui.Screen)

function Pick:build()
    return ui.emptyState{image = Shell.icon('orders'), imageSize = 72, title = 'Your orders', message = 'Pick an order to follow it.'}
end

function OrdersTab:tree()
    return ui.row{gap = 0, align = 'stretch',
        ui.column{id = 'list', grow = 1, align = 'stretch'},
        ui.divider{id = 'line', vertical = true, align = 'stretch'},
        ui.column{id = 'detail', grow = 1, align = 'stretch'},
    }
end

function OrdersTab:start()
    self.split = ui.navigationSplit{primary = {gui = self.gui, id = 'list'}, detail = {gui = self.gui, id = 'detail'}, root = require('screens.orders')({tab = self}), placeholder = function() return Pick() end, collapsed = not self.wide, owner = self}
end

-- A wide window keeps the order beside the list, and a narrow one hides the side of the order and works as one stack.
function OrdersTab:layoutChanged(wide)
    self.gui:set('detail', {visible = wide})
    self.gui:set('line', {visible = wide})
    if not self.split then
        return
    end
    self.split.collapsed = not wide
    local order = self.split.hasDetail and (wide and self.split.detail.top or self.split.primary.top)
    if order then
        order:showBack()
    end
end

-- Opens an order beside the list or over it.
function OrdersTab:openOrder(id)
    self.split:showDetail(require('screens.order')({order = id}))
end

return OrdersTab
