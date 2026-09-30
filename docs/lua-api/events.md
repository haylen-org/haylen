# haylen.events

`haylen.events` is the event bus of the app. Any part of the app publishes an event by name and any other part listens to it, without either side knowing the other. The engine publishes its own lifecycle events on the same bus, such as `appBackground`, `sceneEntered` or `gamepadConnected`, so app events and engine events are heard the same way. Use events for announcements that the whole app may care about, and use [haylen.signal](signal.md) when one object owns the announcement. The [lifecycle guide](../lifecycle.md) explains when the engine events happen.

```lua
local events = require('haylen.events')
```

## Delivery

An event carries any number of Lua values, and every listener receives them as its arguments. Listeners run by descending priority and, with equal priorities, in the order they subscribed. A listener that returns `true` consumes the event, so the listeners after it skip the event and `events.emit` returns `true`.

`events.emit` delivers an event at once, inside the call. `events.post` queues it until the end of the frame, after every scene has rendered, and keeps its values until then. A listener may subscribe and unsubscribe listeners while an event is delivered. A listener that subscribes during a delivery hears the next event, and a listener that an earlier one unsubscribed is skipped.

An error raised by a listener of `events.emit` reaches the code that called `emit` and skips the remaining listeners. An error raised by a listener of a queued event or of an engine event stops the app and shows the error screen with the message and its stack trace.

C++ code publishes and listens on the same bus through `Engine::getEvents()`, which also takes typed events, and `haylen::core::LifecycleEvent` names the engine events.

## Functions

### events.on(name, fn, options)

