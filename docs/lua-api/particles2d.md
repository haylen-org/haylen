# haylen.particles2d

The module `haylen.particles2d` simulates and draws particle effects such as fire, smoke, sparks, dust and explosions. An emitter spawns particles at a rate or in bursts, moves them with speed, gravity and accelerations, fades their size and color over their lifetime and draws them all in one batch. Emitters are configured from Lua tables or from `.particles` effect files loaded as assets.

```lua
local particles2d = require('haylen.particles2d')
```

## Loading effect files

A `.particles` file is a JSON object with the emitter options described below. Load it with [`haylen.assets`](assets.md), which maps the extension to the `particles` asset type, and pass the result to `particles2d.newEmitter`:

```lua
local assets = require('haylen.assets')
local particles2d = require('haylen.particles2d')

local sparks = assets.load('effects/sparks.particles')
local emitter = particles2d.newEmitter(sparks)
```

The optional third argument of `assets.load` and `assets.loadAsync` sets the options of the effect's texture, `filter` (`'nearest'` or `'linear'`) and `wrap` (`'clamp'`, `'repeat'` or `'mirror'`), as in `assets.load('effects/sparks.particles', nil, {filter = 'linear'})`. The texture shares the texture cache with `assets.texture`. A loaded effect is a `ParticleEffect` value, the template of new emitters, with these read-only properties:

| Property | Type | Meaning |
| --- | --- | --- |
| `texturePath` | string | Path of the effect's texture inside the content folder. |
| `config` | table | The effect as an emitter options table, in the form `emitter.config` describes. |

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

The file uses the keys of the emitter options below, except `seed`, with JSON values:

| Key | JSON value |
| --- | --- |
| `texture` | Required path of the particle image, relative to the effect file. |
| `frames` | List of `[x, y, width, height]` rectangles. |
| `bursts` | List of `{"time": seconds, "count": particles}` objects. |
| `lifetime`, `speed`, `radialAcceleration`, `tangentialAcceleration`, `startSize`, `endSize`, `spin` | A number, or a `[min, max]` pair. |
| `gravity`, `shapeSize` | An `[x, y]` pair. |
| `colors` | List of `"#AARRGGBB"` or `"#RRGGBB"` strings. |
| `shape`, `blend` | Names, as in the options table. |
| `rate`, `duration`, `prewarm`, `direction`, `spread`, `damping`, `depth` | Numbers. |
| `maxParticles`, `layer` | Integers. |
| `loop`, `localSpace` | Booleans. |

Errors while loading: `The particle effect "path" has the unknown option "name".`, `The particle color "text" is not a "#RRGGBB" or "#AARRGGBB" color.`, `The emitter shape of a particle effect must be "point", "circle", "ring", "rectangle" or "cone", not "name".`, `The blend mode of a particle effect must be "alpha", "additive", "multiply", "screen", "premultiplied" or "opaque", not "name".` and, for a `maxParticles` or a burst `count` that is not an integer of at least 0, `The particle effect value "name" needs an integer of at least 0.`, plus the validation errors listed under emitter options.

```json
{
    "texture": "../images/spark.png",
    "frames": [[0, 0, 8, 8], [8, 0, 8, 8]],
    "rate": 0,
    "bursts": [{"time": 0, "count": 24}],
    "duration": 0.5,
    "loop": false,
    "maxParticles": 64,
    "lifetime": [0.4, 0.8],
    "speed": [80, 160],
    "direction": -1.5708,
    "spread": 6.2832,
    "gravity": [0, 300],
    "damping": 0.5,
    "startSize": [6, 10],
    "endSize": 0,
    "spin": [-4, 4],
    "colors": ["#FFFFF0A0", "#FFFF8020", "#00FF2000"],
    "shape": "circle",
    "shapeSize": [6, 0],
    "blend": "additive",
    "layer": 5
}
```

## Functions

### particles2d.newEmitter(options)

