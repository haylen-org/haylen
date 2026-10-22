-- Lifecycle log: a listener on every engine event writes what happens, and the panel triggers the events an app can cause itself: scene loads and transitions, a failed load, fullscreen, a simulated notch, the pause, an asset, a GUI, sprites and a socket that retries.
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

local EventsTest = require('categories.events.events-test')
local Journal = require('harness.journal')
local Overlay = require('categories.events.overlay')
local Test = require('harness.test')

local Lifecycle = haylen.class('Lifecycle', EventsTest)
Lifecycle.processMode = 'always'

-- The groups of the log, each with its color in the legend. The test writes its own lines in the color of the last group.
local kGroups = {
    {id = 'app', title = 'App', color = Test.warm},
    {id = 'scene', title = 'Scenes', color = Test.accent},
    {id = 'window', title = 'Window', color = Test.green},
    {id = 'gui', title = 'Interface', color = '#FF5ED3E8'},
    {id = 'device', title = 'Devices', color = '#FFFF9ED8'},
    {id = 'asset', title = 'Assets', color = Test.violet},
    {id = 'network', title = 'Network', color = Test.red},
    {id = 'test', title = 'The test', color = Test.ink},
}
-- Every engine event the log listens to, with its group and what it means, where a word such as "$scene" takes that field of the table of the event.
local kEvents = {
    {'appActive', 'app', 'the app is active again'},
    {'appInactive', 'app', 'the app lost the focus but still shows'},
    {'appBackground', 'app', 'the app went to the background'},
    {'appLowMemory', 'app', 'the system is short of memory'},
    {'appQuitRequested', 'app', 'the player asked to quit'},
    {'paused', 'app', 'the game paused'},
    {'unpaused', 'app', 'the game runs again'},
    {'autoloadStarted', 'app', 'the autoload "$name" started'},
    {'autoloadStopped', 'app', 'the autoload "$name" stopped'},
    {'sceneLoading', 'scene', 'the scene $scene starts to load'},
    {'sceneLoaded', 'scene', 'the scene $scene finished loading'},
    {'sceneLoadFailed', 'scene', 'the scene $scene failed to load with "$error"'},
    {'sceneEntered', 'scene', 'the scene $scene entered the stack'},
    {'sceneExited', 'scene', 'the scene $scene left the stack'},
    {'sceneUnloaded', 'scene', 'the scene $scene unloaded'},
    {'scenePaused', 'scene', 'another scene covers $scene'},
    {'sceneResumed', 'scene', 'the scene $scene is on top again'},
    {'sceneExitTransitionStarted', 'scene', 'a change starts to take $scene off the screen'},
    {'sceneEnterTransitionFinished', 'scene', 'the change ended with $scene on top'},
    {'transitionOutStarted', 'scene', 'the transition starts to hide $from'},
    {'transitionOutFinished', 'scene', 'the transition hid $from'},
    {'transitionInStarted', 'scene', 'the transition starts to show $to'},
    {'transitionInFinished', 'scene', 'the transition showed $to'},
    {'windowResized', 'window', 'the window is $width by $height pixels'},
    {'windowFocusGained', 'window', 'the window has the focus'},
    {'windowFocusLost', 'window', 'the window lost the focus'},
    {'windowFullscreenChanged', 'window', function(value) return value.fullscreen and 'the window went fullscreen' or 'the window left fullscreen' end},
    {'windowOrientationChanged', 'window', 'the screen turned to $orientation'},
    {'windowFoldChanged', 'window', 'the fold of the screen changed to the posture "$posture"'},
    {'windowSafeAreaChanged', 'window', 'the safe area is $width by $height units'},
    {'windowMoved', 'window', 'the window moved to $x, $y'},
    {'windowMonitorsChanged', 'window', 'the monitors changed'},
    {'keyboardShown', 'window', 'the on-screen keyboard covers $height units'},
    {'keyboardHidden', 'window', 'the on-screen keyboard went away'},
    {'guiMounted', 'gui', 'a GUI mounted'},
    {'guiUnmounted', 'gui', 'a GUI unmounted'},
    {'gamepadConnected', 'device', 'gamepad $gamepad, "$name", connected'},
    {'gamepadDisconnected', 'device', 'gamepad $gamepad, "$name", disconnected'},
    {'audioInterrupted', 'device', 'the system took the audio, so every voice paused'},
    {'audioResumed', 'device', 'the system gave the audio back'},
    {'audioRouteChanged', 'device', 'the sound moved to another output'},
    {'systemThemeChanged', 'device', 'the system switched to $theme colors'},
    {'batteryChanged', 'device', 'the battery changed its level or its charging'},
    {'networkOnline', 'network', 'the device is online'},
    {'networkOffline', 'network', 'the device is offline'},
    {'webSocketConnected', 'network', 'a socket connected to $url'},
    {'webSocketDisconnected', 'network', 'the socket to $url closed with the code $code'},
    {'webSocketReconnecting', 'network', 'the socket to $url tries again in $delay seconds, attempt $attempt'},
    {'assetLoaded', 'asset', 'the cache loaded "$path"'},
    {'assetUnloaded', 'asset', 'the cache let go of "$path"'},
    {'assetReloaded', 'asset', 'the asset "$path" reloaded in place'},
    {'objectCreated', 'asset', 'a sprite was made, and $count are alive'},
    {'objectDestroyed', 'asset', 'a sprite was collected, and $count are alive'},
}
local kCode = [[
events.on('transitionOutFinished', function(change) print(change.from, change.to) end, {owner = self})
scene.push(room, {duration = 0.8, loading = view, loadingDelay = 0.1, minimumLoadingTime = 0.6})
function room:load(context)
    context:progress(0.5, 'Building the room')
    async.sleep(100):await()
end]]

