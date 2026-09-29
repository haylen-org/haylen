-- Errors: a request that cannot reach a server resolves its await with nil and the reason instead of raising, a server that answers with an error status still gives a response whose ok is false, async.timeout gives up on a slow promise, and response.json raises for a body that is not JSON, which pcall catches. Nothing here stops the app, with or without a network.
local async = require('async')
local haylen = require('haylen')
local http = require('http')
local ui = require('haylen.ui')

local sample = require('sample')

local Errors = haylen.class('Errors', sample.Test)

Errors.hints = 'Try each failure. Every result shows the call, what came back and how long it took, and the app keeps running whatever the network does.'
Errors.focus = 'host'

-- Each case sends one request and turns what came back into a line of text.
local kCases = {
    {id = 'host', text = 'Unknown host', call = "http.client.get('https://no-such-host.invalid/')", run = function()
        return http.client.get('https://no-such-host.invalid/', {timeoutSeconds = 10}):await()
    end},
    {id = 'refused', text = 'Refused connection', call = "http.client.get('http://127.0.0.1:9/')", run = function()
        return http.client.get('http://127.0.0.1:9/', {timeoutSeconds = 10}):await()
    end},
    {id = 'timeout', text = 'Request timeout', call = "http.client.get('https://httpbin.org/delay/5', {timeoutSeconds = 2})", run = function()
        return http.client.get('https://httpbin.org/delay/5', {timeoutSeconds = 2}):await()
    end},
    {id = 'giveUp', text = 'async.timeout', call = "async.timeout(http.client.get('https://httpbin.org/delay/5'), 1500)", run = function()
        return async.timeout(http.client.get('https://httpbin.org/delay/5', {timeoutSeconds = 10}), 1500):await()
    end},
    {id = 'missing', text = 'Status 404', call = "http.client.get('https://httpbin.org/status/404')", run = function()
        return http.client.get('https://httpbin.org/status/404', {timeoutSeconds = 10}):await()
    end},
    {id = 'server', text = 'Status 500', call = "http.client.get('https://httpbin.org/status/500')", run = function()
        return http.client.get('https://httpbin.org/status/500', {timeoutSeconds = 10}):await()
    end},
    {id = 'certificate', text = 'Expired certificate', call = "http.client.get('https://expired.badssl.com/')", run = function()
        return http.client.get('https://expired.badssl.com/', {timeoutSeconds = 10}):await()
    end},
    {id = 'notJson', text = 'Body that is not JSON', call = "http.client.get('https://httpbin.org/html').json()", json = true, run = function()
        return http.client.get('https://httpbin.org/html', {timeoutSeconds = 10}):await()
    end},
}

-- Describes a response: its status and ok flag, or the error json() raised when the case reads the body as JSON.
local function describe(case, response)
    if case.json then
        local ok, failure = pcall(response.json)
        return ok and 'The body was JSON after all.' or 'Status ' .. response.status .. ', then json() raised: ' .. tostring(failure)
    end
    return string.format('A response with status %d and ok %s', response.status, tostring(response.ok))
end

function Errors:init(entry)
    Errors.super.init(self, entry)
    self.results = {}
end

function Errors:content()
    local buttons = {}
    for index, case in ipairs(kCases) do
        buttons[index] = ui.button{id = case.id, text = case.text, align = 'stretch', onClick = function()
            self:try(case)
        end}
    end
    return {
        ui.panel{width = 520, align = 'stretch', gap = 12, children = buttons},
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Results'},
            ui.scroll{grow = 1, ui.column{id = 'results', gap = 16, padding = {0, 24, 0, 0}}},
        },
    }
end

-- Runs a case in a task of the scene and shows its outcome at the top of the results.
function Errors:try(case)
    local result = {call = case.call, outcome = 'Waiting…', color = 'textMuted'}
    table.insert(self.results, 1, result)
    self:showResults()
    self:spawn(function()
        local started = sample.millis()
        local response, failure = case.run()
        result.outcome = string.format('%d ms: %s', sample.millis() - started, response and describe(case, response) or 'nil and ' .. tostring(failure))
        result.color = response and 'warningText' or 'dangerText'
        self:showResults()
    end)
end

function Errors:showResults()
    local nodes = {}
    for index, result in ipairs(self.results) do
        nodes[index] = ui.column{gap = 4,
            ui.label{text = result.call, font = 'monospace', color = 'accentText'},
            ui.label{text = result.outcome, color = result.color},
        }
    end
    self.document:replaceChildren('results', nodes)
end

return Errors
