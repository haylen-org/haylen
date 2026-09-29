# haylen.animation2d

`haylen.animation2d` plays frame animations on sprites. It cuts animations from sprite sheet grids or from lists of rectangles, reads TexturePacker and Aseprite atlases with their tags and nine-slices, and drives named animations with an animator that reports frame changes and finished animations. Use it for characters, effects and animated UI.

```lua
local animation2d = require('haylen.animation2d')
```

## Loading sprite atlases

A sprite atlas is a JSON file exported by TexturePacker or Aseprite together with the image it describes. Load it with the `atlas` asset type of [haylen.assets](assets.md), which has no file extension of its own, so the type is always given:

```lua
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
local smooth = assets.load('ui/hero.json', 'atlas', {filter = 'linear'})
```

The optional options table accepts the texture options `filter` (`'nearest'` or `'linear'`, default `'nearest'`) and `wrap` (`'clamp'`, `'repeat'` or `'mirror'`, default `'clamp'`). Any other key raises `Unknown key 'name' in texture options.` The atlas image shares the texture cache, so `atlas.texture` is the same texture that `assets.texture` returns for that image with the same options. `assets.loadAsync('ui/hero.json', 'atlas'):await()` loads it without blocking inside a coroutine.

The JSON file supports these fields:

| Field | Meaning |
| --- | --- |
| `frames` | Either an object from frame name to frame (hash layout) or a list of frames with a `filename` each (array layout). |
| `frames[].frame` | Required rectangle `{x, y, w, h}` of the frame in the image. |
| `frames[].rotated` | Must be absent or `false`. Rotated frames raise `Rotated atlas frames are not supported. Disable rotation in the packer.` |
| `frames[].spriteSourceSize`, `frames[].sourceSize` | Trim data. When both are present, the frame remembers where the trimmed rectangle sits inside the original frame, so pivots stay stable while the animation plays. |
| `frames[].duration` | Frame duration in milliseconds, as Aseprite writes it. Frames without it last 100 milliseconds. |
| `meta.image` | Required path of the image, relative to the JSON file. |
| `meta.frameTags` | Aseprite tags, each with `name`, `from` and `to` (frame positions counted from 0), `direction` and `repeat`. Every tag becomes an animation with the durations of its frames. |
| `meta.slices` | Aseprite slices. Slices whose first key has a `center` become nine-slices. Their `bounds` are relative to the frame at the position given by the key's `frame`, which defaults to 0. |

Tag directions `forward` and `reverse` loop, and `pingpong` and `pingpong_reverse` play back and forth. A `repeat` of `"1"` plays the tag once. An absent `repeat` or `"0"` loops forever, and larger counts raise `Aseprite tags that repeat a fixed number of times above one are not supported: name`. Other errors are `Unknown Aseprite tag direction: name` and `An Aseprite tag refers to frames that do not exist.` Both layouts keep their frames in file order, so a tag counts the same positions Aseprite does, even when frames named `hero 0` to `hero 11` would sort differently by name.

## Loop modes

Animations and option tables use these loop names:

| Name | Behavior |
| --- | --- |
| `'loop'` | Starts again after the last frame. |
| `'once'` | Stops on the last frame and reports the finish. |
| `'ping_pong'` | Plays forward and then backward, without showing the end frames twice in a row. |

## Functions

### animation2d.grid(texture, options)

Cuts an `Animation` from a grid of equally sized cells in `texture`. Cells are numbered from 1, left to right and top to bottom. `options`:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `frameWidth` | number | required | Cell width in pixels. |
| `frameHeight` | number | required | Cell height in pixels. |
| `frames` | table | every cell | Cell numbers to play, in order. Cells can repeat. |
| `fps` | number | `10` | Frames per second. |
| `loop` | string | `'loop'` | Loop mode. |
| `margin` | Vec2 | `{0, 0}` | Empty pixels around the whole grid, as `{x, y}`. |
| `spacing` | Vec2 | `{0, 0}` | Empty pixels between cells, as `{x, y}`. |

Errors: `fps must be positive`, `A grid animation needs a texture, a positive frame size and a positive frame rate.`, `The frame size does not fit the texture.` and `A grid animation refers to a cell outside the texture.`

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local idle = animation2d.grid(assets.texture('tiny_swords/units/blue/warrior/warrior_idle.png'), {frameWidth = 192, frameHeight = 192, fps = 10})
local slash = animation2d.grid(assets.texture('tiny_swords/units/blue/warrior/warrior_attack1.png'), {frameWidth = 192, frameHeight = 192, frames = {1, 2, 3, 4}, fps = 12, loop = 'once'})
print(idle.frameCount, slash.duration)
```

### animation2d.fromFrames(texture, frames, options)

Builds an `Animation` from a list of source rectangles in `texture`. `options` is optional and accepts `fps` (default 10) and `loop` (default `'loop'`). An empty list raises `expected at least one frame`.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local fire = animation2d.fromFrames(assets.texture('tiny_swords/effects/fire_01.png'), {
    {0, 0, 64, 64},
    {64, 0, 64, 64},
    {128, 0, 64, 64},
    {192, 0, 64, 64},
}, {fps = 8, loop = 'ping_pong'})
```

