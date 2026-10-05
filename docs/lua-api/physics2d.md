# haylen.physics2d

Rigid body physics built on Box2D. Use it for anything that must collide, bounce, stack, hang from joints or be found with ray casts, shape casts and area queries, from platformer characters to falling crates and trigger zones. On top of bodies and joints it builds ropes, bridges, ragdolls and vehicles, one-way platforms and conveyors, explosions, destructible terrain, fractures and particle fluids.

```lua
local physics2d = require('haylen.physics2d')
```

## Units and coordinates

Positions, sizes, distances and velocities are in world units, the same units the camera and sprites use, with y pointing down. Angles are in radians and angular velocities in radians per second. The world converts world units to meters for Box2D with its `pixelsPerMeter` setting, so a box 64 units wide is one meter wide in a world with the default scale.

Mass comes from the shape density and the shape area in square meters, so `body.mass` is in kilograms. Every other quantity uses world units and kilograms, in and out:

| Quantity | Unit | Example |
| --- | --- | --- |
| Force | kilograms times units per second squared | The call `body:applyForce(0, -body.mass * 600)` accelerates the body upward at 600 units per second squared. |
| Linear impulse | kilograms times units per second | The call `body:applyImpulse(body.mass * 100, 0)` changes the horizontal velocity by 100 units per second. |
| Torque | kilograms times squared units per second squared | A disc of radius `r` has the rotational inertia `body.mass * r * r / 2`, and a torque of that inertia times 3 speeds its spin up by 3 radians per second every second. |
| Angular impulse | kilograms times squared units per second | An angular impulse of the disc's inertia times 2 changes its spin by 2 radians per second at once. |
| `maxMotorForce` | same as a force | Limits prismatic motors, mouse joints and motor joints. |
| `maxMotorTorque` | same as a torque | Limits revolute motors, wheel motors and motor joints. |

So the same numbers give the same motion at every `pixelsPerMeter`, which only decides how large a meter is for the solver's tuning and for the mass of a shape.

## Stepping the world

A world only moves when `world:step()` runs. Step it from the `fixedUpdate` callback of a scene, which the engine calls at the fixed rate set by `fixedRate` in `app.json` (60 times per second by default). Contact and sensor callbacks run inside `world:step()`, right after the simulation advances.

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

## Functions

### physics2d.newWorld(options)

Creates an empty world and returns it. The options table is optional, and unknown keys raise `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `gravity` | Vec2 or `{x, y}` | `{0, 980}` | Gravity in world units per second squared. |
| `pixelsPerMeter` | number | `64` | World units in one Box2D meter. It must be positive. |
| `subSteps` | integer | `4` | Solver sub-steps per step. More sub-steps make stacks and joints stiffer at a higher cost. It must be at least 1. |

A scale that is not positive or fewer than one sub-step raises `A physics world needs positive pixels per meter and at least one sub-step.`. Box2D holds a limited number of worlds at once, 128 by default, and a world beyond them raises `Too many physics worlds exist at once to create another one.`. A world goes once no Lua value refers to it or to one of its handles.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 1200}, pixelsPerMeter = 32, subSteps = 8})
local space = physics2d.newWorld({gravity = {0, 0}})
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

## World

The function `physics2d.newWorld()` returns a `haylen.PhysicsWorld` userdata. Bodies, shapes and joints keep their world alive, so a world lives as long as any of its handles.

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
| `bullet` | boolean | `false` | Enables continuous collision against other dynamic bodies, for fast projectiles. |
| `sleepEnabled` | boolean | `true` | Lets the body fall asleep when it comes to rest. |

A damping that is negative or not finite raises `A physics body needs a finite damping of zero or more.`, and so does assigning one to `body.linearDamping` or `body.angularDamping`.

Static bodies never move and have no mass. Kinematic bodies move only by their velocity and push dynamic bodies without being pushed back. Dynamic bodies respond to gravity, forces and collisions.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local floor = world:createBody({type = 'static', x = 0, y = 300})
local platform = world:createBody({type = 'kinematic', x = 0, y = 100, vx = 60})
local arrow = world:createBody({x = -300, y = 0, vx = 900, rotation = 0.1, bullet = true, gravityScale = 0.5})
local player = world:createBody({x = 0, y = 0, fixedRotation = true, linearDamping = 0.2, sleepEnabled = false})
```

### world:createJoint(type, a, b, options)

Connects bodies `a` and `b` of this world with a joint and returns it. Anchors and targets are world positions. The options table is optional, and unknown keys raise `Unknown option "<key>".`. An unknown type raises a bad argument error with `unknown value '<type>'`, and bodies that are destroyed or belong to another world raise `A joint needs two bodies of this world.`.

