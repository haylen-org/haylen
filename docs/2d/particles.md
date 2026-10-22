# 🎆 Particles

This guide explains particle effects, such as fire, smoke, sparks, rain, magic and explosions, made with the module `haylen.particles2d`. It is for developers who add effects to a game or an app. It covers emitters and effect files, how particles are born, move, collide and look, effects made of several emitters, trails, lights, the library of ready effects and what particles cost. The [`haylen.particles2d` reference](../lua-api/particles2d.md) lists every option, function and error.

## Particles and emitters

A particle is a small image that lives for a short time, such as one spark or one puff of smoke. An emitter creates particles, moves them, changes how they look over their life and removes them when their life ends. Hundreds or thousands of particles together make one effect.

You describe an emitter with options: how many particles it spawns, where, how fast they move, what they look like and how long they live. Create the emitter once, then update and draw it in every frame.

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
})

scene.push({
    update = function(self, dt)
        fire:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        fire:draw()
    end,
})
```

Many options take a range: a number, or a pair `{min, max}`. Each particle picks its own value inside the range, so `lifetime = {0.4, 0.8}` gives some particles a short life and some a longer one. This variety makes effects look natural.

## Choosing the parts

| Need | Use |
| --- | --- |
| One stream or burst of particles, such as sparks or rain. | A `ParticleEmitter`, from `particles2d.newEmitter`. |
| An effect made of layers that play together, such as the flash, fireball, smoke and debris of an explosion. | A `ParticleSystem`, from `particles2d.newSystem` and a composite effect file. |
| A ribbon behind one moving point, such as the tip of a blade or a comet, without particles. | A `Trail`, from `particles2d.newTrail`. |
| Particles that follow an object made of parts, such as the exhaust of a ship. | A node of [`haylen.nodes2d`](nodes.md) that shows the particles and places them. |

## Effect files

An effect file, with the extension `.particles`, holds the options of an emitter as JSON. Artists can tune effects in files without touching code, and the same file serves many emitters.

Load a file with `assets.load`, which returns a `ParticleEffect`. Pass it to `particles2d.newEmitter`, with an optional table of overrides that replace some of its options, such as other colors. The file names the `filter` and `wrap` of its textures, and the options of `assets.load` can replace them.

A composite effect file lists several emitters that play together, each with an offset, a scale and a delay. Create it with `particles2d.newSystem`. A system places every emitter at its offset from the position of the system, turned and scaled with the system. It updates and draws them in the order of the file and restarts them together. The delay of each entry starts the parts in sequence, such as the flash, then the fireball, then the smoke of an explosion.

An effect file can name other effect files, as entries of a composite file and as sub-emitters. Loading follows them. A file that names itself, directly or through other files, raises an error instead of loading forever.

```lua
local assets = require('haylen.assets')
local particles2d = require('haylen.particles2d')

local sparks = particles2d.newEmitter(assets.load('effects/sparks.particles'), {colors = {'#FFA0E0FF', '#002080FF'}})
local explosion = particles2d.newSystem(assets.load('effects/explosion.particles'))
explosion.position = {400, 300}
```

## When particles are born

An emitter spawns particles in three ways, which add up:

- The `rate` spawns a steady stream, in particles per second.
- The `rateOverDistance` spawns particles for each unit the emitter moves, so a fast trail leaves no gaps.
- The `bursts` spawn many particles at once at given moments, such as the sparks of a hit.

The emission cycle sets when the emitter is active. The `delay` waits before the cycle starts. The `duration` sets the length of the cycle, or 0 to emit until you stop the emitter. With `loop` the cycle starts again when it ends, and without it the emitter stops emitting at the end. The `prewarm` simulates some seconds on the first update, so smoke starts fully grown.

The `maxParticles` limit keeps an emitter from growing without end. New particles are skipped while the emitter is full. The `seed` makes an emitter repeatable: the same seed and options always give the same particles.

A one-shot effect, such as an explosion, bursts once and ends. Its `alive` property turns `false` once it finished and its last particle died. Then remove the effect or call `restart` to play it again.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local particles2d = require('haylen.particles2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local hit = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 0, bursts = {{time = 0, count = {16, 24}}}, duration = 0.1, spread = 6.2832, lifetime = 0.3, startSize = 6, endSize = 0})

scene.push({
    update = function(self, dt)
        hit:update(dt)
        if not hit.alive then
            hit.position = {math.random(-300, 300), math.random(-200, 200)}
            hit:restart()
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        hit:draw()
    end,
})
```

