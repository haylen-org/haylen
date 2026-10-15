-- The inbox tab: the conversations with sellers, the newest on top, with the count of unread messages.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Chats = require('chats')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Inbox = haylen.class('Inbox', Page)

function Inbox:enter()
    self:mountGui(Shell.frame('inbox', ui.column{grow = 1, gap = 0,
        ui.row{gap = 12, padding = {14, 20, 8, 20}, ui.label{text = 'Inbox', font = 'title', grow = 1}, ui.badge{id = 'unread', text = '', tone = 'accent', solid = true}},
        ui.collection{id = 'conversations', grow = 1, gap = 2, padding = {0, 12, 24, 12}, maxWidth = 980, rememberFocus = true,
            types = {conversation = Widgets.conversationRow()},
            onSelect = function(event)
                Shell.open(require('screens.chat'), {conversation = event.item})
            end,
            ui.emptyState{image = Shell.icon('chat'), imageSize = 72, title = 'No messages yet', message = 'Message a seller from a listing to start a chat.'},
        },
    }))
    self.list = self.gui:collection('conversations')
    events.on('chatChanged', function()
        self:showConversations()
    end, {owner = self})
    self:showConversations()
end

function Inbox:showConversations()
    self.list:setItems(Chats.list)
    local unread = Chats.unreadCount()
    self.gui:set('unread', {text = unread == 1 and '1 unread' or unread .. ' unread', visible = unread > 0})
end

return Inbox
