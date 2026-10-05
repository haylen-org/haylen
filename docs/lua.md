# Lua guide

Apps for Haylen are written in Lua 5.5, which the engine runs through the [Varn](https://github.com/varn-org/varn) runtime. This guide explains the model an app is built on: the package layout, `app.json`, how modules and assets are found, scenes, autoloads, classes, disk access, asynchronous code, errors, hot reload and the fast paths for performance. The last section shows how a C++ project adds its own Lua modules. Every engine module has a reference page listed in the [Lua API reference](lua-api.md), and the [architecture guide](architecture.md) explains what happens underneath.

## The app package

An app is a package: a folder, or the same folder zipped, with `app.json` at its root, the Lua modules under `source/`, the assets under `content/` and the manifest and Lua modules of every [plugin](plugins.md) the app uses under `plugins/`. Nothing else in the folder belongs to the package, so platform projects, notes, build files and the native parts of plugins can live next to it.

```text
my-app/
  app.json             Window, design resolution and identity, read before any Lua runs.
  source/
    main.lua           Entry point.
    config.lua         Any other Lua module.
    scenes/
      main-menu.lua    Loaded with require('scenes.main-menu').
  content/
    images/hero.png    Loaded with assets.texture('images/hero.png').
    preload.json
  plugins/
    admob/
      plugin.json      The manifest of a plugin that app.json lists.
      source/
        init.lua       Loaded with require('admob').
```

Lua files use `dash-case` names, and asset files and folders use lowercase `snake_case`. The command `python3 haylen.py package my-app -o my-app.zip` zips `app.json`, `source/`, `content/` and the `plugin.json` and `source/` of every plugin of a folder, with `app.json` at the root of the archive. The desktop player runs either form with `haylen my-app` or `haylen my-app.zip`, and the [distribution guide](distribution.md) shows how to run a package on every platform and in the browser.

## app.json

The runtime reads `app.json` before the window exists, so it configures everything that must be known before the first script runs. Every field is optional.

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `"Haylen App"` | Display name of the app. |
| `identifier` | string | `"dev.haylen.app"` | Unique reverse-domain id. It names the folder where `haylen.storage` and `haylen.preferences` keep their files. |
| `version` | string | `"1.0.0"` | Version of the app. |
| `window.title` | string | The value of `name` | Window title. |
| `window.width`, `window.height` | integer | `1280`, `720` | Initial window size. |
| `window.fullscreen` | boolean | `false` | Starts fullscreen. |
| `window.highDpi` | boolean | `true` | Uses the full display density for the framebuffer. |
| `window.resizable` | boolean | `true` | Whether the player may resize the window. |
| `window.vsync` | boolean | `true` | Waits for the display refresh when presenting. |
| `window.sampleCount` | integer | `1` | Multisampling sample count. |
| `window.decorated` | boolean | `true` | Whether the desktop window has a title bar and a border. |
| `window.transparent` | boolean | `false` | Opens the window able to be transparent, and transparent, so the desktop shows through its transparent pixels, and makes `clearColor` default to `"#00000000"`. Only such a window can turn transparent again after `window.setTransparent(false)`. |
| `window.alwaysOnTop` | boolean | `false` | Keeps the desktop window above normal windows. |
| `window.showInTaskbar` | boolean | `true` | Whether the window has a taskbar button, and on macOS whether the app has a Dock icon and a menu bar. |
| `window.focusable` | boolean | `true` | Whether clicking the window activates the app and gives it the keyboard. |
| `window.mousePassthrough` | boolean | `false` | Lets every click pass through the window to what is behind it, until the app gives regions that keep the mouse. |
| `window.position` | string or object | centered | Where the desktop window opens: `"center"`, a point `{"x": 40, "y": 60}` in desktop points, or an anchored placement such as `{"anchor": "bottom", "area": "work", "monitor": "primary", "offset": [0, -8], "fill": "width"}`, described in the [desktop guide](desktop.md#placing-the-window). |
| `design.width`, `design.height` | number | `1920`, `1080` | Design resolution, the coordinate space the app draws and receives input in. |
| `design.scaling` | string | `"expand"` | One of `"fit"`, `"fill"`, `"stretch"`, `"expand"`, `"pixelPerfect"`, `"fitWidth"` or `"none"`, described in [`haylen.viewport`](lua-api/viewport.md). |
| `orientation` | string | `"landscape"` | `"landscape"`, `"portrait"` or `"any"`. |
| `fixedRate` | number | `60` | Fixed updates per second. The function `haylen.fixedStep()` returns the matching step length in seconds. |
| `maxFrameTime` | number | `0.25` | Longest frame time in seconds that the clock accepts, so a stall never causes a burst of fixed steps. |
| `clearColor` | string | `"#FF000000"`, or `"#00000000"` for a transparent window | Background color as `"#RRGGBB"` or `"#AARRGGBB"`. |
| `splash.logo` | string | the Haylen symbol | Image of the launch screen, relative to `content/`, such as `"ui/splash.png"`. |
| `splash.background` | string | The value of `clearColor` | Background color of the launch screen as `"#RRGGBB"` or `"#AARRGGBB"`. |
| `lifecycle.pauseOnBackground` | boolean | `true` | Halts the app while it is in the background. |
| `lifecycle.pauseOnFocusLoss` | boolean | `false` | Halts the app while its window has no focus. |
| `lifecycle.muteOnFocusLoss` | boolean | `false` | Mutes the master bus while the app is not active. |
| `audio.iosSession` | string | `"ambient"` | Category of the audio session on iOS and tvOS: `"ambient"`, `"soloAmbient"` or `"playback"`, described in the [audio guide](audio.md#sessions-and-interruptions). |
| `audio.mixWithOthers` | boolean | `false` | Lets the `"playback"` session play along with other apps. Other sessions reject `true`. |
| `input.mouseAsTouch` | boolean | `false` | Makes the mouse act as a finger from the start, as [`input.setMouseAsTouch`](lua-api/input.md#inputsetmouseastouchenabled) describes. |
| `input.touchAsMouse` | boolean | `false` | Makes a finger act as the mouse from the start, as [`input.setTouchAsMouse`](lua-api/input.md#inputsettouchasmouseenabled) describes. |
| `ui.scaleMode` | string | `"design"` | How large the interface draws: `"design"` or `"physical"`, as [`ui.setScaleMode`](lua-api/ui.md#uisetscalemodemode) describes. |
| `ui.scale` | number | `1` | Factor from 0.25 to 4 that multiplies the size of the interface, as [`ui.setScale`](lua-api/ui.md#uisetscalefactor) describes. |
| `debug.stats` | string | `"off"` | Debug statistics the app starts with: `"off"`, `"compact"` for the frame rate, frame time, draw calls, vertices and instances in a corner, or `"full"` for the debug overlay, described in [`haylen.debug`](lua-api/debug.md). |
| `debug.drawings` | list of strings | `[]` | The [debug drawings](lua-api/debug.md#drawings) the app starts with, such as `["physics", "bounds"]`. An empty name raises `The "debug.drawings" list in "app.json" has an empty drawing name.` |
| `debug.objectEvents` | boolean | `false` | Publishes `objectCreated` and `objectDestroyed` for every counted object, as [`haylen.debug`](lua-api/debug.md#object-counts-and-events) describes. |
| `debug.safeArea` | string or insets | none | Safe area to simulate instead of the one of the device: a device name such as `"iphoneDynamicIsland"` or insets in window points, as [`viewport.setSafeAreaSimulation`](lua-api/viewport.md#viewportsetsafeareasimulationvalue) describes. |
| `debug.showSafeArea` | boolean | `false` | Shows the debug view of the safe area from the start, as [`ui.setSafeAreaVisible`](lua-api/ui.md#uisetsafeareavisiblevisible) describes. |
| `autoload` | array of strings | `[]` | Modules that load as [autoloads](#autoloads) before `source/main.lua`, such as `"state.player-data"`. |
| `native` | object | `{}` | The native libraries the app ships, by name, as the [native code guide](native.md#packaging-libraries-with-an-app) describes. |
| `plugins` | object | `{}` | The [plugins](plugins.md) the app uses, by id in `dash-case`, each with an object of the values of its parameters, such as `{"admob": {"testMode": true}}`. Every id needs `plugins/<id>/plugin.json` in the package, and the app fails to load with `The plugin "admob" in "app.json" has no "plugins/admob/plugin.json" in the package.` otherwise. |

```json
{
    "name": "Tiny Island",
    "identifier": "dev.haylen.tinyisland",
    "version": "1.0.0",
    "window": {"title": "Tiny Island", "width": 1280, "height": 720},
    "design": {"width": 1920, "height": 1080, "scaling": "expand"},
    "lifecycle": {"pauseOnFocusLoss": true},
    "clearColor": "#FF47ABA9"
}
```

Unknown keys are errors, so a misspelled field never goes unnoticed. The loader rejects them with messages such as `Unknown key "widht" in the "window" section of "app.json".`, and it also rejects values of the wrong type, sizes and rates that are not positive, and unknown scaling policies, orientations and colors. A package that fails to load shows `The app could not be loaded.` and the reason on screen. The option `window.resizable` applies to desktop windows, and `window.setResizable` changes it later. The other desktop options of `window` change later through [`haylen.window`](lua-api/window.md#desktop-windows) as well, and the [desktop guide](desktop.md) describes them. The option `orientation` applies to mobile apps: `haylen.py` writes it into the `Info.plist` of the Apple template and the manifest of the Android template, and `haylen_add_app` into the `Info.plist` of iOS apps built with CMake, as the [distribution guide](distribution.md) describes. Desktop windows and the web ignore it. The option `splash` sets the launch screen that iOS, tvOS, Mac Catalyst and Android show while the app starts and the loading page of the web, which `haylen.py` builds from the platform templates as the [distribution guide](distribution.md#splash-screens) describes. Scripts read the resolved configuration, with every default filled in, from `require('haylen').config`.

## source/main.lua

The file `source/main.lua` runs once when the engine starts, after every plugin has installed its modules and the autoloads have started, and before the first frame. It usually loads shared data, configures input and the UI theme and pushes the first scene. This is the whole entry point of Tiny Island.

```lua
local assets = require('haylen.assets')
local input = require('haylen.input')
local localization = require('haylen.localization')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local preferences = require('systems.preferences')

assets.defineGroups(assets.json('preload.json'))
input.loadActions(assets.json('input/actions.json'))
localization.loadFolder('locale')
localization.setFallback('en')
ui.setTheme(ui.loadTheme('ui/theme.json', 'dark'))
preferences.apply()

scene.push(require('scenes.boot').new())
```

## Modules and require

The function `require` returns a module that is already loaded, which is where Varn's modules are from the start. Otherwise it looks in two places. It first builds the modules registered in `package.preload`, which are the engine modules such as `haylen.graphics` and the modules of any plugin a C++ project adds. Then it searches the `source/` folder of the package: `require('scenes.main-menu')` loads `source/scenes/main-menu.lua` and, when that file is missing, `source/scenes/main-menu/init.lua`. A module whose first name part is the id of a plugin of `app.json` comes from the `source/` folder of that plugin instead: `require('admob')` loads `plugins/admob/source/init.lua` and `require('admob.consent')` loads `plugins/admob/source/consent.lua`, so a module of the app named after a plugin is an error when the app starts, as the [plugin guide](plugins.md#modules) explains. Modules load as text, except in a protected release, where the Lua modules of the app and its plugins ship as bytecode that the release build compiled and the module loader of the engine alone loads, as the [content guide](content.md#lua-bytecode) describes. The same names, files and lines appear in errors either way.

The fields `package.path` and `package.cpath` are empty, so nothing is ever loaded from the host file system and an app behaves the same from a folder, a zip file or the browser. A missing module raises `no file 'source/scenes/missing.lua' or 'source/scenes/missing/init.lua' in the app package`.

When `require` loads a module for the first time, it returns a second value, which for a package module is the path of its file. Wrap the call in parentheses when it is the last argument of another call, as in `scene.push((require('scenes.level')))`, because otherwise the path becomes an extra argument.

App code loads chunks only as text. The functions `load` and `loadfile` take only text, so a mode other than `'t'` raises `chunks load only as text, so the mode is 't'` and binary chunks fail to load, `dofile` runs only text files, and `string.dump` does not exist. Precompiled bytecode is never checked by the Lua virtual machine and could build values that crash it, so the only bytecode that ever runs is the bytecode of a protected release, whose signed and encrypted catalog vouches for every module.

The metatables of engine types are protected. The function `getmetatable(value)` returns the type name, such as `'haylen.Sprite'`, so it can tell engine values apart, and neither `setmetatable` nor the debug library can read or replace their metatables. The function `debug.getmetatable` returns the same as `getmetatable`, `debug.setmetatable` raises `The metatable of this value is protected and cannot be changed.` for an engine value, `debug.getregistry` does not exist, and `debug.getupvalue` and `debug.setupvalue` see no upvalues in native functions. A value can therefore never be finalized by hand or given the members of another type.

Varn's modules live in the same state: `async`, `http`, `socket`, `json`, `fs`, `zip`, `crypto`, `log`, `platform`, `process`, `datetime`, `xml` and `ffi`. The [Varn documentation](https://github.com/varn-org/varn) describes them. Apps use `haylen.storage` for their own files and `haylen.log` and `haylen.platform` for logging and native calls, which are different modules from Varn's `log` and `platform`. Browsers have no raw TCP, so the `socket` module works only in native builds.

## Asset paths

Every asset path is relative to the package `content/` folder and never includes the `content/` prefix.

```lua
local assets = require('haylen.assets')

-- Reads `content/sprites/knight/idle.png` from the package.
local idle = assets.texture('sprites/knight/idle.png')
```

Paths use `/` as the separator and `.` segments are ignored. An absolute path raises `The path "<path>" must be relative.`, and a path that leaves the package raises `The path "<path>" must stay inside its root folder.` The same rule holds for every module that reads package files, such as `haylen.tiled`, `haylen.audio`, `haylen.localization` and `haylen.ui`. The [`haylen.assets`](lua-api/assets.md) reference covers caching, preload groups and asynchronous loading.

## Scenes

An app is a stack of scenes, and a scene is any Lua table with optional callbacks, such as `enter`, `exit`, `update`, `render` and `renderUi`, plus a `transparent` field for overlays and a `processMode` field for the game pause. Only the top scene updates and receives input, and the scenes below a transparent scene keep rendering. The engine looks callbacks up by name on every call and passes the table as `self`, so methods inherited through a metatable or a [class](#classes) work. A scene usually lives in its own module, and the [lifecycle guide](lifecycle.md) explains when each callback runs.

```lua
-- The file `source/scenes/title.lua`.
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local title = {}
title.__index = title

function title.new()
    return setmetatable({elapsed = 0}, title)
end

function title:update(dt)
    self.elapsed = self.elapsed + dt
    if input.pressed('confirm') then
        scene.replace(require('scenes.level').new(), {duration = 0.6, color = '#FF000000'})
    end
end

function title:renderUi()
    graphics2d.beginScreen()
    graphics2d.drawText(nil, 'Press start', 960, 540, {size = 72, color = '#FFFFFFFF', anchor = {0.5, 0.5}})
end

return title
```

The file `source/main.lua` then pushes it with `require('haylen.scene').push(require('scenes.title').new())`. The [`haylen.scene`](lua-api/scene.md) reference lists every callback, the stack operations, transitions and event tables. Gameplay reads input through the action map described in the [input guide](input.md), and menus and HUDs use the [UI guide](ui.md).

## Autoloads

Autoloads are modules that load before the first scene and live for the whole app. They hold what every scene shares, such as the player profile, the music or a server connection. The file `app.json` lists them in its `autoload` array, and they load in that order before `source/main.lua` runs. An autoload is the table its module returns, which `require` returns again anywhere and `haylen.autoloads.<name>` holds, where the name is the last part of the module in camel case. It receives `start`, `event`, `fixedUpdate`, `update`, `render`, `renderUi` and `stop` when it defines them, and its `processMode` field decides whether it runs while the game is paused. The function `haylen.autoload` adds one at run time. The [lifecycle guide](lifecycle.md#autoloads) explains when each callback runs.

```json
{
    "name": "Tiny Island",
    "identifier": "dev.haylen.tinyisland",
    "autoload": ["state.session"]
}
```

```lua
-- The file `source/state/session.lua`.
local session = {kills = 0, time = 0}

function session:update(dt)
    self.time = self.time + dt
end

function session:reset()
    self.kills, self.time = 0, 0
end

return session
```

```lua
local haylen = require('haylen')

local session = haylen.autoloads.session
session.kills = session.kills + 1
print('kills', require('state.session').kills) -- kills 1
```

## Classes

The function `haylen.class(name, parent, ...)` builds a class for app-wide types, such as units, items or scenes. It returns the class table, where methods are defined with `function Class:method()`. Calling the class, or its `new` function, creates an instance and calls its `init` method with the arguments.

- The argument `parent`, when given, must be another class. Instances inherit its methods, and the class copies the metamethods it has at that moment, such as `__eq`, `__lt` or `__add`, because Lua looks metamethods up without inheritance.
- The field `Class.super` is the parent, so an overriding method calls the one it overrides with `Class.super.method(self, ...)`.
- The method `object:is(Class)` returns whether the object is an instance of the class, of a class that inherits from it or of a class that includes it as a mixin. It works on classes too, such as `Hero:is(Unit)`.
- The remaining arguments are mixins: tables whose fields are copied into the class unless the class already defines them. The method `Class:include(mixin)` adds one later, and a mixin with an `included(class)` function hears about it.
- The field `Class.name` is the name, `tostring(Class)` reads `class <name>` and `tostring(object)` reads `<name>: <address>`.

The call raises `bad argument #1 to 'class' (string expected, got <type>)` without a name and `bad argument #2 to 'class' (class expected, got <type>)` for a parent that is not a class. The table `scene.Scene` of [`haylen.scene`](lua-api/scene.md#scene) is a class to build scenes on.

```lua
local haylen = require('haylen')

local Walker = {
    walk = function(self) return self.name .. ' walks' end,
    included = function(class) print(class.name .. ' can walk') end,
}

local Unit = haylen.class('Unit')

function Unit:init(name, health)
    self.name, self.health = name, health
end

function Unit:describe()
    return self.name .. ' ' .. self.health
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
    return 'hero ' .. Hero.super.describe(self)
end

local ana = Hero('Ana')
print(ana:describe(), ana:walk()) -- hero Ana 100 Ana walks
print(ana:is(Hero), ana:is(Unit), ana:is(Walker), Hero:is(Unit)) -- true true true true
print(ana == Hero.new('Ana'), tostring(Hero)) -- true class Hero
```

## Disk access

Apps read their own package through [`haylen.assets`](lua-api/assets.md), which works the same from a folder, a zip file or the browser, and they write only to the storage folder of the player, which every platform gives the app for itself.

The module [`haylen.storage`](lua-api/storage.md) reads and writes that folder synchronously. Its paths are relative to the folder and cannot leave it, writes are atomic and create missing folders, and it lists and removes files and keeps save slots. The function `storage.flush()` makes the files durable on platforms that buffer them, such as the browser, and the engine flushes by itself when the app goes to the background.

Varn's `fs` module reads and writes asynchronously, on the I/O pool, and returns promises that a coroutine awaits. The function `storage.root()` returns the absolute path of the storage folder for it. Use `fs` for large files, streaming, directories and anything that should not hold up a frame. Its paths are not confined, so build them from `storage.root()`.

| Task | `haylen.storage` | Varn `fs` |
| --- | --- | --- |
| Read a file | `storage.readText(path)`, `storage.readJson(path)` | `fs.readFile(path)`, or `fs.open(path, 'r')` to stream |
| Write a file | `storage.writeText(path, text)`, `storage.writeJson(path, value)` | `fs.writeFile(path, data)`, `fs.append(path, data)` |
| List files | `storage.list(folder)`, every file below the folder | `fs.readdir(path)`, the entries of one folder |
| Create a folder | Implicit on write | `fs.mkdir(path)` |
| Remove | `storage.remove(path)` | `fs.removeRecursive(path)` |
| Other | `storage.exists(path)`, `storage.flush()`, save slots | `fs.exists(path)`, `fs.stat(path)`, `fs.rename`, `fs.copy` |

```lua
local async = require('async')
local fs = require('fs')
local storage = require('haylen.storage')

storage.writeJson('saves/profile.json', {name = 'Ana', coins = 12})
print(table.concat(storage.list('saves'), ', ')) -- saves/profile.json

async.spawn(function()
    local cache = storage.root() .. '/cache/maps'
    fs.mkdir(cache):await()
    fs.writeFile(cache .. '/island.json', '{"size": 64}'):await()
    local names = fs.readdir(cache):await()
    local profile = fs.readFile(storage.root() .. '/saves/profile.json'):await()
    print(names[1], #profile > 0) -- island.json true
    fs.removeRecursive(storage.root() .. '/cache'):await()
    print(storage.exists('cache/maps/island.json')) -- false
end)
```

## Asynchronous code

Operations that take time return a Varn promise instead of blocking the frame, such as `assets.loadAsync`, `assets.preload`, `platform.call`, `jobs.spawn` and `handle:wait()` of a tween, together with Varn's own `async.sleep` and `http` requests. A coroutine started with `async.spawn` waits for a promise with `:await()`. The coroutine pauses and the app keeps running, and the coroutine resumes at the start of a later frame, before the updates of that frame.

The method `:await()` never raises. It returns the value when the promise resolves, and `nil` and the error message when it rejects, so always check the result.

```lua
local async = require('async')
local assets = require('haylen.assets')
local log = require('haylen.log')

async.spawn(function()
    local texture, failure = assets.loadAsync('images/hero.png'):await()
    if not texture then
        log.warning('the hero could not be loaded: ' .. failure)
        return
    end
    hero = texture
end)
```

The method `:await()` only works inside a coroutine, so code at the top level of `source/main.lua` or in a scene callback starts one with `async.spawn`, or with `scene.spawn(owner, fn)` of [`haylen.scene`](lua-api/scene.md#scenespawnowner-fn) for a task that stops for good when its owner, such as its scene, goes away. The `load` hook of a scene already runs in such a task. The functions `async.spawn` and `async.run` return the handle of the task, whose `task.cancel()` stops it for good: no promise resumes it again, and its coroutine closes, which runs its to-be-closed variables, at once when the task waits and right after its next `:await()` when the task cancels itself. A to-be-closed variable that raises while its task closes stops the app with the error screen like a failed task. An error that escapes the function given to `async.spawn` or `async.run` stops the app and shows the error screen with the stack of that coroutine, like an error in a scene callback, and a web page receives it through `onError`. The engine receives these errors through the handler it sets with `async.onFailure`, together with the errors of `ffi` callbacks that fail outside any `ffi` call and the stack of each error as a list of frames, so an app that sets a handler of its own takes them over and they no longer stop the app. Code that expects a failure catches it with `pcall` inside the coroutine, and a promise that rejects makes `:await()` return `nil` and the error instead of raising.

Long computations written in Lua, such as generating a map, run as jobs of [`haylen.jobs`](lua-api/jobs.md). The Lua state belongs to the frame thread, so a job is a coroutine that works within a time budget each frame and pauses at `jobs.checkpoint()` when the budget is spent. The function `jobs.spawn` returns a promise with the result of the job.

```lua
local async = require('async')
local jobs = require('haylen.jobs')

async.spawn(function()
    local total = jobs.spawn(function(count)
        local sum = 0
        for index = 1, count do
            sum = sum + index
            jobs.checkpoint()
        end
        return sum
    end, 1000000):await()
    print('total', total)
end)
```

Engine work written in C++, such as decoding images and sounds, already runs on worker threads and needs no jobs.

The `timeoutSeconds` option of Varn's `http` requests is one deadline for the whole request, from connecting to the last byte of the answer and across every redirect, so a server that keeps sending a little at a time still ends it on time. It takes any positive number of seconds, a fraction such as `2.5` included, and a request with any other value fails with a message that names the option. A request past its deadline rejects with a message that says it did not finish within its timeout, and a secure request never waits for the server to confirm the close of its connection once the answer is whole. The function `async.timeout` gives up waiting on any promise instead, while the request it waits for goes on until its own deadline.

```lua
local async = require('async')
local http = require('http')

async.spawn(function()
    local response, failure = http.client.get('https://httpbin.org/delay/5', {timeoutSeconds = 2.5}):await()
    if not response then
        print('no answer within 2.5 seconds: ' .. failure)
        return
    end
    print(response.status)
end)
```

## Errors and stack traces

An uncaught error in `source/main.lua`, in a scene callback, in a timer or tween callback, in a task of `async.spawn` or `async.run` or in any other engine callback stops the app. The engine keeps the first error, writes its report to the log, stops updating and rendering the scenes and shows the error screen: the app name and version, the platform and the engine version, the message, the file and line, the lines of the script around the error line and the stack of the failing call, innermost first.

```text
The app stopped with an error
Tiny Island 1.0.0 · macos · Haylen 0.0.1

attempt to index a nil value (local 'enemy')
File "source/scenes/level.lua", line 12

  10  function level:update(dt)
  11      local enemy = self.enemies[1]
> 12      if enemy.health <= 0 then
  13          self:win()
  14      end

Stack
source/scenes/level.lua:12  function <source/scenes/level.lua:10>
```

That report is also what `C` copies to the clipboard, and `R` restarts the app. Both actions are also buttons at the bottom of the screen, which a click or a tap presses, and on touch devices the buttons show without their keys. Gamepads and TV remotes move an outlined focus between the buttons with the directional pad or the arrows of the remote and press the focused one with the south button or the select button of the remote, and the focus starts on the restart button. A report taller than the screen scrolls with the mouse wheel, a drag of the mouse or a finger, the arrow and page keys, Home and End, and the directional pad up and down. The stack names each function the way Lua does, such as `local 'spawnWave'`, `method 'hit'` or `main chunk`, and a function that native code calls, like a scene callback or the function of a task, shows where it is defined, such as `function <source/scenes/level.lua:10>`. The stack of a task of `async.spawn` or `async.run` and of an `ffi` callback comes from Varn and reads the same. A deep stack, such as a runaway recursion, keeps its innermost and its outermost calls around one line that says how many levels it skipped, such as `...  140 levels skipped`. A to-be-closed variable that fails while a cancelled task closes shows its error without a stack, because the stack of the task is gone by then.

Chunk names are package paths, so every position in a message points at a file of the package. The function `haylen.reportError(message)` stops the app the same way with a message of the app's choosing and the stack of the code that reported it. In the browser, the page receives the error in `Module.haylen.onError` as `{message, file, line, traceback, frames}`, where `traceback` is the stack as text and `frames` lists every frame as `{source, line, function, kind}` with the kind `lua`, `c` or `main`. On other platforms the report goes to the console with the rest of the log. The native parts of [plugins](plugins.md#errors-of-the-app) receive the same report, so a crash reporter records it with the stack of the Lua code.

The runtime stays alive on the error screen. Fixing the script restarts the app when hot reload is on, and a browser editor restarts it with `Module.haylen.restart()` or `Module.haylen.run()`.

An app that can return to a safe screen after an error calls [`haylen.setRecoverable(true)`](lua-api/haylen.md#haylensetrecoverablerecoverable). Its error screen then offers `Back to the app` next to copying and restarting, with Escape, the east button and the back button of the platform, and it starts with the focus on it. Going back publishes `appRecovered` with the error at the start of the next frame, before the app updates again, so its listener leaves the scene that failed first, and the app runs on with the same Lua state. Every error also publishes `appError` at the end of its frame, which an app that runs on by itself answers with `haylen.recover()`, the way the test project of the samples runs every test and lists the ones that failed.

```lua
local events = require('haylen.events')
local haylen = require('haylen')
local scene = require('haylen.scene')

haylen.setRecoverable(true)
events.on('appRecovered', function()
    scene.clear()
    scene.push(require('scenes.menu')())
end)
```

## Hot reload

The desktop player treats a package folder named on its command line as an app in development when it also receives `--dev`, as in `haylen --dev samples/games/tiny-island` or `python3 haylen.py run samples/games/tiny-island`. It checks `app.json`, `source/`, `content/` and the `plugin.json` and `source/` of every plugin for changes every half second on the I/O pool, so frames never wait for the file system, and ignores everything else in the folder.

- A changed file under `source/`, a changed `app.json` or a changed `plugin.json` or Lua module of a plugin restarts the app with a fresh Lua state, and `source/main.lua` runs again. This also works from the error screen.
- A changed file under `content/` reloads the assets read from it without a restart. Textures change in place, so sprites that already use them show the new pixels, and other asset types leave the cache so the next load reads the new file.

A restart starts the app from scratch, so state that must survive a reload belongs in [`haylen.storage`](lua-api/storage.md) or [`haylen.preferences`](lua-api/preferences.md). Zip packages and shipped apps are never watched. In the browser, an editor sends changed files with `Module.haylen.setFile` and then calls `Module.haylen.reloadAsset(path)` for an asset or `Module.haylen.run()` for scripts, as the [architecture guide](architecture.md#the-web-runtime) describes.

## Performance

Scripts run on the frame thread, so the Lua work spent on each item decides how many items an app moves every frame. A call from Lua into the engine costs far more than plain Lua arithmetic, and a table read field by field on the C++ side costs more still, so the fast paths avoid both for every item and let C++ do the per-item work.

- **Native properties.** Number, `Vec2` and `Color` properties of engine objects, such as the position of a sprite, the zoom of a camera or the transform of a UI node, are animated by [`haylen.tween`](lua-api/tween.md#native-properties) in C++ without running Lua every frame.
- **Numbers over values.** A number field such as `sprite.x` reads and writes a float, while `sprite.position` creates a `Vec2` for every read. Hot loops read numbers.
- **Float buffers.** A float buffer of [`haylen.collections`](lua-api/collections.md#float-buffers) holds numbers that Lua and C++ share without copying. The expression `buffer[index]` reads and writes one value, and `buffer:set(first, list)` copies a whole Lua array in one call.
- **Bulk APIs.** The function `graphics2d.drawBatch(texture, buffer, layout)` draws the sprites a buffer holds without a table for each one, `SpriteBatch:resize` fills a large batch in one call, `SpriteBatch:writeFields` moves every sprite of a batch from a buffer, `world:readTransforms` and `world:writeTransforms` of [`haylen.physics2d`](lua-api/physics2d.md) move the transforms of many bodies, and `emitter:readPositions` of [`haylen.particles2d`](lua-api/particles2d.md) reads every particle.
- **Measure.** The functions `debug.profile` and `debug.beginScope` of [`haylen.debug`](lua-api/debug.md) time a piece of script, `debug.stats()` counts objects and memory, and the compact statistics show the frame time and draw calls while the app runs.

```lua
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

-- Plain Lua arrays for the math, one copy into the shared buffer, and one call to move every sprite.
local count = 50000
local positions, speeds = {}, {}
for index = 1, count do
    positions[index * 2 - 1], positions[index * 2] = math.random(0, 1920), math.random(0, 1080)
    speeds[index] = math.random(40, 120)
end
local buffer = collections.newFloatBuffer(count * 2)
local fields = {'x', 'y'}
local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:resize(count, {width = 3, height = 3, color = '#FFB0E0FF'})

scene.push({
    update = function(self, dt)
        for index = 1, count do
            local slot = index * 2
            positions[slot] = (positions[slot] + speeds[index] * dt) % 1080
        end
        buffer:set(1, positions)
        batch:writeFields(buffer, fields)
    end,
    render = function(self)
        graphics2d.beginScreen()
        batch:draw()
    end,
})
```

The command `python3 haylen.py bench --suite lua` runs a Lua bunnymark, `engine/bench/lua-benchmark`, on the headless host: every sprite falls and bounces off the edges of the screen, and the benchmark times Lua moving and drawing them in five ways. The mode `tables` keeps one table for each sprite and draws the list with `graphics2d.drawBatch(texture, sprites)`. The mode `calls` keeps the same tables and draws each one with its own `graphics2d.draw(texture, x, y, sprite)`. The mode `buffer` keeps the positions in a float buffer written one value at a time with `buffer[index]` and draws the buffer. The mode `bulk` keeps the positions in a plain Lua array, copies it into the buffer with one `buffer:set(1, positions)` and draws the buffer. The mode `batch` does the same update and moves a sprite batch with `batch:writeFields`. These are the average CPU milliseconds of one frame on an Apple M5 Pro in Release, where the draw column is the recording of the draw calls, including the conversion of the sprites on worker threads, and the frame column covers the whole frame of the engine.

| Sprites | Mode | Update | Draw | Frame |
| --- | --- | --- | --- | --- |
| 10,000 | `tables` | 0.38 | 0.85 | 1.25 |
| 10,000 | `calls` | 0.39 | 2.35 | 2.84 |
| 10,000 | `buffer` | 1.36 | 0.09 | 1.47 |
| 10,000 | `bulk` | 0.42 | 0.08 | 0.50 |
| 10,000 | `batch` | 0.42 | 0.08 | 0.52 |
| 100,000 | `tables` | 3.62 | 8.84 | 12.51 |
| 100,000 | `calls` | 3.75 | 23.80 | 28.25 |
| 100,000 | `buffer` | 13.59 | 1.25 | 14.90 |
| 100,000 | `bulk` | 4.20 | 1.29 | 5.53 |
| 100,000 | `batch` | 4.21 | 1.30 | 5.56 |
| 1,000,000 | `tables` | 37.14 | 86.32 | 123.53 |
| 1,000,000 | `calls` | 36.68 | 238.05 | 281.34 |
| 1,000,000 | `buffer` | 134.54 | 8.13 | 142.74 |
| 1,000,000 | `bulk` | 41.44 | 8.19 | 49.71 |
| 1,000,000 | `batch` | 41.01 | 10.85 | 51.92 |

Plain Lua arithmetic on tables is the fastest way to update, but reading a table for every sprite makes the draw call about ten times slower than reading a buffer, and a `graphics2d.draw` call for every sprite costs about a quarter of a microsecond, almost three times the cost of the same tables in one `drawBatch`. The engine reads an option table in one pass over the keys it holds, so a table costs in proportion to its keys and not to the options a function knows: pass only the keys that differ from the defaults. Every `buffer[index]` access is a call into the engine of about 25 nanoseconds, against a few nanoseconds for an array, so a loop that touches every value through the buffer spends three times longer updating. Doing the math in plain Lua arrays and handing them over with one `buffer:set` combines both strengths: a frame of 100,000 moving sprites costs about 5.5 milliseconds, less than half of what tables cost. Keep `buffer[index]` for values that C++ fills, such as the transforms `world:readTransforms` writes, where Lua reads only a few of them.

## Extending the engine from C++

A C++ project can add its own Lua modules, backed by its own plugin, while the app itself stays in Lua. The public binding toolkit in `engine/include/haylen/lua/` is the same one every built-in module uses, and everything in it lives in the `haylen::lua` namespace.

| Header | What it provides |
| --- | --- |
| `Stack.hpp` | `Stack::push`, `Stack::read` and `Stack::is`, which move C++ values on and off the Lua stack through their converters. |
| `Userdata.hpp` | `Userdata::check`, `Userdata::checkShared`, `Userdata::test` and `Userdata::emplace` for bound objects, `Userdata::equal` for an `__eq` that compares the bound objects, and `Userdata::pushField`, `Userdata::setField` and `Userdata::pushFunction` for storing Lua callbacks on userdata. |
| `Type.hpp` | The `Type<T>` trait that binds a C++ type as userdata. |
| `Converter.hpp` | The `Converter<T>` trait with the conversions of scalars, strings, optionals, vectors and bound types. |
| `EnumNames.hpp` | The `EnumNames<T>` trait for enums passed as strings. Every engine enum has one table of its names, which its specialization and the file parsers share, usually owned by its class, such as `Texture::filterFromName` and `Texture::filterName`. |
| `Table.hpp` | `Table::checkFields` and `Table::readField` for option tables whose unknown keys are errors, and `Table::readFields` with `Table::readValue`, which read an option table in one pass over the keys it holds, for options read on every draw. |
| `Binding.hpp` | `Binding::preload`, which adds a module to the Varn runtime of the engine, `Binding::newModule` and the `Binding::native`, `Binding::function` and `Binding::method` wrappers. |
| `ClassBuilder.hpp` | `ClassBuilder` for metatables with methods and properties. |
| `Runtime.hpp` | `Runtime::getEngine(L)` to reach the engine from a binding, `Runtime::getMainThread` for callbacks that outlive their coroutine, `Runtime::protectedCall`, `Runtime::protectedRun`, `Runtime::runChunk`, `Runtime::runReporting`, `Runtime::reportError` and `Runtime::captureError`. |
| `Reference.hpp` | `Reference`, which keeps a Lua value alive from C++. |
| `JsonConverter.hpp` | `JsonConverter::push` and `JsonConverter::read` to move JSON between C++ and Lua tables. |
| `Promise.hpp` | `Promise`, a result that a Lua coroutine waits for with `:await()` and that native code settles later, from any thread, with `Promise::isSettled`, and `Promise::isPromise` to recognize any Varn promise on the stack. |
| `TypeConverter.hpp` | The bindings of engine types such as `math::Vec2`, `math::Rect`, `math::Color`, `graphics::Texture` and `graphics2d::Sprite`, so bindings accept and return them. |
| `Error.hpp` | `Error`, the exception a failed protected call throws, with the message, the script position and the frames of the stack. |
| `Application.hpp` | `Application`, the application that runs `source/main.lua`. |

A plugin registers its modules in `installLua` with `Binding::preload`, so `require` builds the module table the first time a script asks for it. The function adds the module to the Varn runtime of the engine with `varn::runtime::Runtime::addModule`, and a name that a module of Varn, such as `json`, or of another plugin already has raises `std::runtime_error` with `The Lua module "<name>" cannot be added, because a module of Varn or of another plugin already has its name. Give the module another name.`, which the error screen shows. This complete source file adds an `app.score` module to a Lua app. Like the built-in modules, the plugin keeps its Lua entry points as private static methods.

```cpp
#include <algorithm>
#include <memory>
#include <string_view>

#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace app {

// Keeps the best score of the session and exposes it to Lua as `app.score`.
class ScorePlugin final : public haylen::plugins::Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "score";
    }

    void installLua(haylen::core::Engine&, lua_State* L) override {
        haylen::lua::Binding::preload(L, "app.score", &open);
    }

    int submit(int points) {
        best = std::max(best, points);
        return best;
    }

    [[nodiscard]] int getBest() const noexcept {
        return best;
    }

  private:
    static ScorePlugin& getPlugin(lua_State* L) {
        return haylen::lua::Runtime::getEngine(L).getPlugin<ScorePlugin>();
    }

    static int luaSubmit(lua_State* L) {
        haylen::lua::Stack::push(L, getPlugin(L).submit(haylen::lua::Stack::read<int>(L, 1)));
        return 1;
    }

    static int luaBest(lua_State* L) {
        haylen::lua::Stack::push(L, getPlugin(L).getBest());
        return 1;
    }

    static int open(lua_State* L) {
        const luaL_Reg functions[] = {
            {"submit", &haylen::lua::Binding::native<&luaSubmit>},
            {"best", &haylen::lua::Binding::native<&luaBest>},
            {nullptr, nullptr},
        };
        haylen::lua::Binding::newModule(L, functions);
        return 1;
    }

    int best = 0;
};

// Registers the plugin, then runs `source/main.lua` exactly like an app written only in Lua.
class App final : public haylen::core::Application {
  public:
    void start(haylen::core::Engine& engine) override {
        engine.addPlugin(std::make_unique<ScorePlugin>());
        script.start(engine);
    }

  private:
    haylen::lua::Application script;
};

} // namespace app

std::unique_ptr<haylen::core::Application> haylen::core::Application::create() {
    return std::make_unique<app::App>();
}
```

Scripts use the module like any other.

```lua
local score = require('app.score')

score.submit(120)
print('best score', score.best())
```

The app is built with `haylen_add_app` and the `CPP` option, which tells it that the sources define `haylen::core::Application::create()`.

```cmake
haylen_add_app(my-app CPP
  SOURCES src/App.cpp
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

The template `Binding::native<&f>` wraps a function that takes the Lua state, so a C++ exception thrown inside it, such as the `std::logic_error` of a missing plugin, becomes a Lua error with the same message. The template `Binding::function<&f>` goes one step further and converts the parameters and the result of a plain C++ function or static method, so `static int clampScore(int points)` is exposed with `&haylen::lua::Binding::function<&clampScore>`.

A C++ type becomes a Lua type with a `Type<T>` specialization, which names its metatable and says whether Lua holds the value itself or a `std::shared_ptr` to it, and a `ClassBuilder` that fills the metatable in `installLua`. The metatable is protected like the ones of the engine, so `getmetatable` returns the type name and Lua code never reaches the finalizer. A binding that creates a userdata metatable of its own does it with `Userdata::newMetatable`, which protects it the same way.

```cpp
namespace app {

struct Wallet {
    int coins = 0;

    void add(int amount) {
        coins += amount;
    }
};

} // namespace app

namespace haylen::lua {

template <> struct Type<app::Wallet> {
    static constexpr const char* name = "app.Wallet";
    using Storage = app::Wallet;
};

} // namespace haylen::lua

namespace app {

class EconomyPlugin final : public haylen::plugins::Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "economy";
    }

    void installLua(haylen::core::Engine&, lua_State* L) override {
        haylen::lua::ClassBuilder<Wallet>(L).method<&Wallet::add>("add").field<&Wallet::coins>("coins").install();
        haylen::lua::Binding::preload(L, "app.economy", &open);
    }

  private:
    static int newWallet(lua_State* L) {
        haylen::lua::Userdata::emplace<Wallet>(L);
        return 1;
    }

    static int open(lua_State* L) {
        const luaL_Reg functions[] = {
            {"newWallet", &haylen::lua::Binding::native<&newWallet>},
            {nullptr, nullptr},
        };
        haylen::lua::Binding::newModule(L, functions);
        return 1;
    }
};

} // namespace app
```

Once the application adds `EconomyPlugin` the way it adds `ScorePlugin`, scripts create wallets and use their methods and fields.

```lua
local wallet = require('app.economy').newWallet()
wallet:add(5)
print(wallet.coins)
```

Reading a member the type does not have raises `The type "app.Wallet" has no member "<name>".`, and assigning one without a setter raises `The type "app.Wallet" has no writable property "<name>".`.

A binding that finishes later returns a `lua::Promise`. The binding creates it, pushes it as its result and settles it once with `resolve` (a JSON value that arrives in Lua as plain tables), `resolveWith` (a function that pushes one value on the frame thread) or `reject` (a message that `:await()` returns as its second value). It may be settled from a worker thread, and the waiting coroutine resumes on the frame thread in a later `Runtime::poll()` of Varn. A JSON value that Lua cannot hold, which is binary JSON or JSON nested more than 128 levels deep, rejects the promise with the reason instead, so its coroutines still resume.

```cpp
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"

namespace app {

// Binds `app.text`, whose `countWords` counts the words of a text on a worker thread and returns a promise for the count.
class TextLua final {
  public:
    static int countWords(lua_State* L) {
        std::string text = luaL_checkstring(L, 1);
        haylen::core::Engine& engine = haylen::lua::Runtime::getEngine(L);
        const haylen::lua::Promise promise(engine);
        engine.getJobs().post([promise, text = std::move(text)] {
            int words = 0;
            bool inWord = false;
            for (const char character : text) {
                const bool letter = character != ' ' && character != '\n';
                words += letter && !inWord ? 1 : 0;
                inWord = letter;
            }
            promise.resolve(words);
        });
        promise.push(L);
        return 1;
    }
};

} // namespace app
```

```lua
local async = require('async')
local text = require('app.text')

async.spawn(function()
    print(text.countWords('one small island'):await())
end)
```

A few rules keep extensions safe.

- Every hook and binding runs on the frame thread, and long work goes to `engine.getJobs()` as the [architecture guide](architecture.md#threads-and-async) describes.
- A Lua callback kept by C++ lives in a `lua::Reference`, is called on the main Lua thread and is run inside `lua::Runtime::runReporting`, so a failure reaches the error screen instead of the native code that triggered it.
- The plugin releases every `lua::Reference` it holds in `stop`, because the Lua state closes before plugins are destroyed.
- Project modules use a namespace of their own, such as `app.score`, and leave `haylen.` to the engine.
