# Physics

Haylen simulates 2D rigid bodies with Box2D behind its own API, `haylen.physics2d` in Lua and `haylen::physics2d` in C++. A world holds bodies, the shapes that give them collision and the joints that connect them, and on top of them the engine builds ropes, bridges, ragdolls, vehicles, character movers, grabbers, force fields, one-way platforms, conveyors, explosions, destructible terrain, fractures and particle fluids. This guide explains how the simulation works and how to tune it: units and scale, stepping, smooth drawing, threads, sleeping, the solutions to the problems that physics games meet most often and what each part costs. The [`haylen.physics2d` reference](lua-api/physics2d.md) lists every function, option, default and error with an example of each.

## Worlds, bodies and shapes

A world is created with `physics2d.newWorld()` and moves only when the app steps it. Bodies are static, which never move, kinematic, which move only by the velocity the app gives them and push dynamic bodies without being pushed back, or dynamic, which respond to gravity, forces, contacts and joints. A body has no collision until shapes are added to it: boxes, circles, capsules, polygons, segments and chains, each with a material (density, friction, restitution and rolling resistance), a collision filter and switches for the events it raises. Joints connect two bodies of the same world as hinges, sliders, springs, ropes, welds, wheels, drags and motors.

Body, shape and joint handles keep their world alive, and so do the helpers built on it. Helpers that own bodies or act every step, which are vehicles, movers, grabbers, force fields, terrains and fluids, live while a Lua value refers to them and leave the world with what they added once the garbage collector takes them, so a scene keeps them in its table.

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

## Units and scale

Positions, sizes and velocities are in world units, the units of the camera and of sprites, with y pointing down, and angles are in radians. Box2D works in meters, kilograms and seconds, and each world converts between the two with its `pixelsPerMeter` scale, 64 by default. Every quantity the API takes or returns stays in world units and kilograms, so the same numbers give the same motion at every scale.

| Quantity | Unit |
| --- | --- |
| Position, size, distance | World units |
| Velocity, speed thresholds | World units per second |
| Angle, angular velocity | Radians, and radians per second |
| Density | Kilograms per square meter |
| Mass | Kilograms |
| Force, `maxMotorForce`, `breakForce` | Kilograms times units per second squared |
| Linear impulse | Kilograms times units per second |
| Rotational inertia | Kilograms times squared units |
| Torque, `maxMotorTorque`, `breakTorque` | Kilograms times squared units per second squared |
| Angular impulse | Kilograms times squared units per second |

So `body:applyForce(0, -body.mass * 980)` holds a body up against the default gravity at any scale, and `body:applyTorque(body.inertia * 3)` speeds its spin up by 3 radians per second every second.

The scale matters for the tuning of the solver. Box2D works best when moving bodies are between 0.1 and 10 meters across: its tolerances are fixed in meters, such as the 0.005 meters under which it tells two points apart, its speed limit is 400 meters per second, and its default thresholds for bounces and hits are 1 meter per second. A body much smaller than a tenth of a meter collides poorly, and a body of many meters moves like a slow giant. At the default 64 units per meter, moving bodies should be between about 6 and 640 units across, and a character of 48 to 128 units is ideal. An app whose sprites are much smaller or larger picks the scale that brings its characters near one or two meters, such as 32 units per meter for a game of 16-pixel tiles. The scale also sets how heavy a shape is, because density is per square meter, and the defaults of the speeds of the world, which follow the scale.

## Stepping

A world advances when `world:step(seconds)` runs, and the step belongs in the `fixedUpdate` callback of a scene, which the engine calls at the fixed rate of `fixedRate` in `app.json`, 60 times per second by default, with the same step length every time. A fixed step keeps the simulation stable and makes it repeat exactly, while the frame rate of the display varies. A frame runs zero, one or more fixed steps, so input read in `update` reaches the simulation through the state of the scene, the way the mover example of the reference keeps a jump for the next fixed step.

One step releases the data of the bodies destroyed since the last step, lets vehicles and force fields apply their forces, advances Box2D, breaks the joints loaded beyond their limits, records the transforms that interpolation needs, moves the fluids and then calls the event callbacks of the world. Vehicles, force fields and fluids act through the step hooks of the world, so they run in every step without a call of the app.

### Sub-steps

