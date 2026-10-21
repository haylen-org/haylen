# Particles

Haylen draws fire, smoke, sparks, weather, magic and every other particle effect with `particles2d::Emitter` (`engine/include/haylen/2d/particles/Emitter.hpp`), which `haylen.particles2d` exposes to Lua. This guide explains how emitters simulate and draw their particles, how effects of several emitters, trails and lights fit together, and what they cost. The [`haylen.particles2d` reference](../lua-api/particles2d.md) describes every option and the Lua API.

## Configuration

An emitter takes a `particles2d::EmitterConfig` (`engine/include/haylen/2d/particles/EmitterConfig.hpp`), which a Lua options table or a `.particles` effect file fills with the same keys. The configuration describes when particles are born (the rate, the rate over distance, bursts with cycles and probabilities, the delay and the cycle), where (the spawn area, turned and scaled), how they move (speed and direction, the direction modes, inherited velocity, gravity, radial and tangential acceleration, damping, turbulence, attractors, collision and bounds), how they look (frames and frame modes, sizes and curves, aspect and stretch, rotation, colors with stops and tints, pixel snapping and the order they draw in) and what comes with them (sub-emitters, trails and lights). Every enum has one table of names in `EmitterConfig`, such as `EmitterConfig::kShapeNames`, which the file parser and the Lua binding read with `EmitterConfig::fromName` and `EmitterConfig::nameOf`.

The class `particles2d::Effect` (`Effect.hpp`) parses a `.particles` file. Its `Effect::parse(document, path, read)` reads the keys in one pass, follows the effect files that sub-emitters and composite entries name through `read`, which the asset loader gives the package, refuses an effect that refers to itself, and reads image shapes from their images. The particles plugin decodes the textures of every emitter of the file on the worker thread that loads the effect and shares them with the texture cache on the frame thread, with the `filter` and `wrap` of the file unless the caller of `assets.load` names them.

## Simulation

An emitter keeps its particles in parallel arrays, one for each value a particle has, such as positions, velocities, ages and sizes, and reserves room for `maxParticles` of them when it is configured, so emitting, moving and removing particles allocate nothing. The arrays of optional features, the tints, the aspects, the frames of the random frame modes, the collision flags and the trails, stay empty when the configuration leaves the feature out, so plain emitters keep their cost.

Every update runs these steps:

1. The emitter measures its own velocity from the move since the last update, which `inheritVelocity` passes on and `rateOverDistance` counts.
2. The particles move in chunks of 4096 on the job system when Lua updates the emitter: gravity, radial and tangential acceleration, turbulence from `math::Noise2D` at the position of each particle, the attractors, damping, the speed and spin curves, and the floor, bounds walls and bounds of the configuration. A chunk only touches its own particles, so the result is identical to the update on one thread.
3. World collision casts the move of every particle through the physics world in one `physics2d::RayBatch`, also on the job system.
4. The sub-emitters of hits and of living particles fire, then the expired particles fire the sub-emitters of death and leave the arrays, which compact in place and keep the order particles draw in.
5. New particles spawn: bursts at the position of the emitter, and the particles of the rate and of the distance spread along the segment the emitter moved and over the time of the update. Each one is born at its moment of the update and moves, collides and ages for the part of the update it lived, so a long frame, a slow device or a fast emitter leaves an even stream instead of a clump at the emitter and a gap behind it, and world collision casts these first moves from where the particles were born.
6. The trails record the positions of the particles at fixed moments.
7. The emitters of the sub-emitters update the same way.

Nothing calls Lua for each particle, which is why particle curves take names and tables and never Lua functions.

## Drawing

Drawing turns every live particle into one sprite of the emitter texture and records them as one batch, which the renderer converts on worker threads when it holds thousands of sprites. The trails of the particles of an emitter draw through `particles2d::Ribbon` as strokes of the renderer with round ends and joins, whose edges fade over one pixel at any width and zoom and whose meshes join into one draw call, the lights draw only in lit canvases, and the emitters of the sub-emitters draw after their parent. The method `Emitter::getParticle(index)`, `emitter:particle(index)` in Lua, returns a live particle with the sprite it draws as, for gameplay that reacts to particles and for tests.

