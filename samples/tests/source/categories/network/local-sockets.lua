-- TCP and UDP of Varn on 127.0.0.1, through what sockets meet outside a demo: a stream larger than any read, sends that wait for the peer, a peer that stays silent, a port where nothing listens and a burst of datagrams.
local async = require('async')
local datetime = require('datetime')
local haylen = require('haylen')
local socket = require('socket')

local VarnTest = require('categories.varn.varn-test')

local LocalSockets = haylen.class('LocalSockets', VarnTest)

LocalSockets.host = '127.0.0.1'
-- Varn listeners share their port with any other Varn listener on it, so each run takes a random port of a range of its own.
LocalSockets.ports = {50900, 51899}
LocalSockets.streamSize = 1024 * 1024
LocalSockets.pieceSize = 64 * 1024
LocalSockets.datagrams = 50
LocalSockets.datagramSize = 1024
LocalSockets.hint = 'The server and the client both run in the app, on 127.0.0.1. Run again opens new sockets on another port.'

LocalSockets.excerpts = {
    {'A large stream', [[
for offset = 1, #data, 65536 do
    client:send(data:sub(offset, offset + 65535)):await()
end
-- Another task reads until every byte came back.
local piece = client:receive():await()]]},
    {'Deadlines and refusals', [[
local pending = client:receive()
local data, failure = async.timeout(pending, 300):await()
socket.tcp.connect('127.0.0.1', 1, 1000):await()]]},
    {'A burst of datagrams', [[
for index = 1, 50 do
    sends[index] = sender:sendTo(host, port, string.pack('>I4', index) .. filler)
end
async.all(sends):await()
local packet = receiver:recvFrom():await()]]},
}

function LocalSockets:run()
    local port = math.random(LocalSockets.ports[1], LocalSockets.ports[2])
    local listener, failure = socket.tcp.listen(LocalSockets.host, port):await()
    if not listener then
        self:check('stream', 'A large stream', false, 'The listener failed: ' .. tostring(failure))
        return
    end
    local listening <close> = VarnTest.closing(listener)
    self:serve(listener)
    self:stream(port)
    self:silence(port)
    self:refused()
    self:burst(port)
end

-- Echoes every byte of every client until the listener closes.
function LocalSockets:serve(listener)
    self:spawn(function()
        while true do
            local peer = listener:accept():await()
            if not peer then
                return
            end
            self:spawn(function()
                local open <close> = VarnTest.closing(peer)
                while true do
                    local data = peer:receive():await()
                    if not data or data == '' then
                        return
                    end
                    peer:send(data):await()
                end
            end)
        end
    end)
end

