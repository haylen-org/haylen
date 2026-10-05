-- A socket opened with the `reconnect` option connects again on its own after a lost connection or a failed attempt. Each wait is the previous one times the multiplier, from `initialDelay` up to `maxDelay`, shortened at random by the jitter, and after `maxAttempts` failures in a row the socket gives up and reports `close`. The unreachable server refuses every attempt, so the waits show the whole backoff.
local haylen = require('haylen')
local json = require('json')
local net = require('haylen.net')
local ui = require('haylen.ui')

local SocketTest = require('categories.network.socket-test')
local readout = require('categories.network.readout')
local services = require('categories.network.services')

local Reconnect = haylen.class('Reconnect', SocketTest)

Reconnect.settings = {initialDelay = 0.5, maxDelay = 6, multiplier = 2, jitter = 0.25, maxAttempts = 6}
Reconnect.targets = {{id = 'unreachable', text = 'Unreachable server'}, {id = 'echo', text = 'Echo server'}}

function Reconnect:init(entry)
    Reconnect.super.init(self, entry)
    self.target = 'unreachable'
    self.lines = {}
    self.logged = 0
end

function Reconnect:enter()
    self:frame{
        hint = 'Pick a server and start. The unreachable server shows every attempt and its wait until the socket gives up, and the echo server connects and stays up.',
        focus = 'target',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 640, align = 'stretch', gap = 12,
                ui.segmentedControl{id = 'target', items = Reconnect.targets, selected = self.target, onChange = function(event)
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
                ui.label{text = 'net.connectWebSocket(url, {reconnect = ' .. json.encode(Reconnect.settings) .. '})', font = 'monospace', color = 'accentText'},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Waits before each attempt'},
                ui.column{id = 'attempts', gap = 8},
                ui.sectionTitle{text = 'Events'},
                ui.scroll{grow = 1, focusable = false, ui.list{id = 'events', items = {}}},
            },
        }},
    }
end

function Reconnect:update(dt)
    Reconnect.super.update(self, dt)
    local state = self.socket and string.format('State "%s", attempt %d', self.socket.state, self.socket.attempt) or 'Not started'
    if state ~= self.state then
        self.state = state
        local tone = self.socket and ({open = 'success', closed = 'danger'})[self.socket.state] or 'warning'
        self:set('state', {text = state, tone = self.socket and tone or 'neutral'})
    end
end

function Reconnect:record(text, detail)
    self.logged = self.logged + 1
    table.insert(self.lines, 1, {id = 'line-' .. self.logged, text = text, caption = readout.clock() .. (detail and '  ' .. detail or '')})
    self:set('events', {items = self.lines})
end

-- Replaces the socket with a new one to the chosen server and draws one bar for every wait it schedules.
function Reconnect:start()
    if self.socket then
        self.socket:close()
    end
    local url = services[self.target]
    local socket = self:keep(net.connectWebSocket(url, {reconnect = Reconnect.settings}))
    self.socket = socket
    self.bars = {}
    self.gui:replaceChildren('attempts', {})
    self:set('url', {text = url})
    self:record('Connecting', url)
    self:on(socket, 'open', function()
        self:record('Event "open"', 'Connected, so the count of attempts starts over')
    end)
    self:on(socket, 'error', function(message)
        self:record('Event "error"', message)
    end)
    self:on(socket, 'disconnect', function(code, reason)
        self:record('Event "disconnect"', string.format('Code %d %s', code, reason))
    end)
    self:on(socket, 'reconnecting', function(attempt, delay)
        self:record('Event "reconnecting"', string.format('Attempt %d in %.2f s', attempt, delay))
        self.bars[#self.bars + 1] = ui.row{gap = 12,
            ui.label{text = 'Attempt ' .. attempt, width = 160},
            ui.progress{value = delay / Reconnect.settings.maxDelay, text = string.format('%.2f s', delay), grow = 1},
        }
        self.gui:replaceChildren('attempts', self.bars)
    end)
    self:on(socket, 'close', function(code, reason)
        self:record('Event "close"', string.format('Code %d%s', code, reason ~= '' and ', ' .. reason or ''))
    end)
end

return Reconnect
