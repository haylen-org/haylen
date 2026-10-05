# haylen

The root `haylen` module holds information about the running engine and the app clock. It exposes the engine version, the platform, the graphics backend and the resolved `app.json` configuration, and it reads frame times, scales app time, quits the app or stops it with an error screen. Use it to adapt to the platform, to pause or slow down gameplay and to read timing outside scene callbacks.

```lua
local haylen = require('haylen')
```

## Fields

### haylen.version

The engine version as a string, such as `'0.0.1'`.

```lua
local haylen = require('haylen')

print('running on Haylen ' .. haylen.version)
```

### haylen.platform

The platform the app runs on, one of `'windows'`, `'macos'`, `'linux'`, `'ios'`, `'tvos'`, `'android'` or `'web'`. The headless host used by the engine tests reports `'headless'`.

```lua
local haylen = require('haylen')

local touchFirst = haylen.platform == 'ios' or haylen.platform == 'android'
print('show touch controls:', touchFirst)
```

### haylen.backend

The graphics backend in use, one of `'metal'`, `'d3d11'`, `'glcore'`, `'gles3'`, `'webgpu'` or `'dummy'`. WebGL2 and Android report `'gles3'`, and the headless host reports `'dummy'`.

```lua
local haylen = require('haylen')

print('graphics backend: ' .. haylen.backend)
```

### haylen.config

