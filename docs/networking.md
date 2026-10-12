# Networking

Apps reach the network through [`haylen.net`](lua-api/net.md) for WebSocket clients and through the `http` and `socket` modules of Varn for HTTP clients, HTTP and WebSocket servers and raw TCP and UDP. Every result, event and failure reaches Lua on the frame thread, during the poll of the event loop at the start of a frame or with the events of the frame, so app code never runs on another thread. This guide tells what each platform offers, what the browser and the mobile systems refuse and why, and how timeouts, owners, large data and errors work.

## What each platform offers

| Capability | Desktops | iOS, iPadOS, tvOS and Mac Catalyst | Android | Web |
| --- | --- | --- | --- | --- |
| WebSocket client, `haylen.net` | Yes | Yes | Yes | Yes, as a WebSocket of the page |
| HTTP client, `http.client` | Yes | Yes, through `NSURLSession` | Yes, through `HttpURLConnection` | Yes, through `fetch` of the page |
| HTTP and WebSocket server, `http.createServer` and `http.createApp` | Yes | Yes | Yes | No |
| TCP and UDP, `socket` | Yes | Yes | Yes | No |

The test project checks each row: `NET-001` to `NET-007` against public services, and `NET-008` to `NET-010`, `VRN-004` to `VRN-007` against servers inside the app, so they run without the internet.

## The browser

A page runs inside the rules of the browser, which keep a web page from acting as a server or from talking to any machine it likes.

- A page cannot accept connections, so the browser build has no HTTP server, no WebSocket server and no listener.
- Browsers open no raw TCP or UDP sockets, so the browser build has no `socket` module.
- A socket of `haylen.net` is a WebSocket of the page. A page served over HTTPS reaches only `wss://` addresses, the browser limits the size of messages instead of `maxMessageSize`, a page cannot send pings and never sees pongs, so `socket:ping` raises `Browsers cannot send WebSocket ping frames.` and the event `pong` never comes there, and close codes come from the browser. A failed connection reports `The WebSocket connection to "<url>" failed.` without a cause, because browsers hide the cause from pages on purpose. The option `connectTimeout`, the reconnection, owners and `socket.bufferedAmount` work as on native builds.
- Requests of `http.client` follow the rules of `fetch`: a server of another origin answers only when it allows the origin of the page through CORS, a page served over HTTPS requests only `https://` addresses, and the browser follows redirects itself, so `redirect` must be `'follow'`. A response shows only the headers that the CORS rules expose to the page. The function `http.client.stream` reads the body as it arrives, the head first, and `maxResponseBytes` stops a body at the piece that passes the limit, as on native builds.

## Android and Apple platforms

The Android template declares the permissions `INTERNET` and `ACCESS_NETWORK_STATE`, which every connection and the network events need. The HTTP client of Varn runs on `HttpURLConnection` there, so Android refuses plain `http://` addresses unless the network security configuration of the app allows cleartext traffic, while `haylen.net` and the `socket` module open their own connections and speak plain TCP and `ws://` as on desktops. The emulator reaches the machine that runs it at `10.0.2.2`.

On Apple platforms the HTTP client of Varn runs on `NSURLSession`, so App Transport Security of the `Info.plist` decides which plain `http://` addresses an app may load.

## Timeouts, cancellation and owners

- A socket of `haylen.net` fails an attempt that has not opened after `connectTimeout` seconds, 10 by default, reconnects on its own with the option `reconnect`, and closes with code `1000` when its `owner` ends, such as the scene that opened it.
- A request of `http.client` ends at the deadline of `timeoutSeconds`, which counts from the connection to the last byte of the body. The function `async.timeout(promise, ms)` stops waiting for any promise at a deadline of the app, and a task of `scene.spawn` stops for good with its owner and never resumes. Neither cancels the request, which runs to its end and whose answer nobody receives.
- The option `timeoutMs` of `socket.tcp.connect(host, port, {timeoutMs = 1000})` fails a connection, with the lookup of its host name, that takes longer than its timeout. A `receive` has no timeout of its own, so a task waits for it with `async.timeout` and closes the socket when the peer stays silent, which rejects the pending `receive` with `The socket was closed.`.
- The `Server` that `listen` of `http.createServer` and `http.createApp` returns closes with `server:close()`, or with `server:close({graceful = true})` once the requests in flight finished, and its port is free again at once. A listener of the `socket` module closes with `listener:close()`, which rejects a pending `accept` with `The listener was closed.`.

