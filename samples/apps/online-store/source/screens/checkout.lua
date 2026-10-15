-- The checkout: the shipping address in a form that checks its fields, the delivery, the payment and the total on the button that places the order.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Bag = require('bag')
local Orders = require('orders')
local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')

local Checkout = haylen.class('Checkout', Page)

Checkout.deliveries = {{id = 'standard', text = 'Standard, 3 to 5 days, free over $75'}, {id = 'express', text = 'Express, next day, $9.99'}}
Checkout.payments = {{id = 'card', text = 'Card ending in 4821'}, {id = 'wallet', text = 'Phone wallet'}, {id = 'later', text = 'Pay in three parts'}}

function Checkout:enter()
    local address = Store.me.address
    self.fields = {name = address.name, street = address.street, city = address.city, zip = address.zip}
    self.express = false
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Checkout'),
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 760, align = 'center', gap = 14, padding = {4, 20, 28, 20},
                ui.sectionTitle{text = 'Shipping address'},
                ui.card{gap = 10,
                    self:field('name', 'Full name', 'words'),
                    self:field('street', 'Street and number', 'words'),
                    ui.row{gap = 12, align = 'stretch', ui.column{grow = 2, self:field('city', 'City', 'words')}, ui.column{grow = 1, self:field('zip', 'Postal code', 'none', 'number')}},
                },
                ui.sectionTitle{text = 'Delivery'},
                ui.card{ui.radioGroup{id = 'delivery', items = Checkout.deliveries, selected = 'standard', onChange = function(event)
                    self.express = event.value == 'express'
                    self:showTotal()
                end}},
                ui.sectionTitle{text = 'Payment'},
                ui.card{ui.radioGroup{id = 'payment', items = Checkout.payments, selected = 'card'}},
                ui.card{gap = 8,
                    self:costRow('subtotal', 'Subtotal'),
                    self:costRow('discount', 'Discount'),
                    self:costRow('shipping', 'Shipping'),
                    ui.divider{},
                    self:costRow('total', 'Total', 'heading'),
                },
                ui.button{id = 'place', text = '', variant = 'primary', align = 'stretch', onClick = function()
                    self:place()
                end},
            },
        },
    })
    self:showTotal()
end

function Checkout:field(id, label, capitalize, keyboard)
    return ui.formField{id = id .. 'Field', label = label, required = true,
        ui.textField{id = id, value = self.fields[id], keyboard = keyboard or 'text', autocapitalize = capitalize, returnKey = 'next', onChange = function(event)
            self.fields[id] = event.value
        end},
    }
end

function Checkout:costRow(id, text, font)
    return ui.row{gap = 8, ui.label{text = text, font = font or 'body', color = font and 'text' or 'textMuted', grow = 1}, ui.label{id = id, text = '', font = font or 'body'}}
end

function Checkout:showTotal()
    local totals = Bag.totals(self.express)
    local gui = self.gui
    gui:set('subtotal', {text = Store.money(totals.subtotal)})
    gui:set('discount', {text = totals.discount > 0 and '-' .. Store.money(totals.discount) or 'None'})
    gui:set('shipping', {text = totals.shipping == 0 and 'Free' or Store.money(totals.shipping)})
    gui:set('total', {text = Store.money(totals.total)})
    gui:set('place', {text = 'Place order · ' .. Store.money(totals.total)})
end

-- Checks the address, marks every empty field, and places the order when nothing is missing.
function Checkout:place()
    local valid = true
    for _, id in ipairs({'name', 'street', 'city', 'zip'}) do
        local empty = self.fields[id]:match('^%s*$') ~= nil
        self.gui:set(id .. 'Field', {error = empty and 'This field is required.' or ''})
        valid = valid and not empty
    end
    if not valid then
        Shell.notify('Fill in the address to place the order.', 'warning')
        return
    end
    local order = Orders.place(Bag.lines, Bag.totals(self.express), self.express)
    Bag.clear()
    Shell.showTab('orders')
    Shell.open(require('screens.confirmation'), {order = order.id})
end

return Checkout