## Where particles are born

Without a shape, particles are born at one point, the position of the emitter. The `shape` spreads them over an area. It can be a circle, a ring, a rectangle or its outline, a cone, an ellipse, an arc, a polygon, a line through points, or the visible pixels of an image. The `shapeSize`, `shapeAngle` and `shapeThickness` size and turn the area.

The `image` shape takes an `ImageShape`, the visible pixels of an image. With `colorFromImage` each particle takes the color of its pixel, so a sprite breaks apart into particles of its own colors.

By default, live particles stay where they are when the emitter moves, like smoke behind a moving torch. With `localSpace`, they move with the emitter, like the flames of a campfire that a player carries.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local hero = assets.load('sprites/hero.png', 'imageShape')
local dissolve = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 0, shape = 'image', shapeImage = hero, colorFromImage = true, lifetime = {0.5, 1}, speed = {20, 80}, startSize = 4, endSize = 1})
dissolve:burst(400)
local snow = particles2d.newEmitter({texture = graphics.whiteTexture(), shape = 'rectangle', shapeSize = {960, 10}, direction = 1.5708, rate = 40, lifetime = 8, startSize = 4})
```

## How particles move

A particle starts with a `speed` along a direction. The `direction` is an angle in radians, and the `spread` widens it into a cone. The `directionMode` can send particles away from the center of the spawn area, toward it or along it instead.

Forces then change the motion over the life of each particle:

- The `gravity` pulls in one direction, such as down for sparks or up for bubbles.
- The `radialAcceleration` pushes away from the emitter or pulls toward it, and the `tangentialAcceleration` swirls around it.
- The `damping` slows particles down, like air.
- The `turbulence` pushes particles with smooth noise, so smoke curls, snow drifts and fireflies wander.
- The `attractors` pull particles toward points, such as coins that fly to a counter at the top of the screen.
- The `inheritVelocity` gives new particles part of the velocity of a moving emitter, for exhaust and sparks from moving objects.

A curve, such as `speedCurve`, `sizeCurve` or `spinCurve`, changes a value over the life of a particle. A curve takes the name of an easing, such as `'quadOut'`, or a table, as the `ease` option of [`haylen.tween`](../lua-api/tween.md) does. A curve never takes a Lua function. Curves run for every particle on worker threads, and nothing calls Lua for each particle.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local coins = particles2d.newEmitter({
    texture = graphics.whiteTexture(),
    rate = 0,
    lifetime = 2,
    speed = {200, 400},
    spread = 6.2832,
    damping = 2,
    speedCurve = 'quadIn',
    attractors = {{strength = 2400, radius = 4000, killRadius = 16, space = 'world'}},
})
coins:setAttractor(1, 1700, 60)
coins:burst(10)
```

## Collision

Particles can hit things. The `collision` option picks what:

- A `floor` stops particles at a line below the emitter, for sparks that bounce on the ground.
- The `bounds` keep particles inside a box, for confetti in a panel.
- The `world` casts the move of each particle through the shapes of a physics world of [`haylen.physics2d`](physics.md), which `emitter:setCollisionWorld` gives the emitter.

A particle that hits can bounce, stick where it hit or die. Particles touch a surface at their edge, half their size away from their center, so a ball rests on a floor and stays inside a box. The `sizeScale` of the collision changes that distance for a texture whose shape fills less of it. A scale of 0 collides at the center, for sparks and drops.

The separate option `bounds` is a box that particles stay in without hitting it: they die when they leave it, or come back on the other side with `boundsMode = 'wrap'`.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ground = world:createBody({type = 'static', x = 0, y = 300})
ground:addBox(2000, 40)
local water = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 200, speed = {600, 700}, direction = -0.6, spread = 0.06, gravity = {0, 1100}, collision = {type = 'world', result = 'die'}})
water:setCollisionWorld(world)
local sparks = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 30, gravity = {0, 900}, collision = {type = 'floor', y = 80, bounce = 0.4, sizeScale = 0}})
```

## How particles look

Each particle draws as one sprite of the texture of its emitter. These options shape it:

- The `frames` or the `frameGrid` cut the texture into frames. The `frameMode` plays them over the life of a particle, loops them, or keeps one random frame for the whole life, which suits debris.
- The `startSize`, `endSize` and `sizeCurve` grow or shrink particles over their life. The `aspect` makes them wider or taller.
- The `colors` change the color over the life, spread evenly or at the `colorTimes`. The `tints` give each particle a color of its own, such as the colors of confetti.
- The `rotation` and `spin` turn particles. With `alignToVelocity` a particle points along its motion, and `stretch` lengthens it with its speed, for sparks and rain.
- The `particleOrder` draws new particles on top or below the older ones. Below suits smoke that should not cover its own trail.

Pixel art effects keep to the grid of their art:

- The `pixelSnap` rounds positions to the size of an art pixel.
- The `rotationStep` turns particles only by steps, such as quarter turns.
- The value `'steps'` of `colorBlend` holds each color until the next one instead of blending, which keeps the effect inside its palette.

An emitter draws with the [draw order](../graphics/rendering.md#draw-order) of its options, such as its `layer` and `blend`. An `'additive'` blend suits fire, sparks and magic, which brighten what lies behind them.

```lua
local assets = require('haylen.assets')
local particles2d = require('haylen.particles2d')

