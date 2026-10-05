-- Responses that arrive in pieces with "http.client.stream" of Varn: server-sent events that reach the app one by one as the local server sends them, and a download with its progress.
local datetime = require('datetime')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local http = require('http')

local Test = require('harness.test')
local localServer = require('categories.varn.local-server')
local VarnTest = require('categories.varn.varn-test')

local HttpStreaming = haylen.class('HttpStreaming', VarnTest)

HttpStreaming.hint = 'The dots mark when each event reached the app, on a line of one second with marks 100 ms apart. Run again streams again.'
HttpStreaming.span = 1000

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
    local y = self.results:draw(area) + 8
    local left, right = 24, area.width - 24
    if self.events then
        self:drawEvents(left, right, y)
        y = y + 90
    end
    if self.download then
        self:drawDownload(left, right, y)
    end
end

function HttpStreaming:drawEvents(left, right, y)
    local events = self.events
    local scale = (right - left) / HttpStreaming.span
    local line = y + 50
    Test.caption('Arrival of the events', left, y)
    graphics2d.drawLine(left, line, right, line, 2, Test.line)
    for step = 0, HttpStreaming.span, 100 do
        graphics2d.drawLine(left + step * scale, line - 8, left + step * scale, line + 8, 2, Test.line)
    end

    local now = events.finished or datetime.now():millis() - events.started
    graphics2d.drawLine(left + math.min(now, HttpStreaming.span) * scale, line - 16, left + math.min(now, HttpStreaming.span) * scale, line + 16, 2, Test.warm)
    for _, event in ipairs(events) do
        local x = left + math.min(event.at, HttpStreaming.span) * scale
        graphics2d.drawCircle(x, line, 15, Test.green, {layer = 1})
        Test.caption(tostring(event.tick), x, line, {size = 17, color = '#FF101418', anchor = {0.5, 0.5}})
    end
end

function HttpStreaming:drawDownload(left, right, y)
    local download = self.download
    local fraction = download.expected and download.received / download.expected or 0
    Test.caption(string.format('Downloaded %d of %s bytes', download.received, download.expected or 'unknown'), left, y)
    graphics2d.drawRect({left, y + 32, right - left, 20}, Test.surface)
    graphics2d.drawRect({left, y + 32, (right - left) * math.min(1, fraction), 20}, Test.accent, {layer = 1})
end

return HttpStreaming
