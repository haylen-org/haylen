-- TCP and UDP with Varn's "socket" module: an echo server that the test starts on 127.0.0.1, text and binary messages, the end of a stream, closing a listener and datagrams.
local async = require('async')
local datetime = require('datetime')
local haylen = require('haylen')
local socket = require('socket')

local Exchange = require('categories.varn.exchange')
local VarnTest = require('categories.varn.varn-test')

local Sockets = haylen.class('Sockets', VarnTest)

Sockets.host = '127.0.0.1'
-- Varn listeners share their port with any other Varn listener on it, so each run takes a random port of a high range, where another copy of the app is unlikely to listen.
Sockets.ports = {49000, 49899}
Sockets.hint = 'The server and the client both run in the app, on 127.0.0.1. Run again opens new sockets on another port.'

Sockets.excerpts = {
    {'Server', [[
local listener = socket.tcp.listen('127.0.0.1', port):await()
self:spawn(function()
    local peer = listener:accept():await()
    while true do
        local data = peer:receive():await()
        if not data or data == '' then
            break
        end
        peer:send('Echo: ' .. data):await()
    end
    peer:close():await()
end)]]},
    {'Client', [[
local client = socket.tcp.connect('127.0.0.1', port, 1000):await()
client:send('Hello, island.'):await()
print(client:receive():await())
client:send(string.pack('<I4dz', 7, 3.25, 'crab')):await()
client:close():await()]]},
    {'Datagrams', [[
local a = socket.udp.bind('127.0.0.1', port + 1):await()
local b = socket.udp.bind('127.0.0.1', port + 2):await()
b:sendTo('127.0.0.1', port + 1, 'Ping'):await()
local packet = a:recvFrom():await()
a:sendTo(packet.host, packet.port, 'Pong'):await()]]},
}

function Sockets:init(entry)
    Sockets.super.init(self, entry)
    self.exchange = Exchange('Client', 'Server')
end

function Sockets:run()
    self.exchange:clear()
    local port = math.random(Sockets.ports[1], Sockets.ports[2])
    local listener, failure = socket.tcp.listen(Sockets.host, port):await()
    if not listener then
        self:check('listen', 'Listen', false, 'The listener failed: ' .. tostring(failure))
        return
    end
    local listening <close> = VarnTest.closing(listener)
    self:check('listen', 'Listen', true, string.format('A listener waits on %s:%d.', Sockets.host, port))

    local served = self:serve(listener)
    self:talk(port, served)
    self:closeListener(listener, port, served)
    self:datagrams(port)
end

-- Serves one client at a time until the listener closes, and returns a record of what the server saw.
function Sockets:serve(listener)
    local served = {}
    served.ended, served.resolveEnded = async.deferred()
    served.stopped, served.resolveStopped = async.deferred()
    self:spawn(function()
        while true do
            local peer, failure = listener:accept():await()
            if not peer then
                served.acceptFailure = failure
                served.resolveStopped()
                return
            end
            local open <close> = VarnTest.closing(peer)
            while true do
                local data = peer:receive():await()
                if not data or data == '' then
                    served.lastRead = data
                    break
                end
                peer:send('Echo: ' .. data):await()
                self.exchange:add(2, 'Echo: ' .. Exchange.printable(data))
            end
            peer:close():await()
            served.resolveEnded()
        end
    end)
    return served
end

function Sockets:talk(port, served)
    self.results:set('echo', 'waiting', 'Text', 'Connecting.')
    local started = datetime.now():millis()
    local client, failure = socket.tcp.connect(Sockets.host, port, 1000):await()
    if not client then
        self:check('echo', 'Text', false, 'The connection failed: ' .. tostring(failure))
        return
    end
    local open <close> = VarnTest.closing(client)

    self.exchange:add(1, 'Hello, island.')
    client:send('Hello, island.'):await()
    local reply = client:receive():await()
    self:check('echo', 'Text', reply == 'Echo: Hello, island.', string.format('Sent "Hello, island." and read "%s" %d ms after connecting.', tostring(reply), datetime.now():millis() - started))

    local packed = string.pack('<I4dz', 7, 3.25, 'crab')
    self.exchange:add(1, Exchange.printable(packed))
    client:send(packed):await()
    local echoed = client:receive():await() or ''
    local count, speed, name = string.unpack('<I4dz', echoed, #'Echo: ' + 1)
    self:check('binary', 'Binary data', count == 7 and speed == 3.25 and name == 'crab', string.format('Packed 7, 3.25 and "crab" into %d bytes with "string.pack", and %d, %s and "%s" came back.', #packed, count, speed, name))

    self.exchange:add(1, 'Close')
    client:close():await()
    served.ended:await()
    self:check('end', 'The end of a stream', served.lastRead == '', 'After the client closed, the server read an empty string and closed its side too.')
end

-- Closing a listener ends the accept that waits on it, and the port refuses connections from then on.
function Sockets:closeListener(listener, port, served)
    listener:close():await()
    served.stopped:await()
    local started = datetime.now():millis()
    local client, failure = socket.tcp.connect(Sockets.host, port, 1000):await()
    if client then
        client:close()
    end
    self:check('close', 'Closing the listener', served.acceptFailure ~= nil and client == nil, string.format('The waiting accept ended with "%s", and a new connection was refused after %d ms: %s', tostring(served.acceptFailure), datetime.now():millis() - started, tostring(failure)))
end

function Sockets:datagrams(port)
    self.results:set('udp', 'waiting', 'Datagrams', 'Binding two UDP sockets.')
    local a, failure = socket.udp.bind(Sockets.host, port + 1):await()
    if not a then
        self:check('udp', 'Datagrams', false, 'Binding failed: ' .. tostring(failure))
        return
    end
    local openA <close> = VarnTest.closing(a)
    local b, other = socket.udp.bind(Sockets.host, port + 2):await()
    if not b then
        self:check('udp', 'Datagrams', false, 'Binding failed: ' .. tostring(other))
        return
    end
    local openB <close> = VarnTest.closing(b)

    b:sendTo(Sockets.host, port + 1, 'Ping'):await()
    local packet = a:recvFrom():await()
    a:sendTo(packet.host, packet.port, 'Pong'):await()
    local answer = b:recvFrom():await()
    self:check('udp', 'Datagrams', packet.data == 'Ping' and answer.data == 'Pong' and packet.port == port + 2, string.format('Port %d got "%s" from %s:%d and answered "%s".', port + 1, packet.data, packet.host, packet.port, answer.data))
end

function Sockets:draw(area)
    local y = self.results:draw(area)
    self.exchange:draw(24, area.width - 24, y)
end

return Sockets
