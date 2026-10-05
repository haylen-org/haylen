# haylen.physics2d

Rigid body physics built on Box2D. Use it for anything that must collide, bounce, stack, hang from joints or be found with ray casts, shape casts and area queries, from platformer characters to falling crates and trigger zones. On top of bodies and joints it builds ropes, bridges, ragdolls, vehicles seen from the side and from above, character movers that climb slopes and steps, grabbers that drag bodies with a mouse or a finger, force fields, one-way platforms and conveyors, explosions, destructible terrain, fractures and particle fluids. Worlds step on several threads of the job system, draw their bodies smoothly between fixed steps and predict the flight of thrown bodies. The [physics guide](../physics.md) explains units, stepping, threads, sleeping and tuning in depth, with the solutions to common problems.

```lua
local physics2d = require('haylen.physics2d')
```

## Units and coordinates

Positions, sizes, distances and velocities are in world units, the same units the camera and sprites use, with y pointing down. Angles are in radians and angular velocities in radians per second. The world converts world units to meters for Box2D with its `pixelsPerMeter` setting, so a box 64 units wide is one meter wide in a world with the default scale.

Mass comes from the density of the shapes, in kilograms per square meter, and their area in square meters, so `body.mass` is in kilograms. Every other quantity uses world units and kilograms, in and out: forces and impulses scale with world units, and torques, rotational inertias and angular impulses with squared world units.

| Quantity | Unit | Example |
| --- | --- | --- |
| Force | Kilograms times units per second squared | The call `body:applyForce(0, -body.mass * 600)` accelerates the body upward at 600 units per second squared. |
| Linear impulse | Kilograms times units per second | The call `body:applyImpulse(body.mass * 100, 0)` changes the horizontal velocity by 100 units per second. |
| Rotational inertia | Kilograms times squared units | The property `body.inertia` of a disc of radius `r` is `body.mass * r * r / 2`. |
| Torque | Kilograms times squared units per second squared | The call `body:applyTorque(body.inertia * 3)` speeds the spin up by 3 radians per second every second. |
| Angular impulse | Kilograms times squared units per second | The call `body:applyAngularImpulse(body.inertia * 2)` changes the spin by 2 radians per second at once. |
| `maxMotorForce`, `breakForce`, `constraintForce` | Same as a force | Limits and reports of joints, such as a weld that breaks above five times the weight of its shelf with `breakForce = shelf.mass * 980 * 5`. |
| `maxMotorTorque`, `breakTorque`, `constraintTorque` | Same as a torque | Limits and reports of revolute, wheel and motor joints. |
| `maxSpeed`, `contactPushSpeed`, `restitutionThreshold`, `hitThreshold`, `sleepThreshold` | Units per second | Speeds of the world and of its bodies. |

So the same numbers give the same motion at every `pixelsPerMeter`, which only decides how large a meter is for the tuning of the solver, for the mass of a shape and for the default speeds and tolerances. The [physics guide](../physics.md#units-and-scale) explains how to choose it.

## Stepping the world

A world only moves when `world:step()` runs. Step it from the `fixedUpdate` callback of a scene, which the engine calls at the fixed rate set by `fixedRate` in `app.json` (60 times per second by default) with the length of one fixed step. One step does this, in order:

1. It releases the `data` of the bodies destroyed since the last step, as [Body properties](#body-properties) describes.
2. Vehicles and force fields apply their forces.
3. Box2D advances the bodies in the sub-steps of the world, on its threads.
4. Joints loaded beyond their break limits break.
5. A world that interpolates records where its bodies moved.
6. Fluids move their particles and push the bodies they hit.
7. The event callbacks of the world run for the contacts, hits, sensor overlaps and broken joints of the step.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local ground = world:createBody({type = 'static', x = 0, y = 200})
ground:addBox(800, 40)
local crate = world:createBody({x = 0, y = -200})
crate:addBox(48, 48, {density = 2, friction = 0.4})

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        graphics2d.beginWorld(self.camera)
        graphics2d.drawRect({crate.x - 24, crate.y - 24, 48, 48}, '#FFC08040')
        world:debugDraw({layer = 10})
    end,
})
```

## Threads

A world of more than one thread runs the tasks of each Box2D step on the job system of the engine, split into at most one chunk per thread, and the thread that steps runs the chunks no worker took yet, so a step never waits behind other work of the pool. It uses its threads only while at least 1000 of its bodies are awake, because below that waking the workers costs more than they save, and a smaller world steps on the calling thread alone. On the web every world steps on one thread, and `world.threads` reads 1. A world uses at most 64 threads, and the default stays at 4 or fewer, because the solver gains little from more workers. The number of threads never changes the result: a world steps to the same positions bit for bit on any number of threads, which `world:stateHash()` checks. Fluids and ray batches spread their work over the job system as well, whatever the threads of the world.

## Functions

### physics2d.newWorld(options)

Creates an empty world and returns it. The options table is optional, and unknown keys raise `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `gravity` | Vec2 or `{x, y}` | `{0, 980}` | Gravity in world units per second squared. |
| `pixelsPerMeter` | number | `64` | World units in one Box2D meter. It must be positive. |
| `subSteps` | integer | `4` | Solver sub-steps per step. More sub-steps make stacks and joints stiffer at a higher cost. It must be at least 1. |
| `threads` | integer | The workers of the job system, at most `4` | Threads that step the world, as [Threads](#threads) describes. It must be at least 1. |
| `continuous` | boolean | `true` | Sweeps fast dynamic bodies against static shapes, so they never pass through thin walls and floors. The body option `bullet` extends the sweep to dynamic and kinematic shapes. |
| `sleepEnabled` | boolean | `true` | Lets bodies that come to rest fall asleep, which costs nothing until something wakes them. |
| `interpolate` | boolean | `false` | Records the transforms of the last two steps, so `body:renderTransform()` and `world:readTransforms()` draw bodies smoothly between fixed steps. |
| `contactHertz` | number | `30` | Stiffness of contacts in cycles per second. Higher values push overlapping shapes apart faster and may jitter. |
| `contactDampingRatio` | number | `10` | Damping of the push that separates overlapping shapes. Lower values separate them faster and harder. |
| `contactPushSpeed` | number | `3 * pixelsPerMeter` | Fastest speed in world units per second at which contacts push overlapping shapes apart, 192 at the default scale. |
| `maxSpeed` | number | `400 * pixelsPerMeter` | Speed limit of every body in world units per second, 25600 at the default scale. |
| `restitutionThreshold` | number | `pixelsPerMeter` | Impacts slower than this speed in world units per second do not bounce, whatever the restitution. |
| `hitThreshold` | number | `pixelsPerMeter` | Impacts slower than this speed in world units per second call no `onHit`. |

The scale, the threads and `interpolate` stay as the world was created, and every other option is also a [world property](#world-properties) that changes while the world runs. Speeds left out keep the values Box2D is tuned for in meters: a speed limit of 400 meters per second, overlaps pushed apart at up to 3 meters per second, and bounces and hit events from 1 meter per second.

A scale that is not positive, fewer than one sub-step or fewer than one thread raise `A physics world needs positive pixels per meter, at least one sub-step and at least one thread.`. Box2D holds a limited number of worlds at once, 128 by default, and a world beyond them raises `Too many physics worlds exist at once to create another one.`. A world goes once no Lua value refers to it, to one of its handles or to one of the helpers built on it.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 1200}, pixelsPerMeter = 32, subSteps = 8})
local space = physics2d.newWorld({gravity = {0, 0}, threads = 1, sleepEnabled = false})
local arena = physics2d.newWorld({interpolate = true, maxSpeed = 6000, hitThreshold = 200})
print(world.subSteps, space.threads, arena.interpolate)
```

### physics2d.newRayBatch(count)

Creates a [`RayBatch`](#raybatch) of `count` rays, 0 by default, each of zero length until `batch:setRay` sets it.

### physics2d.drawRay(x1, y1, x2, y2, hit, order)

Draws one ray on the current canvas right away, up to the hit when `hit` is a hit table of any ray cast, of this module or of [`haylen.math`](math.md#ray-casts), [`haylen.spatial2d`](spatial2d.md) and [`haylen.tiled`](tiled.md), with a dot on the hit and a short line along its normal. A `nil` or `false` hit draws the whole ray. The optional `order` table takes the `layer`, `depth` and `blend` fields described in [`haylen.graphics2d`](graphics2d.md).

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local shield = {center = {300, 0}, radius = 40}

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        local hit = m.raycastCircle({0, 0}, {600, 0}, shield)
        physics2d.drawRay(0, 0, 600, 0, hit, {layer = 100})
    end,
})
```

The functions that build ropes, bridges, ragdolls, vehicles, movers, grabbers, force fields, explosions, terrain, fractures and fluids are described in their own sections below. A vehicle, top-down vehicle, mover, grabber, force field, terrain or fluid owns what it adds to the world and lives while a Lua value refers to it, so the garbage collector removes it with its bodies once none does. Keep it in a variable or in the table of its scene. Each one keeps its world alive.

## World

The function `physics2d.newWorld()` returns a `haylen.PhysicsWorld` userdata. Bodies, shapes, joints and the helpers built on the world keep it alive, so a world lives as long as any of them.

### world:createBody(options)

Creates a body and returns it. A body has no collision until shapes are added to it. The options table is optional, and unknown keys raise `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `type` | string | `'dynamic'` | Body type, one of `'static'`, `'kinematic'` or `'dynamic'`. |
| `x`, `y` | number | `0` | Position of the body origin. |
| `rotation` | number | `0` | Angle in radians. |
| `vx`, `vy` | number | `0` | Initial velocity in units per second. |
| `angularVelocity` | number | `0` | Initial spin in radians per second. |
| `linearDamping` | number | `0` | Slows the linear velocity over time. |
| `angularDamping` | number | `0` | Slows the spin over time. |
| `gravityScale` | number | `1` | Multiplies the world gravity for this body. |
| `fixedRotation` | boolean | `false` | Keeps the body from rotating. |
| `bullet` | boolean | `false` | Sweeps the body against dynamic and kinematic bodies too, not only against static ones, so a fast projectile never passes a thin moving body. |
| `fastRotation` | boolean | `false` | Lifts the limit of 45 degrees of rotation per step, so wheels and rotors spin as fast as they are driven. Only creation sets it. |
| `sleepEnabled` | boolean | `true` | Lets the body fall asleep when it comes to rest. |
| `sleepThreshold` | number | `0.05 * pixelsPerMeter` | Speed in units per second below which the body may fall asleep, 3.2 at the default scale. |

A damping that is negative or not finite raises `A physics body needs a finite damping of zero or more.`, and so does assigning one to `body.linearDamping` or `body.angularDamping`. A sleep threshold that is negative or not finite raises `A physics speed needs to be finite and zero or more.`.

Static bodies never move and have no mass. Kinematic bodies move only by their velocity and push dynamic bodies without being pushed back. Dynamic bodies respond to gravity, forces and collisions.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local floor = world:createBody({type = 'static', x = 0, y = 300})
local platform = world:createBody({type = 'kinematic', x = 0, y = 100, vx = 60})
local arrow = world:createBody({x = -300, y = 0, vx = 900, rotation = 0.1, bullet = true, gravityScale = 0.5})
local player = world:createBody({x = 0, y = 0, fixedRotation = true, linearDamping = 0.2, sleepEnabled = false})
local wheel = world:createBody({x = 200, y = 0, fastRotation = true, sleepThreshold = 1})
```

### world:createJoint(type, a, b, options)

Connects bodies `a` and `b` of this world with a joint and returns it. Anchors and targets are world positions. The options table is optional, and unknown keys raise `Unknown option "<key>".`. An unknown type raises a bad argument error with `unknown value '<type>'`, and bodies that are destroyed or belong to another world raise `A joint needs two bodies of this world.`.

