-- The tests of the category in menu order.
return {
    prefix = 'NET',
    title = 'Network',
    description = 'HTTP requests with Varn "http" and WebSockets with "haylen.net" against public echo services, with downloads, errors, reconnection, connection events and a chat, and WebSockets, HTTP and sockets against servers inside the app at their limits.',
    tests = {
        {code = 'NET-001', title = 'HTTP requests', description = 'GET and POST over HTTPS with JSON, showing the status, the headers and the body.', module = 'http'},
        {code = 'NET-002', title = 'Download progress', description = 'A response streamed in chunks with a progress bar, then saved to the user folder.', module = 'download'},
        {code = 'NET-003', title = 'Errors', description = 'Unknown hosts, timeouts, error statuses, bad certificates and bodies that are not JSON.', module = 'errors'},
        {code = 'NET-004', title = 'WebSocket echo', description = 'Text and binary messages, a round-trip ping and closing over a secure WebSocket.', module = 'websocket'},
        {code = 'NET-005', title = 'Reconnection', description = 'Automatic reconnection with a growing wait between attempts.', module = 'reconnect'},
        {code = 'NET-006', title = 'Connection events', description = 'The events of a socket and the connection and network events of the event bus.', module = 'events'},
        {code = 'NET-007', title = 'Chat', description = 'A small chat over the echo socket that queues messages while it reconnects.', module = 'chat'},
        {code = 'NET-008', title = 'Local WebSocket', description = 'A large message, bytes that wait to be sent, a dropped connection that comes back, a connect timeout, a refused port and a socket that closes with its owner, against a server inside the app.', module = 'local-websocket',
            platforms = {'macos', 'windows', 'linux', 'ios', 'tvos', 'android', 'headless'},
            unsupported = {web = 'A browser page cannot accept connections, so the browser build of Varn has no WebSocket server, and this test never reaches the internet.'}},
        {code = 'NET-009', title = 'Local HTTP', description = 'A large upload, a response over its limit, many requests at once, a deadline of the app and a request that ends with its owner, against a server inside the app.', module = 'local-http',
            platforms = {'macos', 'windows', 'linux', 'ios', 'tvos', 'headless'},
            unsupported = {
                web = 'A browser page cannot accept connections, so the browser build of Varn has no HTTP server, and this test never reaches the internet.',
                android = 'Android refuses plain HTTP, which the server inside the app speaks, unless the network security configuration of the app allows it, and the Android template keeps the default that refuses it.',
            }},
        {code = 'NET-010', title = 'Local sockets', description = 'A large TCP stream with sends that wait for the peer, a silent peer, a refused port and a burst of UDP datagrams, on 127.0.0.1.', module = 'local-sockets',
            platforms = {'macos', 'windows', 'linux', 'ios', 'tvos', 'android', 'headless'},
            unsupported = {web = 'Browsers open no raw TCP or UDP sockets, so the browser build of Varn has no "socket" module.'}},
    },
}
