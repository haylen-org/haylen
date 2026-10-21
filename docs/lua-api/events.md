# 📖 `haylen.events`

The module `haylen.events` is the event bus of the app. One part of the app publishes an event by name, and any other part listens to it. Neither side needs to know the other. The engine publishes its own events on the same bus, such as `appBackground`, `sceneEntered` or `gamepadConnected`, so an app hears its own events and the engine events the same way. Use events for news that the whole app may care about. Use [`haylen.signal`](signal.md) instead when one object owns the news, such as a player that announces its health. This page lists every function, every event of the engine and every error. The [lifecycle guide](../core/lifecycle.md) explains when the engine events happen.

```lua
local events = require('haylen.events')
```

## Delivery

An event has a name and carries any number of Lua values. Every listener receives those values as its arguments.

- **Order.** Listeners with a higher priority run first. Listeners with the same priority run in the order they subscribed.
- **Consuming.** A listener that returns `true` consumes the event. The listeners after it skip the event, and `events.emit` returns `true`.
- **Now or at the end of the frame.** The function `events.emit` delivers the event at once, inside the call. The function `events.post` queues the event and delivers it at the end of the frame, after every scene has rendered. A queued event keeps its values until then. An event that a listener posts while the queued events run waits for the end of the next frame.
- **Channels.** A channel is a name that narrows who hears an event, such as `'player'`. The functions `events.emitTo` and `events.postTo` publish an event on a channel. A listener with the option `channel` hears only the events of that channel. A listener without a channel hears every event, on any channel or on none. The engine publishes its events on no channel, so only listeners without a channel hear them.
- **Changes during a delivery.** A listener may subscribe and unsubscribe listeners while an event is delivered. A listener that subscribes during a delivery first hears the next event. A listener that an earlier one unsubscribed is skipped.
- **Errors.** An error raised by a listener of `events.emit` reaches the code that called `events.emit`, and the remaining listeners skip the event. An error raised by a listener of a queued event or of an engine event stops the app and shows the error screen with the message and its stack trace.

## Functions

### events.on(name, fn, options)

