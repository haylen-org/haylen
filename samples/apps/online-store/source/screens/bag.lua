-- The bag: the products with their color, size and quantity, a promo code and the costs, which stand in a card beside the products on wide windows and at the bottom of a phone.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Bag = require('bag')
local Page = require('page')
local Shell = require('shell')
local Style = require('style')
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
    local space = Style.space
    local list = ui.collection{id = 'lines', grow = 1, align = 'stretch', gap = space.xs, padding = wide and {space.l, 0, space.l, 0} or {space.s, space.l}, rememberFocus = true,
        types = {line = {template = ui.row{gap = space.m, padding = {space.s, 0}, alignItems = 'start',
            Widgets.picture({parts = true, width = 112, height = 132, inset = space.s}),
            ui.column{gap = space.xxs, grow = 1,
                ui.label{part = 'name', bind = {text = 'name'}, font = 'button'},
                ui.label{part = 'options', bind = {text = 'options'}, font = 'caption', color = 'textMuted'},
                ui.label{part = 'total', bind = {text = 'total'}, margin = {space.xxs, 0, space.xs, 0}},
                ui.stepper{part = 'quantity', bind = {value = 'quantity'}, value = 1, min = 0, max = 10, width = 180, accessibilityLabel = 'Quantity'},
            },
        }, interactive = false, estimatedSize = 164}},
        onChange = function(event)
            if event.part == 'quantity' then
                Bag.setQuantity(event.cell.item, event.value)
            end
        end,
        ui.emptyState{image = 'images/hero.png', imageSize = 240, title = 'Your bag is empty', message = 'The products you add show here.'},
    }
    local summary = ui.column{id = 'summary', gap = space.s,
        ui.row{gap = space.xs,
            ui.textField{id = 'code', placeholder = 'Promo code', grow = 1, autocapitalize = 'characters', returnKey = 'done', onChange = function(event)
                self.code = event.value
            end, onSubmit = function()
                self:applyCode()
            end},
            ui.button{id = 'apply', text = 'Apply', onClick = function()
                self:applyCode()
            end},
        },
        Widgets.costRow('subtotal', 'Subtotal'),
        Widgets.costRow('discount', 'Discount'),
        Widgets.costRow('shipping', 'Shipping'),
        ui.divider{},
        Widgets.costRow('total', 'Total', true),
        ui.button{id = 'checkout', text = 'Checkout', variant = 'primary', align = 'stretch', margin = {space.xs, 0, 0, 0}, onClick = function()
            Shell.open(require('screens.checkout'))
        end},
    }
    if wide then
        return Shell.centered(Style.size.page, ui.row{gap = space.xl, padding = {0, space.xl}, align = 'stretch',
            list,
            ui.column{width = 440, padding = {space.l, 0}, align = 'start', ui.card{id = 'summaryCard', padding = space.l, style = {colors = {raised = Style.color.sand, border = Style.color.sand}}, summary}},
        }, 1)
    end
    return ui.column{grow = 1, gap = 0, list, ui.column{id = 'summaryCard', gap = 0, ui.divider{}, ui.column{padding = {space.m, space.l}, summary}}}
end

function BagPage:applyCode()
    if Bag.apply(self.code or '') then
        Shell.notify('The code ' .. Bag.code .. ' takes ' .. math.floor(Bag.codes[Bag.code] * 100) .. '% off.', 'success')
    else
        Shell.notify('That code does not exist. Try WELCOME10.', 'warning')
    end
end

function BagPage:showLines()
    self.lines:setItems(Bag.lines)
    local empty = #Bag.lines == 0
    self.gui:set('summaryCard', {visible = not empty})
    if not empty then
        Widgets.showCosts(self.gui, Bag.totals())
    end
end

return BagPage