Creates a `ParticleEmitter` from an emitter options table. The emitter starts emitting at once.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local sparkle = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 30, startSize = 4, endSize = 0, blend = 'additive', seed = 3})
```

### particles2d.newEmitter(effect, overrides)

Creates a `ParticleEmitter` from a loaded `ParticleEffect`. The optional `overrides` table uses the emitter options keys and replaces only the values it names, so the draw order and every other value it leaves out come from the effect. The emitter starts emitting at once.

```lua
local assets = require('haylen.assets')
local particles2d = require('haylen.particles2d')

local sparks = assets.load('effects/sparks.particles')
local blue = particles2d.newEmitter(sparks, {colors = {'#FFA0E0FF', '#002080FF'}, seed = 7})
```

## Emitter options

The function `particles2d.newEmitter`, its overrides table and `emitter:configure` accept these keys, except `seed`, which only `particles2d.newEmitter` accepts:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | required | Particle image. |
| `frames` | table | none | Source `Rect` values played across each particle's lifetime. Without frames each particle shows the whole texture. |
| `rate` | number | `20` | Particles spawned per second while emitting, a finite number of at least 0. Fractional rates carry over between frames. |
| `bursts` | table | none | List of `{time = seconds, count = particles}`. Each burst spawns its particles when the emission cycle reaches its time. |
| `duration` | number | `0` | Length of the emission cycle in seconds, a finite number of at least 0. 0 emits until the app stops the emitter, and each burst then fires once. |
| `loop` | boolean | `false` | Starts the cycle again when it ends. Needs a duration of at least 0.001 seconds. A frame longer than the cycle runs every loop it spans, firing the bursts of each. Without `loop` the emitter stops emitting at the end of the cycle. |
| `prewarm` | number | `0` | Seconds simulated on the first update, from 0 to 60, so effects such as smoke start fully grown. |
| `maxParticles` | integer | `256` | Upper limit of live particles, from 1 to 1000000. New particles are skipped while the emitter is full, however many a burst or the rate asks for. |
| `lifetime` | range | `1` | Seconds each particle lives. |
| `speed` | range | `{50, 100}` | Initial speed in units per second. |
| `direction` | number | `-1.5708` | Emission angle in radians. The default points up. |
| `spread` | number | `0.5` | Width of the emission cone in radians, centered on `direction`. The value `6.2832` emits in every direction. |
| `gravity` | Vec2 | `{0, 0}` | Constant acceleration in units per second squared. |
| `radialAcceleration` | range | `0` | Acceleration away from the emitter, or toward it when negative. |
| `tangentialAcceleration` | range | `0` | Acceleration around the emitter. |
| `damping` | number | `0` | Velocity loss. Each second the velocity is multiplied by `e` raised to `-damping`. |
| `startSize` | range | `16` | Size when a particle is born. |
| `endSize` | range | `16` | Size when a particle dies. The size moves linearly between both. |
| `spin` | range | `0` | Rotation speed in radians per second. |
| `colors` | table | `{'#FFFFFFFF'}` | Colors spread evenly across the lifetime and blended between. |
| `shape` | string | `'point'` | Spawn area: `'point'`, `'circle'`, `'ring'`, `'rectangle'` or `'cone'`. |
| `shapeSize` | Vec2 | `{0, 0}` | Size of the spawn area. The shapes `circle` and `ring` use `x` as the radius. The shape `rectangle` uses `x` and `y` as half the width and half the height. The shape `cone` spawns inside the circular sector of radius `x` that `direction` and `spread` describe. |
| `localSpace` | boolean | `false` | Keeps live particles relative to the emitter, so they move with it. |
| `seed` | integer | `0` | Seed of the emitter's random numbers. The same seed and configuration always give the same particles. Only `particles2d.newEmitter` accepts it, because it starts the random numbers of a new emitter. |
| `layer`, `depth`, `blend` | | `0`, `0`, `'alpha'` | Draw order of the whole emitter. |

A range is a number or a `{min, max}` pair, and each particle picks its own value inside it. Unknown keys raise `Unknown option "name".`, and an unknown shape raises an error that contains `unknown value 'name'`. The configuration is validated and these errors are raised:

- `A particle emitter needs a texture, room for particles and at least one color.`
- `A particle emitter holds at most 1000000 particles.`
- `Particles need a finite, non-negative rate and a positive lifetime range.`
- `Particle durations must be finite and not negative, and prewarm times must be from 0 to 60 seconds.`
- `A looping particle emitter needs a duration of at least 0.001 seconds.`
- `Particle bursts need a time inside the emission cycle.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local particles2d = require('haylen.particles2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local smoke = particles2d.newEmitter({
    texture = assets.texture('effects/dust.png'),
    frames = {{0, 0, 64, 64}, {64, 0, 64, 64}, {128, 0, 64, 64}, {192, 0, 64, 64}},
    rate = 12,
    prewarm = 2,
    lifetime = {1.5, 2.5},
    speed = {20, 40},
    spread = 0.6,
    gravity = {10, -20},
    startSize = {24, 32},
    endSize = {64, 80},
    spin = {-0.5, 0.5},
    colors = {'#A0FFFFFF', '#00FFFFFF'},
    shape = 'circle',
    shapeSize = {12, 0},
    layer = 3,
})

