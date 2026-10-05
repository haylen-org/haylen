-- WebSockets of "haylen.net" against the local server of the app, through what a connection meets outside a demo: a large message, sends faster than the server reads, a server that ends the connection, a server that never answers, a port where nothing listens and a socket that ends with its owner.
local async = require('async')
local datetime = require('datetime')
local haylen = require('haylen')
local net = require('haylen.net')
local socket = require('socket')

local localServer = require('categories.varn.local-server')
local VarnTest = require('categories.varn.varn-test')

local LocalWebSocket = haylen.class('LocalWebSocket', VarnTest)

LocalWebSocket.hint = 'The server runs inside the app on 127.0.0.1, and nothing leaves the device. Run again opens new connections.'
LocalWebSocket.wait = 5000
LocalWebSocket.largeSize = 1024 * 1024
LocalWebSocket.burst = {count = 4, size = 512 * 1024}
-- The silent listener takes a random port of a range of its own, because Varn listeners share their port with any other Varn listener on it.
LocalWebSocket.ports = {49900, 50899}

LocalWebSocket.excerpts = {
    {'Large messages and waiting bytes', [[
local socket = net.connectWebSocket(url)
socket:send(string.rep('0123456789abcdef', 65536))
print(socket.bufferedAmount)]]},
    {'Dropped connections', [[
local socket = net.connectWebSocket(url, {
    reconnect = {initialDelay = 0.1, maxDelay = 0.1, maxAttempts = 3},
})
socket:on('disconnect', function(code) print(code) end)
socket:on('open', function() print(socket.attempt) end)]]},
    {'Failures and owners', [[
net.connectWebSocket(silentUrl, {connectTimeout = 0.5})
net.connectWebSocket('ws://127.0.0.1:1/')
net.connectWebSocket(url, {owner = holder})]]},
}

function LocalWebSocket:run()
    local url = localServer.start().ws
    self:large(url)
    self:backpressure(url)
    self:dropped(url)
    self:timeout()
    self:refused()
    self:owned(url)
end

