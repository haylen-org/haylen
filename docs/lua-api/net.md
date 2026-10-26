# 📖 `haylen.net`

The module `haylen.net` opens WebSocket connections to `ws://` and `wss://` servers and encodes and decodes MessagePack. Use it for live connections where the server sends at any time, such as a chat, live events or a multiplayer match. For HTTP requests and raw TCP and UDP, use the `http` and `socket` modules of Varn. The [networking guide](../network/networking.md) explains when to use which, how a socket lives and what each platform allows.

```lua
local net = require('haylen.net')
```

## Functions

### net.connectWebSocket(url, options)

Starts connecting to `url` in the background and returns a [`WebSocket`](#websocket) in the `'connecting'` state. The address must start with `ws://` or `wss://`. The table `options` is optional.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `protocols` | list of strings | none | The subprotocols, names of message formats that the app and the server agree on, which the socket offers in the header `Sec-WebSocket-Protocol`. The property [`socket.protocol`](#socketprotocol) tells which one the server picked. |
| `maxMessageSize` | integer | `16777216` | The largest message in bytes that the socket accepts from the server, from `1` to `2147483647`, which is 16 MiB by default. On native builds a larger message reports `error` with `The server sent a WebSocket message larger than the maximum of <size> bytes.` and ends the connection with code `1009`. In the browser the limits of the browser apply instead. |
| `connectTimeout` | number | `10` | The seconds an attempt may take to open, above `0` and at most `3600`. An attempt that has not opened in time fails with code `1006` on every platform. A socket that reconnects gives every attempt this time. |
| `owner` | table or userdata | none | An object, such as a scene, whose end closes the socket with code `1000`, as [subscription scopes](../core/lifecycle.md#subscription-scopes) describe. |
| `reconnect` | boolean or table | `false` | The value `true` turns [reconnection](#reconnection) on with its defaults, and a table turns it on with the settings it changes. |

An unknown option raises `Unknown option "<key>".`. A wrong address or a value out of its range raises one of the messages that [errors](#errors) lists.

```lua
local net = require('haylen.net')

local chat = net.connectWebSocket('ws://127.0.0.1:8080/chat', {protocols = {'chat', 'json'}})
print(chat.state) -- connecting

local lobby = net.connectWebSocket('wss://game.example.com/lobby', {reconnect = true})
local scores = net.connectWebSocket('wss://game.example.com/scores', {maxMessageSize = 64 * 1024, connectTimeout = 5})

-- The match socket belongs to the scene, so it closes when the scene unloads.
local match = {}
function match:load()
    self.socket = net.connectWebSocket('wss://game.example.com/match', {owner = self})
end
```

### net.openSocketCount()

Returns how many sockets the engine keeps alive. That is every socket from `net.connectWebSocket` that has not reached the `'closed'` state yet, whether or not the app still holds it. A socket leaves the count at the start of the frame after it closed.

```lua
local net = require('haylen.net')

net.connectWebSocket('ws://127.0.0.1:8080/chat')
print(net.openSocketCount()) -- 1
```

### net.encodeMessagePack(value)

Encodes a Lua value as MessagePack, a compact binary form of JSON values, and returns its bytes as a string. The value follows the rules of JSON values in Lua. A table with consecutive integer keys from `1` becomes an array, any other table becomes a map with string keys, and a `haylen.Bytes` value becomes a MessagePack binary value. Functions, userdata other than bytes, tables that contain themselves and tables nested deeper than 128 levels raise an error.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/state')
socket:on('open', function()
    socket:sendBinary(net.encodeMessagePack({tick = 812, players = {{id = 1, x = 40.5}}}))
end)
```

### net.decodeMessagePack(bytes)

Decodes one MessagePack value from a string of bytes on the frame thread and returns it as a Lua value. Maps become tables and arrays become sequences. A `nil` becomes a missing value, as with JSON, and binary values become strings. The bytes must hold exactly one value. The decoder checks every size and count before it allocates, so hostile bytes never make it reserve more memory than the message holds.

Bytes that end early, hold more bytes after their value, use extension types, claim more items than they hold or nest deeper than 128 levels raise an error. Its message starts with `The bytes are not valid MessagePack:` and says what is wrong, such as `The bytes are not valid MessagePack: bytes follow its value.`.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/state')
socket:on('message', function(data, binary)
    if binary then
        print(net.decodeMessagePack(data).tick)
    end
end)
```

### net.decodeMessagePackAsync(bytes)

Decodes like [`net.decodeMessagePack`](#netdecodemessagepackbytes) on the task pool, so a large message never holds up a frame. It returns a promise that resolves with the value on the frame thread, or rejects with the same message. The engine copies the bytes before the call returns, so the app may change or drop them at once. The web build runs the task when the frame polls its event loop, since the browser build has one thread.

```lua
local async = require('async')
local net = require('haylen.net')

local replay = net.encodeMessagePack({frames = {{tick = 1}, {tick = 2}}})

async.spawn(function()
    local decoded, failure = net.decodeMessagePackAsync(replay):await()
    print(decoded and #decoded.frames or failure) -- 2
end)
```

## WebSocket

A `WebSocket`, named `haylen.WebSocket` in messages, is the socket that [`net.connectWebSocket`](#netconnectwebsocketurl-options) returns. It reports what happens through the [events](#events) that `socket:on` listens to. The engine keeps it alive until it closes, so an app may keep only its listeners. After its `close` event it drops its listeners and reports nothing more. The [networking guide](../network/networking.md#how-a-socket-lives) explains its threads and what happens when the app restarts. Reading a member that it does not have raises `The type "haylen.WebSocket" has no member "<name>".`.

### socket:on(event, listener, options)

Calls `listener` every time the socket reports `event`, and returns a [`Connection`](signal.md#connection) of `haylen.signal`. Its method `disconnect()` stops the listener, and its property `connected` is `true` until then. The [events](#events) section lists each event with the arguments of its listener.

The table `options` may hold an `owner`, a table or a userdata such as a scene, and the listener ends with it, as [subscription scopes](../core/lifecycle.md#subscription-scopes) describe. Without an `owner`, a listener that a module adds at its top level in development belongs to that module, so a [reload](hotReload.md) of the module ends it. An unknown option raises `Unknown option "<name>".`, and an unknown event raises `Unknown WebSocket event "<event>". Sockets report "open", "message", "pong", "disconnect", "reconnecting", "close" and "error".`. An error raised inside a listener stops the app and shows the error screen with the message and its stack trace.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match')
local messages = socket:on('message', function(data, binary)
    print((binary and 'Binary, ' or 'Text, ') .. #data .. ' bytes.')
end)

local function stopListening()
    messages:disconnect()
end

-- A listener owned by a scene ends when the scene unloads.
local lobby = {}
function lobby:load()
    socket:on('message', function(data) print('The lobby heard ' .. data) end, {owner = self})
end
```

### socket:send(text)

Sends `text` as a text message. The text should be valid UTF-8, which browsers require. Sending while the socket is not open raises `The WebSocket to "<url>" is not open.`, so send from the `open` event on.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match')
socket:on('open', function()
    socket:send('{"type": "ready"}')
end)
```

### socket:sendBinary(bytes)

Sends the Lua string `bytes` as a binary message, byte for byte. Sending while the socket is not open raises `The WebSocket to "<url>" is not open.`.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/state')
socket:on('open', function()
    socket:sendBinary(string.pack('<I2ff', 7, 120.5, 48.25))
end)
```

### socket:ping(payload)

Sends a ping frame with the string `payload`, an empty string by default. The server answers with a pong frame that carries the same payload to the `pong` event. A ping without an answer tells a dead connection apart from a quiet one. Native builds send the ping themselves.

Browsers answer the pings of a server on their own, but they give pages no way to send one. So in the browser build `ping` raises `Browsers cannot send WebSocket ping frames.`, and an app that needs a heartbeat there sends an ordinary message that its server answers. Pinging while the socket is not open raises `The WebSocket to "<url>" is not open.`, and a payload longer than 125 bytes raises `A WebSocket ping carries at most 125 bytes.`.

```lua
local net = require('haylen.net')
local timer = require('haylen.timer')

-- Pings every 5 seconds and gives up on a connection that left three pings in a row unanswered.
local socket = net.connectWebSocket('wss://game.example.com/match')
local unanswered = 0
socket:on('open', function()
    timer.every(5, function()
        if unanswered == 3 then
            socket:close(4000, 'No answer.')
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

Starts the closing handshake with `code`, `1000` by default, and `reason`, an empty string by default. The state becomes `'closing'` at once, and the `close` event reports the end. Calling it on a socket that is already closing or closed does nothing. A call during the wait of a reconnecting socket ends the socket at once with the code it gives.

The code must be `1000` or between `3000` and `4999`, or it raises `A WebSocket closes with code 1000 or a code between 3000 and 4999.`. A code beyond the 32-bit integers raises `bad argument #1 to 'close' (integer out of range)`, and a reason longer than 123 bytes raises `A WebSocket close reason fits in 123 bytes.`.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match')
socket:on('message', function(data, binary)
    if data == 'match over' then
        socket:close(4000, 'Finished.')
    end
end)
socket:on('close', function(code, reason)
    print('Left the match with code ' .. code .. '.')
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
            print('The socket is ' .. socket.state .. '.')
        end
    end,
})
```

### socket.url

Read-only string with the address that the socket was opened with.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match')
socket:on('error', function(message)
    print('Could not reach ' .. socket.url .. '. ' .. message)
end)
```

### socket.attempt

Read-only integer with the attempts made since the connection was last open. It is `0` while the connection is open and before the first reconnection.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match', {reconnect = {maxAttempts = 5}})
socket:on('reconnecting', function()
    print('Attempt ' .. socket.attempt .. ' of 5.')
end)
```

### socket.bufferedAmount

Read-only integer with the bytes of the messages sent on the current connection that still wait to go out to the network. An app that sends a lot, such as a stream of positions or a large upload in pieces, sends the next piece only while this stays low. So it never sends faster than the connection carries and never piles up memory. It is `0` without a connection, and in the browser it is the `bufferedAmount` of the WebSocket of the page.

```lua
local net = require('haylen.net')
local scene = require('haylen.scene')

-- Sends a recording in pieces of 64 KiB, at most 1 MiB ahead of the network.
local socket = net.connectWebSocket('wss://game.example.com/replays')
local recording = string.rep('frame', 400000)
local offset = 1
scene.push({
    update = function(self, dt)
        while socket.state == 'open' and offset <= #recording and socket.bufferedAmount < 1024 * 1024 do
            socket:sendBinary(recording:sub(offset, offset + 65535))
            offset = offset + 65536
        end
    end,
})
```

### socket.protocol

Read-only string with the subprotocol that the server picked from `protocols`. It is empty until the socket opens, and when the server picked none.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match', {protocols = {'match.v2', 'match.v1'}})
socket:on('open', function()
    if socket.protocol == 'match.v1' then
        print('The server is an older one.')
    end
end)
```

### States

The property `socket.state` is one of these names.

| State | Meaning |
| --- | --- |
| `'connecting'` | The socket is connecting. It starts in this state, and a reconnecting socket returns to it for every attempt. |
| `'open'` | The connection is up, and `send` works. |
| `'reconnecting'` | The connection dropped or an attempt failed, and the socket waits for its next attempt. |
| `'closing'` | The method `close` was called, and the closing handshake runs. A socket closed while it connects stays in this state until its `close` event and never reports `open`, as in browsers. |
| `'closed'` | The connection is over. The `close` event reports it, and the `disconnect` listeners of a socket that does not reconnect already see this state. |

### Close codes

The `close` and `disconnect` events report the code of the closing handshake.

| Code | Meaning |
| --- | --- |
| `1000` | Normal closure. The method `close()` sends it by default, and a server that answers a close usually echoes the code it received or sends `1000`. |
| `1001` | Going away. A native socket that is dropped while open sends it to the server. |
| `1005` | The close frame of the server carried no code, on native builds. |
| `1006` | Abnormal closure. The connection failed, dropped without a close frame, or the server did not finish the closing handshake within 5 seconds on native builds. An `error` event comes first when the connection failed. |
| `1009` | Message too big. On native builds the socket ends the connection with it when the server sends a message larger than `maxMessageSize`, after an `error` event. |
| `3000` to `4999` | Codes that the app and its server define. The method `close()` accepts them. |

Other codes that a server sends, such as `1008` or `1011`, reach the events as they are.

## Reconnection

A socket opened with the option `reconnect` connects again on its own when the connection drops or an attempt fails, until the app closes it. It waits before every attempt. The first attempt waits `initialDelay`, and every further attempt waits `multiplier` times longer, up to `maxDelay`. Then `jitter` shortens each wait at random by up to that fraction, so many players who lost the same server do not all retry at the same moment. A connection that opens starts the count over. After `maxAttempts` failed attempts in a row the socket gives up and reports `close` with the code of the last failure, and `0` never gives up. Reconnection works the same way on native builds and in the browser.

| Reconnect key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `initialDelay` | number | `0.5` | Seconds before the first attempt, from `0` up. |
| `maxDelay` | number | `30` | The longest wait in seconds, at least `initialDelay`. |
| `multiplier` | number | `2` | How much longer each wait is than the one before, at least `1`. |
| `jitter` | number | `0.5` | A fraction from `0` to `1` that shortens each wait at random. |
| `maxAttempts` | integer | `0` | Failed attempts in a row before the socket closes, where `0` never gives up. |

While it reconnects, the socket keeps its listeners. Each lost connection reports `disconnect`, each planned attempt reports `reconnecting` with the attempt number and the wait, and each connection that opens reports `open` again. Sending while the socket is not open raises an error, so an app keeps what it wants to send until the next `open`.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/lobby', {reconnect = {initialDelay = 1, maxDelay = 20, maxAttempts = 10}})

socket:on('open', function()
    socket:send('{"type": "hello"}')
end)

socket:on('disconnect', function(code, reason)
    print('Connection lost with code ' .. code .. '.')
end)

socket:on('reconnecting', function(attempt, delay)
    print(string.format('Attempt %d in %.1f seconds.', attempt, delay))
end)

socket:on('close', function(code)
    print('Gave up with code ' .. code .. '.')
end)
```

## Events

The method [`socket:on`](#socketonevent-listener-options) listens to these events. The engine delivers them at the start of each frame, in the order they arrived, before the app updates.

| Event | Listener arguments | Meaning |
| --- | --- | --- |
| `'open'` | none | The connection is up, the first time or after reconnecting. The property `socket.protocol` holds the subprotocol that the server picked. |
| `'message'` | `data` (string), `binary` (boolean) | A whole message arrived. A text message arrives as a UTF-8 string with `binary` set to `false`, and a binary message arrives as a string of raw bytes with `binary` set to `true`. |
| `'pong'` | `payload` (string) | A pong frame arrived: the answer of the server to [`socket:ping`](#socketpingpayload), or one that it sent to keep the connection alive. Browsers never report pongs to the page, so the browser build never fires it. |
| `'error'` | `message` (string) | The connection failed, with a message that [errors](#errors) lists. A `close` event follows, with code `1006` on native builds, unless the socket reconnects. |
| `'disconnect'` | `code` (integer), `reason` (string) | An open connection ended, before the socket reconnects or closes. The property `socket.state` already reads `'reconnecting'` or `'closed'`, so sending from the listener raises an error, and `close()` there ends a reconnecting socket instead of its next attempt. |
| `'reconnecting'` | `attempt` (integer), `delay` (number) | The socket planned its next attempt, counted from `1`, after a wait of `delay` seconds. |
| `'close'` | `code` (integer), `reason` (string) | The socket is done: the app closed it, reconnection is off or it gave up. It is the last event of the socket. |

The event bus of [`haylen.events`](events.md#engine-events) hears the same moments of every socket as `webSocketConnected`, `webSocketDisconnected` and `webSocketReconnecting`.

```lua
local events = require('haylen.events')

events.on('webSocketDisconnected', function(socket)
    print('Lost ' .. socket.url .. ' with code ' .. socket.code .. '.')
end)
```

## Errors

These messages are raised by the functions and methods.

| Message | Cause |
| --- | --- |
| `The WebSocket address "<url>" must start with "ws://" or "wss://".` | The address of `net.connectWebSocket` has another scheme. |
| `A WebSocket needs a maximum message size between 1 and 2147483647 bytes.` | The option `maxMessageSize` is `0` or above `2147483647`. |
| `A WebSocket needs a connect timeout above 0 and at most 3600 seconds.` | The option `connectTimeout` is not above `0` and at most `3600`. |
| `WebSocket reconnection needs finite delays from zero up with the maximum at least the initial one, a finite multiplier of at least 1, a jitter between 0 and 1 and a maximum of attempts of at least 0.` | A key of `reconnect` is out of its range or is not a finite number. |
| `An owner must be a table or a userdata, not <type>.` | The option `owner` of `net.connectWebSocket` is another type. |
| `Unknown option "<key>".` | An options table has a key that the function does not take. |
| `Unknown WebSocket event "<event>". Sockets report "open", "message", "pong", "disconnect", "reconnecting", "close" and "error".` | The method `socket:on` names another event. |
| `The WebSocket to "<url>" is not open.` | The methods `send`, `sendBinary` or `ping` ran while the socket was not open. |
| `A WebSocket ping carries at most 125 bytes.` | The payload of `socket:ping` is longer. |
| `Browsers cannot send WebSocket ping frames.` | The method `socket:ping` ran in the browser build. |
| `A WebSocket closes with code 1000 or a code between 3000 and 4999.` | The method `socket:close` got another code. |
| `bad argument #1 to 'close' (integer out of range)` | The code of `socket:close` is beyond the 32-bit integers. |
| `A WebSocket close reason fits in 123 bytes.` | The reason of `socket:close` is longer. |
| `The type "haylen.WebSocket" has no member "<name>".` | The code read a member that a socket does not have. |
| `The bytes are not valid MessagePack: <problem>.` | The bytes of `net.decodeMessagePack` or `net.decodeMessagePackAsync` are broken. |

These messages reach the `error` event of a socket.

| Message | Cause |
| --- | --- |
| `The WebSocket connection to "<url>" did not open within <seconds> seconds.` | An attempt did not open within `connectTimeout`, on every platform. This also covers browsers that would wait minutes for a server that never answers. |
| `The WebSocket connection to "<url>" failed. The server refused the connection.` | Nothing listens on the port, on native builds. |
| `The WebSocket connection to "<url>" failed. The host "<host>" was not found.` | The host name does not resolve, on native builds. |
| `The WebSocket connection to "<url>" failed. The host "<host>" is out of reach of this network.` | No route leads to the host, on native builds. |
| `The WebSocket connection to "<url>" failed. The server reset the connection.` | The server dropped the connection, on native builds. |
| `The WebSocket connection to "<url>" failed. The TLS handshake failed with "<reason>".` | The secure connection of a `wss://` address failed, such as for a certificate that does not verify, on native builds. |
| `The WebSocket connection to "<url>" failed. The server did not accept the WebSocket and answered "<answer>".` | The server refused the upgrade to a WebSocket, on native builds. |
| `The WebSocket connection to "<url>" failed. The network libraries reported "<text>".` | Any other failure of a native build. |
| `The WebSocket connection to "<url>" failed. No CA trust store was found for certificate verification: <searched places>.` | A native build outside Windows found no trust store to check certificates with. |
| `The WebSocket connection to "<url>" failed. No trusted root certificate was found in the system store of Windows.` | A Windows build found no trusted root certificate to check certificates with. |
| `The server sent a WebSocket message larger than the maximum of <size> bytes.` | A message passed `maxMessageSize`, on native builds. The socket then closes with code `1009`. |
| `The WebSocket connection to "<url>" failed.` | The browser build failed to connect. Browsers hide the cause from pages, and when the browser refuses the address itself, its own reason follows. |

On Android, the message of an app that lacks the permission `android.permission.INTERNET` ends with a sentence that names the permission and how to add it, as [Android permissions](system.md#android-permissions) describes.
