-- Events: tick events that a native timer sends every tickInterval seconds while it runs, and loaded, which the native part sent retained when it loaded. This test is the first listener of loaded and connects long after the menu opened, and still receives it.
local haylen = require('haylen')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Events = haylen.class('Events', sample.Test)

local kLoadedWait = 3
local kTicksToPass = 3
local kQuietSeconds = 2.5

-- The payload of loaded outlives the scene, because a retained event reaches only the first listener of its name.
local loaded = {}

function Events:enter()
    self.ticking = false
    self.connections = {demo.onTick(function(payload) self:tick(payload) end)}
    self:frame({
        hint = 'Start the ticks, watch them arrive and stop them.',
        focus = 'ticks',
        controls = {
            ui.button{id = 'ticks', text = 'Start the native ticks', variant = 'primary', onClick = function() self:toggleTicks() end},
            ui.label{text = 'Swift runs a Timer on the main run loop, Kotlin posts to the main Handler, JavaScript uses setInterval and C a thread of the library. The native part sent loaded retained when it loaded, before any Lua ran, and the bridge kept it for the first listener.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "demo.onLoaded(function(payload)\n  print(payload.language)\nend)\ndemo.onTick(function(tick) print(tick.count) end)\ndemo.setTicking(true)"},
        },
    })
    self:listenLoaded()
end

function Events:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
    if self.ticking then
        demo.setTicking(false)
    end
end

function Events:listenLoaded()
    if not self.native then
        return
    end
    local name = 'loaded reaches a late listener'
    if loaded.payload then
        self:showLoaded(name)
        return
    end

    self.results:set('loaded', 'waiting', name, string.format('connected %.1f seconds after the app started', haylen.elapsed()))
    local connectedAt = haylen.elapsed()
    self.connections[#self.connections + 1] = demo.onLoaded(function(payload)
        loaded.payload = payload
        loaded.connectedAt = connectedAt
        self:showLoaded(name)
    end)
    self:spawn(function()
        if not sample.waitFor(function() return loaded.payload ~= nil end, kLoadedWait) then
            self.results:set('loaded', 'skip', name, 'loaded has not arrived. The native part sends it once, when it loads with the process, so an app that restarted in the same process, such as from the error screen, does not receive it again.')
        end
    end)
end

function Events:showLoaded(name)
    local payload = loaded.payload
    self.results:set('loaded', 'pass', name, string.format('%s sent loaded on %s when it loaded, and this listener, which connected %.1f seconds after the app started, received it because the event was retained.', payload.language, payload.platform, loaded.connectedAt))
end

function Events:toggleTicks()
    self:act(function()
        local enabled = not self.ticking
        local answer, err = demo.setTicking(enabled):await()
        if err then
            self.results:failure('ticks', 'tick events arrive every tickInterval seconds', err)
            return
        end
        self.ticking = answer.enabled
        self.interval = answer.interval
        self:set('ticks', {text = self.ticking and 'Stop the native ticks' or 'Start the native ticks'})
        if self.ticking then
            self.ticks = {}
            self.results:set('ticks', 'waiting', 'tick events arrive every tickInterval seconds', string.format('the native timer ticks every %s seconds', tostring(answer.interval)))
            self.results:set('stopped', 'info', 'stopped ticks stay quiet', 'stop the ticks to check it')
        else
            self:checkQuiet()
        end
    end)
end

function Events:tick(payload)
    if not self.ticks then
        return
    end
    self.ticks[#self.ticks + 1] = {count = payload.count, at = haylen.elapsed()}
    self.lastTick = haylen.elapsed()
    local first, last = self.ticks[1], self.ticks[#self.ticks]
    local average = #self.ticks > 1 and (last.at - first.at) / (#self.ticks - 1) or 0
    local state = self.ticking and #self.ticks >= kTicksToPass and 'pass' or 'waiting'
    self.results:set('ticks', state, 'tick events arrive every tickInterval seconds', string.format('%d ticks from %s on the %s thread, the last with the count %d, %.2f seconds apart on average for an interval of %s seconds.', #self.ticks, payload.language, payload.thread, payload.count, average, tostring(self.interval)))
end

-- Ticks already on their way may still arrive right after the stop, so the quiet time starts at the stop.
function Events:checkQuiet()
    local stoppedAt = haylen.elapsed()
    self.results:set('stopped', 'waiting', 'stopped ticks stay quiet', string.format('watching for %.1f seconds', kQuietSeconds))
    sample.waitFor(function() return haylen.elapsed() - stoppedAt >= kQuietSeconds end, kQuietSeconds + 1)
    local late = self.lastTick and self.lastTick > stoppedAt + 0.1
    self.results:set('stopped', late and 'fail' or 'pass', 'stopped ticks stay quiet', late and 'A tick arrived after the timer stopped.' or string.format('No tick arrived in the %.1f seconds after the timer stopped.', kQuietSeconds))
end

return Events
