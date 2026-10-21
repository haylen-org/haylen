# 📖 `haylen.tween`

The module `haylen.tween` changes values smoothly over time. A tween moves fields of tables and properties of engine objects from where they are to new values, along an easing curve. It can repeat, play back and forth, run backwards, jump to any time and be awaited. Ready-made tweens move, scale, rotate, fade, tint, jump, follow paths and Bézier curves, blink, shake and punch. Timelines play tweens, pauses and calls at their places in time, and staggers start the same tween on many targets one after another. Use tweens for UI motion, camera moves, fades, small effects on hits and pickups, and scripted scenes. The [lifecycle guide](../core/lifecycle.md) explains the [pause and the process modes](../core/lifecycle.md#pause-and-process-modes) and the [owners](../core/lifecycle.md#subscription-scopes) that tweens follow.

```lua
local tween = require('haylen.tween')
```

## How tweens run

- The engine advances tweens once per frame, after timers and before the scene update. A tween with `fixedStep = true` advances with each fixed step of physics instead. The [lifecycle guide](../core/lifecycle.md#start-frames-and-stop) shows the whole frame.
- A tween counts scaled time by default, so [`haylen.setTimeScale`](haylen.md) slows it down. With `unscaled = true` it counts real time.
- Its process mode decides whether it plays while the game is paused.
- A tween first waits for its `delay`. Then it reads its start values, so a delayed tween starts from wherever the fields are at that moment. Only [`tween.from`](#tweenfromtarget-seconds-values-options) sets its values at once.
- Every frame, the tween writes the new values. It sets fields of tables and objects by plain assignment, and it writes the [native properties](#native-properties) of engine objects in C++ without running any Lua.
- A tween holds its target weakly. When the garbage collector frees the target, or the engine object behind it is released, the tween kills itself before it writes again.
- An error raised by a callback, or by a target that refuses an assignment, stops the app and shows the error screen, as [errors](../core/lua.md#errors-and-stack-traces) describes.
- Updating thousands of tweens allocates nothing.

## Native properties

A native property is a number, `Vec2` or `Color` property of an engine type that C++ reads and writes directly. A tween writes native properties without calling Lua, so thousands of them cost no script time. Sprites, cameras, lights, particle emitters, the nodes of [`haylen.nodes2d`](nodes2d.md), audio effects, the values of [`haylen.math`](math.md) such as `Vec2` and `Color`, and the transforms of UI nodes have them, among others.

A native field may reach one level into a value, such as `'position.x'` of a `Vec2` property or `'color.a'` of a `Color` property. A pair of fields, such as the `x` and `y` that `tween.move` animates, is native when both are number properties. A field that is not a native property goes through the Lua accessors of the object, like a field of a table. So a tween may even change the `timeScale` of another tween.

The transform that [`gui:transform(id)`](ui.md#guitransformid) returns moves, scales, fades and tints a node of a GUI. The GUI keeps it alive while the node exists, so a tween may target it directly.

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

A tween animates four kinds of values. It finds the kind from the current value of the field, and it reads the end value as the same kind.

| Kind | End values it accepts | How it travels |
| --- | --- | --- |
| Number | A number. | In a straight line. |
| `Vec2` | A `Vec2`, `{x = 10, y = 20}` or `{10, 20}`. | In a straight line. |
| `Color` | A `Color`, a text `'#RRGGBB'` or `'#AARRGGBB'`, or a table with `r`, `g`, `b` and an optional `a`. | Channel by channel, or through the hue with `colorSpace = 'hsv'`. |
| Text | A string. | The end text replaces the start text one character at a time, like a typewriter. |

A field name may be a path with dots. It reaches into nested tables, such as `'stats.hp'`, and into the parts of a value, such as `'position.x'` or `'color.a'`. A `Vec2` has the parts `x` and `y`, and a `Color` has `r`, `g`, `b` and `a`. These values are copies in Lua, so the tween writes a changed copy back to the field that holds the value. A variable that held the old value keeps it. One tween animates every field that its table of values names.

The tween writes numbers as Lua floats, so a counter reads `1500.0`. Format it with `%d` to show `1500`.

These options change how values travel.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `angles` | list of field names | none | Number fields in radians that take the shorter way around the circle, so 350 degrees go to 10 degrees through 360. |
| `integers` | list of field names | none | Number fields that stay whole, such as score counters. |
| `colorSpace` | string | `'rgb'` | The value `'rgb'` blends colors channel by channel, and `'hsv'` blends them through hue, saturation and value. |

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
    print(string.format('Score %d.', hud.score), hud.banner, hud.offset.y)
end)
```

## Options

Every tween and timeline takes an optional `options` table as its last argument. An unknown key raises `Unknown option "<key>".`. A value of the wrong type raises `The option "<key>" of "<function>" is invalid: <reason>.`, such as `The option "loopMode" of "to" is invalid: unknown value 'bounce'.`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `delay` | number | `0` | Seconds to wait before the tween starts. Times such as `time` and `duration` leave it out. A negative delay raises `A tween delay cannot be negative.` |
| `repeatCount` | integer | `0` | Extra loops after the first one. A negative count repeats forever. |
| `loopMode` | string | `'restart'` | How each loop plays. The mode `'restart'` plays every loop from the start, `'yoyo'` plays every other loop backwards and `'incremental'` continues every loop from where the one before ended. A text starts over on every loop. |
| `repeatDelay` | number | `0` | Seconds to wait between loops. A negative value raises `A tween repeat delay cannot be negative.` |
| `timeScale` | number | `1` | The speed of this tween, which multiplies the time scale of its tag. A negative value or one that is not finite raises `A tween time scale must be a finite number of 0 or more.` |
| `tag` | string | `''` | A group name for the [tag functions](#groups), such as `tween.killTag` and `tween.setTimeScale`. |
| `owner` | table or userdata | none | An object, such as a scene, whose end kills the tween, as [subscription scopes](../core/lifecycle.md#subscription-scopes) describe. An owner that is not a table or a userdata raises `An owner must be a table or a userdata, not <type>.` |
| `processMode` | string | `'inherit'` | When the tween plays while the game is paused: `'inherit'`, `'pausable'`, `'whenPaused'`, `'always'` or `'disabled'`. The default `'inherit'` follows the mode of the owner every frame, and without an owner it counts as `'pausable'`. The [lifecycle guide](../core/lifecycle.md#pause-and-process-modes) explains each mode. |
| `reducedMotion` | string | `'keep'` | What the tween does while the system asks for [reduced motion](accessibility.md). The value `'keep'` plays it as it is, `'shorten'` plays it four times faster and `'skip'` completes it at once with its callbacks. Under `'skip'`, a tween that repeats forever holds still until the system no longer asks for reduced motion. |
| `unscaled` | boolean | `false` | Counts real time and ignores the time scale of the app. |
| `fixedStep` | boolean | `false` | Advances with the fixed steps of physics instead of once per frame. |
| `autoKill` | boolean | `true` | Makes the tween leave the engine when it completes. Turn it off to replay, reverse or seek a finished tween. |
| `paused` | boolean | `false` | Creates the tween paused, so it waits for `play`. |

Without an `owner`, a tween that a module creates at its top level in development belongs to that module, so a [reload](hotReload.md) of the module kills it.

The callbacks are options too. Each one is a function, and another value raises `The tween option "<name>" must be a function.`.

| Option | Arguments | Called when |
| --- | --- | --- |
| `onStart` | none | The tween starts playing forward after its delay. It runs again after `restart`. |
| `onUpdate` | `progress` (number) | An update moved the tween. A tween of values passes its eased progress through the current loop, which the `back` and `elastic` curves may push below `0` or above `1`. A timeline passes the plain progress of its current loop. |
| `onLoop` | `loop` (integer) | A new loop begins. The number is `1` when the first repeat begins, then `2`, and so on. An update that crosses more than 256 loops reports only the last 256. |
| `onStep` | `step` (integer) | Only for timelines. A step of the timeline ends while it plays forward, counted from `1`. |
| `onComplete` | none | The tween reaches its end in the direction it plays: the end when it plays forward, and the start when it plays backwards. A tween that repeats forever never completes. |
| `onKill` | none | The tween leaves the engine, once: it was killed, its target or its owner ended, or it completed with `autoKill`. |

A tween lets go of its callbacks and of its easing function once it leaves the engine. So a callback that refers to the handle of its tween never keeps it alive, such as `local t; t = tween.to(box, 1, {x = 5}, {onComplete = function() print(t.time) end})`. A tween with an owner holds its callbacks and its easing function through the owner, so they may refer to the owner without keeping it alive. When the garbage collector frees such an owner, its tweens end at the end of that frame without their callbacks, which went away with the owner.

```lua
local tween = require('haylen.tween')

local lamp = {glow = 0}
tween.to(lamp, 0.5, {glow = 1}, {
    delay = 0.2,
    repeatCount = 3,
    loopMode = 'yoyo',
    repeatDelay = 0.1,
    tag = 'lamps',
    onStart = function() print('The lamp starts to glow.') end,
    onLoop = function(loop) print('Loop ' .. loop .. ' begins.') end,
    onComplete = function() print('The lamp is dark again.') end,
})
```

The tweens of values, which are [`tween.to`](#tweentotarget-seconds-values-options), `tween.from`, `tween.by`, `tween.fromTo` and the [ready-made tweens](#ready-made-tweens), also take these options. Timelines do not.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `ease` | curve | `'linear'` | The easing curve, as [Easing](#easing) describes. |
| `overwrite` | boolean | `false` | Makes this tween the only one that animates its fields. Other tweens, also inside timelines, give up the same fields of the same targets, and a tween left with nothing to animate is killed. |
| `speedBased` | boolean | `false` | Reads `seconds` as units per second. The tween finds its duration when it starts, from the longest distance that one of its values travels. |
| `angles`, `integers`, `colorSpace` | | | As [Values](#values) describes. |

A speed-based tween measures numbers and `Vec2` values in units, colors by the largest change of a channel from `0` to `1`, texts in characters, and paths and Bézier curves by their length. A tween whose values go nowhere ends at once. A speed that is not positive raises `A speed-based tween needs a positive speed.`. A speed-based tween cannot join a timeline, because its duration is only known when it starts.

## Easing

The `ease` option takes a curve in one of these forms, the same forms that [`m.ease`](math.md#measecurve-t) of `haylen.math` accepts.

| Form | Example | Curve |
| --- | --- | --- |
| Name | `'quadOut'` | The name `'linear'`, or the `In`, `Out` and `InOut` variants of `sine`, `quad`, `cubic`, `quart`, `quint`, `expo`, `circ`, `back`, `elastic` and `bounce`. A table `{curve = 'quadOut'}` names the same curve. |
| Back | `{curve = 'backOut', overshoot = 3}` | A `back` curve with its overshoot, `1.70158` by default. |
| Elastic | `{curve = 'elasticOut', amplitude = 1.5, period = 0.4}` | An `elastic` curve with its amplitude and period, `1` and `0.3` by default. |
| Steps | `{steps = 4, position = 'end'}` | Jumps in equal steps, like CSS `steps()`. The `position` is `'start'`, `'end'` (the default), `'both'` or `'none'`. |
| Bézier | `{cubicBezier = {0.25, 0.1, 0.25, 1}}` | A cubic Bézier curve `{x1, y1, x2, y2}`, like CSS `cubic-bezier()`, with both x values between `0` and `1`. |
| Points | `{points = {0, 1.2, 1}}` | Straight lines through numbers spread evenly over the progress, or through `{x, y}` points whose x grows from one point to the next. |
| Function | `function(t) return t * t end` | Any function that takes the progress from `0` to `1` and returns the eased progress. |
| `EasingCurve` | `emitter.config.sizeCurve` | A curve that a function of the engine returned. |

A curve that is wrong raises `The option "ease" of "<function>" is invalid: <reason>.`, with one of the reasons that `m.ease` lists, such as `The option "ease" of "to" is invalid: unknown value 'bouncy'.`. An easing function that returns anything but a number stops the app with `An easing function must return a number.` when the tween plays.

```lua
local tween = require('haylen.tween')

local card = {x = 0, y = 0, scale = 1}

tween.to(card, 0.6, {x = 400}, {ease = {curve = 'backOut', overshoot = 3}})
tween.to(card, 0.6, {y = 200}, {ease = {cubicBezier = {0.68, -0.6, 0.32, 1.6}}})
tween.to(card, 0.6, {scale = 2}, {ease = function(t) return math.sin(t * math.pi / 2) end})
```

## Functions

### tween.to(target, seconds, values, options)

Animates the fields of `target` from their current values to the values of `values` over `seconds`, and returns a [`Tween`](#tween). The `target` is a table or a userdata, such as an engine object. The table `values` maps field names to end values, as [Values](#values) describes. The table `options` is optional and takes the keys of [Options](#options).

A target of another type raises `A tween target must be a table or a userdata, not <type>.`, and a `seconds` that is not above `0` raises `A tween needs a positive duration.`. A key of `values` that is not a string raises `bad argument #3 to 'to' (tween fields are named by strings)`. A missing field, or a field of another type, raises `Cannot tween the field "<name>" because it is not a number, a "Vec2", a "Color" or a text.`.

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

Animates the fields of `target` from the values of `values` back to the values they have now. The fields take the values of `values` at once, even while the delay runs, so the target never shows its end values first. It takes the same arguments and raises the same errors as [`tween.to`](#tweentotarget-seconds-values-options).

```lua
local tween = require('haylen.tween')

local title = {y = 200, alpha = 1}
tween.from(title, 0.8, {y = -150, alpha = 0}, {ease = 'bounceOut', delay = 0.2})
print(title.y, title.alpha) -- -150.0 0.0
```

### tween.by(target, seconds, offsets, options)

Animates the fields of `target` by the offsets of `offsets`, from their current values to those values plus the offsets. With `loopMode = 'incremental'`, every loop moves by the offsets again. A text cannot move by an offset, so a text field raises `A text cannot be tweened by an offset.` when the tween starts.

```lua
local tween = require('haylen.tween')

local conveyor = {x = 0}
tween.by(conveyor, 0.5, {x = 64}, {repeatCount = 3, loopMode = 'incremental'})

require('haylen.timer').after(2.1, function() print(conveyor.x) end) -- 256.0
```

### tween.fromTo(target, seconds, from, to, options)

Animates the fields of `target` from the values of `from` to the values of `to`, whatever the fields hold before. Every field of `from` needs an end value in `to`, or the call raises `A "tween.fromTo" call needs an end value for the field "<name>".`. A field that only `to` names is left alone.

```lua
local tween = require('haylen.tween')

local meter = {fill = 0.8}
tween.fromTo(meter, 1, {fill = 0}, {fill = 1}, {ease = 'quadOut'})
```

## Ready-made tweens

A ready-made tween takes the target, the seconds, one value of its own and the options, and returns a [`Tween`](#tween). Each one animates default fields, such as `x` and `y` for a position. The option `field` names other fields: one field name, such as `'position'` for a `Vec2` field, or a list of two number fields that act as one `Vec2`, such as `{'left', 'top'}`. Another `field` raises `The tween option "field" must be a field name or a list of two number field names.`.

Every ready-made tween takes the keys of [Options](#options), the options of the tweens of values and the options of its own motion. Its value follows [Values](#values), so a value of the wrong kind raises an error that names it, such as `The tween value of "alpha" must be a number.`. A jump, a path or a Bézier curve needs a `Vec2` field or a pair of number fields, or it raises `A jump tween needs a "Vec2" field or a pair of number fields.`, with `path` or `Bézier` in place of `jump`.

### tween.move(target, seconds, position, options)

Moves the fields `x` and `y` to `position`: a `Vec2`, `{x = 10, y = 20}` or `{10, 20}`.

```lua
local tween = require('haylen.tween')

local coin = {x = 100, y = 500}
tween.move(coin, 0.6, {400, 300}, {ease = 'quadOut'})
```

### tween.scale(target, seconds, scale, options)

Scales the fields `scaleX` and `scaleY` to `scale`. One number scales both axes, and a `Vec2` or a pair scales each axis on its own. On a single number field, `scale` is a number.

```lua
local tween = require('haylen.tween')
local m = require('haylen.math')

local coin = {scaleX = 1, scaleY = 1}
tween.scale(coin, 0.3, 1.5, {ease = 'backOut'})

local banner = {size = m.vec2(1, 1)}
tween.scale(banner, 0.3, {2, 1}, {field = 'size'})
```

### tween.rotate(target, seconds, angle, options)

Turns the field `rotation` to `angle` in radians. It always takes the shorter way around the circle.

```lua
local tween = require('haylen.tween')

local wheel = {rotation = 0}
tween.rotate(wheel, 0.6, math.pi)
```

### tween.fade(target, seconds, alpha, options)

Fades the field `color.a`, the alpha of the color, to `alpha`.

```lua
local tween = require('haylen.tween')
local m = require('haylen.math')

local ghost = {color = m.color('#FFFFFFFF')}
tween.fade(ghost, 0.3, 0, {delay = 1.2})

local hint = {alpha = 1}
tween.fade(hint, 0.5, 0, {field = 'alpha'})
```

### tween.tint(target, seconds, color, options)

Changes the field `color` to `color`: a `Color`, a text `'#RRGGBB'` or `'#AARRGGBB'`, or a color table.

```lua
local tween = require('haylen.tween')
local m = require('haylen.math')

local gem = {color = m.color('#FFFFFFFF')}
tween.tint(gem, 0.6, '#FFFFD166', {colorSpace = 'hsv'})
```

### tween.jump(target, seconds, position, options)

Moves the fields `x` and `y` to `position` in hops, and lands on it.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `power` | number | `100` | The height of each hop, toward negative y, which is up on screen. |
| `jumps` | integer | `1` | The number of hops on the way. A count below `1` raises `A jump needs at least one hop.` |

```lua
local tween = require('haylen.tween')

local frog = {x = 100, y = 500}
tween.jump(frog, 0.6, {400, 500}, {power = 120, jumps = 2})
```

### tween.path(target, seconds, points, options)

Moves the fields `x` and `y` through the list `points`, starting where the target is. It travels at a constant speed and ends on the last point. An empty list raises `A path needs at least one point.`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `curved` | boolean | `true` | Passes smoothly through every point on a Catmull-Rom spline, or in straight lines when `false`. |
| `closed` | boolean | `false` | Travels back to the start after the last point and ends there. |
| `orient` | boolean | `false` | Turns a rotation field, in radians, toward where the path heads. |
| `orientField` | string | `'rotation'` | The field that `orient` turns. |

```lua
local tween = require('haylen.tween')
local m = require('haylen.math')

local boat = {position = m.vec2(0, 0), rotation = 0}
tween.path(boat, 4, {{200, 0}, {200, 200}, {0, 200}}, {field = 'position', closed = true, orient = true, repeatCount = -1})
```

### tween.bezier(target, seconds, points, options)

Moves the fields `x` and `y` along a Bézier curve. The list `points` holds one or two control points followed by the end point. One control point makes a quadratic curve, and two make a cubic one. Another number of points raises `A Bézier tween needs one or two control points followed by the end point.`.

```lua
local tween = require('haylen.tween')

local comet = {x = 0, y = 0}
tween.bezier(comet, 1.5, {{300, -200}, {600, 400}, {900, 0}})
```

### tween.blink(target, seconds, count, options)

Switches the field `color.a` between its current value and a hidden value `count` times, and ends on the current value. A count below `1` raises `A blink needs at least one blink.`, and a count that is not an integer raises `bad argument #3 to 'blink' (number has no integer representation)`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `hidden` | value | `0`, or transparent for a color | The value shown between blinks. A `Vec2` or a text field needs it, or the call raises `A blink of a "Vec2" or a text needs the "hidden" option.` |

```lua
local tween = require('haylen.tween')
local m = require('haylen.math')

local portal = {color = m.color('#FF66CCFF')}
tween.blink(portal, 1, 4, {field = 'color', hidden = '#0066CCFF'})
```

### tween.shake(target, seconds, strength, options)

Shakes the fields `x` and `y` around their current values with a strength that fades out, and ends where they started. The `strength` is how far it shakes: one number for both axes, or a pair for each axis. On a number field, it is a number.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `vibrato` | integer | `10` | How many times it swings. A value below `1` raises `A shake needs a vibrato of at least 1.` |
| `randomness` | number | `90` | How far each shake bends at random, in degrees from `0` to `180`. A `Vec2` swings about opposite to the shake before it, bent by up to this angle, and a number makes some swings shorter. Values outside the range stop at its ends. |
| `seed` | integer | random | A number from `0` to `4294967295` that makes the pattern repeat the same way every time. |

```lua
local tween = require('haylen.tween')
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
tween.shake(camera, 0.4, 12, {field = 'position', vibrato = 20, seed = 7})
```

### tween.punch(target, seconds, offset, options)

Springs the fields `x` and `y` toward `offset` and back, swinging less each time, and ends where they started. The `offset` is a `Vec2` or a pair, or a number on a number field.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `vibrato` | integer | `10` | How many times it swings. A value below `1` raises `A punch needs a vibrato of at least 1.` |
| `elasticity` | number | `1` | How far it swings past the start on the way back, from `0` to `1`. Values outside the range stop at its ends. |

```lua
local tween = require('haylen.tween')

local button = {offsetY = 0}
tween.punch(button, 0.5, -16, {field = 'offsetY', vibrato = 6, elasticity = 0.5})
```

## Timelines and staggers

### tween.timeline(options)

Creates an empty [`Timeline`](#timeline) and returns it. A timeline is a tween that plays other tweens, pauses and calls at their places in time. It repeats, yoyos, seeks and reverses like any tween, and everything inside it moves with it. The table `options` is optional and takes the keys of [Options](#options) and `onStep`, but not the options of the tweens of values.

Add the items in the same frame that creates the timeline. A timeline that is still empty when the engine next advances it has nothing to play, so it completes at once.

```lua
local tween = require('haylen.tween')

local door = {angle = 0}
local hero = {x = 0, alpha = 0}

local cutscene = tween.timeline({
    onStep = function(step) print('Step ' .. step .. ' ended.') end,
    onComplete = function() print('The cutscene is over.') end,
})
cutscene:append(tween.to(door, 0.5, {angle = 90}))
    :append(tween.to(hero, 1, {x = 300}))
    :join(tween.to(hero, 0.3, {alpha = 1}))
    :append(0.25)
    :append(function() print('The hero waves.') end)
```

### tween.stagger(targets, seconds, make, options)

Builds one tween for each element of the sequence `targets` with `make(target, index)`, which returns that tween. Then it plays them in a new [`Timeline`](#timeline) as one step, each one `seconds` after the one before, and returns the timeline. The table `options` is optional and takes the keys of [`tween.timeline`](#tweentimelineoptions) and `origin`, which picks the order.

| Origin | Order |
| --- | --- |
| `'start'` | The default. Starts with the first target. |
| `'end'` | Starts with the last target. |
| `'center'` | Starts from the middle and goes outwards, so targets at the same distance from the middle start together. |

An empty sequence raises `A stagger needs at least one tween.`. A `make` that returns anything but a tween or a timeline raises `bad argument #-1 to 'stagger' (haylen.Tween expected, got <type>)`.

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

These functions act on many tweens at once. A tag puts tweens in a group, which the tag functions control together. Tags reach only tweens that play on their own, since a tween inside a timeline follows its timeline. Tweens without a tag share the empty tag `''`, so `tween.killTag('')` kills them all.

### tween.killTag(tag)

Kills every tween with the tag, which runs their `onKill`. The fields keep their current values.

```lua
local tween = require('haylen.tween')

local clouds = {x = 0}
tween.to(clouds, 30, {x = 1920}, {repeatCount = -1, tag = 'world'})
tween.killTag('world')
print(tween.size()) -- 0
```

### tween.completeTag(tag, withCallbacks)

Jumps every tween with the tag to its end. The argument `withCallbacks`, `true` when left out, runs the callbacks on the way, such as `onComplete`. Tweens that repeat forever do not complete.

```lua
local tween = require('haylen.tween')

local menu = {x = 0, alpha = 1}
tween.to(menu, 0.5, {x = 300}, {tag = 'menu'})
tween.to(menu, 0.5, {alpha = 0}, {tag = 'menu', onComplete = function() print('The menu is hidden.') end})

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

Scales the time of every tween with the tag, also of tweens created later with that tag. The scale multiplies the own `timeScale` of each tween. A scale that is negative or not finite raises `A tween time scale must be a finite number of 0 or more.`.

```lua
local tween = require('haylen.tween')

local enemy = {x = 0}
tween.to(enemy, 2, {x = 500}, {tag = 'enemies'})

-- Bullet time for the enemies only.
tween.setTimeScale('enemies', 0.25)
```

### tween.timeScale(tag)

Returns the time scale of the tag, which is `1` unless `tween.setTimeScale` changed it.

```lua
local tween = require('haylen.tween')

tween.setTimeScale('ui', 2)
print(tween.timeScale('ui'), tween.timeScale('world')) -- 2.0 1.0
```

### tween.killTarget(target)

Kills every tween that animates `target`, also tweens inside timelines. A tween that animates other targets too is killed as a whole.

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

Kills every tween at once, which runs their `onKill`. A timeline kills the tweens inside it too. The fields keep their current values, and every pending `wait()` promise resolves with `false`.

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

Returns the number of tweens that the engine plays. It counts delayed and paused tweens, and finished tweens without `autoKill`. A timeline counts as one, whatever it holds.

```lua
local tween = require('haylen.tween')

local coin = {y = 0}
tween.to(coin, 0.3, {y = -40}, {loopMode = 'yoyo', repeatCount = 1})

if tween.size() > 0 then
    print('Animations are still playing.')
end
```

## Tween

A `Tween`, named `haylen.Tween` in messages, is the handle that every tween function returns. Dropping the handle does not stop the tween. The control methods return the handle, so calls chain, such as `handle:restart():pause()`. Reading a member that it does not have raises `The type "haylen.Tween" has no member "<name>".`, and writing a property that is read-only raises `The type "haylen.Tween" has no writable property "<name>".`.

### handle:play()

Plays forward from the current time, also after `pause` or `reverse`. A tween that already reached its end stays there, so use `restart` to play it again.

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

Goes back to the start without running callbacks, waits the delay again and plays forward. It does nothing on a tween that left the engine, so a tween to replay needs `autoKill = false`.

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

Plays backwards from the current time and completes at the start. It does not wait for what is left of the delay.

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

Jumps to `position`: a time in seconds, or the name of a label on a [`Timeline`](#timeline). It runs no callbacks, and the tween no longer waits for its delay. A negative time counts as `0`, and a time past the end stops at the end. Seeking a timeline renders every tween it passes. Seeking a tween that repeats forever moves within the current loop.

A time that is NaN raises `A tween cannot seek to a time that is not a number.`. A label on a tween that is not a timeline raises `calling 'seek' on bad self (haylen.Timeline expected, got haylen.Tween)`.

```lua
local tween = require('haylen.tween')

local bar = {width = 0}
local growing = tween.to(bar, 2, {width = 400}, {paused = true})
growing:seek(0.5)
print(bar.width) -- 100.0
```

### handle:complete(withCallbacks)

Jumps to the end in the direction the tween plays, which is the start for a tween that plays backwards. The argument `withCallbacks`, `true` when left out, runs the callbacks on the way, such as `onComplete`, and `false` skips them. A tween with `autoKill` then leaves the engine and runs its `onKill` either way. It does nothing on a tween that repeats forever or that left the engine.

```lua
local tween = require('haylen.tween')

local door = {angle = 0}
local opening = tween.to(door, 2, {angle = 90}, {onComplete = function() print('The door is open.') end})

opening:complete()
print(door.angle) -- 90.0
```

### handle:kill()

Stops the tween for good and runs its `onKill`. The fields keep their current values. A tween inside a timeline leaves it, and a timeline kills the tweens inside it. Killing a tween that already left the engine does nothing.

```lua
local tween = require('haylen.tween')

local spinner = {rotation = 0}
local spinning = tween.rotate(spinner, 1, math.pi / 2, {repeatCount = -1, loopMode = 'incremental', onKill = function() print('The spinner stopped.') end})
spinning:kill()
print(spinning.alive) -- false
```

### handle:wait()

Returns a Varn promise that resolves with `true` the next time the tween completes, or with `false` when it is killed first. A tween that already left the engine returns a promise that resolves at once: with `true` when it completed, and with `false` when it was killed before. A tween with `autoKill = false` that already completed waits for its next completion. A tween that its own `onComplete` kills still resolves with `true`. Await the promise with `:await()` inside `async.spawn` to write the steps of an animation as straight-line code.

```lua
local tween = require('haylen.tween')
local async = require('async')

local chest = {scale = 1, alpha = 1}

async.spawn(function()
    tween.to(chest, 0.2, {scale = 1.3}, {ease = 'backOut'}):wait():await()
    local completed = tween.to(chest, 0.4, {alpha = 0}):wait():await()
    print(completed and 'The chest opened.' or 'The chest animation stopped.')
end)
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `alive` | boolean | read | It is `true` until the tween is killed, or leaves the engine after it completes. |
| `playing` | boolean | read | It is `true` while the tween is alive, not paused and not completed. |
| `paused` | boolean | read | It is `true` while the tween is paused. |
| `reversed` | boolean | read | It is `true` while the tween plays backwards. |
| `completed` | boolean | read | It is `true` once the tween reached its end in the direction it plays. It turns `false` again when the tween moves away from that end. |
| `progress` | number | read and write | How far the tween is through all its loops, from `0` to `1`, or through the current loop when it repeats forever. Writing it seeks there, and a value outside `0` to `1` stops at the nearest end. |
| `time` | number | read and write | The time in seconds through all loops and the pauses between them, without the delay. Writing it seeks, like `seek`. |
| `timeScale` | number | read and write | The speed of the tween. A negative value or one that is not finite raises `A tween time scale must be a finite number of 0 or more.` |
| `duration` | number | read | The length of one loop in seconds. A speed-based tween knows it only once it starts, and reads `0` before. |
| `totalDuration` | number | read | The length of all loops with the pauses between them, or `math.huge` for a tween that repeats forever. |
| `delay` | number | read | The delay before the tween starts. |
| `tag` | string | read | The tag of the tween, `''` without one. |

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

A `Timeline`, named `haylen.Timeline` in messages, is the handle that [`tween.timeline`](#tweentimelineoptions) and [`tween.stagger`](#tweenstaggertargets-seconds-make-options) return. It is a [`Tween`](#tween) too, so everything that takes a tween takes a timeline. It has the methods `timeline:play()`, `timeline:pause()`, `timeline:resume()`, `timeline:restart()`, `timeline:reverse()`, `timeline:seek(position)`, `timeline:complete(withCallbacks)`, `timeline:kill()` and `timeline:wait()`. It has the properties `alive`, `playing`, `paused`, `reversed`, `completed`, `progress`, `time`, `timeScale`, `duration`, `totalDuration`, `delay` and `tag`. They work as they do for a tween, and `duration` is where the last item of the timeline ends. Reading a member that it does not have raises `The type "haylen.Timeline" has no member "<name>".`.

The methods below place items in the timeline and return it, so calls chain. An item is a tween or timeline handle, a number of seconds to wait or a function to call. Each `append` starts a new step at the end of the timeline, and `join` adds to the last step. The callback `onStep` reports each step that ends.

A tween joins a timeline right after it is created, before it starts and while it is in no other timeline. Otherwise the method raises `Only a tween that has not started and is in no other timeline can join a timeline.`. A tween that joins leaves the engine and plays inside the timeline. Its own `delay` places it later in the timeline, and its `timeScale` stretches or shrinks its place. A timeline that would hold itself, or a timeline that holds it, raises `A timeline cannot hold itself.`.

Inside a timeline, a tween follows the playhead of the timeline. It reads its start values when the playhead first reaches it, so it starts from where the tweens before it left the fields. Its delay, loops, `timeScale`, easing and callbacks still apply. Its `paused`, `processMode`, `unscaled`, `fixedStep`, `reducedMotion`, `autoKill` and `tag` no longer matter. A function item runs each time the playhead passes its time, forward or backward, but not when a seek jumps over it.

### timeline:append(item)

Places the item at the end, as a new step. A number appends a pause of that many seconds, which must be above `0` or it raises `A tween needs a positive duration.`. A function is called when the playhead reaches it.

```lua
local tween = require('haylen.tween')

local ball = {y = 0}
tween.timeline()
    :append(tween.to(ball, 0.4, {y = 300}, {ease = 'quadIn'}))
    :append(0.1)
    :append(function() print('The ball bounces.') end)
    :append(tween.to(ball, 0.4, {y = 0}, {ease = 'quadOut'}))
```

### timeline:join(item)

Places the item in parallel with the last step, starting when that step starts. A function is called at that time. It takes a tween, a timeline or a function, and on an empty timeline it works like `append`.

```lua
local tween = require('haylen.tween')
local m = require('haylen.math')

local logo = {scale = 0.5, color = m.color('#00FFFFFF')}
tween.timeline()
    :append(tween.to(logo, 0.6, {scale = 1}, {ease = 'backOut'}))
    :join(tween.fade(logo, 0.6, 1))
    :join(function() print('The logo appears.') end)
```

### timeline:insert(position, item)

Places the item at `position`, a time in seconds or the name of a label, without starting a new step. A negative time counts as `0`. It takes a tween, a timeline or a function. An unknown label raises `The timeline has no label named "<name>".`.

```lua
local tween = require('haylen.tween')

local sky = {brightness = 1}
local sun = {y = 0}
tween.timeline()
    :append(tween.to(sun, 4, {y = 600}))
    :addLabel('dusk', 3)
    :insert('dusk', tween.to(sky, 1, {brightness = 0.2}))
    :insert(2, function() print('The shadows grow.') end)
```

### timeline:addLabel(name, seconds)

Names a time for `insert` and `seek`. Without `seconds`, the label names the current end of the timeline. A negative time counts as `0`, and a name that exists moves its label. An empty name raises `A timeline label needs a name.`.

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

Read-only number of items in the timeline, with its pauses and calls.

```lua
local tween = require('haylen.tween')

local box = {x = 0}
local line = tween.timeline():append(tween.to(box, 1, {x = 10})):append(0.5):append(function() end)
print(line.size, line.duration) -- 3 1.5
```

## Errors

These messages come from the functions and methods when they are called.

| Message | Cause |
| --- | --- |
| `bad argument #<n> to '<function>' (<type> expected, got <type>)` | An argument has the wrong type, such as a `seconds` that is not a number or a table of values that is not a table. |
| `A tween target must be a table or a userdata, not <type>.` | The target of a tween function is another type. |
| `A tween needs a positive duration.` | The `seconds` of a tween, or a pause of a timeline, is not above `0`. |
| `A speed-based tween needs a positive speed.` | The `seconds` of a tween with `speedBased = true` is not above `0`. |
| `bad argument #3 to 'to' (tween fields are named by strings)` | A key of the table of values is not a string. |
| `Cannot tween the field "<name>" because it is not a number, a "Vec2", a "Color" or a text.` | The field is missing or holds another type. |
| `Cannot tween the field "<name>" because part of its path is <type>, not a table or an object.` | A step of the path of the field is not a table or a userdata. |
| `The tween field "<name>" has an empty part.` | The name of the field has two dots in a row, or starts or ends with a dot. |
| `Cannot tween the field "<name>" as part of a "Vec2" because it is not a number.` | One field of a pair of fields is not a number. |
| `The type "<type>" has no member "<name>".` | The path of a field reaches a member that an engine object does not have, such as `'zoom.z'`. |
| `Cannot tween an object that was already released.` | The engine object behind the target was already released. |
| `The tween value of "<name>" must be a number.` | The end value of a number field is another type. |
| `The tween value of "<name>" must be a text.` | The end value of a text field is another type. |
| `bad argument #<n> to '<function>' (Vec2 or table with x and y expected, got <type>)` | The end value of a `Vec2` field is another type. |
| `bad argument #<n> to '<function>' (Color, color text or table with r, g and b expected, got <type>)` | The end value of a `Color` field is another type. |
| `bad argument #<n> to '<function>' (invalid color text, expected #RRGGBB or #AARRGGBB)` | A color text has another form. |
| `Expected a number in field "<name>".` | A table for a `Vec2` or a `Color` lacks a number, such as `"y"`. |
| `A "tween.fromTo" call needs an end value for the field "<name>".` | A field of `from` has no value in `to`. |
| `Unknown option "<key>".` | An options table has a key that the function does not take. |
| `Option tables only accept string keys.` | An options table has a key that is not a string. |
| `The option "<key>" of "<function>" is invalid: <reason>.` | An option has a value of the wrong type, names no value of its list, or is a wrong curve. |
| `The tween option "<name>" must be a function.` | A callback option is not a function. |
| `The tween option "angles" must be a list of field names.` | The option `angles` or `integers` is not a table. |
| `The tween option "colorSpace" must be "rgb" or "hsv".` | The option `colorSpace` names another space. |
| `The tween option "field" must be a field name or a list of two number field names.` | The option `field` of a ready-made tween is another value. |
| `An owner must be a table or a userdata, not <type>.` | The option `owner` is another type. |
| `A tween delay cannot be negative.` | The option `delay` is below `0`. |
| `A tween repeat delay cannot be negative.` | The option `repeatDelay` is below `0`. |
| `A tween time scale must be a finite number of 0 or more.` | The option or property `timeScale`, or the scale of `tween.setTimeScale`, is negative or not finite. |
| `A jump tween needs a "Vec2" field or a pair of number fields.` | A jump, a path or a Bézier curve has another field, with `path` or `Bézier` in place of `jump`. |
| `A path needs at least one point.` | The list of points of `tween.path` is empty. |
| `A Bézier tween needs one or two control points followed by the end point.` | The list of points of `tween.bezier` holds fewer than 2 or more than 3 points. |
| `A blink of a "Vec2" or a text needs the "hidden" option.` | A blink of such a field has no `hidden` value. |
| `A jump needs at least one hop.` | The option `jumps` is below `1`. |
| `A blink needs at least one blink.` | The count of `tween.blink` is below `1`. |
| `A shake needs a vibrato of at least 1.` | The option `vibrato` of `tween.shake` is below `1`. |
| `A punch needs a vibrato of at least 1.` | The option `vibrato` of `tween.punch` is below `1`. |
| `A stagger needs at least one tween.` | The sequence of `tween.stagger` is empty. |
| `bad argument #-1 to 'stagger' (haylen.Tween expected, got <type>)` | The function `make` of `tween.stagger` returned something else than a tween. |
| `bad argument #1 to 'append' (haylen.Tween expected, got <type>)` | An item of `append` or `join` is another type, with the name of that method. The item of `insert` raises the same message with `#2 to 'insert'`. |
| `Only a tween that has not started and is in no other timeline can join a timeline.` | The tween already started, was killed or is in another timeline. |
| `A timeline cannot hold itself.` | A timeline would hold itself or a timeline that holds it. |
| `A speed-based tween cannot join a timeline, because its duration is only known when it starts.` | A tween with `speedBased = true` joined a timeline. |
| `The timeline has no label named "<name>".` | The label of `insert` or `seek` does not exist. |
| `A timeline label needs a name.` | The name of `addLabel` is empty. |
| `A tween cannot seek to a time that is not a number.` | The time of `seek`, `time` or `progress` is NaN. |
| `calling 'seek' on bad self (haylen.Timeline expected, got haylen.Tween)` | The method `seek` got a label on a tween that is not a timeline. |
| `The type "haylen.Tween" has no member "<name>".` | The code read a member that a tween does not have, or `haylen.Timeline` for a timeline. |
| `The type "haylen.Tween" has no writable property "<name>".` | The code wrote a property that is read-only, or `haylen.Timeline` for a timeline. |

These messages stop the app and show the error screen, because they happen while the tween plays.

| Message | Cause |
| --- | --- |
| `A text cannot be tweened by an offset.` | A tween of `tween.by` started on a text field. |
| `A tween can only travel between two values of the same kind.` | A field holds another kind of value when the tween starts than when it was created. |
| `Cannot tween the field "<name>" because a "Vec2" has only "x" and "y".` | The field that holds a part became a `Vec2` without that part. |
| `Cannot tween the field "<name>" because a "Color" has only "r", "g", "b" and "a".` | The field that holds a part became a `Color` without that part. |
| `An easing function must return a number.` | The easing function returned another value. |
