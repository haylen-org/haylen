# 📖 `haylen.collections`

The module `haylen.collections` holds three containers for code that runs every frame. Pools recycle Lua values, such as sprites, bullets and particles, so hot code stops making garbage. Ring buffers keep the newest values of a history, such as the recent positions of a trail or the times of the last frames. Float buffers hold numbers that Lua and C++ share, so thousands of sprites or bodies change without a table for each one. The [performance section](../core/lua.md#performance) of the Lua guide shows when each one pays off.

```lua
local collections = require('haylen.collections')
```

## Pools

A pool lends objects and takes them back. The method `pool:acquire` takes an idle object, or makes a new one with the function `create`, and `pool:release` gives it back for the next acquire. An object that the pool lends is active, and an object that waits in the pool is idle. With a capacity, the pool lends at most that many objects at once.

### collections.newPool(options)

Creates a [`Pool`](#pool) and returns it. The table `options` is required and takes these keys.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `create` | function | required | Returns a new object. It must not return `nil`. |
| `reset` | function | none | Called as `reset(object, ...)` on every acquire, with the arguments of `acquire`, to set the object up again. |
| `release` | function | none | Called as `release(object)` on every release, to hide or stop the object. |
| `capacity` | integer | `0` | The most objects that the pool lends at once, or no limit with `0`. |
| `prewarm` | integer | `0` | Objects to create right away, so the first acquires create none. |

A missing `create` raises `bad argument #1 to 'newPool' (create must be a function)`, and the [errors](#errors) section lists the other mistakes.

```lua
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

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

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        bullets:each(function(sprite) sprite:draw() end)
    end,
})
```

## Pool

A `Pool`, named `haylen.Pool` in messages, is the value that [`collections.newPool`](#collectionsnewpooloptions) returns. Errors raised by its functions `create`, `reset` and `release` reach the code that called the method.

### pool:acquire(...)

Takes an idle object, the one released last, or creates one with `create` while the pool lends fewer objects than its capacity. Then it calls `reset(object, ...)` with the arguments and returns the object. It returns `nil` when the pool already lends its whole capacity.

```lua
local collections = require('haylen.collections')

local shots = collections.newPool({
    create = function() return {} end,
    reset = function(shot, x, y) shot.x, shot.y = x, y end,
    capacity = 2,
})

local first = shots:acquire(10, 20)
local second = shots:acquire(0, 0)
print(first.x, second ~= nil, shots:acquire(0, 0)) -- 10 true nil
```

### pool:release(object)

Gives an active object back to the pool, calls `release(object)` and returns `true`. It returns `false` and does nothing for an object that the pool does not lend, such as one released already or one of another pool.

```lua
local collections = require('haylen.collections')

local pool = collections.newPool({create = function() return {} end})
local item = pool:acquire()
print(pool:release(item), pool:release(item)) -- true false
```

### pool:releaseAll()

Releases every active object, calling `release` for each one.

```lua
local collections = require('haylen.collections')
local scene = require('haylen.scene')

local enemies = collections.newPool({create = function() return {health = 10} end})
for _ = 1, 8 do
    enemies:acquire()
end

scene.push({
    exit = function(self)
        enemies:releaseAll()
        print(enemies.active, enemies.idle) -- 0 8
    end,
})
```

### pool:each(fn)

Calls `fn(object)` for every active object, in no particular order. It walks a list taken before the first call, so `fn` may release objects. Objects acquired during the walk wait for the next call.

```lua
local collections = require('haylen.collections')
local scene = require('haylen.scene')

local sparks = collections.newPool({
    create = function() return {life = 0} end,
    reset = function(spark) spark.life = 1 end,
})
for _ = 1, 5 do
    sparks:acquire()
end

scene.push({
    update = function(self, dt)
        sparks:each(function(spark)
            spark.life = spark.life - dt
            if spark.life <= 0 then
                sparks:release(spark)
            end
        end)
    end,
})
```

### pool:prewarm(count)

Creates idle objects until the pool holds `count` objects, active and idle together, or reaches its capacity. It does nothing when the pool already holds that many. Call it behind a loading screen, so the first busy frame creates nothing.

```lua
local collections = require('haylen.collections')

local particles = collections.newPool({create = function() return {x = 0, y = 0} end, capacity = 500})
particles:prewarm(300)
print(particles.idle) -- 300
```

### Pool properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `active` | integer | read | The objects lent out now. |
| `idle` | integer | read | The objects that wait to be acquired. |
| `capacity` | integer | read | The most objects that the pool lends at once, or `0` without a limit. |

```lua
local collections = require('haylen.collections')

local bullets = collections.newPool({create = function() return {} end, capacity = 100, prewarm = 10})
bullets:acquire()
print(bullets.active, bullets.idle, bullets.capacity) -- 1 9 100
```

## Ring buffers

A ring buffer keeps at most a fixed number of values, its capacity, in one block of memory that never grows. Pushing into a full buffer drops the oldest value. Positions count from `1` for the oldest value.

### collections.newRingBuffer(capacity)

Creates a [`RingBuffer`](#ringbuffer) that holds up to `capacity` values and returns it. A capacity of `0` raises `A ring buffer needs a capacity of at least one.`.

```lua
local collections = require('haylen.collections')

local trail = collections.newRingBuffer(16)
print(trail.capacity, trail.size) -- 16 0
```

## RingBuffer

A `RingBuffer`, named `haylen.RingBuffer` in messages, is the value that [`collections.newRingBuffer`](#collectionsnewringbuffercapacity) returns. It holds any Lua values.

### ring:push(value)

Adds `value` as the newest value. It returns `true` when the buffer was full and the oldest value had to go to make room, and `false` otherwise.

```lua
local collections = require('haylen.collections')

local inputs = collections.newRingBuffer(3)
for _, key in ipairs({'left', 'left', 'right'}) do
    inputs:push(key)
end
print(inputs:push('jump')) -- true
```

### ring:pop()

Removes the oldest value and returns it, or returns `nil` when the buffer is empty.

```lua
local collections = require('haylen.collections')

local inputs = collections.newRingBuffer(3)
for _, key in ipairs({'left', 'left', 'right', 'jump'}) do
    inputs:push(key)
end
print(inputs:pop(), inputs.size) -- left 2
```

### ring:front()

Returns the oldest value without removing it, or `nil` when the buffer is empty.

```lua
local collections = require('haylen.collections')

local frameTimes = collections.newRingBuffer(60)
frameTimes:push(16.6)
frameTimes:push(17.1)
print(frameTimes:front()) -- 16.6
```

### ring:back()

Returns the newest value without removing it, or `nil` when the buffer is empty.

```lua
local collections = require('haylen.collections')

local frameTimes = collections.newRingBuffer(60)
frameTimes:push(16.6)
frameTimes:push(17.1)
print(frameTimes:back()) -- 17.1
```

### ring:get(position)

Returns the value at the integer `position`, from `1` for the oldest value up to `ring.size` for the newest one. A position outside that range returns `nil`.

```lua
local collections = require('haylen.collections')

local moves = collections.newRingBuffer(4)
for _, move in ipairs({'down', 'right', 'punch'}) do
    moves:push(move)
end
print(moves:get(1), moves:get(3), moves:get(4)) -- down punch nil
```

### ring:values()

Returns a new list of every value, from the oldest to the newest.

```lua
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local trail = collections.newRingBuffer(20)
scene.push({
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

### ring:clear()

Removes every value. The capacity stays the same.

```lua
local collections = require('haylen.collections')

local frames = collections.newRingBuffer(2)
frames:push(16.6)
frames:push(16.8)
frames:clear()
print(frames.size) -- 0
```

### Ring buffer properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of values in the buffer. |
| `capacity` | integer | read | The most values that the buffer holds. |
| `full` | boolean | read | Whether the buffer holds `capacity` values, so the next push drops the oldest one. |

```lua
local collections = require('haylen.collections')

local frames = collections.newRingBuffer(2)
frames:push(16.6)
frames:push(16.8)
print(frames.size, frames.capacity, frames.full) -- 2 2 true
```

## Float buffers

A float buffer holds a fixed number of numbers in memory that C++ reads and writes in place, with no copy. The bulk functions of the engine take it instead of a table for each item. For example, [`graphics2d.drawBatch`](graphics2d.md#graphics2ddrawbatchtexture-buffer-layout-order) and [`batch:writeFields`](graphics2d.md#batchwritefieldsbuffer-fields-first) read sprites from it, [`world:readTransforms`](physics2d.md#worldreadtransformsbodies-buffer-first-interpolated) writes the transforms of many bodies into it, and [`emitter:readPositions`](particles2d.md#emitterreadpositionsbuffer-first) writes the positions of particles into it.

- Positions count from `1`, like a Lua list.
- Every access checks the positions. A position outside the buffer raises `Float buffer positions <first> to <last> fall outside its size of <size>.`. A position that is not an integer counts as position `0`.
- The buffer stores 32-bit floats, as the GPU uses them, so a value reads back as the nearest 32-bit float.

### collections.newFloatBuffer(size, value)

Creates a [`FloatBuffer`](#floatbuffer) of `size` numbers and returns it. Every number starts as `value`, which defaults to `0`. The size never changes. A negative size raises `bad argument #1 to 'newFloatBuffer' (the size cannot be negative)`.

```lua
local collections = require('haylen.collections')

local positions = collections.newFloatBuffer(2 * 1000)
local alphas = collections.newFloatBuffer(1000, 1)
print(#positions, alphas[1]) -- 2000 1.0
```

## FloatBuffer

A `FloatBuffer`, named `haylen.FloatBuffer` in messages, is the value that [`collections.newFloatBuffer`](#collectionsnewfloatbuffersize-value) returns, or that an engine function returns with its results, such as [`noise:sampleGrid`](math.md#noisesamplegridoptions). It has no properties. Reading a member that it does not have raises `The type "haylen.FloatBuffer" has no member "<name>".`.

### buffer[index]

Reads or writes the number at the position `index`. Index access skips the lookup of a method, so it is the fastest way to touch one number from Lua. A plain Lua list is still faster for numbers that only Lua uses. Writing a value that is not a number raises `bad argument #3 to 'newindex' (number expected, got <type>)`.

```lua
local collections = require('haylen.collections')

local positions = collections.newFloatBuffer(4)
positions[1], positions[2] = 120, 64
positions[3] = positions[1] + 16
print(positions[3]) -- 136.0
```

### #buffer

Returns the size of the buffer.

```lua
local collections = require('haylen.collections')

local buffer = collections.newFloatBuffer(300)
for index = 1, #buffer do
    buffer[index] = index / #buffer
end
```

### buffer:set(first, ...)

Copies numbers into the buffer from the position `first` on. The numbers are the arguments after `first`, or every value of a Lua list given as the second argument. One call that copies a whole list is the fast way to hand numbers that Lua computed to C++. A list entry that is not a number raises `Float buffer values are numbers, and entry <n> of the list is not.`.

```lua
local collections = require('haylen.collections')

local buffer = collections.newFloatBuffer(6)
buffer:set(1, 10, 20)
buffer:set(3, {30, 40, 50, 60})
print(buffer[4]) -- 40.0
```

### buffer:get(first, count)

Returns `count` numbers from the position `first` on, as separate results. The count defaults to `1`, and a negative count raises `bad argument #2 to 'get' (the count cannot be negative)`.

```lua
local collections = require('haylen.collections')

local buffer = collections.newFloatBuffer(6)
buffer:set(1, {1, 2, 3, 4, 5, 6})
local x, y, rotation = buffer:get(4, 3)
print(x, y, rotation) -- 4.0 5.0 6.0
```

### buffer:fill(value, first, count)

Sets every number of the buffer to `value`. With `first`, it sets only `count` numbers from the position `first` on, and `count` is then required.

```lua
local collections = require('haylen.collections')

local alphas = collections.newFloatBuffer(100)
alphas:fill(1)
alphas:fill(0, 51, 50)
print(alphas[50], alphas[51]) -- 1.0 0.0
```

## Errors

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | The options of `collections.newPool` hold a key that it does not take. |
| `Option tables only accept string keys.` | The options of `collections.newPool` hold a key that is not a string. |
| `The option "<key>" of "newPool" is invalid: <reason>.` | The option `capacity` or `prewarm` is not an integer of `0` or more. |
| `bad argument #1 to 'newPool' (create must be a function)` | The option `create` is missing or is not a function. |
| `bad argument #1 to 'newPool' (reset and release must be functions when present)` | The option `reset` or `release` is not a function. |
| `The "create" function of the pool returned "nil".` | The function `create` returned `nil` while the pool created an object. |
| `A ring buffer needs a capacity of at least one.` | The capacity of `collections.newRingBuffer` is `0`. |
| `bad argument #1 to 'newFloatBuffer' (the size cannot be negative)` | The size of `collections.newFloatBuffer` is negative. |
| `Float buffer positions <first> to <last> fall outside its size of <size>.` | A read or a write of a float buffer reaches outside it. |
| `Float buffer values are numbers, and entry <n> of the list is not.` | The list of `buffer:set` holds something other than a number. |
| `bad argument #<n> to '<function>' (<reason>)` | An argument has the wrong type or range, such as a negative count of `pool:prewarm` or `buffer:get`, a position of `ring:get` that is not an integer or a value of `buffer:set` that is not a number. |
| `The type "<type>" has no member "<name>".` | The code read a member that a `haylen.Pool`, `haylen.RingBuffer` or `haylen.FloatBuffer` does not have. |
| `The type "<type>" has no writable property "<name>".` | The code wrote a named property of a pool, a ring buffer or a float buffer, which have no writable properties. |
