# 📖 `haylen.particles2d`

The module `haylen.particles2d` simulates and draws particle effects: emitters, systems of several emitters, trails and `.particles` effect files. The [particles guide](../2d/particles.md) explains how particles are born, move and look, how effects fit together, the library of ready effects and what particles cost.

```lua
local particles2d = require('haylen.particles2d')
```

## Functions

### particles2d.newEmitter(options)

Creates a [`ParticleEmitter`](#particleemitter) from a table of [emitter options](#emitter-options). The emitter starts emitting at once, after its delay.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local sparkle = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 30, startSize = 4, endSize = 0, blend = 'additive', seed = 3})
```

### particles2d.newEmitter(effect, overrides)

Creates a [`ParticleEmitter`](#particleemitter) from a loaded [`ParticleEffect`](#particleeffect) of one emitter. The optional table `overrides` takes the keys of the [emitter options](#emitter-options) and replaces only the values it names, so the draw order and every other value it leaves out come from the effect. A composite effect raises `The particle effect holds several emitters, so create it with "particles2d.newSystem".`

```lua
local assets = require('haylen.assets')
local particles2d = require('haylen.particles2d')

local sparks = assets.load('effects/sparks.particles')
local blue = particles2d.newEmitter(sparks, {colors = {'#FFA0E0FF', '#002080FF'}, seed = 7})
```

### particles2d.newSystem(effect, options)

Creates a [`ParticleSystem`](#particlesystem) from a loaded [`ParticleEffect`](#particleeffect). It makes one emitter for each entry of a composite effect, or one emitter for an effect of one emitter. The optional table `options` takes `seed`, the seed of the first emitter, from which the seeds of the others count up. Any other key raises `Unknown option "<key>".`

```lua
local assets = require('haylen.assets')
local particles2d = require('haylen.particles2d')

local explosion = particles2d.newSystem(assets.load('effects/explosion.particles'), {seed = 2})
explosion.position = {400, 300}
```

### particles2d.newTrail(options)

Creates a [`Trail`](#trail), a ribbon that follows a moving point. The table `options` takes these keys, and any other key raises `Unknown option "<key>".`:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `lifetime` | number | `0.4` | The seconds each point of the ribbon lives, which sets how long the ribbon is behind a moving point. |
| `minDistance` | number | `4` | The distance the position moves before the trail adds a point. |
| `maxPoints` | integer | `64` | The most points the trail keeps, from 2 to 4096. The oldest goes when a new one needs room. |
| `widthStart`, `widthEnd` | number | `12`, `0` | The width at the head and at the tail. |
| `colors` | table | `{'#FFFFFFFF', '#00FFFFFF'}` | Colors spread evenly from the head to the tail. |
| `texture` | Texture | white | An image stretched along the ribbon from the head, across its width. |
| `layer`, `depth`, `sortOffset`, `visibility`, `blend`, `material`, `emission`, `unshaded`, `lightMask` | | | The draw order of the ribbon, as in [`graphics2d.draw`](graphics2d.md#draw-order). |

The options are checked, and these errors can come up: `A trail needs a positive, finite lifetime and a minimum distance of at least 0.`, `A trail keeps from 2 to 4096 points.` and `A trail needs widths of at least 0 and at least one color.`

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local particles2d = require('haylen.particles2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local slash = particles2d.newTrail({lifetime = 0.25, widthStart = 24, colors = {'#FFFFFFFF', '#00A0E0FF'}, blend = 'additive'})

scene.push({
    update = function(self, dt)
        slash.x, slash.y = camera:screenToWorld(input.mousePosition())
        slash:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        slash:draw()
    end,
})
```

## Emitter options

The function `particles2d.newEmitter`, its overrides table and `emitter:configure` take these keys. Only `particles2d.newEmitter` takes `seed`. A range is a number, or a pair `{min, max}` from which each particle picks its own value.

### Emission

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `rate` | number | `20` | Particles spawned per second while emitting, a finite number of at least 0. Fractions carry over between frames. |
| `rateOverDistance` | number | `0` | Particles spawned per unit the emitter moves, added to the rate, so fast trails leave no gaps. The particles of the rate and of the distance spread along the path the emitter moved in the frame. |
| `bursts` | table | none | A list of `{time = seconds, count = n or {min, max}, cycles = 1, interval = 0, probability = 1}`. A burst fires when the emission cycle reaches its time and repeats `cycles` times, `interval` seconds apart. Each time it picks a count in its range and fires with its probability. |
| `delay` | number | `0` | Seconds before the emission cycle starts, so the parts of a group start in sequence. |
| `duration` | number | `0` | The length of the emission cycle in seconds, a finite number of at least 0. With 0 the emitter emits until the app stops it, and each burst fires once. |
| `loop` | boolean | `false` | Starts the cycle again when it ends. It needs a duration of at least 0.001 seconds. A frame longer than the cycle runs every loop it spans and fires the average count of the bursts of each. Without `loop`, the emitter stops emitting at the end of the cycle. |
| `prewarm` | number | `0` | Seconds simulated on the first update, from 0 to 60, so effects such as smoke start fully grown. |
| `maxParticles` | integer | `256` | The most live particles, from 1 to 1000000. New particles are skipped while the emitter is full. |
| `seed` | integer | `0` | The seed of the random numbers of the emitter. The same seed and options always give the same particles. Only `particles2d.newEmitter` takes it. |

### Spawn area

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `shape` | string | `'point'` | The spawn area: `'point'`, `'circle'`, `'ring'`, `'rectangle'`, `'cone'`, `'ellipse'`, `'rectangleEdge'`, `'arc'`, `'polygon'`, `'polyline'` or `'image'`. |
| `shapeSize` | Vec2 | `{0, 0}` | The size of the spawn area. The shapes `circle`, `ring` and `arc` use `x` as the radius, and `circle` spawns with an even density. The shapes `rectangle` and `rectangleEdge` use `x` and `y` as half the width and half the height, inside or on the outline. The shape `ellipse` uses both as its radii. The shape `cone` spawns inside the circular sector of radius `x` that `direction` and `spread` describe. The shape `image` scales the image to this size, or keeps one unit per pixel when it is zero. |
| `shapeAngle` | number | `0` | Turns the spawn area, in radians. |
| `shapeArc` | range | `{0, 6.2832}` | The start and end angle of the `arc` shape. |
| `shapePoints` | table | none | A list of `Vec2` points. The `polygon` shape spawns inside them and the `polyline` shape along them. The method `emitter:setShapePoints` replaces them. |
| `shapeThickness` | number | `0` | Spreads the outline shapes `ring`, `arc`, `rectangleEdge` and `polyline` across this width. |
| `shapeImage` | ImageShape | none | The visible pixels of an image for the `image` shape, from `assets.load(path, 'imageShape', options)`. |
| `colorFromImage` | boolean | `false` | Tints every particle of the `image` shape with the color of its pixel. |
| `localSpace` | boolean | `false` | Keeps live particles relative to the emitter, so they move with it. |

### Motion

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `lifetime` | range | `1` | Seconds each particle lives. |
| `speed` | range | `{50, 100}` | The starting speed in units per second. |
| `speedCurve` | curve | none | Eases the speed from its full value at birth to 0 at the end of the life, as `sizeCurve` eases the size, such as `'expoOut'` for a puff that stops abruptly or `'quadIn'` for dust that slows down late. The particle keeps facing its motion, `stretch` follows the speed it has, and `false` removes the curve. |
| `direction` | number | `-1.5708` | The emission angle in radians. The default points up, `0` points right and `1.5708` down. |
| `directionMode` | string | `'fixed'` | The value `'fixed'` uses `direction`, `'outward'` sends particles away from the center of the spawn area, `'inward'` toward it and `'tangent'` along it, each with `spread` around that angle. |
| `spread` | number | `0.5` | The width of the emission cone in radians, centered on the direction. The value `6.2832` emits in every direction. |
| `inheritVelocity` | number | `0` | The share of the velocity of the moving emitter that new particles take. Negative values send them backwards. |
| `gravity` | Vec2 | `{0, 0}` | A constant acceleration in units per second squared. |
| `radialAcceleration` | range | `0` | An acceleration away from the emitter, or toward it when negative. |
| `tangentialAcceleration` | range | `0` | An acceleration around the emitter, clockwise on the screen. |
| `damping` | number | `0` | The loss of velocity. Each second the velocity is multiplied by `e` raised to `-damping`. |
| `turbulence` | table | `{strength = 0, frequency = 0.01, speed = 0.5}` | Noise that pushes particles around: `strength` in units per second squared, `frequency`, the scale of the noise in the world, and `speed`, how fast it changes. |
| `attractors` | table | none | A list of `{x, y, strength, radius = 100, killRadius = 0, space = 'emitter'}`. Each pulls the particles inside its radius toward its position, harder the closer they get, and removes those within the kill radius. The position is relative to the emitter, or a point of the world with `space = 'world'`. |
| `collision` | table | `{type = 'none'}` | Makes particles hit something, as [collision](#collision) describes. |
| `bounds` | Rect | none | A rectangle relative to the emitter that particles stay in. The value `false` removes it. |
| `boundsMode` | string | `'kill'` | Particles that leave the bounds die with `'kill'` and come back on the other side with `'wrap'`. |

### Look

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | required | The particle image. |
| `frames` | table | none | The source `Rect` values of the frames. Without frames or a frame grid, each particle shows the whole texture. |
| `frameGrid` | table | none | `{columns, rows, count = 0}` cuts the texture into equal frames, row by row, of which the first `count` play, all of them when it is 0. An emitter takes frames or a frame grid, not both. |
| `frameMode` | string | `'overLife'` | The frames play once across the life with `'overLife'`, loop at the frame rate with `'loop'`, loop from a random frame with `'loopRandomStart'`, or stay on one random frame for the whole life with `'random'`. |
| `frameRate` | number | `10` | Frames per second of the looping modes. |
| `startSize` | range | `16` | The size when a particle is born. |
| `endSize` | range | `16` | The size when a particle dies. |
| `endSizeScale` | range | none | Picks the end size of each particle as a multiple of its own start size instead of `endSize`, so `1` keeps the size of every particle while sizes still vary between them. The value `false` removes it. |
| `sizeCurve` | curve | `'linear'` | How the size moves from the start size to the end size, such as `{points = {0, 1.2, 1}}` for a pop that overshoots. |
| `aspect` | range | `1` | The width divided by the height of each particle. |
| `stretch` | number | `0` | Lengthens each particle along its width by its speed times this value. |
| `rotation` | range | `0` | The rotation at birth in radians, such as `{0, 6.2832}` for random orientations. |
| `rotationStep` | number | `0` | Snaps every drawn rotation to multiples of this angle, such as `1.5708` for quarter turns. |
| `tilt` | number | `0` | Lays the particles on a plane through the emitter that leans away from the viewer by this angle in radians, from 0 to 1.5708, as [effects on the ground](../2d/particles.md#effects-on-the-ground) explains. Their positions, trails and lights come nearer to the emitter along the height of the screen by the cosine of the angle. |
| `alignToVelocity` | boolean | `false` | Adds the angle of the motion to the rotation, so textures that point right follow their motion. A vertical texture takes a `rotation` of `-1.5708`. |
| `spin` | range | `0` | The rotation speed in radians per second. |
| `spinCurve` | curve | none | Eases the spin from its full value at birth to 0 at the end of the life, such as `'quadOut'` for a coin that spins down. The value `false` removes it. |
| `colors` | table | `{'#FFFFFFFF'}` | Colors over the lifetime, spread evenly or at `colorTimes`, multiplied with the texture. |
| `colorTimes` | table | none | One time from 0 to 1 for each color, in growing order, which places the color stops, such as `{0, 0.1, 0.8, 1}` for a quick fade in and a late fade out. |
| `colorBlend` | string | `'smooth'` | The colors blend into each other with `'smooth'` and hold until the next stop with `'steps'`. |
| `tints` | table | none | Colors that new particles pick from to multiply their colors. |
| `tintMode` | string | `'random'` | New particles pick a tint at random with `'random'`, in order with `'cycle'`, or by their moment in the emission cycle with `'cycleTime'`, which takes turns every second without a duration. |
| `pixelSnap` | number | `0` | Rounds drawn positions to multiples of this many units. |
| `particleOrder` | string | `'oldestFirst'` | New particles draw on top with `'oldestFirst'` and below the older ones with `'newestFirst'`. |

A curve takes the forms of the `ease` option of [`haylen.tween`](tween.md), except functions, because a curve runs for every particle on worker threads. A function raises `The particle curve "<name>" takes a curve name or a table, not a function, because it runs for every particle.`

### Collision

The `collision` table takes these keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `type` | string | `'none'` | The value `'floor'` stops particles at a horizontal line `y` units below the emitter, `'bounds'` keeps them inside `area`, and `'world'` casts their moves through the shapes of the physics world that `emitter:setCollisionWorld` gives the emitter. |
| `y` | number | `0` | The height of the floor relative to the emitter. |
| `area` | Rect | empty | A rectangle relative to the emitter whose walls the `bounds` type uses. |
| `bounce` | number | `0.5` | The share of the speed into the surface that a bouncing particle keeps. |
| `friction` | number | `0` | The share of the speed along the surface that a hit takes, from 0 to 1. |
| `result` | string | `'bounce'` | Particles that hit bounce with `'bounce'`, stop where they hit with `'stick'`, or die with `'die'`. |
| `lifeLoss` | number | `0` | The share of the lifetime each hit takes, from 0 to 1. |
| `sizeScale` | number | `1` | The share of the size of a particle that touches surfaces. A particle stops at half its size times this scale from what it hits, so `1` rests it on its edge and `0` collides at its center. |

Hits slower than 30 units per second only stop a particle against the surface, so particles that rest on a floor neither bounce nor report hits.

### Sub-emitters

The `subEmitters` key takes a list of sub-emitters. Each spawns the particles of another effect where the particles of this emitter are born, die, hit something or, at a rate, while they live. Every sub-emitter keeps one emitter for all its particles, which updates and draws with its parent. It ignores the rate, bursts, delay, prewarm, light and local space of its effect.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `effect` | ParticleEffect or table | required | The effect of the spawned particles: a loaded effect of one emitter or an options table. |
| `overrides` | table | none | Options applied on top of the effect. |
| `trigger` | string | `'death'` | The moment that spawns: `'birth'`, `'death'`, `'collision'` or `'alive'`. |
| `count` | integer or range | `1` | The particles spawned each time. |
| `rate` | number | `10` | Particles per second for each living particle of the `alive` trigger. They are born at moments spread over each update, where the particle was at each moment. |
| `probability` | number | `1` | The chance of each spawn, from 0 to 1. |
| `inheritVelocity` | number | `0` | The share of the velocity of the particle that the spawned particles take. |
| `inheritColor` | boolean | `false` | Tints the spawned particles with the color the particle had. |

A composite effect in `effect` raises `A sub-emitter needs an effect of one emitter, not a composite effect.`, and any other value raises `A sub-emitter needs a "ParticleEffect" or an options table in "effect".`

### Trails and lights

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `trail` | table | `{length = 0}` | A ribbon behind every particle: `length`, the points it keeps, at most 64, `lifetime = 0.25`, the seconds it spans, `widthStart = 6` and `widthEnd = 0`, `colors = {'#FFFFFFFF', '#00FFFFFF'}` from the particle to the end, multiplied by the color of the particle, and `texture`. The trails draw with the draw order of the emitter. A length of 0 draws no trail. |
| `light` | table | none | A light at the emitter in lit canvases: `type = 'point'`, `offset`, `radius = 160`, `color`, `intensity = 1`, `flicker = {speed, amount}`, which wavers like `lighting2d.flicker`, and `fade`. The `fade` is `'none'`, `'cycle'` to go out when the emitter stops emitting, or `'count'` to scale with the live particles over their peak, so a flash dies with its particles. The value `false` removes it. |
| `particleLights` | table | none | `{radius = 24, intensity = 0.5, max = 16}` draws a small light in the color of up to `max` particles, spread over them and scaled by their alpha, in lit canvases. The value `false` removes them. |

Lights draw only in lit canvases, so the same emitter works in every canvas.

### Draw order

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `layer`, `depth`, `sortOffset`, `visibility`, `blend` | | `0`, `0`, `0`, `1`, `'alpha'` | The draw order of the whole emitter, as in [`graphics2d.draw`](graphics2d.md#draw-order). |
| `material` | Material | none | Shades the particles with a custom shader. Only Lua takes it, because materials are objects of the running app. |
| `emission`, `unshaded`, `lightMask` | | `0`, `false`, `1` | How the particles take light in lit canvases. |
| `distortion` | number | `0` | Above 0, the particles bend the image of their canvas instead of drawing colors, with their coverage times this strength, as the [distortion of `haylen.graphics2d`](graphics2d.md#distortion) explains. Canvases without lighting or post-processing skip them. |

### Errors of emitter options

Unknown keys raise `Unknown option "<key>".`, and an unknown name raises an error that contains `unknown value '<name>'`. A count that is not an integer or a pair raises an argument error that says `a count needs an integer or the pair {min, max}`. The options are checked, and these errors can come up:

- `A particle emitter needs a texture, room for particles and at least one color.`
- `A particle emitter holds at most 1000000 particles.`
- `Particles need a finite, non-negative rate and a positive lifetime range.`
- `Particle durations must be finite and not negative, and prewarm times must be from 0 to 60 seconds.`
- `A looping particle emitter needs a duration of at least 0.001 seconds.`
- `A particle delay and a rate over distance must be finite and not negative.`
- `A particle burst needs at least one cycle, an interval of at least 0, a count range whose minimum is at most its maximum and a probability from 0 to 1.`
- `Particle bursts need a time inside the emission cycle.`, for every firing of the cycles of a burst.
- `Particle turbulence needs a finite strength and speed and a positive frequency.`
- `A particle attractor needs a finite strength and a radius and a kill radius of at least 0.`
- `Particle collision needs a bounce of at least 0, and a friction and a life loss from 0 to 1.`
- `Particle collision needs a finite size scale of at least 0.`
- `Particle collision with bounds needs an area with a positive width and height.`
- `Particle bounds need a positive width and height.`
- `A particle sub-emitter needs an effect, a count range whose minimum is at most its maximum, a finite rate of at least 0 and a probability from 0 to 1.`
- `A particle emitter takes either frames or a frame grid, not both.`
- `A particle frame grid needs at least one column and one row and a count of at most its cells.`
- `A particle frame rate must be positive and finite.`
- `Particle color times need one time from 0 to 1 for each color, in growing order.`
- `A particle end size scale, stretch, rotation step and pixel snap must be at least 0, and an aspect must be positive.`
- `A particle tilt must be from 0 to 1.5708 radians.`
- `A particle trail holds at most 64 points, a million points across all the particles of the emitter, and needs a positive lifetime, widths of at least 0 and at least one color.`
- `A particle light needs a positive radius, an intensity and a flicker speed of at least 0 and a flicker amount from 0 to 1.`
- `Particle lights need a positive radius, an intensity of at least 0 and at most 1024 lights.`
- `The polygon shape of a particle emitter needs at least three points that enclose an area.`
- `The polyline shape of a particle emitter needs at least two points.`
- `The image shape of a particle emitter needs a shape image.`
- `The shape thickness of a particle emitter must be finite and not negative.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local particles2d = require('haylen.particles2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local leaves = particles2d.newEmitter({
    texture = assets.texture('effects/leaf-sheet.png', {filter = 'linear'}),
    frameGrid = {columns = 4, rows = 1},
    frameMode = 'loopRandomStart',
    frameRate = 6,
    rate = 3,
    prewarm = 10,
    lifetime = {6, 9},
    speed = {40, 80},
    direction = 1.5708,
    gravity = {20, 10},
    turbulence = {strength = 60, frequency = 0.006, speed = 0.5},
    startSize = {18, 26},
    endSizeScale = 1,
    rotation = {0, 6.2832},
    spin = {-1.5, 1.5},
    colors = {'#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'},
    colorTimes = {0, 0.9, 1},
    tints = {'#FFE89A3C', '#FFD0603C', '#FFE8C850'},
    shape = 'rectangle',
    shapeSize = {900, 10},
    layer = 3,
})

scene.push({
    update = function(self, dt)
        leaves:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        leaves:draw()
    end,
})
```

## Effect files

A `.particles` file is a JSON object with the [emitter options](#emitter-options). Load it with [`haylen.assets`](assets.md), which maps the extension to the `particles` asset type and returns a [`ParticleEffect`](#particleeffect). The file names the `filter` and `wrap` of its textures, `'nearest'` and `'clamp'` by default. The optional third argument of `assets.load` and `assets.loadAsync` replaces the keys it names, as in `assets.load('effects/sparks.particles', nil, {filter = 'nearest'})`. The textures share the texture cache with `assets.texture`.

```lua
local assets = require('haylen.assets')
local particles2d = require('haylen.particles2d')

local sparks = assets.load('effects/sparks.particles')
print(sparks.texturePath, sparks.config.rate, sparks.config.lifetime[1], sparks.config.lifetime[2])
local slow = sparks.config
slow.speed = {slow.speed[1] / 2, slow.speed[2] / 2}
local emitter = particles2d.newEmitter(slow)
```

### Effect file format

The file takes the keys of the emitter options, except `seed` and `material`, with JSON values, plus `filter` and `wrap` for its textures:

| Key | JSON value |
| --- | --- |
| `texture` | The required path of the particle image, relative to the effect file. |
| `filter`, `wrap` | The names of the texture options, as in `assets.texture`. |
| `frames` | A list of `[x, y, width, height]` rectangles. |
| `bursts` | A list of `{"time", "count", "cycles", "interval", "probability"}` objects, where `count` is an integer or a `[min, max]` pair. |
| `lifetime`, `speed`, `radialAcceleration`, `tangentialAcceleration`, `startSize`, `endSize`, `endSizeScale`, `aspect`, `rotation`, `spin`, `shapeArc` | A number, or a `[min, max]` pair. |
| `gravity`, `shapeSize` | An `[x, y]` pair. |
| `bounds` | An `[x, y, width, height]` rectangle. |
| `shapePoints` | A list of `[x, y]` points. |
| `colors`, `tints` | A list of `"#AARRGGBB"` or `"#RRGGBB"` strings. |
| `colorTimes` | A list of numbers. |
| `sizeCurve`, `speedCurve`, `spinCurve` | A curve name such as `"quadOut"`, or an object such as `{"curve": "backOut", "overshoot": 3}`, `{"curve": "elasticOut", "amplitude": 1.5, "period": 0.4}`, `{"steps": 4, "position": "end"}`, `{"cubicBezier": [0.2, 0, 0.4, 1]}` or `{"points": [0, 1.2, 1]}`. |
| `frameGrid`, `turbulence`, `collision`, `trail`, `light`, `particleLights` | Objects with the keys of the options table, where colors are strings, vectors are pairs and `collision.area` is a rectangle. The `texture` of a trail is a path relative to the effect file. |
| `attractors` | A list of objects with the keys of the options table. |
| `subEmitters` | A list of objects whose `effect` is the path of another effect file relative to this one, with `overrides`, an object of emitter keys applied on top of it, and the other keys of the options table. |
| `shapeImage` | The path of an image relative to the effect file, or `{"path", "source", "alphaThreshold"}`, which the effect reads into an image shape while it loads. |
| `shape`, `frameMode`, `directionMode`, `colorBlend`, `tintMode`, `boundsMode`, `particleOrder`, `blend` | Names, as in the options table. |
| `rate`, `rateOverDistance`, `delay`, `duration`, `prewarm`, `frameRate`, `direction`, `spread`, `inheritVelocity`, `damping`, `stretch`, `rotationStep`, `tilt`, `shapeAngle`, `shapeThickness`, `pixelSnap`, `depth`, `sortOffset`, `emission`, `distortion` | Numbers. |
| `maxParticles`, `layer`, `visibility`, `lightMask` | Integers. |
| `loop`, `localSpace`, `alignToVelocity`, `colorFromImage`, `unshaded` | Booleans. |

```json
{
    "texture": "../images/spark.png",
    "filter": "linear",
    "frameGrid": {"columns": 4, "rows": 1},
    "frameMode": "loopRandomStart",
    "rate": 0,
    "bursts": [{"time": 0, "count": [20, 28]}],
    "duration": 0.05,
    "maxParticles": 64,
    "lifetime": [0.4, 0.8],
    "speed": [300, 600],
    "direction": -1.5708,
    "spread": 6.2832,
    "gravity": [0, 900],
    "damping": 1.5,
    "startSize": [6, 10],
    "endSize": 0,
    "alignToVelocity": true,
    "stretch": 0.02,
    "colors": ["#FFFFF0A0", "#FFFF8020", "#00FF2000"],
    "colorTimes": [0, 0.2, 1],
    "collision": {"type": "floor", "y": 80, "bounce": 0.3},
    "subEmitters": [{"effect": "../dust/puff.particles", "trigger": "collision", "probability": 0.3}],
    "light": {"radius": 200, "color": "#FFFFC060", "fade": "count"},
    "blend": "additive",
    "layer": 5
}
```

### Composite effect files

A file whose only key is `emitters` describes a composite effect: a list of emitters that `particles2d.newSystem` creates together and draws in the order of the list. Each entry is another effect file, with `"effect": "path"` and optional `overrides`, or the emitter options themselves, next to these keys:

| Key | JSON value | Meaning |
| --- | --- | --- |
| `name` | String | The name that `system:emitter(name)` finds the emitter by. |
| `offset` | `[x, y]` | The position relative to the system, turned and scaled with it. |
| `scale` | Number | The scale of the emitter relative to the system, `1` by default. |
| `delay` | Number | Seconds added to the delay of the emitter, so parts start in sequence. |
| `layerOffset` | Integer | Added to the layer of the emitter. |
| `overrides` | Object | Emitter keys applied on top of the effect file the entry names. |

```json
{
    "emitters": [
        {"name": "smoke", "effect": "../smoke/explosion-smoke.particles", "delay": 0.05},
        {"name": "fireball", "effect": "fireball.particles"},
        {"name": "ring", "effect": "shockwave.particles", "overrides": {"colors": ["#FFFF30C0", "#0040F0FF"]}},
        {"name": "flash", "effect": "flash.particles", "scale": 0.5}
    ]
}
```

### Errors of effect files

Loading a file can raise these errors, besides the [errors of emitter options](#errors-of-emitter-options):

- `The particle effect "<path>" has the unknown option "<name>".`
- `The particle effect value "<name>" has the unknown option "<key>".`, for a key of a nested object.
- `The particle effect value "<name>" needs a number.`, and the same for the other kinds of value.
- `The particle effect value "<name>" must be "first", "second" or "third", not "<value>".`, for a name that is not in its list.
- `The particle effect value "<name>" names the unknown curve "<value>".`
- `The particle color "<text>" is not a "#RRGGBB" or "#AARRGGBB" color.`
- `The particle effect value "<name>" needs an integer of at least 0.`, for a count.
- `The particle effect "<path>" needs a texture.`
- `The particle effect "<path>" refers to itself through "<path>".`, for effects that name each other in a loop.
- `The particle effect "<path>" refers to the composite effect "<path>", where it needs an effect of one emitter.`

## ParticleEmitter

A `ParticleEmitter` spawns, moves and draws particles. It comes from `particles2d.newEmitter`, or from `system:emitter` and `system:emitters`. Two emitter values compare equal with `==` when they are the same emitter.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | read and write | The position of the emitter in world coordinates. |
| `position` | Vec2 | read and write | The same position as a vector. Reading returns a copy. |
| `scale` | number | read and write | The scale of the particles it spawns from now on: their spawn area, speeds, sizes, gravity and accelerations. |
| `rotation` | number | read and write | Turns the direction, the spawn area and the particles it spawns from now on, in radians. |
| `emitting` | boolean | read and write | Whether the emitter spawns particles. Live particles keep moving when it is `false`. |
| `count` | integer | read | The number of live particles. |
| `alive` | boolean | read | The value is `true` while the emitter emits, waits for its delay, or it or its sub-emitters have live particles. Remove a finished one-shot effect when it turns `false`. |
| `cycleTime` | number | read | The seconds that have passed in the current emission cycle. |
| `config` | table | read | The current configuration as an emitter options table without `seed`. |

The table of `config` is a copy, so change the emitter with `emitter:configure`. It goes back into `particles2d.newEmitter` as it is. Its ranges and counts are `{min, max}` pairs, and its curves are in the forms the options take. Its `gravity`, `shapeSize` and points are `Vec2` values, its `colors` are `Color` values, its `frames` and `bounds` are `Rect` values, and the `effect` of each sub-emitter is its options table.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local sparkle = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 30, speed = {40, 80}, blend = 'additive'})
local config = sparkle.config
print(config.rate, config.speed[1], config.speed[2], config.blend, config.maxParticles, sparkle.cycleTime)
local twin = particles2d.newEmitter(config)
```

### emitter:update(dt)

Runs the prewarm time on the first update after creation or `restart`. Then it moves every particle by `dt` seconds, makes them hit what they collide with, removes the ones that expired, spawns new ones and updates the emitters of the sub-emitters. Large emitters simulate on worker threads.

### emitter:draw()

Draws every live particle in the active canvas as one batch, with the draw order of the emitter. Then it draws their trails, their lights in lit canvases and the particles of the sub-emitters.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local particles2d = require('haylen.particles2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local fire = particles2d.newEmitter({
    texture = graphics.whiteTexture(),
    rate = 60,
    lifetime = {0.4, 0.8},
    speed = {60, 120},
    startSize = 10,
    endSize = 2,
    colors = {'#FFFFE080', '#FFFF6020', '#00FF0000'},
    blend = 'additive',
    light = {radius = 180, color = '#FFFFB060', flicker = {speed = 8, amount = 0.2}},
})

scene.push({
    update = function(self, dt)
        fire.x = fire.x + 30 * dt
        fire:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF202030'})
        fire:draw()
    end,
})
```

### emitter:positions()

Returns a list with the position of every live particle as a `Vec2`. Positions are in world coordinates, or relative to the emitter position when `localSpace` is `true`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local particles2d = require('haylen.particles2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local embers = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 10, startSize = 4, endSize = 1, colors = {'#FFFFA040'}})

scene.push({
    update = function(self, dt)
        embers:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF202030'})
        embers:draw()
        for _, position in ipairs(embers:positions()) do
            graphics2d.drawLight({x = position.x, y = position.y, radius = 24, color = '#FFFFA040', intensity = 0.6})
        end
    end,
})
```

### emitter:readPositions(buffer, first)

Copies the `x` and `y` of every live particle into a [float buffer](collections.md#float-buffers), two values for each particle from the value at `first`, which defaults to 1. It returns how many particles fit. Unlike `positions`, it creates no `Vec2` for each particle, so it suits large emitters read in every frame. A `first` outside the buffer raises an argument error that says `the first position is outside the buffer`.

```lua
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local sparks = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 0, maxParticles = 1000})
sparks:burst(400)
local positions = collections.newFloatBuffer(2 * 1000)
local count = sparks:readPositions(positions)
for index = 1, count do
    local x, y = positions:get(index * 2 - 1, 2)
end
print(count) -- 400
```

### emitter:particle(index)

Returns the live particle at `index`, from 1 to `count` in the order the particles were born. The table has `x` and `y` in world coordinates, the velocity `vx` and `vy` it moves with, which the `speedCurve` scales, and `age` and `lifetime` in seconds. It also has the sprite the particle draws as in this frame: `width`, `height`, `rotation`, `color` and `source`. An index outside the particles raises an argument error that says `the emitter has no live particle at this index`.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local smoke = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 0, startSize = 10, endSize = 40, colors = {'#FFFFFFFF', '#00FFFFFF'}})
smoke:burst(1)
smoke:update(0.5)
local puff = smoke:particle(1)
print(puff.width, puff.color.a, puff.age / puff.lifetime)
```

### emitter:burst(count)

Spawns `count` particles at once, up to `maxParticles`, so a huge count simply fills the emitter.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local hit = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 0, spread = 6.2832, lifetime = 0.3, startSize = 6, endSize = 0})
hit.position = {400, 300}
hit:burst(16)
```

### emitter:configure(options)

Changes the configuration. Only the keys present in `options` change, with the same keys, checks and errors as `particles2d.newEmitter`. Live particles stay, and so do the live particles of the sub-emitters that keep their effect. The key `seed` raises `Unknown option "seed".`, because the random numbers of a running emitter keep going.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local rain = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 50, direction = 1.5708, spread = 0.1, speed = 400})
rain:configure({rate = 200, colors = {'#C0A0C0FF'}})
```

### emitter:setAttractor(index, x, y)

Moves the attractor at `index`, from 1 to the number of attractors, in the space it was given in. An index outside the attractors raises an argument error that says `the emitter has no attractor at this index`.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local coins = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 0, lifetime = 2, speed = {200, 400}, spread = 6.2832, damping = 2, attractors = {{strength = 2400, radius = 4000, killRadius = 16, space = 'world'}}})
coins:setAttractor(1, 1700, 60)
coins:burst(10)
```

### emitter:setShapePoints(points)

Replaces the points of the `polygon` and `polyline` shapes with a list of `Vec2`, such as the path of a bolt that changes in every frame. The points are checked as the options are.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local bolt = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 200, lifetime = 0.2, speed = {20, 80}, spread = 6.2832, shape = 'polyline', shapePoints = {{0, 0}, {1, 0}}, blend = 'additive'})
bolt:setShapePoints({{0, -300}, {40, -200}, {-20, -100}, {30, 0}})
```

