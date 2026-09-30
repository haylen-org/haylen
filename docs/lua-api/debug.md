# haylen.debug

The module `haylen.debug` shows the debug statistics and reads them, counts objects, samples monitors, and reads the frame profiler and the recent log. Use it to measure where frame time goes, to profile app code with named scopes and to show performance numbers in debug tools.

```lua
local debug = require('haylen.debug')
```

The name `debug` shadows the standard Lua `debug` library in the file that requires it. Pick another local name, such as `debugging`, when the file also needs the standard one.

## Statistics display

The engine shows its statistics in one of three modes.

| Mode | What shows |
| --- | --- |
| `'off'` | Nothing. Every app starts this way unless `app.json` picks another mode. |
| `'compact'` | Three lines in the bottom left corner of the safe area: frames per second and the last frame time, draw calls and vertices, and GPU instances. The 2D renderer draws them above everything else, including scene transitions and the error screen, so they work in every app, with or without UI. |
| `'full'` | The debug overlay, a window drawn over the app with the frame rate, the fastest, slowest and one percent low frame times, the fixed steps of the frame, a graph of recent frame times, the profiler scopes of the last frame, the rendering statistics, the memory of Lua, textures, render targets and sounds, the counters of scenes, tweens, timers, voices by bus, sockets, assets, bodies, contacts and particles, the usage of the GPU pools, the object counts, the signals and event listeners with their emissions, warnings about listeners whose owner is gone, the monitors with their graphs and the recent log. Its close button turns the statistics off. |

