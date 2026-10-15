-- The checkout: where the order goes, how fast, how the person pays, the tip and a note, with the total on the button that places the order.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Account = require('data.account')
local Basket = require('basket')
local Catalog = require('data.catalog')
local Orders = require('orders')
local Page = require('page')
local Shell = require('shell')

local Checkout = haylen.class('Checkout', Page)

Checkout.speeds = {
    {id = 'standard', text = 'Standard'},
    {id = 'priority', text = 'Priority, $2.99'},
}

Checkout.tips = {
    {id = '0', text = 'None'},
    {id = '0.1', text = '10%'},
    {id = '0.15', text = '15%'},
    {id = '0.2', text = '20%'},
}

function Checkout:enter()
    self.address = Account.address
    self.payment = Account.payments[1].id
    self.priority = false
    self.tip = 0.15
    local addresses, payments = {}, {}
    for _, address in ipairs(Account.addresses) do
        addresses[#addresses + 1] = {id = address.id, text = address.text .. ', ' .. address.line}
    end
    for _, payment in ipairs(Account.payments) do
        payments[#payments + 1] = {id = payment.id, text = payment.text}
    end
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Checkout'),
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 760, align = 'center', gap = 14, padding = {4, 20, 24, 20},
                ui.sectionTitle{text = 'Deliver to'},
                ui.card{ui.radioGroup{id = 'address', items = addresses, selected = self.address, onChange = function(event)
                    self.address = event.value
                end}},
                ui.sectionTitle{text = 'Delivery'},
                ui.segmentedControl{id = 'speed', items = Checkout.speeds, selected = 'standard', onChange = function(event)
                    self.priority = event.value == 'priority'
                    self:showTotal()
                end},
                ui.label{id = 'eta', text = '', font = 'caption', color = 'textMuted'},
                ui.sectionTitle{text = 'Payment'},
                ui.card{ui.radioGroup{id = 'payment', items = payments, selected = self.payment, onChange = function(event)
                    self.payment = event.value
                end}},
                ui.sectionTitle{text = 'Tip for the courier'},
                ui.segmentedControl{id = 'tip', items = Checkout.tips, selected = '0.15', onChange = function(event)
                    self.tip = tonumber(event.value)
                    self:showTotal()
                end},
                ui.formField{label = 'Note for the courier', help = 'Such as the gate code or where to leave the bag.',
                    ui.textArea{id = 'note', rows = 3, placeholder = 'Leave it at the door', maxLength = 200},
                },
                ui.card{gap = 8,
                    self:costRow('subtotal', 'Subtotal'),
                    self:costRow('delivery', 'Delivery'),
                    self:costRow('service', 'Service'),
                    self:costRow('tipAmount', 'Tip'),
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

function Checkout:costRow(id, text, font)
    return ui.row{gap = 8, ui.label{text = text, font = font or 'body', color = font and 'text' or 'textMuted', grow = 1}, ui.label{id = id, text = '', font = font or 'body'}}
end

function Checkout:showTotal()
    local totals = Basket.totals(self.tip, self.priority)
    local restaurant = Catalog.restaurant(Basket.restaurant)
    local gui = self.gui
    gui:set('subtotal', {text = Catalog.money(totals.subtotal)})
    gui:set('delivery', {text = totals.delivery == 0 and 'Free' or Catalog.money(totals.delivery)})
    gui:set('service', {text = Catalog.money(totals.service)})
    gui:set('tipAmount', {text = Catalog.money(totals.tip)})
    gui:set('total', {text = Catalog.money(totals.total)})
    gui:set('place', {text = 'Place order · ' .. Catalog.money(totals.total)})
    local minutes = restaurant.minutes - (self.priority and 10 or 0)
    gui:set('eta', {text = string.format('Arrives in %d to %d minutes.', minutes, minutes + 10)})
end

-- Places the order, empties the cart and shows the order on its map.
function Checkout:place()
    local restaurant = Catalog.restaurant(Basket.restaurant)
    local lines = {}
    for index, line in ipairs(Basket.lines) do
        lines[index] = {name = line.name, quantity = line.quantity, total = line.total}
    end
    local order = Orders.place(lines, restaurant, Basket.totals(self.tip, self.priority), self.address, self.payment)
    Basket.clear()
    Shell.showTab('orders')
    Shell.open(require('screens.tracking'), {order = order.id})
end

return Checkout
