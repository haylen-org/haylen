# haylen.debug

The module `haylen.debug` shows the debug statistics and reads them, switches the debug drawings, counts objects, samples monitors, reads the frame profiler and the recent log, records traces, keeps breadcrumbs, reads the report of the last native crash, and runs the developer console and the inspector. Use it to measure where frame time goes, to see what the app draws and simulates, to profile app code with named scopes and to show performance numbers in debug tools.

```lua
local debug = require('haylen.debug')
```

The name `debug` shadows the standard Lua `debug` library in the file that requires it. Pick another local name, such as `debugging`, when the file also needs the standard one.

## Statistics display

The engine shows its statistics in one of three modes.

| Mode | What shows |
| --- | --- |
| `'off'` | Nothing. Every app starts this way unless `app.json` picks another mode. |
| `'compact'` | A line for each [item](#compact-items) it shows in the bottom left corner of the safe area, the frame rate, the draw calls and the geometry unless the app or the player picks others. The 2D renderer draws them above everything else, including scene transitions and the error screen, so they work in every app, with or without UI. The numbers change four times per second, so they stay readable, and the frames between allocate nothing. |
| `'full'` | The debug overlay, a window drawn over the app with the frame rate, the fastest, slowest and one percent low frame times, the fixed steps of the frame, a graph of recent frame times, the budget of the poll of the event loop with whether work ran and what it waits for, the profiler scopes of the last frame, the scenes of the stack from the bottom up with their names, states and process modes and whether the scenes below them keep rendering, a checkbox for every [drawing](#drawings) and for every [item](#compact-items) of the compact mode, the rendering statistics, the memory of Lua, textures, render targets, static batches, the buffers of the renderer and sounds, the counters of scenes, tweens, timers, voices by bus, sockets, assets, bodies, contacts and particles, the usage of the GPU pools, the reads and downloads of the [content](content.md#contentstats), the object counts, the signals and event listeners with their emissions, warnings about listeners whose owner is gone, the monitors with their graphs and the recent log. Its close button turns the statistics off. |

F3 cycles through `off`, `compact` and `full`, so it opens the overlay as the panel where the player ticks what the drawings and the compact mode show, `debug.setToggleKey` picks another key, and `debug.setStatsMode` picks a mode from Lua. The setting `"debug": {"stats": "compact"}` in `app.json` starts the app in a mode, as the [Lua guide](../core/lua.md#appjson) lists. The statistics describe the frame before, because the renderer only knows its numbers once it has submitted a frame.

## Drawings

Drawings show what the app draws and simulates over every canvas it draws, in the coordinates of that canvas, on top of everything else and unlit. Each drawing has a name:

| Drawing | What shows |
| --- | --- |
| `'physics'` | The bodies, shapes, joints and contact points of every physics world that [`physics2d.newWorld`](physics2d.md) made, in world and render target canvases, with lines of the same width on the screen at any zoom. Bodies take the colors of their state: awake, sleeping, static or kinematic. |
| `'bounds'` | An outline around every sprite, sprite of a batch, nine-slice piece and baked batch, named after the path of the texture the assets loaded it from, and a fainter outline around every text block, in every canvas. |
| Any other name | The drawers that the app or a plugin adds with [`debug.addDrawer`](#debugadddrawername-fn-options). |

F4 turns every drawing off while any is on, and otherwise turns on every drawing the engine and the app know, `debug.setDrawKey` picks another key, and `debug.setDrawing` switches one drawing from Lua. The setting `"debug": {"drawings": ["physics", "bounds"]}` in `app.json` starts the app with drawings on, as the [Lua guide](../core/lua.md#appjson) lists. The statistics key wins when an app gives both the same key.

```lua
local debug = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')

debug.setDrawing('physics', true)
debug.addDrawer('paths', function(kind)
    if kind == 'world' then
        graphics2d.drawPolyline({{0, 0}, {200, 80}, {400, 40}}, 2 * graphics2d.canvasUnitSize(), '#FF40E0FF')
    end
end)
```

## Object counts and events

The statistics count the objects of every type that were created, are alive and were destroyed. Every userdata type exported to Lua counts under its type name, such as `haylen.Sprite` or `haylen.Tween`, from the moment Lua creates the value until the garbage collector frees it. Engine resources count under their own names: `Texture` and `RenderTarget` with the bytes of their pixels, `Font`, `FontFamily`, `RichText`, `Sound` with the bytes of its samples or its encoded file, `PhysicsBody`, `PhysicsContact`, `ParticleEmitter`, `Particle`, `Gui`, `UiCell`, `VectorImage` and `Tween`. A C++ project counts its own types with `haylen::debug::ObjectCounter` and `TrackedObject`.

The call `debug.setObjectEvents(true)` also publishes `objectCreated` and `objectDestroyed` on [`haylen.events`](events.md) for every counted object, with `{type, count}`, queued for the end of the frame because objects come and go on any thread, even inside the garbage collector. The events cost time in an app that creates many objects every frame, so they start off, and `"debug": {"objectEvents": true}` in `app.json` turns them on from the start.

## Monitors

A monitor is a function the engine calls once per frame to sample a number the app cares about, such as the enemies alive or the size of a queue. The full overlay shows every monitor with its value and a graph of its last 240 samples, and `debug.monitors()` returns them. A monitor that raises an error or returns something other than a number stops the app with the error screen.

## Profiler

The engine measures every frame in named scopes: `scripts` for promises, coroutines and platform replies, `fixedUpdate`, `update`, `render` and `submit`. App scopes nest inside the scope that is running, so a scope opened in the `update` callback of a scene sits under `update` at depth 1. Scopes with the same name under the same parent add up within a frame, and their `calls` count how often they ran. Scopes left open, for example by an error, close with the scope around them, or at the end of the frame. Scopes opened outside a frame, such as at the top level of `source/main.lua`, are not recorded.

## Traces

A trace records a span of frames into a file in the trace event JSON format, which the profilers of browsers and trace viewers open, so the work of each frame shows on a timeline. It holds a lane for each thread: `frame` with every frame and every profiler scope nested as they ran, a lane for each worker of the task pool and the I/O pool with the jobs it ran as `job` and `ioJob`, and a `gpu` lane with the time the GPU took for each frame, as `gpuFrame`, where the graphics API measures it, which Metal does on macOS, iOS, tvOS and Mac Catalyst. The other graphics APIs, Direct3D 11, OpenGL, OpenGL ES, WebGL 2 and WebGPU, leave the `gpu` lane empty. A GPU span ends when the frame thread learns that the GPU finished, a frame or two after the frame it measures, so its length is exact while its place on the timeline is late.

The function `debug.startTrace` starts a trace, and `debug.stopTraceAsync` stops it and writes it into user storage on the I/O pool. A trace keeps at most 262144 spans in memory by default, and counts the spans it drops once it is full. Recording costs a lock per span, so a trace measures a little more than the frames take without one.

## Inspector

The inspector lists the values of the running app and edits them while it runs, by paths of keys from its roots:

| Root | Holds |
| --- | --- |
| `watched` | The values that `debug.watch` exposes, held weakly, so a watched value never outlives its owner. |
| `scenes` | The tables of the scenes on the stack, from the bottom, as [`scene.list()`](scene.md#scenelist) returns them. |
| `autoloads` | The tables of the [autoloads](haylen.md#haylenautoloads) by name. |
| `globals` | The global table. |

A table lists its keys, at most 200 of them in order, integer keys first. An object of a bound type, such as a camera, a sprite or a body, lists the properties that its type declares, which the engine records for every type, those of plugins too. A path joins keys with dots, such as `watched.player.items.2`, where a key that reads as an integer names the item of a sequence. Numbers, booleans and strings are editable, and an edit goes through the same setter that Lua uses, with its checks, so `set watched.camera.minZoom -1` fails with the error of the camera.

The inspector window shows the roots as a tree whose open nodes refresh four times per second, with fields that edit a value when Enter confirms it. It opens with `debug.setInspectorVisible(true)` or the console command `inspector`, and follows the console: it is on in development runs and with `"debug": {"console": true}` in `app.json`. The console commands `inspect` and `set` reach the same values from the terminal of `haylen run` on a phone or a TV, or from a web page through `Module.haylen.console(line)`, which is how a desktop inspects a device. Nothing of the inspector runs while its window is closed. The inspector does not pick objects on the screen or move them with handles yet.

## Crashes and breadcrumbs

A breadcrumb is a step of the app that a crash report shows, such as opening a screen or buying an item, with data of the app. The engine keeps the last 100 breadcrumbs, and each one publishes `breadcrumbAdded` on [`haylen.events`](events.md), so a crash reporting or analytics plugin forwards it to its service. The engine itself sends nothing anywhere.

The engine captures native crashes, which no Lua error handler sees, such as a fault in a plugin, a driver or the engine: fatal signals on macOS, iOS, iPadOS, tvOS, Mac Catalyst, Linux and Android, and unhandled exceptions on Windows. The crash writes a small report into the user storage of the app, with its reason, the last 30 breadcrumbs, the last 20 log lines and the name, identifier and version of the app, and goes on to the system and the crash handlers that were there before, so the crash reporters of the system and of plugins still receive it. The next start of the app reads the report once with `debug.lastCrash()`. The report is prepared at the end of each frame that added a breadcrumb or logged a line, so the crash writes text that exists already. Browsers have no native crashes, since a fault of WebAssembly ends as a JavaScript error, which reaches the page through `onError`, so the web never has a report. Errors of Lua scripts go to the [error screen](../core/lua.md#errors-and-stack-traces), not to crash reports.

## Console

The developer console runs commands that the engine, plugins and the app register, typed as one line such as `gold 50` or `say "Hello there"`. Words are separated by spaces, double quotes keep spaces inside one word, and a backslash keeps the next character as it is. Each command declares the kinds of its arguments, and the console checks and converts them before the command runs, so a command receives integers, numbers, booleans and strings, never text it has to parse.

The console window lists the output, the line being typed, the commands as buttons and the history, with Tab for completion and the arrows for the history. It opens with the grave accent key, which `debug.setConsoleKey` changes, with a touch of four fingers at once, or with `debug.setConsoleVisible(true)`. A command without arguments runs from its button, and a command with arguments puts its name on the line, where the on-screen keyboard of a phone or a TV types the rest, and the window works with a gamepad or a remote through the navigation of the interface. Nothing of the console runs while it is closed.

The console is on in development runs, and `"debug": {"console": true}` in `app.json` turns it on in any other run. While it is off, it neither opens nor runs lines from outside the app, and the app still registers and runs its own commands with `debug.runCommand`. Lines also arrive from outside the app:

- In a Debug run of `haylen run` on a device, a simulator, an emulator or the browser, every line typed in the terminal goes to the connected apps through the development server, and their results print there, besides `r` and `R`, which send the saved files at once and restart the apps, as the [distribution guide](../tools/distribution.md) describes.
- A web page runs a line with `Module.haylen.console(line)`, as the [runtime API](../tools/build.md#runtime-api) describes.

The engine registers these commands.

| Command | Meaning |
| --- | --- |
| `help [word]` | Lists the commands with how to type them, or shows one. |
| `clear` | Clears the output of the console. |
| `stats <word>` | Shows the statistics as `off`, `compact` or `full`. |
| `draw <word> [boolean]` | Turns a [debug drawing](#drawings) on or off, or the other way when the line leaves the state out. |
| `timeScale [number]` | Shows the time scale, or sets it. |
| `pause [boolean]` | Pauses the app or resumes it, or switches when the line leaves the state out. |
| `restart` | Restarts the app. |
| `inspect [word]` | Lists the values at a path of the [inspector](#inspector), such as `inspect watched.player`, or its roots. |
| `set <word> <text>` | Sets the number, boolean or string at a path of the inspector, such as `set watched.player.speed 300`. |
| `inspector [boolean]` | Shows or hides the inspector window, or switches it when the line leaves the state out. |
| `lua <text>` | Runs Lua code and prints what it returns, such as `lua player.speed`, only in development runs. |

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

### Compact items

The compact mode shows a line for each item that is on, in this order:

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

The first three are on unless the `app.json` list `"debug": {"statsItems": ["frameRate", "physics"]}` picks others, and the checkboxes of the overlay and these functions change them while the app runs.

### debug.setStatsItem(item, enabled), debug.statsItem(item), debug.statsItems()

The function `setStatsItem` shows or hides an [item](#compact-items) of the compact mode, `statsItem` returns whether it shows, and `statsItems` returns the names of the items that show, in the order they draw. An unknown name raises an error that ends with `(unknown value '<name>')`.

```lua
local debug = require('haylen.debug')

debug.setStatsMode('compact')
debug.setStatsItem('physics', true)
debug.setStatsItem('geometry', false)
print(table.concat(debug.statsItems(), ', ')) -- frameRate, drawCalls, physics
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

### debug.setDrawing(name, enabled)

Turns the [drawing](#drawings) named `name` on or off. A name that no drawer has yet stays on and shows once a drawer of that name is added.

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

Returns the names of the drawings that are on, in order of name.

```lua
local debug = require('haylen.debug')

print(table.concat(debug.drawings(), ', '))
```

### debug.drawingNames()

Returns the names of every drawing the engine and the app know, `bounds`, `physics` and the names of the drawers added with `debug.addDrawer`, in order of name.

```lua
local debug = require('haylen.debug')

for _, name in ipairs(debug.drawingNames()) do
    print(name, debug.drawing(name))
end
```

### debug.setDrawKey(key)

Picks the key that turns every drawing on or off, by the key names of [`haylen.input`](input.md). The value `nil` turns the shortcut off. The default key is `'f4'`.

```lua
local debug = require('haylen.debug')

debug.setDrawKey('f6')
```

### debug.drawKey()

Returns the name of the key that switches the drawings, or `nil` when the shortcut is off.

```lua
local debug = require('haylen.debug')

print(debug.drawKey())
```

### debug.addDrawer(name, fn, options)

Adds a drawer to the drawing named `name`, which calls `fn(kind)` in every canvas just before the canvas closes while the drawing is on, where `kind` is `'world'`, `'screen'` or `'target'`. The function draws with [`haylen.graphics2d`](graphics2d.md) in the coordinates of the canvas, such as the path of an enemy or the cells of a navigation grid. It returns the [`Connection`](signal.md#connection) of the drawer, whose `disconnect` removes it and whose `blocked` holds it. The argument `options` is an optional table whose `owner` removes the drawer when the owner ends, as the [owners of `haylen.events`](events.md#owners) describe. An empty name raises `a drawing needs a name`, and an unknown option raises `Unknown option "<name>".`

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

### debug.stats()

Returns a snapshot of the engine statistics as a table with these fields. Rendering numbers describe the last frame the renderer finished.

| Field | Contents |
| --- | --- |
| `frame` | `fps` over the recent frames, from the intervals at which the display showed them, the work of the frames as `milliseconds` of the last frame, `average`, `minimum` and `maximum` over the recent frames and `onePercentLow`, the average of the slowest one percent of them, all in milliseconds, and `fixedSteps` of the current frame. |
| `eventLoop` | The last poll of the event loop of Varn: its `budget` in milliseconds, a quarter of the frame time between 1 and 8, `ran`, whether any job, timer or socket callback ran, `pending`, whether work remains, and `idle`, the milliseconds the loop can wait before it has work of its own, which is `0` when the budget ended the poll with work still ready and `-1` when only sockets, workers or the platform can bring work. |
| `rendering` | `drawCalls`, `passes`, `canvases`, `sprites`, `instances`, `vertices`, `indices`, `lights`, `occluders`, `shadows`, `textureSwitches` and `uploadedBytes`. |
| `memory` | Bytes of `lua`, the Lua heap, and the estimated bytes of `textures`, `targets` for render targets, `batches` for the GPU instance buffers of static sprite batches, `buffers` for the vertex, index and instance buffers that the renderer streams sprites, shapes, text and batches through every frame, and `sounds`. |
| `counts` | `scenes` on the stack, root `tweens`, `timers`, playing `voices`, `assetsCached`, `assetsPending`, open `sockets`, physics `bodies` and `contacts`, and live `particles`. |
| `buses` | The audio buses by name, such as `music` and `sfx`, each with the `voices` that play through it, how many of them are `playing` and `paused`, and `processing`, whether its process mode runs in the current pause state. |
| `pools` | The pools of the GPU device by name, `images`, `buffers`, `samplers`, `programs` and `pipelines`, each with `used` and `size`. A full pool makes the next GPU object fail. |
| `objects` | Every counted type by name, each with `kind` (`'userdata'` or `'native'`), `created`, `alive`, `destroyed` and `bytes`, as [object counts](#object-counts-and-events) describes. |

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

The argument `options` is an optional table. Its `owner`, a table or a userdata such as a scene, removes the monitor when the owner ends, as the [owners of `haylen.events`](events.md#owners) describe. The owner keeps `fn`, so `fn` may refer to the owner without keeping it alive. An owner of another type raises `An owner must be a table or a userdata, not <type>.`, and an unknown option raises `Unknown option "<name>".`

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

### debug.frame()

Returns the measurements of the last finished frame as a table. Before the first frame every number is `0` and `scopes` is empty.

| Field | Type | Meaning |
| --- | --- | --- |
| `milliseconds` | number | Work of the last frame, from the start of its update to the end of its rendering, in milliseconds. |
| `average` | number | Average work of the recent frames, in milliseconds. |
| `fps` | number | Frames per second that the display showed over the recent frames, from the intervals the platform reports for the frames it shows: the display link on Apple platforms, the timestamps of `requestAnimationFrame` on the web, the Choreographer on Android and the time between presented frames on the desktops. They hold the wait for the display and the frames it missed, unlike the work of the frames, or `0` before the second frame. |
| `gpuMilliseconds` | number or `nil` | Time the GPU took for the last frame it finished, which lags a frame or two behind, where the graphics API measures it, which Metal does. It is `nil` on the other graphics APIs and before the GPU finished a frame. |
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

Closes the scope the last `beginScope` opened. Closing a scope when no scope of `beginScope` is open, such as the scope of the engine the code runs in, raises `A profiler scope was closed without being opened.`.

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

### debug.addCommand(name, fn, options)

Adds a command to the console and returns its [`Connection`](signal.md#connection), replacing the command with the same name. The argument `name` is one word without spaces or quotes. When the command runs, `fn` receives its arguments, converted to their kinds, and returns the text to print as a string or a number, or nothing. An error that `fn` raises fails the command with its message instead of stopping the app, and a value of another type fails it with `A console command returns a string, a number or nothing.`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `help` | string | `''` | The text that `help` and the window show next to the command. |
| `args` | list of strings | `{}` | The kinds of the arguments: `'integer'`, `'number'`, `'string'` for one word, `'boolean'` for `true` or `false`, and `'text'` for the rest of the line as it was typed, which comes last. A `?` after a kind, such as `'integer?'`, makes the argument optional, and optional arguments come last. |
| `owner` | table or userdata | the default owner | Removes the command when the owner ends, as the [owners of `haylen.events`](events.md#owners) describe. |

An unknown kind raises `The argument kind "colour" is unknown. It is "integer", "number", "string", "boolean" or "text", with "?" after it for an optional argument.`, a name with spaces raises `The command name "two words" is not one word. Name a command with letters, digits and symbols without spaces or quotes.`, and an unknown option raises `Unknown option "<key>".`.

When a line gives an argument of the wrong kind, leaves out a required one or gives too many, the command fails without running, with messages such as `The command "gold" needs an integer as its first argument, not "fifty". Type it as "gold <integer>".` and `The command "gold" takes 1 argument. Type it as "gold <integer>".`.

```lua
local debugging = require('haylen.debug')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.gold = 0
        debugging.addCommand('gold', function(amount)
            self.gold = self.gold + amount
            return 'Gold ' .. self.gold
        end, {help = 'Adds gold to the player.', args = {'integer'}, owner = self})
    end,
})
```

### debug.removeCommand(name)

Removes a command and returns whether it existed.

```lua
local debugging = require('haylen.debug')

debugging.removeCommand('gold')
```

### debug.runCommand(line)

Runs a line as the console does, prints it and its output in the console and records it in the history, and returns whether it worked and what it printed. A line that names no command returns `false` and `There is no command "silver". Type "help" to list the commands.`, and a quote that is not closed returns `false` and `The line has a quote that is not closed.`. The app runs its commands this way whether the console is on or off, such as from a cheat menu of its own.

```lua
local debugging = require('haylen.debug')

local ok, output = debugging.runCommand('gold 50')
print(ok, output)
```

### debug.commands()

Returns every command in order of name, each as `{name, help, usage}`, where `usage` shows how to type it, such as `gold <integer> [word]`.

```lua
local debugging = require('haylen.debug')

for _, command in ipairs(debugging.commands()) do
    print(command.usage .. '  ' .. command.help)
end
```

### debug.completeCommand(line)

Returns the lines that complete a partial line: the names of the commands that start with its only word, each followed by a space when the command takes arguments, and `true` and `false` for a boolean argument.

```lua
local debugging = require('haylen.debug')

print(table.concat(debugging.completeCommand('ti'), ', '))
```

### debug.commandHistory()

Returns the last 100 lines that ran, oldest first, without repeating a line that ran twice in a row.

```lua
local debugging = require('haylen.debug')

local history = debugging.commandHistory()
print(history[#history])
```

### debug.setConsoleVisible(visible)

Opens or closes the console window. Opening it while the console is off raises `The console is off outside development runs. Set "debug.console" to true in "app.json" to turn it on.`.

```lua
local debugging = require('haylen.debug')

if debugging.consoleEnabled() then
    debugging.setConsoleVisible(true)
end
```

### debug.consoleVisible()

Returns whether the console window shows.

```lua
local debugging = require('haylen.debug')

print(debugging.consoleVisible())
```

### debug.consoleEnabled()

Returns whether the console is on, which a development run or `debug.console` in `app.json` decides.

```lua
local debugging = require('haylen.debug')

print(debugging.consoleEnabled())
```

### debug.setConsoleKey(key)

Picks the [key](input.md#key-names) that opens and closes the console, or turns the shortcut off with `nil`. It is `'graveAccent'` by default.

```lua
local debugging = require('haylen.debug')

debugging.setConsoleKey('f1')
```

### debug.consoleKey()

Returns the key that opens the console, or `nil` when the shortcut is off.

```lua
local debugging = require('haylen.debug')

print(debugging.consoleKey())
```

### debug.startTrace(options)

Starts recording a [trace](#traces). Starting while a trace records raises `A trace is already recording. Stop it with "debug.stopTraceAsync" before starting another.`, and an unknown option raises `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `frames` | integer | none | Stops recording after this many frames, while the trace waits for `debug.stopTraceAsync` to write it. Without it, the trace records until it stops. Fewer than 1 raises `A trace records at least 1 frame.`. |
| `capacity` | integer | `262144` | The most spans the trace keeps. Fewer than 1 raises `A trace holds at least 1 span.`. |

```lua
local debugging = require('haylen.debug')
local timer = require('haylen.timer')

debugging.startTrace({frames = 300})
timer.after(6, function()
    debugging.stopTraceAsync('traces/menu.json')
end)
```

### debug.tracing()

Returns whether a trace records, which turns `false` once a trace of a number of frames recorded them.

```lua
local debugging = require('haylen.debug')

print(debugging.tracing())
```

### debug.stopTraceAsync(path)

Stops the trace and writes it to the file `path` of [user storage](storage.md), on the I/O pool, and returns a promise that resolves with `{path, spans, dropped}`, the number of spans written and the number the full trace dropped, or rejects with the error of the write. Calling it without a trace raises `No trace is recording. Start one with "debug.startTrace".`. The file is JSON, which the profilers of browsers and trace viewers open.

```lua
local async = require('async')
local debugging = require('haylen.debug')

debugging.startTrace()
async.spawn(function()
    async.sleep(5000):await()
    local trace = debugging.stopTraceAsync('traces/level.json'):await()
    print('Wrote ' .. trace.spans .. ' spans to ' .. trace.path)
end)
```

### debug.addBreadcrumb(message, data)

Adds a [breadcrumb](#crashes-and-breadcrumbs) with an optional table of JSON values, and publishes `breadcrumbAdded`. Data that JSON cannot hold, such as a function, raises an error.

```lua
local debugging = require('haylen.debug')

debugging.addBreadcrumb('Opened the shop.', {items = 12})
```

### debug.breadcrumbs()

Returns the kept breadcrumbs, oldest first, each as `{time, message, data}`, where `time` is the seconds since 1970.

```lua
local debugging = require('haylen.debug')

for _, crumb in ipairs(debugging.breadcrumbs()) do
    print(os.date('%H:%M:%S', math.floor(crumb.time)) .. ' ' .. crumb.message)
end
```

### debug.lastCrash()

Returns the report of the native crash that ended the last run of the app, or `nil`. A report is `{reason, context}`, where `reason` is `'segmentationFault'`, `'busError'`, `'illegalInstruction'`, `'arithmeticError'`, `'abort'` or `'trap'` on the platforms with signals, and `'accessViolation'`, `'stackOverflow'`, `'illegalInstruction'`, `'divideByZero'`, `'breakpoint'` or `'exception'` on Windows. The `context` holds `app` with `name`, `identifier` and `version`, `breadcrumbs` as `{time, message, data}` and `log` as `{level, text}`, cut to fit 32 KiB. The engine removes the report once it read it, so the start after holds none.

```lua
local debugging = require('haylen.debug')

local crash = debugging.lastCrash()
if crash then
    print('The last run stopped with ' .. crash.reason .. '.')
end
```

### debug.watch(name, value)

Exposes a value under the root `watched` of the [inspector](#inspector), replacing the value with the same name. The inspector holds it weakly, so it disappears once nothing else holds it.

```lua
local debugging = require('haylen.debug')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.player = {speed = 220, jump = 520}
        debugging.watch('player', self.player)
    end,
    exit = function(self)
        debugging.unwatch('player')
    end,
})
```

### debug.unwatch(name)

Removes a value from the root `watched`.

```lua
local debugging = require('haylen.debug')

debugging.unwatch('player')
```

### debug.inspect(path)

Returns the values at a path of the [inspector](#inspector), such as `'watched.player'`, or its roots without one, each as `{key, type, value, editable, expandable}`, where `type` is the Lua type or the name of a bound type, `value` the value as text, `editable` whether the inspector edits it and `expandable` whether it holds values to list. A missing root raises `The inspector has no root "<name>". Its roots are "watched", "scenes", "autoloads" and "globals".`, and a path through a value that holds nothing raises `The value at "<path>" is of the type "<type>", which holds no "<key>".`.

```lua
local debugging = require('haylen.debug')

debugging.watch('player', {speed = 220})
for _, entry in ipairs(debugging.inspect('watched.player')) do
    print(entry.key, entry.type, entry.value)
end
```

### debug.setInspectorVisible(visible)

Shows or hides the inspector window. Showing it while the console is off raises `The inspector is off outside development runs. Set "debug.console" to true in "app.json" to turn it on.`.

```lua
local debugging = require('haylen.debug')

if debugging.consoleEnabled() then
    debugging.setInspectorVisible(true)
end
```

### debug.inspectorVisible()

Returns whether the inspector window shows.

```lua
local debugging = require('haylen.debug')

print(debugging.inspectorVisible())
```
