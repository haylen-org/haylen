# Particles

Haylen draws fire, smoke, sparks, weather, magic and every other particle effect with `particles2d::Emitter` (`engine/include/haylen/2d/particles/Emitter.hpp`), which `haylen.particles2d` exposes to Lua. This guide explains how emitters simulate and draw their particles and what they cost. The [`haylen.particles2d` reference](lua-api/particles2d.md) describes every option and the Lua API.

## Simulation

An emitter keeps its particles in parallel arrays, one for each value a particle has, such as positions, velocities, ages and sizes, and reserves room for `maxParticles` of them when it is created, so emitting, moving and removing particles allocate nothing. Every update moves the particles, removes the expired ones by compacting the arrays in place, which keeps the order they draw in, and spawns the new ones. An emitter of thousands of particles moves them in chunks on the job system when Lua updates it, and the result is identical to the update on one thread. Nothing calls Lua for each particle.

Drawing turns every live particle into one sprite of the emitter texture and records them as one batch, which the renderer converts on worker threads when it holds thousands of sprites.

## The particle benchmark

The command `python3 haylen.py bench --suite particles` builds `haylen-particle-benchmark` in Release and runs it on the headless host. It prints the workers of the job system and, for each scene, the average time of one frame in milliseconds. These numbers come from an Apple M5 Pro with a Release build, and absolute values vary from machine to machine and from run to run.

| Scene | What it measures | Average ms |
| --- | --- | --- |
| One emitter of 100000 particles, one thread | The update of a fountain whose rate keeps about 100 thousand particles alive, with gravity, damping and radial and tangential acceleration, on the frame thread. | 0.893 |
| One emitter of 100000 particles, job system | The same update with the job system. | 0.878 |
| 1000 emitters of 100 particles | The update of a thousand small fountains. | 0.737 |
| Spawning 200000 particles per second | The update of an emitter that replaces 20 thousand particles ten times per second. | 0.332 |
| Drawing 1 emitter of 100000 particles | The draw call of the large fountain, which builds its sprites. | 1.152 |
| Drawing 1000 emitters of 100 particles | The draw calls of the thousand small fountains. | 1.649 |
| 500 emitters of about 60 particles updated from Lua | One `emitter:update` call from Lua for each of 500 emitters. | 0.232 |