| Type | Behavior |
| --- | --- |
| `'distance'` | Keeps the anchor `ax, ay` on `a` and the anchor `bx, by` on `b` apart, at their current distance unless `length` is given. It can be springy and limited. |
| `'revolute'` | Pins both bodies at `ax, ay` and lets them rotate around it, like a hinge or a wheel axle. Its angle is 0 in the pose the bodies have when the joint is created. |
| `'prismatic'` | Lets `b` slide relative to `a` along the axis through `ax, ay`, like a piston or an elevator, and keeps the angle between them that they have when the joint is created. |
| `'weld'` | Glues both bodies together at `ax, ay`. |
| `'wheel'` | Lets `b` slide along the axis through `ax, ay`, usually on a spring, and rotate freely, like a car suspension. |
| `'mouse'` | Pulls `b` toward a target that starts at `bx, by`. Body `a` is usually a static body and is not moved. |
| `'motor'` | Drives `b` toward the offset from `a` it had when the joint was created. |
| `'filter'` | Only keeps `a` and `b` from colliding with each other, without holding them together. |

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `ax`, `ay` | number | `0` | Anchor on `a`. Revolute, prismatic, weld and wheel joints use it as the shared anchor. |
| `bx`, `by` | number | `0` | Anchor on `b` for distance joints and the first target of mouse joints. |
| `collideConnected` | boolean | `false` | Lets the two bodies keep colliding with each other. |
| `enableLimit` | boolean | `false` | Enables the `lower` and `upper` limits. |
| `lower`, `upper` | number | `0` | Limits in radians for revolute joints, measured from the pose at creation, translations in world units for prismatic and wheel joints and the minimum and maximum length for distance joints. |
| `enableMotor` | boolean | `false` | Enables the motor of revolute, prismatic and wheel joints. |
| `motorSpeed` | number | `0` | Motor speed in radians per second, or in world units per second for prismatic joints. |
| `maxMotorForce` | number | `0` | Maximum motor force of prismatic joints, maximum force of motor joints and maximum pull of mouse joints, as a force in world units. |
| `maxMotorTorque` | number | `0` | Maximum motor torque of revolute and wheel joints and maximum torque of motor joints, as a torque in world units. |
| `enableSpring` | boolean | `false` | Makes distance, revolute, prismatic and wheel joints springy. |
| `hertz` | number | `0` | Spring stiffness in cycles per second. Weld joints use it for both the linear and the angular spring, and 0 keeps them rigid. |
| `dampingRatio` | number | `0` | Spring damping, where 1 stops the oscillation without overshoot. |
| `axisX`, `axisY` | number | `1`, `0` | Slide axis of prismatic and wheel joints. |
| `length` | number | `0` | Rest length of a distance joint. Zero keeps the distance the anchors have when the joint is created. |

Mouse joints use 4 hertz, a damping ratio of 1 and a pull that accelerates `b` at up to `1000 * pixelsPerMeter` units per second squared when `hertz`, `dampingRatio` or `maxMotorForce` are left at 0. Filter joints ignore every option.

Box2D checks limits even while `enableLimit` is `false`, so the joint is not created when they are out of its range:

- A distance joint shorter than 0.005 meters, which is 0.32 world units at the default 64 pixels per meter, raises `A distance joint needs a length of at least 0.005 meters.`. This happens with the default options, whose anchors are both at `0, 0`, so give a distance joint its anchors or a `length`.
- Revolute limits must lie within 0.99 pi radians of zero with `lower` not above `upper`, or the call raises `A revolute joint needs a lower limit that is not above the upper one, both within 0.99 pi radians of zero.`.
- Prismatic and wheel joints with `lower` above `upper` raise `A prismatic or wheel joint needs a lower limit that is not above the upper one.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ceiling = world:createBody({type = 'static', x = 0, y = -200})
ceiling:addBox(400, 20)

local lamp = world:createBody({x = 0, y = -80})
lamp:addCircle(16)
local rope = world:createJoint('distance', ceiling, lamp, {ax = 0, ay = -200, bx = 0, by = -80, enableSpring = true, hertz = 2, dampingRatio = 0.3})

local door = world:createBody({x = 100, y = -150})
door:addBox(10, 80)
local hinge = world:createJoint('revolute', ceiling, door, {ax = 100, ay = -190, enableLimit = true, lower = -1.2, upper = 1.2})

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
local glue = world:createJoint('weld', chassis, handle, {ax = 0, ay = 80})

local ghost = world:createBody({x = 0, y = 0})
ghost:addBox(20, 20)
world:createJoint('filter', ceiling, ghost)
```

### world:step(deltaSeconds)