F3 cycles through `off`, `compact` and `full`, `debug.setToggleKey` picks another key, and `debug.setStatsMode` picks a mode from Lua. The setting `"debug": {"stats": "compact"}` in `app.json` starts the app in a mode, as the [Lua guide](../lua.md#appjson) lists. The statistics describe the frame before, because the renderer only knows its numbers once it has submitted a frame.

## Object counts and events

The statistics count the objects of every type that were created, are alive and were destroyed. Every userdata type exported to Lua counts under its type name, such as `haylen.Sprite` or `haylen.Tween`, from the moment Lua creates the value until the garbage collector frees it. Engine resources count under their own names: `Texture` and `RenderTarget` with the bytes of their pixels, `Font`, `FontFamily`, `RichText`, `Sound` with the bytes of its samples or its encoded file, `PhysicsBody`, `PhysicsContact`, `ParticleEmitter`, `Particle`, `UiDocument` and `Tween`. A C++ project counts its own types with `haylen::debug::ObjectCounter` and `TrackedObject`.

The call `debug.setObjectEvents(true)` also publishes `objectCreated` and `objectDestroyed` on [`haylen.events`](events.md) for every counted object, with `{type, count}`, queued for the end of the frame because objects come and go on any thread, even inside the garbage collector. The events cost time in an app that creates many objects every frame, so they start off, and `"debug": {"objectEvents": true}` in `app.json` turns them on from the start.

## Monitors

A monitor is a function the engine calls once per frame to sample a number the app cares about, such as the enemies alive or the size of a queue. The full overlay shows every monitor with its value and a graph of its last 240 samples, and `debug.monitors()` returns them. A monitor that raises an error or returns something other than a number stops the app with the error screen.

## Profiler

The engine measures every frame in named scopes: `scripts` for promises, coroutines and platform replies, `fixedUpdate`, `update`, `render` and `submit`. App scopes nest inside the scope that is running, so a scope opened in the `update` callback of a scene sits under `update` at depth 1. Scopes with the same name under the same parent add up within a frame, and their `calls` count how often they ran. Scopes left open, for example by an error, close with the scope around them, or at the end of the frame. Scopes opened outside a frame, such as at the top level of `source/main.lua`, are not recorded.

## Functions

### debug.setStatsMode(mode)

Shows the statistics in `mode`, which is `'off'`, `'compact'` or `'full'`. An unknown name raises an error that ends with `(unknown value '<name>')`.

```lua
local debug = require('haylen.debug')

debug.setStatsMode('compact')
```

### debug.statsMode()

Returns the current mode, `'off'`, `'compact'` or `'full'`.

```lua
local debug = require('haylen.debug')
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.keyPressed('f1') then
            debug.setStatsMode(debug.statsMode() == 'off' and 'full' or 'off')
        end
    end,
})
```

### debug.setToggleKey(key)

Picks the key that cycles the statistics through `off`, `compact` and `full`, by the key names of [`haylen.input`](input.md) such as `'f3'`, `'f5'` or `'graveAccent'`. The value `nil` turns the shortcut off, which suits release builds. The default key is `'f3'`. An unknown name raises an error that ends with `(unknown value '<name>')`.

```lua
local debug = require('haylen.debug')

debug.setToggleKey('graveAccent')

local release = true
if release then
    debug.setToggleKey(nil)
end
```

### debug.toggleKey()

Returns the name of the key that cycles the statistics, or `nil` when the shortcut is off.

```lua
local debug = require('haylen.debug')

local key = debug.toggleKey()
print(key and ('press ' .. key .. ' for the debug statistics') or 'the debug statistics have no shortcut')
```

### debug.stats()

Returns a snapshot of the engine statistics as a table with these fields. Rendering numbers describe the last frame the renderer finished.

| Field | Contents |
| --- | --- |
| `frame` | `fps`, `milliseconds` of the last frame, `average`, `minimum` and `maximum` over the recent frames, `onePercentLow`, the average of the slowest one percent of them, all in milliseconds, and `fixedSteps` of the current frame. |
| `rendering` | `drawCalls`, `passes`, `canvases`, `sprites`, `instances`, `vertices`, `indices`, `lights`, `occluders`, `shadows`, `textureSwitches` and `uploadedBytes`. |
| `memory` | Bytes of `lua`, the Lua heap, and the estimated bytes of `textures`, `targets` for render targets, and `sounds`. |
| `counts` | `scenes` on the stack, root `tweens`, `timers`, playing `voices`, `assetsCached`, `assetsPending`, open `sockets`, physics `bodies` and `contacts`, and live `particles`. |
| `buses` | The audio buses by name, such as `music` and `sfx`, each with the `voices` that play through it, how many of them are `playing` and `paused`, and `processing`, whether its process mode runs in the current pause state. |
| `pools` | The GPU pools by name, `images`, `views`, `buffers`, `samplers`, `shaders` and `pipelines`, each with `used` and `size`. A full pool makes the next GPU object fail. |
| `objects` | Every counted type by name, each with `kind` (`'userdata'` or `'native'`), `created`, `alive`, `destroyed` and `bytes`, as [object counts](#object-counts-and-events) describes. |

```lua
local debug = require('haylen.debug')
local timer = require('haylen.timer')

timer.every(5, function()
    local stats = debug.stats()
    print(string.format('%.0f FPS, 1%% low %.2f ms, Lua %.1f MB', stats.frame.fps, stats.frame.onePercentLow, stats.memory.lua / 1048576))
    print(stats.rendering.drawCalls .. ' draw calls, ' .. stats.pools.images.used .. ' of ' .. stats.pools.images.size .. ' images')
    local sprites = stats.objects['haylen.Sprite']
    if sprites then
        print(sprites.alive .. ' sprites alive, ' .. sprites.created .. ' created so far')
    end
end)
```

### debug.setObjectEvents(enabled)

Turns the `objectCreated` and `objectDestroyed` events on or off, as [object counts](#object-counts-and-events) describes.

```lua
local debug = require('haylen.debug')
local events = require('haylen.events')

debug.setObjectEvents(true)
events.on('objectDestroyed', function(object)
    print(object.count .. ' ' .. object.type .. ' destroyed')
end)
```

### debug.objectEvents()

Returns `true` while object events are on.

```lua
local debug = require('haylen.debug')

if debug.objectEvents() then
    print('every object creation is published')
end
```

### debug.addMonitor(name, fn, options)

Adds a [monitor](#monitors) named `name` that calls `fn` once per frame and shows the number it returns in the full overlay, and returns its [`Connection`](signal.md#connection). A monitor with the same name is replaced, which ends the connection of the old one. The method `connection:disconnect()` removes the monitor, and while `connection.blocked` is `true` the monitor keeps its value and history without calling `fn`.

The argument `options` is an optional table. Its `owner`, a table or a userdata such as a scene, removes the monitor when the owner ends, as the [owners of `haylen.events`](events.md#owners) describe. The owner keeps `fn`, so `fn` may refer to the owner without keeping it alive. An owner of another type raises `An owner must be a table or a userdata, not <type>.`, and an unknown option raises `Unknown option "<name>"`.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

local enemies = {}
debug.addMonitor('enemies', function()
    return #enemies
end)
local memory = debug.addMonitor('lua memory (KB)', function()
    return collectgarbage('count')
end)
memory.blocked = true

local level = {}
function level:enter()
    self.bullets = {}
    debug.addMonitor('bullets', function()
        return #self.bullets
    end, {owner = self})
end
scene.push(level)
```

### debug.removeMonitor(name)

Removes the monitor named `name` and returns whether there was one.

```lua
local debug = require('haylen.debug')

debug.addMonitor('score', function() return 0 end)
print(debug.removeMonitor('score')) -- true
print(debug.removeMonitor('score')) -- false
```

### debug.monitors()

Returns every monitor as a table with `name`, the last `value` and `history`, the recent samples oldest first.

```lua
local debug = require('haylen.debug')
local timer = require('haylen.timer')

timer.every(2, function()
    for _, monitor in ipairs(debug.monitors()) do
        print(monitor.name .. ' = ' .. monitor.value .. ' over ' .. #monitor.history .. ' frames')
    end
end)
```

### debug.hotReloadWatching()

Returns `true` while hot reload watches the package folder of an app in development, which is the case when the `haylen` player runs a package folder named on its command line. Then changed scripts and `app.json` restart the app, and changed assets reload in place. Zip packages and shipped apps return `false`.

```lua
local debug = require('haylen.debug')

if debug.hotReloadWatching() then
    print('save a file to see the change')
end
```

### debug.frame()

Returns the measurements of the last finished frame as a table. Before the first frame every number is `0` and `scopes` is empty.

| Field | Type | Meaning |
| --- | --- | --- |
| `milliseconds` | number | Duration of the last frame. |
| `average` | number | Average duration of the recent frames, in milliseconds. |
| `fps` | number | Frames per second derived from `average`, or `0` before the first frame. |
| `scopes` | list | Scopes of the last frame, parents before their children. |

Every scope is a table with these fields.

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | Name of the scope. |
| `milliseconds` | number | Time spent in the scope during the frame, added over every call. |
| `calls` | integer | Number of times the scope ran during the frame. |
| `depth` | integer | Nesting level, where `0` is a top level scope. |

```lua
local debug = require('haylen.debug')
local timer = require('haylen.timer')

timer.every(1, function()
    local frame = debug.frame()
    print(string.format('%.0f FPS, %.2f ms', frame.fps, frame.milliseconds))
    for _, scope in ipairs(frame.scopes) do
        print(string.rep('  ', scope.depth) .. scope.name .. ' ' .. string.format('%.2f ms', scope.milliseconds) .. ' x' .. scope.calls)
    end
end)
```

### debug.frameHistory()

Returns the durations of the recent frames in milliseconds, oldest first. The history holds up to 240 frames.

```lua
local debug = require('haylen.debug')
local timer = require('haylen.timer')

timer.every(5, function()
    local slowest = 0
    for _, milliseconds in ipairs(debug.frameHistory()) do
        slowest = math.max(slowest, milliseconds)
    end
    print(string.format('slowest recent frame %.2f ms', slowest))
end)
```

### debug.beginScope(name)

Opens a profiler scope named `name` inside the scope that is running. Every `beginScope` needs a matching `debug.endScope`.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

scene.push({
    enemies = {},
    update = function(self, dt)
        debug.beginScope('enemies')
        for _, enemy in ipairs(self.enemies) do
            enemy.x = enemy.x + enemy.speed * dt
        end
        debug.endScope()
    end,
})
```

### debug.endScope()

Closes the scope the last `beginScope` opened. Closing a scope when none is open raises `A profiler scope was closed without being opened.`.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        debug.beginScope('pathfinding')
        local steps = 0
        for _ = 1, 1000 do
            steps = steps + 1
        end
        debug.endScope()
    end,
})
```

### debug.profile(name, fn, ...)

Calls `fn` with the remaining arguments inside a scope named `name` and returns everything `fn` returns. An error raised by `fn` reaches the caller unchanged, and the scope still closes by the end of the frame.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

local function findPath(fromX, fromY, toX, toY)
    return {{fromX, fromY}, {toX, toY}}
end

scene.push({
    update = function(self, dt)
        self.path = debug.profile('findPath', findPath, 0, 0, 10, 4)
    end,
})
```

### debug.recentLog(count)

Returns up to `count` of the most recent log lines, oldest first, or every kept line when `count` is omitted. A negative `count` raises a bad argument error with `expected a non-negative integer`. The engine keeps the last 200 lines logged by the app, the engine and Varn from any thread. Every line is a table with these fields.

| Field | Type | Meaning |
| --- | --- | --- |
| `level` | string | `'debug'`, `'info'`, `'warning'` or `'error'`. |
| `text` | string | The line as it was printed. |

```lua
local debug = require('haylen.debug')
local log = require('haylen.log')

log.warning('The well is dry')
for _, line in ipairs(debug.recentLog(5)) do
    if line.level == 'warning' or line.level == 'error' then
        print(line.text)
    end
end
```