local leaves = particles2d.newEmitter({
    texture = assets.texture('effects/leaf-sheet.png', {filter = 'linear'}),
    frameGrid = {columns = 4, rows = 1},
    frameMode = 'random',
    rate = 3,
    prewarm = 10,
    lifetime = {6, 9},
    direction = 1.5708,
    turbulence = {strength = 60, frequency = 0.006, speed = 0.5},
    startSize = {18, 26},
    endSizeScale = 1,
    rotation = {0, 6.2832},
    spin = {-1.5, 1.5},
    tints = {'#FFE89A3C', '#FFD0603C', '#FFE8C850'},
    shape = 'rectangle',
    shapeSize = {900, 10},
})
```

## Effects on the ground

Some effects lie on the ground of a scene seen at an angle, such as a magic circle under a character in a view from above. The `tilt` lays the particles on a plane through the emitter that leans away from the viewer by an angle. The particles move as they would on a flat screen. Drawing then brings their positions nearer to the emitter along the height of the screen, by the cosine of the tilt, and shortens their sprites the same way after their rotation. So a ring, a magic circle or the strokes of a whirlwind spin flat on the ground as ellipses, while the ellipse itself stays still. A sprite takes the same `tilt` in `graphics2d.draw`, sprite batches and float buffers.

```lua
local assets = require('haylen.assets')
local particles2d = require('haylen.particles2d')

local circle = particles2d.newEmitter({texture = assets.texture('effects/rune.png'), rate = 0, bursts = {{time = 0, count = 1}}, lifetime = 3, speed = 0, spin = 1, startSize = 200, endSize = 200, tilt = 1.1})
```

## Sub-emitters

A sub-emitter spawns the particles of another effect at the particles of its emitter. It can spawn them where particles are born, where they die, where they hit something, or at a rate while they live. A raindrop that splashes where it dies is a sub-emitter, and so is a firework that bursts into sparks.

Each sub-emitter keeps one emitter for all its particles. It updates with its parent, before it, and draws with its parent, after it. So a rain of thousands of drops that splash costs two emitters, not thousands, and the particles that a parent spawns in it start at the moment that spawned them. A living particle fires the `alive` trigger at moments spread over each update, where the particle was at each moment, so a fast rocket leaves a smooth trail of smoke, not a clump every frame.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local splash = {texture = graphics.whiteTexture(), speed = {40, 120}, spread = 2, lifetime = 0.3, gravity = {0, 600}, startSize = 3, endSize = 0}
local rain = particles2d.newEmitter({
    texture = graphics.whiteTexture(),
    rate = 400,
    direction = 1.5708,
    spread = 0.05,
    speed = 900,
    shape = 'rectangle',
    shapeSize = {960, 0},
    bounds = {-1000, -100, 2000, 700},
    subEmitters = {{effect = splash, trigger = 'death', count = 3}},
})
```

## Trails

A trail is a ribbon behind a moving point. There are two kinds:

- A `Trail` follows one point you move yourself, such as the tip of a blade or a projectile, without any particles. It adds a point when its position has moved far enough and drops the points older than its lifetime.
- The `trail` option of an emitter gives every particle a ribbon of its own, for meteors and fireworks. The emitter records the positions of all its particles at the same moments.

Both draw as strokes with round ends and joins. Their width and colors go from the head to the tail, and their edges fade over one pixel at any width and zoom. The head moves on between the points a trail keeps, so the newest point can lie just behind it. When that point lies closer to the head than the ribbon is wide there, the head takes its place, so the ribbon never turns on a segment shorter than its width, which would dent its edge at the head.

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

