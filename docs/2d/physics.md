# 🎱 Physics

This guide explains how 2D physics works in Haylen and how to tune it. It is for developers who make things fall, collide, stack, swing on joints or drive on wheels. It covers worlds, bodies, shapes and joints, units and scale, stepping, smooth drawing, threads, sleeping, collisions and queries. It then lists the problems that physics games meet most often, with the option or helper that solves each one, and ends with what each part costs. The [`haylen.physics2d` reference](../lua-api/physics2d.md) lists every function, option, default and error, with an example of each.

## What a world holds

Haylen simulates rigid bodies with Box2D, a physics library, behind its own API. Lua uses the module `haylen.physics2d`, and C++ uses the namespace `haylen::physics2d`.

A world is the space where the simulation runs. Create one with [`physics2d.newWorld`](../lua-api/physics2d.md#physics2dnewworldoptions). A world moves only when the app steps it. It holds three kinds of things:

- **Bodies** are the objects that move. A body has a position, a rotation and a velocity. Create one with [`world:createBody`](../lua-api/physics2d.md#worldcreatebodyoptions).
- **Shapes** give a body its collision. A body without shapes collides with nothing. Add shapes with [`body:addBox`](../lua-api/physics2d.md#bodyaddboxwidth-height-options) and the other `add` methods.
- **Joints** connect two bodies of the same world. Create one with [`world:createJoint`](../lua-api/physics2d.md#worldcreatejointtype-a-b-options).

A body has one of three types:

| Type | How it moves | Use it for |
| --- | --- | --- |
| `'static'` | It never moves and has no mass. | Floors, walls and terrain. |
| `'kinematic'` | It moves only by the velocity the app gives it. It pushes dynamic bodies, and they never push it back. | Moving platforms, doors and elevators. |
| `'dynamic'` | It responds to gravity, forces, contacts and joints. | Crates, balls, characters and debris. |

A shape is a box, a circle, a capsule, a polygon, a segment or a chain. Each shape has a material: its density, its friction, its restitution (how much it bounces) and its rolling resistance. Each shape also has a collision filter, which decides what it collides with, and switches for the events it raises.

A joint holds two bodies together in one of these ways:

- A hinge, `'revolute'`, such as a door or a wheel axle.
- A slider, `'prismatic'`, such as a piston or an elevator.
- A spring or a rope, `'distance'`.
- A weld, `'weld'`, which glues the bodies.
- A wheel on a suspension, `'wheel'`.
- A drag toward a point, `'mouse'`.
- A motor, `'motor'`, which drives one body toward a place next to the other.
- A filter, `'filter'`, which only keeps the two bodies from colliding.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.world = physics2d.newWorld({interpolate = true})
        local ground = self.world:createBody({type = 'static'})
        ground:addChain({{-800, 300}, {800, 300}}, false)
        self.crate = self.world:createBody({x = 0, y = 0})
        self.crate:addBox(48, 48, {density = 2, friction = 0.4})
        self.camera = graphics2d.newCamera()
    end,
    fixedUpdate = function(self, step)
        self.world:step(step)
    end,
    render = function(self)
        local x, y = self.crate:renderTransform()
        graphics2d.beginWorld(self.camera)
        graphics2d.drawRect({x - 24, y - 24, 48, 48}, '#FFC08040')
    end,
})
```

### How long things live

Body, shape and joint handles keep their world alive. A world goes away once no Lua value refers to it, to one of its handles or to one of its helpers.

Some helpers own bodies or act in every step: vehicles, movers, grabbers, force fields, terrains and fluids. Each one lives while a Lua value refers to it. When the garbage collector takes it, it leaves the world and takes with it what it added. So keep these helpers in a variable or in the table of their scene.

### Moving a body to another world

A body can move to another world with [`body:moveToWorld`](../lua-api/physics2d.md#bodymovetoworldworld). The engine rebuilds the body in the new world with its state, its shapes and its data. Then it destroys the body in the first world, together with its joints. From the next step the body obeys the world it landed in. Use this for an object that passes a portal, or that leaves one level for another.

## Units and scale

Positions, sizes and velocities are in world units. These are the units of the camera and of sprites, with y pointing down. Angles are in radians.

Box2D works in meters, kilograms and seconds. Each world converts between world units and meters with its `pixelsPerMeter` scale, which is 64 by default. So a box 64 units wide is one meter wide. Every value that the API takes or returns stays in world units and kilograms. So the same numbers give the same motion at every scale.

| Quantity | Unit | Example |
| --- | --- | --- |
| Position, size, distance | World units | A box of `addBox(64, 64)` is 64 units wide. |
| Velocity, speed limits and thresholds | World units per second | The world option `maxSpeed`, and `contactPushSpeed`, `restitutionThreshold`, `hitThreshold` and `sleepThreshold`. |
| Angle, angular velocity | Radians, and radians per second | A `rotation` of `math.pi` turns a body upside down. |
| Density | Kilograms per square meter | The mass of a shape is its density times its area in square meters. |
| Mass | Kilograms | The property `body.mass`. |
| Force, `maxMotorForce`, `breakForce`, `constraintForce` | Kilograms times units per second squared | The call `body:applyForce(0, -body.mass * 600)` pushes a body upward at 600 units per second squared. |
| Linear impulse | Kilograms times units per second | The call `body:applyImpulse(body.mass * 100, 0)` adds 100 units per second to the horizontal velocity. |
| Rotational inertia | Kilograms times squared units | The `body.inertia` of a disc of radius `r` is `body.mass * r * r / 2`. |
| Torque, `maxMotorTorque`, `breakTorque`, `constraintTorque` | Kilograms times squared units per second squared | The call `body:applyTorque(body.inertia * 3)` speeds the spin up by 3 radians per second every second. |
| Angular impulse | Kilograms times squared units per second | The call `body:applyAngularImpulse(body.inertia * 2)` adds 2 radians per second to the spin at once. |

So `body:applyForce(0, -body.mass * 980)` holds a body up against the default gravity at any scale.

### Choosing the scale

The scale matters for the solver, the part of Box2D that resolves contacts and joints. Box2D works best when moving bodies are between 0.1 and 10 meters across. Its tolerances are fixed in meters: it tells two points apart only when they are more than 0.005 meters apart. Its speed limit is 400 meters per second. Its default thresholds for bounces and hits are 1 meter per second.

A body much smaller than a tenth of a meter collides poorly. A body of many meters moves like a slow giant. At the default 64 units per meter, moving bodies should be between about 6 and 640 units across. A character of 48 to 128 units is ideal.

If your sprites are much smaller or larger, pick the scale that makes your characters about one or two meters tall. For example, a game of 16-pixel tiles works well at 32 units per meter. The scale also changes how heavy a shape is, because density is per square meter. It also changes the default speeds of the world, which follow the scale.

## Stepping

A world advances only when [`world:step(seconds)`](../lua-api/physics2d.md#worldstepdeltaseconds) runs. Call it in the `fixedUpdate` callback of a scene. The engine calls `fixedUpdate` at the fixed rate of `fixedRate` in `app.json`, 60 times per second by default, with the same step length every time.

A fixed step keeps the simulation stable, and it makes the simulation repeat exactly, while the frame rate of the display changes. One frame may run zero, one or several fixed steps. So input that `update` reads reaches the simulation through the state of the scene. The [mover example](../lua-api/physics2d.md#physics2dnewmoverworld-options) of the reference keeps a jump this way until the next fixed step.

### What one step does

One step does this, in order:

1. It releases the `data` of the bodies destroyed since the last step.
2. Vehicles and force fields apply their forces.
3. Box2D advances the bodies in the sub-steps of the world, on its threads.
4. Joints loaded beyond their break limits break.
5. A world that interpolates records where its bodies moved.
6. Fluids move their particles and push the bodies they hit.
7. The event callbacks of the world run for the contacts, hits, sensor overlaps and broken joints of the step.

Vehicles, force fields and fluids act through the step hooks of the world. So they run in every step without a call from the app.

### Sub-steps

Box2D solves each step in several sub-steps, 4 by default. It finds contacts only once per step. More sub-steps make stacks, ragdolls, chains and suspensions stiffer, and they settle faster. The cost grows with them. The physics benchmark steps a pyramid of 820 boxes in 0.27 milliseconds with 2 sub-steps, 0.36 with 4 and 0.54 with 8.

Box2D also caps how stiff contacts and joints can be. Contacts reach at most an eighth of the sub-step rate, and joints a quarter of it. So at 60 steps per second with 4 sub-steps, contacts reach at most 30 hertz and joints 60 hertz. Stiffer settings need more sub-steps. The property `world.subSteps` changes them while the world runs.

### Smooth drawing between fixed steps

A display that refreshes 120 or 144 times per second shows several frames for each fixed step of 60. A display at 50 frames per second skips steps. So bodies drawn where they are move in uneven jumps.

Interpolation fixes this. A world created with `interpolate = true` records the transforms of the last two steps of every body that moved. It then draws each body between them, by the interpolation of the frame that [`haylen.interpolation()`](../lua-api/haylen.md#hayleninterpolation) returns. Read one body with [`body:renderTransform()`](../lua-api/physics2d.md#bodyrendertransform), which returns its blended `x`, `y` and `rotation`. Read thousands of bodies into a float buffer in one call with [`world:readTransforms(bodies, buffer, 1, true)`](../lua-api/physics2d.md#worldreadtransformsbodies-buffer-first-interpolated).

Interpolated bodies show up to one fixed step behind the simulation, which nobody notices at 60 steps per second. A body that a teleport placed shows where it is, so a respawn never slides across the screen. Interpolation has a small cost. Reading the interpolated transforms of 5000 bodies takes 0.043 milliseconds, against 0.026 without. Recording them also costs a little in each step. So a world that draws only static or slow bodies leaves interpolation off.

```lua
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld({interpolate = true})
local ground = world:createBody({type = 'static', x = 960, y = 1060})
ground:addBox(1920, 40)
local bodies = {}
for index = 1, 1000 do
    local crate = world:createBody({x = math.random(100, 1800), y = math.random(-3000, 0)})
    crate:addBox(16, 16)
    bodies[index] = crate
end
local transforms = collections.newFloatBuffer(#bodies * 3)
local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:resize(#bodies, {width = 16, height = 16, color = '#FFB07040'})

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    update = function(self, dt)
        world:readTransforms(bodies, transforms, 1, true)
        batch:writeFields(transforms, {'x', 'y', 'rotation'})
    end,
    render = function(self)
        graphics2d.beginScreen()
        batch:draw()
    end,
})
```

## Threads

Box2D splits a step into tasks, such as finding contacts, or solving the constraints of one color of the graph of contacts. The engine runs those tasks on its job system through the private class `StepTasks`. Each task splits into at most one chunk per thread of the world. The workers of the task pool take chunks as they come. The thread that steps runs every chunk that no worker took yet. So a step never waits behind other work queued on the pool, such as a navigation mesh that builds in the background. A task of one chunk runs on the stepping thread alone.

A world steps on the number of threads of its `threads` option. The default is the number of workers of the job system, but at most 4, because the solver gains little from more workers and slow cores hold it back. A world uses at most 64 threads, whatever it asks for. It uses its threads only while at least 1000 of its bodies are awake. Below that, waking the workers costs more than they save. So a world of a few hundred bodies steps on the frame thread at no extra cost. The web build has one thread, so every world there steps on one thread and `world.threads` reads 1.

Threads never change the result. Box2D solves in the same order on any number of threads. So a world steps to the same positions and velocities, bit for bit, on one thread or on eight. The method [`world:stateHash(bodies)`](../lua-api/physics2d.md#worldstatehashbodies) returns the same number for both. It also checks that a replay matches its recording. The physics benchmark steps a pile of 4000 falling boxes in 2.12 milliseconds on one thread, 1.75 on two, 1.53 on four and 1.52 on eight.

Fluids run their passes with `JobSystem::parallelFor`, and ray batches spread their rays the same way, whatever the threads of the world.

## Sleeping

Bodies that touch, or that a joint connects, form an island. An island falls asleep once every body in it has moved slower than its sleep threshold for half a second. Sleeping bodies cost nothing. The physics benchmark steps a settled pile of 4000 boxes in less than a thousandth of a millisecond while it sleeps, and in about 1.6 milliseconds with sleeping off.

A sleeping body wakes in these cases:

- An awake body touches it.
- A force, an impulse or a velocity reaches it.
- One of its joints changes.
- The gravity of the world changes.
- The app writes `body.awake = true` or calls [`world:wakeAll()`](../lua-api/physics2d.md#worldwakeall).

The sleep threshold is 0.05 meters per second by default, which is 3.2 units per second at the default scale. The body option `sleepThreshold` changes it for one body. A body that should never sleep sets `sleepEnabled = false`, such as a player character driven by forces. Setting `world.sleepEnabled = false` keeps every body of the world awake, for a level that must keep moving.

Force fields never wake a body when they push it. Fluid particles wake a body only when they change its speed by more than 0.05 meters per second. So crates that float in still water fall asleep. When a rule of your game changes something the solver cannot see, such as making resting bodies fall again, call `world:wakeAll()`.

## Collisions and events

### Events

A world reports what happened in a step through callbacks that the app sets on it. The callbacks run inside `world:step()`, after Box2D has advanced the bodies.

| Callback | When it runs |
| --- | --- |
| `world.onContactBegin` | Two solid shapes start touching. |
| `world.onContactEnd` | Two solid shapes stop touching. |
| `world.onHit` | Two solid shapes hit each other faster than the hit threshold of the world. |
| `world.onSensorBegin` | A shape starts to overlap a sensor, a shape that detects others and never collides. |
| `world.onSensorEnd` | A shape stops overlapping a sensor. |
| `world.onJointBreak` | A step broke a joint because its load passed its break limit. |

Each shape has switches for its events: `contactEvents`, `hitEvents` and `sensorEvents`. Shapes that nobody listens to, such as debris, turn them off, which also saves the cost of the events reaching Lua. The [reference](../lua-api/physics2d.md#contacts-and-events) lists the arguments of each callback.

### What a contact tells

Every contact tells what happened in the step that solved it. The tables of `onContactBegin` and `onHit`, and the list of [`body:contacts()`](../lua-api/physics2d.md#bodycontacts), hold these facts, as the [contact tables](../lua-api/physics2d.md#contact-tables) list:

- **The points** where the shapes touch. A circle on a floor touches at one point, and a box lying flat at two. Each point has the distance between the surfaces, which is negative while they overlap.
- **The normal**, a direction from the first shape toward the second. It tells the side each shape was hit from, such as whether a character landed on an enemy or ran into it.
- **The approach speed**, how fast the shapes closed in along the normal before the step solved them. It tells how hard an impact was, so it suits damage, sounds and shaking. It is about 0 for bodies at rest.
- **The relative velocity** of the bodies at the contact after the step, such as how fast a crate slides along the floor or a wheel spins on it.
- **The force** that presses the shapes together at the end of the step. For a body at rest it is its weight. So a pressure plate, a scale or a crate that crushes what lies under it can read it. Each point also has its impulses along the normal and along the surface.

Box2D records the manifold of a new contact, its points and normal, before it solves the step. The engine reads the contact as the step left it, with its impulses and approach speeds. For a contact that ended within the same step, it keeps the manifold of the event. In C++ the class `physics2d::ContactManifold` holds the same values, in the `manifold` of a `ContactEvent` and of a `Body::Contact`. The test project shows contacts in `PHY-064`.

### Who collides with whom

Every shape has a category, a mask and a group. Two shapes collide when the category of each one shares a bit with the mask of the other. A shared group overrides both: shapes with the same positive group always collide, and shapes with the same negative group never do. The parts of a ragdoll or a vehicle share a negative group, so they never hit each other. The [reference](../lua-api/physics2d.md#collision-filtering) shows the rule with an example.

## Finding bodies

Besides events, the app can ask the world what lies somewhere. These questions never change the world.

| You want to | Use |
| --- | --- |
| Find the shapes in a rectangle, around a circle or under a point | [`world:queryRect`](../lua-api/physics2d.md#worldqueryrectrect-filter), `world:queryCircle` and `world:queryPoint` |
| Select bodies with a tap or a click, with a margin for a finger | [`world:pick`](../lua-api/physics2d.md#worldpickcamera-x-y-filter) |
| Find what a line hits first, or every hit along it | [`world:raycast`](../lua-api/physics2d.md#worldraycastx1-y1-x2-y2-filter) and `world:raycastAll` |
| Move a circle, a box, a capsule or a polygon along a path and find its first hit | `world:castCircle`, `world:castBox`, `world:castCapsule` and `world:castPolygon` |
| Bounce a laser between mirrors, or spread a cone of vision | `world:bounceRay` and `world:rayFan` |
| Check whether a guard sees the player | `world:lineOfSight` |
| Cast thousands of rays at once, such as the sensors of many agents | [`world:raycastBatch`](../lua-api/physics2d.md#worldraycastbatchbatch-filter) with a `RayBatch` |
| Draw the aim of a throw that matches the real flight | [`world:predictPath`](../lua-api/physics2d.md#worldpredictpathx-y-vx-vy-options) |
| See the rays while you tune them | `world.debugRays` and `world:debugDrawRays` |

Ray casts take filters with a category, a mask and a group, like shapes, and an `accept` function that can skip a hit. So a bullet can pass through glass, or a ragdoll can look past its own limbs.

## Helpers

The engine builds common game objects on top of bodies and joints. Each helper runs in C++, so it costs no Lua call per body.

| Helper | What it makes | Test |
| --- | --- | --- |
| [`physics2d.newRope`](../lua-api/physics2d.md#physics2dnewropeworld-options) and `physics2d.newBridge` | A chain of segments, pinned or hung from bodies, and a bridge of planks. | `PHY-005`, `PHY-006` |
| [`physics2d.newRagdoll`](../lua-api/physics2d.md#physics2dnewragdollworld-options) | A human figure of eleven parts with the angle limits of real joints. | `PHY-007` |
| [`physics2d.newVehicle`](../lua-api/physics2d.md#physics2dnewvehicleworld-options) | A car seen from the side, on two wheels with springy suspension. | `PHY-008` |
| [`physics2d.newTopDownVehicle`](../lua-api/physics2d.md#physics2dnewtopdownvehicleworld-options) | A car seen from above, which grips, drifts and skids. | `PHY-028` |
| [`physics2d.newMover`](../lua-api/physics2d.md#physics2dnewmoverworld-options) | A character that walks on slopes, steps up stairs and rides platforms. | `PHY-021`, `PHY-031` |
| [`physics2d.newGrabber`](../lua-api/physics2d.md#physics2dnewgrabberworld-options) | A hand that drags bodies with a mouse or a finger. | `PHY-027` |
| [`physics2d.newForceField`](../lua-api/physics2d.md#physics2dnewforcefieldworld-options) | An area with wind, a magnet, a planet or water. | `PHY-048` to `PHY-051` |
| [`physics2d.explode`](../lua-api/physics2d.md#physics2dexplodeworld-options) | A blast that pushes bodies away from a point. | `PHY-009` |
| [`physics2d.newTerrain`](../lua-api/physics2d.md#physics2dnewterrainworld-options) | Ground that bombs and tools carve and fill. | `PHY-010` |
| [`physics2d.fracture`](../lua-api/physics2d.md#physics2dfracturebody-options) | Pieces of a body that breaks where it was hit. | `PHY-011` |
| [`physics2d.newFluid`](../lua-api/physics2d.md#physics2dnewfluidworld-options) | A liquid of particles where crates float or sink. | `PHY-012`, `PHY-059` |

Shapes also have two built-in behaviors. A shape with a `oneWay` direction is a [one-way platform](../lua-api/physics2d.md#one-way-platforms). A shape with a `tangentSpeed` is a [conveyor belt](../lua-api/physics2d.md#conveyors) (`PHY-014`).

## Common problems and their solutions

Physics games meet the same problems again and again. Each problem below names the option or helper that solves it. The [test project](../../samples/tests/README.md) shows each one in the test that the table names, most of them with the problem and the solution side by side.

| Problem | Solution | Test |
| --- | --- | --- |
| [Fast bodies pass through walls](#fast-bodies-pass-through-walls) | The world option `continuous`, the body option `bullet` and `maxSpeed` | `PHY-019` |
| [Bodies catch on the joints between tiles](#bodies-catch-on-the-joints-between-tiles) | Chains, and the merged collision of Tiled maps | `PHY-020`, `TLD-013` |
| [Platforms to jump through and drop through](#platforms-to-jump-through-and-drop-through) | The shape option `oneWay` and `dropThrough` | `PHY-013`, `PHY-031` |
| [Characters on slopes and steps](#characters-on-slopes-and-steps) | The mover of `physics2d.newMover` | `PHY-021`, `PHY-031` |
| [Moving platforms that drop their loads](#moving-platforms-that-drop-their-loads) | `body:moveTo` on a kinematic body | `PHY-022` |
| [Fast falls onto thin platforms](#fast-falls-onto-thin-platforms) | Continuous collision, `bullet` and `maxSpeed` | `PHY-019` |
| [Joints that stretch or explode](#joints-that-stretch-or-explode) | Sub-steps, mass ratios, the length limit of ropes and break limits | `PHY-023`, `PHY-030` |
| [Stacks that jitter and bodies that fly apart](#stacks-that-jitter-and-bodies-that-fly-apart) | Sub-steps, `contactHertz` and `contactPushSpeed` | `PHY-024`, `PHY-046` |
| [Motion that stutters](#motion-that-stutters) | The world option `interpolate` | `PHY-025` |
| [Small bounces and floods of hit events](#small-bounces-and-floods-of-hit-events) | `restitutionThreshold`, `hitThreshold` and the event switches | `PHY-026` |
| [Dragging bodies with a mouse or a finger](#dragging-bodies-with-a-mouse-or-a-finger) | The grabber of `physics2d.newGrabber` | `PHY-027` |
| [Ragdolls that get stuck](#ragdolls-that-get-stuck) | The `stiffness` of `physics2d.newRagdoll` | `PHY-007`, `PHY-027` |
| [Vehicles that flip](#vehicles-that-flip) | `physics2d.newVehicle` and `physics2d.newTopDownVehicle` | `PHY-008`, `PHY-028` |
| [Tall bodies that tip over](#tall-bodies-that-tip-over) | The body property `centerOfMass` | `PHY-029` |
| [Balls that roll forever](#balls-that-roll-forever) | The shape option `rollingResistance` | `PHY-036`, `PHY-042`, `PHY-044` |
| [Wind, magnets, planets and water](#wind-magnets-planets-and-water) | Force fields | `PHY-048` to `PHY-051` |
| [Aiming throws](#aiming-throws) | `world:predictPath` | `PHY-035` |
| [Picking thin or small bodies](#picking-thin-or-small-bodies) | The `radius` of `world:pick` and the `pickRadius` of a grabber | `PHY-027` |
| [Liquids](#liquids) | The fluid of `physics2d.newFluid` | `PHY-012`, `PHY-059` |

### Fast bodies pass through walls

A body that moves farther in one step than a wall is thick can end the step on the far side of the wall. This is called tunneling.

The world option `continuous` is on by default. It sweeps every fast dynamic body against static shapes along its path. So balls, characters and debris stop at thin walls, floors and terrain. The sweep skips dynamic and kinematic shapes. A projectile reaches those with the body option `bullet = true`. A bullet sweeps against every body, so it stops at a thin moving door or a thin dynamic plank. Each bullet costs a sweep, which [`world:stats()`](../lua-api/physics2d.md#worldstats) reports as `continuousMilliseconds`. So give `bullet` only to small fast bodies, such as arrows and bullets.

Other settings help too. More sub-steps make contacts stiffer, so a fast heavy body does not push into a wall or a pile. A higher `fixedRate` in `app.json` shortens how far a body moves between two steps. The world option `maxSpeed` caps the speed of every body, 400 meters per second by default. A body asked to go faster moves at the cap. A lower cap keeps everything within what continuous collision catches well.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}, continuous = true, maxSpeed = 20000})
local arrow = world:createBody({x = 0, y = 0, vx = 6000, bullet = true})
arrow:addBox(24, 4)
```

### Bodies catch on the joints between tiles

A box that slides over a floor made of separate boxes or segments can stop dead at the seams, or hop. At a seam, its contact can point sideways into the next box, even when the surfaces line up exactly. These are called ghost collisions.

A chain of segments, made with [`body:addChain`](../lua-api/physics2d.md#bodyaddchainpoints-loop-options), solves this. A chain knows the neighbors of each segment and smooths the contacts at its joints. So bodies slide along floors, walls and slopes without catching. An open chain collides along every segment between the points it lists: the world extends its first and last segments with the points that smooth the contacts at its ends. A loop closes an outline.

Tiled maps get this for free. The collision that [`map:buildCollision`](tiled.md#collision) builds from an orthogonal map merges the solid shapes of touching tiles into chain loops around each solid region. So a level of square and sloped tiles behaves like one smooth outline. Round shapes help too: a character made of a capsule or a circle rides over a small step where a box would catch.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local floor = world:createBody({type = 'static'})
-- One chain instead of one box per tile, listed from left to right so bodies stand on top of it.
floor:addChain({{0, 400}, {320, 400}, {448, 336}, {800, 336}}, false)
```

### Platforms to jump through and drop through

A one-way platform lets bodies jump through it from below and land on it from above. Make one with the shape option `oneWay = {0, -1}`, the direction that its open side faces in the space of its body.

A contact with the platform holds only while it leans at most 60 degrees from that direction. So bodies also pass the platform from its sides. A body lands only when it was outside the platform at the start of the step, or at most 0.02 meters inside it. So a jump that rises halfway into a platform falls back under it, instead of popping up on top. A body that the platform held in the last step keeps standing on it, however deep a fast landing took it, while the solver pushes it out. So a character that lands from a high jump stays on the platform. A mover follows the same rules.

To fall through the platform a body stands on, call [`body:dropThrough(seconds)`](../lua-api/physics2d.md#bodydropthroughseconds) or [`mover:dropThrough(seconds)`](../lua-api/physics2d.md#moverdropthroughseconds). The body passes every platform for 0.2 seconds by default, and then until it leaves the platform it is inside. So it lands on the next platform below. Tiled objects and tile shapes whose `oneWay` property is `true` become such platforms.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ledge = world:createBody({type = 'static', x = 0, y = 0})
ledge:addBox(200, 8, {oneWay = {0, -1}})
local crate = world:createBody({x = -60, y = -40, fixedRotation = true})
crate:addBox(24, 24)
local hero = physics2d.newMover(world, {x = 60, y = -40})
-- Called when the player presses down while standing on the ledge.
crate:dropThrough()
hero:dropThrough()
```

### Characters on slopes and steps

A dynamic body with friction makes a poor character:

- On a slope, its friction must be high to keep it from sliding down while it stands still. Then it sticks to walls and drags on the ground.
- It leaves the ground at every crest and bounces down slopes.
- It needs a jump for every small step.
- The solver pushes it around when crates press on it.

The helper [`physics2d.newMover`](../lua-api/physics2d.md#physics2dnewmoverworld-options) makes a kinematic character instead. It is an upright capsule that moves by the distance the app asks for and slides along what it hits. It stands still on slopes up to `maxSlope` and walks up them at full speed. It steps onto ledges up to `stepHeight`. It stays on the ground over crests and down slopes within `snapDistance`. It stops at walls and ceilings, jumps up through one-way platforms and lands on them, and pushes crates with at most `pushForce`.

The app keeps the velocity. In each fixed step it adds gravity, clips the velocity with `mover:clip`, moves with `mover:move` and clips again. The properties `grounded`, `onWall`, `onCeiling` and `groundNormal` tell what the mover touched. The reference shows the whole loop.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local hero = physics2d.newMover(world, {x = 0, y = 0, radius = 14, height = 56, maxSlope = math.rad(45), stepHeight = 20, snapDistance = 10})
```

### Moving platforms that drop their loads

A platform moved by a teleport, with `body:setTransform` or by writing its `x` and `y`, shoves aside what it lands in and leaves the bodies on it behind. A teleport has no velocity, so friction cannot carry anything along.

A kinematic platform that moves by velocity carries bodies. The method [`body:moveTo(x, y, rotation)`](../lua-api/physics2d.md#bodymovetox-y-rotation-seconds) gives it the velocity that reaches the target in one fixed step. So a platform that follows a path, a tween or an animation calls it once per fixed step, with the next position, before `world:step()`. The velocity stays until something changes it. A call with the place the platform already holds stops it.

Movers ride the body they stand on by following the point under their feet. The property `mover.groundVelocity` tells the speed of that point. A jump from a moving platform adds it to its own speed.

```lua
local physics2d = require('haylen.physics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local lift = world:createBody({type = 'kinematic', x = 0, y = 300})
lift:addBox(160, 16)

scene.push({
    fixedUpdate = function(self, step)
        local time = haylen.elapsed() + step
        lift:moveTo(0, 300 - (math.sin(time) + 1) * 150)
        world:step(step)
    end,
})
```

### Fast falls onto thin platforms

A body that falls a long way gets fast enough to cross a thin platform within one step. Continuous collision, on by default, catches it on static platforms. One-way platforms land fast bodies wherever they reach them. A moving platform is kinematic, and continuous collision skips kinematic shapes. So bodies that fall fast onto moving platforms set `bullet = true`. The world option `maxSpeed` caps the speed of every fall. A cap near the fastest speed the game needs keeps falls within reach of the sweeps.

### Joints that stretch or explode

A chain of many bodies stretches under a heavy load, because the solver cannot hold every joint exactly within one step. A heavy body hung from light ones stretches it most. These settings help:

- More sub-steps make joints stiffer.
- Keep the masses of joined bodies within about ten to one of each other. Then the solver stays accurate. So a heavy lamp hangs from a few strong links rather than many light ones.
- A rope made with [`physics2d.newRope`](../lua-api/physics2d.md#physics2dnewropeworld-options) adds a slack distance joint between its two ends, unless `limitLength` is `false`. It holds a load a hundred times heavier than a segment at the length of the rope. The limit holds only while the rope is whole. In the next step after a link is cut, breaks or loses its segment, the limit goes too, and the pieces fall apart.
- Each joint holds itself together with a stiffness of `constraintHertz`, 60 by default, damped by `constraintDampingRatio`, 2 by default. Lower values make a joint soft on purpose, like rubber.
- A joint that should give way under a load breaks at `breakForce` or `breakTorque`, instead of tearing its bodies apart. The callback `world.onJointBreak` reports it, as [breaking joints](../lua-api/physics2d.md#breaking-joints) shows.

Joints explode when they fight each other or fight the contacts. Two joints that pin a body at different points do this, and so do bodies held inside each other. So let joined bodies keep their own space, or keep `collideConnected` off, which is the default.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({subSteps = 8})
local anvil = world:createBody({x = 0, y = 400})
anvil:addCircle(24, {density = 40})
local chain = physics2d.newRope(world, {from = {0, 0}, to = {0, 380}, segments = 20, pinStart = true, endBody = anvil, limitLength = true})
```

### Stacks that jitter and bodies that fly apart

Contacts act as soft springs. Their stiffness is `contactHertz`, 30 by default. Their damping is `contactDampingRatio`, 10 by default. Together they decide how fast overlapping shapes separate, but never faster than `contactPushSpeed`, 3 meters per second by default.

Bodies spawned inside each other, or squeezed by a heavy load, fly apart when the push is strong. A lower `contactPushSpeed` separates them gently. Spawning bodies apart avoids the problem. A tall stack that sways or jitters settles with more sub-steps, which also allow a stiffer `contactHertz`. Once a stack settles, sleeping stops it entirely.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({subSteps = 8, contactHertz = 40, contactPushSpeed = 120})
```

### Motion that stutters

Bodies that move in uneven jumps on a fast display, or that shake while the camera follows them, are drawn where the last fixed step left them. A world created with `interpolate = true` draws them between their last two steps, as [smooth drawing between fixed steps](#smooth-drawing-between-fixed-steps) describes. A camera that follows a body should follow its interpolated position too.

### Small bounces and floods of hit events

A bouncy ball at rest keeps bouncing a little, and every tiny bounce of a pile calls `onHit`. Two thresholds of the world control this:

- Impacts slower than `restitutionThreshold`, 1 meter per second by default, never bounce. Raise it to stop the small bounces. Lower it to let slow balls with full restitution bounce.
- Impacts slower than `hitThreshold`, 1 meter per second by default, call no `onHit`. A game that plays impact sounds raises it to the speed of an impact you can hear.

Shapes that should stay quiet, such as debris, particles and decorations, turn off `contactEvents`, `hitEvents` and `sensorEvents`. This also saves the cost of the events reaching Lua. A contact raises its events when either shape has the switch on. So turn the switch off on both sides of the contacts that should stay quiet.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({restitutionThreshold = 96, hitThreshold = 300})
local pebble = world:createBody({x = 0, y = 0})
pebble:addCircle(4, {contactEvents = false, hitEvents = false})
```

### Dragging bodies with a mouse or a finger

A mouse joint pulls one body toward the pointer with a force sized for that one body. So a heavy chain or a ragdoll held by one hand barely moves, while a pebble flies off.

The helper [`physics2d.newGrabber`](../lua-api/physics2d.md#physics2dnewgrabberworld-options) solves this. It takes the dynamic body nearest to the pointer within its `pickRadius`, and holds it by the point it took. It pulls with a soft spring whose force is `strength` times the weight of the held body and of everything joined to it. So one hand of a ragdoll drags the whole figure, and a chain follows as well as a coin. A finger covers more than one point, so a pick radius the size of a fingertip finds thin and small bodies near the touch.

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')

local world = physics2d.newWorld()
local hand = physics2d.newGrabber(world, {pickRadius = 24, strength = 30})

-- Called every frame with the pointer in world units.
local function drag(x, y)
    if input.mousePressed() then
        hand:grab(x, y)
    elseif input.mouseReleased() then
        hand:release()
    end
    hand:moveTo(x, y)
end
```

### Ragdolls that get stuck

A limp ragdoll folds into the corners of stairs and around the edges of steps. Its limbs catch on geometry while the rest of the figure is dragged on.

A ragdoll made with [`physics2d.newRagdoll`](../lua-api/physics2d.md#physics2dnewragdollworld-options) resists bending with `stiffness`. Each joint gets a motor sized from the weight of the limb below it, so the same stiffness holds a small and a large figure alike, at any scale. Its capsule shapes slide over edges where boxes would catch. The angle limits of real joints keep its poses natural.

Figures that should collide with each other take a negative `group` each, because parts that share a negative group never collide. A grabber drags a figure by one hand over a step instead of tearing that hand off.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local first = physics2d.newRagdoll(world, {x = 0, y = 0, stiffness = 0.3, group = -1})
local second = physics2d.newRagdoll(world, {x = 200, y = 0, stiffness = 0.3, group = -2})
```

### Vehicles that flip

A car built from a box and two motorized wheels flips easily. Its center of mass sits in the middle of the box, high above the axles. A motor with a large fixed torque spins the wheels up at once, which lifts the nose.

The vehicle of [`physics2d.newVehicle`](../lua-api/physics2d.md#physics2dnewvehicleworld-options) avoids this:

- It puts the center of mass at the height of the axles.
- It sizes the torque of its motors from its mass and the `acceleration` it should have. So the throttle speeds it up evenly, up to `topSpeed`.
- It couples the springs of its axles with `antiRoll`, which keeps the chassis level when it speeds up and brakes.
- Its wheels use `fastRotation`. Box2D otherwise limits every body to 45 degrees of rotation per step, which caps the speed of small fast wheels.
- In the air, the throttle turns the chassis with `airControl`, so a player can land on the wheels.
- The `brake` holds the wheels with the deceleration of `brakeAcceleration`.

A car seen from above is [`physics2d.newTopDownVehicle`](../lua-api/physics2d.md#physics2dnewtopdownvehicleworld-options), in a world without gravity. Its tires grip sideways up to `grip`, so it drifts when a turn asks for more. The handbrake keeps only `handbrakeGrip` of the rear grip. The steering lock shrinks as the speed grows.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local buggy = physics2d.newVehicle(world, {x = 0, y = 200, drive = 'all', acceleration = 700, topSpeed = 1100, antiRoll = 0.6, airControl = 5})
local space = physics2d.newWorld({gravity = {0, 0}})
local racer = physics2d.newTopDownVehicle(space, {grip = 1600, handbrakeGrip = 0.2, drive = 'rear'})
```

### Tall bodies that tip over

A body's center of mass is where its shapes put it, usually in its middle. A tall block with its center of mass high falls over at a small push. Write the body property [`centerOfMass`](../lua-api/physics2d.md#body-properties) to move it low in the body, like the keel of a boat. The block then rocks back upright. The method `body:resetMass()` puts the center of mass back where the shapes put it.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local buoy = world:createBody({x = 0, y = 0})
buoy:addBox(30, 120)
-- Low in the body, 50 units below its middle, so it stands back up.
buoy.centerOfMass = {0, 50}
```

### Balls that roll forever

Friction stops a body that slides. A ball rolls without sliding, so on a flat floor it rolls on until something hits it. Rolling resistance slows round shapes that roll.

A contact takes the larger `rollingResistance` of its two shapes. So grass that slows every ball sets it on the ground, and a heavy ball that rolls less sets it on the ball. A ball with a rolling resistance of 0.3 stops from 300 units per second within a few seconds. The cannon wheels of `PHY-036` roll with resistance. The tests `PHY-042` and `PHY-044` slow their balls with damping instead.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local lawn = world:createBody({type = 'static', x = 0, y = 300})
lawn:addBox(2000, 40, {rollingResistance = 0.2})
```

### Wind, magnets, planets and water

A force applied from Lua to every body in an area costs one call per body per step. A force field of [`physics2d.newForceField`](../lua-api/physics2d.md#physics2dnewforcefieldworld-options) does the same work in C++, before every step:

- A `'radial'` field pulls toward its center, or pushes away with a negative strength. Use it for magnets, attractors and planets.
- A `'directional'` field pushes one way. Use it for wind and jets.
- A `'vortex'` field swirls.
- A `'buoyancy'` field is water. It floats each shape by the area it has under the surface. So bodies lighter than the water float with the right share under it, and heavier ones sink.

The strength is an acceleration, the same for every body, unless `acceleration` is `false`, which pushes light bodies further. A field can fade with distance, with a `'linear'` falloff or an `'inverseSquare'` one, which works like gravity. Drags slow bodies toward a `flow`, for currents and air resistance. A field never wakes the bodies it pushes, so floating bodies fall asleep.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local pool = physics2d.newForceField(world, {kind = 'buoyancy', x = 0, y = 300, width = 800, height = 200, density = 1.2, linearDrag = 1})
local wind = physics2d.newForceField(world, {kind = 'directional', x = -600, y = 0, width = 300, height = 600, strength = 900, direction = {1, 0}})
```

### Aiming throws

A trajectory drawn with the formula of a parabola misses where the body really lands. The solver integrates gravity, damping and the speed limit in sub-steps, and the formula does not.

The method [`world:predictPath(x, y, vx, vy, options)`](../lua-api/physics2d.md#worldpredictpathx-y-vx-vy-options) follows the same integration as the solver, with the gravity scale and the damping of the thrown body. It sweeps a circle of the body's radius through the world. It returns the points the throw passes and the shape it would hit. So the aim of a slingshot matches the real throw.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local points, hit = world:predictPath(-400, 200, 600, -700, {radius = 8})
print(#points, hit and hit.body)
```

### Picking thin or small bodies

A tap or a click finds a body only when the point lies inside one of its shapes. That misses a thin rope or a small coin under a finger. The filter of [`world:pick(camera, x, y, filter)`](../lua-api/physics2d.md#worldpickcamera-x-y-filter) takes a `radius` in design coordinates. The pick then finds the shapes within that distance of the point too, nearest first, with the shapes that contain the point before the others. A grabber takes the same kind of radius in world units, as `pickRadius`. Every test that drags bodies uses it.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')

local world = physics2d.newWorld()
local camera = graphics2d.newCamera()
local picked = world:pick(camera, 400, 300, {radius = 16})
print(#picked)
```

### Liquids

A liquid made of circle bodies costs a body and its contacts per drop, and its drops never hold together. A fluid of [`physics2d.newFluid`](../lua-api/physics2d.md#physics2dnewfluidworld-options) keeps its particles outside Box2D, in flat arrays that a spatial hash sorts every step. It spreads its passes over the job system.

How a fluid behaves:

- Every step pushes apart the particles that came closer than the spacing of the liquid, in a few density passes. So the liquid keeps its volume under its own weight, like water.
- The passes move a particle at most 3 meters per second. So a body that falls in squeezes the liquid for a moment instead of throwing it everywhere.
- The particles collide with the shapes of the world along their path over the step. So walls thinner than a particle hold the liquid at any speed and step length.
- The particles push the dynamic bodies they hit, so crates float or sink by their density. A body takes those pushes only until it moves away from the particles as fast as they came at it. So a crate far lighter than the liquid floats still instead of bouncing off it, and a slow push only raises the surface by what it displaces.
- More `iterations` compress a deep liquid less. More `viscosity` calms it sooner.
- The method [`fluid:draw`](../lua-api/physics2d.md#fluiddrawoptions) draws the particles as metaballs, straight from the arrays.

A fluid costs about as much as the step of a few thousand boxes. A tank of 1800 particles with six crates steps in 0.57 milliseconds, 4000 particles in 1.0 and 8000 in 1.45. The property `fluid.stepMilliseconds` tells what a fluid takes in your scene.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local tank = world:createBody({type = 'static'})
tank:addChain({{-300, -400}, {-300, 200}, {300, 200}, {300, -400}}, false)
local water = physics2d.newFluid(world, {maxParticles = 3000})
water:fill({-280, -200, 560, 200})
```

## Performance

The cost of a world grows with what is awake and touching:

- The step costs in proportion to the awake bodies and their contacts and joints. Sleeping bodies cost nothing, so letting bodies sleep is the largest saving there is.
- Sub-steps multiply the cost of solving. Contacts are found once per step.
- Threads shorten the step of a world of more than 1000 awake bodies, up to about four threads.
- Bullets cost a sweep against every nearby body in each step. Continuous collision against static shapes costs little.
- Chains are cheaper than boxes for terrain. A chain segment has no area, and its neighbors smooth its contacts. One merged outline replaces many tiles.
- Queries cost in proportion to the shapes they find. Every ray cast costs a walk through the tree of shapes. Send thousands of rays per frame through `world:raycastBatch`, which spreads them over the job system.
- Events cost a Lua call each. So shapes that raise events nobody reads turn their switches off.
- Reading positions one property at a time costs a call across the Lua boundary per value. The method `world:readTransforms` reads thousands of bodies in one call.
- Fluids cost in proportion to their particles and the neighbors of each particle.

### Debug and Release builds

A Debug build of the engine has no optimizations, so it runs physics several times slower than a Release build. A pile of 4000 falling boxes takes about 27 milliseconds a step on one thread in a Debug build, and 2.1 in Release. The desktop player of `haylen run` and the apps built for each platform use the Release engine. The player runs a Debug engine only with `--engine-config Debug`, to debug the engine itself. Measure physics with `haylen bench --suite physics`, as the [build guide](../tools/build.md#bench) describes.

### The physics benchmark

The command `haylen bench --suite physics` builds `haylen-physics-benchmark` in Release and runs it on the headless host. It prints the workers of the job system. Then, for each scene, it prints the average time of one step or one operation in milliseconds. These numbers come from an Apple silicon Mac with a Release build. Absolute values vary from machine to machine and from run to run.

| Scene | What it measures | Average ms |
| --- | --- | --- |
| Pile of 4000 boxes falling, 1 thread | A step of 4000 boxes of 20 units that fall into a pit and pile up, on one thread. | 2.12 |
| Pile of 4000 boxes falling, 2 threads | The same pile on two threads. | 1.75 |
| Pile of 4000 boxes falling, 4 threads | The same pile on four threads. | 1.53 |
| Pile of 4000 boxes falling, 8 threads | The same pile on eight threads. | 1.52 |
| Pile of 4000 boxes at rest, sleeping on | A step of the pile after ten seconds, asleep, on four threads. | 0.000 |
| Pile of 4000 boxes at rest, sleeping off | A step of the same settled pile with sleeping off. | 1.55 |
| Pyramid of 60 rows (1830 boxes), 1 thread | A step of a pyramid of 1830 boxes that never sleep, on one thread. | 0.84 |
| Pyramid of 60 rows (1830 boxes), 4 threads | The same pyramid on four threads. | 0.61 |
| Pyramid of 40 rows, 2 sub-steps | A step of a pyramid of 820 boxes that never sleep, on four threads, with 2 sub-steps. | 0.27 |
| Pyramid of 40 rows, 4 sub-steps | The same pyramid with 4 sub-steps. | 0.36 |
| Pyramid of 40 rows, 8 sub-steps | The same pyramid with 8 sub-steps. | 0.54 |
| Chain of 300 links with a heavy ball | A step of 300 capsules joined by revolute joints that swing a ball twenty times denser. | 0.064 |
| 20 ragdolls tumbling down stairs | A step of 20 ragdolls 200 units tall that fall down ten stairs. | 0.10 |
| Fluid of 1800 particles, whole step | A step of a tank of fluid with six floating crates, the fluid included. | 0.57 |
| Fluid of 4000 particles, whole step | The same tank with 4000 particles. | 1.0 |
| Fluid of 8000 particles, whole step | The same tank with 8000 particles. | 1.45 |
| 10000 ray casts through a pile of 2000 boxes | 10000 rays of 600 units cast one by one down into a settled pile, in one run. | 7.1 |
| 10000 circle queries in a pile of 2000 boxes | 10000 circle queries of radius 40 in the same pile, in one run. | 14.9 |
| Read the transforms of 5000 bodies | One `readTransforms` call over 5000 bodies. | 0.026 |
| Read the transforms of 5000 bodies, interpolated | The same call with interpolation. | 0.043 |
| Pile of 2000 boxes stepped from Lua | A step of a pile of 2000 boxes called from Lua on one thread. | 0.89 |
| Pile of 2000 boxes stepped from Lua with callbacks | The same step with `onHit` and `onContactBegin` set in Lua. | 0.92 |

The benchmark also prints the time of the fluid pass alone next to each whole step. It prints the time of the debug drawing of a pile of 2000 boxes, and the number of events that reached Lua.

What the numbers say:

- On four threads, a large awake pile steps about a quarter faster, and so does a large pyramid. Four more threads add almost nothing, which is why worlds default to at most four. Small worlds, such as the chain and the ragdolls, stay on one thread, where they cost what they cost without threads.
- A pile at rest costs nothing while it sleeps, and as much as a falling pile while it is kept awake.
- Going from 2 to 4 sub-steps adds about a third to the step. Going from 4 to 8 adds about a half.
- Joints and ragdolls are cheap: a chain of 300 links and 20 ragdolls step in about a tenth of a millisecond.
- A fluid of thousands of particles costs about a millisecond. Its cost grows slower than its particles, because its passes run in parallel.
- A ray cast costs about 0.7 microseconds, and a circle query that returns its shapes about 1.5. The timings of one run of each are noisy.
- Reading 5000 transforms in bulk costs about 5 nanoseconds per body.
- Stepping from Lua costs what the step costs. The hit and contact callbacks of a pile of 2000 boxes add less than the noise of the run.

### A frame of thousands of bodies

The stress sample `samples/stress/many-bodies` shows what a whole frame of a physics game costs. It holds a bin of 8000 toys of four shapes, each one a Box2D body, with 28,500 contacts. A blast every three seconds keeps them awake. One sprite batch, filled by `world:readTransforms` with interpolation, draws them under the UI of the sample. These are its readouts after the pour, with the Release engine of each platform:

| Platform | Physics step | Average frame | Notes |
| --- | --- | --- | --- |
| macOS on an Apple M5 Pro, 5 performance and 10 efficiency cores | 2.68 ms on the job system | 8.32 ms, the 120 Hz of the display | The step waits for its workers, and the rest of the frame waits for the next drawable. |
| Chrome on the same Mac, WebGPU | 4.86 ms on the one thread of the web | 6.19 ms, 60 frames per second | The web build steps on one thread, about 1.8 times the time of the native step. |
| Android emulator on the same Mac, arm64 | 6.15 ms | 12.30 ms | The emulator runs arm code natively but draws without a window on a software GPU, so its frame rate says nothing of a device, and its step runs on the cores the emulator gives it. |

A sample of the main thread of the macOS run, taken with `sample` over eight seconds, splits the frame like this:

- The physics step takes 28 percent of the samples, mostly waiting for the tasks of Box2D on the workers.
- Waiting for the next drawable of the display takes 23 percent.
- The Lua update of the scene, with `readTransforms` and the batch, takes 5 percent.
- The renderer, the UI and the submission take about 2 percent together.

On the workers, the solver of contacts, the collision of polygons and the splitting of islands for sleeping take most of the time, all inside Box2D.

What follows from it:

- The step of Box2D is the whole cost of a physics game. The engine adds about a tenth of it around the step. In that time it reads thousands of transforms in one call, fills the batch that draws them and runs the Lua of the scene.
- The step scales over the job system up to about four threads. The web build has one thread and pays about 1.8 times the native step. So a web game keeps about half the awake bodies of its desktop version within the same frame.
- Letting the pile sleep is still the largest saving. A settled bin of 8000 toys steps in no time once it sleeps, and the blasts of the sample keep it awake on purpose.
- Emulators and simulators measure the CPU of the Mac, not the device. So take the budget of awake bodies of a phone or a TV from the readout of the sample on the device itself.

## From C++

The C++ API lives in `haylen/2d/physics/`, in the namespace `haylen::physics2d`.

- A `World` takes its `World::Settings` and, for more than one thread, the job system of the engine, as in `physics2d::World world({.threads = 4}, &engine.getJobs())`. A world of more than one thread without a job system throws `std::invalid_argument` with `A physics world with more than one thread needs a job system.`.
- `Body`, `Shape` and `Joint` are handles. A handle turns invalid when what it refers to goes, or when its world goes.
- `Rope`, `Ragdoll`, `Vehicle`, `TopDownVehicle`, `Mover`, `Grabber`, `ForceField`, `Fluid`, `Terrain`, `Explosion`, `Fracture`, `PathPredictor`, `Raycaster` and `RayBatch` are the helpers that the Lua module wraps, with the same options and errors.
- Code that acts on a world in every step adds a function with `World::addStepHook(World::StepPhase::Before, hook)`, which runs before Box2D advances, where forces apply. With `StepPhase::After`, it runs where the positions of the step are known. Remove it with `removeStepHook`. Vehicles, force fields and fluids work this way.
- `World::getInterpolatedTransform` blends one body, `World::computeStateHash` hashes a list of bodies and `World::getStats` returns the counts and times of the last step.

Code that moves through a world by queries, as the mover does, follows the rules of one-way platforms with three members:

- `World::kOneWayThreshold` is the least lean toward the open side of a platform at which a contact holds.
- `World::kOneWayDepth` is how deep in meters a body may be inside a platform and still land on it.
- `World::isDroppingThrough(body)` tells whether `Body::dropThrough` lets a body pass every platform.
