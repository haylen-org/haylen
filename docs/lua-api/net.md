# haylen.net

`haylen.net` opens WebSocket connections to `ws://` and `wss://` servers. Use it for real-time multiplayer, chat, live events and any server that pushes messages to the app. For plain HTTP requests use Varn's `http` module, whose timeouts the [Lua guide](../lua.md#asynchronous-code) explains, and for raw TCP use Varn's `socket` module.

```lua
local net = require('haylen.net')
```

## Connections

A socket connects in the background and reports what happens through events. The engine delivers the events of every socket at the start of each frame, in the order they arrived, before the app updates. The engine keeps a socket alive until it closes, so an app may keep only its listeners and let the socket object go. After the `close` event a socket drops its listeners and reports nothing more. When the app stops or restarts, the engine closes every open socket without reporting `close`.

On native builds every socket runs on a thread of its own, which sleeps until the server or the app has something for it. Closing or releasing a socket never waits for that thread, even while it is still connecting, because the app restarts, quits or drops the last reference to it: the socket reports nothing more, and its thread ends the connection on its own and never touches the released socket. Opening the connection and the TLS handshake of a `wss://` address end at once when the socket is released, the upgrade request ends when the server answers or after 10 seconds, and looking up the host name has no time limit of its own. Opening the connection, the TLS handshake and the upgrade request each fail when the server leaves them without an answer for 10 seconds. When the process exits, it waits for connection threads that still use the network libraries, which is at most the 10 seconds of an upgrade request, and never for a thread that is looking up a host name. A socket answers server pings, sends its own with `socket:ping`, joins fragmented messages, refuses messages larger than `maxMessageSize` and checks the certificates of `wss://` servers against the trust store Varn finds, or the system root store on Windows. A missing trust store fails the connection with `No trust store was found to check the certificate of a wss:// server: <searched places>.`. In the browser a socket is a WebSocket of the page, so the browser rules apply, such as a page served over HTTPS reaching only `wss://` addresses and the browser limiting the size of messages instead of `maxMessageSize`. The browser build reports the same events except `pong`, since browsers keep pong frames from the page and cannot send pings, and they reach the app on its next frame. Its `error` event carries `The WebSocket connection to <url> failed.`, or the message of the browser when it rejects the address, and its close codes come from the browser.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/lobby', {protocols = {'lobby.v1'}})

socket:on('open', function()
    socket:send('{"type": "join", "name": "Ana"}')
end)

socket:on('message', function(data, binary)
    if not binary then
        print('server says ' .. data)
    end
end)

socket:on('error', function(message)
    print('connection problem: ' .. message)
end)

socket:on('close', function(code, reason)
    print('closed with ' .. code .. ' ' .. reason)
end)
```

## Reconnection

A socket opened with the `reconnect` option connects again on its own when the connection drops or an attempt fails, until the app closes it. It waits before every attempt: the first attempt waits `initialDelay`, every further attempt waits `multiplier` times longer, up to `maxDelay`, and `jitter` shortens each wait at random by up to that fraction, so many players who lost the same server do not all retry at the same moment. A connection that opens starts the count over. After `maxAttempts` failed attempts in a row the socket gives up and reports `close` with the code of the last failure, and `0` never gives up. Reconnection works the same way on native builds and in the browser.

While it reconnects, the socket keeps its listeners. Each lost connection reports `disconnect`, each scheduled attempt reports `reconnecting` with the attempt number and the wait, and each connection that opens reports `open` again. The event bus hears the same moments as `webSocketDisconnected`, `webSocketReconnecting` and `webSocketConnected`, as [haylen.events](events.md#engine-events) lists. Messages sent while the socket is not open raise an error, so an app queues what it wants to send until the next `open`. `close()` during a wait ends the socket at once with the code it gives.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/lobby', {reconnect = {initialDelay = 1, maxDelay = 20, maxAttempts = 10}})

socket:on('open', function()
    socket:send('{"type": "hello"}')
end)

socket:on('disconnect', function(code, reason)
    print('connection lost with ' .. code)
end)

socket:on('reconnecting', function(attempt, delay)
    print(string.format('attempt %d in %.1f seconds', attempt, delay))
end)

socket:on('close', function(code)
    print('gave up with ' .. code)
end)
```

## States

`socket.state` is one of these names.

| State | Meaning |
| --- | --- |
| `'connecting'` | The socket is connecting. It starts in this state, and a reconnecting socket returns to it for every attempt. |
| `'open'` | The connection is up and `send` works. |
| `'reconnecting'` | The connection dropped or an attempt failed, and the socket waits for its next attempt. |
| `'closing'` | `close` was called and the closing handshake is running. A socket closed while connecting stays in this state even when the connection opens. |
| `'closed'` | The connection is over. The `close` event reports it, and the `disconnect` listeners of a socket that does not reconnect already see this state. |

