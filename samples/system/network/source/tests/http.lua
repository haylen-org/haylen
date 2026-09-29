-- HTTP requests with Varn's http module: http.client.get and http.client.post return promises that a task of the scene awaits, the json option sends a table as JSON, and the response carries the status, the headers with lowercase names and the body. A failure, such as no network, resolves the await with nil and the reason, which the page shows.
local haylen = require('haylen')
local http = require('http')
local ui = require('haylen.ui')

local sample = require('sample')
local services = require('services')

local Http = haylen.class('Http', sample.Test)

Http.hints = 'Pick a service and send a GET or a POST, or type any address. Switch between the headers and the body of the response with the tabs.'
Http.focus = 'service'

local kTimeout = 15

function Http:init(entry)
    Http.super.init(self, entry)
    self.service = services.http[1]
    self.address = 'https://httpbin.org/uuid'
    self.requests = 0
end

function Http:content()
    local items = {}
    for index, service in ipairs(services.http) do
        items[index] = {id = service.id, text = service.text}
    end
    return {
        ui.panel{width = 640, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Service'},
            ui.segmentedControl{id = 'service', items = items, selected = self.service.id, onChange = function(event)
                for _, service in ipairs(services.http) do
                    if service.id == event.value then
                        self.service = service
                    end
                end
            end},
            ui.button{id = 'get', text = 'GET with a query', align = 'stretch', onClick = function()
                self:request('GET', self.service.get, function()
                    return http.client.get(self.service.get, {query = {sample = 'haylen', language = 'lua'}, headers = {['Accept'] = 'application/json'}, timeoutSeconds = kTimeout})
                end)
            end},
            ui.button{id = 'post', text = 'POST JSON', variant = 'primary', align = 'stretch', onClick = function()
                self.requests = self.requests + 1
                local payload = {player = 'Ana', score = 4200 + self.requests, items = {'rope', 'lamp'}, sentAt = sample.clock()}
                self:request('POST', self.service.post, function()
                    return http.client.post(self.service.post, {json = payload, timeoutSeconds = kTimeout})
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
                ui.statusIndicator{id = 'status', text = 'No request yet', tone = 'information'},
                ui.spacer{grow = 1},
                ui.label{id = 'timing', text = '', color = 'textMuted'},
            },
            ui.label{id = 'request', text = '', font = 'monospace', color = 'accentText'},
            ui.tabs{id = 'tabs', grow = 1, items = {{id = 'body', text = 'Body'}, {id = 'headers', text = 'Headers'}}, selected = 'body',
                ui.scroll{ui.label{id = 'body', text = '', font = 'monospace'}},
                ui.scroll{ui.table{id = 'headers', columns = {{text = 'Header', width = 420}, {text = 'Value'}}, rows = {}}},
            },
        },
    }
end

function Http:getAddress()
    self:request('GET', self.address, function()
        return http.client.get(self.address, {timeoutSeconds = kTimeout})
    end)
end

-- Sends a request in a task of the scene and shows what came back: the status, the time it took, the headers and the body.
function Http:request(method, url, send)
    self:show('status', {text = 'Waiting for ' .. url, tone = 'information'})
    self:show('request', {text = method .. ' ' .. url})
    self:spawn(function()
        local started = sample.millis()
        local ok, response, failure = pcall(function()
            return send():await()
        end)
        local elapsed = string.format('%d ms', sample.millis() - started)
        if not ok or response == nil then
            self:show('status', {text = 'The request failed', tone = 'danger'})
            self:show('timing', {text = elapsed})
            self:show('body', {text = tostring(ok and failure or response)})
            self:show('headers', {rows = {}})
            return
        end

        local rows = {}
        for name, value in pairs(response.headers) do
            rows[#rows + 1] = {id = name, cells = {name, tostring(value)}}
        end
        table.sort(rows, function(a, b)
            return a.id < b.id
        end)
        self:show('status', {text = string.format('%d %s', response.status, response.ok and 'OK' or 'Error'), tone = response.ok and 'success' or 'warning'})
        self:show('timing', {text = string.format('%s, %s', elapsed, sample.bytes(#response.body))})
        self:show('body', {text = sample.body(response.body)})
        self:show('headers', {rows = rows})
    end)
end

return Http
