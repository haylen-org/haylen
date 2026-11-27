# 🧰 Developer tools

This guide explains the tools that help you see inside a running app: the statistics with their window, the profiler and traces, the debug drawings, the inspector that reads and edits values and picks what the app draws, the developer console of the terminal, and GPU captures. It also explains how the keys of `haylen app run` reach those tools on any device and which tools stay in a shipped app. The Lua debugger has a guide of its own, the [debugger guide](debugger.md), and the [`haylen.debug` reference](../lua-api/debug.md) lists every function, option and message of these tools.

## The tools at a glance

Each tool answers one kind of question. The table says how to reach it: a key on the keyboard of the app, a key in the terminal of [`haylen app run`](../reference/commands.md#app-run), a command of the developer console or a function of Lua.

| Tool | It answers | Key in the app | Key in the terminal | Console command | Lua |
| --- | --- | --- | --- | --- | --- |
| [Statistics](../lua-api/debug.md#statistics) | How fast is the app, and what does it hold? | F3 | `s` | `stats` | `debug.setStatsMode` |
| [Drawings](../lua-api/debug.md#drawings) | Where are the bodies, the sprites and the shapes I cannot see? | F4 | `p` | `draw` | `debug.setDrawing` |
| [Inspector](../lua-api/debug.md#inspector) | What value does this field hold right now, and what if it held another? | F2 | `i` | `inspector`, `inspect` and `set` | `debug.setInspectorVisible` |
| [Console](../lua-api/debug.md#console) | Can I run this command in the app without a button for it? | None | `d` | All of them | `debug.runCommand` |
| [GPU captures](../lua-api/debug.md#gpu-captures) | Which draw went wrong on this device, and with which state? | None | `g` | `gpuCapture` | `debug.captureGpuAsync` |
| [Profiler](../lua-api/debug.md#profiler) and [traces](../lua-api/debug.md#traces) | Where does the time of a frame go? | None | None | None | `debug.profile` and `debug.startTrace` |

The keys of the app are defaults, and each one changes or turns off with its own function, such as `debug.setToggleKey`. The keys of the terminal act on every app that the run connected, whatever the device: a phone, a TV or a browser shows its statistics and its inspector without a keyboard of its own.

## Where the tools work

Most tools belong to development. An app is in development when `haylen app run` starts it in the Debug configuration, or when `haylen app serve` hands it to another device, as [development mode](distribution.md#development-mode) explains. Then the terminal reaches it, the console takes lines from outside the app, the inspector opens and the pick on the screen works.

Outside development, the console and the inspector are off unless the key `console` of the `debug` section of `haylen.json` is `true`. The statistics, the drawings and the profiler stay available to the code of the app in every build, so an app can show its own performance panel or add a hidden command for testers. Turn off the shortcuts of the keyboard in a build that players get, so a press of F3 never opens the statistics in a store build.

```lua
local debug = require('haylen.debug')
local hotReload = require('haylen.hotReload')

if not hotReload.active() then
    debug.setToggleKey(nil)
    debug.setDrawKey(nil)
end
```

The section `debug` of `haylen.json` also starts an app with tools on, such as the compact statistics or the physics drawing, which suits a stress test or a sample.

```json
{
    "name": "Crowd Test",
    "identifier": "com.example.crowdtest",
    "debug": {"stats": "compact", "statsItems": ["frameRate", "drawCalls", "counters"], "drawings": ["physics"]}
}
```

## Statistics

The statistics show how the app runs. They come in two sizes. The compact mode draws a few lines in a corner of the safe area, such as the frame rate, the draw calls and the geometry, which you can keep on while you play. The full mode opens the statistics window, with graphs of the frame times, the scopes of the profiler, the scenes of the stack, the memory of each kind of resource, the counts of objects and the recent log. Each press of F3, or of `s` in the terminal, moves to the next mode.

Two numbers tell different things. The frames per second come from when the display showed the frames, so they include the wait for the display and the frames it missed. The time of a frame is the work of the engine and the app inside one frame. An app that shows 60 frames per second with a frame time of 4 milliseconds has room to spare, while one that shows 60 with 15 milliseconds is close to its limit. Where the GPU device measures it, the GPU time of a frame tells whether the GPU or the CPU sets the pace.

A monitor adds a number of the app to the window, with a graph of its recent values, such as the enemies alive or the length of a queue. Give it the scene as its `owner`, so it leaves with the scene.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

local Level = {}

function Level:enter()
    self.enemies = {}
    self.pending = {}
    debug.addMonitor('Enemies', function() return #self.enemies end, {owner = self})
    debug.addMonitor('Pending paths', function() return #self.pending end, {owner = self})
end

scene.push(Level)
```

The same numbers reach Lua through [`debug.stats`](../lua-api/debug.md#debugstats), so a test or a benchmark reads them, and an app draws its own readout with them.

## The profiler and traces

The profiler splits every frame into named scopes, such as `update`, `render` and `submit`, and shows how long each took in the statistics window. The app opens scopes of its own inside them with [`debug.profile`](../lua-api/debug.md#debugprofilename-fn-) or with a pair of `debug.beginScope` and `debug.endScope`. Scopes with the same name add up within a frame and count their calls, so a scope inside a loop shows its total.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

local function steer(units, dt)
    for _, unit in ipairs(units) do
        unit.x = unit.x + unit.speed * dt
    end
end

scene.push({
    units = {},
    update = function(self, dt)
        debug.profile('steer', steer, self.units, dt)
        debug.beginScope('collisions')
        -- The collision tests of the level run here.
        debug.endScope()
    end,
})
```

A scope tells the average story of a frame. A hitch that comes once every few seconds needs a timeline instead, which a trace records. [`debug.startTrace`](../lua-api/debug.md#debugstarttraceoptions) records every frame and scope, the jobs of the task and I/O pools, the reads and decodes of assets and the GPU time, and [`debug.stopTraceAsync`](../lua-api/debug.md#debugstoptraceasyncpath) writes them to a JSON file in the storage of the app. The profilers of browsers and other trace viewers open that file, and its lanes show which thread did what while the hitch happened.

```lua
local async = require('async')
local debug = require('haylen.debug')

debug.addCommand('trace', function(frames)
    debug.startTrace({frames = frames})
    async.spawn(function()
        async.sleep(frames * 20):await()
        local trace = debug.stopTraceAsync('traces/hitch.json'):await()
        print('The trace holds ' .. trace.spans .. ' spans in ' .. trace.path .. '.')
    end)
    return 'Recording ' .. frames .. ' frames.'
end, {help = 'Records a trace of the next frames.', args = {'integer'}})
```

## Debug drawings

A debug drawing draws over the canvases of the app what it does not show, in the coordinates of each canvas and above everything else. The engine has two drawings. The drawing `physics` draws the bodies, shapes, joints and contacts of every physics world, colored by their state, which shows at once a collider that does not match its sprite or a body that never sleeps. The drawing `bounds` outlines every sprite with the path of its texture and every block of text, which finds a sprite drawn off screen or a text box larger than its words.

An app adds drawings of its own with [`debug.addDrawer`](../lua-api/debug.md#debugadddrawername-fn-options), such as the path an enemy follows, the cells of a grid or the range of a tower. The drawer draws with the functions of `haylen.graphics2d` and runs only while its drawing is on, so it costs nothing otherwise.

```lua
local debug = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local Level = {path = {{100, 100}, {260, 140}, {400, 320}}}

function Level:enter()
    debug.addDrawer('paths', function(kind)
        if kind ~= 'world' then
            return
        end
        local width = 2 * graphics2d.canvasUnitSize()
        for index = 2, #self.path do
            local from, to = self.path[index - 1], self.path[index]
            graphics2d.drawLine(from[1], from[2], to[1], to[2], width, '#FF40C0FF')
        end
    end, {owner = self})
end

scene.push(Level)
```

F4, or `p` in the terminal, turns every drawing on or off together, and the console command `draw` with a name switches one of them, such as `draw paths`. The statistics window also has a box for each drawing.

## The inspector

The inspector is a window that lists the values of the running app as a tree and lets you change them while it runs. Its roots are the scenes of the stack, the autoloads, the global table, the values that the app hands it with [`debug.watch`](../lua-api/debug.md#debugwatchname-value) and what the pick on the screen selected. An object of the engine, such as a sprite or a camera, lists its properties. A number, a boolean or a string edits in place, and the edit goes through the same setter as Lua, so a value that the setter refuses is refused with its message.

So the inspector suits tuning. Watch the table of the player, open the inspector and change its jump height while you play, until the jump feels right, then copy the value into the code.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

local Play = {}

function Play:enter()
    self.player = {speed = 240, jumpHeight = 96, gravity = 1800}
    debug.watch('player', self.player)
end

function Play:exit()
    debug.unwatch('player')
end

scene.push(Play)
```

The console reaches the same values as text, which suits a device without a pointer: `inspect watched.player` lists them, and `set watched.player.speed 300` changes one.

### Picking on the screen

The inspector also picks what the app draws. With its box `Pick on the screen` on, a tap or a click selects the sprite or the node on top under it instead of reaching the app, and the inspector shows its properties under the root `selected`. Handles appear around the selection: a dot moves it, a ring turns it and a square scales it. Each drag sets the property through its setter, like any edit of the inspector, so a level is laid out by hand while it runs. The pick works only in development, as [picking on the screen](../lua-api/debug.md#picking-on-the-screen) describes with the draws it reaches.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        local picked = debug.selected()
        if picked and picked.x then
            self.label = string.format('The selection is at %.0f, %.0f.', picked.x, picked.y)
        end
    end,
})
```

## The developer console

The developer console runs commands in the app as lines of text, such as `timeScale 0.25` for slow motion, `pause`, `restart` or `lua player.speed`. Its prompt lives in the terminal of `haylen app run`, which the key `d` opens, so the console works the same for an app on a phone, a TV or a web page. A line runs in every connected app, and each answer prints above the prompt, apart from the log. The section [the developer console](distribution.md#the-developer-console) lists the keys of the prompt, such as Tab, which completes the names of commands.

The engine brings commands for its own tools and the game pause, which the [console reference](../lua-api/debug.md#console) lists. An app adds its own with [`debug.addCommand`](../lua-api/debug.md#debugaddcommandname-fn-options). Each command declares the kinds of its arguments, and the console converts the words of the line before the command runs, so the command receives an integer and never has to parse text. A command that belongs to a scene takes the scene as its `owner`.

```lua
local debug = require('haylen.debug')
local scene = require('haylen.scene')

local Shop = {coins = 0}

function Shop:enter()
    debug.addCommand('coins', function(amount)
        self.coins = self.coins + amount
        return 'The player has ' .. self.coins .. ' coins.'
    end, {help = 'Gives the player coins.', args = {'integer'}, owner = self})

    debug.addCommand('unlock', function(item, everything)
        return everything and 'Every item is unlocked.' or ('The item ' .. item .. ' is unlocked.')
    end, {help = 'Unlocks an item of the shop.', args = {'string', 'boolean?'}, owner = self})
end

scene.push(Shop)
```

The app runs the same commands itself with [`debug.runCommand`](../lua-api/debug.md#debugruncommandline), whether the console takes lines from outside or not. So a cheat menu of the app for testers shares its commands with the console.

```lua
local debug = require('haylen.debug')
local ui = require('haylen.ui')

ui.mount(ui.row{gap = 8,
    ui.button{text = 'Add 100 coins', onClick = function()
        local ok, output = debug.runCommand('coins 100')
        print(ok and output or ('The command failed: ' .. output))
    end},
    ui.button{text = 'Slow motion', onClick = function()
        debug.runCommand('timeScale 0.25')
    end},
})
```

## GPU captures

A GPU capture records every command that the GPU device of the app ran during a few frames into a file of the capture tool of its graphics API. The debugger of that tool opens the file on a computer and replays each draw with its pipeline, its bindings and its textures, step by step. So a frame that draws wrong on one phone, such as a texture that turns black or a blend that looks wrong, is captured there and studied at a desk.

The tool depends on the graphics API: the frame capture of Metal on Apple platforms, which Xcode opens, PIX or RenderDoc on Direct3D 12, and RenderDoc on Direct3D 11, Vulkan and OpenGL. The engine never brings the tool itself. The Debug runs of `haylen app run` turn on the frame capture of Metal for the apps of Apple platforms, while RenderDoc and PIX must launch the app, so that they hook the graphics API before it starts. A browser gives a page no capture tool. The [frame captures](../graphics/rhi.md#frame-captures) section of the RHI guide tells how to load each tool.

There are four ways to ask for a capture:

- The key `g` in the terminal of `haylen app run`, which captures the next frame of every connected app.
- The command [`haylen app capture`](../reference/commands.md#app-capture) from another terminal, which also takes `--frames`.
- The console command `gpuCapture`, with the number of frames.
- The function [`debug.captureGpuAsync`](../lua-api/debug.md#debugcapturegpuasyncoptions) in the code of the app, such as when a test sees a wrong frame.

The capture goes into the folder `gpu-captures` of the storage of the app, and the terminal prints its path. On a desktop the file is right there. From a simulator or a device, copy it out of the storage of the app with the tools of each platform, such as `xcrun simctl get_app_container` for a simulator and `adb shell run-as` for a debuggable Android app.

```lua
local async = require('async')
local debug = require('haylen.debug')
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if not input.keyPressed('f9') then
            return
        end
        if debug.gpuCaptureTool() == 'none' then
            print('This graphics API has no capture tool.')
            return
        end
        async.spawn(function()
            local capture, message = debug.captureGpuAsync({frames = 3}):await()
            print(capture and ('The capture is in ' .. capture.path .. '.') or message)
        end)
    end,
})
```

A capture that cannot start answers with a code and a sentence that says what is missing, such as RenderDoc that did not launch the app, so the first try tells what to change.

## The keys of a run

A run of `haylen app run` prints a summary of the app and the keys that act on it, and then the log of the app. The keys of the developer tools are `d` for the console, `s` for the statistics, `i` for the inspector, `p` for the drawings and `g` for a GPU capture, next to the keys that reload, restart, clear the screen and quit. A key acts at once without Enter, and the terminal prints the answer of each app, such as the new mode of its statistics. The list of [while the app runs](distribution.md#while-the-app-runs) describes every key.

Those keys need the development server of the run, so a Release run offers only the keys that clear, list and quit. A run without an interactive terminal, such as one in continuous integration, reads no key and still streams the log.

```sh
haylen app run ~/apps/my-game --target android --device emulator-5554
haylen app capture ~/apps/my-game --frames 2
```
