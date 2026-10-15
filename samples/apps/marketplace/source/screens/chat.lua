-- A chat with a seller about a listing: the listing on top, the messages in a recycled list that sticks to its end, and the field that sends a message.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Chats = require('chats')
local Market = require('data.market')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Chat = haylen.class('Chat', Page)

-- The bubbles of the person take the accent color of the theme.
Chat.bubbleStyle = {colors = {raised = 'accent', border = 'accent'}}

function Chat:enter()
    local conversation
    for _, other in ipairs(Chats.list) do
        if other.id == self.params.conversation then
            conversation = other
        end
    end
    self.conversation = conversation
    local listing = Market.listing(conversation.listing)
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header(conversation.name),
        Shell.centered(900, ui.column{gap = 0,
            ui.card{margin = {0, 16, 8, 16}, padding = 10,
                ui.row{gap = 12,
                    Widgets.picture({width = 64, height = 64, radius = 12, inset = 6, image = listing.image, backdrop = listing.backdrop}),
                    ui.column{gap = 0, grow = 1, ui.label{text = listing.title, font = 'button', wrap = false}, ui.label{text = listing.priceText .. ' · ' .. listing.condition, font = 'caption', color = 'textMuted'}},
                    ui.button{id = 'view', text = 'View', onClick = function()
                        Shell.open(require('screens.listing'), {listing = listing.id})
                    end},
                },
            },
            ui.collection{id = 'messages', grow = 1, gap = 8, padding = {8, 16, 16, 16}, stickToEnd = true,
                types = {
                    day = {template = ui.label{part = 'text', bind = {text = 'text'}, font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'}, interactive = false},
                    incoming = {template = ui.row{gap = 0,
                        ui.card{maxWidth = 560, padding = {10, 16}, gap = 2, ui.label{part = 'text', bind = {text = 'text'}}, ui.label{part = 'time', bind = {text = 'time'}, font = 'caption', color = 'textMuted'}},
                        ui.spacer{grow = 1},
                    }, interactive = false},
                    outgoing = {template = ui.row{gap = 0,
                        ui.spacer{grow = 1},
                        ui.card{maxWidth = 560, padding = {10, 16}, gap = 2, style = Chat.bubbleStyle, ui.label{part = 'text', bind = {text = 'text'}, color = 'onAccent'}, ui.label{part = 'time', bind = {text = 'time'}, font = 'caption', color = 'onAccent', align = 'end'}},
                    }, interactive = false},
                },
            },
            ui.row{gap = 10, padding = {8, 16, 14, 16},
                ui.textField{id = 'draft', placeholder = 'Write a message', returnKey = 'send', grow = 1, onChange = function(event)
                    self.draft = event.value
                end, onSubmit = function(event)
                    self:send(event.value)
                end},
                ui.button{id = 'send', icon = Shell.icon('send'), variant = 'primary', accessibilityLabel = 'Send', tooltip = 'Send', onClick = function()
                    self:send(self.draft or '')
                end},
            },
        }, 1),
    })
    self.messages = self.gui:collection('messages')
    events.on('chatChanged', function(changed)
        if changed == conversation then
            self.messages:setItems(conversation.messages)
        end
    end, {owner = self})
    self.messages:setItems(conversation.messages)
    Chats.read(conversation)
end

function Chat:send(text)
    if text:match('^%s*$') then
        return
    end
    Chats.send(self.conversation, text)
    self.draft = ''
    self.gui:set('draft', {value = ''})
end

function Chat:exit()
    Chats.read(nil)
end

return Chat