A table with the configuration read from `app.json`, with defaults filled in for every missing field. It is a copy, so changing it has no effect on the engine.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `'Haylen App'` | Display name of the app. |
| `identifier` | string | `'dev.haylen.app'` | Unique id, which also names the storage folder. |
| `version` | string | `'1.0.0'` | Version of the app. |
| `window.title` | string | The value of `name` | Initial window title. |
| `window.width` | integer | `1280` | Initial window width. |
| `window.height` | integer | `720` | Initial window height. |
| `window.fullscreen` | boolean | `false` | Whether the app starts fullscreen. |
| `window.highDpi` | boolean | `true` | Whether the framebuffer uses the full display density. |
| `window.resizable` | boolean | `true` | Whether the player can resize the window. |
| `window.vsync` | boolean | `true` | Whether presentation waits for the display refresh. |
| `window.sampleCount` | integer | `1` | Multisampling sample count. |
| `window.decorated` | boolean | `true` | Whether the desktop window has a title bar and a border. |
| `window.transparent` | boolean | `false` | Whether the window opened able to be transparent, and transparent. |
| `window.alwaysOnTop` | boolean | `false` | Whether the desktop window stays above normal windows. |
| `window.showInTaskbar` | boolean | `true` | Whether the window has a taskbar button or a Dock icon. |
| `window.focusable` | boolean | `true` | Whether clicking the window activates the app. |
| `window.mousePassthrough` | boolean | `false` | Whether clicks start passing through the window. |
| `window.position` | table | absent | Where the desktop window opened, present only when `app.json` sets it, as `{x, y}` or `{anchor, area, monitor, offset, fill}`. |
| `design.width` | number | `1920` | Design resolution width in units. |
| `design.height` | number | `1080` | Design resolution height in units. |
| `design.scaling` | string | `'expand'` | Scaling policy, see `haylen.viewport`. |
| `orientation` | string | `'landscape'` | `'landscape'`, `'portrait'` or `'any'`. |
| `fixedRate` | number | `60` | Fixed updates per second. The function `haylen.fixedStep()` returns the matching step length in seconds. |
| `maxFrameTime` | number | `0.25` | Longest frame duration in seconds that the clock accepts. |
| `clearColor` | string | `'#FF000000'`, or `'#00000000'` for a transparent window | Background color as `#AARRGGBB`. |
| `splash.logo` | string | `''` | Image of the launch screen relative to `content/`, empty for the Haylen logo. |
| `splash.background` | string | The value of `clearColor` | Background color of the launch screen as `#AARRGGBB`. |
| `lifecycle.pauseOnBackground` | boolean | `true` | Whether the app halts in the background, see `haylen.lifecycle()`. |
| `lifecycle.pauseOnFocusLoss` | boolean | `false` | Whether the app halts while its window has no focus. |
| `lifecycle.muteOnFocusLoss` | boolean | `false` | Whether the master bus is muted while the app is not active. |
| `audio.iosSession` | string | `'ambient'` | Category of the audio session on iOS and tvOS: `'ambient'`, `'soloAmbient'` or `'playback'`, described in the [audio guide](../audio.md#sessions-and-interruptions). |
| `audio.mixWithOthers` | boolean | `false` | Whether the `'playback'` session plays along with other apps. |
| `debug.stats` | string | `'off'` | Debug statistics the app starts with: `'off'`, `'compact'` or `'full'`, see [`haylen.debug`](debug.md). |
| `debug.objectEvents` | boolean | `false` | Whether every counted object publishes `objectCreated` and `objectDestroyed`, see [`haylen.debug`](debug.md#object-counts-and-events). |
| `debug.safeArea` | string or table | absent | The safe area simulated instead of the one of the device, present only when `app.json` sets it, see [`viewport.setSafeAreaSimulation`](viewport.md#viewportsetsafeareasimulationvalue). |
| `debug.showSafeArea` | boolean | `false` | Whether the debug view of the safe area shows from the start, see [`ui.setSafeAreaVisible`](ui.md#uisetsafeareavisiblevisible). |
| `autoload` | list of strings | `{}` | Modules that load as autoloads before `source/main.lua`, see `haylen.autoload`. |
| `native` | table | `{}` | The native libraries the app ships by name, as `app.json` lists them for `haylen.py`, see the [native code guide](../native.md#packaging-libraries-with-an-app). |
| `plugins` | table | `{}` | The plugins of the app by id with the parameter values that `app.json` gives them, see the [plugin guide](../plugins.md). The field [`platform.plugin(id).config`](platform.md#plugin-handles) adds the defaults of their parameters. |

```lua
local haylen = require('haylen')

local config = haylen.config
print(config.name .. ' ' .. config.version)
print('design size', config.design.width, config.design.height, config.design.scaling)
print('fixed updates per second', config.fixedRate)
print('halts in the background', config.lifecycle.pauseOnBackground)
```

### haylen.autoloads

A table that maps the name of every autoload to its table, as `haylen.autoload` describes. It is empty when the app has no autoloads.

```lua
local haylen = require('haylen')

for name, autoload in pairs(haylen.autoloads) do
    print('autoload', name, autoload)
end
```

### haylen.class

The class helper for app-wide Lua classes, with inheritance, an `init` constructor, `super`, `is` checks and mixins. The [Lua guide](../lua.md#classes) describes it.

```lua
local haylen = require('haylen')

local Unit = haylen.class('Unit')

function Unit:init(name, health)
    self.name, self.health = name, health
end

local knight = Unit('Knight', 120)
print(knight.name, knight:is(Unit), tostring(Unit)) -- Knight true class Unit
```

## Functions

### haylen.elapsed()

Returns the app time in seconds since the app started. It is the sum of every scaled frame delta, so it stops while the time scale is zero.

```lua
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')

require('haylen.scene').push({
    render = function(self)
        local pulse = 0.5 + 0.5 * math.sin(haylen.elapsed() * 4)
        graphics2d.beginScreen()
        graphics2d.drawCircle(960, 540, 40 + 20 * pulse, '#FFFFD166')
    end,
})
```

### haylen.delta()

Returns the scaled duration of the current frame in seconds. The real frame time is first clamped to `config.maxFrameTime` and then multiplied by the time scale. It is the same value that the scene `update` callback receives.

```lua
local haylen = require('haylen')

local boat = {x = 0, speed = 120}

local function drift(entity)
    entity.x = entity.x + entity.speed * haylen.delta()
end

require('haylen.scene').push({
    update = function(self, dt)
        drift(boat)
    end,
})
```

### haylen.unscaledDelta()

Returns the real duration of the current frame in seconds, clamped to `config.maxFrameTime` and not affected by the time scale. Use it for things that keep moving while the app is paused, such as menu animations.

```lua
local haylen = require('haylen')

local menu = {glow = 0}

require('haylen.scene').push({
    update = function(self, dt)
        -- Slow motion changes dt, and the menu still animates at full speed.
        menu.glow = (menu.glow + haylen.unscaledDelta()) % 1
    end,
})
```

### haylen.frameIndex()

Returns the number of frames the engine has started, as an integer. It is `0` while `source/main.lua` runs and `1` during the first frame.

```lua
local haylen = require('haylen')

require('haylen.scene').push({
    update = function(self, dt)
        -- Expensive checks run on every tenth frame only.
        if haylen.frameIndex() % 10 == 0 then
            print('checking quests on frame', haylen.frameIndex())
        end
    end,
})
```

### haylen.timeScale()

Returns the current time scale. The value `1` is normal speed.

```lua
local haylen = require('haylen')

local frozen = haylen.timeScale() == 0
print('frozen:', frozen)
```

### haylen.setTimeScale(scale)

Sets the speed of app time. The scale `0.5` plays at half speed, `0` freezes app time and values above `1` speed the app up. Negative values count as `0`. A frozen app still calls updates, with a delta of zero, and `haylen.setPaused` is the way to stop what the pause should stop. The scale applies to `haylen.delta()`, `haylen.elapsed()`, the `dt` of scene updates, and timers and tweens that are not `unscaled`. Fixed updates keep their step length but run less or more often, so a physics world stepped from `fixedUpdate` slows down with the rest of the app. The function `haylen.unscaledDelta()` is not affected.

```lua
local haylen = require('haylen')
local timer = require('haylen.timer')

local function slowMotion(seconds)
    haylen.setTimeScale(0.25)
    -- Timers run on app time, so a quarter-speed wait of 0.25 seconds lasts one real second.
    timer.after(seconds * 0.25, function() haylen.setTimeScale(1) end)
end

slowMotion(1)
```

### haylen.fixedStep()

Returns the length of one fixed step in seconds, which is `1 / config.fixedRate`. It is the same value that the scene `fixedUpdate` callback receives, and the time scale does not change it.

```lua
local haylen = require('haylen')

-- A dash counted in fixed steps lasts the same number of physics steps on every device.
local dashSteps = math.ceil(0.25 / haylen.fixedStep())
local dash = {remaining = dashSteps}

require('haylen.scene').push({
    fixedUpdate = function(self, step)
        if dash.remaining > 0 then
            dash.remaining = dash.remaining - 1
        end
    end,
})
print('a dash lasts', dashSteps, 'fixed steps')
```

### haylen.interpolation()

Returns how far app time has advanced from the last fixed step toward the next one, from `0` up to but not including `1`. Rendering a position between its values before and after the last fixed step with this factor keeps motion smooth when the frame rate and the fixed rate differ.

```lua
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')

local ball = {previous = 0, current = 0, speed = 300}

require('haylen.scene').push({
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

### haylen.paused()

Returns `true` while the game is paused.

```lua
local haylen = require('haylen')

print('paused:', haylen.paused()) -- paused: false
```

### haylen.setPaused(paused)

Pauses or unpauses the game. Scenes, autoloads, timers and tweens in the `'pausable'` process mode stop, those in `'whenPaused'` start, and fixed steps stop accumulating. The scenes that the change stops or starts get `paused` or `unpaused`, and the change publishes the `paused` or `unpaused` event of [`haylen.events`](events.md). The [lifecycle guide](../lifecycle.md#pause-and-process-modes) explains the process modes.

```lua
local haylen = require('haylen')
local scene = require('haylen.scene')

local menu = {
    processMode = 'whenPaused',
    transparent = true,
    enter = function(self) haylen.setPaused(true) end,
    exit = function(self) haylen.setPaused(false) end,
}

scene.push({update = function(self, dt) end})
scene.push(menu)
require('haylen.timer').after(0.5, function() print('paused:', haylen.paused()) end, {processMode = 'always'})
```

### haylen.appState()

Returns where the app stands with the platform: `'active'` in the foreground with the focus, `'inactive'` while visible without the focus, interrupted by the system, such as by a phone call, or covered by native UI of a plugin, and `'background'` while hidden. The [lifecycle guide](../lifecycle.md#app-states) explains what the engine does in each state.

```lua
local haylen = require('haylen')

if haylen.appState() == 'active' then
    print('the player is here')
end
```

### haylen.appCovered()

Returns `true` while native UI of a plugin covers the app, such as a full screen ad, a consent form, a sign-in sheet, a purchase dialog or a [screen](platform.md#screens) of a plugin. A covered app is `'inactive'`, halted and muted, whatever the lifecycle options say, and it comes back as it was once the last cover ends, as the [lifecycle guide](../lifecycle.md#covered-by-native-ui) explains.

```lua
local events = require('haylen.events')
local haylen = require('haylen')

events.on('appInactive', function()
    if haylen.appCovered() then
        print('a native view covers the game')
    end
end)
```

### haylen.networkState()

Returns whether the device reaches the network, as the platform last reported it: `'online'`, `'offline'`, or `'unknown'` until a report arrives. Browsers, Android and Apple platforms, macOS included, report the network from the start, usually by the first frame, and then publish every change as the `networkOnline` and `networkOffline` events of [`haylen.events`](events.md#engine-events). Windows, Linux and the headless host never report it, so it stays `'unknown'` there, and so does an Android app without the permission `android.permission.ACCESS_NETWORK_STATE`, as [Android permissions](system.md#android-permissions) describes.

```lua
local haylen = require('haylen')
local events = require('haylen.events')

local function show(state)
    print('the network is ' .. state)
end

show(haylen.networkState())
events.on('networkOnline', function() show('online') end)
events.on('networkOffline', function() show('offline') end)
```

### haylen.halted()

Returns `true` while the lifecycle options halt the app in its current state or native UI covers it, so no time passes for scenes, autoloads, timers and tweens.

```lua
local haylen = require('haylen')

print('halted:', haylen.halted()) -- halted: false
```

### haylen.lifecycle()

Returns a table with the current lifecycle options: `pauseOnBackground`, `pauseOnFocusLoss` and `muteOnFocusLoss`. They start with the `lifecycle` object of `app.json`.

```lua
local haylen = require('haylen')

local options = haylen.lifecycle()
print(options.pauseOnBackground, options.pauseOnFocusLoss, options.muteOnFocusLoss) -- true false false
```

### haylen.setLifecycle(options)

Changes the lifecycle options that `options` names and keeps the others. Unknown keys raise `Unknown option "<key>".`

| Key | Type | Meaning |
| --- | --- | --- |
| `pauseOnBackground` | boolean | Halts the app in the background. |
| `pauseOnFocusLoss` | boolean | Halts the app while its window has no focus. |
| `muteOnFocusLoss` | boolean | Mutes the master bus while the app is not active. |

```lua
local haylen = require('haylen')

-- A music player keeps playing when the player switches to another window.
haylen.setLifecycle({pauseOnFocusLoss = false, muteOnFocusLoss = false})
```

### haylen.autoload(name, module)

Loads `module` as an autoload, calls its `start` and returns its table. An autoload lives for the whole app, gets the callbacks the [lifecycle guide](../lifecycle.md#autoloads) lists and is kept in `haylen.autoloads` under `name`. With one argument, the name is the last part of the module in camel case, so `haylen.autoload('state.player-data')` names it `playerData`. Autoloads listed in the `autoload` field of `app.json` load before `source/main.lua` runs.

The module must return a table, a name can only be taken once and a table can only be one autoload, otherwise the call raises `The autoload module "<module>" must return a table.`, `An autoload named "<name>" already exists.` or `The module "<module>" is already the autoload "<name>".`

```lua
-- The file `source/systems/music.lua`.
local music = {processMode = 'always', track = 'day'}

function music:start()
    print('music plays ' .. self.track)
end

return music
```

```lua
local haylen = require('haylen')

local music = haylen.autoload('soundtrack', 'systems.music')
print(haylen.autoloads.soundtrack == music, require('systems.music') == music) -- true true
```

### haylen.quit()

Stops the app and asks the platform to close it. No further frames run. On Android the activity of the app finishes.

```lua
local haylen = require('haylen')
local input = require('haylen.input')

input.loadActions({actions = {{name = 'quit', type = 'button', bindings = {'key:escape', 'button:back'}}}})

require('haylen.scene').push({
    update = function(self, dt)
        if input.pressed('quit') then
            haylen.quit()
        end
    end,
})
```

### haylen.requestRestart()

Starts the app again from its package once the current frame ends, the way a hot reload of an edited script does: the scenes, the autoloads and the plugins stop, and a new Lua state runs `source/main.lua`. What belongs to the platform outlives the restart, such as the covers of native UI, the edges that native views reserve, the streams of plugins and a [screen](platform.md#screens) that shows, whose end reaches the restarted app as `screenRestored`.

```lua
local haylen = require('haylen')
local input = require('haylen.input')

input.loadActions({actions = {{name = 'restart', type = 'button', bindings = {'key:f5'}}}})

-- F5 starts the app again, which runs every script of the package anew.
require('haylen.scene').push({
    update = function(self, dt)
        if input.pressed('restart') then
            haylen.requestRestart()
        end
    end,
})
```

### haylen.reportError(message)

Stops the app with an error. The report of the error is logged at the error level, app updates and scene rendering stop and the error screen shows `message` with the stack of the code that called `reportError` until the app is restarted. Only the first error is kept, so later calls do nothing. Uncaught errors in scene callbacks, timers, tweens and `async` tasks end up on the same screen automatically.

```lua
local haylen = require('haylen')
local storage = require('haylen.storage')

local ok, failure = pcall(storage.readJson, 'levels/custom.json')
if not ok and storage.exists('levels/custom.json') then
    haylen.reportError('The custom level is corrupted: ' .. failure)
end
```
