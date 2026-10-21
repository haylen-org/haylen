-- The messages tab: the conversations, the newest on top, with a dot on those with messages not read yet, narrowed by a typed name.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Chats = require('chats')
local Page = require('page')
local Shell = require('shell')
local Style = require('style')

local Inbox = haylen.class('Inbox', Page)

function Inbox:enter()
    local space = Style.space
    self.query = ''
    self:mountGui(Shell.frame('messages', ui.column{grow = 1, gap = 0,
        Shell.centered(760, ui.column{gap = space.s, padding = {space.m, space.m, space.s, space.m},
            ui.label{text = 'Messages', font = 'title'},
            ui.filterField{id = 'query', placeholder = 'Search', returnKey = 'search', style = {metrics = {controlRadius = Style.radius.field}}, onChange = function(event)
                self.query = string.lower(event.value)
                self:showConversations()
            end},
        }),
        Shell.centered(760, ui.collection{id = 'conversations', grow = 1, gap = 0, padding = {0, 0, space.xl, 0}, rememberFocus = true,
            types = {conversation = {template = ui.row{gap = space.s, padding = {space.s, space.m},
                ui.avatar{part = 'avatar', bind = {image = 'avatar'}, size = 64},
                ui.column{gap = 0, grow = 1,
                    ui.label{part = 'name', bind = {text = 'name'}, font = 'button', wrap = false},
                    ui.row{gap = space.xs,
                        ui.label{part = 'last', bind = {text = 'last', color = 'lastColor'}, text = '', wrap = false, grow = 1},
                        ui.label{part = 'time', bind = {text = 'time'}, text = '', font = 'caption', color = 'textMuted'},
                    },
                },
                ui.icon{part = 'dot', bind = {visible = 'hasUnread'}, image = Shell.icon('dot'), size = 24, color = 'accent', visible = false, accessibilityLabel = 'Unread'},
            }, estimatedSize = 88}},
            onSelect = function(event)
                Shell.open(require('screens.chat'), {conversation = event.item})
            end,
            ui.emptyState{image = Shell.icon('messages'), imageSize = 64, title = 'No conversations', message = 'Send a post to a friend to start one.'},
        }, 1),
    }))
    Shell.followUnread(self)
    self.list = self.gui:collection('conversations')
    events.on('chatChanged', function()
        self:showConversations()
    end, {owner = self})
    self:showConversations()
end

function Inbox:showConversations()
    local shown, ids = {}, {}
    for _, conversation in ipairs(Chats.list) do
        if self.query == '' or string.find(string.lower(conversation.name .. ' ' .. conversation.handle), self.query, 1, true) then
            shown[#shown + 1] = conversation
            ids[#ids + 1] = conversation.id
        end
    end
    -- The conversations keep their tables, so the texts a message changed in place bind again.
    self.list:setItems(shown)
    self.list:reload(ids)
end

return Inbox
