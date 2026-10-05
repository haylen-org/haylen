-- HTTP of Varn against the local server of the app, through what requests meet outside a demo: a large upload, a response over the limit of the request, many requests at once, a deadline of the app and a request whose task ends with its owner.
local async = require('async')
local datetime = require('datetime')
local haylen = require('haylen')
local http = require('http')
local scene = require('haylen.scene')

local localServer = require('categories.varn.local-server')
local VarnTest = require('categories.varn.varn-test')

local LocalHttp = haylen.class('LocalHttp', VarnTest)

LocalHttp.hint = 'The server runs inside the app on 127.0.0.1, and nothing leaves the device. Run again sends every request again.'
LocalHttp.uploadSize = 4 * 1024 * 1024
LocalHttp.parallel = 20
-- The route "/slow" answers after 3 seconds, so the owned request ends its check a little later.
LocalHttp.slowMillis = 3200

LocalHttp.excerpts = {
    {'Large bodies and limits', [[
http.client.post(url .. '/upload', {
    body = string.rep('u', 4 * 1024 * 1024),
    headers = {['Content-Type'] = 'application/octet-stream'},
}):await()
http.client.get(url .. '/download', {maxResponseBytes = 1024 * 1024}):await()]]},
    {'Many at once and deadlines', [[
local requests = {}
for index = 1, 20 do
    requests[index] = http.client.get(url .. '/hello', {query = {name = 'Guest ' .. index}})
end
async.all(requests):await()
async.timeout(http.client.get(url .. '/slow'), 300):await()]]},
    {'Owners', [[
scene.spawn(holder, function()
    http.client.get(url .. '/slow'):await()
    print('Never runs once the holder ended.')
end)]]},
}

function LocalHttp:run()
    local url = localServer.start().url
    local owned = self:startOwned(url)
    self:upload(url)
    self:limit(url)
    self:many(url)
    self:deadline(url)
    self:finishOwned(owned)
end

function LocalHttp:upload(url)
    local name = 'Large upload'
    self.results:set('upload', 'waiting', name, string.format('Sending %d bytes to "/upload".', LocalHttp.uploadSize))
    local started = datetime.now():millis()
    local response, failure = http.client.post(url .. '/upload', {
        body = string.rep('u', LocalHttp.uploadSize),
        headers = {['Content-Type'] = 'application/octet-stream'},
        timeoutSeconds = 20,
    }):await()
    if not response then
        self:check('upload', name, false, 'The request failed: ' .. tostring(failure))
        return
    end
    local body = response.json()
    self:check('upload', name, response.status == 200 and body.received == LocalHttp.uploadSize and body.intact, string.format('Status %d after %d ms, and the server read %d intact bytes.', response.status, datetime.now():millis() - started, body.received))
end

-- A response larger than "maxResponseBytes" rejects the request instead of filling the memory of the app.
function LocalHttp:limit(url)
    local name = 'Response limit'
    self.results:set('limit', 'waiting', name, 'Reading "/download", which is larger than the limit of 1 MiB.')
    local response, failure = http.client.get(url .. '/download', {maxResponseBytes = 1024 * 1024}):await()
    self:check('limit', name, response == nil and failure ~= nil, response and string.format('The request resolved with %d bytes.', #response.body) or string.format('The request of %d bytes rejected: %s', localServer.downloadSize, tostring(failure)))
end

function LocalHttp:many(url)
    local name = 'Many at once'
    self.results:set('many', 'waiting', name, string.format('Sending %d requests at once.', LocalHttp.parallel))
    local started = datetime.now():millis()
    local requests = {}
    for index = 1, LocalHttp.parallel do
        requests[index] = http.client.get(url .. '/hello', {query = {name = 'Guest ' .. index}})
    end
    local responses, failure = async.all(requests):await()
    if not responses then
        self:check('many', name, false, 'A request failed: ' .. tostring(failure))
        return
    end
    local matching = 0
    for index, response in ipairs(responses) do
        if response.status == 200 and response.json().message == string.format('Hello, Guest %d.', index) then
            matching = matching + 1
        end
    end
    self:check('many', name, matching == LocalHttp.parallel, string.format('%d of %d answers matched their requests, all in %d ms.', matching, LocalHttp.parallel, datetime.now():millis() - started))
end

-- A deadline of the app on any promise, which stops waiting for a request without changing its options.
function LocalHttp:deadline(url)
    local name = 'Deadline of the app'
    self.results:set('deadline', 'waiting', name, 'Waiting at most 300 ms for "/slow", which answers after 3 seconds.')
    local started = datetime.now():millis()
    local response, failure = async.timeout(http.client.get(url .. '/slow'), 300):await()
    local elapsed = datetime.now():millis() - started
    self:check('deadline', name, response == nil and failure ~= nil and elapsed < 1000, string.format('Rejected after %d ms: %s', elapsed, tostring(failure)))
end

-- Starts a request in a task that a table owns and lets the table go, so the task must close at once and never resume when the answer comes.
function LocalHttp:startOwned(url)
    local owned = {started = datetime.now():millis()}
    self.results:set('owner', 'waiting', 'Ended with its owner', 'Waiting on "/slow" in a task that a table owns.')
    local holder = {}
    scene.spawn(holder, function()
        local closing <close> = setmetatable({}, {__close = function()
            owned.closedAt = datetime.now():millis()
        end})
        http.client.get(url .. '/slow'):await()
        owned.resumed = true
    end)
    holder = nil
    collectgarbage()
    collectgarbage()
    return owned
end

function LocalHttp:finishOwned(owned)
    local left = owned.started + LocalHttp.slowMillis - datetime.now():millis()
    if left > 0 then
        async.sleep(left):await()
    end
    local closedAfter = owned.closedAt and string.format('closed %d ms after the start', owned.closedAt - owned.started) or 'never closed'
    self:check('owner', 'Ended with its owner', owned.closedAt ~= nil and not owned.resumed, string.format('The task %s, and %s once "/slow" answered.', closedAfter, owned.resumed and 'it resumed' or 'it did not resume'))
end

return LocalHttp
