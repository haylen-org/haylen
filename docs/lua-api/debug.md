# 📖 `haylen.debug`

The module `haylen.debug` holds the tools that measure and inspect a running app: the debug statistics, the debug drawings, object counts, monitors, the frame profiler, the recent log, traces, breadcrumbs with the report of the last native crash, the developer console and the inspector. Use it to find where frame time goes, to see what the app draws and simulates, and to show performance numbers in your own debug tools. The [Lua guide](../core/lua.md#errors-and-stack-traces) explains the error screen, and the [architecture guide](../core/architecture.md#the-frame) explains the frame that the profiler measures.

```lua
local debug = require('haylen.debug')
```

The name `debug` hides the standard Lua library `debug` in the file that requires the module. Pick another local name, such as `debugging`, in a file that also needs the standard one.

## Statistics

The engine shows its statistics in one of three modes:

| Mode | What shows |
| --- | --- |
| `'off'` | Nothing. Every app starts this way, unless `app.json` picks another mode. |
| `'compact'` | One line for each [compact item](#compact-items) that is on, in the bottom left corner of the safe area. By default those are the frame rate, the draw calls and the geometry. The 2D renderer draws the lines above everything else, scene transitions and the error screen included, so they work in every app, with or without UI. The numbers change four times per second, so they stay readable, and the frames in between allocate nothing. |
| `'full'` | The debug overlay, a window drawn over the app. Its close button turns the statistics off. |

The overlay shows:

- The frame rate, the fastest, slowest and one percent low frame times, the fixed steps of the frame and a graph of recent frame times.
- The budget of the poll of the event loop, whether work ran and what it waits for.
- The profiler scopes of the last frame.
- The scenes of the stack from the bottom up, with their names, states and process modes, and whether the scenes below them keep rendering.
- A checkbox for every [drawing](#drawings) and for every [compact item](#compact-items).
- The rendering statistics, and the memory of Lua, textures, render targets, static batches, the buffers of the renderer and sounds.
- The counts of scenes, tweens, timers, voices by bus, sockets, assets, bodies, contacts and particles, and the use of the GPU pools.
- The reads and downloads of the [content](content.md#contentstats), the object counts, and the signals and event listeners with their emissions.
- Warnings about listeners whose owner is gone, the monitors with their graphs, and the recent log.

The key F3 cycles through `off`, `compact` and `full`. So it also opens the overlay, where the player ticks what the drawings and the compact mode show. The function `debug.setToggleKey` picks another key, and `debug.setStatsMode` picks a mode from Lua. The setting `"debug": {"stats": "compact"}` in `app.json` starts the app in a mode, as the [Lua guide](../core/lua.md#appjson) lists. The statistics describe the frame before, because the renderer knows its numbers only once it has submitted a frame.

### Compact items

The compact mode shows one line for each item that is on, in this order:

| Item | Line |
| --- | --- |
| `'frameRate'` | The frames per second and the time of the last frame. |
| `'drawCalls'` | The draw calls and render passes. |
| `'geometry'` | The vertices and GPU instances. |
| `'memory'` | The memory of Lua and of textures. |
| `'counters'` | The scenes on the stack, the tweens and the timers. |
| `'physics'` | The physics bodies and contacts. |
| `'particles'` | The live particles. |
| `'audio'` | The voices that play. |

The first three are on, unless the list `"debug": {"statsItems": ["frameRate", "physics"]}` of `app.json` picks others. The checkboxes of the overlay and the functions below change them while the app runs.

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

### debug.setStatsItem(item, enabled)

Shows or hides a [compact item](#compact-items). The argument `enabled` is a boolean. An unknown name raises an error that ends with `(unknown value '<name>')`.

```lua
local debug = require('haylen.debug')

debug.setStatsMode('compact')
debug.setStatsItem('physics', true)
debug.setStatsItem('geometry', false)
```

### debug.statsItem(item)

Returns whether a [compact item](#compact-items) shows. An unknown name raises an error that ends with `(unknown value '<name>')`.

```lua
local debug = require('haylen.debug')

print(debug.statsItem('memory')) -- false
```

### debug.statsItems()

Returns the names of the compact items that show, in the order they draw.

```lua
local debug = require('haylen.debug')

debug.setStatsItem('physics', true)
debug.setStatsItem('geometry', false)
print(table.concat(debug.statsItems(), ', ')) -- frameRate, drawCalls, physics
```

### debug.setToggleKey(key)

Picks the key that cycles the statistics through `off`, `compact` and `full`, by the [key names](input.md#key-names) of `haylen.input`, such as `'f3'`, `'f5'` or `'graveAccent'`. The default key is `'f3'`. The value `nil` turns the shortcut off, which suits release builds. An unknown name raises an error that ends with `(unknown value '<name>')`.

```lua
local debug = require('haylen.debug')

debug.setToggleKey('f5')

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
print(key and ('Press ' .. key .. ' for the debug statistics.') or 'The debug statistics have no shortcut.')
```

### debug.stats()

Returns a snapshot of the statistics of the engine as a table. The numbers of rendering describe the last frame that the renderer finished.

| Field | Contents |
| --- | --- |
| `frame` | `fps`, the frames per second over the recent frames, from the intervals at which the display showed them. The work of the frames in milliseconds: `milliseconds` of the last frame, and `average`, `minimum`, `maximum` and `onePercentLow`, the average of the slowest one percent, over the recent frames. And `fixedSteps` of the current frame. |
| `eventLoop` | The last poll of the event loop of Varn: its `budget` in milliseconds, which is a quarter of the frame time between 1 and 8. Then `ran`, whether any job, timer or socket callback ran, and `pending`, whether work remains. And `idle`, the milliseconds that the loop can wait before it has work of its own, which is `0` when the budget ended the poll with work still ready, and `-1` when only sockets, workers or the platform can bring work. |
| `rendering` | `drawCalls`, `passes`, `canvases`, `sprites`, `instances`, `vertices`, `indices`, `lights`, `occluders`, `shadows`, `textureSwitches` and `uploadedBytes`. |
| `memory` | Bytes of `lua`, the Lua heap. Estimated bytes of `textures`, of `targets` for render targets, of `batches` for the GPU instance buffers of static sprite batches, of `buffers` for the vertex, index and instance buffers through which the renderer streams sprites, shapes, text and batches every frame, and of `sounds`. |
| `counts` | `scenes` on the stack, root `tweens`, `timers`, playing `voices`, `assetsCached`, `assetsPending`, open `sockets`, physics `bodies` and `contacts`, and live `particles`. |
| `buses` | The audio buses by name, such as `music` and `sfx`. Each holds the `voices` that play through it, how many of them are `playing` and `paused`, and `processing`, whether its process mode runs in the current pause state. |
| `pools` | The pools of the GPU device by name, `images`, `buffers`, `samplers`, `programs` and `pipelines`, each with `used` and `size`. A full pool makes the next GPU object fail. |
| `objects` | Every counted type by name, each with `kind` (`'userdata'` or `'native'`), `created`, `alive`, `destroyed` and `bytes`, as [object counts](#object-counts-and-events) describe. |

```lua
local debug = require('haylen.debug')
local timer = require('haylen.timer')

timer.every(5, function()
    local stats = debug.stats()
    print(string.format('%.0f FPS, 1%% low %.2f ms, Lua %.1f MB', stats.frame.fps, stats.frame.onePercentLow, stats.memory.lua / 1048576))
    print(stats.rendering.drawCalls .. ' draw calls, ' .. stats.pools.images.used .. ' of ' .. stats.pools.images.size .. ' images')
    if stats.eventLoop.idle == 0 then
        print(string.format('The event loop left work for the next frame after its budget of %.2f ms.', stats.eventLoop.budget))
    end
    local sprites = stats.objects['haylen.Sprite']
    if sprites then
        print(sprites.alive .. ' sprites alive, ' .. sprites.created .. ' created so far')
    end
end)
```

## Drawings

Drawings show what the app draws and simulates. They draw over every canvas that the app draws, in the coordinates of that canvas, on top of everything else and unlit. Each drawing has a name:

| Drawing | What shows |
| --- | --- |
| `'physics'` | The bodies, shapes, joints and contact points of every physics world that [`physics2d.newWorld`](physics2d.md) made, in world and render target canvases, with lines of the same width on the screen at any zoom. Bodies take the colors of their state: awake, sleeping, static or kinematic. |
| `'bounds'` | An outline around every sprite, sprite of a batch, nine-slice piece and baked batch, named after the path of the texture that the assets loaded it from, and a fainter outline around every text block, in every canvas. |
| Any other name | The drawers that the app or a plugin adds with [`debug.addDrawer`](#debugadddrawername-fn-options). |

The key F4 turns every drawing off while any is on, and otherwise turns on every drawing that the engine and the app know. The function `debug.setDrawKey` picks another key, and `debug.setDrawing` switches one drawing from Lua. The setting `"debug": {"drawings": ["physics", "bounds"]}` in `app.json` starts the app with drawings on, as the [Lua guide](../core/lua.md#appjson) lists. When an app gives both shortcuts the same key, the key of the statistics wins.

### debug.setDrawing(name, enabled)

Turns the drawing named `name` on or off. A name that no drawer has yet stays on, and shows once a drawer of that name is added.

```lua
local debug = require('haylen.debug')

debug.setDrawing('bounds', true)
```

### debug.drawing(name)

Returns `true` while the drawing named `name` is on.

```lua
local debug = require('haylen.debug')

print(debug.drawing('physics'))
```

### debug.drawings()

Returns the names of the drawings that are on, in the order of their names.

```lua
local debug = require('haylen.debug')

print(table.concat(debug.drawings(), ', '))
```

### debug.drawingNames()

Returns the names of every drawing that the engine and the app know, `bounds`, `physics` and the names of the drawers added with `debug.addDrawer`, in the order of their names.

```lua
local debug = require('haylen.debug')

for _, name in ipairs(debug.drawingNames()) do
    print(name, debug.drawing(name))
end
```

### debug.setDrawKey(key)

Picks the key that turns every drawing on or off, by the [key names](input.md#key-names) of `haylen.input`. The default key is `'f4'`. The value `nil` turns the shortcut off.

```lua
local debug = require('haylen.debug')

debug.setDrawKey('f6')
```

### debug.drawKey()

Returns the name of the key that switches the drawings, or `nil` when the shortcut is off.

```lua
local debug = require('haylen.debug')

print(debug.drawKey()) -- f4
```

### debug.addDrawer(name, fn, options)

Adds a drawer to the drawing named `name`. While the drawing is on, the drawer calls `fn(kind)` in every canvas just before the canvas closes, where `kind` is `'world'`, `'screen'` or `'target'`. The function draws with [`haylen.graphics2d`](graphics2d.md) in the coordinates of the canvas, such as the path of an enemy or the cells of a navigation grid.

It returns the [`Connection`](signal.md#connection) of the drawer. Its method `disconnect` removes the drawer, and its property `blocked` holds it. The argument `options` is an optional table whose `owner`, a table or a userdata such as a scene, removes the drawer when the owner ends, as the [owners of `haylen.events`](events.md#owners) describe. Without an `owner`, the drawer takes the default owner.

An empty name raises a bad argument error with `a drawing needs a name`. An owner of another type raises `An owner must be a table or a userdata, not <type>.`, and an unknown option raises `Unknown option "<key>".`.

```lua
local debug = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local level = {enemies = {{x = 100, y = 200, range = 120}}}
function level:enter()
    debug.addDrawer('ranges', function(kind)
        if kind ~= 'world' then
            return
        end
        for _, enemy in ipairs(self.enemies) do
            graphics2d.drawRing(enemy.x, enemy.y, enemy.range, 2 * graphics2d.canvasUnitSize(), '#C0FF4040')
        end
    end, {owner = self})
end
scene.push(level)
```

## Object counts and events

The statistics count the objects of every type that were created, are alive and were destroyed:

- Every userdata type that is exported to Lua counts under its type name, such as `haylen.Sprite` or `haylen.Tween`, from the moment Lua creates the value until the garbage collector frees it.
- Engine resources count under their own names: `Texture` and `RenderTarget` with the bytes of their pixels, `Font`, `FontFamily`, `RichText`, `Sound` with the bytes of its samples or its encoded file, `PhysicsBody`, `PhysicsContact`, `ParticleEmitter`, `Particle`, `Gui`, `UiCell`, `VectorImage` and `Tween`.
- A C++ project counts its own types with `haylen::debug::ObjectCounter` and `TrackedObject`.

[`debug.stats`](#debugstats) returns the counts in `objects`. The events `objectCreated` and `objectDestroyed` report each object as it comes and goes, as [events](#events) describe. They cost time in an app that creates many objects every frame, so they start off.

### debug.setObjectEvents(enabled)

Turns the events `objectCreated` and `objectDestroyed` on or off. The setting `"debug": {"objectEvents": true}` in `app.json` turns them on from the start.

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
    print('Every object creation is published.')
end
```

## Monitors

A monitor is a function that the engine calls once per frame to sample a number the app cares about, such as the enemies alive or the size of a queue. The full overlay shows every monitor with its value and a graph of its last 240 samples, and `debug.monitors()` returns them.

### debug.addMonitor(name, fn, options)

Adds a monitor named `name`, which calls `fn` once per frame and shows the number it returns in the full overlay. It returns the [`Connection`](signal.md#connection) of the monitor. A monitor with the same name is replaced, which ends the connection of the old one. The method `connection:disconnect()` removes the monitor. While `connection.blocked` is `true`, the monitor keeps its value and history without calling `fn`.

The argument `options` is an optional table. Its `owner`, a table or a userdata such as a scene, removes the monitor when the owner ends, as the [owners of `haylen.events`](events.md#owners) describe. The owner keeps `fn`, so `fn` may refer to the owner without keeping it alive. Without an `owner`, the monitor takes the default owner.

An owner of another type raises `An owner must be a table or a userdata, not <type>.`, and an unknown option raises `Unknown option "<key>".`. A monitor that raises an error stops the app with the error screen, and so does one that returns something other than a number, with `A debug monitor function returns a number.`.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

local enemies = {}
debug.addMonitor('enemies', function()
    return #enemies
end)
local memory = debug.addMonitor('Lua memory (KB)', function()
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

## Profiler

The engine measures every frame in named scopes: `scripts` for promises, coroutines and platform replies, `fixedUpdate`, `update`, `render` and `submit`. These rules hold:

- App scopes nest inside the scope that is running. So a scope opened in the `update` callback of a scene sits under `update`, at depth 1.
- Scopes with the same name under the same parent add up within a frame, and their `calls` count how often they ran.
- Scopes left open, such as by an error, close with the scope around them, or at the end of the frame.
- Scopes opened outside a frame, such as at the top level of `source/main.lua`, are not recorded.

### debug.frame()

Returns the measurements of the last finished frame as a table. Before the first frame every number is `0`, and `scopes` is empty.

| Field | Type | Meaning |
| --- | --- | --- |
| `milliseconds` | number | The work of the last frame, from the start of its update to the end of its rendering, in milliseconds. |
| `average` | number | The average work of the recent frames, in milliseconds. |
| `fps` | number | The frames per second that the display showed over the recent frames, or `0` before the second frame. It comes from the intervals that the platform reports for the frames it shows: the display link on Apple platforms, the timestamps of `requestAnimationFrame` on the web, the Choreographer on Android and the time between presented frames on the desktops. So it holds the wait for the display and the frames it missed, unlike the work of the frames. |
| `gpuMilliseconds` | number or `nil` | The time that the GPU took for the last frame it finished, which lags a frame or two behind. It is `nil` where the GPU device does not measure it, which the [capability matrix](../graphics/rhi.md#capability-matrix) lists for each graphics API, and before the GPU finished a frame. |
| `scopes` | list | The scopes of the last frame, parents before their children. |

Every scope is a table with these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | The name of the scope. |
| `milliseconds` | number | The time spent in the scope during the frame, added over every call. |
| `calls` | integer | How many times the scope ran during the frame. |
| `depth` | integer | The nesting level, where `0` is a top level scope. |

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
    print(string.format('The slowest recent frame took %.2f ms.', slowest))
end)
```

### debug.beginScope(name)

Opens a profiler scope named `name` inside the scope that is running. Every `beginScope` needs a matching [`debug.endScope`](#debugendscope).

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

Closes the scope that the last `beginScope` opened. Closing a scope while no scope of `beginScope` is open, such as the scope of the engine that the code runs in, raises `A profiler scope was closed without being opened.`.

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

Calls `fn` with the remaining arguments inside a scope named `name`, and returns everything that `fn` returns. An error that `fn` raises reaches the caller unchanged, and the scope still closes by the end of the frame.

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

## Log

The engine keeps the last 200 lines that the app, the engine and Varn logged, from any thread.

### debug.recentLog(count)

Returns up to `count` of the most recent log lines, oldest first, or every kept line when `count` is left out. A negative `count` raises a bad argument error with `expected a non-negative integer`. Every line is a table with these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `level` | string | `'debug'`, `'info'`, `'warning'` or `'error'`. |
| `text` | string | The line as it was printed. |

```lua
local debug = require('haylen.debug')
local log = require('haylen.log')

log.warning('The well is dry.')
for _, line in ipairs(debug.recentLog(5)) do
    if line.level == 'warning' or line.level == 'error' then
        print(line.text)
    end
end
```

## Traces

A trace records a span of frames into a file in the trace event JSON format, which the profilers of browsers and trace viewers open. So the work of each frame shows on a timeline. A trace has a lane for each thread:

- `frame`, with every frame and every profiler scope, nested as they ran.
- A lane for each worker of the task pool and of the I/O pool, with the jobs it ran as `job` and `ioJob`.
- `gpu`, with the time that the GPU took for each frame as `gpuFrame`, where the GPU device measures it, as the [capability matrix](../graphics/rhi.md#capability-matrix) lists. Elsewhere the lane stays empty. A GPU span ends when the frame thread learns that the GPU finished, a frame or two after the frame it measures. So its length is exact, while its place on the timeline is late.

A trace keeps at most 262144 spans in memory by default, and counts the spans it drops once it is full. Recording costs a lock per span, so a trace measures a little more than the frames take without one.

### debug.startTrace(options)

Starts recording a trace. The table `options` is optional:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `frames` | integer | none | Stops recording after this many frames, while the trace waits for `debug.stopTraceAsync` to write it. Without it, the trace records until it stops. Fewer than 1 raises `A trace records at least 1 frame.`. |
| `capacity` | integer | `262144` | The most spans that the trace keeps. Fewer than 1 raises `A trace holds at least 1 span.`. |

Starting while a trace records raises `A trace is already recording. Stop it with "debug.stopTraceAsync" before starting another.`, and an unknown option raises `Unknown option "<key>".`.

```lua
local debug = require('haylen.debug')
local timer = require('haylen.timer')

debug.startTrace({frames = 300})
timer.after(6, function()
    debug.stopTraceAsync('traces/menu.json')
end)
```

### debug.tracing()

Returns whether a trace records. It turns `false` once a trace with `frames` recorded them.

```lua
local debug = require('haylen.debug')

print(debug.tracing())
```

### debug.stopTraceAsync(path)

Stops the trace and writes it to the file `path` of [user storage](storage.md), on the I/O pool. It returns a promise that resolves with `{path, spans, dropped}`, the number of spans written and the number that the full trace dropped, or rejects with the error of the write. Calling it without a trace raises `No trace is recording. Start one with "debug.startTrace".`. The file is JSON, which the profilers of browsers and trace viewers open.

```lua
local async = require('async')
local debug = require('haylen.debug')

debug.startTrace()
async.spawn(function()
    async.sleep(5000):await()
    local trace = debug.stopTraceAsync('traces/level.json'):await()
    print('Wrote ' .. trace.spans .. ' spans to ' .. trace.path)
end)
```

## Crashes and breadcrumbs

A breadcrumb is a step of the app that a crash report shows, such as opening a screen or buying an item, with data of the app. The engine keeps the last 100 breadcrumbs. Each one publishes the event `breadcrumbAdded`, so a plugin of crash reports or analytics forwards it to its service. The engine itself sends nothing anywhere.

The engine captures native crashes, which no Lua error handler sees, such as a fault in a plugin, a driver or the engine. It catches fatal signals on macOS, iOS, iPadOS, tvOS, Mac Catalyst, Linux and Android, and unhandled exceptions on Windows. The crash writes a small report into the user storage of the app, then goes on to the system and the crash handlers that were there before. So the crash reporters of the system and of plugins still receive it.

The report holds the reason of the crash, the last 30 breadcrumbs, the last 20 log lines, and the name, identifier and version of the app. The engine prepares it at the end of each frame that added a breadcrumb or logged a line, so the crash writes text that exists already. The next start of the app reads it once with `debug.lastCrash()`.

Browsers have no native crashes, since a fault of WebAssembly ends as a JavaScript error, which reaches the page through `onError`. So the web never has a report. Errors of Lua scripts go to the [error screen](../core/lua.md#errors-and-stack-traces), not to crash reports.

### debug.addBreadcrumb(message, data)

Adds a breadcrumb with the string `message` and an optional table `data` of JSON values, and publishes `breadcrumbAdded`. Data that JSON cannot hold, such as a function, raises an error.

```lua
local debug = require('haylen.debug')

debug.addBreadcrumb('Opened the shop.', {items = 12})
```

### debug.breadcrumbs()

Returns the kept breadcrumbs, oldest first, each as `{time, message, data}`, where `time` is the seconds since 1970.

```lua
local debug = require('haylen.debug')

for _, crumb in ipairs(debug.breadcrumbs()) do
    print(os.date('%H:%M:%S', math.floor(crumb.time)) .. ' ' .. crumb.message)
end
```

### debug.lastCrash()

Returns the report of the native crash that ended the last run of the app, or `nil`. A report is `{reason, context}`:

- `reason` is `'segmentationFault'`, `'busError'`, `'illegalInstruction'`, `'arithmeticError'`, `'abort'` or `'trap'` on the platforms with signals, and `'accessViolation'`, `'stackOverflow'`, `'illegalInstruction'`, `'divideByZero'`, `'breakpoint'` or `'exception'` on Windows.
- `context` holds `app` with `name`, `identifier` and `version`, `breadcrumbs` as `{time, message, data}`, and `log` as `{level, text}`. Each message and line keeps at most 256 characters. When the context would pass 32 KiB, it drops the log and the data of the breadcrumbs.

The engine removes the report once it read it, so the next start holds none.

```lua
local debug = require('haylen.debug')

local crash = debug.lastCrash()
if crash then
    print('The last run stopped with ' .. crash.reason .. '.')
end
```

## Console

The developer console runs commands that the engine, plugins and the app register, typed as one line, such as `gold 50` or `say "Hello there"`. Spaces separate the words, double quotes keep spaces inside one word, and a backslash keeps the next character as it is. Each command declares the kinds of its arguments, and the console checks and converts them before the command runs. So a command receives integers, numbers, booleans and strings, never text it has to parse.

The console window lists the output, the line being typed, the commands as buttons and the history, with Tab for completion and the arrows for the history. It opens with the grave accent key, which `debug.setConsoleKey` changes, with a touch of four fingers at once, or with `debug.setConsoleVisible(true)`. A command without arguments runs from its button. A command with arguments puts its name on the line, where the on-screen keyboard of a phone or a TV types the rest. The window works with a gamepad or a remote through the navigation of the interface. Nothing of the console runs while it is closed.

The console is on in development runs, and `"debug": {"console": true}` in `app.json` turns it on in any other run. While it is off, it neither opens nor runs lines from outside the app, but the app still registers and runs its own commands with `debug.runCommand`. Lines also arrive from outside the app:

- In a Debug run of `haylen run` on a device, a simulator, an emulator or the browser, every line typed in the terminal goes to the connected apps through the development server, and their results print there, as [development mode](../tools/distribution.md#the-development-server) describes.
- A web page runs a line with `Module.haylen.console(line)`, as the [runtime API](../tools/build.md#runtime-api) describes.

The engine registers these commands:

| Command | Meaning |
| --- | --- |
| `help [word]` | Lists the commands with how to type them, or shows one. |
| `clear` | Clears the output of the console. |
| `stats <word>` | Shows the statistics as `off`, `compact` or `full`. |
| `draw <word> [boolean]` | Turns a [debug drawing](#drawings) on or off, or switches it when the line leaves the state out. |
| `timeScale [number]` | Shows the time scale, or sets it. |
| `pause [boolean]` | Pauses the app or resumes it, or switches when the line leaves the state out. |
| `restart` | Restarts the app. |
| `inspect [word]` | Lists the values at a path of the [inspector](#inspector), such as `inspect watched.player`, or its roots. |
| `set <word> <text>` | Sets the number, boolean or string at a path of the inspector, such as `set watched.player.speed 300`. |
| `inspector [boolean]` | Shows or hides the inspector window, or switches it when the line leaves the state out. |
| `lua <text>` | Runs Lua code and prints what it returns, such as `lua player.speed`, only in development runs. |

### debug.addCommand(name, fn, options)

Adds a command to the console and returns its [`Connection`](signal.md#connection). It replaces a command with the same name. The argument `name` is one word without spaces or quotes. When the command runs, `fn` receives its arguments converted to their kinds. It returns the text to print as a string or a number, or nothing.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `help` | string | `''` | The text that `help` and the window show next to the command. |
| `args` | list of strings | `{}` | The kinds of the arguments: `'integer'`, `'number'`, `'string'` for one word, `'boolean'` for `true` or `false`, and `'text'` for the rest of the line as it was typed, which comes last. A `?` after a kind, such as `'integer?'`, makes the argument optional, and optional arguments come last. |
| `owner` | table or userdata | the default owner | Removes the command when the owner ends, as the [owners of `haylen.events`](events.md#owners) describe. |

The function raises these errors:

- An unknown kind raises `The argument kind "colour" is unknown. It is "integer", "number", "string", "boolean" or "text", with "?" after it for an optional argument.`.
- A name with spaces raises `The command name "two words" is not one word. Name a command with letters, digits and symbols without spaces or quotes.`.
- A `'text'` argument before another raises `The command "<name>" has an argument of the kind "text" before another argument. A text argument takes the rest of the line, so it comes last.`.
- A required argument after an optional one raises `The command "<name>" has a required argument after an optional one. Put the optional arguments last.`.
- An unknown option raises `Unknown option "<key>".`.

A command fails without stopping the app in these cases, and the console prints the message:

- An error that `fn` raises fails the command with its message.
- A value of another type fails it with `A console command returns a string, a number or nothing.`.
- A command whose owner ended fails with `The command ended with its owner.`.
- A line that gives an argument of the wrong kind, leaves out a required one or gives too many fails without running, with messages such as `The command "gold" needs an integer as its first argument, not "fifty". Type it as "gold <integer>".` and `The command "gold" takes 1 argument. Type it as "gold <integer>".`.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.gold = 0
        debug.addCommand('gold', function(amount)
            self.gold = self.gold + amount
            return 'Gold ' .. self.gold
        end, {help = 'Adds gold to the player.', args = {'integer'}, owner = self})
    end,
})
```

### debug.removeCommand(name)

Removes a command and returns whether it existed.

```lua
local debug = require('haylen.debug')

debug.removeCommand('gold')
```

### debug.runCommand(line)

Runs a line as the console does, prints it and its output in the console, and records it in the history. It returns whether it worked and what it printed. A line that names no command returns `false` and `There is no command "silver". Type "help" to list the commands.`. A quote that is not closed returns `false` and `The line has a quote that is not closed.`. The app runs its commands this way whether the console is on or off, such as from a cheat menu of its own.

```lua
local debug = require('haylen.debug')

local ok, output = debug.runCommand('gold 50')
print(ok, output)
```

### debug.commands()

Returns every command in the order of their names, each as `{name, help, usage}`. The `usage` shows how to type the command, such as `gold <integer> [word]`.

```lua
local debug = require('haylen.debug')

for _, command in ipairs(debug.commands()) do
    print(command.usage .. '  ' .. command.help)
end
```

### debug.completeCommand(line)

Returns the lines that complete a partial line: the names of the commands that start with its only word, each followed by a space when the command takes arguments, and `true` and `false` for a boolean argument.

```lua
local debug = require('haylen.debug')

print(table.concat(debug.completeCommand('ti'), ', '))
```

### debug.commandHistory()

Returns the last 100 lines that ran, oldest first, without repeating a line that ran twice in a row.

```lua
local debug = require('haylen.debug')

local history = debug.commandHistory()
print(history[#history])
```

### debug.setConsoleVisible(visible)

Opens or closes the console window. Opening it while the console is off raises `The console is off outside development runs. Set "debug.console" to true in "app.json" to turn it on.`.

```lua
local debug = require('haylen.debug')

if debug.consoleEnabled() then
    debug.setConsoleVisible(true)
end
```

### debug.consoleVisible()

Returns whether the console window shows.

```lua
local debug = require('haylen.debug')

print(debug.consoleVisible())
```

### debug.consoleEnabled()

Returns whether the console is on, which a development run or `debug.console` in `app.json` decides.

```lua
local debug = require('haylen.debug')

print(debug.consoleEnabled())
```

### debug.setConsoleKey(key)

Picks the [key](input.md#key-names) that opens and closes the console, or turns the shortcut off with `nil`. It is `'graveAccent'` by default.

```lua
local debug = require('haylen.debug')

debug.setConsoleKey('f1')
```

### debug.consoleKey()

Returns the key that opens the console, or `nil` when the shortcut is off.

```lua
local debug = require('haylen.debug')

print(debug.consoleKey()) -- graveAccent
```

## Inspector

The inspector lists the values of the running app by paths of keys from its roots, and edits them while the app runs:

| Root | Holds |
| --- | --- |
| `watched` | The values that `debug.watch` exposes, held weakly, so a watched value never outlives its owner. |
| `scenes` | The tables of the scenes on the stack, from the bottom, as [`scene.list()`](scene.md#scenelist) returns them. |
| `autoloads` | The tables of the [autoloads](haylen.md#haylenautoloads) by name. |
| `globals` | The global table. |

A table lists its keys, at most 200 of them, in order, integer keys first. An object of a bound type, such as a camera, a sprite or a body, lists the properties that its type declares, which the engine records for every type, those of plugins too. A path joins keys with dots, such as `watched.player.items.2`, where a key that reads as an integer names the item of a sequence. Numbers, booleans and strings are editable. An edit goes through the same setter that Lua uses, with its checks, so `set watched.camera.minZoom -1` fails with the error of the camera.

The inspector window shows the roots as a tree whose open nodes refresh four times per second, with fields that edit a value when Enter confirms it. It opens with `debug.setInspectorVisible(true)` or the console command `inspector`. It follows the console: it is on in development runs and with `"debug": {"console": true}` in `app.json`. The console commands `inspect` and `set` reach the same values from the terminal of `haylen run` on a phone or a TV, or from a web page through `Module.haylen.console(line)`. That is how a desktop inspects a device. Nothing of the inspector runs while its window is closed. The inspector does not pick objects on the screen or move them with handles yet.

### debug.watch(name, value)

Exposes a value under the root `watched` of the inspector, replacing the value with the same name. The inspector holds it weakly, so it disappears once nothing else holds it.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.player = {speed = 220, jump = 520}
        debug.watch('player', self.player)
    end,
    exit = function(self)
        debug.unwatch('player')
    end,
})
```

### debug.unwatch(name)

Removes a value from the root `watched`.

```lua
local debug = require('haylen.debug')

debug.unwatch('player')
```

### debug.inspect(path)

Returns the values at a path of the inspector, such as `'watched.player'`, or its roots without a path. Each value is a table `{key, type, value, editable, expandable}`:

| Field | Meaning |
| --- | --- |
| `key` | The key of the value at the path. |
| `type` | The Lua type, or the name of a bound type. |
| `value` | The value as text. |
| `editable` | Whether the inspector edits the value. |
| `expandable` | Whether the value holds values to list. |

A missing root raises `The inspector has no root "<name>". Its roots are "watched", "scenes", "autoloads" and "globals".`. A path through a value that holds nothing raises `The value at "<path>" is of the type "<type>", which holds no "<key>".`. A path that ends at a value with nothing to list raises `The value at "<path>" is of the type "<type>", which holds nothing to list.`.

```lua
local debug = require('haylen.debug')

debug.watch('player', {speed = 220})
for _, entry in ipairs(debug.inspect('watched.player')) do
    print(entry.key, entry.type, entry.value)
end
```

### debug.setInspectorVisible(visible)

Shows or hides the inspector window. Showing it while the console is off raises `The inspector is off outside development runs. Set "debug.console" to true in "app.json" to turn it on.`.

```lua
local debug = require('haylen.debug')

if debug.consoleEnabled() then
    debug.setInspectorVisible(true)
end
```

### debug.inspectorVisible()

Returns whether the inspector window shows.

```lua
local debug = require('haylen.debug')

print(debug.inspectorVisible())
```

## Events

The module publishes these events on [`haylen.events`](events.md):

| Event | Payload | When |
| --- | --- | --- |
| `objectCreated` | `{type, count}` | A counted object was created, while [`debug.setObjectEvents`](#debugsetobjecteventsenabled) has the events on. The events queue for the end of the frame, because objects come and go on any thread, even inside the garbage collector. |
| `objectDestroyed` | `{type, count}` | A counted object was destroyed, in the same way. |
| `breadcrumbAdded` | `{message, data, time}` | [`debug.addBreadcrumb`](#debugaddbreadcrumbmessage-data) added a breadcrumb, where `time` counts the seconds since 1970. |

```lua
local events = require('haylen.events')

events.on('breadcrumbAdded', function(crumb)
    print('Breadcrumb: ' .. crumb.message)
end)
```

## Errors

| Message | Raised by |
| --- | --- |
| An error that ends with `(unknown value '<name>')` | `debug.setStatsMode`, `debug.setStatsItem` and `debug.statsItem` with an unknown name, and `debug.setToggleKey`, `debug.setDrawKey` and `debug.setConsoleKey` with an unknown key. |
| `a drawing needs a name` | `debug.addDrawer` with an empty name, inside a bad argument error. |
| `An owner must be a table or a userdata, not <type>.` | `debug.addDrawer`, `debug.addMonitor` and `debug.addCommand` with an owner of another type. |
| `Unknown option "<key>".` | Every function that takes options, with an unknown key. |
| `A debug monitor function returns a number.` | A monitor whose function returns something other than a number. It stops the app with the error screen. |
| `A profiler scope was closed without being opened.` | `debug.endScope` without an open scope of `beginScope`. |
| `expected a non-negative integer` | `debug.recentLog` with a negative count, inside a bad argument error. |
| `A trace is already recording. Stop it with "debug.stopTraceAsync" before starting another.` | `debug.startTrace` while a trace records. |
| `A trace records at least 1 frame.`, `A trace holds at least 1 span.` | `debug.startTrace` with `frames` or `capacity` below 1. |
| `No trace is recording. Start one with "debug.startTrace".` | `debug.stopTraceAsync` without a trace. |
| `The console is off outside development runs. Set "debug.console" to true in "app.json" to turn it on.` | `debug.setConsoleVisible(true)` while the console is off, and lines from outside the app. |
| `The inspector is off outside development runs. Set "debug.console" to true in "app.json" to turn it on.` | `debug.setInspectorVisible(true)` while the console is off. |
| The messages of [`debug.addCommand`](#debugaddcommandname-fn-options) and [`debug.runCommand`](#debugruncommandline) | The console. |
| The messages of [`debug.inspect`](#debuginspectpath) | The inspector. |