Subscribes `fn` to the events named `name` and returns a [Connection](signal.md#connection). `options` is an optional table with the keys below, and unknown keys raise `Unknown option '<key>'.`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `channel` | string | `''` | Listens only to events published on this channel with `events.emitTo` or `events.postTo`. Without a channel the listener hears every channel. |
| `priority` | integer | `0` | Listeners with a higher priority run first. |
| `once` | boolean | `false` | Unsubscribes after the first event that reaches the listener. Events of other channels and events that the filter rejects do not count. |
| `owner` | table or userdata | `nil` | Unsubscribes when the owner ends, as [Owners](#owners) describes. |
| `filter` | function | `nil` | Receives the same values as `fn` and returns whether `fn` runs for this event. |

```lua
local events = require('haylen.events')

local wallet = {coins = 0}
events.on('coinCollected', function(amount)
    wallet.coins = wallet.coins + amount
end)

events.on('coinCollected', function(amount)
    print('a pile of ' .. amount .. ' coins')
end, {priority = 5, filter = function(amount) return amount >= 10 end})

events.on('coinCollected', function()
    print('the first coin of the run')
end, {once = true})

events.emit('coinCollected', 1)
events.emit('coinCollected', 25)
print(wallet.coins) -- 26
```

### events.emit(name, ...)

Delivers the event named `name` with the given values to its listeners right away and returns `true` when a listener consumed it, or `false` otherwise.

```lua
local events = require('haylen.events')

-- The shield listens first and absorbs one hit while it is up.
local shield = {up = true}
events.on('hit', function(damage)
    if shield.up then
        shield.up = false
        return true
    end
end, {priority = 10})

local player = {health = 100}
events.on('hit', function(damage)
    player.health = player.health - damage
end)

print(events.emit('hit', 30)) -- true
print(events.emit('hit', 30)) -- false
print(player.health) -- 70
```

### events.emitTo(channel, name, ...)

Delivers the event like `events.emit` on the given channel. Listeners of that channel and listeners without a channel hear it.

```lua
local events = require('haylen.events')

events.on('damage', function(amount) print('the player took ' .. amount) end, {channel = 'player'})
events.on('damage', function(amount) print('someone took ' .. amount) end)

events.emitTo('player', 'damage', 5)
events.emitTo('enemy', 'damage', 8)
```

### events.post(name, ...)

Queues the event and delivers it at the end of the frame, after rendering. The values are kept until then, and the call returns nothing because no listener has run yet.

```lua
local events = require('haylen.events')

events.on('levelSaved', function(slot)
    print('saved ' .. slot)
end)

events.post('levelSaved', 'slot1')
print('queued') -- printed before 'saved slot1'
```

### events.postTo(channel, name, ...)

Queues the event like `events.post` on the given channel.

```lua
local events = require('haylen.events')

events.on('chat', function(text) print('team: ' .. text) end, {channel = 'team'})

events.postTo('team', 'chat', 'regroup at the campfire')
```

### events.topics()

Returns a sequence with one table per event name that has ever had a listener, with the fields `name`, `listeners` (the listeners still subscribed), `emissions` (the events delivered since the first listener subscribed) and `stale` (listeners whose owner is already gone, such as an owner table the garbage collector took, which the next delivery or the end of the frame removes). Use it to find listeners that pile up or events that nobody hears.

```lua
local events = require('haylen.events')

events.on('turnStarted', function() end)
events.emit('turnStarted')
events.emit('turnStarted')

for _, topic in ipairs(events.topics()) do
    print(topic.name, topic.listeners, topic.emissions, topic.stale)
end
```

## Owners

An owner ties listeners to the life of a table or a userdata, such as a scene, an autoload, a UI document or a game object. Pass it as the `owner` option and every listener it holds unsubscribes when the owner ends. A scene ends when it unloads, a UI document when it is unmounted, and any other owner when the garbage collector frees it, in which case its listeners end at the end of that frame. Timers of [haylen.timer](timer.md), tweens of [haylen.tween](tween.md), signal connections of [haylen.signal](signal.md), monitors of [haylen.debug](debug.md#debugaddmonitorname-fn-options) and documents of [haylen.ui](ui.md#uimounttree-options) take the same `owner` option, and the tasks of [scene.spawn](scene.md#scenespawnowner-fn) stop with their owner too.

The owner keeps the functions of its listeners, and the bus does not keep the owner. A listener may therefore refer to its owner freely without keeping it alive. `scene.listen(owner, name, fn, options)` of [haylen.scene](scene.md) is a shorthand for `events.on(name, fn, {owner = owner})`, and scenes built on `scene.Scene` call it as `self:listen(name, fn)`.

```lua
local events = require('haylen.events')

local Guard = {}
Guard.__index = Guard

function Guard.new(name)
    local self = setmetatable({name = name, alert = false}, Guard)
    -- The listener ends with the guard and never keeps it alive.
    events.on('alarm', function() self.alert = true end, {owner = self})
    return self
end

local guard = Guard.new('north gate')
events.emit('alarm')
print(guard.name, guard.alert) -- north gate true

guard = nil
collectgarbage()
```

## Engine events

The engine publishes these events on the bus. Events with data pass it to listeners as one table, events of scenes and UI documents pass the scene table or the document, and the other events pass nothing. A scene pushed from C++ arrives as `false`, also inside the tables of the transition phases, and a document mounted from C++ arrives as `nil`.

| Event | When | Value |
| --- | --- | --- |
| `appStarted` | After `source/main.lua` ran, before the first frame. | None. |
| `appActive` | The app came back to the foreground with focus. | None. |
| `appInactive` | The app is still visible, but the window lost the focus, the system interrupted it, such as with a phone call or another app taking the audio focus on Android, or native UI of a plugin covers it, as `haylen.appCovered()` tells. | None. |
| `appBackground` | The app went to the background. The engine makes the files of `haylen.storage` durable right after the listeners run. | None. |
| `appLowMemory` | The platform is short of memory, after the engine dropped released assets. | None. |
| `appQuitRequested` | The player asked to close the window. | None. |
| `appStopping` | The app is about to stop, before the scenes leave and the autoloads stop. | None. |
| `paused`, `unpaused` | `haylen.setPaused` changed the pause of the game. | None. |
| `sceneLoading`, `sceneLoaded` | A scene started loading, or its load finished. | The scene. |
| `sceneLoadFailed` | The load of a scene failed, right before the scene unloads. | `{scene, error}`, with the message of the error. |
| `sceneEntered`, `sceneExited` | A scene entered or left the stack. | The scene. |
| `sceneUnloaded` | A scene unloaded, after it exited, after its load failed or when its preload was cancelled. | The scene. |
| `scenePaused`, `sceneResumed` | A scene was covered by a pushed scene, or is on top again. | The scene. |
| `sceneExitTransitionStarted` | A change started to take the top scene off the screen. | The scene. |
| `sceneEnterTransitionFinished` | A change ended and its transition finished, on the top scene after it. | The scene. |
| `sceneCoverStarted`, `sceneCoverFinished` | The cover of an effect that covers the screen started, or reached full cover. | `{from, to}`, the top scenes before and after the change. |
| `sceneHoldStarted`, `sceneHoldFinished` | The covered screen started to wait for the next scene, or stopped waiting. | `{from, to}` |
| `sceneRevealStarted`, `sceneRevealFinished` | The effect started or finished showing the scenes after the change, which is the whole effect for effects that show both scenes. | `{from, to}`, where `to` is the scene the change put or left on top. |
| `pluginStarted`, `pluginStopped` | A plugin started or stopped. Built-in plugins start before any script runs. | `{name}` |
| `autoloadStarted`, `autoloadStopped` | An autoload started or stopped. | `{name}` |
| `windowResized` | The framebuffer changed size. | `{width, height}` in pixels. |
| `windowFocusGained`, `windowFocusLost` | The window gained or lost the keyboard focus. | None. |
| `windowFullscreenChanged` | The window entered or left fullscreen. | `{fullscreen}` |
| `windowOrientationChanged` | The screen turned between landscape and portrait, as `window.orientation()` of [haylen.window](window.md) reports it. Desktop windows always count as landscape. | `{orientation}`, `'landscape'` or `'portrait'`. |
| `windowSafeAreaChanged` | The safe area moved, for example after a rotation or when a native view of a plugin reserved or released an edge of the screen. | `{x, y, width, height}`, the same rectangle as `viewport.safeRect()`. |
| `windowMoved` | The desktop window moved, dragged by the player, placed by the app or moved by the system. A move publishes each new position once. | `{x, y}`, the top left corner of `window.frame()` of [haylen.window](window.md#desktop-windows) in desktop points. |
| `windowMonitorsChanged` | A monitor connected, disconnected or changed, or its work area changed, such as when the taskbar moved. `window.monitors()` returns the new list. | None. |
| `uiDocumentMounted`, `uiDocumentUnmounted` | A UI document of [haylen.ui](ui.md) was mounted or unmounted. | The document. |
| `gamepadConnected`, `gamepadDisconnected` | A gamepad was plugged in or removed. Gamepads that are connected when the app starts are announced on its first frame. | `{gamepad, name}`, where `gamepad` counts from 1 like [haylen.input](input.md). |
| `audioInterrupted`, `audioResumed` | The system took the audio, such as for a phone call, and every voice paused, or gave it back while the app is active and the voices resumed, as [haylen.audio](audio.md#events) explains. | None. |
| `audioRouteChanged` | The audio output moved to another device, such as headphones that were unplugged. | None. |
| `keyboardShown` | The on-screen keyboard appeared or changed its frame, such as when a suggestion bar shows. | `{x, y, width, height}`, the area it covers in design units, like `viewport.safeRect()`. |
| `keyboardHidden` | The on-screen keyboard went away. | None. |
| `networkOnline`, `networkOffline` | The device gained or lost its network, where the platform reports it: browsers, Android and Apple platforms, macOS included. The first report publishes the state the app starts in, usually on its first frame, and [haylen.networkState()](haylen.md#haylennetworkstate) returns the last state at any time. | None. |
| `systemThemeChanged` | The system switched between light and dark colors, as `system.theme()` of [haylen.system](system.md#systemtheme) reports them. | `{theme}`, `'light'` or `'dark'`. |
| `batteryChanged` | The level, the charging or the state of the battery changed, as `system.battery()` of [haylen.system](system.md#systembattery) reports it. | `{level, charging, state}` |
| `webSocketConnected` | A WebSocket of [haylen.net](net.md) opened, the first time or after reconnecting. | `{url, protocol}` |
| `webSocketDisconnected` | An open WebSocket lost its connection or closed, before it reconnects or ends. | `{url, code, reason}` |
| `webSocketReconnecting` | A WebSocket with reconnection scheduled its next attempt after the connection dropped or an attempt failed. | `{url, attempt, delay}`, where `attempt` counts from 1 and `delay` is the wait in seconds. |
| `assetLoaded` | An asset of [haylen.assets](assets.md) entered the cache, queued for the end of the frame. | `{type, path}` |
| `assetUnloaded` | The last holder of a cached asset let it go, such as the last preload group that held it, queued for the end of the frame. | `{type, path}` |
| `assetReloaded` | A changed file updated a live asset in place during hot reload, queued for the end of the frame. | `{type, path}` |
| `objectCreated`, `objectDestroyed` | While object events are on, a counted object was created or destroyed, queued for the end of the frame, as [haylen.debug](debug.md#object-counts-and-events) describes. | `{type, count}`, where `type` is a counter name such as `'haylen.Sprite'` or `'Texture'`. |

```lua
local events = require('haylen.events')
local storage = require('haylen.storage')

events.on('appBackground', function()
    storage.writeJson('saves/autosave.json', {savedAt = os.time()})
end)

events.on('gamepadConnected', function(info)
    print('gamepad ' .. info.gamepad .. ' connected: ' .. info.name)
end)

events.on('windowSafeAreaChanged', function(safe)
    print('lay out the HUD inside', safe.x, safe.y, safe.width, safe.height)
end)

events.on('sceneEntered', function(entered)
    print('entered ' .. tostring(entered.name))
end)

events.on('assetUnloaded', function(asset)
    print('released ' .. asset.type .. ' ' .. asset.path)
end)

events.on('webSocketReconnecting', function(socket)
    print(string.format('%s retries in %.1f seconds, attempt %d', socket.url, socket.delay, socket.attempt))
end)

require('haylen.scene').push({name = 'title'})
```
