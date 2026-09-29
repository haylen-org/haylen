-- Reconnection: a socket opened with the reconnect option connects again on its own after a lost connection or a failed attempt. Each wait is the previous one times the multiplier, from initialDelay up to maxDelay, shortened at random by the jitter, and after maxAttempts failures in a row the socket gives up and reports close. The unreachable server refuses every attempt, so the waits show the whole backoff.
local haylen = require('haylen')
local json = require('json')
local net = require('haylen.net')
local ui = require('haylen.ui')

local sample = require('sample')
local services = require('services')

local Reconnect = haylen.class('Reconnect', sample.Test)

Reconnect.hints = 'Pick a server and start. The unreachable server shows every attempt and its wait until the socket gives up, and the echo server connects and stays up.'
Reconnect.focus = 'target'

local kSettings = {initialDelay = 0.5, maxDelay = 6, multiplier = 2, jitter = 0.25, maxAttempts = 6}
local kTargets = {{id = 'unreachable', text = 'Unreachable server'}, {id = 'echo', text = 'Echo server'}}

function Reconnect:init(entry)
    Reconnect.super.init(self, entry)
    self.target = 'unreachable'
    self.lines = {}
    self.logged = 0
end

function Reconnect:content()
    return {
        ui.panel{width = 640, align = 'stretch', gap = 12,
            ui.segmentedControl{id = 'target', items = kTargets, selected = self.target, onChange = function(event)
                self.target = event.value
            end},
            ui.label{id = 'url', text = '', font = 'monospace', color = 'textMuted'},
            ui.row{gap = 12,
                ui.button{id = 'start', text = 'Start', variant = 'primary', grow = 1, onClick = function()
                    self:start()
                end},
                ui.button{id = 'stop', text = 'Stop', grow = 1, onClick = function()
                    if self.socket then
                        self.socket:close()
                    end
                end},
            },
            ui.statusIndicator{id = 'state', text = 'Not started', tone = 'neutral'},
            ui.label{text = 'net.connectWebSocket(url, {reconnect = ' .. json.encode(kSettings) .. '})', font = 'monospace', color = 'accentText'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Waits before each attempt'},
            ui.column{id = 'attempts', gap = 8},
            ui.sectionTitle{text = 'Events'},
            ui.scroll{grow = 1, focusable = false, ui.list{id = 'log', items = {}}},
        },
    }
end

function Reconnect:update(dt)
    local state = self.socket and string.format('State: %s, attempt %d', self.socket.state, self.socket.attempt) or 'Not started'
    if state ~= self.state then
        self.state = state
        local tone = self.socket and ({open = 'success', closed = 'danger'})[self.socket.state] or 'warning'
        self:show('state', {text = state, tone = self.socket and tone or 'neutral'})
    end
end

function Reconnect:log(text, detail)
    self.logged = self.logged + 1
    table.insert(self.lines, 1, {id = 'line-' .. self.logged, text = text, caption = sample.clock() .. (detail and '  ' .. detail or '')})
    self:show('log', {items = self.lines})
end

-- Replaces the socket with a new one to the chosen server and draws one bar for every wait it schedules.
function Reconnect:start()
    if self.socket then
        self.socket:close()
    end
    local url = services[self.target]
    local socket = self:keep(net.connectWebSocket(url, {reconnect = kSettings}))
    self.socket = socket
    self.bars = {}
    self.document:replaceChildren('attempts', {})
    self:show('url', {text = url})
    self:log('Connecting', url)
    self:on(socket, 'open', function()
        self:log('open', 'connected, so the count of attempts starts over')
    end)
    self:on(socket, 'error', function(message)
        self:log('error', message)
    end)
    self:on(socket, 'disconnect', function(code, reason)
        self:log('disconnect', string.format('code %d %s', code, reason))
    end)
    self:on(socket, 'reconnecting', function(attempt, delay)
        self:log('reconnecting', string.format('attempt %d in %.2f s', attempt, delay))
        self.bars[#self.bars + 1] = ui.row{gap = 12,
            ui.label{text = 'Attempt ' .. attempt, width = 160},
            ui.progress{value = delay / kSettings.maxDelay, text = string.format('%.2f s', delay), grow = 1},
        }
        self.document:replaceChildren('attempts', self.bars)
    end)
    self:on(socket, 'close', function(code, reason)
        self:log('close', string.format('code %d%s', code, reason ~= '' and ', ' .. reason or ''))
    end)
end

return Reconnect