An emitter with a `tilt` draws on a plane through the emitter that leans away from the viewer, the ground of a top-down or side view seen at an angle. Its particles move as they would on a flat screen, and drawing brings their positions nearer to the emitter along the height of the screen by the cosine of the tilt and shortens their sprites the same way after their rotation, in the vertex shader, so a ring, a magic circle or the strokes of a whirlwind spin flat on the ground as ellipses while the ellipse itself stays still. A sprite takes the same `tilt` in `graphics2d.draw`, sprite batches and float buffers.

## Systems and composite effects

A `particles2d::System` (`System.hpp`) holds the emitters of an effect, one for each entry of a composite file or one for an effect of a single emitter. It places every emitter at its offset from the position of the system, turned and scaled with it, updates and draws them in the order of the file and restarts them together, and the delay of each entry starts the parts in sequence, such as the flash, the fireball, the smoke and the debris of an explosion. Lua creates systems with `particles2d.newSystem(effect)`.

## Trails

A `particles2d::Trail` (`Trail.hpp`) is a ribbon that follows a moving point, such as a blade tip or a projectile, without particles. It adds a point when its position moves far enough, drops the points older than its lifetime, and draws one stroke with round ends whose width and colors go from the head to the tail and whose edges fade over one pixel. Particles take a ribbon each through the `trail` option, which records their positions at the same moments for all of them.

## Effect library