| Type | Behavior |
| --- | --- |
| `'distance'` | Keeps the anchor `ax, ay` on `a` and the anchor `bx, by` on `b` apart, at their current distance unless `length` is given. It can be springy, limited between a shortest and a longest length, and driven by a motor that changes its length. A spring of 0 hertz with a limit makes a rope that goes slack below its longest length. |
| `'revolute'` | Pins both bodies at `ax, ay` and lets them rotate around it, like a hinge or a wheel axle. Its angle is 0 in the pose the bodies have when the joint is created, and its spring pulls toward `targetAngle`. |
| `'prismatic'` | Lets `b` slide relative to `a` along the axis through `ax, ay`, like a piston or an elevator, and keeps the angle between them that they have when the joint is created. Its spring pulls toward `targetTranslation`. |
| `'weld'` | Glues both bodies together at `ax, ay`, rigidly or on a spring. |
| `'wheel'` | Lets `b` slide along the axis through `ax, ay`, usually on a spring, and rotate freely, like a car suspension. |
| `'mouse'` | Pulls `b` on a soft spring toward a target that starts at `bx, by`. Body `a` is usually a static body and is not moved. |
| `'motor'` | Drives `b` toward an offset and an angle from `a`, which start at those the bodies have when the joint is created. |
| `'filter'` | Only keeps `a` and `b` from colliding with each other, without holding them together. |

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `ax`, `ay` | number | `0` | Anchor on `a`. Revolute, prismatic, weld and wheel joints use it as the shared anchor. |
| `bx`, `by` | number | `0` | Anchor on `b` for distance joints and the first target of mouse joints. |
| `collideConnected` | boolean | `false` | Lets the two bodies keep colliding with each other. |
| `enableLimit` | boolean | `false` | Enables the `lower` and `upper` limits of revolute, prismatic, wheel and distance joints. |
| `lower`, `upper` | number | `0` | Limits in radians for revolute joints, measured from the pose at creation, translations in world units for prismatic and wheel joints and the shortest and longest length for distance joints. |
| `enableMotor` | boolean | `false` | Enables the motor of revolute, prismatic, wheel and distance joints. |
| `motorSpeed` | number | `0` | Motor speed in radians per second for revolute and wheel joints, or in world units per second for prismatic and distance joints. |
| `maxMotorForce` | number | `0` | Maximum motor force of prismatic and distance joints, maximum force of motor joints and maximum pull of mouse joints, as a force in world units. |
| `maxMotorTorque` | number | `0` | Maximum motor torque of revolute and wheel joints and maximum torque of motor joints, as a torque in world units. |
| `enableSpring` | boolean | `false` | Makes distance, revolute, prismatic and wheel joints springy. |
| `hertz` | number | `0` | Spring stiffness in cycles per second. Weld joints use it for both the linear and the angular spring, and 0 keeps them rigid. |
| `dampingRatio` | number | `0` | Spring damping, where 1 stops the oscillation without overshoot. |
| `targetAngle` | number | `0` | Angle in radians that the spring of a revolute joint pulls toward. |
| `targetTranslation` | number | `0` | Translation in world units that the spring of a prismatic joint pulls toward. |
| `axisX`, `axisY` | number | `1`, `0` | Slide axis of prismatic and wheel joints. |
| `length` | number | `0` | Rest length of a distance joint. Zero keeps the distance the anchors have when the joint is created. |
| `breakForce` | number | `nil` | Force beyond which a step breaks the joint, described in [Breaking joints](#breaking-joints). Without it the joint never breaks from a force. |
| `breakTorque` | number | `nil` | Torque beyond which a step breaks the joint. Without it the joint never breaks from a torque. |

Mouse joints use 4 hertz, a damping ratio of 1 and a pull that accelerates `b` at up to `1000 * pixelsPerMeter` units per second squared when `hertz`, `dampingRatio` or `maxMotorForce` are left at 0. Filter joints ignore every option. Every setting also changes while the joint runs, as [Joint properties](#joint-properties) describes.

Box2D checks limits even while `enableLimit` is `false`, so the joint is not created when they are out of its range:

- A distance joint shorter than 0.005 meters, which is 0.32 world units at the default 64 pixels per meter, raises `A distance joint needs a length of at least 0.005 meters.`. This happens with the default options, whose anchors are both at `0, 0`, so give a distance joint its anchors or a `length`.
- Revolute limits must lie within 0.99 pi radians of zero with `lower` not above `upper`, or the call raises `A revolute joint needs a lower limit that is not above the upper one, both within 0.99 pi radians of zero.`.
- Prismatic, wheel and distance joints with `lower` above `upper` raise `A prismatic, wheel or distance joint needs a lower limit that is not above the upper one.`.
- A negative `breakForce` or `breakTorque` raises `A joint needs a break force and a break torque of zero or more.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ceiling = world:createBody({type = 'static', x = 0, y = -200})
ceiling:addBox(400, 20)

local lamp = world:createBody({x = 0, y = -80})
lamp:addCircle(16)
local cord = world:createJoint('distance', ceiling, lamp, {ax = 0, ay = -200, bx = 0, by = -80, enableSpring = true, hertz = 2, dampingRatio = 0.3})

local bucket = world:createBody({x = -100, y = -60})
bucket:addBox(30, 30)
local winch = world:createJoint('distance', ceiling, bucket, {ax = -100, ay = -200, bx = -100, by = -60, enableSpring = true, enableLimit = true, lower = 20, upper = 300, enableMotor = true, motorSpeed = -40, maxMotorForce = 5000})

local door = world:createBody({x = 100, y = -150})
door:addBox(10, 80)
local hinge = world:createJoint('revolute', ceiling, door, {ax = 100, ay = -190, enableLimit = true, lower = -1.2, upper = 1.2, enableSpring = true, hertz = 1, targetAngle = 0.5})

local lift = world:createBody({x = -150, y = -100})
lift:addBox(80, 10)
local piston = world:createJoint('prismatic', ceiling, lift, {ax = -150, ay = -100, axisX = 0, axisY = 1, enableLimit = true, lower = -50, upper = 150, enableMotor = true, motorSpeed = 40, maxMotorForce = 500})

local chassis = world:createBody({x = 0, y = 100})
chassis:addBox(120, 30)
local wheel = world:createBody({x = 40, y = 125})
wheel:addCircle(18)
local suspension = world:createJoint('wheel', chassis, wheel, {ax = 40, ay = 125, axisX = 0, axisY = 1, enableSpring = true, hertz = 4, dampingRatio = 0.7, enableMotor = true, motorSpeed = 10, maxMotorTorque = 4000})

local handle = world:createBody({x = 0, y = 60})
handle:addBox(20, 20)
local glue = world:createJoint('weld', chassis, handle, {ax = 0, ay = 80, breakForce = handle.mass * 980 * 20})

local ghost = world:createBody({x = 0, y = 0})
ghost:addBox(20, 20)
world:createJoint('filter', ceiling, ghost)
```

### world:step(deltaSeconds)

Advances the simulation by `deltaSeconds`, as [Stepping the world](#stepping-the-world) describes, and then calls the event callbacks for the contacts, hits, sensor overlaps and broken joints of this step. An error raised by a callback propagates out of `world:step()`.

```lua
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

### world:wakeAll()

Wakes every sleeping body of the world, for changes the solver does not notice by itself, such as a rule of the app that makes resting bodies move again. Writing `world.gravity` and turning `world.sleepEnabled` off wake every body without it.

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()

scene.push({
    update = function(self, dt)
        if input.keyPressed('r') then
            world:wakeAll()
        end
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

### world:readTransforms(bodies, buffer, first, interpolated)

Copies `x`, `y` and `rotation` of every body in the list `bodies` into a float buffer of [`haylen.collections`](collections.md#float-buffers), three values for each body in order, from the position `first`, which defaults to 1. Together with `SpriteBatch:writeFields` of [`haylen.graphics2d`](graphics2d.md) it keeps thousands of sprites on their bodies with two calls a frame instead of one property read for each body. With `interpolated` set to `true`, a world created with `interpolate` gives each body between its transforms of the last two steps, blended by [`haylen.interpolation()`](haylen.md#hayleninterpolation), like [`body:renderTransform()`](#bodyrendertransform).

Every body must be a live body of the world, and the buffer must hold three values for each one from `first`, or the call raises `Body transforms need live bodies of this world.` or `Body transforms take three floats for each body.`. A `first` outside the buffer raises a bad argument error with `the first position is outside the buffer`, and `interpolated` in a world without interpolation raises `Only a world created with interpolation blends the transforms of its bodies.`.

```lua
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld({interpolate = true})
local crates, count = {}, 500
for index = 1, count do
    local crate = world:createBody({x = math.random(100, 1800), y = math.random(-2000, 0)})
    crate:addBox(16, 16)
    crates[index] = crate
end
local ground = world:createBody({type = 'static', x = 960, y = 1060})
ground:addBox(1920, 40)

local transforms = collections.newFloatBuffer(count * 3)
local fields = {'x', 'y', 'rotation'}
local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:resize(count, {width = 16, height = 16, color = '#FFB07040'})

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    update = function(self, dt)
        world:readTransforms(crates, transforms, 1, true)
        batch:writeFields(transforms, fields)
    end,
    render = function(self)
        graphics2d.beginScreen()
        batch:draw()
    end,
})
```

### world:writeTransforms(bodies, buffer, first)

Moves every body in the list `bodies` to the `x`, `y` and `rotation` the buffer holds for it, three values for each body from the position `first`, like setting its transform one body at a time. It suits kinematic bodies that follow positions an app computes in bulk, and it raises the errors of `world:readTransforms()`.

```lua
local collections = require('haylen.collections')
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local platforms = {world:createBody({type = 'kinematic'}), world:createBody({type = 'kinematic'})}
local transforms = collections.newFloatBuffer(6)
transforms:set(1, {200, 400, 0, 600, 400, 0.2})
world:writeTransforms(platforms, transforms)
print(platforms[2].position.x) -- 600.0
```

### world:stats()

Returns a table with the counts of the world and the times in milliseconds that the parts of its last step took, which a debug panel shows while tuning a scene.

| Field | Meaning |
| --- | --- |
| `bodies` | Bodies in the world. |
| `awakeBodies` | Bodies that are awake. |
| `shapes` | Shapes of every body. |
| `contacts` | Contacts Box2D tracks between shapes whose bounds overlap, touching or not. |
| `joints` | Joints in the world. |
| `islands` | Islands, the groups of bodies joined by contacts and joints that sleep and wake together. |
| `stepMilliseconds` | Time of the whole Box2D step. |
| `collideMilliseconds` | Time spent finding and updating contacts. |
| `solveMilliseconds` | Time spent solving contacts and joints and moving the bodies. |
| `continuousMilliseconds` | Part of the solve spent sweeping bullets. |
| `sleepMilliseconds` | Part of the solve spent putting islands to sleep. |
| `hookMilliseconds` | Time the vehicles, force fields and fluids of the world took in the step, outside the Box2D step. |

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        local stats = world:stats()
        graphics2d.beginScreen()
        graphics2d.drawText(nil, string.format('Bodies %d, awake %d, step %.2f ms', stats.bodies, stats.awakeBodies, stats.stepMilliseconds), 40, 40, {size = 28})
    end,
})
```

### world:stateHash(bodies)

Returns an integer hash of the positions, rotations and velocities of the bodies in the list `bodies`, bit for bit, so two runs that should match, such as a replay and its recording or the same world on another number of threads, compare one number. Every body must be a live body of the world, or the call raises `A state hash needs live bodies of this world.`.

```lua
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local ball = world:createBody({x = 0, y = 0})
ball:addCircle(10)
local steps = 0

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
        steps = steps + 1
        if steps == 60 then
            print('state after one second', world:stateHash({ball}))
        end
    end,
})
```

### world:predictPath(x, y, vx, vy, options)

Predicts the flight of a body thrown from `x, y` with the velocity `vx, vy` and returns the list of points it passes, as `Vec2` values, and the hit of the first shape it would reach, or `nil`. The path follows the solver of the world: it advances in the sub-steps of the world with its gravity and its speed limit and with the gravity scale and the linear damping of the options, so the aim of a slingshot matches the real throw of a body with the same settings. It keeps one point per step, starting at the launch position, and sweeps a circle of `radius` between them, or a ray when the radius is 0. The path ends at the hit, where the last point is the center of the circle as it touches, or the hit point of the ray. The hit is a table like those of [`world:raycast()`](#worldraycastx1-y1-x2-y2-filter). The options table is optional, and unknown keys raise `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `steps` | integer | `120` | Steps to predict, two seconds at the default fixed rate. |
| `step` | number | `haylen.fixedStep()` | Length of one step in seconds. |
| `radius` | number | `0` | Radius of the swept circle, usually the radius of the thrown body. |
| `gravityScale` | number | `1` | Gravity scale of the thrown body. |
| `linearDamping` | number | `0` | Linear damping of the thrown body. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | The shapes the path hits, as in a [ray cast filter](#ray-cast-filters). |

A step that is not positive, fewer than one step, or a negative radius or damping raise `A path prediction needs a positive step, at least one step, and a radius and damping of zero or more.`. A swept circle that starts touching a shape hits it at once, so leave the thrown body out of the mask, as the example does, or predict before the body exists.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local kGround, kStones = 1, 2
local world = physics2d.newWorld()
local ground = world:createBody({type = 'static', x = 0, y = 300})
ground:addBox(2000, 40, {category = kGround})
local camera = graphics2d.newCamera()
local launch = {x = -400, y = 200}

scene.push({
    enter = function(self)
        self.vx, self.vy = 0, 0
    end,
    update = function(self, dt)
        local x, y = camera:screenToWorld(input.mousePosition())
        self.vx, self.vy = (launch.x - x) * 4, (launch.y - y) * 4
        if input.mousePressed() then
            local stone = world:createBody({x = launch.x, y = launch.y, vx = self.vx, vy = self.vy})
            stone:addCircle(8, {category = kStones, mask = kGround})
        end
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        local points, hit = world:predictPath(launch.x, launch.y, self.vx, self.vy, {radius = 8, mask = kGround})
        graphics2d.drawPolyline(points, 2, '#FFFFE080')
        if hit then
            graphics2d.drawCircle(hit.x, hit.y, 6, '#FFFF6040')
        end
        world:debugDraw()
    end,
})
```

### world:raycast(x1, y1, x2, y2, filter)

Casts a ray from `x1, y1` to `x2, y2` and returns the closest hit that passes the filter, or `nil` when the ray hits nothing. The optional filter is described in [Ray cast filters](#ray-cast-filters). Sensor shapes are hit like any other shape, so give them their own category and leave it out of the filter mask to ignore them. A ray that starts inside a shape does not see that shape, and a ray sees a chain only from the side the chain collides on.

Every ray and shape cast returns hits as tables with these fields.

| Field | Type | Meaning |
| --- | --- | --- |
| `shape` | Shape | The shape the cast hit. |
| `body` | Body | The body of that shape. |
| `x`, `y` | number | The hit point. |
| `normalX`, `normalY` | number | The surface normal at the hit point, as a unit vector. |
| `fraction` | number | How far along the cast the hit is, from 0 at the start to 1 at the end. |
| `distance` | number | The same span in world units. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local wall = world:createBody({type = 'static', x = 300, y = 0})
wall:addBox(20, 400)

local hit = world:raycast(0, 0, 600, 0)
if hit then
    print(hit.x, hit.y, hit.normalX, hit.fraction, hit.distance, hit.body == wall)
end
```

### world:raycastAll(x1, y1, x2, y2, filter)

Casts a ray from `x1, y1` to `x2, y2` and returns the list of hits that pass the filter, sorted from the start of the ray to its end. The list is empty when the ray hits nothing. Besides the fields of a [ray cast filter](#ray-cast-filters), the filter takes an integer `limit` that keeps only the first hits, so a bullet can pierce a given number of targets. A limit of 0, the default, keeps every hit.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
for index = 1, 5 do
    local post = world:createBody({type = 'static', x = index * 100, y = 0})
    post:addBox(10, 100)
end

for _, hit in ipairs(world:raycastAll(0, 0, 600, 0)) do
    print(hit.x, hit.fraction)
end

local pierced = world:raycastAll(0, 0, 600, 0, {limit = 2})
print(#pierced)
```

### world:castCircle(x, y, radius, dx, dy, filter)

Sweeps a circle of `radius` centered on `x, y` along the translation `dx, dy` and returns the first hit, or `nil`. The fraction tells how far the circle travels before it touches, so the circle stops at `x + dx * fraction, y + dy * fraction`. A circle that starts touching a shape hits it at fraction 0 with a zero normal. Use it for thick bullets, character probes and anything wider than a ray. A negative radius raises `A circle cast needs a finite radius of zero or more.`

### world:castBox(x, y, width, height, rotation, dx, dy, filter)

Sweeps a box of `width` by `height` centered on `x, y` and turned by `rotation` radians along the translation `dx, dy`, like `world:castCircle`. A size that is not positive raises `A box cast needs a positive size.`

### world:castCapsule(x1, y1, x2, y2, radius, dx, dy, filter)

Sweeps a capsule around the segment from `x1, y1` to `x2, y2` along the translation `dx, dy`, like `world:castCircle`. A radius that is not positive raises `A capsule cast needs a positive radius.`

### world:castPolygon(points, dx, dy, filter)

Sweeps the convex hull of the list `points` along the translation `dx, dy`, like `world:castCircle`. Fewer than three or more than eight points raise `A polygon cast needs between three and eight points.`

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local wall = world:createBody({type = 'static', x = 300, y = 0})
wall:addBox(20, 400)

local ball = world:castCircle(0, 0, 16, 600, 0)
print(ball.fraction * 600, ball.x, ball.normalX)

local crate = world:castBox(0, 0, 40, 40, 0.3, 600, 0)
local pill = world:castCapsule(0, -20, 0, 20, 8, 600, 0)
local wedge = world:castPolygon({{0, -20}, {30, 0}, {0, 20}}, 600, 0)
print(crate.distance, pill.distance, wedge.distance)
```

### world:bounceRay(x, y, dx, dy, length, bounces, filter)

Follows a ray from `x, y` in the direction `dx, dy` for `length` world units that reflects off every shape it hits, like a laser between mirrors or a ricocheting bullet, for at most `bounces` reflections. Returns the list of bounce hits in order and the point where the path ends, as three values. The `distance` and `fraction` of each hit measure the whole path up to that bounce. A zero direction raises a bad argument error with `the direction must not be zero`, a negative bounce count one with `the bounce count must not be negative`, and a length that is not finite raises `A bouncing physics ray needs a finite length.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local left = world:createBody({type = 'static', x = -200, y = 0})
left:addBox(20, 800)
local right = world:createBody({type = 'static', x = 200, y = 0})
right:addBox(20, 800)

local hits, endX, endY = world:bounceRay(0, 0, 1, 0.5, 2000, 3)
for index, hit in ipairs(hits) do
    print(index, hit.x, hit.y, hit.distance)
end
print('laser ends at', endX, endY)
```

### world:rayFan(x, y, angle, spread, count, length, filter)

Casts `count` rays of `length` world units from `x, y`, spread evenly across an arc of `spread` radians centered on `angle`, like a cone of vision or a shotgun blast. Returns a list with one entry per ray in order of increasing angle, which is the hit of that ray or `false` when it hit nothing. A single ray points along `angle`, and a length that is not finite raises a bad argument error with `the length must be finite`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local rock = world:createBody({type = 'static', x = 200, y = 0})
rock:addCircle(40)

local fan = world:rayFan(0, 0, 0, math.rad(60), 7, 400)
for index, hit in ipairs(fan) do
    print(index, hit and hit.distance or 'clear')
end
```

### world:lineOfSight(x1, y1, x2, y2, filter)

Returns `true` when no shape that passes the filter blocks the segment between both points, such as a guard checking whether it sees the player. Give the characters their own categories and leave them out of the mask, or skip them with `accept`, so they do not block their own sight.

```lua
local physics2d = require('haylen.physics2d')

local kWalls = 2
local world = physics2d.newWorld({gravity = {0, 0}})
local wall = world:createBody({type = 'static', x = 100, y = 0})
wall:addBox(20, 60, {category = kWalls})

print(world:lineOfSight(0, 0, 200, 0, {mask = kWalls}), world:lineOfSight(0, 100, 200, 100, {mask = kWalls}))
```

### world:raycastBatch(batch, filter)

Casts every ray of a [`RayBatch`](#raybatch) and stores the closest hit of each one in the batch, spread over the workers of the engine job system. Thousands of rays per frame, such as the sensors of many AI agents or the lines of a lidar effect, cost a fraction of separate `world:raycast` calls and reuse the buffers of the batch once it has grown. The filter takes `category`, `mask` and `group`, and an `accept` function raises `Ray batches take no "accept" function, because their rays run on worker threads.` The batch keeps the world alive for `batch:shape` and `batch:body`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
for _, wall in ipairs({{0, -300, 620, 20}, {0, 300, 620, 20}, {-300, 0, 20, 620}, {300, 0, 20, 620}}) do
    local body = world:createBody({type = 'static', x = wall[1], y = wall[2]})
    body:addBox(wall[3], wall[4])
end

local batch = physics2d.newRayBatch(360)
for index = 1, batch.size do
    local angle = math.rad(index)
    batch:setRay(index, 0, 0, math.cos(angle) * 1000, math.sin(angle) * 1000)
end
world:raycastBatch(batch)

local hit, x, y = batch:hit(45)
print(hit, x, y, batch:body(45).type)
```

### world:pick(camera, x, y, filter)

Returns the list of shapes under or near the screen point `x, y`, seen through `camera`, nearest first, which is how a click or a tap selects bodies. Screen points are in design coordinates, like the pointer positions of [`haylen.input`](input.md). The optional filter takes `category` and `mask`, as in [Query filters](#query-filters), and `radius`, the distance in design coordinates around the point within which shapes count, 0 by default, so a finger finds thin and small bodies near it. Shapes that contain the point come first, then the others by their distance from it. Unknown keys raise `Unknown option "<key>".`, and a negative radius raises a bad argument error with `the radius must be zero or more`.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld({gravity = {0, 0}})
local crate = world:createBody({x = 0, y = 0})
crate:addBox(60, 60)
local rod = world:createBody({x = 200, y = 0})
rod:addBox(4, 200)
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        if input.mousePressed() then
            local x, y = input.mousePosition()
            local picked = world:pick(camera, x, y, {radius = 16})
            if picked[1] then
                print('picked', picked[1].body == crate, picked[1].body == rod)
            end
        end
    end,
})
```

### world:debugDrawRays(order)

Draws the casts recorded since the last call while `world.debugRays` is `true` and clears them: each ray up to its hit or its end, a dot on the hit and a short line along its normal. Shape casts draw the path of their center. The world keeps the casts of one frame at most, because the first cast of a new frame drops the casts that no call drew, so recording without drawing never piles up casts. Call it from the `render` callback of a scene after `graphics2d.beginWorld()`. The optional `order` table takes the `layer`, `depth` and `blend` fields described in [`haylen.graphics2d`](graphics2d.md).

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld({gravity = {0, 0}})
local wall = world:createBody({type = 'static', x = 200, y = 0})
wall:addBox(20, 400)
world.debugRays = true
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        world:rayFan(0, 0, 0, math.rad(90), 9, 400)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        world:debugDrawRays({layer = 100})
    end,
})
```

### world:queryRect(rect, filter)

Returns a list of the shapes whose bounding boxes overlap `rect`. The rectangle is a `Rect` or a table `{x, y, width, height}`, and the optional filter is described in [Query filters](#query-filters). Bounding boxes are larger than rotated or round shapes, so check the result further when exact overlap matters. A negative width or height raises `A rectangle query needs a width and height of zero or more.`, so build a rectangle dragged up or to the left with `m.fromMinMax(dragStart:min(dragEnd), dragStart:max(dragEnd))` of [`haylen.math`](math.md) before the query.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local rock = world:createBody({type = 'static', x = 50, y = 50})
rock:addCircle(10)

for _, shape in ipairs(world:queryRect({0, 0, 100, 100})) do
    print(shape.body == rock)
end
```

### world:queryCircle(x, y, radius, filter)

Returns a list of the shapes that overlap the circle at `x, y` with the given radius. The optional filter is described in [Query filters](#query-filters).

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local barrel = world:createBody({x = 30, y = 0})
barrel:addCircle(12)

-- Pushes every dynamic body caught in an explosion away from its center.
local centerX, centerY = 0, 0
for _, shape in ipairs(world:queryCircle(centerX, centerY, 80)) do
    local body = shape.body
    if body.type == 'dynamic' then
        body:applyImpulse((body.x - centerX) * body.mass * 5, (body.y - centerY) * body.mass * 5)
    end
end
```

### world:queryPoint(x, y, filter)

Returns a list of the shapes that contain the point `x, y`. The optional filter is described in [Query filters](#query-filters). Segments and chains have no inside, so a point inside the area that a chain loop outlines, such as a merged region of [Tiled collision](#tiled-collision), finds nothing.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld({gravity = {0, 0}})
local button = world:createBody({type = 'static', x = 0, y = 0})
button:addBox(100, 40)

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
    end,
    update = function(self, dt)
        if input.mousePressed() then
            local x, y = self.camera:screenToWorld(input.mousePosition())
            print(#world:queryPoint(x, y) > 0)
        end
    end,
})
```

### world:debugDraw(order)

Draws the outlines of every shape and joint and the contact points of the world on the current canvas, with lines of the same width on the screen at any zoom, which helps while tuning collisions. Call it from the `render` callback of a scene after `graphics2d.beginWorld()`. The optional `order` table takes the `layer`, `depth` and `blend` fields described in [`haylen.graphics2d`](graphics2d.md). The `physics` [debug drawing](debug.md#drawings) draws every world that `physics2d.newWorld` made this way in every world canvas, without a call.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local ball = world:createBody({x = 0, y = 0})
ball:addCircle(20)

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        graphics2d.beginWorld(self.camera)
        world:debugDraw({layer = 100})
    end,
})
```

### World properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `gravity` | Vec2 | read and write | Gravity in world units per second squared. It accepts a `Vec2` or a table `{x, y}`, and writing it wakes every body. |
| `bodyCount` | integer | read | Number of bodies in the world. |
| `awakeBodyCount` | integer | read | Number of bodies that are awake. |
| `pixelsPerMeter` | number | read | The scale the world was created with. |
| `threads` | integer | read | The threads that step the world, 1 on the web. |
| `interpolate` | boolean | read | Whether the world records the transforms of its bodies for interpolation. |
| `subSteps` | integer | read and write | Solver sub-steps per step, at least 1. |
| `continuous` | boolean | read and write | Whether fast dynamic bodies sweep against static shapes. |
| `sleepEnabled` | boolean | read and write | Whether bodies fall asleep at rest. Turning it off wakes every body. |
| `maxSpeed` | number | read and write | Speed limit of every body in world units per second. |
| `contactHertz` | number | read and write | Stiffness of contacts in cycles per second, positive. |
| `contactDampingRatio` | number | read and write | Damping of the push that separates overlapping shapes, positive. |
| `contactPushSpeed` | number | read and write | Fastest speed in world units per second at which contacts push overlapping shapes apart. |
| `restitutionThreshold` | number | read and write | Slowest impact speed in world units per second that bounces. |
| `hitThreshold` | number | read and write | Slowest impact speed in world units per second that calls `onHit`. |
| `debugRays` | boolean | read and write | Records every ray and shape cast of the world for `world:debugDrawRays()` while `true`. |
| `onContactBegin` | function or nil | read and write | Called when two shapes start touching. |
| `onContactEnd` | function or nil | read and write | Called when two shapes stop touching. |
| `onHit` | function or nil | read and write | Called when two shapes hit each other fast. |
| `onSensorBegin` | function or nil | read and write | Called when a shape enters a sensor. |
| `onSensorEnd` | function or nil | read and write | Called when a shape leaves a sensor. |
| `onJointBreak` | function or nil | read and write | Called when a step breaks a joint. |

Fewer than one sub-step raises `A physics world needs at least one sub-step.`, a speed that is negative or not finite raises `A physics speed needs to be finite and zero or more.`, a contact stiffness that is not positive raises `A physics world needs a positive contact stiffness.` and a contact damping ratio that is not positive raises `A physics world needs a positive contact damping ratio.`. Assigning a value other than a function or `nil` to a callback raises an error, and so does reading or writing an unknown property.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
world.gravity = {0, -400}
world.subSteps = 8
world.maxSpeed = 8000
world.contactHertz = 40
world.hitThreshold = 300
print(world.gravity.x, world.gravity.y, world.bodyCount, world.awakeBodyCount, world.pixelsPerMeter, world.threads)
```

## Events

Assign a function to a callback property of the world to receive its events. Every callback runs inside `world:step()` for the events of that step, in this order: contact begins, contact ends, hits, sensor begins, sensor ends and joint breaks. The events of a step are gathered before the first callback runs, so a callback may destroy bodies or even step the world again and every event of the step still arrives. Body and shape handles passed to callbacks are new userdata every time, so compare them with `==` and keep per-body state in `body.data` instead of using handles as table keys.

Shapes decide which events they raise with the switches of [Shape options](#shape-options). A contact calls `onContactBegin` and `onContactEnd` when either of its shapes has `contactEvents` on and `onHit` when either has `hitEvents` on, so quiet shapes such as the debris of an explosion turn them off on both sides. A sensor reports a visitor only when both shapes have `sensorEvents` on.

### world.onContactBegin(a, b, contact)

Called when two solid shapes start touching. The arguments `a` and `b` are the bodies of `contact.shapeA` and `contact.shapeB`.

| Field | Type | Meaning |
| --- | --- | --- |
| `shapeA`, `shapeB` | Shape | The two shapes. |
| `x`, `y` | number | The first contact point, or `0, 0` when Box2D reports no point. |
| `normalX`, `normalY` | number | The contact normal, pointing from `shapeA` to `shapeB`. |
| `speed` | number | Always 0 for this event. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ground = world:createBody({type = 'static', x = 0, y = 200})
ground:addBox(800, 40)
local player = world:createBody({x = 0, y = 0})
player:addBox(32, 48)
player.data = {name = 'player', grounded = false}

world.onContactBegin = function(a, b, contact)
    for _, body in ipairs({a, b}) do
        if body.data and body.data.name == 'player' then
            body.data.grounded = true
        end
    end
end
```

### world.onContactEnd(a, b, contact)

Called when two solid shapes stop touching. The contact table carries only `shapeA` and `shapeB`, and its point, normal and speed are 0. When a shape was destroyed during the step, its body argument is `nil` and its shape handle reports `valid == false`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
world.onContactEnd = function(a, b, contact)
    if a and b then
        print('separated', a.data, b.data)
    end
end
```

### world.onHit(a, b, contact)

Called when two solid shapes hit each other faster than the hit threshold of the world, `world.hitThreshold`, which is one meter per second, or `pixelsPerMeter` world units per second, unless the world sets it. The contact table has the fields of `onContactBegin`, with the hit point, the normal and `speed`, the approach speed in world units per second. Use it for impact sounds and damage.

```lua
local physics2d = require('haylen.physics2d')
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local world = physics2d.newWorld({hitThreshold = 120})
local thud = assets.load('sfx/thud.wav')

world.onHit = function(a, b, contact)
    audio.play(thud, {volume = math.min(1, contact.speed / 800), x = contact.x, y = contact.y})
end
```

### world.onSensorBegin(sensor, visitor, shapes)

Called when a shape starts overlapping a sensor shape. The argument `sensor` is the body of the sensor shape and `visitor` is the body of the other shape. The `shapes` table has the fields `sensorShape` and `visitorShape`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local zone = world:createBody({type = 'static', x = 0, y = 200})
zone:addBox(200, 40, {sensor = true})
zone.data = {kind = 'water'}

world.onSensorBegin = function(sensor, visitor, shapes)
    if sensor.data and sensor.data.kind == 'water' then
        visitor.gravityScale = 0.3
    end
end
```

### world.onSensorEnd(sensor, visitor, shapes)

Called when a shape stops overlapping a sensor shape, with the same arguments as `onSensorBegin`. When a shape was destroyed during the step, its body argument is `nil`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
world.onSensorEnd = function(sensor, visitor, shapes)
    if visitor then
        visitor.gravityScale = 1
    end
end
```

### world.onJointBreak(joint, info)

Called for every joint that the step broke because its force passed its `breakForce` or its torque passed its `breakTorque`. The handle `joint` already reports `valid == false`. The table `info` holds `bodyA` and `bodyB`, the bodies the joint held, each `nil` when it is gone, and `forceX`, `forceY` and `torque`, the load that broke the joint, in world units.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
world.onJointBreak = function(joint, info)
    print('a joint broke under', math.abs(info.forceY), 'with', info.bodyB and info.bodyB.data)
end
```

## Body

The method `world:createBody()` returns a `haylen.Body` userdata. Every member except `valid`, `world` and `destroy` raises `The body was destroyed.` once the body is gone.

### body:addBox(width, height, options)

Adds a box centered on the body origin and returns its shape. The optional options table is described in [Shape options](#shape-options), and invalid options raise their error before the shape exists. A size that is not positive raises `A physics box needs a positive size.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local crate = world:createBody({x = 0, y = 0})
local shape = crate:addBox(32, 32, {density = 2, friction = 0.4, restitution = 0.1})
local lid = crate:addBox(36, 6, {offsetY = -19, rotation = 0.05})
```

### body:addCircle(radius, options)

Adds a circle and returns its shape. The circle is centered on the body origin moved by `offsetX` and `offsetY`. A radius that is not positive raises `A physics circle needs a positive radius.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ball = world:createBody({x = 0, y = 0, bullet = true})
ball:addCircle(8, {restitution = 0.8, density = 0.5, rollingResistance = 0.05})
```

### body:addCapsule(x1, y1, x2, y2, radius, options)

Adds a capsule, a segment from `x1, y1` to `x2, y2` in body coordinates rounded by `radius`, and returns its shape. Capsules suit characters because they slide over tile seams. A radius that is not positive raises `A physics capsule needs a positive radius.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local hero = world:createBody({x = 0, y = 0, fixedRotation = true})
hero:addCapsule(0, -16, 0, 16, 12, {friction = 0})
```

### body:addSegment(x1, y1, x2, y2, options)

Adds a line segment from `x1, y1` to `x2, y2` in body coordinates and returns its shape. Segments have no area, so they suit static walls and floors. Ends 0.005 meters apart or closer, which is 0.32 world units at the default 64 pixels per meter, raise `A physics segment needs ends more than 0.005 meters apart.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local floor = world:createBody({type = 'static', x = 0, y = 0})
floor:addSegment(-400, 200, 400, 200)
```

### body:addPolygon(points, options)

Adds a simple polygon and returns a list of its shapes. The points are a list of `Vec2` values or `{x, y}` tables in body coordinates. A convex outline with at most eight points becomes one shape, and any other outline is split into convex pieces of at most eight points with `m.polygon.decompose` of [`haylen.math`](math.md). Fewer than three points raise `A physics polygon needs at least three points.`, and outlines without area raise `A physics polygon needs at least three points that are not on one line.` or `A physics polygon needs a non-degenerate outline.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ramp = world:createBody({type = 'static', x = 0, y = 100})
local shapes = ramp:addPolygon({{0, 0}, {200, 0}, {200, -80}})
local cave = world:createBody({type = 'static', x = 300, y = 100})
cave:addPolygon({{0, 0}, {120, 0}, {60, -40}, {120, -120}, {0, -120}}, {friction = 0.9})
```

### body:addChain(points, loop, options)

Adds a chain of one-sided segments through the points and returns a list of its segment shapes. Chains suit terrain outlines, because bodies slide over the joints between their segments without catching, where separate boxes or segments catch on every seam. A `loop` set to `true` closes the outline and needs at least four points. An open chain needs at least two points and collides along every segment between the points it lists, because the world extends its first and last segments with the points that smooth the contacts at its ends. Chains use `friction`, `restitution`, `rollingResistance`, `tangentSpeed`, `oneWay`, the collision filter, the event switches, `offsetX`, `offsetY` and `rotation` from the options and ignore `density` and `sensor`. Fewer points raise `A physics chain needs at least four points for a loop and two for an open chain.`.

Each segment collides only on its left side as seen on screen, walking from one point to the next, and bodies pass through it from the other side. So an open chain listed from left to right carries bodies on top of it, a loop listed counter-clockwise on screen holds bodies inside it, like the walls of an arena, and a loop listed clockwise on screen is solid from the outside, like an island. Ray casts hit a chain only from that side too, while shape casts find it from both sides. Destroying any segment destroys the whole chain.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local hills = world:createBody({type = 'static', x = 0, y = 0})
hills:addChain({{-600, 200}, {-300, 150}, {0, 220}, {300, 120}, {600, 200}}, false, {friction = 0.8})

-- Down the left wall, along the floor and up the right wall, so the walls face inward.
local tank = world:createBody({type = 'static', x = 0, y = 0})
tank:addChain({{-300, -200}, {-300, 300}, {300, 300}, {300, -200}}, false)

local rock = world:createBody({type = 'static', x = 0, y = 0})
rock:addChain({{-60, -40}, {60, -40}, {60, 40}, {-60, 40}}, true)
```

### Shape options

Every `add` method takes the same optional options table. Unknown keys raise `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `density` | number | `1` | Mass per square meter, finite and not negative. |
| `friction` | number | `0.6` | Friction coefficient, usually between 0 and 1, finite and not negative. |
| `restitution` | number | `0` | Bounciness, where 1 keeps all the speed of a bounce, finite and not negative. |
| `rollingResistance` | number | `0` | Slows round shapes that roll on others, such as a ball on grass, finite and not negative. |
| `sensor` | boolean | `false` | Makes the shape a sensor that reports overlaps and never collides. |
| `category` | integer | `1` | Collision category bits of the shape. |
| `mask` | integer | `-1` (all bits) | Categories the shape collides with. |
| `group` | integer | `0` | Collision group, described in [Collision filtering](#collision-filtering). |
| `offsetX`, `offsetY` | number | `0` | Moves the shape away from the body origin. |
| `rotation` | number | `0` | Rotates the shape around the body origin before the offset, in radians. |
| `tangentSpeed` | number | `0` | Turns the surface into a conveyor that carries touching bodies at this speed, described in [Conveyors](#conveyors). |
| `oneWay` | Vec2 | `nil` | Makes the shape a one-way platform, described in [One-way platforms](#one-way-platforms). |
| `contactEvents` | boolean | `true` | Lets the contacts of the shape call `onContactBegin` and `onContactEnd`. |
| `hitEvents` | boolean | `true` | Lets the contacts of the shape call `onHit`. |
| `sensorEvents` | boolean | `true` | For a sensor, whether it detects visitors, and for any other shape, whether sensors detect it. |

A density, friction, restitution or rolling resistance that is negative or not finite raises `A physics shape needs a finite density, friction, restitution and rolling resistance of zero or more.`, and a zero `oneWay` direction raises `A one-way direction cannot be zero.`, both before the shape is created. The materials and the event switches also change while the shape exists, as [Shape properties](#shape-properties) describes.

### body:shapes()

Returns a list of every shape of the body, including chain segments.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local body = world:createBody()
body:addBox(20, 20)
body:addCircle(10, {offsetY = -20})
for _, shape in ipairs(body:shapes()) do
    shape.mask = 1
end
```

### body:outlines()

Returns the outline of every shape of the body in world units, where the body is, as a list of tables with `points`, a list of `Vec2` values, and `closed`, which is `true` when the last point joins back to the first one. Each outline is what [`shape:outline()`](#shapeoutline) returns, except that the segments of each chain join into one outline through every point the chain lists, closed for a loop. Use it to draw bodies made by the engine, such as the pieces of `physics2d.fracture` or the bones of `physics2d.newRagdoll`, without knowing their shapes.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local camera = graphics2d.newCamera()
local glass = world:createBody({x = 0, y = -200})
glass:addBox(64, 64)
local pieces = physics2d.fracture(glass)

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        for _, piece in ipairs(pieces) do
            for _, outline in ipairs(piece:outlines()) do
                if outline.closed then
                    graphics2d.drawPolygon(outline.points, '#FF9AD0EC')
                end
                graphics2d.drawPolyline(outline.points, 2, '#FF1B1E2B', outline.closed)
            end
        end
    end,
})
```

### body:contacts()

Returns a list of the contacts of the body that touch, one table for each pair of touching shapes. Unlike the events, the list tells at any moment what a body stands on or pushes against.

| Field | Type | Meaning |
| --- | --- | --- |
| `shape` | Shape | The shape of this body. |
| `otherShape` | Shape | The shape it touches. |
| `other` | Body | The body of `otherShape`. |
| `x`, `y` | number | The first contact point. |
| `normalX`, `normalY` | number | The contact normal as a unit vector, pointing from `shape` toward `otherShape`. |
| `impulse` | number | The impulse that pushed the shapes apart in the last step, in kilograms times units per second. |

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local ground = world:createBody({type = 'static', x = 0, y = 200})
ground:addBox(800, 40)
local player = world:createBody({x = 0, y = 0, fixedRotation = true})
player:addCapsule(0, -16, 0, 16, 12)

-- The ground lies below the player, so its normal points down, toward positive y.
local function standing(body)
    for _, contact in ipairs(body:contacts()) do
        if contact.normalY > 0.7 then
            return true
        end
    end
    return false
end

scene.push({
    update = function(self, dt)
        if input.keyPressed('space') and standing(player) then
            player:applyImpulse(0, -player.mass * 500)
        end
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

### body:applyForce(fx, fy, px, py)

Applies a force until the next step and wakes the body. Without `px, py` the force acts on the center of mass. With a world point `px, py` it also makes the body spin.

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local ship = world:createBody({gravityScale = 0})
ship:addBox(30, 20)

scene.push({
    fixedUpdate = function(self, step)
        if input.keyDown('up') then
            ship:applyForce(0, -ship.mass * 600)
        end
        if input.keyDown('right') then
            ship:applyForce(0, -ship.mass * 50, ship.x + 15, ship.y)
        end
        world:step(step)
    end,
})
```

### body:applyImpulse(ix, iy, px, py)

Changes the velocity at once by the impulse divided by the mass and wakes the body. Without `px, py` the impulse acts on the center of mass. With a world point `px, py` it also makes the body spin.

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local player = world:createBody({fixedRotation = true})
player:addBox(32, 48)

scene.push({
    update = function(self, dt)
        if input.keyPressed('space') then
            player:applyImpulse(0, -player.mass * 500)
        end
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

### body:applyTorque(torque)

Applies a torque until the next step and wakes the body. The torque is in kilograms times squared world units per second squared, as described in [Units and coordinates](#units-and-coordinates).

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld({gravity = {0, 0}})
local wheel = world:createBody()
wheel:addCircle(32)

scene.push({
    fixedUpdate = function(self, step)
        if input.keyDown('right') then
            -- Speeds the spin up by 3 radians per second every second.
            wheel:applyTorque(wheel.inertia * 3)
        end
        world:step(step)
    end,
})
```

### body:applyAngularImpulse(impulse)

Changes the angular velocity at once by the impulse divided by the rotational inertia and wakes the body. The impulse is in kilograms times squared world units per second.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local coin = world:createBody()
coin:addCircle(8)
-- Spins the coin up to 10 radians per second at once.
coin:applyAngularImpulse(coin.inertia * 10)
print(coin.angularVelocity)
```

### body:setTransform(x, y, rotation)

Teleports the body to `x, y` with the given rotation in radians. The rotation defaults to 0, so pass `body.rotation` to keep the current angle. A teleport skips the sweeps of continuous collision and shoves aside what the body lands in, so it suits spawning and respawning, while bodies that push and carry others, such as moving platforms, move with [`body:moveTo()`](#bodymovetox-y-rotation-seconds).

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local player = world:createBody({x = 0, y = 0})
player:addBox(32, 48)
player:setTransform(100, -50, player.rotation)
```

### body:moveTo(x, y, rotation, seconds)

Gives the body the velocity that brings it to `x, y` and the angle `rotation` after `seconds`, so a kinematic platform that follows a path, a tween or an animation pushes and carries what touches it instead of teleporting through it. The rotation defaults to the current one and the time to one fixed step, `haylen.fixedStep()`, so calling it once per fixed step before `world:step()` moves the body exactly to each target. The velocity stays until something changes it, and a target the body already holds stops it. Static bodies ignore it. A time that is not positive raises `A physics body moves to a target over a positive time.`.

```lua
local physics2d = require('haylen.physics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local platform = world:createBody({type = 'kinematic', x = 0, y = 200})
platform:addBox(160, 16)
local crate = world:createBody({x = 0, y = 170})
crate:addBox(32, 32)

scene.push({
    fixedUpdate = function(self, step)
        local time = haylen.elapsed() + step
        platform:moveTo(math.sin(time) * 300, 200 + math.cos(time * 2) * 40)
        world:step(step)
    end,
})
```

### body:velocityAt(x, y)

Returns the velocity of the world point `x, y` as if it were fixed to the body, as a `Vec2`, which includes the spin of the body, such as the speed of the rim of a wheel or of the ground under the feet of a character.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local wheel = world:createBody({x = 0, y = 0, vx = 10, angularVelocity = 2})
wheel:addCircle(50)
local rim = wheel:velocityAt(0, 50)
print(rim.x, rim.y) -- -90.0 0.0
```

### body:resetMass()

Computes the mass, the center of mass and the rotational inertia of the body from the density and the area of its shapes again, which undoes the values that `mass`, `centerOfMass` and `inertia` set. Adding or destroying a shape and changing a density compute them from the shapes again too.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local barrel = world:createBody()
barrel:addCircle(20)
barrel.mass = barrel.mass * 3
barrel:resetMass()
print(barrel.mass)
```

### body:dropThrough(seconds)

Lets the body pass through every one-way platform for `seconds`, 0.2 by default, and through a platform it is still inside after that until it leaves it, so a character drops through the platform it stands on and lands on the next one below. It wakes the body. A negative or infinite time raises `A physics body drops through one-way platforms for a finite time of zero or more.`. The section [One-way platforms](#one-way-platforms) has an example.

### body:renderTransform()

Returns the `x`, `y` and `rotation` to draw the body at in this frame. In a world created with `interpolate`, the body lies between its transforms of the last two steps, blended by [`haylen.interpolation()`](haylen.md#hayleninterpolation), which keeps motion smooth when frames and fixed steps do not line up. A body that the last step did not move, or that a teleport placed, reads where it is, and so does every body of a world without interpolation.

```lua
local physics2d = require('haylen.physics2d')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld({interpolate = true})
local ground = world:createBody({type = 'static', x = 0, y = 300})
ground:addBox(1200, 40)
local crate = world:createBody({x = 0, y = -200, angularVelocity = 2})
crate:addBox(48, 48)
local camera = graphics2d.newCamera()

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        local x, y, rotation = crate:renderTransform()
        graphics2d.beginWorld(camera)
        graphics2d.draw(graphics.whiteTexture(), x, y, {width = 48, height = 48, rotation = rotation, color = '#FFC08040'})
    end,
})
```

### body:destroy()

Destroys the body with its shapes and joints and releases its `data` at once. Destroying a body twice does nothing.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local enemy = world:createBody()
enemy:addCircle(10)
enemy:destroy()
print(enemy.valid)
```

### Body properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | boolean | read | The value is `false` once the body is destroyed. |
| `world` | PhysicsWorld | read | The world that owns the body. |
| `type` | string | read and write | `'static'`, `'kinematic'` or `'dynamic'`. |
| `x`, `y` | number | read and write | Position of the body origin. Writing teleports the body. |
| `position` | Vec2 | read and write | Position as a `Vec2`. Writing accepts a `Vec2` or `{x, y}` and teleports the body. |
| `rotation` | number | read and write | Angle in radians. |
| `velocity` | Vec2 | read and write | Linear velocity in units per second. Writing accepts a `Vec2` or `{x, y}`. |
| `angularVelocity` | number | read and write | Spin in radians per second. |
| `mass` | number | read and write | Mass in kilograms, computed from the shapes. Writing it scales the inertia with it. |
| `inertia` | number | read and write | Rotational inertia around the center of mass, in kilograms times squared units. |
| `centerOfMass` | Vec2 | read and write | Center of mass in the space of the body. Writing accepts a `Vec2` or `{x, y}`, and a center low in the body keeps it upright like a keel. |
| `worldCenter` | Vec2 | read | Center of mass in world units. |
| `awake` | boolean | read and write | Whether the body is awake. Sleeping bodies are skipped by the solver until something touches them, and writing `true` wakes them. |
| `sleepEnabled` | boolean | read and write | Lets the body fall asleep when it comes to rest. |
| `sleepThreshold` | number | read and write | Speed in units per second below which the body may fall asleep. |
| `enabled` | boolean | read and write | Disabled bodies leave the simulation and stop colliding until enabled again. |
| `linearDamping` | number | read and write | Slows the linear velocity over time. It must be finite and not negative. |
| `angularDamping` | number | read and write | Slows the spin over time. It must be finite and not negative. |
| `gravityScale` | number | read and write | Multiplies the world gravity for this body. |
| `fixedRotation` | boolean | read and write | Keeps the body from rotating. |
| `bullet` | boolean | read and write | Sweeps the body against dynamic and kinematic bodies too. |
| `data` | any | read and write | Any Lua value attached to the body, such as the entity it belongs to. The world keeps it until the body is destroyed: `body:destroy()` releases it at once, and the next `world:step()` releases the data of bodies destroyed any other way, such as by `physics2d.fracture`, the `destroy` of a rope, ragdoll, vehicle or mover, a terrain, or the garbage collection of a helper that owns bodies. |

A mass, inertia or center of mass that is negative or not finite raises `A physics body needs a finite mass, center of mass and inertia of zero or more.`, and a sleep threshold that is negative or not finite raises `A physics speed needs to be finite and zero or more.`. Two handles of the same body compare equal with `==`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local crate = world:createBody({x = 0, y = 0})
crate:addBox(32, 32)
crate.data = {name = 'crate', health = 3}
crate.velocity = {120, 0}
crate.centerOfMass = {0, 12}
crate.sleepThreshold = 1
crate.type = 'kinematic'
crate.x = 50
print(crate.position.x, crate.velocity.x, crate.mass, crate.inertia, crate.worldCenter.y, crate.awake, crate.world == world)
```

## Shape

The `add` methods, `body:shapes()`, queries and events return `haylen.Shape` userdata. Every member except `valid` and `destroy` raises `The shape was destroyed.` once the shape is gone.

### shape:destroy()

Removes the shape from its body. A chain segment destroys its whole chain. Destroying a shape twice does nothing.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local body = world:createBody()
local shield = body:addCircle(40, {sensor = true})
shield:destroy()
print(shield.valid)
```

### shape:outline()

Returns the outline of the shape in world units, where its body is, as a table with `points`, a list of `Vec2` values, and `closed`, which is `true` when the last point joins back to the first one. A polygon gives its corners, closed. A circle gives points around it, closed, and a capsule points around its two round ends, closed, which join into its straight sides, one step every two world units of radius and between 16 and 96 steps for a whole turn. A segment or a chain segment gives its two ends, open. The call `graphics2d.drawPolygon(outline.points, color)` fills a closed outline and `graphics2d.drawPolyline(outline.points, thickness, color, outline.closed)` strokes any of them.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local body = world:createBody({x = 100, y = 50})
local ball = body:addCircle(12)
local outline = ball:outline()
print(#outline.points, outline.closed)
```

### shape:overlaps()

Returns the list of shapes inside a sensor shape after the last step, which stays right through teleports and destroyed visitors where counting the sensor events would not. Visitors need `sensorEvents` on, as for the events, and static shapes count too, so a pressure plate keeps a small gap above the ground it rests on or the ground turns off its `sensorEvents`. A shape that is not a sensor raises `Only sensor shapes have overlaps.`.

```lua
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local zone = world:createBody({type = 'static', x = 0, y = 200})
local trigger = zone:addBox(300, 60, {sensor = true})
local crate = world:createBody({x = 0, y = 0})
crate:addBox(32, 32)

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
        if #trigger:overlaps() > 0 then
            print('the zone holds', #trigger:overlaps(), 'shapes')
        end
    end,
})
```

### Shape properties

The geometry properties return new values on every read. The `points` are in the space of the body, where the offset and rotation of the shape options already apply, and `worldPoints` are the same points in world units where the body is.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | boolean | read | The value is `false` once the shape or its body is destroyed. |
| `body` | Body | read | The body the shape belongs to. |
| `kind` | string | read | `'circle'`, `'capsule'`, `'segment'`, `'polygon'` or `'chainSegment'`, one segment of a chain. |
| `points` | list of Vec2 | read | The points that place the shape on its body: the center of a circle, the two centers of a capsule, the two ends of a segment or a chain segment, or the corners of a polygon in order around it. |
| `worldPoints` | list of Vec2 | read | The `points` in world units. |
| `radius` | number | read | The radius of a circle or a capsule, and `0` for the other kinds. |
| `sensor` | boolean | read | Whether the shape is a sensor. |
| `bounds` | Rect | read | The bounding box Box2D tracks for the shape, which includes a small collision margin. |
| `density` | number | read and write | Mass per square meter. Writing it updates the mass of the body. |
| `friction` | number | read and write | Friction coefficient. |
| `restitution` | number | read and write | Bounciness. |
| `rollingResistance` | number | read and write | Resistance of round shapes to rolling. |
| `contactEvents` | boolean | read and write | Whether the contacts of the shape call `onContactBegin` and `onContactEnd`. |
| `hitEvents` | boolean | read and write | Whether the contacts of the shape call `onHit`. |
| `sensorEvents` | boolean | read and write | Whether a sensor detects visitors, or whether sensors detect this shape. |
| `category` | integer | read and write | Collision category bits. |
| `mask` | integer | read and write | Categories the shape collides with. The default of all bits reads as `-1`. |
| `group` | integer | read and write | Collision group, described in [Collision filtering](#collision-filtering). |
| `tangentSpeed` | number | read and write | Conveyor speed of the surface in world units per second. |
| `oneWay` | Vec2 or nil | read and write | Direction of a one-way platform in the space of the body, or `nil` for a solid shape. A zero vector raises `A one-way direction cannot be zero.` |

A density, friction, restitution or rolling resistance that is negative or not finite raises `A physics shape needs a finite density, friction, restitution and rolling resistance of zero or more.`. Two handles of the same shape compare equal with `==`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local body = world:createBody()
local shape = body:addBox(20, 20)
shape.category = 4
shape.mask = 1 | 2
shape.group = -1
shape.friction = 0.1
shape.density = 3
shape.hitEvents = false
print(shape.body == body, shape.sensor, shape.group, shape.bounds.width, body.mass)
print(shape.kind, #shape.points, shape.worldPoints[1].x, shape.radius)
```

## Joint

The method `world:createJoint()` returns a `haylen.Joint` userdata. Every setting a joint type takes at creation also changes while the joint runs, and changing one wakes the bodies of the joint. Every member except `valid` and `destroy` raises `The physics joint was destroyed.` once the joint or one of its bodies is gone, and a member that the type of the joint lacks raises an error that names the joint types that have it.

### Joint properties

| Property | Joints | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | All | read | The value is `false` once the joint or one of its bodies is destroyed. |
| `type` | All | read | The joint type, such as `'revolute'`. |
| `bodyA`, `bodyB` | All | read | The two bodies the joint connects. |
| `anchorA`, `anchorB` | All | read | Where the joint holds each body in world units now, as a `Vec2`, which is where a rope or a spring between the bodies is drawn. |
| `constraintForce` | All | read | The force the joint applied in the last step to hold its bodies, as a `Vec2` in world units. |
| `constraintTorque` | All | read | The torque the joint applied in the last step. |
| `linearSeparation` | All | read | How far the anchors drifted apart in world units, which grows while a joint stretches. |
| `angularSeparation` | All | read | How far the angle drifted from what the joint holds, in radians. |
| `constraintHertz` | All | read and write | Stiffness that holds the joint together in cycles per second, `60` by default. Lower values make the joint soft. |
| `constraintDampingRatio` | All | read and write | Damping of that stiffness, `2` by default. |
| `breakForce`, `breakTorque` | All | read and write | The load beyond which a step breaks the joint, or `nil` for a joint that never breaks from it. |
| `target` | Mouse | read and write | The point the joint pulls `bodyB` toward, as a `Vec2`. Writing accepts a `Vec2` or `{x, y}`. |
| `angle` | Revolute | read | The angle from the pose at creation, in radians. |
| `translation` | Prismatic | read | The translation along the axis, in world units. |
| `currentLength` | Distance | read | The distance between the anchors. |
| `length` | Distance | read and write | The rest length. |
| `enableLimit` | Revolute, prismatic, wheel, distance | read and write | Whether the limits hold. |
| `lower`, `upper` | Revolute, prismatic, wheel, distance | read and write | The limits: angles for revolute joints, translations for prismatic and wheel joints and the shortest and longest length for distance joints. |
| `enableMotor` | Revolute, prismatic, wheel, distance | read and write | Whether the motor drives the joint. |
| `motorSpeed` | Revolute, prismatic, wheel, distance | read and write | The motor speed in radians per second for revolute and wheel joints, and in world units per second for prismatic and distance joints. |
| `maxMotorForce` | Prismatic, distance, mouse, motor | read and write | The most force the motor or the pull applies. |
| `maxMotorTorque` | Revolute, wheel, motor | read and write | The most torque the motor applies. |
| `motorForce` | Prismatic, distance | read | The force the motor applied in the last step. |
| `motorTorque` | Revolute, wheel | read | The torque the motor applied in the last step. |
| `enableSpring` | Distance, revolute, prismatic, wheel, mouse, weld | read and write | Whether the spring acts. Mouse and weld joints are always springy and read `true`, and only distance, revolute, prismatic and wheel joints switch it. |
| `hertz`, `dampingRatio` | Distance, revolute, prismatic, wheel, mouse, weld | read and write | The stiffness of the spring in cycles per second and its damping. A weld joint of 0 hertz is rigid. |
| `targetAngle` | Revolute | read and write | The angle the spring pulls toward. |
| `targetTranslation` | Prismatic | read and write | The translation the spring pulls toward. |
| `linearOffset`, `angularOffset` | Motor | read and write | The offset of `bodyB` from `bodyA` in the space of `bodyA`, as a `Vec2`, and the angle between them, which the joint drives toward. |

A member that the joint type lacks raises one of `Only mouse joints have a target.`, `Only revolute joints have an angle.`, `Only prismatic joints have a translation.`, `Only distance joints have a current length.`, `Only distance joints have a length.`, `Only revolute, prismatic, wheel and distance joints have limits.`, `Only revolute, prismatic, wheel and distance joints have a motor.`, `Only revolute, prismatic, wheel and distance joints have a motor speed.`, `Only prismatic, distance, mouse and motor joints have a maximum motor force.`, `Only revolute, wheel and motor joints have a maximum motor torque.`, `Only prismatic and distance joints report a motor force.`, `Only revolute and wheel joints report a motor torque.`, `Only distance, revolute, prismatic, wheel, mouse and weld joints have a spring.`, `Only distance, revolute, prismatic and wheel joints switch their spring.`, `Only revolute joints have a target angle.`, `Only prismatic joints have a target translation.` or `Only motor joints have offsets.`.

Values out of range raise these errors.

- A `lower` above `upper` raises `A joint needs a lower limit that is not above the upper one.`, and revolute limits beyond 0.99 pi radians raise `A revolute joint needs a lower limit that is not above the upper one, both within 0.99 pi radians of zero.`.
- A distance joint shorter than 0.005 meters raises `A distance joint needs a length of at least 0.005 meters.`.
- A maximum motor force or torque that is negative or not finite raises `A joint needs a finite maximum motor force of zero or more.` or `A joint needs a finite maximum motor torque of zero or more.`.
- A spring stiffness or damping that is negative or not finite raises `A joint spring needs a finite stiffness and damping ratio of zero or more.`.
- A constraint stiffness that is not positive raises `A joint needs a positive constraint stiffness.`, and a negative or infinite constraint damping raises `A joint needs a finite constraint damping ratio of zero or more.`.
- A negative break limit raises `A joint needs a break force and a break torque of zero or more.`.

Two handles of the same joint compare equal with `==`.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local base = world:createBody({type = 'static'})
local lever = world:createBody({x = 60, y = 0})
lever:addBox(120, 10)
local hinge = world:createJoint('revolute', base, lever, {enableSpring = true, hertz = 3, dampingRatio = 0.5})
local hook = world:createBody({x = 0, y = 100})
hook:addCircle(10)
local cable = world:createJoint('distance', base, hook, {bx = 0, by = 100, enableSpring = true, enableLimit = true, upper = 100})

scene.push({
    update = function(self, dt)
        if input.keyPressed('up') then
            hinge.targetAngle = -1
        elseif input.keyPressed('down') then
            hinge.targetAngle = 0.5
        end
        if input.keyDown('space') then
            cable.upper = math.max(20, cable.upper - 100 * dt)
        end
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, string.format('Angle %.2f, cable %.0f', hinge.angle, cable.currentLength), 40, 40, {size = 28})
    end,
})
```

### joint.target

The target of a mouse joint, which writing moves. This scene drags a crate with the mouse.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local anchor = world:createBody({type = 'static'})
local crate = world:createBody({x = 0, y = 0})
crate:addBox(32, 32)
local drag = nil

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
    end,
    update = function(self, dt)
        local x, y = self.camera:screenToWorld(input.mousePosition())
        if input.mousePressed() then
            drag = world:createJoint('mouse', anchor, crate, {bx = x, by = y})
        elseif input.mouseReleased() and drag then
            drag:destroy()
            drag = nil
        elseif drag then
            drag.target = {x, y}
        end
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

The grabber of [Grabbers](#grabbers) drags bodies this way with a pull sized for everything joined to the body it holds.

### joint.motorSpeed

The motor speed of a revolute or wheel joint in radians per second, or of a prismatic or distance joint in world units per second, which writing changes and which wakes the bodies.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local base = world:createBody({type = 'static'})
local blade = world:createBody()
blade:addBox(100, 8)
local fan = world:createJoint('revolute', base, blade, {enableMotor = true, motorSpeed = 2, maxMotorTorque = 20000})
fan.motorSpeed = -fan.motorSpeed * 2
print(fan.motorSpeed) -- -4.0
```

### Breaking joints

A joint with a `breakForce` breaks once the force it applies to hold its bodies passes it, and one with a `breakTorque` once its torque passes that, both in world units. The step that loads the joint beyond its limit destroys it after Box2D advances the bodies and calls `world.onJointBreak` with it, in the order of the joints. Without the callback the joint breaks all the same.

```lua
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local wall = world:createBody({type = 'static', x = 0, y = 0})
local shelf = world:createBody({x = 0, y = 40})
shelf:addBox(80, 10)
local bracket = world:createJoint('weld', wall, shelf, {ax = 0, ay = 0, breakForce = shelf.mass * 980 * 5})
local anvil = world:createBody({x = 0, y = -200})
anvil:addBox(40, 40, {density = 20})

world.onJointBreak = function(joint, info)
    print(joint == bracket, joint.valid, info.bodyB == shelf, math.abs(info.forceY))
end

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

### joint:destroy()

Removes the joint, so its bodies move independently again. Destroying a joint twice does nothing, and destroying one of its bodies destroys the joint too.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local a = world:createBody({type = 'static'})
local b = world:createBody({x = 50})
b:addCircle(10)
local link = world:createJoint('weld', a, b)
link:destroy()
print(link.valid)
```

## Query filters

The `query` methods and `world:pick()` take an optional filter table with the integer fields `category` (default `1`) and `mask` (default all bits). A shape is found when its category shares a bit with the filter mask and the filter category shares a bit with the shape mask. The filter of `world:pick()` also takes `radius`. Other keys raise `Unknown option "<key>".`.

```lua
local physics2d = require('haylen.physics2d')

local kWalls = 2
local kEnemies = 4
local world = physics2d.newWorld({gravity = {0, 0}})
local wall = world:createBody({type = 'static', x = 200})
wall:addBox(20, 200, {category = kWalls})
local enemy = world:createBody({x = 100})
enemy:addCircle(10, {category = kEnemies})

local blocked = world:raycast(0, 0, 400, 0, {mask = kWalls})
local seen = world:queryCircle(0, 0, 150, {mask = kEnemies})
print(blocked.body == wall, #seen)
```

## Ray cast filters

The ray and shape casts, `world:lineOfSight()` and `world:raycastBatch()` take an optional filter table with these fields, and `world:predictPath()` takes its `category`, `mask` and `group`. Other keys raise `Unknown option "<key>".`.

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `category` | integer | `1` | The category of the cast. A shape is seen when its category shares a bit with the mask and its mask shares a bit with this category, like a collision. |
| `mask` | integer | all bits | The categories the cast sees. |
| `group` | integer | `0` | A shape that shares a nonzero group is always seen when the group is positive and never seen when it is negative, so a ragdoll can look past its own limbs. |
| `accept` | function | none | Called with each candidate hit table in order from the start of the cast, after the other fields. A `true` result takes the hit, and a `false` one skips it and keeps looking. Errors raised by it propagate out of the cast. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local glass = world:createBody({type = 'static', x = 100})
glass:addBox(10, 200)
glass.data = {transparent = true}
local wall = world:createBody({type = 'static', x = 200})
wall:addBox(20, 200)

local hit = world:raycast(0, 0, 400, 0, {
    accept = function(candidate)
        return not (candidate.body.data and candidate.body.data.transparent)
    end,
})
print(hit.body == wall)
```

## RayBatch

A `RayBatch` holds rays cast together by `world:raycastBatch()` with a result slot for each one. Rays count from 1, and an index outside the batch raises a bad argument error with `the ray index is outside the batch`. The batch keeps its buffers, so casting the same number of rays again reuses them, and reading results creates no tables.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read and write | Number of rays. Growing the batch adds rays of zero length and keeps the existing ones. |

### batch:setRay(index, x1, y1, x2, y2)

Sets the ray at `index` to run from `x1, y1` to `x2, y2`.

### batch:ray(index)

Returns the ray at `index` as `x1, y1, x2, y2`.

### batch:hit(index)

Returns `false` when the ray at `index` hit nothing in the last cast, and otherwise `true`, `x`, `y`, `normalX`, `normalY` and `fraction`.

### batch:shape(index)

Returns the shape the ray at `index` hit in the last cast, or `nil`.

### batch:body(index)

Returns the body the ray at `index` hit in the last cast, or `nil`.

```lua
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld({gravity = {0, 0}})
local rock = world:createBody({type = 'static', x = 150, y = 0})
rock:addCircle(30)

local agents = {{x = 0, y = 0}, {x = 0, y = 80}, {x = 300, y = 0}}
local sensors = physics2d.newRayBatch(#agents)

scene.push({
    update = function(self, dt)
        for index, agent in ipairs(agents) do
            sensors:setRay(index, agent.x, agent.y, agent.x + 200, agent.y)
        end
        world:raycastBatch(sensors)
        for index, agent in ipairs(agents) do
            local blocked, x = sensors:hit(index)
            agent.blockedAhead = blocked and x - agent.x or nil
        end
    end,
})
```

## Collision filtering

Two shapes collide when the category of each one shares a bit with the mask of the other. Categories and masks are 64-bit integers, so up to 64 categories exist. Shapes that share a positive `group` always collide and shapes that share a negative `group` never collide, whatever their categories and masks say. The parts of a ragdoll or a vehicle share a negative group, so two of them with the same group pass through each other, and each one that should hit the others takes a group of its own.

```lua
local physics2d = require('haylen.physics2d')

local kPlayer = 1
local kEnemy = 2
local kPickup = 4
local world = physics2d.newWorld()

local player = world:createBody()
player:addBox(32, 48, {category = kPlayer, mask = kEnemy | kPickup})
local coin = world:createBody({type = 'static', x = 100})
coin:addCircle(8, {category = kPickup, mask = kPlayer, sensor = true})

-- Parts of one ragdoll share a negative group and never collide with each other.
local torso = world:createBody({x = 300})
torso:addBox(20, 40, {group = -1})
local arm = world:createBody({x = 320})
arm:addBox(8, 30, {group = -1})
```

## One-way platforms

A shape with a `oneWay` direction only blocks bodies that touch it from the side the direction points to. The direction is in the space of the body, so it turns with the body: with y pointing down, `{0, -1}` on an unrotated body makes a platform that bodies jump through from below and land on from above, and the same platform turned upside down blocks bodies from below and lets them fall through from above. A contact holds only while its normal leans at most 60 degrees away from the direction, so bodies also pass the platform from its sides. A body lands on the platform only when it was outside it at the start of the step, or at most 0.02 meters inside it, so a body that rises halfway into a platform falls back under it instead of popping up on top, and a body that falls fast lands wherever it reaches the platform. Setting `shape.oneWay = nil` makes the shape solid again.

The method `body:dropThrough()` lets a body fall through the platform it stands on. [Movers](#movers) pass one-way platforms from their closed side and land on their open side the same way, and `mover:dropThrough()` drops a mover through the platform it stands on. Tiled objects and tile shapes whose `oneWay` property is `true` become platforms that point up, as [Tiled collision](#tiled-collision) describes.

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
for _, y in ipairs({0, 120, 240}) do
    local ledge = world:createBody({type = 'static', x = 0, y = y})
    ledge:addBox(240, 8, {oneWay = {0, -1}})
end

local player = world:createBody({x = 0, y = -40, fixedRotation = true})
player:addCapsule(0, -12, 0, 12, 10)

scene.push({
    update = function(self, dt)
        if input.keyPressed('space') then
            player:applyImpulse(0, -player.mass * 600)
        elseif input.keyPressed('down') then
            player:dropThrough()
        end
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

## Conveyors

A shape with a `tangentSpeed` moves the bodies it touches along its surface at that speed in world units per second, clockwise around the shape as seen on screen, so a positive speed carries bodies on top of a platform to the right. Friction decides how fast they catch up.

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local belt = world:createBody({type = 'static', x = 0, y = 200})
local surface = belt:addBox(600, 16, {friction = 0.8, tangentSpeed = 120})
local box = world:createBody({x = -200, y = 170})
box:addBox(24, 24)

scene.push({
    update = function(self, dt)
        if input.keyPressed('r') then
            surface.tangentSpeed = -surface.tangentSpeed
        end
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

## Ropes and bridges

### physics2d.newRope(world, options)

Builds a chain of segments from `from` to `to` joined by revolute joints and returns a `Rope`. An end can hang from a body, or be pinned in place by a static anchor that the rope creates and destroys with itself. With `limitLength`, which is on by default, a slack distance joint between the two ends keeps the rope from stretching past its length, which a chain of revolute joints alone does under a load much heavier than its segments. Unknown keys raise `Unknown option "<key>".`, and a rope without segments, with ends in the same place or without thickness raises `A rope needs at least one segment, two distinct ends and a positive thickness.`. An invalid material or damping raises the errors of [Shape options](#shape-options) and [`world:createBody`](#worldcreatebodyoptions), and an end body that is destroyed or belongs to another world raises `A joint needs two bodies of this world.`. A rope that fails leaves none of its bodies in the world.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `from`, `to` | Vec2 | `{0, 0}` | Ends of the rope in world units. |
| `segments` | integer | `10` | Number of segments. |
| `thickness` | number | `4` | Width of each segment. |
| `density`, `friction` | number | `1`, `0.6` | Material of the segments. |
| `linearDamping`, `angularDamping` | number | `0`, `0.5` | Damping of the segments, which calms swinging. |
| `planks` | boolean | `false` | Makes the segments boxes instead of capsules. |
| `pinStart`, `pinEnd` | boolean | `false` | Pins an end without a body to a static anchor. |
| `limitLength` | boolean | `true` | Adds the slack joint that keeps the rope from stretching past its length. |
| `startBody`, `endBody` | Body | `nil` | Hangs an end from a body at the end point. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | Collision filter of the segments. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local lamp = world:createBody({x = 0, y = 150})
lamp:addCircle(12, {density = 20})
local rope = physics2d.newRope(world, {from = {0, 0}, to = {0, 150}, segments = 12, pinStart = true, endBody = lamp})
print(#rope:bodies(), #rope:joints()) -- 12 14
```

### physics2d.newBridge(world, options)

Builds a rope of planks with both ends pinned, unless the options give bodies for them, which makes a rope bridge. It takes the options of `newRope`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local bridge = physics2d.newBridge(world, {from = {-200, 100}, to = {200, 100}, segments = 16, thickness = 8})
print(#bridge:joints()) -- 18
```

### rope:bodies(), rope:joints(), rope:segments(), rope:points(), rope:destroy()

The methods `bodies` and `joints` return lists of the segment bodies and of the joints: those between segments, those that hang the ends and the length limit last. The method `segments` returns `{x, y, rotation, length}` for each segment, which places a sprite at its center. The method `points` returns the ends of the segments in order as `Vec2`, which draws the rope as a line. The method `destroy` removes the segments, their joints and the anchors of the rope.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local rope = physics2d.newRope(world, {from = {0, 0}, to = {200, 0}, segments = 10, pinStart = true})
local camera = graphics2d.newCamera()

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawPolyline(rope:points(), 3, '#FF8D6E63')
    end,
})
```

### Rope properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | boolean | read | The value is `false` once a segment was destroyed. |
| `segmentLength` | number | read | Length of each segment. |

## Ragdolls

### physics2d.newRagdoll(world, options)

Builds a human figure seen from the side, made of eleven capsules joined by revolute joints with the angle limits of real joints, and returns a `Ragdoll`. Its parts share a negative collision group, so they never collide with each other, and collide with the rest of the world through the category and mask of the options. Two figures with the same group pass through each other, so figures that should collide take a negative group each. Every joint resists bending with a motor that aims at no motion, whose torque comes from `stiffness`: at 1 a joint holds the limb below it straight out against standard gravity, 9.8 meters per second squared, whatever the height of the figure and the scale of the world, and at 0 the figure is limp. A height that is not positive, a group that is not negative or a stiffness outside 0 to 1 raise `A ragdoll needs a positive height, a negative collision group and a stiffness from 0 to 1.`, and an invalid material raises the error of [Shape options](#shape-options) without leaving any part in the world.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Center of the hips. |
| `height` | number | `128` | Height from the top of the head to the feet. |
| `density`, `friction` | number | `1`, `0.6` | Material of the parts. |
| `stiffness` | number | `0.2` | How much the joints resist bending, from 0 to 1. |
| `category`, `mask` | integer | `1`, all bits | Collision filter of the parts against the rest of the world. |
| `group` | integer | `-1` | Negative collision group of the parts. |
| `vx`, `vy` | number | `0` | Starting velocity of every part. |

```lua
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local ground = world:createBody({type = 'static', x = 0, y = 300})
ground:addBox(1000, 20)
local limp = physics2d.newRagdoll(world, {x = -100, y = 100, height = 120, stiffness = 0, vx = 200})
local stiff = physics2d.newRagdoll(world, {x = 100, y = 100, height = 120, stiffness = 0.6, group = -2})
stiff:body('head'):applyImpulse(-stiff.mass * 100, 0)

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

### ragdoll:body(part), ragdoll:bodies(), ragdoll:joints(), ragdoll:destroy()

The method `body` returns the body of a part: `head`, `chest`, `hips`, `upperArmLeft`, `lowerArmLeft`, `upperArmRight`, `lowerArmRight`, `upperLegLeft`, `lowerLegLeft`, `upperLegRight` or `lowerLegRight`, and other names raise a bad argument error with `unknown ragdoll part`. The method `bodies` returns a table of every body keyed by part name, `joints` the list of joints and `destroy` removes the figure.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local doll = physics2d.newRagdoll(world)
for name, body in pairs(doll:bodies()) do
    body.data = {part = name}
end
print(#doll:joints()) -- 10
```

### Ragdoll properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | boolean | read | The value is `false` once a part was destroyed. |
| `mass` | number | read | The mass of the parts that are left, in kilograms. |

## Vehicles

### physics2d.newVehicle(world, options)

Builds a car seen from the side and returns a `Vehicle`: a box chassis on two wheels held by wheel joints on springy suspension along the vertical axis of the chassis. The motors size their torque from the mass of the car and the acceleration the throttle asks for, so the car accelerates, brakes and coasts without lifting its nose or flipping. The center of mass sits at the height of the axles, the wheels spin as fast as they are driven, an anti-roll coupling between the axles resists pitching, and the throttle turns the car in the air. The parts share a negative collision group, so the wheels never hit the chassis. The front of the car is its positive x side. The options table is optional, and unknown keys raise `Unknown option "<key>".`. A chassis or wheel without size, a negative suspension travel, a group that is not negative, an acceleration, top speed or brake that is not positive, a negative air control or an anti-roll outside 0 to 1 raise `A vehicle needs a chassis and wheels with a size, a suspension travel of zero or more, a negative collision group, a positive acceleration, top speed and brake, air control of zero or more and an anti-roll from 0 to 1.`, and an invalid material raises the error of [Shape options](#shape-options) without leaving any part in the world.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Center of the chassis. |
| `chassisWidth`, `chassisHeight` | number | `120`, `30` | Size of the chassis. |
| `wheelRadius` | number | `16` | Radius of both wheels. |
| `rearWheel`, `frontWheel` | Vec2 | `{-40, 20}`, `{40, 20}` | Wheel positions relative to the chassis center. |
| `density`, `wheelDensity`, `wheelFriction` | number | `2`, `1`, `0.9` | Materials of the chassis and wheels. |
| `suspensionHertz`, `suspensionDamping` | number | `5`, `0.7` | Stiffness and damping of the springs. |
| `suspensionTravel` | number | `10` | How far the wheels move up and down from where they start. |
| `acceleration` | number | `600` | Acceleration of full throttle in units per second squared. |
| `topSpeed` | number | `900` | Speed of the rims of the driven wheels at full throttle, in units per second. |
| `brakeAcceleration` | number | `1600` | Deceleration of the full brake in units per second squared. |
| `centerOfMass` | Vec2 | The height of the axles | Center of mass of the chassis relative to its center. |
| `airControl` | number | `6` | Angular acceleration in radians per second squared that full throttle gives the chassis while no wheel touches the ground. |
| `antiRoll` | number | `0.5` | How much the springs of the two axles push against each other, from 0 to 1, which keeps the chassis level when it speeds up and slows down. |
| `drive` | string | `'rear'` | Driven wheels: `'rear'`, `'front'` or `'all'`. |
| `category`, `mask`, `group` | integer | `1`, all bits, `-2` | Collision filter of the parts, with a negative group. |
| `bullet` | boolean | `false` | Sweeps the chassis and the wheels against moving bodies too. |

### Vehicle members

| Member | Type | Access | Meaning |
| --- | --- | --- | --- |
| `throttle` | number | read and write | From -1, full reverse, to 1, full ahead, and 0 lets the car roll. Values outside are clamped. |
| `brake` | number | read and write | From 0 to 1, the share of the full brake that holds the wheels. Values outside are clamped. |
| `grounded` | boolean | read | Whether a wheel touched the ground in the last step. |
| `speed` | number | read | Speed of the chassis along its own axis in units per second, positive toward its front. |
| `chassis`, `rearWheel`, `frontWheel` | Body | read | The bodies of the car. |
| `drive` | string | read | The driven wheels. |
| `valid` | boolean | read | The value is `false` once a part was destroyed. |
| `vehicle:joints()` | function | | Returns the two wheel joints. |
| `vehicle:destroy()` | function | | Removes the car. |

Every member except `drive`, `valid` and `destroy` raises `The vehicle was destroyed.` once a part is gone.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local road = world:createBody({type = 'static'})
road:addChain({{-2000, 300}, {0, 300}, {400, 220}, {800, 300}, {4000, 300}}, false, {friction = 0.9})
local car = physics2d.newVehicle(world, {x = 0, y = 240, drive = 'all', acceleration = 800, topSpeed = 1200})
local camera = graphics2d.newCamera()

scene.push({
    fixedUpdate = function(self, step)
        local ahead = input.keyDown('right') and 1 or 0
        local back = input.keyDown('left') and 1 or 0
        car.throttle = ahead - back
        car.brake = input.keyDown('down') and 1 or 0
        world:step(step)
    end,
    render = function(self)
        camera.position = car.chassis.position
        graphics2d.beginWorld(camera)
        world:debugDraw()
        graphics2d.beginScreen()
        graphics2d.drawText(nil, string.format('Speed %.0f', car.speed), 40, 40, {size = 32})
    end,
})
```

### physics2d.newTopDownVehicle(world, options)

Builds a car seen from above and returns a `TopDownVehicle`: a box body with a front and a rear axle whose tires grip sideways up to a limit, so it steers, drifts when a turn asks for more grip than the tires have, skids on the handbrake and slows down by rolling drag. It faces its positive x axis at rotation 0 and suits a world without gravity. Steering turns the front tires at a limited speed, and the lock shrinks toward `highSpeedLock` of itself at the top speed, so the car stays drivable when it is fast. The options table is optional, and unknown keys raise `Unknown option "<key>".`. A car without size, an acceleration, speed, brake, grip or steering speed that is not positive, a negative steering lock or rolling drag, a handbrake grip or high speed lock outside 0 to 1 or a front axle that is not ahead of the rear one raise `A top-down vehicle needs a size, a front axle ahead of the rear one, positive accelerations, speeds, brake, grip and steering speed, a steering lock and rolling drag of zero or more, and a handbrake grip and high speed lock from 0 to 1.`, and an invalid material raises the error of [Shape options](#shape-options) without leaving the body in the world.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Center of the car. |
| `rotation` | number | `0` | Heading in radians. |
| `length`, `width` | number | `80`, `40` | Size of the body along and across the car. |
| `density`, `friction`, `restitution` | number | `1`, `0.3`, `0.2` | Material of the body. |
| `frontAxle`, `rearAxle` | number | `26`, `-26` | Distances of the axles along the car from its center, front positive. |
| `acceleration`, `reverseAcceleration` | number | `700`, `350` | Accelerations of full throttle ahead and in reverse, in units per second squared. |
| `topSpeed`, `reverseSpeed` | number | `900`, `300` | Fastest speeds ahead and in reverse, in units per second. |
| `brakeAcceleration` | number | `1500` | Deceleration of the full brake in units per second squared. |
| `grip` | number | `1400` | Sideways acceleration the tires hold before they slide, in units per second squared. |
| `handbrakeGrip` | number | `0.25` | Share of the rear grip that the handbrake keeps. |
| `steeringLock` | number | `0.6` | Largest angle of the front tires in radians. |
| `steeringSpeed` | number | `4` | How fast the front tires turn, in radians per second. |
| `highSpeedLock` | number | `0.4` | Share of the steering lock left at the top speed. |
| `rollingDrag` | number | `0.4` | Share of its speed the car loses every second without throttle. |
| `angularDamping` | number | `2` | Angular damping of the body. |
| `drive` | string | `'rear'` | Driven axles: `'rear'`, `'front'` or `'all'`. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | Collision filter of the body. |
| `bullet` | boolean | `false` | Sweeps the body against moving bodies too. |

### TopDownVehicle members

| Member | Type | Access | Meaning |
| --- | --- | --- | --- |
| `throttle` | number | read and write | From -1, full reverse, to 1, full ahead. Values outside are clamped, and a throttle wakes the car. |
| `steering` | number | read and write | From -1, full left as seen from the driver, to 1, full right. Values outside are clamped. |
| `brake` | number | read and write | From 0 to 1, the share of the full brake. Values outside are clamped. |
| `handbrake` | boolean | read and write | Whether the handbrake loosens the rear tires. |
| `steeringAngle` | number | read | The angle the front tires turn, in radians. |
| `speed` | number | read | Speed along the car in units per second, positive forward. |
| `slip` | number | read | Sideways speed of the rear axle in units per second, which grows while the car drifts. |
| `drifting` | boolean | read | Whether a tire slid in the last step because the turn asked for more grip than it has. |
| `body` | Body | read | The body of the car. |
| `valid` | boolean | read | The value is `false` once the body was destroyed. |
| `car:destroy()` | function | | Removes the car. |

Every member except `valid` and `destroy` raises `The vehicle was destroyed.` once the body is gone.

```lua
local physics2d = require('haylen.physics2d')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld({gravity = {0, 0}})
local car = physics2d.newTopDownVehicle(world, {x = 0, y = 0, rotation = -math.pi / 2, grip = 1200})
local camera = graphics2d.newCamera()

scene.push({
    fixedUpdate = function(self, step)
        car.throttle = (input.keyDown('up') and 1 or 0) - (input.keyDown('down') and 1 or 0)
        car.steering = (input.keyDown('right') and 1 or 0) - (input.keyDown('left') and 1 or 0)
        car.handbrake = input.keyDown('space')
        world:step(step)
    end,
    render = function(self)
        local body = car.body
        camera.position = body.position
        graphics2d.beginWorld(camera)
        graphics2d.draw(graphics.whiteTexture(), body.x, body.y, {width = 80, height = 40, rotation = body.rotation, color = car.drifting and '#FFFFB040' or '#FF4080FF'})
    end,
})
```

## Movers

### physics2d.newMover(world, options)

Creates a `Mover`, a kinematic character: an upright capsule that moves by the displacements the app asks for and slides along what it hits, without the solver pushing it around. It stands still on slopes up to `maxSlope`, keeps its speed along them, steps onto ledges up to `stepHeight`, stays on the ground over crests and down slopes within `snapDistance`, rides the bodies it stands on, stops at ceilings and walls, passes [one-way platforms](#one-way-platforms) from their closed side and lands on their open side like bodies do, and pushes the dynamic bodies it walks into. The capsule that collides covers the mover above its step height, and a ray under it finds the ground, so steps rise under it smoothly. A kinematic body of that capsule follows the mover, so other bodies collide with it too, and the mover passes through sensors. The options table is optional, and unknown keys raise `Unknown option "<key>".`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Center of the mover. |
| `radius` | number | `12` | Radius of the capsule. |
| `height` | number | `48` | Height from the feet to the top of the head. |
| `maxSlope` | number | `0.87` | Steepest ground in radians the mover stands on and walks up, about 50 degrees. |
| `stepHeight` | number | `0` | Highest ledge the mover steps onto. |
| `snapDistance` | number | `8` | How far below its feet the mover finds ground to stay on while it does not move up. |
| `pushForce` | number | `20000` | Most force in world units the mover pushes the dynamic bodies it walks into with, at the height of its middle. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | Collision filter of the mover and of its body. |

A radius of 0.01 meters or less, a negative step height or a height that leaves less than twice the radius above the step height raise `A mover needs a radius of more than 0.01 meters, a step height of zero or more and a height that leaves twice the radius above the step height.`, and a negative push force raises `A mover needs a push force of zero or more.`.

Move the mover once per fixed step, before `world:step()`: add gravity to its velocity, clip the velocity against what it hit, move by the velocity times the step and clip again, as the example does. Its body follows it by velocity over the step, so what the body touches is pushed instead of shoved apart.

### Mover members

| Member | Type | Access | Meaning |
| --- | --- | --- | --- |
| `mover:move(dx, dy)` | function | | Moves by the displacement in world units, sliding along what it hits, and returns how far it went as a `Vec2`. On walkable ground a downward part of the displacement, such as gravity, holds the mover on the ground instead of sliding it down, the sideways part follows the ground at its full length, and a part upward leaves the ground. The ground it stands on carries it by how far it moved since the last move. |
| `mover:clip(vx, vy)` | function | | Returns the velocity without what pushes into the ground, the walls and the ceilings of the last move, as a `Vec2`, which stops a fall on landing and a jump at a ceiling. The speed toward dynamic bodies stays, because the mover pushes them. |
| `mover:dropThrough(seconds)` | function | | Lets the mover pass every one-way platform for `seconds`, 0.2 by default, and a platform it is still inside after that until it leaves it, so it drops through the platform it stands on. A negative or infinite time raises `A physics body drops through one-way platforms for a finite time of zero or more.`. |
| `mover:destroy()` | function | | Destroys the body of the mover, after which the mover stops moving. |
| `position` | Vec2 | read and write | The center of the mover. Writing accepts a `Vec2` or `{x, y}` and teleports the mover and its body. |
| `x`, `y` | number | read | The center of the mover. |
| `grounded` | boolean | read | Whether the mover stands on walkable ground. |
| `groundNormal` | Vec2 | read | The normal of the ground under the mover, `{0, -1}` in the air. |
| `groundBody` | Body or nil | read | The body the mover stands on. |
| `groundVelocity` | Vec2 | read | The velocity of the ground under the feet, which a jump from a moving platform adds. |
| `onWall`, `onCeiling` | boolean | read | Whether the mover touches a wall or a ceiling. |
| `body` | Body | read | The kinematic body that follows the mover. |
| `radius`, `height`, `stepHeight` | number | read | The size of the mover. |
| `maxSlope` | number | read and write | The steepest walkable ground in radians. |
| `snapDistance` | number | read and write | How far below its feet the mover finds ground to stay on. |
| `valid` | boolean | read | The value is `false` once the mover was destroyed. |

A slope limit that is negative or not below half pi radians raises `A mover needs a slope limit of zero or more and below half pi radians.`, a snap distance that is negative or not finite raises `A mover needs a finite snap distance of zero or more.`, and every member except `valid` and `destroy` raises `The mover was destroyed.` once the mover is destroyed.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local ground = world:createBody({type = 'static'})
ground:addChain({{-800, 300}, {0, 300}, {300, 150}, {800, 150}}, false)
local ledge = world:createBody({type = 'static', x = -300, y = 292})
ledge:addBox(100, 16)
local shelf = world:createBody({type = 'static', x = -500, y = 180})
shelf:addBox(160, 8, {oneWay = {0, -1}})
local hero = physics2d.newMover(world, {x = -600, y = 260, radius = 14, height = 56, stepHeight = 20, maxSlope = math.rad(50)})
local camera = graphics2d.newCamera()

scene.push({
    enter = function(self)
        self.velocity = {x = 0, y = 0}
        self.jump = false
    end,
    update = function(self, dt)
        if input.keyPressed('space') then
            self.jump = true
        elseif input.keyPressed('down') then
            hero:dropThrough()
        end
    end,
    fixedUpdate = function(self, step)
        local direction = (input.keyDown('right') and 1 or 0) - (input.keyDown('left') and 1 or 0)
        local vy = self.velocity.y + 980 * step
        if self.jump and hero.grounded then
            vy = -560 + hero.groundVelocity.y
        end
        self.jump = false
        self.velocity = hero:clip(direction * 260, vy)
        hero:move(self.velocity.x * step, self.velocity.y * step)
        self.velocity = hero:clip(self.velocity.x, self.velocity.y)
        world:step(step)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawRect({hero.x - 14, hero.y - 28, 28, 56}, hero.grounded and '#FF60A0FF' or '#FFFFA060')
        world:debugDraw()
    end,
})
```

## Grabbers

### physics2d.newGrabber(world, options)

Creates a `Grabber`, which drags dynamic bodies with a mouse, a finger or a cursor. It takes the body nearest to a point within its pick radius, holds it by the point it took, and pulls that point toward a target with a soft spring whose force grows with everything joined to the body, so one hand of a ragdoll drags the whole figure and a heavy chain follows as well as a pebble. A point outside the shapes takes the body by the nearest point of its shape, so the pull never jumps. Sensors and static and kinematic bodies are never taken. The grabber owns a static anchor body of its world. The options table is optional, and unknown keys raise `Unknown option "<key>".`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `pickRadius` | number | `0` | Distance in world units around the point within which bodies are taken. |
| `strength` | number | `30` | Pull as a multiple of the weight under standard gravity, 9.8 meters per second squared, of the held body and every dynamic body joined to it, directly or through others. |
| `hertz`, `dampingRatio` | number | `5`, `0.7` | Stiffness and damping of the spring that pulls. |
| `category`, `mask` | integer | `1`, all bits | The shapes the grabber takes, as in [Query filters](#query-filters). |

A pick radius that is negative or not finite raises `A grabber needs a finite pick radius of zero or more.`, and a strength, stiffness or damping that is not positive raises `A grabber needs a positive strength, stiffness and damping ratio.`, at creation and when the properties change.

### Grabber members

| Member | Type | Access | Meaning |
| --- | --- | --- | --- |
| `grabber:grab(x, y)` | function | | Takes the dynamic body nearest to the world point, releasing the one it held, and returns it, or `nil` when no dynamic body is within the pick radius. |
| `grabber:moveTo(x, y)` | function | | Moves the target the held point is pulled toward. |
| `grabber:release()` | function | | Lets the held body go. |
| `holding` | boolean | read | Whether the grabber holds a body. |
| `body` | Body or nil | read | The body it holds. |
| `target` | Vec2 | read | The target of the pull. |
| `handle` | Vec2 | read | Where the held point of the body is, which draws the line of the drag, and the target while nothing is held. |
| `force` | number | read | The most force of the pull in world units, which `grab` sized for the bodies joined to the held one, and 0 while nothing is held. |
| `pickRadius`, `strength`, `hertz`, `dampingRatio` | number | read and write | The options, which also change the pull of a body already held. |

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local ground = world:createBody({type = 'static', x = 0, y = 300})
ground:addBox(1600, 40)
local crate = world:createBody({x = -200, y = 200})
crate:addBox(48, 48)
local doll = physics2d.newRagdoll(world, {x = 200, y = 200})
local hand = physics2d.newGrabber(world, {pickRadius = 24})
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        local x, y = camera:screenToWorld(input.mousePosition())
        if input.mousePressed() then
            hand:grab(x, y)
        elseif input.mouseReleased() then
            hand:release()
        end
        hand:moveTo(x, y)
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        world:debugDraw()
        if hand.holding then
            graphics2d.drawLine(hand.handle.x, hand.handle.y, hand.target.x, hand.target.y, 2, '#FFFFE080')
        end
    end,
})
```

## Force fields

### physics2d.newForceField(world, options)

Creates a `ForceField`, an area of the world that pushes the dynamic bodies inside it every step, in C++ without a call per body, before Box2D advances the bodies. Radial fields pull toward their center, like magnets, attractors and the gravity of planets, or push away with a negative strength. Directional fields push one way, like wind and jets of air. Vortex fields swirl clockwise on screen with a positive strength. Buoyancy fields are water: they float each shape by the area it has under the surface, at the center of that area, with the weight of the water it displaces, and drag it toward the flow, so bodies lighter than the water float and heavier ones sink. Radial, directional and vortex fields push a body whose center of mass lies in the area. Applying a field never wakes a body, so floating and resting bodies fall asleep, and changing the field wakes the bodies in it. The options table is required, and unknown keys raise `Unknown option "<key>".`.

The area is a circle of `radius` around the position when the radius is positive, a rectangle of `width` by `height` centered on the position when both are positive, and otherwise the polygon of `points` around the position. A buoyancy field needs a rectangle or a convex polygon, whose top is the surface of the water.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | `'radial'` | One of `'radial'`, `'directional'`, `'vortex'` or `'buoyancy'`. |
| `x`, `y` | number | `0` | Position of the field. |
| `radius` | number | `0` | Radius of a circular area. |
| `width`, `height` | number | `0` | Size of a rectangular area. |
| `points` | list of Vec2 | None | Corners of a polygonal area, relative to the position. |
| `strength` | number | `0` | Acceleration in units per second squared, the same for every body, or a force in world units when `acceleration` is `false`. |
| `direction` | Vec2 | `{1, 0}` | Direction of a directional field. |
| `falloff` | string | `'none'` | How the strength fades with the distance from the position: `'none'`, `'linear'` to zero at the radius, or at half the larger side of other areas, or `'inverseSquare'` with the square of `minDistance` over the squared distance, like gravity. |
| `minDistance` | number | `0` | Distance within which an inverse square field keeps its full strength, such as the surface of a planet. |
| `acceleration` | boolean | `true` | Treats `strength` as an acceleration. A field of forces pushes light bodies further than heavy ones. |
| `density` | number | `1` | Density of the water of a buoyancy field in kilograms per square meter. |
| `linearDrag` | number | `0` | Share of their speed relative to the flow that bodies lose every second. |
| `angularDrag` | number | `0` | Share of their spin that bodies lose every second. |
| `flow` | Vec2 | `{0, 0}` | Velocity the drag pulls bodies toward, such as a current or the wind of a drag field. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | The bodies the field pushes, chosen like the shapes that collide with a shape of this filter, as [Collision filtering](#collision-filtering) explains: a shared positive group always counts, a shared negative group never does, and otherwise each category must be in the mask of the other. |
| `enabled` | boolean | `true` | Whether the field pushes. |

An area without size, a polygon of fewer than three points, a buoyancy area that is not convex, a zero direction, a negative drag, density or minimum distance, or an inverse square falloff without a minimum distance raise `A force field needs a circle, a rectangle or a polygon of at least three points, convex for buoyancy, a direction that is not zero, drags, a density and a minimum distance of zero or more, and a minimum distance for an inverse square falloff.`. An unknown kind or falloff raises a bad argument error with `unknown value '<name>'`.

### ForceField members

| Member | Type | Access | Meaning |
| --- | --- | --- | --- |
| `kind` | string | read | The kind of the field. |
| `enabled` | boolean | read and write | Whether the field pushes. |
| `strength` | number | read and write | The strength of the field. |
| `position` | Vec2 | read and write | The position of the field, which moves its area. |
| `direction` | Vec2 | read and write | The direction of a directional field. |
| `flow` | Vec2 | read and write | The velocity the drag pulls toward. |
| `density` | number | read and write | The density of the water. |
| `linearDrag`, `angularDrag` | number | read and write | The drags of the field. |
| `bounds` | Rect | read | The bounds of the area. |
| `bodyCount` | integer | read | The bodies the field pushed in the last step. |
| `valid` | boolean | read | The value is `false` once the field was destroyed. |
| `field:destroy()` | function | | Stops the field for good. |

A zero direction raises `A force field needs a direction that is not zero.`, a negative density `A force field needs a density of zero or more.` and a negative drag `A force field needs drags of zero or more.`. Every member except `valid` and `destroy` raises `The force field was destroyed.` once the field is destroyed.

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local kMetal = 2
local world = physics2d.newWorld()
local floor = world:createBody({type = 'static', x = 0, y = 520})
floor:addBox(2400, 40)

local pool = physics2d.newForceField(world, {kind = 'buoyancy', x = 400, y = 350, width = 600, height = 300, density = 1.2, linearDrag = 1, angularDrag = 1, flow = {40, 0}})
local fan = physics2d.newForceField(world, {kind = 'directional', x = -400, y = 200, width = 200, height = 600, strength = 1500, direction = {0, -1}})
local magnet = physics2d.newForceField(world, {kind = 'radial', x = -800, y = 0, radius = 300, strength = 2000, falloff = 'linear', mask = kMetal})

for index = 1, 6 do
    local crate = world:createBody({x = -900 + index * 220, y = -200})
    crate:addBox(40, 40, {density = index % 2 == 0 and 0.5 or 2, category = index % 3 == 0 and kMetal or 1})
end

scene.push({
    update = function(self, dt)
        if input.keyPressed('f') then
            fan.enabled = not fan.enabled
        end
        if input.keyPressed('m') then
            magnet.strength = -magnet.strength
        end
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

A planet is a radial field with an inverse square falloff whose minimum distance is its radius, in a world without gravity. A body launched at the speed `math.sqrt(strength * minDistance ^ 2 / distance)` orbits it in a circle.

```lua
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld({gravity = {0, 0}, subSteps = 8})
local surface, gravity = 100, 800
local planet = physics2d.newForceField(world, {kind = 'radial', radius = 2000, strength = gravity, falloff = 'inverseSquare', minDistance = surface})
local ground = world:createBody({type = 'static'})
ground:addCircle(surface)

local orbit = 300
local moon = world:createBody({x = orbit, y = 0, vy = math.sqrt(gravity * surface * surface / orbit)})
moon:addCircle(8)

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

## Explosions

### physics2d.explode(world, options)

Pushes the dynamic bodies around a point away from it and returns a list of `{body, x, y, impulseX, impulseY}` with the point each body was pushed at and the impulse it took. Each body takes the impulse, scaled by the falloff over its distance, at the point of its shapes closest to the center, so bodies hit off center spin. A radius that is not positive raises `An explosion needs a positive radius.`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Center of the blast. |
| `radius` | number | `100` | Reach of the blast. |
| `impulse` | number | `500` | Impulse at the center. |
| `falloff` | string | `'linear'` | Fading toward the radius: `'none'`, `'linear'` or `'quadratic'`. |
| `occlusion` | boolean | `false` | Spares bodies behind other shapes as seen from the center. Sensors and shapes the filter skips never shield a body. |
| `category`, `mask` | integer | `1`, all bits | Filter of the bodies the blast reaches and of the shapes that shield them. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local barrel = world:createBody({x = 40, y = 0})
barrel:addCircle(16)
local hits = physics2d.explode(world, {x = 0, y = 10, radius = 200, impulse = 800, occlusion = true})
for _, hit in ipairs(hits) do
    print(hit.body == barrel, hit.impulseX, hit.impulseY)
end
```

## Destructible terrain

### physics2d.newTerrain(world, options)

Creates a `Terrain`: destructible ground stored as a grid of samples from 0 (empty) to 1 (solid) that bitmaps and polygons fill and that shapes and explosions carve. The solid areas become chain loops on one static body per chunk, traced with marching squares, and `terrain:update()` rebuilds only the chunks that changed. Sample `(column, row)` lies at `{x, y} + {column, row} * cellSize`, so the terrain covers `(columns - 1) * cellSize` by `(rows - 1) * cellSize`. The terrain owns its bodies and starts empty. Fewer than 2 by 2 samples, a cell or chunk size that is not positive or a negative tolerance raise `A terrain needs at least 2 by 2 samples, a positive cell size and chunk size, and a tolerance of at least zero.`, and an invalid material raises the error of [Shape options](#shape-options).

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `columns`, `rows` | integer | `257`, `129` | Number of samples on each side. |
| `cellSize` | number | `4` | Distance between samples. |
| `x`, `y` | number | `0` | Position of the first sample. |
| `chunkSize` | integer | `32` | Cells per chunk side. Smaller chunks rebuild faster after a small carve. |
| `simplifyTolerance` | number | `1` | How far outlines may stray from the traced edge when simplified, in world units. |
| `friction`, `restitution` | number | `0.6`, `0` | Material of the ground. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | Collision filter of the ground. |

```lua
local physics2d = require('haylen.physics2d')
local m = require('haylen.math')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 201, rows = 101, cellSize = 4})
local noise = m.noise(7)
terrain.samples = function(column, row)
    local surface = 200 + noise:fractal(column / 40, 0) * 60
    return row * 4 > surface and 1 or 0
end
print(terrain:update(), #terrain:bodies())
```

### terrain.samples, terrain:sample(column, row)

Reading `samples` returns a new list of every sample from 0 to 1 stored row by row, which can shade the ground. Writing it replaces every sample with such a list, like the alpha channel of an image, or with the results of a function of the column and row. A list of another length raises `The terrain needs one value per sample.`. The method `sample` returns one value, and a sample outside the grid raises `The sample is outside the terrain.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 3, rows = 2})
terrain.samples = {0, 0.5, 1, 1, 1, 1}
print(terrain:sample(1, 0), #terrain.samples) -- 0.50196081399918 6
```

### terrain:fill(x, y, radius), terrain:carve(x, y, radius), terrain:fillPolygon(points), terrain:carvePolygon(points)

Adds or removes material inside a circle or a polygon with soft edges, so outlines follow the shape between samples. The chunks the change reaches wait for the next `update`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 129, rows = 65, cellSize = 8})
terrain:fillPolygon({{0, 256}, {1024, 256}, {1024, 512}, {0, 512}})
terrain:carve(300, 256, 40)
terrain:fill(700, 240, 30)
terrain:carvePolygon({{500, 250}, {560, 250}, {560, 330}, {500, 330}})
terrain:update()
```

### terrain:explode(x, y, radius, blast)

Carves a crater and applies a blast with the options of `physics2d.explode`, whose center defaults to the crater center and whose radius defaults to twice the crater radius, and returns the hits.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 129, rows = 65, cellSize = 8})
terrain:fillPolygon({{0, 256}, {1024, 256}, {1024, 512}, {0, 512}})
terrain:update()
local hits = terrain:explode(400, 256, 48, {impulse = 900})
terrain:update()
print(#hits)
```

### terrain:update(), terrain:isSolid(point), terrain:outlines(), terrain:bodies()

The method `update` rebuilds the collision of the chunks changed since the last update and returns how many it rebuilt. The method `isSolid` tells whether a point lies inside the ground. The method `outlines` returns the collision outlines of every chunk as lists of `Vec2`, which also draw the ground, and `bodies` the bodies of the chunks that hold ground.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 65, rows = 33, cellSize = 8})
terrain:fill(256, 256, 120)
terrain:update()
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        for _, outline in ipairs(terrain:outlines()) do
            graphics2d.drawPolygon(outline, '#FF795548')
        end
    end,
})
print(terrain:isSolid({256, 256}))
```

### Terrain properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `columns`, `rows` | integer | read | Number of samples on each side. |
| `cellSize` | number | read | Distance between samples. |
| `samples` | list of numbers | read and write | Every sample, described in [`terrain.samples`](#terrainsamples-terrainsamplecolumn-row). |
| `bounds` | Rect | read | Area the terrain covers. |
| `chunkCount` | integer | read | Number of chunks. |
| `dirtyChunkCount` | integer | read | Chunks waiting for `update`. |

## Fracture

### physics2d.fracture(body, options)

Replaces a body with one body per piece of its polygon shapes and returns the pieces, which keep its type, material, filter and motion. The pieces are the Voronoi cells of random points inside the body, which gather around the impact point when there is one. A body without polygon shapes stays as it is and returns an empty list. A piece thinner than the tolerance of Box2D, as in a sliver less than about 0.02 meters thick, raises the error of `body:addPolygon()` and leaves the body whole.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `pieces` | integer | `8` | Number of cells. A value below 1 raises `A fracture needs at least one piece.` |
| `impact` | Vec2 | `nil` | Point in world units where the pieces get smaller. |
| `seed` | integer | `0` | Seed of the random points. |
| `minimumArea` | number | `4` | Pieces smaller than this are dropped. |

```lua
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local ground = world:createBody({type = 'static', x = 0, y = 300})
ground:addBox(1000, 40)
local crate = world:createBody({x = 100, y = -400})
crate:addBox(64, 64)
world.onHit = function(a, b, contact)
    if (a == crate or b == crate) and crate.valid and contact.speed > 400 then
        physics2d.fracture(crate, {pieces = 10, impact = {contact.x, contact.y}})
    end
end

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

### physics2d.splitPolygon(shape, options)

Splits a shape, given like the shapes of `m.polygon`, into pieces with the options of `fracture` and returns them, each a list of outlines. The pieces together cover the shape, which suits breaking sprites without physics.

```lua
local physics2d = require('haylen.physics2d')

local pieces = physics2d.splitPolygon({{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {pieces = 6, seed = 3})
print(#pieces)
```

## Fluids

### physics2d.newFluid(world, options)

Creates a `Fluid`: a liquid of particles that hold together and spread apart with the double density relaxation of particle fluids, collide with the shapes of the world and push the dynamic bodies they hit, so crates float on it or sink by their density. The particles are not bodies. The fluid keeps them in flat arrays and moves them inside every `world:step()`, after Box2D advances the bodies, with its passes spread over the job system. They collide with the solid shapes that pass their filter, and with chains from the side the chains collide on, while sensors, ray casts and queries never see them. A push that changes the speed of a sleeping body by less than 0.05 meters per second leaves it asleep, so crates resting in still water fall asleep. The options table is optional, and unknown keys raise `Unknown option "<key>".`. A radius that is not positive, a smoothing radius not larger than it, or a negative density, friction, restitution, rest density, stiffness, viscosity or top speed raise `A fluid needs a positive particle radius, a larger smoothing radius, and a density, friction, restitution, rest density, stiffness, viscosity and top speed of zero or more.`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `radius` | number | `4` | Collision radius of the particles. |
| `smoothingRadius` | number | `16` | Distance within which particles interact, which also sets the spacing of `fluid:fill` and the size of the metaballs `fluid:draw` draws. |
| `density` | number | `1` | Kilograms per square meter of the area of a particle, which gives the particles the mass they push bodies with. |
| `friction` | number | `0.1` | How much particles that slide along shapes slow down, from 0 to 1. |
| `restitution` | number | `0` | How much particles bounce off shapes. |
| `restDensity` | number | `1.8` | Density the pressure pulls toward, where larger values pack the liquid tighter. |
| `stiffness`, `nearStiffness` | number | `0.008`, `0.02` | Strength of the pressure and of the near pressure that keeps particles from clumping, tuned for 60 steps per second and scaled with the step. |
| `viscosity` | number | `0.15` | How much particles moving toward each other slow down, which evens out the speeds of neighbors. |
| `gravityScale` | number | `1` | Multiplies the world gravity for the particles. |
| `maxSpeed` | number | `3000` | Speed limit of the particles in units per second. |
| `maxParticles` | integer | `8192` | Most particles the fluid holds. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | The shapes the particles collide with, chosen like the shapes that collide with a shape of this filter, as [Collision filtering](#collision-filtering) explains. |

```lua
local physics2d = require('haylen.physics2d')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local tank = world:createBody({type = 'static'})
-- Down the left wall, along the floor and up the right wall, so the walls hold the water inside.
tank:addChain({{-300, -400}, {-300, 200}, {300, 200}, {300, -400}}, false)
local water = physics2d.newFluid(world, {radius = 4, smoothingRadius = 16, maxParticles = 3000})
water:fill({-280, -200, 560, 200})
local crate = world:createBody({x = 0, y = -350})
crate:addBox(60, 40, {density = 0.5})
local camera = graphics2d.newCamera()

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        water:draw({color = '#FF4FA3F7', outlineColor = '#FFB3E5FC', outlineWidth = 0.1, layer = 1})
        graphics2d.draw(graphics.whiteTexture(), crate.x, crate.y, {width = 60, height = 40, rotation = crate.rotation, color = '#FF8D6E63', layer = 2})
    end,
})
```

### fluid:spawn(x, y, vx, vy), fluid:fill(rect, vx, vy), fluid:remove(index), fluid:clear()

The method `spawn` adds a particle with an optional velocity and returns `false` when the fluid is full. The method `fill` fills a rectangle, a `Rect` or `{x, y, width, height}`, with particles half a smoothing radius apart, nudged off a perfect grid so the pressure spreads them, with an optional velocity, and returns how many it added before the fluid was full. The method `remove` removes the particle at a position from 1, moving the last particle into its place, and `clear` removes every particle. A position below 1 raises a bad argument error with `particles count from 1`, and one past the last particle raises `The fluid has no such particle.`.

```lua
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local basin = world:createBody({type = 'static'})
basin:addChain({{-200, -100}, {-200, 200}, {200, 200}, {200, -100}}, false)
local water = physics2d.newFluid(world, {maxParticles = 2000})

scene.push({
    fixedUpdate = function(self, step)
        water:spawn(math.random(-20, 20), -300, 0, 150)
        world:step(step)
    end,
})
```

### fluid:positions(buffer, first), fluid:velocities(buffer, first)

The methods write two numbers per particle, x then y, and return what they wrote into. A float buffer of [`haylen.collections`](collections.md#float-buffers) receives them from the position `first`, 1 by default, and must hold two values for each particle from there, or the call raises a bad argument error with `the buffer needs two values for each particle`. A list receives them from 1 and loses the entries after them, and without an argument the methods return a new list. Passing the same buffer or list every frame reads the fluid without allocating.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local water = physics2d.newFluid(world)
water:fill({0, 0, 80, 80})
local positions = {}
local camera = graphics2d.newCamera()

scene.push({
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        water:positions(positions)
        for index = 1, #positions, 2 do
            graphics2d.drawCircle(positions[index], positions[index + 1], water.radius, '#804FC3F7')
        end
    end,
})
```

### fluid:draw(options)

Draws the particles as metaballs on the current canvas straight from the arrays of the fluid, without a table per particle, the way [`graphics2d.drawMetaballs`](graphics2d.md#graphics2ddrawmetaballspoints-radius-options) draws them. The optional table takes `radius`, the radius of each ball, which defaults to 0.6 smoothing radii, the `color`, `outlineColor`, `outlineWidth` and `threshold` of `drawMetaballs` and the [draw order](graphics2d.md#draw-order) keys apart from `material`. Unknown keys raise `Unknown option "<key>".`, and the values raise the errors of `drawMetaballs`. An empty fluid draws nothing. The example of [`physics2d.newFluid`](#physics2dnewfluidworld-options) draws a fluid this way.

### Fluid properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | Number of particles. |
| `radius` | number | read | Radius of the particles. |
| `stepMilliseconds` | number | read | Time the fluid took in the last step, in milliseconds. The [physics guide](../physics.md#performance) lists what fluids of several sizes cost. |

## Tiled collision

The method [`map:buildCollision(world)`](tiled.md#mapbuildcollisionworld) of `haylen.tiled` creates static bodies of `world` for the collision of a Tiled map and returns them as a list of `haylen.Body` handles, in map order: one for each tile layer whose `collision` property is not `false` and one for each object layer with collision objects. On orthogonal maps the solid closed shapes of touching tiles merge into chain loops around each solid region, so bodies slide along floors, walls and slopes without catching on the joints between tiles, and `map:setTile` traces the regions it touches again. Objects and tile shapes whose `sensor` property is `true` become sensors, and those whose `oneWay` property is `true` become [one-way platforms](#one-way-platforms) that only block bodies from above. Chain loops have no inside, so `world:queryPoint()` finds nothing inside a merged region, and rays hit its outline from outside.

```lua
local physics2d = require('haylen.physics2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local world = physics2d.newWorld()
local map = tiled.newMapRenderer(assets.load('maps/level.tmj'))
local walls = map:buildCollision(world)
local camera = graphics2d.newCamera()
print(#walls, walls[1].type)

scene.push({
    update = function(self, dt)
        if input.mousePressed() then
            -- Digging a tile out of the ground traces the collision around it again.
            local column, row = map:worldToCell(camera:screenToWorld(input.mousePosition()))
            map:setTile('ground', column, row, 0)
        end
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        map:draw(camera)
        world:debugDraw()
    end,
})
```

## Errors

Invalid input raises Lua errors with these messages.

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | An options or filter table has a key the call does not accept. |
| `unknown value '<name>'` | A body type, joint type, drive, force field kind or falloff is not one of the names listed in this page. It comes inside a bad argument error. |
| `The type "haylen.Body" has no member "<name>".` | A member of a body, shape, joint, world or helper does not exist. The type name changes with the userdata. |
| `The type "haylen.Body" has no writable property "<name>".` | A read-only property was assigned. The type name changes with the userdata. |
| `A physics world needs positive pixels per meter, at least one sub-step and at least one thread.` | The function `physics2d.newWorld()` received a bad scale, sub-step count or thread count. |
| `Too many physics worlds exist at once to create another one.` | The function `physics2d.newWorld()` was called while Box2D holds as many worlds as it can. |
| `A physics world needs at least one sub-step.` | The property `world.subSteps` received fewer than one sub-step. |
| `A physics speed needs to be finite and zero or more.` | A speed of the world or the sleep threshold of a body is negative or not finite. |
| `A physics world needs a positive contact stiffness.` | The property `world.contactHertz` received a value that is not positive. |
| `A physics world needs a positive contact damping ratio.` | The property `world.contactDampingRatio` received a value that is not positive. |
| `Body transforms need live bodies of this world.` | The methods `world:readTransforms()` or `world:writeTransforms()` received a destroyed body or one of another world. |
| `Body transforms take three floats for each body.` | The buffer of `world:readTransforms()` or `world:writeTransforms()` is too short. |
| `the first position is outside the buffer` | The position `first` of `world:readTransforms()` or `world:writeTransforms()` lies outside the buffer. It comes inside a bad argument error. |
| `Only a world created with interpolation blends the transforms of its bodies.` | The method `world:readTransforms()` asked for interpolated transforms of a world without `interpolate`. |
| `A state hash needs live bodies of this world.` | The method `world:stateHash()` received a destroyed body or one of another world. |
| `A path prediction needs a positive step, at least one step, and a radius and damping of zero or more.` | The method `world:predictPath()` received bad options. |
| `A rectangle query needs a width and height of zero or more.` | The method `world:queryRect()` received a negative width or height. |
| `the radius must be zero or more` | The filter of `world:pick()` has a negative radius. It comes inside a bad argument error. |
| `A circle cast needs a finite radius of zero or more.` | The method `world:castCircle()` received a negative radius. |
| `A box cast needs a positive size.` | The method `world:castBox()` received a zero or negative size. |
| `A capsule cast needs a positive radius.` | The method `world:castCapsule()` received a zero or negative radius. |
| `A polygon cast needs between three and eight points.` | The method `world:castPolygon()` received too few or too many points. |
| `A bouncing physics ray needs a finite length.` | The method `world:bounceRay()` received an infinite length. |
| `the direction must not be zero` | The method `world:bounceRay()` received a zero direction. It comes inside a bad argument error. |
| `the bounce count must not be negative` | The method `world:bounceRay()` received a negative bounce count. It comes inside a bad argument error. |
| `the length must be finite` | The method `world:rayFan()` received an infinite length. It comes inside a bad argument error. |
| `Ray batches take no "accept" function, because their rays run on worker threads.` | The method `world:raycastBatch()` received a filter with `accept`. |
| `the ray index is outside the batch` | A ray batch method received an index outside the batch. It comes inside a bad argument error. |
| `The body was destroyed.` | A destroyed body was used. |
| `A physics body needs a finite damping of zero or more.` | A body option or property received a negative or infinite damping. |
| `A physics body needs a finite mass, center of mass and inertia of zero or more.` | The property `body.mass`, `body.inertia` or `body.centerOfMass` received a negative or infinite value. |
| `A physics body moves to a target over a positive time.` | The method `body:moveTo()` received a time that is not positive. |
| `A physics body drops through one-way platforms for a finite time of zero or more.` | The method `body:dropThrough()` or `mover:dropThrough()` received a negative or infinite time. |
| `A physics shape needs a finite density, friction, restitution and rolling resistance of zero or more.` | Shape options or properties, or the material options of a rope, ragdoll, vehicle or terrain, are negative or not finite. |
| `A one-way direction cannot be zero.` | The property `shape.oneWay` or the `oneWay` option received a zero vector. |
| `A physics box needs a positive size.` | The method `body:addBox()` received a zero or negative size. |
| `A physics circle needs a positive radius.` | The method `body:addCircle()` received a zero or negative radius. |
| `A physics capsule needs a positive radius.` | The method `body:addCapsule()` received a zero or negative radius. |
| `A physics segment needs ends more than 0.005 meters apart.` | The method `body:addSegment()` received ends in about the same place. |
| `A physics polygon needs at least three points.` | The method `body:addPolygon()` received fewer than three points. |
| `A physics polygon needs at least three points that are not on one line.` | The method `body:addPolygon()` received a convex outline without area. |
| `A physics polygon needs a non-degenerate outline.` | The method `body:addPolygon()` received an outline whose pieces have no area. |
| `A physics chain needs at least four points for a loop and two for an open chain.` | The method `body:addChain()` received too few points. |
| `The shape was destroyed.` | A destroyed shape was used. |
| `Only sensor shapes have overlaps.` | The method `shape:overlaps()` was called on a shape that is not a sensor. |
| `A joint needs two bodies of this world.` | A joint body is destroyed or belongs to another world. |
| `A distance joint needs a length of at least 0.005 meters.` | A distance joint has no `length` and anchors in about the same place, or a `length` that is too short. |
| `A revolute joint needs a lower limit that is not above the upper one, both within 0.99 pi radians of zero.` | A revolute joint received limits in the wrong order or beyond 0.99 pi radians. |
| `A prismatic, wheel or distance joint needs a lower limit that is not above the upper one.` | The method `world:createJoint()` received limits in the wrong order. |
| `A joint needs a lower limit that is not above the upper one.` | The property `joint.lower` or `joint.upper` received a limit past the other one. |
| `A joint needs a break force and a break torque of zero or more.` | A break limit is negative. |
| `A joint needs a positive constraint stiffness.` | The property `joint.constraintHertz` received a value that is not positive. |
| `A joint needs a finite constraint damping ratio of zero or more.` | The property `joint.constraintDampingRatio` received a negative or infinite value. |
| `A joint needs a finite maximum motor force of zero or more.` | The property `joint.maxMotorForce` received a negative or infinite value. |
| `A joint needs a finite maximum motor torque of zero or more.` | The property `joint.maxMotorTorque` received a negative or infinite value. |
| `A joint spring needs a finite stiffness and damping ratio of zero or more.` | The property `joint.hertz` or `joint.dampingRatio` received a negative or infinite value. |
| `Only mouse joints have a target.` and the other messages that start with `Only` | A joint property was used on a joint type that lacks it, as [Joint properties](#joint-properties) lists. |
| `The physics joint was destroyed.` | A destroyed joint was used. |
| `A rope needs at least one segment, two distinct ends and a positive thickness.` | The function `physics2d.newRope()` or `physics2d.newBridge()` received bad options. |
| `A ragdoll needs a positive height, a negative collision group and a stiffness from 0 to 1.` | The function `physics2d.newRagdoll()` received bad options. |
| `unknown ragdoll part` | The method `ragdoll:body()` received an unknown part name. It comes inside a bad argument error. |
| `A vehicle needs a chassis and wheels with a size, a suspension travel of zero or more, a negative collision group, a positive acceleration, top speed and brake, air control of zero or more and an anti-roll from 0 to 1.` | The function `physics2d.newVehicle()` received bad options. |
| `A top-down vehicle needs a size, a front axle ahead of the rear one, positive accelerations, speeds, brake, grip and steering speed, a steering lock and rolling drag of zero or more, and a handbrake grip and high speed lock from 0 to 1.` | The function `physics2d.newTopDownVehicle()` received bad options. |
| `The vehicle was destroyed.` | A vehicle whose bodies are gone was used. |
| `A mover needs a radius of more than 0.01 meters, a step height of zero or more and a height that leaves twice the radius above the step height.` | The function `physics2d.newMover()` received a bad size. |
| `A mover needs a push force of zero or more.` | The function `physics2d.newMover()` received a negative push force. |
| `A mover needs a slope limit of zero or more and below half pi radians.` | The function `physics2d.newMover()` or the property `mover.maxSlope` received a bad slope limit. |
| `A mover needs a finite snap distance of zero or more.` | The function `physics2d.newMover()` or the property `mover.snapDistance` received a bad snap distance. |
| `The mover was destroyed.` | A destroyed mover was used. |
| `A grabber needs a finite pick radius of zero or more.` | The function `physics2d.newGrabber()` or the property `grabber.pickRadius` received a negative or infinite radius. |
| `A grabber needs a positive strength, stiffness and damping ratio.` | The function `physics2d.newGrabber()` or a property of the grabber received a value that is not positive. |
| `A force field needs a circle, a rectangle or a polygon of at least three points, convex for buoyancy, a direction that is not zero, drags, a density and a minimum distance of zero or more, and a minimum distance for an inverse square falloff.` | The function `physics2d.newForceField()` received bad options. |
| `A force field needs a direction that is not zero.` | The property `field.direction` received a zero vector. |
| `A force field needs a density of zero or more.` | The property `field.density` received a negative value. |
| `A force field needs drags of zero or more.` | The property `field.linearDrag` or `field.angularDrag` received a negative value. |
| `The force field was destroyed.` | A destroyed force field was used. |
| `An explosion needs a positive radius.` | The function `physics2d.explode()` or `terrain:explode()` received a radius that is not positive. |
| `A terrain needs at least 2 by 2 samples, a positive cell size and chunk size, and a tolerance of at least zero.` | The function `physics2d.newTerrain()` received bad options. |
| `The terrain needs one value per sample.` | The property `terrain.samples` received a list of another length. |
| `The sample is outside the terrain.` | The method `terrain:sample()` received a column or row outside the grid. |
| `A fracture needs at least one piece.` | The function `physics2d.fracture()` or `physics2d.splitPolygon()` received fewer than one piece. |
| `A fluid needs a positive particle radius, a larger smoothing radius, and a density, friction, restitution, rest density, stiffness, viscosity and top speed of zero or more.` | The function `physics2d.newFluid()` received bad options. |
| `particles count from 1` | The method `fluid:remove()` received a position below 1. It comes inside a bad argument error. |
| `The fluid has no such particle.` | The method `fluid:remove()` received a position past the last particle. |
| `the buffer needs two values for each particle` | The float buffer of `fluid:positions()` or `fluid:velocities()` is too short. It comes inside a bad argument error. |