-- Sends a stream larger than any single read in awaited pieces while another task reads the echo, so the sends wait whenever the peer falls behind.
function LocalSockets:stream(port)
    local name = 'A large stream'
    self.results:set('stream', 'waiting', name, string.format('Sending %d bytes through the echo server.', LocalSockets.streamSize))
    local client, failure = socket.tcp.connect(LocalSockets.host, port, 1000):await()
    if not client then
        self:check('stream', name, false, 'The connection failed: ' .. tostring(failure))
        return
    end
    local open <close> = VarnTest.closing(client)

    local data = {}
    for index = 1, LocalSockets.streamSize // 256 do
        data[index] = string.rep(string.char(index % 256), 256)
    end
    data = table.concat(data)
    local received, reads = {}, 0
    local done, finish = async.deferred()
    self:spawn(function()
        local total = 0
        while total < #data do
            local piece = client:receive():await()
            if not piece or piece == '' then
                break
            end
            received[#received + 1] = piece
            total = total + #piece
            reads = reads + 1
        end
        finish()
    end)

    local started = datetime.now():millis()
    for offset = 1, #data, LocalSockets.pieceSize do
        client:send(data:sub(offset, offset + LocalSockets.pieceSize - 1)):await()
    end
    async.timeout(done, 10000):await()
    local elapsed = datetime.now():millis() - started
    local echoed = table.concat(received)
    self:check('stream', name, echoed == data, string.format('Sent %d bytes in pieces of %d KiB, and %d bytes came back in order in %d reads after %d ms.', #data, LocalSockets.pieceSize // 1024, #echoed, reads, elapsed))
end

-- The server reads and never writes to a client that sends nothing, so only a deadline of the app ends the wait, and closing the socket releases the receive.
function LocalSockets:silence(port)
    local name = 'A silent peer'
    self.results:set('silence', 'waiting', name, 'Waiting at most 300 ms for a server that has nothing to say.')
    local client, failure = socket.tcp.connect(LocalSockets.host, port, 1000):await()
    if not client then
        self:check('silence', name, false, 'The connection failed: ' .. tostring(failure))
        return
    end

    local started = datetime.now():millis()
    local pending = client:receive()
    local data, missed = async.timeout(pending, 300):await()
    local elapsed = datetime.now():millis() - started
    client:close():await()
    local after, reason = pending:await()
    local ended = after == nil and string.format('rejected with "%s"', tostring(reason)) or string.format('resolved with %d bytes', #after)
    self:check('silence', name, data == nil and missed ~= nil and elapsed < 1000, string.format('The deadline rejected after %d ms with "%s", and after the close the pending receive %s.', elapsed, tostring(missed), ended))
end

-- Nothing listens on port 1, so the system refuses the connection at once.
function LocalSockets:refused()
    local name = 'No server'
    self.results:set('refused', 'waiting', name, 'Connecting to "127.0.0.1:1".')
    local started = datetime.now():millis()
    local client, failure = socket.tcp.connect(LocalSockets.host, 1, 1000):await()
    if client then
        client:close()
    end
    self:check('refused', name, client == nil and failure ~= nil, string.format('Rejected after %d ms: %s', datetime.now():millis() - started, tostring(failure)))
end

-- Datagrams sent at once with their numbers, which the loopback interface delivers whole and in order.
function LocalSockets:burst(port)
    local name = 'A burst of datagrams'
    self.results:set('burst', 'waiting', name, string.format('Sending %d datagrams of %d bytes.', LocalSockets.datagrams, LocalSockets.datagramSize))
    local receiver, failure = socket.udp.bind(LocalSockets.host, port + 1):await()
    if not receiver then
        self:check('burst', name, false, 'Binding failed: ' .. tostring(failure))
        return
    end
    local openReceiver <close> = VarnTest.closing(receiver)
    local sender, other = socket.udp.bind(LocalSockets.host, port + 2):await()
    if not sender then
        self:check('burst', name, false, 'Binding failed: ' .. tostring(other))
        return
    end
    local openSender <close> = VarnTest.closing(sender)

    local numbers = {}
    local done, finish = async.deferred()
    self:spawn(function()
        while #numbers < LocalSockets.datagrams do
            local packet = receiver:recvFrom():await()
            if not packet or #packet.data ~= LocalSockets.datagramSize then
                break
            end
            numbers[#numbers + 1] = string.unpack('>I4', packet.data)
        end
        finish()
    end)

    local started = datetime.now():millis()
    local filler = string.rep('d', LocalSockets.datagramSize - 4)
    local sends = {}
    for index = 1, LocalSockets.datagrams do
        sends[index] = sender:sendTo(LocalSockets.host, port + 1, string.pack('>I4', index) .. filler)
    end
    async.all(sends):await()
    async.timeout(done, 5000):await()
    local ordered = #numbers == LocalSockets.datagrams
    for index, number in ipairs(numbers) do
        ordered = ordered and number == index
    end
    self:check('burst', name, ordered, string.format('Received %d of %d datagrams, %s, after %d ms.', #numbers, LocalSockets.datagrams, ordered and 'whole and in order' or 'not all in order', datetime.now():millis() - started))
end

return LocalSockets
