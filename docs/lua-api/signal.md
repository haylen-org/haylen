# 📖 `haylen.signal`

The module `haylen.signal` creates signals. A signal is a value that an object keeps to announce that something happened, such as the `healthChanged` signal of a player. Other code connects functions to it, and emitting the signal calls every connected function with the emitted values. The object never needs to know who listens. Use a signal when one object owns the news. Use [`haylen.events`](events.md) for news by name that the whole app may care about. This page lists the functions, the `Signal` and `Connection` types and the errors.

```lua
local signal = require('haylen.signal')
```

## Delivery

- **Order.** Listeners with a higher priority run first. Listeners with the same priority run in the order they connected.
- **Changes during an emit.** A listener may connect and disconnect listeners while the signal emits. A listener connected during an emit is first called by the next emit. A listener that an earlier one disconnected is skipped.
- **Errors.** An error raised by a listener stops the emit. The remaining listeners are skipped, and the error reaches the code that called `emit`.
- **Deferred listeners.** A listener connected with the option `deferred` runs at the end of the frame instead, after every scene has rendered, with the values of the emit. An error raised there stops the app and shows the error screen.
- **Blocking.** The property `blocked` of a signal silences all its listeners, and the property `blocked` of a connection silences one listener. Neither disconnects anything.

## Functions

### signal.new(name)