### emitter:setCollisionWorld(world, filter)

Gives the `world` collision type the shapes of a physics world of [`haylen.physics2d`](physics2d.md) that the optional filter `{category, mask}` lets through, as the queries of a world take it. The value `nil` removes the world. Each update casts the move of every particle through the world in one batch, on worker threads for large emitters. The emitter keeps the world alive while it uses it.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ground = world:createBody({type = 'static', x = 0, y = 300})
ground:addBox(2000, 40)
local water = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 200, speed = {600, 700}, direction = -0.6, spread = 0.06, gravity = {0, 1100}, collision = {type = 'world', result = 'die'}})
water:setCollisionWorld(world)
```

### emitter:clear()

Removes every live particle, also those of the sub-emitters. Emission goes on.

### emitter:restart()

Removes every live particle, rewinds the emission cycle, arms the delay, the bursts and the prewarm again and turns `emitting` back on.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local particles2d = require('haylen.particles2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local explosion = particles2d.newEmitter(assets.load('effects/sparks.particles'))

scene.push({
    update = function(self, dt)
        explosion:update(dt)
        if not explosion.alive then
            explosion.position = {math.random(-300, 300), math.random(-200, 200)}
            explosion:restart()
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        explosion:draw()
    end,
    exit = function(self)
        explosion:clear()
    end,
})
```

