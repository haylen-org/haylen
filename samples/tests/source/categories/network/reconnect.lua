-- A socket opened with the `reconnect` option connects again on its own after a lost connection or a failed attempt. Each wait is the previous one times the multiplier, from `initialDelay` up to `maxDelay`, shortened at random by the jitter, and after `maxAttempts` failures in a row the socket gives up and reports `close`. The unreachable server refuses every attempt, so the waits show the whole backoff.
local haylen = require('haylen')
local net = require('haylen.net')
local ui = require('haylen.ui')

local SocketTest = require('categories.network.socket-test')
local readout = require('categories.network.readout')
local services = require('categories.network.services')

local Reconnect = haylen.class('Reconnect', SocketTest)

Reconnect.code = [[
local socket = net.connectWebSocket(url, {reconnect = {initialDelay = 0.5, maxDelay = 6, multiplier = 2, jitter = 0.25, maxAttempts = 6}})
socket:on('reconnecting', function(attempt, delay) print(attempt, delay) end)
socket:on('close', function(code, reason) print('Gave up with ' .. code) end)]]

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
        code = Reconnect.code,
        hint = 'The test starts with the unreachable server, which shows every attempt and its wait until the socket gives up. Pick the echo server and start again to connect and stay up.',
        focus = 'target',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 640, align = 'stretch', gap = 12,
                ui.segmentedControl{id = 'target', items = Reconnect.targets, selected = self.target, onChange = function(event)
                    self.target = event.value
                    self:set('url', {text = services[self.target]})
                end},
                ui.label{id = 'url', text = services[self.target], font = 'monospace', color = 'textMuted'},
                ui.row{wrap = true, gap = 12,
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
                ui.sectionTitle{text = 'Events', margin = {12, 0, 0, 0}},
                ui.label{id = 'eventsEmpty', text = 'Start to see every attempt, wait and close arrive.', color = 'textMuted'},
                ui.scroll{grow = 1, height = 0, focusable = false, ui.list{id = 'events', items = {}}},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Waits before each attempt'},
                ui.label{text = string.format('Each wait doubles from %.1f seconds up to %d seconds, shortened at random by up to a quarter, and the socket gives up after %d failures in a row.', Reconnect.settings.initialDelay, Reconnect.settings.maxDelay, Reconnect.settings.maxAttempts), color = 'textMuted'},
                ui.column{gap = 10, children = self:plannedWaits()},
            },
        }},
    }
    self:start()
end

-- The bar of the wait the settings plan before an attempt, in grey until that attempt comes.
function Reconnect.plannedWait(attempt)
    local settings = Reconnect.settings
    local planned = math.min(settings.maxDelay, settings.initialDelay * settings.multiplier ^ (attempt - 1))
    return {value = planned / settings.maxDelay, tone = 'neutral', text = string.format('Up to %.1f s', planned), style = {colors = {accent = 'borderStrong'}}}
end

-- One bar for every wait the settings plan, which lights up with the wait the socket drew once that attempt comes.
function Reconnect:plannedWaits()
    local bars = {}
    for attempt = 1, Reconnect.settings.maxAttempts do
        local bar = Reconnect.plannedWait(attempt)
        bar.id, bar.grow = 'wait-' .. attempt, 1
        bars[attempt] = ui.row{gap = 12, ui.label{text = 'Attempt ' .. attempt, width = 160, color = 'textMuted'}, ui.progress(bar)}
    end
    return bars
end

Reconnect.states = {connecting = 'Connecting', open = 'Connected', reconnecting = 'Waiting to reconnect', closing = 'Closing', closed = 'Closed'}

function Reconnect:update(dt)
    Reconnect.super.update(self, dt)
    local state = self.socket and string.format('%s, attempt %d', Reconnect.states[self.socket.state] or self.socket.state, self.socket.attempt) or 'Not started'
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
    self:set('eventsEmpty', {visible = false})
end

-- Puts every bar back to the wait the settings plan.
function Reconnect:resetWaits()
    for attempt = 1, Reconnect.settings.maxAttempts do
        self:set('wait-' .. attempt, Reconnect.plannedWait(attempt))
    end
end

-- Replaces the socket with a new one to the chosen server and lights the bar of every wait it schedules.
function Reconnect:start()
    if self.socket then
        self.socket:close()
    end
    local url = services[self.target]
    local socket = self:keep(net.connectWebSocket(url, {reconnect = Reconnect.settings}))
    self.socket = socket
    self:resetWaits()
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
        if attempt <= Reconnect.settings.maxAttempts then
            self:set('wait-' .. attempt, {value = delay / Reconnect.settings.maxDelay, tone = 'warning', text = string.format('Waited %.2f s', delay), style = {}})
        end
    end)
    self:on(socket, 'close', function(code, reason)
        self:record('Event "close"', string.format('Code %d%s', code, reason ~= '' and ', ' .. reason or ''))
    end)
end

return Reconnect
