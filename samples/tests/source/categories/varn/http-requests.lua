-- HTTP requests with Varn's "http" module against the local server of the category: GET with a query, POST with JSON, other methods, error statuses, timeouts, a missing server and URL encoding, and a server of its own that the test opens on a free port and closes.
local datetime = require('datetime')
local haylen = require('haylen')
local http = require('http')

local localServer = require('categories.varn.local-server')
local VarnTest = require('categories.varn.varn-test')

local HttpRequests = haylen.class('HttpRequests', VarnTest)

HttpRequests.hint = 'The server runs inside the app on 127.0.0.1, and nothing leaves the device. Run again sends the requests again.'

HttpRequests.excerpts = {
    {'Server', [[
local app = http.createApp()
app:get('/hello', function(ctx)
    local name = ctx.query.name or 'stranger'
    ctx:header('X-Served-By', 'Varn')
    ctx:json({message = 'Hello, ' .. name .. '.'})
end)
app:post('/scores', function(ctx)
    local entry = ctx:body()
    table.insert(scores, entry)
    ctx:status(201):json({saved = entry.player, count = #scores})
end)
local server = app:listen({host = '127.0.0.1', port = 0})
local url = 'http://127.0.0.1:' .. server.port]]},
    {'Client', [[
local response = http.client.get(url .. '/hello', {
    query = {name = 'Ana Bo'},
}):await()
print(response.status, response.json().message)
print(response.headers['x-served-by'])

local saved = http.client.post(url .. '/scores', {
    json = {player = 'Ana', score = 4200},
}):await()

local _, failure = http.client.get(url .. '/slow', {
    timeoutSeconds = 0.5,
}):await()]]},
    {'Closing a server', [[
local server = app:listen({host = '127.0.0.1', port = 0})
local other = http.createApp()
local ok, busy = pcall(other.listen, other, {host = '127.0.0.1', port = server.port})
server:close({graceful = true}):await()
local _, refused = http.client.get(url):await()]]},
}

function HttpRequests:run()
    local url = localServer.start().url
    self:showServer()
    self:get(url)
    self:post(url)
    self:delete(url)
    self:missing(url)
    self:timeout(url)
    self:refused()
    self:encoding()
    self:closing()
end

function HttpRequests:update(dt)
    HttpRequests.super.update(self, dt)
    if localServer.url then
        self:showServer()
    end
end

function HttpRequests:showServer()
    self.results:set('server', 'info', 'Local server', string.format('A Varn app listens on "%s" and has served %d requests.', localServer.url, localServer.requests))
end

-- Awaits a request and fails the row with the reason when the request rejects.
function HttpRequests:answer(key, name, request)
    self.results:set(key, 'waiting', name, 'Waiting for the server.')
    local response, failure = request:await()
    if not response then
        self:check(key, name, false, 'The request failed: ' .. tostring(failure))
    end
    return response
end

function HttpRequests:get(url)
    local started = datetime.now():millis()
    local response = self:answer('get', 'GET with a query', http.client.get(url .. '/hello', {
        query = {name = 'Ana Bo'},
    }))
    if not response then
        return
    end
    local message, servedBy = response.json().message, response.headers['x-served-by']
    self:check('get', 'GET with a query', response.status == 200 and message == 'Hello, Ana Bo.' and servedBy == 'Varn', string.format('Status %d after %d ms with "%s", served by "%s" as "%s".', response.status, datetime.now():millis() - started, message, tostring(servedBy), tostring(response.headers['content-type'])))
end

function HttpRequests:post(url)
    local saved = self:answer('post', 'POST with JSON', http.client.post(url .. '/scores', {
        json = {player = 'Ana', score = 4200},
    }))
    if not saved then
        return
    end
    local body = saved.json()
    self:check('post', 'POST with JSON', saved.status == 201 and body.saved == 'Ana', string.format('Status %d, and the server saved the score of "%s" and keeps %d now.', saved.status, tostring(body.saved), body.count))
end

function HttpRequests:delete(url)
    local response = self:answer('delete', 'Other methods', http.client.request({url = url .. '/scores', method = 'DELETE'}))
    if response then
        self:check('delete', 'Other methods', response.status == 204 and response.body == '', string.format('Status %d with %d bytes of body for the method "DELETE" of "http.client.request".', response.status, #response.body))
    end
end

-- An error status still resolves the promise, and only a request that gets no answer rejects.
function HttpRequests:missing(url)
    local response = self:answer('missing', 'Error statuses', http.client.get(url .. '/missing'))
    if response then
        self:check('missing', 'Error statuses', response.status == 404 and not response.ok, string.format('Status %d with "%s" resolved the promise, and "ok" is %s.', response.status, response.body, tostring(response.ok)))
    end
end

function HttpRequests:timeout(url)
    self.results:set('timeout', 'waiting', 'Timeouts', 'Waiting at most half a second for "/slow", which answers after 3 seconds.')
    local started = datetime.now():millis()
    local response, failure = http.client.get(url .. '/slow', {
        timeoutSeconds = 0.5,
    }):await()
    self:check('timeout', 'Timeouts', response == nil and failure ~= nil, string.format('Rejected after %d ms: %s', datetime.now():millis() - started, tostring(failure)))
end

-- Nothing listens on port 1, so the connection is refused at once.
function HttpRequests:refused()
    self.results:set('refused', 'waiting', 'No server', 'Waiting for "http://127.0.0.1:1".')
    local started = datetime.now():millis()
    local response, failure = http.client.get('http://127.0.0.1:1/', {timeoutSeconds = 5}):await()
    self:check('refused', 'No server', response == nil and failure ~= nil, string.format('Rejected after %d ms: %s', datetime.now():millis() - started, tostring(failure)))
end

-- A server binds its port alone, reports the free port it got for port 0 and gives the port back once it closes.
function HttpRequests:closing()
    local name = 'Closing a server'
    self.results:set('closing', 'waiting', name, 'Opening a server on a free port.')
    local app = http.createApp()
    app:get('/', function(ctx)
        ctx:text('Open.')
    end)
    local server = app:listen({host = '127.0.0.1', port = 0})
    local url = string.format('http://127.0.0.1:%d/', server.port)
    local open = http.client.get(url):await()
    local other = http.createApp()
    local taken, busy = pcall(other.listen, other, {host = '127.0.0.1', port = server.port})
    if taken then
        busy:close():await()
    end
    server:close({graceful = true}):await()
    local after, refused = http.client.get(url, {timeoutSeconds = 5}):await()
    self:check('closing', name, open ~= nil and open.body == 'Open.' and not taken and after == nil, string.format('The server answered on port %d, a second server on that port failed with "%s", and once closed a request was refused: %s', server.port, tostring(busy), tostring(refused)))
end

function HttpRequests:encoding()
    local encoded = http.urlEncode('Ana & Bo')
    local decoded = http.urlDecode(encoded)
    self:check('encoding', 'URL encoding', encoded == 'Ana%20%26%20Bo' and decoded == 'Ana & Bo', string.format('The text "Ana & Bo" encodes as "%s" and decodes back to "%s".', encoded, decoded))
end

return HttpRequests