## ParticleEffect

A `ParticleEffect` is a loaded `.particles` file, the template of new emitters and systems. Two effect values compare equal with `==` when they are the same effect.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `texturePath` | string or nil | read | The path of the texture of the effect inside the content folder, or `nil` for a composite effect. |
| `config` | table or nil | read | The effect as an emitter options table, in the form of `emitter.config`, or `nil` for a composite effect. |
| `composite` | boolean | read | The value is `true` for a file of several emitters. |
| `parts` | table | read | The emitters of a composite effect as `{name, offset, scale, texturePath, config}`, empty for an effect of one emitter. |

```lua
local assets = require('haylen.assets')

local explosion = assets.load('effects/explosion.particles')
print(explosion.composite)
for _, part in ipairs(explosion.parts) do
    print(part.name, part.offset.x, part.offset.y, part.scale, part.config.delay)
end
```

## ParticleSystem

A `ParticleSystem` holds the emitters of an effect, which update, draw, move, scale, turn and restart together. It comes from `particles2d.newSystem`. Every emitter sits at its offset from the position of the system, turned and scaled with it, and draws in the order the effect lists it.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | read and write | The position of the system in world coordinates. |
| `position` | Vec2 | read and write | The same position as a vector. |
| `scale`, `rotation` | number | read and write | The scale and the rotation that every emitter takes, with its own scale on top. |
| `emitting` | boolean | read and write | Whether any emitter emits. Setting it starts or stops them all. |
| `count` | integer | read | The live particles of every emitter. |
| `alive` | boolean | read | The value is `true` while any emitter is alive. |

