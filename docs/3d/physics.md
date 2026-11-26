# 🪨 3D physics

This guide explains how 3D physics works in Haylen: what a world is, how it steps, how it uses threads and memory, what its limits do, how a world shows that two runs match, which shapes bodies collide with, and how a plugin puts another physics engine behind the same API. The [reference of `haylen.physics3d`](../lua-api/physics3d.md) lists every function, option and property.

## Worlds

A world is one 3D simulation. It holds bodies and moves them each time the app steps it. Everything in a world is in meters, seconds and kilograms, with `y` up, as the [units of 3D](coordinates.md#units) list, so a box one unit wide is one meter wide and falls as a real box of that size falls. The default gravity pulls down with the standard gravity of the Earth, `m.standardGravity`.

```lua
local physics3d = require('haylen.physics3d')

function scene:load()
    self.world = physics3d.newWorld({owner = self})
end

function scene:fixedUpdate(seconds)
    self.world:step(seconds)
end
```

A world belongs to its owner, such as the scene that made it, and ends with it, so a world never outlives the scene it belongs to. The method `world:destroy` ends it earlier. Once a world ends, every call on it raises an error, so code that keeps a world it should not keep fails at once instead of stepping a world that is gone.

An app makes as many worlds as it needs, such as one for the level and one for a preview of an object that turns in a menu. Worlds never touch each other.

## Settings

The section `physics3d` of `haylen.json` gives the settings that every world of the app starts from, and `physics3d.newWorld` changes any of them for one world. The engine reads the section before any Lua runs, and a key it does not know or a value out of its range stops the app with a sentence that names the key, such as `physics3d.velocitySteps`.

```json
{
    "name": "Moon Base",
    "physics3d": {
        "gravity": [0, -1.62, 0],
        "velocitySteps": 12,
        "maxBodies": 4096
    }
}
```

Most settings change while the world lives, such as the gravity, the solver passes and sleeping. The limits stay for the life of the world, since the world reserves their memory when it is made: `maxBodies`, `maxBodyPairs`, `maxContactConstraints`, `threads` and `temporaryMemory`.

## Steps

A step advances the world by the time it receives. Step a world in the `fixedUpdate` of a scene, which runs at the fixed rate of `haylen.json`, so every step lasts the same time whatever the frame rate, and the simulation behaves the same on a slow phone and on a fast desktop.

Each step solves in passes. The velocity passes, `velocitySteps`, make bodies push each other with the right impulses, and the position passes, `positionSteps`, move overlapping bodies apart. A step can also split into `collisionSteps` equal parts, each of which finds the collisions again. A game that steps at 30 hertz with fast bodies keeps the accuracy of 60 hertz with two collision steps.

```lua
local racing = physics3d.newWorld({collisionSteps = 2, velocitySteps = 12})
```

Contacts start a little before bodies touch, within `speculativeContactDistance`, so a fast body stops at the surface instead of sinking into it. The solver leaves `penetrationSlop` of overlap between resting bodies, so a pile does not jitter, and a body bounces only when it hits faster than `restitutionThreshold`, so a box set down gently stays down.

## Sleeping

A body that rests for `timeBeforeSleep` seconds, with every point slower than `sleepVelocity`, falls asleep. A sleeping body costs nothing in a step until something touches it, moves it or changes the world around it. Leave sleeping on in almost every world, since a level full of resting objects then steps as fast as an empty one. An app that redraws only on change keeps drawing while a world has awake bodies, and stops once they all sleep.

## Threads

A world steps on the job system of the engine, which runs on the worker threads of Varn. The thread that steps the world takes part too, and runs every job that no worker started yet, so a step never waits behind other work that the workers run, such as an image that decodes. Waking the workers costs more than a small world saves, so a world steps on its `threads` only while a thousand bodies or more are awake, and on the thread that steps it otherwise. A page in the browser has no worker, so its worlds step on the thread of the frames.

Lua never runs on a worker. A step finishes before `world:step` returns, so everything a script reads after it is the state of the finished step.

## Memory

A world reserves its memory when it is made, and a step allocates nothing, whether its bodies rest or move. The broad phase, which finds the pairs of bodies whose bounds touch, rebuilds its trees as bodies move in memory that the world reserves for `maxBodies` bodies, and the data of one step lives in the temporary memory of the world, `temporaryMemory` megabytes, which every step reuses. A step that needs more than that still runs: it allocates the rest, which costs time, and the engine writes once in the log which setting to raise.

## Limits

The limits of a world keep its memory bounded. A step that finds more pairs of touching bodies than `maxBodyPairs`, or more contacts than `maxContactConstraints`, leaves the rest out, so some bodies may sink into each other in that step. The engine writes once in the log which limit the world ran into and where to raise it. Raise the limits of a world that holds many bodies close together, such as a pile of thousands of crates.

## Determinism

A world gives the same result for the same input on the same platform and the same build of the app, whatever threads its steps run on. The method `world:stateHash` hashes the whole state of the world, every body, contact and constraint, bit for bit, so two runs compare one number. A replay checks that it plays back the game it recorded, and a game that runs in lockstep over the network checks that the players stay in step.

```lua
local hash = world:stateHash()
```

Different platforms and builds round some numbers differently, so the hashes of a desktop and a phone may differ. Games that need the same result on every platform send the inputs and the state between the players instead of the inputs alone.

## Shapes

A shape is the form that a body collides with. It is separate from what the app draws, so a detailed model of a crate collides as a plain box and costs little. A shape never changes once it is made. Many bodies share one shape, so a thousand crates hold one box, and a shape lives while a body or a compound holds it.

```lua
local crate = physics3d.newBox({1, 1, 1})
local ball = physics3d.newSphere(0.11)
```

### Choosing a shape

Simple shapes collide the fastest and the steadiest. Pick the simplest shape that fits the object.

| Shape | Suits |
| --- | --- |
| Sphere | Balls, marbles, round projectiles. |
| Box | Crates, walls, floors, buildings seen from far. |
| Capsule | Characters, which slide over steps and edges, and limbs. |
| Tapered capsule | Limbs that are thicker at one end, such as the arm of a ragdoll. |
| Cylinder and tapered cylinder | Barrels, wheels, pillars, buckets. |
| Convex hull | Rocks, bottles, the simplified form of any convex model. |
| Triangle mesh | The level, the terrain of a model, buildings that never move. |
| Heightfield | Terrain made of a grid of heights. |
| Plane | An endless floor or water level. |
| Compound | Objects made of parts, such as a chair of boxes or a car of a box and wheels. |

Round shapes stand along `y` around their middle. A body turns its shape, so a wheel is a cylinder on a body turned on its side.

### Solid shapes and surfaces

A sphere, a box, a capsule, a cylinder, a hull and a compound of them have a volume. They take a mass from their density, so a dynamic body moves them by real forces. The density is in kilograms per cubic meter, `1000` by default, the density of water. Wood is about `600`, stone about `2500` and steel about `7800`.

```lua
local stone = physics3d.newSphere(0.2, {density = 2500})
print(stone.mass) -- 83.8 kilograms
```

A triangle mesh, a heightfield and a plane are surfaces without a volume, so they have no mass, and only static and kinematic bodies take them. A dynamic object of a concave form, such as a bowl or a chair, is a compound of convex shapes.

The edges of a box, a cylinder and a hull are rounded by a small convex radius inside their size, `0.05` meters by default, or less for a thin box, such as a wall of two centimeters. The rounding makes their collisions faster and steadier and never grows the shape. A convex radius of 0 keeps the edges sharp.

### Meshes and terrain

A triangle mesh takes its positions and triangles from a `FloatBuffer`, from lists or from a [`MeshBvh`](../lua-api/spatial3d.md) that the app already built for its ray casts. The corners of a triangle run counter-clockwise on its front. Triangles without area are left out, so a mesh exported with a few broken triangles still works.

A heightfield takes a square grid of heights, with the meters between samples and the factor of the heights in its scale. It stores the heights in less memory than a mesh of the same terrain and collides faster. The holes of a heightfield are samples without collision, such as the mouth of a cave.

```lua
local terrain = physics3d.newHeightField(heights, 129, {scale = {2, 40, 2}, offset = {-128, 0, -128}})
```

A large mesh or terrain takes time to build. The functions that end in `Async` build it on the task pool and return a promise, so the frames go on while it builds.

```lua
async.spawn(function()
    local level = physics3d.newMeshAsync(positions, indices):await()
end)
```

### Compounds

A compound joins shapes into one, each child with its position and rotation in the space of the compound. A static compound builds its tree once and suits objects whose parts never change. A mutable compound gives each body that takes it a copy of its own, whose children the body adds, removes and moves while it lives, such as a ship that gains parts.

A scaled shape reuses one shape at another size, and an offset center of mass moves the point a body turns around without moving the shape, such as the low center of mass that keeps a car on its wheels.

```lua
local car = physics3d.newOffsetCenterOfMass(physics3d.newBox({1.8, 1.2, 4.2}), {0, -0.4, 0})
```

A shape belongs to the backend that made it, the one that new worlds take, and only the worlds of that backend take it.

## Backends

A backend is the physics engine that simulates the worlds. The built-in backend is Jolt Physics, named `jolt`. Jolt is a dependency that the engine pins, and the backend lives in `engine/src/3d/physics/jolt/`, the only place of the engine that includes the headers of Jolt, which a test of the repository checks. The headers of Jolt hold inline functions that use the SIMD instructions Jolt compiles with, so a source that included them with other flags would give one function two codes.

The backends live in the `physics3d::BackendRegistry` of the `plugins::Physics3DPlugin`. The built-in backend registers there the same way the backend of a plugin does, and new worlds take the backend that the registry selects, the first one registered unless a plugin selects another. A C++ plugin registers its own backend in its `start`, which runs after the one of the built-in plugin, and selects it.

```cpp
void AcmePhysicsPlugin::start(haylen::core::Engine& engine) {
    haylen::physics3d::BackendRegistry& backends = engine.getPlugin<haylen::plugins::Physics3DPlugin>().getBackends();
    backends.add(std::make_unique<AcmeBackend>());
    backends.select("acme");
}
```

A backend implements `physics3d::Backend`, which makes a `physics3d::Simulation` for each world. The world checks every value before it reaches the simulation, keeps the settings, reports the limits and ends the simulation when the world ends, so code that uses a world behaves the same with every backend.

## x64 processors

Jolt picks its SIMD instructions when it compiles, so every x64 build of the engine compiles with SSE4.2 and POPCNT, and the entry points of Windows and Linux stop with a clear sentence on a processor without them, as the [support table](../tools/distribution.md#platform-support) describes. ARM processors use Neon, 32-bit ARM uses plain math, and the web uses the SIMD of WebAssembly.