Subscribes the function `fn` to the events named `name` and returns a [`Connection`](signal.md#connection). Call `connection:disconnect()` to stop listening. The table `options` is optional and takes these keys.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `channel` | string | `''` | Hears only the events of this channel, which `events.emitTo` and `events.postTo` publish. The empty string hears every channel. |
| `priority` | integer | `0` | Listeners with a higher priority run first. A negative priority runs after the default one. |
| `once` | boolean | `false` | Unsubscribes after the first event that reaches the listener. Events of other channels, events that the filter rejects and events that an earlier listener consumed do not count. |
| `owner` | table or userdata | the default owner | Unsubscribes when the owner ends, as [owners](#owners) describes. |
| `filter` | function | none | Runs before `fn` with the same values. The function `fn` runs only when the filter returns a value other than `nil` or `false`. |

An unknown key raises `Unknown option "<key>".`, and the [errors](#errors) section lists the other mistakes.

```lua
local events = require('haylen.events')

local wallet = {coins = 0}
events.on('coinCollected', function(amount)
    wallet.coins = wallet.coins + amount
end)

-- The filter lets only large piles through, and the priority makes this listener run first.
events.on('coinCollected', function(amount)
    print('A pile of ' .. amount .. ' coins.')
end, {priority = 5, filter = function(amount) return amount >= 10 end})

events.on('coinCollected', function()
    print('The first coin of the run.')
end, {once = true})

local counter = events.on('coinCollected', function()
    print('One more pickup.')
end)

events.emit('coinCollected', 1)
events.emit('coinCollected', 25)
print(wallet.coins) -- 26

counter:disconnect()
```

### events.emit(name, ...)

Delivers the event `name` with the values after it to its listeners right away. It returns `true` when a listener consumed the event and `false` otherwise, also when nobody listens. An error raised by a listener reaches the caller.

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

Delivers the event like `events.emit`, on the channel `channel`. The listeners of that channel and the listeners without a channel hear it. It returns whether a listener consumed the event.

```lua
local events = require('haylen.events')

events.on('damage', function(amount) print('The player took ' .. amount .. '.') end, {channel = 'player'})
events.on('damage', function(amount) print('Someone took ' .. amount .. '.') end)

-- Both listeners hear the first event, and only the second one hears the other.
events.emitTo('player', 'damage', 5)
events.emitTo('enemy', 'damage', 8)
```

### events.post(name, ...)

Queues the event `name` with the values after it and delivers it at the end of the frame, after rendering. The values stay alive until then. The call returns nothing, because no listener has run yet.

```lua
local events = require('haylen.events')

events.on('levelSaved', function(slot)
    print('The level is saved in ' .. slot .. '.')
end)

events.post('levelSaved', 'slot1')
print('The save is queued.') -- Prints before the listener runs.
```

### events.postTo(channel, name, ...)

Queues the event like `events.post`, on the channel `channel`.

```lua
local events = require('haylen.events')

events.on('chat', function(text) print('Team: ' .. text) end, {channel = 'team'})

events.postTo('team', 'chat', 'Regroup at the campfire.')
```

### events.topics()

Returns a list with one table for each event name that has ever had a listener, sorted by name. Use it to find listeners that pile up or events that nobody hears. Each table has these fields.

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | The name of the event. |
| `listeners` | integer | The listeners that are still subscribed. |
| `emissions` | integer | Every delivery of the event since its first listener subscribed, whether a listener heard it or not. |
| `stale` | integer | Listeners whose owner is already gone, such as an owner table that the garbage collector freed. The next delivery of the event or the end of the frame removes them. |

```lua
local events = require('haylen.events')

events.on('turnStarted', function() end)
events.emit('turnStarted')
events.emit('turnStarted')

for _, topic in ipairs(events.topics()) do
    print(topic.name .. ': ' .. topic.listeners .. ' listeners, ' .. topic.emissions .. ' deliveries, ' .. topic.stale .. ' stale.')
end
```

## Owners

An owner is a table or a userdata, such as a scene, an autoload, a GUI or a game object, that ends a listener together with itself. Pass it as the option `owner`. The [subscription scopes](../core/lifecycle.md#subscription-scopes) of the lifecycle guide explain the idea. These are the API facts.

- A scene ends when it unloads, and a GUI ends when it is unmounted.
- Any other owner ends when the garbage collector frees it. Its listeners then end at the end of that frame.
- The owner keeps the functions of its listeners, and the bus never keeps the owner. So a listener may use its owner freely without keeping it alive.
- Without the option `owner`, a listener belongs to the default owner. In development, a listener that a module adds while its top level runs belongs to that module, so a [reload](hotReload.md) of the module ends it. Otherwise the listener lives until it disconnects or the app stops.
- An owner that is not a table or a userdata raises `An owner must be a table or a userdata, not <type>.`.

The function [`scene.listen(owner, source, fn, options)`](scene.md#scenelistenowner-source-fn-options) of `haylen.scene` subscribes with an owner in one call, and scenes built on `scene.Scene` call it as `self:listen(name, fn)`. Every engine function that registers a listener, a task or a job takes the same option, such as `signal:connect`, `timer.after`, `tween.to`, `jobs.spawn`, `socket:on` and `debug.addMonitor`.

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

local guard = Guard.new('North gate')
events.emit('alarm')
print(guard.name .. ' is alert: ' .. tostring(guard.alert) .. '.')

-- Once the collector frees the guard, its listener ends at the end of the frame.
guard = nil
collectgarbage()
```

## Engine events

The engine publishes these events on the bus. An event with data passes it to its listeners as one table, so a later version can add fields without breaking the listeners that exist. An event without data passes nothing. Most events arrive at once, while the engine handles the change. The events marked as queued arrive at the end of the frame, like an event of `events.post`. The lifecycle guide explains [where each platform event comes from](../core/lifecycle.md#events-by-platform).

A scene that C++ code pushed arrives as `false` in the fields `scene`, `from` and `to`. A GUI that C++ code mounted leaves the field `gui` empty.

### App and plugins

| Event | When | Value |
| --- | --- | --- |
| `appStarted` | After `source/main.lua` ran, before the first frame. | None. |
| `appActive` | The app came back to the foreground with the focus. | None. |
| `appInactive` | The app is still visible but does not have the focus. The window lost the focus, the system interrupted the app, such as with a phone call or another app that took the audio focus on Android, or native UI of a plugin covers the app, as [`haylen.appCovered()`](haylen.md#haylenappcovered) tells. | None. |
| `appBackground` | The app went to the background. Right after the listeners run, the engine makes the files of `haylen.storage` durable and saves the unsaved changes of `haylen.preferences`. | None. |
| `appLowMemory` | The platform is short of memory. The engine has already dropped the assets that nothing holds. | None. |
| `appQuitRequested` | The player asked to close the window, or to quit the app through its menu, Command+Q or the Dock on macOS. | None. |
| `appStopping` | The app is about to stop, before the scenes leave and the autoloads stop. | None. |
| `appError` | An error stopped the app and the error screen shows. Queued. | The report `{message, file, line, traceback, frames}` of the error, which the web page also receives in `onError`, as [errors and stack traces](../core/lua.md#errors-and-stack-traces) describes. |
| `appRecovered` | The app leaves the error screen, through the `Back to the app` button of a [recoverable](haylen.md#haylensetrecoverablerecoverable) app or `haylen.recover()`. It arrives at the start of the next frame, before the app updates again. | The report of the error, like `appError`. |
| `paused`, `unpaused` | The function [`haylen.setPaused`](haylen.md#haylensetpausedpaused) changed the pause of the game. | None. |
| `pluginStarted`, `pluginStopped` | A plugin started or stopped. The built-in plugins start before any script runs. | `{name}` |
| `autoloadStarted`, `autoloadStopped` | An autoload started or stopped. | `{name}` |

```lua
local events = require('haylen.events')
local storage = require('haylen.storage')

events.on('appBackground', function()
    storage.writeJson('data/autosave.json', {savedAt = os.time()})
end)

events.on('appError', function(report)
    print('The app stopped at ' .. report.file .. ':' .. report.line .. '.')
end)
```

### Scenes and transitions

The [scene lifecycle](../core/lifecycle.md#scene-lifecycle) explains the order of these events.

| Event | When | Value |
| --- | --- | --- |
| `sceneLoading`, `sceneLoaded` | A scene started loading, or its load finished. | `{scene}` |
| `sceneLoadFailed` | The load of a scene failed, right before the scene unloads. | `{scene, error}`, where `error` is the message of the error. |
| `sceneEntered`, `sceneExited` | A scene entered or left the stack. | `{scene}` |
| `sceneUnloaded` | A scene unloaded: after it exited, after its load failed or when its preload was cancelled. | `{scene}` |
| `scenePaused`, `sceneResumed` | A pushed scene covered the scene, or the scene is on top again. | `{scene}` |
| `sceneExitTransitionStarted` | A change started to take the top scene off the screen. | `{scene}` |
| `sceneEnterTransitionFinished` | A change ended and its transition finished. It names the top scene after the change. | `{scene}` |
| `transitionOutStarted`, `transitionOutFinished` | The out half of a transition started to hide the current scenes, or finished. An effect that covers the screen finishes at full cover, and the next scene then loads. An effect that shows both scenes finishes at its exit point. | `{from, to}`, the top scene before the change and the scene that the change brings on top. |
| `transitionInStarted`, `transitionInFinished` | The in half of a transition started to show the scenes after the change, or finished, which ends the change. An effect that shows both scenes starts both halves together. | `{from, to}`, where `to` is the top scene after the change. |

The field `from` or `to` is absent when that side has no scene, such as `to` after the last scene left the stack.

```lua
local events = require('haylen.events')
local scene = require('haylen.scene')

events.on('sceneEntered', function(event)
    print('Entered the scene ' .. tostring(event.scene.name) .. '.')
end)

events.on('sceneLoadFailed', function(event)
    print('A scene failed to load: ' .. event.error)
end)

scene.push({name = 'title'})
```

### Window and screen

| Event | When | Value |
| --- | --- | --- |
| `windowResized` | The framebuffer changed size. | `{width, height}` in pixels. |
| `windowFocusGained`, `windowFocusLost` | The window gained or lost the keyboard focus. | None. |
| `windowFullscreenChanged` | The window entered or left fullscreen. | `{fullscreen}` |
| `windowOrientationChanged` | The screen turned between landscape and portrait, as [`window.orientation()`](window.md#windoworientation) reports it. Desktop windows always count as landscape. | `{orientation}`, `'landscape'` or `'portrait'`. |
| `windowSafeAreaChanged` | The safe area moved, such as after a rotation, or when a native view of a plugin reserved or released an edge of the screen. | `{x, y, width, height}` in design units, the same rectangle as [`viewport.safeRect()`](viewport.md#viewportsaferect). |
| `windowFoldChanged` | The fold of a foldable or dual-screen device across the window appeared, moved, went away or changed its state, or a simulation replaced it, as [`window.fold()`](window.md#folds-and-dual-screens) reports it. | `{posture, segments, fold}`. The field `posture` is `'flat'`, `'tabletop'` or `'book'`. The list `segments` holds the areas of the window on each side of the fold as `{x, y, width, height}` in design units, or one area for the whole window without a fold. The field `fold` is absent without a fold, and otherwise holds `x`, `y`, `width` and `height` in design units, `axis`, `state`, `separating` and `occluding`. |
| `windowMoved` | The desktop window moved, because the player dragged it, the app placed it or the system moved it. A move publishes each new position once. | `{x, y}`, the top left corner of [`window.frame()`](window.md#desktop-windows) in desktop points. |
| `windowMonitorsChanged` | A monitor connected, disconnected or changed, or its work area changed, such as when the taskbar moved. The function `window.monitors()` returns the new list. | None. |
| `keyboardShown` | The on-screen keyboard appeared or changed its frame, such as when a suggestion bar shows. | `{x, y, width, height}`, the area it covers in design units, like `viewport.safeRect()`. |
| `keyboardHidden` | The on-screen keyboard went away. | None. |
| `guiMounted`, `guiUnmounted` | A GUI of [`haylen.ui`](ui.md) was mounted or unmounted. | `{gui}` |

```lua
local events = require('haylen.events')

events.on('windowSafeAreaChanged', function(safe)
    print('The HUD fits in ' .. safe.width .. ' by ' .. safe.height .. ' units.')
end)

events.on('keyboardShown', function(area)
    print('The keyboard covers ' .. area.height .. ' units at the bottom.')
end)
```

### Devices and system

| Event | When | Value |
| --- | --- | --- |
| `gamepadConnected`, `gamepadDisconnected` | A gamepad was plugged in or removed. The gamepads that are connected when the app starts are announced on its first frame. | `{gamepad, name}`, where `gamepad` counts from 1 like [`haylen.input`](input.md). |
| `audioInterrupted`, `audioResumed` | The system took the audio, such as for a phone call, and every voice paused. Or the system gave the audio back while the app is active, and the voices resumed. The [audio reference](audio.md#events) explains both. | None. |
| `audioRouteChanged` | The audio output moved to another device, such as when headphones were unplugged. | None. |
| `audioUnlocked` | A browser let the audio play after the first interaction with the page. | None. |
| `systemThemeChanged` | The system switched between light and dark colors, as [`system.theme()`](system.md#systemtheme) reports them. | `{theme}`, `'light'` or `'dark'`. |
| `batteryChanged` | The level, the charging or the state of the battery changed, as [`system.battery()`](system.md#systembattery) reports it. | `{level, charging, state}`, without `level` when the platform does not know it. |
| `mediaCommand` | A now-playing control of the system, a media key or a headset button asked the app to change what it plays, as [`system.setNowPlaying`](system.md#systemsetnowplayinginfo) describes. | `{command, position}`, where `position` in seconds comes only with the command `'seek'`. |
| `accessibilitySettingsChanged` | The text scale, reduced motion, high contrast, reduced transparency or screen reader of the system changed, as [`accessibility.settings()`](accessibility.md#accessibilitysettings) reports them. | `{textScale, reducedMotion, highContrast, reducedTransparency, screenReader}` |
| `saveSlotWritten` | The function `storage.writeSlot()` or `storage.writeSlotAsync()` of [`haylen.storage`](storage.md) wrote a slot. A cloud save plugin uploads it. | `{slot, revision, version}` |

```lua
local events = require('haylen.events')

events.on('gamepadConnected', function(info)
    print('Gamepad ' .. info.gamepad .. ' is a ' .. info.name .. '.')
end)

events.on('systemThemeChanged', function(change)
    print('The system now uses ' .. change.theme .. ' colors.')
end)
```

### Network

| Event | When | Value |
| --- | --- | --- |
| `networkOnline`, `networkOffline` | The device gained or lost its network. Browsers report it, Android reports it with the permission `android.permission.ACCESS_NETWORK_STATE`, and Apple platforms report it, macOS included. The first report publishes the state the app starts in, usually on its first frame. [`haylen.networkState()`](haylen.md#haylennetworkstate) returns the last state at any time. | None. |
| `webSocketConnected` | A WebSocket of [`haylen.net`](net.md) opened, the first time or after reconnecting. | `{url, protocol}` |
| `webSocketDisconnected` | An open WebSocket lost its connection or closed, before it reconnects or ends. | `{url, code, reason}` |
| `webSocketReconnecting` | A WebSocket with reconnection planned its next attempt after the connection dropped or an attempt failed. | `{url, attempt, delay}`, where `attempt` counts from 1 and `delay` is the wait in seconds. |

```lua
local events = require('haylen.events')

events.on('networkOffline', function()
    print('The device is offline.')
end)

events.on('webSocketReconnecting', function(socket)
    print(string.format('The socket to %s retries in %.1f seconds, attempt %d.', socket.url, socket.delay, socket.attempt))
end)
```

### Assets, content and development

| Event | When | Value |
| --- | --- | --- |
| `assetLoaded` | An asset of [`haylen.assets`](assets.md) entered the cache. Queued. | `{type, path}` |
| `assetUnloaded` | The last holder of a cached asset let it go, such as the last preload group that held it. Queued. | `{type, path}` |
| `assetReloaded` | During hot reload, a changed file updated a live asset in place. Queued. | `{type, path}` |
| `assetChanged` | During hot reload, a changed file left behind a live asset of a type that cannot update in place. The object that the app holds is stale until it loads the file again. Queued. | `{type, path}` |
| `contentUpdateAvailable` | A check of [`haylen.content`](content.md#events) found an update of the content. | `{generation, downloadBytes}` |
| `contentUpdateReady` | An update of the content is installed and plays from the next start. | `{generation}` |
| `contentUpdateFailed` | A check or a download of an update failed. | `{code, message}` |
| `contentRecovered` | The app fell back to earlier content when it started, because downloaded content could not play. It arrives during `appStarted`, so the listeners of `source/main.lua` hear it. | `{manifest, reason}`, where `manifest` is the ID of the content that could not play. |
| `moduleReloaded` | During hot reload, a changed Lua module reloaded in place, once for each module of a save, as [`haylen.hotReload`](hotReload.md) describes. Queued. | `{module, path}` |
| `breadcrumbAdded` | The function `debug.addBreadcrumb()` added a breadcrumb, which a crash reporting or analytics plugin forwards to its service. Queued. | `{message, data, time}`, where `time` counts the seconds since 1970. |
| `objectCreated`, `objectDestroyed` | While object events are on, a counted object was created or destroyed, as [`haylen.debug`](debug.md#object-counts-and-events) describes. Queued. | `{type, count}`, where `type` is a counter name such as `'haylen.Sprite'` or `'Texture'`. |

```lua
local events = require('haylen.events')

events.on('assetUnloaded', function(asset)
    print('Released the ' .. asset.type .. ' ' .. asset.path .. '.')
end)

events.on('moduleReloaded', function(change)
    print('Reloaded the module ' .. change.module .. '.')
end)
```

## C++

C++ code publishes and listens on the same bus through `Engine::getEvents()`. The data of an event from C++ is JSON, which Lua listeners receive as a table. The bus also carries typed events, which only C++ hears. The class `haylen::core::LifecycleEvent` names every engine event, and a `core::ConnectionScope` ends the listeners it holds when its owner is destroyed.

```cpp
#include "haylen/core/ConnectionScope.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"

// The listener ends with the tracker, since the scope disconnects it.
class LevelTracker final {
  public:
    explicit LevelTracker(haylen::core::Engine& engine) : events(engine.getEvents()) {
        listeners.add(events.on(haylen::core::LifecycleEvent::kAppBackground, [this](haylen::core::EventBus::Event&) { ++backgroundVisits; }));
    }

    // Lua listeners of "levelFinished" receive the table {level = 3} for the third level.
    void finish(int level) {
        events.emit("levelFinished", {{"level", level}});
    }

  private:
    haylen::core::EventBus& events;
    haylen::core::ConnectionScope listeners;
    int backgroundVisits = 0;
};
```

## Errors

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | The options of `events.on` hold a key that it does not take. |
| `Option tables only accept string keys.` | The options of `events.on` hold a key that is not a string. |
| `The option "<key>" of "on" is invalid: <reason>.` | An option has a value of the wrong type, such as a `priority` that is not an integer or a `once` that is not a boolean. |
| `An owner must be a table or a userdata, not <type>.` | The option `owner` is another type. |
| `bad argument #<n> to '<function>' (<type> expected, got <type>)` | An argument has the wrong type, such as a name or a channel that is not a string, a listener or a `filter` that is not a function, or options that are not a table. |
