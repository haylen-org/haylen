-- The orders tab: the orders with their state, the newest on top, each opening its tracking.
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
        ui.row{gap = 12, padding = {14, 16, 8, 20}, ui.label{text = 'Orders', font = 'title', grow = 1}, Widgets.bagButton(self)},
        Shell.centered(980, ui.collection{id = 'orders', gap = 4, padding = {0, 12, 24, 12}, rememberFocus = true,
            types = {order = {template = ui.row{gap = 14, padding = {8, 8},
                Widgets.picture({parts = true, width = 84, height = 92, radius = 14, inset = 8, image = 'products/tee.svg', backdrop = 'products/backdrops/sand.svg'}),
                ui.column{gap = 2, grow = 1,
                    ui.label{part = 'title', bind = {text = 'title'}, font = 'button', wrap = false},
                    ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false},
                    ui.badge{part = 'status', bind = {text = 'status', tone = 'tone'}, text = '', tone = 'success'},
                },
                ui.icon{image = Shell.icon('forward'), size = 24, color = 'textMuted'},
            }, estimatedSize = 112}},
            onSelect = function(event)
                Shell.open(require('screens.order'), {order = event.item})
            end,
        }, 1),
    }))
    Widgets.showCount(self.gui)
    self.list = self.gui:collection('orders')
    events.on('orderChanged', function()
        self.list:setItems(Orders.list)
    end, {owner = self})
    self.list:setItems(Orders.list)
end

return OrderList
