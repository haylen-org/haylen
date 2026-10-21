# 🌙 Lua guide

This guide is the first page to read after the [quick start](../../README.md#quick-start). It walks you through how a Haylen app is made in Lua: the app package, `app.json`, modules, scenes, owners, asynchronous code, errors and hot reload. Each section explains one idea with a short example and links to the reference for the details. The last sections cover performance and how a C++ project adds its own Lua modules.

Haylen apps are written in Lua 5.5. The engine runs them through [Varn](https://github.com/varn-org/varn), a Lua runtime with an event loop, worker threads and modules for the network and the file system. Every engine module has a reference page in the [Lua API reference](../lua-api/README.md), and the [architecture guide](architecture.md) explains what happens underneath.

## The app package

An app is a package. A package is a folder, or the same folder zipped, that holds these entries:

- `app.json` at the root, with the settings of the app.
- `source/`, with the Lua modules of the app.
- `content/`, with the assets: images, sounds, fonts, maps and data files.
- `plugins/`, with the manifest and the Lua modules of every [plugin](../platform/plugins.md) that the app uses.

Nothing else in the folder belongs to the package. So platform projects, notes, build files and the native parts of plugins can live next to it.

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
    ads/
      plugin.json      The manifest of a plugin that app.json lists.
      source/
        init.lua       Loaded with require('ads').
```

Lua files take `dash-case` names, such as `main-menu.lua`. Asset files and folders take lowercase names with dashes, such as `audio/fire-loop.ogg`.

The desktop player runs a folder with `haylen my-app` and a zip file with `haylen my-app.zip`. To make the zip file, run `haylen package my-app -o my-app.zip`. It zips `app.json`, `source/`, `content/` and the `plugin.json` and `source/` of every plugin, with `app.json` at the root of the archive. The [distribution guide](../tools/distribution.md) shows how to run a package on every platform and in the browser.

## app.json

The file `app.json` holds the settings that the engine needs before any Lua runs, such as the size of the window. The runtime reads it before the window exists. Every field is optional, so an app may start with a name and an identifier and add the rest when it needs it.

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

These are the parts of `app.json` that most apps set.

| Part | What it sets |
| --- | --- |
| `name`, `identifier`, `version` | Who the app is. The `identifier` also names the folder where the app keeps the files of the player. |
| `window` | The size and title of the window, fullscreen, and the desktop modes, such as a window without a frame. |
| `design` | The design resolution: the space of units that the app draws in, whatever the size of the screen. |
| `orientation`, `splash` | How a phone holds the app, and the launch screen. |
| `lifecycle`, `audio` | What the app does when it loses the focus, and how it shares sound with other apps. |
| `debug` | The debug statistics, drawings and the reload mode of development. |
| `autoload`, `plugins` | The modules that load before the first scene, and the plugins of the app. |

The reference of [`haylen.config`](../lua-api/haylen.md#haylenconfig) lists every field with its default. Scripts read the same settings, with every default filled in, from `require('haylen').config`.

The engine checks the file strictly. An unknown key is an error, so a misspelled field never goes unnoticed. A value of the wrong type or out of its range is an error too. A package whose `app.json` fails shows the reason on the screen instead of the app, and the reference lists [every message](../lua-api/haylen.md#errors-of-appjson).

## source/main.lua

The file `source/main.lua` is the entry point of the app. It runs once when the engine starts, after every plugin has installed its modules and the autoloads have started, and before the first frame. It usually loads shared data, sets up input and the theme of the interface, and shows the first scene. This is the whole entry point of the sample Tiny Island.

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

Split an app into modules: one Lua file per scene, system or kind of object. The function `require(name)` loads a module once and returns what the module returns. Later calls with the same name return the same value.

`require` looks for a module in this order:

1. A module that is already loaded. Varn's modules are loaded from the start.
2. A module of `package.preload`. These are the engine modules, such as `haylen.graphics`, and the modules that the plugins of a C++ project add.
3. A module of the app, in the `source/` folder. The name `scenes.main-menu` loads `source/scenes/main-menu.lua`, and when that file is missing, `source/scenes/main-menu/init.lua`.

A module whose first part is the id of a plugin in `app.json` comes from the `source/` folder of that plugin instead. So `require('ads')` loads `plugins/ads/source/init.lua`, and `require('ads.consent')` loads `plugins/ads/source/consent.lua`. A module of the app named after a plugin is therefore an error when the app starts, as the [plugin guide](../platform/plugins.md#modules) explains.

```lua
-- The file `source/config.lua`.
return {startLevel = 'forest', lives = 3}
```

```lua
local config = require('config')

print('The game starts in the ' .. config.startLevel .. ' with ' .. config.lives .. ' lives.')
```

A missing module raises `no file 'source/scenes/missing.lua' or 'source/scenes/missing/init.lua' in the app package`. The fields `package.path` and `package.cpath` are empty, so nothing ever loads from the file system of the computer. An app therefore behaves the same from a folder, a zip file or the browser.

The first time `require` loads a module, it returns a second value. For a module of the package that value is the path of its file. When `require` is the last argument of another call, wrap it in parentheses, as in `scene.push((require('scenes.level')))`. Otherwise the path becomes an extra argument of that call.

### Text only

Modules load as Lua text. The one exception is a protected release, where the Lua modules of the app and its plugins ship as bytecode, compiled by the release build. Only the module loader of the engine loads that bytecode, as the [content guide](../content/content.md#lua-bytecode) describes. Errors name the same files and lines either way.

App code itself loads chunks only as text:

- The functions `load` and `loadfile` take only text. A mode other than `'t'` raises `chunks load only as text, so the mode is 't'`, and a binary chunk fails to load.
- The function `dofile` runs only text files.
- The function `string.dump` does not exist.

The reason is safety. The Lua virtual machine never checks precompiled bytecode, and bad bytecode can build values that crash it. So the only bytecode that ever runs is the bytecode of a protected release, whose signed and encrypted catalog vouches for every module.

### Engine values are protected

The values that the engine hands to Lua, such as sprites and textures, have protected metatables. A metatable is the hidden table that gives a Lua value its methods and operators.

- The function `getmetatable(value)` returns the name of the type, such as `'haylen.Sprite'`. Use it to tell engine values apart.
- The function `setmetatable` cannot replace their metatables, and neither can the debug library. The function `debug.getmetatable` returns the same as `getmetatable`, and `debug.setmetatable` raises `The metatable of this value is protected and cannot be changed.` for an engine value.
- The functions `debug.getregistry`, `debug.getuservalue` and `debug.setuservalue` do not exist.
- The functions `debug.getupvalue` and `debug.setupvalue` see no upvalues in native functions.
- The frames of native functions show no locals to `debug.getlocal`, since their stack slots hold the state of the engine while they run. They refuse `debug.setlocal` with `The locals of native functions cannot be changed.`, and `debug.getinfo` leaves `func` out for them.

So no script can finalize an engine value by hand or give it the members of another type, and no internal function of the engine runs with arguments it did not expect.

### The modules of Varn

Varn's modules live in the same Lua state: `async`, `http`, `socket`, `json`, `fs`, `zip`, `crypto`, `log`, `platform`, `process`, `datetime`, `xml` and `ffi`. The [Varn documentation](https://github.com/varn-org/varn) describes them. Two of them share a name with an engine module, so pick the right one: apps log with `haylen.log` and call native code with `haylen.platform`, which are not Varn's `log` and `platform`. For files of the player, use `haylen.storage`, as [files of the player](#disk-access) shows. Browsers have no raw TCP, so the `socket` module works only in native builds, as the [networking guide](../network/networking.md) explains with the other limits of the browser.

## Asset paths

Every asset path is relative to the `content/` folder of the package and never starts with `content/`.

```lua
local assets = require('haylen.assets')

-- Reads `content/sprites/knight/idle.png` from the package.
local idle = assets.texture('sprites/knight/idle.png')
```

Paths use `/` as the separator, and `.` segments are ignored. An absolute path raises `The path "<path>" must be relative.`, and a path that leaves the package raises `The path "<path>" must stay inside its root folder.`. The same rule holds for every module that reads files of the package, such as `haylen.tiled`, `haylen.audio`, `haylen.localization` and `haylen.ui`. The [`haylen.assets`](../lua-api/assets.md) reference covers the cache, preload groups and loading in the background.

## Scenes

An app shows one screen at a time, such as a title screen, a level or a pause menu. Each screen is a scene, and the scenes form a stack. The scene on top runs, and pushing a scene on top of another, such as a pause menu over a level, keeps the one below alive.

A scene is any Lua table with optional hooks: functions that the engine calls at the right moment, such as `enter`, `exit`, `update`, `render` and `renderUi`. Two fields change how a scene behaves:

- `transparent = true` keeps the scenes below drawing, for an overlay such as a pause menu.
- `processMode` decides whether the scene runs while the game is paused, as the [lifecycle guide](lifecycle.md#pause-and-process-modes) explains.

Only the top scene updates and receives input. The engine looks each hook up by name on every call and passes the table as `self`, so methods that a table inherits through a metatable or a [class](#classes) work too. A scene usually lives in a module of its own.

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

Then `source/main.lua` shows it with `require('haylen.scene').push(require('scenes.title').new())`.

A scene can also load what it needs in its `load` hook before it shows, with progress and an optional loading view. The [lifecycle guide](lifecycle.md#scene-lifecycle) explains the states of a scene and when each hook runs, and the [`haylen.scene`](../lua-api/scene.md) reference lists every hook, stack operation and transition. Gameplay reads input through the action map of the [input guide](../input/input.md), and menus and HUDs use the [UI guide](../ui/ui.md).

## Autoloads

Some things live longer than any scene, such as the profile of the player, the music or a connection to a server. Make such a module an autoload: list it in the `autoload` array of `app.json`, and it loads before `source/main.lua` runs and lives for the whole app. An autoload is the table that its module returns. It receives hooks like a scene, such as `start`, `update` and `stop`.

```json
{
    "name": "Tiny Island",
    "identifier": "com.haylen.tinyisland",
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

Any module reaches the autoload through `require` or through `haylen.autoloads`, under the last part of its module name in camel case.

```lua
local haylen = require('haylen')

local session = haylen.autoloads.session
session.kills = session.kills + 1
print(require('state.session').kills) -- 1
```

The [lifecycle guide](lifecycle.md#autoloads) lists the hooks and when they run, and [`haylen.autoload`](../lua-api/haylen.md#haylenautoloadname-module) adds an autoload while the app runs. The [loading guide](../content/loading.md#autoloads) explains how autoloads differ from preloads and how a release ships them.

## Classes

Lua has no classes of its own, but games have many objects of the same kind, such as enemies, items or scenes. The function `haylen.class(name, parent, ...)` builds a class: a table of methods that every object of the class shares. A class can inherit from a parent class and take mixins, which are tables of methods that several classes share. Calling the class creates an object and runs its `init` method.

```lua
local haylen = require('haylen')

local Unit = haylen.class('Unit')

function Unit:init(name, health)
    self.name, self.health = name, health
end

function Unit:describe()
    return self.name .. ' has ' .. self.health .. ' health.'
end

local Hero = haylen.class('Hero', Unit)

function Hero:init(name)
    Hero.super.init(self, name, 100)
end

local ana = Hero('Ana')
print(ana:describe()) -- Ana has 100 health.
print(ana:is(Unit)) -- true
```

The [`haylen.class`](../lua-api/haylen.md#haylenclassname-parent-) reference lists every member of a class, such as `super`, `is` and `include`. The table `scene.Scene` of [`haylen.scene`](../lua-api/scene.md#scene) is a class to build scenes on.

## Owners

A listener, a timer or a tween that outlives the thing it belongs to keeps running code for something that is gone, such as a scene that the player left. Owners prevent that. Every function that registers a callback takes an `owner` option. When the owner ends, everything registered with it ends too.

```lua
local events = require('haylen.events')
local scene = require('haylen.scene')
local timer = require('haylen.timer')

local level = {}

function level:enter()
    -- The listener and the timer belong to the level, so they end when it unloads.
    events.on('coinCollected', function(value) print('A coin of ' .. value .. ' points.') end, {owner = self})
    timer.every(1, function() print('The level ticks.') end, {owner = self})
end

scene.push(level)
```

A scene ends when it unloads, and any other table ends when the garbage collector frees it. The [lifecycle guide](lifecycle.md#subscription-scopes) lists every kind of owner and every function that takes one.

## Disk access

An app reads its own package through [`haylen.assets`](../lua-api/assets.md), which works the same from a folder, a zip file or the browser. It writes only to the storage folder of the player, which every platform gives the app for itself. Two modules write there.

The module [`haylen.storage`](../lua-api/storage.md) reads and writes that folder right away, on the frame thread. Each of its file and slot functions also has an `Async` version that runs on the I/O pool, a set of worker threads for slow input and output, as [calls on the I/O pool](../lua-api/storage.md#calls-on-the-io-pool) describes. Its paths are relative to the folder and cannot leave it. Its writes are atomic, which means a crash never leaves half a file, and they create missing folders. It also lists and removes files and keeps save slots. The function `storage.flush()` makes the files durable on platforms that buffer them, such as the browser. The engine flushes by itself when the app goes to the background.

The module `fs` of Varn reads and writes in the background, on the I/O pool too. It returns promises that a task awaits, as [asynchronous code](#asynchronous-code) explains. Use `fs` for large files, for streaming, for folders and for anything that should not hold up a frame. Its paths are not confined, so build them from `storage.root()`, the absolute path of the storage folder.

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

storage.writeJson('data/profile.json', {name = 'Ana', coins = 12})
print(table.concat(storage.list('data'), ', ')) -- data/profile.json

async.spawn(function()
    local cache = storage.root() .. '/cache/maps'
    fs.mkdir(cache):await()
    fs.writeFile(cache .. '/island.json', '{"size": 64}'):await()
    local names = fs.readdir(cache):await()
    local profile = fs.readFile(storage.root() .. '/data/profile.json'):await()
    print(names[1], #profile > 0) -- island.json true
    fs.removeRecursive(storage.root() .. '/cache'):await()
    print(storage.exists('cache/maps/island.json')) -- false
end)
```

## Asynchronous code

Some work takes time, such as loading a large image, waiting for a server or waiting for a timer. A frame must never wait for it, or the app would freeze. So such functions return a promise instead of the result. A promise is a value that settles later: it resolves with a result or rejects with an error.

Engine functions that return promises include `assets.loadAsync`, `assets.preload`, `platform.call`, `jobs.spawn` and `handle:wait()` of a tween. Varn's `async.sleep` and its `http` requests return promises too.

### Tasks and await

A task is a function that runs as a coroutine, so it can pause and resume. Start one with `async.spawn(fn)`. Inside a task, `promise:await()` pauses the task until the promise settles, while the app keeps running. Then it returns the result.

The method `:await()` never raises. It returns the value when the promise resolves, and `nil` and the error message when it rejects. So always check the result.

```lua
local async = require('async')
local assets = require('haylen.assets')
local log = require('haylen.log')

local hero = {}

async.spawn(function()
    local texture, failure = assets.loadAsync('images/hero.png'):await()
    if not texture then
        log.warning('The hero could not be loaded: ' .. failure)
        return
    end
    hero.texture = texture
end)
```

`:await()` works only inside a task. Code at the top level of `source/main.lua` or in a scene hook starts one with `async.spawn`. The `load` hook of a scene already runs in a task. For a task that belongs to a scene, use `scene.spawn(owner, fn)` of [`haylen.scene`](../lua-api/scene.md#scenespawnowner-fn): the task stops for good when its owner, such as its scene, goes away.

### When tasks resume

A task resumes during the poll of the event loop at the start of a frame, before the updates of that frame. The poll has a budget of a quarter of the frame time, between 1 and 8 milliseconds. It runs in passes. A promise that settles during a pass, such as the one of `async.sleep(0)` or `async.yield()`, resumes its task in the next pass. That pass runs in the same frame while the budget lasts, and in the next frame once it is spent. An `:await()` of a promise that already settled answers at once, without a pause.

So a loop of many small steps awaits `async.yield()` now and then to hand the frame back. A long computation that never awaits runs as a [job](#long-computations) instead. The debug overlay and `debug.stats().eventLoop` of [`haylen.debug`](../lua-api/debug.md#debugstats) show the budget and whether work was left for the next frame.

### Cancelling a task

The functions `async.spawn` and `async.run` return the handle of the task. Its `task.cancel()` stops the task for good: no promise resumes it again, and its coroutine closes. Closing runs its to-be-closed variables, which are the locals declared with `<close>`. That happens at once when the task waits, and right after its next `:await()` when the task cancels itself. A to-be-closed variable that raises while its task closes stops the app with the error screen, like a failed task.

```lua
local async = require('async')

local countdown = async.spawn(function()
    for seconds = 3, 1, -1 do
        print(seconds .. ' seconds left.')
        async.sleep(1000):await()
    end
end)

async.spawn(function()
    async.sleep(1500):await()
    countdown.cancel()
    print('The countdown was cancelled.')
end)
```

### Failures nobody hears

An error that escapes the function of `async.spawn` or `async.run` stops the app. The error screen shows it with the stack of that task, like an error in a scene hook, and a web page receives it through `onError`. Code that expects a failure catches it with `pcall` inside the task.

A promise that rejects while nothing awaits it, catches it or passes it to a combinator such as `async.all` is an error of the app too. The work that the app started failed, and nobody heard of it. Varn checks once the code that ran when the promise rejected returns to the event loop, so a task that awaits the promise a little later still observes it. The error screen then shows the error of the promise after the sentence `A promise rejected, and nothing awaited or caught it.`. It shows the stack that the rejection carries, such as the stack of the task of `async.promise` that failed, and no stack when native code rejected it, such as a failed request.

- Work whose failure does not matter catches it with `promise:catch(fn)`, which returns a new promise of what `fn(err)` returns.
- Promises meant to settle together are awaited with `async.all` or `async.allSettled`. Awaiting them one after another would leave the later ones unobserved while the task waits for the first.

```lua
local async = require('async')
local http = require('http')

-- A ping whose failure the app does not care about.
http.client.get('https://example.com/ping'):catch(function() end)

async.spawn(function()
    -- Both requests are observed at once, so a failure of the second one never stops the app while the task waits for the first.
    local pages, failure = async.all({http.client.get('https://example.com/a'), http.client.get('https://example.com/b')}):await()
    print(pages and #pages or failure)
end)
```

The engine receives these failures through the handler that it sets with `async.onFailure`. The same handler receives the errors of `ffi` callbacks that fail outside any `ffi` call, of the handlers of Varn's servers and of the callbacks of `http.client.stream`, each with its stack as a list of frames. An app that sets a handler of its own takes them over, and they no longer stop the app.

A rejection that the engine reports another way never stops the app like this. Such rejections are a scene change whose load failed, whose error goes to `onError` or the log as [`haylen.scene`](../lua-api/scene.md#errors) describes, a group of `context:preload` that failed the load it belongs to, and a platform call, a dialog or a screen that was cancelled.

### Long computations

Long computations written in Lua, such as generating a map, run as jobs of [`haylen.jobs`](../lua-api/jobs.md). The Lua state belongs to the frame thread, so a job cannot run on another thread. Instead, a job is a coroutine that works within a time budget each frame and pauses at `jobs.checkpoint()` when the budget is spent. The function `jobs.spawn` returns a promise of the result of the job.

```lua
local async = require('async')
local jobs = require('haylen.jobs')

async.spawn(function()
    local total = jobs.spawn(function()
        local sum = 0
        for index = 1, 1000000 do
            sum = sum + index
            jobs.checkpoint()
        end
        return sum
    end):await()
    print('The total is ' .. total .. '.')
end)
```

Engine work written in C++, such as decoding images and sounds, already runs on worker threads and needs no jobs.

### Timeouts of requests

The option `timeoutSeconds` of Varn's `http` requests is one deadline for the whole request. It counts from the connection to the last byte of the answer, across every redirect, so a server that keeps sending a little at a time still ends on time. It takes any positive number of seconds, fractions such as `2.5` included, and any other value fails the request with a message that names the option. A request past its deadline rejects with a message that says it did not finish within its timeout. A secure request never waits for the server to confirm the close of its connection once the answer is whole.

The function `async.timeout(promise, ms)` stops waiting for any promise instead, while the request it waits for goes on until its own deadline. The [networking guide](../network/networking.md#timeouts-cancellation-and-owners) shows the other ways to stop waiting.

```lua
local async = require('async')
local http = require('http')

async.spawn(function()
    local response, failure = http.client.get('https://httpbin.org/delay/5', {timeoutSeconds = 2.5}):await()
    if not response then
        print('No answer within 2.5 seconds: ' .. failure)
        return
    end
    print(response.status)
end)
```

## Errors and stack traces

An error that nobody catches stops the app. It may come from `source/main.lua`, a scene hook, a timer or tween callback, a task of `async.spawn` or `async.run`, or any other engine callback. The engine keeps the first error, writes its report to the log, stops updating and drawing the scenes, and shows the error screen. The screen shows the name and version of the app, the platform and the version of the engine, the message, the file and line, the lines of the script around the error line, and the stack of the failing call, innermost first.

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

### Using the error screen

- `C` copies that report to the clipboard, and `R` restarts the app. Both actions are also buttons at the bottom of the screen, which a click or a tap presses. On touch devices the buttons show without their keys.
- Gamepads and TV remotes move an outlined focus between the buttons with the directional pad or the arrows of the remote. The south button or the select button of the remote presses the focused one. The focus starts on the restart button.
- A report taller than the screen scrolls with the mouse wheel, a drag of the mouse or a finger, the arrow and page keys, Home and End, and the directional pad up and down.

### Reading the stack

The stack names each function the way Lua does, such as `local 'spawnWave'`, `method 'hit'` or `main chunk`. A function that native code calls, such as a scene hook or the function of a task, shows where it is defined instead, such as `function <source/scenes/level.lua:10>`. The stack of a task of `async.spawn` or `async.run` and of an `ffi` callback comes from Varn and reads the same. A deep stack, such as a runaway recursion, keeps its innermost and outermost calls around one line that says how many levels it skipped, such as `...  140 levels skipped`. A to-be-closed variable that fails while a cancelled task closes shows its error without a stack, because the stack of the task is gone by then.

Chunk names are paths of the package, so every position in a message points at a file of the package.

### Where the report goes

The log receives the report on every platform. In the browser, the page receives the error in `Module.haylen.onError` as `{message, file, line, traceback, frames}`. There `traceback` is the stack as text, and `frames` lists every frame as `{source, line, function, kind}` with the kind `lua`, `c` or `main`. The native parts of [plugins](../platform/plugins.md#errors-of-the-app) receive the same report, so a crash reporter records it with the stack of the Lua code.

The function [`haylen.reportError(message)`](../lua-api/haylen.md#haylenreporterrormessage) stops the app the same way, with a message of the app and the stack of the code that reported it.

### Failures that go to the caller

Some failures go to the code that asked for the work instead of the error screen.

- A promise that rejects, such as the one of a failed request, of an asset that failed to load or of a job whose function raised, makes `:await()` return `nil` and the error. It stops the app only when nothing awaits or catches it, as [failures nobody hears](#failures-nobody-hears) describes.
- A scene `load` that fails while the scene below stays sends its error to the `onError` of the change or to the log, as [`haylen.scene`](../lua-api/scene.md#errors) describes.
- An error in a handler of `platform.registerHandler` fails the call that asked for it.
- An error in a handler or route of the HTTP servers of Varn's `http` module, in a callback of a WebSocket route or in a callback of `http.client.stream` stops the app with the stack of the handler. The server still answers the request that failed with status `500`, and the WebSocket connection stays open.

Warnings of Lua, such as an error in a `__gc` metamethod or a call of `warn`, go to the log as warnings that start with `Lua warning:`. The calls `warn('@off')` and `warn('@on')` turn them off and on.

### Going on after an error

The runtime stays alive on the error screen. In development, saving the fix resumes or restarts the app, as [hot reload](#hot-reload) describes. A browser editor restarts it with `Module.haylen.restart()` or `Module.haylen.run()`.

An app that can return to a safe screen after an error calls [`haylen.setRecoverable(true)`](../lua-api/haylen.md#haylensetrecoverablerecoverable). Its error screen then offers `Back to the app` next to copying and restarting, also with Escape, the east button and the back button of the platform, and the focus starts on it. Going back publishes `appRecovered` with the error at the start of the next frame, before the app updates again. So its listener first leaves the scene that failed, and the app then runs on with the same Lua state. Every error also publishes `appError` at the end of its frame. An app that runs on by itself answers it with `haylen.recover()`, the way the test project of the samples runs every test and lists the ones that failed.

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

Hot reload applies the files that you save to the app while it runs, so you see a change without starting the app again. It works while the app runs in development:

- The desktop player runs an app in development with `--dev` and the package folder, as in `haylen --dev samples/games/tiny-island`, and so does `haylen run samples/games/tiny-island`.
- The page of `haylen run samples/games/tiny-island --platform web` runs in development too.

The player checks `app.json`, `source/`, `content/` and the `plugin.json` and `source/` of every plugin for changes every half second of real time. It checks on the I/O pool, so frames never wait for the file system, and it ignores everything else in the folder. Every save reaches the app at the start of a frame, also while the error screen shows and while the app is halted, such as an app that pauses while the editor has the focus.

### What a save does

- **A Lua module that the app loaded** reloads in place. Its functions run the new code everywhere, while the state of the app stays, as [`haylen.hotReload`](../lua-api/hotReload.md#how-a-module-reloads) describes. A module that fails to compile or to run changes nothing and shows its error, while the other changed modules of the save still reload. Saving the fix reloads it. A module keeps the text it loaded, so a save while its first load runs reloads it like any later save.
- **A file under `content/`** reloads the assets read from it without a restart. Textures, shaders and JSON change in place, so sprites that already use a texture show the new pixels. Other asset types leave the cache, so the next load reads the new file, and every live asset that went stale gets the event `assetChanged`, as [`haylen.assets`](../lua-api/assets.md#hot-reload-of-assets) describes. The texts of a folder that `localization.loadFolder` loaded and the themes that `ui.loadTheme` loaded rebuild from their changed files. A changed font file restarts the app.
- **A Lua file that no module loaded yet** changes nothing, because the next `require` reads it. While an error shows, such as one of a module whose first load failed, saving the file resumes the app, so the code that failed requires it again, or restarts the app when the error cannot resume.
- **A change that cannot apply in place** restarts the app with a fresh Lua state, and `source/main.lua` runs again. The log names the reason. These changes restart the app: a changed `app.json`, `source/main.lua` or `plugin.json`, a removed module, a module that now returns another kind of value, a module that returns another value that is neither a table nor a function, such as a string, a module whose functions capture two different variables with the same name, a module that called `hotReload.restartOnChange()`, a save that touches more than 32 loaded modules, and every changed module while `debug.reload` of `app.json` is `"restart"`. A restart works even from an app whose `app.json` failed to load, so fixing the file restarts it.
- **Other files** never count: hidden files, backup files and files under `source/` that are not Lua. So the swap and backup files that editors write change nothing. A file that is still being written waits for the next check, and a change saved while the app restarts reaches the app that starts.

### Resume or restart

The error screen tells how a fix applies.

- An error of code that runs again every frame can resume. Such code is an `update` hook, a timer, a listener, a reload or its `reloaded` hook. Saving the fix reloads the module, and the app runs on from where it stopped, with its state. The screen announces it with `Save a fix to resume the app.`. A resumed app may hold state that the failed frame left half updated, and `R` always gives a clean start.
- An error of a step of the lifecycle stopped that step half way, so saving any file restarts the app. The screen announces it with `Saving a file of the app restarts it.`. Such steps are the start of the app, `source/main.lua`, a scene `load`, `enter`, `exit`, `unload`, `pause` or `resume`, an autoload `start` or `stop`, and a task of `async.spawn`, `async.run` or `scene.spawn` that failed.

### What a reload cannot change

A reload keeps state, but it cannot rewrite code that already runs or that the app built while it ran.

- A coroutine suspended in old code, such as a loop of `scene.spawn`, runs the old body of its function until it returns. Its calls through tables and the shared variables of the module reach the new code. A scene restarts such loops in its `reloaded` hook.
- An anonymous function created at run time, such as `function() self:fire() end` inside `enter`, keeps its old body. Its calls through `self`, module tables and module variables reach the new code.
- Fields that a constructor sets in `init` keep their old values in existing objects, because `init` does not run again. A renamed field or variable starts from its initial value.
- A value that a module computed from another module when it loaded stays old until the module reloads or computes it again in its `reloaded` hook.
- What runs at the top level of a module without an owner to end it, such as `scene.push` or `async.spawn`, runs again on every reload. So the top level of a module only defines things and registers what belongs to it. Objects that other code connects to belong in `hotReload.keep`.
- Every reload walks the whole Lua heap once to put the new functions in place, so a large app pauses for one longer frame at each save.

State that must survive a restart belongs in [`haylen.storage`](../lua-api/storage.md) or [`haylen.preferences`](../lua-api/preferences.md).

### Devices, browsers and editors

Zip packages and shipped apps are never watched. The command `haylen run <app> --platform web` serves the page in development, connected to a development server that pushes every file saved in the app to the open pages, which reload it the same way. The command `run` with `--platform android`, `ios-simulator`, `tvos-simulator`, `ios`, `tvos`, `macos` or `catalyst` launches the app connected to such a server, so the emulator, the simulators and the devices reload every save too, as the [distribution guide](../tools/distribution.md#development-mode) describes. A browser editor turns development on with `Module.haylen.development`, sends changed files with `Module.haylen.setFile` and applies them with `Module.haylen.applyChanges()`, as the [build guide](../tools/build.md#runtime-api) describes.

## Performance

Scripts run on the frame thread, so the Lua work spent on each item decides how many items an app can move every frame. A call from Lua into the engine costs far more than plain Lua arithmetic. A table that C++ reads field by field costs more still. So the fast paths avoid both for every item and let C++ do the work for each item.

- **Native properties.** [`haylen.tween`](../lua-api/tween.md#native-properties) animates the number, `Vec2` and `Color` properties of engine objects in C++, without running Lua every frame. Examples are the position of a sprite, the zoom of a camera and the transform of a UI node.
- **Numbers over values.** A number field such as `sprite.x` reads and writes a float, while `sprite.position` creates a new `Vec2` for every read. Hot loops read numbers.
- **Float buffers.** A float buffer of [`haylen.collections`](../lua-api/collections.md#float-buffers) holds numbers that Lua and C++ share without copying. The expression `buffer[index]` reads and writes one value, and `buffer:set(first, list)` copies a whole Lua array in one call.
- **Bulk APIs.** These calls handle many objects at once. `graphics2d.drawBatch(texture, buffer, layout)` draws the sprites that a buffer holds without a table for each one. `SpriteBatch:resize` fills a large batch in one call, and `SpriteBatch:writeFields` moves every sprite of a batch from a buffer. `world:readTransforms` and `world:writeTransforms` of [`haylen.physics2d`](../lua-api/physics2d.md) move the transforms of many bodies, and `emitter:readPositions` of [`haylen.particles2d`](../lua-api/particles2d.md) reads every particle.
- **Garbage.** The Lua state collects garbage in generational mode. It frees the short-lived tables and closures of a frame in small, frequent steps instead of long pauses. The standard `collectgarbage` still controls it: `collectgarbage('incremental')` and `collectgarbage('generational')` switch the mode, `collectgarbage('step')` runs a step at a moment the app picks, such as behind a loading screen, and `collectgarbage('count')` and the `memory` statistics of `debug.stats()` show how much Lua holds. Hot paths that create nothing, through pools, buffers and bulk APIs, give the collector nothing to do.
- **Measure.** The functions `debug.profile` and `debug.beginScope` of [`haylen.debug`](../lua-api/debug.md) time a piece of script, and `debug.stats()` counts objects and memory. While the app runs, the compact statistics show the frame time, the draw calls and the counts that the app picks.

This example keeps the math in plain Lua arrays, copies them into the shared buffer once, and moves every sprite with one call.

```lua
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

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

### The cost of each way

The command `haylen bench --suite lua` runs the Lua sprite benchmark `engine/bench/lua-benchmark` on the headless host. Every sprite falls and bounces off the edges of the screen, and the benchmark times Lua moving and drawing them in five ways.

| Mode | How it moves and draws the sprites |
| --- | --- |
| `tables` | One table for each sprite, and the list drawn with `graphics2d.drawBatch(texture, sprites)`. |
| `calls` | The same tables, each drawn with its own `graphics2d.draw(texture, x, y, sprite)`. |
| `buffer` | The positions in a float buffer, written one value at a time with `buffer[index]`, and the buffer drawn. |
| `bulk` | The positions in a plain Lua array, copied into the buffer with one `buffer:set(1, positions)`, and the buffer drawn. |
| `batch` | The update of `bulk`, and a sprite batch moved with `batch:writeFields`. |

These are the average CPU milliseconds of one frame on an Apple M5 Pro in Release. The draw column is the recording of the draw calls, including the conversion of the sprites on worker threads, and the frame column covers the whole frame of the engine.

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

What the numbers show:

- Plain Lua arithmetic on tables is the fastest way to update. But reading a table for every sprite makes the draw about ten times slower than reading a buffer.
- A `graphics2d.draw` call for every sprite costs about a quarter of a microsecond, almost three times the cost of the same tables in one `drawBatch`.
- The engine reads an option table in one pass over the keys it holds. So a table costs in proportion to its keys, not to the options that a function knows. Pass only the keys that differ from the defaults.
- Every `buffer[index]` access is a call into the engine of about 25 nanoseconds, against a few nanoseconds for an array. So a loop that touches every value through the buffer spends three times longer updating.
- Doing the math in plain Lua arrays and handing them over with one `buffer:set` combines both strengths. A frame of 100,000 moving sprites then costs about 5.5 milliseconds, less than half of what tables cost.

Keep `buffer[index]` for values that C++ fills, such as the transforms that `world:readTransforms` writes, where Lua reads only a few of them.

## Extending the engine from C++

A C++ project can add Lua modules of its own, backed by a plugin of its own, while the app itself stays in Lua. The public binding toolkit in `engine/include/haylen/lua/` is the same one that every built-in module uses, and everything in it lives in the namespace `haylen::lua`.

| Header | What it gives |
| --- | --- |
| `Stack.hpp` | `Stack::push`, `Stack::read` and `Stack::is`, which move C++ values on and off the Lua stack through their converters. |
| `Userdata.hpp` | `Userdata::check`, `Userdata::checkShared`, `Userdata::test` and `Userdata::emplace` for bound objects, `Userdata::equal` for an `__eq` that compares the bound objects, and `Userdata::pushField`, `Userdata::setField` and `Userdata::pushFunction`, which store Lua callbacks on userdata. |
| `Type.hpp` | The trait `Type<T>`, which binds a C++ type as userdata. |
| `Converter.hpp` | The trait `Converter<T>`, with the conversions of scalars, strings, optionals, vectors and bound types. |
| `EnumNames.hpp` | The trait `EnumNames<T>` for enums passed as strings. Every engine enum has one table of its names, which its specialization and the file parsers share. Its class usually owns it, such as `Texture::filterFromName` and `Texture::filterName`. |
| `Table.hpp` | `Table::checkFields` and `Table::readField` for option tables whose unknown keys are errors, and `Table::readFields` with `Table::readValue`, which read an option table in one pass over the keys it holds, for options read on every draw. |
| `Binding.hpp` | `Binding::preload`, which adds a module to the Varn runtime of the engine, `Binding::newModule`, and the wrappers `Binding::native`, `Binding::function` and `Binding::method`. |
| `ClassBuilder.hpp` | `ClassBuilder`, which builds metatables with methods and properties. |
| `Runtime.hpp` | `Runtime::getEngine(L)`, which reaches the engine from a binding, `Runtime::getMainThread` for callbacks that outlive their coroutine, `Runtime::protectedCall`, `Runtime::protectedRun`, `Runtime::runChunk`, `Runtime::runReporting`, `Runtime::reportError` and `Runtime::captureError`. |
| `Reference.hpp` | `Reference`, which keeps a Lua value alive from C++. |
| `JsonConverter.hpp` | `JsonConverter::push` and `JsonConverter::read`, which move JSON between C++ and Lua tables. |
| `Promise.hpp` | `Promise`, a result that a Lua task awaits and that native code settles later, from any thread, with `Promise::isSettled`, and `Promise::isPromise`, which recognizes any Varn promise on the stack. |
| `TypeConverter.hpp` | The bindings of engine types such as `math::Vec2`, `math::Rect`, `math::Color`, `graphics::Texture` and `graphics2d::Sprite`, so bindings take and return them. |
| `Error.hpp` | `Error`, the exception that a failed protected call throws, with the message, the script position and the frames of the stack. |
| `Application.hpp` | `Application`, the application that runs `source/main.lua`. |

### A module of your own

A plugin registers its modules in its `installLua` hook with `Binding::preload`. Then `require` builds the module table the first time a script asks for it. `Binding::preload` adds the module to the Varn runtime of the engine with `varn::runtime::Runtime::addModule`. A name that a module of Varn, such as `json`, or of another plugin already has raises `std::runtime_error` with `The Lua module "<name>" cannot be added, because a module of Varn or of another plugin already has its name. Give the module another name.`, which the error screen shows.

This complete source file adds an `app.score` module to a Lua app. Like the built-in modules, the plugin keeps its Lua entry points as private static methods.

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
print('The best score is ' .. score.best() .. '.')
```

The app builds with `haylen_add_app` and the option `CPP`, which says that its sources define `haylen::core::Application::create()`.

```cmake
haylen_add_app(my-app CPP
  SOURCES src/App.cpp
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

The template `Binding::native<&f>` wraps a function that takes the Lua state. A C++ exception thrown inside it, such as the `std::logic_error` of a missing plugin, becomes a Lua error with the same message. The template `Binding::function<&f>` goes one step further: it converts the parameters and the result of a plain C++ function or static method. So `static int clampScore(int points)` is exposed with `&haylen::lua::Binding::function<&clampScore>`.

### A type of your own

A C++ type becomes a Lua type with two things. A specialization of `Type<T>` names its metatable and says whether Lua holds the value itself or a `std::shared_ptr` to it. A `ClassBuilder` fills the metatable in `installLua`.

The metatable is protected like the ones of the engine, so `getmetatable` returns the type name and Lua code never reaches the finalizer. It also keeps the getters and setters of the properties, so the [inspector](../lua-api/debug.md#inspector) lists and edits the properties of every bound type, those of plugins too. A binding that creates a userdata metatable of its own does it with `Userdata::newMetatable`, which protects it the same way.

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
print('The wallet holds ' .. wallet.coins .. ' coins.')
```

Reading a member that the type does not have raises `The type "app.Wallet" has no member "<name>".`. Assigning one without a setter raises `The type "app.Wallet" has no writable property "<name>".`.

### A function that finishes later

A binding that finishes later returns a `lua::Promise`. The binding creates it, pushes it as its result and settles it once with one of these methods.

| Method | What the waiting task gets |
| --- | --- |
| `resolve` | A JSON value, which arrives in Lua as plain tables. |
| `resolveWith` | The one value that a function pushes on the frame thread. |
| `reject` | A message, which `:await()` returns as its second value. It stops the app when nothing awaits or catches the promise. |
| `rejectHandled` | The same, for a failure that reaches the app another way or that the app asked for, such as a cancellation. It never stops the app. |

The promise may be settled from a worker thread. The waiting task resumes on the frame thread, in a later poll of the event loop of Varn. A JSON value that Lua cannot hold, which is binary JSON or JSON nested more than 128 levels deep, rejects the promise with the reason instead, so its tasks still resume.

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

### Rules for extensions

- Every hook and binding runs on the frame thread. Long work goes to `engine.getJobs()`, as the [architecture guide](architecture.md#threads-and-async) describes.
- C++ keeps a Lua callback in a `lua::Reference`, calls it on the main Lua thread and runs it inside `lua::Runtime::runReporting`. So a failure reaches the error screen instead of the native code that triggered it.
- The plugin releases every `lua::Reference` it holds in `stop`, because the Lua state closes before plugins are destroyed.
- Project modules use a namespace of their own, such as `app.score`, and leave `haylen.` to the engine.
