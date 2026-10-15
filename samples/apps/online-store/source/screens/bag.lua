-- The bag: the products with their color, size and quantity, a promo code and the costs, beside the products on wide windows.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Bag = require('bag')
local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')
local Widgets = require('widgets')

local BagPage = haylen.class('BagPage', Page)

function BagPage:enter()
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Your bag'),
        ui.column{id = 'body', grow = 1, gap = 0},
    })
    events.on('bagChanged', function()
        self:showLines()
    end, {owner = self})
end

function BagPage:layoutChanged(wide)
    self.gui:replaceChildren('body', {self:body(wide)})
    self.lines = self.gui:collection('lines')
    self:showLines()
end

function BagPage:body(wide)
    local list = ui.collection{id = 'lines', grow = 1, align = 'stretch', gap = 6, padding = {4, 12, 16, 12},
        types = {line = {template = ui.row{gap = 14, padding = {8, 8},
            Widgets.picture({parts = true, width = 100, height = 110, radius = 14, inset = 10, image = 'products/tee.svg', backdrop = 'products/backdrops/peach.svg'}),
            ui.column{gap = 2, grow = 1,
                ui.label{part = 'name', bind = {text = 'name'}, font = 'button'},
                ui.label{part = 'options', bind = {text = 'options'}, font = 'caption', color = 'textMuted'},
                ui.label{part = 'total', bind = {text = 'total'}, font = 'button', color = 'accentText'},
                ui.stepper{part = 'quantity', bind = {value = 'quantity'}, value = 1, min = 0, max = 10, width = 190},
            },
        }, estimatedSize = 130}},
        onChange = function(event)
            if event.part == 'quantity' then
                Bag.setQuantity(event.cell.item, event.value)
            end
        end,
        ui.emptyState{image = 'images/hero.png', imageSize = 220, title = 'Your bag is empty', message = 'Products you add show here.'},
    }
    local summary = {id = 'summary', gap = 10, padding = {16, 20, 16, 20},
        ui.row{gap = 10,
            ui.textField{id = 'code', placeholder = 'Promo code', grow = 1, autocapitalize = 'characters', returnKey = 'done', onChange = function(event)
                self.code = event.value
            end, onSubmit = function()
                self:applyCode()
            end},
            ui.button{id = 'apply', text = 'Apply', onClick = function()
                self:applyCode()
            end},
        },
        self:costRow('subtotal', 'Subtotal'),
        self:costRow('discount', 'Discount'),
        self:costRow('shipping', 'Shipping'),
        ui.divider{},
        self:costRow('total', 'Total', 'heading'),
        ui.button{id = 'checkout', text = 'Checkout', variant = 'primary', align = 'stretch', onClick = function()
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

function BagPage:costRow(id, text, font)
    return ui.row{gap = 8, ui.label{text = text, font = font or 'body', color = font and 'text' or 'textMuted', grow = 1}, ui.label{id = id, text = '', font = font or 'body'}}
end

function BagPage:applyCode()
    if Bag.apply(self.code or '') then
        Shell.notify('The code ' .. Bag.code .. ' takes ' .. math.floor(Bag.codes[Bag.code] * 100) .. '% off.')
    else
        Shell.notify('That code does not exist. Try WELCOME10.', 'warning')
    end
end

function BagPage:showLines()
    self.lines:setItems(Bag.lines)
    local totals = Bag.totals()
    local empty = #Bag.lines == 0
    local gui = self.gui
    gui:set('summary', {visible = not empty})
    if empty then
        return
    end
    gui:set('subtotal', {text = Store.money(totals.subtotal)})
    gui:set('discount', {text = totals.discount > 0 and '-' .. Store.money(totals.discount) or 'None'})
    gui:set('shipping', {text = totals.shipping == 0 and 'Free' or Store.money(totals.shipping)})
    gui:set('total', {text = Store.money(totals.total)})
end

return BagPage
