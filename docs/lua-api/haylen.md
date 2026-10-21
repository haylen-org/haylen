# 📖 `haylen`

The root module `haylen` tells an app about the engine and the device it runs on, and it controls the app as a whole. Use it to read the engine version, the platform, the graphics API and the settings of `app.json`, to read and scale the clock of the app, to pause the game, to follow the app state, to add autoloads and classes, and to quit, restart or stop the app with an error. The [Lua guide](../core/lua.md) and the [lifecycle guide](../core/lifecycle.md) explain the ideas behind it.

```lua
local haylen = require('haylen')
```

## The engine and the app

### haylen.version

The version of the engine, as a string such as `'0.0.1'`.

```lua
local haylen = require('haylen')

print('Running on Haylen ' .. haylen.version .. '.')
```

### haylen.platform

The platform that the app runs on, as a string. It is `'windows'`, `'macos'`, `'linux'`, `'ios'`, `'tvos'`, `'android'` or `'web'`. iPhones, iPads and Mac Catalyst apps report `'ios'`. The headless host that the engine tests use reports `'headless'`. The module [`haylen.system`](system.md) tells more about the device.

```lua
local haylen = require('haylen')

local touchFirst = haylen.platform == 'ios' or haylen.platform == 'android'
print('Show the touch controls: ' .. tostring(touchFirst) .. '.')
```

### haylen.graphicsApi

The graphics API that the GPU device draws through, as a string: `'metal'`, `'direct3D11'`, `'openGl'`, `'openGlEs'`, `'webGl2'`, `'webGpu'` or `'none'`. A build with the default graphics API of its platform reports these names.

| Platform | Graphics API |
| --- | --- |
| Apple platforms | `'metal'` |
| Windows | `'direct3D11'` |
| Linux | `'openGl'` |
| Android | `'openGlEs'` |
| Web | `'webGpu'` or `'webGl2'`, whichever the page picked |
| Headless host | `'none'`, since nothing draws there |

