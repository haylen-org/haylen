-- Lifecycle log: a listener on every engine event writes what happens, and the panel triggers the events an app can cause itself: scene loads and transitions, a failed load, fullscreen, a simulated notch, the pause, an asset, a document, sprites and a socket that retries.
local assets = require('haylen.assets')
local async = require('async')
local debugging = require('haylen.debug')
local events = require('haylen.events')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local net = require('haylen.net')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Journal = require('journal')
local sample = require('sample')

local Lifecycle = haylen.class('Lifecycle', sample.Test)
Lifecycle.processMode = 'always'

local kEvents = {
    'appActive', 'appInactive', 'appBackground', 'appLowMemory', 'appQuitRequested', 'paused', 'unpaused',
    'sceneLoading', 'sceneLoaded', 'sceneLoadFailed', 'sceneEntered', 'sceneExited', 'sceneUnloaded', 'scenePaused', 'sceneResumed',
    'sceneExitTransitionStarted', 'sceneEnterTransitionFinished', 'sceneCoverStarted', 'sceneCoverFinished',
    'sceneHoldStarted', 'sceneHoldFinished', 'sceneRevealStarted', 'sceneRevealFinished', 'autoloadStarted', 'autoloadStopped',
    'windowResized', 'windowFocusGained', 'windowFocusLost', 'windowFullscreenChanged', 'windowOrientationChanged', 'windowSafeAreaChanged',
    'uiDocumentMounted', 'uiDocumentUnmounted', 'gamepadConnected', 'gamepadDisconnected', 'audioInterrupted', 'audioResumed', 'audioRouteChanged',
    'keyboardShown', 'keyboardHidden', 'networkOnline', 'networkOffline', 'systemThemeChanged', 'batteryChanged', 'webSocketConnected', 'webSocketDisconnected', 'webSocketReconnecting',
    'assetLoaded', 'assetUnloaded', 'assetReloaded', 'objectCreated', 'objectDestroyed',
}
local kColors = {app = sample.warm, scene = sample.accent, window = sample.green, asset = '#FFC9A0FF', webSocket = sample.red}
local kCode = [[
events.on('sceneHoldStarted', function(transfer) print(transfer.from, transfer.to) end, {owner = self})
scene.push(room, {duration = 0.8, loading = view, loadingDelay = 0.1, minimumLoadingTime = 0.6})
function room:load(context) context:progress(0.5, 'Building the room') async.sleep(100):await() end]]

-- Names a scene or a document by its type, or says that C++ pushed the scene.
local function sceneName(value)
    if value == false then
        return 'a C++ scene'
    end
    return (tostring(value):match('^[^:]+'))
end

