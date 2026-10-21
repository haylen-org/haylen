-- A request that cannot reach a server resolves its `await` with `nil` and the reason instead of raising, a server that answers with an error status still gives a response whose `ok` is `false`, `async.timeout` gives up on a slow promise, and `response.json` raises for a body that is not JSON, which `pcall` catches. Nothing here stops the app, with or without a network.
local async = require('async')
local haylen = require('haylen')
local http = require('http')
local ui = require('haylen.ui')

local Test = require('harness.test')
local readout = require('categories.network.readout')

local Errors = haylen.class('Errors', Test)

Errors.code = [[
local response, failure = http.client.get('https://no-such-host.invalid/', {timeoutSeconds = 10}):await()  -- No response, and the reason.
local missing = http.client.get('https://httpbin.org/status/404'):await()  -- A response whose `ok` is `false`.
local late = async.timeout(http.client.get('https://httpbin.org/delay/5'), 1500):await()
local page = http.client.get('https://httpbin.org/html'):await()
local ok, problem = pcall(page.json)  -- The body is not JSON, so it raises.]]

-- Each case sends one request and turns what came back into a line of text.
Errors.cases = {
    {id = 'host', group = 'network', text = 'Unknown host', call = "http.client.get('https://no-such-host.invalid/')", run = function()
        return http.client.get('https://no-such-host.invalid/', {timeoutSeconds = 10}):await()
    end},
    {id = 'refused', group = 'network', text = 'Refused connection', call = "http.client.get('http://127.0.0.1:9/')", run = function()
        return http.client.get('http://127.0.0.1:9/', {timeoutSeconds = 10}):await()
    end},
    {id = 'timeout', group = 'time', text = 'Request timeout', call = "http.client.get('https://httpbin.org/delay/5', {timeoutSeconds = 2})", run = function()
        return http.client.get('https://httpbin.org/delay/5', {timeoutSeconds = 2}):await()
    end},
    {id = 'giveUp', group = 'time', text = 'Timeout with "async.timeout"', call = "async.timeout(http.client.get('https://httpbin.org/delay/5'), 1500)", run = function()
        return async.timeout(http.client.get('https://httpbin.org/delay/5', {timeoutSeconds = 10}), 1500):await()
    end},
    {id = 'missing', group = 'status', text = 'Status 404', call = "http.client.get('https://httpbin.org/status/404')", run = function()
        return http.client.get('https://httpbin.org/status/404', {timeoutSeconds = 10}):await()
    end},
    {id = 'server', group = 'status', text = 'Status 500', call = "http.client.get('https://httpbin.org/status/500')", run = function()
        return http.client.get('https://httpbin.org/status/500', {timeoutSeconds = 10}):await()
    end},
    {id = 'certificate', group = 'network', text = 'Expired certificate', call = "http.client.get('https://expired.badssl.com/')", run = function()
        return http.client.get('https://expired.badssl.com/', {timeoutSeconds = 10}):await()
    end},
    {id = 'notJson', group = 'content', text = 'Body that is not JSON', call = "http.client.get('https://httpbin.org/html').json()", json = true, run = function()
        return http.client.get('https://httpbin.org/html', {timeoutSeconds = 10}):await()
    end},
}

-- Describes a response in a badge and a sentence: its status and `ok` flag, or the error `json()` raised when the case reads the body as JSON.
local function describe(case, response)
    if case.json then
        local ok, failure = pcall(response.json)
        return ok and 'JSON' or 'Not JSON', ok and 'The body was JSON after all.' or string.format('The status was %d, and "json()" raised: %s', response.status, tostring(failure))
    end
    return 'Status ' .. response.status, string.format('A response came back with the status %d and "ok" set to "%s".', response.status, tostring(response.ok))
end

-- The kinds of failure, each with a title over its buttons and the tone of its results.
Errors.groups = {
    {id = 'network', title = 'No connection', tone = 'danger'},
    {id = 'time', title = 'Too slow', tone = 'warning'},
    {id = 'status', title = 'Error statuses', tone = 'information'},
    {id = 'content', title = 'Bad content', tone = 'accent'},
}

-- The cases that answer at once, which the test tries when it opens.
Errors.opening = {'refused', 'missing', 'notJson'}

-- Every case has a button in its group and a card on the board of results, in the same order, so the board shows every failure from the start.
function Errors:enter()
    local controls, cards = {}, {}
    for _, group in ipairs(Errors.groups) do
        controls[#controls + 1] = ui.label{text = group.title, font = 'caption', color = 'textMuted', margin = {#controls > 0 and 8 or 0, 0, 0, 0}}
        for _, case in ipairs(Errors.cases) do
            if case.group == group.id then
                controls[#controls + 1] = ui.button{id = case.id, text = case.text, align = 'stretch', onClick = function()
                    self:try(case)
                    self.gui:command('results', 'scrollTo', {node = case.id .. '-card'})
                end}
                cards[#cards + 1] = ui.card{id = case.id .. '-card', align = 'stretch', gap = 6, padding = {14, 20},
                    ui.row{gap = 12, ui.label{text = case.text, font = 'heading', grow = 1}, ui.badge{id = case.id .. '-state', text = 'Not tried yet', tone = 'neutral'}},
                    ui.label{text = case.call, font = 'monospace', color = 'accentText'},
                    ui.label{id = case.id .. '-outcome', text = string.format('Press "%s" to send it.', case.text), color = 'textMuted'},
                }
            end
        end
    end
    self:frame{
        code = Errors.code,
        hint = 'Try each failure. Every card shows the call, what came back and how long it took, and the app keeps running whatever the network does.',
        focus = 'host',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 520, align = 'stretch', ui.scroll{grow = 1, height = 0, ui.column{gap = 10, children = controls}}},
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Results'},
                ui.scroll{id = 'results', grow = 1, height = 0, ui.column{gap = 12, children = cards}},
            },
        }},
    }
    for _, case in ipairs(Errors.cases) do
        for _, id in ipairs(Errors.opening) do
            if case.id == id then
                self:try(case)
            end
        end
    end
end

-- Runs a case in a task of the scene and shows its outcome on its card.
function Errors:try(case)
    local tone
    for _, group in ipairs(Errors.groups) do
        if group.id == case.group then
            tone = group.tone
        end
    end
    self:set(case.id .. '-state', {text = 'Sending', tone = 'information'})
    self:set(case.id .. '-outcome', {text = 'Waiting for the answer.', color = 'textMuted'})
    self:spawn(function()
        local started = readout.millis()
        local response, failure = case.run()
        local state, answer = 'No response', string.format('No response came back, only the reason: %s', tostring(failure))
        if response then
            state, answer = describe(case, response)
        end
        self:set(case.id .. '-state', {text = state, tone = tone})
        self:set(case.id .. '-outcome', {text = string.format('%s It took %d ms.', answer:match('[.!?]$') and answer or answer .. '.', readout.millis() - started), color = 'text'})
    end)
end

return Errors
