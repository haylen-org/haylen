-- The local server of the Varn tests: one Varn HTTP app on 127.0.0.1 that answers JSON requests, streams server-sent events and a download, and echoes WebSocket messages, so the network tests never reach the internet. Varn gives a server no way to stop, so it runs until the app stops, and the first test that needs it starts it for every other.
local async = require('async')
local http = require('http')

local localServer = {}

localServer.host = '127.0.0.1'
-- Varn listeners share their port with any other Varn listener on it, so the server takes a random port of a high range, where another copy of the app is unlikely to listen.
localServer.ports = {41000, 48999}
localServer.downloadSize = 4 * 1024 * 1024

function localServer.start()
    if localServer.url then
        return localServer
    end
    local port = math.random(localServer.ports[1], localServer.ports[2])
    local app = http.createApp()
    localServer.app = app
    localServer.requests = 0
    localServer.closes = 0
    localServer.scores = {}
    localServer.download = string.rep('Varn', localServer.downloadSize // 4)

    app:use(function(ctx, next)
        localServer.requests = localServer.requests + 1
        next()
    end)

    app:get('/hello', function(ctx)
        local name = ctx.query.name or 'stranger'
        ctx:header('X-Served-By', 'Varn')
        ctx:json({message = 'Hello, ' .. name .. '.'})
    end)

    app:post('/scores', function(ctx)
        local entry = ctx:body()
        table.insert(localServer.scores, entry)
        ctx:status(201):json({saved = entry.player, count = #localServer.scores})
    end)

    app:delete('/scores', function(ctx)
        localServer.scores = {}
        ctx:status(204):send()
    end)

    app:get('/slow', function(ctx)
        async.sleep(3000):await()
        ctx:text('Too late.')
    end)

    app:get('/events', function(ctx)
        local stream = ctx:sse()
        for tick = 1, 6 do
            stream:send('tick', tostring(tick))
            async.sleep(150):await()
        end
        stream:close()
    end)

    app:get('/download', function(ctx)
        ctx:type('application/octet-stream'):send(localServer.download)
    end)

    app:ws('/echo', {
        open = function(conn)
            conn:send('Welcome.')
        end,
        message = function(conn, data)
            local news = data:match('^/broadcast (.+)$')
            if news then
                app:wsBroadcast('/echo', news)
                return
            end
            conn:send('Echo: ' .. data)
        end,
        close = function(conn)
            localServer.closes = localServer.closes + 1
        end,
    })

    app:listen({host = localServer.host, port = port})
    localServer.port = port
    localServer.url = string.format('http://%s:%d', localServer.host, port)
    localServer.ws = string.format('ws://%s:%d/echo', localServer.host, port)
    return localServer
end

return localServer
