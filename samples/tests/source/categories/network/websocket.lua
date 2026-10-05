-- The function `net.connectWebSocket` connects in the background and reports `open`, `message`, `error` and `close`, `send` and `sendBinary` send text and raw bytes, and `close` ends the connection with a code. The socket answers the pings of the server by itself, so the ping here is a message the echo server sends back, which measures the round trip.
local haylen = require('haylen')
local net = require('haylen.net')
local ui = require('haylen.ui')

local SocketTest = require('categories.network.socket-test')
local readout = require('categories.network.readout')
local services = require('categories.network.services')

local WebSocket = haylen.class('WebSocket', SocketTest)

WebSocket.tones = {connecting = 'information', open = 'success', reconnecting = 'warning', closing = 'warning', closed = 'danger'}
WebSocket.ping = 'ping '

function WebSocket:init(entry)
    WebSocket.super.init(self, entry)
    self.text = 'Hello from Haylen!'
    self.sent = 0
    self.lines = {}
    self.logged = 0
end

function WebSocket:enter()
    self:frame{
        hint = 'Connect, then send text, binary data or a ping. The echo server sends everything back. Close ends the connection with the code 1000, or 4000 for a code of the app.',
        focus = 'connect',
        content = {ui.row{grow = 1, gap = 24,
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
                        self:sendText(WebSocket.ping .. readout.millis())
                    end},
                },
                ui.label{id = 'roundTrip', text = 'No ping yet.', color = 'accentText'},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'What happened'},
                ui.scroll{grow = 1, focusable = false, ui.list{id = 'events', items = {}}},
            },
        }},
    }
    self:connect()
end

function WebSocket:update(dt)
    WebSocket.super.update(self, dt)
    local state = self.socket and self.socket.state or 'not connected'
    if state ~= self.state then
        self.state = state
        self:set('state', {text = 'State "' .. state .. '"', tone = WebSocket.tones[state] or 'neutral'})
    end
end

function WebSocket:record(text, detail)
    self.logged = self.logged + 1
    table.insert(self.lines, 1, {id = 'line-' .. self.logged, text = text, caption = readout.clock() .. (detail and '  ' .. detail or '')})
    self.lines[80] = nil
    self:set('events', {items = self.lines})
end

-- Opens a new socket unless one is open or on its way, and listens to all of its events until the test ends.
function WebSocket:connect()
    if self.socket and self.socket.state ~= 'closed' then
        self:record('The socket is "' .. self.socket.state .. '" already')
        return
    end
    local socket = self:keep(net.connectWebSocket(services.echo))
    self.socket = socket
    self:record('Called "net.connectWebSocket(url)"', 'Connecting to "' .. socket.url .. '"')
    self:on(socket, 'open', function()
        self:record('Event "open"', 'The connection is up')
    end)
    self:on(socket, 'message', function(data, binary)
        self:receive(data, binary)
    end)
    self:on(socket, 'error', function(message)
        self:record('Event "error"', message)
    end)
    self:on(socket, 'close', function(code, reason)
        self:record('Event "close"', string.format('Code %d%s', code, reason ~= '' and ', ' .. reason or ''))
    end)
end

function WebSocket:close(code, reason)
    if self.socket then
        self.socket:close(code, reason)
        self:record(string.format('Called "socket:close(%d)"', code), 'The state is "' .. self.socket.state .. '"')
    end
end

-- Sending needs an open socket, so a press before it opens tells the player instead of raising.
function WebSocket:ready()
    if self.socket and self.socket.state == 'open' then
        return true
    end
    self:record('Not sent', 'The socket is not open')
    return false
end

function WebSocket:sendText(text)
    if self:ready() then
        self.socket:send(text)
        self:record('Called "socket:send"', text)
    end
end

-- Packs a counter and the time into eight little-endian bytes.
function WebSocket:sendBinary()
    if self:ready() then
        self.sent = self.sent + 1
        local bytes = string.pack('<I4f', self.sent, haylen.elapsed())
        self.socket:sendBinary(bytes)
        self:record('Called "socket:sendBinary"', #bytes .. ' bytes: ' .. readout.hex(bytes))
    end
end

function WebSocket:receive(data, binary)
    if binary and #data == 8 then
        local count, time = string.unpack('<I4f', data)
        self:record('Event "message", binary', string.format('%d bytes: %s, counter %d, time %.2f', #data, readout.hex(data), count, time))
        return
    end
    if binary then
        self:record('Event "message", binary', #data .. ' bytes: ' .. readout.hex(data))
        return
    end
    local sentAt = data:match('^' .. WebSocket.ping .. '(%d+)$')
    if sentAt then
        self:set('roundTrip', {text = string.format('The round trip took %d ms.', readout.millis() - tonumber(sentAt))})
    end
    self:record('Event "message", text', data)
end

return WebSocket
