-- The checkout: the shipping address in a form that marks every empty field, the delivery, the payment and the total on the button that places the order, with the costs in a card beside the form on wide windows.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Bag = require('bag')
local Orders = require('orders')
local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')
local Style = require('style')
local Widgets = require('widgets')

local Checkout = haylen.class('Checkout', Page)

Checkout.deliveries = {{id = 'standard', text = 'Standard, 3 to 5 days, free over $75'}, {id = 'express', text = 'Express, next day, $9.99'}}
Checkout.payments = {{id = 'card', text = 'Card ending in 4821'}, {id = 'wallet', text = 'Phone wallet'}, {id = 'later', text = 'Pay in three parts'}}

function Checkout:enter()
    local address = Store.me.address
    self.fields = {name = address.name, street = address.street, city = address.city, zip = address.zip}
    self.express = false
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Checkout'),
        ui.scroll{grow = 1, height = 0, ui.column{id = 'body', gap = 0}},
    })
end

function Checkout:layoutChanged(wide)
    local space = Style.space
    local form = ui.column{gap = space.l, grow = 1,
        self:section('Shipping address', ui.column{gap = space.s,
            self:field('name', 'Full name', 'words'),
            self:field('street', 'Street and number', 'words'),
            ui.row{gap = space.s, align = 'stretch', ui.column{grow = 2, self:field('city', 'City', 'words')}, ui.column{grow = 1, self:field('zip', 'Postal code', 'none', 'number')}},
        }),
        self:section('Delivery', ui.radioGroup{id = 'delivery', items = Checkout.deliveries, selected = self.express and 'express' or 'standard', onChange = function(event)
            self.express = event.value == 'express'
            self:showTotal()
        end}),
        self:section('Payment', ui.radioGroup{id = 'payment', items = Checkout.payments, selected = 'card'}),
    }
    local summary = ui.card{gap = space.s, padding = space.l, style = {colors = {raised = Style.color.sand, border = Style.color.sand}},
        ui.label{text = 'Summary', font = 'heading'},
        Widgets.costRow('subtotal', 'Subtotal'),
        Widgets.costRow('discount', 'Discount'),
        Widgets.costRow('shipping', 'Shipping'),
        ui.divider{},
        Widgets.costRow('total', 'Total', true),
        ui.button{id = 'place', text = '', variant = 'primary', align = 'stretch', margin = {space.xs, 0, 0, 0}, onClick = function()
            self:place()
        end},
    }
    local body
    if wide then
        summary.width = 420
        summary.align = 'start'
        body = Shell.centered(1100, ui.row{gap = space.xl, padding = {space.l, space.xl, space.xxl, space.xl}, align = 'start', form, summary})
    else
        body = ui.column{gap = space.l, padding = {space.l, space.l, space.xxl, space.l}, form, summary}
    end
    self.gui:replaceChildren('body', {body})
    self:showTotal()
end

function Checkout:section(title, content)
    return ui.column{gap = Style.space.s, ui.label{text = title, font = 'heading'}, content}
end

function Checkout:field(id, label, capitalize, keyboard)
    return ui.formField{id = id .. 'Field', label = label, required = true,
        ui.textField{id = id, value = self.fields[id], keyboard = keyboard or 'text', autocapitalize = capitalize, returnKey = 'next', onChange = function(event)
            self.fields[id] = event.value
        end},
    }
end

function Checkout:showTotal()
    local totals = Bag.totals(self.express)
    Widgets.showCosts(self.gui, totals)
    self.gui:set('place', {text = 'Place order, ' .. Store.money(totals.total)})
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