The test project holds a library of effects in `samples/tests/content/particles/library/`, one folder for each category, such as `hits`, `magic-circles`, `buffs`, `status`, `explosions`, `fire` and `weather`, next to the `textures` folder that every effect draws from. The file `catalog.json` lists every effect with its name, its style (`casual`, `realistic`, `neon` or `pixel`), the view it suits (`top` for the effects that lie on the ground of a view from above, `side` for the floor of a platform game and `any` for both) and its kind: `loop` effects emit until the app stops them, `once` effects are one-shot cycles that end on their own, and `cycle` effects repeat a cycle of bursts. Most effects are composite files of several emitters that play together as layers, the way the effects of finished games are built: a large soft halo that lights everything around the effect, a hot flare or a burst of rays at its center, rings that open, painted frame sequences that play over the life of a particle with `frameMode`, such as a blast, an impact, a licking flame, a crackling bolt or a splash crown, sparkles, sparks and debris that fly off, smoke that billows, and icons such as hearts, skulls and arrows that rise over a character. The side view effects of `movement`, `pickups`, `combat` and `level`, such as jump and landing dust, dashes, ground pounds, collected coins and keys, muzzle flashes, broken blocks, lava, waterfalls and jetpacks, sit on the floor at the emitter, where their pieces fall with gravity and bounce, stick or splash with a floor `collision`, and run along the floor to the sides. The weapons of `weapons`, such as a flamethrower, lasers, a plasma shot and a rifle burst, fire to the right from the chest of a character that stands at the emitter, the effects of `bars` run along a strip centered on the emitter, such as an energy bar, a bar that charges and the flash of damage, and the effects of `character` wrap a character that stands there, such as a shield bubble, an energy outline that follows its silhouette along a `polyline` shape, a shock and an aura of flames. Rings, magic circles and the strokes of whirlwinds lie on the ground with the [`tilt`](../lua-api/particles2d.md#look) of their emitters and keep spinning flat. The tool `samples/tests/tools/particles/write-effect-library.py` writes every effect from these layers, the pixel art ones from pieces of the pixel atlas drawn with the `nearest` filter, snapped to their art pixels, turned by quarter turns and held in their palette with `colorBlend = 'steps'`, and `PRT-034` of the test project browses the library and edits the main values of each effect live, while `PRT-035` loads and runs every file of the library and fails with the file and the error of any effect that breaks.

An app takes effects from the library like this:

1. Copy the effect files it needs, keeping their category folders, and the `textures` folder next to them, into its own `content` folder, such as `content/effects/hits/hit-rays.particles` and `content/effects/textures/`. The effects draw from three atlases, `../textures/effects.png` for the painted pieces, `../textures/sequences.png` for the painted frame sequences, such as an explosion that billows, a flame that licks and a bolt that crackles, and `../textures/pixel.png` for the pixel art, and name the `frames` of their pieces as rectangles of the atlas, so the folders keep their places relative to each other and the emitters of an effect batch together. The files `effects.json`, `sequences.json` and `pixel.json` name every piece of the atlases for code that builds emitters, and a ribbon and the sheet of runes that `frameGrid` cuts stay images of their own. A composite effect that names other effect files, such as the full pixel art explosion, also needs those files, and an effect with sub-emitters the files they name.
2. Load an effect with `assets.load('effects/fire/campfire-realistic.particles')` and create it with `particles2d.newEmitter(effect)`, or with `particles2d.newSystem(effect)` for a composite effect. The effects of the painted atlas name the filter `linear` in the file, and the pixel effects keep `nearest`.
3. Place it with `position`, `scale` and `rotation`, adjust it with the overrides of `particles2d.newEmitter` or with `emitter:configure`, such as other `colors`, and restart one-shot effects with `restart` when they fire again.

The values of the library use the scale of the test project: world units at a 1920 by 1080 design resolution with camera zoom 1, a character about 128 units tall, and pixel art with art pixels of 4 units. Effects such as rain and snow cover the width of that view from just above it, and the app moves them with its camera.

The effects `fire/heat-haze`, `explosions/shockwave-distortion` and `scifi/shield-ripple-distortion` bend the image with a `distortion` instead of drawing colors, so they show only in world canvases with lighting or post-processing, as the [distortion section](../lua-api/graphics2d.md#distortion) explains, and the test `EFX-002` shows them over a landscape.

## Performance

- An emitter costs its live particles, and the optional features cost only the emitters that use them. Turbulence samples noise twice for each particle, every attractor and collision type tests each particle, and world collision casts a ray for each moving particle.
- Large emitters update on the job system when Lua updates them, and C++ apps pass the job system to `Emitter::update`.
- Each emitter draws at least one batch, so many small emitters of one texture cost more draw calls than one larger emitter. Trails add a mesh per emitter, and lights a light for each emitter light and each particle light.
- Sub-emitters keep one emitter for all their particles, so a rain of thousands of drops that splash costs two emitters.
- Measure in Release builds, because Debug builds run without optimizations.

## The particle benchmark

The command `haylen bench --suite particles` builds `haylen-particle-benchmark` in Release and runs it on the headless host. It prints the workers of the job system and, for each scene, the average time of one frame in milliseconds. These numbers come from an Apple M5 Pro with a Release build, and absolute values vary from machine to machine and from run to run. The column "Before" is the emitter before turbulence, attractors, collision, sub-emitters, trails, lights and the other features of the configuration existed, and the column "After" is the current emitter, so the features cost nothing to the emitters that leave them out. The scenes of the new features have no earlier number.

| Scene | What it measures | Before ms | After ms |
| --- | --- | --- | --- |
| One emitter of 100000 particles, one thread | The update of a fountain whose rate keeps about 100 thousand particles alive, with gravity, damping and radial and tangential acceleration, on the frame thread. | 0.893 | 0.895 |
| One emitter of 100000 particles, job system | The same update with the job system. | 0.878 | 0.657 |
| 1000 emitters of 100 particles | The update of a thousand small fountains. | 0.737 | 0.724 |
| Spawning 200000 particles per second | The update of an emitter that replaces 20 thousand particles ten times per second. | 0.332 | 0.237 |
| One emitter of 100000 particles with turbulence, attractor, floor | The large fountain with turbulence, an attractor and a floor it bounces on, with the job system. | | 0.867 |
| Rain of 2500 drops per second that splash where they die | Rain that dies at the bounds of the ground, where a sub-emitter spawns a splash of three particles for each drop. | | 0.031 |
| Drawing 1 emitter of 100000 particles | The draw call of the large fountain, which builds its sprites. | 1.152 | 0.910 |
| Drawing 1000 emitters of 100 particles | The draw calls of the thousand small fountains. | 1.649 | 1.302 |
| Drawing 100 emitters of 100 particles with trails | The draw calls of a hundred small fountains whose particles draw a ribbon of eight points each, 10 thousand ribbons in all. | | 2.388 |
| 500 emitters of about 60 particles updated from Lua | One `emitter:update` call from Lua for each of 500 emitters. | 0.232 | 0.223 |
