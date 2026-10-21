# 📖 `haylen.timer`

The module `haylen.timer` calls a Lua function once after a delay, or again and again at an interval. Use it for cooldowns, delayed effects and checks that repeat. For a value that changes smoothly over time, use [`haylen.tween`](tween.md). For code that reads best as a list of waits, use `async.sleep` of Varn inside `async.spawn`. The [lifecycle guide](../core/lifecycle.md) explains when timers run in the frame, how the [pause and the process modes](../core/lifecycle.md#pause-and-process-modes) stop them and how [owners](../core/lifecycle.md#subscription-scopes) end them.

```lua
local timer = require('haylen.timer')
```

## How timers run

- Timers advance once per frame, after the fixed steps and before tweens and the scene update.
- A timer fires on the first frame where at least its time has passed. Its callback receives no arguments.
- By default a timer counts scaled time. So [`haylen.setTimeScale`](haylen.md) slows it down, and a time scale of `0` freezes it. With `unscaled = true` it counts real time instead.
- By default a timer stops while the game is paused. Its `processMode` option changes that.
- When several timers are due in the same frame, they fire in the order their time ran out. On a tie, the timer created first fires first. So a callback can still cancel another timer that is due later in the same frame.
- A timer created inside a timer callback starts counting on the next frame.
- An error raised inside a callback stops the app and shows the error screen with the message and its stack trace, as [errors](../core/lua.md#errors-and-stack-traces) describes.
- When the app stops or restarts, every timer is dropped.

## Options

The functions `timer.after` and `timer.every` take an optional `options` table.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `owner` | table or userdata | none | An object, such as a scene, whose end cancels the timer, as [subscription scopes](../core/lifecycle.md#subscription-scopes) describe. The owner holds the callback, so the callback may refer to the owner without keeping it alive. |
| `processMode` | string | `'inherit'` | When the timer counts while the game is paused: `'inherit'`, `'pausable'`, `'whenPaused'`, `'always'` or `'disabled'`. The default `'inherit'` follows the mode of the owner every frame, and without an owner it counts as `'pausable'`. The [lifecycle guide](../core/lifecycle.md#pause-and-process-modes) explains each mode. |
| `unscaled` | boolean | `false` | Counts real time and ignores the time scale. |
| `count` | integer | `-1` | Only for `timer.every`. The timer stops after this many calls. A negative count repeats until the timer is cancelled. |

Without an `owner`, a timer that a module creates at its top level in development belongs to that module, so a [reload](hotReload.md) of the module cancels it.

An unknown key raises `Unknown option "<key>".`, so `count` raises it for `timer.after`. A value of the wrong type raises `The option "<key>" of "<function>" is invalid: <reason>.`, such as `The option "processMode" of "after" is invalid: unknown value 'paused'.`. An `owner` that is not a table or a userdata raises `An owner must be a table or a userdata, not <type>.`.

```lua
local timer = require('haylen.timer')

-- A hint for the pause menu counts real seconds, even while the game is paused or in slow motion.
timer.after(2, function() print('Press start to continue.') end, {processMode = 'always', unscaled = true})
```

## Functions

### timer.after(seconds, callback, options)

Calls `callback` once, on the first frame where at least `seconds` have passed. A delay of `0` or less runs the callback the next time the timers advance. The table `options` is optional and takes the keys of [Options](#options) except `count`.

Returns the id of the timer, an integer that [`timer.cancel`](#timercancelid), [`timer.pause`](#timerpauseid-paused) and [`timer.active`](#timeractiveid) take. A `seconds` that is not a number raises `bad argument #1 to 'after' (number expected, got <type>)`, and a `callback` that is not a function raises `bad argument #2 to 'after' (function expected, got <type>)`.

```lua
local timer = require('haylen.timer')

local shield = {active = true}
timer.after(3, function()
    shield.active = false
    print('The shield expired.')
end)
```

### timer.every(seconds, callback, options)

Calls `callback` every `seconds`, the first time after one interval. The option `count` limits the number of calls, and a count of `0` raises `A repeating timer needs a positive count, or a negative count to repeat until it is cancelled.`. The table `options` is optional and takes every key of [Options](#options). Returns the id of the timer.

A long frame can cover several intervals. The callback then runs once for each interval in that frame, so no call is lost. A timer runs at most 256 times in one frame, and a timer with an interval that tiny drops the intervals it still owes after that. An interval of `0` or less calls the callback once per frame.

```lua
local timer = require('haylen.timer')
local scene = require('haylen.scene')

local countdown = 3
timer.every(1, function()
    print('Starting in ' .. countdown .. '.')
    countdown = countdown - 1
end, {count = 3})

-- The spawner belongs to the level, so it stops when the level unloads.
local level = {enemies = 0}
timer.every(0.5, function() level.enemies = level.enemies + 1 end, {owner = level})
scene.push(level)
```

### timer.cancel(id)

Stops the timer so its callback never runs again. Cancelling a timer that already finished, was already cancelled or never existed does nothing. A callback may cancel its own timer or another one.

```lua
local timer = require('haylen.timer')

local warning = timer.after(10, function() print('Hurry up.') end)

-- The player reached the goal in time.
timer.cancel(warning)
```

### timer.pause(id, paused)

Pauses or resumes one timer. The argument `paused` is `true` when it is left out, and `false` resumes the timer. A paused timer keeps the time it has left and does not count down. Pausing a timer that does not exist does nothing.

```lua
local timer = require('haylen.timer')

local spawner = timer.every(2, function() print('An enemy appears.') end)

local function openMenu()
    timer.pause(spawner)
end

local function closeMenu()
    timer.pause(spawner, false)
end

openMenu()
closeMenu()
```

### timer.active(id)

Returns `true` while the timer is scheduled, also while it is paused. Returns `false` once it has finished, was cancelled or never existed. During the last call of its callback, a timer already reads as not active.

```lua
local timer = require('haylen.timer')

local cooldown = timer.after(0.8, function() end)

local function tryDash()
    if timer.active(cooldown) then
        return false
    end
    cooldown = timer.after(0.8, function() end)
    return true
end

print(tryDash()) -- false
```

## Errors

| Message | Cause |
| --- | --- |
| `bad argument #1 to 'after' (number expected, got <type>)` | The `seconds` of `timer.after` or `timer.every` is not a number. |
| `bad argument #2 to 'after' (function expected, got <type>)` | The `callback` of `timer.after` or `timer.every` is not a function. |
| `bad argument #3 to 'after' (table expected, got <type>)` | The `options` of `timer.after` or `timer.every` is neither a table nor `nil`. |
| `Unknown option "<key>".` | The options table has a key that the function does not take, such as `count` for `timer.after`. |
| `Option tables only accept string keys.` | The options table has a key that is not a string. |
| `The option "<key>" of "<function>" is invalid: <reason>.` | An option has a value of the wrong type, or `processMode` names no mode. |
| `An owner must be a table or a userdata, not <type>.` | The option `owner` is another type. |
| `A repeating timer needs a positive count, or a negative count to repeat until it is cancelled.` | The option `count` of `timer.every` is `0`. |
| `bad argument #1 to 'cancel' (number expected, got <type>)` | The id of `timer.cancel`, `timer.pause` or `timer.active` is not an integer. A negative id raises `bad argument #1 to 'cancel' (expected a non-negative integer)`. |

The function in each message is the one that was called, such as `'every'` or `'pause'`.
