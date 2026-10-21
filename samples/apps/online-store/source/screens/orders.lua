-- The orders tab: the orders with their state, the newest on top, each opening its progress.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Orders = require('orders')
local Page = require('page')
local Shell = require('shell')
local Style = require('style')
local Widgets = require('widgets')

local OrderList = haylen.class('OrderList', Page)

function OrderList:enter()
    local space = Style.space
    self:mountGui(Shell.frame(self, 'orders', ui.column{grow = 1, gap = 0,
        Shell.centered(Style.size.reading, ui.column{gap = space.xxs, padding = {space.l, space.l, space.s, space.l},
            ui.label{text = 'Your orders', font = 'title'},
            ui.label{text = 'Follow an order from the warehouse to your door.', color = 'textMuted'},
        }),
        Shell.centered(Style.size.reading, ui.collection{id = 'orders', gap = 0, padding = {0, space.l, space.xl, space.l}, rememberFocus = true,
            types = {order = {template = ui.column{gap = 0,
                ui.row{gap = space.m, padding = {space.m, space.xs},
                    Widgets.picture({parts = true, width = 96, height = 112, inset = space.s}),
                    ui.column{gap = space.xxs, grow = 1,
                        ui.label{part = 'title', bind = {text = 'title'}, font = 'button', wrap = false},
                        ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted', wrap = false},
                        ui.badge{part = 'status', bind = {text = 'status', tone = 'tone'}, text = '', tone = 'success', margin = {space.xxs, 0, 0, 0}},
                    },
                    ui.icon{image = Shell.icon('forward'), size = 24, color = 'textMuted'},
                },
                ui.divider{},
            }, estimatedSize = 146}},
            onSelect = function(event)
                Shell.open(require('screens.order'), {order = event.item})
            end,
        }, 1),
    }, Shell.phoneBar(self, 'Orders')))
    self.list = self.gui:collection('orders')
    events.on('orderChanged', function(changed)
        self.list:setItems(Orders.list)
        self.list:reload(changed.id)
    end, {owner = self})
    self.list:setItems(Orders.list)
end

return OrderList