## Lights, glow and distortion

Particles can light the scene in a lit canvas of the [lighting guide](lighting.md). The `light` option puts one light at the emitter, which can flicker and fade with the particles. The `particleLights` option puts a small light on some particles in their own colors. Lights draw only in lit canvases, so the same emitter works in every canvas. The `emission` option makes particles glow at night.

The `distortion` option makes particles bend the image behind them instead of drawing colors, for heat haze and shock rings. It shows only in canvases with lighting or post-processing, as the [distortion section](../graphics/rendering.md#distortion) explains.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local torch = particles2d.newEmitter({
    texture = graphics.whiteTexture(),
    rate = 40,
    lifetime = {0.3, 0.6},
    colors = {'#FFFFE080', '#00FF4000'},
    blend = 'additive',
    emission = 1,
    light = {radius = 180, color = '#FFFFB060', flicker = {speed = 8, amount = 0.2}},
})
```

## Moving, turning and scaling effects

Move an emitter or a system with `x`, `y` or `position`. The `scale` and `rotation` apply to the particles it spawns from then on: the scale multiplies their spawn area, speeds, sizes, gravity and accelerations, and the rotation turns their direction and spawn area. So one effect serves several sizes, both sides of a character, and a muzzle that aims.

Set `emitting` to `false` to stop spawning. Live particles keep moving until they die. Call `clear` to remove every live particle, and `restart` to play the effect again from the start.

## Particles in gameplay

Gameplay can read the particles of an emitter, such as embers that hurt the player or coins to pick up:

- `emitter:positions()` returns the position of every live particle.
- `emitter:readPositions(buffer)` copies the positions into a float buffer without creating a value per particle, for large emitters read in every frame.
- `emitter:particle(index)` returns one particle with its velocity, age and the sprite it draws as.

```lua
local graphics = require('haylen.graphics')
local particles2d = require('haylen.particles2d')

local embers = particles2d.newEmitter({texture = graphics.whiteTexture(), rate = 0, startSize = 4})
embers:burst(20)
local playerX, playerY = 0, 0
for _, position in ipairs(embers:positions()) do
    if math.abs(position.x - playerX) < 8 and math.abs(position.y - playerY) < 8 then
        print('The player touched an ember.')
    end
