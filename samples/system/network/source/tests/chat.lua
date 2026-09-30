-- A chat over the echo socket: messages go out as JSON and come back from the echo server, the socket reconnects on its own after a lost connection, and messages written while it is not open wait in a queue that the next open sends, since sending on a closed socket raises.
local haylen = require('haylen')
local json = require('json')
local net = require('haylen.net')
local ui = require('haylen.ui')

local sample = require('sample')
local services = require('services')

local Chat = haylen.class('Chat', sample.Test)

Chat.hints = 'Type a message and press Enter or Send. The newest message is at the top. The echo server sends every message back, and messages written while the connection is down wait for it.'
Chat.focus = 'message'

local kLimit = 40
local kStates = {open = {'Connected', 'success'}, connecting = {'Connecting', 'information'}, reconnecting = {'Reconnecting', 'warning'}, closing = {'Closing', 'warning'}, closed = {'Offline', 'danger'}}

function Chat:init(entry)
    Chat.super.init(self, entry)
    self.name = 'Ana'
    self.draft = ''
    self.messages = {}
    self.queue = {}
end

function Chat:content()
    return {
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.row{gap = 12,
                ui.statusIndicator{id = 'state', text = 'Connecting', tone = 'information'},
                ui.spacer{grow = 1},
                ui.badge{id = 'queued', text = '', tone = 'warning', visible = false},
            },
            ui.scroll{id = 'scroll', grow = 1, ui.column{id = 'messages', gap = 12, padding = {0, 24, 0, 0}}},
            ui.row{gap = 12,
                ui.textField{id = 'name', value = self.name, width = 220, maxLength = 16, autocapitalize = 'words', returnKey = 'next', onChange = function(event)
                    self.name = event.value
                end},
                ui.textField{id = 'message', value = '', placeholder = 'Write a message', grow = 1, maxLength = 300, returnKey = 'send', onChange = function(event)
                    self.draft = event.value
                end, onSubmit = function()
                    self:write()
                end},
                ui.button{id = 'send', text = 'Send', variant = 'primary', onClick = function()
                    self:write()
                end},
            },
        },
    }
end

-- The socket reconnects forever with the default waits, so the chat comes back whenever the network does.
function Chat:enter()
    Chat.super.enter(self)
    local socket = self:keep(net.connectWebSocket(services.echo, {reconnect = true}))
    self.socket = socket
    self:on(socket, 'open', function()
        self:note('Connected to "' .. socket.url .. '"')
        self:flush()
    end)
    self:on(socket, 'message', function(data, binary)
        self:receive(data, binary)
    end)
    self:on(socket, 'disconnect', function(code)
        self:note('The connection dropped with code ' .. code)
    end)
    self:on(socket, 'error', function(message)
        self:note(message)
    end)
end

function Chat:update(dt)
    local state = self.socket.state == 'reconnecting' and 'reconnecting ' .. self.socket.attempt or self.socket.state
    if state ~= self.state then
        self.state = state
        local label, tone = table.unpack(kStates[self.socket.state])
        self:show('state', {text = self.socket.state == 'reconnecting' and label .. ', attempt ' .. self.socket.attempt or label, tone = tone})
    end
end

-- Sends the draft now when the socket is open, and queues it otherwise.
function Chat:write()
    if self.draft == '' then
        return
    end
    local message = json.encode({name = self.name, text = self.draft, time = sample.clock()})
    self.queue[#self.queue + 1] = message
    self.draft = ''
    self:show('message', {value = ''})
    self:flush()
end

function Chat:flush()
    if self.socket.state == 'open' then
        for _, message in ipairs(self.queue) do
            self.socket:send(message)
            self:add('mine', json.decode(message))
        end
        self.queue = {}
    end
    self:show('queued', {text = #self.queue .. ' waiting', visible = #self.queue > 0})
end

-- Messages of the chat come back as JSON, and anything else the server sends, such as its greeting, shows as a note.
function Chat:receive(data, binary)
    local ok, message = pcall(json.decode, data)
    if binary or not ok or type(message) ~= 'table' or message.text == nil then
        self:note('The server says: ' .. (binary and #data .. ' bytes' or data))
        return
    end
    self:add('echo', message)
end

function Chat:note(text)
    self:add('note', {text = text, time = sample.clock()})
end

function Chat:add(kind, message)
    table.insert(self.messages, {kind = kind, message = message})
    if #self.messages > kLimit then
        table.remove(self.messages, 1)
    end
    self:showMessages()
end

-- Draws the newest message first, mine on the right, the echoes on the left and notes in the middle.
function Chat:showMessages()
    local nodes = {}
    for index = #self.messages, 1, -1 do
        local entry = self.messages[index]
        local message = entry.message
        if entry.kind == 'note' then
            nodes[#nodes + 1] = ui.label{text = message.time .. '  ' .. message.text, font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'}
        else
            local mine = entry.kind == 'mine'
            local bubble = ui.card{maxWidth = 900, gap = 4,
                ui.label{text = (mine and 'You as ' or 'Echo of ') .. message.name .. ', ' .. message.time, font = 'caption', color = mine and 'accentText' or 'successText'},
                ui.label{text = message.text},
            }
            nodes[#nodes + 1] = ui.row{mine and ui.spacer{grow = 1} or bubble, mine and bubble or ui.spacer{grow = 1}}
        end
    end
    self.document:replaceChildren('messages', nodes)
end

return Chat