Box2D solves each step in several sub-steps, 4 by default, and finds contacts once per step. More sub-steps make stacks, ragdolls, chains and suspensions stiffer and settle them faster, at a cost that grows with them: the physics benchmark steps a pyramid of 820 boxes in 0.27 milliseconds with 2 sub-steps, 0.36 with 4 and 0.54 with 8. Box2D also caps the stiffness of contacts at an eighth of the sub-step rate and that of joints at a quarter of it, so at 60 steps per second with 4 sub-steps contacts reach at most 30 hertz and joints 60 hertz, and stiffer settings need more sub-steps. The property `world.subSteps` changes them while the world runs.

### Smooth drawing between fixed steps

A display that refreshes 120 or 144 times per second shows several frames per fixed step of 60, and a display at 50 frames skips steps, so bodies drawn where they are move in uneven jumps. A world created with `interpolate = true` records the transforms of the last two steps of every body that moved, and draws each body between them by the interpolation of the frame, which [`haylen.interpolation()`](lua-api/haylen.md#hayleninterpolation) returns. The method `body:renderTransform()` returns the blended `x`, `y` and `rotation` of one body, and `world:readTransforms(bodies, buffer, 1, true)` writes those of thousands of bodies into a float buffer in one call. Interpolated bodies show up to one fixed step behind the simulation, which nobody notices at 60 steps per second, and a body that a teleport placed shows where it is, so respawns never blend across the screen. Reading the interpolated transforms of 5000 bodies takes 0.043 milliseconds against 0.026 without, and recording them costs a little more in each step, so a world that draws only static or slow bodies leaves it off.

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

Box2D splits a step into tasks, such as finding contacts and solving the constraints of each color of the graph of contacts, and the engine runs those tasks on its job system through the private class `StepTasks`. Each task splits into at most one chunk per thread of the world, the workers of the task pool take chunks as they come, and the thread that steps runs every chunk no worker took yet, so a step never waits behind other work queued on the pool, such as a navigation mesh that builds in the background. A task of one chunk runs on the stepping thread alone.

A world steps on the number of threads of its `threads` option, which defaults to the workers of the job system and at most 4, because the solver gains little from more workers and slow cores hold it back. It uses those threads only while at least 1000 of its bodies are awake, because waking the workers costs more than they save in a smaller world, so a world of a few hundred bodies steps on the frame thread at no extra cost. The web build is single-threaded, so every world there steps on one thread and `world.threads` reads 1.

Threads never change the result. Box2D solves in the same order on any number of threads, so a world steps to the same positions and velocities bit for bit on one thread or on eight, and `world:stateHash(bodies)` returns the same number for both, which also checks that a replay matches its recording. The physics benchmark steps a pile of 4000 falling boxes in 2.12 milliseconds on one thread, 1.75 on two, 1.53 on four and 1.52 on eight.

Fluids run their passes with `JobSystem::parallelFor` and ray batches spread their rays the same way, whatever the threads of the world.

## Sleeping

Bodies that touch or are joined to each other form an island, and an island falls asleep once every body in it has stayed slower than its sleep threshold for half a second. Sleeping bodies cost nothing: the physics benchmark steps a settled pile of 4000 boxes in less than a thousandth of a millisecond while it sleeps, and in 1.6 milliseconds with sleeping off. A sleeping body wakes when an awake body touches it, when a force, an impulse or a velocity reaches it, when a joint of it changes, when the world gravity changes and when the app writes `body.awake = true` or calls `world:wakeAll()`.

The threshold is 0.05 meters per second by default, 3.2 units per second at the default scale, and `sleepThreshold` changes it per body. A body that should never sleep, such as a player character driven by forces, sets `sleepEnabled = false`, and `world.sleepEnabled = false` keeps every body awake, which turns sleeping off for a whole level that must keep moving. Force fields never wake a body when they push it, and fluid particles wake a body only when they change its speed by more than 0.05 meters per second, so crates that float in still water fall asleep. A change the solver does not see by itself, such as a rule of the app that makes resting bodies fall again, calls `world:wakeAll()`.

## Common problems and their solutions

Each problem below names the option or helper that solves it. The [test project](../samples/tests/README.md) shows each one in the test named at its end, most of them with the problem and the solution side by side.

### Fast bodies pass through walls

A body that moves farther in one step than the thickness of a wall can end the step on its far side, which is called tunneling. The world option `continuous`, on by default, sweeps every fast dynamic body against static shapes, so balls, characters and debris stop at thin walls, floors and terrain. The sweep skips dynamic and kinematic shapes, which a projectile reaches with `bullet = true`: a bullet sweeps against every body, so it stops at a thin moving door or a thin dynamic plank, at the cost of continuous collision for each bullet, which `world:stats().continuousMilliseconds` reports. Give `bullet` only to small fast bodies such as arrows and bullets. More sub-steps make contacts stiffer, so a fast heavy body that reaches a wall or a pile does not push into it, and a higher `fixedRate` in `app.json` shortens how far a body moves between two steps. The world option `maxSpeed` caps the speed of every body, 400 meters per second by default, so a body asked to go faster moves at the cap, and a lower cap keeps everything within what continuous collision catches well. The test project shows it in `PHY-019`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}, continuous = true, maxSpeed = 20000})
local arrow = world:createBody({x = 0, y = 0, vx = 6000, bullet = true})
arrow:addBox(24, 4)
```

### Bodies catch on the joints between tiles and boxes

A box that slides over a floor made of separate boxes or segments stops dead at the seams, or hops, because at a joint its contact can point sideways into the next box, even when the surfaces line up exactly. These are called ghost collisions. A chain of segments, made with `body:addChain`, knows the neighbors of each segment and smooths the contacts at its joints, so bodies slide along floors, walls and slopes without catching. An open chain collides along every segment between the points it lists, because the world extends its first and last segments with the points that smooth the contacts at its ends, and a loop closes an outline. The collision that `map:buildCollision` builds from an orthogonal Tiled map merges the solid shapes of touching tiles into chain loops around each solid region, solid from the outside, so a level of square and sloped tiles behaves like one smooth outline. Round shapes help too: a character of a capsule or a circle rides over a small step where a box catches. The test project shows it in `PHY-020`, and `TLD-013` digs the merged collision of a Tiled map.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local floor = world:createBody({type = 'static'})
-- One chain instead of one box per tile, listed from left to right so bodies stand on top of it.
floor:addChain({{0, 400}, {320, 400}, {448, 336}, {800, 336}}, false)
```