## Close codes

The `close` event reports the code of the closing handshake.

| Code | Meaning |
| --- | --- |
| `1000` | Normal closure. `close()` sends it by default, and a server that answers a close usually echoes the code it received or sends `1000`. |
| `1001` | Going away. A native socket dropped while open sends it to the server. |
| `1005` | The close frame of the server carried no code, on native builds. |
| `1006` | Abnormal closure. The connection failed, dropped without a close frame, or the server did not finish the closing handshake within 5 seconds on native builds. An `error` event comes first when the connection failed. |
| `1009` | Message too big. On native builds the socket ends the connection with it when the server sends a message larger than `maxMessageSize`, after an `error` event. |
| `3000` to `4999` | Codes the app and its server define. `close()` accepts them. |

Other codes a server sends, such as `1008` or `1011`, reach the `close` event as they are.

## Functions

### net.connectWebSocket(url, options)

Starts connecting to `url` and returns a `haylen.WebSocket` in the `connecting` state. The options table is optional, and unknown keys raise `Unknown option '<key>'.`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `protocols` | list of strings | none | Subprotocols offered to the server in the `Sec-WebSocket-Protocol` header. `socket.protocol` tells which one the server picked. |
| `maxMessageSize` | integer | `16777216` | Largest message in bytes the socket accepts from the server, from `1` to `2147483647`, which is 16 MiB by default. On native builds a larger message reports `error` with `The server sent a WebSocket message larger than the maximum of <size> bytes.` and ends the connection with code `1009`. In the browser the limits of the browser apply instead. |
| `reconnect` | boolean or table | `false` | `true` turns [reconnection](#reconnection) on with the defaults below, and a table turns it on with the settings it changes. |

| Reconnect key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `initialDelay` | number | `0.5` | Seconds before the first attempt. |
| `maxDelay` | number | `30` | Longest wait in seconds, at least `initialDelay`. |
| `multiplier` | number | `2` | How much longer each wait is than the one before, at least `1`. |
| `jitter` | number | `0.5` | Fraction between `0` and `1` that shortens each wait at random. |
| `maxAttempts` | integer | `0` | Failed attempts in a row before the socket closes, where `0` never gives up. |

An address that does not start with `ws://` or `wss://` raises `A WebSocket address starts with ws:// or wss://: <url>`, and a `maxMessageSize` of `0` or above `2147483647` raises `A WebSocket needs a maximum message size between 1 and 2147483647 bytes.`. Reconnect settings outside these ranges raise `WebSocket reconnection needs delays from zero up with the maximum at least the initial one, a multiplier of at least 1, a jitter between 0 and 1 and a maximum of attempts of at least 0.`.

```lua
local net = require('haylen.net')

local chat = net.connectWebSocket('ws://127.0.0.1:8080/chat', {protocols = {'chat', 'json'}})
print(chat.state)

local lobby = net.connectWebSocket('wss://game.example.com/lobby', {reconnect = true})
local scores = net.connectWebSocket('wss://game.example.com/scores', {maxMessageSize = 64 * 1024})
```

### net.openSocketCount()

Returns how many sockets the engine keeps alive, which is every socket from `net.connectWebSocket` that has not reached the `closed` state yet, whether or not the app still holds it. A socket leaves the count at the start of the frame after it closed.

```lua
local net = require('haylen.net')

net.connectWebSocket('ws://127.0.0.1:8080/chat')
print(net.openSocketCount())
```

## WebSocket

A `haylen.WebSocket` is the socket `net.connectWebSocket` returns. Reading a member it does not have raises `The type haylen.WebSocket has no member '<name>'.`.

### socket:on(event, listener, options)

Calls `listener` every time the socket reports `event` and returns a `haylen.Connection`, whose `disconnect()` method stops the listener and whose `connected` property is `true` until then. `options` may hold an `owner`, a table or a userdata such as a scene, and the listener ends with it, as [subscription scopes](../lifecycle.md#subscription-scopes) describe. An unknown option raises `Unknown option '<name>'`. An error raised inside a listener stops the app and shows the error screen with the message and its stack trace. An unknown event raises `Unknown WebSocket event '<event>'. Sockets report open, message, pong, disconnect, reconnecting, close and error.`.

| Event | Listener arguments | Meaning |
| --- | --- | --- |
| `'open'` | none | The connection is up, the first time or after reconnecting. `socket.protocol` holds the subprotocol the server picked. |
| `'message'` | `data` (string), `binary` (boolean) | A whole message arrived. Text messages arrive as UTF-8 strings with `binary` set to `false`, and binary messages arrive as strings of raw bytes with `binary` set to `true`. |
| `'pong'` | `payload` (string) | A pong frame arrived, the answer of the server to [socket:ping](#socketpingpayload) or one it sent to keep the connection alive. Browsers never report pongs to the page, so the browser build never fires it. |
| `'error'` | `message` (string) | The connection failed. A `close` event follows, with code `1006` on native builds, unless the socket reconnects. |
| `'disconnect'` | `code` (integer), `reason` (string) | An open connection ended, before the socket either reconnects or closes. `socket.state` already reads `'reconnecting'` or `'closed'`, so sending from the listener raises an error, and `close()` there ends a reconnecting socket instead of its next attempt. |
| `'reconnecting'` | `attempt` (integer), `delay` (number) | The socket scheduled its next attempt, counted from 1, after waiting `delay` seconds. |
| `'close'` | `code` (integer), `reason` (string) | The socket is done: the app closed it, reconnection is off or it gave up. It is the last event of the socket. |

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match')
local messages = socket:on('message', function(data, binary)
    print((binary and 'binary ' or 'text ') .. #data .. ' bytes')
end)

local function stopListening()
    messages:disconnect()
end

-- A listener owned by a scene ends when the scene unloads.
local lobby = {}
function lobby:load()
    socket:on('message', function(data) print('lobby heard ' .. data) end, {owner = self})
end
```

### socket:send(text)

Sends `text` as a text message. The text should be valid UTF-8, which browsers require. Sending while the socket is not open raises `The WebSocket to <url> is not open.`, so apps send from the `open` event on.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match')
socket:on('open', function()
    socket:send('{"type": "ready"}')
end)
```

### socket:sendBinary(bytes)

Sends the Lua string `bytes` as a binary message, byte for byte. Sending while the socket is not open raises `The WebSocket to <url> is not open.`.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/state')
socket:on('open', function()
    socket:sendBinary(string.pack('<I2ff', 7, 120.5, 48.25))
end)
```

### socket:ping(payload)

Sends a ping frame with the string `payload`, which defaults to an empty string, and the server answers with a pong frame that carries the same payload to the `pong` event. A ping that gets no answer tells a dead connection apart from a quiet one. Native builds send the ping themselves. Browsers answer the pings of a server on their own but give pages no way to send one, so in the browser build `ping` raises `Browsers cannot send WebSocket ping frames.`, and an app that needs a heartbeat there sends an ordinary message that its server answers. Pinging while the socket is not open raises `The WebSocket to <url> is not open.`, and a payload longer than 125 bytes raises `A WebSocket ping carries at most 125 bytes.`.

```lua
local net = require('haylen.net')
local timer = require('haylen.timer')

-- Pings every 5 seconds and gives up on a connection that left three pings in a row unanswered.
local socket = net.connectWebSocket('wss://game.example.com/match')
local unanswered = 0
socket:on('open', function()
    timer.every(5, function()
        if unanswered == 3 then
            socket:close(4000, 'no answer')
            return
        end
        unanswered = unanswered + 1
        socket:ping('heartbeat')
    end, {owner = socket})
end)
socket:on('pong', function(payload)
    unanswered = 0
end)
```

### socket:close(code, reason)

Starts the closing handshake with `code`, which defaults to `1000`, and `reason`, which defaults to an empty string. The state becomes `closing` at once, and the `close` event reports the end. Calling it on a socket that is already closing or closed does nothing. A code other than `1000` or `3000` to `4999` raises `A WebSocket closes with code 1000 or a code between 3000 and 4999.`, and a reason longer than 123 bytes raises `A WebSocket close reason fits in 123 bytes.`.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match')
socket:on('message', function(data, binary)
    if data == 'match over' then
        socket:close(4000, 'finished')
    end
end)
socket:on('close', function(code, reason)
    print('left the match ' .. code)
end)
```

### socket.state

Read-only string with the [state](#states) of the socket.

```lua
local net = require('haylen.net')
local scene = require('haylen.scene')

local socket = net.connectWebSocket('wss://game.example.com/match')

scene.push({
    update = function(self, dt)
        if socket.state ~= self.lastState then
            self.lastState = socket.state
            print('the socket is ' .. socket.state)
        end
    end,
})
```

### socket.url

Read-only string with the address the socket was opened with.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match')
socket:on('error', function(message)
    print('could not reach ' .. socket.url .. ': ' .. message)
end)
```

### socket.attempt

Read-only integer with the attempts made since the connection was last open, which is `0` while it is open and before the first reconnect.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match', {reconnect = {maxAttempts = 5}})
socket:on('reconnecting', function()
    print('attempt ' .. socket.attempt .. ' of 5')
end)
```

### socket.protocol

Read-only string with the subprotocol the server picked from `protocols`. It is empty until the socket opens and when the server picked none.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match', {protocols = {'match.v2', 'match.v1'}})
socket:on('open', function()
    if socket.protocol == 'match.v1' then
        print('talking to an older server')
    end
end)
```
