# haylen.collections

`haylen.collections` provides object pools that recycle Lua values, such as sprites, bullets and particles, so hot paths stop creating garbage, ring buffers that keep the newest values of a history, such as recent positions for a trail or frame times for a graph, and float buffers that Lua and C++ share, so thousands of sprites or bodies change without a table for each one.

```lua
local collections = require('haylen.collections')
```

## Pools

### collections.newPool(options)

Creates a `Pool`. Objects come from the `create` function the first time and are recycled after that. Unknown keys raise `Unknown option '<key>'.`, a missing `create` raises `create must be a function`, and a `create` that returns `nil` raises `The create function of the pool returned nil.`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `create` | function | required | Returns a new object. |
| `reset` | function | `nil` | Called as `reset(object, ...)` on every acquire with the arguments of `acquire`, to set the object up again. |
| `release` | function | `nil` | Called as `release(object)` on every release, to hide or stop the object. |
| `capacity` | integer | `0` | Most objects the pool lends at once, or no limit with `0`. |
| `prewarm` | integer | `0` | Objects created right away, so the first acquisitions do not create any. |

```lua
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local assets = require('haylen.assets')

local texture = assets.texture('images/bullet.png')
local bullets = collections.newPool({
    create = function()
        return graphics2d.newSprite(texture)
    end,
    reset = function(sprite, x, y)
        sprite.x = x
        sprite.y = y
    end,
    release = function(sprite)
        sprite.rotation = 0
    end,
    capacity = 200,
    prewarm = 50,
})

require('haylen.scene').push({
    render = function(self)
        graphics2d.beginScreen()
        bullets:each(function(sprite) sprite:draw() end)
    end,
})
```

### pool:acquire(...)

Takes an idle object, or creates one while the pool is below its capacity, passes it to `reset` with the arguments and returns it. Returns `nil` when the pool lends its whole capacity.

```lua
local collections = require('haylen.collections')

local shots = collections.newPool({create = function() return {} end, reset = function(shot, x, y) shot.x, shot.y = x, y end, capacity = 2})
local first = shots:acquire(10, 20)
print(first.x, shots:acquire(0, 0) ~= nil, shots:acquire(0, 0)) -- 10 true nil
```

### pool:release(object)

Returns an active object to the pool after calling `release`, and returns `true`. Returns `false` for an object the pool does not lend, such as one released already.

```lua
local collections = require('haylen.collections')

local pool = collections.newPool({create = function() return {} end})
local item = pool:acquire()
print(pool:release(item), pool:release(item)) -- true false
```

### pool:releaseAll(), pool:each(fn), pool:prewarm(count)

`releaseAll` releases every active object. `each` calls `fn(object)` for every active object, from a list taken before the first call, so `fn` may release objects. `prewarm` creates idle objects until the pool holds `count` objects or reaches its capacity.

```lua
local collections = require('haylen.collections')

local sparks = collections.newPool({create = function() return {life = 0} end, reset = function(spark) spark.life = 1 end})
for i = 1, 5 do sparks:acquire() end
require('haylen.scene').push({
    update = function(self, dt)
        sparks:each(function(spark)
            spark.life = spark.life - dt
            if spark.life <= 0 then sparks:release(spark) end
        end)
    end,
    exit = function(self)
        sparks:releaseAll()
    end,
})
```

### Pool properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `active` | integer | read | Objects lent out. |
| `idle` | integer | read | Objects waiting to be acquired. |
| `capacity` | integer | read | Most objects lent at once, or `0` without a limit. |

## Ring buffers

### collections.newRingBuffer(capacity)

Creates a `RingBuffer` that holds up to `capacity` values over a buffer that never reallocates. Pushing into a full buffer drops the oldest value. A capacity of zero raises `A ring buffer needs a capacity of at least one.`

```lua
local collections = require('haylen.collections')

local trail = collections.newRingBuffer(16)
print(trail.capacity, trail.size) -- 16 0
```

### buffer:push(value), buffer:pop()

`push` appends a value as the newest one and returns `true` when the oldest value had to go to make room. `pop` removes and returns the oldest value, or `nil` when the buffer is empty.