### One-way platforms and dropping through them

A platform that bodies and movers jump through from below and land on from above is a shape with `oneWay = {0, -1}`, the direction that its open side faces in the space of its body. A contact holds only while it leans at most 60 degrees from that direction, so bodies also pass the platform from its sides, and only when the body was outside the platform at the start of the step, or at most 0.02 meters inside it, so a jump that rises halfway into a platform falls back under it instead of popping up on top. A mover follows the same rules: it passes the platform from its closed side and stands on its open side. The methods `body:dropThrough(seconds)` and `mover:dropThrough(seconds)` let a body or a mover fall through the platform it stands on, for 0.2 seconds by default and then until it leaves the platform, so it lands on the next one below. Tiled objects and tile shapes whose `oneWay` property is `true` become such platforms. The test project shows it in `PHY-013` and `PHY-031`.

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

A dynamic body with friction makes a poor character. On a slope its friction must be high to keep it from sliding down while idle, and then it sticks to walls and drags on the ground, it leaves the ground at every crest and bounces down slopes, it needs a jump for every small step, and the solver pushes it around when crates press on it. The helper `physics2d.newMover` is a kinematic character instead: an upright capsule that moves by the displacement the app asks for and slides along what it hits. It stands still on slopes up to `maxSlope` and walks up them at its full speed, steps onto ledges up to `stepHeight`, stays on the ground over crests and down slopes within `snapDistance`, stops at walls and ceilings, jumps up through one-way platforms and lands on them, and pushes crates with at most `pushForce`. The app keeps the velocity: each fixed step adds gravity, clips the velocity with `mover:clip`, moves with `mover:move` and clips again, and `grounded`, `onWall`, `onCeiling` and `groundNormal` tell what the mover touched. The reference shows the whole loop. The test project shows it in `PHY-021` and `PHY-031`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local hero = physics2d.newMover(world, {x = 0, y = 0, radius = 14, height = 56, maxSlope = math.rad(45), stepHeight = 20, snapDistance = 10})
```

### Moving platforms that carry bodies

A platform that teleports with `body:setTransform` or its `x` and `y` shoves aside what it lands in and leaves the bodies on it behind, because a teleport has no velocity for friction to carry. A kinematic platform carries bodies when it moves by velocity, and `body:moveTo(x, y, rotation)` gives it the velocity that reaches the target in one fixed step, so a platform that follows a path, a tween or an animation calls it once per fixed step with the next position, before `world:step()`. Its velocity stays until something changes it, and a call with the place the platform already holds stops it. Movers ride the body they stand on by following the point under their feet, and `mover.groundVelocity` tells the speed of that point, which a jump from a moving platform adds to its own. The test project shows it in `PHY-022`.

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

### High-speed falls onto thin platforms

A body that falls a long way reaches speeds at which it crosses a thin platform within one step. Continuous collision, on by default, catches it on static platforms, and one-way platforms land fast bodies wherever they reach them. A moving platform is kinematic, which continuous collision skips, so bodies that fall fast onto moving platforms set `bullet = true`. The world option `maxSpeed` caps the speed of every fall, and a cap near the fastest speed the game needs keeps falls within reach of the sweeps. The test project shows it in `PHY-019`.

### Joints that stretch or explode

A chain of many bodies stretches under a heavy load, because the solver cannot hold every joint exactly within a step, and a heavy body hung from light ones stretches it most. More sub-steps make joints stiffer. Keeping the masses of joined bodies within about ten to one of each other keeps the solver accurate, so a heavy lamp hangs from a few strong links rather than many light ones. A rope made with `physics2d.newRope` adds a slack distance joint between its two ends unless `limitLength` is `false`, which holds a load a hundred times heavier than a segment at the length of the rope. Each joint holds itself together with a stiffness of `constraintHertz`, 60 by default, damped by `constraintDampingRatio`, 2 by default, and lower values make a joint soft on purpose, like rubber. A joint that should give way under a load breaks at `breakForce` or `breakTorque` instead of tearing the bodies apart, and `world.onJointBreak` reports it. Joints explode when they fight each other or the contacts, such as two joints that pin a body at different points or bodies held inside each other, so let joined bodies keep their own space, or turn `collideConnected` off, which is the default. The test project shows it in `PHY-023`, and `PHY-030` breaks joints above their limits.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({subSteps = 8})
local anvil = world:createBody({x = 0, y = 400})
anvil:addCircle(24, {density = 40})
local chain = physics2d.newRope(world, {from = {0, 0}, to = {0, 380}, segments = 20, pinStart = true, endBody = anvil, limitLength = true})
```

