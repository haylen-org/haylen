-- An order: its state in the serif, the stages from the order to the door on a line of steps that fills as it moves on, the products and the total.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Orders = require('orders')
local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')
local Style = require('style')
local Widgets = require('widgets')

local Order = haylen.class('Order', Page)

function Order:enter()
    local order = Orders.find(self.params.order)
    self.order = order
    local space = Style.space
    local steps = {gap = 0}
    for index, stage in ipairs(Orders.stages) do
        local last = index == #Orders.stages
        steps[#steps + 1] = ui.row{gap = space.m, alignItems = 'start',
            ui.column{gap = 0, width = 32, alignItems = 'center',
                ui.image{id = 'stageDot' .. index, image = 'images/step.svg', width = 32, height = 32, accessibilityHidden = true},
                ui.divider{id = 'stageLine' .. index, vertical = true, height = 44, visible = not last, style = {metrics = {borderWidth = 3}}},
            },
            ui.column{gap = 0, grow = 1, padding = {2, 0, last and 0 or space.s, 0},
                ui.label{id = 'stageTitle' .. index, text = stage.title, font = 'button', color = 'textMuted'},
                ui.label{text = stage.message, font = 'caption', color = 'textMuted'},
            },
        }
    end
    local lines = {gap = space.s}
    for _, line in ipairs(order.lines) do
        lines[#lines + 1] = ui.row{gap = space.m,
            Widgets.picture({width = 76, height = 88, inset = space.xs, image = line.image, backdrop = line.backdrop, tint = line.tint}),
            ui.column{gap = 0, grow = 1, ui.label{text = line.name}, ui.label{text = line.options .. ' · Quantity ' .. line.quantity, font = 'caption', color = 'textMuted'}},
            ui.label{text = line.total, font = 'button'},
        }
    end
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header(order.number),
        ui.scroll{grow = 1, height = 0,
            Shell.centered(Style.size.reading, ui.column{gap = space.l, padding = {space.l, space.l, space.xxl, space.l},
                ui.card{gap = space.xs, padding = space.l, style = {colors = {raised = Style.color.sand, border = Style.color.sand}},
                    ui.label{id = 'status', text = '', font = 'title'},
                    ui.label{id = 'statusMessage', text = '', color = 'textMuted'},
                    ui.label{text = 'To ' .. Store.me.address.street .. ', ' .. Store.me.address.city, font = 'caption', color = 'textMuted'},
                },
                ui.column(steps),
                ui.label{text = 'Products', font = 'heading'},
                ui.column(lines),
                ui.divider{},
                ui.row{gap = space.s, ui.label{text = 'Total', font = 'heading', grow = 1}, ui.label{text = Store.money(order.total), font = 'heading'}},
            }),
        },
    })
    events.on('orderChanged', function(changed)
        if changed == order then
            self:showStage()
        end
    end, {owner = self})
    self:showStage()
end

-- Fills the steps the order reached, and the line to each one.
function Order:showStage()
    local order = self.order
    local stage = Orders.stages[order.stage]
    self.gui:set('status', {text = stage.title})
    self.gui:set('statusMessage', {text = stage.message})
    for index = 1, #Orders.stages do
        local done = index <= order.stage
        self.gui:set('stageDot' .. index, {image = done and 'images/step-done.svg' or 'images/step.svg'})
        self.gui:set('stageTitle' .. index, {color = done and 'text' or 'textMuted'})
        if index < #Orders.stages then
            self.gui:set('stageLine' .. index, {color = index < order.stage and 'successText' or 'border'})
        end
    end
end

return Order
