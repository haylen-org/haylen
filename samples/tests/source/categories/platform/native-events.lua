-- The events the native code of the plugin `platform-sample` sends through the bridge, a ticker that native code runs on request and an event sent from Lua the way native code does, next to the events the platform itself reports to the scene.
local haylen = require('haylen')
local json = require('json')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local Journal = require('harness.journal')
local Test = require('harness.test')
local platformSample = require('platform-sample')

local NativeEvents = haylen.class('NativeEvents', Test)

NativeEvents.platformEvents = {
    focusGained = true, focusLost = true, suspended = true, resumed = true, lowMemory = true, quitRequested = true,
    networkChanged = true, interruptionBegan = true, interruptionEnded = true, keyboardChanged = true, resized = true,
}

function NativeEvents:enter()
    self.bridge = Journal(30)
    self.system = Journal(30)
    self.emitted = 0
    -- Bridge listeners stay connected until they are disconnected, so the scene keeps them and disconnects them on exit.
    self.connections = {
        platformSample.onTick(function(payload) self.bridge:add('Event "platform-sample.tick": ' .. json.encode(payload), Test.green) end),
        platformSample.onActivity(function(payload) self.bridge:add('Event "platform-sample.activity": ' .. json.encode(payload), Test.accent) end),
    }
    self:frame{
        hint = 'Start the ticker, switch to another app or tab and come back.',
        focus = 'ticker',
        controls = {
            ui.button{id = 'ticker', text = 'Start the native ticker', variant = 'primary', onClick = function() self:startTicker() end},
            ui.button{id = 'emit', text = 'Send "platform-sample.tick" from Lua', onClick = function()
                self.emitted = self.emitted + 1
                platform.emit('platform-sample.tick', {count = self.emitted, source = 'Lua'})
            end},
            ui.label{id = 'answer', text = 'The ticker has not started.', color = 'textMuted'},
            ui.label{text = 'The method "platform-sample.ticker" asks native code for five "platform-sample.tick" events half a second apart. Native code also sends "platform-sample.activity" by itself: Android when the activity resumes or pauses, Apple platforms when the app becomes active or resigns, and the web page when the tab shows or hides. The desktop player runs no native code of the plugin, so only Lua sends events there.', color = 'textMuted', font = 'caption'},
        },
        code = "local connection = platform.on('platform-sample.tick', function(payload)\n  print(payload.count)\nend)\nplatform.emit('platform-sample.tick', {count = 1})\nconnection:disconnect()",
    }
end

function NativeEvents:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
    NativeEvents.super.exit(self)
end

function NativeEvents:startTicker()
    self:set('answer', {text = 'Waiting for "platform-sample.ticker".'})
    self:spawn(function()
        local result, err = platformSample.ticker(5, 500):await()
        self:set('answer', {text = result and 'The method "platform-sample.ticker" answered ' .. json.encode(result) .. '.' or 'The method "platform-sample.ticker" failed: ' .. err.message})
    end)
end

-- The platform reports these events to the top scene as soon as they arrive.
function NativeEvents:event(event)
    if NativeEvents.platformEvents[event.type] then
        local detail = event.type == 'networkChanged' and ', online "' .. tostring(event.online) .. '"' or ''
        self.system:add('Event "' .. event.type .. '"' .. detail, Test.warm)
    end
end

function NativeEvents:update(dt)
    NativeEvents.super.update(self, dt)
    local connected = 0
    for _, connection in ipairs(self.connections) do
        connected = connected + (connection.connected and 1 or 0)
    end
    self:status(string.format('Platform "%s", %d listeners connected, %d pending calls, app state "%s"', haylen.platform, connected, platform.pendingCallCount(), haylen.appState()))
end

function NativeEvents:draw(area)
    local half = area.width / 2
    Test.caption('Bridge events of "platform.on"', 24, 20, {size = 24, color = Test.ink})
    self.bridge:draw(24, 64, area.height - 88, 19)
    Test.caption('Platform events, the event hook of the scene', half + 12, 20, {size = 24, color = Test.ink})
    self.system:draw(half + 12, 64, area.height - 88, 19)
end

return NativeEvents