### Stacks that jitter and bodies that push apart too hard

Contacts are soft springs. Their stiffness is `contactHertz`, 30 by default, and their damping is `contactDampingRatio`, 10 by default, and together they decide how fast overlapping shapes separate, never faster than `contactPushSpeed`, 3 meters per second by default. Bodies spawned inside each other, or squeezed by a heavy load, fly apart when the push is strong: a lower `contactPushSpeed` separates them gently, and spawning bodies apart avoids it. A tall stack that sways or jitters settles with more sub-steps, which also allow a stiffer `contactHertz`, and sleeping stops a settled stack entirely. The test project shows it in `PHY-024` and `PHY-046`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({subSteps = 8, contactHertz = 40, contactPushSpeed = 120})
```

### Motion that stutters between the fixed step and the frame

Bodies that move in uneven jumps on a fast display, or that shake while the camera follows them, are drawn where the last fixed step left them. A world created with `interpolate = true` draws them between their last two steps, through `body:renderTransform()` or `world:readTransforms(bodies, buffer, 1, true)`, as [Smooth drawing between fixed steps](#smooth-drawing-between-fixed-steps) describes. A camera that follows a body follows its interpolated position too. The test project shows it in `PHY-025`.

### Small bounces and floods of hit events

A bouncy ball at rest keeps bouncing a little, and every tiny bounce of a pile calls `onHit`. Impacts slower than `restitutionThreshold`, 1 meter per second by default, never bounce, so raising it stops the small bounces and lowering it lets slow balls with full restitution bounce. Impacts slower than `hitThreshold`, 1 meter per second by default, call no `onHit`, so a game that plays impact sounds raises it to the speed of an audible impact. Shapes that should stay quiet, such as debris, particles and decorations, turn off `contactEvents`, `hitEvents` and `sensorEvents`, which also saves the cost of the events reaching Lua. A contact raises its events when either shape has the switch on, so the switch goes off on both sides of the contacts that should stay quiet. The test project shows it in `PHY-026`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({restitutionThreshold = 96, hitThreshold = 300})
local pebble = world:createBody({x = 0, y = 0})
pebble:addCircle(4, {contactEvents = false, hitEvents = false})
```

