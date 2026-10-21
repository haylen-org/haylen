-- A chat over the echo socket: messages go out as JSON and come back from the echo server, the socket reconnects on its own after a lost connection, and messages written while it is not open wait in a queue that the next open sends, since sending on a closed socket raises.
local haylen = require('haylen')
local json = require('json')
local net = require('haylen.net')
local ui = require('haylen.ui')

local SocketTest = require('categories.network.socket-test')
local readout = require('categories.network.readout')
local services = require('categories.network.services')

local Chat = haylen.class('Chat', SocketTest)

Chat.code = [[
local socket = net.connectWebSocket('wss://echo.websocket.org', {reconnect = true})
socket:on('open', function() flush(queue) end)  -- Sends what waited while the connection was down.
socket:on('message', function(data, binary) add(json.decode(data)) end)
local message = json.encode({name = 'Ana', text = 'Hello'})
if socket.state == 'open' then socket:send(message) else queue[#queue + 1] = message end]]

Chat.limit = 40
Chat.openers = {'Hi! Is the ferry running today?', 'The tide looks calm from the lighthouse.'}
Chat.states = {open = {'Connected', 'success'}, connecting = {'Connecting', 'information'}, reconnecting = {'Reconnecting', 'warning'}, closing = {'Closing', 'warning'}, closed = {'Offline', 'danger'}}

function Chat:init(entry)
    Chat.super.init(self, entry)
    self.name = 'Ana'
    self.draft = ''
    self.messages = {}
    self.queue = {}
end

-- The socket reconnects forever with the default waits, so the chat comes back whenever the network does.
function Chat:enter()
    self:frame{
        code = Chat.code,
        hint = 'Type a message and press Enter or Send. The echo server sends every message back, and messages written while the connection is down wait for it.',
        focus = 'message',
        content = {ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.row{gap = 12,
                ui.statusIndicator{id = 'state', text = 'Connecting', tone = 'information'},
                ui.spacer{grow = 1},
                ui.badge{id = 'queued', text = '', tone = 'warning', visible = false},
            },
            ui.scroll{id = 'scroll', grow = 1, height = 0, ui.column{id = 'messages', gap = 14}},
            ui.row{gap = 12, alignItems = 'end',
                ui.formField{label = 'Your name', width = 240, ui.textField{id = 'name', value = self.name, maxLength = 16, autocapitalize = 'words', returnKey = 'next', onChange = function(event)
                    self.name = event.value
                end}},
                ui.formField{label = 'Message', grow = 1, ui.textField{id = 'message', value = '', placeholder = 'Write a message', maxLength = 300, returnKey = 'send', onChange = function(event)
                    self.draft = event.value
                end, onSubmit = function()
                    self:write()
                end}},
                ui.button{id = 'send', text = 'Send', variant = 'primary', onClick = function()
                    self:write()
                end},
            },
        }},
    }
    local socket = self:keep(net.connectWebSocket(services.echo, {reconnect = true}))
    self.socket = socket
    self:on(socket, 'open', function()
        self:note('Connected to "' .. socket.url .. '".')
        self:flush()
    end)
    self:on(socket, 'message', function(data, binary)
        self:receive(data, binary)
    end)
    self:on(socket, 'disconnect', function(code)
        self:note('The connection dropped with the code ' .. code .. '.')
    end)
    self:on(socket, 'error', function(message)
        self:note(message)
    end)
    -- A few messages wait in the queue from the start, so the chat opens with a conversation.
    for _, text in ipairs(Chat.openers) do
        self.draft = text
        self:write()
    end
end

function Chat:update(dt)
    Chat.super.update(self, dt)
    local state = self.socket.state == 'reconnecting' and 'reconnecting ' .. self.socket.attempt or self.socket.state
    if state ~= self.state then
        self.state = state
        local label, tone = table.unpack(Chat.states[self.socket.state])
        self:set('state', {text = self.socket.state == 'reconnecting' and label .. ', attempt ' .. self.socket.attempt or label, tone = tone})
    end
end

-- Sends the draft now when the socket is open, and queues it otherwise.
function Chat:write()
    if self.draft == '' then
        return
    end
    local message = json.encode({name = self.name, text = self.draft, time = readout.clock()})
    self.queue[#self.queue + 1] = message
    self.draft = ''
    self:set('message', {value = ''})
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
    self:set('queued', {text = #self.queue .. ' waiting', visible = #self.queue > 0})
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
    self:add('note', {text = text, time = readout.clock()})
end

function Chat:add(kind, message)
    table.insert(self.messages, {kind = kind, message = message})
    if #self.messages > Chat.limit then
        table.remove(self.messages, 1)
    end
    self:showMessages()
end

-- Shows the messages from the oldest to the newest, which the scroll keeps in view: mine on the right, the echoes on the left with the avatar of the name and notes in the middle.
function Chat:showMessages()
    local nodes = {}
    for index, entry in ipairs(self.messages) do
        local message = entry.message
        local id = 'message-' .. index
        if entry.kind == 'note' then
            nodes[index] = ui.label{id = id, text = message.time .. '  ' .. message.text, font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'}
        else
            local mine = entry.kind == 'mine'
            local bubble = ui.card{maxWidth = 820, gap = 4, padding = {12, 18}, style = mine and {colors = {raised = 'accentBackground', border = 'accent'}} or nil,
                ui.label{text = (mine and 'You' or 'Echo of ' .. message.name) .. ', ' .. message.time, font = 'caption', color = mine and 'accentText' or 'successText'},
                ui.label{text = message.text},
            }
            local avatar = ui.avatar{name = message.name, size = 48}
            nodes[index] = ui.row{id = id, gap = 12, alignItems = 'end', align = 'stretch', children = mine and {ui.spacer{grow = 1}, bubble, avatar} or {avatar, bubble, ui.spacer{grow = 1}}}
        end
    end
    self.gui:replaceChildren('messages', nodes)
    if #nodes > 0 then
        self.gui:command('scroll', 'scrollTo', {node = 'message-' .. #nodes, align = 'end'})
    end
end

return Chat
