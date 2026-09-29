-- The public services the tests talk to. They echo what they receive, so the sample needs no server of its own.
return {
    http = {
        {id = 'httpbin', text = 'httpbin.org', get = 'https://httpbin.org/get', post = 'https://httpbin.org/post'},
        {id = 'postman', text = 'postman-echo.com', get = 'https://postman-echo.com/get', post = 'https://postman-echo.com/post'},
    },
    downloads = {
        {id = 'file', text = 'Test file', caption = '10 MB from the Cloudflare speed test, with its length', url = 'https://speed.cloudflare.com/__down?bytes=10000000', file = 'test-file.bin'},
        {id = 'photo', text = 'Photo', caption = 'A 3840 by 2160 JPEG from Lorem Picsum, after a redirect', url = 'https://picsum.photos/3840/2160', file = 'photo.jpg'},
        {id = 'chunked', text = 'Chunked stream', caption = '100 KB from httpbin.org, sent without a length', url = 'https://httpbin.org/stream-bytes/102400?chunk_size=4096', file = 'stream.bin'},
    },
    echo = 'wss://echo.websocket.org',
    -- Nothing listens on this port of the device itself, so every connection is refused at once.
    unreachable = 'wss://127.0.0.1:9',
}
