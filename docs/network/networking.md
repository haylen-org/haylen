# 🌐 Networking

This guide shows how an app talks to servers and to other devices. It covers HTTP requests, WebSockets, raw TCP and UDP sockets and servers that run inside the app, and it helps you pick the right one. It also explains what each platform allows, how timeouts and owners work, how to send large data and how errors reach your code. Read it before you add an online feature, such as a leaderboard, a chat or a multiplayer match.

## Pick a tool

Haylen gives an app three modules for the network. The modules `http` and `socket` come from [Varn](https://github.com/varn-org/varn), the Lua runtime of the engine, and the module `haylen.net` comes from the engine.

| You want to | Use | Module |
| --- | --- | --- |
| Call a web API, send a score, read a news feed or download a file | `http.client.get`, `http.client.post` and `http.client.stream` | `http` of Varn |
| Keep a live connection where the server can send at any time, such as a chat or a match | `net.connectWebSocket` | [`haylen.net`](../lua-api/net.md) |
| Speak a protocol of your own over TCP or UDP, such as a game on the local network | `socket.tcp` and `socket.udp` | `socket` of Varn |
| Serve HTTP or WebSockets from the app itself, such as a tool on the local network or a test server | `http.createServer` and `http.createApp` | `http` of Varn |
| Pack messages into small binary data | `net.encodeMessagePack` and `net.decodeMessagePack` | [`haylen.net`](../lua-api/net.md#netencodemessagepackvalue) |
| Know whether the device is online | `haylen.networkState()` and the events `networkOnline` and `networkOffline` | [`haylen`](../lua-api/haylen.md#haylennetworkstate) |

Use HTTP when the app asks and the server answers. It works on every platform, and every server, proxy and content delivery network speaks it. Use a WebSocket when the server must send to the app without being asked, such as the moves of other players. Use raw sockets only when you need a protocol of your own or UDP, because the browser has none.

The images, sounds and maps of the app itself never need these modules. The engine brings them to the device on its own, as the [loading guide](../content/loading.md#delivery) explains.

## Results arrive on the frame thread

App code never runs on another thread. Every result, event and failure of the network reaches Lua on the frame thread: during the poll of the event loop at the start of a frame, or with the events of the frame. So every callback may change scenes, sprites and the UI.

The functions of `http` and `socket` return promises. A promise is a value that settles later with a result or an error. A task waits for it with `:await()`, which returns the result, or `nil` and the error message. Start a task with `scene.spawn(owner, fn)`, which ends the task for good when its owner ends, such as the scene that started it, or with `async.spawn(fn)` for a task that belongs to nobody. A WebSocket reports through listeners instead, which `socket:on` adds.

```lua
local http = require('http')
local scene = require('haylen.scene')

local leaderboard = {}

function leaderboard:enter()
    -- The task ends with the scene, so it never touches a scene that is gone.
    scene.spawn(self, function()
        local response, failure = http.client.get('https://api.example.com/scores', {timeoutSeconds = 10}):await()
        if not response then
            print('The scores did not load: ' .. failure)
            return
        end
        self.scores = response.json()
    end)
end

return leaderboard
```

The [Lua guide](../core/lua.md#asynchronous-code) explains tasks and promises in depth, and why a promise that fails while nothing waits for it stops the app.

## HTTP requests

The client of the `http` module sends a request and returns a promise of the response. The functions `http.client.get(url, options)` and `http.client.post(url, options)` cover most needs, and `http.client.request(options)` takes any method in its `method` option.

The response is a table:

| Field | Meaning |
| --- | --- |
| `status` | The HTTP status, such as `200` or `404`. |
| `ok` | `true` when the status is below `400`. |
| `headers` | The headers of the response, with lowercase names, such as `headers['content-type']`. |
| `body` | The body as a Lua string. |
| `json()` | A function that parses the body as JSON and returns it. |

A status such as `404` or `500` still resolves the promise, so check `ok`. The promise rejects only when no answer came, such as when the host does not exist or the deadline passed.

```lua
local async = require('async')
local http = require('http')

async.spawn(function()
    -- The option "query" adds the values to the address, and "json" sends a table as JSON.
    local page = http.client.get('https://api.example.com/news', {query = {language = 'en', page = 2}}):await()
    if page and page.ok then
        print(#page.json().items .. ' news items.')
    end

    local saved, failure = http.client.post('https://api.example.com/scores', {json = {player = 'Ana', score = 4200}, headers = {['Authorization'] = 'Bearer token-of-the-player'}}):await()
    print(saved and saved.status or failure)
end)
```

These options of a request come up most often. The reference of Varn lists every option.

| Option | Default | Meaning |
| --- | --- | --- |
| `query` | none | A table of values that the client adds to the address, sorted and encoded. |
| `json` | none | A value that the client sends as JSON with the header `Content-Type: application/json`. |
| `body` | none | The body as a string, for any other format. |
| `headers` | none | A table of the headers to send. |
| `timeoutSeconds` | `60` | One deadline for the whole request, as [timeouts](#timeouts-cancellation-and-owners) describes. |
| `maxResponseBytes` | 64 MB | The largest body the client accepts. A larger one fails the request. |
| `redirect` | `'follow'` | What to do with a redirect: `'follow'`, `'manual'` or `'error'`. |

### Several requests at once

Start every request first and wait for all of them together with `async.all`. It returns the responses in the order of the requests, or `nil` and the first error. Waiting for each request in turn would leave the later ones unobserved while the task waits for the first.

```lua
local async = require('async')
local http = require('http')

async.spawn(function()
    local pages, failure = async.all({
        http.client.get('https://api.example.com/profile'),
        http.client.get('https://api.example.com/inventory'),
        http.client.get('https://api.example.com/friends'),
    }):await()
    if not pages then
        print('The account did not load: ' .. failure)
        return
    end
    print(pages[1].status, pages[2].status, pages[3].status)
end)
```

### Large downloads

A request keeps its whole body in memory. For a large file, use `http.client.stream(options, onChunk)` instead. It calls `onResponse` of its options with the status and the headers first, then calls `onChunk` with each piece of the body as it arrives, and resolves once the body ended. A callback that falls behind holds only a bounded window of the body, so a slow app never gathers a whole download in memory. Save the file with the `fs` module of Varn under the folder of [`haylen.storage`](../lua-api/storage.md#storageroot).

```lua
local async = require('async')
local fs = require('fs')
local http = require('http')
local storage = require('haylen.storage')

async.spawn(function()
    local pieces, received, expected = {}, 0, nil
    local done, failure = http.client.stream({
        url = 'https://cdn.example.com/levels/forest.bin',
        timeoutSeconds = 120,
        maxResponseBytes = 256 * 1024 * 1024,
        onResponse = function(status, headers)
            expected = tonumber(headers['content-length'])
        end,
    }, function(piece)
        pieces[#pieces + 1] = piece
        received = received + #piece
        print(string.format('%d of %s bytes', received, expected or 'unknown'))
    end):await()
    if not done then
        print('The download failed: ' .. failure)
        return
    end
    fs.mkdir(storage.root() .. '/downloads'):await()
    fs.writeFile(storage.root() .. '/downloads/forest.bin', table.concat(pieces)):await()
end)
```

## WebSockets

A WebSocket is one connection that stays open, where the app and the server both send messages whenever they want. Open one with [`net.connectWebSocket(url, options)`](../lua-api/net.md#netconnectwebsocketurl-options), which takes a `ws://` address, or a `wss://` address for a connection encrypted with TLS, the encryption of HTTPS. The socket connects in the background and reports what happens through events: `open`, `message`, `error`, `close` and a few more that the [reference](../lua-api/net.md#socketonevent-listener-options) lists.

```lua
local json = require('json')
local net = require('haylen.net')

local lobby = {}

function lobby:load()
    -- The socket belongs to the scene, so it closes when the scene unloads.
    self.socket = net.connectWebSocket('wss://game.example.com/lobby', {owner = self})

    self.socket:on('open', function()
        self.socket:send(json.encode({type = 'join', name = 'Ana'}))
    end, {owner = self})

    self.socket:on('message', function(data, binary)
        if not binary then
            local message = json.decode(data)
            print(message.type)
        end
    end, {owner = self})

    self.socket:on('close', function(code, reason)
        print('The lobby closed with code ' .. code .. '. ' .. reason)
    end, {owner = self})
end

return lobby
```

A message that the app sends before the socket is open raises an error, so send from the `open` event on.

### How a socket lives

The engine delivers the events of every socket at the start of each frame, in the order they arrived, before the app updates. It keeps a socket alive until the socket closes, so an app may keep only its listeners and let the socket object go. The `close` event is the last one: the socket then drops its listeners. When the app stops or restarts, the engine closes every open socket without a `close` event.

On native builds each socket runs on a thread of its own, which sleeps until the server or the app has something for it. Closing or releasing a socket never waits for that thread, even while it still connects. This happens when the app restarts, when it quits or when it drops its last reference to the socket. The socket then reports nothing more, and its thread ends the connection on its own without touching the released socket. The opening of the connection and the TLS handshake of a `wss://` address end at once. The upgrade request, which turns the HTTP connection into a WebSocket, ends when the server answers or when the `connectTimeout` of the attempt runs out. The lookup of the host name has no time limit of its own. When the process exits, it waits only for connection threads that still use the network libraries. That wait is at most what is left of the `connectTimeout` of an upgrade request, and it never covers a thread that is looking up a host name.

A native socket answers the pings of the server on its own, joins messages that the server sends in fragments and refuses messages larger than its `maxMessageSize`. It checks the certificate of a `wss://` server against the trust store that Varn finds, which is the system root store on Windows. In the browser, a socket is a WebSocket of the page, as [the browser](#the-browser) describes.

### Reconnecting

Networks drop. Turn on the option `reconnect`, and the socket opens a new connection by itself after a drop or a failed attempt. It goes on until the app closes it, and it waits a little longer before each new attempt, at random, so many players who lost the same server never retry at the same moment. The [reference](../lua-api/net.md#reconnection) lists the settings of the waits.

While it reconnects, the socket keeps its listeners. Each lost connection reports `disconnect`, each planned attempt reports `reconnecting`, and each connection that opens reports `open` again. Messages cannot be sent while the socket is not open, so keep them in a queue and send them on the next `open`.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/chat', {reconnect = {initialDelay = 1, maxDelay = 20}})
local outbox = {}

local function say(text)
    if socket.state == 'open' then
        socket:send(text)
    else
        outbox[#outbox + 1] = text
    end
end

socket:on('open', function()
    for _, text in ipairs(outbox) do
        socket:send(text)
    end
    outbox = {}
end)

socket:on('reconnecting', function(attempt, delay)
    print(string.format('Attempt %d in %.1f seconds.', attempt, delay))
end)

say('Hello, everyone.')
```

### Heartbeats

A connection can die without a word, such as when a phone leaves its network. A ping is a small frame that the server answers with a pong, so a ping without an answer shows that the connection is dead and not just quiet. Send one with `socket:ping(payload)`, and listen to the `pong` event for the answer. Browsers give pages no way to send pings, so in the browser build an app that needs a heartbeat sends an ordinary message that its server answers. The [reference of `socket:ping`](../lua-api/net.md#socketpingpayload) shows a complete heartbeat.

### Binary messages and MessagePack

Text messages suit JSON. Binary messages suit compact data, such as the positions of a match. Send a Lua string of bytes with `socket:sendBinary(bytes)`, and a binary message arrives with `binary` set to `true`. Build the bytes with `string.pack`, or encode a whole table with MessagePack, a binary form of JSON values that is smaller and faster to read.

```lua
local net = require('haylen.net')

local socket = net.connectWebSocket('wss://game.example.com/match')

socket:on('open', function()
    socket:sendBinary(net.encodeMessagePack({tick = 812, players = {{id = 1, x = 40.5, y = 12}}}))
end)

socket:on('message', function(data, binary)
    if binary then
        local state = net.decodeMessagePack(data)
        print('Tick ' .. state.tick)
    end
end)
```

A large message decodes off the frame thread with `net.decodeMessagePackAsync(bytes)`, which returns a promise.

### Events of the event bus

Every socket also publishes `webSocketConnected`, `webSocketDisconnected` and `webSocketReconnecting` on [`haylen.events`](../lua-api/events.md#engine-events), so one listener can follow every connection of the app, such as a status icon in the corner of the screen.

## TCP and UDP sockets

The `socket` module of Varn opens raw connections. TCP carries a stream of bytes in order, and UDP carries separate messages called datagrams, which may arrive late, twice or never. Every operation returns a promise. The browser has no raw sockets, so this module does not exist there.

### A TCP client and server

A listener waits for connections on a port. Its `accept` resolves to a socket for each client. Both sides then `send` and `receive` bytes. A `receive` resolves to an empty string once the other side closed its end of the stream.

```lua
local async = require('async')
local socket = require('socket')

async.spawn(function()
    -- Port 0 asks the system for a free port, which the listener reports.
    local listener = socket.tcp.listen('127.0.0.1', 0):await()
    async.spawn(function()
        local client = listener:accept():await()
        local request = client:receive():await()
        client:send('Echo: ' .. request):await()
        client:close():await()
    end)

    local connection, failure = socket.tcp.connect('127.0.0.1', listener.port, {timeoutMs = 2000}):await()
    if not connection then
        print(failure)
        return
    end
    connection:send('Hello.'):await()
    print(connection:receive():await())
    connection:close():await()
    listener:close():await()
end)
```

The function `socket.tls.connect(host, port, options)` opens a TCP connection encrypted with TLS, with the same methods.

### UDP

A UDP socket binds a port and then sends and receives datagrams. A game that sends the state of each frame sends many datagrams at once with `udp:sendMany(list)` and reads every datagram that waits with `udp:recvMany(maxDatagrams)`, one call each way instead of one await per datagram.

```lua
local async = require('async')
local socket = require('socket')

async.spawn(function()
    local server = socket.udp.bind('127.0.0.1', 0):await()
    local client = socket.udp.bind('127.0.0.1', 0):await()

    local burst = {}
    for index = 1, 10 do
        burst[index] = {host = '127.0.0.1', port = server.port, data = string.pack('<I4', index)}
    end
    client:sendMany(burst):await()

    local datagrams = server:recvMany(64):await()
    print(#datagrams .. ' datagrams, the first from port ' .. datagrams[1].port)

    client:close():await()
    server:close():await()
end)
```

### Ports and host names

A listener, a server or a UDP socket binds its port alone. A second one on a busy port fails with a message such as `The address 127.0.0.1:41000 is already in use.` instead of sharing the port. Port `0` asks the system for a free port, which the listener reports in `listener.port`, the server in `server.port` and a UDP socket in `udp.port`. So an app that serves something for itself, such as the servers of the test project, never collides with another program.

Host names resolve on the I/O pool, a set of worker threads for slow input and output. A connect, a listen, a bind or a datagram never waits for the resolver on the frame thread. The function `socket.resolve(host)` lists the addresses of a name.

## Servers inside the app

The `http` module also runs servers. The function `http.createServer(handler)` calls one function for every request, and `http.createApp()` adds routes, middleware and WebSocket routes with `app:ws(path, handlers)`. Their `listen` returns the running server. A server suits a tool on the local network, a companion app or a test that must run without the internet. The browser cannot accept connections, so it has no servers.

```lua
local http = require('http')
local net = require('haylen.net')

local app = http.createApp()

app:get('/hello', function(ctx)
    ctx:json({message = 'Hello, ' .. (ctx.query.name or 'player') .. '.'})
end)

app:ws('/echo', {
    message = function(conn, data, binary)
        conn:send(data, {binary = binary})
    end,
})

local server = app:listen({host = '127.0.0.1', port = 0})
local echo = net.connectWebSocket(string.format('ws://127.0.0.1:%d/echo', server.port))
echo:on('open', function()
    echo:send('Ping.')
end)
echo:on('message', function(data)
    print(data)
    server:close()
end)
```

A handler or a route runs as a task, so it may `:await()`. A handler that fails answers its request with status `500`, and a WebSocket connection whose callback fails stays open.

## What each platform offers

| Capability | Desktops | iOS, iPadOS, tvOS and Mac Catalyst | Android | Web |
| --- | --- | --- | --- | --- |
| WebSocket client, `haylen.net` | Yes | Yes | Yes | Yes, as a WebSocket of the page |
| HTTP client, `http.client` | Yes | Yes, through `NSURLSession` | Yes, through `HttpURLConnection` | Yes, through `fetch` of the page |
| HTTP and WebSocket server, `http.createServer` and `http.createApp` | Yes | Yes | Yes | No |
| TCP and UDP, `socket` | Yes | Yes | Yes | No |

The test project checks each row. The tests `NET-001` to `NET-007` run against public services. The tests `NET-008` to `NET-010` and `VRN-004` to `VRN-007` run against servers inside the app, so they need no internet.

### The browser

A web page lives inside the rules of the browser. They keep a page from acting as a server and from talking to any machine it likes.

- A page cannot accept connections. So the browser build has no HTTP server, no WebSocket server and no listener.
- Browsers open no raw TCP or UDP sockets. So the browser build has no `socket` module.
- A socket of `haylen.net` is a WebSocket of the page. A page served over HTTPS reaches only `wss://` addresses. The browser limits the size of messages instead of `maxMessageSize`, and the close codes come from the browser.
- A page cannot send pings and never sees pongs. So `socket:ping` raises `Browsers cannot send WebSocket ping frames.`, and the event `pong` never comes. Every other event comes as on native builds, on the next frame of the app.
- Browsers hide the cause of a failed connection from pages on purpose. A failed connection reports `The WebSocket connection to "<url>" failed.` without a cause. When the browser refuses to open the address at all, its own reason follows.
- The option `connectTimeout`, the reconnection, the owners and `socket.bufferedAmount` work as on native builds.
- Requests of `http.client` follow the rules of `fetch`, the request function of the browser. A server on another origin answers only when it allows the origin of the page through CORS, the rules by which a server lets pages of other sites read its answers. A page served over HTTPS requests only `https://` addresses.
- The browser follows redirects itself, so `redirect` must be `'follow'`. A response shows only the headers that the CORS rules expose to the page.
- The function `http.client.stream` reads the body as it arrives, with the head first, and `maxResponseBytes` stops a body at the piece that passes the limit, as on native builds.

### Android

The Android template declares the permissions `INTERNET` and `ACCESS_NETWORK_STATE`. Every connection needs the first, and the network events need the second. The HTTP client of Varn runs on `HttpURLConnection` there. So Android refuses plain `http://` addresses unless the network security configuration of the app allows cleartext traffic. The `haylen.net` sockets and the `socket` module open their own connections, so they speak plain TCP and `ws://` as on desktops. The emulator reaches the machine that runs it at `10.0.2.2`.

### Apple platforms

On Apple platforms the HTTP client of Varn runs on `NSURLSession`. So App Transport Security, set in the `Info.plist`, decides which plain `http://` addresses an app may load.

## Timeouts, cancellation and owners

Every kind of connection has its own way to stop waiting.

- A socket of `haylen.net` fails an attempt that has not opened after `connectTimeout` seconds, `10` by default. The opening of the connection, the TLS handshake and the upgrade request share that time. With the option `reconnect` the socket then tries again on its own. A socket with an `owner` closes with code `1000` when the owner ends, such as the scene that opened it.
- A request of `http.client` ends at its `timeoutSeconds` deadline, which counts from the connection to the last byte of the body, as the [Lua guide](../core/lua.md#asynchronous-code) details.
- The function `async.timeout(promise, ms)` stops waiting for any promise at a deadline of the app. A task of `scene.spawn` stops for good when its owner ends and never resumes. Neither of them cancels the request itself: it runs to its end, and nobody receives its answer.
- The option `timeoutMs` of `socket.tcp.connect(host, port, {timeoutMs = 1000})` fails a connection that takes longer, the lookup of its host name included.
- A `receive` of the `socket` module has no timeout of its own. A task waits for it with `async.timeout` and closes the socket when the peer stays silent, which rejects the pending `receive` with `The socket was closed.`.
- A server that `listen` returned closes with `server:close()`. With `server:close({graceful = true})` it first lets the requests in flight finish. Either way its port is free again at once.
- A listener of the `socket` module closes with `listener:close()`, which rejects a pending `accept` with `The listener was closed.`.

```lua
local async = require('async')
local socket = require('socket')

async.spawn(function()
    local peer, failure = socket.tcp.connect('192.168.1.20', 7000, {timeoutMs = 3000}):await()
    if not peer then
        print(failure)
        return
    end
    -- Gives up after 5 seconds of silence and closes the socket.
    local data = async.timeout(peer:receive(), 5000):await()
    if not data then
        peer:close():await()
        return
    end
    print(#data .. ' bytes.')
end)
```

## Large data and backpressure

Backpressure means that a sender slows down to the speed of the receiver, so data never piles up in memory. Each kind of connection gives the app a way to do it.

- A socket of `haylen.net` queues what the app sends, and `socket.bufferedAmount` counts the bytes that still wait. An app that sends faster than the network takes waits until that number falls, as the [reference](../lua-api/net.md#socketbufferedamount) shows.
- On native builds, the thread of a `haylen.net` socket keeps reading while it waits for room to write. So two sides that both send large messages never block each other, and messages up to `maxMessageSize`, 16 MiB by default, arrive whole.
- An awaited `send` of the `socket` module waits until the data left for the peer. So a task that awaits its sends never queues more than the peer takes. Sends that nobody awaits queue up to `maxQueuedBytes`, 64 MiB by default, and a send past that rejects at once.
- Requests of `http.client` hold their bodies in memory, up to `maxResponseBytes` for a response. The function `http.client.stream` hands a response to a callback in pieces as they arrive, as [large downloads](#large-downloads) shows.
- A handler of a server inside the app streams a body with `ctx:write(data)`. Its promise settles once the data left for the client, so a handler that awaits its writes never holds more than the client takes.
- A WebSocket route of `app:ws` sends with `conn:send(data, {binary = true})`. Its promise resolves to `true` once the message left and to `false` when the connection closed first, and `conn.bufferedAmount` counts what waits.

## Throughput within a frame

The engine polls the event loop of Varn once per frame, with a budget of a quarter of the frame time, between 1 and 8 milliseconds, as the [architecture guide](../core/architecture.md#the-frame) describes. The loop runs in passes while they make progress and the budget lasts. Each pass serves every socket of the `socket` module and every connection of the servers of the `http` module until it would block. So a task that reads again after a receive reads in the same frame, a request to a server inside the app gets its answer within one frame, and a burst of datagrams or connections is served at once.

A socket of `haylen.net` runs on a thread of its own, so the budget of the poll never slows it. Its messages only wait for the next frame to reach Lua.

## The network state

The function `haylen.networkState()` returns `'online'`, `'offline'` or `'unknown'`, and the events `networkOnline` and `networkOffline` report each change. Use them to show an offline badge, or to wait for the network before a retry. Windows, Linux and the headless host never report the network, so the state stays `'unknown'` there. The [reference](../lua-api/haylen.md#haylennetworkstate) lists the platforms and their permissions.

```lua
local events = require('haylen.events')
local haylen = require('haylen')

local badge = {visible = haylen.networkState() == 'offline'}

events.on('networkOffline', function()
    badge.visible = true
end, {owner = badge})

events.on('networkOnline', function()
    badge.visible = false
end, {owner = badge})
```

## Errors

A failure always names the address and the cause.

- A socket of `haylen.net` reports a failed connection in its `error` event, such as `The WebSocket connection to "ws://127.0.0.1:1/" failed. The server refused the connection.` or `The WebSocket connection to "<url>" did not open within <seconds> seconds.`. On Android, the message of an app without the permission `INTERNET` ends with what to add. The [reference](../lua-api/net.md#errors) lists every message.
- The modules of Varn reject their promises with messages that name the address and the cause, such as `The connection to 127.0.0.1:1 was refused.`.
- An error raised in a listener, a callback or a task stops the app with the error screen and the stack of the code that failed. The same holds for a handler or a route of a server of the `http` module, a callback of a WebSocket route and a callback of `http.client.stream`. The [Lua guide](../core/lua.md#errors-and-stack-traces) describes the error screen.
- A server answers the request whose handler failed with status `500`, and the WebSocket connection whose callback failed stays open.
