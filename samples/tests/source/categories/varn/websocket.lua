-- WebSocket between the two halves of the engine: the local server of the category answers with "app:ws" of Varn's "http" module, and the app connects with "haylen.net".
local async = require('async')
local datetime = require('datetime')
local haylen = require('haylen')
local net = require('haylen.net')

local Exchange = require('categories.varn.exchange')
local localServer = require('categories.varn.local-server')
local VarnTest = require('categories.varn.varn-test')

local WebSocket = haylen.class('WebSocket', VarnTest)

WebSocket.hint = 'The server runs inside the app on 127.0.0.1, and nothing leaves the device. Run again opens a new connection.'
WebSocket.wait = 3000

WebSocket.excerpts = {
    {'Server', [[
app:ws('/echo', {
    open = function(conn)
        conn:send('Welcome.')
    end,
    message = function(conn, data)
        local news = data:match('^/broadcast (.+)$')
        if news then
            app:wsBroadcast('/echo', news)
            return
        end
        conn:send('Echo: ' .. data)
    end,
})]]},
    {'Client', [[
local socket = net.connectWebSocket(url)
socket:on('message', function(data, binary)
    print(data, binary)
end, {owner = self})
socket:send('Hello, server.')
socket:sendBinary(string.pack('<I2f', 7, 1.5))
socket:ping('beat')
socket:close(1000, 'Done.')]]},
}

function WebSocket:init(entry)
    WebSocket.super.init(self, entry)
    self.exchange = Exchange('App', 'Server')
end

-- Opens a socket whose events the task of the test awaits one by one, through a queue that the listeners fill.
function WebSocket:run()
    self.exchange:clear()
    local url = localServer.start().ws
    local started = datetime.now():millis()
    local socket = net.connectWebSocket(url)
    local open <close> = VarnTest.closing(socket)
    self.inbox = {}
    for _, event in ipairs({'open', 'message', 'pong', 'close', 'error'}) do
        socket:on(event, function(...)
            self:receive(event, ...)
        end, {owner = self})
    end

    self.results:set('open', 'waiting', 'Open', string.format('Connecting to "%s".', url))
    local opened, problem = self:nextEvent('open')
    if not opened then
        self:check('open', 'Open', false, problem and problem.event == 'error' and 'The connection failed: ' .. problem[1] or 'The connection did not open within 3 seconds.')
        return
    end
    local welcome = self:nextEvent('message')
    self:check('open', 'Open', welcome ~= nil and welcome[1] == 'Welcome.', string.format('Connected to "%s" after %d ms, and the server said "%s".', url, datetime.now():millis() - started, tostring(welcome and welcome[1])))

    self:text(socket)
    self:binary(socket)
    self:broadcast(socket)
    self:ping(socket)
    self:closeSocket(socket)
end

-- Queues what a listener heard and wakes the task when it waits for it.
function WebSocket:receive(event, ...)
    self.inbox[#self.inbox + 1] = {event = event, ...}
    if event == 'message' then
        self.exchange:add(2, Exchange.printable((...)))
    elseif event == 'pong' then
        self.exchange:add(2, 'Pong "' .. (...) .. '"')
    end
    if self.wake then
        local wake = self.wake
        self.wake = nil
        wake()
    end
end

-- Returns the arguments of the next event `name`, or nil and the error or the close that came first, or nil when nothing comes in time.
function WebSocket:nextEvent(name)
    local deadline = datetime.now():millis() + WebSocket.wait
    while datetime.now():millis() < deadline do
        local entry = table.remove(self.inbox, 1)
        if entry then
            if entry.event == name then
                return entry
            end
            if entry.event == 'error' or entry.event == 'close' then
                return nil, entry
            end
        else
            local arrived
            arrived, self.wake = async.deferred()
            async.timeout(arrived, deadline - datetime.now():millis()):await()
        end
    end
    return nil
end

function WebSocket:text(socket)
    self.exchange:add(1, 'Hello, server.')
    socket:send('Hello, server.')
    local reply = self:nextEvent('message')
    self:check('text', 'Text', reply ~= nil and reply[1] == 'Echo: Hello, server.' and not reply[2], string.format('Sent "Hello, server." and got "%s" back as text.', tostring(reply and reply[1])))
end

-- The server of Varn answers every message in a text frame, so the bytes come back whole in a text message.
function WebSocket:binary(socket)
    local packed = string.pack('<I2f', 7, 1.5)
    self.exchange:add(1, Exchange.printable(packed))
    socket:sendBinary(packed)
    local reply = self:nextEvent('message')
    local echoed = reply and reply[1]:sub(#'Echo: ' + 1)
    local count, speed = string.unpack('<I2f', echoed or string.rep('\0', 6))
    self:check('binary', 'Binary', echoed == packed, string.format('Sent %d bytes packed from 7 and 1.5, and the server echoed %d, %s in a text message, the only kind it sends.', #packed, count, speed))
end

function WebSocket:broadcast(socket)
    self.exchange:add(1, '/broadcast Low tide.')
    socket:send('/broadcast Low tide.')
    local news = self:nextEvent('message')
    self:check('broadcast', 'Broadcast', news ~= nil and news[1] == 'Low tide.', string.format('The server sent "%s" to every connection of "/echo" with "app:wsBroadcast".', tostring(news and news[1])))
end

function WebSocket:ping(socket)
    self.exchange:add(1, 'Ping "beat"')
    socket:ping('beat')
    local pong = self:nextEvent('pong')
    self:check('ping', 'Ping', pong ~= nil and pong[1] == 'beat', string.format('The server answered the ping with a pong that carries "%s".', tostring(pong and pong[1])))
end

function WebSocket:closeSocket(socket)
    local closesBefore = localServer.closes
    self.exchange:add(1, 'Close 1000 "Done."')
    socket:close(1000, 'Done.')
    local closed = self:nextEvent('close')
    async.sleep(100):await()
    self:check('close', 'Close', closed ~= nil and closed[1] == 1000 and localServer.closes > closesBefore, string.format('Closed with %s "%s", and the "close" handler of the server ran.', tostring(closed and closed[1]), tostring(closed and closed[2])))
end

function WebSocket:draw(area)
    local y = self.results:draw(area)
    self.exchange:draw(24, area.width - 24, y)
end

return WebSocket
