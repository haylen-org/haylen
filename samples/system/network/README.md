# Haylen Network

A Lua sample with one scene per way an app talks to a server: HTTP requests with Varn's `http` module and WebSockets with [`haylen.net`](../../../docs/lua-api/net.md). The menu lists the tests, each test opens as its own scene with a Back button, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu. Every request runs in a task of its scene, started with `self:spawn`, so an answer that arrives after the player left never reaches a page that is gone, and the sockets of a test close when it does.

| Test | What it shows |
| --- | --- |
| HTTP requests | The functions `http.client.get` with a query and `http.client.post` with a JSON body against `httpbin.org` or `postman-echo.com`, and a GET of any typed address, with the status, the time, the headers and the body of the response. |
| Download progress | The function `http.client.stream` with `onResponse` and a chunk callback feeding a progress bar, for a 10 MB file with a known length, a photo behind a redirect and a chunked stream without a length, and `http.client.get` for the same files in one piece, each saved to the user folder with Varn's `fs`. |
| Errors | An unknown host, a refused connection, a request timeout, `async.timeout`, the statuses 404 and 500, an expired certificate and `response.json()` on a body that is not JSON, each with what came back and how long it took. |
| WebSocket echo | The function `net.connectWebSocket` to `wss://echo.websocket.org` with its `open`, `message`, `error` and `close` events, `send` and `sendBinary`, a round-trip ping made of a message the server echoes, and `close` with the codes 1000 and 4000. |
| Reconnection | The `reconnect` option against a server that refuses every attempt, with one bar for the wait before each attempt as it grows by the multiplier, until the socket gives up, and against the echo server. |
| Connection events | The events of every socket next to `webSocketConnected`, `webSocketDisconnected`, `webSocketReconnecting`, `networkOnline` and `networkOffline` of [`haylen.events`](../../../docs/lua-api/events.md), with counters and `net.openSocketCount()`. |
| Chat | A chat over the echo socket with reconnection, JSON messages, bubbles for sent and echoed messages and a queue for the messages written while the connection is down. |

The sample needs no server of its own: `source/services.lua` lists the public services it uses. Without a network every test stays up and shows why its requests fail, and the chat and the reconnection test keep retrying until the network comes back.

The engine socket answers the pings of a server by itself and has no call to send a ping frame, so the ping of the echo test is an ordinary message that the server sends back. In the browser, `http.client.stream` of Varn cannot stream and fails with its reason, while the fetch in one piece works, and sockets follow the rules of the browser, such as a page served over HTTPS reaching only `wss://` addresses.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run samples/system/network` |
| macOS app | `python3 make.py run samples/system/network --platform macos` |
| iPhone and iPad simulator | `python3 make.py run samples/system/network --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run samples/system/network --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run samples/system/network --platform android --device <serial>` |
| Browser | `python3 make.py run samples/system/network --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test, a button or a row | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Type an address or a message | Click the field and type, Enter sends | South button on the field opens the on-screen keyboard | Tap the field | Select the field |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