end
```

## How an update works

An emitter keeps its particles in parallel arrays, one for each value a particle has, such as positions, velocities, ages and sizes. It reserves room for `maxParticles` particles when it is configured, so spawning, moving and removing particles allocate nothing. The arrays of optional features, such as tints, aspects, random frames, collision flags and trails, stay empty when the options leave the feature out, so plain emitters keep their cost.

Every update runs these steps:

1. The emitters of the sub-emitters update the same way, so the particles that the next steps spawn in them start where and when they are born.
2. The emitter measures its own velocity from its move since the last update. The options `inheritVelocity` and `rateOverDistance` use it.
3. The particles move: gravity, radial and tangential acceleration, turbulence, which samples `math::Noise2D` at the position of each particle, attractors, damping, the speed and spin curves, and the floor, bounds and walls. When Lua updates the emitter, they move in chunks of 4096 on the job system. A chunk touches only its own particles, so the result is the same as on one thread.
4. World collision casts the move of every particle through the physics world in one `physics2d::RayBatch`, also on the job system.
5. The sub-emitters of hits and of living particles fire. A living particle fires at moments spread over the update, each where the particle was then, and what it spawns moves for the rest of the update. Then the expired particles fire the sub-emitters of death and leave the arrays, which close their gaps in place and keep the order the particles draw in.
6. New particles spawn. Bursts spawn at the position of the emitter. The particles of the rate and of the distance spread along the path the emitter moved and over the time of the update. Each particle is born at its own moment inside the update, then moves, collides and ages for the part of the update it lived. So a long frame, a slow device or a fast emitter leaves an even stream, not a clump at the emitter and a gap behind it. World collision casts these first moves from where the particles were born. Each new particle fires the sub-emitters of births where it was born.
7. The trails record the positions of the particles at fixed moments.

## How drawing works

A draw turns every live particle into one sprite of the texture of the emitter and records them all as one batch. The renderer converts large batches on worker threads. The trails of an emitter draw as strokes whose meshes join into one draw call. Its lights draw only in lit canvases, and the particles of its sub-emitters draw after it.

## The effect library

The test project holds a library of ready effects in `samples/tests/content/particles/library/`. It has one folder for each category, such as `hits`, `magic-circles`, `buffs`, `status`, `explosions`, `fire` and `weather`, next to the `textures` folder that every effect draws from.

The file `catalog.json` lists every effect with:

- Its name.
- Its style: `casual`, `realistic`, `neon` or `pixel`.
- The view it suits: `top` for effects that lie on the ground of a view from above, `side` for the floor of a platform game, and `any` for both.
- Its kind: `loop` effects emit until the app stops them, `once` effects play one cycle and end on their own, and `cycle` effects repeat a cycle of bursts.

Most effects are composite files of several emitters that play together as layers. A large soft halo lights everything around the effect. A hot flare or a burst of rays sits at its center. Rings open. Painted frame sequences play over the life of a particle with `frameMode`, such as a blast, an impact, a licking flame, a crackling bolt or a splash crown. Sparkles, sparks and debris fly off, smoke billows, and icons such as hearts, skulls and arrows rise over a character.

Some categories place their effects in a particular way:

- The side view effects of `movement`, `pickups`, `combat` and `level` sit on the floor at the emitter. They include jump and landing dust, dashes, ground pounds, collected coins and keys, muzzle flashes, broken blocks, lava, waterfalls and jetpacks. Their pieces fall with gravity and bounce, stick or splash with a floor `collision`, and run along the floor to the sides.
- The effects of `weapons`, such as a flamethrower, lasers, a plasma shot and a rifle burst, fire to the right from the chest of a character that stands at the emitter.
- The effects of `bars` run along a strip centered on the emitter, such as an energy bar, a bar that charges and the flash of damage.
- The effects of `character` wrap a character that stands at the emitter, such as a shield bubble, an energy outline that follows its silhouette along a `polyline` shape, a shock and an aura of flames.
- Rings, magic circles and the strokes of whirlwinds lie on the ground with the `tilt` of their emitters and keep spinning flat.

The tool `samples/tests/tools/particles/write-effect-library.py` writes every effect from these layers. The pixel art effects use pieces of the pixel atlas drawn with the `nearest` filter, snapped to their art pixels, turned by quarter turns and held in their palette with `colorBlend = 'steps'`. The test `PRT-034` of the test project browses the library and edits the main values of each effect live. The test `PRT-035` loads and runs every file of the library, and fails with the file and the error of any effect that breaks.

### Using a library effect

1. Copy the effect files you need into the `content` folder of your app, keeping their category folders, and copy the `textures` folder next to them, such as `content/effects/hits/hit-rays.particles` and `content/effects/textures/`. The effects draw from three atlases: `../textures/effects.png` for the painted pieces, `../textures/sequences.png` for the painted frame sequences and `../textures/pixel.png` for the pixel art. They name the `frames` of their pieces as rectangles of the atlas, so the folders must keep their places relative to each other, and the emitters of an effect batch together. The files `effects.json`, `sequences.json` and `pixel.json` name every piece of the atlases for code that builds emitters. A ribbon and the sheet of runes that `frameGrid` cuts stay images of their own. A composite effect that names other effect files, such as the full pixel art explosion, needs those files too, and so does an effect with sub-emitters.
2. Load an effect with `assets.load('effects/fire/campfire-realistic.particles')`. Create it with `particles2d.newEmitter(effect)`, or with `particles2d.newSystem(effect)` for a composite effect. The effects of the painted atlas name the filter `linear` in the file, and the pixel effects keep `nearest`.
3. Place it with `position`, `scale` and `rotation`. Adjust it with the overrides of `particles2d.newEmitter` or with `emitter:configure`, such as other `colors`. Restart a one-shot effect with `restart` when it fires again.

The values of the library use the scale of the test project. They are world units at a design resolution of 1920 by 1080 with a camera zoom of 1. A character is about 128 units tall, and an art pixel of the pixel art is 4 units. Effects such as rain and snow cover the width of that view from just above it, and the app moves them with its camera.

The effects `fire/heat-haze`, `explosions/shockwave-distortion` and `scifi/shield-ripple-distortion` use `distortion`, so they show only in world canvases with lighting or post-processing. The test `EFX-002` shows them over a landscape.

```lua
local assets = require('haylen.assets')
local particles2d = require('haylen.particles2d')

