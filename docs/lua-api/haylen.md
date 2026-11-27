# 📖 `haylen`

The root module `haylen` tells an app about the engine and the device it runs on, and it controls the app as a whole. Use it to read the engine version, the platform, the graphics API and the settings of `haylen.json`, to read and scale the clock of the app, to pause the game, to follow the app state, to add autoloads and classes, and to quit, restart or stop the app with an error. The [Lua guide](../core/lua.md) and the [lifecycle guide](../core/lifecycle.md) explain the ideas behind it.

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

The platform that the app runs on, as a string. It is `'windows'`, `'macos'`, `'linux'`, `'ios'`, `'tvos'`, `'android'` or `'web'`. iPhones, iPads, Mac Catalyst apps and visionOS apps report `'ios'`. The headless host that the engine tests use reports `'headless'`, and a [port](../platform/ports.md) to another platform reports the name of its platform. The module [`haylen.system`](system.md) tells more about the device.

```lua
local haylen = require('haylen')

local touchFirst = haylen.platform == 'ios' or haylen.platform == 'android'
print('Show the touch controls: ' .. tostring(touchFirst) .. '.')
```

### haylen.graphicsApi

The graphics API that the GPU device draws through, as a string: `'metal'`, `'direct3D12'`, `'direct3D11'`, `'vulkan'`, `'openGl'`, `'openGlEs'`, `'webGl2'`, `'webGpu'`, `'port'` or `'none'`. The window takes the first API that starts, in the order of [`window.graphicsApis`](../reference/haylen-json.md#window) and then in the order of the platform.

| Platform | Graphics APIs in the order of the platform |
| --- | --- |
| Apple platforms | `'metal'` |
| Windows | `'direct3D12'`, `'direct3D11'`, `'vulkan'`, `'openGl'` |
| Linux | `'vulkan'`, `'openGl'` |
| Android | `'vulkan'`, `'openGlEs'` |
| Web | `'webGpu'`, `'webGl2'` |
| Headless host | `'none'`, since nothing draws there |
| A [port](../platform/ports.md) with a device of its own | `'port'`, then the APIs of the engine that the port compiles |

The function [`graphics.apiName()`](graphics.md#graphicsapiname) returns it too.

```lua
local haylen = require('haylen')

print('The graphics API is ' .. haylen.graphicsApi .. '.')
local web = haylen.graphicsApi == 'webGpu' or haylen.graphicsApi == 'webGl2'
print('Runs in a browser: ' .. tostring(web) .. '.')
```

### haylen.config

A table with the settings of `haylen.json`, with the default filled in for every field that the file leaves out. The table is a copy, so changing it changes nothing in the engine. The [Lua guide](../core/lua.md#haylenjson) explains what `haylen.json` is for. The tables below list every field of `haylen.json`, which is also every key of `haylen.config`.

Every field is optional, and the table holds every field, with the default of each one that `haylen.json` leaves out, besides the fields that have no default. The table gives colors back as `#AARRGGBB`.

```json
{
    "name": "Tiny Island",
    "identifier": "com.haylen.tinyisland",
    "version": "1.0.0",
    "category": "game",
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

The [reference of `haylen.json`](../reference/haylen-json.md) lists every key with its type, its default and its meaning, and the messages of every value that the engine refuses.

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

### haylen.drawing()

Returns `true` while the current frame draws the app. A frame draws nothing while the app is in the background and while an opaque screen of a plugin hides it, a covered app draws one frame once the cover began, which stays on screen, and no more while the cover lasts, and an app that [redraws on change](window.md#windowsetredrawvalue) draws only the frames that something changed in. Work that only prepares what a frame shows can skip the frames that draw nothing.

```lua
local haylen = require('haylen')

print('Drawing: ' .. tostring(haylen.drawing()) .. '.')
```

### haylen.lifecycle()

Returns a new table with the current lifecycle options: `pauseOnBackground`, `pauseOnFocusLoss` and `muteOnFocusLoss`. They start with the `lifecycle` section of `haylen.json`, whose [table](../reference/haylen-json.md#lifecycle) says what each one does.

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

Returns whether the device reaches the network, as the platform last reported it: `'online'`, `'offline'`, or `'unknown'` until a report arrives. Browsers, Android, Apple platforms, Windows and Linux report the network from the start, usually by the first frame. Then they publish every change as the events `networkOnline` and `networkOffline` of [`haylen.events`](events.md#engine-events). The [lifecycle guide](../core/lifecycle.md#events-by-platform) lists where each platform reads it.

On the headless host nothing reports the network, so the state stays `'unknown'`. It also stays `'unknown'` on a Linux desktop without GIO, and in an Android app without the permission `android.permission.ACCESS_NETWORK_STATE`, as [Android permissions](system.md#android-permissions) describes.

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

With one argument, `haylen.autoload(module)`, the name is the last part of the module in camel case. So `haylen.autoload('state.player-data')` names it `playerData`. The autoloads that the `autoload` list of `haylen.json` names load in its order before `source/main.lua` runs.

Adding the same module under the same name again returns the autoload that exists. So a module that adds an autoload at its top level runs again unchanged when it [reloads](hot-reload.md). What `start` registers without an `owner` then lives as long as the autoload, instead of ending with that reload. Starting an autoload publishes the event `autoloadStarted` with its name.

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

Returns whether [`haylen.quit`](#haylenquitoptions) closes the app. It does on macOS, Windows, Linux, Android and the headless player. iPhones, iPads, Apple TVs, Mac Catalyst windows and web pages close only through the system or the browser. There the function returns `false`, and a quit only stops the app. An app that [another app launched](#haylenlaunchpath-options) is the exception: its quit goes back to that app everywhere. Show a Quit button only where this returns `true`.

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

### haylen.launch(path, options)

Runs another app package in place of this app, once the current frame ends. The string `path` names a folder or a zip file of the file system that holds a `haylen.json`. The launched app starts with a new engine and a new Lua state. Use it for an app that opens other apps, such as one that downloads a package and plays it.

When the launched app quits, this app starts again from its own package. The launched app quits through [`haylen.quit`](#haylenquitoptions), the Close button of its error screen, the back button at the root of the app on Android and Apple TV, or a hold that the launched app cannot take, which works on every device:

| Device | Hold for 1.5 seconds |
| --- | --- |
| Keyboard, on desktops, the web, Mac Catalyst and tablets with a keyboard | `Escape` |
| Apple TV and Android TV remotes | Play and pause, since the Apple TV takes a hold of its Menu button to the Home screen of the system |
| Android phones, tablets and TVs | The back button, or the back gesture kept before you let go |
| Gamepad | The back button, such as View or Select |
| Touch screen | Three fingers that stay still |

Apple Vision Pro has the holds of a keyboard and a gamepad, since a pinch there acts as one finger. The launched app hears the keys, buttons and fingers of the hold as always, so a short press does what the app does with it. A panel at the top of the screen fills its bar while the hold lasts, and the release that ends the hold never reaches the app that starts again. A launched app can launch another app in turn.

The links that open the process, such as those of a URL scheme that the plugins of this app declare, belong to this app, since the system knows only its schemes. A link that arrives while a launched app plays ends that app, and every app it launched, and this app hears the link as [`linkOpened`](events.md#app-and-plugins) once it starts again. So an app that opens other apps from links, such as from a QR code that the camera of the system reads, takes the next link while it plays one.

The table `options` is optional.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `address` | string | none | The address that the package came from, such as `https://example.com/apps/demo.zip`, which names a launched app whose `haylen.json` names no identifier, as the [distribution guide](../tools/distribution.md#identifier) describes. So two packages that a launcher downloads to one file never share an identifier or their storage. Without it, the resolved path of the package names such an app. |
| `developmentServer` | string | none | The `ws://` or `wss://` address of a development server, with its token, which plays the launched app in development. The server sends the files saved on the machine of the developer, which [hot reload](../core/lua.md#hot-reload) applies in place, and the keys of the run restart the app and reach its developer console. An app without it plays as it is. |
| `nativeAccess` | boolean | `true` | Whether the launched app reaches native code. With `false` the launched app has no `ffi`, `process` or [`haylen.native`](native.md) module, as a global or through `require`, and no `io.popen` or `os.execute`, so it reaches the system only through the modules of the engine and the plugins of this app. Use it for apps that come from addresses that people give. |

An app launched without native access passes that limit on: every app it launches has no native access either, whatever its options say. Requiring one of those modules there raises `The app that launched this app gave it no native access, so the module "ffi" is not available.`, with the name of the module. The app that launched it keeps its own native access for its return.

Each app plays with its own development session. A launched app that names a server plays in development even when the app that launched it ships as a release, and an app that names none plays without the files that a development server sends to the app that launched it. The app that launched it starts again with its own session, or with none.

A path with nothing at it raises `No app package is at "<path>". Launch a folder or a zip file that holds "haylen.json".`, an `address` without `://` raises `The address "<address>" of the launch holds no "://". Name the address that the package came from, such as "https://example.com/apps/garden.zip".`, and a `developmentServer` of another kind raises `The development server "<address>" is not an address that starts with "ws://" or "wss://".`. An unknown key raises `Unknown option "<key>".`. A package that cannot open, such as one without a valid `haylen.json`, shows the error screen in place of the launched app. A launched app whose engine cannot start, such as one whose storage folder cannot be made, has no error screen, so its error goes to the log and this app starts again. The [headless player](../tools/testing.md#the-test-project) then exits with `1`.

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
    haylen.launch(path, {address = 'https://example.com/apps/demo.zip'})
end)
```

```lua
local haylen = require('haylen')
local storage = require('haylen.storage')

-- Plays a downloaded package in development, connected to the server that "haylen app serve" runs for the app on the computer of the developer.
haylen.launch(storage.root() .. '/demo.zip', {developmentServer = 'ws://192.168.0.10:8000/haylen/development?token=3f9a1c2b7d4e'})
```

```lua
local haylen = require('haylen')
local storage = require('haylen.storage')

-- Plays a package downloaded from an address that a person typed, which reaches the system only through the engine.
haylen.launch(storage.root() .. '/downloaded.zip', {nativeAccess = false})
```

### haylen.launched()

Returns `true` when another app [launched](#haylenlaunchpath-options) this one, so quitting it starts that app again. Use it to offer a way back only when something launched the app.

```lua
local haylen = require('haylen')
local ui = require('haylen.ui')

if haylen.launched() then
    ui.mount(ui.button{text = 'Back', anchor = 'topLeft', margin = 16, onClick = function() haylen.quit() end})
end
```

### haylen.launchArguments()

Returns the launch arguments of the app as a new list of strings, empty when the launch gave none. They are the arguments after `--` on the command line of the player or of the executable of a desktop app, the launch arguments of the process on Apple platforms, such as those of a UI test, the list of strings in the extra `dev.haylen.arguments` of the intent that started the activity on Android, and each `?argument=` of the address of a web page. Only the app that the process plays first has them, so an app that another app [launched](#haylenlaunchpath-options) gets an empty list, and the app that launched it gets them again once it starts again. Any program that starts the app chooses them, so read them as input that an app checks before it trusts it.

```lua
local haylen = require('haylen')
local scene = require('haylen.scene')

-- Opens the level that "-- --level 3" names, such as from a script that tests the level.
local arguments = haylen.launchArguments()
for index = 1, #arguments - 1 do
    if arguments[index] == '--level' then
        scene.push(require('scenes.level')(tonumber(arguments[index + 1])))
    end
end
```

### haylen.packageInfo(path)

Reads what an app package says about itself, such as the name and the logo that a list of downloaded apps shows, without running it. The string `path` names a folder or a zip file of the file system that holds a `haylen.json`. It returns a Varn promise that resolves on a later frame with a table, since it opens the package on the I/O pool.

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | The `name` of `haylen.json`. |
| `identifier` | string | The `identifier` of `haylen.json`. |
| `version` | string | The `version` of `haylen.json`. |
| `build` | integer | The `build` of `haylen.json`, 1 when it gives none. |
| `logo` | string or nil | The path of `splash.logo`, relative to `content/`, or nil when `haylen.json` names none. |
| `logoBytes` | string or nil | The bytes of that file, such as the text of an SVG document or the bytes of a PNG image, which [`graphics.newTexture`](graphics.md#graphicsnewtexturebytes-options) or [`graphics.newVectorImage`](graphics.md#graphicsnewvectorimagetext) turn into a picture. |

A path with nothing at it rejects with `The app package could not be read. No app package is at "<path>". Name a folder or a zip file that holds "haylen.json".`, and a package that cannot open, or whose `haylen.json` is not valid, rejects with the reason after the same first sentence.

```lua
local async = require('async')
local graphics = require('haylen.graphics')
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

-- Shows the name and the logo of a downloaded package before it plays.
async.spawn(function()
    local info, failure = haylen.packageInfo(storage.root() .. '/demo.zip'):await()
    if not info then
        print(failure)
        return
    end
    if info.logo then
        ui.setImage('@demo', info.logo:match('%.svg$') and graphics.newVectorImage(info.logoBytes) or graphics.newTexture(info.logoBytes))
    end
    ui.mount(ui.row{gap = 12, ui.image{image = info.logo and '@demo' or 'ui/app.svg', width = 48, height = 48}, ui.label{text = info.name}})
end)
```

### haylen.requestRestart()

Starts the app again from its package once the current frame ends, the way a hot reload of an edited `haylen.json` does. The scenes, the autoloads and the plugins stop, and a new Lua state runs `source/main.lua`. What belongs to the platform outlives the restart: the covers of native UI, the edges that native views reserve, the streams of plugins, and a [screen](platform.md#screens) that shows. The end of that screen reaches the restarted app as `screenRestored`.

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
| `No app package is at "<path>". Launch a folder or a zip file that holds "haylen.json".` | Nothing is at the path of `haylen.launch`. |
| `The address "<address>" of the launch holds no "://". Name the address that the package came from, such as "https://example.com/apps/garden.zip".` | The option `address` of `haylen.launch` is not an address. |
| `The development server "<address>" is not an address that starts with "ws://" or "wss://".` | The option `developmentServer` of `haylen.launch` is not the address of a development server. |
| `The app package could not be read. <reason>` | The promise of `haylen.packageInfo` rejects, such as for a path with nothing at it. |
| `An autoload needs a name.` | The name of `haylen.autoload` is empty. |
| `An autoload named "<name>" already exists.` | Another module is already the autoload of that name. |
| `The autoload module "<module>" must return a table.` | The module of `haylen.autoload` returned something other than a table. |
| `The module "<module>" is already the autoload "<name>".` | The table of the module is already an autoload under another name. |
| `bad argument #1 to 'class' (string expected, got <type>)` | The name of `haylen.class` is not a string. |
| `bad argument #2 to 'class' (class expected, got <type>)` | The parent of `haylen.class` is not a class. |
| `bad argument #1 to 'include' (table expected, got <type>)` | The mixin of `Class:include` is not a table. |