### animation2d.newAnimator()

Creates an empty `Animator`.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local sheet = assets.texture('tiny_swords/units/blue/warrior/warrior_run.png')
local warrior = graphics2d.newSprite(sheet, {x = 400, y = 300})
local animator = animation2d.newAnimator()
animator:add('run', animation2d.grid(sheet, {frameWidth = 192, frameHeight = 192, fps = 12}))
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

## Animation

An `Animation` is a value that holds a texture, a list of frames with their durations and a loop mode. `animator:add` stores a copy, so changing an animation afterwards does not change the animator.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `duration` | number | read | Length of one pass in seconds, the sum of the frame durations. |
| `cycleDuration` | number | read | Length of one full cycle in seconds. It equals `duration`, except on ping-pong animations, whose cycle also covers the way back without repeating the end frames. |
| `frameCount` | integer | read | Number of frames. |
| `texture` | Texture | read | Texture the frames come from. |
| `loop` | string | read and write | Loop mode. |

### anim:frame(index)

Returns the source `Rect` of frame `index`, counting from 1. An index outside the animation raises `frame index out of range`.

### anim:frameAt(seconds)

Returns the frame number, counting from 1, that the animation shows `seconds` after it starts, following its loop mode.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local run = animation2d.grid(assets.texture('tiny_swords/units/blue/warrior/warrior_run.png'), {frameWidth = 192, frameHeight = 192, fps = 10})
run.loop = 'ping_pong'
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
| `playing` | boolean | read | False after `stop` and after a one-shot animation finishes. |
| `finished` | boolean | read | True when the current animation plays once and has reached its end. |
| `queued` | integer | read | Number of animations waiting in the queue. |
| `speed` | number | read and write | Playback speed multiplier. Defaults to 1. |
| `pivotX`, `pivotY` | number | read and write | Pivot that `apply` gives sprites, as a fraction of the untrimmed frame. Default to 0.5. |
| `onFrame` | function or nil | read and write | Called when the frame changes. |
| `onFinish` | function or nil | read and write | Called when a one-shot animation ends. |

### animator:add(name, anim)

Stores a copy of `anim` under `name`, replacing an animation with the same name. An empty name or an animation without frames raises `An animation needs a name, a texture and at least one frame.`

### animator:has(name)

Returns true when an animation is stored under `name`.

### animator:animation(name)

Returns a copy of the `Animation` stored under `name`. An unknown name raises `Unknown animation: name`.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local animator = animation2d.newAnimator()
animator:add('idle', animation2d.grid(assets.texture('tiny_swords/units/blue/warrior/warrior_idle.png'), {frameWidth = 192, frameHeight = 192}))
print(animator:has('idle'), animator:has('fly'))
local idle = animator:animation('idle')
print(idle.frameCount, idle.duration)
```

### animator:play(name, restart)

Switches to the animation `name` and clears the queue. Playing the current animation again keeps its time and resumes it after `stop`, unless `restart` is true. An unknown name raises `Unknown animation: name`.

### animator:stop()

Pauses the current animation. `play` with the same name resumes it.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {{name = 'attack', type = 'button', bindings = {'key:space'}}, {name = 'freeze', type = 'button', bindings = {'key:f'}}}})

local warrior = 'tiny_swords/units/blue/warrior/'
local animator = animation2d.newAnimator()
animator:add('idle', animation2d.grid(assets.texture(warrior .. 'warrior_idle.png'), {frameWidth = 192, frameHeight = 192}))
animator:add('attack', animation2d.grid(assets.texture(warrior .. 'warrior_attack1.png'), {frameWidth = 192, frameHeight = 192, fps = 12, loop = 'once'}))
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

Adds `name` to the queue. The next queued animation starts from its first frame on the update that ends the current pass: when a one-shot animation finishes, or when a looping animation reaches the end of the cycle it is in, measured with `cycleDuration`. A looping animation that has already played for a while still finishes its current cycle before the queued one starts. With no current animation, or when the current one-shot animation has already finished, `queue` plays `name` at once. An unknown name raises `Unknown animation: name`.

### animator:clearQueue()

Removes every queued animation.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local warrior = 'tiny_swords/units/blue/warrior/'
local animator = animation2d.newAnimator()
animator:add('attack', animation2d.grid(assets.texture(warrior .. 'warrior_attack1.png'), {frameWidth = 192, frameHeight = 192, loop = 'once'}))
animator:add('guard', animation2d.grid(assets.texture(warrior .. 'warrior_guard.png'), {frameWidth = 192, frameHeight = 192, loop = 'once'}))
animator:add('idle', animation2d.grid(assets.texture(warrior .. 'warrior_idle.png'), {frameWidth = 192, frameHeight = 192}))

animator:play('attack')
animator:queue('guard')
animator:queue('idle')
print(animator.queued)
animator:clearQueue()
```

