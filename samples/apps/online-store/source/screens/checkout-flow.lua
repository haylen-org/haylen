-- The flow of the bag, which the app presents over everything as a sheet: a bar with the way out and the title of the step, which stays while the steps slide under it in a stack of their own, from the bag to the checkout and the confirmation of the order.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Style = require('style')

local CheckoutFlow = haylen.class('CheckoutFlow', Page)

function CheckoutFlow:tree()
    local space = Style.space
    return ui.column{gap = 0, align = 'stretch',
        ui.row{gap = space.xs, padding = {0, space.s}, height = Style.size.header,
            ui.button{id = 'way', icon = Shell.icon('close'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Close', tooltip = 'Close', onClick = ui.goBack},
            ui.label{id = 'title', text = 'Your bag', font = 'heading', wrap = false, grow = 1},
        },
        ui.divider{},
        ui.column{id = 'steps', grow = 1, gap = 0, align = 'stretch'},
    }
end

function CheckoutFlow:start()
    self.steps = ui.navigationStack{gui = self.gui, id = 'steps', root = require('screens.bag')(), owner = self, onChange = function(event)
        self:showStep(event.top, event.size)
    end}
    self:showStep(self.steps.top, self.steps.size)
end

-- The bar takes the title of the step on top, and its first button closes the flow on the first step and goes back a step on the others.
function CheckoutFlow:showStep(step, size)
    local first = size == 1
    self.gui:set('title', {text = step.title})
    self.gui:set('way', {icon = Shell.icon(first and 'close' or 'back'), accessibilityLabel = first and 'Close' or 'Back', tooltip = first and 'Close' or 'Back'})
end

return CheckoutFlow