Advances the simulation by `deltaSeconds` and then calls the event callbacks for the contacts, hits and sensor overlaps of this step. An error raised by a callback propagates out of `world:step()`. Before it advances, the step also releases the `data` of every body destroyed since the last step, as described in [Body properties](#body-properties).

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

### world:readTransforms(bodies, buffer, first)

Copies `x`, `y` and `rotation` of every body in the list `bodies` into a float buffer of [`haylen.collections`](collections.md#float-buffers), three values for each body in order, from the position `first`, which defaults to 1. Together with `SpriteBatch:writeFields` of [`haylen.graphics2d`](graphics2d.md) it keeps thousands of sprites on their bodies with two calls a frame instead of one property read for each body. Every body must be a live body of the world, and the buffer must hold three values for each one, or the call raises `Body transforms need live bodies of this world.` or `Body transforms take three floats for each body.`.

```lua
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
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
        world:readTransforms(crates, transforms)
        batch:writeFields(transforms, fields)
    end,
    render = function(self)
        graphics2d.beginScreen()
        batch:draw()
    end,
})
```

### world:writeTransforms(bodies, buffer, first)

Moves every body in the list `bodies` to the `x`, `y` and `rotation` the buffer holds for it, three values for each body from the position `first`, like setting its transform one body at a time. It suits kinematic bodies that follow positions an app computes in bulk.

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

### world:raycast(x1, y1, x2, y2, filter)

Casts a ray from `x1, y1` to `x2, y2` and returns the closest hit that passes the filter, or `nil` when the ray hits nothing. The optional filter is described in [Ray cast filters](#ray-cast-filters). Sensor shapes are hit like any other shape, so give them their own category and leave it out of the filter mask to ignore them. A ray that starts inside a shape does not see that shape.

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

Follows a ray from `x, y` in the direction `dx, dy` for `length` world units that reflects off every shape it hits, like a laser between mirrors or a ricocheting bullet, for at most `bounces` reflections. Returns the list of bounce hits in order and the point where the path ends, as three values. The `distance` and `fraction` of each hit measure the whole path up to that bounce. A zero direction, a negative bounce count or a length that is not finite raise an error.

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

Returns the list of shapes under the screen point `x, y`, seen through `camera`, which is how a click or a tap selects bodies. Screen points are in design coordinates, like the pointer positions of [`haylen.input`](input.md). The optional filter takes `category` and `mask`.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld({gravity = {0, 0}})
local crate = world:createBody({x = 0, y = 0})
crate:addBox(60, 60)
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        if input.mousePressed() then
            for _, shape in ipairs(world:pick(camera, input.mousePosition())) do
                print('picked', shape.body == crate)
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

Returns a list of the shapes that contain the point `x, y`. The optional filter is described in [Query filters](#query-filters).

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
    render = function(self)
        graphics2d.beginWorld(self.camera)
        world:debugDraw({layer = 100})
    end,
})
```

### World properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `gravity` | Vec2 | read and write | Gravity in world units per second squared. It accepts a `Vec2` or a table `{x, y}`. |
| `bodyCount` | integer | read | Number of bodies in the world. |
| `pixelsPerMeter` | number | read | The scale the world was created with. |
| `debugRays` | boolean | read and write | Records every ray and shape cast of the world for `world:debugDrawRays()` while `true`. |
| `onContactBegin` | function or nil | read and write | Called when two shapes start touching. |
| `onContactEnd` | function or nil | read and write | Called when two shapes stop touching. |
| `onHit` | function or nil | read and write | Called when two shapes hit each other fast. |
| `onSensorBegin` | function or nil | read and write | Called when a shape enters a sensor. |
| `onSensorEnd` | function or nil | read and write | Called when a shape leaves a sensor. |

Assigning a value other than a function or `nil` to a callback raises an error, and so does reading or writing an unknown property.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
world.gravity = {0, -400}
print(world.gravity.x, world.gravity.y, world.bodyCount, world.pixelsPerMeter)
```

## Events

Assign a function to a callback property of the world to receive its events. Every callback runs inside `world:step()` for the events of that step, in this order: contact begins, contact ends, hits, sensor begins and sensor ends. The events of a step are gathered before the first callback runs, so a callback may destroy bodies or even step the world again and every event of the step still arrives. Body and shape handles passed to callbacks are new userdata every time, so compare them with `==` and keep per-body state in `body.data` instead of using handles as table keys.

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

Called when two solid shapes hit each other faster than the Box2D hit threshold, which is one meter per second, or `pixelsPerMeter` world units per second. The contact table has the fields of `onContactBegin`, with the hit point, the normal and `speed`, the approach speed in world units per second. Use it for impact sounds and damage.

```lua
local physics2d = require('haylen.physics2d')
local assets = require('haylen.assets')
local audio = require('haylen.audio')

local world = physics2d.newWorld()
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
ball:addCircle(8, {restitution = 0.8, density = 0.5})
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

Adds a chain of one-sided segments through the points and returns a list of its segment shapes. Chains suit terrain outlines because bodies slide over their joints smoothly. A `loop` set to `true` closes the outline. An open chain collides from its second point to its next-to-last point, and its first and last points only smooth the contacts at its ends. Chains use `friction`, `restitution`, the collision filter, `offsetX`, `offsetY` and `rotation` from the options and ignore `density` and `sensor`. Fewer than four points raise `A physics chain needs at least four points.`.

Each segment collides only on its left side as seen on screen, walking from one point to the next, and bodies pass through it from the other side. So an open chain listed from left to right carries bodies on top of it, a loop listed counter-clockwise on screen holds bodies inside it, like the walls of an arena, and a loop listed clockwise on screen is solid from the outside, like an island.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local hills = world:createBody({type = 'static', x = 0, y = 0})
hills:addChain({{-600, 200}, {-300, 150}, {0, 220}, {300, 120}, {600, 200}}, false, {friction = 0.8})

-- Down the left wall, along the floor and up the right wall, so the walls face inward.
local arena = world:createBody({type = 'static', x = 0, y = 0})
arena:addChain({{-400, -300}, {-400, 300}, {400, 300}, {400, -300}}, true)

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
| `sensor` | boolean | `false` | Makes the shape a sensor that reports overlaps and never collides. |
| `category` | integer | `1` | Collision category bits of the shape. |
| `mask` | integer | `-1` (all bits) | Categories the shape collides with. |
| `group` | integer | `0` | Collision group, described in [Collision filtering](#collision-filtering). |
| `offsetX`, `offsetY` | number | `0` | Moves the shape away from the body origin. |
| `rotation` | number | `0` | Rotates the shape around the body origin before the offset, in radians. |
| `tangentSpeed` | number | `0` | Turns the surface into a conveyor that carries touching bodies at this speed, described in [Conveyors](#conveyors). |
| `oneWay` | Vec2 | `nil` | Makes the shape a one-way platform, described in [One-way platforms](#one-way-platforms). |

A density, friction or restitution that is negative or not finite raises `A physics shape needs a finite density, friction and restitution of zero or more.`, and a zero `oneWay` direction raises `A one-way direction cannot be zero.`, both before the shape is created.

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

Returns the outline of every shape of the body in world units, where the body is now, as a list of tables with `points`, a list of `Vec2` values, and `closed`, which is `true` when the last point joins back to the first one. Each outline is what [`shape:outline()`](#shapeoutline) returns, except that the segments of each chain join into one outline, closed for a loop. An open chain collides only between its second point and its next-to-last point, since Box2D keeps its first and last points to smooth the contacts at its ends, so its outline runs between those two. Use it to draw bodies made by the engine, such as the pieces of `physics2d.fracture` or the bones of `physics2d.newRagdoll`, without knowing their shapes.

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
})
```

### body:applyTorque(torque)

Applies a torque until the next step and wakes the body. The torque is in kilograms times squared world units per second squared, as described in [Units and coordinates](#units-and-coordinates).

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local wheel = world:createBody()
wheel:addCircle(32)
local inertia = wheel.mass * 32 * 32 / 2
wheel:applyTorque(inertia * 3)
world:step(1)
print(wheel.angularVelocity)
```

### body:applyAngularImpulse(impulse)

Changes the angular velocity at once by the impulse divided by the rotational inertia and wakes the body. The impulse is in kilograms times squared world units per second.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({gravity = {0, 0}})
local coin = world:createBody()
coin:addCircle(8)
coin:applyAngularImpulse(coin.mass * 8 * 8 / 2 * 10)
print(coin.angularVelocity)
```

### body:setTransform(x, y, rotation)

Teleports the body to `x, y` with the given rotation in radians. The rotation defaults to 0, so pass `body.rotation` to keep the current angle.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local player = world:createBody({x = 0, y = 0})
player:addBox(32, 48)
player:setTransform(100, -50, player.rotation)
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
| `position` | Vec2 | read and write | Position as a `Vec2`. Writing accepts a `Vec2` or `{x, y}`. |
| `rotation` | number | read and write | Angle in radians. |
| `velocity` | Vec2 | read and write | Linear velocity in units per second. Writing accepts a `Vec2` or `{x, y}`. |
| `angularVelocity` | number | read and write | Spin in radians per second. |
| `mass` | number | read | Mass in kilograms, computed from the shapes. |
| `awake` | boolean | read and write | Whether the body is awake. Sleeping bodies are skipped by the solver until something touches them, and writing `true` wakes them. |
| `enabled` | boolean | read and write | Disabled bodies leave the simulation and stop colliding until enabled again. |
| `linearDamping` | number | read and write | Slows the linear velocity over time. It must be finite and not negative. |
| `angularDamping` | number | read and write | Slows the spin over time. It must be finite and not negative. |
| `gravityScale` | number | read and write | Multiplies the world gravity for this body. |
| `fixedRotation` | boolean | read and write | Keeps the body from rotating. |
| `bullet` | boolean | read and write | Enables continuous collision against other dynamic bodies. |
| `data` | any | read and write | Any Lua value attached to the body, such as the entity it belongs to. The world keeps it until the body is destroyed: `body:destroy()` releases it at once, and the next `world:step()` releases the data of bodies destroyed any other way, such as by `physics2d.fracture`, the `destroy` of a rope, ragdoll or vehicle, a fluid or a terrain. |

Two handles of the same body compare equal with `==`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local crate = world:createBody({x = 0, y = 0})
crate:addBox(32, 32)
crate.data = {name = 'crate', health = 3}
crate.velocity = {120, 0}
crate.type = 'kinematic'
crate.x = 50
print(crate.position.x, crate.velocity.x, crate.mass, crate.awake, crate.world == world)
```

## Shape

The `add` methods, `body:shapes()`, queries and events return `haylen.Shape` userdata. Every member except `valid` and `destroy` raises `The shape was destroyed.` once the shape is gone.

### shape:destroy()

Removes the shape from its body. Destroying a shape twice does nothing. Chain segments belong to their chain and raise `Chain segments are destroyed together with their body.`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local body = world:createBody()
local shield = body:addCircle(40, {sensor = true})
shield:destroy()
print(shield.valid)
```

### shape:outline()

Returns the outline of the shape in world units, where its body is now, as a table with `points`, a list of `Vec2` values, and `closed`, which is `true` when the last point joins back to the first one. A polygon gives its corners, closed. A circle gives points around it, closed, and a capsule points around its two round ends, closed, which join into its straight sides, one step every two world units of radius and between 16 and 96 steps for a whole turn. A segment or a chain segment gives its two ends, open. The call `graphics2d.drawPolygon(outline.points, color)` fills a closed outline and `graphics2d.drawPolyline(outline.points, thickness, color, outline.closed)` strokes any of them.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local body = world:createBody({x = 100, y = 50})
local ball = body:addCircle(12)
local outline = ball:outline()
print(#outline.points, outline.closed)
```

### Shape properties

The geometry properties return new values on every read. The `points` are in the space of the body, where the offset and rotation of the shape options already apply, and `worldPoints` are the same points in world units where the body is now.

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
| `category` | integer | read and write | Collision category bits. |
| `mask` | integer | read and write | Categories the shape collides with. The default of all bits reads as `-1`. |
| `group` | integer | read and write | Collision group, described in [Collision filtering](#collision-filtering). |
| `tangentSpeed` | number | read and write | Conveyor speed of the surface in world units per second. |
| `oneWay` | Vec2 or nil | read and write | Direction of a one-way platform in the space of the body, or `nil` for a solid shape. A zero vector raises `A one-way direction cannot be zero.` |

Two handles of the same shape compare equal with `==`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local body = world:createBody()
local shape = body:addBox(20, 20)
shape.category = 4
shape.mask = 1 | 2
shape.group = -1
print(shape.body == body, shape.sensor, shape.group, shape.bounds.width)
print(shape.kind, #shape.points, shape.worldPoints[1].x, shape.radius)
```

## Joint

The method `world:createJoint()` returns a `haylen.Joint` userdata.

### joint.target

The target of a mouse joint in world units as a `Vec2`, which writing moves. Writing accepts a `Vec2` or `{x, y}`. Other joint types raise `Only mouse joints have a target.`, and a destroyed joint raises `The physics joint was destroyed.`.

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

The motor speed of a revolute or wheel joint in radians per second, or of a prismatic joint in world units per second, which writing changes. Other joint types raise `Only revolute, prismatic and wheel joints have a motor speed.`, and a destroyed joint raises `The physics joint was destroyed.`.

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

### Joint properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | boolean | read | The value is `false` once the joint or one of its bodies is destroyed. |
| `target` | Vec2 | read and write | The target of a mouse joint, described in [`joint.target`](#jointtarget). |
| `motorSpeed` | number | read and write | The motor speed of a revolute, prismatic or wheel joint, described in [`joint.motorSpeed`](#jointmotorspeed). |

## Query filters

The `query` methods and `world:pick()` take an optional filter table with the integer fields `category` (default `1`) and `mask` (default all bits). A shape is found when its category shares a bit with the filter mask and the filter category shares a bit with the shape mask. Other keys raise `Unknown option "<key>".`.

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

The ray and shape casts, `world:lineOfSight()` and `world:raycastBatch()` take an optional filter table with these fields. Other keys raise `Unknown option "<key>".`.

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

A `RayBatch` holds rays cast together by `world:raycastBatch()` with a result slot for each one. Rays count from 1, and an index outside the batch raises `the ray index is outside the batch`. The batch keeps its buffers, so casting the same number of rays again reuses them, and reading results creates no tables.

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

Two shapes collide when the category of each one shares a bit with the mask of the other. Categories and masks are 64-bit integers, so up to 64 categories exist. Shapes that share a positive `group` always collide and shapes that share a negative `group` never collide, whatever their categories and masks say.

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

A shape with a `oneWay` direction only blocks bodies that touch it from the side the direction points to. The direction is in the space of the body, so it turns with the body: with y pointing down, `{0, -1}` on an unrotated body makes a platform that bodies jump through from below and land on from above, and the same platform turned upside down blocks bodies from below and lets them fall through from above. Contacts whose normal leans less than 60 degrees toward the direction are disabled while Box2D solves them, so bodies also pass the platform from its sides. Setting `shape.oneWay = nil` makes the shape solid again.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ledge = world:createBody({type = 'static', x = 0, y = 0})
ledge:addBox(160, 8, {oneWay = {0, -1}})

local player = world:createBody({x = 0, y = 80, vy = -600, fixedRotation = true})
player:addCapsule(0, -12, 0, 12, 10)
for i = 1, 120 do world:step(1 / 60) end
print(player.y < 0) -- true, standing on the ledge
```

## Conveyors

A shape with a `tangentSpeed` moves the bodies it touches along its surface at that speed in world units per second, clockwise around the shape as seen on screen, so a positive speed carries bodies on top of a platform to the right. Friction decides how fast they catch up.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local belt = world:createBody({type = 'static', x = 0, y = 200})
local surface = belt:addBox(600, 16, {friction = 0.8, tangentSpeed = 120})
local box = world:createBody({x = -200, y = 170})
box:addBox(24, 24)
for i = 1, 120 do world:step(1 / 60) end
surface.tangentSpeed = -surface.tangentSpeed
print(box.x > -200) -- true
```

## Ropes and bridges

### physics2d.newRope(world, options)

Builds a chain of segments from `from` to `to` joined by revolute joints and returns a `Rope`. An end can hang from a body, or be pinned in place by a static anchor that the rope creates and destroys with itself. Unknown keys raise `Unknown option "<key>".`, and a rope without segments, with ends in the same place or without thickness raises `A rope needs at least one segment, two distinct ends and a positive thickness.`. An invalid material or damping raises the errors of [Shape options](#shape-options) and [`world:createBody`](#worldcreatebodyoptions), and an end body that is destroyed or belongs to another world raises `A joint needs two bodies of this world.`. A rope that fails leaves none of its bodies in the world.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `from`, `to` | Vec2 | `{0, 0}` | Ends of the rope in world units. |
| `segments` | integer | `10` | Number of segments. |
| `thickness` | number | `4` | Width of each segment. |
| `density`, `friction` | number | `1`, `0.6` | Material of the segments. |
| `linearDamping`, `angularDamping` | number | `0`, `0.5` | Damping of the segments, which calms swinging. |
| `planks` | boolean | `false` | Makes the segments boxes instead of capsules. |
| `pinStart`, `pinEnd` | boolean | `false` | Pins an end without a body to a static anchor. |
| `startBody`, `endBody` | Body | `nil` | Hangs an end from a body at the end point. |
| `category`, `mask`, `group` | integer | | Collision filter of the segments. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local lamp = world:createBody({x = 0, y = 150})
lamp:addCircle(12)
local rope = physics2d.newRope(world, {from = {0, 0}, to = {0, 150}, segments = 12, pinStart = true, endBody = lamp})
print(#rope:bodies(), #rope:joints()) -- 12 13
```

### physics2d.newBridge(world, options)

Builds a rope of planks with both ends pinned, unless the options give bodies for them, which makes a rope bridge. It takes the options of `newRope`.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local bridge = physics2d.newBridge(world, {from = {-200, 100}, to = {200, 100}, segments = 16, thickness = 8})
print(#bridge:joints()) -- 17
```

### rope:bodies(), rope:joints(), rope:segments(), rope:points(), rope:destroy()

The methods `bodies` and `joints` return lists of the segment bodies and of the joints, anchors included. The method `segments` returns `{x, y, rotation, length}` for each segment, which places a sprite at its center. The method `points` returns the ends of the segments in order as `Vec2`, which draws the rope as a line. The method `destroy` removes the segments, their joints and the anchors of the rope.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')

local world = physics2d.newWorld()
local rope = physics2d.newRope(world, {from = {0, 0}, to = {200, 0}, segments = 10, pinStart = true})
require('haylen.scene').push({
    update = function(self, dt) world:step(dt) end,
    render = function(self)
        graphics2d.beginWorld(graphics2d.newCamera())
        local points = rope:points()
        for i = 2, #points do
            graphics2d.drawLine(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, 3, '#FF8D6E63')
        end
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

Builds a human figure seen from the side, made of eleven capsules joined by revolute joints with the angle limits of real joints, and returns a `Ragdoll`. Its parts share a negative collision group, so they never collide with each other. A height that is not positive or a group that is not negative raises `A ragdoll needs a positive height and a negative collision group.`, and an invalid material raises the error of [Shape options](#shape-options) without leaving any part in the world.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Center of the hips. |
| `height` | number | `128` | Height from the top of the head to the feet. |
| `density`, `friction` | number | `1`, `0.6` | Material of the parts. |
| `jointFriction` | number | `0` | Torque that resists bending, which makes the figure less limp. |
| `group` | integer | `-1` | Negative collision group of the parts. |
| `vx`, `vy` | number | `0` | Starting velocity of every part. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local ground = world:createBody({type = 'static', x = 0, y = 300})
ground:addBox(1000, 20)
local doll = physics2d.newRagdoll(world, {x = 0, y = 100, height = 120, jointFriction = 50, vx = 200})
doll:body('head'):applyImpulse(-50, 0)
```

### ragdoll:body(part), ragdoll:bodies(), ragdoll:joints(), ragdoll:destroy()

The method `body` returns the body of a part: `head`, `chest`, `hips`, `upperArmLeft`, `lowerArmLeft`, `upperArmRight`, `lowerArmRight`, `upperLegLeft`, `lowerLegLeft`, `upperLegRight` or `lowerLegRight`, and other names raise `unknown ragdoll part`. The method `bodies` returns a table of every body keyed by part name, `joints` the list of joints and `destroy` removes the figure. The property `ragdoll.valid` turns `false` once a part was destroyed.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local doll = physics2d.newRagdoll(world)
for name, body in pairs(doll:bodies()) do
    body.data = {part = name}
end
print(#doll:joints()) -- 10
```

## Vehicles

### physics2d.newVehicle(world, options)

Builds a car with a box chassis on two wheels and returns a `Vehicle`. Wheel joints hold the wheels on springy suspension along the vertical axis of the chassis, and their motors drive the car. The parts share a negative collision group. A chassis or wheel without size, a negative suspension travel or a group that is not negative raises `A vehicle needs a chassis and wheels with a size, a suspension travel of zero or more and a negative collision group.`, and an invalid material raises the error of [Shape options](#shape-options) without leaving any part in the world.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Center of the chassis. |
| `chassisWidth`, `chassisHeight` | number | `120`, `30` | Size of the chassis. |
| `wheelRadius` | number | `16` | Radius of both wheels. |
| `rearWheel`, `frontWheel` | Vec2 | `{-40, 20}`, `{40, 20}` | Wheel positions relative to the chassis center. |
| `density`, `wheelDensity`, `wheelFriction` | number | `1`, `1`, `0.9` | Materials of the chassis and wheels. |
| `suspensionHertz`, `suspensionDamping` | number | `5`, `0.7` | Stiffness and damping of the springs. |
| `suspensionTravel` | number | `10` | How far the wheels move up and down from where they start. |
| `maxMotorTorque` | number | `50000` | Torque of the motors in world units. |
| `drive` | string | `'rear'` | Driven wheels: `'rear'`, `'front'` or `'all'`. |
| `group` | integer | `-2` | Negative collision group of the parts. |

```lua
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')

local world = physics2d.newWorld()
local road = world:createBody({type = 'static', x = 0, y = 300})
road:addBox(4000, 20)
local car = physics2d.newVehicle(world, {x = 0, y = 240, drive = 'all'})
require('haylen.scene').push({
    update = function(self, dt)
        car.motorSpeed = input.keyDown('right') and 15 or (input.keyDown('left') and -15 or 0)
        world:step(dt)
    end,
})
```

### Vehicle members

| Member | Type | Access | Meaning |
| --- | --- | --- | --- |
| `chassis`, `rearWheel`, `frontWheel` | Body | read | The bodies of the car. |
| `motorSpeed` | number | read and write | Speed of the driven wheels in radians per second. Positive speeds spin them clockwise on screen and drive the car toward positive x, and zero brakes with the full torque. Setting it wakes a parked car. |
| `drive` | string | read | The driven wheels. |
| `valid` | boolean | read | The value is `false` once a part was destroyed. |
| `vehicle:joints()` | function | | Returns the two wheel joints. |
| `vehicle:destroy()` | function | | Removes the car. |

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
| `category`, `mask` | integer | | Filter of the bodies the blast reaches and of the shapes that shield them. |

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
| `category`, `mask`, `group` | integer | | Collision filter of the ground. |

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

Reading `samples` returns a new list of every sample from 0 to 1 stored row by row, which can shade the ground. Writing it replaces every sample with such a list, like the alpha channel of an image, or with the results of a function of the column and row. A list of another length raises `The terrain needs one value per sample.`. The method `sample` returns one value.

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

local world = physics2d.newWorld()
local terrain = physics2d.newTerrain(world, {columns = 65, rows = 33, cellSize = 8})
terrain:fill(256, 256, 120)
terrain:update()
require('haylen.scene').push({
    render = function(self)
        graphics2d.beginWorld(graphics2d.newCamera())
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

local world = physics2d.newWorld()
local crate = world:createBody({x = 100, y = 0})
crate:addBox(64, 64)
world.onHit = function(a, b, contact)
    if a == crate and contact.speed > 400 then
        physics2d.fracture(crate, {pieces = 10, impact = {contact.x, contact.y}})
    end
end
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

Creates a `Fluid`: a liquid of small circle bodies that collide with the world, held together and spread apart by the double density relaxation of particle fluids. Call `fluid:update(dt)` right before `world:step(dt)`. The fluid owns its particles, and a particle whose body is destroyed elsewhere, such as in a contact callback that drains it, leaves the fluid the next time it is updated, counted or read. A radius that is not positive or a smoothing radius not larger than it raises `A fluid needs a positive particle radius and a larger smoothing radius.`, and an invalid material raises the error of [Shape options](#shape-options).

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `radius` | number | `4` | Collision radius of the particles, and the radius metaballs start from. |
| `smoothingRadius` | number | `16` | Distance within which particles interact. |
| `density`, `friction`, `restitution` | number | `1`, `0`, `0` | Material of the particles. |
| `restDensity` | number | `2` | Density the pressure pulls toward, where larger values pack the liquid tighter. |
| `stiffness`, `nearStiffness` | number | `0.01`, `0.02` | Strength of the pressure and of the near pressure that keeps particles from clumping, tuned for 60 steps per second. |
| `viscosity` | number | `0.2` | How much particles moving toward each other slow down. |
| `maxParticles` | integer | `4096` | Most particles the fluid holds. |
| `category`, `mask`, `group` | integer | | Collision filter of the particles. |

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local tank = world:createBody({type = 'static', x = 0, y = 0})
-- The first and last points only smooth the ends, so the walls run from the second point to the next-to-last one.
tank:addChain({{-200, -260}, {-200, -200}, {-200, 200}, {200, 200}, {200, -200}, {200, -260}}, false)
local water = physics2d.newFluid(world, {radius = 4, maxParticles = 800})
water:fill({-150, -150, 300, 120})
require('haylen.scene').push({
    update = function(self, dt)
        water:update(dt)
        world:step(dt)
    end,
})
```

### fluid:spawn(x, y, vx, vy), fluid:fill(rect, vx, vy), fluid:remove(particle), fluid:clear()

The method `spawn` adds a particle and returns `false` when the fluid is full. The method `fill` fills a rectangle with particles two radii apart and returns how many it added. The method `remove` removes the particle at a position from 1, moving the last particle into its place, and `clear` removes every particle.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld()
local water = physics2d.newFluid(world)
require('haylen.scene').push({
    update = function(self, dt)
        water:spawn(0, -100, math.random(-20, 20), 150)
        water:update(dt)
        world:step(dt)
    end,
})
```

### fluid:positions(buffer), fluid:velocities(buffer), fluid:bodies()

The methods `positions` and `velocities` write two numbers per particle, x then y, into `buffer` or a new list, clear the entries after them and return the list. Passing the same buffer every frame feeds a metaball renderer without allocating. The method `bodies` returns the particle bodies.

```lua
local physics2d = require('haylen.physics2d')
local graphics2d = require('haylen.graphics2d')

local world = physics2d.newWorld()
local water = physics2d.newFluid(world)
water:fill({0, 0, 80, 80})
local buffer = {}
require('haylen.scene').push({
    render = function(self)
        graphics2d.beginWorld(graphics2d.newCamera())
        water:positions(buffer)
        for i = 1, #buffer, 2 do
            graphics2d.drawCircle(buffer[i], buffer[i + 1], water.radius * 2, '#804FC3F7')
        end
    end,
})
```

### Fluid properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | Number of particles. |
| `radius` | number | read | Radius of the particles. |

## Tiled collision

The method `map:buildCollision(world)` from [`haylen.tiled`](tiled.md) creates static bodies of `world` for the collision shapes of a Tiled map and returns them as a list of `haylen.Body` handles.

```lua
local physics2d = require('haylen.physics2d')
local assets = require('haylen.assets')
local tiled = require('haylen.tiled')

local world = physics2d.newWorld()
local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local walls = map:buildCollision(world)
print(#walls, walls[1].type)
```

## Errors

Invalid input raises Lua errors with these messages.

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | An options or filter table has a key the call does not accept. |
| `A physics world needs positive pixels per meter and at least one sub-step.` | The function `physics2d.newWorld()` received a bad scale or sub-step count. |
| `Too many physics worlds exist at once to create another one.` | The function `physics2d.newWorld()` was called while Box2D holds as many worlds as it can. |
| `unknown value '<name>'` | A body type or joint type is not one of the names listed in this page. It comes inside a bad argument error. |
| `A joint needs two bodies of this world.` | A joint body is destroyed or belongs to another world. |
| `A distance joint needs a length of at least 0.005 meters.` | A distance joint has no `length` and anchors in about the same place, or a `length` that is too short. |
| `A revolute joint needs a lower limit that is not above the upper one, both within 0.99 pi radians of zero.` | A revolute joint received limits in the wrong order or beyond 0.99 pi radians. |
| `A prismatic or wheel joint needs a lower limit that is not above the upper one.` | A prismatic or wheel joint received limits in the wrong order. |
| `The body was destroyed.` | A destroyed body was used. |
| `The shape was destroyed.` | A destroyed shape was used. |
| `The physics joint was destroyed.` | A destroyed joint was used. |
| `A physics body needs a finite damping of zero or more.` | A body option or property received a negative or infinite damping. |
| `A physics shape needs a finite density, friction and restitution of zero or more.` | Shape options, or the material options of a rope, ragdoll, vehicle, terrain or fluid, are negative or not finite. |
| `A physics box needs a positive size.` | The method `body:addBox()` received a zero or negative size. |
| `A physics circle needs a positive radius.` | The method `body:addCircle()` received a zero or negative radius. |
| `A physics capsule needs a positive radius.` | The method `body:addCapsule()` received a zero or negative radius. |
| `A physics segment needs ends more than 0.005 meters apart.` | The method `body:addSegment()` received ends in about the same place. |
| `A physics polygon needs at least three points.` | The method `body:addPolygon()` received fewer than three points. |
| `A physics chain needs at least four points.` | The method `body:addChain()` received fewer than four points. |
| `Chain segments are destroyed together with their body.` | The method `shape:destroy()` was called on a chain segment. |
| `A rectangle query needs a width and height of zero or more.` | The method `world:queryRect()` received a negative width or height. |
| `Only mouse joints have a target.` | The property `joint.target` was used on another joint type. |
| `Only revolute, prismatic and wheel joints have a motor speed.` | The property `joint.motorSpeed` was used on another joint type. |
| `A one-way direction cannot be zero.` | The property `shape.oneWay` or the `oneWay` option received a zero vector. |
| `A rope needs at least one segment, two distinct ends and a positive thickness.` | The function `physics2d.newRope()` or `physics2d.newBridge()` received bad options. |
| `A ragdoll needs a positive height and a negative collision group.` | The function `physics2d.newRagdoll()` received bad options. |
| `A vehicle needs a chassis and wheels with a size, a suspension travel of zero or more and a negative collision group.` | The function `physics2d.newVehicle()` received bad options. |
| `An explosion needs a positive radius.` | The function `physics2d.explode()` or `terrain:explode()` received a radius that is not positive. |
| `A terrain needs at least 2 by 2 samples, a positive cell size and chunk size, and a tolerance of at least zero.` | The function `physics2d.newTerrain()` received bad options. |
| `The terrain needs one value per sample.` | The property `terrain.samples` received a list of another length. |
| `A fracture needs at least one piece.` | The function `physics2d.fracture()` or `physics2d.splitPolygon()` received fewer than one piece. |
| `A fluid needs a positive particle radius and a larger smoothing radius.` | The function `physics2d.newFluid()` received bad radii. |
| `A circle cast needs a finite radius of zero or more.` | The method `world:castCircle()` received a negative radius. |
| `A box cast needs a positive size.` | The method `world:castBox()` received a zero or negative size. |
| `A capsule cast needs a positive radius.` | The method `world:castCapsule()` received a zero or negative radius. |
| `A polygon cast needs between three and eight points.` | The method `world:castPolygon()` received too few or too many points. |
| `A bouncing physics ray needs a finite length.` | The method `world:bounceRay()` received an infinite length. |
| `the length must be finite` | The method `world:rayFan()` received an infinite length. It comes inside a bad argument error. |
| `Ray batches take no "accept" function, because their rays run on worker threads.` | The method `world:raycastBatch()` received a filter with `accept`. |
| `The type "haylen.Body" has no member "<name>".` | A body, shape, joint or world member does not exist. The type name changes with the userdata. |
| `The type "haylen.Body" has no writable property "<name>".` | A read-only property was assigned. The type name changes with the userdata. |
