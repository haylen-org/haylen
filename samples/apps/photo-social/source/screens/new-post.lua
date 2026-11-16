-- A new post, which the app presents over everything as a flow of its own: a bar with the way out, the title of the step and its action, which stays while the steps slide under it in a stack of their own, from the picture to its edit and its caption.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Style = require('style')

local NewPost = haylen.class('NewPost', Page)

function NewPost:tree()
    local space = Style.space
    return ui.column{gap = 0, align = 'stretch',
        ui.row{gap = space.xs, padding = {0, space.s}, height = Style.size.header,
            ui.button{id = 'back', icon = Shell.icon('close'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Close', tooltip = 'Close', onClick = ui.goBack},
            ui.label{id = 'title', text = 'New post', font = 'heading', wrap = false, grow = 1},
            ui.button{id = 'action', text = 'Next', variant = 'link', onClick = function()
                self.steps.top:action()
            end},
        },
        ui.divider{},
        ui.column{id = 'steps', grow = 1, gap = 0, align = 'stretch'},
    }
end

function NewPost:start()
    self.steps = ui.navigationStack{gui = self.gui, id = 'steps', root = require('screens.compose-pick')(), owner = self, onChange = function(event)
        self:showStep(event.top, event.size)
    end}
    self:showStep(self.steps.top, self.steps.size)
end

-- The bar takes the title and the action of the step on top, and its first button closes the new post on the first step and goes back a step on the others.
function NewPost:showStep(step, size)
    local first = size == 1
    self.gui:set('title', {text = step.title})
    self.gui:set('action', {text = step.actionText})
    self.gui:set('back', {icon = Shell.icon(first and 'close' or 'back'), accessibilityLabel = first and 'Close' or 'Back', tooltip = first and 'Close' or 'Back'})
end

return NewPost
