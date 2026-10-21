-- Responses that arrive in pieces with "http.client.stream" of Varn: server-sent events that reach the app one by one as the local server sends them, a body the server writes in parts with "ctx:write", and a download with its progress.
local datetime = require('datetime')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local http = require('http')

local Test = require('harness.test')
local localServer = require('categories.varn.local-server')
local VarnTest = require('categories.varn.varn-test')

local HttpStreaming = haylen.class('HttpStreaming', VarnTest)

HttpStreaming.hint = 'The dots mark when each event reached the app, on a line of one second with marks 100 ms apart, and the bar fills with the download. Run again streams again.'
HttpStreaming.span = 1000
HttpStreaming.stageHeight = 230
HttpStreaming.margin = 24
HttpStreaming.bar = 22

HttpStreaming.excerpts = {
    {'Server', [[
app:get('/events', function(ctx)
    local stream = ctx:sse()
    for tick = 1, 6 do
        stream:send('tick', tostring(tick))
        async.sleep(150):await()
    end
    stream:close()
end)]]},
    {'Writing in parts', [[
app:get('/parts', function(ctx)
    ctx:type('text/plain')
    ctx:header('X-Content-Type-Options', 'nosniff')
    for part = 1, 4 do
        local line = 'Part ' .. part .. '\n'
        if not ctx:write(line):await() then
            return
        end
        async.sleep(100):await()
    end
end)]]},
    {'Client', [[
local done, failure = http.client.stream({
    url = url .. '/events',
    onResponse = function(status, headers)
        head = {status, headers['content-type']}
    end,
}, function(chunk)
    for tick in chunk:gmatch('data: (%d+)') do
        events[#events + 1] = tonumber(tick)
    end
end):await()]]},
}

function HttpStreaming:run()
    local url = localServer.start().url
    self.events = nil
    self.download = nil
    self:serverSentEvents(url)
    self:writtenParts(url)
    self:largeDownload(url)
end

-- The callbacks of a stream only keep what arrives, and the task of the test reads it, so nothing of the test runs after it exits.
function HttpStreaming:serverSentEvents(url)
    self.results:set('head', 'waiting', 'The head first', 'Waiting for the head of "/events".')
    self.results:set('events', 'waiting', 'Events as they come', 'Waiting for the events of "/events".')
    local started = datetime.now():millis()
    local head, events, headFirst = {}, {started = started}, nil
    self.events = events

    local done, failure = http.client.stream({
        url = url .. '/events',
        onResponse = function(status, headers)
            head = {status, headers['content-type']}
        end,
    }, function(chunk)
        if headFirst == nil then
            headFirst = head[1] ~= nil
        end
        for tick in chunk:gmatch('data: (%d+)') do
            events[#events + 1] = {tick = tonumber(tick), at = datetime.now():millis() - started}
        end
    end):await()
    events.finished = datetime.now():millis() - started

    if not done then
        self:check('events', 'Events as they come', false, 'The stream failed: ' .. tostring(failure))
        return
    end
    local spread = #events > 0 and events[#events].at - events[1].at or 0
    self:check('head', 'The head first', headFirst and head[1] == 200 and tostring(head[2]):find('text/event-stream', 1, true) ~= nil, string.format('The function "onResponse" saw status %s with "%s" before the first piece of the body.', tostring(head[1]), tostring(head[2])))
    self:check('events', 'Events as they come', #events == 6 and spread >= 500, string.format('Got %d events spread over %d ms, each one when the server sent it.', #events, spread))
end

-- Each "ctx:write" leaves as soon as the client takes it, so the parts arrive while the handler still runs instead of together at its end.
function HttpStreaming:writtenParts(url)
    local name = 'Writes that stream'
    self.results:set('parts', 'waiting', name, 'Waiting for the parts of "/parts".')
    local started = datetime.now():millis()
    local arrivals = {}
    local done, failure = http.client.stream({url = url .. '/parts'}, function(chunk)
        for part in chunk:gmatch('Part (%d+)') do
            arrivals[#arrivals + 1] = {part = tonumber(part), at = datetime.now():millis() - started}
        end
    end):await()

    if not done then
        self:check('parts', name, false, 'The stream failed: ' .. tostring(failure))
        return
    end
    local spread = #arrivals > 0 and arrivals[#arrivals].at - arrivals[1].at or 0
    self:check('parts', name, #arrivals == localServer.parts and spread >= 250, string.format('Got %d parts spread over %d ms, each one when the handler wrote it.', #arrivals, spread))
end

function HttpStreaming:largeDownload(url)
    self.results:set('download', 'waiting', 'A download in pieces', 'Waiting for "/download".')
    local download = {received = 0, pieces = 0}
    self.download = download

    local done, failure = http.client.stream({
        url = url .. '/download',
        onResponse = function(status, headers)
            download.expected = tonumber(headers['content-length'])
        end,
    }, function(chunk)
        download.received = download.received + #chunk
        download.pieces = download.pieces + 1
    end):await()

    if not done then
        self:check('download', 'A download in pieces', false, 'The stream failed: ' .. tostring(failure))
        return
    end
    local length = download.expected and string.format('the %d bytes the head announced', download.expected) or 'a length the head did not announce'
    self:check('download', 'A download in pieces', download.received == localServer.downloadSize and download.pieces > 1, string.format('Received %d bytes in %d pieces, of %s.', download.received, download.pieces, length))
end

function HttpStreaming:draw(area)
    local margin = HttpStreaming.margin
    local left, right = margin, area.width - margin
    if self.events then
        self:drawEvents(left, right, margin)
    end
    if self.download then
        self:drawDownload(left, right, margin + 116)
    end
end

-- Draws the second of the events as a groove with a mark every 100 ms, the moment the stream reached as a fill and every event as a numbered dot where it arrived.
function HttpStreaming:drawEvents(left, right, y)
    local events = self.events
    local bar = HttpStreaming.bar
    local scale = (right - left) / HttpStreaming.span
    local line = y + 60
    graphics2d.drawText(nil, 'Arrival of the events', left, y, {size = 20, color = Test.ink, layer = 2})
    graphics2d.drawShape({left, line - bar / 2, right - left, bar}, {radius = bar / 2, color = Test.line})
    for step = 100, HttpStreaming.span - 100, 100 do
        graphics2d.drawCircle(left + step * scale, line, 3, Test.muted, {layer = 1})
    end
    local now = events.finished or datetime.now():millis() - events.started
    graphics2d.drawShape({left, line - bar / 2, math.max(bar, math.min(now, HttpStreaming.span) * scale), bar}, {radius = bar / 2, color = '#664C7DFF', layer = 1})
    for _, event in ipairs(events) do
        local x = left + math.min(event.at, HttpStreaming.span) * scale
        graphics2d.drawCircle(x, line, 16, Test.green, {layer = 2})
        graphics2d.drawText(nil, tostring(event.tick), x, line, {size = 17, color = '#FF101418', anchor = {0.5, 0.5}, layer = 3})
    end
end

-- Draws the download as a groove that fills with round ends as its pieces arrive.
function HttpStreaming:drawDownload(left, right, y)
    local download = self.download
    local bar = HttpStreaming.bar
    local fraction = download.expected and download.received / download.expected or 0
    local text = download.expected and string.format('Downloaded %.1f of %.1f MB in %d pieces', download.received / 1048576, download.expected / 1048576, download.pieces) or string.format('Downloaded %.1f MB in %d pieces', download.received / 1048576, download.pieces)
    graphics2d.drawText(nil, text, left, y, {size = 20, color = Test.ink, layer = 2})
    graphics2d.drawShape({left, y + 34, right - left, bar}, {radius = bar / 2, color = Test.line})
    if fraction > 0 then
        graphics2d.drawShape({left, y + 34, math.max(bar, (right - left) * math.min(1, fraction)), bar}, {radius = bar / 2, color = Test.accent, layer = 1})
    end
end

return HttpStreaming
