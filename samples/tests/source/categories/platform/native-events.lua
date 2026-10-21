-- The events the native code of the plugin `platform-sample` sends through the bridge: the test checks that a ticker that native code runs on request sends its ticks from the language of the platform, and that an event sent from Lua the way native code does arrives, next to the events the platform itself reports to the scene.
local async = require('async')
local haylen = require('haylen')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local SampleTest = require('categories.platform.sample-test')
local platformSample = require('platform-sample')

local NativeEvents = haylen.class('NativeEvents', SampleTest)

NativeEvents.ticks = 3
NativeEvents.interval = 100
NativeEvents.wait = 3
NativeEvents.kept = 30

NativeEvents.platformEvents = {
    focusGained = true, focusLost = true, suspended = true, resumed = true, lowMemory = true, quitRequested = true,
    networkChanged = true, interruptionBegan = true, interruptionEnded = true, keyboardChanged = true, resized = true,
}

function NativeEvents:enter()
    self.system = {}
    self.logged = 0
    self.ticks = {}
    self.emitted = 0
    -- Bridge listeners stay connected until they are disconnected, so the scene keeps them and disconnects them on exit.
    self.connections = {
        platformSample.onTick(function(payload) self.ticks[#self.ticks + 1] = payload end),
        platformSample.onActivity(function(payload) self.results:set('activity', 'pass', 'Native code reports the activity of the app', string.format('%s sent "platform-sample.activity" with the state "%s".', tostring(payload.source), tostring(payload.state))) end),
    }
    self:frame{
        hint = 'Run the checks again, or switch to another app or tab and come back to see "platform-sample.activity".',
        focus = 'run',
        checksTitle = 'Events of the native code',
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'The method "platform-sample.ticker" asks native code for "platform-sample.tick" events a short time apart. Native code also sends "platform-sample.activity" by itself when the app or the tab shows or hides.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Events of the platform'},
                ui.column{id = 'systemEmpty', grow = 1, gap = 16, justify = 'center', align = 'stretch',
                    ui.icon{image = 'plugins/bell.svg', size = 96, color = 'textMuted', align = 'center'},
                    ui.label{text = 'No event yet. Switch to another app or tab and come back, or resize the window, to see one arrive at the event hook of the scene.', color = 'textMuted', textAlign = 'center', align = 'center', maxWidth = 480},
                },
                ui.scroll{id = 'systemScroll', grow = 1, height = 0, focusable = false, visible = false, ui.list{id = 'systemEvents', items = {}}},
            },
        },
        code = "local connection = platform.on('platform-sample.tick', function(payload)\n  print(payload.count)\nend)\nplatform.emit('platform-sample.tick', {count = 1})\nconnection:disconnect()",
    }
    self:run()
end

function NativeEvents:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
    NativeEvents.super.exit(self)
end

function NativeEvents:run()
    self.results:clear()
    self.ticks = {}
    local native = self:checkNative()
    self:spawn(function()
        self:checkEmit()
        if native then
            self:checkTicker()
            self.results:set('activity', 'info', 'Native code reports the activity of the app', 'Switch to another app or tab and come back.')
        end
    end)
end

-- An event that Lua sends the way native code does reaches the listeners of the plugin on every platform.
function NativeEvents:checkEmit()
    local name = 'An event sent from Lua arrives'
    self.emitted = self.emitted + 1
    local count = self.emitted
    platform.emit('platform-sample.tick', {count = count, source = 'Lua'})
    local arrived = NativeEvents.waitFor(function() return self:findTick('Lua', count) end, 1)
    self.results:set('emit', arrived and 'pass' or 'fail', name, arrived and 'The listener heard "platform-sample.tick" from Lua.' or 'The listener heard nothing within a second.')
end

function NativeEvents:checkTicker()
    local name = 'The native ticker sends its ticks'
    self.results:set('ticker', 'waiting', name, 'Waiting for the ticks.')
    local result, err = platformSample.ticker(NativeEvents.ticks, NativeEvents.interval):await()
    if err then
        self.results:set('ticker', 'fail', name, string.format('%s (code "%s")', err.message, tostring(err.code)))
        return
    end
    local language = SampleTest.languages[haylen.platform]
    local heard = NativeEvents.waitFor(function() return self:findTick(language, NativeEvents.ticks) end, NativeEvents.wait)
    local count = 0
    for _, tick in ipairs(self.ticks) do
        count = count + (tick.source == language and 1 or 0)
    end
    local passed = heard and count == NativeEvents.ticks and result.count == NativeEvents.ticks
    self.results:set('ticker', passed and 'pass' or 'fail', name, string.format('The ticker started for %s ticks %s ms apart, and %d of %d ticks came from %s.', tostring(result.count), tostring(result.interval), count, NativeEvents.ticks, language))
end

function NativeEvents:findTick(source, count)
    for _, tick in ipairs(self.ticks) do
        if tick.source == source and tick.count == count then
            return true
        end
    end
    return false
end

-- Waits inside a task until `condition` holds, checking every frame for at most `seconds` of app time, and returns whether it held.
function NativeEvents.waitFor(condition, seconds)
    local deadline = haylen.elapsed() + seconds
    while not condition() and haylen.elapsed() < deadline do
        async.sleep(10):await()
    end
    return condition()
end

-- The platform reports these events to the top scene as soon as they arrive, and the list shows the newest first.
function NativeEvents:event(event)
    if not NativeEvents.platformEvents[event.type] then
        return
    end
    self.logged = self.logged + 1
    local detail = event.type == 'networkChanged' and (event.online and 'The network is online.' or 'The network is offline.') or 'At frame ' .. haylen.frameIndex() .. '.'
    table.insert(self.system, 1, {id = 'event-' .. self.logged, text = 'Event "' .. event.type .. '"', caption = os.date('%H:%M:%S') .. '  ' .. detail})
    self.system[NativeEvents.kept + 1] = nil
    self:set('systemEvents', {items = self.system})
    self:set('systemEmpty', {visible = false})
    self:set('systemScroll', {visible = true})
end

return NativeEvents
