-- A conversation: the person on top, the messages in a recycled list that sticks to its end, with the posts that travel in them as pictures, and the field that sends a message. The field stays above the on-screen keyboard, and a tap on the send button sends at once while the keyboard is open.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Chats = require('chats')
local Page = require('page')
local Shell = require('shell')
local Social = require('data.social')
local Style = require('style')
local Widgets = require('widgets')

local Chat = haylen.class('Chat', Page)

-- The bubbles of the person take the accent of the theme, and those of the others the soft gray of the app.
Chat.outgoing = {colors = {raised = 'accent', border = 'accent'}, metrics = {panelRadius = Style.radius.bubble}}
Chat.incoming = {colors = {raised = Style.color.mist, border = Style.color.mist}, metrics = {panelRadius = Style.radius.bubble}}

function Chat:tree()
    local conversation = Chats.find(self.params.conversation)
    self.conversation = conversation
    local person = Social.person(conversation.person)
    local space = Style.space
    local bubble = {maxWidth = 520, padding = {space.s, space.m}, gap = space.xxs}
    local function card(style, color, align)
        local node = {style = style}
        for key, value in pairs(bubble) do
            node[key] = value
        end
        node[1] = ui.label{part = 'text', bind = {text = 'text'}, color = color}
        node[2] = ui.label{part = 'time', bind = {text = 'time'}, font = 'caption', color = color == 'onAccent' and 'onAccent' or 'textMuted', align = align}
        return ui.card(node)
    end
    return ui.column{gap = 0,
        ui.column{gap = 0,
            ui.row{gap = space.s, padding = {0, space.m, 0, space.s}, height = Style.size.header,
                ui.button{id = 'back', icon = Shell.icon('back'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Back', tooltip = 'Back', onClick = ui.goBack},
                ui.avatar{image = person.avatar, size = Style.size.avatarSmall},
                ui.column{gap = 0, grow = 1,
                    ui.label{text = person.name, font = 'button', wrap = false},
                    ui.label{text = 'Active now', font = 'caption', color = 'textMuted', wrap = false},
                },
                ui.button{id = 'profile', text = 'Profile', variant = 'link', onClick = function()
                    Widgets.openPerson(person.id)
                end},
            },
            ui.divider{},
        },
        Shell.centered(820, ui.column{gap = 0,
            ui.collection{id = 'messages', grow = 1, gap = space.xs, padding = {space.m, space.m}, stickToEnd = true,
                types = {
                    day = {template = ui.label{part = 'text', bind = {text = 'text'}, font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'}, interactive = false},
                    incoming = {template = ui.row{gap = 0, card(Chat.incoming, 'text', 'start'), ui.spacer{grow = 1}}, interactive = false},
                    outgoing = {template = ui.row{gap = 0, ui.spacer{grow = 1}, card(Chat.outgoing, 'onAccent', 'end')}, interactive = false},
                    outgoingPost = {template = ui.row{gap = 0, ui.spacer{grow = 1},
                        ui.column{gap = space.xxs, width = 300,
                            ui.image{part = 'photo', bind = {image = 'photo'}, image = Widgets.placeholder, fit = 'cover', aspectRatio = 1, align = 'stretch', radius = Style.radius.bubble},
                            ui.label{part = 'time', bind = {text = 'time'}, font = 'caption', color = 'textMuted', align = 'end'},
                        },
                    }, interactive = false},
                },
            },
            ui.divider{},
            ui.row{gap = space.s, padding = {space.s, space.m, space.m, space.m},
                ui.textField{id = 'draft', placeholder = 'Message', returnKey = 'send', grow = 1, maxLength = 500, style = {metrics = {controlRadius = Style.radius.field}},
                    onChange = function(event)
                        self.draft = event.value
                    end,
                    onSubmit = function(event)
                        self:send(event.value)
                    end},
                ui.button{id = 'send', icon = Shell.icon('send'), variant = 'primary', width = 60, accessibilityLabel = 'Send', tooltip = 'Send', style = {metrics = {controlRadius = 30, controlPaddingX = 0}}, onClick = function()
                    self:send(self.draft or '')
                end},
            },
        }, 1),
    }
end

function Chat:start()
    self.messages = self.gui:collection('messages')
    events.on('chatChanged', function(changed)
        if changed == self.conversation then
            self.messages:setItems(self.conversation.messages)
        end
    end, {owner = self})
    self.messages:setItems(self.conversation.messages)
end

-- The open chat reads its messages while it shows, and its back button shows while there is somewhere to go back to, which a chat beside the list of a wide window does not have.
function Chat:appear()
    Page.appear(self)
    Chats.read(self.conversation)
    self:showBack()
end

function Chat:showBack()
    self.gui:set('back', {visible = self.stack ~= nil and self.stack.canGoBack})
end

function Chat:send(text)
    if text:match('^%s*$') then
        return
    end
    Chats.send(self.conversation, text)
    self.draft = ''
    self.gui:set('draft', {value = ''})
end

function Chat:disappeared()
    Chats.read(nil)
end

return Chat