local campfire = particles2d.newSystem(assets.load('effects/fire/campfire-realistic.particles'))
campfire.position = {640, 520}
campfire.scale = 0.75
```

## Performance

- An emitter costs its live particles. The optional features cost only the emitters that use them. Turbulence samples noise twice for each particle, every attractor and collision type tests each particle, and world collision casts a ray for each moving particle.
- Large emitters update on the job system when Lua updates them, and C++ apps pass the job system to `Emitter::update`.
- Each emitter draws at least one batch, so many small emitters of one texture cost more draw calls than one larger emitter. Trails add a mesh for each emitter, and lights add a light for each emitter light and each particle light.
- Sub-emitters keep one emitter for all their particles.
- Measure in Release builds, because Debug builds run without optimizations.

### The particle benchmark

The command `haylen bench --suite particles` builds `haylen-particle-benchmark` in Release and runs it on the headless host. It prints the workers of the job system and, for each scene, the average time of one frame in milliseconds. These numbers come from an Apple M5 Pro with a Release build. Absolute values vary from machine to machine and from run to run.

| Scene | What it measures | ms |
| --- | --- | --- |
| One emitter of 100000 particles, one thread | The update of a fountain whose rate keeps about 100 thousand particles alive, with gravity, damping and radial and tangential acceleration, on the frame thread. | 0.895 |
| One emitter of 100000 particles, job system | The same update with the job system. | 0.657 |
| 1000 emitters of 100 particles | The update of a thousand small fountains. | 0.724 |
| Spawning 200000 particles per second | The update of an emitter that replaces 20 thousand particles ten times per second. | 0.237 |
| One emitter of 100000 particles with turbulence, attractor, floor | The large fountain with turbulence, an attractor and a floor it bounces on, with the job system. | 0.867 |
| Rain of 2500 drops per second that splash where they die | Rain that dies at the bounds of the ground, where a sub-emitter spawns a splash of three particles for each drop. | 0.031 |
| Drawing 1 emitter of 100000 particles | The draw call of the large fountain, which builds its sprites. | 0.910 |
| Drawing 1000 emitters of 100 particles | The draw calls of the thousand small fountains. | 1.302 |
| Drawing 100 emitters of 100 particles with trails | The draw calls of a hundred small fountains whose particles draw a ribbon of eight points each, 10 thousand ribbons in all. | 2.388 |
| 500 emitters of about 60 particles updated from Lua | One `emitter:update` call from Lua for each of 500 emitters. | 0.223 |

## From C++

The classes live in `haylen/2d/particles/`, in the namespace `haylen::particles2d`:

- `Emitter` (`Emitter.hpp`) simulates and draws one emitter. Its method `getParticle(index)` returns a live particle with the sprite it draws as, for gameplay that reacts to particles and for tests.
- `EmitterConfig` (`EmitterConfig.hpp`) holds the options, which a Lua table or an effect file fills with the same keys. Every enum of it has one table of names, such as `EmitterConfig::kShapeNames`, which the file parser and the Lua binding read with `EmitterConfig::fromName` and `EmitterConfig::nameOf`.
- `Effect` (`Effect.hpp`) parses a `.particles` file. Its `Effect::parse(document, path, read)` reads the keys in one pass. It follows the effect files that sub-emitters and composite entries name through `read`, which the asset loader gives the package. It refuses an effect that refers to itself, and reads image shapes from their images. The particles plugin decodes the textures of every emitter of a file on the worker thread that loads it, and shares them with the texture cache on the frame thread.
- `System` (`System.hpp`) holds the emitters of an effect, one for each entry of a composite file, or one for an effect of a single emitter.
- `Trail` (`Trail.hpp`) is the ribbon behind one moving point, and `Ribbon` draws the trails of particles as strokes of the renderer.

```cpp
#include <any>
#include <optional>
#include <utility>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/particles/Emitter.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"

class Campfire final : public haylen::core::Scene {
  public:
    void enter(haylen::core::Engine& engine, const std::any& params) override {
        haylen::particles2d::EmitterConfig config;
        config.texture = engine.getAssets().texture("effects/spark.png");
        config.rate = 60.0F;
        config.colors = {haylen::math::Color::fromHex(0xFFE080FFU), haylen::math::Color::transparent()};
        fire.emplace(std::move(config), 7);
    }

    void update(haylen::core::Engine& engine, float deltaSeconds) override {
        fire->update(deltaSeconds, engine.getJobs());
    }

    void render(haylen::core::Engine& engine) override {
        engine.getRenderer2D().beginWorld(camera);
        fire->draw(engine.getRenderer2D());
    }

  private:
    haylen::graphics2d::Camera camera;
    std::optional<haylen::particles2d::Emitter> fire;
};
```