scene.push({
    update = function(self, dt)
        smoke:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        smoke:draw()
    end,
})
```

## ParticleEmitter

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | read and write | Emitter position in world coordinates. |
| `position` | Vec2 | read and write | The same position as a vector. Reading returns a copy. |
| `emitting` | boolean | read and write | Whether the emitter spawns particles. Live particles keep moving when it is `false`. |
| `count` | integer | read | Number of live particles. |
| `alive` | boolean | read | The value is `true` while the emitter is emitting or still has live particles. Remove finished one-shot effects when it turns `false`. |
| `cycleTime` | number | read | Seconds elapsed in the current emission cycle. |
| `config` | table | read | The current configuration as an emitter options table without `seed`. Ranges are `{min, max}` pairs, `gravity` and `shapeSize` are `Vec2` values, `colors` are `Color` values and `frames` are `Rect` values. The table is a copy, so change the emitter with `emitter:configure`. |

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local sparkle = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 30, speed = {40, 80}, blend = 'additive'})
local config = sparkle.config
print(config.rate, config.speed[1], config.speed[2], config.blend, config.maxParticles)
local twin = particles2d.newEmitter(config)
```

### emitter:update(dt)

Runs the prewarm time on the first update after creation or `restart`, then moves every particle by `dt` seconds, removes the ones that expired and spawns new ones. Large emitters simulate on worker threads.

### emitter:draw()

Draws every live particle in the active canvas as one batch, with the emitter's draw order.

```lua
local assets = require('haylen.assets')
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
})

scene.push({
    update = function(self, dt)
        fire.x = fire.x + 30 * dt
        fire:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        fire:draw()
    end,
})
```

### emitter:positions()

Returns a list with the position of every live particle as a `Vec2`, in world coordinates, or relative to the emitter position when `localSpace` is `true`. Use it for collisions, pickups or lights that follow particles.

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

Copies `x` and `y` of every live particle into a float buffer of [`haylen.collections`](collections.md#float-buffers), two values for each particle from the position `first`, which defaults to 1, and returns how many particles fit. Unlike `positions`, it creates no `Vec2` for each particle, so it suits large emitters read every frame.

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

Changes the configuration. Only the keys present in `options` change, with the same keys, validation and errors as `particles2d.newEmitter`. Live particles stay. The key `seed` raises `Unknown option "seed".` because the random numbers of a running emitter keep going.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local rain = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 50, direction = 1.5708, spread = 0.1, speed = 400})
rain:configure({rate = 200, colors = {'#C0A0C0FF'}})
```

### emitter:clear()

Removes every live particle. Emission continues.

### emitter:restart()

Removes every live particle, rewinds the emission cycle, rearms the bursts and the prewarm and turns `emitting` back on.

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
