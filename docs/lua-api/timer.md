# haylen.timer

`haylen.timer` runs functions after a delay or at a fixed interval. Timers advance once per frame, after the fixed steps and before tweens and the scene update. By default they count scaled time, so `haylen.setTimeScale(0)` freezes them together with the rest of the app, and they stop while the game is paused. Use timers for cooldowns, delayed effects and periodic checks. For animated values use [haylen.tween](tween.md), and for code that reads best as a sequence of waits use Varn's `async.sleep` inside `async.spawn`.

```lua
local timer = require('haylen.timer')
```

Timer callbacks receive no arguments. An error raised inside a callback stops the app and shows the error screen with the message and its stack trace. A timer created inside another timer callback starts counting on the next frame. Every timer is dropped when the app stops or restarts.

## Options

`timer.after` and `timer.every` take an optional options table, and unknown keys raise `Unknown option '<key>'.`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `owner` | table or userdata | `nil` | Cancels the timer when the owner ends, as the [owners of haylen.events](events.md#owners) describe. The owner keeps the callback, so the callback may refer to the owner without keeping it alive. |
| `processMode` | string | `'inherit'` | `'inherit'`, `'pausable'`, `'whenPaused'`, `'always'` or `'disabled'`. A timer that inherits takes the mode of its owner, and without an owner it counts as `'pausable'`. The [lifecycle guide](../lifecycle.md#pause-and-process-modes) explains the modes. |
| `unscaled` | boolean | `false` | Counts real time, ignoring the time scale. |
| `count` | integer | `-1` | `timer.every` only. Stops after that many calls, or repeats until the timer is cancelled when it is negative. |

## Functions

### timer.after(seconds, callback, options)

Calls `callback` once, on the first frame where at least `seconds` have passed. A negative delay counts as zero, so the callback runs on the next frame. Returns the timer id, an integer that the other functions take. `callback` must be a function, otherwise the call raises `bad argument #2 to 'after' (function expected, got string)` or the equivalent for the given type.

```lua
local timer = require('haylen.timer')

local shield = {active = true}
timer.after(3, function()
    shield.active = false
    print('shield expired')
end)

-- A menu hint appears after two real seconds, even while the game is paused or in slow motion.
timer.after(2, function() print('press start') end, {processMode = 'always', unscaled = true})
```

### timer.every(seconds, callback, options)

Calls `callback` every `seconds`, the first time after one interval. The `count` option limits the number of calls, and a count of zero raises `A repeating timer needs a positive count, or a negative count to repeat until it is cancelled.` Returns the timer id. When a long frame covers several intervals, the callback runs once for each interval in that frame, so no call is lost. An interval of zero calls the callback once per frame.

```lua
local timer = require('haylen.timer')
local scene = require('haylen.scene')

local countdown = 3
timer.every(1, function()
    print(countdown)
    countdown = countdown - 1
end, {count = 3})

-- The spawner belongs to the level, so it stops when the level unloads.
local level = {enemies = 0}
timer.every(0.5, function() level.enemies = level.enemies + 1 end, {owner = level})
scene.push(level)
```

### timer.cancel(id)

Stops a timer so its callback never runs again. Cancelling a timer that already finished, was already cancelled or never existed does nothing. A callback may cancel its own timer or another one.

```lua
local timer = require('haylen.timer')

local warning = timer.after(10, function() print('hurry up') end)

-- The player reached the goal in time.
timer.cancel(warning)
```

### timer.pause(id, paused)

Pauses or resumes one timer. `paused` defaults to `true`, and `false` resumes the timer. A paused timer keeps its remaining time and does not count down.

```lua
local timer = require('haylen.timer')

local spawner = timer.every(2, function() print('spawn enemy') end)

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

Returns `true` while the timer is scheduled, including while it is paused, and `false` once it has finished, has been cancelled or never existed.

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