### system:update(dt)

Updates every emitter by `dt` seconds, as `emitter:update` does.

### system:draw()

Draws every emitter in the active canvas, in the order of the effect.

### system:restart()

Restarts every emitter, as `emitter:restart` does.

### system:clear()

Removes the live particles of every emitter.

### system:emitter(name)

Returns the `ParticleEmitter` with the `name` of its entry in the composite file, or `nil`.

### system:emitters()

Returns the list of every emitter, in the order of the effect.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local particles2d = require('haylen.particles2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local explosion = particles2d.newSystem(assets.load('effects/explosion.particles'))
local smoke = explosion:emitter('smoke')
print(#explosion:emitters(), smoke and smoke.count)

scene.push({
    update = function(self, dt)
        if input.mousePressed('left') then
            explosion.x, explosion.y = camera:screenToWorld(input.mousePosition())
            explosion.scale = 0.5 + math.random()
            explosion:restart()
        end
        explosion:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        explosion:draw()
    end,
    exit = function(self)
        explosion:clear()
    end,
})
```

## Trail

A `Trail` is a ribbon from `particles2d.newTrail` that follows its position. It adds a point whenever its position moves `minDistance` away from the last point and drops the points older than its lifetime. While it emits, its head follows the position.

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | read and write | The point the trail follows. |
| `position` | Vec2 | read and write | The same point as a vector. |
| `emitting` | boolean | read and write | Whether the trail adds points. A trail that stops keeps fading until its last point expires. |
| `count` | integer | read | The points the trail keeps. |
| `alive` | boolean | read | The value is `true` while the trail emits or keeps points. |
| `config` | table | read | The options of the trail as a table that `particles2d.newTrail` takes. |

### trail:update(dt)

Ages the points by `dt` seconds and adds a point when the position moved far enough.

### trail:draw()

Draws the ribbon in the active canvas as a stroke with round ends whose edges fade over one pixel.

### trail:clear()

Drops every point.

### trail:configure(options)

Changes only the options that `options` names, with the keys, checks and errors of `particles2d.newTrail`.

```lua
local graphics2d = require('haylen.graphics2d')
local particles2d = require('haylen.particles2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local comet = particles2d.newTrail({lifetime = 0.6, widthStart = 30, colors = {'#FFFFF0C0', '#80FF9030', '#00FF2000'}, blend = 'additive'})
local time = 0

scene.push({
    update = function(self, dt)
        time = time + dt
        comet.position = {math.cos(time) * 400, math.sin(time * 2) * 200}
        comet:update(dt)
        if time > 10 and comet.emitting then
            comet.emitting = false
            comet:configure({widthStart = 12})
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        comet:draw()
    end,
    exit = function(self)
        comet:clear()
    end,
})
```

## ImageShape

An `ImageShape` holds the visible pixels of an image, which the `image` shape spawns on. Load it from an image file with `assets.load(path, 'imageShape', options)`, or decode it on a worker thread with `assets.loadAsync`. Two values compare equal with `==` when they are the same shape. The table `options` takes:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `source` | Rect | the whole image | The `{x, y, width, height}` part of the image to read. |
| `alphaThreshold` | number | `0.5` | The alpha, from above 0 to 1, that a pixel needs to count. |

An image without visible pixels raises `An image shape needs at least one pixel whose alpha reaches the threshold.`, and a source outside the image raises `The source of an image shape must lie inside the image.`

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `count` | integer | read | The number of visible pixels. |
| `width`, `height` | integer | read | The size of the source. |

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local hero = assets.load('sprites/hero.png', 'imageShape', {source = {0, 0, 64, 64}})
print(hero.count, hero.width, hero.height)
local dissolve = particles2d.newEmitter({
    texture = graphics.whiteTexture(), rate = 0, lifetime = {0.5, 1}, speed = {20, 80}, spread = 0.6, gravity = {0, -200},
    startSize = 4, endSize = 1, shape = 'image', shapeImage = hero, shapeSize = {128, 128}, colorFromImage = true, blend = 'additive',
})
dissolve:burst(400)
```