### Dragging bodies with a mouse or a finger

A mouse joint pulls one body toward the pointer with a force sized for that body, so a heavy chain or a ragdoll held by one hand barely moves while a pebble flies off. The helper `physics2d.newGrabber` takes the dynamic body nearest to the pointer within its `pickRadius`, holds it by the point it took and pulls with a soft spring whose force is `strength` times the weight of the held body and of everything joined to it, so one hand of a ragdoll drags the whole figure and a chain follows as well as a coin. A finger covers more than one point, so a pick radius of a fingertip finds thin and small bodies near the touch. The test project shows it in `PHY-027`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local hand = physics2d.newGrabber(world, {pickRadius = 24, strength = 30})
-- On a press: hand:grab(x, y), on a move: hand:moveTo(x, y), on a release: hand:release().
```

### Ragdolls that stay stuck

A limp ragdoll folds into the corners of stairs and around the edges of steps, and its limbs catch on geometry while the rest of the figure is dragged on. A ragdoll made with `physics2d.newRagdoll` resists bending with `stiffness`, which sizes a motor in every joint from the weight of the limb below it, so the same stiffness holds a small and a large figure alike at any scale. Its capsule shapes slide over edges where boxes catch, and the angle limits of real joints keep its poses natural. Figures that should collide with each other take a negative `group` each, since parts that share a negative group never collide, and a grabber drags a figure by one hand over a step instead of tearing that hand off. The test project shows it in `PHY-007` and `PHY-027`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local first = physics2d.newRagdoll(world, {x = 0, y = 0, stiffness = 0.3, group = -1})
local second = physics2d.newRagdoll(world, {x = 200, y = 0, stiffness = 0.3, group = -2})
```

### Vehicles that flip or stand on their rear wheels

A car built from a box and two motorized wheels flips easily: its center of mass sits in the middle of the box, high above the axles, and a motor with a large fixed torque spins the wheels up at once, which lifts the nose. The vehicle of `physics2d.newVehicle` puts the center of mass at the height of the axles, sizes the torque of its motors from its mass and the `acceleration` it should have, so throttle speeds it up evenly up to `topSpeed`, and couples the springs of its axles with `antiRoll`, which keeps the chassis level when it speeds up and brakes. Its wheels are created with `fastRotation`, since Box2D otherwise limits every body to 45 degrees of rotation per step, which caps the speed of small fast wheels. In the air the throttle turns the chassis with `airControl`, so a player lands on the wheels, and `brake` holds the wheels with the deceleration of `brakeAcceleration`. A car seen from above is `physics2d.newTopDownVehicle`, in a world without gravity: its tires grip sideways up to `grip`, so it drifts when a turn asks for more, the handbrake keeps only `handbrakeGrip` of the rear grip, and the steering lock shrinks with speed. The test project shows it in `PHY-008` and `PHY-028`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local buggy = physics2d.newVehicle(world, {x = 0, y = 200, drive = 'all', acceleration = 700, topSpeed = 1100, antiRoll = 0.6, airControl = 5})
local space = physics2d.newWorld({gravity = {0, 0}})
local racer = physics2d.newTopDownVehicle(space, {grip = 1600, handbrakeGrip = 0.2, drive = 'rear'})
```

### Balls that roll forever

Friction stops a body that slides, but a ball rolls without sliding, so on a flat floor it rolls on until something hits it. Rolling resistance slows round shapes that roll: a contact takes the larger `rollingResistance` of its two shapes, so grass that slows every ball sets it on the ground, and a heavy ball that rolls less sets it on the ball. A ball with a rolling resistance of 0.3 stops from 300 units per second within a few seconds. The test project shows it in `PHY-036`, whose cannon wheels roll with resistance, and `PHY-042` and `PHY-044` slow their balls with damping.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local lawn = world:createBody({type = 'static', x = 0, y = 300})
lawn:addBox(2000, 40, {rollingResistance = 0.2})
```

