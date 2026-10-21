# 📖 `haylen.jobs`

The module `haylen.jobs` spreads long Lua work over several frames, so the app keeps its frame rate while the work goes on. The Lua state belongs to the frame thread, so a job is a coroutine that runs there for a share of each frame, its time budget. Use jobs for heavy work written in Lua, such as generating a map, precomputing paths or processing a large save. The engine already decodes images and sounds and does its other C++ work on worker threads, so that work needs no jobs. The [asynchronous code](../core/lua.md#asynchronous-code) section of the Lua guide explains promises and tasks.

```lua
local jobs = require('haylen.jobs')
```

## How jobs run

Every frame, during the engine update, the jobs take turns in the order they were spawned until the budget is used up.

- A job runs until it calls [`jobs.checkpoint()`](#jobscheckpoint) after the budget is spent, or until it ends or fails. The checkpoint is the only place where a job may pause.
- A job that paused goes to the back of the line, so several jobs move on fairly.
- The first job in line always gets its turn, even in a frame that started late, so the jobs always move on.
- A job that never calls `jobs.checkpoint()` runs to its end in a single turn.
- The engine drops every job when the app stops or restarts.

## Functions

### jobs.spawn(fn, options)

Starts the function `fn` as a job and returns a Varn promise. The job begins at the next engine update, not inside the call. The function `fn` receives no arguments, so it takes its inputs from the values it closes over. The table `options` is optional and takes one key.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `owner` | table or userdata | the [default owner](events.md#owners) | Ends the job when the owner ends, before its next turn, so its code never runs again for an owner that is gone. Its promise then rejects with `The job ended with its owner.`, which never stops the app. |

The promise settles like this:

- When `fn` returns, it resolves with the first value that `fn` returned, or `nil` when it returned nothing.
- When `fn` raises an error, it rejects with the message, a line break and the stack of the job, one frame per line. An error value that is neither a string nor a number gives the message `The job failed with an error that is not a string.`.
- When the job waits for a promise or calls `coroutine.yield`, the job ends and its promise rejects with `A job may only pause at "jobs.checkpoint". Wait for promises inside "async.spawn" instead.`.

Wait for the result with `:await()` inside a task of `async.spawn`. It returns the value, or `nil` and the message when the job failed. A failed job whose promise nothing awaits or catches stops the app with its error, as an [unobserved rejection](../core/lua.md#asynchronous-code) does. When a failure does not matter, catch it with `promise:catch(fn)`.

```lua
local async = require('async')
local jobs = require('haylen.jobs')
local m = require('haylen.math')

local function generateHeights(size, seed)
    local noise = m.noise(seed)
    local heights = {}
    for y = 1, size do
        for x = 1, size do
            heights[(y - 1) * size + x] = noise:fractal(x / 32, y / 32)
        end
        jobs.checkpoint()
    end
    return heights
end

async.spawn(function()
    local heights, failure = jobs.spawn(function() return generateHeights(256, 42) end):await()
    if heights then
        print('Generated ' .. #heights .. ' tiles.')
    else
        print('The generation failed: ' .. failure)
    end
end)

-- A job that belongs to a scene ends when the scene unloads.
local level = {}
function level:load()
    jobs.spawn(function()
        for index = 1, 100000 do
            jobs.checkpoint()
        end
    end, {owner = self})
end
```

### jobs.checkpoint()

Pauses the running job until the next frame when the budget of this frame is spent, and returns at once otherwise. Call it often inside the loops of a job. Calling it outside a job, or inside a coroutine that a job started, raises `A "jobs.checkpoint" call only runs inside a job started with "jobs.spawn".`.

```lua
local jobs = require('haylen.jobs')

local world = {trees = {}}

jobs.spawn(function()
    for index = 1, 100000 do
        world.trees[index] = {x = (index * 37) % 4096, y = (index * 91) % 4096}
        jobs.checkpoint()
    end
end)
```

### jobs.setBudget(milliseconds)

Sets how many milliseconds of each frame all the jobs may use together. The default is `4`. Fractions are allowed and round to whole microseconds, with at least one microsecond. The value `math.huge` lets the jobs run without a limit. A budget of `0`, below `0` or `NaN` raises `The job budget must be a positive number of milliseconds.`.

```lua
local jobs = require('haylen.jobs')

-- A loading screen can give the jobs most of the frame.
jobs.setBudget(12)
```

### jobs.budget()

Returns the current budget in milliseconds. After `jobs.setBudget(math.huge)` it returns a very large number.

```lua
local jobs = require('haylen.jobs')

print('The jobs may use ' .. jobs.budget() .. ' milliseconds of each frame.')
```

### jobs.runningCount()

Returns the number of jobs that have not finished yet. It counts the jobs that have not started and the job that is running now.

```lua
local graphics2d = require('haylen.graphics2d')
local jobs = require('haylen.jobs')
local scene = require('haylen.scene')

jobs.spawn(function()
    for step = 1, 50000 do
        jobs.checkpoint()
    end
end)

scene.push({
    renderUi = function(self)
        if jobs.runningCount() > 0 then
            graphics2d.beginScreen()
            graphics2d.drawText(nil, 'Loading...', 80, 980, {size = 36, color = '#FFFFFFFF'})
        end
    end,
})
```

## Errors

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | The options of `jobs.spawn` hold a key other than `owner`. |
| `An owner must be a table or a userdata, not <type>.` | The option `owner` is another type. |
| `bad argument #<n> to '<function>' (<type> expected, got <type>)` | The first argument of `jobs.spawn` is not a function, its second one is not a table, or the budget of `jobs.setBudget` is not a number. |
| `A "jobs.checkpoint" call only runs inside a job started with "jobs.spawn".` | The function `jobs.checkpoint` ran outside a job. |
| `The job budget must be a positive number of milliseconds.` | The budget of `jobs.setBudget` is `0`, negative or `NaN`. |

These messages reject the promise of a job.

| Message | Cause |
| --- | --- |
| `<message>` followed by the stack of the job | The function of the job raised an error. |
| `The job failed with an error that is not a string.` | The function of the job raised an error value that is neither a string nor a number. |
| `A job may only pause at "jobs.checkpoint". Wait for promises inside "async.spawn" instead.` | The job waited for a promise or called `coroutine.yield`. |
| `The job ended with its owner.` | The owner of the job ended before the job finished. This rejection never stops the app. |
