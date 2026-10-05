-- HTTP requests with Varn `http`: `http.client.get` and `http.client.post` return promises that a task of the scene awaits, the `json` option sends a table as JSON, and the response carries the status, the headers with lowercase names and the body. A failure, such as no network, resolves the `await` with `nil` and the reason, which the page shows.
local haylen = require('haylen')
local http = require('http')
local ui = require('haylen.ui')

local Test = require('harness.test')
local readout = require('categories.network.readout')
local services = require('categories.network.services')

local Http = haylen.class('Http', Test)

Http.timeout = 15

function Http:init(entry)
    Http.super.init(self, entry)
    self.address = 'https://httpbin.org/uuid'
    self.requests = 0
end

function Http:enter()
    self:frame{
        hint = 'Send a GET or a POST to the echo service, or type any address. Switch between the body and the headers of the response with the tabs.',
        focus = 'get',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 640, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Echo service'},
                ui.label{text = services.get, font = 'monospace', color = 'textMuted'},
                ui.button{id = 'get', text = 'GET with a query', align = 'stretch', onClick = function()
                    self:request('GET', services.get, function()
                        return http.client.get(services.get, {query = {test = 'haylen', language = 'lua'}, headers = {['Accept'] = 'application/json'}, timeoutSeconds = Http.timeout})
                    end)
                end},
                ui.button{id = 'post', text = 'POST JSON', variant = 'primary', align = 'stretch', onClick = function()
                    self.requests = self.requests + 1
                    local payload = {player = 'Ana', score = 4200 + self.requests, items = {'rope', 'lamp'}, sentAt = readout.clock()}
                    self:request('POST', services.post, function()
                        return http.client.post(services.post, {json = payload, timeoutSeconds = Http.timeout})
                    end)
                end},
                ui.sectionTitle{text = 'Any address'},
                ui.textField{id = 'address', value = self.address, keyboard = 'url', returnKey = 'go', onChange = function(event)
                    self.address = event.value
                end, onSubmit = function()
                    self:getAddress()
                end},
                ui.button{id = 'fetch', text = 'GET this address', align = 'stretch', onClick = function()
                    self:getAddress()
                end},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.row{gap = 12,
                    ui.statusIndicator{id = 'state', text = 'No request yet', tone = 'information'},
                    ui.spacer{grow = 1},
                    ui.label{id = 'timing', text = '', color = 'textMuted'},
                },
                ui.label{id = 'request', text = '', font = 'monospace', color = 'accentText'},
                ui.tabs{id = 'tabs', grow = 1, items = {{id = 'body', text = 'Body'}, {id = 'headers', text = 'Headers'}}, selected = 'body',
                    ui.scroll{ui.label{id = 'body', text = '', font = 'monospace'}},
                    ui.scroll{ui.table{id = 'headers', columns = {{text = 'Header', width = 420}, {text = 'Value'}}, rows = {}}},
                },
            },
        }},
    }
end

function Http:getAddress()
    self:request('GET', self.address, function()
        return http.client.get(self.address, {timeoutSeconds = Http.timeout})
    end)
end

-- Sends a request in a task of the scene and shows what came back: the status, the time it took, the headers and the body.
function Http:request(method, url, send)
    self:set('state', {text = 'Waiting for "' .. url .. '"', tone = 'information'})
    self:set('request', {text = method .. ' ' .. url})
    self:spawn(function()
        local started = readout.millis()
        local ok, response, failure = pcall(function()
            return send():await()
        end)
        local elapsed = string.format('%d ms', readout.millis() - started)
        if not ok or response == nil then
            self:set('state', {text = 'The request failed', tone = 'danger'})
            self:set('timing', {text = elapsed})
            self:set('body', {text = tostring(ok and failure or response)})
            self:set('headers', {rows = {}})
            return
        end

        local rows = {}
        for name, value in pairs(response.headers) do
            rows[#rows + 1] = {id = name, cells = {name, tostring(value)}}
        end
        table.sort(rows, function(a, b)
            return a.id < b.id
        end)
        self:set('state', {text = string.format('Status %d, %s', response.status, response.ok and 'OK' or 'error'), tone = response.ok and 'success' or 'warning'})
        self:set('timing', {text = string.format('%s, %s', elapsed, readout.bytes(#response.body))})
        self:set('body', {text = readout.body(response.body)})
        self:set('headers', {rows = rows})
    end)
end

return Http
