-- The tests of the category in menu order.
return {
    prefix = 'NET',
    title = 'Network',
    description = 'HTTP requests with Varn "http" and WebSockets with "haylen.net" against public echo services, with downloads, errors, reconnection, connection events and a chat.',
    tests = {
        {code = 'NET-001', title = 'HTTP requests', description = 'GET and POST over HTTPS with JSON, showing the status, the headers and the body.', module = 'http'},
        {code = 'NET-002', title = 'Download progress', description = 'A response streamed in chunks with a progress bar, then saved to the user folder.', module = 'download'},
        {code = 'NET-003', title = 'Errors', description = 'Unknown hosts, timeouts, error statuses, bad certificates and bodies that are not JSON.', module = 'errors'},
        {code = 'NET-004', title = 'WebSocket echo', description = 'Text and binary messages, a round-trip ping and closing over a secure WebSocket.', module = 'websocket'},
        {code = 'NET-005', title = 'Reconnection', description = 'Automatic reconnection with a growing wait between attempts.', module = 'reconnect'},
        {code = 'NET-006', title = 'Connection events', description = 'The events of a socket and the connection and network events of the event bus.', module = 'events'},
        {code = 'NET-007', title = 'Chat', description = 'A small chat over the echo socket that queues messages while it reconnects.', module = 'chat'},
    },
}