-- Writes a field of the table of an engine event for people: a scene by the name of its class in quotes, and a number without trailing zeros.
local function fieldText(value, key)
    local field = value[key]
    if key == 'scene' or key == 'from' or key == 'to' then
        if field == nil then
            return 'no scene'
        end
        return field == false and 'a scene of C++' or '"' .. tostring(field):match('^[^:]+') .. '"'
    end
    return type(field) == 'number' and string.format('%g', field) or tostring(field)
end

-- Writes what an engine event means, from its sentence and the fields of its table.
local function describe(meaning, value)
    if type(meaning) == 'function' then
        return meaning(value)
    end
    return (meaning:gsub('%$(%a+)', function(key) return fieldText(value or {}, key) end))
end

-- A scene that takes a while to load, reporting its progress to the loading view.
local Room = haylen.class('Room', Overlay)

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
local Broken = haylen.class('Broken', Overlay)

function Broken:load()
    async.sleep(200):await()
    error('The server did not answer.')
end

-- The loading view draws a card with the message of the load and a bar with round ends over the covered screen.
local loadingView = {
    render = function(self, progress, message)
        graphics2d.beginScreen()
        local area = graphics2d.canvasBounds()
        local width = math.min(800, area.width - 96)
        local x, y = area.x + (area.width - width) / 2, area.y + area.height / 2 - 70
        graphics2d.drawShape({x - 40, y - 40, width + 80, 180}, {radius = 24, color = '#F2232938', borderWidth = 2, borderColor = '#FF2C3147'})
        graphics2d.drawText(nil, message or '', x + width / 2, y + 10, {size = 34, anchor = {0.5, 0.5}, layer = 1})
        graphics2d.drawShape({x, y + 60, width, 28}, {radius = 14, color = '#FF15172A', layer = 1})
        graphics2d.drawShape({x + 3, y + 63, math.max(22, (width - 6) * progress), 22}, {radius = 11, color = '#FFF2C14E', layer = 2})
    end,
}

