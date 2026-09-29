-- Native events: the events the native code of this app sends through the bridge, a ticker that native code runs on request and an event sent from Lua the way native code does, next to the events the platform itself reports to the scene.
local haylen = require('haylen')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local Journal = require('journal')
local sample = require('sample')

local NativeEvents = haylen.class('NativeEvents', sample.Test)

local kPlatformEvents = {
    focus_gained = true, focus_lost = true, suspended = true, resumed = true, low_memory = true, quit_requested = true,
    network_changed = true, interruption_began = true, interruption_ended = true, keyboard_changed = true, resized = true,
}

function NativeEvents:enter()
    self.bridge = Journal(30)
    self.system = Journal(30)
    self.emitted = 0
    -- Bridge listeners stay connected until they are disconnected, so the scene keeps them and disconnects them on exit.
    self.connections = {
        platform.on('sample.tick', function(payload) self.bridge:add('sample.tick ' .. sample.json(payload), sample.green) end),
        platform.on('sample.activity', function(payload) self.bridge:add('sample.activity ' .. sample.json(payload), sample.accent) end),
    }
    self:frame({
        hint = 'Start the ticker, switch to another app or tab and come back.',
        focus = 'ticker',
        controls = {
            ui.button{id = 'ticker', text = 'Start the native ticker', variant = 'primary', onClick = function() self:startTicker() end},
            ui.button{id = 'emit', text = 'Send sample.tick from Lua', onClick = function()
                self.emitted = self.emitted + 1
                platform.emit('sample.tick', {count = self.emitted, source = 'Lua'})
            end},
            ui.label{id = 'answer', text = 'The ticker has not started.', color = 'textMuted'},
            ui.label{text = 'sample.ticker asks native code for five sample.tick events half a second apart. Native code also sends sample.activity by itself: Android when the activity resumes or pauses, Apple platforms when the app becomes active or resigns, and the web page when the tab shows or hides. The desktop player runs no code of this app, so only Lua sends events there.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "local connection = platform.on('sample.tick', function(payload)\n  print(payload.count)\nend)\nplatform.emit('sample.tick', {count = 1})\nconnection:disconnect()"},
        },
    })
end

function NativeEvents:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
end

function NativeEvents:startTicker()
    self:set('answer', {text = 'Waiting for sample.ticker.'})
    self:spawn(function()
        local result, err = platform.call('sample.ticker', {count = 5, interval = 500}):await()
        self:set('answer', {text = result and 'sample.ticker answered ' .. sample.json(result) or 'sample.ticker failed: ' .. err})
    end)
end

-- The platform reports these events to the top scene as soon as they arrive.
function NativeEvents:event(event)
    if kPlatformEvents[event.type] then
        local detail = event.type == 'network_changed' and ' online ' .. tostring(event.online) or ''
        self.system:add(event.type .. detail, sample.warm)
    end
end

function NativeEvents:update(dt)
    NativeEvents.super.update(self, dt)
    local connected = 0
    for _, connection in ipairs(self.connections) do
        connected = connected + (connection.connected and 1 or 0)
    end
    self:status(string.format('platform %s   listeners connected %d   pending calls %d   app state %s', haylen.platform, connected, platform.pendingCalls(), haylen.appState()))
end

function NativeEvents:draw(area)
    local half = area.width / 2
    sample.caption('Bridge events, platform.on', 24, 20, {size = 24, color = sample.ink})
    self.bridge:draw(24, 64, area.height - 88, 19)
    sample.caption('Platform events, the event hook of the scene', half + 12, 20, {size = 24, color = sample.ink})
    self.system:draw(half + 12, 64, area.height - 88, 19)
end

return NativeEvents