### Wind, magnets, planets and water

Applying a force to every body in an area from Lua costs a call per body per step. A force field of `physics2d.newForceField` does it in C++ before every step: a `'radial'` field pulls toward its center, or pushes away with a negative strength, for magnets, attractors and planets, a `'directional'` field pushes one way for wind and jets, a `'vortex'` field swirls, and a `'buoyancy'` field is water that floats each shape by the area it has under the surface, so bodies lighter than the water float with the right share under it and heavier ones sink. The strength is an acceleration, the same for every body, unless `acceleration` is `false`, which pushes light bodies further, and fields fade with `'linear'` or `'inverseSquare'` falloffs, the second like gravity. Drags slow bodies toward a `flow`, for currents and air resistance. A field never wakes the bodies it pushes, so floating bodies fall asleep. The test project shows it in `PHY-048`, `PHY-049`, `PHY-050` and `PHY-051`.

### Aiming throws

A trajectory drawn with the formula of a parabola misses where the body really lands, because the solver integrates gravity, damping and the speed limit in sub-steps. The method `world:predictPath(x, y, vx, vy, options)` follows the same integration as the solver, with the gravity scale and the damping of the thrown body, and sweeps a circle of its radius through the world, so it returns the points the throw passes and the shape it would hit, which draws the aim of a slingshot that matches the real throw. The test project shows it in `PHY-035`.

### Picking thin or small bodies

A tap or a click finds a body only when the point lies inside one of its shapes, which misses a thin rope or a small coin under a finger. The filter of `world:pick(camera, x, y, filter)` takes a `radius` in design coordinates, so the pick finds the shapes within that distance of the point too, nearest first, with the shapes that contain the point before the others. A grabber takes the same kind of radius in world units, as `pickRadius`. The test project shows it in `PHY-027` and every test that drags bodies.

### Liquids

A liquid made of circle bodies costs a body and its contacts per drop, and its drops never hold together. A fluid of `physics2d.newFluid` keeps its particles outside Box2D, in flat arrays that a spatial hash sorts every step, so pressure holds them together and spreads them apart like a liquid, and spreads the passes over the job system. The particles collide with the shapes of the world and push the dynamic bodies they hit, so crates float or sink by their density, and `fluid:draw` draws them as metaballs straight from the arrays. A fluid costs less than the step of a few thousand boxes: a tank of 1800 particles with six crates steps in 0.24 milliseconds, 4000 particles in 0.37 and 8000 in 0.49, and `fluid.stepMilliseconds` tells what a fluid takes in a scene. The test project shows it in `PHY-012` and `PHY-059`.

## Performance

The cost of a world grows with what is awake and touching:

- The step costs in proportion to the awake bodies and their contacts and joints, and sleeping bodies cost nothing, so letting bodies sleep is the largest saving there is.
- Sub-steps multiply the cost of solving, while contacts are found once per step.
- Threads shorten the step of a world of more than 1000 awake bodies, up to about four threads.
- Bullets cost a sweep against every nearby body each step, and continuous collision against static shapes costs little.
- Chains are cheaper than boxes for terrain, because a chain segment has no area and its neighbors smooth its contacts, and one merged outline replaces many tiles.
- Queries cost in proportion to the shapes they find, and every ray cast costs a walk through the tree of shapes. Thousands of rays per frame go through `world:raycastBatch`, which spreads them over the job system.
- Events cost a Lua call each, so shapes that raise events nobody reads turn their switches off.
- Reading positions one property at a time costs a call across the Lua boundary per value, and `world:readTransforms` reads thousands of bodies in one call.
- Fluids cost in proportion to their particles and the neighbors of each particle.

### Debug and Release builds

