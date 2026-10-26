# 🔄 Lifecycle

This guide explains what happens to an app from its start to its stop. It covers the order in which the engine calls scenes, autoloads and plugins, the states that the platform moves the app through, how the game pause and the process modes decide what runs, how autoloads live next to the scenes, and how listeners, timers and tweens end together with the thing that owns them. Read it once you have a first scene running, as the [Lua guide](lua.md#scenes) shows. The engine publishes each moment of this lifecycle as an event of [`haylen.events`](../lua-api/events.md), with the same names in C++ and Lua, and the [list of engine events](../lua-api/events.md#engine-events) gives their values.

## Start, frames and stop

The engine starts an app in a fixed order.

1. The runtime reads `app.json` and creates the window and the engine. Every built-in plugin starts and installs its Lua modules, and each one publishes `pluginStarted`.
2. The autoloads that `app.json` lists load in order. Each one gets its `start` hook and publishes `autoloadStarted`.
3. The file `source/main.lua` runs. It usually adds listeners and pushes the first scene.
4. The engine publishes `appStarted`, and the first frame begins.

Then every frame runs the same steps. The [architecture guide](architecture.md#the-frame) lists them in full detail.

1. The engine gathers what happened since the last frame:
    - It publishes the changes of the window and of the gamepads.
    - Assets that finished decoding in the background create their GPU resources, within the upload budget of the frame.
    - Varn's event loop runs. Promises settle, and the tasks of `async` and of scenes resume.
    - Native replies of the platform bridge arrive.
2. Fixed steps run for the time that passed. For each step, the top scene gets `fixedUpdate`, fixed-step tweens advance, and the autoloads get `fixedUpdate`.
3. The frame updates. Timers and tweens advance, and the autoloads get `update`. Then scene loads that finished or failed settle, the pending scene change moves through its phases, the loading view gets `update`, and the top scene gets `update`.
4. The frame draws:
    - The visible scenes get `render`, and then the loading view and the autoloads get `render`.
    - The visible scenes get `renderUi`, and then the loading view, the autoloads and the GUIs get `renderUi`. A GUI that a scene owns draws only while its scene shows.
    - During a scene transition, the scenes before and after the change each draw into an image of their own, with the GUIs they own. The loading view, the autoloads and the GUIs of no scene draw with the current scenes, and the transition effect draws the two images on the screen.
5. Events queued with `events.post` and deferred signal listeners run.

Platform events, such as keys, touches and focus changes, arrive between frames. The autoloads get them first and then the top scene, as the [scene reference](../lua-api/scene.md#events) describes.

When the app stops, the engine does these steps in order:

1. It publishes `appStopping`.
2. It removes every scene from the top down, together with a scene that is still loading and the preloaded scenes.
3. It stops the autoloads from the last to the first.
4. It stops the plugins in reverse order.

A restart stops the app the same way, even in the middle of a load. Then it creates a new engine with a fresh Lua state that runs the whole start again. A hot reload of an edited `app.json` is such a restart. A hot reload of an edited Lua module runs none of this: the module reloads in place, and the modules that require it, the autoloads, the scenes of the stack and the objects of its classes hear the `reloaded` hook, as [`haylen.hotReload`](../lua-api/hotReload.md) describes.

An app restarts in the state that the platform holds it in:

- An app that restarts while the platform keeps it in the background hears `appBackground` right after `appStarted`, and draws nothing until it comes back. That happens, for example, when a timer restarts it under the activity of a screen on Android.
- An app that restarts while its window lacks the focus, or while an interruption of the system holds it, starts `'inactive'` the same way. That happens, for example, for a hot reload saved from an editor in front of the window.

On Android the app also stops this way when its activity closes: through `haylen.quit()`, through the back button of the root screen, or when the player removes it from the recent apps. The activity then finishes normally.

```lua
local events = require('haylen.events')
local scene = require('haylen.scene')

for _, name in ipairs({'appStarted', 'sceneLoaded', 'sceneEntered', 'sceneEnterTransitionFinished', 'appStopping'}) do
    events.on(name, function() print('The event ' .. name .. ' fired.') end)
end

scene.push({
    enter = function(self) print('The scene entered.') end,
    update = function(self, dt)
        if not self.announced then
            self.announced = true
            print('The scene got its first update.')
        end
    end,
})
```

### Launching another app

An app can launch another app package with [`haylen.launch(path)`](../lua-api/haylen.md#haylenlaunchpath), such as one that it downloaded. Once the frame ends, the app stops the way a restart does, and the launched package starts with a new engine. The launched app may stop through `haylen.quit()`, through the Close button of its error screen, or through the back button at its root on Android and Apple TV. Then the launcher starts again from its own package, as a new run of the whole start. The function [`haylen.launched()`](../lua-api/haylen.md#haylenlaunched) tells an app which way it started.

## App states

The platform moves the app between three states. The function `haylen.appState()` returns the current one, and each change publishes an event.

| State | Meaning | Event |
| --- | --- | --- |
| `'active'` | In the foreground with the focus. | `appActive` |
| `'inactive'` | Still visible, but the window lost the focus, the system interrupted the app, such as with a phone call, or native UI of a plugin covers it. | `appInactive` |
| `'background'` | Hidden, such as a minimized window, another app on a phone or a hidden browser tab. | `appBackground` |

Each platform tells the engine about these changes in its own way.

- **iOS, iPadOS, tvOS and Mac Catalyst.** The scene of the app tells the engine. A scene that resigns active makes the app `'inactive'`, such as under the app switcher, Control Center, the Notification Center, a phone call or Siri, or behind another app in front of a Mac Catalyst window. A scene in the background makes it `'background'`, and a scene that becomes active again makes it `'active'`.
- **The web.** The page tells the engine. The window of the page losing the focus makes the app `'inactive'`. The focus moving between the canvas and other elements of the page, such as the buttons of a plugin dialog, changes nothing. A hidden tab (`visibilitychange`) sends the app to `'background'`, and a visible one brings it back. A page that goes away (`pagehide`) starts saving the files of [`haylen.storage`](../lua-api/storage.md) once more. IndexedDB finishes that save when the browser lets it, since a page cannot hold its unload back. So the `appBackground` of the hidden tab before is what makes the files durable.
- **Android.** The window of the activity losing the focus to a dialog, a message or the notification shade makes the app `'inactive'`. That also holds when the app comes back from the background while such UI still has the focus. An Android app also holds the audio focus while it is in the foreground. Another app that takes the audio focus, for a phone call or an alarm, interrupts the app. The app then stays `'inactive'` until the focus comes back, even when its window has the focus. An app that comes to the foreground during a call waits, interrupted, until the system grants it the focus. A lasting loss, such as music that the player starts in another app, keeps the app interrupted until it comes back to the foreground.

The [events by platform](#events-by-platform) list where every event comes from.

### What the engine does on each change

The engine takes care of what every app needs when the state changes.

- **Leaving `'active'`** releases every held key, mouse button and touch, and every virtual button and stick of the on-screen controls. So nothing stays pressed when the player comes back. The window losing the focus releases them too, also in an app that something else made `'inactive'` before, since their releases go to the window that takes the focus.
- **Going to `'background'`** stops rendering, so the app does no GPU work, and it suspends the audio device. Once the listeners of `appBackground` have run, the files of [`haylen.storage`](../lua-api/storage.md) become durable, and preferences with unsaved changes are saved, including what those listeners changed. That matters because the platform may end an app in the background without warning. An app saves its own progress in a listener of `appBackground`. The [resume state](../lua-api/storage.md#resume-state) that it keeps with `storage.setResumeState` reaches the disk at the same moment, so a launch after the system ended the app finds where the player was.
- **Coming back from `'background'`** resumes the audio. The first frame after a halt takes no time, so timers, tweens and physics do not jump by the time the app was away.
- **An interruption of the audio by the system**, such as a phone call, an alarm, Siri or another Android app that takes the audio focus, pauses every voice and publishes `audioInterrupted`. Its end resumes them and publishes `audioResumed` once the app is active. A change of the audio output publishes `audioRouteChanged`, as the [audio guide](../audio/audio.md#sessions-and-interruptions) explains.

### Halting the app

The lifecycle options decide whether the app keeps running while it is not active. The `lifecycle` section of `app.json` sets them, and `haylen.setLifecycle` changes them while the app runs. A halted app lets no time pass: scenes, autoloads, timers, tweens and fixed steps stand still, and input is held back. Varn's event loop still delivers network replies and the results of `async` tasks. The function `haylen.halted()` tells whether the app is halted right now.

| Option | Default | Meaning |
| --- | --- | --- |
| `pauseOnBackground` | `true` | Halts the app in the background. |
| `pauseOnFocusLoss` | `false` | Halts the app while it is inactive. Tiny Island turns it on, so the run stands still whenever the window loses the focus. It also opens its pause menu on `appInactive` and `appBackground`, so the run stays paused when the player comes back. |
| `muteOnFocusLoss` | `false` | Mutes the output while the app is not active. The mute applies after every bus, so the mutes of the buses stay what the app and the player chose, and the app reads and saves them unchanged. |

```json
{
    "name": "Tiny Island",
    "identifier": "com.haylen.tinyisland",
    "lifecycle": {"pauseOnFocusLoss": true, "muteOnFocusLoss": true}
}
```

```lua
local events = require('haylen.events')
local haylen = require('haylen')
local storage = require('haylen.storage')

local progress = {day = 3, wood = 12}

events.on('appBackground', function()
    storage.writeJson('data/progress.json', progress)
end)

events.on('appActive', function()
    print('Welcome back. The app is ' .. haylen.appState() .. '.')
end)

-- A music player keeps playing when the window loses the focus, and a strategy game may keep simulating in the background.
haylen.setLifecycle({muteOnFocusLoss = false})
print(haylen.lifecycle().pauseOnFocusLoss, haylen.halted())
```

### Frames while the app is away

The event loop and the platform bridge run inside the frames of the app. So nothing arrives while the platform runs no frames at all. Each platform runs frames differently while the app is away.

- **Android.** A dialog, a message or the notification shade only takes the focus of the window, and the frames keep running under it. The activity pauses in the background and under every activity over the app, such as a document picker, a permission prompt or a screen that is an activity of its own. The app is then in the background and draws nothing, but it keeps ticking ten times a second, for as long as the process lives. Each tick is a frame of the app in the background. So Varn's event loop runs, Varn timers fire, `call:cancel()` and timeouts take effect, and network callbacks, `async` results and the replies and events of the platform bridge arrive while the other activity shows. An app whose `pauseOnBackground` is `false` also updates at that rate.
- **iOS, iPadOS, tvOS and Mac Catalyst.** The frames go on while the scene is inactive, such as under the app switcher or Control Center, and they stop while it is in the background.
- **The web.** Browsers stop the frames of a hidden tab.

Where the frames stop, these things wait and arrive in the first frame after the app comes back: the replies and events of the platform bridge, the answers of native dialogs and screens, Varn timers, network callbacks and `async` results.

Android also freezes the processes of cached apps. Freezing stops every thread of the app until it comes back, its ticks, the transfers of the network and the threads of native plugins included, so a connection may time out meanwhile. An app that the person left, such as with Home, stays the previous app for about a minute. Then it becomes cached, and Android freezes it some seconds later.

The [screens](../lua-api/platform.md#screens) of plugins follow the same rules on Android:

- A screen that is an activity of the app itself, such as the confirm screen of the demo plugin of the test project, keeps the process in the foreground, which Android never freezes.
- A screen of another app that the app started, such as the document picker, a browser page or a purchase flow, makes the app the previous app, which freezes about a minute later. Until then the app keeps ticking under the screen, and the answers of its threads and of the network reach it as they come.
- The answer of the screen itself always arrives, since Android wakes the process to deliver it.

### Memory, quitting and device changes

The engine publishes other moments of the app and the device as events too.

- **Low memory.** A warning of the system publishes `appLowMemory`, after the engine has dropped every asset that nothing holds anymore. That is the moment to unload preload groups and caches that the app can rebuild.
- **Quit requests.** A request to close the window publishes `appQuitRequested`. It also comes from Quit in the menu of a Mac app, Command+Q and the Dock, as the [desktop guide](../platform/desktop.md#menus-and-quitting) describes. The function `haylen.quit()` ends the app.
- **The on-screen keyboard.** The events `keyboardShown` and `keyboardHidden` follow it. The UI already avoids it by lifting the focused text field above it, as the [text input guide](../ui/text-input.md) explains.
- **The network.** The events `networkOnline` and `networkOffline` follow the network where browsers, Android and Apple platforms, macOS included, report it. The first report publishes the state that the app starts in, usually on its first frame, and later reports publish each change. The function `haylen.networkState()` returns the last state at any time.
- **Theme and battery.** The events `systemThemeChanged` and `batteryChanged` follow the light or dark colors of the system and the battery, once per change, as [`haylen.system`](../lua-api/system.md#events) describes.
- **Accessibility.** The event `accessibilitySettingsChanged` follows the text scale, reduced motion, contrast, transparency and screen reader of the system, as [`haylen.accessibility`](../lua-api/accessibility.md#events) describes.
- **Orientation and folds.** The event `windowOrientationChanged` follows the screen, whose orientation `window.orientation()` reads and `window.lockOrientation` locks, as [`haylen.window`](../lua-api/window.md) describes. The event `windowFoldChanged` follows the fold of foldable and dual-screen devices, its posture and the segments of the window on each side of it, as [folds and dual screens](../lua-api/window.md#folds-and-dual-screens) describes.
- **The safe area.** The event `windowSafeAreaChanged` follows the safe area of [`haylen.viewport`](../lua-api/viewport.md#viewportsaferect). It is the safe area of the device, widened edge by edge by the edges that native views of plugins reserve, such as a banner at the bottom. So UI anchored to the safe area moves out of their way on its own.
- **Desktop windows.** On desktops, `windowMoved` follows the position of the window and `windowMonitorsChanged` follows the monitors and their work areas, as the [desktop guide](../platform/desktop.md) describes.

```lua
local events = require('haylen.events')

events.on('networkOffline', function()
    print('The game plays offline until the network comes back.')
end)

events.on('keyboardShown', function(frame)
    print('The keyboard covers the screen from y = ' .. frame.y .. '.')
end)
```

### Covered by native UI

Plugins show native UI over the app, such as a full screen ad, a consent form, a sign-in sheet or a purchase dialog. While it shows, they tell the engine that it covers the app. Covered content must never play under native UI. So while any cover lasts, the app is `'inactive'` and halted, and its output is muted. That holds whatever the lifecycle options say and whatever the app unmutes meanwhile, and `haylen.appCovered()` returns `true`.

Covers nest. When the last one ends, the app returns to the state it would have without them: `'active'` when its window has the focus and nothing else interrupts it, with the mute that the player chose and without a jump of the clock. An app that goes to the background while covered goes there as usual. It comes back `'inactive'` while the cover lasts, and it stays in the background when the cover ends there.

The engine takes the cover at the start of every frame and publishes the state changes as usual. So an app pauses its game on `appInactive` the same way for a cover as for a phone call. A halted app would draw the same frame again and again. Instead, the engine draws one frame once the cover begins, which shows what the app drew for `appInactive`, such as a pause menu, and keeps it on the screen. It draws the frame again only when the window changes size or the app comes back from the background, since the frame on the screen no longer fits then. It draws every frame again once the cover ends.

The cover belongs to the platform, so an app that restarts under native UI, such as after a hot reload, becomes covered on its first frame. The [plugin guide](../platform/plugins.md#engine-services) shows how native code covers the app.

The [screens](../lua-api/platform.md#screens) of plugins, such as a paywall or a sign-in page, cover the app the same way:

- The engine covers the app at the start of the frame after it asked for a screen, and only then hands the screen to the platform. So the app is `'inactive'`, halted and muted before the screen shows, and the cover ends when the screen ends.
- A screen that the app asks for while it is `'inactive'` in the foreground, such as right in the answer of a dialog before its window has the focus again, waits until the app is `'active'`. Then it covers the app the same way.
- While an opaque screen shows, the engine draws nothing at all, and the last frame stays on the screen.
- The screen belongs to the process as well. An app that restarts under it, such as after `haylen.requestRestart()`, starts covered. The end of the screen reaches it as the retained event `screenRestored` of the plugin, with the state that the earlier app gave.
- The same event carries the end of a screen whose process ended while it showed. Examples are a web page that left for a redirect and loaded again, and an Android app that the system ended in the background, as the [plugin guide](../platform/plugins.md#plugin-screens) describes.

```lua
local events = require('haylen.events')
local haylen = require('haylen')

events.on('appInactive', function()
    if haylen.appCovered() then
        print('An ad covers the game, which stands still until it closes.')
    end
end)

events.on('appActive', function()
    print('Back in the game.')
end)
```

## Events by platform

Every event of the engine reaches the app on every platform where its source exists. The tables below say where each event of the platform comes from, where it never fires, and why. Native UI of plugins makes the app `'inactive'` on every platform while it [covers the app](#covered-by-native-ui). The test `PLT-006` of the test project lists these events for the platform it runs on and marks each one as it fires.

The other events come from the engine itself and fire the same way everywhere: `appStarted`, `appStopping`, `appError`, `appRecovered`, `paused`, `unpaused`, every scene event, `pluginStarted`, `pluginStopped`, `autoloadStarted`, `autoloadStopped`, `guiMounted`, `guiUnmounted`, the asset events, `moduleReloaded`, `objectCreated`, `objectDestroyed` and the WebSocket events.

### macOS

| Event | Source |
| --- | --- |
| `appActive`, `appInactive` | The window becoming and stopping being the key window, such as when another app or window comes to the front or the app hides with Command+H. |
| `appBackground` | The window minimized into the Dock. The app comes back when the window is restored. |
| `appLowMemory` | The memory pressure of the system, which a dispatch source follows, at the warning and at the critical level. |
| `appQuitRequested` | The close button of the window, Quit in the menu, Command+Q, the Dock and logging out, as the [desktop guide](../platform/desktop.md#menus-and-quitting) describes. |
| `windowResized`, `windowFocusGained`, `windowFocusLost`, `windowFullscreenChanged` | The window, resized by the player or the app, its key state and its full screen mode. |
| `windowOrientationChanged`, `windowFoldChanged` | Never, because a desktop window counts as landscape and no Mac folds. |
| `windowSafeAreaChanged` | The camera housing of a Mac with a notch, for a full screen or borderless window that reaches it, the native views of plugins and the simulation. |
| `windowMoved`, `windowMonitorsChanged` | The moves of the window and the changes of the displays and their work areas, through AppKit. |
| `keyboardShown`, `keyboardHidden` | Never, because a Mac types with its keyboard and has no keyboard on the screen. |
| `networkOnline`, `networkOffline` | The path monitor of the Network framework. |
| `audioInterrupted`, `audioResumed` | Never, because macOS does not interrupt the audio of apps. |
| `audioRouteChanged` | A change of the default output device of Core Audio, such as headphones plugged in. |
| `systemThemeChanged`, `batteryChanged` | The appearance of the app and the power sources of the Mac, as [`haylen.system`](../lua-api/system.md#platforms) describes. |
| `accessibilitySettingsChanged` | The display options of the accessibility settings and the state of VoiceOver, as [`haylen.accessibility`](../lua-api/accessibility.md#platforms) describes. |
| `gamepadConnected`, `gamepadDisconnected` | The gamepad API of SDL, which reads GameController and HID controllers. |

### Windows

| Event | Source |
| --- | --- |
| `appActive`, `appInactive` | The window gaining and losing the keyboard focus. |
| `appBackground` | The window minimized into the taskbar. The app comes back when the window is restored. |
| `appLowMemory` | Never, because the engine follows no memory notification of Windows. |
| `appQuitRequested` | The close button of the window and Alt+F4. |
| `windowResized`, `windowFocusGained`, `windowFocusLost`, `windowFullscreenChanged` | The window, resized by the player or the app, its focus and its full screen mode. |
| `windowOrientationChanged`, `windowFoldChanged` | Never, because a desktop window counts as landscape and Windows offers desktop apps no API for the fold of a device. |
| `windowSafeAreaChanged` | Only the native views of plugins and the simulation, because nothing of the system covers a window. |
| `windowMoved`, `windowMonitorsChanged` | The moves of the window and the changes of the displays, their settings and the work area, through the messages of the window. |
| `keyboardShown`, `keyboardHidden` | Never, because the engine follows no keyboard on the screen on Windows. |
| `networkOnline`, `networkOffline` | Never, because the engine follows no network state on Windows, and `haylen.networkState()` stays `'unknown'`. |
| `audioInterrupted`, `audioResumed` | Never, because Windows does not interrupt the audio of apps. |
| `audioRouteChanged` | A change of the default output device of WASAPI. |
| `systemThemeChanged`, `batteryChanged` | The app theme of the personalization settings and the power status of the system, as [`haylen.system`](../lua-api/system.md#platforms) describes. |
| `accessibilitySettingsChanged` | The accessibility settings and the transparency effects of the personalization settings, as [`haylen.accessibility`](../lua-api/accessibility.md#platforms) describes. |
| `gamepadConnected`, `gamepadDisconnected` | The gamepad API of SDL, which reads XInput, Windows.Gaming.Input, raw input and HID controllers. |

### Linux

| Event | Source |
| --- | --- |
| `appActive`, `appInactive` | The window gaining and losing the keyboard focus. |
| `appBackground` | The window minimized, which X11 reports as hidden. The app comes back when the window is restored. |
| `appLowMemory` | Never, because the engine follows no memory pressure of Linux. |
| `appQuitRequested` | The close button of the window and the close shortcut of the window manager, usually Alt+F4. |
| `windowResized`, `windowFocusGained`, `windowFocusLost`, `windowFullscreenChanged` | The window, resized by the player or the app, its focus and its full screen mode. |
| `windowOrientationChanged`, `windowFoldChanged` | Never, because a desktop window counts as landscape and Linux desktops report no folds to windows. |
| `windowSafeAreaChanged` | Only the native views of plugins and the simulation, because nothing of the system covers a window. |
| `windowMoved`, `windowMonitorsChanged` | The moves of the window and the changes of the monitors and the work area, through X11. |
| `keyboardShown`, `keyboardHidden` | Never, because the engine follows no keyboard on the screen on Linux. |
| `networkOnline`, `networkOffline` | Never, because the engine follows no network state on Linux, and `haylen.networkState()` stays `'unknown'`. |
| `audioInterrupted`, `audioResumed` | Never, because Linux does not interrupt the audio of apps. |
| `audioRouteChanged` | A stream that PulseAudio, or PipeWire through it, moves to another output. |
| `systemThemeChanged`, `batteryChanged` | The color scheme of the settings portal and the power supplies of the system, as [`haylen.system`](../lua-api/system.md#platforms) describes. |
| `accessibilitySettingsChanged` | The appearance settings of the settings portal and the text scaling factor of the GNOME interface settings, as [`haylen.accessibility`](../lua-api/accessibility.md#platforms) describes. |
| `gamepadConnected`, `gamepadDisconnected` | The gamepad API of SDL, which reads the event devices under `/dev/input` and HID controllers. |

### iOS and iPadOS

| Event | Source |
| --- | --- |
| `appActive`, `appInactive` | The scene of the app becoming and resigning active, such as under the app switcher, Control Center, the Notification Center, a phone call or Siri. An interruption of the audio session alone pauses the voices without changing the state, because iOS does not always report its end. |
| `appBackground` | The scene in the background, such as after Home, another app or the lock of the device. |
| `appLowMemory` | The memory warnings of UIKit. |
| `appQuitRequested` | Never, because iOS apps never quit on request: the system ends them in the background. |
| `windowResized` | The view of the app, such as a rotation or a new size of an iPad window. |
| `windowFocusGained`, `windowFocusLost` | The scene becoming and resigning active. |
| `windowFullscreenChanged` | Never, because the app always fills its window. |
| `windowOrientationChanged` | The interface orientation of the scene. |
| `windowFoldChanged` | Never, because no iPhone or iPad folds. |
| `windowSafeAreaChanged` | The safe area insets of the view, such as a rotation that moves the notch or the Dynamic Island, and the native views of plugins. |
| `windowMoved`, `windowMonitorsChanged` | Never, because the window has no desktop. |
| `keyboardShown`, `keyboardHidden` | The keyboard notifications of UIKit while a text field edits. |
| `networkOnline`, `networkOffline` | The path monitor of the Network framework. |
| `audioInterrupted`, `audioResumed` | The interruptions of the audio session, such as a phone call, an alarm or Siri. |
| `audioRouteChanged` | The route changes of the audio session, such as headphones unplugged. |
| `systemThemeChanged`, `batteryChanged` | The trait collection of the app and the battery of the device, as [`haylen.system`](../lua-api/system.md#platforms) describes. |
| `accessibilitySettingsChanged` | The settings of `UIAccessibility` and the preferred content size of the app, as [`haylen.accessibility`](../lua-api/accessibility.md#platforms) describes. |
| `gamepadConnected`, `gamepadDisconnected` | GameController. |

### tvOS

| Event | Source |
| --- | --- |
| `appActive`, `appInactive` | The scene of the app becoming and resigning active, such as under the Control Center of the TV. An interruption of the audio session alone pauses the voices without changing the state, because tvOS does not always report its end. |
| `appBackground` | The scene in the background, such as after the TV button or another app. |
| `appLowMemory` | The memory warnings of UIKit. |
| `appQuitRequested` | Never, because tvOS apps never quit on request. |
| `windowResized` | The view of the app. |
| `windowFocusGained`, `windowFocusLost` | The scene becoming and resigning active. |
| `windowFullscreenChanged`, `windowOrientationChanged`, `windowFoldChanged` | Never, because the app always fills the TV in landscape. |
| `windowSafeAreaChanged` | The safe area insets of the view, the overscan margins of the TV, and the native views of plugins. |
| `windowMoved`, `windowMonitorsChanged` | Never, because the window has no desktop. |
| `keyboardShown`, `keyboardHidden` | Never, because the keyboard of tvOS is a screen of the system over the app, which covers no part of it. |
| `networkOnline`, `networkOffline` | The path monitor of the Network framework. |
| `audioInterrupted`, `audioResumed` | The interruptions of the audio session. |
| `audioRouteChanged` | The route changes of the audio session. |
| `systemThemeChanged`, `batteryChanged` | The trait collection of the app. `batteryChanged` never fires, since a TV runs on mains power. |
| `accessibilitySettingsChanged` | The settings of `UIAccessibility` and the preferred content size of the app, as [`haylen.accessibility`](../lua-api/accessibility.md#platforms) describes. |
| `gamepadConnected`, `gamepadDisconnected` | GameController, the Siri Remote included. |

### Mac Catalyst

| Event | Source |
| --- | --- |
| `appActive`, `appInactive` | The scene of the app becoming and resigning active, such as when another app comes to the front. |
| `appBackground` | The scene in the background, such as a minimized window. |
| `appLowMemory` | The memory warnings of UIKit. |
| `appQuitRequested` | Never, because Mac Catalyst ends the app on Quit and Command+Q without asking it. The engine then stops the app through its [lifecycle](#start-frames-and-stop). |
| `windowResized` | The window, resized by the player. |
| `windowFocusGained`, `windowFocusLost` | The scene becoming and resigning active. |
| `windowFullscreenChanged`, `windowOrientationChanged`, `windowFoldChanged` | Never, because the engine runs the window as the iPad window it is, which counts as landscape. |
| `windowSafeAreaChanged` | The safe area insets of the view and the native views of plugins. |
| `windowMoved`, `windowMonitorsChanged` | Never, because UIKit places Mac Catalyst windows on the desktop without telling apps where. |
| `keyboardShown`, `keyboardHidden` | Never, because a Mac has no keyboard on the screen. |
| `networkOnline`, `networkOffline` | The path monitor of the Network framework. |
| `audioInterrupted`, `audioResumed` | The interruptions of the audio session, which the Mac rarely sends. |
| `audioRouteChanged` | The route changes of the audio session. |
| `systemThemeChanged`, `batteryChanged` | The trait collection of the app and the power sources of the Mac, as [`haylen.system`](../lua-api/system.md#platforms) describes. |
| `accessibilitySettingsChanged` | The settings of `UIAccessibility` and the preferred content size of the app, as [`haylen.accessibility`](../lua-api/accessibility.md#platforms) describes. |
| `gamepadConnected`, `gamepadDisconnected` | GameController. |

### Android

| Event | Source |
| --- | --- |
| `appActive`, `appInactive` | The window of the activity gaining and losing the focus, such as under a dialog, a message, the notification shade or a permission prompt, and the audio focus taken by another app. |
| `appBackground` | The activity paused, in the background and under every activity over the app, such as a document picker or a screen of a plugin. |
| `appLowMemory` | The trim memory callbacks of the activity, at every level but the one that only says the interface is hidden. |
| `appQuitRequested` | Never from the system, because back on the root screen and the recent apps close the activity, which stops the app through the [lifecycle](#start-frames-and-stop). |
| `windowResized` | The surface of the activity, such as a rotation, the split screen or a foldable that folds. |
| `windowFocusGained`, `windowFocusLost` | The window of the activity gaining and losing the focus. |
| `windowFullscreenChanged` | Never, because the app always fills its window. |
| `windowOrientationChanged` | The configuration of the activity. |
| `windowFoldChanged` | Jetpack WindowManager, on foldable and dual-screen devices, as [folds and dual screens](../lua-api/window.md#folds-and-dual-screens) describes. |
| `windowSafeAreaChanged` | The insets of the cutout, of the status bar while it shows and of the system bars that stay visible, and the native views of plugins. |
| `windowMoved`, `windowMonitorsChanged` | Never, because the window has no desktop. |
| `keyboardShown`, `keyboardHidden` | The insets of the input method while a text field edits. |
| `networkOnline`, `networkOffline` | The connectivity manager, with the permission `ACCESS_NETWORK_STATE`. |
| `audioInterrupted`, `audioResumed` | The audio focus, which another app takes for a call, an alarm or its own sound. |
| `audioRouteChanged` | The AAudio stream that the system moves to another output. |
| `systemThemeChanged`, `batteryChanged` | The night mode of the configuration and the battery broadcast, as [`haylen.system`](../lua-api/system.md#platforms) describes. |
| `accessibilitySettingsChanged` | The font scale of the configuration, the animator duration scale, the contrast of the UI mode and touch exploration, as [`haylen.accessibility`](../lua-api/accessibility.md#platforms) describes. |
| `gamepadConnected`, `gamepadDisconnected` | The input devices that Android adds and removes, with the name of each controller, and a controller whose first event comes before Android tells of it. The remote of an Android TV reports the directional pad as keys, so it is no gamepad. |

### Web

| Event | Source |
| --- | --- |
| `appActive`, `appInactive` | The window of the page gaining and losing the focus. |
| `appBackground` | A hidden tab, through `visibilitychange`, and `Module.haylen.pause()` of the page. |
| `appLowMemory` | Never, because browsers tell pages nothing about memory pressure. |
| `appQuitRequested` | Never, because a page that closes cannot wait for the app. |
| `windowResized` | The size of the canvas, which a `ResizeObserver` follows. |
| `windowFocusGained`, `windowFocusLost` | The window of the page gaining and losing the focus. |
| `windowFullscreenChanged` | The Fullscreen API. |
| `windowOrientationChanged` | The Screen Orientation API, or the `orientation` media query. |
| `windowFoldChanged` | The Viewport Segments and Device Posture APIs where the browser offers them, as [folds and dual screens](../lua-api/window.md#folds-and-dual-screens) describes. |
| `windowSafeAreaChanged` | The part of the CSS `env(safe-area-inset-*)` insets that covers the canvas, and the native views of plugins. |
| `windowMoved`, `windowMonitorsChanged` | Never, because a page has no place on a desktop. |
| `keyboardShown`, `keyboardHidden` | The `visualViewport` of the page while a text field edits on a phone or a tablet. |
| `networkOnline`, `networkOffline` | The `online` and `offline` events of the browser. |
| `audioInterrupted`, `audioResumed` | Never, because browsers suspend the audio of the page without telling it why. The runtime resumes it at the next tap, click or key, as the [audio guide](../audio/audio.md#sessions-and-interruptions) describes. |
| `audioRouteChanged` | Never, because the audio output of the page follows the output of the browser without telling the page. |
| `systemThemeChanged`, `batteryChanged` | The `prefers-color-scheme` media query and the Battery Status API, as [`haylen.system`](../lua-api/system.md#platforms) describes. |
| `accessibilitySettingsChanged` | The media queries of reduced motion, contrast and transparency and the default font size of the browser, as [`haylen.accessibility`](../lua-api/accessibility.md#platforms) describes. |
| `gamepadConnected`, `gamepadDisconnected` | The gamepad API of SDL, which reads the Gamepad API of the browser, once a button of the gamepad is pressed. |

## Assets, connections and objects

Assets, sockets and engine objects announce their life as events as well.

- **Assets.** The event `assetLoaded` follows an asset into the cache of [`haylen.assets`](../lua-api/assets.md). The event `assetUnloaded` follows the moment its last holder lets it go, whether a script, a sprite or the last preload group that held it. The event `assetReloaded` follows a changed file that hot reload applied to a live asset. The event `assetChanged` follows a changed file that left behind a live asset of a type that cannot update in place. The asset manager learns about a release wherever it happens, even inside the garbage collector or on a worker thread. So these events are queued and arrive at the end of the frame, in the order they happened.
- **WebSockets.** Every WebSocket of [`haylen.net`](../lua-api/net.md) publishes `webSocketConnected` when it opens and `webSocketDisconnected` when an open connection ends. A socket opened with reconnection publishes `webSocketReconnecting` each time it plans an attempt, with the attempt number and the wait before it, until a connection opens again or it gives up.
- **Objects.** The debug statistics count objects by type: every userdata type exported to Lua, and engine resources such as textures, fonts, sounds, bodies, emitters, GUIs and tweens. While object events are on, every creation and destruction also publishes `objectCreated` or `objectDestroyed` with the type name, queued for the end of the frame. Turn them on with `debug.setObjectEvents(true)` or `debug.objectEvents` in `app.json`. They are off by default, because a busy app creates many objects every frame.

```lua
local debugging = require('haylen.debug')
local events = require('haylen.events')

events.on('assetUnloaded', function(asset)
    print('Freed the ' .. asset.type .. ' ' .. asset.path .. '.')
end)

events.on('webSocketDisconnected', function(socket)
    print('Lost ' .. socket.url .. ' with code ' .. socket.code .. '.')
end)

debugging.setObjectEvents(true)
events.on('objectCreated', function(object)
    if object.type == 'haylen.Sprite' then
        print('A sprite was created.')
    end
end)
```

## Scene lifecycle

Scenes live on a stack. Every change of the stack, `push`, `replace`, `pop`, `popTo` and `popToRoot`, runs through one pipeline:

1. The transition takes the current scene off the screen.
2. The next scene loads in the background in its `load` hook, with progress and, if you want, behind a loading view.
3. The next scene enters.
4. The transition shows it.

The [`haylen.scene`](../lua-api/scene.md) reference lists the operations, the options and the hooks in full.

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
| `unloaded` | It released what it loaded: right after `exit`, after a failed load, or when its preload was cancelled. | `unload` | `sceneUnloaded` |

A scene that unloaded can load again, since the same scene table may be pushed again later. Every scene that started loading unloads exactly once. So `unload` pairs with `load` the way `exit` pairs with `enter`, and it also releases a load that stopped half way.

### Effects that cover the screen

The fade and the other effects that show one scene at a time work in three phases. Their out phase hides the current scene. Then they keep the screen covered while the next scene loads. Their in phase shows the next scene.

- The next scene of a `replace` loads only once the replaced scene unloaded at full cover, for a lower peak of memory.
- The next scene of a `push` starts loading when the out phase starts, and so does the next scene of a `replace` whose `unloadBeforeLoad` is `false`. Such a replaced scene stays until the next scene loaded, and exits right before the next scene enters.

This timeline shows a `replace` with a fade and a loading view.

```text
scene.replace(game, {duration = 1, loading = view, loadingDelay = 0.2, minimumLoadingTime = 0.5})

  time   menu (on top)              game (next)                  screen                          events
  ----   -------------------------  ---------------------------  ------------------------------  -----------------------------------------
  0      exitTransitionStarted                                    out: the menu fades out          sceneExitTransitionStarted menu
                                                                                                   transitionOutStarted
  ...    update, render                                           the menu, fading
  0.5    exit                                                     full cover                       transitionOutFinished, sceneExited menu
         unload                     load starts                                                    sceneUnloaded menu, sceneLoading game
  ...                               load runs, :await()           the covered frame
  0.7                               progress reaches the view     view:enter, then view:update     (the view appears after loadingDelay)
  ...                               loaded                        the view stays its minimum time  sceneLoaded game
  1.2                                                             the view fades out
  1.45                              enter                         view:exit, in starts             sceneEntered game
                                                                                                   transitionInStarted
  ...                               update, render                the game fades in
  1.95                              enterTransitionFinished       the change ended                 transitionInFinished
                                                                                                   sceneEnterTransitionFinished game
```

While the screen stays covered, the effect draws at its switch point with the last frame of the outgoing scenes and no scene at all. So the transition itself is the loading screen when the change has no loading view, and waiting costs almost no GPU work. The drawing of the plugins, the autoloads and the GUIs of no scene goes with the current scenes: into the outgoing image during the out phase, over the covered frame while the screen stays covered, and into the incoming image during the in phase.

Once the load is done and the loading view stayed its minimum time, the view fades out into the covered frame over `loadingFadeOut` seconds, `0.25` by default. So the in phase starts from the covered frame without a cut. During the fade, the view, the autoloads and the GUIs draw over the covered frame into an image of their own. The effect blends that image over the covered frame with the opacity that is left. A `loadingFadeOut` of `0` takes the view away at once. A view over the current scenes goes away at once too, since the scenes under it keep running. That is the case before an effect that shows both scenes, or in a change without an effect.

A scene that a push covers receives `pause` only at full cover, and `resume` when the pushed scene pops. Popped scenes exit and unload at full cover, while the scene below them receives `resume`.

### Effects that show both scenes

Crossfades, slides, pushes and the other effects that show both scenes at once load first. The current scenes stay on the screen and keep running meanwhile, with the loading view over them once its delay passed. A change without a transition loads the same way, and then it switches within one update in this order: `exitTransitionStarted` on the top scene, the exits and unloads, `pause` or `resume`, `enter` and `enterTransitionFinished`. A change without an effect publishes no transition events.

```text
scene.replace(game, {effect = 'slideIn', duration = 0.5})

  time   menu (on top)              game (next)                  screen                          events
  ----   -------------------------  ---------------------------  ------------------------------  -----------------------------------------
  0      update, render             load starts                  the menu                         sceneLoading game
  ...    update, render             load runs, :await()          the menu, and the view if any
  0.3                               loaded                                                         sceneLoaded game
         exitTransitionStarted      enter                        the effect starts                sceneExitTransitionStarted menu
                                                                                                   sceneEntered game, transitionOutStarted, transitionInStarted
  ...    render (outgoing image)    update, render (incoming)    both scenes, each in its image
  0.8    exit, unload               enterTransitionFinished      exit point at the end            sceneExited menu, sceneUnloaded menu
                                                                                                   transitionOutFinished, transitionInFinished
                                                                                                   sceneEnterTransitionFinished game
```

The scenes that leave the stack exit at the exit point of the effect, in the update that reaches it and before that frame draws. So nothing draws them after they exit. The GUIs that a scene owns go into its image, so the GUIs of the menu leave with the menu and never show over the game, and they take no input while they leave. A scene that a push covers keeps its GUIs mounted, and they show again when the scene is on top.

### Loading, preloading and errors

The `load` hook runs as a task that the scene owns. So it can wait on promises with `:await()`, and it can also return a promise. Its load context, a [`SceneLoad`](../lua-api/scene.md#sceneload), does three things:

- `context:progress(value, message)` reports progress.
- `context.params` holds the `params` of the change.
- `context:preload(groups)` preloads asset groups, and their progress folds into the progress of the load.

The asset manager decodes on the worker pools and creates GPU resources within the upload budget of each frame. So the frame never stalls, and the in phase stays smooth right after the load.

The function `scene.preload(scene, params)` starts a load in the background without changing the stack. A later change that takes the scene is then instant, or waits only for the rest of its load. Preloaded groups whose assets download later bring them first, as the [loading guide](../content/loading.md#scenes) describes.

A load fails when `load` raises, when the promise it returned rejects, or when an asset of a group it preloads fails. A failed load unloads the scene, publishes `sceneLoadFailed`, and ends the change without the scene: its promise rejects and `onComplete` receives `false`. Then:

- When the scene that was on top is still alive, the change keeps it. An effect that covers the screen shows it again, and the scene receives `enterTransitionFinished`. The failure goes to the `onError` of the change, or to the log.
- When that scene already unloaded, the failure goes to `onError`, which may send the app to another scene, and to the error screen when there is no `onError`.

### Queues, the pause and the background

- **The queue.** Changes requested during a change wait for it, in the order they were requested. That includes changes that its hooks and its `onError` request. The function `scene.clear` drops them all and resolves their promises with `false`.
- **The game pause.** Transitions, loads and loading views run on real time. So they go on at full speed while the game is paused or the time scale is zero. The top scene still updates only when its process mode runs, and a covered scene never updates.
- **Input.** By default a change holds input back from its start to its end, the load included, so neither the scenes, the action map nor the focus navigation of the UI see a key, a button or a touch. A key or a button that is still held when the change ends counts as pressed again only after it is released. So the press that opened a menu never closes it. The option `blockInput = false` lets input through.
- **The background and focus loss.** A halted app lets no time pass, so a change stands still in its phase, and its loading delay and minimum time wait too. Its load goes on while the platform runs frames, since Varn's event loop still runs, promises still settle and the worker pools still decode. But an app in the background creates no GPU resources, so decoded assets wait for it to come back. Once the app runs again, the finished load settles in the next update, and the change goes on.
- **Restart and stop.** The engine removes every scene. The scene that is still loading and the preloaded scenes unload too, their tasks are cancelled, and the pending asset callbacks are dropped. So a hot reload in the middle of a load leaves nothing behind.

Everything that a scene creates with itself as the owner belongs to it and ends when it unloads. So no timer, tween, listener, GUI or task of a scene ever runs once the scene is gone, as [subscription scopes](#subscription-scopes) explains.

```lua
local async = require('async')
local events = require('haylen.events')
local scene = require('haylen.scene')

local function tracked(name)
    return {
        load = function(self) print('The ' .. name .. ' loads.') async.sleep(100):await() end,
        enter = function() print('The ' .. name .. ' enters.') end,
        exit = function() print('The ' .. name .. ' exits.') end,
        unload = function() print('The ' .. name .. ' unloads.') end,
        pause = function() print('The ' .. name .. ' pauses.') end,
        resume = function() print('The ' .. name .. ' resumes.') end,
        exitTransitionStarted = function() print('The exit transition of the ' .. name .. ' started.') end,
        enterTransitionFinished = function() print('The enter transition of the ' .. name .. ' finished.') end,
    }
end

for _, name in ipairs({'transitionOutStarted', 'transitionOutFinished', 'transitionInStarted', 'transitionInFinished'}) do
    events.on(name, function(transfer) print('The event ' .. name .. ' fired.') end)
end

scene.push(tracked('map'))
async.spawn(function()
    local shown = scene.push(tracked('inventory'), {duration = 0.4, color = '#FF000000'}):await()
    print('The inventory shows: ' .. tostring(shown) .. '.')
    scene.pop({duration = 0.2}):await()
    print('Back on the map.')
end)
```

## Pause and process modes

The function `haylen.setPaused(true)` pauses the game, and `haylen.paused()` reads whether it is paused. The pause belongs to the game and has nothing to do with the app states:

- A halted app stops everything.
- A paused game stops only what the pause should stop, and keeps running what should run while paused, such as a pause menu.

Changing the pause publishes `paused` or `unpaused`. Fixed steps do not pile up while the game is paused.

Scenes, autoloads, timers, tweens and sounds each have a process mode. The mode decides whether they run in the current pause state. Sounds take the mode of their audio bus unless they have their own. The buses start with effects and ambience pausable, while music and interface sounds keep playing, as the [audio guide](../audio/audio.md#pause-and-process-modes) explains.

| Mode | Runs while the game plays | Runs while the game is paused |
| --- | --- | --- |
| `'inherit'` | Like its parent. | Like its parent. |
| `'pausable'` | Yes | No |
| `'whenPaused'` | No | Yes |
| `'always'` | Yes | Yes |
| `'disabled'` | No | No |

The mode `'inherit'` is the default, and what "the parent" is depends on the thing:

- A scene inherits the mode of the scene below it on the stack, and resolves to `'pausable'` at the bottom. So a settings screen pushed over a pause menu runs while the game is paused, like the menu.
- A timer or a tween with an owner inherits the mode of its owner: the resolved mode of a scene, or the `processMode` field of any other table, such as an autoload. It reads that mode again every frame. So changing the `processMode` of the owner, or of a scene below it, changes what its timers and tweens do at once.
- A timer or a tween without an owner counts as `'pausable'`.

The mode decides whether the top scene and each autoload get `update`, `fixedUpdate` and input events. Scenes and autoloads keep drawing in every mode, so a paused level stays on the screen under its menu. Other events, such as focus changes, reach them in every mode.

When the pause changes, every scene on the stack whose mode stops or starts it hears about it, from the bottom of the stack up. A scene gets `paused` when the change stops it, and `unpaused` when the change lets it run again. So a scene in `'whenPaused'` gets `unpaused` when the game pauses, and scenes in `'always'` or `'disabled'` hear nothing.

Timers and tweens also choose their clock. By default they count scaled time, which `haylen.setTimeScale` slows down or freezes. With `unscaled = true` they count real time. The pause and the time scale are separate: a time scale of zero freezes scaled time but keeps calling updates with a `dt` of zero, while the pause stops what it pauses. GUIs of [`haylen.ui`](../lua-api/ui.md) keep real time, so menus animate and answer input while the game is paused.

```lua
local haylen = require('haylen')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local tween = require('haylen.tween')

local level = {
    enemies = 0,
    paused = function(self) print('The level stopped.') end,
    unpaused = function(self) print('The level runs again.') end,
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
    unpaused = function(self) print('The menu runs.') end,
}

scene.push(level)
timer.after(1, function()
    print('Enemies before the pause: ' .. level.enemies .. '.')
    scene.push(menu)
end)
-- This timer counts through the pause, while the timer of the level stood still as long as the menu was open.
timer.after(2.6, function() print('Enemies after the pause: ' .. level.enemies .. '.') end, {processMode = 'always'})
```

## Autoloads

Autoloads are Lua modules that load before the first scene and live for the whole app. They hold what every scene shares, such as the profile of the player, the music or the connection to a server. An autoload is the table that its module returns. It receives these hooks when it defines them, with the table as `self`.

| Hook | When it runs |
| --- | --- |
| `start` | Once, right after the module loads. |
| `event` | For every platform event, before the top scene. Input events arrive only when its process mode runs. |
| `fixedUpdate` | For every fixed step, after the top scene, when its process mode runs. |
| `update` | Once per frame, before the scenes update, when its process mode runs. |
| `render`, `renderUi` | Every frame, after the scenes of the same pass. |
| `stop` | When the app stops, after the scenes left, from the last autoload to the first. Every autoload stops even when an earlier one fails, and the first failure shows afterwards. |

How autoloads are added and named:

- The `autoload` list of `app.json` names the modules, which load in its order before `source/main.lua` runs.
- The function `haylen.autoload(module)` adds one later, and `haylen.autoload(name, module)` gives it a name of its own.
- The name defaults to the last part of the module in camel case, so `state.player-data` becomes `playerData`. Then `haylen.autoloads.playerData` holds the table, which `require('state.player-data')` returns too.
- The `processMode` field of the table sets its process mode. Without one, an autoload counts as `'pausable'`.
- Starting and stopping an autoload publishes `autoloadStarted` and `autoloadStopped` with its name.

In a release, the autoloads load after the content that the app needs to start is on the device. They are Lua bytecode of the build of the app, which never downloads, as the [loading guide](../content/loading.md#autoloads) describes.

```json
{
    "name": "Tiny Island",
    "identifier": "com.haylen.tinyisland",
    "autoload": ["state.player-data"]
}
```

```lua
-- The file `source/state/player-data.lua`.
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
-- The file `source/state/music.lua`.
return {
    processMode = 'always',
    start = function(self) print('The music starts.') end,
}
```

```lua
local haylen = require('haylen')

local playerData = haylen.autoloads.playerData
playerData.coins = playerData.coins + 5
print(playerData.coins, require('state.player-data') == playerData)

local music = haylen.autoload('state.music')
print(haylen.autoloads.music == music)
```

## Subscription scopes

A listener, a timer or a tween that outlives the thing it belongs to keeps running code for something that is gone. Owners prevent that. Every function that registers something takes an `owner` option, a table or a userdata, and ends what it registered when the owner ends. These functions take an owner: `events.on`, `signal:connect`, `net.connectWebSocket`, `socket:on`, `timer.after`, `timer.every`, every tween of `haylen.tween`, `jobs.spawn`, `ui.mount`, `ui.onEvent`, `debug.addMonitor`, and the progress function of `assets.preload`.

The function `scene.spawn(owner, fn)` runs a task that the owner holds. The task stops for good with its owner, so it never resumes, even when a promise it waits for settles later.

| Owner | Ends |
| --- | --- |
| A scene table | When the scene unloads. |
| A GUI | When it is unmounted. |
| Any other table or userdata, such as an autoload or a game object | When the garbage collector frees it. What it held ends at the end of that frame. |

The owner holds the functions of its listeners, and the event bus does not hold the owner. So a function may refer to its owner without keeping it alive.

The function `scene.listen(owner, signalOrName, fn, options)` connects to a signal or listens to an event in one call. A scene built on `scene.Scene` with `haylen.class` calls it as `self:listen(signalOrName, fn, options)`. A timer or a tween that inherits its process mode follows the mode of its owner too.

Without an `owner`, these functions take a default owner. In development, the default is the module whose top level is running, so a [reload](lua.md#hot-reload) of the module ends what it registered. Anywhere else, what they register has no owner and lasts until the app removes it.

```lua
local events = require('haylen.events')
local haylen = require('haylen')
local scene = require('haylen.scene')
local signal = require('haylen.signal')
local timer = require('haylen.timer')

local healthChanged = signal.new('healthChanged')

local Arena = haylen.class('Arena', scene.Scene)

function Arena:enter()
    self:listen(healthChanged, function(health) print('The health is ' .. health .. '.') end)
    self:listen('waveStarted', function(wave) print('Wave ' .. wave .. ' started.') end)
    timer.every(0.25, function() print('The arena ticks.') end, {owner = self, count = 2})
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
        print('Listeners left: ' .. healthChanged.size .. '.')
    end)
end)
```

## From C++

C++ code sees the same lifecycle through the engine.

| C++ | What it does |
| --- | --- |
| `Engine::getAppState()` and the signal `appStateChanged` | Follow the app states. |
| `Engine::isAppCovered()` | Tells whether native UI covers the app. |
| `Engine::getScreens()` | Opens the screens of plugins. |
| `Engine::getReservedInsets()` | Returns the edges that native views reserve, in framebuffer pixels. |
| `Engine::setPaused`, `Engine::isPaused` and the signal `pausedChanged` | Control the pause. |
| `Engine::setLifecycle` | Changes the lifecycle options. |
| `TimerScheduler::Options` | Gives a timer a process mode and `unscaled`. |
| `setProcessMode`, `setUnscaledTime` and `setFixedStep` of a tween | Give a tween its process mode, its clock and its step. |
| `Engine::getEvents()` | The event bus. `core::LifecycleEvent` names every engine event. |
| `core::ConnectionScope` | A member that ends every connection it holds when its owner is destroyed. |

A `core::Scene` overrides the same hooks as a Lua scene table. Its `getState` returns its state, its `getProcessMode` returns its mode, and `Scene::listen` ties a connection to the scene until it unloads.

Its `load(engine, context)` receives a `core::SceneLoad`:

- The load finishes when the hook returns, unless the hook takes a `SceneLoad::Deferral` with `context.defer()`. It then completes or fails the deferral later.
- A deferral destroyed before it completed fails the load with `The scene load was dropped before it finished.`. So a load never waits for work that was dropped. A hook that throws while it holds a deferral fails the load with its own error instead.
- `context.preload(group)` holds the load until an asset group loaded.

`SceneManager::push` and `replace` take `SceneManager::Options` with the transition, the params as a `std::any`, a `core::LoadingView`, the loading delay, minimum time and fade-out, `unloadBeforeLoad`, the completion, which receives a `Result` with the outcome and the error, and `onError`. `SceneManager::preload` and `cancelPreload` preload scenes, and the events of the transition phases carry a `SceneManager::Transfer`.

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
