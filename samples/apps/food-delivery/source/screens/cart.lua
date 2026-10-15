-- The cart: the dishes with their quantities, the costs and the way to the checkout. Wide windows show the costs beside the dishes.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Basket = require('basket')
local Catalog = require('data.catalog')
local Page = require('page')
local Shell = require('shell')

local Cart = haylen.class('Cart', Page)

function Cart:enter()
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Your cart', {
            ui.button{id = 'clear', icon = Shell.icon('trash'), variant = 'icon', tooltip = 'Empty the cart', accessibilityLabel = 'Empty the cart', onClick = function(event)
                event.gui:set('confirmClear', {open = true})
            end},
        }),
        ui.column{id = 'body', grow = 1, gap = 0},
        ui.dialog{id = 'confirmClear', title = 'Empty the cart?', message = 'Every dish leaves the cart.',
            buttons = {{id = 'keep', text = 'Keep'}, {id = 'clear', text = 'Empty the cart', variant = 'destructive'}},
            onAnswer = function(event)
                if event.button == 'clear' then
                    Basket.clear()
                end
            end},
    })
    events.on('basketChanged', function()
        self:showLines()
    end, {owner = self})
end

function Cart:layoutChanged(wide)
    self.gui:replaceChildren('body', {self:body(wide)})
    self.lines = self.gui:collection('lines')
    self:showLines()
end

function Cart:body(wide)
    local list = ui.collection{id = 'lines', grow = 1, align = 'stretch', gap = 4, padding = {4, 12, 16, 12}, maxWidth = 900,
        types = {line = {template = ui.row{gap = 14, padding = {8, 8},
            ui.image{part = 'image', bind = {image = 'image'}, fit = 'cover', width = 96, height = 80, radius = 14},
            ui.column{gap = 2, grow = 1,
                ui.label{part = 'name', bind = {text = 'name'}, font = 'button'},
                ui.label{part = 'caption', bind = {text = 'caption'}, font = 'caption', color = 'textMuted'},
                ui.label{part = 'total', bind = {text = 'total'}, font = 'button', color = 'accentText'},
            },
            ui.stepper{part = 'quantity', bind = {value = 'quantity'}, value = 1, min = 0, max = 20, width = 200},
        }, estimatedSize = 104}},
        onChange = function(event)
            if event.part == 'quantity' then
                Basket.setQuantity(event.cell.item, event.value)
            end
        end,
        ui.emptyState{image = Shell.icon('bag'), imageSize = 80, title = 'Your cart is empty', message = 'Dishes you add from a restaurant show here.'},
    }
    local summary = {id = 'summary', gap = 10, padding = {16, 20, 16, 20},
        ui.label{id = 'restaurant', text = '', font = 'button'},
        self:costRow('subtotal', 'Subtotal'),
        self:costRow('delivery', 'Delivery'),
        self:costRow('service', 'Service'),
        ui.divider{},
        self:costRow('total', 'Total', 'heading'),
        ui.button{id = 'checkout', text = 'Go to checkout', variant = 'primary', align = 'stretch', onClick = function()
            Shell.open(require('screens.checkout'))
        end},
    }
    if wide then
        summary.width = 440
        summary.align = 'start'
        return ui.row{grow = 1, gap = 16, padding = {0, 20, 16, 8}, align = 'stretch', list, ui.card(summary)}
    end
    summary.style = Shell.flatBar
    return ui.column{grow = 1, gap = 0, list, ui.divider{}, ui.panel(summary)}
end

function Cart:costRow(id, text, font)
    return ui.row{gap = 8, ui.label{text = text, font = font or 'body', color = font and 'text' or 'textMuted', grow = 1}, ui.label{id = id, text = '', font = font or 'body'}}
end

function Cart:showLines()
    self.lines:setItems(Basket.lines)
    local totals = Basket.totals()
    local empty = #Basket.lines == 0
    local gui = self.gui
    gui:set('summary', {visible = not empty})
    gui:set('clear', {enabled = not empty})
    if empty then
        return
    end
    gui:set('restaurant', {text = 'From ' .. Catalog.restaurant(Basket.restaurant).name})
    gui:set('subtotal', {text = Catalog.money(totals.subtotal)})
    gui:set('delivery', {text = totals.delivery == 0 and 'Free' or Catalog.money(totals.delivery)})
    gui:set('service', {text = Catalog.money(totals.service)})
    gui:set('total', {text = Catalog.money(totals.total)})
end

return Cart
