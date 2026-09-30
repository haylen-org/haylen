# Lifecycle

This guide explains what happens to an app from start to stop: the order in which the engine calls scenes, autoloads and plugins, the states the app goes through with the platform, how the game pause and the process modes decide what runs, how autoloads live next to the scenes and how listeners, timers and tweens end together with the thing that owns them. The engine publishes the moments of this lifecycle as events on [haylen.events](lua-api/events.md), with the same names in C++ and Lua, and the [events reference](lua-api/events.md#engine-events) lists them with their values.

## Start, frames and stop

The engine starts an app in a fixed order.

1. The runtime reads `app.json`, creates the window and the engine, and every built-in plugin starts and installs its Lua modules, each one publishing `pluginStarted`.
2. The autoloads that `app.json` lists load in order, and each one gets `start` and publishes `autoloadStarted`.
3. `source/main.lua` runs, which usually subscribes listeners and pushes the first scene.
4. The engine publishes `appStarted`, and the first frame begins.

Every frame then runs the same steps.

1. Window and gamepad changes are published, assets that finished decoding in the background create their GPU resources within the upload budget of the frame, Varn's event loop runs, where promises settle and `async` coroutines and scene tasks resume, and native replies of the platform bridge arrive.
2. Fixed steps run for the time that passed: the top scene gets `fixedUpdate`, fixed-step tweens advance and the autoloads get `fixedUpdate`.
3. Timers and tweens advance and the autoloads get `update`. Then scene loads that finished or failed settle, the pending scene change moves through its phases, the loading view gets `update` and the top scene gets `update`.
4. The visible scenes get `render`, the loading view and the autoloads get `render`, the visible scenes get `renderUi`, and the loading view, the autoloads and UI documents get `renderUi`. During a scene transition, the scenes before and after the change do this into their own images, the loading view, the autoloads and UI documents drawing with the current scenes, and the transition effect draws the two images on the screen.
5. Events queued with `events.post` and deferred signal listeners run.

Platform events, such as keys, touches and focus changes, arrive between frames. The autoloads get them first and then the top scene, as the [scene reference](lua-api/scene.md#events) describes.

When the app stops, the engine publishes `appStopping`, removes every scene from the top down, together with a scene that is still loading and the preloaded scenes, stops the autoloads from the last to the first and stops the plugins in reverse order. A restart, such as a hot reload of an edited script, stops the app the same way, even in the middle of a load, and creates a new engine with a fresh Lua state that runs the whole sequence again. On Android the app stops the same way when its activity closes, through `haylen.quit()`, the back button of the root screen or the player removing it from the recent apps, and the activity then finishes normally.

```lua
local events = require('haylen.events')
local scene = require('haylen.scene')

for _, name in ipairs({'appStarted', 'sceneLoaded', 'sceneEntered', 'sceneEnterTransitionFinished', 'appStopping'}) do
    events.on(name, function() print(name) end)
end

scene.push({
    enter = function(self) print('enter') end,
    update = function(self, dt)
        if not self.announced then
            self.announced = true
            print('first update')
        end
    end,
})
```

## App states

The platform moves the app between three states, which `haylen.appState()` returns and whose changes publish events.

| State | Meaning | Event |
| --- | --- | --- |
| `'active'` | In the foreground with the focus. | `appActive` |
| `'inactive'` | Still visible, but the window lost the focus, the system interrupted the app, such as with a phone call, or native UI of a plugin covers it. | `appInactive` |
| `'background'` | Hidden, such as a minimized window, another app on a phone or a hidden browser tab. | `appBackground` |

On the web the page tells the engine: the window of the page losing the focus makes the app `'inactive'`, while the focus moving between the canvas and other elements of the page, such as the buttons of a plugin dialog, changes nothing, a hidden tab (`visibilitychange`) sends the app to `'background'` and a visible one brings it back, and a page that goes away (`pagehide`) makes the files of [haylen.storage](lua-api/storage.md) durable once more. On Android the app holds the audio focus while it is in the foreground, and another app that takes it, for a phone call or an alarm, interrupts the app, which stays `'inactive'` until the focus comes back, even when its window has the focus.

The engine takes care of what every app needs when the state changes.

- Leaving `'active'` releases every held key, mouse button and touch, and every virtual button and stick of the on-screen controls, so nothing stays pressed when the player comes back.
- Going to `'background'` stops rendering, so the app does no GPU work, and suspends the audio device. Preferences with unsaved changes are saved, and once the listeners of `appBackground` have run the files of [haylen.storage](lua-api/storage.md) become durable, because the platform may end an app in the background without warning. An app saves its own progress in a listener of `appBackground`.
- Coming back from `'background'` resumes the audio, and the first frame after a halt takes no time, so timers, tweens and physics do not jump by the time the app was away.
- A system interruption of the audio, such as a phone call, an alarm, Siri or another Android app taking the audio focus, pauses every voice and publishes `audioInterrupted`. Its end resumes them and publishes `audioResumed` once the app is active, and a change of the audio output publishes `audioRouteChanged`, as the [audio guide](audio.md#sessions-and-interruptions) explains.

Whether the app keeps running while it is not active depends on the lifecycle options, which `app.json` sets in its `lifecycle` object and `haylen.setLifecycle` changes at run time. A halted app lets no time pass: scenes, autoloads, timers, tweens and fixed steps stand still and input is held back, while Varn's event loop still delivers network replies and `async` results. `haylen.halted()` tells whether the app is halted right now.

The event loop and the bridge run inside the frames of the app, so nothing arrives while the platform runs no frames at all. On Android the frames stop while the activity is paused or its window lacks the focus, which includes the background, on iOS, iPadOS and tvOS while the scene is not active, and browsers stop the frames of a hidden tab. The replies and events of the platform bridge, the answers of native dialogs and screens, Varn timers, network callbacks and `async` results then wait, and they arrive in the first frame after the app comes back.

Android also freezes the processes of cached apps, which stops every thread of the app, the transfers of the network and the threads of native plugins included, until the app comes back, so a connection may time out meanwhile. An app that the person left, such as with Home, stays the previous app for about a minute, then becomes cached, and Android freezes it some seconds later. The [screens](lua-api/platform.md#screens) of plugins follow the same rules. A screen that is an activity of the app itself, such as the confirm screen of the plugins sample, keeps the process in the foreground, which Android never freezes, while a screen of another app that the app started, such as the document picker, a browser page or a purchase flow, makes the app the previous app, which freezes about a minute later. Until then the threads of the app keep working while its frames stand still, and their answers reach the app in its first frame after the screen. The answer of the screen itself always arrives, since Android wakes the process to deliver it.

| Option | Default | Meaning |
| --- | --- | --- |
| `pauseOnBackground` | `true` | Halts the app in the background. |
| `pauseOnFocusLoss` | `false` | Halts the app while it is inactive. Tiny Island turns it on, so the run stands still whenever the window loses focus, and opens its pause menu on `appInactive` and `appBackground`, so the run stays paused when the player comes back. |
| `muteOnFocusLoss` | `false` | Mutes the master bus while the app is not active, and restores the mute the player chose when it comes back. |

```json
{
    "name": "Tiny Island",
    "identifier": "dev.haylen.tinyisland",
    "lifecycle": {"pauseOnFocusLoss": true, "muteOnFocusLoss": true}
}
```

```lua
local haylen = require('haylen')
local events = require('haylen.events')
local storage = require('haylen.storage')

local progress = {day = 3, wood = 12}

events.on('appBackground', function()
    storage.writeJson('saves/progress.json', progress)
end)

events.on('appActive', function()
    print('welcome back, the app is ' .. haylen.appState())
end)

-- A music player keeps playing when the window loses focus, and a strategy game may keep simulating in the background.
haylen.setLifecycle({muteOnFocusLoss = false})
print(haylen.lifecycle().pauseOnFocusLoss, haylen.halted())
```

Low memory warnings publish `appLowMemory` after the engine has dropped every asset that nothing holds anymore, which is the moment to unload preload groups and caches the app can rebuild. A request to close the window publishes `appQuitRequested`, and `haylen.quit()` ends the app.

The device and its screen publish their changes too. `keyboardShown` and `keyboardHidden` follow the on-screen keyboard, whose frame the UI already avoids by lifting the focused text field above it, as the [text input guide](text-input.md) explains. `networkOnline` and `networkOffline` follow the network where browsers, Android and Apple platforms, macOS included, report it: the first report publishes the state the app starts in, usually on its first frame, and later reports publish each change, while `haylen.networkState()` returns the last state at any time. `systemThemeChanged` and `batteryChanged` follow the light or dark colors of the system and the battery, once per change, as [haylen.system](lua-api/system.md#events) describes. `windowOrientationChanged` follows the screen, whose orientation `window.orientation()` reads and `window.lockOrientation` locks, as [haylen.window](lua-api/window.md) describes. `windowSafeAreaChanged` follows the safe area of [haylen.viewport](lua-api/viewport.md#viewportsaferect), which is the safe area of the device widened, edge by edge, by the edges that native views of plugins reserve, such as a banner at the bottom, so UI anchored to the safe area moves out of their way on its own. On desktops, `windowMoved` follows the position of the window and `windowMonitorsChanged` the monitors and their work areas, as the [desktop guide](desktop.md) describes.

```lua
local events = require('haylen.events')

events.on('networkOffline', function()
    print('playing offline until the network comes back')
end)

events.on('keyboardShown', function(frame)
    print('the keyboard covers the screen from y = ' .. frame.y)
end)
```

### Covered by native UI

Plugins show native UI over the app, such as a full screen ad, a consent form, a sign-in sheet or a purchase dialog, and tell the engine that it covers the app while it shows. Covered content must never play under native UI, so while any cover lasts the app is `'inactive'`, halted and its master bus muted, whatever the lifecycle options say, and `haylen.appCovered()` returns `true`. Covers nest, and when the last one ends the app returns to the state it would have without them, `'active'` when its window has the focus and nothing else interrupts it, with the mute the player chose and without a jump of the clock. An app that goes to the background while covered goes there as usual, comes back `'inactive'` while the cover lasts, and stays in the background when the cover ends there.

The engine takes the cover at the start of every frame and publishes the state changes as usual, so an app pauses its game on `appInactive` the same way for a cover as for a phone call. A halted app would draw the same frame again and again, so the engine draws one frame once the cover begins, which shows what the app drew for `appInactive`, such as a pause menu, and keeps it on screen. It draws the frame again only when the window changes size or the app comes back from the background, since the frame on screen no longer fits then, and draws every frame again once the cover ends. The cover belongs to the platform, so an app that restarts under native UI, such as after a hot reload, becomes covered on its first frame. The [plugin guide](plugins.md#engine-services) shows how native code covers the app.

The [screens](lua-api/platform.md#screens) of plugins, such as a paywall or a sign-in page, cover the app the same way. The engine covers the app at the start of the frame after it asked for a screen and hands the screen to the platform only then, so the app is `'inactive'`, halted and muted before the screen shows, and the cover ends when the screen ends. While an opaque screen shows, the engine draws nothing at all and the last frame stays on screen. The screen belongs to the process as well: an app that restarts under it, such as after `haylen.requestRestart()`, starts covered, and the end of the screen reaches it as the retained event `screenRestored` of the plugin, with the state that the earlier app gave. The same event carries the end of a screen whose process ended while it showed, such as a web page that left for a redirect and loaded again, or an Android app that the system ended in the background, as the [plugin guide](plugins.md#plugin-screens) describes.

```lua
local events = require('haylen.events')
local haylen = require('haylen')

events.on('appInactive', function()
    if haylen.appCovered() then
        print('an ad covers the game, which stands still until it closes')
    end
end)

events.on('appActive', function()
    print('back in the game')
end)
```

## Assets, connections and objects

Assets announce their life as well. `assetLoaded` follows an asset into the cache of [haylen.assets](lua-api/assets.md), `assetUnloaded` follows the moment its last holder lets go, whether a script, a sprite or the last preload group that held it, and `assetReloaded` follows a changed file that hot reload applied to a live asset. The asset manager learns about a release wherever it happens, even inside the garbage collector or on a worker thread, so these events are queued and arrive at the end of the frame, in the order they happened.

Every WebSocket of [haylen.net](lua-api/net.md) publishes `webSocketConnected` when it opens and `webSocketDisconnected` when an open connection ends. A socket opened with reconnection publishes `webSocketReconnecting` with the attempt number and the wait before it, each time it schedules an attempt, until a connection opens again or it gives up.

The debug statistics count objects by type: every userdata type exported to Lua, and engine resources such as textures, fonts, sounds, bodies, emitters, UI documents and tweens. While object events are on, through `debug.setObjectEvents(true)` or `debug.objectEvents` in `app.json`, every creation and destruction also publishes `objectCreated` or `objectDestroyed` with the type name, queued for the end of the frame. They are off by default because a busy app creates many objects every frame.

```lua
local events = require('haylen.events')
local debugging = require('haylen.debug')

events.on('assetUnloaded', function(asset)
    print('freed ' .. asset.type .. ' ' .. asset.path)
end)

events.on('webSocketDisconnected', function(socket)
    print('lost ' .. socket.url .. ' with code ' .. socket.code)
end)

debugging.setObjectEvents(true)
events.on('objectCreated', function(object)
    if object.type == 'haylen.Sprite' then
        print('a sprite was created')
    end
end)
```

## Scene lifecycle

Scenes live on a stack, and every change of it, `push`, `replace`, `pop`, `popTo` and `popToRoot`, runs through one pipeline: the transition takes the current scene off the screen, the next scene loads asynchronously in its `load` hook, with progress and optionally behind a loading view, it enters, and the transition shows it. [haylen.scene](lua-api/scene.md) lists the operations, the options and the hooks in full.

### States

Every scene goes through the same states, which `scene.state(scene)` and `Scene::getState()` return.

| State | Reached when | Hook | Event |
| --- | --- | --- | --- |
| `created` | A change or a preload takes the scene. | None. | None. |
| `loading` | Its load starts. | `load` | `sceneLoading` |
| `loaded` | Its load finished. | None. | `sceneLoaded` |
| `entering` | It enters the stack, or comes back to the top. | `enter`, or `resume` | `sceneEntered`, or `sceneResumed` |
| `active` | The transition that shows it finished. | `enterTransitionFinished` | `sceneEnterTransitionFinished` |
| `covered` | Another scene is pushed on top of it. | `pause` | `scenePaused` |
| `exiting` | A change starts taking it off the stack. | `exitTransitionStarted` on the top scene | `sceneExitTransitionStarted` |
| `exited` | It left the stack. | `exit` | `sceneExited` |
| `unloaded` | It released what it loaded, right after `exit`, after a failed load or when its preload was cancelled. | `unload` | `sceneUnloaded` |

A scene that unloaded can load again, since the same scene table may be pushed again later. Every scene that started loading unloads exactly once, so `unload` pairs with `load` the way `exit` pairs with `enter`, and it also releases a load that stopped halfway.

### Effects that cover the screen

The fade and the other effects that show one scene at a time cover, hold and reveal. The next scene of a `replace` loads only once the replaced scene unloaded at full cover, for a lower peak of memory, unless `unloadBeforeLoad` is `false`, and the next scene of a `push` loads during the cover.

```text
scene.replace(game, {duration = 1, loading = view, loadingDelay = 0.2, minimumLoadingTime = 0.5})

  time   menu (on top)              game (next)                  screen                          events
  ----   -------------------------  ---------------------------  ------------------------------  -----------------------------------------
  0      exitTransitionStarted                                    cover: the menu fades out        sceneExitTransitionStarted menu
                                                                                                   sceneCoverStarted
  ...    update, render                                           the menu, fading
  0.5    exit                                                     full cover                       sceneCoverFinished, sceneExited menu
         unload                     load starts                                                    sceneUnloaded menu, sceneLoading game
                                                                                                   sceneHoldStarted
  ...                               load runs, :await()           hold: the covered frame
  0.7                               progress reaches the view     view:enter, then view:update     (the view appears after loadingDelay)
  ...                               loaded                        the view stays its minimum time  sceneLoaded game
  1.2                                                             the view fades out
  1.45                              enter                         view:exit, reveal starts         sceneHoldFinished, sceneEntered game
                                                                                                   sceneRevealStarted
  ...                               update, render                the game fades in
  1.95                              enterTransitionFinished       the change ended                 sceneRevealFinished
                                                                                                   sceneEnterTransitionFinished game
```

The hold draws the effect at its switch point with the last frame of the outgoing scenes and no scene at all, so the transition itself is the loading screen when the change has no loading view, and waiting costs almost no GPU work. Once the load is done and the view stayed its minimum time, the view fades out into the covered frame over `loadingFadeOut` seconds, 0.25 by default, so the reveal starts from the covered frame without a cut. During the fade the view, the autoloads and the UI documents render over the covered frame into an image of their own, which the effect blends over the covered frame with the opacity left, and a `loadingFadeOut` of 0 takes the view away at once. A view over the current scenes, before an effect that shows both scenes or a change without an effect, goes away at once, since the scenes under it keep running. A pushed-over scene only receives `pause` at full cover and `resume` when the pushed scene pops, and popped scenes exit and unload at full cover while the scene below them receives `resume`.

### Effects that show both scenes

Crossfades, slides, pushes and the other effects that show both scenes at once load first, while the current scenes stay on the screen and keep running, with the loading view over them once the delay passed. A change without a transition loads the same way and then switches within one update.

```text
scene.replace(game, {effect = 'slideIn', duration = 0.5})

  time   menu (on top)              game (next)                  screen                          events
  ----   -------------------------  ---------------------------  ------------------------------  -----------------------------------------
  0      update, render             load starts                  the menu                         sceneLoading game
  ...    update, render             load runs, :await()          the menu, and the view if any
  0.3                               loaded                                                         sceneLoaded game
         exitTransitionStarted      enter                        the effect starts                sceneExitTransitionStarted menu
                                                                                                   sceneEntered game, sceneRevealStarted
  ...    render (outgoing image)    update, render (incoming)    both scenes, each in its image
  0.8    exit, unload               enterTransitionFinished      exit point at the end            sceneExited menu, sceneUnloaded menu
                                                                                                   sceneRevealFinished
                                                                                                   sceneEnterTransitionFinished game
```

The scenes that leave the stack exit at the exit point of the effect, in the update that reaches it and before that frame renders, so nothing draws them after they exit.

### Loading, preloading and errors

`load` runs as a task that the scene owns, so it can wait on promises with `:await()`, and it can also return a promise. The load context reports progress with `context:progress(value, message)`, reads the `params` of the change and preloads asset groups with `context:preload(groups)`, whose progress folds into the progress of the load. The asset manager decodes on the worker pools and creates GPU resources within the upload budget of each frame, so the frame never stalls and the reveal stays smooth right after the load. `scene.preload(scene, params)` starts a load in the background without changing the stack, and a later change that takes the scene is instant, or waits only for the rest of its load.

A failed load, an error in `load`, a rejected promise it returned or an asset of a group it preloads that failed, unloads the scene, publishes `sceneLoadFailed` and ends the change without it: its promise rejects and `onComplete` receives `false`. When the scene that was on top is still alive, the change keeps it, an effect that covers the screen reveals it again, and the failure goes to the `onError` of the change or to the log. When it already unloaded, the failure goes to `onError`, which may route the app to another scene, and to the error screen without one.

### Queues, the pause and the background

- **Queue**: changes requested during a change wait for it in request order, including changes that its hooks and `onError` request, and `scene.clear` drops them all, resolving their promises with `false`.
- **Game pause**: transitions, loads and loading views run on real time, so they go on at full speed while the game is paused or the time scale is zero. The top scene still updates only when its process mode runs, and a covered scene never updates.
- **Input**: by default a change holds input back from its start to its end, including the load, so neither the scenes nor the action map see a key or a touch. A key or button that is still held when the change ends counts as pressed again only after it is released, so the press that opened a menu never closes it. `blockInput = false` lets input through.
- **Background and focus loss**: a halted app lets no time pass, so a change stands still in its phase and its loading delay and minimum time wait too. Its load keeps going while the platform runs frames, since Varn's event loop still runs, promises still settle and the worker pools still decode, but an app in the background creates no GPU resources, so decoded assets wait for it to come back. Once the app runs again, the finished load settles in the next update and the change goes on.
- **Restart and stop**: the engine removes every scene, the scene that is still loading and the preloaded scenes unload too, their tasks are cancelled and the pending asset callbacks are dropped, so a hot reload in the middle of a load leaves nothing behind.

Everything a scene creates with itself as the owner belongs to it and ends when it unloads, so no timer, tween, listener, UI document or coroutine of a scene ever runs once it is gone, as [Subscription scopes](#subscription-scopes) explains.

```lua
local async = require('async')
local events = require('haylen.events')
local scene = require('haylen.scene')

local function tracked(name)
    return {
        load = function(self) print(name .. ' load') async.sleep(100):await() end,
        enter = function() print(name .. ' enter') end,
        exit = function() print(name .. ' exit') end,
        unload = function() print(name .. ' unload') end,
        pause = function() print(name .. ' pause') end,
        resume = function() print(name .. ' resume') end,
        exitTransitionStarted = function() print(name .. ' exit transition started') end,
        enterTransitionFinished = function() print(name .. ' enter transition finished') end,
    }
end

for _, name in ipairs({'sceneCoverStarted', 'sceneHoldStarted', 'sceneRevealStarted', 'sceneRevealFinished'}) do
    events.on(name, function(transfer) print(name) end)
end

scene.push(tracked('map'))
async.spawn(function()
    local shown = scene.push(tracked('inventory'), {duration = 0.4, color = '#FF000000'}):await()
    print('inventory shown', shown)
    scene.pop({duration = 0.2}):await()
    print('back on the map')
end)
```

## Pause and process modes

`haylen.setPaused(true)` pauses the game, and `haylen.paused()` reads it. The pause is part of the game and has nothing to do with the app states: a halted app stops everything, while a paused game stops only what the pause should stop and keeps running what should run while paused, such as a pause menu. Changing the pause publishes `paused` or `unpaused`, and fixed steps do not accumulate while the game is paused.

Scenes, autoloads, timers, tweens and sounds each have a process mode that decides whether they run in the current pause state. Sounds take the mode of their audio bus unless they have their own, and the buses start with effects and ambience pausable while music and interface sounds keep playing, as the [audio guide](audio.md#pause-and-process-modes) explains.

| Mode | Runs while the game plays | Runs while the game is paused |
| --- | --- | --- |
| `'inherit'` | Like its parent. | Like its parent. |
| `'pausable'` | Yes | No |
| `'whenPaused'` | No | Yes |
| `'always'` | Yes | Yes |
| `'disabled'` | No | No |

`'inherit'` is the default. A scene inherits the mode of the scene below it on the stack and resolves to `'pausable'` at the bottom, so a settings screen pushed over a pause menu runs while the game is paused, like the menu. A timer or tween with an owner inherits the mode of its owner: the resolved mode of a scene, or the `processMode` field of any other table, such as an autoload. It reads that mode again every frame, so changing the `processMode` of the owner, or of a scene below it, changes what its timers and tweens do at once. Without an owner it counts as `'pausable'`.

The mode decides whether the top scene gets `update`, `fixedUpdate` and input events, and whether an autoload gets `update`, `fixedUpdate` and input events. Scenes and autoloads keep rendering in every mode, so a paused level stays on screen under its menu. Other events, such as focus changes, reach them in every mode.

When the pause changes, every scene on the stack whose mode stops or starts it hears about it, from the bottom of the stack up. A scene gets `paused` when the change stops it and `unpaused` when the change lets it run again. A scene in `'whenPaused'` therefore gets `unpaused` when the game pauses, and scenes in `'always'` or `'disabled'` hear nothing.

Timers and tweens also choose their clock. By default they count scaled time, which `haylen.setTimeScale` slows down or freezes, and with `unscaled = true` they count real time. The pause and the time scale are independent: a time scale of zero freezes scaled time but keeps calling updates with a delta of zero, while the pause stops what it pauses. UI documents of [haylen.ui](lua-api/ui.md) keep real time, so menus animate and answer input while the game is paused.

```lua
local haylen = require('haylen')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local tween = require('haylen.tween')

local level = {
    enemies = 0,
    paused = function(self) print('level stopped') end,
    unpaused = function(self) print('level runs again') end,
}
timer.every(0.5, function() level.enemies = level.enemies + 1 end, {owner = level})

local menu = {
    processMode = 'whenPaused',
    transparent = true,
    glow = {alpha = 0},
    enter = function(self)
        haylen.setPaused(true)
        -- The owner makes the tween run while paused, like the menu.
        tween.to(self.glow, 0.5, {alpha = 1}, {owner = self, repeatCount = -1, loopMode = 'yoyo'})
        timer.after(1, function() scene.pop() end, {owner = self})
    end,
    exit = function(self)
        haylen.setPaused(false)
    end,
    unpaused = function(self) print('menu runs') end,
}

scene.push(level)
timer.after(1, function()
    print('enemies before the pause', level.enemies)
    scene.push(menu)
end)
-- This timer counts through the pause, while the timer of the level stood still as long as the menu was open.
timer.after(2.6, function() print('enemies after the pause', level.enemies) end, {processMode = 'always'})
```

## Autoloads

Autoloads are Lua modules that load before the first scene and live for the whole app. They hold what every scene shares, such as the player profile, the music or the connection to a server. An autoload is the table its module returns, and it receives these callbacks when it defines them, with the table as `self`.

| Callback | Called |
| --- | --- |
| `start` | Once, right after the module loads. |
| `event` | For every platform event, before the top scene. Input events arrive only when its process mode runs. |
| `fixedUpdate` | For every fixed step, after the top scene, when its process mode runs. |
| `update` | Once per frame, before the scenes update, when its process mode runs. |
| `render`, `renderUi` | Every frame, after the scenes of the same pass. |
| `stop` | When the app stops, after the scenes left, from the last autoload to the first. |

The `autoload` list of `app.json` names the modules, which load in order before `source/main.lua` runs. `haylen.autoload(module)` adds one later, and `haylen.autoload(name, module)` gives it a name of its own. The name defaults to the last part of the module in camel case, so `state.player-data` becomes `playerData`, and `haylen.autoloads.playerData` holds the table, which `require('state.player-data')` returns too. The `processMode` field of the table sets its process mode, and without one an autoload counts as `'pausable'`. Starting and stopping an autoload publishes `autoloadStarted` and `autoloadStopped` with its name.

```json
{
    "name": "Tiny Island",
    "identifier": "dev.haylen.tinyisland",
    "autoload": ["state.player-data"]
}
```

```lua
-- source/state/player-data.lua
local storage = require('haylen.storage')

local playerData = {coins = 0, playTime = 0}

function playerData:start()
    if storage.exists('profile.json') then
        self.coins = storage.readJson('profile.json').coins
    end
end

function playerData:update(dt)
    self.playTime = self.playTime + dt
end

function playerData:stop()
    storage.writeJson('profile.json', {coins = self.coins})
end

return playerData
```

```lua
-- source/state/music.lua
return {
    processMode = 'always',
    start = function(self) print('music starts') end,
}
```

```lua
local haylen = require('haylen')

local playerData = haylen.autoloads.playerData
playerData.coins = playerData.coins + 5
print('coins', playerData.coins, require('state.player-data') == playerData)

local music = haylen.autoload('state.music')
print(haylen.autoloads.music == music)
```

## Subscription scopes

A listener, a timer or a tween that outlives the thing it belongs to keeps running code for something that is gone. Owners prevent that. Every function that registers something takes an `owner` option, a table or a userdata, and ends what it registered when the owner ends: `events.on`, `signal:connect`, `socket:on`, `timer.after`, `timer.every`, every tween of `haylen.tween`, `ui.mount` and `ui.onEvent`. `scene.spawn(owner, fn)` runs a task that the owner holds, which stops for good with it, so it never resumes even when a promise it waits for settles later.

| Owner | Ends |
| --- | --- |
| A scene table | When the scene unloads. |
| A UI document | When it is unmounted. |
| Any other table or userdata, such as an autoload or a game object | When the garbage collector frees it. What it held ends at the end of that frame. |

The owner holds the functions of its listeners and the bus does not hold the owner, so a function may refer to its owner without keeping it alive. `scene.listen(owner, signalOrName, fn, options)` connects to a signal or subscribes to an event in one call, and a scene built on `scene.Scene` with `haylen.class` calls it as `self:listen(signalOrName, fn, options)`. A timer or tween that inherits its process mode follows the one of its owner too.

```lua
local haylen = require('haylen')
local events = require('haylen.events')
local scene = require('haylen.scene')
local signal = require('haylen.signal')
local timer = require('haylen.timer')

local healthChanged = signal.new('healthChanged')

local Arena = haylen.class('Arena', scene.Scene)

function Arena:enter()
    self:listen(healthChanged, function(health) print('health', health) end)
    self:listen('waveStarted', function(wave) print('wave', wave) end)
    timer.every(0.25, function() print('arena tick') end, {owner = self, count = 2})
end

scene.push(Arena())
timer.after(0.1, function()
    healthChanged:emit(80)
    events.emit('waveStarted', 1)
end)

timer.after(1, function()
    scene.pop()
    timer.after(0.1, function()
        -- The arena unloaded, so nothing it registered runs anymore.
        healthChanged:emit(40)
        events.emit('waveStarted', 2)
        print('listeners left', healthChanged.size)
    end)
end)
```

## From C++

C++ code sees the same lifecycle through the engine. `Engine::getAppState()` and the `appStateChanged` signal follow the app states, `Engine::isAppCovered()` tells whether native UI covers the app, `Engine::getScreens()` opens the screens of plugins, `Engine::getReservedInsets()` returns the edges that native views reserve in framebuffer pixels, `Engine::setPaused`, `isPaused` and the `pausedChanged` signal control the pause, and `Engine::setLifecycle` changes the lifecycle options. Timers take a `TimerScheduler::Options` with a process mode and `unscaled`, and tweens take `setProcessMode`, `setUnscaledTime` and `setFixedStep`. `Engine::getEvents()` is the event bus, `core::LifecycleEvent` names every engine event, and a `core::ConnectionScope` member ends every connection it holds when its owner is destroyed.

A `core::Scene` overrides the same hooks as a Lua scene table, `getState` returns its state, `getProcessMode` returns its mode, and `Scene::listen` ties a connection to the scene until it unloads. Its `load(engine, context)` receives a `core::SceneLoad`: the load finishes when the hook returns, unless the hook takes a `SceneLoad::Deferral` with `context.defer()` and completes or fails it later, and `context.preload(group)` holds the load until an asset group loaded. A deferral destroyed before it completed fails the load, so a load never waits for work that was dropped. `SceneManager::push` and `replace` take `SceneManager::Options` with the transition, the params as a `std::any`, a `core::LoadingView`, the loading delay, minimum time and fade-out, `unloadBeforeLoad`, the completion, which receives a `Result` with the outcome and the error, and `onError`. `SceneManager::preload` and `cancelPreload` preload scenes, and the events of the transition phases carry a `SceneManager::Transfer`.

```cpp
#include <any>
#include <memory>
#include <optional>

#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneLoad.hpp"
#include "haylen/core/SceneManager.hpp"

// A level that builds its navigation data on a worker while the transition holds the covered screen. The worker writes into shared data, so a level that unloads before the job returns is never touched, and its deferral then changes nothing.
class Level final : public haylen::core::Scene {
  public:
    void load(haylen::core::Engine& engine, haylen::core::SceneLoad& context) override {
        context.preload("level");
        auto deferral = std::make_shared<haylen::core::SceneLoad::Deferral>(context.defer());
        engine.getJobs().run([] { return 42; }, [target = cells, deferral](haylen::core::JobSystem::Result<int> result) {
            *target = result.value;
            deferral->complete();
        });
    }

    void enter(haylen::core::Engine&, const std::any& params) override {
        difficulty = std::any_cast<int>(params);
    }

  private:
    std::shared_ptr<std::optional<int>> cells = std::make_shared<std::optional<int>>();
    int difficulty = 0;
};

void openLevel(haylen::core::Engine& engine) {
    engine.getScenes().replace(std::make_shared<Level>(), {.transition = haylen::core::SceneManager::Transition::fade(0.8F), .params = 2});
}
```