Creates a [`Signal`](#signal) with no listeners and returns it. The string `name` is optional. It labels the signal in [`signal.list()`](#signallist), which lists only named signals, and the property `name` returns it.

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

player.healthChanged:connect(function(health) print('The health is now ' .. health .. '.') end)
player:damage(30)
```

### signal.list()

Returns a list with one table for each named signal that is still alive, in no particular order. The list never keeps a signal alive. Use it to find listeners that pile up. Each table has these fields.

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | The name given to `signal.new`. |
| `listeners` | integer | The connected listeners. |
| `emissions` | integer | Every emit since the signal was created, blocked ones included. |
| `stale` | integer | Listeners whose owner is already gone, such as an owner table that the garbage collector freed. The next emit or the end of the frame removes them. |

```lua
local signal = require('haylen.signal')

local scoreChanged = signal.new('scoreChanged')
scoreChanged:connect(function() end)
scoreChanged:emit(10)

for _, entry in ipairs(signal.list()) do
    print(entry.name .. ': ' .. entry.listeners .. ' listeners, ' .. entry.emissions .. ' emits, ' .. entry.stale .. ' stale.')
end
```

## Signal

A `Signal`, named `haylen.Signal` in messages, is the value that [`signal.new`](#signalnewname) returns. Reading a member that it does not have raises `The type "haylen.Signal" has no member "<name>".`.

### sig:connect(fn, options)

Connects the function `fn` to the signal and returns a [`Connection`](#connection). The same function may connect more than once, and it is then called once for each connection. The table `options` is optional and takes these keys.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `priority` | integer | `0` | Listeners with a higher priority run first. |
| `once` | boolean | `false` | Disconnects the listener right before its first call. |
| `deferred` | boolean | `false` | Calls the listener at the end of the frame, once for each emit, with the values of that emit. A call is skipped when the listener disconnects before then. A deferred `once` listener disconnects at its first emit and still runs at the end of that frame. |
| `owner` | table or userdata | the default owner | Disconnects the listener when the owner ends, as the [owners](events.md#owners) of `haylen.events` describe. |

An unknown key raises `Unknown option "<key>".`, and the [errors](#errors) section lists the other mistakes.

Without an owner, the signal keeps `fn` alive until it disconnects. With an owner, the owner keeps `fn` instead. The signal holds a function the way an owner does, so a listener that uses the signal, or a table that holds it, never keeps the signal alive. When the signal itself is garbage collected, its listeners end with it. Dropping the returned connection does not disconnect the listener.

```lua
local signal = require('haylen.signal')

local coinCollected = signal.new()
local hud = {coins = 0}

coinCollected:connect(function(amount) hud.coins = hud.coins + amount end)
coinCollected:connect(function() print('The combo counter goes first.') end, {priority = 10})
coinCollected:connect(function() print('The first coin ever.') end, {once = true})
coinCollected:connect(function() print('The coins are saved at the end of the frame.') end, {deferred = true})

coinCollected:emit(1)
coinCollected:emit(5)
print(hud.coins) -- 6
```

### sig:emit(...)

Calls every connected function with the values given to `emit`, and returns nothing. A blocked signal calls nothing but still counts the emit.

```lua
local signal = require('haylen.signal')

local damaged = signal.new()
damaged:connect(function(target, amount) print('The ' .. target .. ' took ' .. amount .. ' damage.') end)
damaged:connect(function(target) print('The ' .. target .. ' flashes.') end)

damaged:emit('goblin', 12)
```

### sig:clear()

Disconnects every listener at once. Their connections then report `connected` as `false`.

```lua
local signal = require('haylen.signal')

local waveStarted = signal.new()
waveStarted:connect(function(wave) print('Wave ' .. wave .. ' starts.') end)

-- The level ends, so no listener should run anymore.
waveStarted:clear()
waveStarted:emit(2)
print(waveStarted.size) -- 0
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of connected listeners. |
| `emissionCount` | integer | read | Every emit since the signal was created, blocked ones included. |
| `name` | string | read | The name given to `signal.new`, or an empty string. |
| `blocked` | boolean | read and write | While it is `true`, an emit calls no listener and still counts. It starts as `false`. |

Writing any property other than `blocked` raises `The type "haylen.Signal" has no writable property "<name>".`.

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
print(steps, stepped.emissionCount, stepped.name) -- 1 2 stepped
```

## Connection

A `Connection`, named `haylen.Connection` in messages, stands for one listener that something registered. The method `sig:connect` returns it, and so do [`events.on`](events.md#eventsonname-fn-options), [`scene.listen`](scene.md#scenelistenowner-source-fn-options), [`socket:on`](net.md#socketonevent-listener-options) and the other functions of the engine that add a listener. Reading a member that it does not have raises `The type "haylen.Connection" has no member "<name>".`, and writing any property other than `blocked` raises `The type "haylen.Connection" has no writable property "<name>".`.

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

Read-only boolean that is `true` while the listener is connected. It becomes `false` once the listener disconnects, runs as a `once` listener or loses its owner, and once its signal is cleared or garbage collected.

```lua
local signal = require('haylen.signal')

local opened = signal.new()
local connection = opened:connect(function() end, {once = true})
opened:emit()
print(connection.connected) -- false
```

### connection.blocked

Readable and writable boolean, `false` at first. While it is `true`, the listener stays connected but is skipped.

```lua
local signal = require('haylen.signal')

local jumped = signal.new()
local sound = jumped:connect(function() print('The jump sound plays.') end)

sound.blocked = true
jumped:emit()
sound.blocked = false
jumped:emit()
```

## C++

C++ code uses the typed `core::Signal` template, which has the same priorities, `once` listeners, owners and blocking. A slot whose option `queue` is the frame queue of the engine runs at the end of the frame, like a deferred listener. A `core::ScopedConnection` disconnects its listener when it is destroyed, and a `core::ConnectionScope` disconnects every connection it holds.

```cpp
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/core/Signal.hpp"

class Player final {
  public:
    haylen::core::Signal<int> healthChanged;
};

// The bar stops listening when it is destroyed, so the player never calls a bar that is gone.
class HealthBar final {
  public:
    explicit HealthBar(Player& player) : connection(player.healthChanged.connect([this](int health) { shown = health; }, {.priority = 10})) {}

  private:
    int shown = 100;
    haylen::core::ScopedConnection connection;
};
```

## Errors

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | The options of `sig:connect` hold a key that it does not take. |
| `Option tables only accept string keys.` | The options of `sig:connect` hold a key that is not a string. |
| `The option "<key>" of "connect" is invalid: <reason>.` | An option has a value of the wrong type, such as a `priority` that is not an integer or a `deferred` that is not a boolean. |
| `An owner must be a table or a userdata, not <type>.` | The option `owner` is another type. |
| `The type "haylen.Signal" has no member "<name>".` | The code read a member that a signal does not have. |
| `The type "haylen.Signal" has no writable property "<name>".` | The code wrote a property of a signal other than `blocked`. |
| `The type "haylen.Connection" has no member "<name>".` | The code read a member that a connection does not have. |
| `The type "haylen.Connection" has no writable property "<name>".` | The code wrote a property of a connection other than `blocked`. |
| `bad argument #<n> to '<function>' (<type> expected, got <type>)` | An argument has the wrong type, such as a listener that is not a function, a name of `signal.new` that is not a string, options that are not a table or a `blocked` that is not a boolean. |
