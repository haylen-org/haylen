# haylen.animation2d

The module `haylen.animation2d` plays frame animations on sprites. It cuts animations from sprite sheet grids or from lists of frames, reads TexturePacker and Aseprite atlases with their tags, nine-slices and frame data, and drives named animations with an animator that reports frame changes, frame events and finished animations. Frames carry the data action games read at them: a pivot, named boxes such as hitboxes and hurtboxes, named points such as a hand or a muzzle, named events, and a length in ticks for animations that advance by fixed steps. Use it for characters, effects and animated UI.

```lua
local animation2d = require('haylen.animation2d')
```

## Loading sprite atlases

A sprite atlas is a JSON file exported by TexturePacker or Aseprite together with the image it describes. Load it with the `atlas` asset type of [`haylen.assets`](assets.md), which has no file extension of its own, so the type is always given:

```lua
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
local smooth = assets.load('ui/hero.json', 'atlas', {filter = 'linear'})
```

The optional options table accepts the texture options `filter` (`'nearest'` or `'linear'`, default `'nearest'`) and `wrap` (`'clamp'`, `'repeat'` or `'mirror'`, default `'clamp'`). Any other key raises `Unknown option "name".` The atlas image shares the texture cache, so `atlas.texture` is the same texture that `assets.texture` returns for that image with the same options. The call `assets.loadAsync('ui/hero.json', 'atlas'):await()` loads it without blocking inside a coroutine.

The JSON file supports these fields:

| Field | Meaning |
| --- | --- |
| `frames` | Either an object from frame name to frame (hash layout) or a list of frames with a `filename` each (array layout). |
| `frames[].frame` | Required rectangle `{x, y, w, h}` of the frame in the image. |
| `frames[].rotated` | Must be absent or `false`. Rotated frames raise `Rotated atlas frames are not supported. Disable rotation in the packer.` |
| `frames[].spriteSourceSize`, `frames[].sourceSize` | Trim data. When both are present, the frame remembers where the trimmed rectangle sits inside the original frame, so pivots stay stable while the animation plays. |
| `frames[].duration` | Frame duration in milliseconds, as Aseprite writes it. Frames without it last 100 milliseconds. |
| `frames[].data` | The user data of the frame, which names its [events](#frame-data) separated by commas, such as `"swing, whoosh"`. |
| `meta.image` | Required path of the image, relative to the JSON file. |
| `meta.frameTags` | Aseprite tags, each with `name`, `from` and `to` (frame positions counted from 0), `direction` and `repeat`. Every tag becomes an animation with the durations of its frames. |
| `meta.slices` | Aseprite slices. Slices whose first key has a `center` become nine-slices. Their `bounds` are relative to the frame at the position given by the key's `frame`, which defaults to 0. Every other slice becomes [frame data](#frame-data): a box named after the slice on the frames its keys cover, or a point when the user data `data` of the slice is `"point"`. |

Tag directions `forward` and `reverse` loop, and `pingpong` and `pingpong_reverse` play back and forth. A `repeat` of `"1"` plays the tag once. An absent `repeat` or `"0"` loops forever, and larger counts raise `The Aseprite tag "name" repeats more than once, which atlases do not support. Leave its repeat count empty to loop forever or set it to 1 to play once.` Other errors are `The direction of an Aseprite tag must be "forward", "reverse", "pingpong" or "pingpong_reverse", not "name".` and `An Aseprite tag refers to frames that do not exist.` Both layouts keep their frames in file order, so a tag counts the same positions Aseprite does, even when frames named `hero 0` to `hero 11` would sort differently by name.

## Frame data

Every frame of an animation holds, in pixels of its untrimmed frame:

- `pivot`, the point the frame stands on, which `animator:apply` gives the sprite in place of `pivotX` and `pivotY`.
- `boxes`, named rectangles such as hitboxes and hurtboxes, which [`animator:box`](#animatorboxname-sprite) places in the world.
- `points`, named points such as a hand or a muzzle, which [`animator:point`](#animatorpointname-sprite) places in the world.
- `events`, names that [`animator.onEvent`](#animatoroneventname-event-frame) receives when the animation enters the frame.
- `ticks`, the length of the frame in fixed steps, for animations that advance with [`animator:step`](#animatorstepticks) so their frames end exactly on steps. Every frame of an animation counts ticks or none does, which otherwise raises `Every frame of an animation counts ticks, or none does.`.

Atlases read frame data from their slices and frames. A slice key holds from its frame until the frame of the next key, the last key until the last frame, and a key with an empty `bounds` hides the slice from its frame on. A box slice marks the `bounds` of each key as its box, and the `pivot` of a key, relative to its bounds, becomes the pivot of the frames it covers. A point slice marks the `pivot` of each key, or the middle of its bounds. `animation2d.fromFrames` takes frame data as tables, `anim:addEvent` adds events in code, and `anim:frameData`, `atlas:frame` and `animator:box` read it back.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local atlas = assets.load('fighters/brawler.json', 'atlas')
local fighter = graphics2d.newSprite(atlas.texture, {x = 400, y = 500, flipHorizontal = true})
local animator = animation2d.newAnimator()
animator:add('punch', atlas:animation('punch'))
animator.onEvent = function(name, event, frame)
    if event == 'swing' then
        local hitbox = animator:box('hit', fighter)
        print(frame, hitbox and hitbox.width)
    end
end
animator:play('punch')

scene.push({
    update = function(self, dt)
        animator:update(dt)
        animator:apply(fighter)
    end,
})
```

## Loop modes

Animations and option tables use these loop names:

| Name | Behavior |
| --- | --- |
| `'loop'` | Starts again after the last frame. |
| `'once'` | Stops on the last frame and reports the finish. |
| `'pingPong'` | Plays forward and then backward, without showing the end frames twice in a row. |

## Functions

### animation2d.fromGrid(texture, options)

Cuts an `Animation` from a grid of equally sized cells in `texture`. Cells are numbered from 1, left to right and top to bottom. `options`:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `frameWidth` | number | required | Cell width in pixels. |
| `frameHeight` | number | required | Cell height in pixels. |
| `cells` | table | every cell | Cell numbers to play, in order. Cells can repeat. |
| `framesPerSecond` | number | `10` | Frames per second. |
| `loop` | string | `'loop'` | Loop mode. |
| `margin` | Vec2 | `{0, 0}` | Empty pixels around the whole grid, as `{x, y}`. |
| `spacing` | Vec2 | `{0, 0}` | Empty pixels between cells, as `{x, y}`. |
| `ticksPerFrame` | integer | `0` | When above 0, every frame lasts this many ticks and the animation advances with `animator:step`. |

Errors: `framesPerSecond must be positive`, `A grid animation needs a texture, a positive frame size and a positive frame rate.`, `The frame size does not fit the texture.` and `A grid animation refers to a cell outside the texture.`

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local idle = animation2d.fromGrid(assets.texture('sprites/knight/idle.png'), {frameWidth = 192, frameHeight = 192, framesPerSecond = 10})
local slash = animation2d.fromGrid(assets.texture('sprites/knight/attack.png'), {frameWidth = 192, frameHeight = 192, cells = {1, 2, 3, 4}, framesPerSecond = 12, loop = 'once'})
print(idle.frameCount, slash.duration)
```

### animation2d.fromFrames(texture, frames, options)

Builds an `Animation` from a list of frames in `texture`. A frame is a source rectangle, or a table with `source`, the rectangle, and its [frame data](#frame-data): `pivot` (`{x, y}`), `duration` in seconds, `ticks`, `boxes` (a list of `{name, x, y, width, height}`), `points` (a list of `{name, x, y}`) and `events` (a list of names). The argument `options` is optional and accepts `framesPerSecond` (default 10), which gives the frames without a `duration` theirs, and `loop` (default `'loop'`). An empty list raises `expected at least one frame`, and a key a frame table does not know raises `Unknown option "<key>".`

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local fire = animation2d.fromFrames(assets.texture('effects/fire.png'), {
    {0, 0, 64, 64},
    {64, 0, 64, 64},
    {128, 0, 64, 64},
    {192, 0, 64, 64},
}, {framesPerSecond = 8, loop = 'pingPong'})

local punch = animation2d.fromFrames(assets.texture('fighters/brawler.png'), {
    {source = {0, 0, 240, 240}, pivot = {117, 237}, ticks = 2},
    {source = {240, 0, 240, 240}, pivot = {111, 237}, ticks = 3, events = {'swing'}, boxes = {{name = 'hit', x = 150, y = 60, width = 100, height = 40}}},
    {source = {480, 0, 240, 240}, pivot = {111, 237}, ticks = 4},
}, {loop = 'once'})
print(fire.frameCount, punch.countsTicks)
```

### animation2d.newAnimator()

Creates an empty `Animator`.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local sheet = assets.texture('sprites/knight/run.png')
local warrior = graphics2d.newSprite(sheet, {x = 400, y = 300})
local animator = animation2d.newAnimator()
animator:add('run', animation2d.fromGrid(sheet, {frameWidth = 192, frameHeight = 192, framesPerSecond = 12}))
animator:play('run')

scene.push({
    update = function(self, dt)
        animator:update(dt)
        animator:apply(warrior)
    end,
    render = function(self)
        graphics2d.beginScreen()
        warrior:draw()
    end,
})
```

### animation2d.newAnimatedBatch(texture, animations)

Creates an [`AnimatedBatch`](#animatedbatch) of `texture` that plays the animations of the table `animations`, which maps names to `Animation` values of that texture. The animations all count seconds or all count ticks. An animation of another texture or without frames raises `An animation of an animated batch needs a name, at least one frame and the texture of the batch.`, and a mix of seconds and ticks raises `The animations of an animated batch all count seconds or all count ticks.`.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local atlas = assets.load('units/villager.json', 'atlas')
local crowd = animation2d.newAnimatedBatch(atlas.texture, {walk = atlas:animation('walk'), idle = atlas:animation('idle')})
for index = 1, 2000 do
    crowd:add({x = math.random(0, 4000), y = math.random(0, 3000), animation = index % 3 == 0 and 'idle' or 'walk', speed = 0.8 + math.random() * 0.4})
end
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        crowd:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera, {sort = 'y'})
        crowd:draw({layer = 1})
    end,
})
```

## Animation

An `Animation` is a value that holds a texture, a list of frames with their durations and a loop mode. The method `animator:add` stores a copy, so changing an animation afterwards does not change the animator.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `duration` | number | read | Length of one pass in seconds, the sum of the frame durations, or in ticks for an animation that counts them. |
| `cycleDuration` | number | read | Length of one full cycle in seconds or ticks. It equals `duration`, except on ping-pong animations, whose cycle also covers the way back without repeating the end frames. |
| `countsTicks` | boolean | read | Whether the frames count ticks, so the animation advances with `animator:step`. |
| `frameCount` | integer | read | Number of frames. |
| `texture` | Texture | read | Texture the frames come from. |
| `loop` | string | read and write | Loop mode. |

### anim:frame(index)

Returns the source `Rect` of frame `index`, counting from 1. An index outside the animation raises `frame index out of range`.

### anim:frameAt(time)

Returns the frame number, counting from 1, that the animation shows `time` seconds, or ticks, after it starts, following its loop mode.

### anim:frameData(index)

Returns the frame `index`, counting from 1, as a table with `source`, `offset`, `originalSize`, `duration`, `ticks`, `pivot` (a `Vec2`, absent without one), `boxes`, `points` and `events`, in the shape `fromFrames` takes them. An index outside the animation raises `frame index out of range`.

### anim:addEvent(index, name)

Adds the event `name` to the frame `index`, counting from 1, which an animator that plays the animation reports with `onEvent`. An animator keeps the copy it received, so add events before `animator:add`.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local walk = animation2d.fromGrid(assets.texture('sprites/knight/run.png'), {frameWidth = 192, frameHeight = 192, framesPerSecond = 10})
walk:addEvent(2, 'step')
walk:addEvent(6, 'step')
print(walk:frameData(2).events[1], walk:frameData(1).source.width)
```

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local run = animation2d.fromGrid(assets.texture('sprites/knight/run.png'), {frameWidth = 192, frameHeight = 192, framesPerSecond = 10})
run.loop = 'pingPong'
print(run.duration, run.cycleDuration)
run.loop = 'once'
local source = run:frame(2)
print(source.x, run:frameAt(0.25), run:frameAt(10))
```

## Animator

An `Animator` plays named animations and applies the current frame to sprites. It advances only when the app calls `animator:update`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `current` | string or nil | read | Name of the current animation, or `nil` before the first `play`. |
| `frame` | integer | read | Current frame number, counting from 1. |
| `time` | number | read | Seconds the current animation has played, scaled by `speed`. |
| `playing` | boolean | read | The value is `false` after `stop` and after a one-shot animation finishes. |
| `finished` | boolean | read | The value is `true` when the current animation plays once and has reached its end. |
| `queuedCount` | integer | read | Number of animations waiting in the queue. |
| `speed` | number | read and write | Playback speed multiplier. Defaults to 1. |
| `pivotX`, `pivotY` | number | read and write | Pivot that `apply` gives sprites, as a fraction of the untrimmed frame. Default to 0.5. |
| `onFrame` | function or nil | read and write | Called for every frame the animation enters. |
| `onEvent` | function or nil | read and write | Called for every event of a frame the animation enters. |
| `onFinish` | function or nil | read and write | Called when a one-shot animation ends. |

### animator:add(name, anim)

Stores a copy of `anim` under `name`, replacing an animation with the same name. An empty name or an animation without frames raises `An animation needs a name, a texture and at least one frame.`

### animator:has(name)

Returns `true` when an animation is stored under `name`.

### animator:animation(name)

Returns a copy of the `Animation` stored under `name`. An unknown name raises `The animation "name" does not exist in this animator.`

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local animator = animation2d.newAnimator()
animator:add('idle', animation2d.fromGrid(assets.texture('sprites/knight/idle.png'), {frameWidth = 192, frameHeight = 192}))
print(animator:has('idle'), animator:has('fly'))
local idle = animator:animation('idle')
print(idle.frameCount, idle.duration)
```

### animator:play(name, restart)

Switches to the animation `name` and clears the queue. Playing the current animation again keeps its time and resumes it after `stop`, unless `restart` is `true`. An unknown name raises `The animation "name" does not exist in this animator.`

### animator:stop()

Pauses the current animation. Calling `play` with the same name resumes it.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {{name = 'attack', type = 'button', bindings = {'key:space'}}, {name = 'freeze', type = 'button', bindings = {'key:f'}}}})

local knight = 'sprites/knight/'
local animator = animation2d.newAnimator()
animator:add('idle', animation2d.fromGrid(assets.texture(knight .. 'idle.png'), {frameWidth = 192, frameHeight = 192}))
animator:add('attack', animation2d.fromGrid(assets.texture(knight .. 'attack.png'), {frameWidth = 192, frameHeight = 192, framesPerSecond = 12, loop = 'once'}))
animator:play('idle')

scene.push({
    update = function(self, dt)
        if input.pressed('attack') then
            animator:play('attack', true)
        end
        if input.pressed('freeze') then
            animator:stop()
        end
        animator:update(dt)
    end,
})
```

### animator:queue(name)

Adds `name` to the queue. The next queued animation starts from its first frame on the update that ends the current pass: when a one-shot animation finishes, or when a looping animation reaches the end of the cycle it is in, measured with `cycleDuration`. A looping animation that has already played for a while still finishes its current cycle before the queued one starts. With no current animation, or when the current one-shot animation has already finished, `queue` plays `name` at once from its first frame, which replays a one-shot animation that just finished when `name` is that animation. An unknown name raises `The animation "name" does not exist in this animator.`

### animator:clearQueue()

Removes every queued animation.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local knight = 'sprites/knight/'
local animator = animation2d.newAnimator()
animator:add('attack', animation2d.fromGrid(assets.texture(knight .. 'attack.png'), {frameWidth = 192, frameHeight = 192, loop = 'once'}))
animator:add('guard', animation2d.fromGrid(assets.texture(knight .. 'guard.png'), {frameWidth = 192, frameHeight = 192, loop = 'once'}))
animator:add('idle', animation2d.fromGrid(assets.texture(knight .. 'idle.png'), {frameWidth = 192, frameHeight = 192}))

animator:play('attack')
animator:queue('guard')
animator:queue('idle')
print(animator.queuedCount)
animator:clearQueue()
```

### animator:update(dt)

Advances the current animation by `dt` seconds times `speed`, calls `onFrame` and `onEvent` for every frame it enters, in order, even the frames a long update passes, and `onFinish`, and starts the next queued animation when the current pass ends. The events of the first frame run with the first update after an animation starts. A looping animation that a single update moves more than two cycles reports only the frames of its last two cycles. A callback that plays another animation stops the reports of the old one. Errors raised by the callbacks propagate out of `update`, and an animation that counts ticks raises `The animation "<name>" counts ticks, so it advances with "step".`.

### animator:step(ticks)

Advances an animation that counts ticks by `ticks` whole steps, 1 by default, times `speed`, with the same callbacks as `update`, so a game on a fixed step plays its frames exactly. An animation that counts seconds raises `The animation "<name>" counts seconds, so it cannot step by ticks.`.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local scene = require('haylen.scene')

local sheet = assets.texture('fighters/brawler.png')
local animator = animation2d.newAnimator()
animator:add('jab', animation2d.fromGrid(sheet, {frameWidth = 240, frameHeight = 240, cells = {1, 2, 3}, ticksPerFrame = 2, loop = 'once'}))
animator:play('jab')

scene.push({
    fixedUpdate = function(self, step)
        animator:step(1)
    end,
})
```

### animator:apply(sprite)

Sets the texture, source, width, height and pivot of `sprite`, or of the sprite a [`Node`](nodes2d.md#node) shows, from the current frame. The pivot is the pivot of the frame, or `pivotX` and `pivotY` in the untrimmed frame, so trimmed atlas frames stay in place, and it mirrors where the sprite flips, so a flipped character stays on its feet. Position, scale, rotation, color and flips stay untouched. Without a current animation it does nothing.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local sheet = assets.texture('sprites/knight/idle.png')
local warrior = graphics2d.newSprite(sheet, {x = 400, y = 400})
local animator = animation2d.newAnimator()
animator:add('idle', animation2d.fromGrid(sheet, {frameWidth = 192, frameHeight = 192}))
animator:play('idle')
animator.pivotY = 0.75
animator.speed = 1.5

scene.push({
    update = function(self, dt)
        animator:update(dt)
        animator:apply(warrior)
    end,
    render = function(self)
        graphics2d.beginScreen()
        warrior:draw()
    end,
})
```

### animator:box(name, sprite)

Returns the box `name` of the current frame as a `Rect` in the coordinates of the sprite, a `Sprite` or a `Node` that shows one, placed, sized, scaled, turned and flipped as the sprite draws, or `nil` when the frame has no such box. A turned box returns the rectangle around its corners.

### animator:point(name, sprite)

Returns the `x` and `y` of the point `name` of the current frame placed through the sprite, or `nil` when the frame has no such point.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local atlas = assets.load('units/archer.json', 'atlas')
local archer = graphics2d.newSprite(atlas.texture, {x = 300, y = 400})
local animator = animation2d.newAnimator()
animator:add('shoot', atlas:animation('shoot'))
animator:play('shoot')

scene.push({
    update = function(self, dt)
        animator:update(dt)
        animator:apply(archer)
    end,
    render = function(self)
        graphics2d.beginScreen()
        archer:draw()
        local x, y = animator:point('muzzle', archer)
        if x then
            graphics2d.drawCircle(x, y, 4, '#FFFFE040')
        end
    end,
})
```

## Events

Callbacks are assigned to the animator's properties and run inside `animator:update`. Assign `nil` to remove one, and assigning anything other than a function or `nil` raises an error. A callback that captures its own animator does not keep it alive forever.

### animator.onFrame(name, frame)

Called with the animation name and the frame number, counting from 1, for every frame the animation enters, in order. It is not called for the first frame when an animation starts.

### animator.onEvent(name, event, frame)

Called with the animation name, the event name and the frame number, counting from 1, for every event of every frame the animation enters, in order, including the first frame on the first update after the animation starts.

### animator.onFinish(name)

Called once with the animation name when a one-shot animation reaches its end. The callback may call `play` to switch animations.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local log = require('haylen.log')

local knight = 'sprites/knight/'
local animator = animation2d.newAnimator()
animator:add('idle', animation2d.fromGrid(assets.texture(knight .. 'idle.png'), {frameWidth = 192, frameHeight = 192}))
animator:add('attack', animation2d.fromGrid(assets.texture(knight .. 'attack.png'), {frameWidth = 192, frameHeight = 192, framesPerSecond = 12, loop = 'once'}))

animator.onFrame = function(name, frame)
    if name == 'attack' and frame == 3 then
        log.info('The sword hits on frame 3.')
    end
end
animator.onFinish = function(name)
    animator:play('idle')
end
animator:play('attack')
```

## AnimatedBatch

An `AnimatedBatch` is a [sprite batch](graphics2d.md#spritebatch) whose sprites each play an animation of a shared set with their own time, speed and scale, advanced in C++, so a crowd of thousands of animated sprites costs one call to update and one to draw. Every update writes the source, the size and the pivot of the frame each sprite shows, mirrored where the sprite flips, and the batch draws in one call. Batches of 4096 sprites or more advance on the worker pool. Sprites are numbered from 1, and removing one renumbers the sprites after it.

### batch:add(sprite)

Adds a sprite and returns its number. The table takes the keys of a [sprite batch sprite](graphics2d.md#sprite-fields), whose `source`, `width`, `height`, `pivotX` and `pivotY` the animation sets, and `animation`, the name of the animation it plays from its start, the first by name without one, `speed` (default 1), `scaleX` and `scaleY` (default 1), which scale the frames, and `partColors`. An unknown animation raises `The animation "<name>" does not exist in this animated batch.`.

### batch:set(index, fields)

Changes the keys of `add` that `fields` gives. A new `animation` plays from its start unless the sprite already plays it.

### batch:get(index)

Returns the sprite as the table of `batch:get` of a sprite batch, with `animation`, `speed`, `scaleX`, `scaleY`, `frame`, counted from 1, and `time`.

### batch:play(index, name, restart)

Switches a sprite to the animation `name` from its start, unless it plays it already and `restart` is not `true`.

### batch:animation(index) and batch:frame(index)

Return the name of the animation of a sprite and the frame it shows, counted from 1.

### batch:remove(index), batch:clear(), batch:reserve(count) and batch:size()

Remove a sprite, remove every sprite, make room for a number of sprites and return the number of sprites. An index outside the batch raises `sprite index out of range`.

### batch:animationNames()

Returns the names of the animations of the batch.

### batch:writeFields(buffer, fields, first) and batch:readFields(buffer, fields, first)

Move the fields of many sprites from and into a float buffer in one call, as [`batch:writeFields`](graphics2d.md#batchwritefieldsbuffer-fields-first) of a sprite batch does.

### batch:update(dt) and batch:step(ticks)

Advance every sprite by `dt` seconds, or by `ticks` whole steps, 1 by default, for animations that count ticks, times its speed. A batch of the other unit raises `The animations of the animated batch count ticks, so it advances with "step".` or `The animations of the animated batch count seconds, so it cannot step by ticks.`.

### batch:finished()

Returns the numbers of the sprites whose one-shot animation ended in the last update.

### batch:on(event, listener, options)

Listens to `'finish'`, which calls the listener once per update with the list of the sprites whose one-shot animation ended in it, and returns a `Connection`. The table `options` takes `owner`, and without one the listener takes the default owner.

### batch:draw(order)

Draws every sprite in one call with the [draw order](graphics2d.md#draw-order) keys of `order`.

The property `texture` returns the texture of the batch.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local atlas = assets.load('units/soldier.json', 'atlas')
local army = animation2d.newAnimatedBatch(atlas.texture, {march = atlas:animation('march'), fall = atlas:animation('fall')})
for index = 1, 500 do
    army:add({x = (index % 25) * 40, y = (index // 25) * 40, animation = 'march'})
end

scene.push({
    enter = function(self)
        army:on('finish', function(fallen)
            for _, index in ipairs(fallen) do
                army:set(index, {color = '#80FFFFFF'})
            end
        end, {owner = self})
        army:play(7, 'fall')
    end,
    update = function(self, dt)
        army:update(dt)
    end,
    render = function(self)
        graphics2d.beginScreen()
        army:draw()
    end,
})
```

## SpriteAtlas

A `SpriteAtlas` holds the named frames, animations and nine-slices of an atlas loaded with `assets.load(path, 'atlas')`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | read | The atlas image. |

### atlas:frameNames()

Returns the list of frame names in file order.

### atlas:animationNames()

Returns the names of the animations made from Aseprite tags, sorted.

### atlas:sliceNames()

Returns the names of the nine-slices made from Aseprite slices, sorted.

```lua
local assets = require('haylen.assets')
local log = require('haylen.log')

local atlas = assets.load('ui/hero.json', 'atlas')
log.info('Frames: ' .. table.concat(atlas:frameNames(), ', '))
log.info('Animations: ' .. table.concat(atlas:animationNames(), ', '))
log.info('Slices: ' .. table.concat(atlas:sliceNames(), ', '))
```

### atlas:hasFrame(name)

Returns `true` when the atlas has a frame named `name`.

### atlas:hasAnimation(name)

Returns `true` when the atlas has an animation made from the tag `name`.

### atlas:hasSlice(name)

Returns `true` when the atlas has a nine-slice made from the slice `name`.

```lua
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
print(atlas:hasFrame('hero 0'), atlas:hasAnimation('walk'), atlas:hasSlice('panel'), atlas:hasSlice('missing'))
```

### atlas:frame(name)

Describes the frame `name` with a table of these fields. An unknown name raises `The frame "name" does not exist in this atlas.`

| Field | Type | Meaning |
| --- | --- | --- |
| `source` | Rect | The frame rectangle in the atlas image, the same as `atlas:source(name)`. |
| `offset` | Vec2 | Where the trimmed rectangle starts inside the original frame. It is zero for untrimmed frames. |
| `originalSize` | Vec2 | Size of the frame before trimming. |
| `duration` | number | Frame duration in seconds. |
| `ticks` | integer | 0, since atlas frames count seconds. |
| `pivot` | Vec2 or nil | The pivot that a box slice gives the frame. |
| `boxes` | table | The boxes of the frame, each `{name, x, y, width, height}`. |
| `points` | table | The points of the frame, each `{name, x, y}`. |
| `events` | table | The events of the frame. |

```lua
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
local frame = atlas:frame('hero 0')
print(frame.source.width, frame.offset.x, frame.originalSize.x, frame.duration, #frame.boxes, #frame.events)
```

### atlas:animation(name)

Returns a copy of the `Animation` made from the tag `name`. An unknown name raises `The animation "name" does not exist in this atlas.`

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
local animator = animation2d.newAnimator()
animator:add('walk', atlas:animation('walk'))
animator:play('walk')
```

### atlas:slice(name)

Returns the `NineSlice` made from the slice `name`, ready for `graphics2d.drawNineSlice`. An unknown name raises `The slice "name" does not exist in this atlas.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local atlas = assets.load('ui/hero.json', 'atlas')
local panel = atlas:slice('panel')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawNineSlice(panel, {100, 100, 400, 240})
    end,
})
```

### atlas:source(name)

Returns the source `Rect` of the frame `name` in the atlas image. An unknown name raises `The frame "name" does not exist in this atlas.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local atlas = assets.load('ui/hero.json', 'atlas')
local icon = graphics2d.newSprite(atlas.texture, {source = atlas:source('hero 1'), x = 64, y = 64})
```

### atlas:apply(sprite, name)

Shows the frame `name` on `sprite`, setting its texture, source, width, height and pivot. The pivot stays at the center of the untrimmed frame. An unknown name raises `The frame "name" does not exist in this atlas.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local atlas = assets.load('ui/hero.json', 'atlas')
local sprite = graphics2d.newSprite(atlas.texture, {x = 300, y = 200})
atlas:apply(sprite, 'hero 0')
```

### atlas:animationFromFrames(names, options)

Builds an `Animation` that shows the named frames in order at a fixed rate. The argument `options` is optional and accepts `framesPerSecond` (default 10) and `loop` (default `'loop'`). An empty list raises `An atlas animation needs frames and a positive frame rate.` and an unknown frame raises `The frame "name" does not exist in this atlas.`

```lua
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
local blink = atlas:animationFromFrames({'hero 1', 'hero 2', 'hero 1'}, {framesPerSecond = 6, loop = 'once'})
print(blink.duration)
```

## Performance

An animated batch advances its sprites in C++, so a crowd costs one call from Lua per frame instead of an `update` and an `apply` per unit. The command `haylen bench --suite animation` measures it on the headless host, on an Apple M5 Pro in Release with 15 workers:

| Scene | Average ms per frame |
| --- | --- |
| Animated batch of 100,000 sprites, one thread | 0.930 |
| Animated batch of 100,000 sprites, job system | 0.191 |
| 2,000 units with one animator each, updated and applied from Lua | 0.232 |
| 2,000 units of one animated batch, updated from Lua | 0.017 |