```lua
local collections = require('haylen.collections')

local inputs = collections.newRingBuffer(3)
for _, key in ipairs({'left', 'left', 'right', 'jump'}) do
    inputs:push(key)
end
print(inputs:pop(), inputs.size) -- left 2
```

### buffer:front(), buffer:back(), buffer:get(position), buffer:values()

`front` returns the oldest value and `back` the newest one, or `nil` when the buffer is empty. `get` returns the value at a position from 1 for the oldest, or `nil` past the newest. `values` returns a list from the oldest to the newest value.

```lua
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')

local trail = collections.newRingBuffer(20)
require('haylen.scene').push({
    update = function(self, dt)
        trail:push({x = math.random(0, 100), y = math.random(0, 100)})
    end,
    render = function(self)
        graphics2d.beginScreen()
        local points = trail:values()
        if #points > 1 then
            graphics2d.drawPolyline(points, 2, '#FFFFFFFF')
        end
    end,
})
```

### buffer:clear(), buffer.size, buffer.capacity, buffer.full

`clear` removes every value. `size` is the number of values, `capacity` the most values the buffer holds and `full` whether it holds that many.

```lua
local collections = require('haylen.collections')

local frames = collections.newRingBuffer(2)
frames:push(16.6)
frames:push(16.8)
print(frames.full, frames.size)
frames:clear()
print(frames.size) -- 0
```

## Float buffers

A float buffer holds a fixed number of floats in memory that C++ reads and writes in place. Lua fills it one value at a time with `buffer[index]` or many values at once with `set`, and the bulk APIs of the engine take it instead of a table per item: `graphics2d.drawBatch` and `SpriteBatch:writeFields` of [haylen.graphics2d](graphics2d.md) read sprites from it, `world:readTransforms` of [haylen.physics2d](physics2d.md) writes the transforms of many bodies into it, and `emitter:readPositions` of [haylen.particles2d](particles2d.md) writes particle positions into it. Values count from one like a Lua array, and every access checks the bounds, raising `Float buffer positions <first> to <last> fall outside its size of <size>.` outside them. Values are stored as 32-bit floats, like the GPU uses them. The [performance section of the Lua guide](../lua.md#performance) shows when a buffer pays off.

### collections.newFloatBuffer(size, value)

Returns a `haylen.FloatBuffer` of `size` floats, all set to `value`, which defaults to `0`. The size never changes. A negative size raises `the size cannot be negative`.

```lua
local collections = require('haylen.collections')

local positions = collections.newFloatBuffer(2 * 1000)
print(#positions) -- 2000
```

### buffer[index], #buffer

Reads and writes one value, and `#buffer` returns the size. Index access skips the method lookup, so it is the fastest way to touch a single value from Lua, while a plain Lua array is still faster for values that only Lua uses.

```lua
local collections = require('haylen.collections')

local positions = collections.newFloatBuffer(4)
positions[1], positions[2] = 120, 64
positions[3] = positions[1] + 16
print(#positions, positions[3]) -- 4 136.0
```

### buffer:set(first, ...), buffer:set(first, list)

Copies numbers into the buffer from the position `first` on, either the arguments after `first` or every value of a Lua array. Copying a whole array with one call is the fast way to hand values that Lua computed in a plain array to C++. A value that is not a number raises `Float buffer values are numbers, and entry <n> of the list is not.`.

```lua
local collections = require('haylen.collections')

local buffer = collections.newFloatBuffer(6)
buffer:set(1, 10, 20)
buffer:set(3, {30, 40, 50, 60})
print(buffer[4]) -- 40.0
```

### buffer:get(first, count)

Returns `count` values from the position `first` on as separate results, one value when `count` is omitted.

```lua
local collections = require('haylen.collections')

local buffer = collections.newFloatBuffer(6)
buffer:set(1, {1, 2, 3, 4, 5, 6})
local x, y, rotation = buffer:get(4, 3)
print(x, y, rotation) -- 4.0 5.0 6.0
```

### buffer:fill(value, first, count)

Sets every value to `value`, or `count` values from the position `first` on when both are given.

```lua
local collections = require('haylen.collections')

local alphas = collections.newFloatBuffer(100)
alphas:fill(1)
alphas:fill(0, 51, 50)
print(alphas[50], alphas[51]) -- 1.0 0.0
```