-- Writes the value of an engine event in a few words: a scene, the scenes of a transition phase or the fields of a table.
local function describe(value)
    if value == nil then
        return ''
    end
    if type(value) == 'userdata' then
        return sceneName(value)
    end
    if value.from ~= nil or value.to ~= nil then
        return 'from ' .. sceneName(value.from) .. ' to ' .. sceneName(value.to)
    end
    if value.scene ~= nil then
        return sceneName(value.scene) .. ': ' .. tostring(value.error)
    end
    if getmetatable(value) then
        return sceneName(value)
    end
    local fields = {}
    for key, field in pairs(value) do
        fields[#fields + 1] = key .. ' ' .. (type(field) == 'number' and string.format('%g', field) or tostring(field))
    end
    table.sort(fields)
    return table.concat(fields, ', ')
end

-- A scene that takes a while to load, reporting its progress to the loading view.
local Room = haylen.class('Room', sample.Overlay)

function Room:load(context)
    for step = 1, 6 do
        context:progress(step / 6, 'Building the room, part ' .. step .. ' of 6')
        async.sleep(120):await()
    end
end

function Room:enter()
    self:card('A room that loaded', {ui.label{text = 'It loaded behind the fade with a loading view. Close it to see it exit and unload.', color = 'textMuted'}})
end

-- A scene whose load fails, which ends the change without it.
local Broken = haylen.class('Broken', sample.Overlay)

function Broken:load()
    async.sleep(200):await()
    error('The server did not answer.')
end

-- The loading view draws the progress and the message of the load over the covered screen.
local loadingView = {
    render = function(self, progress, message)
        graphics2d.beginScreen()
        local area = graphics2d.canvasBounds()
        local x, y, width = area.x + area.width / 2 - 400, area.y + area.height / 2, 800
        graphics2d.drawRect({x, y, width, 24}, '#FF3A3F55')
        graphics2d.drawRect({x, y, width * progress, 24}, '#FFF2C14E', {layer = 1})
        graphics2d.drawText(nil, message or '', area.x + area.width / 2, y - 40, {size = 36, anchor = {0.5, 0.5}})
    end,
}

function Lifecycle:enter()
    local journal = Journal(80)
    self.journal = journal
    for _, name in ipairs(kEvents) do
        local color = kColors[name:match('^webSocket') or name:match('^%l+')] or sample.ink
        events.on(name, function(value)
            if name:match('^object') and value.type ~= 'haylen.Sprite' then
                return
            end
            journal:add(name .. '  ' .. describe(value), color)
        end, {owner = self})
    end

    self:frame({
        hint = 'Switch windows, resize, plug in a gamepad or turn the device to see the platform events too.',
        code = kCode,
        controls = {
            ui.button{id = 'room', text = 'Push a scene that loads', variant = 'primary', onClick = function() self:pushRoom() end},
            ui.button{id = 'broken', text = 'Push a failing load', onClick = function() self:pushBroken() end},
            ui.toggle{id = 'pause', text = 'Pause the game', onChange = function(event) haylen.setPaused(event.checked) end},
            ui.toggle{id = 'notch', text = 'Simulate a notch', onChange = function(event) viewport.setSafeAreaSimulation(event.checked and 'iphoneDynamicIsland' or nil) end},
            ui.toggle{id = 'objects', text = 'Object events', onChange = function(event) debugging.setObjectEvents(event.checked) end},
            ui.button{id = 'sprite', text = 'Create and drop a sprite', onClick = function() self:touchSprite() end},
            ui.button{id = 'asset', text = 'Load and release an asset', onClick = function() self:touchAsset() end},
            ui.button{id = 'document', text = 'Mount a document', onClick = function() self:touchDocument() end},
            ui.button{id = 'socket', text = 'Open a socket that retries', onClick = function() self:openSocket() end},
            ui.button{id = 'fullscreen', text = 'Toggle fullscreen', onClick = function() window.setFullscreen(not window.fullscreen()) end},
            ui.button{id = 'clear', text = 'Clear the log', onClick = function() journal:clear() end},
        },
        focus = 'room',
    })
end

function Lifecycle:exit()
    haylen.setPaused(false)
    viewport.setSafeAreaSimulation(nil)
    debugging.setObjectEvents(false)
    if self.socket then
        self.socket:close()
    end
end

function Lifecycle:pushRoom()
    if not scene.transitioning() then
        scene.push(Room(), {duration = 0.8, loading = loadingView, loadingDelay = 0.1, minimumLoadingTime = 0.6})
    end
end

function Lifecycle:pushBroken()
    if not scene.transitioning() then
        scene.push(Broken(), {duration = 0.6, onError = function(message) self.journal:add('onError: ' .. message, sample.red) end})
    end
end

function Lifecycle:touchSprite()
    local sprite = graphics2d.newSprite(graphics.whiteTexture())
    self.journal:add('Made ' .. tostring(sprite):match('^[^:]+') .. ', dropping it now', sample.muted)
    sprite = nil
    collectgarbage()
end

-- Holds the badge for half a second, then lets it go so the cache unloads it.
function Lifecycle:touchAsset()
    self.badge = assets.texture('images/badge.png')
    timer.after(0.5, function()
        self.badge = nil
        collectgarbage()
    end, {owner = self})
end

function Lifecycle:touchDocument()
    local toast = ui.mount(ui.toast{id = 'toast', text = 'A document that unmounts itself', open = true}, {owner = self, layer = 2})
    timer.after(1.2, function() toast:unmount() end, {owner = self})
end

-- Nothing listens on the discard port, so the socket fails, retries twice and closes.
function Lifecycle:openSocket()
    if self.socket then
        self.socket:close()
    end
    self.socket = net.connectWebSocket('ws://127.0.0.1:9/', {reconnect = {initialDelay = 0.5, maxDelay = 1, maxAttempts = 2}})
    self.socket:on('close', function(code) self.journal:add('The socket gave up with code ' .. code, sample.red) end)
end

function Lifecycle:update(dt)
    Lifecycle.super.update(self, dt)
    self:status(string.format('app %s   paused %s   halted %s   scenes %d   sockets %d', haylen.appState(), haylen.paused(), haylen.halted(), scene.size(), net.openSocketCount()))
end

function Lifecycle:draw(area)
    self.journal:draw(24, 16, area.height - 32, 24)
end

return Lifecycle
