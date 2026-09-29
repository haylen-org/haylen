-- WebSocket echo with haylen.net: net.websocket connects in the background and reports open, message, error and close, send and sendBinary send text and raw bytes, and close ends the connection with a code. The socket answers the pings of the server by itself, so the ping here is a message the echo server sends back, which measures the round trip.
local haylen = require('haylen')
local net = require('haylen.net')
local ui = require('haylen.ui')

local sample = require('sample')
local services = require('services')

local WebSocket = haylen.class('WebSocket', sample.Test)

WebSocket.hints = 'Connect, then send text, binary data or a ping. The echo server sends everything back. Close ends the connection with code 1000, or 4000 for a code of the app.'
WebSocket.focus = 'connect'

local kStates = {connecting = 'information', open = 'success', reconnecting = 'warning', closing = 'warning', closed = 'danger'}
local kPing = 'ping '

function WebSocket:init(entry)
    WebSocket.super.init(self, entry)
    self.text = 'Hello from Haylen!'
    self.sent = 0
    self.lines = {}
    self.logged = 0
end

function WebSocket:content()
    return {
        ui.panel{width = 640, align = 'stretch', gap = 12,
            ui.statusIndicator{id = 'state', text = 'Not connected', tone = 'neutral'},
            ui.label{text = services.echo, font = 'monospace', color = 'textMuted'},
            ui.row{gap = 12,
                ui.button{id = 'connect', text = 'Connect', variant = 'primary', grow = 1, onClick = function()
                    self:connect()
                end},
                ui.button{id = 'close', text = 'Close', grow = 1, onClick = function()
                    self:close(1000, 'bye')
                end},
                ui.button{id = 'closeApp', text = 'Close 4000', grow = 1, onClick = function()
                    self:close(4000, 'the app is done')
                end},
            },
            ui.sectionTitle{text = 'Send'},
            ui.textField{id = 'message', value = self.text, maxLength = 200, returnKey = 'send', onChange = function(event)
                self.text = event.value
            end, onSubmit = function()
                self:sendText(self.text)
            end},
            ui.grid{columns = 3, gap = 12,
                ui.button{id = 'send', text = 'Send text', align = 'stretch', onClick = function()
                    self:sendText(self.text)
                end},
                ui.button{id = 'binary', text = 'Send binary', align = 'stretch', onClick = function()
                    self:sendBinary()
                end},
                ui.button{id = 'ping', text = 'Ping', align = 'stretch', onClick = function()
                    self:sendText(kPing .. sample.millis())
                end},
            },
            ui.label{id = 'roundTrip', text = 'No ping yet.', color = 'accentText'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'What happened'},
            ui.scroll{grow = 1, focusable = false, ui.list{id = 'log', items = {}}},
        },
    }
end

function WebSocket:enter()
    WebSocket.super.enter(self)
    self:connect()
end

function WebSocket:update(dt)
    local state = self.socket and self.socket.state or 'not connected'
    if state ~= self.state then
        self.state = state
        self:show('state', {text = 'State: ' .. state, tone = kStates[state] or 'neutral'})
    end
end

function WebSocket:log(text, detail)
    self.logged = self.logged + 1
    table.insert(self.lines, 1, {id = 'line-' .. self.logged, text = text, caption = sample.clock() .. (detail and '  ' .. detail or '')})
    self.lines[80] = nil
    self:show('log', {items = self.lines})
end

-- Opens a new socket unless one is open or on its way, and listens to all of its events until the test ends.
function WebSocket:connect()
    if self.socket and self.socket.state ~= 'closed' then
        self:log('The socket is ' .. self.socket.state .. ' already')
        return
    end
    local socket = self:keep(net.websocket(services.echo))
    self.socket = socket
    self:log('net.websocket(url)', 'connecting to ' .. socket.url)
    self:on(socket, 'open', function()
        self:log('open', 'the connection is up')
    end)
    self:on(socket, 'message', function(data, binary)
        self:receive(data, binary)
    end)
    self:on(socket, 'error', function(message)
        self:log('error', message)
    end)
    self:on(socket, 'close', function(code, reason)
        self:log('close', string.format('code %d%s', code, reason ~= '' and ', ' .. reason or ''))
    end)
end

function WebSocket:close(code, reason)
    if self.socket then
        self.socket:close(code, reason)
        self:log(string.format('socket:close(%d)', code), 'the state is ' .. self.socket.state)
    end
end

-- Sending needs an open socket, so a press before it opens tells the player instead of raising.
function WebSocket:ready()
    if self.socket and self.socket.state == 'open' then
        return true
    end
    self:log('Not sent', 'the socket is not open')
    return false
end

function WebSocket:sendText(text)
    if self:ready() then
        self.socket:send(text)
        self:log('send', text)
    end
end

-- Packs a counter and the time into eight little-endian bytes.
function WebSocket:sendBinary()
    if self:ready() then
        self.sent = self.sent + 1
        local bytes = string.pack('<I4f', self.sent, haylen.time())
        self.socket:sendBinary(bytes)
        self:log('sendBinary', #bytes .. ' bytes: ' .. sample.hex(bytes))
    end
end

function WebSocket:receive(data, binary)
    if binary and #data == 8 then
        local count, time = string.unpack('<I4f', data)
        self:log('message, binary', string.format('%d bytes: %s = counter %d, time %.2f', #data, sample.hex(data), count, time))
        return
    end
    if binary then
        self:log('message, binary', #data .. ' bytes: ' .. sample.hex(data))
        return
    end
    local sentAt = data:match('^' .. kPing .. '(%d+)$')
    if sentAt then
        self:show('roundTrip', {text = string.format('Round trip %d ms', sample.millis() - tonumber(sentAt))})
    end
    self:log('message, text', data)
end

return WebSocket