The name is the same on every backend, and [`graphics.apiName()`](graphics.md#graphicsapiname) returns it too.

```lua
local haylen = require('haylen')

print('The graphics API is ' .. haylen.graphicsApi .. '.')
local web = haylen.graphicsApi == 'webGpu' or haylen.graphicsApi == 'webGl2'
print('Runs in a browser: ' .. tostring(web) .. '.')
```

### haylen.config

A table with the settings of `app.json`, with the default filled in for every field that the file leaves out. The table is a copy, so changing it changes nothing in the engine. The [Lua guide](../core/lua.md#appjson) explains what `app.json` is for. The tables below list every field of `app.json`, which is also every key of `haylen.config`.

Every field is optional. A field marked "absent" has no value in `haylen.config` until `app.json` sets it. Colors are written as `"#RRGGBB"` or `"#AARRGGBB"`, and `haylen.config` gives them back as `#AARRGGBB`.

```json
{
    "name": "Tiny Island",
    "identifier": "com.haylen.tinyisland",
    "version": "1.0.0",
    "window": {"title": "Tiny Island", "width": 1280, "height": 720},
    "design": {"width": 1920, "height": 1080, "scaling": "expand"},
    "lifecycle": {"pauseOnFocusLoss": true},
    "clearColor": "#FF47ABA9"
}
```

```lua
local haylen = require('haylen')

local config = haylen.config
print(config.name .. ' ' .. config.version)
print('The design size is ' .. config.design.width .. ' by ' .. config.design.height .. '.')
print('Fixed updates per second: ' .. config.fixedRate .. '.')
print('Halts in the background: ' .. tostring(config.lifecycle.pauseOnBackground) .. '.')
```

#### Identity

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `"Haylen App"` | The name of the app that people see. |
| `identifier` | string | `"com.haylen.app"` | A unique id in reverse domain form. It also names the folder where [`haylen.storage`](storage.md) and [`haylen.preferences`](preferences.md) keep their files. |
| `version` | string | `"1.0.0"` | The version of the app. |
| `description` | string | The value of `name` | One sentence about the app. The Windows and Linux templates give it to the executable, the desktop entry, the AppStream metainfo and the packages. |
| `company` | string | The value of `name` | The person or company that publishes the app, in the same places. |
| `copyright` | string | `"Copyright © <company>"` | The copyright notice of the app, which the Windows executable and the installer show. |
| `website` | string | `""` | The address of the website of the app. The AppStream metainfo, the `.deb` package and the Windows installer link to it. |

#### Window

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `window.title` | string | The value of `name` | The title of the window. |
| `window.width`, `window.height` | integer | `1280`, `720` | The size of the window when it opens. |
| `window.fullscreen` | boolean | `false` | Starts in fullscreen. |
| `window.highDpi` | boolean | `true` | Uses the full density of the display for the framebuffer. |
| `window.resizable` | boolean | `true` | Whether the player may resize a desktop window. [`window.setResizable`](window.md#desktop-windows) changes it later. |
| `window.vsync` | boolean | `true` | Waits for the refresh of the display before it shows a frame. |
| `window.sampleCount` | integer | `1` | The sample count of multisampling. |
| `window.decorated` | boolean | `true` | Whether the desktop window has a title bar and a border. |
| `window.transparent` | boolean | `false` | Opens the window able to be transparent, and transparent, so the desktop shows through its transparent pixels. It also makes `clearColor` default to `"#00000000"`. Only a window that opened this way can turn transparent again after `window.setTransparent(false)`. |
| `window.alwaysOnTop` | boolean | `false` | Keeps the desktop window above normal windows. |
| `window.showInTaskbar` | boolean | `true` | Whether the window has a taskbar button. On macOS it also decides whether the app has a Dock icon and a menu bar. |
| `window.focusable` | boolean | `true` | Whether a click on the window activates the app and gives it the keyboard. |
| `window.mousePassthrough` | boolean | `false` | Lets every click pass through the window to what is behind it, until the app names regions that keep the mouse. |
| `window.position` | string or object | Centered, absent | Where the desktop window opens: `"center"`, a point such as `{"x": 40, "y": 60}` in desktop points, or an anchored placement such as `{"anchor": "bottom", "area": "work", "monitor": "primary", "offset": [0, -8], "fill": "width"}`. The [desktop guide](../platform/desktop.md#placing-the-window) describes it. `haylen.config` holds it as `{x, y}` or `{anchor, area, monitor, offset, fill}`. |
| `window.statusBar` | string | `"hidden"` | How the status bar of phones and tablets shows when the app launches: `"hidden"`, or `"auto"`, `"light"` or `"dark"` content, as [`window.setStatusBar`](window.md#windowsetstatusbarvalue) describes. |
| `window.statusBarColor` | string | Absent | The color, such as `"#FF1E2A44"`, that the engine paints behind the status bar while it shows, as [`window.setStatusBarColor`](window.md#windowsetstatusbarcolorcolor) describes. |

The desktop options of `window` change later through [`haylen.window`](window.md#desktop-windows), and the [desktop guide](../platform/desktop.md) describes them.

#### Design, orientation and timing

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `design.width`, `design.height` | number | `1920`, `1080` | The design resolution: the space of units that the app draws in and receives input in. |
| `design.scaling` | string | `"expand"` | How the design resolution fits the screen: `"fit"`, `"fill"`, `"stretch"`, `"expand"`, `"pixelPerfect"`, `"fitWidth"` or `"none"`, as [`haylen.viewport`](viewport.md) describes. |
| `orientation` | string | `"landscape"` | The orientation of a mobile app: `"landscape"`, `"portrait"` or `"any"`. The tool `haylen.py` writes it into the `Info.plist` of the Apple template and the manifest of the Android template, and `haylen_add_app` writes it into the `Info.plist` of iOS apps built with CMake, as the [distribution guide](../tools/distribution.md) describes. Desktop windows and the web ignore it. |
| `fixedRate` | number | `60` | Fixed updates per second. [`haylen.fixedStep()`](#haylenfixedstep) returns the length of one step in seconds. |
| `maxFrameTime` | number | `0.25` | The longest frame time in seconds that the clock accepts, so a stall never causes a burst of fixed steps. |
| `clearColor` | string | `"#FF000000"`, or `"#00000000"` for a transparent window | The background color. |

#### Splash screen

The splash screen is the launch screen that iOS, tvOS, Mac Catalyst and Android show while the app starts, and the loading page of the web. The tool `haylen.py` builds it from the platform templates, as the [distribution guide](../tools/distribution.md#splash-screens) describes.

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `splash.logo` | string | The Haylen symbol, `''` in `haylen.config` | The image of the launch screen, relative to `content/`, such as `"ui/splash.png"`. |
| `splash.background` | string | The value of `clearColor` | The background color of the launch screen. |
| `splash.darkBackground` | string | The value of `splash.background`, absent in `haylen.config` | The background color of the launch screen while the system uses dark colors. |
| `splash.duration` | number | `0` | The least seconds that the launch screen stays from the launch. It always lasts until the first frame of the app. |
| `splash.fadeOut` | number | `0.25` | The seconds that the launch screen takes to fade out into the app. |

#### Lifecycle

These options start the values of [`haylen.lifecycle()`](#haylenlifecycle), and the [lifecycle guide](../core/lifecycle.md#app-states) explains them.

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `lifecycle.pauseOnBackground` | boolean | `true` | Halts the app while it is in the background. |
| `lifecycle.pauseOnFocusLoss` | boolean | `false` | Halts the app while its window has no focus. |
| `lifecycle.muteOnFocusLoss` | boolean | `false` | Mutes the output while the app is not active. |

#### Audio

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `audio.iosSession` | string | `"ambient"` | The category of the audio session on iOS and tvOS: `"ambient"`, `"soloAmbient"` or `"playback"`, as the [audio guide](../audio/audio.md#sessions-and-interruptions) describes. |
| `audio.mixWithOthers` | boolean | `false` | Lets the `"playback"` session play along with other apps. The other sessions refuse `true`. |
| `audio.latency` | string | `"balanced"` | How the output trades latency for power: `"low"`, `"balanced"` or `"powerSaving"`, as [`audio.outputLatency`](audio.md#audiooutputlatency) describes. |
| `audio.limiter` | boolean | `true` | Puts a [limiter](audio.md#limiter) on the `master` bus, so the mix never clips. |

#### Input and UI

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `input.mouseAsTouch` | boolean | `false` | Makes the mouse act as a finger from the start, as [`input.setMouseAsTouch`](input.md#inputsetmouseastouchenabled) describes. |
| `input.touchAsMouse` | boolean | `false` | Makes a finger act as the mouse from the start, as [`input.setTouchAsMouse`](input.md#inputsettouchasmouseenabled) describes. |
| `input.scrollSpeed` | number | `1` | Multiplies every scroll of the wheels and the trackpads from the start, as [`input.setScrollSpeed`](input.md#inputsetscrollspeedspeed) describes. |
| `ui.scaleMode` | string | `"design"` | How large the interface draws: `"design"` or `"physical"`, as [`ui.setScaleMode`](ui.md#uisetscalemodemode) describes. |
| `ui.scale` | number | `1` | A factor from `0.25` to `4` that multiplies the size of the interface, as [`ui.setScale`](ui.md#uisetscalefactor) describes. |

#### Debug

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `debug.stats` | string | `"off"` | The debug statistics that the app starts with: `"off"`, `"compact"` for a line of each compact item in a corner, or `"full"` for the debug overlay, as [`haylen.debug`](debug.md) describes. |
| `debug.statsItems` | list of strings | `["frameRate", "drawCalls", "geometry"]` | The [items](debug.md#compact-items) that the compact statistics show: `"frameRate"`, `"drawCalls"`, `"geometry"`, `"memory"`, `"counters"`, `"physics"`, `"particles"` and `"audio"`. |
| `debug.drawings` | list of strings | `[]` | The [debug drawings](debug.md#drawings) that the app starts with, such as `["physics", "bounds"]`. |
| `debug.objectEvents` | boolean | `false` | Publishes `objectCreated` and `objectDestroyed` for every counted object, as [`haylen.debug`](debug.md#object-counts-and-events) describes. |
| `debug.console` | boolean | `false` | Turns the [developer console](debug.md#console) on outside development runs, so it opens and runs the lines of the web page. Development runs always have it. |
| `debug.safeArea` | string or object | Absent | A safe area to simulate instead of the one of the device: a device name such as `"iphoneDynamicIsland"`, or insets in window points, as [`viewport.setSafeAreaSimulation`](viewport.md#viewportsetsafeareasimulationvalue) describes. |
| `debug.fold` | string or object | Absent | A fold to simulate instead of the one of the device: a preset such as `"book"` or `"tabletop"`, or an object with `axis`, `state` and `hinge`, as [`window.setFoldSimulation`](window.md#windowsetfoldsimulationvalue) describes. |
| `debug.showSafeArea` | boolean | `false` | Shows the debug view of the safe area from the start, as [`ui.setSafeAreaVisible`](ui.md#uisetsafeareavisiblevisible) describes. |
| `debug.reload` | string | `"module"` | How a changed Lua module applies while the app runs in development. `"module"` reloads it in place with the state of the app, and `"restart"` restarts the app, as [hot reload](../core/lua.md#hot-reload) describes. |

#### Content, autoloads, native libraries and plugins

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `content.updates` | object | Absent | The server of the protected content of a release, with `baseUrl`, `channel`, `"stable"` by default, and `enabled`, `true` by default. Without it the app has no content server. The [delivery settings](../content/content.md#delivery) describe it. |
| `content.delivery` | list of objects | `[]` | The rules that let assets arrive after the start of the app. Each rule has `match`, a pattern of paths relative to `content/`, or `group`, the name of a preload group, and either `mode`, which is `"required"`, `"prefetch"` or `"onDemand"`, or `pack`, the name of a pack that a store delivers on its own. The first rule that matches an asset decides. Without rules every asset is required. In `haylen.config` a rule that names a group becomes one rule with `match` for each entry of the group. See [`haylen.content`](content.md) and the [delivery settings](../content/content.md#delivery). |
| `content.groups` | string | Absent | The group manifest that defines the groups the rules name, relative to `content/`, such as `"groups.json"`. |
| `autoload` | list of strings | `[]` | The modules that load as [autoloads](#haylenautoloadname-module) before `source/main.lua`, such as `"state.player-data"`. |
| `native` | object | `{}` | The native libraries that the app ships, by name, as the [native code guide](../platform/native.md#packaging-libraries-with-an-app) describes. The engine keeps the section as `app.json` wrote it. |
| `plugins` | object | `{}` | The [plugins](../platform/plugins.md) that the app uses, by id in `dash-case`, each with an object of the values of its parameters, such as `{"ads": {"testMode": true}}`. Every id needs `plugins/<id>/plugin.json` in the package. The field [`platform.plugin(id).config`](platform.md#plugin-handles) adds the defaults of their parameters. |

The [errors](#errors-of-appjson) section lists the message of every value that `app.json` refuses.

## Time

### haylen.elapsed()

Returns the app time: the seconds since the app started. It is the sum of every scaled frame time, so it stands still while the time scale is zero and while the app is halted.

```lua
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        local pulse = 0.5 + 0.5 * math.sin(haylen.elapsed() * 4)
        graphics2d.beginScreen()
        graphics2d.drawCircle(960, 540, 40 + 20 * pulse, '#FFFFD166')
    end,
})
```

### haylen.delta()

Returns the scaled length of the current frame in seconds. The engine takes the real frame time, cuts it to `config.maxFrameTime`, and multiplies it by the time scale. It is the same value that the `update` hook of a scene receives as `dt`, so code outside a hook can read it too.

```lua
local haylen = require('haylen')
local scene = require('haylen.scene')

local boat = {x = 0, speed = 120}

local function drift(entity)
    entity.x = entity.x + entity.speed * haylen.delta()
end

scene.push({
    update = function(self, dt)
        drift(boat)
    end,
})
```

### haylen.unscaledDelta()

Returns the real length of the current frame in seconds, cut to `config.maxFrameTime`. The time scale does not change it. Use it for things that keep moving while the game runs slowly or stands still, such as the animations of a menu.

```lua
local haylen = require('haylen')
local scene = require('haylen.scene')

local menu = {glow = 0}

scene.push({
    update = function(self, dt)
        -- Slow motion changes dt, and the menu still animates at full speed.
        menu.glow = (menu.glow + haylen.unscaledDelta()) % 1
    end,
})
```

### haylen.frameIndex()

Returns how many frames the engine has started, as an integer. It is `0` while `source/main.lua` runs and `1` during the first frame.

```lua
local haylen = require('haylen')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        -- The quests are checked on every tenth frame only.
        if haylen.frameIndex() % 10 == 0 then
            print('Checking the quests on frame ' .. haylen.frameIndex() .. '.')
        end
    end,
})
```

### haylen.timeScale()

Returns the current time scale. The value `1` is normal speed.

```lua
local haylen = require('haylen')

print('Frozen: ' .. tostring(haylen.timeScale() == 0) .. '.')
```

### haylen.setTimeScale(scale)

Sets the speed of app time. A `scale` of `0.5` plays at half speed, `0` freezes app time and values above `1` speed the app up. A negative value counts as `0`. A value that is not finite, such as `math.huge`, raises `The time scale must be a finite number.`.

The scale changes `haylen.delta()`, `haylen.elapsed()`, the `dt` of scene updates, and the timers and tweens that are not `unscaled`. It does not change `haylen.unscaledDelta()`. A frozen app still calls its updates, with a `dt` of zero. To stop only what a pause should stop, use [`haylen.setPaused`](#haylensetpausedpaused) instead.

Fixed updates keep their step length but run less or more often, so a physics world that steps in `fixedUpdate` slows down with the rest of the app. A frame runs at most 240 fixed steps. A scale so large that a frame owes more drops the rest instead of holding the frame up.

```lua
local haylen = require('haylen')
local timer = require('haylen.timer')

local function slowMotion(seconds)
    haylen.setTimeScale(0.25)
    -- Timers run on app time, so a wait of a quarter of the seconds lasts the whole seconds in real time.
    timer.after(seconds * 0.25, function() haylen.setTimeScale(1) end)
end

slowMotion(1)
```

### haylen.fixedStep()

Returns the length of one fixed step in seconds, which is `1 / config.fixedRate`. It is the same value that the `fixedUpdate` hook of a scene receives. The time scale does not change it.

```lua
local haylen = require('haylen')
local scene = require('haylen.scene')

-- A dash counted in fixed steps lasts the same number of physics steps on every device.
local dashSteps = math.ceil(0.25 / haylen.fixedStep())
local dash = {remaining = dashSteps}

scene.push({
    fixedUpdate = function(self, step)
        if dash.remaining > 0 then
            dash.remaining = dash.remaining - 1
        end
    end,
})
print('A dash lasts ' .. dashSteps .. ' fixed steps.')
```

### haylen.interpolation()

Returns how far app time has moved from the last fixed step toward the next one, from `0` up to, but not including, `1`. Draw a position between its value before and after the last fixed step with this factor. Motion then stays smooth when the frame rate and the fixed rate differ.

```lua
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local ball = {previous = 0, current = 0, speed = 300}

scene.push({
    fixedUpdate = function(self, step)
        ball.previous = ball.current
        ball.current = ball.current + ball.speed * step
    end,
    render = function(self)
        local x = ball.previous + (ball.current - ball.previous) * haylen.interpolation()
        graphics2d.beginScreen()
        graphics2d.drawCircle(x, 540, 24, '#FFFFFFFF')
    end,
})
```

## The pause and the app state

### haylen.paused()

Returns `true` while the game is paused.

```lua
local haylen = require('haylen')

print('Paused: ' .. tostring(haylen.paused()) .. '.')
```

### haylen.setPaused(paused)

Pauses the game with `true` and lets it run again with `false`. Scenes, autoloads, timers and tweens in the process mode `'pausable'` stop, and those in `'whenPaused'` start. Fixed steps stop piling up while the game is paused. The scenes that the change stops or starts get their `paused` or `unpaused` hook, and the change publishes the event `paused` or `unpaused` of [`haylen.events`](events.md#engine-events). The [lifecycle guide](../core/lifecycle.md#pause-and-process-modes) explains the process modes.

```lua
local haylen = require('haylen')
local scene = require('haylen.scene')
local timer = require('haylen.timer')

local menu = {
    processMode = 'whenPaused',
    transparent = true,
    enter = function(self) haylen.setPaused(true) end,
    exit = function(self) haylen.setPaused(false) end,
}

scene.push({update = function(self, dt) end})
scene.push(menu)
timer.after(0.5, function() print('Paused: ' .. tostring(haylen.paused()) .. '.') end, {processMode = 'always'})
```

### haylen.appState()

Returns where the app stands with the platform, as one of these names.

| State | Meaning |
| --- | --- |
| `'active'` | The app is in the foreground and has the focus. |
| `'inactive'` | The app is visible but has no focus, the system interrupted it, such as with a phone call, or native UI of a plugin covers it. |
| `'background'` | The app is hidden. |

The [lifecycle guide](../core/lifecycle.md#app-states) explains what the engine does in each state, and the events `appActive`, `appInactive` and `appBackground` report each change.

```lua
local haylen = require('haylen')

if haylen.appState() == 'active' then
    print('The player is here.')
end
```

### haylen.appCovered()

Returns `true` while native UI of a plugin covers the app. Such UI is a full screen ad, a consent form, a sign-in sheet, a purchase dialog or a [screen](platform.md#screens) of a plugin. A covered app is `'inactive'`, halted and muted, whatever the lifecycle options say. It comes back as it was once the last cover ends, as the [lifecycle guide](../core/lifecycle.md#covered-by-native-ui) explains.

```lua
local events = require('haylen.events')
local haylen = require('haylen')

events.on('appInactive', function()
    if haylen.appCovered() then
        print('A native view covers the game.')
    end
end)
```

### haylen.halted()

Returns `true` while no time passes for scenes, autoloads, timers and tweens. That happens while the lifecycle options halt the app in its current state, and while native UI covers it.

```lua
local haylen = require('haylen')

print('Halted: ' .. tostring(haylen.halted()) .. '.')
```

### haylen.lifecycle()

Returns a new table with the current lifecycle options: `pauseOnBackground`, `pauseOnFocusLoss` and `muteOnFocusLoss`. They start with the `lifecycle` section of `app.json`, whose [table](#lifecycle) says what each one does.

```lua
local haylen = require('haylen')

local options = haylen.lifecycle()
print(options.pauseOnBackground, options.pauseOnFocusLoss, options.muteOnFocusLoss) -- true false false
```

### haylen.setLifecycle(options)

Changes the lifecycle options that the table `options` names, and keeps the others. The keys are those of [`haylen.lifecycle()`](#haylenlifecycle), each a boolean. An unknown key raises `Unknown option "<key>".`.

| Key | Meaning |
| --- | --- |
| `pauseOnBackground` | Halts the app in the background. |
| `pauseOnFocusLoss` | Halts the app while its window has no focus. |
| `muteOnFocusLoss` | Mutes the output while the app is not active. |

```lua
local haylen = require('haylen')

-- A music player keeps playing when the player switches to another window.
haylen.setLifecycle({pauseOnFocusLoss = false, muteOnFocusLoss = false})
```

### haylen.networkState()

Returns whether the device reaches the network, as the platform last reported it: `'online'`, `'offline'`, or `'unknown'` until a report arrives. Browsers, Android and Apple platforms, macOS included, report the network from the start, usually by the first frame. Then they publish every change as the events `networkOnline` and `networkOffline` of [`haylen.events`](events.md#engine-events).

On Windows, on Linux and on the headless host nothing reports the network, so the state stays `'unknown'`. It also stays `'unknown'` in an Android app without the permission `android.permission.ACCESS_NETWORK_STATE`, as [Android permissions](system.md#android-permissions) describes.

```lua
local events = require('haylen.events')
local haylen = require('haylen')

local function show(state)
    print('The network is ' .. state .. '.')
end

show(haylen.networkState())
events.on('networkOnline', function() show('online') end)
events.on('networkOffline', function() show('offline') end)
```

## Autoloads and classes

### haylen.autoloads

A table that maps the name of every autoload to its table. It is empty when the app has no autoloads. [`haylen.autoload`](#haylenautoloadname-module) adds to it.

```lua
local haylen = require('haylen')

for name in pairs(haylen.autoloads) do
    print('The app has the autoload ' .. name .. '.')
end
```

### haylen.autoload(name, module)

Loads the Lua module `module` as an autoload, calls its `start` hook and returns its table. An autoload is a module whose table lives for the whole app and receives the hooks that the [lifecycle guide](../core/lifecycle.md#autoloads) lists. The table is kept in `haylen.autoloads` under `name`.

With one argument, `haylen.autoload(module)`, the name is the last part of the module in camel case. So `haylen.autoload('state.player-data')` names it `playerData`. The autoloads that the `autoload` list of `app.json` names load in its order before `source/main.lua` runs.

Adding the same module under the same name again returns the autoload that exists. So a module that adds an autoload at its top level runs again unchanged when it [reloads](hotReload.md). What `start` registers without an `owner` then lives as long as the autoload, instead of ending with that reload. Starting an autoload publishes the event `autoloadStarted` with its name.

The call raises an error when the name is empty, when another module already has the name, when the module returns something other than a table, and when the table is already another autoload. The [errors](#errors) section lists the messages. An error inside the module or its `start` shows on the error screen with the stack of the module.

```lua
-- The file `source/systems/music.lua`.
local music = {processMode = 'always', track = 'day'}

function music:start()
    print('The music plays the ' .. self.track .. ' track.')
end

return music
```

```lua
local haylen = require('haylen')

local music = haylen.autoload('soundtrack', 'systems.music')
print(haylen.autoloads.soundtrack == music, require('systems.music') == music) -- true true
```

### haylen.class(name, parent, ...)

Builds a class: a table of methods that creates objects which share those methods. Use it for the types of an app, such as units, items or scenes. The [Lua guide](../core/lua.md#classes) explains when classes help.

- `name` is a string, the name of the class.
- `parent` is optional. When given, it must be another class, and the new class inherits its methods. The new class also copies the metamethods that the parent has at that moment, such as `__eq`, `__lt` or `__add`, because Lua looks metamethods up without inheritance.
- The other arguments are mixins: tables whose fields are copied into the class, unless the class already defines them.

The call returns the class table. Define methods on it with `function Class:method()`. Calling the class, or its `new` function, creates an object and calls its `init` method with the arguments. A call without a name raises `bad argument #1 to 'class' (string expected, got <type>)`, and a `parent` that is not a class raises `bad argument #2 to 'class' (class expected, got <type>)`.

A class has these members.

| Member | Meaning |
| --- | --- |
| `Class(...)`, `Class.new(...)` | Creates an object, calls `object:init(...)` when the class has `init`, and returns the object. |
| `Class.name` | The name of the class. |
| `Class.super` | The parent class, or `nil`. An overriding method calls the method it overrides with `Class.super.method(self, ...)`. |
| `Class.mixins` | A table whose keys are the mixins of the class. |
| `object:is(Class)` | Returns whether the object is an instance of `Class`, of a class that inherits from it, or of a class that includes it as a mixin. It works on classes too, such as `Hero:is(Unit)`. |
| `Class:include(mixin)` | Adds a mixin later and returns the class. A mixin with a function `included(class)` hears about it, and that function is not copied. A mixin that is not a table raises `bad argument #1 to 'include' (table expected, got <type>)`. |
| `tostring(Class)`, `tostring(object)` | Read `class <name>` and `<name>: <address>`. |

The table `scene.Scene` of [`haylen.scene`](scene.md#scene) is a class to build scenes on.

```lua
local haylen = require('haylen')

local Walker = {
    walk = function(self) return self.name .. ' walks.' end,
    included = function(class) print(class.name .. ' can walk.') end,
}

local Unit = haylen.class('Unit')

function Unit:init(name, health)
    self.name, self.health = name, health
end

function Unit:describe()
    return self.name .. ' has ' .. self.health .. ' health.'
end

function Unit.__eq(a, b)
    return a.name == b.name
end

local Hero = haylen.class('Hero', Unit, Walker)

function Hero:init(name)
    Hero.super.init(self, name, 100)
    self.level = 1
end

function Hero:describe()
    return 'The hero ' .. Hero.super.describe(self)
end

local ana = Hero('Ana')
print(ana:describe(), ana:walk()) -- The hero Ana has 100 health. Ana walks.
print(ana:is(Hero), ana:is(Unit), ana:is(Walker), Hero:is(Unit)) -- true true true true
print(ana == Hero.new('Ana'), tostring(Hero)) -- true class Hero
```

## Quitting, launching and restarting

### haylen.canQuit()

Returns whether [`haylen.quit`](#haylenquitoptions) closes the app. It does on macOS, Windows, Linux, Android and the headless player. iPhones, iPads, Apple TVs, Mac Catalyst windows and web pages close only through the system or the browser. There the function returns `false`, and a quit only stops the app. An app that [another app launched](#haylenlaunchpath) is the exception: its quit goes back to that app everywhere. Show a Quit button only where this returns `true`.

```lua
local haylen = require('haylen')
local ui = require('haylen.ui')

if haylen.canQuit() then
    ui.mount(ui.button{text = 'Quit', anchor = 'topRight', margin = 16, onClick = function() haylen.quit() end})
end
```

### haylen.quit(options)

Stops the app and asks the platform to close it. No more frames run. On Android the activity of the app finishes. Where [`haylen.canQuit`](#haylencanquit) returns `false`, the app only stops.

The table `options` is optional and holds the exit status of the process.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `status` | integer | `0` | The exit status of the process, from `0` to `255`, where `0` means success. |

An unknown key raises `Unknown option "<key>".`. A `status` that is not such an integer raises `The option "status" of "quit" is invalid: <reason>.`. The reason is `expected a non-negative integer` below `0`, `integer out of range` above `255`, and `number has no integer representation` for a fraction.

On macOS, Windows and Linux the process exits with the status, in the player `haylen` as in a built app. So does the [headless player](../tools/testing.md#the-test-project), which exits with `1` instead when an error was reported. A process whose window closes, such as through its close button or Command+Q, exits with `0`, and a process whose engine could not start exits with `1`. iOS, tvOS, Mac Catalyst, Android and the web have no exit status for an app, so they ignore it.

```lua
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {{name = 'quit', type = 'button', bindings = {'key:escape', 'button:back'}}}})

scene.push({
    update = function(self, dt)
        if input.pressed('quit') then
            haylen.quit()
        end
    end,
})
```

```lua
local haylen = require('haylen')
local log = require('haylen.log')

-- A check that runs on the headless player, such as in continuous integration, fails the job with the status 1 when a level has no exit.
local levels = {{name = 'forest', exit = {x = 12, y = 4}}, {name = 'cave'}}
local broken = 0
for _, level in ipairs(levels) do
    if not level.exit then
        log.error(string.format('The level "%s" has no exit.', level.name))
        broken = broken + 1
    end
end
haylen.quit({status = broken == 0 and 0 or 1})
```

### haylen.launch(path)

Runs another app package in place of this app, once the current frame ends. The string `path` names a folder or a zip file of the file system that holds an `app.json`. The launched app starts with a new engine and a new Lua state. Use it for an app that opens other apps, such as one that downloads a package and plays it.

When the launched app quits, this app starts again from its own package. The launched app quits through [`haylen.quit`](#haylenquitoptions), the Close button of its error screen, or the back button at the root of the app on Android and Apple TV. A launched app can launch another app in turn. It plays as it is, without the files that a development server sends to the app that launched it.

A path with nothing at it raises `No app package is at "<path>". Launch a folder or a zip file that holds "app.json".`. A package that cannot open, such as one without a valid `app.json`, shows the error screen in place of the launched app.

```lua
local async = require('async')
local fs = require('fs')
local haylen = require('haylen')
local http = require('http')
local storage = require('haylen.storage')

-- Downloads a zipped package into the files of the app and plays it.
async.spawn(function()
    local response, failure = http.client.get('https://example.com/apps/demo.zip'):await()
    if not response then
        print('The download failed: ' .. failure)
        return
    end
    local path = storage.root() .. '/demo.zip'
    fs.writeFile(path, response.body):await()
    haylen.launch(path)
end)
```

### haylen.launched()

Returns `true` when another app [launched](#haylenlaunchpath) this one, so quitting it starts that app again. Use it to offer a way back only when something launched the app.

```lua
local haylen = require('haylen')
local ui = require('haylen.ui')

if haylen.launched() then
    ui.mount(ui.button{text = 'Back', anchor = 'topLeft', margin = 16, onClick = function() haylen.quit() end})
end
```

### haylen.requestRestart()

Starts the app again from its package once the current frame ends, the way a hot reload of an edited `app.json` does. The scenes, the autoloads and the plugins stop, and a new Lua state runs `source/main.lua`. What belongs to the platform outlives the restart: the covers of native UI, the edges that native views reserve, the streams of plugins, and a [screen](platform.md#screens) that shows. The end of that screen reaches the restarted app as `screenRestored`.

```lua
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {{name = 'restart', type = 'button', bindings = {'key:f5'}}}})

-- The key F5 starts the app again, which runs every script of the package anew.
scene.push({
    update = function(self, dt)
        if input.pressed('restart') then
            haylen.requestRestart()
        end
    end,
})
```

## The error screen

An error that nobody catches stops the app and shows the error screen, as the [Lua guide](../core/lua.md#errors-and-stack-traces) explains. These functions report errors and leave the error screen.

### haylen.reportError(message)

Stops the app with an error of your own. The engine logs the report of the error at the error level, stops the updates of the app and the drawing of its scenes, and shows the error screen. The screen shows the string `message` with the stack of the code that called `reportError`. It stays until the app restarts or, in a [recoverable](#haylensetrecoverablerecoverable) app, until the player goes back to the app.

The engine keeps only the first error while the screen shows, so later calls do nothing. Every error, reported or not, publishes the event `appError` of [`haylen.events`](events.md#engine-events) at the end of its frame.

```lua
local haylen = require('haylen')
local storage = require('haylen.storage')

local ok, failure = pcall(storage.readJson, 'levels/custom.json')
if not ok and storage.exists('levels/custom.json') then
    haylen.reportError('The custom level is corrupted: ' .. failure)
end
```

### haylen.recoverable()

Returns whether the error screen offers to go back to the app, as [`haylen.setRecoverable`](#haylensetrecoverablerecoverable) set it. An app starts not recoverable.

```lua
local haylen = require('haylen')

print('Recoverable: ' .. tostring(haylen.recoverable()) .. '.')
```

### haylen.setRecoverable(recoverable)

With `true`, the error screen offers to go back to the app, next to copying the report and restarting the app. Use it in an app that can return to a safe screen after an error, such as a menu or a list of levels.

On a recoverable error screen, these go back to the app: Escape, the east button of a gamepad, the back button of Android, the Menu button of a TV remote, and the button `Back to the app`, which has the focus from the start. Going back publishes the event `appRecovered` of [`haylen.events`](events.md#engine-events) with the error, at the start of the next frame and before the app updates again. The app then runs on from where it was, so its listeners put it in order first, such as by leaving the scene that failed.

```lua
local events = require('haylen.events')
local haylen = require('haylen')
local scene = require('haylen.scene')

haylen.setRecoverable(true)
events.on('appRecovered', function(error)
    print('Back from the error: ' .. error.message)
    scene.clear()
    scene.push(require('scenes.menu')())
end)
```

### haylen.recover()

Goes back to the app from the error screen at the start of the next frame, the way the button `Back to the app` does. It works whether the app is recoverable or not, so it suits an app that runs on by itself after an error, such as one that lists the failures of a test run. It publishes `appRecovered` before the app updates again. It does nothing while no error stops the app.

```lua
local events = require('haylen.events')
local haylen = require('haylen')

events.on('appError', function(error)
    print('Failed: ' .. error.message)
    haylen.recover()
end)
```

## Errors

These messages are raised by the functions of the module.

| Message | Cause |
| --- | --- |
| `The time scale must be a finite number.` | The scale of `haylen.setTimeScale` is not finite. |
| `Unknown option "<key>".` | The table of `haylen.setLifecycle` or `haylen.quit` has a key that the function does not take. |
| `The option "status" of "quit" is invalid: <reason>.` | The `status` of `haylen.quit` is not an integer from `0` to `255`. |
| `No app package is at "<path>". Launch a folder or a zip file that holds "app.json".` | Nothing is at the path of `haylen.launch`. |
| `An autoload needs a name.` | The name of `haylen.autoload` is empty. |
| `An autoload named "<name>" already exists.` | Another module is already the autoload of that name. |
| `The autoload module "<module>" must return a table.` | The module of `haylen.autoload` returned something other than a table. |
| `The module "<module>" is already the autoload "<name>".` | The table of the module is already an autoload under another name. |
| `bad argument #1 to 'class' (string expected, got <type>)` | The name of `haylen.class` is not a string. |
| `bad argument #2 to 'class' (class expected, got <type>)` | The parent of `haylen.class` is not a class. |
| `bad argument #1 to 'include' (table expected, got <type>)` | The mixin of `Class:include` is not a table. |

### Errors of app.json

The engine checks `app.json` before any Lua runs. A package whose `app.json` fails to load shows `The app could not be loaded.` and the reason on the screen. In development the app still runs, so saving a fixed `app.json` restarts it.

| Message | Cause |
| --- | --- |
| `Unknown key "<key>" in "app.json".` | The file has a field that the engine does not know. A field of a section names its section, such as `Unknown key "widht" in the "window" section of "app.json".` |
| `The value of "<key>" in "app.json" has the wrong type.` | A value is not of the type that the tables above give. |
| `The value of "<key>" in "app.json" must be positive.` | One of `window.width`, `window.height`, `window.sampleCount`, `design.width`, `design.height`, `fixedRate` and `maxFrameTime` is `0` or less. |
| `The value of "<key>" in "app.json" must be a number of at least 0.` | The `splash.duration` or `splash.fadeOut` is negative or not finite. |
| `The "<key>" value "<value>" in "app.json" is not a color such as "#RRGGBB" or "#AARRGGBB".` | The `clearColor`, `window.statusBarColor`, `splash.background` or `splash.darkBackground` is not a color. |
| `The "window.statusBar" value "<value>" in "app.json" is unknown. It is "hidden", "auto", "light" or "dark".` | The status bar has another value. |
| `The "window.position" in "app.json" is invalid. <reason>` | The placement of the window is wrong, for the reason that follows. |
| `The "design.scaling" value "<value>" in "app.json" is unknown. It is "fit", "fill", "stretch", "expand", "pixelPerfect", "fitWidth" or "none".` | The scaling policy has another value. |
| `The orientation "<value>" in "app.json" is unknown. It is "landscape", "portrait" or "any".` | The orientation has another value. |
| `The "audio.iosSession" value "<value>" in "app.json" is unknown. It is "ambient", "soloAmbient" or "playback".` | The audio session has another value. |
| `The "audio.mixWithOthers" option in "app.json" needs the "iosSession" value "playback".` | The option `mixWithOthers` is `true` with another session. |
| `The "audio.latency" value "<value>" in "app.json" is unknown. It is "low", "balanced" or "powerSaving".` | The latency has another value. |
| `The "input.scrollSpeed" value in "app.json" must be a positive number.` | The scroll speed is not a positive finite number. |
| `The "ui.scaleMode" value "<value>" in "app.json" is unknown. It is "design" or "physical".` | The scale mode has another value. |
| `The "ui.scale" value in "app.json" must be from 0.25 to 4.` | The scale of the interface is out of its range. |
| `The "debug.stats" value "<value>" in "app.json" is unknown. It is "off", "compact" or "full".` | The debug statistics have another value. |
| `The "debug.statsItems" item "<item>" in "app.json" is unknown. It is "frameRate", "drawCalls", "geometry", "memory", "counters", "physics", "particles" or "audio".` | An item of the compact statistics is unknown. |
| `The "debug.drawings" list in "app.json" has an empty drawing name.` | A debug drawing has an empty name. |
| `The "debug.safeArea" in "app.json" is invalid. <reason>` | The simulated safe area is wrong, for the reason that follows. |
| `The "debug.fold" in "app.json" is invalid. <reason>` | The simulated fold is wrong, for the reason that follows. |
| `The "debug.reload" option must be "module" or "restart".` | The reload mode has another value. |
| `The "autoload" list in "app.json" has an empty module name.` | An autoload has an empty name. |
| `The "native" section of "app.json" must be an object of libraries.` | The section `native` is not an object. |
| `The "plugins" section of "app.json" must be an object of plugins by id.` | The section `plugins` is not an object. |
| `The plugin id "<id>" in "app.json" is not in "dash-case", such as "cloud-analytics".` | A plugin id has other characters. |
| `The plugin "<id>" in "app.json" must have an object of parameter values.` | The values of a plugin are not an object. |
| `The plugin "<id>" in "app.json" has no "plugins/<id>/plugin.json" in the package.` | The package lacks the manifest of a plugin that `app.json` lists. |

The [delivery settings](../content/content.md#delivery) list the messages of the `content` section.
