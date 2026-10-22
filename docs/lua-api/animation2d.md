# 📖 `haylen.animation2d`

The module `haylen.animation2d` plays frame animations on sprites, crowds and state graphs, skeletal and cutout animation on skeletons, and procedural motion on chains, and it packs atlases at run time. The [animation guide](../2d/animation.md) explains which player suits which need and how frame data, state graphs, skeletons and chains work.

```lua
local animation2d = require('haylen.animation2d')
```

## Functions

### animation2d.fromGrid(texture, options)

Cuts an [`Animation`](#animation) from a grid of cells of the same size in `texture`. Cells count from 1, left to right and top to bottom. The table `options` takes:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `frameWidth` | number | required | The cell width in pixels. |
| `frameHeight` | number | required | The cell height in pixels. |
| `cells` | table | every cell | The cell numbers to play, in order. Cells can repeat. |
| `framesPerSecond` | number | `10` | Frames per second. |
| `loop` | string | `'loop'` | The [loop mode](#loop-modes). |
| `margin` | Vec2 | `{0, 0}` | Empty pixels around the whole grid, as `{x, y}`. |
| `spacing` | Vec2 | `{0, 0}` | Empty pixels between cells, as `{x, y}`. |
| `ticksPerFrame` | integer | `0` | Above 0, every frame lasts this many ticks and the animation advances with `animator:step`. |

These errors can come up: an argument error that says `framesPerSecond must be positive`, `A grid animation needs a texture, a positive frame size and a positive frame rate.`, `The frame size does not fit the texture.` and `A grid animation refers to a cell outside the texture.` Any other key raises `Unknown option "<key>".`

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local idle = animation2d.fromGrid(assets.texture('sprites/knight/idle.png'), {frameWidth = 192, frameHeight = 192, framesPerSecond = 10})
local slash = animation2d.fromGrid(assets.texture('sprites/knight/attack.png'), {frameWidth = 192, frameHeight = 192, cells = {1, 2, 3, 4}, framesPerSecond = 12, loop = 'once'})
print(idle.frameCount, slash.duration)
```

### animation2d.fromFrames(texture, frames, options)

Builds an [`Animation`](#animation) from a list of frames in `texture`. A frame is a source rectangle, or a table with `source`, the rectangle, and its [frame data](#frame-data): `pivot` (`{x, y}`), `duration` in seconds, `ticks`, `boxes` (a list of `{name, x, y, width, height}`), `points` (a list of `{name, x, y}`) and `events` (a list of names). The optional table `options` takes `framesPerSecond` (default `10`), which gives the frames without a `duration` theirs, and `loop` (default `'loop'`).

An empty list raises an argument error that says `expected at least one frame`. A key that a frame table does not know raises `Unknown option "<key>".` A frame that lasts no time raises `A frame lasts a positive number of seconds.`, and a mix of frames with and without ticks raises `Every frame of an animation counts ticks, or none does.`

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

Creates an empty [`Animator`](#animator).

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

Creates an [`AnimatedBatch`](#animatedbatch) of `texture` that plays the animations of the table `animations`, which maps names to `Animation` values of that texture. The animations all count seconds or all count ticks. An animation of another texture or without frames raises `An animation of an animated batch needs a name, at least one frame and the texture of the batch.`, and a mix of seconds and ticks raises `The animations of an animated batch all count seconds or all count ticks.`

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

### animation2d.newStateGraph(description)

Creates a [`StateGraph`](#stategraph) from a [description](#state-graph-descriptions) table, or from the package path of a JSON file with the same keys, and checks it once.

```lua
local animation2d = require('haylen.animation2d')

local graph = animation2d.newStateGraph({
    parameters = {speed = 0, jump = 'trigger'},
    states = {idle = {animation = 'idle'}, run = {animation = 'run'}, jump = {animation = 'jump'}},
    initial = 'idle',
    transitions = {
        {from = 'idle', to = 'run', when = 'speed > 0.1'},
        {from = 'run', to = 'idle', when = 'speed <= 0.1'},
        {to = 'jump', when = 'jump'},
        {from = 'jump', to = 'idle', atEnd = true, fade = 0.1},
    },
})
local villager = animation2d.newStateGraph('animation/villager.graph.json')
print(#graph.states, graph.parameters.jump)
```

### animation2d.newSkeletonData(description, atlas)

Creates [`SkeletonData`](#skeletondata) from a description table in the [`.skeleton` format](#skeleton-files), without its `atlas` key, and a `SpriteAtlas`. The function `assets.load(path)` loads `.skeleton` files with their atlas on a worker thread instead.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local atlas = assets.load('animation/puppet.json', 'atlas')
local data = animation2d.newSkeletonData({
    bones = {{name = 'root'}, {name = 'body', parent = 'root', y = -40}},
    slots = {{name = 'body', bone = 'body', attachment = 'body'}},
    skins = {default = {body = {body = {}}}},
}, atlas)
print(data.bones[2])
```

### animation2d.newSkeleton(data, options)

Creates a [`Skeleton`](#skeleton) of `data` in its setup pose. The optional table `options` takes `skin`, `x`, `y`, `rotation`, `scaleX`, `scaleY` and `color`, and any other key raises `Unknown option "<key>".`

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local knight = animation2d.newSkeleton(assets.load('characters/knight.skeleton'), {skin = 'blue', x = 0, y = 200})
knight:play('walk', {track = 1})

scene.push({
    enter = function(self)
        knight:on('event', function(event) if event.name == 'step' then print('Footstep.') end end, {owner = self})
    end,
    update = function(self, dt)
        if input.pressed('attack') then
            knight:crossFade('slash', 0.1, {track = 2, loop = false})
        end
        knight:setIkTarget('aim', camera:screenToWorld(input.mousePosition()))
        knight:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera, {sort = 'y'})
        knight:draw({layer = 2})
    end,
})
```

### animation2d.newAtlasBuilder()

Returns an empty [`AtlasBuilder`](#atlasbuilder).

```lua
local animation2d = require('haylen.animation2d')

local builder = animation2d.newAtlasBuilder()
print(builder.count)
```

### animation2d.newChain(options)

Returns a [`Chain`](#chain) that lies straight from its root along `angle`. The table `options` takes these keys, and any other key raises `Unknown option "<key>".`:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | The root, the first point. |
| `lengths` | list of numbers | | The length of each segment, from the root on. |
| `segments`, `length` | integer, number | | A number of segments of the same length, in place of `lengths`. |
| `angle` | number | `0` | The direction in radians that the chain lies along from the root when it is made. |
| `maxBend` | number | `math.pi` | The largest bend in radians between two segments in a row, from 0, which keeps the chain straight, to `math.pi`, which lets it fold. |

Both `lengths` and `segments`, or neither, raise `A chain takes the lengths of its segments either as "lengths" or as "segments" of the same "length".`. Fewer than one segment raises `A chain needs at least one segment in "segments".`, and a length that is not a number greater than zero raises `Every segment of a chain needs a finite length greater than zero.`

```lua
local animation2d = require('haylen.animation2d')

local leg = animation2d.newChain({x = 400, y = 200, lengths = {90, 90}})
local snake = animation2d.newChain({x = 200, y = 300, segments = 24, length = 14, angle = math.pi, maxBend = 0.5})
print(leg.count, leg.length, snake.count)
```

## Loop modes

Animations and option tables use these loop names:

| Name | Behavior |
| --- | --- |
| `'loop'` | Starts again after the last frame. |
| `'once'` | Stops on the last frame and reports the finish. |
| `'pingPong'` | Plays forward and then backward, without showing the end frames twice in a row. |

## Frame data

Every frame of an animation holds these fields, in pixels of its untrimmed frame:

| Field | Meaning |
| --- | --- |
| `pivot` | The point the frame stands on, which `animator:apply` gives the sprite in place of `pivotX` and `pivotY`. |
| `boxes` | Named rectangles, such as hitboxes and hurtboxes, which [`animator:box`](#animatorboxname-sprite) places in the world. |
| `points` | Named points, such as a hand or a muzzle, which [`animator:point`](#animatorpointname-sprite) places in the world. |
| `events` | Names that [`animator.onEvent`](#animatoroneventname-event-frame) receives when the animation enters the frame. |
| `ticks` | The length of the frame in fixed steps, for animations that advance with [`animator:step`](#animatorstepticks). Every frame of an animation counts ticks or none does, which otherwise raises `Every frame of an animation counts ticks, or none does.` |

The function `animation2d.fromFrames` takes frame data as tables, `anim:addEvent` adds events in code, and `anim:frameData`, `atlas:frame` and `animator:box` read it back. Atlases read frame data from their slices, as [atlas files](#atlas-files) describe.

## Atlas files

A sprite atlas is a JSON file in the hash or array layout, with frame tags and slices, together with the image it describes. Load it with the `atlas` asset type of [`haylen.assets`](assets.md). The type has no file extension of its own, so always name it. The result is a [`SpriteAtlas`](#spriteatlas).

The optional options table takes the texture options `filter` (`'nearest'` or `'linear'`, default `'nearest'`), `wrap` (`'clamp'`, `'repeat'` or `'mirror'`, default `'clamp'`) and `mipmaps` (default `false`). Mipmaps keep smaller copies of the image for frames drawn far smaller than they are painted, such as a crowd. Any other key raises `Unknown option "<key>".` The atlas image shares the texture cache, so `atlas.texture` is the same texture that `assets.texture` returns for that image with the same options. The call `assets.loadAsync('ui/hero.json', 'atlas'):await()` loads it without blocking, inside a coroutine.

```lua
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
local smooth = assets.load('ui/hero.json', 'atlas', {filter = 'linear'})
```

The JSON file supports these fields:

| Field | Meaning |
| --- | --- |
| `frames` | Either an object from frame name to frame (hash layout) or a list of frames with a `filename` each (array layout). |
| `frames[].frame` | The required rectangle `{x, y, w, h}` of the frame in the image, which has an area and lies inside the image. Any other rectangle raises `The frame "<name>" of the atlas must lie inside its image of <width> by <height> pixels, with an area.`, since it would draw pixels the image does not hold. |
| `frames[].rotated` | Must be absent or `false`. Rotated frames raise `Rotated atlas frames are not supported. Disable rotation in the packer.` |
| `frames[].spriteSourceSize`, `frames[].sourceSize` | Trim data. When both are present, the frame remembers where the trimmed rectangle sits inside the original frame, so pivots stay still while the animation plays. |
| `frames[].duration` | The frame duration in milliseconds. Frames without it last 100 milliseconds. |
| `frames[].data` | The user data of the frame, which names its [events](#frame-data) separated by commas, such as `"swing, whoosh"`. |
| `meta.image` | The required path of the image, relative to the JSON file. |
| `meta.frameTags` | Frame tags, each with `name`, `from` and `to` (frame positions counted from 0), `direction` and `repeat`. Every tag becomes an animation with the durations of its frames. |
| `meta.slices` | Slices. A slice whose first key has a `center` becomes a nine-slice. Its `bounds` are relative to the frame at the position the `frame` of the key gives, 0 by default. Every other slice becomes [frame data](#frame-data): a box named after the slice on the frames its keys cover, or a point when the user data `data` of the slice is `"point"`. |

The tag directions `forward` and `reverse` loop, and `pingpong` and `pingpong_reverse` play back and forth. A `repeat` of `"1"` plays the tag once. An absent `repeat` or `"0"` loops forever. Both layouts keep their frames in file order, so a tag counts the positions of the file, even when frames named `hero 0` to `hero 11` would sort in another order by name.

A slice key holds from its frame until the frame of the next key, and the last key until the last frame. A key with an empty `bounds` hides the slice from its frame on. A box slice marks the `bounds` of each key as its box, and the `pivot` of a key, relative to its bounds, becomes the pivot of the frames it covers. A point slice marks the `pivot` of each key, or the middle of its bounds.

Loading an atlas can also raise these errors:

- `The frame tag "<name>" repeats more than once, which atlases do not support. Leave its repeat count empty to loop forever or set it to 1 to play once.`
- `The direction of a frame tag must be "forward", "reverse", "pingpong" or "pingpong_reverse", not "<name>".`
- `A frame tag refers to frames that do not exist.`
- `The atlas has the frame "<name>" twice.` and `The atlas has the animation "<name>" twice.`
- `The slice "<name>" has a key for a frame that does not exist.`
- `The slice "<name>" reaches past the pixels its frame keeps in the atlas. Keep the slice inside the opaque pixels of the frame or export the atlas without trimming.`

## State graph descriptions

A state graph description is a table, or a JSON file with the same keys, that [`animation2d.newStateGraph`](#animation2dnewstategraphdescription) reads. The [state graphs section of the guide](../2d/animation.md#state-graphs) explains how transitions fire and how directions turn.

| Key | Meaning |
| --- | --- |
| `parameters` | Parameters by name with their starting value: a number, a boolean, `'trigger'`, or a vector such as `{0, 1}`. |
| `states` | States by name. Each has `animation`, the animation it plays, or `directions`, a table from `right`, `down`, `left` and `up`, plus `downRight`, `downLeft`, `upLeft` and `upRight` for eight directions, to the animation of each, with `by`, the vector parameter that picks one. |
| `initial` | The state a player starts in, or the first state by name without one. |
| `transitions` | A list of transitions, tried in order. Each has `to`, the state it enters, `from`, a state or `'any'` (the default) for every other state, `when`, a condition, `exitTime`, the share of one pass of the animation of the state that must have played (`0` by default), `atEnd = true`, which is an exit time of 1, and `fade`, the seconds a skeleton crossfades. |

A condition compares parameters and numbers with `==`, `~=` (or `!=`), `<`, `<=`, `>` and `>=`. It reads a boolean or a trigger alone, reads the parts of a vector as `name.x` and `name.y`, takes the words `true` and `false`, and joins terms with `and`, `or`, `not` and parentheses, such as `'speed > 0.1 and not crouching'`.

A description with a mistake raises one of these errors, where `<n>` is the number of a transition:

- `A state graph description must be an object with "parameters", "states" and "transitions".` and `Unknown option "<key>".`
- `The parameter "<name>" must start as a number, a boolean, "trigger" or a vector of two numbers.`
- `A state graph needs at least one state.`
- `The state "<name>" must be an object with an "animation" or "directions".`, `The state "<name>" has the unknown key "<key>".` and `The state "<name>" plays an "animation" or a set of "directions", one of the two.`
- `The directions of the state "<name>" need "by", the name of a vector parameter.` and `The directions of the state "<name>" name the four straight directions, or all eight.`
- `The state "<name>" names the direction "<direction>". Directions are "right", "down", "left" and "up", with "downRight", "downLeft", "upLeft" and "upRight" for eight, each with the name of an animation.`
- `The initial state "<name>" does not exist.`
- `The transition <n> must be an object with "from" and "to".` and `The transition <n> has the unknown key "<key>".`
- `The transition <n> starts from the unknown state "<name>".` and `The transition <n> goes to the unknown state "<name>".`
- `The transition <n> needs an exit time and a fade of at least 0.`
- `The condition "<text>" of the transition <n> <problem>.`, where `<problem>` is `ends too early`, `misses a closing parenthesis`, `has an unexpected "<text>"`, `has a number it cannot read, "<text>"`, `names the unknown parameter "<name>"`, `reads the vector "<name>" without ".x" or ".y"` or `reads a component of "<name>", which is no vector`.

```json
{
    "parameters": {"moving": false},
    "states": {"idle": {"animation": "idle"}, "walk": {"animation": "walk"}},
    "transitions": [
        {"from": "idle", "to": "walk", "when": "moving"},
        {"from": "walk", "to": "idle", "when": "not moving", "exitTime": 0.5}
    ]
}
```

## Skeleton files

A `.skeleton` file is JSON in pixels and degrees, with y growing downward. The [skeletons section of the guide](../2d/animation.md#skeletons) explains bones, slots, skins, tracks and constraints. Every key that is not listed raises an error that names it.

| Key | Meaning |
| --- | --- |
| `atlas` | The path of the [sprite atlas](#atlas-files) whose frames the attachments show, relative to the file. |
| `bones` | A list of bones, parents before their children. Each has `name`, `parent` (every bone but the first, the root), `x`, `y`, `rotation`, `scaleX`, `scaleY` and `length`, which inverse kinematics reads. |
| `slots` | A list of slots in draw order. Each has `name`, `bone`, `attachment` (the name it starts with), `color` and `blend`, one of the blend modes of [draws](graphics2d.md#draw-order) up to `opaque`, since the modes that mix with what lies below blend groups alone. |
| `skins` | Skins by name, each mapping slot names to attachments by name. The skin `default` always exists, and another skin falls back to it. An attachment has `type` (`'region'`, the default, `'mesh'`, `'point'` or `'boundingBox'`), `frame` (the frame of the atlas, the name of the attachment by default), `x`, `y`, `rotation`, `scaleX`, `scaleY`, `width`, `height` and `color`. A mesh has `uvs` from 0 to 1 over its frame, `triangles`, and `vertices` in the space of its bone or `weights`, a list for each vertex of `{bone, x, y, weight}`. |
| `ik` | Inverse kinematics constraints, each with `name`, `bones` (one bone, or a bone and its child), `target`, `bendPositive` and `mix`. |
| `transform` | Transform constraints, each with `name`, `bones`, `target`, `rotateMix`, `translateMix`, `rotation`, `x` and `y`. |
| `animations` | Animations by name. Each has `bones` (`rotate` keys with `value`, and `translate` and `scale` keys with `x` and `y`), `slots` (`attachment` keys with `name`, an empty name to hide, and `color` keys with `color`), `deform` (by slot and mesh, keys with `offsets` for each vertex or weight), `ik` (keys with `mix` and `bendPositive`), `drawOrder` (keys with the `order` of every slot), `events` (each with `time`, `name`, `text` and `number`) and an optional `duration`, which defaults to the time of its last key. |

Every key of a timeline has `time` and a `curve` toward the next key: `'linear'`, the default, `'stepped'`, the name of an [easing](tween.md#easing), or four numbers of a cubic Bezier curve. Events at the time 0 of a looping animation fire each time it wraps.

Errors name the part at fault, such as these:

- `A skeleton file needs "atlas", the path of the atlas of its attachments.` and `A skeleton file has the unknown key "<key>".`
- `A skeleton needs at least one bone.` and `The bone "<name>" needs a parent, since only the first bone is the root.`
- `The bone "arm" names the unknown bone "torso".`
- `Every bone of a skeleton needs a name of its own, and "<name>" is empty or taken.`, and the same for slots.
- `The slot "<name>" has the unknown blend mode "<blend>".` and `The slot "<name>" has the blend mode "<blend>", which mixes with what lies below and blends groups alone.`
- `The slot "<name>" starts with the attachment "<attachment>", which no skin has.`
- `The frame "cape" of the attachment "cape" of the slot "cape" does not exist in the atlas of the skeleton.`
- `The keys of the rotate of the bone "root" in the animation "walk" must follow each other in time.`
- `The bone "<name>" in the animation "<animation>" has the unknown timeline "<timeline>". Bones take "rotate", "translate" and "scale".`, and the same for slots, which take `"attachment"` and `"color"`.
- A key of the wrong type raises an error such as `The key "<key>" of the bone "<name>" must be a number.`

```json
{
    "atlas": "puppet.json",
    "bones": [
        {"name": "root"},
        {"name": "hip", "parent": "root", "y": -74},
        {"name": "thigh", "parent": "hip", "rotation": 90, "length": 36}
    ],
    "slots": [{"name": "thigh", "bone": "thigh", "attachment": "thigh"}],
    "skins": {"default": {"thigh": {"thigh": {"x": 18, "rotation": 90}}}},
    "animations": {
        "walk": {
            "bones": {"thigh": {"rotate": [{"time": 0, "value": 25, "curve": "sineInOut"}, {"time": 0.4, "value": -25, "curve": "sineInOut"}, {"time": 0.8, "value": 25}]}},
            "events": [{"time": 0.4, "name": "step"}]
        }
    }
}
```

## Animation

An `Animation` is a value that holds a texture, a list of frames with their durations and a loop mode. It comes from `animation2d.fromGrid`, `animation2d.fromFrames`, `atlas:animation` and `atlas:animationFromFrames`. The method `animator:add` stores a copy, so changing an animation afterwards does not change the animator.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `duration` | number | read | The length of one pass in seconds, the sum of the frame durations, or in ticks for an animation that counts them. |
| `cycleDuration` | number | read | The length of one full cycle in seconds or ticks. It equals `duration`, except on ping-pong animations, whose cycle also covers the way back without repeating the end frames. |
| `countsTicks` | boolean | read | Whether the frames count ticks, so the animation advances with `animator:step`. |
| `frameCount` | integer | read | The number of frames. |
| `texture` | Texture | read | The texture the frames come from. |
| `loop` | string | read and write | The [loop mode](#loop-modes). |

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local run = animation2d.fromGrid(assets.texture('sprites/knight/run.png'), {frameWidth = 192, frameHeight = 192, framesPerSecond = 10})
run.loop = 'pingPong'
print(run.duration, run.cycleDuration, run.countsTicks, run.frameCount, run.texture)
```

### anim:frame(index)

Returns the source `Rect` of the frame `index`, counted from 1. An index outside the animation raises an argument error that says `frame index out of range`.

### anim:frameAt(time)

Returns the frame number, counted from 1, that the animation shows `time` seconds, or ticks, after it starts, following its loop mode.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local run = animation2d.fromGrid(assets.texture('sprites/knight/run.png'), {frameWidth = 192, frameHeight = 192, framesPerSecond = 10, loop = 'once'})
local source = run:frame(2)
print(source.x, run:frameAt(0.25), run:frameAt(10))
```

### anim:frameData(index)

Returns the frame `index`, counted from 1, as a table with `source`, `offset`, `originalSize`, `duration`, `ticks`, `pivot` (a `Vec2`, absent without one), `boxes`, `points` and `events`, in the shape `fromFrames` takes them. An index outside the animation raises an argument error that says `frame index out of range`.

### anim:addEvent(index, name)

Adds the event `name` to the frame `index`, counted from 1, which an animator that plays the animation reports with `onEvent`. An animator keeps the copy it received, so add events before `animator:add`. An index outside the animation raises an argument error that says `frame index out of range`.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local walk = animation2d.fromGrid(assets.texture('sprites/knight/run.png'), {frameWidth = 192, frameHeight = 192, framesPerSecond = 10})
walk:addEvent(2, 'step')
walk:addEvent(6, 'step')
print(walk:frameData(2).events[1], walk:frameData(1).source.width)
```

## Animator

An `Animator` plays named animations and applies the current frame to sprites. It advances only when the app calls `animator:update` or `animator:step`. Create one with `animation2d.newAnimator`.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `current` | string or nil | read | The name of the current animation, or `nil` before the first `play`. |
| `frame` | integer | read | The current frame number, counted from 1. |
| `time` | number | read | The seconds the current animation has played, scaled by `speed`. |
| `playing` | boolean | read | The value is `false` after `stop` and after a one-shot animation finishes. |
| `finished` | boolean | read | The value is `true` when the current animation plays once and has reached its end. |
| `queuedCount` | integer | read | The number of animations waiting in the queue. |
| `speed` | number | read and write | The playback speed multiplier, `1` by default. |
| `pivotX`, `pivotY` | number | read and write | The pivot that `apply` gives sprites for frames without a pivot, as a fraction of the untrimmed frame, `0.5` by default. |
| `onFrame` | function or nil | read and write | Called for every frame the animation enters, as [`animator.onFrame`](#animatoronframename-frame) describes. |
| `onEvent` | function or nil | read and write | Called for every event of a frame the animation enters, as [`animator.onEvent`](#animatoroneventname-event-frame) describes. |
| `onComplete` | function or nil | read and write | Called when a one-shot animation ends, as [`animator.onComplete`](#animatoroncompletename) describes. |

### animator:add(name, anim)

Stores a copy of `anim` under `name`, and replaces an animation with the same name. An empty name or an animation without frames raises `An animation needs a name, a texture and at least one frame.`

### animator:has(name)

Returns `true` when an animation is stored under `name`.

### animator:animation(name)

Returns a copy of the `Animation` stored under `name`. An unknown name raises `The animation "<name>" does not exist in this animator.`

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

Switches to the animation `name` and clears the queue. Playing the current animation again keeps its time, and resumes it after `stop`, unless `restart` is `true`. An unknown name raises `The animation "<name>" does not exist in this animator.`

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
        print(animator.current, animator.frame, animator.time, animator.playing, animator.finished)
    end,
})
```

### animator:seek(time)

Moves the current animation to `time` and shows the frame there. The time is in seconds, or in ticks for an animation that counts ticks. It calls none of `onFrame`, `onEvent` and `onComplete`. Use it when a character switches to another animation and keeps its step, such as when it turns around. Before the first `play`, it does nothing. A `time` that is not a number raises an argument error.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local knight = 'sprites/knight/'
local animator = animation2d.newAnimator()
animator:add('walkLeft', animation2d.fromGrid(assets.texture(knight .. 'walk-left.png'), {frameWidth = 192, frameHeight = 192}))
animator:add('walkRight', animation2d.fromGrid(assets.texture(knight .. 'walk-right.png'), {frameWidth = 192, frameHeight = 192}))
animator:play('walkLeft')
animator:update(0.3)

-- The knight turns around and keeps the step it was on.
local step = animator.time
animator:play('walkRight')
animator:seek(step)
print(animator.frame)
```

### animator:queue(name)

Adds `name` to the queue. The next queued animation starts from its first frame on the update that ends the current pass. A one-shot animation ends its pass when it finishes. A looping animation ends its pass at the end of the cycle it is in, measured with `cycleDuration`, so a looping animation that has already played for a while still finishes its current cycle first. With no current animation, or when the current one-shot animation has already finished, `queue` plays `name` at once from its first frame, which replays a one-shot animation that just finished when `name` is that animation. An unknown name raises `The animation "<name>" does not exist in this animator.`

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

Advances the current animation by `dt` seconds times `speed`. It calls `onFrame` and `onEvent` for every frame it enters, in order, even the frames a long update passes, and `onComplete`, and starts the next queued animation when the current pass ends. The events of the first frame run with the first update after an animation starts. A looping animation that a single update moves more than two cycles reports only the frames of its last two cycles. A callback that plays another animation stops the reports of the old one. Errors raised by the callbacks propagate out of `update`. An animation that counts ticks raises `The animation "<name>" counts ticks, so it advances with "step".`

### animator:step(ticks)

Advances an animation that counts ticks by `ticks` whole steps, 1 by default, times `speed`, with the same callbacks as `update`, so a game on a fixed step plays its frames exactly. A negative count raises an argument error that says `ticks must not be negative`. An animation that counts seconds raises `The animation "<name>" counts seconds, so it cannot step by ticks.`

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

Sets the texture, source, width, height and pivot of `sprite`, or of the sprite a [`Node`](nodes2d.md#node) shows, from the current frame. The pivot is the pivot of the frame, or `pivotX` and `pivotY` in the untrimmed frame, so trimmed atlas frames stay in place. It mirrors where the sprite flips, so a flipped character stays on its feet. Position, scale, rotation, color and flips stay untouched. Without a current animation it does nothing.

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
animator.pivotX = 0.5
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

Returns the box `name` of the current frame as a `Rect` in the coordinates of the sprite, a `Sprite` or a `Node` that shows one. The box is placed, sized, scaled, turned and flipped as the sprite draws. A turned box returns the rectangle around its corners. A frame without such a box returns `nil`.

### animator:point(name, sprite)

Returns the `x` and `y` of the point `name` of the current frame, placed through the sprite, or `nil` when the frame has no such point.

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
        local body = animator:box('body', archer)
        if body then
            graphics2d.drawRectOutline(body, 2, '#FF40FF40')
        end
    end,
})
```

### animator.onFrame(name, frame)

The callback receives the animation name and the frame number, counted from 1, for every frame the animation enters, in order. It is not called for the first frame when an animation starts. Callbacks run inside `animator:update` and `animator:step`. Assign `nil` to remove one, and anything other than a function or `nil` raises an argument error. A callback that captures its own animator does not keep it alive forever.

### animator.onEvent(name, event, frame)

The callback receives the animation name, the event name and the frame number, counted from 1, for every event of every frame the animation enters, in order. That includes the first frame, on the first update after the animation starts.

### animator.onComplete(name)

The callback receives the animation name once, when a one-shot animation reaches its end. It may call `play` to switch animations.

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
animator.onEvent = function(name, event, frame)
    log.info('The event ' .. event .. ' fired on frame ' .. frame .. '.')
end
animator.onComplete = function(name)
    animator:play('idle')
end
animator:play('attack')
```

## AnimatedBatch

An `AnimatedBatch` is a [sprite batch](graphics2d.md#spritebatch) whose sprites each play an animation of a shared set with their own time, speed and scale, advanced in C++. It comes from `animation2d.newAnimatedBatch`. Every update writes the source, the size and the pivot of the frame each sprite shows, mirrored where the sprite flips. Batches of 4096 sprites or more advance on the worker pool. Sprites are numbered from 1, and removing one renumbers the sprites after it. An index outside the batch raises an argument error that says `sprite index out of range`.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | read | The texture of the batch. |

### batch:add(sprite)

Adds a sprite and returns its number. The table takes the keys of a [sprite batch sprite](graphics2d.md#sprite-fields), whose `source`, `width`, `height`, `pivotX` and `pivotY` the animation sets. It also takes `animation`, the name of the animation the sprite plays from its start, the first by name without one, `speed` (default `1`), `scaleX` and `scaleY` (default `1`), which scale the frames, and `partColors`. An unknown animation raises `The animation "<name>" does not exist in this animated batch.`, and a batch without animations raises `The animated batch has no animation to play.`

### batch:set(index, fields)

Changes the keys of `add` that `fields` gives. A new `animation` plays from its start, unless the sprite already plays it.

### batch:get(index)

Returns the sprite as the table of `batch:get` of a sprite batch, with `animation`, `speed`, `scaleX`, `scaleY`, `frame`, counted from 1, and `time`.

### batch:play(index, name, restart)

Switches a sprite to the animation `name` from its start, unless it plays it already and `restart` is not `true`.

### batch:seek(index, time)

Moves a sprite to `time` of its animation and shows the frame there. The time is in seconds, or in ticks for animations that count ticks.

### batch:animation(index)

Returns the name of the animation of a sprite.

### batch:frame(index)

Returns the frame that a sprite shows, counted from 1.

### batch:remove(index)

Removes a sprite, and renumbers the sprites after it.

### batch:clear()

Removes every sprite.

### batch:reserve(count)

Makes room for `count` sprites ahead of adding them.

### batch:size()

Returns the number of sprites.

### batch:animationNames()

Returns the names of the animations of the batch.

### batch:writeFields(buffer, fields, first)

Moves the fields of many sprites from a float buffer into the batch in one call, as [`batch:writeFields`](graphics2d.md#batchwritefieldsbuffer-fields-first) of a sprite batch does. A `first` below 1 raises an argument error that says `sprites are numbered from one`.

### batch:readFields(buffer, fields, first)

Copies the fields of many sprites into a float buffer in one call, as `batch:readFields` of a sprite batch does. A `first` below 1 raises an argument error that says `sprites are numbered from one`.

### batch:update(dt)

Advances every sprite by `dt` seconds times its speed. A batch whose animations count ticks raises `The animations of the animated batch count ticks, so it advances with "step".`

### batch:step(ticks)

Advances every sprite by `ticks` whole steps, 1 by default, times its speed, for animations that count ticks. A negative count raises an argument error that says `ticks must not be negative`, and a batch whose animations count seconds raises `The animations of the animated batch count seconds, so it cannot step by ticks.`

### batch:finished()

Returns the numbers of the sprites whose one-shot animation ended in the last update.

### batch:on(event, listener, options)

Listens to `'complete'`, which calls the listener once per update with the list of the sprites whose one-shot animation ended in it, and returns a `Connection`. The optional table `options` takes `owner`, and without one the listener takes the default owner. Another event raises `Unknown animated batch event "<event>". Animated batches report "complete".`

### batch:draw(order)

Draws every sprite in one call, with the [draw order](graphics2d.md#draw-order) keys of the optional table `order`.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local atlas = assets.load('units/soldier.json', 'atlas')
local army = animation2d.newAnimatedBatch(atlas.texture, {march = atlas:animation('march'), fall = atlas:animation('fall')})
army:reserve(500)
for index = 1, 500 do
    local soldier = army:add({x = (index % 25) * 40, y = (index // 25) * 40, animation = 'march'})
    -- Each soldier starts on a step of its own, so the army never marches in lockstep.
    army:seek(soldier, (index % 7) * 0.1)
end
print(army:size(), army:animation(7), army:frame(7), #army:animationNames())
local places = collections.newFloatBuffer(500 * 2)
army:readFields(places, {'x', 'y'})
for index = 1, 500 do
    places[index * 2] = places[index * 2] + 100
end
army:writeFields(places, {'x', 'y'})
army:remove(500)

scene.push({
    enter = function(self)
        army:on('complete', function(fallen)
            for _, index in ipairs(fallen) do
                army:set(index, {color = '#80FFFFFF'})
            end
        end, {owner = self})
        army:play(7, 'fall')
    end,
    update = function(self, dt)
        army:update(dt)
        print(#army:finished(), army:get(7).time)
    end,
    exit = function(self)
        army:clear()
    end,
    render = function(self)
        graphics2d.beginScreen()
        army:draw()
    end,
})
```

## StateGraph

A `StateGraph` is a checked [description](#state-graph-descriptions) of states, made by `animation2d.newStateGraph`, which any number of players share.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `states` | table | read | The names of the states. |
| `parameters` | table | read | The kind of each parameter by its name: `'number'`, `'boolean'`, `'trigger'` or `'vector'`. |

### graph:newPlayer(target, options)

Returns a [`StateGraphPlayer`](#stategraphplayer) that runs the graph on a target. The target is an `Animator`, a sprite of an `AnimatedBatch`, or a track of a `Skeleton`. The optional table `options` gives the number of the sprite of a batch as `sprite`, or the track of a skeleton as `track`, 1 by default. The player starts in the initial state and plays its animation. A track below 1 raises an argument error that says `tracks are numbered from one`.

A target without an animation that a state plays raises `The state graph plays the animation "<name>", which its target lacks.`, and a batch without a valid `sprite` raises `A state graph player of an animated batch needs the number of one of its sprites in "sprite".`

A player on a skeleton crossfades by the `fade` of each transition. Players on several tracks of one skeleton make layers, such as legs that walk on one track while the arms aim on another.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local graph = animation2d.newStateGraph('animation/villager.graph.json')
local puppet = animation2d.newSkeleton(assets.load('animation/puppet.skeleton'))
local legs = graph:newPlayer(puppet, {track = 1})
local arms = graph:newPlayer(puppet, {track = 2})
print(legs.state, arms.state)
```

## StateGraphPlayer

A `StateGraphPlayer` keeps the parameter values and the state of one character. It switches the animations of its target and never advances it, so the app updates its animator or batch as usual. Parameters are stored by index and conditions are evaluated in C++, so only the changes of state reach Lua.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `state` | string | read | The current state. |
| `stateTime` | number | read | The time the state has lasted, from the `dt` of the updates. |
| `animation` | string | read | The animation the state plays, the one of the current direction in a directional state. |

### player:set(name, value)

Sets a parameter by its kind: a number, a boolean, a vector as a `Vec2` or a table of two numbers, or `true` to set a trigger. A parameter the graph lacks raises `The state graph has no parameter "<name>".`

### player:get(name)

Returns the value of a parameter by its kind. A trigger reads as `false`. A parameter the graph lacks raises `The state graph has no parameter "<name>".`

### player:trigger(name)

Sets a trigger, which stays set until a transition whose condition reads it fires. A parameter of another kind raises `The parameter "<name>" is a <kind>, not a trigger.`

### player:update(dt)

Turns a directional state to the direction of its vector and takes at most one transition.

### player:enter(state)

Enters a state at once, from its start. A state the graph lacks raises `The state graph has no state "<name>".`

### player:on(event, listener, options)

Listens to `'stateChanged'`, which calls the listener with the state left and the state entered, and returns a `Connection`. The optional table `options` takes `owner`, and without one the listener takes the default owner. Another event raises `Unknown state graph player event "<event>". Players report "stateChanged".`

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local scene = require('haylen.scene')

local graph = animation2d.newStateGraph('animation/villager.graph.json')
local atlas = assets.load('units/villager.json', 'atlas')
local crowd = animation2d.newAnimatedBatch(atlas.texture, {idle = atlas:animation('idle'), walk = atlas:animation('walk')})
crowd:add({x = 100, y = 100})
local villager = graph:newPlayer(crowd, {sprite = 1})

scene.push({
    enter = function(self)
        villager:on('stateChanged', function(from, to) print(from .. ' to ' .. to) end, {owner = self})
        villager:set('moving', true)
        villager:trigger('wave')
        print(villager:get('moving'))
    end,
    update = function(self, dt)
        villager:update(dt)
        crowd:update(dt)
        if villager.stateTime > 10 then
            villager:enter('idle')
        end
        print(villager.state, villager.animation)
    end,
})
```

## SkeletonData

`SkeletonData` is the shared, unchanging data of a `.skeleton` file, from `assets.load(path)` or `animation2d.newSkeletonData`. Two values compare equal with `==` when they are the same data.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `bones` | table | read | The names of the bones. |
| `slots` | table | read | The names of the slots. |
| `skins` | table | read | The names of the skins. |
| `animations` | table | read | The names of the animations. |
| `events` | table | read | The names of the events. |

### data:duration(name)

Returns the length of an animation in seconds. An animation the data lacks raises `The skeleton has no animation "<name>".`

```lua
local assets = require('haylen.assets')

local data = assets.load('animation/puppet.skeleton')
print(#data.bones, #data.slots, data.skins[1], #data.events, data:duration(data.animations[1]))
```

## Skeleton

A `Skeleton` is one posed character of [`SkeletonData`](#skeletondata), from `animation2d.newSkeleton`. Tracks are numbered from 1 to 32, and a track below 1 raises an argument error that says `tracks are numbered from one`.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `x`, `y`, `rotation`, `scaleX`, `scaleY` | number | read and write | The place of the root in the world, which tweens animate natively. |
| `color` | Color | read and write | Multiplies the color of every slot. |
| `skin` | string | read and write | The skin whose attachments the slots show. A skin the data lacks raises `The skeleton has no skin "<name>".` |
| `data` | SkeletonData | read | The data of the skeleton. |

### skeleton:play(name, options)

Plays an animation on a track from its start. The optional table `options` takes `track`, counted from 1 (default `1`), `loop` (default `true`) and `fade`, the seconds it crossfades from what the track played (default `0`). An animation the data lacks raises `The skeleton has no animation "<name>".`, a negative fade raises `A fade lasts zero seconds or more.` and a track past 32 raises `A skeleton has at most 32 tracks.`

### skeleton:crossFade(name, seconds, options)

Plays an animation on a track from its start, crossfading from what the track played over `seconds`. The optional table `options` takes `track` and `loop`, as `play` does, with the same errors.

### skeleton:stop(track)

Clears a track, 1 by default.

### skeleton:setTrackWeight(track, weight)

Sets the weight from 0 to 1 that a track mixes in by. A track past 32 raises `A skeleton has at most 32 tracks.`

### skeleton:trackAnimation(track)

Returns the name of the animation of a track, or `nil`.

### skeleton:trackTime(track)

Returns the time of a track in seconds.

### skeleton:setAttachment(slot, name)

Shows the attachment `name` of the skin in a slot, or nothing with `nil`. A slot the data lacks raises `The skeleton has no slot "<slot>".`, and an attachment the skin lacks raises `The slot "<slot>" has no attachment "<name>" in the skin "<skin>".`

### skeleton:bone(name)

Returns a table with the local `x`, `y`, `rotation`, `scaleX` and `scaleY` of a bone and its `worldX`, `worldY`, `worldRotation` and `worldScaleX` as of the last pose, to attach other things to the bone. A bone the data lacks raises `The skeleton has no bone "<name>".`

### skeleton:setIkTarget(constraint, x, y)

Makes an inverse kinematics constraint reach a point in the world instead of its target bone. A constraint the data lacks raises `The skeleton has no inverse kinematics constraint "<name>".`, as the next two methods do.

### skeleton:clearIkTarget(constraint)

Gives an inverse kinematics constraint back its target bone.

### skeleton:setIkMix(constraint, mix)

Sets the mix from 0 to 1 that an inverse kinematics constraint starts every pose with, which its timelines change.

### skeleton:update(dt)

Advances every track by `dt` seconds and poses the skeleton, firing the events that the tracks passed.

### skeleton:pose()

Poses the skeleton at the current times of its tracks, such as after moving it.

### skeleton:point(slot)

Returns the `x` and `y` in the world of the point attachment that a slot shows, or `nil`.

### skeleton:bounds()

Returns the `Rect` around every region and mesh of the last pose, which culls skeletons outside the view.

### skeleton:draw(order)

Draws the slots in draw order, with the [draw order](graphics2d.md#draw-order) keys of the optional table `order`.

### skeleton:on(event, listener, options)

Listens to `'event'`, which calls the listener with the table `{name, track, animation, text, number}` of every event the tracks pass, where `track` counts from 1, and returns a `Connection`. The optional table `options` takes `owner`, and without one the listener takes the default owner. Another event raises `Unknown skeleton event "<event>". Skeletons report "event".`

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local puppet = animation2d.newSkeleton(assets.load('animation/puppet.skeleton'), {x = 400, y = 500})
puppet:play('walk', {track = 1})
puppet:play('wave', {track = 2, fade = 0.2})
puppet:setTrackWeight(2, 0.5)
puppet:setAttachment('hat', 'crown')
puppet:setIkMix('aim', 0.8)
puppet.x = 420
puppet.rotation, puppet.scaleX, puppet.scaleY = 0.1, -1, 1
puppet:pose()

scene.push({
    update = function(self, dt)
        puppet:update(dt)
        local hand = puppet:bone('frontFore')
        print(hand.worldX, hand.worldY, puppet:trackAnimation(1), puppet:trackTime(1))
        if puppet:trackTime(2) > 3 then
            puppet:stop(2)
            puppet:clearIkTarget('aim')
        end
    end,
    render = function(self)
        graphics2d.beginScreen()
        local area = puppet:bounds()
        if area.width > 0 then
            puppet:draw()
        end
        local x, y = puppet:point('hand')
        if x then
            graphics2d.drawCircle(x, y, 4, '#FFFFE040')
        end
    end,
})
```

## AtlasBuilder

An `AtlasBuilder` collects frames and packs them into one texture, as the [guide](../2d/animation.md#atlases-at-run-time) explains. It comes from `animation2d.newAtlasBuilder`.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `count` | integer | read | The frames added so far. |

### builder:add(name, image, area)

Adds an [`Image`](graphics.md#image), or an area of it, as the frame `name`. A name given twice raises `The atlas has the frame "<name>" twice.`, and an area outside the image raises `The area of the frame "<name>" of an atlas is not inside its image.`

### builder:addAtlas(prefix, atlas, image)

Adds every frame of a `SpriteAtlas` with its pivot, boxes, points and events, and every animation of it, each named with `prefix` in front. The frames are cut from `image`, the pixels of the texture of the atlas, such as `assets.loadAsync('hero.png', 'image')` gives. An image of another size than the texture raises `The image of an atlas added to an atlas builder has the size of the texture of the atlas.`

### builder:build(options)

Packs the frames into a `SpriteAtlas` on the frame thread and returns it. The optional table `options` takes:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `padding` | integer | `2` | Pixels between frames and around the edges. |
| `extrude` | integer | `1` | Pixels each frame repeats its edges outward, at most half the padding. |
| `maxSize` | integer | `4096` | The largest width and height, at most `graphics.maxTextureSize()`. |
| `trim` | boolean | `true` | Leaves out the transparent borders of every frame. |
| `filter`, `wrap`, `mipmaps` | | | The options of the texture, as `graphics.newTexture` takes them. |

No frames raise `An atlas needs at least one image.`. A negative padding or an extrusion over half the padding raises `An atlas needs a padding of zero or more, an extrusion of at most half the padding and a positive largest size.`, and frames that need more room raise `The images of the atlas need more room than a texture of <n> pixels.`

### builder:buildAsync(options)

Packs the frames on the task pool and returns a promise of the `SpriteAtlas`, with the options and errors of `build`. Only the texture is made on the frame thread, and frames added while it packs stay out.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local async = require('async')

async.spawn(function()
    local builder = animation2d.newAtlasBuilder()
    builder:add('gem', assets.loadAsync('items/gem.png', 'image'):await())
    builder:add('coin', assets.loadAsync('items/coins.png', 'image'):await(), {0, 0, 32, 32})
    local items = builder:build({padding = 2, trim = true})
    for _, name in ipairs({'knight', 'archer'}) do
        local atlas = assets.loadAsync('units/' .. name .. '.json', 'atlas', {filter = 'linear'}):await()
        builder:addAtlas(name .. '/', atlas, assets.loadAsync('units/' .. name .. '.png', 'image'):await())
    end
    local packed = builder:buildAsync({padding = 4, extrude = 2, filter = 'linear'}):await()
    print(builder.count, items:hasFrame('gem'), #packed:frameNames())
end)
```

## SpriteAtlas

A `SpriteAtlas` holds the named frames, animations and nine-slices of an atlas, from `assets.load(path, 'atlas')`, `builder:build` or `atlas:subset`. Two atlas values compare equal with `==` when they are the same atlas.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | read | The atlas image. |

### atlas:frameNames()

Returns the list of frame names in file order.

### atlas:animationNames()

Returns the names of the animations made from frame tags, sorted.

### atlas:sliceNames()

Returns the names of the nine-slices made from slices, sorted.

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

Describes the frame `name` with a table of these fields. An unknown name raises `The frame "<name>" does not exist in this atlas.`

| Field | Type | Meaning |
| --- | --- | --- |
| `source` | Rect | The frame rectangle in the atlas image, the same as `atlas:source(name)`. |
| `offset` | Vec2 | Where the trimmed rectangle starts inside the original frame. It is zero for untrimmed frames. |
| `originalSize` | Vec2 | The size of the frame before trimming. |
| `duration` | number | The frame duration in seconds. |
| `ticks` | integer | `0`, since atlas frames count seconds. |
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

Returns a copy of the `Animation` made from the tag `name`. An unknown name raises `The animation "<name>" does not exist in this atlas.`

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
local animator = animation2d.newAnimator()
animator:add('walk', atlas:animation('walk'))
animator:play('walk')
```

### atlas:slice(name)

Returns the `NineSlice` made from the slice `name`, ready for `graphics2d.drawNineSlice`. An unknown name raises `The slice "<name>" does not exist in this atlas.`

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

Returns the source `Rect` of the frame `name` in the atlas image. An unknown name raises `The frame "<name>" does not exist in this atlas.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local atlas = assets.load('ui/hero.json', 'atlas')
local icon = graphics2d.newSprite(atlas.texture, {source = atlas:source('hero 1'), x = 64, y = 64})
```

### atlas:apply(sprite, name)

Shows the frame `name` on `sprite` and sets its texture, source, width, height and pivot. The pivot stays at the center of the untrimmed frame. An unknown name raises `The frame "<name>" does not exist in this atlas.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local atlas = assets.load('ui/hero.json', 'atlas')
local sprite = graphics2d.newSprite(atlas.texture, {x = 300, y = 200})
atlas:apply(sprite, 'hero 0')
```

### atlas:subset(prefix)

Returns a `SpriteAtlas` of the frames, animations and slices whose names start with `prefix`, named without it, over the same texture, such as one character of an atlas that an [`AtlasBuilder`](#atlasbuilder) packed with many.

```lua
local assets = require('haylen.assets')

local atlas = assets.load('units/army.json', 'atlas')
local knight = atlas:subset('knight/')
print(knight:hasAnimation('run'), knight.texture == atlas.texture)
```

### atlas:animationFromFrames(names, options)

Builds an `Animation` that shows the named frames in order at a fixed rate. The optional table `options` takes `framesPerSecond` (default `10`) and `loop` (default `'loop'`). An empty list raises `An atlas animation needs frames and a positive frame rate.`, and an unknown frame raises `The frame "<name>" does not exist in this atlas.`

```lua
local assets = require('haylen.assets')

local atlas = assets.load('ui/hero.json', 'atlas')
local blink = atlas:animationFromFrames({'hero 1', 'hero 2', 'hero 1'}, {framesPerSecond = 6, loop = 'once'})
print(blink.duration)
```

## Chain

A `Chain` is a row of points joined by segments of fixed lengths, which procedural animation bends, as the [guide](../2d/animation.md#procedural-animation-with-chains) explains. It comes from [`animation2d.newChain`](#animation2dnewchainoptions). Points are numbered from 1 at the root to `chain.count` at the end, and segments from 1 to `chain.count - 1`, where a segment runs from its point to the next one.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | read and write | The root. Writing it moves the whole chain with it. |
| `count` | integer | read | The points of the chain, one more than its segments. |
| `length` | number | read | The sum of the lengths of its segments. |
| `maxBend` | number | read and write | The largest bend between two segments in a row, kept between 0 and `math.pi`. |

### chain:reach(x, y, options)

Bends the chain from its root toward the point and returns whether its end reached it within the tolerance. A point out of reach stretches the chain straight toward it. The optional table `options` is read in one pass, so a reach for every leg in every frame costs little:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `pole` | vector | none | A point on the side the joints bend toward. Without it, the joints keep the side they bend to. |
| `iterations` | integer | `10` | The most passes of the solver, at least 1. Fewer raise an argument error that says `a reach needs at least one iteration`. |
| `tolerance` | number | `0.01` | How near the end must come to the point. |

### chain:follow(x, y)

Moves the first point to the head and drags each point after it to the length of its segment from the one before, within `maxBend`.

### chain:straighten(angle)

Lays the chain straight from its root along the angle in radians.

### chain:point(index)

Returns the `x` and `y` of a point. An index outside the chain raises an argument error that says `points are numbered from one to the count of the chain`.

### chain:angle(segment)

Returns the angle in radians of a segment. A segment outside the chain raises an argument error that says `segments are numbered from one to the count of the chain minus one`.

### chain:setPoint(index, x, y)

Places one point without moving the others, such as a point of a body pushed out of a wall or onto the ground. The next `reach` or `follow` restores the lengths of the segments from there. An index outside the chain raises the argument error of `chain:point`.

### chain:writePoints(buffer, first)

Writes the `x` and `y` of every point in order into a [`FloatBuffer`](collections.md#float-buffers) from the value at `first` on, which defaults to 1, so many chains reach Lua in one call each. A `first` below 1 raises an argument error that says `the values of a buffer are numbered from one`, and a buffer too small raises `The buffer holds <n> values, and the <n> points of the chain need <n> from value <n> on.`

```lua
local animation2d = require('haylen.animation2d')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local leg = animation2d.newChain({x = 400, y = 200, lengths = {90, 90}})
local snake = animation2d.newChain({x = 200, y = 300, segments = 24, length = 14, angle = math.pi, maxBend = 0.5})
local points = collections.newFloatBuffer(snake.count * 2)
local pole = {400, 0}

scene.push({
    update = function(self, dt)
        local x, y = input.mousePosition()
        local reached = leg:reach(x, y, {pole = pole, iterations = 12, tolerance = 0.5})
        snake:follow(x, y)
        if y > 600 then
            snake:setPoint(snake.count, x, 600)
        end
        snake:writePoints(points)
        if not reached then
            leg:straighten(leg:angle(1))
        end
        snake.maxBend = 0.4
    end,
    render = function(self)
        graphics2d.beginScreen()
        for index = 1, snake.count - 1 do
            local x1, y1 = snake:point(index)
            local x2, y2 = snake:point(index + 1)
            graphics2d.drawLine(x1, y1, x2, y2, 18 - index * 0.5, '#FF35B37E')
        end
        local hipX, hipY = leg:point(1)
        local kneeX, kneeY = leg:point(2)
        local footX, footY = leg:point(3)
        graphics2d.drawLine(hipX, hipY, kneeX, kneeY, 10, '#FFE8A93A')
        graphics2d.drawLine(kneeX, kneeY, footX, footY, 8, '#FFE8A93A')
    end,
})
```
