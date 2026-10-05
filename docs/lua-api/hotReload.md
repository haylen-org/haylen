# haylen.hotReload

The module `haylen.hotReload` tells whether the app runs in development, where a saved file reaches the running app, and lets modules shape how they reload: a value a module keeps across its reloads, and a module that restarts the app instead of reloading. Every app has the module, so code that uses it runs unchanged in apps that ship, where nothing reloads. The [Lua guide](../lua.md#hot-reload) explains what a reload keeps and what it replaces.

```lua
local hotReload = require('haylen.hotReload')
```

## How a module reloads

In development, a saved Lua module that the app already loaded runs again in a staging environment with an owner of its own. A module whose new code fails to compile or raises while it runs changes nothing, and the error screen shows the error until a fix is saved. A module that ran without errors joins its new code to the state of the app:

- Its functions are replaced everywhere, in the module table, in other modules that copied them, in callbacks the engine holds, such as timers and listeners, and in the methods that classes copied, so the next call runs the new code.
- The variables that its functions capture, such as `local count = 0`, keep their values, and the old and new functions share them from then on.
- A field or a variable that holds a number, a string, a boolean or `nil` takes the edited literal while the app never changed it, and keeps the value the app gave it otherwise. A field that the new source no longer has goes away unless the app changed it.
- Tables of the module and classes of `haylen.class` keep their identity and merge field by field, so every instance sees the new methods. Engine objects, such as signals, worlds and GUIs, and instances of classes keep their identity and drop the ones the new load created.
- The globals that the module writes merge into the globals by the same rules.
- What the module registered at its top level without an `owner`, such as listeners of `haylen.events`, connections of `haylen.signal`, timers, tweens, GUIs of `haylen.ui` and monitors of `haylen.debug`, belongs to the module, so its new load replaces them and nothing runs twice.

Once every module of a save reloaded, the reloaded modules and the modules that require them, the autoloads, the scenes from the bottom of the stack to the top and the instances of the reloaded classes hear the `reloaded` hook, each table once, and the event `moduleReloaded` reaches the [event bus](events.md#engine-events) for every module.

## Functions

### hotReload.active()

Returns `true` while the app runs in development, where saved files reload in place or restart the app, and `false` in an app that ships.

```lua
local hotReload = require('haylen.hotReload')

if hotReload.active() then
    print('Save a module to see the change while the app runs.')
end
```

### hotReload.mode()

Returns how a changed module applies in development, the `debug.reload` value of `app.json`: `'module'`, the default, reloads it in place, and `'restart'` restarts the app for every changed module that the app loaded.

```lua
local hotReload = require('haylen.hotReload')

print('Changed modules apply by ' .. hotReload.mode())
```

### hotReload.keep(key, create)

Returns the value that the module that runs its top level kept under the string `key` in its previous load, or calls `create()` the first time, keeps its result and returns it. A reload then hands the module the same value, such as a table with signals that other code connected to, so a connection that the new load makes lands on the value that lives on. In an app that ships, every call returns a fresh result of `create()`. In development, a call outside the top level of a module raises `The function "keep" works only while a module loads.`, and a `key` that is not a string raises `bad argument #1 to 'keep' (string expected, got <type>)`.

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

Marks the module that runs its top level, so a saved change to its file restarts the app instead of reloading the module, for modules whose state a reload cannot merge. In an app that ships it does nothing. In development, a call outside the top level of a module raises `The function "restartOnChange" works only while a module loads.`

```lua
-- The file `source/systems/world-builder.lua`.
local hotReload = require('haylen.hotReload')

-- The world builds a native world at load time, which a reload cannot rebuild.
hotReload.restartOnChange()

return {seed = 42}
```

## The reloaded hook

A table that defines `reloaded(self, info)` hears it after every save that reloaded modules in place, with `info.modules`, the names of the reloaded modules, and `info.paths`, their files. Each call is protected like a scene hook, so an error in it shows on the error screen. A scene rebuilds what it built from code at run time there, such as closures it handed to timers or the GUIs of its interface, and a module recomputes the values it derived from other modules when it loaded.

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
