-- The messages tab: the conversations and the open chat. A wide window shows them side by side, with a note in place of the chat until one opens, and a phone shows them as one stack, where the chat slides over the conversations. The split lives in this screen, so it keeps its chat while another tab shows, and the route of a chat shows its chat as the detail of this split.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')

local Messages = haylen.class('Messages', Page)

-- What the side of the chat shows while no chat is open.
local Pick = haylen.class('Pick', ui.Screen)

function Pick:build()
    return ui.emptyState{image = Shell.icon('messages'), imageSize = 72, title = 'Your messages', message = 'Pick a conversation to read it.'}
end

function Messages:tree()
    return ui.row{gap = 0, align = 'stretch',
        ui.column{id = 'list', grow = 1, align = 'stretch'},
        ui.divider{id = 'line', vertical = true, align = 'stretch'},
        ui.column{id = 'detail', grow = 2, align = 'stretch'},
    }
end

function Messages:start()
    self.split = ui.navigationSplit{primary = {gui = self.gui, id = 'list'}, detail = {gui = self.gui, id = 'detail'}, root = require('screens.inbox')(), placeholder = function() return Pick() end, collapsed = not self.wide, owner = self}
end

-- A wide window keeps the chat beside the conversations, and a narrow one hides the side of the chat and works as one stack.
function Messages:layoutChanged(wide)
    self.gui:set('detail', {visible = wide})
    self.gui:set('line', {visible = wide})
    if not self.split then
        return
    end
    self.split.collapsed = not wide
    local chat = self.split.hasDetail and (wide and self.split.detail.top or self.split.primary.top)
    if chat and chat.showBack then
        chat:showBack()
    end
end

return Messages
