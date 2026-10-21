# 📖 `haylen.hotReload`

The module `haylen.hotReload` tells whether the app runs in development, where a saved file reaches the running app. It also lets a module shape how it reloads: a module can keep a value across its reloads, or restart the app instead of reloading. Every app has the module, so code that uses it runs unchanged in apps that ship, where nothing reloads. The [Lua guide](../core/lua.md#hot-reload) explains hot reload: what a save reloads, what restarts the app, and what a reload cannot change.

```lua
local hotReload = require('haylen.hotReload')
```

## How a module reloads

In development, a saved Lua module that the app already loaded runs again, in a staging environment with an owner of its own.

A module whose new code fails to compile, or raises an error while it runs, changes nothing. The other modules of the same save still reload, and the error screen shows the error until a fix is saved.

A module that ran without errors joins its new code to the state of the app:

- Its functions are replaced everywhere: in the module table, in other modules that copied them, in callbacks that the engine holds, such as timers and listeners, and in the methods that classes copied. So the next call runs the new code.
- The variables that its functions capture, such as `local count = 0`, keep their values. The old and new functions share them from then on.
- A field or a variable that holds a number, a string, a boolean or `nil` takes the edited literal, as long as the app never changed it. Otherwise it keeps the value that the app gave it. A field that the new source no longer has goes away, unless the app changed it.
- Tables of the module and classes of `haylen.class` keep their identity and merge field by field, so every instance sees the new methods.
- Arrays that `json.decode` of Varn returns, and tables of `json.array` and `json.object`, are tables of data like any other, although Varn marks them with a metatable of its own.
- A field keyed by a table of the module, such as `labels[Kind]`, merges under the live table of its key, whatever the order of the fields.
- Engine objects, such as signals, worlds and GUIs, and instances of classes keep their identity. The ones that the new load created are dropped.
- The globals that the module writes merge into the globals by the same rules.

What the module registered at its top level without an `owner` belongs to the module. So its new load replaces those things and nothing runs twice. That covers listeners of `haylen.events`, listeners of the sockets of `haylen.net`, connections of `haylen.signal`, timers, tweens, GUIs of `haylen.ui` and monitors of `haylen.debug`. Two things outlive the reload, because neither runs again:

- What the `create` of [`hotReload.keep`](#hotreloadkeepkey-create) registers belongs to the kept value.
- What the `start` of an autoload that the module adds with `haylen.autoload` registers lives as long as the autoload.

Once every module of a save reloaded, these tables hear the [`reloaded` hook](#the-reloaded-hook), each table once: the reloaded modules and the modules that require them, the autoloads, the scenes from the bottom of the stack to the top, and the instances of the reloaded classes. The event [`moduleReloaded`](#events) reaches the event bus for every module.

## Functions

### hotReload.active()

Returns `true` while the app runs in development, where saved files reload in place or restart the app. It returns `false` in an app that ships.

```lua
local hotReload = require('haylen.hotReload')

if hotReload.active() then
    print('Save a module to see the change while the app runs.')
end
```

### hotReload.mode()

Returns how a changed module applies in development, which is the value `debug.reload` of `app.json`:

- `'module'`, the default, reloads the module in place.
- `'restart'` restarts the app for every changed module that the app loaded.

```lua
local hotReload = require('haylen.hotReload')

print('Changed modules apply by ' .. hotReload.mode())
```

### hotReload.keep(key, create)

Keeps one value of a module across its reloads. The first time, it calls `create()`, keeps the result under the string `key` and returns it. On every later load of the same module, it returns the value that the previous load kept under that key, without calling `create`. So a reload hands the module the same value, such as a table with signals that other code connected to, and a connection that the new load makes lands on the value that lives on.

What `create()` registers without an `owner`, such as a listener of `haylen.events`, belongs to the kept value, so it outlives the reloads of the module. A load of the module that fails drops the values that it kept first, and ends what they registered. In an app that ships, every call returns a fresh result of `create()`.

The function raises these errors:

- A `key` that is not a string raises `bad argument #1 to 'keep' (string expected, got <type>)`.
- A `create` that is not a function raises `bad argument #2 to 'keep' (function expected, got <type>)`.
- In development, a call outside the top level of a module raises `The function "keep" works only while a module loads.`.

```lua
-- The file `source/state/player.lua`.
local hotReload = require('haylen.hotReload')
local signal = require('haylen.signal')

-- The table and its signal survive every reload of this module, so the connection below always lands on the live signal, and the reload ends the connection of the previous load.
local player = hotReload.keep('player', function()
    return {health = 100, healthChanged = signal.new('player.healthChanged')}
end)

player.healthChanged:connect(function(health)
    print('Health ' .. health)
end)

function player:damage(amount)
    self.health = math.max(self.health - amount, 0)
    self.healthChanged:emit(self.health)
end

return player
```

### hotReload.restartOnChange()

Marks the module that runs its top level, so a saved change to its file restarts the app instead of reloading the module. Use it for a module whose state a reload cannot merge. In an app that ships it does nothing. In development, a call outside the top level of a module raises `The function "restartOnChange" works only while a module loads.`.

```lua
-- The file `source/systems/world-builder.lua`.
local hotReload = require('haylen.hotReload')

-- The world builds a native world at load time, which a reload cannot rebuild.
hotReload.restartOnChange()

return {seed = 42}
```

## The reloaded hook

A table that defines `reloaded(self, info)` hears it after every save that reloaded modules in place. The table `info` holds `modules`, the names of the reloaded modules, and `paths`, their files.

The `reloaded` of a class of `haylen.class`, or of another table whose `__index` is the table itself, is the hook of its instances. So a module that returns a class does not hear it, while its instances, such as the scenes made from it, do. Each call is protected like a scene hook. So an error in it shows on the error screen until a save reloads without errors and resumes the app.

In this hook a scene rebuilds what it built from code at run time, such as closures that it handed to timers, or the GUIs of its interface. A module recomputes the values that it derived from other modules when it loaded.

```lua
-- The file `source/scenes/level.lua`.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Level = haylen.class('Level', scene.Scene)

function Level:enter()
    self.score = 0
    self:buildHud()
end

function Level:buildHud()
    if self.hud then
        self.hud:unmount()
    end
    self.hud = ui.mount(ui.label{id = 'score', text = 'Score ' .. self.score}, {owner = self})
end

-- Runs after a module reloaded in place, with the new code and the kept state, so the interface takes the new layout and handlers.
function Level:reloaded(info)
    print('Reloaded ' .. table.concat(info.modules, ', '))
    self:buildHud()
end

return Level
```

## Events

Hot reload publishes the event `moduleReloaded` on the [event bus](events.md#engine-events), once for each module of a save that reloaded in place. The event queues for the end of the frame, and its payload is `{module, path}`.

```lua
local events = require('haylen.events')

events.on('moduleReloaded', function(reload)
    print('Reloaded ' .. reload.module .. ' from ' .. reload.path)
end)
```
