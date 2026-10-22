# 📖 `haylen.physics2d`

The module `haylen.physics2d` simulates 2D rigid bodies with Box2D. Use it for anything that must collide, bounce, stack, hang from joints or be found with ray casts and queries, from falling crates to platformer characters and trigger zones. It also builds ropes, ragdolls, vehicles, character movers, grabbers, force fields, explosions, destructible terrain, fractures and fluids. The [physics guide](../2d/physics.md) explains worlds, units, stepping, threads and sleeping, and the solutions to the problems that physics games meet most often.

```lua
local physics2d = require('haylen.physics2d')
```

## Units

Positions, sizes, distances and velocities are in world units, the units of the camera and of sprites, with y pointing down. Angles are in radians. Masses are in kilograms, and forces, impulses, torques and inertias combine kilograms with world units. Each world converts world units to meters for Box2D with its `pixelsPerMeter` scale, so the same numbers give the same motion at every scale. The [units table](../2d/physics.md#units-and-scale) of the guide lists every quantity with an example.

## Functions

The module has these functions. Each one is described in the section of what it makes.

| Function | Returns | Section |
| --- | --- | --- |
| [`physics2d.newWorld(options)`](#physics2dnewworldoptions) | A world | [PhysicsWorld](#physicsworld) |
| [`physics2d.newRayBatch(count)`](#physics2dnewraybatchcount) | A batch of rays | [Queries and ray casts](#queries-and-ray-casts) |
| [`physics2d.drawRay(x1, y1, x2, y2, hit, order)`](#physics2ddrawrayx1-y1-x2-y2-hit-order) | Nothing | [Queries and ray casts](#queries-and-ray-casts) |
| [`physics2d.newRope(world, options)`](#physics2dnewropeworld-options) | A rope | [Rope](#rope) |
| [`physics2d.newBridge(world, options)`](#physics2dnewbridgeworld-options) | A rope of planks | [Rope](#rope) |
| [`physics2d.newRagdoll(world, options)`](#physics2dnewragdollworld-options) | A ragdoll | [Ragdoll](#ragdoll) |
| [`physics2d.newVehicle(world, options)`](#physics2dnewvehicleworld-options) | A car seen from the side | [Vehicle](#vehicle) |
| [`physics2d.newTopDownVehicle(world, options)`](#physics2dnewtopdownvehicleworld-options) | A car seen from above | [TopDownVehicle](#topdownvehicle) |
| [`physics2d.newMover(world, options)`](#physics2dnewmoverworld-options) | A character mover | [Mover](#mover) |
| [`physics2d.newGrabber(world, options)`](#physics2dnewgrabberworld-options) | A grabber | [Grabber](#grabber) |
| [`physics2d.newForceField(world, options)`](#physics2dnewforcefieldworld-options) | A force field | [ForceField](#forcefield) |
| [`physics2d.explode(world, options)`](#physics2dexplodeworld-options) | The bodies the blast pushed | [Explosions](#explosions) |
| [`physics2d.newTerrain(world, options)`](#physics2dnewterrainworld-options) | Destructible ground | [Terrain](#terrain) |
| [`physics2d.fracture(body, options)`](#physics2dfracturebody-options) | The pieces of a body | [Fractures](#fractures) |
| [`physics2d.splitPolygon(shape, options)`](#physics2dsplitpolygonshape-options) | The pieces of an outline | [Fractures](#fractures) |
| [`physics2d.newFluid(world, options)`](#physics2dnewfluidworld-options) | A fluid | [Fluid](#fluid) |

A vehicle, top-down vehicle, mover, grabber, force field, terrain or fluid owns what it adds to the world. It lives while a Lua value refers to it, and the garbage collector removes it, with its bodies, once none does. So keep it in a variable or in the table of its scene. Each one keeps its world alive.

## PhysicsWorld

A `PhysicsWorld`, named `haylen.PhysicsWorld` in messages, holds bodies, shapes and joints and moves them when the app steps it. The function `physics2d.newWorld` makes one. Its bodies, shapes, joints and helpers keep it alive, so a world lives as long as any of them. The [guide](../2d/physics.md#what-a-world-holds) explains what a world holds.

A world has many methods. This section describes those that step and read the world, and the other sections describe the rest:

| Topic | Members |
| --- | --- |
| Stepping and reading | [`world:step`](#worldstepdeltaseconds), [`world:wakeAll`](#worldwakeall), [`world:readTransforms`](#worldreadtransformsbodies-buffer-first-interpolated), [`world:writeTransforms`](#worldwritetransformsbodies-buffer-first), [`world:stats`](#worldstats), [`world:stateHash`](#worldstatehashbodies) and [`world:debugDraw`](#worlddebugdraworder) |
| Bodies | [`world:createBody`](#worldcreatebodyoptions) |
| Joints | [`world:createJoint`](#worldcreatejointtype-a-b-options) |
| Queries | [`world:queryRect`](#worldqueryrectrect-filter), [`world:queryCircle`](#worldquerycirclex-y-radius-filter), [`world:queryPoint`](#worldquerypointx-y-filter) and [`world:pick`](#worldpickcamera-x-y-filter) |
| Ray and shape casts | [`world:raycast`](#worldraycastx1-y1-x2-y2-filter), [`world:raycastAll`](#worldraycastallx1-y1-x2-y2-filter), [`world:castCircle`](#worldcastcirclex-y-radius-dx-dy-filter), [`world:castBox`](#worldcastboxx-y-width-height-rotation-dx-dy-filter), [`world:castCapsule`](#worldcastcapsulex1-y1-x2-y2-radius-dx-dy-filter), [`world:castPolygon`](#worldcastpolygonpoints-dx-dy-filter), [`world:bounceRay`](#worldbouncerayx-y-dx-dy-length-bounces-filter), [`world:rayFan`](#worldrayfanx-y-angle-spread-count-length-filter), [`world:lineOfSight`](#worldlineofsightx1-y1-x2-y2-filter), [`world:raycastBatch`](#worldraycastbatchbatch-filter), [`world:predictPath`](#worldpredictpathx-y-vx-vy-options) and [`world:debugDrawRays`](#worlddebugdrawraysorder) |
| Events | [`world.onContactBegin`](#worldoncontactbegina-b-contact), [`world.onContactEnd`](#worldoncontactenda-b-contact), [`world.onHit`](#worldonhita-b-contact), [`world.onSensorBegin`](#worldonsensorbeginsensor-visitor-shapes), [`world.onSensorEnd`](#worldonsensorendsensor-visitor-shapes) and [`world.onJointBreak`](#worldonjointbreakjoint-info) |
| Properties | [World properties](#world-properties), such as `gravity`, `subSteps` and `debugRays` |

### physics2d.newWorld(options)

Creates an empty world and returns it. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `gravity` | Vec2 or `{x, y}` | `{0, 980}` | Gravity in world units per second squared. |
| `pixelsPerMeter` | number | `64` | World units in one Box2D meter. It must be positive. |
| `subSteps` | integer | `4` | Solver sub-steps per step, at least 1. More sub-steps make stacks and joints stiffer and cost more. |
| `threads` | integer | The workers of the job system, at most `4` | Threads that step the world, at least 1. The [guide](../2d/physics.md#threads) explains when a world uses them. |
| `continuous` | boolean | `true` | Sweeps fast dynamic bodies against static shapes, so they never pass through thin walls and floors. The body option `bullet` extends the sweep to dynamic and kinematic shapes. |
| `sleepEnabled` | boolean | `true` | Lets bodies at rest fall asleep, which costs nothing until something wakes them. |
| `interpolate` | boolean | `false` | Records the transforms of the last two steps, so `body:renderTransform()` and `world:readTransforms()` draw bodies smoothly between fixed steps. |
| `contactHertz` | number | `30` | Stiffness of contacts in cycles per second. Higher values push overlapping shapes apart faster and may jitter. |
| `contactDampingRatio` | number | `10` | Damping of the push that separates overlapping shapes. Lower values separate them faster and harder. |
| `contactPushSpeed` | number | `3 * pixelsPerMeter` | Fastest speed in world units per second at which contacts push overlapping shapes apart, 192 at the default scale. |
| `maxSpeed` | number | `400 * pixelsPerMeter` | Speed limit of every body in world units per second, 25600 at the default scale. |
| `restitutionThreshold` | number | `pixelsPerMeter` | Impacts slower than this, in world units per second, never bounce, whatever the restitution. |
| `hitThreshold` | number | `pixelsPerMeter` | Impacts slower than this, in world units per second, call no `onHit`. |

The scale, the threads and `interpolate` stay as the world was created. Every other option is also a [world property](#world-properties) that changes while the world runs. The speeds you leave out keep the values Box2D is tuned for in meters. That is a speed limit of 400 meters per second, overlaps pushed apart at up to 3 meters per second, and bounces and hits from 1 meter per second.

A scale that is not positive, fewer than one sub-step or fewer than one thread raise `A physics world needs positive pixels per meter, at least one sub-step and at least one thread.`. Box2D holds a limited number of worlds at once, 128 by default. A world beyond them raises `Too many physics worlds exist at once to create another one.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 1200}, pixelsPerMeter = 32, subSteps = 8})
local space = physics2d.newWorld({gravity = {0, 0}, threads = 1, sleepEnabled = false})
local arena = physics2d.newWorld({interpolate = true, maxSpeed = 6000, hitThreshold = 200})
print(world.subSteps, space.threads, arena.interpolate)
```

### world:step(deltaSeconds)

Advances the simulation by `deltaSeconds`, then calls the event callbacks for the contacts, hits, sensor overlaps and broken joints of this step. Call it from the `fixedUpdate` callback of a scene, with the length of the fixed step. The [guide](../2d/physics.md#what-one-step-does) lists what one step does, in order. An error raised by a callback propagates out of `world:step()`.

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

### world:wakeAll()

Wakes every sleeping body of the world. Use it for changes the solver does not notice by itself, such as a rule of the app that makes resting bodies move again. Writing `world.gravity`, and turning `world.sleepEnabled` off, already wake every body.

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

Copies the `x`, `y` and `rotation` of every body in the list `bodies` into a float buffer of [`haylen.collections`](collections.md#float-buffers). It writes three values for each body, in order, from the position `first`, which defaults to 1. Together with `SpriteBatch:writeFields` of [`haylen.graphics2d`](graphics2d.md), it keeps thousands of sprites on their bodies with two calls a frame, instead of one property read per body.

With `interpolated` set to `true`, a world created with `interpolate` gives each body between its transforms of the last two steps. It blends them by [`haylen.interpolation()`](haylen.md#hayleninterpolation), like [`body:renderTransform()`](#bodyrendertransform).

These errors can happen:

- A body that is destroyed or belongs to another world raises `Body transforms need live bodies of this world.`.
- A buffer without three values for each body from `first` raises `Body transforms take three floats for each body.`.
- A `first` outside the buffer raises a bad argument error with `the first position is outside the buffer`.
- An `interpolated` read in a world without interpolation raises `Only a world created with interpolation blends the transforms of its bodies.`.

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

Moves every body in the list `bodies` to the `x`, `y` and `rotation` that the buffer holds for it, three values for each body from the position `first`. It works like setting the transform of each body in turn. It suits kinematic bodies that follow positions the app computes in bulk. It raises the errors of `world:readTransforms()`.

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

Returns a table with the counts of the world and the times, in milliseconds, of the parts of its last step. A debug panel shows them while you tune a scene.

| Field | Meaning |
| --- | --- |
| `bodies` | Bodies in the world. |
| `awakeBodies` | Bodies that are awake. |
| `shapes` | Shapes of every body. |
| `contacts` | Contacts that Box2D tracks between shapes whose bounds overlap, touching or not. |
| `joints` | Joints in the world. |
| `islands` | Islands, the groups of bodies joined by contacts and joints that sleep and wake together. |
| `stepMilliseconds` | Time of the whole Box2D step. |
| `collideMilliseconds` | Time spent finding and updating contacts. |
| `solveMilliseconds` | Time spent solving contacts and joints and moving the bodies. |
| `continuousMilliseconds` | Part of the solve spent sweeping bullets. |
| `sleepMilliseconds` | Part of the solve spent putting islands to sleep. |
| `hookMilliseconds` | Time that the vehicles, force fields and fluids of the world took in the step, outside the Box2D step. |

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

Returns an integer hash of the positions, rotations and velocities of the bodies in the list `bodies`, bit for bit. Two runs that should match compare one number, such as a replay and its recording, or the same world on another number of threads. A body that is destroyed or belongs to another world raises `A state hash needs live bodies of this world.`.

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
            print('State after one second', world:stateHash({ball}))
        end
    end,
})
```

### world:debugDraw(order)

Draws the outlines of every shape and joint, and the contact points, of the world on the current canvas. The lines keep the same width on the screen at any zoom, which helps while you tune collisions. Call it from the `render` callback of a scene, after `graphics2d.beginWorld()`. The optional table `order` takes the `layer`, `depth` and `blend` fields of [`haylen.graphics2d`](graphics2d.md). The `physics` [debug drawing](debug.md#drawings) draws every world that `physics2d.newWorld` made this way, in every world canvas, without a call.

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
| `gravity` | Vec2 | read and write | Gravity in world units per second squared. Writing takes a `Vec2` or a table `{x, y}` and wakes every body. |
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
| `debugRays` | boolean | read and write | While `true`, the world records every ray and shape cast for [`world:debugDrawRays()`](#worlddebugdrawraysorder). |
| `onContactBegin` | function or nil | read and write | Called when two shapes start touching. |
| `onContactEnd` | function or nil | read and write | Called when two shapes stop touching. |
| `onHit` | function or nil | read and write | Called when two shapes hit each other fast. |
| `onSensorBegin` | function or nil | read and write | Called when a shape enters a sensor. |
| `onSensorEnd` | function or nil | read and write | Called when a shape leaves a sensor. |
| `onJointBreak` | function or nil | read and write | Called when a step breaks a joint. |

These errors can happen:

- Fewer than one sub-step raises `A physics world needs at least one sub-step.`.
- A speed that is negative or not finite raises `A physics speed needs to be finite and zero or more.`.
- A contact stiffness that is not positive raises `A physics world needs a positive contact stiffness.`.
- A contact damping ratio that is not positive raises `A physics world needs a positive contact damping ratio.`.
- A callback that is not a function or `nil` raises a bad argument error, and so does reading or writing an unknown property.

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

## Body

A `Body`, named `haylen.Body` in messages, is an object that moves. The method `world:createBody()` makes one. A body has no collision until shapes are added to it with [`body:addBox`](#bodyaddboxwidth-height-options), [`body:addCircle`](#bodyaddcircleradius-options), [`body:addCapsule`](#bodyaddcapsulex1-y1-x2-y2-radius-options), [`body:addSegment`](#bodyaddsegmentx1-y1-x2-y2-options), [`body:addPolygon`](#bodyaddpolygonpoints-options) or [`body:addChain`](#bodyaddchainpoints-loop-options), which the [Shape](#shape) section describes.

Every member except `valid`, `world` and `destroy` raises `The body was destroyed.` once the body is gone. Two handles of the same body compare equal with `==`.

### world:createBody(options)

Creates a body and returns it. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `type` | string | `'dynamic'` | The body type: `'static'`, `'kinematic'` or `'dynamic'`, as the [guide](../2d/physics.md#what-a-world-holds) explains. |
| `x`, `y` | number | `0` | Position of the body origin. |
| `rotation` | number | `0` | Angle in radians. |
| `vx`, `vy` | number | `0` | Starting velocity in units per second. |
| `angularVelocity` | number | `0` | Starting spin in radians per second. |
| `linearDamping` | number | `0` | Slows the linear velocity over time. |
| `angularDamping` | number | `0` | Slows the spin over time. |
| `gravityScale` | number | `1` | Multiplies the world gravity for this body. |
| `fixedRotation` | boolean | `false` | Keeps the body from rotating. |
| `bullet` | boolean | `false` | Sweeps the body against dynamic and kinematic bodies too, not only static ones, so a fast projectile never passes a thin moving body. |
| `fastRotation` | boolean | `false` | Lifts the limit of 45 degrees of rotation per step, so wheels and rotors spin as fast as they are driven. Only creation sets it. |
| `sleepEnabled` | boolean | `true` | Lets the body fall asleep when it comes to rest. |
| `sleepThreshold` | number | `0.05 * pixelsPerMeter` | Speed in units per second below which the body may fall asleep, 3.2 at the default scale. |

A damping that is negative or not finite raises `A physics body needs a finite damping of zero or more.`. A sleep threshold that is negative or not finite raises `A physics speed needs to be finite and zero or more.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local floor = world:createBody({type = 'static', x = 0, y = 300})
local platform = world:createBody({type = 'kinematic', x = 0, y = 100, vx = 60})
local arrow = world:createBody({x = -300, y = 0, vx = 900, rotation = 0.1, bullet = true, gravityScale = 0.5})
local player = world:createBody({x = 0, y = 0, fixedRotation = true, linearDamping = 0.2, sleepEnabled = false})
local wheel = world:createBody({x = 200, y = 0, fastRotation = true, sleepThreshold = 1})
```

### body:shapes()

Returns a list of every shape of the body, chain segments included.

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

Returns the outline of every shape of the body, in world units, where the body is now. Each outline is a table with `points`, a list of `Vec2` values, and `closed`, which is `true` when the last point joins back to the first. Each outline is what [`shape:outline()`](#shapeoutline) returns, except that the segments of a chain join into one outline through every point of the chain, closed for a loop. Use it to draw bodies that the engine made, such as the pieces of `physics2d.fracture` or the parts of `physics2d.newRagdoll`, without knowing their shapes.

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

Returns a list of the contacts of the body that touch, one table for each pair of touching shapes. Each table has `shape`, the shape of this body, `otherShape`, the shape it touches, and `other`, the body of `otherShape`. It also has the fields of a [contact table](#contact-tables), read from this body: the normal points from `shape` toward `otherShape`, and the relative velocity is that of `other`. Unlike the events, the list tells at any moment what a body stands on and pushes against, and how hard.

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

Applies a force until the next step and wakes the body. Without `px, py`, the force acts on the center of mass. With a world point `px, py`, it also makes the body spin.

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

Changes the velocity at once, by the impulse divided by the mass, and wakes the body. Without `px, py`, the impulse acts on the center of mass. With a world point `px, py`, it also makes the body spin.

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

Applies a torque until the next step and wakes the body. The torque is in kilograms times squared world units per second squared.

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

Changes the angular velocity at once, by the impulse divided by the rotational inertia, and wakes the body. The impulse is in kilograms times squared world units per second.

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

Teleports the body to `x, y` with the given rotation in radians. The rotation defaults to 0, so pass `body.rotation` to keep the current angle. A teleport skips the sweeps of continuous collision and shoves aside what the body lands in. So it suits spawning and respawning. Bodies that push and carry others, such as moving platforms, move with [`body:moveTo()`](#bodymovetox-y-rotation-seconds) instead.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local player = world:createBody({x = 0, y = 0})
player:addBox(32, 48)
player:setTransform(100, -50, player.rotation)
```

### body:moveTo(x, y, rotation, seconds)

Gives the body the velocity that brings it to `x, y` and the angle `rotation` after `seconds`. So a kinematic platform that follows a path, a tween or an animation pushes and carries what touches it, instead of teleporting through it. The rotation defaults to the current one. The time defaults to one fixed step, `haylen.fixedStep()`, so one call per fixed step before `world:step()` moves the body exactly to each target. The velocity stays until something changes it, and a target the body already holds stops it. Static bodies ignore the call. A time that is not positive raises `A physics body moves to a target over a positive time.`.

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

Returns the velocity of the world point `x, y` as if it were fixed to the body, as a `Vec2`. It includes the spin of the body, so it gives the speed of the rim of a wheel, or of the ground under the feet of a character.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local wheel = world:createBody({x = 0, y = 0, vx = 10, angularVelocity = 2})
wheel:addCircle(50)
local rim = wheel:velocityAt(0, 50)
print(rim.x, rim.y) -- -90.0 0.0
```

### body:resetMass()

Computes the mass, the center of mass and the rotational inertia of the body again, from the density and the area of its shapes. This undoes the values that the properties `mass`, `centerOfMass` and `inertia` set. Adding or destroying a shape, and changing a density, also compute them from the shapes again.

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

Lets the body pass through every [one-way platform](#one-way-platforms) for `seconds`, 0.2 by default. After that, it still passes the platform it is inside until it leaves it. So a character drops through the platform it stands on and lands on the next one below. The call wakes the body. A negative or infinite time raises `A physics body drops through one-way platforms for a finite time of zero or more.`.

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local ledge = world:createBody({type = 'static', x = 0, y = 100})
ledge:addBox(240, 8, {oneWay = {0, -1}})
local player = world:createBody({x = 0, y = 60, fixedRotation = true})
player:addCapsule(0, -12, 0, 12, 10)

scene.push({
    update = function(self, dt)
        if input.keyPressed('down') then
            player:dropThrough(0.3)
        end
    end,
    fixedUpdate = function(self, step)
        world:step(step)
    end,
})
```

### body:renderTransform()

Returns the `x`, `y` and `rotation` at which to draw the body in this frame. In a world created with `interpolate`, the body lies between its transforms of the last two steps, blended by [`haylen.interpolation()`](haylen.md#hayleninterpolation). This keeps motion smooth when frames and fixed steps do not line up. A body that the last step did not move, or that a teleport placed, reads where it is. So does every body of a world without interpolation.

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

Destroys the body with its shapes and joints, and releases its `data` at once. Destroying a body twice does nothing.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local enemy = world:createBody()
enemy:addCircle(10)
enemy:destroy()
print(enemy.valid)
```

### body:moveToWorld(world)

Moves the body to another `PhysicsWorld` and returns the body there. From the next step, the body obeys that world, such as an object thrown through a portal into a world of other gravity.

The body keeps its type, place, rotation, velocities, damping, gravity scale, flags and mass. It keeps every shape with its material, filter, sensor, events, tangent speed and one-way side. All of these are in world units, so in a world of another `pixelsPerMeter` the body keeps its size and speed. Its `data` goes along. The body of the first world is destroyed with its joints, since joints cannot cross worlds, so the old handle reports `valid` as `false`. Moving a body to the world it is in raises `A body moves only to another world.`.

```lua
local physics2d = require('haylen.physics2d')

local land = physics2d.newWorld({gravity = {0, 980}})
local space = physics2d.newWorld({gravity = {0, 0}})
local rock = land:createBody({x = 0, y = 0, vx = 400})
rock:addCircle(16, {restitution = 0.4})
rock.data = {name = 'rock'}

rock = rock:moveToWorld(space) -- It floats on from here, with the speed it had.
print(rock.world == space, rock.data.name)
```

### Body properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | boolean | read | The value is `false` once the body is destroyed. |
| `world` | PhysicsWorld | read | The world that owns the body. |
| `type` | string | read and write | `'static'`, `'kinematic'` or `'dynamic'`. |
| `x`, `y` | number | read and write | Position of the body origin. Writing teleports the body. |
| `position` | Vec2 | read and write | Position as a `Vec2`. Writing takes a `Vec2` or `{x, y}` and teleports the body. |
| `rotation` | number | read and write | Angle in radians. |
| `velocity` | Vec2 | read and write | Linear velocity in units per second. Writing takes a `Vec2` or `{x, y}`. |
| `angularVelocity` | number | read and write | Spin in radians per second. |
| `mass` | number | read and write | Mass in kilograms, computed from the shapes. Writing it scales the inertia with it. |
| `inertia` | number | read and write | Rotational inertia around the center of mass, in kilograms times squared units. |
| `centerOfMass` | Vec2 | read and write | Center of mass in the space of the body. Writing takes a `Vec2` or `{x, y}`. A center low in the body keeps it upright, like a keel. |
| `worldCenter` | Vec2 | read | Center of mass in world units. |
| `awake` | boolean | read and write | Whether the body is awake. The solver skips sleeping bodies until something touches them. Writing `true` wakes the body. |
| `sleepEnabled` | boolean | read and write | Lets the body fall asleep when it comes to rest. |
| `sleepThreshold` | number | read and write | Speed in units per second below which the body may fall asleep. |
| `enabled` | boolean | read and write | A disabled body leaves the simulation and stops colliding until it is enabled again. |
| `linearDamping` | number | read and write | Slows the linear velocity over time. It must be finite and not negative. |
| `angularDamping` | number | read and write | Slows the spin over time. It must be finite and not negative. |
| `gravityScale` | number | read and write | Multiplies the world gravity for this body. |
| `fixedRotation` | boolean | read and write | Keeps the body from rotating. |
| `bullet` | boolean | read and write | Sweeps the body against dynamic and kinematic bodies too. |
| `data` | any | read and write | Any Lua value attached to the body, such as the entity it belongs to. |

The world keeps `data` until the body is destroyed. The method `body:destroy()` releases it at once. The next `world:step()` releases the data of bodies destroyed any other way:

- The function `physics2d.fracture` destroyed them.
- The `destroy` of a rope, ragdoll, vehicle or mover destroyed them.
- A terrain destroyed them.
- The garbage collector took a helper that owned them.

These errors can happen:

- A mass, inertia or center of mass that is negative or not finite raises `A physics body needs a finite mass, center of mass and inertia of zero or more.`.
- A damping that is negative or not finite raises `A physics body needs a finite damping of zero or more.`.
- A sleep threshold that is negative or not finite raises `A physics speed needs to be finite and zero or more.`.

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

A `Shape`, named `haylen.Shape` in messages, gives a body its collision. The `add` methods of a body make shapes, and `body:shapes()`, queries, casts and events return them. Every member except `valid` and `destroy` raises `The shape was destroyed.` once the shape is gone. Two handles of the same shape compare equal with `==`.

### body:addBox(width, height, options)

Adds a box centered on the body origin and returns its shape. The optional table `options` holds the [shape options](#shape-options), and invalid options raise their error before the shape exists. A size that is not positive raises `A physics box needs a positive size.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local crate = world:createBody({x = 0, y = 0})
local shape = crate:addBox(32, 32, {density = 2, friction = 0.4, restitution = 0.1})
local lid = crate:addBox(36, 6, {offsetY = -19, rotation = 0.05})
```

### body:addCircle(radius, options)

Adds a circle and returns its shape. The circle is centered on the body origin, moved by `offsetX` and `offsetY`. A radius that is not positive raises `A physics circle needs a positive radius.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ball = world:createBody({x = 0, y = 0, bullet = true})
ball:addCircle(8, {restitution = 0.8, density = 0.5, rollingResistance = 0.05})
```

### body:addCapsule(x1, y1, x2, y2, radius, options)

Adds a capsule and returns its shape. A capsule is the segment from `x1, y1` to `x2, y2`, in body coordinates, rounded by `radius`. Capsules suit characters, because they slide over the seams between tiles. A radius that is not positive raises `A physics capsule needs a positive radius.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local hero = world:createBody({x = 0, y = 0, fixedRotation = true})
hero:addCapsule(0, -16, 0, 16, 12, {friction = 0})
```

### body:addSegment(x1, y1, x2, y2, options)

Adds a line segment from `x1, y1` to `x2, y2`, in body coordinates, and returns its shape. Segments have no area, so they suit static walls and floors. Ends 0.005 meters apart or closer raise `A physics segment needs ends more than 0.005 meters apart.`. That is 0.32 world units at the default 64 pixels per meter.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local floor = world:createBody({type = 'static', x = 0, y = 0})
floor:addSegment(-400, 200, 400, 200)
```

### body:addPolygon(points, options)

Adds a simple polygon and returns a list of its shapes. The points are a list of `Vec2` values or `{x, y}` tables, in body coordinates. A convex outline of at most eight points becomes one shape. Any other outline is split into convex pieces of at most eight points with `m.polygon.decompose` of [`haylen.math`](math.md).

These errors can happen:

- Fewer than three points raise `A physics polygon needs at least three points.`.
- A convex outline without area raises `A physics polygon needs at least three points that are not on one line.`.
- An outline whose pieces have no area raises `A physics polygon needs a non-degenerate outline.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ramp = world:createBody({type = 'static', x = 0, y = 100})
local shapes = ramp:addPolygon({{0, 0}, {200, 0}, {200, -80}})
local cave = world:createBody({type = 'static', x = 300, y = 100})
cave:addPolygon({{0, 0}, {120, 0}, {60, -40}, {120, -120}, {0, -120}}, {friction = 0.9})
```

### body:addChain(points, loop, options)

Adds a chain of one-sided segments through the points and returns a list of its segment shapes. Chains suit terrain outlines: bodies slide over the joints between their segments without catching, as the [guide](../2d/physics.md#bodies-catch-on-the-joints-between-tiles) explains.

- With `loop` set to `true`, the chain closes its outline and needs at least four points.
- An open chain needs at least two points. It collides along every segment between the points it lists.
- Fewer points raise `A physics chain needs at least four points for a loop and two for an open chain.`.
- Chains use `friction`, `restitution`, `rollingResistance`, `tangentSpeed`, `oneWay`, the collision filter, the event switches, `offsetX`, `offsetY` and `rotation` from the options. They ignore `density` and `sensor`.
- Destroying any segment destroys the whole chain.

Each segment collides only on its left side as seen on screen, walking from one point to the next. Bodies pass through it from the other side. So an open chain listed from left to right carries bodies on top of it. A loop listed counter-clockwise on screen holds bodies inside it, like the walls of an arena. A loop listed clockwise on screen is solid from the outside, like an island. Ray casts hit a chain only from its solid side too, while shape casts find it from both sides.

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

Every `add` method takes the same optional table of options. An unknown key raises `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `density` | number | `1` | Mass per square meter, finite and not negative. |
| `friction` | number | `0.6` | Friction coefficient, usually between 0 and 1, finite and not negative. |
| `restitution` | number | `0` | Bounciness, finite and not negative. At 1 a bounce keeps all the speed. |
| `rollingResistance` | number | `0` | Slows round shapes that roll on others, such as a ball on grass, finite and not negative. |
| `sensor` | boolean | `false` | Makes the shape a sensor, which reports overlaps and never collides. |
| `category` | integer | `1` | Collision category bits of the shape. |
| `mask` | integer | `-1` (all bits) | Categories the shape collides with. |
| `group` | integer | `0` | Collision group, as [collision filtering](#collision-filtering) describes. |
| `offsetX`, `offsetY` | number | `0` | Moves the shape away from the body origin. |
| `rotation` | number | `0` | Turns the shape around the body origin before the offset, in radians. |
| `tangentSpeed` | number | `0` | Turns the surface into a [conveyor](#conveyors) that carries touching bodies at this speed. |
| `oneWay` | Vec2 | `nil` | Makes the shape a [one-way platform](#one-way-platforms). |
| `contactEvents` | boolean | `true` | Lets the contacts of the shape call `onContactBegin` and `onContactEnd`. |
| `hitEvents` | boolean | `true` | Lets the contacts of the shape call `onHit`. |
| `sensorEvents` | boolean | `true` | For a sensor, whether it detects visitors. For any other shape, whether sensors detect it. |

A density, friction, restitution or rolling resistance that is negative or not finite raises `A physics shape needs a finite density, friction, restitution and rolling resistance of zero or more.`. A zero `oneWay` direction raises `A one-way direction cannot be zero.`. Both raise before the shape is created. The materials and the event switches also change while the shape exists, as [Shape properties](#shape-properties) lists.

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

Returns the outline of the shape in world units, where its body is now. The result is a table with `points`, a list of `Vec2` values, and `closed`, which is `true` when the last point joins back to the first.

- A polygon gives its corners, closed.
- A circle gives points around it, closed.
- A capsule gives points around its two round ends, closed, which join into its straight sides.
- A segment or a chain segment gives its two ends, open.

Round outlines take one step every two world units of radius, between 16 and 96 steps for a whole turn. The call `graphics2d.drawPolygon(outline.points, color)` fills a closed outline, and `graphics2d.drawPolyline(outline.points, thickness, color, outline.closed)` strokes any of them.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local body = world:createBody({x = 100, y = 50})
local ball = body:addCircle(12)
local outline = ball:outline()
print(#outline.points, outline.closed)
```

### shape:overlaps()

Returns the list of shapes inside a sensor shape after the last step. It stays right through teleports and destroyed visitors, where counting the sensor events would not. Visitors need `sensorEvents` on, as for the events. Static shapes count too, so a pressure plate keeps a small gap above the ground it rests on, or the ground turns off its `sensorEvents`. A shape that is not a sensor raises `Only sensor shapes have overlaps.`.

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
            print('The zone holds', #trigger:overlaps(), 'shapes')
        end
    end,
})
```

### Shape properties

The geometry properties return new values on every read. The `points` are in the space of the body, with the offset and rotation of the shape options applied. The `worldPoints` are the same points in world units, where the body is now.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | boolean | read | The value is `false` once the shape or its body is destroyed. |
| `body` | Body | read | The body the shape belongs to. |
| `kind` | string | read | `'circle'`, `'capsule'`, `'segment'`, `'polygon'` or `'chainSegment'`, one segment of a chain. |
| `points` | list of Vec2 | read | The points that place the shape on its body: the center of a circle, the two centers of a capsule, the two ends of a segment or a chain segment, or the corners of a polygon in order around it. |
| `worldPoints` | list of Vec2 | read | The `points` in world units. |
| `radius` | number | read | The radius of a circle or a capsule, and `0` for the other kinds. |
| `sensor` | boolean | read | Whether the shape is a sensor. |
| `bounds` | Rect | read | The bounding box that Box2D tracks for the shape, which includes a small collision margin. |
| `density` | number | read and write | Mass per square meter. Writing it updates the mass of the body. |
| `friction` | number | read and write | Friction coefficient. |
| `restitution` | number | read and write | Bounciness. |
| `rollingResistance` | number | read and write | Resistance of round shapes to rolling. |
| `contactEvents` | boolean | read and write | Whether the contacts of the shape call `onContactBegin` and `onContactEnd`. |
| `hitEvents` | boolean | read and write | Whether the contacts of the shape call `onHit`. |
| `sensorEvents` | boolean | read and write | Whether a sensor detects visitors, or whether sensors detect this shape. |
| `category` | integer | read and write | Collision category bits. |
| `mask` | integer | read and write | Categories the shape collides with. The default of all bits reads as `-1`. |
| `group` | integer | read and write | Collision group, as [collision filtering](#collision-filtering) describes. |
| `tangentSpeed` | number | read and write | Conveyor speed of the surface in world units per second. |
| `oneWay` | Vec2 or nil | read and write | Direction of a one-way platform in the space of the body, or `nil` for a solid shape. A zero vector raises `A one-way direction cannot be zero.`. |

A density, friction, restitution or rolling resistance that is negative or not finite raises `A physics shape needs a finite density, friction, restitution and rolling resistance of zero or more.`.

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

### Collision filtering

Two shapes collide when the category of each one shares a bit with the mask of the other. Categories and masks are 64-bit integers, so up to 64 categories exist. The group overrides both. Shapes that share a positive `group` always collide. Shapes that share a negative `group` never collide. The parts of a ragdoll or a vehicle share a negative group. So two figures with the same group pass through each other, and each figure that should hit the others takes a group of its own.

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

### One-way platforms

A shape with a `oneWay` direction blocks only the bodies that touch it from the side the direction points to. The direction is in the space of the body, so it turns with the body. With y pointing down, `{0, -1}` on a body that is not rotated makes a platform: bodies jump through it from below and land on it from above. The same platform turned upside down blocks bodies from below and lets them fall through from above. Setting `shape.oneWay = nil` makes the shape solid again. The [guide](../2d/physics.md#platforms-to-jump-through-and-drop-through) explains when a body lands on a platform and when it passes.

The method [`body:dropThrough()`](#bodydropthroughseconds) lets a body fall through the platform it stands on. [Movers](#mover) pass one-way platforms the same way, and [`mover:dropThrough()`](#moverdropthroughseconds) drops a mover through. Tiled objects and tile shapes whose `oneWay` property is `true` become platforms that point up, as [Tiled collision](#tiled-collision) describes.

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

### Conveyors

A shape with a `tangentSpeed` moves the bodies it touches along its surface, at that speed in world units per second. They move clockwise around the shape as seen on screen. So a positive speed carries the bodies on top of a platform to the right. Friction decides how fast they catch up.

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

## Joint

A `Joint`, named `haylen.Joint` in messages, connects two bodies of the same world. The method `world:createJoint()` makes one. Every setting a joint type takes at creation also changes while the joint runs, and a change wakes the bodies of the joint. Every member except `valid` and `destroy` raises `The physics joint was destroyed.` once the joint or one of its bodies is gone. A member that the type of the joint lacks raises an error that names the joint types that have it. Two handles of the same joint compare equal with `==`.

### world:createJoint(type, a, b, options)

Connects the bodies `a` and `b` of this world with a joint of the given `type` and returns it. Anchors and targets are world positions. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`. An unknown type raises a bad argument error with `unknown value '<type>'`. Bodies that are destroyed, or that belong to another world, raise `A joint needs two bodies of this world.`.

| Type | Behavior |
| --- | --- |
| `'distance'` | Keeps the anchor `ax, ay` on `a` and the anchor `bx, by` on `b` apart, at their current distance unless `length` is given. It can be springy, limited between a shortest and a longest length, and driven by a motor that changes its length. A spring of 0 hertz with a limit makes a rope that goes slack below its longest length. |
| `'revolute'` | Pins both bodies at `ax, ay` and lets them rotate around it, like a hinge or a wheel axle. Its angle is 0 in the pose the bodies have when the joint is created. Its spring pulls toward `targetAngle`. |
| `'prismatic'` | Lets `b` slide relative to `a` along the axis through `ax, ay`, like a piston or an elevator. It keeps the angle between them that they have when the joint is created. Its spring pulls toward `targetTranslation`. |
| `'weld'` | Glues both bodies together at `ax, ay`, rigidly or on a spring. |
| `'wheel'` | Lets `b` slide along the axis through `ax, ay`, usually on a spring, and rotate freely, like a car suspension. |
| `'mouse'` | Pulls `b` on a soft spring toward a target that starts at `bx, by`. Body `a` is usually a static body, and the joint does not move it. |
| `'motor'` | Drives `b` toward an offset and an angle from `a`. Both start at those the bodies have when the joint is created. |
| `'filter'` | Only keeps `a` and `b` from colliding with each other, without holding them together. |

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `ax`, `ay` | number | `0` | Anchor on `a`. Revolute, prismatic, weld and wheel joints use it as their shared anchor. |
| `bx`, `by` | number | `0` | Anchor on `b` for distance joints, and the first target of mouse joints. |
| `collideConnected` | boolean | `false` | Lets the two bodies keep colliding with each other. |
| `enableLimit` | boolean | `false` | Turns on the `lower` and `upper` limits of revolute, prismatic, wheel and distance joints. |
| `lower`, `upper` | number | `0` | The limits: angles in radians for revolute joints, measured from the pose at creation, translations in world units for prismatic and wheel joints, and the shortest and longest length for distance joints. |
| `enableMotor` | boolean | `false` | Turns on the motor of revolute, prismatic, wheel and distance joints. |
| `motorSpeed` | number | `0` | Motor speed in radians per second for revolute and wheel joints, or in world units per second for prismatic and distance joints. |
| `maxMotorForce` | number | `0` | The most force of the motor of prismatic and distance joints, of motor joints and of the pull of mouse joints, as a force in world units. |
| `maxMotorTorque` | number | `0` | The most torque of the motor of revolute and wheel joints, and of motor joints, as a torque in world units. |
| `enableSpring` | boolean | `false` | Makes distance, revolute, prismatic and wheel joints springy. |
| `hertz` | number | `0` | Spring stiffness in cycles per second. Weld joints use it for both the linear and the angular spring, and 0 keeps them rigid. |
| `dampingRatio` | number | `0` | Spring damping. At 1 the oscillation stops without overshoot. |
| `targetAngle` | number | `0` | Angle in radians that the spring of a revolute joint pulls toward. |
| `targetTranslation` | number | `0` | Translation in world units that the spring of a prismatic joint pulls toward. |
| `axisX`, `axisY` | number | `1`, `0` | Slide axis of prismatic and wheel joints. |
| `length` | number | `0` | Rest length of a distance joint. Zero keeps the distance that the anchors have when the joint is created. |
| `breakForce` | number | `nil` | Force beyond which a step breaks the joint, as [breaking joints](#breaking-joints) describes. Without it, the joint never breaks from a force. |
| `breakTorque` | number | `nil` | Torque beyond which a step breaks the joint. Without it, the joint never breaks from a torque. |

Mouse joints have their own values for the settings left at 0. Their `hertz` is 4 and their `dampingRatio` is 1. Their `maxMotorForce` is a pull that accelerates `b` at up to `1000 * pixelsPerMeter` units per second squared. Filter joints ignore every option. Every setting also changes while the joint runs, as [Joint properties](#joint-properties) lists.

Box2D checks limits even while `enableLimit` is `false`. So the joint is not created when its limits are out of range:

- A distance joint shorter than 0.005 meters raises `A distance joint needs a length of at least 0.005 meters.`. That is 0.32 world units at the default 64 pixels per meter. The default options put both anchors at `0, 0`, which is too short, so give a distance joint its anchors or a `length`.
- Revolute limits must lie within 0.99 pi radians of zero, with `lower` not above `upper`. Otherwise the call raises `A revolute joint needs a lower limit that is not above the upper one, both within 0.99 pi radians of zero.`.
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

### joint:destroy()

Removes the joint, so its bodies move on their own again. Destroying a joint twice does nothing. Destroying one of its bodies destroys the joint too.

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

### Joint properties

| Property | Joints | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | All | read | The value is `false` once the joint or one of its bodies is destroyed. |
| `type` | All | read | The joint type, such as `'revolute'`. |
| `bodyA`, `bodyB` | All | read | The two bodies the joint connects. |
| `anchorA`, `anchorB` | All | read | Where the joint holds each body now, in world units, as a `Vec2`. Draw a rope or a spring between the bodies there. |
| `constraintForce` | All | read | The force the joint applied in the last step to hold its bodies, as a `Vec2` in world units. |
| `constraintTorque` | All | read | The torque the joint applied in the last step. |
| `linearSeparation` | All | read | How far the anchors drifted apart in world units, which grows while a joint stretches. |
| `angularSeparation` | All | read | How far the angle drifted from what the joint holds, in radians. |
| `constraintHertz` | All | read and write | Stiffness that holds the joint together in cycles per second, `60` by default. Lower values make the joint soft. |
| `constraintDampingRatio` | All | read and write | Damping of that stiffness, `2` by default. |
| `breakForce`, `breakTorque` | All | read and write | The load beyond which a step breaks the joint, or `nil` for a joint that never breaks from it. |
| `target` | Mouse | read and write | The point the joint pulls `bodyB` toward, as a `Vec2`. Writing takes a `Vec2` or `{x, y}`. |
| `angle` | Revolute | read | The angle from the pose at creation, in radians. |
| `translation` | Prismatic | read | The translation along the axis, in world units. |
| `currentLength` | Distance | read | The distance between the anchors. |
| `length` | Distance | read and write | The rest length. |
| `enableLimit` | Revolute, prismatic, wheel, distance | read and write | Whether the limits hold. |
| `lower`, `upper` | Revolute, prismatic, wheel, distance | read and write | The limits: angles for revolute joints, translations for prismatic and wheel joints, and the shortest and longest length for distance joints. |
| `enableMotor` | Revolute, prismatic, wheel, distance | read and write | Whether the motor drives the joint. |
| `motorSpeed` | Revolute, prismatic, wheel, distance | read and write | The motor speed, as [`joint.motorSpeed`](#jointmotorspeed) describes. |
| `maxMotorForce` | Prismatic, distance, mouse, motor | read and write | The most force the motor or the pull applies. |
| `maxMotorTorque` | Revolute, wheel, motor | read and write | The most torque the motor applies. |
| `motorForce` | Prismatic, distance | read | The force the motor applied in the last step. |
| `motorTorque` | Revolute, wheel | read | The torque the motor applied in the last step. |
| `enableSpring` | Distance, revolute, prismatic, wheel, mouse, weld | read and write | Whether the spring acts. Mouse and weld joints are always springy and read `true`. Only distance, revolute, prismatic and wheel joints can switch it. |
| `hertz`, `dampingRatio` | Distance, revolute, prismatic, wheel, mouse, weld | read and write | The stiffness of the spring in cycles per second, and its damping. A weld joint of 0 hertz is rigid. |
| `targetAngle` | Revolute | read and write | The angle the spring pulls toward. |
| `targetTranslation` | Prismatic | read and write | The translation the spring pulls toward. |
| `linearOffset`, `angularOffset` | Motor | read and write | The offset of `bodyB` from `bodyA` in the space of `bodyA`, as a `Vec2`, and the angle between them, which the joint drives toward. |

A member that the joint type lacks raises one of these messages: `Only mouse joints have a target.`, `Only revolute joints have an angle.`, `Only prismatic joints have a translation.`, `Only distance joints have a current length.`, `Only distance joints have a length.`, `Only revolute, prismatic, wheel and distance joints have limits.`, `Only revolute, prismatic, wheel and distance joints have a motor.`, `Only revolute, prismatic, wheel and distance joints have a motor speed.`, `Only prismatic, distance, mouse and motor joints have a maximum motor force.`, `Only revolute, wheel and motor joints have a maximum motor torque.`, `Only prismatic and distance joints report a motor force.`, `Only revolute and wheel joints report a motor torque.`, `Only distance, revolute, prismatic, wheel, mouse and weld joints have a spring.`, `Only distance, revolute, prismatic and wheel joints switch their spring.`, `Only revolute joints have a target angle.`, `Only prismatic joints have a target translation.` or `Only motor joints have offsets.`.

Values out of range raise these errors:

- A `lower` above `upper` raises `A joint needs a lower limit that is not above the upper one.`. Revolute limits beyond 0.99 pi radians raise `A revolute joint needs a lower limit that is not above the upper one, both within 0.99 pi radians of zero.`.
- A distance joint shorter than 0.005 meters raises `A distance joint needs a length of at least 0.005 meters.`.
- A maximum motor force or torque that is negative or not finite raises `A joint needs a finite maximum motor force of zero or more.` or `A joint needs a finite maximum motor torque of zero or more.`.
- A spring stiffness or damping that is negative or not finite raises `A joint spring needs a finite stiffness and damping ratio of zero or more.`.
- A constraint stiffness that is not positive raises `A joint needs a positive constraint stiffness.`. A negative or infinite constraint damping raises `A joint needs a finite constraint damping ratio of zero or more.`.
- A negative break limit raises `A joint needs a break force and a break torque of zero or more.`.

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

The target of a mouse joint, which a write moves. This scene drags a crate with the mouse. The [grabber](#grabber) drags bodies this way, with a pull sized for everything joined to the body it holds.

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

### joint.motorSpeed

The motor speed of a revolute or wheel joint in radians per second, or of a prismatic or distance joint in world units per second. A write changes it and wakes the bodies.

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

A joint with a `breakForce` breaks once the force it applies to hold its bodies passes that value. A joint with a `breakTorque` breaks once its torque passes that value. Both are in world units. The step that loads the joint beyond its limit destroys it after Box2D advances the bodies. It then calls [`world.onJointBreak`](#worldonjointbreakjoint-info) with it, in the order of the joints. Without the callback the joint breaks all the same.

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

## Queries and ray casts

A world answers questions about what lies where. Queries find the shapes in an area. Ray casts find what a line hits, and shape casts find what a moving shape hits. None of them change the world. The [guide](../2d/physics.md#finding-bodies) says which one to use for what.

### Hit tables

Every ray cast and shape cast returns its hits as tables with these fields.

| Field | Type | Meaning |
| --- | --- | --- |
| `shape` | Shape | The shape the cast hit. |
| `body` | Body | The body of that shape. |
| `x`, `y` | number | The hit point. |
| `normalX`, `normalY` | number | The surface normal at the hit point, as a unit vector. |
| `fraction` | number | How far along the cast the hit is, from 0 at the start to 1 at the end. |
| `distance` | number | The same span in world units. |

### Query filters

The `query` methods and `world:pick()` take an optional filter table with the integer fields `category`, `1` by default, and `mask`, all bits by default. A shape is found when its category shares a bit with the filter mask, and the filter category shares a bit with the shape mask. The filter of `world:pick()` also takes `radius`. Other keys raise `Unknown option "<key>".`.

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

### Ray cast filters

The ray casts, the shape casts, `world:lineOfSight()` and `world:raycastBatch()` take an optional filter table with these fields. The method `world:predictPath()` takes its `category`, `mask` and `group`. Other keys raise `Unknown option "<key>".`.

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `category` | integer | `1` | The category of the cast. A shape is seen when its category shares a bit with the mask, and its mask shares a bit with this category, like a collision. |
| `mask` | integer | all bits | The categories the cast sees. |
| `group` | integer | `0` | A shape that shares a nonzero group is always seen when the group is positive, and never seen when it is negative. So a ragdoll can look past its own limbs. |
| `accept` | function | none | Called with each candidate hit table, in order from the start of the cast, after the other fields. A `true` result takes the hit. A `false` result skips it and keeps looking. An error raised by it propagates out of the cast. |

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

### world:queryRect(rect, filter)

Returns a list of the shapes whose bounding boxes overlap `rect`. The rectangle is a `Rect` or a table `{x, y, width, height}`. The optional filter is a [query filter](#query-filters). Bounding boxes are larger than rotated or round shapes, so check the result further when exact overlap matters. A negative width or height raises `A rectangle query needs a width and height of zero or more.`. So build a rectangle that the player dragged up or to the left with `m.fromMinMax(dragStart:min(dragEnd), dragStart:max(dragEnd))` of [`haylen.math`](math.md) before the query.

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

Returns a list of the shapes that overlap the circle of `radius` at `x, y`. The optional filter is a [query filter](#query-filters).

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

Returns a list of the shapes that contain the point `x, y`. The optional filter is a [query filter](#query-filters). Segments and chains have no inside. So a point inside the area that a chain loop outlines finds nothing, such as inside a merged region of [Tiled collision](#tiled-collision).

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

### world:pick(camera, x, y, filter)

Returns the list of shapes under or near the screen point `x, y`, seen through `camera`, nearest first. This is how a click or a tap selects bodies. Screen points are in design coordinates, like the pointer positions of [`haylen.input`](input.md).

The optional filter takes `category` and `mask`, like a [query filter](#query-filters). It also takes `radius`, the distance in design coordinates around the point within which shapes count, 0 by default. So a finger finds thin and small bodies near it. Shapes that contain the point come first, then the others by their distance from it. An unknown key raises `Unknown option "<key>".`. A negative radius raises a bad argument error with `the radius must be zero or more`.

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
                print('Picked', picked[1].body == crate, picked[1].body == rod)
            end
        end
    end,
})
```

### world:raycast(x1, y1, x2, y2, filter)

Casts a ray from `x1, y1` to `x2, y2` and returns the closest [hit](#hit-tables) that passes the filter, or `nil` when the ray hits nothing. The optional filter is a [ray cast filter](#ray-cast-filters). Sensors are hit like any other shape. To ignore them, give them their own category and leave it out of the filter mask. A ray that starts inside a shape does not see that shape. A ray sees a chain only from the side the chain collides on.

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

Casts a ray from `x1, y1` to `x2, y2` and returns the list of hits that pass the filter, sorted from the start of the ray to its end. The list is empty when the ray hits nothing. Besides the fields of a [ray cast filter](#ray-cast-filters), the filter takes an integer `limit`, which keeps only the first hits. So a bullet can pierce a given number of targets. A limit of 0, the default, keeps every hit.

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

Sweeps a circle of `radius`, centered on `x, y`, along the translation `dx, dy`, and returns the first hit or `nil`. The `fraction` tells how far the circle travels before it touches, so it stops at `x + dx * fraction, y + dy * fraction`. A circle that starts touching a shape hits it at fraction 0, with a zero normal. Use it for thick bullets, character probes and anything wider than a ray. A negative radius raises `A circle cast needs a finite radius of zero or more.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local wall = world:createBody({type = 'static', x = 300, y = 0})
wall:addBox(20, 400)

local ball = world:castCircle(0, 0, 16, 600, 0)
print(ball.fraction * 600, ball.x, ball.normalX)
```

### world:castBox(x, y, width, height, rotation, dx, dy, filter)

Sweeps a box of `width` by `height`, centered on `x, y` and turned by `rotation` radians, along the translation `dx, dy`, like `world:castCircle`. A size that is not positive raises `A box cast needs a positive size.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local wall = world:createBody({type = 'static', x = 300, y = 0})
wall:addBox(20, 400)

local crate = world:castBox(0, 0, 40, 40, 0.3, 600, 0)
print(crate.distance)
```

### world:castCapsule(x1, y1, x2, y2, radius, dx, dy, filter)

Sweeps a capsule around the segment from `x1, y1` to `x2, y2` along the translation `dx, dy`, like `world:castCircle`. A radius that is not positive raises `A capsule cast needs a positive radius.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local wall = world:createBody({type = 'static', x = 300, y = 0})
wall:addBox(20, 400)

local pill = world:castCapsule(0, -20, 0, 20, 8, 600, 0)
print(pill.distance)
```

### world:castPolygon(points, dx, dy, filter)

Sweeps the convex hull of the list `points` along the translation `dx, dy`, like `world:castCircle`. Fewer than three or more than eight points raise `A polygon cast needs between three and eight points.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local wall = world:createBody({type = 'static', x = 300, y = 0})
wall:addBox(20, 400)

local wedge = world:castPolygon({{0, -20}, {30, 0}, {0, 20}}, 600, 0)
print(wedge.distance)
```

### world:bounceRay(x, y, dx, dy, length, bounces, filter)

Follows a ray from `x, y` in the direction `dx, dy` for `length` world units, reflecting off every shape it hits, for at most `bounces` reflections. It works like a laser between mirrors or a ricocheting bullet. It returns three values: the list of bounce hits in order, and the `x` and `y` of the point where the path ends. The `distance` and `fraction` of each hit measure the whole path up to that bounce.

A zero direction raises a bad argument error with `the direction must not be zero`. A negative bounce count raises one with `the bounce count must not be negative`. A length that is not finite raises `A bouncing physics ray needs a finite length.`.

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
print('The laser ends at', endX, endY)
```

### world:rayFan(x, y, angle, spread, count, length, filter)

Casts `count` rays of `length` world units from `x, y`, spread evenly across an arc of `spread` radians centered on `angle`, like a cone of vision or a shotgun blast. It returns a list with one entry per ray, in order of increasing angle. Each entry is the hit of that ray, or `false` when it hit nothing. A single ray points along `angle`. A length that is not finite raises a bad argument error with `the length must be finite`.

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

Returns `true` when no shape that passes the filter blocks the segment between both points, such as a guard that checks whether it sees the player. Give the characters their own categories and leave them out of the mask, or skip them with `accept`, so they do not block their own sight.

```lua
local physics2d = require('haylen.physics2d')

local kWalls = 2
local world = physics2d.newWorld({gravity = {0, 0}})
local wall = world:createBody({type = 'static', x = 100, y = 0})
wall:addBox(20, 60, {category = kWalls})

print(world:lineOfSight(0, 0, 200, 0, {mask = kWalls}), world:lineOfSight(0, 100, 200, 100, {mask = kWalls}))
```

### world:raycastBatch(batch, filter)

Casts every ray of a [`RayBatch`](#raybatch) and stores the closest hit of each ray in the batch. The rays spread over the workers of the job system of the engine. Thousands of rays per frame, such as the sensors of many agents or the lines of a lidar effect, cost a fraction of separate `world:raycast` calls. Once the batch has grown, it reuses its buffers.

The filter takes `category`, `mask` and `group`. An `accept` function raises `Ray batches take no "accept" function, because their rays run on worker threads.`. The batch keeps the world alive for `batch:shape` and `batch:body`.

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

### world:predictPath(x, y, vx, vy, options)

Predicts the flight of a body thrown from `x, y` with the velocity `vx, vy`. It returns two values: the list of points the body passes, as `Vec2` values, and the [hit](#hit-tables) of the first shape it would reach, or `nil`.

The path follows the solver of the world. It advances in the sub-steps of the world, with its gravity and its speed limit, and with the gravity scale and the linear damping of the options. So the aim of a slingshot matches the real throw of a body with the same settings. It keeps one point per step, starting at the launch position. Between the points it sweeps a circle of `radius`, or a ray when the radius is 0. The path ends at the hit. There, the last point is the center of the circle as it touches, or the hit point of the ray. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `steps` | integer | `120` | Steps to predict, two seconds at the default fixed rate. |
| `step` | number | `haylen.fixedStep()` | Length of one step in seconds. |
| `radius` | number | `0` | Radius of the swept circle, usually the radius of the thrown body. |
| `gravityScale` | number | `1` | Gravity scale of the thrown body. |
| `linearDamping` | number | `0` | Linear damping of the thrown body. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | The shapes the path hits, as in a [ray cast filter](#ray-cast-filters). |

A step that is not positive, fewer than one step, or a negative radius or damping raise `A path prediction needs a positive step, at least one step, and a radius and damping of zero or more.`. A swept circle that starts touching a shape hits it at once. So leave the thrown body out of the mask, as the example does, or predict before the body exists.

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

### physics2d.newRayBatch(count)

Creates a [`RayBatch`](#raybatch) of `count` rays, 0 by default. Each ray has zero length until `batch:setRay` sets it.

```lua
local physics2d = require('haylen.physics2d')

local sensors = physics2d.newRayBatch(64)
print(sensors.size) -- 64
```

### physics2d.drawRay(x1, y1, x2, y2, hit, order)

Draws one ray on the current canvas right away. When `hit` is a hit table of any ray cast, the ray stops at the hit, with a dot on it and a short line along its normal. The hit may come from this module, from [`haylen.math`](math.md#ray-casts), from [`haylen.spatial2d`](spatial2d.md) or from [`haylen.tiled`](tiled.md). A `nil` or `false` hit draws the whole ray. The optional table `order` takes the `layer`, `depth` and `blend` fields of [`haylen.graphics2d`](graphics2d.md).

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

### world:debugDrawRays(order)

Draws the casts that the world recorded since the last call while `world.debugRays` was `true`, and clears them. It draws each ray up to its hit or its end, with a dot on the hit and a short line along its normal. Shape casts draw the path of their center. Call it from the `render` callback of a scene after `graphics2d.beginWorld()`. The optional table `order` takes the `layer`, `depth` and `blend` fields of [`haylen.graphics2d`](graphics2d.md).

The world keeps the casts of one frame at most. The first cast of a new frame drops the casts that no call drew. So recording without drawing never piles up casts.

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

## RayBatch

A `RayBatch`, named `haylen.RayBatch` in messages, holds rays that [`world:raycastBatch()`](#worldraycastbatchbatch-filter) casts together, with a result slot for each ray. The function `physics2d.newRayBatch` makes one. Rays count from 1. An index outside the batch raises a bad argument error with `the ray index is outside the batch`. The batch keeps its buffers, so casting the same number of rays again reuses them, and reading the results creates no tables.

### batch:setRay(index, x1, y1, x2, y2)

Sets the ray at `index` to run from `x1, y1` to `x2, y2`.

```lua
local physics2d = require('haylen.physics2d')

local batch = physics2d.newRayBatch(2)
batch:setRay(1, 0, 0, 300, 0)
batch:setRay(2, 0, 0, 0, 300)
```

### batch:ray(index)

Returns the ray at `index` as four values: `x1`, `y1`, `x2` and `y2`.

```lua
local physics2d = require('haylen.physics2d')

local batch = physics2d.newRayBatch(1)
batch:setRay(1, 10, 20, 300, 20)
local x1, y1, x2, y2 = batch:ray(1)
print(x1, y1, x2, y2) -- 10.0 20.0 300.0 20.0
```

### batch:hit(index)

Returns `false` when the ray at `index` hit nothing in the last cast. Otherwise it returns six values: `true`, `x`, `y`, `normalX`, `normalY` and `fraction`.

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

### batch:shape(index)

Returns the shape that the ray at `index` hit in the last cast, or `nil`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local rock = world:createBody({type = 'static', x = 150, y = 0})
rock:addCircle(30)
local batch = physics2d.newRayBatch(1)
batch:setRay(1, 0, 0, 300, 0)
world:raycastBatch(batch)
print(batch:shape(1).kind) -- circle
```

### batch:body(index)

Returns the body that the ray at `index` hit in the last cast, or `nil`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local rock = world:createBody({type = 'static', x = 150, y = 0})
rock:addCircle(30)
local batch = physics2d.newRayBatch(1)
batch:setRay(1, 0, 0, 300, 0)
world:raycastBatch(batch)
print(batch:body(1) == rock) -- true
```

### RayBatch properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read and write | Number of rays. Growing the batch adds rays of zero length and keeps the existing ones. |

```lua
local physics2d = require('haylen.physics2d')

local batch = physics2d.newRayBatch(4)
batch.size = 8
print(batch.size) -- 8
```

## RayDebugDraw

A `RayDebugDraw`, named `haylen.RayDebugDraw` in messages, records the casts of a world for [`world:debugDrawRays()`](#worlddebugdrawraysorder). A world creates one inside itself when `world.debugRays` becomes `true`, and drops it when it becomes `false`. The app never holds one, and the type has no members of its own. Turn recording on and off through the world.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
world.debugRays = true
world:raycast(0, 0, 300, 0)
world.debugRays = false
```

## Contacts and events

Set a function on a callback property of the world to receive its events. Every callback runs inside `world:step()` for the events of that step, in this order: contact begins, contact ends, hits, sensor begins, sensor ends and joint breaks. The world gathers the events of a step before the first callback runs. So a callback may destroy bodies, or even step the world again, and every event of the step still arrives.

Body and shape handles passed to callbacks are new userdata every time. So compare them with `==`, and keep per-body state in `body.data` instead of using handles as table keys.

Shapes decide which events they raise with the switches of the [shape options](#shape-options). A contact calls `onContactBegin` and `onContactEnd` when either of its shapes has `contactEvents` on, and `onHit` when either has `hitEvents` on. So quiet shapes, such as the debris of an explosion, turn them off on both sides. A sensor reports a visitor only when both shapes have `sensorEvents` on.

### Contact tables

The contact tables of `onContactBegin` and `onHit`, and the tables of [`body:contacts()`](#bodycontacts), tell what the contact did in the last step, in world units. The [guide](../2d/physics.md#what-a-contact-tells) explains what each value is good for.

| Field | Type | Meaning |
| --- | --- | --- |
| `x`, `y` | number | The first contact point, or `0, 0` without points. |
| `normalX`, `normalY` | number | The contact normal as a unit vector, pointing from the first shape toward the second. So the second shape was hit from the side the normal points away from, and the first from the side it points to. |
| `speed` | number | How fast the shapes closed in along the normal before the step solved them, the fastest over the points, in world units per second. It is 0 for shapes that rest or move apart. |
| `relativeVelocityX`, `relativeVelocityY` | number | The velocity of the second body relative to the first at the first point after the step, in world units per second. |
| `force` | number | The force that presses the shapes together at the end of the step, in kilograms times world units per second squared. For a body at rest it is `body.mass` times the gravity. |
| `points` | list | The one or two points of the contact. Each is a table with `x`, `y`, `separation`, `normalImpulse`, `tangentImpulse` and `approachSpeed`. |

The fields of each point mean this:

- The field `separation` is the distance between the surfaces there, negative while they overlap.
- The fields `normalImpulse` and `tangentImpulse` are the impulses that pushed the shapes apart and held them along the surface in the last sub-step, in kilograms times world units per second.
- The field `approachSpeed` is how fast the shapes closed in at that point.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
world.onContactBegin = function(a, b, contact)
    -- Damage grows with the speed of the impact, and the normal tells which side took it.
    if contact.speed > 300 then
        print('Hard landing', contact.speed, contact.normalY > 0 and 'on top' or 'from below', #contact.points)
    end
end
```

### world.onContactBegin(a, b, contact)

Called when two solid shapes start touching. The arguments `a` and `b` are the bodies of `contact.shapeA` and `contact.shapeB`. The table `contact` holds `shapeA`, `shapeB` and the fields of a [contact table](#contact-tables), where `speed` is how fast the shapes met.

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

Called when two solid shapes stop touching. The table `contact` carries `shapeA` and `shapeB`. Its point, normal, speed, velocity and force are 0, and its `points` list is empty. When a shape was destroyed during the step, its body argument is `nil` and its shape handle reports `valid == false`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
world.onContactEnd = function(a, b, contact)
    if a and b then
        print('Separated', a.data, b.data)
    end
end
```

### world.onHit(a, b, contact)

Called when two solid shapes hit each other faster than `world.hitThreshold`. That is one meter per second, or `pixelsPerMeter` world units per second, unless the world sets it. The table `contact` has the fields of `onContactBegin`. So it holds the hit point, the normal and `speed`, the approach speed in world units per second. It also holds the points, the velocity and the force of the contact. Use it for impact sounds and damage.

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

Called when a shape starts to overlap a sensor shape. The argument `sensor` is the body of the sensor shape, and `visitor` is the body of the other shape. The table `shapes` has the fields `sensorShape` and `visitorShape`.

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

Called for every joint that the step broke, because its force passed its `breakForce` or its torque passed its `breakTorque`. The handle `joint` already reports `valid == false`. The table `info` holds `bodyA` and `bodyB`, the bodies the joint held, each `nil` when it is gone. It also holds `forceX`, `forceY` and `torque`, the load that broke the joint, in world units.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
world.onJointBreak = function(joint, info)
    print('A joint broke under', math.abs(info.forceY), 'with', info.bodyB and info.bodyB.data)
end
```

## Rope

A `Rope`, named `haylen.Rope` in messages, is a chain of segments joined by revolute joints. The functions `physics2d.newRope` and `physics2d.newBridge` make one.

### physics2d.newRope(world, options)

Builds a chain of segments from `from` to `to`, joined by revolute joints, and returns a `Rope`. An end can hang from a body. An end can also be pinned in place by a static anchor, which the rope creates and destroys with itself. The table `options` is required, and an unknown key raises `Unknown option "<key>".`.

With `limitLength`, on by default, a slack distance joint between the two ends keeps the rope from stretching past its length. A chain of revolute joints alone stretches under a load much heavier than its segments. The limit goes in the next step after any other joint of the rope is destroyed, breaks or goes with its segment. So the pieces of a cut rope fall apart.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `from`, `to` | Vec2 | `{0, 0}` | Ends of the rope in world units. |
| `segments` | integer | `10` | Number of segments. |
| `thickness` | number | `4` | Width of each segment. |
| `density`, `friction` | number | `1`, `0.6` | Material of the segments. |
| `linearDamping`, `angularDamping` | number | `0`, `0.5` | Damping of the segments, which calms the swing. |
| `planks` | boolean | `false` | Makes the segments boxes instead of capsules. |
| `pinStart`, `pinEnd` | boolean | `false` | Pins an end that has no body to a static anchor. |
| `limitLength` | boolean | `true` | Adds the slack joint that keeps the rope from stretching past its length while the rope is whole. |
| `startBody`, `endBody` | Body | `nil` | Hangs an end from a body, at the end point. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | Collision filter of the segments. |

These errors can happen, and a rope that fails leaves none of its bodies in the world:

- A rope without segments, with both ends in the same place or without thickness raises `A rope needs at least one segment, two distinct ends and a positive thickness.`.
- An invalid material or damping raises the errors of the [shape options](#shape-options) and of [`world:createBody`](#worldcreatebodyoptions).
- An end body that is destroyed or belongs to another world raises `A joint needs two bodies of this world.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local lamp = world:createBody({x = 0, y = 150})
lamp:addCircle(12, {density = 20})
local rope = physics2d.newRope(world, {from = {0, 0}, to = {0, 150}, segments = 12, pinStart = true, endBody = lamp})
print(#rope:bodies(), #rope:joints()) -- 12 14
```

### physics2d.newBridge(world, options)

Builds a rope of planks with both ends pinned, unless the options give bodies for them, which makes a rope bridge. It takes the options of `physics2d.newRope`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local bridge = physics2d.newBridge(world, {from = {-200, 100}, to = {200, 100}, segments = 16, thickness = 8})
print(#bridge:joints()) -- 18
```

### rope:bodies()

Returns the list of the segment bodies, from the start of the rope to its end.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local rope = physics2d.newRope(world, {from = {0, 0}, to = {200, 0}, segments = 8, pinStart = true})
for _, segment in ipairs(rope:bodies()) do
    segment.data = {rope = true}
end
```

### rope:joints()

Returns the list of the joints of the rope: the joints between segments, then the joints that hang the ends, and the length limit last. Destroying a joint of the list cuts the rope there, and the length limit goes in the next step.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local rope = physics2d.newRope(world, {from = {0, 0}, to = {0, 200}, segments = 10, pinStart = true})
-- Cuts the rope in the middle.
rope:joints()[5]:destroy()
```

### rope:segments()

Returns a table `{x, y, rotation, length}` for each segment, which places a sprite at its center.

```lua
local physics2d = require('haylen.physics2d')
local graphics = require('haylen.graphics')
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
        for _, segment in ipairs(rope:segments()) do
            graphics2d.draw(graphics.whiteTexture(), segment.x, segment.y, {width = segment.length, height = 4, rotation = segment.rotation, color = '#FF8D6E63'})
        end
    end,
})
```

### rope:points()

Returns the ends of the segments in order, as `Vec2` values, which draw the rope as a line.

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

### rope:destroy()

Removes the segments, their joints and the anchors of the rope.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local rope = physics2d.newRope(world, {from = {0, 0}, to = {0, 120}, pinStart = true})
rope:destroy()
print(rope.valid) -- false
```

### Rope properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | boolean | read | The value is `false` once a segment was destroyed. |
| `segmentLength` | number | read | Length of each segment. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local rope = physics2d.newRope(world, {from = {0, 0}, to = {0, 200}, segments = 10})
print(rope.segmentLength, rope.valid) -- 20.0 true
```

## Ragdoll

A `Ragdoll`, named `haylen.Ragdoll` in messages, is a human figure seen from the side. The function `physics2d.newRagdoll` makes one.

### physics2d.newRagdoll(world, options)

Builds a figure of eleven capsules joined by revolute joints, with the angle limits of real joints, and returns a `Ragdoll`. Its parts share a negative collision group, so they never collide with each other. They collide with the rest of the world through the category and mask of the options. Two figures with the same group pass through each other, so figures that should collide take a negative group each.

Every joint resists bending with a motor that aims at no motion, whose torque comes from `stiffness`. At 1, a joint holds the limb below it straight out against standard gravity, 9.8 meters per second squared, whatever the height of the figure and the scale of the world. At 0, the figure is limp. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Center of the hips. |
| `height` | number | `128` | Height from the top of the head to the feet. |
| `density`, `friction` | number | `1`, `0.6` | Material of the parts. |
| `stiffness` | number | `0.2` | How much the joints resist bending, from 0 to 1. |
| `category`, `mask` | integer | `1`, all bits | Collision filter of the parts against the rest of the world. |
| `group` | integer | `-1` | Negative collision group of the parts. |
| `vx`, `vy` | number | `0` | Starting velocity of every part. |

A height that is not positive, a group that is not negative or a stiffness outside 0 to 1 raise `A ragdoll needs a positive height, a negative collision group and a stiffness from 0 to 1.`. An invalid material raises the error of the [shape options](#shape-options), without leaving any part in the world.

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

### ragdoll:body(part)

Returns the body of one part. The parts are `head`, `chest`, `hips`, `upperArmLeft`, `lowerArmLeft`, `upperArmRight`, `lowerArmRight`, `upperLegLeft`, `lowerLegLeft`, `upperLegRight` and `lowerLegRight`. Another name raises a bad argument error with `unknown ragdoll part`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local doll = physics2d.newRagdoll(world, {x = 0, y = 0})
doll:body('lowerArmRight'):applyImpulse(300, 0)
```

### ragdoll:bodies()

Returns a table of every body of the figure, keyed by part name.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local doll = physics2d.newRagdoll(world)
for name, body in pairs(doll:bodies()) do
    body.data = {part = name}
end
```

### ragdoll:joints()

Returns the list of the joints of the figure.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local doll = physics2d.newRagdoll(world)
print(#doll:joints()) -- 10
```

### ragdoll:destroy()

Removes the figure with its parts and joints.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local doll = physics2d.newRagdoll(world)
doll:destroy()
print(doll.valid) -- false
```

### Ragdoll properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | boolean | read | The value is `false` once a part was destroyed. |
| `mass` | number | read | The mass of the parts that are left, in kilograms. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local doll = physics2d.newRagdoll(world, {height = 160, density = 2})
print(doll.mass, doll.valid)
```

## Vehicle

A `Vehicle`, named `haylen.Vehicle` in messages, is a car seen from the side. The function `physics2d.newVehicle` makes one. The [guide](../2d/physics.md#vehicles-that-flip) explains why it does not flip.

### physics2d.newVehicle(world, options)

Builds a car and returns a `Vehicle`. It is a box chassis on two wheels, held by wheel joints on springy suspension along the vertical axis of the chassis. The motors size their torque from the mass of the car and the acceleration that the throttle asks for. So the car speeds up, brakes and coasts without lifting its nose or flipping. The center of mass sits at the height of the axles, the wheels spin as fast as they are driven, and an anti-roll coupling between the axles resists pitching. The throttle turns the car in the air.

The parts share a negative collision group, so the wheels never hit the chassis. The front of the car is its positive x side. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Center of the chassis. |
| `chassisWidth`, `chassisHeight` | number | `120`, `30` | Size of the chassis. |
| `wheelRadius` | number | `16` | Radius of both wheels. |
| `rearWheel`, `frontWheel` | Vec2 | `{-40, 20}`, `{40, 20}` | Wheel positions relative to the center of the chassis. |
| `density`, `wheelDensity`, `wheelFriction` | number | `2`, `1`, `0.9` | Materials of the chassis and the wheels. |
| `suspensionHertz`, `suspensionDamping` | number | `5`, `0.7` | Stiffness and damping of the springs. |
| `suspensionTravel` | number | `10` | How far the wheels move up and down from where they start. |
| `acceleration` | number | `600` | Acceleration of full throttle in units per second squared. |
| `topSpeed` | number | `900` | Speed of the rims of the driven wheels at full throttle, in units per second. |
| `brakeAcceleration` | number | `1600` | Deceleration of the full brake in units per second squared. |
| `centerOfMass` | Vec2 | The height of the axles | Center of mass of the chassis, relative to its center. |
| `airControl` | number | `6` | Angular acceleration in radians per second squared that full throttle gives the chassis while no wheel touches the ground. |
| `antiRoll` | number | `0.5` | How much the springs of the two axles push against each other, from 0 to 1. It keeps the chassis level when it speeds up and slows down. |
| `drive` | string | `'rear'` | Driven wheels: `'rear'`, `'front'` or `'all'`. |
| `category`, `mask`, `group` | integer | `1`, all bits, `-2` | Collision filter of the parts, with a negative group. |
| `bullet` | boolean | `false` | Sweeps the chassis and the wheels against moving bodies too. |

Bad options raise `A vehicle needs a chassis and wheels with a size, a suspension travel of zero or more, a negative collision group, a positive acceleration, top speed and brake, air control of zero or more and an anti-roll from 0 to 1.`. These options are bad:

- A chassis or a wheel without size.
- A negative suspension travel.
- A group that is not negative.
- An acceleration, a top speed or a brake that is not positive.
- A negative air control.
- An anti-roll outside 0 to 1.

An unknown `drive` raises a bad argument error with `unknown value '<name>'`. An invalid material raises the error of the [shape options](#shape-options), without leaving any part in the world.

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

### vehicle:joints()

Returns the two wheel joints, rear first.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local vehicle = physics2d.newVehicle(world)
for _, suspension in ipairs(vehicle:joints()) do
    suspension.hertz = 3
end
```

### vehicle:destroy()

Removes the car with its chassis, wheels and joints.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local vehicle = physics2d.newVehicle(world)
vehicle:destroy()
print(vehicle.valid) -- false
```

### Vehicle properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `throttle` | number | read and write | From -1, full reverse, to 1, full ahead. At 0 the car rolls. Values outside are clamped. |
| `brake` | number | read and write | From 0 to 1, the share of the full brake that holds the wheels. Values outside are clamped. |
| `grounded` | boolean | read | Whether a wheel touched the ground in the last step. |
| `speed` | number | read | Speed of the chassis along its own axis in units per second, positive toward its front. |
| `chassis`, `rearWheel`, `frontWheel` | Body | read | The bodies of the car. |
| `drive` | string | read | The driven wheels. |
| `valid` | boolean | read | The value is `false` once a part was destroyed. |

Every member except `drive`, `valid` and `destroy` raises `The vehicle was destroyed.` once a part is gone.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local vehicle = physics2d.newVehicle(world, {drive = 'front'})
vehicle.throttle = 2
print(vehicle.throttle, vehicle.drive, vehicle.grounded, vehicle.chassis.mass)
```

## TopDownVehicle

A `TopDownVehicle`, named `haylen.TopDownVehicle` in messages, is a car seen from above. The function `physics2d.newTopDownVehicle` makes one.

### physics2d.newTopDownVehicle(world, options)

Builds a car and returns a `TopDownVehicle`. It is a box body with a front and a rear axle whose tires grip sideways up to a limit. So it steers, drifts when a turn asks for more grip than the tires have, skids on the handbrake, and slows down by rolling drag. It faces its positive x axis at rotation 0, and it suits a world without gravity.

Steering turns the front tires at a limited speed. The steering lock shrinks toward `highSpeedLock` of itself at the top speed, so the car stays easy to drive when it is fast. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

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
| `rollingDrag` | number | `0.4` | Share of its speed that the car loses every second without throttle. |
| `angularDamping` | number | `2` | Angular damping of the body. |
| `drive` | string | `'rear'` | Driven axles: `'rear'`, `'front'` or `'all'`. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | Collision filter of the body. |
| `bullet` | boolean | `false` | Sweeps the body against moving bodies too. |

Bad options raise `A top-down vehicle needs a size, a front axle ahead of the rear one, positive accelerations, speeds, brake, grip and steering speed, a steering lock and rolling drag of zero or more, and a handbrake grip and high speed lock from 0 to 1.`. These options are bad:

- A car without size.
- An acceleration, a speed, a brake, a grip or a steering speed that is not positive.
- A negative steering lock or rolling drag.
- A handbrake grip or a high speed lock outside 0 to 1.
- A front axle that is not ahead of the rear one.

An unknown `drive` raises a bad argument error with `unknown value '<name>'`. An invalid material raises the error of the [shape options](#shape-options), without leaving the body in the world.

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

### car:destroy()

Removes the car and its body.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local car = physics2d.newTopDownVehicle(world)
car:destroy()
print(car.valid) -- false
```

### TopDownVehicle properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `throttle` | number | read and write | From -1, full reverse, to 1, full ahead. Values outside are clamped, and a throttle wakes the car. |
| `steering` | number | read and write | From -1, full left as seen from the driver, to 1, full right. Values outside are clamped. |
| `brake` | number | read and write | From 0 to 1, the share of the full brake. Values outside are clamped. |
| `handbrake` | boolean | read and write | Whether the handbrake loosens the rear tires. |
| `steeringAngle` | number | read | The angle of the front tires, in radians. |
| `speed` | number | read | Speed along the car in units per second, positive forward. |
| `slip` | number | read | Sideways speed of the rear axle in units per second, which grows while the car drifts. |
| `drifting` | boolean | read | Whether a tire slid in the last step, because the turn asked for more grip than it has. |
| `body` | Body | read | The body of the car. |
| `valid` | boolean | read | The value is `false` once the body was destroyed. |

Every member except `valid` and `destroy` raises `The vehicle was destroyed.` once the body is gone.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local car = physics2d.newTopDownVehicle(world)
car.steering = -0.5
car.handbrake = true
print(car.steering, car.handbrake, car.speed, car.slip, car.drifting)
```

## Mover

A `Mover`, named `haylen.Mover` in messages, is a kinematic character. The function `physics2d.newMover` makes one. The [guide](../2d/physics.md#characters-on-slopes-and-steps) explains why a mover makes a better character than a dynamic body.

### physics2d.newMover(world, options)

Creates a `Mover`: an upright capsule that moves by the distances the app asks for, and slides along what it hits, without the solver pushing it around. It does all of this:

- It stands still on slopes up to `maxSlope` and keeps its speed along them.
- It steps onto ledges up to `stepHeight`.
- It stays on the ground over crests and down slopes within `snapDistance`.
- It rides the bodies it stands on.
- It stops at ceilings and walls.
- It passes [one-way platforms](#one-way-platforms) from their closed side and lands on their open side, like bodies do.
- It pushes the dynamic bodies it walks into.

The capsule that collides covers the mover above its step height. A ray under it finds the ground, so steps rise under it smoothly. A kinematic body of that capsule follows the mover, so other bodies collide with it too. The mover passes through sensors. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Center of the mover. |
| `radius` | number | `12` | Radius of the capsule. |
| `height` | number | `48` | Height from the feet to the top of the head. |
| `maxSlope` | number | `0.87` | Steepest ground in radians that the mover stands on and walks up, about 50 degrees. |
| `stepHeight` | number | `0` | Highest ledge the mover steps onto. |
| `snapDistance` | number | `8` | How far below its feet the mover finds ground to stay on, while it does not move up. |
| `pushForce` | number | `20000` | The most force in world units with which the mover pushes the dynamic bodies it walks into, at the height of its middle. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | Collision filter of the mover and of its body. |

These errors can happen:

- A radius of 0.01 meters or less, a negative step height, or a height that leaves less than twice the radius above the step height raise `A mover needs a radius of more than 0.01 meters, a step height of zero or more and a height that leaves twice the radius above the step height.`.
- A negative push force raises `A mover needs a push force of zero or more.`.
- A bad `maxSlope` or `snapDistance` raises the errors of [Mover properties](#mover-properties).

Move the mover once per fixed step, before `world:step()`. Add gravity to its velocity, clip the velocity against what it hit, move by the velocity times the step and clip again, as the example does. Its body follows it by velocity over the step, so what the body touches is pushed instead of shoved apart.

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

### mover:move(dx, dy)

Moves the mover by the distance `dx, dy` in world units, sliding along what it hits, and returns how far it went as a `Vec2`. On walkable ground, a downward part of the distance, such as gravity, holds the mover on the ground instead of sliding it down. The sideways part follows the ground at its full length, and an upward part leaves the ground. The ground the mover stands on carries it by how far the ground moved since the last move.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local floor = world:createBody({type = 'static', x = 0, y = 100})
floor:addBox(800, 20)
local hero = physics2d.newMover(world, {x = 0, y = 60})
local moved = hero:move(40, 10)
print(moved.x, moved.y, hero.grounded)
```

### mover:clip(vx, vy)

Returns the velocity `vx, vy` without what pushes into the ground, the walls and the ceilings of the last move, as a `Vec2`. This stops a fall on landing and a jump at a ceiling. The speed toward dynamic bodies stays, because the mover pushes them.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local floor = world:createBody({type = 'static', x = 0, y = 100})
floor:addBox(800, 20)
local hero = physics2d.newMover(world, {x = 0, y = 60})
hero:move(0, 20)
local velocity = hero:clip(120, 400)
print(velocity.x, velocity.y) -- The fall stops on the floor.
```

### mover:dropThrough(seconds)

Lets the mover pass every one-way platform for `seconds`, 0.2 by default. After that, it still passes the platform it is inside until it leaves it. So the mover drops through the platform it stands on. A negative or infinite time raises `A physics body drops through one-way platforms for a finite time of zero or more.`.

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')

local world = physics2d.newWorld()
local hero = physics2d.newMover(world, {x = 0, y = 0})
if input.keyPressed('down') and hero.grounded then
    hero:dropThrough(0.25)
end
```

### mover:destroy()

Destroys the body of the mover. After that the mover stops moving.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local hero = physics2d.newMover(world)
hero:destroy()
print(hero.valid) -- false
```

### Mover properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `position` | Vec2 | read and write | The center of the mover. Writing takes a `Vec2` or `{x, y}` and teleports the mover and its body. |
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

These errors can happen:

- A slope limit that is negative or not below half pi radians raises `A mover needs a slope limit of zero or more and below half pi radians.`.
- A snap distance that is negative or not finite raises `A mover needs a finite snap distance of zero or more.`.
- Every member except `valid` and `destroy` raises `The mover was destroyed.` once the mover is destroyed.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local hero = physics2d.newMover(world, {x = 100, y = 0, radius = 14, height = 56})
hero.position = {200, 50}
hero.maxSlope = math.rad(40)
print(hero.x, hero.y, hero.radius, hero.height, hero.stepHeight, hero.onWall, hero.groundBody)
```

## Grabber

A `Grabber`, named `haylen.Grabber` in messages, drags dynamic bodies with a mouse, a finger or a cursor. The function `physics2d.newGrabber` makes one. The [guide](../2d/physics.md#dragging-bodies-with-a-mouse-or-a-finger) explains why it pulls better than a mouse joint.

### physics2d.newGrabber(world, options)

Creates a `Grabber`. It takes the body nearest to a point within its pick radius, holds it by the point it took, and pulls that point toward a target with a soft spring. The force of the spring grows with everything joined to the body. So one hand of a ragdoll drags the whole figure, and a heavy chain follows as well as a pebble. A point outside the shapes takes the body by the nearest point of its shape, so the pull never jumps. The grabber never takes sensors, static bodies or kinematic bodies. It owns a static anchor body of its world. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `pickRadius` | number | `0` | Distance in world units around the point within which bodies are taken. |
| `strength` | number | `30` | The pull, as a multiple of the weight under standard gravity, 9.8 meters per second squared, of the held body and every dynamic body joined to it, directly or through others. |
| `hertz`, `dampingRatio` | number | `5`, `0.7` | Stiffness and damping of the spring that pulls. |
| `category`, `mask` | integer | `1`, all bits | The shapes the grabber takes, as in a [query filter](#query-filters). |

A pick radius that is negative or not finite raises `A grabber needs a finite pick radius of zero or more.`. A strength, stiffness or damping that is not positive raises `A grabber needs a positive strength, stiffness and damping ratio.`. Both happen at creation and when the properties change.

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

### grabber:grab(x, y)

Takes the dynamic body nearest to the world point `x, y`, releasing the body it held, and returns it. It returns `nil` when no dynamic body is within the pick radius.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local crate = world:createBody({x = 0, y = 0})
crate:addBox(48, 48)
local grabber = physics2d.newGrabber(world, {pickRadius = 16})
print(grabber:grab(30, 0) == crate) -- true
```

### grabber:moveTo(x, y)

Moves the target that the held point is pulled toward.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local crate = world:createBody({x = 0, y = 0})
crate:addBox(48, 48)
local grabber = physics2d.newGrabber(world)
grabber:grab(0, 0)
grabber:moveTo(200, -100)
print(grabber.target.x, grabber.target.y) -- 200.0 -100.0
```

### grabber:release()

Lets the held body go.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local crate = world:createBody({x = 0, y = 0})
crate:addBox(48, 48)
local grabber = physics2d.newGrabber(world)
grabber:grab(0, 0)
grabber:release()
print(grabber.holding, grabber.force) -- false 0.0
```

### Grabber properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `holding` | boolean | read | Whether the grabber holds a body. |
| `body` | Body or nil | read | The body it holds. |
| `target` | Vec2 | read | The target of the pull. |
| `handle` | Vec2 | read | Where the held point of the body is, which draws the line of the drag. It is the target while nothing is held. |
| `force` | number | read | The most force of the pull in world units, which `grab` sized for the bodies joined to the held one. It is 0 while nothing is held. |
| `pickRadius`, `strength`, `hertz`, `dampingRatio` | number | read and write | The options. A change also applies to a body already held. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local grabber = physics2d.newGrabber(world)
grabber.pickRadius = 32
grabber.strength = 60
print(grabber.holding, grabber.body, grabber.handle.x, grabber.hertz, grabber.dampingRatio)
```

## ForceField

A `ForceField`, named `haylen.ForceField` in messages, is an area of the world that pushes the dynamic bodies inside it. The function `physics2d.newForceField` makes one. The [guide](../2d/physics.md#wind-magnets-planets-and-water) explains each kind of field.

### physics2d.newForceField(world, options)

Creates a `ForceField`. It pushes the bodies inside it every step, before Box2D advances them, in C++ without a call per body.

- A `'radial'` field pulls toward its center, like a magnet, an attractor or the gravity of a planet. A negative strength pushes away.
- A `'directional'` field pushes one way, like wind or a jet of air.
- A `'vortex'` field swirls, clockwise on screen with a positive strength.
- A `'buoyancy'` field is water. It floats each shape by the area it has under the surface, at the center of that area, with the weight of the water it displaces. It also drags the shape toward the flow. So bodies lighter than the water float, and heavier ones sink.

Radial, directional and vortex fields push a body whose center of mass lies in the area. Applying a field never wakes a body, so floating and resting bodies fall asleep. Changing the field wakes the bodies in it. The table `options` is required, and an unknown key raises `Unknown option "<key>".`.

The area is a circle of `radius` around the position when the radius is positive. Otherwise, it is a rectangle of `width` by `height`, centered on the position, when both are positive. Otherwise, it is the polygon of `points` around the position. A buoyancy field needs a rectangle or a convex polygon, whose top is the surface of the water.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | `'radial'` | One of `'radial'`, `'directional'`, `'vortex'` or `'buoyancy'`. |
| `x`, `y` | number | `0` | Position of the field. |
| `radius` | number | `0` | Radius of a circular area. |
| `width`, `height` | number | `0` | Size of a rectangular area. |
| `points` | list of Vec2 | none | Corners of a polygonal area, relative to the position. |
| `strength` | number | `0` | Acceleration in units per second squared, the same for every body. When `acceleration` is `false`, it is a force in world units. |
| `direction` | Vec2 | `{1, 0}` | Direction of a directional field. |
| `falloff` | string | `'none'` | How the strength fades with the distance from the position. With `'linear'` it falls to zero at the radius, or at half the larger side of other areas. With `'inverseSquare'` it is multiplied by the square of `minDistance` over the squared distance, like gravity. |
| `minDistance` | number | `0` | Distance within which an inverse square field keeps its full strength, such as the surface of a planet. |
| `acceleration` | boolean | `true` | Treats `strength` as an acceleration. A field of forces pushes light bodies further than heavy ones. |
| `density` | number | `1` | Density of the water of a buoyancy field, in kilograms per square meter. |
| `linearDrag` | number | `0` | Share of their speed relative to the flow that bodies lose every second. |
| `angularDrag` | number | `0` | Share of their spin that bodies lose every second. |
| `flow` | Vec2 | `{0, 0}` | Velocity that the drag pulls bodies toward, such as a current or the wind of a drag field. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | The bodies the field pushes, chosen like the shapes that would collide with a shape of this filter, as [collision filtering](#collision-filtering) describes. |
| `enabled` | boolean | `true` | Whether the field pushes. |

Bad options raise `A force field needs a circle, a rectangle or a polygon of at least three points, convex for buoyancy, a direction that is not zero, drags, a density and a minimum distance of zero or more, and a minimum distance for an inverse square falloff.`. These options are bad:

- An area without size.
- A polygon of fewer than three points.
- A buoyancy area that is not convex.
- A zero direction.
- A negative drag, density or minimum distance.
- An inverse square falloff without a minimum distance.

An unknown kind or falloff raises a bad argument error with `unknown value '<name>'`.

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

### field:destroy()

Stops the field for good.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local field = physics2d.newForceField(world, {kind = 'vortex', radius = 200, strength = 300})
field:destroy()
print(field.valid) -- false
```

### ForceField properties

| Property | Type | Access | Meaning |
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

These errors can happen:

- A zero direction raises `A force field needs a direction that is not zero.`.
- A negative density raises `A force field needs a density of zero or more.`.
- A negative drag raises `A force field needs drags of zero or more.`.
- Every member except `valid` and `destroy` raises `The force field was destroyed.` once the field is destroyed.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local field = physics2d.newForceField(world, {kind = 'directional', width = 400, height = 400, strength = 600})
field.direction = {0, -1}
field.position = {100, 0}
field.linearDrag = 0.5
print(field.kind, field.bounds.width, field.bodyCount, field.enabled)
```

## Explosions

### physics2d.explode(world, options)

Pushes the dynamic bodies around a point away from it. It returns a list of `{body, x, y, impulseX, impulseY}` tables, with the point each body was pushed at and the impulse it took. Each body takes the impulse, scaled by the falloff over its distance, at the point of its shapes closest to the center. So bodies hit off center spin. The table `options` is required, and an unknown key raises `Unknown option "<key>".`. A radius that is not positive raises `An explosion needs a positive radius.`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Center of the blast. |
| `radius` | number | `100` | Reach of the blast. |
| `impulse` | number | `500` | Impulse at the center. |
| `falloff` | string | `'linear'` | How the impulse fades toward the radius: `'none'`, `'linear'` or `'quadratic'`. |
| `occlusion` | boolean | `false` | Spares the bodies behind other shapes, as seen from the center. Sensors and shapes that the filter skips never shield a body. |
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

## Terrain

A `Terrain`, named `haylen.Terrain` in messages, is destructible ground. The function `physics2d.newTerrain` makes one.

### physics2d.newTerrain(world, options)

Creates a `Terrain`. The ground is stored as a grid of samples from 0, empty, to 1, solid. Bitmaps and polygons fill it, and shapes and explosions carve it. The solid areas become chain loops on one static body per chunk, traced with marching squares. The method `terrain:update()` rebuilds only the chunks that changed.

Sample `(column, row)` lies at `{x, y} + {column, row} * cellSize`. So the terrain covers `(columns - 1) * cellSize` by `(rows - 1) * cellSize`. The terrain owns its bodies and starts empty. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `columns`, `rows` | integer | `257`, `129` | Number of samples on each side. |
| `cellSize` | number | `4` | Distance between samples. |
| `x`, `y` | number | `0` | Position of the first sample. |
| `chunkSize` | integer | `32` | Cells per chunk side. Smaller chunks rebuild faster after a small carve. |
| `simplifyTolerance` | number | `1` | How far the outlines may stray from the traced edge when they are simplified, in world units. |
| `friction`, `restitution` | number | `0.6`, `0` | Material of the ground. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | Collision filter of the ground. |

Fewer than 2 by 2 samples, a cell or chunk size that is not positive or a negative tolerance raise `A terrain needs at least 2 by 2 samples, a positive cell size and chunk size, and a tolerance of at least zero.`. An invalid material raises the error of the [shape options](#shape-options).

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

### terrain.samples

Reading `samples` returns a new list of every sample from 0 to 1, stored row by row, which can shade the ground. Writing it replaces every sample. It takes a list of the same layout, like the alpha channel of an image, or a function of the column and row that returns each sample. A list of another length raises `The terrain needs one value per sample.`. The chunks that change wait for the next `update`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 3, rows = 2})
terrain.samples = {0, 0.5, 1, 1, 1, 1}
print(#terrain.samples) -- 6
```

### terrain:sample(column, row)

Returns the sample at `column, row`, from 0 to 1. Each sample is stored in one byte, so a written value reads back rounded to the nearest of 256 levels. A sample outside the grid raises `The sample is outside the terrain.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 3, rows = 2})
terrain.samples = {0, 0.5, 1, 1, 1, 1}
print(terrain:sample(1, 0)) -- 0.50196081399918
```

### terrain:fill(x, y, radius)

Adds material inside the circle of `radius` at `x, y`, with soft edges, so outlines follow the shape between samples. The chunks the change reaches wait for the next `update`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 129, rows = 65, cellSize = 8})
terrain:fill(512, 256, 80)
terrain:update()
```

### terrain:carve(x, y, radius)

Removes material inside the circle of `radius` at `x, y`, with soft edges. The chunks the change reaches wait for the next `update`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 129, rows = 65, cellSize = 8})
terrain:fillPolygon({{0, 256}, {1024, 256}, {1024, 512}, {0, 512}})
terrain:carve(300, 256, 40)
terrain:update()
```

### terrain:fillPolygon(points)

Adds material inside the polygon of `points`, a list of `Vec2` values or `{x, y}` tables, with soft edges. The chunks the change reaches wait for the next `update`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 129, rows = 65, cellSize = 8})
terrain:fillPolygon({{0, 300}, {500, 200}, {1024, 300}, {1024, 512}, {0, 512}})
terrain:update()
```

### terrain:carvePolygon(points)

Removes material inside the polygon of `points`, with soft edges. The chunks the change reaches wait for the next `update`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 129, rows = 65, cellSize = 8})
terrain:fillPolygon({{0, 256}, {1024, 256}, {1024, 512}, {0, 512}})
terrain:carvePolygon({{500, 250}, {560, 250}, {560, 330}, {500, 330}})
terrain:update()
```

### terrain:explode(x, y, radius, blast)

Carves a crater of `radius` at `x, y` and applies a blast, then returns the hits of the blast, like [`physics2d.explode`](#physics2dexplodeworld-options). The optional table `blast` takes the options of `physics2d.explode`. Its center defaults to the center of the crater, and its radius to twice the radius of the crater.

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

### terrain:update()

Rebuilds the collision of the chunks changed since the last update, and returns how many it rebuilt.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 65, rows = 33, cellSize = 8})
terrain:fill(256, 128, 60)
print(terrain.dirtyChunkCount, terrain:update(), terrain.dirtyChunkCount)
```

### terrain:isSolid(point)

Returns `true` when the point, a `Vec2` or `{x, y}`, lies inside the ground.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 65, rows = 33, cellSize = 8})
terrain:fill(256, 128, 60)
print(terrain:isSolid({256, 128}), terrain:isSolid({20, 20})) -- true false
```

### terrain:outlines()

Returns the collision outlines of every chunk as lists of `Vec2`, which also draw the ground. An outline wound the other way from the outline around it is a cave. Passed whole to [`graphics2d.drawPolygon`](graphics2d.md#graphics2ddrawpolygonpoints-color-order), the outlines fill as one shape, so the chunks join without a seam and the caves stay open.

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
        graphics2d.drawPolygon(terrain:outlines(), '#FF795548')
    end,
})
```

### terrain:bodies()

Returns the static bodies of the chunks that hold ground.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 65, rows = 33, cellSize = 8})
terrain:fill(256, 128, 60)
terrain:update()
for _, body in ipairs(terrain:bodies()) do
    body.data = {ground = true}
end
```

### Terrain properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `columns`, `rows` | integer | read | Number of samples on each side. |
| `cellSize` | number | read | Distance between samples. |
| `samples` | list of numbers | read and write | Every sample, as [`terrain.samples`](#terrainsamples) describes. |
| `bounds` | Rect | read | The area the terrain covers. |
| `chunkCount` | integer | read | Number of chunks. |
| `dirtyChunkCount` | integer | read | Chunks that wait for `update`. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 65, rows = 33, cellSize = 8, chunkSize = 16})
print(terrain.columns, terrain.rows, terrain.cellSize, terrain.bounds.width, terrain.chunkCount) -- 65 33 8.0 512.0 8
```

## Fractures

### physics2d.fracture(body, options)

Replaces a body with one body per piece of its polygon shapes, and returns the pieces. They keep the type, material, filter and motion of the body. The pieces are the Voronoi cells of random points inside the body, which gather around the impact point when there is one.

Cells that meet near the impact point can leave slivers thinner than the tolerance of Box2D, about 0.02 meters. They add nothing to the collision, so they are left out, and a fracture into many pieces never fails on them. A body without polygon shapes, or whose pieces would all be slivers, stays as it is, and the function returns an empty list. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `pieces` | integer | `8` | Number of cells. A value below 1 raises `A fracture needs at least one piece.`. |
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

Splits a shape, given like the shapes of `m.polygon` of [`haylen.math`](math.md), into pieces with the options of `physics2d.fracture`, and returns them, each a list of outlines. The pieces together cover the shape. It suits breaking sprites without physics.

```lua
local physics2d = require('haylen.physics2d')

local pieces = physics2d.splitPolygon({{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {pieces = 6, seed = 3})
print(#pieces)
```

## Fluid

A `Fluid`, named `haylen.Fluid` in messages, is a liquid of particles. The function `physics2d.newFluid` makes one. The [guide](../2d/physics.md#liquids) explains how a fluid behaves and what it costs.

### physics2d.newFluid(world, options)

Creates a `Fluid`. Its particles keep the density of particles half a smoothing radius apart, with the position based density constraints of particle fluids, so the liquid barely compresses under its own weight. They collide with the shapes of the world, and they push the dynamic bodies they hit, so crates float on the liquid or sink by their density.

The particles are not bodies. The fluid keeps them in flat arrays and moves them inside every `world:step()`, after Box2D advances the bodies, with its passes spread over the job system. Some rules follow from this:

- The particles collide with the solid shapes that pass their filter, and with chains from the side the chains collide on.
- Sensors, ray casts and queries never see the particles.
- Collisions follow the path of each particle over the step. So walls thinner than a particle hold it at its top speed and at any step length. A particle that touched a shape stays on the side it touched.
- The density passes move a particle at most 3 meters per second. So an impact squeezes the liquid for a moment instead of throwing it apart.
- A body takes the pushes of the particles only until it moves away from them as fast as they came at it. It also counts how far gravity moved it into them over the step. So a body far lighter than the liquid floats still instead of bouncing off it.
- A push that changes the speed of a sleeping body by less than 0.05 meters per second leaves it asleep. So crates that rest in still water fall asleep.

The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `radius` | number | `4` | Collision radius of the particles. |
| `smoothingRadius` | number | `16` | Distance within which particles interact. It also sets the spacing of `fluid:fill`, the density at which the liquid rests and the size of the metaballs that `fluid:draw` draws. |
| `density` | number | `1` | Kilograms per square meter of the area of a particle, which gives the particles the mass they push bodies with. |
| `friction` | number | `0.1` | How much particles that slide along shapes slow down, from 0 to 1. |
| `restitution` | number | `0` | How much particles bounce off shapes. |
| `viscosity` | number | `0.1` | How much the velocity of each particle blends toward those of its neighbors every step, from 0 to 1, which calms the motion within the liquid. |
| `iterations` | integer | `5` | Density passes in every step. More passes compress the liquid less and cost more. |
| `gravityScale` | number | `1` | Multiplies the world gravity for the particles. |
| `maxSpeed` | number | `3000` | Speed limit of the particles in units per second. |
| `maxParticles` | integer | `8192` | The most particles the fluid holds. |
| `category`, `mask`, `group` | integer | `1`, all bits, `0` | The shapes the particles collide with, chosen like the shapes that would collide with a shape of this filter, as [collision filtering](#collision-filtering) describes. |

Bad options raise `A fluid needs a positive particle radius, a larger smoothing radius, at least one iteration, a viscosity from 0 to 1, and a density, friction, restitution and top speed of zero or more.`. These options are bad:

- A radius that is not positive.
- A smoothing radius that is not larger than the radius.
- Fewer than one iteration.
- A viscosity outside 0 to 1.
- A negative density, friction, restitution or top speed.

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

### fluid:spawn(x, y, vx, vy)

Adds a particle at `x, y` with the optional velocity `vx, vy`, 0 by default. It returns `true`, or `false` when the fluid is full.

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

### fluid:fill(rect, vx, vy)

Fills a rectangle, a `Rect` or `{x, y, width, height}`, with particles half a smoothing radius apart. They are nudged off a perfect grid, so the pressure spreads them. The optional velocity `vx, vy` is 0 by default. It returns how many particles it added before the fluid was full.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local water = physics2d.newFluid(world, {maxParticles = 500})
print(water:fill({0, 0, 400, 400}, 0, 50)) -- 500
```

### fluid:remove(index)

Removes the particle at the position `index`, counted from 1, and moves the last particle into its place. A position below 1 raises a bad argument error with `particles count from 1`. A position past the last particle raises `The fluid has no such particle.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local water = physics2d.newFluid(world)
water:fill({0, 0, 80, 80})
water:remove(1)
print(water.size)
```

### fluid:clear()

Removes every particle.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local water = physics2d.newFluid(world)
water:fill({0, 0, 80, 80})
water:clear()
print(water.size) -- 0
```

### fluid:positions(buffer, first)

Writes the positions of the particles, two numbers per particle, `x` then `y`, and returns what it wrote into.

- A float buffer of [`haylen.collections`](collections.md#float-buffers) receives them from the position `first`, 1 by default. It must hold two values for each particle from there, or the call raises a bad argument error with `the buffer needs two values for each particle`.
- A list receives them from 1, and loses the entries after them.
- Without an argument, the method returns a new list.

Passing the same buffer or list every frame reads the fluid without allocating.

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

### fluid:velocities(buffer, first)

Writes the velocities of the particles, two numbers per particle, like [`fluid:positions`](#fluidpositionsbuffer-first), and returns what it wrote into.

```lua
local collections = require('haylen.collections')
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local water = physics2d.newFluid(world)
water:fill({0, 0, 80, 80}, 30, 0)
local velocities = collections.newFloatBuffer(water.size * 2)
water:velocities(velocities)
print(velocities:get(1)) -- 30.0
```

### fluid:draw(options)

Draws the particles as metaballs on the current canvas, straight from the arrays of the fluid, without a table per particle. It works like [`graphics2d.drawMetaballs`](graphics2d.md#graphics2ddrawmetaballspoints-radius-options). An empty fluid draws nothing. The example of [`physics2d.newFluid`](#physics2dnewfluidworld-options) draws a fluid this way.

The optional table takes `radius`, the radius of each ball, which defaults to 0.6 smoothing radii. It also takes the `color`, `outlineColor`, `outlineWidth` and `threshold` of `drawMetaballs`, and the [draw order](graphics2d.md#draw-order) keys apart from `material`. An unknown key raises `Unknown option "<key>".`, and the values raise the errors of `drawMetaballs`.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local lava = physics2d.newFluid(world, {smoothingRadius = 20})
lava:fill({0, 0, 200, 100})
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        lava:draw({radius = 14, color = '#FFFF7020', layer = 3})
    end,
})
```

### Fluid properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | Number of particles. |
| `radius` | number | read | Radius of the particles. |
| `stepMilliseconds` | number | read | Time the fluid took in the last step, in milliseconds. The [guide](../2d/physics.md#the-physics-benchmark) lists what fluids of several sizes cost. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local water = physics2d.newFluid(world, {radius = 5})
water:fill({0, 0, 80, 80})
world:step(1 / 60)
print(water.size, water.radius, water.stepMilliseconds)
```

## Tiled collision

The method [`map:buildCollision(world)`](tiled.md#mapbuildcollisionworld) of `haylen.tiled` creates static bodies in a world for the collision of a Tiled map, and returns them as a list of `Body` handles. On orthogonal maps the solid shapes of touching tiles merge into chain loops, so bodies slide along them without catching. Tiled shapes can also become sensors and [one-way platforms](#one-way-platforms). Chain loops have no inside, so `world:queryPoint()` finds nothing inside a merged region, and rays hit its outline only from outside. The reference of `map:buildCollision` lists every rule.

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
| `Unknown option "<key>".` | An options or filter table has a key that the call does not take. |
| `unknown value '<name>'` | A body type, joint type, drive, force field kind or falloff is not one of the names this page lists. It comes inside a bad argument error. |
| `The type "haylen.Body" has no member "<name>".` | A member of a body, shape, joint, world or helper does not exist. The type name changes with the userdata. |
| `The type "haylen.Body" has no writable property "<name>".` | A read-only property was written. The type name changes with the userdata. |
| `A physics world needs positive pixels per meter, at least one sub-step and at least one thread.` | The function `physics2d.newWorld()` got a bad scale, sub-step count or thread count. |
| `Too many physics worlds exist at once to create another one.` | The function `physics2d.newWorld()` ran while Box2D holds as many worlds as it can. |
| `A physics world needs at least one sub-step.` | The property `world.subSteps` got fewer than one sub-step. |
| `A physics speed needs to be finite and zero or more.` | A speed of the world, or the sleep threshold of a body, is negative or not finite. |
| `A physics world needs a positive contact stiffness.` | The property `world.contactHertz` got a value that is not positive. |
| `A physics world needs a positive contact damping ratio.` | The property `world.contactDampingRatio` got a value that is not positive. |
| `Body transforms need live bodies of this world.` | The method `world:readTransforms()` or `world:writeTransforms()` got a destroyed body or one of another world. |
| `Body transforms take three floats for each body.` | The buffer of `world:readTransforms()` or `world:writeTransforms()` is too short. |
| `the first position is outside the buffer` | The position `first` of `world:readTransforms()` or `world:writeTransforms()` lies outside the buffer. It comes inside a bad argument error. |
| `Only a world created with interpolation blends the transforms of its bodies.` | The method `world:readTransforms()` asked for interpolated transforms of a world without `interpolate`. |
| `A state hash needs live bodies of this world.` | The method `world:stateHash()` got a destroyed body or one of another world. |
| `A path prediction needs a positive step, at least one step, and a radius and damping of zero or more.` | The method `world:predictPath()` got bad options. |
| `A rectangle query needs a width and height of zero or more.` | The method `world:queryRect()` got a negative width or height. |
| `the radius must be zero or more` | The filter of `world:pick()` has a negative radius. It comes inside a bad argument error. |
| `A circle cast needs a finite radius of zero or more.` | The method `world:castCircle()` got a negative radius. |
| `A box cast needs a positive size.` | The method `world:castBox()` got a size of zero or less. |
| `A capsule cast needs a positive radius.` | The method `world:castCapsule()` got a radius of zero or less. |
| `A polygon cast needs between three and eight points.` | The method `world:castPolygon()` got too few or too many points. |
| `A bouncing physics ray needs a finite length.` | The method `world:bounceRay()` got an infinite length. |
| `the direction must not be zero` | The method `world:bounceRay()` got a zero direction. It comes inside a bad argument error. |
| `the bounce count must not be negative` | The method `world:bounceRay()` got a negative bounce count. It comes inside a bad argument error. |
| `the length must be finite` | The method `world:rayFan()` got an infinite length. It comes inside a bad argument error. |
| `Ray batches take no "accept" function, because their rays run on worker threads.` | The method `world:raycastBatch()` got a filter with `accept`. |
| `the ray index is outside the batch` | A method of a ray batch got an index outside the batch. It comes inside a bad argument error. |
| `The body was destroyed.` | A destroyed body was used. |
| `A body moves only to another world.` | The method `body:moveToWorld()` got the world the body is in. |
| `A physics body needs a finite damping of zero or more.` | A body option or property got a negative or infinite damping. |
| `A physics body needs a finite mass, center of mass and inertia of zero or more.` | The property `body.mass`, `body.inertia` or `body.centerOfMass` got a negative or infinite value. |
| `A physics body moves to a target over a positive time.` | The method `body:moveTo()` got a time that is not positive. |
| `A physics body drops through one-way platforms for a finite time of zero or more.` | The method `body:dropThrough()` or `mover:dropThrough()` got a negative or infinite time. |
| `A physics shape needs a finite density, friction, restitution and rolling resistance of zero or more.` | The shape options or properties, or the material options of a rope, ragdoll, vehicle or terrain, are negative or not finite. |
| `A one-way direction cannot be zero.` | The property `shape.oneWay` or the option `oneWay` got a zero vector. |
| `A physics box needs a positive size.` | The method `body:addBox()` got a size of zero or less. |
| `A physics circle needs a positive radius.` | The method `body:addCircle()` got a radius of zero or less. |
| `A physics capsule needs a positive radius.` | The method `body:addCapsule()` got a radius of zero or less. |
| `A physics segment needs ends more than 0.005 meters apart.` | The method `body:addSegment()` got ends in about the same place. |
| `A physics polygon needs at least three points.` | The method `body:addPolygon()` got fewer than three points. |
| `A physics polygon needs at least three points that are not on one line.` | The method `body:addPolygon()` got a convex outline without area. |
| `A physics polygon needs a non-degenerate outline.` | The method `body:addPolygon()` got an outline whose pieces have no area. |
| `A physics chain needs at least four points for a loop and two for an open chain.` | The method `body:addChain()` got too few points. |
| `The shape was destroyed.` | A destroyed shape was used. |
| `Only sensor shapes have overlaps.` | The method `shape:overlaps()` ran on a shape that is not a sensor. |
| `A joint needs two bodies of this world.` | A body of a joint, or an end body of a rope, is destroyed or belongs to another world. |
| `A distance joint needs a length of at least 0.005 meters.` | A distance joint has no `length` and anchors in about the same place, or a `length` that is too short. |
| `A revolute joint needs a lower limit that is not above the upper one, both within 0.99 pi radians of zero.` | A revolute joint got limits in the wrong order or beyond 0.99 pi radians. |
| `A prismatic, wheel or distance joint needs a lower limit that is not above the upper one.` | The method `world:createJoint()` got limits in the wrong order. |
| `A joint needs a lower limit that is not above the upper one.` | The property `joint.lower` or `joint.upper` got a limit past the other one. |
| `A joint needs a break force and a break torque of zero or more.` | A break limit is negative. |
| `A joint needs a positive constraint stiffness.` | The property `joint.constraintHertz` got a value that is not positive. |
| `A joint needs a finite constraint damping ratio of zero or more.` | The property `joint.constraintDampingRatio` got a negative or infinite value. |
| `A joint needs a finite maximum motor force of zero or more.` | The property `joint.maxMotorForce` got a negative or infinite value. |
| `A joint needs a finite maximum motor torque of zero or more.` | The property `joint.maxMotorTorque` got a negative or infinite value. |
| `A joint spring needs a finite stiffness and damping ratio of zero or more.` | The property `joint.hertz` or `joint.dampingRatio` got a negative or infinite value. |
| `Only mouse joints have a target.` and the other messages that start with `Only` | A joint property was used on a joint type that lacks it, as [Joint properties](#joint-properties) lists. |
| `The physics joint was destroyed.` | A destroyed joint was used. |
| `A rope needs at least one segment, two distinct ends and a positive thickness.` | The function `physics2d.newRope()` or `physics2d.newBridge()` got bad options. |
| `A ragdoll needs a positive height, a negative collision group and a stiffness from 0 to 1.` | The function `physics2d.newRagdoll()` got bad options. |
| `unknown ragdoll part` | The method `ragdoll:body()` got an unknown part name. It comes inside a bad argument error. |
| `A vehicle needs a chassis and wheels with a size, a suspension travel of zero or more, a negative collision group, a positive acceleration, top speed and brake, air control of zero or more and an anti-roll from 0 to 1.` | The function `physics2d.newVehicle()` got bad options. |
| `A top-down vehicle needs a size, a front axle ahead of the rear one, positive accelerations, speeds, brake, grip and steering speed, a steering lock and rolling drag of zero or more, and a handbrake grip and high speed lock from 0 to 1.` | The function `physics2d.newTopDownVehicle()` got bad options. |
| `The vehicle was destroyed.` | A vehicle whose bodies are gone was used. |
| `A mover needs a radius of more than 0.01 meters, a step height of zero or more and a height that leaves twice the radius above the step height.` | The function `physics2d.newMover()` got a bad size. |
| `A mover needs a push force of zero or more.` | The function `physics2d.newMover()` got a negative push force. |
| `A mover needs a slope limit of zero or more and below half pi radians.` | The function `physics2d.newMover()` or the property `mover.maxSlope` got a bad slope limit. |
| `A mover needs a finite snap distance of zero or more.` | The function `physics2d.newMover()` or the property `mover.snapDistance` got a bad snap distance. |
| `The mover was destroyed.` | A destroyed mover was used. |
| `A grabber needs a finite pick radius of zero or more.` | The function `physics2d.newGrabber()` or the property `grabber.pickRadius` got a negative or infinite radius. |
| `A grabber needs a positive strength, stiffness and damping ratio.` | The function `physics2d.newGrabber()` or a property of the grabber got a value that is not positive. |
| `A force field needs a circle, a rectangle or a polygon of at least three points, convex for buoyancy, a direction that is not zero, drags, a density and a minimum distance of zero or more, and a minimum distance for an inverse square falloff.` | The function `physics2d.newForceField()` got bad options. |
| `A force field needs a direction that is not zero.` | The property `field.direction` got a zero vector. |
| `A force field needs a density of zero or more.` | The property `field.density` got a negative value. |
| `A force field needs drags of zero or more.` | The property `field.linearDrag` or `field.angularDrag` got a negative value. |
| `The force field was destroyed.` | A destroyed force field was used. |
| `An explosion needs a positive radius.` | The function `physics2d.explode()` or the method `terrain:explode()` got a radius that is not positive. |
| `A terrain needs at least 2 by 2 samples, a positive cell size and chunk size, and a tolerance of at least zero.` | The function `physics2d.newTerrain()` got bad options. |
| `The terrain needs one value per sample.` | The property `terrain.samples` got a list of another length. |
| `The sample is outside the terrain.` | The method `terrain:sample()` got a column or row outside the grid. |
| `A fracture needs at least one piece.` | The function `physics2d.fracture()` or `physics2d.splitPolygon()` got fewer than one piece. |
| `A fluid needs a positive particle radius, a larger smoothing radius, at least one iteration, a viscosity from 0 to 1, and a density, friction, restitution and top speed of zero or more.` | The function `physics2d.newFluid()` got bad options. |
| `particles count from 1` | The method `fluid:remove()` got a position below 1. It comes inside a bad argument error. |
| `The fluid has no such particle.` | The method `fluid:remove()` got a position past the last particle. |
| `the buffer needs two values for each particle` | The float buffer of `fluid:positions()` or `fluid:velocities()` is too short. It comes inside a bad argument error. |