## Ports

A listener, a server or a UDP socket binds its port alone, so a second one on a busy port fails with a message such as `The address 127.0.0.1:41000 is already in use.` instead of sharing it. Port `0` asks the system for a free port, which the listener reports in `listener.port`, the server in `server.port` and a UDP socket in `udp.port`, so an app that serves something for itself, such as the servers of the test project, never collides with another program. Host names resolve on the I/O pool, so a connect, a listen, a bind or a datagram never waits on the frame thread for the resolver, and `socket.resolve(host)` lists the addresses of a name.

## Large data and backpressure

- A socket of `haylen.net` queues what the app sends, and `socket.bufferedAmount` counts the bytes that still wait, so an app that sends faster than the network takes waits until it falls. On native builds the thread of the socket keeps reading while it waits for room to write, so two sides that both send large messages never stall each other, and messages up to `maxMessageSize`, 16 MiB by default, arrive whole.
- An awaited `send` of the `socket` module waits until the data left for the peer, so a task that awaits its sends never queues more than the peer takes. Sends that nobody awaits queue up to `maxQueuedBytes`, 64 MiB by default, and a send past it rejects at once.
- The datagrams of a frame travel in one call each way with `udp:sendMany(list)` and `udp:recvMany(maxDatagrams)`, instead of one await per datagram.
- Requests of `http.client` hold their bodies in memory, up to `maxResponseBytes` for a response, and `http.client.stream` hands a response to a callback in pieces as it arrives, with a bounded window of the body held for a callback that falls behind.
- A handler of a server inside the app streams a body with `ctx:write(data)`, whose promise settles once the data left for the client, so a handler that awaits its writes never holds more than the client takes. The WebSocket connections of `app:ws` send with `conn:send(data, {binary = true})`, whose promise resolves to `true` once the message left and to `false` when the connection closed first, and count what waits in `conn.bufferedAmount`.

## Throughput within a frame

The engine polls the event loop of Varn once per frame with a budget of a quarter of the frame time, between 1 and 8 milliseconds, as the [architecture guide](architecture.md#the-frame) describes. The loop runs in passes while they make progress and the budget lasts, and each pass serves every socket of the `socket` module and every connection of the servers of the `http` module until it would block, so a task that reads again after a receive reads in the same frame, a request to a server inside the app is answered within one frame, and a burst of datagrams or connections is served at once. A socket of `haylen.net` runs on a thread of its own and is not bound by the poll: its messages only wait for the next frame to reach Lua.

## Errors

A socket of `haylen.net` reports a failed connection with its address and the cause, such as `The WebSocket connection to "ws://127.0.0.1:1/" failed. The server refused the connection.`, or `The WebSocket connection to "<url>" did not open within <seconds> seconds.`, and an Android app without the permission `INTERNET` ends the message with what to add. The modules of Varn reject their promises with the messages of Varn, which name the address and the cause, such as `The connection to 127.0.0.1:1 was refused.`, and an error raised in a listener, a callback, a task, a handler or route of a server of the `http` module, a callback of a WebSocket route or a callback of `http.client.stream` stops the app with the error screen and the stack of the code that failed, as the [Lua guide](lua.md#errors-and-stack-traces) describes. A server answers the request whose handler failed with status `500`, and the WebSocket connection whose callback failed stays open.