The desktop player that `python3 haylen.py run` builds is a Debug build by default, without optimizations, where physics runs several times slower than in the Release engine that apps built for platforms link: a pile of 4000 falling boxes takes about 27 milliseconds a step on one thread in a Debug build and 2.1 in Release, and a fluid of 1800 particles 0.86 and 0.24. Measure physics with `python3 haylen.py bench --suite physics`, or run an app in the Release player with `python3 haylen.py run <app> --config Release`, as the [build guide](build.md#bench) describes.

### The physics benchmark

The command `python3 haylen.py bench --suite physics` builds `haylen-physics-benchmark` in Release and runs it on the headless host. It prints the workers of the job system and, for each scene, the average time of one step or one operation in milliseconds. These numbers come from an Apple silicon Mac with a Release build, and absolute values vary from machine to machine and from run to run.

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
| Fluid of 1800 particles, whole step | A step of a tank of fluid with six floating crates, the fluid included. | 0.24 |
| Fluid of 4000 particles, whole step | The same tank with 4000 particles. | 0.37 |
| Fluid of 8000 particles, whole step | The same tank with 8000 particles. | 0.49 |
| 10000 ray casts through a pile of 2000 boxes | 10000 rays of 600 units cast one by one down into a settled pile, in one run. | 7.1 |
| 10000 circle queries in a pile of 2000 boxes | 10000 circle queries of radius 40 in the same pile, in one run. | 14.9 |
| Read the transforms of 5000 bodies | One `readTransforms` call over 5000 bodies. | 0.026 |
| Read the transforms of 5000 bodies, interpolated | The same call with interpolation. | 0.043 |
| Pile of 2000 boxes stepped from Lua | A step of a pile of 2000 boxes called from Lua on one thread. | 0.89 |
| Pile of 2000 boxes stepped from Lua with callbacks | The same step with `onHit` and `onContactBegin` set in Lua. | 0.92 |

The benchmark also prints the time of the fluid pass alone next to each whole step, the debug drawing of a pile of 2000 boxes and the number of events that reached Lua.

The numbers say this:

- Threads shorten the step of a large awake pile by about a quarter on four threads and of a large pyramid by about a quarter too, and four more threads add almost nothing, which is why worlds default to at most four. Small worlds such as the chain and the ragdolls stay on one thread, where they cost what they cost without threads.
- A pile at rest costs nothing while it sleeps and as much as a falling pile while it is kept awake.
- Going from 2 to 4 sub-steps adds about a third to the step, and going from 4 to 8 about a half.
- Joints and ragdolls are cheap: a chain of 300 links and 20 ragdolls step in about a tenth of a millisecond.
- A fluid of thousands of particles costs a fraction of a millisecond, and its cost grows slower than its particles because its passes run in parallel.
- A ray cast costs about 0.7 microseconds and a circle query that returns its shapes about 1.5, and the timings of one run of each are noisy.
- Reading 5000 transforms in bulk costs about 5 nanoseconds per body.
- Stepping from Lua costs what the step costs, and the hit and contact callbacks of a pile of 2000 boxes add less than the noise of the run.

## From C++

The C++ API lives in `haylen/2d/physics/` in the `haylen::physics2d` namespace. A `World` takes its `World::Settings` and, for more than one thread, the job system of the engine, as in `physics2d::World world({.threads = 4}, &engine.getJobs())`, and a world of more than one thread without a job system throws `std::invalid_argument` with `A physics world with more than one thread needs a job system.`. `Body`, `Shape` and `Joint` are handles that turn invalid when what they refer to or their world goes. `Rope`, `Ragdoll`, `Vehicle`, `TopDownVehicle`, `Mover`, `Grabber`, `ForceField`, `Fluid`, `Terrain`, `Explosion`, `Fracture`, `PathPredictor`, `Raycaster` and `RayBatch` are the helpers the Lua module wraps, with the same options and errors. Code that acts on a world every step adds a function with `World::addStepHook(World::StepPhase::Before, hook)`, which runs before Box2D advances, where forces apply, or with `StepPhase::After`, where the positions of the step are known, and removes it with `removeStepHook`, which is how vehicles, force fields and fluids work. `World::getInterpolatedTransform` blends one body, `World::computeStateHash` hashes a list of bodies and `World::getStats` returns the counts and times of the last step. Code that moves through a world by queries, as the mover does, follows the rules of one-way platforms with `World::kOneWayThreshold`, the least lean toward the open side of a platform at which a contact holds, `World::kOneWayDepth`, how deep in meters a body may be inside a platform and still land on it, and `World::isDroppingThrough(body)`, which tells whether `Body::dropThrough` lets a body pass every platform.
