-- The public services the tests talk to. They echo what they receive or serve test data, so the project needs no server of its own.
return {
    get = 'https://httpbin.org/get',
    post = 'https://httpbin.org/post',
    downloads = {
        {id = 'file', text = 'Large file', caption = '10 MB with its length', url = 'https://speed.cloudflare.com/__down?bytes=10000000', file = 'large-file.bin', icon = 'files/icons/binary.svg'},
        {id = 'redirect', text = 'After redirects', caption = 'A JSON document behind two redirects', url = 'https://httpbin.org/redirect/2', file = 'redirected.json', icon = 'files/icons/data.svg'},
        {id = 'chunked', text = 'Chunked stream', caption = '100 KB sent without a length', url = 'https://httpbin.org/stream-bytes/100000?chunk_size=4000', file = 'stream.bin', icon = 'delivery/icons/cloud.svg'},
    },
    echo = 'wss://echo.websocket.org',
    -- Nothing listens on this port of the device itself, so every connection is refused at once.
    unreachable = 'wss://127.0.0.1:9',
}
