# haylen.jobs

The module `haylen.jobs` spreads long Lua work across several frames so the app keeps its frame rate. The Lua state belongs to the frame thread, so jobs run there as coroutines that share a time budget each frame, and a job pauses at `jobs.checkpoint()` once the budget is spent. Use jobs for heavy work written in Lua, such as generating a map, precomputing paths or processing a large save. Engine work written in C++, such as image and audio decoding, already runs on worker threads and does not need jobs.

```lua
local jobs = require('haylen.jobs')
```

Every frame, during the engine update, the jobs take turns in the order they were spawned until the budget is used up. A job that spends the rest of the budget goes to the back of the line, so several jobs progress fairly. A job that never calls `jobs.checkpoint()` runs to its end in a single turn. Every job is dropped when the app stops or restarts.

## Functions

### jobs.spawn(fn, ...)

Starts `fn(...)` as a job and returns a Varn promise. The job begins on the next engine update, not inside the call. The promise resolves with the first value that `fn` returns, or `nil` when it returns nothing. When `fn` raises an error, the promise rejects with the message followed by the stack of the job, one frame per line.

Wait for the result with `:await()` inside `async.spawn` from Varn's `async` module. It returns the value, or `nil` and the error message when the job failed. The argument `fn` must be a function, otherwise the call raises `bad argument #1 to 'spawn' (function expected, got nil)` or the equivalent for the given type.

```lua
local jobs = require('haylen.jobs')
local async = require('async')
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
    local heights, failure = jobs.spawn(generateHeights, 256, 42):await()
    if heights then
        print('generated ' .. #heights .. ' tiles')
    else
        print('generation failed: ' .. failure)
    end
end)
```

### jobs.checkpoint()

Pauses the running job until the next frame when this frame's budget is spent, and returns right away otherwise. Call it regularly inside loops. It is the only way a job may pause. Awaiting a promise or calling `coroutine.yield` inside a job rejects its promise with `A job may only pause at "jobs.checkpoint". Wait for promises inside "async.spawn" instead.`

Calling it outside a job raises `A "jobs.checkpoint" call only runs inside a job started with "jobs.spawn".`

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

Sets how many milliseconds of each frame the jobs may use together. The default is `4`. Fractions are allowed and are rounded to whole microseconds, and `math.huge` lets the jobs run without a limit. A value that is not positive raises `The job budget must be a positive number of milliseconds.`

```lua
local jobs = require('haylen.jobs')

-- Loading screens can give jobs most of the frame.
jobs.setBudget(12)
```

### jobs.budget()

Returns the current budget in milliseconds.

```lua
local jobs = require('haylen.jobs')

print('jobs may use ' .. jobs.budget() .. ' ms per frame')
```

### jobs.runningCount()

Returns the number of jobs that have not finished yet, including jobs that have not started.

```lua
local jobs = require('haylen.jobs')
local graphics2d = require('haylen.graphics2d')

jobs.spawn(function()
    for step = 1, 50000 do
        jobs.checkpoint()
    end
end)

require('haylen.scene').push({
    renderUi = function(self)
        if jobs.runningCount() > 0 then
            graphics2d.beginScreen()
            graphics2d.drawText(nil, 'Loading...', 80, 980, {size = 36, color = '#FFFFFFFF'})
        end
    end,
})
```
