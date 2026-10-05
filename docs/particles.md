# Particles

Haylen draws fire, smoke, sparks, weather, magic and every other particle effect with `particles2d::Emitter` (`engine/include/haylen/2d/particles/Emitter.hpp`), which `haylen.particles2d` exposes to Lua. This guide explains how emitters simulate and draw their particles, how effects of several emitters, trails and lights fit together, and what they cost. The [`haylen.particles2d` reference](lua-api/particles2d.md) describes every option and the Lua API.

## Configuration

An emitter takes a `particles2d::EmitterConfig` (`engine/include/haylen/2d/particles/EmitterConfig.hpp`), which a Lua options table or a `.particles` effect file fills with the same keys. The configuration describes when particles are born (the rate, the rate over distance, bursts with cycles and probabilities, the delay and the cycle), where (the spawn area, turned and scaled), how they move (speed and direction, the direction modes, inherited velocity, gravity, radial and tangential acceleration, damping, turbulence, attractors, collision and bounds), how they look (frames and frame modes, sizes and curves, aspect and stretch, rotation, colors with stops and tints, pixel snapping and the order they draw in) and what comes with them (sub-emitters, trails and lights). Every enum has one table of names in `EmitterConfig`, such as `EmitterConfig::kShapeNames`, which the file parser and the Lua binding read with `EmitterConfig::fromName` and `EmitterConfig::nameOf`.

`particles2d::Effect` (`Effect.hpp`) parses a `.particles` file. Its `Effect::parse(document, path, read)` reads the keys in one pass, follows the effect files that sub-emitters and composite entries name through `read`, which the asset loader gives the package, refuses an effect that refers to itself, and reads image shapes from their images. The particles plugin decodes the textures of every emitter of the file on the worker thread that loads the effect and shares them with the texture cache on the frame thread, with the `filter` and `wrap` of the file unless the caller of `assets.load` names them.

## Simulation

An emitter keeps its particles in parallel arrays, one for each value a particle has, such as positions, velocities, ages and sizes, and reserves room for `maxParticles` of them when it is configured, so emitting, moving and removing particles allocate nothing. The arrays of optional features, the tints, the aspects, the frames of the random frame modes, the collision flags and the trails, stay empty when the configuration leaves the feature out, so plain emitters keep their cost.

Every update runs these steps:

1. The emitter measures its own velocity from the move since the last update, which `inheritVelocity` passes on and `rateOverDistance` counts.
2. The particles move in chunks of 4096 on the job system when Lua updates the emitter: gravity, radial and tangential acceleration, turbulence from `math::Noise2D` at the position of each particle, the attractors, damping, the speed and spin curves, and the floor, bounds walls and bounds of the configuration. A chunk only touches its own particles, so the result is identical to the update on one thread.
3. World collision casts the move of every particle through the physics world in one `physics2d::RayBatch`, also on the job system.
4. The sub-emitters of hits and of living particles fire, then the expired particles fire the sub-emitters of death and leave the arrays, which compact in place and keep the order particles draw in.
5. The trails record the positions of the particles at fixed moments, and new particles spawn: bursts at the position of the emitter, and the particles of the rate and of the distance spread along the segment the emitter moved.
6. The emitters of the sub-emitters update the same way.

Nothing calls Lua for each particle, which is why particle curves take names and tables and never Lua functions.

## Drawing

Drawing turns every live particle into one sprite of the emitter texture and records them as one batch, which the renderer converts on worker threads when it holds thousands of sprites. The trails of all particles of an emitter draw as one mesh through `particles2d::Ribbon`, the lights draw only in lit canvases, and the emitters of the sub-emitters draw after their parent. The method `Emitter::getParticle(index)`, `emitter:particle(index)` in Lua, returns a live particle with the sprite it draws as, for gameplay that reacts to particles and for tests.

## Systems and composite effects

A `particles2d::System` (`System.hpp`) holds the emitters of an effect, one for each entry of a composite file or one for an effect of a single emitter. It places every emitter at its offset from the position of the system, turned and scaled with it, updates and draws them in the order of the file and restarts them together, and the delay of each entry starts the parts in sequence, such as the flash, the fireball, the smoke and the debris of an explosion. Lua creates systems with `particles2d.newSystem(effect)`.

## Trails

A `particles2d::Trail` (`Trail.hpp`) is a ribbon that follows a moving point, such as a blade tip or a projectile, without particles. It adds a point when its position moves far enough, drops the points older than its lifetime, and draws one mesh whose width and colors go from the head to the tail. Particles take a ribbon each through the `trail` option, which records their positions at the same moments for all of them.

## Performance

- An emitter costs its live particles, and the optional features cost only the emitters that use them. Turbulence samples noise twice for each particle, every attractor and collision type tests each particle, and world collision casts a ray for each moving particle.
- Large emitters update on the job system when Lua updates them, and C++ apps pass the job system to `Emitter::update`.
- Each emitter draws at least one batch, so many small emitters of one texture cost more draw calls than one larger emitter. Trails add a mesh per emitter, and lights a light for each emitter light and each particle light.
- Sub-emitters keep one emitter for all their particles, so a rain of thousands of drops that splash costs two emitters.
- Measure in Release builds, because Debug builds run without optimizations.

## The particle benchmark

The command `python3 haylen.py bench --suite particles` builds `haylen-particle-benchmark` in Release and runs it on the headless host. It prints the workers of the job system and, for each scene, the average time of one frame in milliseconds. These numbers come from an Apple M5 Pro with a Release build, and absolute values vary from machine to machine and from run to run. The column "Before" is the emitter before turbulence, attractors, collision, sub-emitters, trails, lights and the other features of the configuration existed, and the column "After" is the current emitter, so the features cost nothing to the emitters that leave them out. The scenes of the new features have no earlier number.

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