-- Opens a socket whose events the task awaits one by one, through a queue that its listeners fill.
function LocalWebSocket:open(url, options)
    local opened = net.connectWebSocket(url, options)
    local inbox = {}
    for _, event in ipairs({'open', 'message', 'disconnect', 'reconnecting', 'close', 'error'}) do
        opened:on(event, function(...)
            inbox[#inbox + 1] = {event = event, ...}
            if inbox.wake then
                local wake = inbox.wake
                inbox.wake = nil
                wake()
            end
        end, {owner = self})
    end
    return opened, inbox
end

-- Returns the arguments of the next event `name`, or nil and the event that came first instead, or nil when nothing comes in time.
function LocalWebSocket:nextEvent(inbox, name)
    local deadline = datetime.now():millis() + LocalWebSocket.wait
    while true do
        local entry = table.remove(inbox, 1)
        if entry then
            if entry.event == name then
                return entry
            end
            return nil, entry
        end
        local left = deadline - datetime.now():millis()
        if left <= 0 then
            return nil
        end
        local arrived
        arrived, inbox.wake = async.deferred()
        async.timeout(arrived, left):await()
    end
end

-- Describes the event that came instead of the expected one, for the detail of a failed check.
function LocalWebSocket.describe(entry)
    if not entry then
        return 'nothing within 5 seconds'
    end
    local values = {}
    for index, value in ipairs(entry) do
        values[index] = tostring(value)
    end
    return string.format('the event "%s" with "%s"', entry.event, table.concat(values, '", "'))
end

-- Waits for the socket to open and for the greeting of the server, or fails the check `key`.
function LocalWebSocket:greeted(inbox, key, name)
    local opened, other = self:nextEvent(inbox, 'open')
    local welcome
    if opened then
        welcome, other = self:nextEvent(inbox, 'message')
    end
    if welcome and welcome[1] == 'Welcome.' then
        return true
    end
    self:check(key, name, false, 'The socket did not open and hear the greeting of the server, and got ' .. LocalWebSocket.describe(other) .. '.')
    return false
end

function LocalWebSocket:large(url)
    local name = 'Large messages'
    self.results:set('large', 'waiting', name, string.format('Sending %d bytes to "%s".', LocalWebSocket.largeSize, url))
    local opened, inbox = self:open(url)
    local open <close> = VarnTest.closing(opened)
    if not self:greeted(inbox, 'large', name) then
        return
    end

    local payload = string.rep('0123456789abcdef', LocalWebSocket.largeSize // 16)
    local started = datetime.now():millis()
    opened:send(payload)
    local reply, other = self:nextEvent(inbox, 'message')
    local whole = reply ~= nil and reply[1] == 'Echo: ' .. payload
    self:check('large', name, whole, string.format('Sent %d bytes in one message, and %s after %d ms.', #payload, whole and 'the whole echo came back' or 'got ' .. LocalWebSocket.describe(reply or other), datetime.now():millis() - started))
end

-- The sends queue faster than the server reads, so "bufferedAmount" counts the bytes that wait and falls back to zero once the server took them all.
function LocalWebSocket:backpressure(url)
    local name = 'Bytes waiting to be sent'
    local burst = LocalWebSocket.burst
    self.results:set('buffered', 'waiting', name, string.format('Sending %d messages of %d KiB at once.', burst.count, burst.size // 1024))
    local opened, inbox = self:open(url)
    local open <close> = VarnTest.closing(opened)
    if not self:greeted(inbox, 'buffered', name) then
        return
    end

    local chunk = string.rep('b', burst.size)
    local started = datetime.now():millis()
    for _ = 1, burst.count do
        opened:send(chunk)
    end
    local waiting = opened.bufferedAmount

    local echoes, other = 0, nil
    while echoes < burst.count do
        local reply
        reply, other = self:nextEvent(inbox, 'message')
        if not reply then
            break
        end
        echoes = echoes + 1
    end
    local left = opened.bufferedAmount
    local detail = string.format('Right after the sends %d bytes waited, %d of %d echoes came back after %d ms, and %d bytes wait now.', waiting, echoes, burst.count, datetime.now():millis() - started, left)
    if echoes < burst.count then
        detail = detail .. ' The socket got ' .. LocalWebSocket.describe(other) .. '.'
    end
    self:check('buffered', name, waiting > 0 and echoes == burst.count and left == 0, detail)
end

-- The server ends the connection when it reads "/drop", and a socket with "reconnect" opens a new one on its own and hears the greeting again.
function LocalWebSocket:dropped(url)
    local name = 'Reconnection'
    self.results:set('reconnect', 'waiting', name, 'Asking the server to end the connection.')
    local opened, inbox = self:open(url, {reconnect = {initialDelay = 0.1, maxDelay = 0.1, maxAttempts = 3}})
    local open <close> = VarnTest.closing(opened)
    if not self:greeted(inbox, 'reconnect', name) then
        return
    end

    local started = datetime.now():millis()
    opened:send('/drop')
    local lost, other = self:nextEvent(inbox, 'disconnect')
    local scheduled = lost and self:nextEvent(inbox, 'reconnecting')
    local again = scheduled and self:greeted(inbox, 'reconnect', name)
    if not lost or not scheduled then
        self:check('reconnect', name, false, 'The server ended the connection, and the socket got ' .. LocalWebSocket.describe(other) .. '.')
        return
    end
    if again then
        self:check('reconnect', name, opened.state == 'open', string.format('The server closed with %s, the socket waited %.1f seconds before attempt %d and was open again with the greeting after %d ms.', tostring(lost[1]), scheduled[2], scheduled[1], datetime.now():millis() - started))
    end
end

-- A listener that never accepts leaves the upgrade request without an answer, so only "connectTimeout" ends the attempt.
function LocalWebSocket:timeout()
    local name = 'Connect timeout'
    local port = math.random(LocalWebSocket.ports[1], LocalWebSocket.ports[2])
    local listener, failure = socket.tcp.listen(localServer.host, port):await()
    if not listener then
        self:check('timeout', name, false, 'The silent listener failed: ' .. tostring(failure))
        return
    end
    local listening <close> = VarnTest.closing(listener)
    local url = string.format('ws://%s:%d/silent', localServer.host, port)
    self.results:set('timeout', 'waiting', name, string.format('Waiting at most half a second for "%s", which never answers.', url))

    local started = datetime.now():millis()
    local opened, inbox = self:open(url, {connectTimeout = 0.5})
    local open <close> = VarnTest.closing(opened)
    local _, failed = self:nextEvent(inbox, 'open')
    local closed = self:nextEvent(inbox, 'close')
    local elapsed = datetime.now():millis() - started
    local message = failed and failed.event == 'error' and failed[1]
    local expected = message and message:find('did not open within 0.5 seconds', 1, true) ~= nil
    self:check('timeout', name, expected and closed ~= nil and closed[1] == 1006 and elapsed >= 450, string.format('After %d ms the socket reported "%s" and closed with %s.', elapsed, tostring(message or LocalWebSocket.describe(failed)), tostring(closed and closed[1])))
end

-- Nothing listens on port 1, so the system refuses the connection at once, and the error says so.
function LocalWebSocket:refused()
    local name = 'No server'
    local url = 'ws://127.0.0.1:1/'
    self.results:set('refused', 'waiting', name, string.format('Connecting to "%s".', url))
    local started = datetime.now():millis()
    local opened, inbox = self:open(url)
    local open <close> = VarnTest.closing(opened)
    local _, failed = self:nextEvent(inbox, 'open')
    local message = failed and failed.event == 'error' and failed[1]
    self:check('refused', name, message and message:find('refused', 1, true) ~= nil, string.format('After %d ms the socket reported "%s".', datetime.now():millis() - started, tostring(message or LocalWebSocket.describe(failed))))
end

-- A socket that an owner holds closes when the owner ends, here a table that the test lets go and the garbage collector takes.
function LocalWebSocket:owned(url)
    local name = 'Closed with its owner'
    self.results:set('owner', 'waiting', name, 'Opening a socket that a table owns.')
    self.holder = {}
    local opened, inbox = self:open(url, {owner = self.holder})
    local open <close> = VarnTest.closing(opened)
    if not self:greeted(inbox, 'owner', name) then
        return
    end

    local closesBefore = localServer.closes
    self.holder = nil
    collectgarbage()
    collectgarbage()
    local lost, other = self:nextEvent(inbox, 'disconnect')
    local closed
    if lost then
        closed, other = self:nextEvent(inbox, 'close')
    end
    async.sleep(100):await()
    self:check('owner', name, closed ~= nil and closed[1] == 1000 and localServer.closes > closesBefore, string.format('Once the owner was collected the socket %s, and the server saw %d more close.', closed and string.format('closed with %d', closed[1]) or 'got ' .. LocalWebSocket.describe(other), localServer.closes - closesBefore))
end

return LocalWebSocket