### animator:update(dt)

Advances the current animation by `dt` seconds times `speed`, calls `onFrame` and `onFinish`, and starts the next queued animation when the current pass ends. Errors raised by the callbacks propagate out of `update`.

### animator:apply(sprite)

Sets the texture, source, width, height and pivot of `sprite` from the current frame. The pivot follows `pivotX` and `pivotY` in the untrimmed frame, so trimmed atlas frames stay in place. Position, scale, rotation, color and flips stay untouched. Without a current animation it does nothing.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local sheet = assets.texture('tiny_swords/units/blue/warrior/warrior_idle.png')
local warrior = graphics2d.newSprite(sheet, {x = 400, y = 400})
local animator = animation2d.newAnimator()
animator:add('idle', animation2d.grid(sheet, {frameWidth = 192, frameHeight = 192}))
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

## Events

Callbacks are assigned to the animator's properties and run inside `animator:update`. Assign `nil` to remove one, and assigning anything other than a function or `nil` raises an error. A callback that captures its own animator does not keep it alive forever.

### animator.onFrame(name, frame)

Called with the animation name and the new frame number, counting from 1, each time the shown frame changes. It is not called for the first frame when an animation starts.

### animator.onFinish(name)

Called once with the animation name when a one-shot animation reaches its end. The callback may call `play` to switch animations.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local log = require('haylen.log')

local warrior = 'tiny_swords/units/blue/warrior/'
local animator = animation2d.newAnimator()
animator:add('idle', animation2d.grid(assets.texture(warrior .. 'warrior_idle.png'), {frameWidth = 192, frameHeight = 192}))
animator:add('attack', animation2d.grid(assets.texture(warrior .. 'warrior_attack1.png'), {frameWidth = 192, frameHeight = 192, fps = 12, loop = 'once'}))

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

Returns true when the atlas has a frame named `name`.

### atlas:hasAnimation(name)

Returns true when the atlas has an animation made from the tag `name`.

### atlas:hasSlice(name)

Returns true when the atlas has a nine-slice made from the slice `name`.

```lua
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
print(atlas:hasFrame('hero 0'), atlas:hasAnimation('walk'), atlas:hasSlice('panel'), atlas:hasSlice('missing'))
```

### atlas:frame(name)

Describes the frame `name` with a table of these fields. An unknown name raises `Unknown atlas frame: name`.

| Field | Type | Meaning |
| --- | --- | --- |
| `source` | Rect | The frame rectangle in the atlas image, the same as `atlas:source(name)`. |
| `offset` | Vec2 | Where the trimmed rectangle starts inside the original frame. It is zero for untrimmed frames. |
| `originalSize` | Vec2 | Size of the frame before trimming. |
| `duration` | number | Frame duration in seconds. |

```lua
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
local frame = atlas:frame('hero 0')
print(frame.source.width, frame.offset.x, frame.originalSize.x, frame.duration)
```

### atlas:animation(name)

Returns a copy of the `Animation` made from the tag `name`. An unknown name raises `Unknown atlas animation: name`.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
local animator = animation2d.newAnimator()
animator:add('walk', atlas:animation('walk'))
animator:play('walk')
```

### atlas:slice(name)

Returns the `NineSlice` made from the slice `name`, ready for `graphics2d.drawNineSlice`. An unknown name raises `Unknown atlas slice: name`.

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

Returns the source `Rect` of the frame `name` in the atlas image. An unknown name raises `Unknown atlas frame: name`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local atlas = assets.load('ui/hero.json', 'atlas')
local icon = graphics2d.newSprite(atlas.texture, {source = atlas:source('hero 1'), x = 64, y = 64})
```

### atlas:apply(sprite, name)

Shows the frame `name` on `sprite`, setting its texture, source, width, height and pivot. The pivot stays at the center of the untrimmed frame. An unknown name raises `Unknown atlas frame: name`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local atlas = assets.load('ui/hero.json', 'atlas')
local sprite = graphics2d.newSprite(atlas.texture, {x = 300, y = 200})
atlas:apply(sprite, 'hero 0')
```

### atlas:animationFromFrames(names, options)

Builds an `Animation` that shows the named frames in order at a fixed rate. `options` is optional and accepts `fps` (default 10) and `loop` (default `'loop'`). An empty list raises `An atlas animation needs frames and a positive frame rate.` and an unknown frame raises `Unknown atlas frame: name`.

```lua
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
local blink = atlas:animationFromFrames({'hero 1', 'hero 2', 'hero 1'}, {fps = 6, loop = 'once'})
print(blink.duration)
```
