# haylen.signal

`haylen.signal` creates signals in the model of the signals of Godot. A signal lets one object announce that something happened without knowing who listens, and emitting it calls every connected function with the emitted values. Use a signal when one object owns the announcement, such as a player that announces its health, and use [haylen.events](events.md) for announcements by name that the whole app may care about.

```lua
local signal = require('haylen.signal')
```

## Delivery

Listeners run by descending priority and, with equal priorities, in the order they connected. A listener may connect and disconnect listeners while the signal emits. A listener connected during an emit is first called by the next emit, and a listener that an earlier one disconnected is skipped. An error raised by a listener stops the emit, skips the remaining listeners and reaches the code that called `emit`. Deferred listeners run at the end of the frame instead, after every scene has rendered, with the values of the emit.

C++ code uses the typed `core::Signal` template with the same features, and `core::ScopedConnection` and `core::ConnectionScope` disconnect with RAII.

## Functions

### signal.new(name)

Creates a signal with no listeners and returns it. The optional `name` labels the signal in `signal.list()`, which lists only named signals.

```lua
local signal = require('haylen.signal')

local player = {
    health = 100,
    healthChanged = signal.new('player.healthChanged'),
    died = signal.new('player.died'),
}

function player:damage(amount)
    self.health = math.max(self.health - amount, 0)
    self.healthChanged:emit(self.health)
    if self.health == 0 then
        self.died:emit()
    end
end

player.healthChanged:connect(function(health) print('health', health) end)
player:damage(30)
```

### signal.list()

Returns a sequence with one table per named signal that is still alive, with the fields `name`, `listeners`, `emissions` (every emit since the signal was created, blocked ones included) and `stale` (listeners whose owner is already gone, such as an owner table the garbage collector took, which the next emit or the end of the frame removes). Use it to find listeners that pile up.

```lua
local signal = require('haylen.signal')

local scoreChanged = signal.new('scoreChanged')
scoreChanged:connect(function() end)
scoreChanged:emit(10)

for _, entry in ipairs(signal.list()) do
    print(entry.name, entry.listeners, entry.emissions, entry.stale)
end
```

## Signal

### sig:connect(fn, options)

Connects `fn` to the signal and returns a [Connection](#connection). The same function may be connected more than once, and it is then called once per connection. `options` is an optional table with the keys below, and unknown keys raise `Unknown option '<key>'.`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `priority` | integer | `0` | Listeners with a higher priority run first. |
| `once` | boolean | `false` | Disconnects the listener right before its first call. |
| `deferred` | boolean | `false` | Calls the listener at the end of the frame with the values of each emit. It is skipped when it disconnects before then. |
| `owner` | table or userdata | `nil` | Disconnects when the owner ends, as the [owners of haylen.events](events.md#owners) describe. |

The signal keeps `fn` alive until it disconnects, except for a listener with an owner, whose function the owner keeps. Dropping the returned connection does not disconnect it.

```lua
local signal = require('haylen.signal')

local coinCollected = signal.new()
local hud = {coins = 0}

coinCollected:connect(function(amount) hud.coins = hud.coins + amount end)
coinCollected:connect(function() print('the combo counter goes first') end, {priority = 10})
coinCollected:connect(function() print('first coin ever') end, {once = true})
coinCollected:connect(function() print('saved at the end of the frame') end, {deferred = true})

coinCollected:emit(1)
coinCollected:emit(5)
print(hud.coins) -- 6
```

### sig:emit(...)

Calls every connected function with the given values.

```lua
local signal = require('haylen.signal')

local damaged = signal.new()
damaged:connect(function(target, amount) print(target .. ' took ' .. amount .. ' damage') end)
damaged:connect(function(target) print('flash ' .. target) end)

damaged:emit('goblin', 12)
```

### sig:clear()

Disconnects every listener at once.

```lua
local signal = require('haylen.signal')

local waveStarted = signal.new()
waveStarted:connect(function(wave) print('wave', wave) end)

-- The level ends, so no listener should run anymore.
waveStarted:clear()
waveStarted:emit(2)
print(waveStarted.size) -- 0
```

### Properties

| Property | Access | Meaning |
| --- | --- | --- |
| `size` | read | The number of connected listeners. |
| `emissions` | read | Every emit since the signal was created, blocked ones included. |
| `name` | read | The name given to `signal.new`, or an empty string. |
| `blocked` | read and write | While `true`, emits call no listener and still count. |

```lua
local signal = require('haylen.signal')

local stepped = signal.new('stepped')
local steps = 0
stepped:connect(function() steps = steps + 1 end)

-- A cutscene silences the footsteps without disconnecting anything.
stepped.blocked = true
stepped:emit()
stepped.blocked = false
stepped:emit()
print(steps, stepped.emissions, stepped.name) -- 1 2 stepped
```

## Connection

A `Connection` stands for one registered listener. `sig:connect` returns it, and so do `events.on`, `scene.listen` and the other engine functions that register listeners.

### connection:disconnect()

Removes the listener. Calling it again does nothing.

```lua
local signal = require('haylen.signal')

local tick = signal.new()
local count = 0
local connection
connection = tick:connect(function()
    count = count + 1
    if count == 3 then
        connection:disconnect()
    end
end)

for _ = 1, 5 do
    tick:emit()
end
print(count) -- 3
```

### connection.connected

Read-only boolean that is `true` while the listener is connected. It becomes `false` once the listener disconnects, runs as a `once` listener, loses its owner, or its signal is cleared or garbage collected.

```lua
local signal = require('haylen.signal')

local opened = signal.new()
local connection = opened:connect(function() end, {once = true})
opened:emit()
print(connection.connected) -- false
```

### connection.blocked

Readable and writable boolean. While it is `true` the listener stays connected but is skipped.

```lua
local signal = require('haylen.signal')

local jumped = signal.new()
local sound = jumped:connect(function() print('boing') end)

sound.blocked = true
jumped:emit()
sound.blocked = false
jumped:emit()
```
