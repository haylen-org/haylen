-- The local server of the Varn and network tests: one Varn HTTP app on 127.0.0.1 that answers JSON requests, measures uploads, streams server-sent events, a download and a body it writes in parts, and echoes WebSocket messages, so these tests never reach the internet. The first test that needs it starts it for every other, on a free port the system picks, and it runs until the app stops.
local async = require('async')
local http = require('http')

local localServer = {}

localServer.host = '127.0.0.1'
localServer.downloadSize = 4 * 1024 * 1024
localServer.parts = 4

function localServer.start()
    if localServer.url then
        return localServer
    end
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

    -- Every write goes out as soon as the client takes it, its promise settles then, and the end of the handler ends the body. The URL Loading System of Apple platforms holds back the first bytes of a body whose type it may sniff, which "nosniff" turns off.
    app:get('/parts', function(ctx)
        ctx:type('text/plain'):header('X-Content-Type-Options', 'nosniff')
        for part = 1, localServer.parts do
            if not ctx:write('Part ' .. part .. '\n'):await() then
                return
            end
            async.sleep(100):await()
        end
    end)

    app:post('/upload', function(ctx)
        local body = ctx.req.body
        ctx:json({received = #body, intact = body == string.rep('u', #body)})
    end)

    app:ws('/echo', {
        open = function(conn)
            conn:send('Welcome.')
        end,
        message = function(conn, data, binary)
            if data == '/drop' then
                conn:close()
                return
            end
            local news = not binary and data:match('^/broadcast (.+)$')
            if news then
                app:wsBroadcast('/echo', news)
                return
            end
            conn:send(binary and data or 'Echo: ' .. data, {binary = binary})
        end,
        close = function(conn, code, reason)
            localServer.closes = localServer.closes + 1
            localServer.lastClose = {code = code, reason = reason}
        end,
    })

    -- Port 0 asks the system for a free port, which the server then reports.
    local server = app:listen({host = localServer.host, port = 0})
    localServer.server = server
    localServer.url = string.format('http://%s:%d', server.host, server.port)
    localServer.ws = string.format('ws://%s:%d/echo', server.host, server.port)
    return localServer
end

return localServer
