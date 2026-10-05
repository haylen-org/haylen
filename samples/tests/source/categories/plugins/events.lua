-- The events `tick` of a native timer that runs every `tickInterval` seconds, and `loaded`, which the native part sent retained when it loaded. This test is the first listener of `loaded` and connects long after the app started, and still receives it.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Events = haylen.class('Events', DemoTest)

Events.loadedWait = 3
Events.ticksToPass = 3
Events.quietSeconds = 2.5
Events.ticksName = 'The events "tick" arrive every "tickInterval" seconds'

-- The payload of `loaded` outlives the scene, because a retained event reaches only the first listener of its name.
Events.loaded = {}

function Events:enter()
    self.ticking = false
    self.connections = {demo.onTick(function(payload) self:tick(payload) end)}
    self:frame{
        hint = 'Start the ticks, watch them arrive and stop them.',
        focus = 'ticks',
        controls = {
            ui.button{id = 'ticks', text = 'Start the native ticks', variant = 'primary', onClick = function() self:toggleTicks() end},
            ui.label{text = 'Swift runs a "Timer" on the main run loop, Kotlin posts to the main "Handler", JavaScript uses "setInterval" and C a thread of the library. The native part sent "loaded" retained when it loaded, before any Lua ran, and the bridge kept it for the first listener.', color = 'textMuted', font = 'caption'},
        },
        code = "demo.onLoaded(function(payload)\n  print(payload.language)\nend)\ndemo.onTick(function(tick) print(tick.count) end)\ndemo.setTicking(true)",
    }
    self:listenLoaded()
end

function Events:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
    if self.ticking then
        demo.setTicking(false)
    end
    Events.super.exit(self)
end

function Events:listenLoaded()
    if not self.native then
        return
    end
    local name = 'The event "loaded" reaches a late listener'
    if Events.loaded.payload then
        self:showLoaded(name)
        return
    end

    self.results:set('loaded', 'waiting', name, string.format('Connected %.1f seconds after the app started.', haylen.elapsed()))
    local connectedAt = haylen.elapsed()
    self.connections[#self.connections + 1] = demo.onLoaded(function(payload)
        Events.loaded.payload = payload
        Events.loaded.connectedAt = connectedAt
        self:showLoaded(name)
    end)
    self:spawn(function()
        if not DemoTest.waitFor(function() return Events.loaded.payload ~= nil end, Events.loadedWait) then
            self.results:set('loaded', 'skip', name, 'The event "loaded" has not arrived. The native part sends it once, when it loads with the process, so an app that restarted in the same process, such as from the error screen, does not receive it again.')
        end
    end)
end

function Events:showLoaded(name)
    local loaded = Events.loaded
    self.results:set('loaded', 'pass', name, string.format('%s sent "loaded" on "%s" when it loaded, and this listener, which connected %.1f seconds after the app started, received it because the event was retained.', loaded.payload.language, loaded.payload.platform, loaded.connectedAt))
end

function Events:toggleTicks()
    self:act(function()
        local enabled = not self.ticking
        local answer, err = demo.setTicking(enabled):await()
        if err then
            self.results:failure('ticks', Events.ticksName, err)
            return
        end
        self.ticking = answer.enabled
        self.interval = answer.interval
        self:set('ticks', {text = self.ticking and 'Stop the native ticks' or 'Start the native ticks'})
        if self.ticking then
            self.ticks = {}
            self.results:set('ticks', 'waiting', Events.ticksName, string.format('The native timer ticks every %s seconds.', tostring(answer.interval)))
            self.results:set('stopped', 'info', 'Stopped ticks stay quiet', 'Stop the ticks to check it.')
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
    local state = self.ticking and #self.ticks >= Events.ticksToPass and 'pass' or 'waiting'
    self.results:set('ticks', state, Events.ticksName, string.format('%d ticks from %s on the thread "%s", the last with the count %d, %.2f seconds apart on average for an interval of %s seconds.', #self.ticks, payload.language, payload.thread, payload.count, average, tostring(self.interval)))
end

-- Ticks already on their way may still arrive right after the stop, so the quiet time starts at the stop.
function Events:checkQuiet()
    local stoppedAt = haylen.elapsed()
    self.results:set('stopped', 'waiting', 'Stopped ticks stay quiet', string.format('Watching for %.1f seconds.', Events.quietSeconds))
    DemoTest.waitFor(function() return haylen.elapsed() - stoppedAt >= Events.quietSeconds end, Events.quietSeconds + 1)
    local late = self.lastTick and self.lastTick > stoppedAt + 0.1
    self.results:set('stopped', late and 'fail' or 'pass', 'Stopped ticks stay quiet', late and 'A tick arrived after the timer stopped.' or string.format('No tick arrived in the %.1f seconds after the timer stopped.', Events.quietSeconds))
end

return Events
