-- The tests of the sample in menu order. Each module returns a scene class that takes its entry.
return {
    {id = 'http', title = 'HTTP requests', description = 'GET and POST over HTTPS with JSON, showing the status, the headers and the body.', module = 'tests.http'},
    {id = 'download', title = 'Download progress', description = 'A response streamed in chunks with a progress bar, then saved to the user folder.', module = 'tests.download'},
    {id = 'errors', title = 'Errors', description = 'Unknown hosts, timeouts, error statuses, bad certificates and bodies that are not JSON.', module = 'tests.errors'},
    {id = 'websocket', title = 'WebSocket echo', description = 'Text and binary messages, a round-trip ping and closing over a secure WebSocket.', module = 'tests.websocket'},
    {id = 'reconnect', title = 'Reconnection', description = 'Automatic reconnection with a growing wait between attempts.', module = 'tests.reconnect'},
    {id = 'events', title = 'Connection events', description = 'The events of a socket and the connection and network events of the event bus.', module = 'tests.events'},
    {id = 'chat', title = 'Chat', description = 'A small chat over the echo socket that queues messages while it reconnects.', module = 'tests.chat'},
}
