-- The confirmation of an order: the illustration, the thanks in the serif, the number and the total of the order and the way to follow it.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Orders = require('orders')
local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')
local Style = require('style')

local Confirmation = haylen.class('Confirmation', Page)

function Confirmation:enter()
    local order = Orders.find(self.params.order)
    local space = Style.space
    self:mountGui(ui.scroll{grow = 1, onCancel = Shell.back,
        Shell.centered(Style.size.reading, ui.column{gap = space.m, padding = {space.xxl, space.l},
            ui.image{image = 'images/hero.png', width = 360, height = 270, fit = 'contain', align = 'center', accessibilityHidden = true},
            ui.label{text = 'Thank you, Rosa!', font = 'title', textAlign = 'center', align = 'stretch', style = Style.text('title', Style.type.display)},
            ui.label{text = string.format('%s is confirmed. We sent the receipt of %s to %s.', order.number, Store.money(order.total), Store.me.email), color = 'textMuted', textAlign = 'center', align = 'stretch'},
            ui.column{gap = space.s, margin = {space.l, 0, 0, 0}, width = 420, align = 'center',
                ui.button{id = 'track', text = 'Follow the order', variant = 'primary', align = 'stretch', autofocus = true, onClick = function()
                    Shell.replace(require('screens.order'), {order = order.id})
                end},
                ui.button{id = 'continue', text = 'Keep shopping', align = 'stretch', onClick = function()
                    Shell.showTab('home')
                end},
            },
        }),
    })
end

return Confirmation
