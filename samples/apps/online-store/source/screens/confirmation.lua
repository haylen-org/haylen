-- The confirmation of an order: its number, its total and the way to follow it.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Orders = require('orders')
local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')

local Confirmation = haylen.class('Confirmation', Page)

function Confirmation:enter()
    local order = Orders.find(self.params.order)
    self:mountGui(ui.column{gap = 16, padding = 28, justify = 'center', onCancel = Shell.back,
        Shell.centered(560, ui.column{gap = 14,
            ui.image{image = 'images/hero.png', width = 320, height = 240, fit = 'contain', align = 'center', accessibilityHidden = true},
            ui.label{text = 'Thank you, Rosa!', font = 'title', textAlign = 'center', align = 'stretch'},
            ui.label{text = string.format('%s is confirmed. We sent the receipt of %s to %s.', order.number, Store.money(order.total), Store.me.email), color = 'textMuted', textAlign = 'center', align = 'stretch'},
            ui.button{id = 'track', text = 'Follow the order', variant = 'primary', align = 'stretch', autofocus = true, onClick = function()
                Shell.replace(require('screens.order'), {order = order.id})
            end},
            ui.button{id = 'continue', text = 'Keep shopping', align = 'stretch', onClick = function()
                Shell.showTab('home')
            end},
        }),
    })
end

return Confirmation
