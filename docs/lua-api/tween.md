# haylen.tween

The module `haylen.tween` animates values over time. A tween moves fields of tables and properties of engine objects toward target values with an easing curve, and it can repeat, yoyo, run backwards, seek and be awaited. Ready-made tweens move, scale, rotate, fade, tint, jump, follow paths and Bézier curves, blink, shake and punch. Timelines play tweens, pauses and calls at their places in time and nest into each other, and staggers start the same tween on many targets one after another. Use tweens for UI motion, camera moves, fades, juice such as hits and pickups, and scripted cutscenes.

```lua
local tween = require('haylen.tween')
```

## How tweens run

The engine advances tweens once per frame, after timers and before the scene update, and fixed-step tweens with the fixed updates of physics. Each tween runs by its process mode, as the [lifecycle guide](../lifecycle.md#pause-and-process-modes) explains, on scaled time by default, so `haylen.setTimeScale` slows it down, or on real time with `unscaled = true`. Updating thousands of tweens allocates nothing.

A tween reads its start values when it first renders, after its delay, so a delayed tween continues from wherever the fields are at that moment. Every frame it writes the new values: fields of tables and objects by plain assignment, and properties of engine objects, such as the position of a sprite, the zoom of a camera or the color of a light, natively without running any Lua. A tween holds its target weakly and stops before it writes again once the target is garbage collected or the engine object behind it is released. An error raised by a callback, or by a target that refuses an assignment, stops the app and shows the error screen.

## Native properties

Number, `Vec2` and `Color` properties of engine objects are native: a tween reads and writes them in C++ without calling Lua, so thousands of them cost no script time. Sprites, cameras, lights, particle emitters and the transforms of UI nodes are the usual targets. The transform that `document:transform(id)` of [`haylen.ui`](ui.md#documenttransformid) returns moves, scales, fades and tints a node of a UI document, and the document keeps it alive while the node exists, so a tween may target it directly.

```lua
local math2d = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local hud = ui.mount(ui.label{id = 'combo', text = 'Combo x3', font = 'title'})
local combo = hud:transform('combo')
combo.scale = math2d.vec2(1.6, 1.6)
tween.to(combo, 0.3, {scale = math2d.vec2(1, 1), opacity = 0.8, tint = '#FFFFD040'}, {ease = 'quadOut'})
```

## Values

A tween animates numbers, `Vec2` values, `Color` values and texts, and the end value is read as the kind of the current value. A `Vec2` field accepts a `Vec2`, `{x = 10, y = 20}` or `{10, 20}`, and a `Color` field accepts a `Color`, a `'#AARRGGBB'` string or a color table of [`haylen.math`](math.md). A text reveals the end text over the start text one character at a time, like a typewriter. A field name may be a path into nested tables or into the components of a value, such as `'position.x'` or `'color.a'`, and one tween animates as many fields as its value table names.

These tween options change how values travel.

| Key | Type | Meaning |
| --- | --- | --- |
| `angles` | list of field names | Numbers in radians that take the shorter way around the circle, so 350 degrees tween to 10 degrees through 360. |
| `integers` | list of field names | Numbers that stay whole, such as score counters. |
| `colorSpace` | string | Either `'rgb'`, the default, or `'hsv'`, which blends colors through hue, saturation and value. |

```lua
local tween = require('haylen.tween')
local m = require('haylen.math')

local hud = {score = 0, heading = math.rad(350), banner = '', tint = m.color('#FFFF0000'), offset = m.vec2(0, 0)}

tween.to(hud, 1, {score = 1500, heading = math.rad(10), banner = 'Wave cleared', tint = '#FF0000FF', ['offset.y'] = -40}, {
    angles = {'heading'},
    integers = {'score'},
    colorSpace = 'hsv',
})

require('haylen.timer').after(0.5, function()
    print(hud.score, hud.banner, hud.offset.y)
end)
```

## Options

Every tween and timeline takes an optional options table, and unknown keys raise `Unknown option "<key>".`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `delay` | number | `0` | Seconds to wait before the tween starts. Times such as `time` and `duration` exclude it. |
| `repeatCount` | integer | `0` | Extra loops after the first one. A negative count repeats forever. |
| `loopMode` | string | `'restart'` | The mode `'restart'` plays every loop from the start, `'yoyo'` plays every other loop backwards and `'incremental'` continues every loop from where the previous one ended. |
| `repeatDelay` | number | `0` | Seconds to wait between loops. |
| `timeScale` | number | `1` | Speed of this tween, multiplied with the time scale of its tag. |
| `tag` | string | `''` | Group name for the tag functions, such as `tween.killTag` and `tween.setTimeScale`. |
| `owner` | table or userdata | `nil` | Kills the tween when the owner ends, as the [owners of `haylen.events`](events.md#owners) describe. A tween that inherits its process mode follows the mode of its owner every frame, so it changes along with the owner. |
| `processMode` | string | `'inherit'` | `'inherit'`, `'pausable'`, `'whenPaused'`, `'always'` or `'disabled'`. |
| `unscaled` | boolean | `false` | Counts real time, ignoring the time scale. |
| `fixedStep` | boolean | `false` | Advances with the fixed steps of physics instead of once per frame. |
| `autoKill` | boolean | `true` | Leaves the engine when it completes. Turn it off to replay, reverse or seek a finished tween. |
| `paused` | boolean | `false` | Creates the tween paused, so it waits for `play`. |
| `onStart` | function | `nil` | Called when the tween starts playing forward after its delay. |
| `onUpdate` | function | `nil` | Called after every frame that moved the tween, with its eased progress, which the `back` and `elastic` curves may push past 0 and 1. |
| `onLoop` | function | `nil` | Called with the number of the loop that begins, from 1. |
| `onComplete` | function | `nil` | Called when the tween reaches its end in the direction it plays. |
| `onKill` | function | `nil` | Called once when the tween leaves the engine: when it is killed, or when it completes with `autoKill`. |

Tweens that animate values also take these options, and timelines take `onStep`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `ease` | curve | `'linear'` | Easing curve, as [Easing](#easing) describes. |
| `overwrite` | boolean | `false` | Makes this tween the only one that animates its fields: other tweens give them up, and tweens left with nothing to animate are killed. |
| `speedBased` | boolean | `false` | Reads the seconds as units per second and finds the duration from the distance the values travel. A speed-based tween cannot join a timeline. |
| `angles`, `integers`, `colorSpace` | | | As [Values](#values) describes. |
| `onStep` | function | `nil` | Timelines only. Called with the index of the step that ends, from 1. |

## Easing

The `ease` option takes a curve in one of these forms, the same ones `m.ease` of [`haylen.math`](math.md) accepts.

| Form | Example | Curve |
| --- | --- | --- |
| Name | `'quadOut'` | `'linear'` or the `In`, `Out` and `InOut` variants of `sine`, `quad`, `cubic`, `quart`, `quint`, `expo`, `circ`, `back`, `elastic` and `bounce`. |
| Back | `{curve = 'backOut', overshoot = 3}` | A `back` curve with its overshoot, 1.70158 by default. |
| Elastic | `{curve = 'elasticOut', amplitude = 1.5, period = 0.4}` | An `elastic` curve with its amplitude and period, 1 and 0.3 by default. |
| Steps | `{steps = 4, position = 'end'}` | Jumps in steps like CSS `steps()`, with `position` `'start'`, `'end'`, `'both'` or `'none'`. |
| Bézier | `{cubicBezier = {0.25, 0.1, 0.25, 1}}` | A cubic Bézier like CSS `cubic-bezier()`, with x values between 0 and 1. |
| Points | `{points = {0, 1.2, 1}}` | Straight lines through evenly spaced values, or through `{x, y}` points with growing x. |
| Function | `function(t) return t * t end` | Any function from progress to eased progress. |

```lua
local tween = require('haylen.tween')

local card = {x = 0, y = 0, scale = 1}

tween.to(card, 0.6, {x = 400}, {ease = {curve = 'backOut', overshoot = 3}})
tween.to(card, 0.6, {y = 200}, {ease = {cubicBezier = {0.68, -0.6, 0.32, 1.6}}})
tween.to(card, 0.6, {scale = 2}, {ease = function(t) return math.sin(t * math.pi / 2) end})
```

## Functions

### tween.to(target, seconds, values, options)

Animates fields of `target` from their current values to `values` over `seconds` and returns a [`Tween`](#tween). The argument `target` is a table, an object or an engine userdata, `values` maps field names to end values and `options` is the optional table of [Options](#options). The call raises `A tween target must be a table or a userdata, not <type>.` for another target, `A tween needs a positive duration.` for a duration that is not positive, `Cannot tween the field "<name>" because it is not a number, a "Vec2", a "Color" or a text.` for a field of another type or a missing field, and `bad argument #3 to 'to' (tween fields are named by strings)` for keys that are not strings.

```lua
local tween = require('haylen.tween')
local graphics2d = require('haylen.graphics2d')

local panel = {x = -600, alpha = 0}
tween.to(panel, 0.4, {x = 80, alpha = 1}, {ease = 'backOut'})

-- The camera is an engine object, so its zoom and position animate without running Lua.
local camera = graphics2d.newCamera()
tween.to(camera, 2, {zoom = {1.5, 1.5}, ['position.x'] = 320}, {ease = 'sineInOut', delay = 0.4})
```

### tween.from(target, seconds, values, options)

Animates fields of `target` from `values` back to the values they have when the call is made. The fields take the starting values at once, even while the delay runs, so the target never shows its end values first. It accepts the same arguments and raises the same errors as `tween.to`.

```lua
local tween = require('haylen.tween')

local title = {y = 200, alpha = 1}
tween.from(title, 0.8, {y = -150, alpha = 0}, {ease = 'bounceOut', delay = 0.2})
print(title.y, title.alpha) -- -150.0 0.0
```

### tween.by(target, seconds, offsets, options)

Animates fields of `target` by the given offsets, from their current values to the current values plus the offsets. With `loopMode = 'incremental'` every loop moves by the offsets again.

```lua
local tween = require('haylen.tween')

local conveyor = {x = 0}
tween.by(conveyor, 0.5, {x = 64}, {repeatCount = 3, loopMode = 'incremental'})

require('haylen.timer').after(2.1, function() print(conveyor.x) end) -- 256.0
```

### tween.fromTo(target, seconds, from, to, options)

Animates fields of `target` from the values of `from` to the values of `to`, whatever the fields hold before. Every field of `from` needs an end value, otherwise the call raises `A "tween.fromTo" call needs an end value for the field "<name>".`

```lua
local tween = require('haylen.tween')

local meter = {fill = 0.8}
tween.fromTo(meter, 1, {fill = 0}, {fill = 1}, {ease = 'quadOut'})
```

## Ready-made tweens

Ready-made tweens take the target, the seconds, one value of their own and the options. Each one animates default fields, such as `x` and `y` for positions, and the `field` option names others: one field name, such as `'position'` for a `Vec2` field, or a list of two number fields, such as `{'left', 'top'}`. They accept every option of [Options](#options) and return a [`Tween`](#tween).

| Function | Default fields | Value |
| --- | --- | --- |
| `tween.move(target, seconds, position, options)` | `x`, `y` | The position to reach. |
| `tween.scale(target, seconds, scale, options)` | `scaleX`, `scaleY` | The scale to reach, one number for both axes or a pair. |
| `tween.rotate(target, seconds, angle, options)` | `rotation` | The angle to reach in radians, taking the shorter way around. |
| `tween.fade(target, seconds, alpha, options)` | `color.a` | The alpha to reach. |
| `tween.tint(target, seconds, color, options)` | `color` | The color to reach. |
| `tween.jump(target, seconds, position, options)` | `x`, `y` | The position to land on. |
| `tween.path(target, seconds, points, options)` | `x`, `y` | The points to travel through. |
| `tween.bezier(target, seconds, points, options)` | `x`, `y` | One or two control points followed by the end point. |
| `tween.blink(target, seconds, count, options)` | `color.a` | How many times to blink. |
| `tween.shake(target, seconds, strength, options)` | `x`, `y` | How far to shake, one number for both axes or a pair. |
| `tween.punch(target, seconds, offset, options)` | `x`, `y` | The offset to spring toward and back from. |

The motions of jumps, paths, Bézier curves, blinks, shakes and punches take their own options.

| Function | Key | Default | Meaning |
| --- | --- | --- | --- |
| `jump` | `power` | `100` | Height of each hop, toward negative y, which is up on screen. |
| `jump` | `jumps` | `1` | Number of jumps on the way. |
| `path` | `curved` | `true` | Passes smoothly through every point as a Catmull-Rom spline, or in straight lines when `false`. |
| `path` | `closed` | `false` | Travels back to the start after the last point. |
| `path` | `orient` | `false` | Turns a rotation field along the path. |
| `path` | `orientField` | `'rotation'` | The field that `orient` turns. |
| `blink` | `hidden` | `0`, or transparent for colors | The value shown between blinks. A `Vec2` or a text needs it. |
| `shake`, `punch` | `vibrato` | `10` | How many times it swings. |
| `shake` | `randomness` | `90` | How far, in degrees from 0 to 180, each shake of a vector bends from the previous direction. |
| `shake` | `seed` | Random | Makes the pattern repeatable. |
| `punch` | `elasticity` | `1` | From 0 to 1, how far it swings past the start on the way back. |

Paths travel at constant speed and end on the last point. Blinks, shakes and punches come back to the start value. A path, a Bézier curve or a jump needs a `Vec2` field or a pair of number fields, and raises `A path tween needs a "Vec2" field or a pair of number fields.` or the same message for the other kinds otherwise.

```lua
local tween = require('haylen.tween')
local m = require('haylen.math')

local coin = {x = 100, y = 500, scaleX = 1, scaleY = 1, rotation = 0, color = m.color('#FFFFFFFF')}
tween.jump(coin, 0.6, {400, 500}, {power = 120, jumps = 2})
tween.scale(coin, 0.6, 1.5, {ease = 'backOut'})
tween.rotate(coin, 0.6, math.pi)
tween.tint(coin, 0.6, '#FFFFD166', {delay = 0.6})
tween.fade(coin, 0.3, 0, {delay = 1.2})

local boat = {position = m.vec2(0, 0), rotation = 0}
tween.path(boat, 4, {{200, 0}, {200, 200}, {0, 200}}, {field = 'position', closed = true, orient = true, repeatCount = -1})

local comet = {x = 0, y = 0}
tween.bezier(comet, 1.5, {{300, -200}, {600, 400}, {900, 0}})

local portal = {color = m.color('#FF66CCFF')}
tween.blink(portal, 1, 4, {field = 'color', hidden = '#0066CCFF'})

local camera = require('haylen.graphics2d').newCamera()
tween.shake(camera, 0.4, 12, {field = 'position', vibrato = 20, seed = 7})

local button = {offsetY = 0}
tween.punch(button, 0.5, -16, {field = 'offsetY', vibrato = 6, elasticity = 0.5})
```

## Timelines and staggers

### tween.timeline(options)

Creates an empty [`Timeline`](#timeline) and returns it. A timeline is a tween that plays other tweens, pauses and calls at their places in time. It takes every option of [Options](#options) except those that animate values, plus `onStep`, and it repeats, yoyos, seeks and reverses like any tween, moving everything inside it. Tweens join a timeline right after they are created, before they play, and leave the engine to play inside it. A tween that has already started raises `Only a tween that has not started and is in no other timeline can join a timeline.`

```lua
local tween = require('haylen.tween')

local door = {angle = 0}
local hero = {x = 0, alpha = 0}

local cutscene = tween.timeline({onStep = function(step) print('step ' .. step) end, onComplete = function() print('done') end})
cutscene:append(tween.to(door, 0.5, {angle = 90}))
    :append(tween.to(hero, 1, {x = 300}))
    :join(tween.to(hero, 0.3, {alpha = 1}))
    :append(0.25)
    :append(function() print('the hero waves') end)
```

### tween.stagger(targets, seconds, make, options)

Builds one tween per target of the sequence `targets` with `make(target, index)`, which returns the tween, and plays them in a new timeline, each one starting `seconds` after the previous one. The `origin` option picks the order: `'start'`, the default, starts with the first target, `'end'` with the last and `'center'` from the middle outwards. The timeline takes every other timeline option.

```lua
local tween = require('haylen.tween')

local tiles = {}
for index = 1, 5 do
    tiles[index] = {y = 0}
end

tween.stagger(tiles, 0.08, function(tile, index)
    return tween.to(tile, 0.3, {y = -40}, {ease = 'quadOut', repeatCount = 1, loopMode = 'yoyo'})
end, {origin = 'center', tag = 'board'})
```

## Groups

### tween.killTag(tag)

Kills every tween with the tag, running their `onKill`.

```lua
local tween = require('haylen.tween')

local clouds = {x = 0}
tween.to(clouds, 30, {x = 1920}, {repeatCount = -1, tag = 'world'})
tween.killTag('world')
print(tween.size()) -- 0
```

### tween.completeTag(tag, withCallbacks)

Jumps every tween with the tag to its end. The argument `withCallbacks`, `true` by default, runs the callbacks on the way, such as `onComplete`. Tweens that repeat forever do not complete.

```lua
local tween = require('haylen.tween')

local menu = {x = 0, alpha = 1}
tween.to(menu, 0.5, {x = 300}, {tag = 'menu'})
tween.to(menu, 0.5, {alpha = 0}, {tag = 'menu', onComplete = function() print('menu hidden') end})

-- The player skipped the animation, so it snaps to the end.
tween.completeTag('menu')
print(menu.x, menu.alpha) -- 300.0 0.0
```

### tween.pauseTag(tag)

Pauses every tween with the tag.

```lua
local tween = require('haylen.tween')

local clouds = {x = 0}
tween.to(clouds, 30, {x = 1920}, {repeatCount = -1, tag = 'world'})
tween.pauseTag('world')
```

### tween.resumeTag(tag)

Resumes every tween with the tag.

```lua
local tween = require('haylen.tween')

local clouds = {x = 0}
tween.to(clouds, 30, {x = 1920}, {repeatCount = -1, tag = 'world', paused = true})
tween.resumeTag('world')
```

### tween.setTimeScale(tag, scale)

Scales the time of every tween with the tag, including tweens created later, on top of their own `timeScale`.

```lua
local tween = require('haylen.tween')

local enemy = {x = 0}
tween.to(enemy, 2, {x = 500}, {tag = 'enemies'})

-- Bullet time for the enemies only.
tween.setTimeScale('enemies', 0.25)
```

### tween.timeScale(tag)

Returns the time scale of the tag, `1` unless `tween.setTimeScale` changed it.

```lua
local tween = require('haylen.tween')

tween.setTimeScale('ui', 2)
print(tween.timeScale('ui'), tween.timeScale('world')) -- 2.0 1.0
```

### tween.killTarget(target)

Kills every tween that animates `target`, including tweens inside timelines.

```lua
local tween = require('haylen.tween')

local enemy = {x = 0, y = 0}
tween.move(enemy, 2, {300, 200})
tween.shake(enemy, 2, 4)

local function defeat()
    tween.killTarget(enemy)
end

defeat()
print(tween.size()) -- 0
```

### tween.killAll()

Kills every tween at once, running their `onKill`. Fields keep their current values, and every pending `wait()` promise resolves with `false`.

```lua
local tween = require('haylen.tween')
local scene = require('haylen.scene')

local function restartLevel(level)
    tween.killAll()
    scene.clear()
    scene.push(level)
end

restartLevel({})
```

### tween.size()

Returns the number of tweens the engine plays, including delayed and paused ones and finished ones without `autoKill`. Tweens inside timelines count as their timeline.

```lua
local tween = require('haylen.tween')

local coin = {y = 0}
tween.to(coin, 0.3, {y = -40}, {loopMode = 'yoyo', repeatCount = 1})

if tween.size() > 0 then
    print('animations are still playing')
end
```

## Tween

The handle that every tween function returns. Dropping it does not stop the tween. The control methods return the handle, so calls chain, such as `handle:restart():pause()`.

### handle:play()

Plays forward from the current time, also after `pause` or `reverse`.

```lua
local tween = require('haylen.tween')

local drawer = {y = 0}
local opening = tween.to(drawer, 0.4, {y = 120}, {paused = true})

local function open()
    opening:play()
end

open()
```

### handle:pause()

Pauses the tween. The fields keep their current values until it plays again.

```lua
local tween = require('haylen.tween')

local boat = {x = 0}
local sailing = tween.to(boat, 10, {x = 1000})
sailing:pause()
print(sailing.paused, sailing.playing) -- true false
```

### handle:resume()

Continues a paused tween in the direction it played.

```lua
local tween = require('haylen.tween')
local timer = require('haylen.timer')

local boat = {x = 0}
local sailing = tween.to(boat, 10, {x = 1000}):pause()
timer.after(1, function() sailing:resume() end)
```

### handle:restart()

Goes back to the start, waits the delay again and plays forward.

```lua
local tween = require('haylen.tween')

local flash = {alpha = 0}
local pulse = tween.to(flash, 0.2, {alpha = 1}, {autoKill = false})

local function hit()
    pulse:restart()
end

hit()
```

### handle:reverse()

Plays backwards from the current time and completes at the start.

```lua
local tween = require('haylen.tween')
local timer = require('haylen.timer')

local drawer = {y = 0}
local opening = tween.to(drawer, 0.4, {y = 120}, {autoKill = false})

timer.after(1, function()
    opening:reverse()
    print(opening.reversed) -- true
end)
```

### handle:seek(position)

Jumps to a time in seconds, or to a label of a timeline, without running callbacks. Seeking a timeline renders every tween it passes, and seeking a tween that repeats forever moves within the current loop.

```lua
local tween = require('haylen.tween')

local bar = {width = 0}
local growing = tween.to(bar, 2, {width = 400}, {paused = true})
growing:seek(0.5)
print(bar.width) -- 100.0
```

### handle:complete(withCallbacks)

Jumps to the end in the direction the tween plays. The argument `withCallbacks`, `true` by default, runs the callbacks on the way. A tween that repeats forever does not complete.

```lua
local tween = require('haylen.tween')

local door = {angle = 0}
local opening = tween.to(door, 2, {angle = 90}, {onComplete = function() print('door open') end})

opening:complete()
print(door.angle) -- 90.0
```

### handle:kill()

Stops the tween for good and runs its `onKill`. A tween inside a timeline leaves it. The fields keep their current values.

```lua
local tween = require('haylen.tween')

local spinner = {rotation = 0}
local spinning = tween.rotate(spinner, 1, math.pi / 2, {repeatCount = -1, loopMode = 'incremental', onKill = function() print('stopped') end})
spinning:kill()
print(spinning.alive) -- false
```

### handle:wait()

Returns a Varn promise that resolves with `true` when the tween next completes, or with `false` when it is killed first. A tween that already ended returns a promise that resolves at once. Await it with `:await()` inside `async.spawn` to write animation steps as straight-line code.

```lua
local tween = require('haylen.tween')
local async = require('async')

local chest = {scale = 1, alpha = 1}

async.spawn(function()
    tween.to(chest, 0.2, {scale = 1.3}, {ease = 'backOut'}):wait():await()
    local completed = tween.to(chest, 0.4, {alpha = 0}):wait():await()
    print('chest opened', completed)
end)
```

### Properties

| Property | Access | Meaning |
| --- | --- | --- |
| `alive` | read | The value is `true` until the tween is killed or leaves the engine after completing. |
| `playing` | read | The value is `true` while the tween is active, not paused and not completed. |
| `paused` | read | The value is `true` while the tween is paused. |
| `reversed` | read | The value is `true` while the tween plays backwards. |
| `completed` | read | The value is `true` once the tween reached its end in the direction it plays. |
| `progress` | read and write | How far the tween is through all its loops, from 0 to 1, or through the current loop when it repeats forever. Writing it seeks. |
| `time` | read and write | The current time in seconds, without the delay. Writing it seeks. |
| `timeScale` | read and write | The speed of the tween. |
| `duration` | read | The length of one loop. A speed-based tween knows it once it starts. |
| `totalDuration` | read | The length of all loops with the pauses between them, `math.huge` for a tween that repeats forever. |
| `delay` | read | The delay before the tween starts. |
| `tag` | read | The tag of the tween. |

```lua
local tween = require('haylen.tween')

local ring = {radius = 0}
local growing = tween.to(ring, 1, {radius = 50}, {delay = 0.25, repeatCount = 1, loopMode = 'yoyo', autoKill = false})
print(growing.duration, growing.totalDuration, growing.delay) -- 1.0 2.0 0.25

growing.timeScale = 2
growing.progress = 0.25
print(growing.time, ring.radius) -- 0.5 25.0
```

## Timeline

A timeline is a [`Tween`](#tween) with every method and property above, plus the methods that place items in it. Each method returns the timeline, so calls chain. Items are tween or timeline handles, numbers of seconds to wait and functions to call.

### timeline:append(item)

Places the item at the end, as a new step. A number appends a pause of that many seconds, and a function is called when the timeline reaches it.

```lua
local tween = require('haylen.tween')

local ball = {y = 0}
tween.timeline()
    :append(tween.to(ball, 0.4, {y = 300}, {ease = 'quadIn'}))
    :append(0.1)
    :append(function() print('bounce') end)
    :append(tween.to(ball, 0.4, {y = 0}, {ease = 'quadOut'}))
```

### timeline:join(item)

Places the item in parallel with the last step, starting when it starts. A function is called at that time.

```lua
local tween = require('haylen.tween')
local m = require('haylen.math')

local logo = {scale = 0.5, color = m.color('#00FFFFFF')}
tween.timeline()
    :append(tween.to(logo, 0.6, {scale = 1}, {ease = 'backOut'}))
    :join(tween.fade(logo, 0.6, 1))
    :join(function() print('logo appears') end)
```

### timeline:insert(position, item)

Places the item at a time in seconds or at a label, without starting a new step. An unknown label raises `The timeline has no label named "<name>".`

```lua
local tween = require('haylen.tween')

local sky = {brightness = 1}
local sun = {y = 0}
tween.timeline()
    :append(tween.to(sun, 4, {y = 600}))
    :addLabel('dusk', 3)
    :insert('dusk', tween.to(sky, 1, {brightness = 0.2}))
    :insert(2, function() print('the shadows grow') end)
```

### timeline:addLabel(name, seconds)

Names a time for `insert` and `seek`, the current end of the timeline when `seconds` is left out.

```lua
local tween = require('haylen.tween')

local hero = {x = 0}
local intro = tween.timeline({autoKill = false, paused = true})
    :append(tween.to(hero, 1, {x = 100}))
    :addLabel('fight')
    :append(tween.to(hero, 1, {x = 300}))

-- Skipping the intro jumps straight to the fight.
intro:seek('fight')
print(hero.x) -- 100.0
```

### timeline.size

Read-only number of items in the timeline, pauses and calls included.

```lua
local tween = require('haylen.tween')

local box = {x = 0}
local line = tween.timeline():append(tween.to(box, 1, {x = 10})):append(0.5):append(function() end)
print(line.size, line.duration) -- 3 1.5
```