function Lifecycle:enter()
    local journal = Journal(80)
    self.journal = journal
    local colors = {}
    for _, group in ipairs(kGroups) do
        colors[group.id] = group.color
    end
    for _, entry in ipairs(kEvents) do
        local name, color, meaning = entry[1], colors[entry[2]], entry[3]
        events.on(name, function(value)
            if name:match('^object') and value.type ~= 'haylen.Sprite' then
                return
            end
            journal:add('Event "' .. name .. '": ' .. describe(meaning, value) .. '.', color)
        end, {owner = self})
    end

    self:frame({
        hint = 'Switch windows, resize, plug in a gamepad or turn the device to see the platform events too.',
        code = kCode,
        controls = {
            ui.button{id = 'clear', text = 'Clear the log', onClick = function() journal:clear() end},
            ui.label{text = 'Scenes and the pause', font = 'caption', color = 'textMuted'},
            ui.button{id = 'room', text = 'Push a scene that loads', variant = 'primary', onClick = function() self:pushRoom() end},
            ui.button{id = 'broken', text = 'Push a failing load', onClick = function() self:pushBroken() end},
            ui.toggle{id = 'pause', text = 'Pause the game', onChange = function(event) haylen.setPaused(event.checked) end},
            ui.label{text = 'Objects and assets', font = 'caption', color = 'textMuted'},
            ui.toggle{id = 'objects', text = 'Object events', onChange = function(event) debugging.setObjectEvents(event.checked) end},
            ui.button{id = 'sprite', text = 'Create and drop a sprite', onClick = function() self:touchSprite() end},
            ui.button{id = 'asset', text = 'Load and release an asset', onClick = function() self:touchAsset() end},
            ui.button{id = 'gui', text = 'Mount a GUI', onClick = function() self:touchGui() end},
            ui.button{id = 'socket', text = 'Open a socket that retries', onClick = function() self:openSocket() end},
            ui.label{text = 'Window', font = 'caption', color = 'textMuted'},
            ui.toggle{id = 'notch', text = 'Simulate a notch', onChange = function(event) viewport.setSafeAreaSimulation(event.checked and 'iphoneDynamicIsland' or nil) end},
            ui.button{id = 'fullscreen', text = 'Toggle fullscreen', onClick = function() window.setFullscreen(not window.fullscreen()) end},
        },
        focus = 'room',
    })
end

function Lifecycle:exit()
    Lifecycle.super.exit(self)
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
        scene.push(Broken(), {duration = 0.6, onError = function(message) self.journal:add('The handler "onError" of the change received "' .. message .. '".', Test.ink) end})
    end
end

function Lifecycle:touchSprite()
    local sprite = graphics2d.newSprite(graphics.whiteTexture())
    self.journal:add('The test made a sprite and dropped it.', Test.ink)
    sprite = nil
    collectgarbage()
end

-- Holds the badge for half a second, then lets it go so the cache unloads it.
function Lifecycle:touchAsset()
    self.badge = assets.texture('events/badge.png')
    timer.after(0.5, function()
        self.badge = nil
        collectgarbage()
    end, {owner = self})
end

function Lifecycle:touchGui()
    local toast = ui.mount(ui.toast{id = 'toast', text = 'A GUI that unmounts itself', open = true}, {owner = self, layer = 2})
    timer.after(1.2, function() toast:unmount() end, {owner = self})
end

-- Nothing listens on the discard port, so the socket fails, retries twice and closes.
function Lifecycle:openSocket()
    if self.socket then
        self.socket:close()
    end
    self.socket = net.connectWebSocket('ws://127.0.0.1:9/', {reconnect = {initialDelay = 0.5, maxDelay = 1, maxAttempts = 2}})
    self.socket:on('close', function(code) self.journal:add('The socket gave up with the code ' .. code .. '.', Test.ink) end)
end

function Lifecycle:update(dt)
    Lifecycle.super.update(self, dt)
    self:status(string.format('The app is %s, the game is %s and %s, with %d scenes and %d open sockets.', haylen.appState(), haylen.paused() and 'paused' or 'running', haylen.halted() and 'halted' or 'not halted', scene.size(), net.openSocketCount()))
end

function Lifecycle:draw(area)
    local margin = 24
    local x = margin
    for _, group in ipairs(kGroups) do
        graphics2d.drawShape({x, 38, 20, 20}, {kind = 'ellipse', color = group.color, layer = 2})
        self:label(group.title, x + 30, 48, {anchor = {0, 0.5}, size = 24})
        x = x + 50 + graphics2d.measureText(self.font, group.title, {size = 24})
    end
    local top = EventsTest.titleRoom
    self:logPanel(self.journal, {margin, top, area.width - margin * 2, area.height - top - margin}, 'Engine events as they happen')
end

return Lifecycle
