# 📖 `haylen.physics3d`

The module `haylen.physics3d` makes 3D physics worlds in meters, which Jolt Physics simulates unless a plugin selects another backend. This page lists the functions of the module and every member of its worlds, bodies, joints and shapes. The [3D physics guide](../3d/physics.md) explains worlds, steps, threads, limits, shapes and backends.

```lua
local physics3d = require('haylen.physics3d')
```

## Units

Every distance and position is in meters, every speed in meters per second, every time in seconds, every mass in kilograms, every density in kilograms per cubic meter and gravity in meters per second squared, with `y` up, as the [units of 3D](../3d/coordinates.md#units) list. The memory of a step is in megabytes. Vertices, triangles and samples count from one, like a Lua array.

## Functions

| Function | Returns |
| --- | --- |
| [`physics3d.newWorld(options)`](#physics3dnewworldoptions) | A [`PhysicsWorld3D`](#physicsworld3d) |
| [`physics3d.backends()`](#physics3dbackends) | The names of the registered backends |
| [`physics3d.category(name)`](#physics3dcategoryname) | The bit of a collision category |
| [`physics3d.categories()`](#physics3dcategories) | The names of the collision categories |
| [`physics3d.newSphere(radius, options)`](#physics3dnewsphereradius-options) | A [`Shape3D`](#shape3d) |
| [`physics3d.newBox(size, options)`](#physics3dnewboxsize-options) | A `Shape3D` |
| [`physics3d.newCapsule(height, radius, options)`](#physics3dnewcapsuleheight-radius-options) | A `Shape3D` |
| [`physics3d.newTaperedCapsule(height, topRadius, bottomRadius, options)`](#physics3dnewtaperedcapsuleheight-topradius-bottomradius-options) | A `Shape3D` |
| [`physics3d.newCylinder(height, radius, options)`](#physics3dnewcylinderheight-radius-options) | A `Shape3D` |
| [`physics3d.newTaperedCylinder(height, topRadius, bottomRadius, options)`](#physics3dnewtaperedcylinderheight-topradius-bottomradius-options) | A `Shape3D` |
| [`physics3d.newConvexHull(points, options)`](#physics3dnewconvexhullpoints-options) | A `Shape3D` |
| [`physics3d.newMesh(positions, indices)`](#physics3dnewmeshpositions-indices) | A `Shape3D` |
| [`physics3d.newHeightField(heights, sampleCount, options)`](#physics3dnewheightfieldheights-samplecount-options) | A `Shape3D` |
| [`physics3d.newPlane(normal, distance, options)`](#physics3dnewplanenormal-distance-options) | A `Shape3D` |
| [`physics3d.newCompound(children, options)`](#physics3dnewcompoundchildren-options) | A `Shape3D` |
| [`physics3d.newScaled(shape, scale)`](#physics3dnewscaledshape-scale) | A `Shape3D` |
| [`physics3d.newOffsetCenterOfMass(shape, offset)`](#physics3dnewoffsetcenterofmassshape-offset) | A `Shape3D` |
| [`physics3d.newConvexHullAsync(points, options)`](#asynchronous-shapes) | A promise of a `Shape3D` |
| [`physics3d.newMeshAsync(positions, indices)`](#asynchronous-shapes) | A promise of a `Shape3D` |
| [`physics3d.newHeightFieldAsync(heights, sampleCount, options)`](#asynchronous-shapes) | A promise of a `Shape3D` |

### physics3d.newWorld(options)

Creates an empty world and returns it. Its settings start from the `physics3d` section of `haylen.json`, and the table `options`, which is optional, changes any of them for this world. An unknown key raises `Unknown option "<key>".`, and a value out of its range raises a sentence that names it, such as `The value of "velocitySteps" must be from 2 to 64.`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `gravity` | Vec3 or `{x, y, z}` | `{0, -m.standardGravity, 0}` | Gravity in meters per second squared, which pulls every body that gravity moves. |
| `collisionSteps` | integer | `1` | The equal parts of each step, from 1 to 64. Each part finds the collisions and solves them again, which a world of fast bodies needs at a low step rate. |
| `velocitySteps` | integer | `10` | The passes of the velocity solver in each part, from 2 to 64. More passes stack bodies and hold joints more firmly. |
| `positionSteps` | integer | `2` | The passes that push overlapping bodies apart in each part, from 0 to 64. |
| `sleepEnabled` | boolean | `true` | Whether bodies that rest fall asleep, which costs nothing until something wakes them. |
| `timeBeforeSleep` | number | `0.5` | The seconds a body rests before it sleeps. |
| `sleepVelocity` | number | `0.03` | The speed in meters per second under which every point of a body counts as resting. |
| `speculativeContactDistance` | number | `0.02` | The distance in meters at which a contact starts before the bodies touch, which stops fast bodies before they overlap. |
| `penetrationSlop` | number | `0.02` | The overlap in meters that the solver leaves, which keeps resting bodies from jittering. |
| `restitutionThreshold` | number | `1` | The speed in meters per second under which a hit does not bounce. |
| `maxBodies` | integer | `10240` | The most bodies the world holds, from 1 to 8388607. |
| `maxBodyPairs` | integer | `65536` | The most pairs of bodies whose bounds touch in one step, from 1 to 8388607. |
| `maxContactConstraints` | integer | `20480` | The most contacts the world solves in one step, from 1 to 8388607. |
| `threads` | integer | The workers of the engine, up to 4 | The threads that a step runs on, from 1 to 64, while enough bodies are awake. |
| `temporaryMemory` | integer | `16` | The megabytes that one step works in, from 1 to 1024. |
| `interpolate` | boolean | `false` | Whether the world keeps the transforms of the last two steps of the bodies that move, so they draw smoothly between fixed steps. |
| `enhancedInternalEdgeRemoval` | boolean | `false` | Whether new bodies slide smoothly over the edges inside meshes unless they set their own. |
| `owner` | table or userdata | The default owner | What the world belongs to. The world ends with its owner. |

```lua
local physics3d = require('haylen.physics3d')

local moon = physics3d.newWorld({gravity = {0, -1.62, 0}, velocitySteps = 12, owner = self})
```

### physics3d.backends()

Returns the names of the registered backends in alphabetical order, such as `{'jolt'}`. The built-in backend is `jolt`, and a C++ plugin may register and select another one, as the [guide](../3d/physics.md#backends) explains.

```lua
print(table.concat(require('haylen.physics3d').backends(), ', ')) -- jolt
```

### physics3d.category(name)

Returns the bit of a category that `physics3d.categories` of `haylen.json` names, 1 for the first name, 2 for the second, 4 for the third and so on. A name that the list lacks raises a sentence with the names it has.

```lua
local player = physics3d.category('player')
```

### physics3d.categories()

Returns the names of the categories of `physics3d.categories` in `haylen.json`, in the order of their bits.

## PhysicsWorld3D

A `PhysicsWorld3D`, named `haylen.PhysicsWorld3D` in messages, holds the bodies of a 3D simulation and moves them when the app steps it. Every member of a destroyed world, but `destroyed` and `destroy`, raises `The 3D physics world was destroyed.`.

### world:step(seconds)

Advances the world by the time in seconds, which is zero or more. Step a world in the `fixedUpdate` of a scene with the time it receives, so the simulation keeps the same step whatever the frame rate. A negative time, or one that is not a number, raises `A step of a 3D physics world takes a time of zero seconds or more.`.

```lua
function scene:fixedUpdate(seconds)
    self.world:step(seconds)
end
```

### world:readTransforms(bodies, buffer, first, interpolated)

Copies the position and rotation of every body of the list `bodies` into the `FloatBuffer` `buffer`, seven floats for each body in order, `x`, `y`, `z` and the `x`, `y`, `z` and `w` of its rotation, from the position `first`, 1 unless the call names another. With `interpolated` set to `true` in a world made with `interpolate`, the transforms lie between the last two steps by the interpolation of the frame clock. One call syncs thousands of models with their bodies, without a call of Lua for each body.

- A body that is destroyed or belongs to another world raises `Body transforms need live bodies of this world.`.
- A buffer without seven values for each body from `first` raises `Body transforms take seven floats for each body.`.
- The value `interpolated` in a world without `interpolate` raises `Only a 3D physics world made with "interpolate" blends the transforms of its bodies.`.

```lua
local transforms = collections.newFloatBuffer(#crates * 7)

function scene:draw()
    self.world:readTransforms(self.crates, transforms, 1, true)
    self.batch:setTransforms(transforms)
end
```

### world:writeTransforms(bodies, buffer, first)

Teleports every body of the list to the position and rotation that the buffer holds for it, in the layout of `world:readTransforms`. A rotation is normalized, so one that drifted from a length of one still places its body.

### world:destroy()

Ends the world at once with everything it holds, without waiting for the garbage collector. Destroying a destroyed world does nothing.

### world:stats()

Returns a table that tells what the world holds and what its last step cost.

| Field | Type | Meaning |
| --- | --- | --- |
| `bodies` | integer | The bodies of the world. |
| `activeBodies` | integer | The bodies that are awake. |
| `stepThreads` | integer | The threads that the last step ran on, or 0 before the first step. |
| `stepMilliseconds` | number | The milliseconds that the last step took. |
| `temporaryMemoryPeak` | integer | The most bytes of the temporary memory that any step used. |

### world:stateHash()

Returns an integer that hashes the whole state that the steps advance, bit for bit. Two worlds that receive the same bodies and the same steps on the same platform and build give the same hash, which checks that a replay or a lockstep game stays in step.

```lua
local hash = world:stateHash()
```

### World properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `gravity` | Vec3 | read and write | The option `gravity`. |
| `collisionSteps` | integer | read and write | The option `collisionSteps`. |
| `velocitySteps` | integer | read and write | The option `velocitySteps`. |
| `positionSteps` | integer | read and write | The option `positionSteps`. |
| `sleepEnabled` | boolean | read and write | The option `sleepEnabled`. |
| `timeBeforeSleep` | number | read and write | The option `timeBeforeSleep`. |
| `sleepVelocity` | number | read and write | The option `sleepVelocity`. |
| `speculativeContactDistance` | number | read and write | The option `speculativeContactDistance`. |
| `penetrationSlop` | number | read and write | The option `penetrationSlop`. |
| `restitutionThreshold` | number | read and write | The option `restitutionThreshold`. |
| `maxBodies` | integer | read | The option `maxBodies`, which stays for the life of the world. |
| `maxBodyPairs` | integer | read | The option `maxBodyPairs`, which stays for the life of the world. |
| `maxContactConstraints` | integer | read | The option `maxContactConstraints`, which stays for the life of the world. |
| `threads` | integer | read | The option `threads`, which stays for the life of the world. |
| `temporaryMemory` | integer | read | The option `temporaryMemory`, which stays for the life of the world. |
| `interpolate` | boolean | read | The option `interpolate`, which stays for the life of the world. |
| `enhancedInternalEdgeRemoval` | boolean | read and write | The option `enhancedInternalEdgeRemoval`, which the bodies made from then on take. |
| `onJointBreak` | function or nil | read and write | The function that hears each joint that a step breaks, as [Breaking joints](#breaking-joints) describes. |
| `backend` | string | read | The name of the backend that simulates the world. |
| `bodyCount` | integer | read | The bodies of the world. |
| `activeBodyCount` | integer | read | The bodies that are awake. |
| `destroyed` | boolean | read | Whether the world was destroyed. |

A value written out of its range raises the same sentence as the option, and the world keeps the value it had.

```lua
world.gravity = {0, -3.71, 0}
world.velocitySteps = 14
```

## Body3D

A `Body3D`, named `haylen.Body3D` in messages, is an object of a world that collides with its shape. The method [`world:createBody`](#worldcreatebodyoptions) makes one. Every member but `valid` and `destroy` raises `The 3D physics body was destroyed.` once the body is gone, and `The 3D physics world of the body was destroyed.` once its world is gone. Two values of the same body compare equal with `==`. The [guide](../3d/physics.md#bodies) explains the types of bodies and how they move.

### world:createBody(options)

Makes a body and returns it. The table `options` needs `shape`, and every other option is optional. An unknown key raises `Unknown option "<key>".`, and a value out of its range raises a sentence that names it, such as `The linear damping of a 3D body must be a finite number of zero or more.`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `shape` | Shape3D | none | The shape the body collides with. |
| `type` | string | `'dynamic'` | `'static'` never moves, `'kinematic'` moves only by its velocity and `moveTo`, and `'dynamic'` moves by forces and contacts. |
| `position` | Vec3 or `{x, y, z}` | `{0, 0, 0}` | Where the origin of the shape lies. |
| `rotation` | Quat or `{x, y, z, w}` | no turn | The rotation, a quaternion of length one. |
| `linearVelocity` | Vec3 or `{x, y, z}` | `{0, 0, 0}` | The velocity in meters per second, which a static body does not take. |
| `angularVelocity` | Vec3 or `{x, y, z}` | `{0, 0, 0}` | The spin in radians per second around each axis of the world. |
| `motionQuality` | string | `'discrete'` | `'discrete'` moves in jumps of one step, and `'linearCast'` sweeps the shape along its path, so a fast body never passes a thin wall. |
| `locks` | table | none | The axes the body keeps, `{x, y, z, rotationX, rotationY, rotationZ}` of booleans, such as `{z = true, rotationX = true, rotationY = true}` for a body that moves in the plane `x`, `y`. A dynamic body keeps at least one axis free. |
| `mass` | number | from the shape | The mass in kilograms, which scales the inertia of the shape with it. |
| `inertia` | Vec3 or `{x, y, z}` | from the shape | The inertia around the center of mass along the axes of the body, in kilograms times square meters. |
| `centerOfMass` | Vec3 or `{x, y, z}` | from the shape | The point the body turns around, in the space of the body. |
| `linearDamping` | number | `0.05` | How fast the body loses its speed, as the air slows it. |
| `angularDamping` | number | `0.05` | How fast the body loses its spin. |
| `gravityScale` | number | `1` | The share of the gravity of the world that pulls the body, `0` for none and negative to fall up. |
| `maxLinearSpeed` | number | `500` | The highest speed in meters per second. |
| `maxAngularSpeed` | number | `47.12` | The highest spin in radians per second, a quarter turn in each step of 60 hertz. |
| `friction` | number | `0.2` | How much the body grips what it slides on, 0 for ice and 1 for rubber on concrete. |
| `restitution` | number | `0` | How much the body bounces, 0 for none and 1 for a bounce that keeps its whole speed. |
| `frictionCombine` | string | `'average'` | How the friction of two touching bodies combines: `'average'`, `'minimum'`, `'multiply'` or `'maximum'`. Of the rules of the two bodies, the one later in that list wins. |
| `restitutionCombine` | string | `'average'` | How the restitution of two touching bodies combines, by the same rules. |
| `sleepEnabled` | boolean | `true` | Whether the body falls asleep once it rests. |
| `sensor` | boolean | `false` | Whether the body detects what enters it without pushing it, such as a trigger zone. |
| `collideWithStatic` | boolean | `false` | Whether a kinematic body or a sensor also meets static and kinematic bodies, such as a trigger that finds the walls. |
| `enhancedInternalEdgeRemoval` | boolean | the world option | Whether the body slides smoothly over the edges inside a mesh, which costs a little time. |
| `enabled` | boolean | `true` | Whether the body takes part in the simulation. |
| `category` | string, integer or list | `1` | The categories of the body: the name of a category of `physics3d.categories`, an integer of 32 bits or a list of both. |
| `mask` | string, integer or list | every category | The categories the body collides with, in the forms of `category`. |
| `group` | integer | `0` | Bodies of the same positive group always collide and bodies of the same negative group never do, whatever their categories and masks. |

```lua
local physics3d = require('haylen.physics3d')

local world = physics3d.newWorld({owner = self})
world:createBody({type = 'static', shape = physics3d.newBox({100, 1, 100}), position = {0, -0.5, 0}})
local crate = world:createBody({shape = physics3d.newBox({1, 1, 1}, {density = 600}), position = {0, 4, 0}, friction = 0.6})
```

### body:applyForce(force, point)

Pushes the body with a force in newtons over the next step, at the world point `point` when the call names one, which also turns the body, and at its center of mass otherwise. Only a dynamic body takes forces, and any other raises `Only a dynamic 3D body takes forces.`.

```lua
function scene:fixedUpdate(seconds)
    self.boat:applyForce({0, 0, -800}, self.boat.worldCenter)
    self.world:step(seconds)
end
```

### body:applyImpulse(impulse, point)

Changes the velocity of the body at once by an impulse in newton seconds, at the world point `point` when the call names one. An impulse of the mass times a speed adds that speed, such as a kick or a jump.

```lua
ball:applyImpulse({0, ball.mass * 5, 0})
```

### body:applyTorque(torque)

Turns the body with a torque in newton meters around each axis of the world over the next step.

### body:applyAngularImpulse(impulse)

Changes the spin of the body at once by an angular impulse.

### body:setTransform(position, rotation)

Teleports the body to the position, and to the rotation when the call names one, which suits spawning and respawning. A teleport skips the sweep of a linear cast and pushes out what the body lands in, so a body that should push and carry others moves with `moveTo` instead.

```lua
player:setTransform({0, 1, 0})
```

### body:moveTo(position, rotation, seconds)

Gives a kinematic or dynamic body the velocities that bring it to the transform after `seconds`, normally the time of one fixed step, so a moving platform or a door pushes and carries what touches it. A static body raises an error, and a time that is not above zero raises `A 3D body moves to a transform over a time above zero seconds.`.

```lua
function scene:fixedUpdate(seconds)
    self.time = self.time + seconds
    self.lift:moveTo({0, 2 + math.sin(self.time) * 2, 0}, m.quat(), seconds)
    self.world:step(seconds)
end
```

### body:velocityAt(point)

Returns the velocity of a world point that moves with the body, with its spin, such as the speed of the ground under the feet of a character.

### body:resetMass()

Takes the mass, inertia and center of mass from the shape and its density again, forgetting the ones that were set.

### body:addChild(shape, position, rotation)

Adds a child to the own copy of a mutable compound that the body took, at the position and rotation in the space of the body, and returns its index. A body whose shape is no mutable compound raises an error.

```lua
local ship = world:createBody({shape = physics3d.newCompound({{shape = hull}}, {mutable = true})})
local wing = ship:addChild(physics3d.newBox({4, 0.2, 1}), {2.5, 0, 0})
```

### body:removeChild(index)

Removes a child of the own mutable compound of the body. A dynamic body keeps at least one child, since a body without a volume has no mass.

### body:setChildTransform(index, position, rotation)

Moves a child of the own mutable compound of the body.

### body:renderTransform()

Returns the position and the rotation to draw the body at. In a world made with `interpolate`, they lie between the last two steps of the body by the interpolation of the frame clock, so the body draws smoothly when frames and fixed steps do not line up. Otherwise they are where the body is.

```lua
function scene:draw()
    local position, rotation = self.crate:renderTransform()
    self.model:draw(position, rotation)
end
```

### body:destroy()

Destroys the body. Destroying a destroyed body does nothing.

### Body3D properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | boolean | read | Whether the body and its world live. |
| `world` | PhysicsWorld3D | read | The world of the body. |
| `type` | string | read and write | The option `type`. A dynamic body needs a shape with a volume. |
| `position` | Vec3 | read and write | Where the body is. Writing it teleports the body, and tweens animate it in C++. |
| `x`, `y`, `z` | number | read and write | One part of the position. |
| `rotation` | Quat | read and write | The rotation of the body. Writing it teleports the body, and tweens animate it in C++. |
| `linearVelocity` | Vec3 | read and write | The velocity, which tweens animate in C++. A static body raises an error on a write. |
| `angularVelocity` | Vec3 | read and write | The spin, which tweens animate in C++. |
| `motionQuality` | string | read and write | The option `motionQuality`. |
| `locks` | table | read and write | The option `locks`, as a table of six booleans. |
| `mass` | number | read and write | The mass in kilograms. |
| `inertia` | Vec3 | read and write | The inertia around the center of mass along the axes of the body. |
| `centerOfMass` | Vec3 | read and write | The center of mass in the space of the body. |
| `worldCenter` | Vec3 | read | Where the center of mass is in the world. |
| `linearDamping`, `angularDamping`, `gravityScale` | number | read and write | The options of the same names. |
| `maxLinearSpeed`, `maxAngularSpeed` | number | read and write | The options of the same names. |
| `friction`, `restitution` | number | read and write | The options of the same names. |
| `frictionCombine`, `restitutionCombine` | string | read and write | The options of the same names. |
| `sleepEnabled` | boolean | read and write | The option `sleepEnabled`. |
| `awake` | boolean | read and write | Whether the body is awake. Writing `true` wakes it and `false` sends it to sleep. |
| `sensor`, `collideWithStatic`, `enhancedInternalEdgeRemoval`, `enabled` | boolean | read and write | The options of the same names. |
| `category`, `mask` | integer | read and write | The options of the same names, read as integers and written in any of their forms. |
| `group` | integer | read and write | The option `group`. |
| `shape` | Shape3D | read and write | The shape, or the own copy of a mutable compound. A new shape gives the body its mass unless the mass was set. |
| `bounds` | Box | read | The box that holds the body in the world. |
| `childCount` | integer | read | The children of the own mutable compound of the body. |
| `data` | any | read and write | A value of the app that the body keeps, such as the entity it belongs to. |

```lua
local tween = require('haylen.tween')

tween.to(door, 0.8, {rotation = m.fromAxisAngle(m.up(), math.pi / 2)}, {ease = 'quadOut'})
```

### Body errors

| Message | Cause |
| --- | --- |
| `A 3D body needs the option "shape", such as "physics3d.newBox({1, 1, 1})".` | The options of `world:createBody` hold no shape. |
| `A dynamic 3D body needs a shape with a volume, and a mesh has none. Make the body static or kinematic, or give it a convex hull or a compound of convex shapes.` | A dynamic body with a mesh, a heightfield or a plane. |
| `A dynamic 3D body locked on every axis never moves. Make it static, or leave one axis free.` | Locks on all six axes of a dynamic body. |
| `A static 3D body never moves, so it takes no velocity. Make it kinematic to move it.` | A velocity given to a static body. |
| `Only a dynamic 3D body takes forces.` and the other sentences that start with `Only a dynamic` | A push of a static or kinematic body. |
| `The 3D physics world holds "maxBodies" bodies already.` and the rest of its sentence | A world that holds `maxBodies`. |
| `The 3D physics body was destroyed.` | A member of a destroyed body. |
| `No 3D collision category is named "<name>".` and the rest of its sentence | A name that `physics3d.categories` of `haylen.json` lacks. |
| `The option "mask" of a 3D body takes the names of categories or whole numbers of 32 bits.` | A category or mask that is neither. |
| `The 3D physics world of the body was destroyed.` | A member of a body whose world was destroyed. |
| `The shape of a 3D body was made by the 3D physics backend "<name>", and the world takes only shapes of its own backend "<name>".` | A shape of another backend. |

## Joint3D

A `Joint3D`, named `haylen.Joint3D` in messages, holds two bodies of a world together, or one body to the world. The method [`world:createJoint`](#worldcreatejointtype-first-second-options) makes one. Every member but `valid` and `destroy` raises `The 3D physics joint was destroyed.` once the joint or one of its bodies is gone. A member that the type of the joint lacks raises a sentence that names the types that have it, such as `Only the joints "hinge" have an angle, and this joint is "fixed".`. Two values of the same joint compare equal with `==`. The [guide](../3d/physics.md#joints) explains which joint suits which object.

### world:createJoint(type, first, second, options)

Makes a joint of the type between the bodies `first` and `second`, or between `first` and the world when `second` is `nil`, and returns it. Every point and axis is in world space as the bodies stand when the joint is made. The angles, positions, limits and motors measure the second body against the first, and the body against the world in a joint to the world.

| Type | Holds |
| --- | --- |
| `'fixed'` | The bodies glued as they stand. |
| `'point'` | The bodies pinned at `point`, free to turn around it, such as a chain link. |
| `'distance'` | The anchors `point` and `secondPoint` between `lower` and `upper` meters apart, such as a rope or a spring. |
| `'hinge'` | The bodies turning around `axis` through `point`, such as a door or a wheel. |
| `'slider'` | The bodies moving along `axis`, such as a drawer or a piston. |
| `'cone'` | The `axis` of the second body inside a cone of `halfConeAngle` around the `axis` of the first. |
| `'swingTwist'` | A shoulder or a hip of a ragdoll: a swing inside the cone of `halfConeAngle` and `planeHalfConeAngle` and a twist around `axis` between `lower` and `upper`. |
| `'sixDof'` | Six degrees of freedom: each axis of `axes` free, limited or fixed, with a motor of its own. |
| `'path'` | The second body moving along the spline `path`, which moves with the first body, such as a roller coaster. |
| `'gear'` | Two hinges that turn together by `ratio`. |
| `'rackAndPinion'` | A hinge, the pinion, and a slider, the rack, that move together by `ratio` radians for each meter. |
| `'pulley'` | Two ropes from `point` and `secondPoint` over `fixedPoint` and `secondFixedPoint`, whose lengths add up to a constant. |

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `point` | Vec3 or `{x, y, z}` | `{0, 0, 0}` | Where the joint holds the first body, and the second unless `secondPoint` says otherwise. |
| `secondPoint` | Vec3 or `{x, y, z}` | `point` | Where the joint holds the second body. |
| `axis` | Vec3 or `{x, y, z}` | `{0, 1, 0}` | The axis of a hinge, a slider, a cone, a swing twist joint and the gear on the first body, and the `x` of a six degrees of freedom joint. |
| `normal` | Vec3 or `{x, y, z}` | `{1, 0, 0}` | The direction at a right angle to `axis` where the angle of a hinge is 0, the plane of a swing twist joint and the `y` of a six degrees of freedom joint. |
| `secondAxis` | Vec3 or `{x, y, z}` | `{0, 1, 0}` | The axis of the gear on the second body, or the axis of the rack. |
| `lower`, `upper` | number | none | The limits of a hinge or a twist in radians, from -pi to pi, of a slider in meters, and of the lengths of a distance joint or a pulley in meters. Both or neither. Without them a hinge, a slider and a twist are free and a distance joint and a pulley keep their length when made. |
| `limitSpring` | table | rigid | A spring that softens the limits of a hinge, a slider or a distance joint, as [Springs](#springs) describes. |
| `friction` | number | `0` | The torque or force that holds a hinge, a slider, a swing twist joint or a path still. |
| `halfConeAngle`, `planeHalfConeAngle` | number | `0` | The half angles of the cone of a cone or a swing twist joint, from 0 to pi. |
| `motor` | table | off | The motor of a hinge, a slider, a path or the twist of a swing twist joint, as [Motors](#motors) describes. |
| `swingMotor` | table | off | The motor of the swing of a swing twist joint. |
| `targetRotation` | Quat or `{x, y, z, w}` | no turn | The rotation that the motors of position of a swing twist joint and of the turns of a six degrees of freedom joint drive toward. |
| `targetAngularVelocity` | Vec3 or `{x, y, z}` | `{0, 0, 0}` | The spin that the motors of velocity of a swing twist joint drive at. |
| `axes` | table | every axis free | The axes of a six degrees of freedom joint by name, `x`, `y`, `z`, `rotationX`, `rotationY` and `rotationZ`, each a table `{mode, lower, upper, limitSpring, friction, motor}` whose `mode` is `'free'`, `'limited'` or `'fixed'`. |
| `path` | Spline3D | none | The spline of a path joint. |
| `pathRotation` | string | `'free'` | How the body on a path turns: `'free'`, `'aroundTangent'`, `'aroundNormal'`, `'aroundBinormal'`, `'withPath'` or `'fixed'`. |
| `pathFraction` | number | `0` | Where on the path the body starts, from 0 to 1. |
| `ratio` | number | `1` | The ratio of a gear, a rack and pinion or a pulley, which stays as the joint is made. |
| `joints` | list | none | The two hinges of a gear, or the hinge and the slider of a rack and pinion, so it never drifts from them. |
| `fixedPoint`, `secondFixedPoint` | Vec3 or `{x, y, z}` | `{0, 0, 0}` | The fixed points that the ropes of a pulley run over. |
| `priority` | integer | `0` | Joints of a higher priority are solved later in each pass, so they hold better. |
| `velocitySteps`, `positionSteps` | integer | `0` | More solver passes for the island of the joint, 0 for the passes of the world. |
| `collideConnected` | boolean | `false` | Whether the two bodies of the joint collide with each other. |
| `enabled` | boolean | `true` | Whether the joint holds its bodies. |
| `breakForce`, `breakTorque` | number | none | The force in newtons or the torque in newton meters that breaks the joint, as [Breaking joints](#breaking-joints) describes. |

```lua
local door = world:createBody({shape = physics3d.newBox({1, 2, 0.1}), position = {0.5, 1, 0}})
local hinge = world:createJoint('hinge', door, nil, {point = {0, 1, 0}, axis = {0, 1, 0}, normal = {1, 0, 0}, lower = 0, upper = math.pi / 2})
```

### Springs

A spring is a table `{mode, frequency, stiffness, damping}`. In the mode `'frequency'`, the default, `frequency` is in hertz and `damping` is the ratio of its damping, where 1 stops it without a bounce. In the mode `'stiffness'`, `stiffness` is in newtons per meter and `damping` in newton seconds per meter. A frequency or a stiffness of 0 makes the spring rigid.

### Motors

A motor is a table `{state, targetVelocity, targetPosition, maxForce, maxTorque, spring}`. Its `state` is `'off'`, `'velocity'`, which drives at `targetVelocity` in meters or radians per second, or `'position'`, which drives toward `targetPosition` in meters, radians or the share of a path through its `spring`, 2 hertz with a damping of 1 unless it says otherwise. The motor drives with at most `maxForce` newtons and `maxTorque` newton meters.

```lua
wheel.motor = {state = 'velocity', targetVelocity = 12, maxTorque = 400}
```

### Breaking joints

A joint with `breakForce` or `breakTorque` is destroyed by the first step that loads it past one of them, and the world calls `world.onJointBreak` once for it, after the step, with the joint and a table `{first, second, force, torque}`, where `second` is `nil` for a joint to the world. The joint is no longer valid in the callback, and it still compares equal with the values of it that the app keeps.

```lua
world.onJointBreak = function(joint, info)
    print('A joint broke with a force of', info.force)
end
```

### joint:setLimits(lower, upper)

Changes both limits at once, so the lower one never passes the upper one between two writes.

### joint:axis(name)

Returns the settings of an axis of a six degrees of freedom joint, as a table of the option `axes`.

### joint:setAxis(name, settings)

Changes an axis of a six degrees of freedom joint.

```lua
six:setAxis('x', {mode = 'limited', lower = -0.5, upper = 0.5, motor = {state = 'position', targetPosition = 0.2}})
```

### joint:destroy()

Destroys the joint. Destroying a destroyed joint does nothing.

### Joint3D properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `valid` | boolean | read | Whether the joint, its bodies and its world live. |
| `type` | string | read | The type of the joint. |
| `first`, `second` | Body3D | read | The bodies of the joint, `second` `nil` for a joint to the world. |
| `enabled`, `priority` | boolean, integer | read and write | The options of the same names. |
| `force`, `torque` | number | read | The force and torque that held the joint in the last step. |
| `breakForce`, `breakTorque` | number or nil | read and write | The options of the same names. |
| `lower`, `upper` | number | read | The limits, which `joint:setLimits` changes. |
| `limitSpring`, `motor`, `swingMotor` | table | read and write | The options of the same names. |
| `friction`, `halfConeAngle`, `planeHalfConeAngle` | number | read and write | The options of the same names. |
| `targetRotation` | Quat | read and write | The option `targetRotation`. |
| `targetAngularVelocity` | Vec3 | read and write | The option `targetAngularVelocity`. |
| `ratio` | number | read | The option `ratio`. |
| `angle` | number | read | The angle of a hinge in radians. |
| `position` | number | read | The position of a slider along its axis in meters. |
| `length` | number | read | The length between the anchors of a distance joint in meters. |
| `fraction` | number | read | How far the body on a path is, from 0 to 1. |

### Joint errors

| Message | Cause |
| --- | --- |
| `The 3D physics joint was destroyed.` | A member of a destroyed joint. |
| `Only the joints "hinge" have an angle, and this joint is "fixed".` and the other sentences that start with `Only the joints` | A member that the type of the joint lacks. |
| `A 3D joint holds two different bodies, or one body and the world.` | One body twice. |
| `A joint of the type "pulley" needs a second body.` | A path, a gear, a rack and pinion or a pulley without a second body. |
| `The limits of a 3D joint must be finite numbers, the lower one not above the upper one.` | Limits out of order. |
| `The normal of a 3D joint must stand at a right angle to its axis.` | A normal that leans along the axis. |
| `A path joint needs the option "path", a spline of two points or more.` | A path joint without a spline. |
| `A joint of the type "gear" takes as its joints two live hinges of its own world.` | A gear or a rack and pinion with other joints. |
| `The axes of a 3D joint are "x", "y", "z", "rotationX", "rotationY" and "rotationZ".` | An unknown name in `axes`. |

## Shape3D

A `Shape3D`, named `haylen.Shape3D` in messages, is the collision shape of 3D bodies. The functions `physics3d.new...` make one with the backend that new worlds take. A shape never changes once it is made, so many bodies share one, and two values of the same shape compare equal with `==`. Round shapes stand along `y` around their middle. The [guide](../3d/physics.md#shapes) explains which shape suits which object.

A shape with a volume takes a mass from its density, in kilograms per cubic meter, which is `1000` unless the option `density` says otherwise, the density of water. A mesh, a heightfield and a plane have no volume, so only static and kinematic bodies take them.

The convex radius rounds the edges of a box, a cylinder or a hull inside its size, which makes its collisions faster and steadier. A box or a cylinder takes `0.05` meters, or half of its smallest size when that is less, unless the option `convexRadius` gives a radius from 0 to that half. A hull takes `0.05` meters at most unless the option gives another most, and its radius shrinks to fit the hull.

| Option | Type | Default | Shapes | Meaning |
| --- | --- | --- | --- | --- |
| `density` | number | `1000` | Sphere, box, capsules, cylinders, hull | The density that gives the shape its mass. |
| `convexRadius` | number | `0.05`, or half the smallest size of a thinner box or cylinder | Box, cylinders, hull | The radius that rounds the edges, the most one of a hull. |

An unknown option raises `Unknown option "<key>".`.

### physics3d.newSphere(radius, options)

Makes a ball of the radius.

```lua
local ball = physics3d.newSphere(0.11)
```

### physics3d.newBox(size, options)

Makes a box of the full width, height and depth of the `Vec3` or `{x, y, z}` table `size`.

```lua
local crate = physics3d.newBox({1, 1, 1}, {density = 600})
```

### physics3d.newCapsule(height, radius, options)

Makes a capsule standing along `y`, whose `height` runs from the bottom of its lower cap to the top of its upper cap and is more than twice its radius. A capsule suits characters, since it slides over steps and edges.

```lua
local hiker = physics3d.newCapsule(1.8, 0.3)
```

### physics3d.newTaperedCapsule(height, topRadius, bottomRadius, options)

Makes a capsule whose caps have radii of their own, from the bottom of the lower cap to the top of the upper cap. The caps must not hold each other, so the height is more than the sum of the radii plus their difference. It stands around the middle between the centers of its caps.

```lua
local limb = physics3d.newTaperedCapsule(0.6, 0.06, 0.09)
```

### physics3d.newCylinder(height, radius, options)

Makes a cylinder standing along `y`, such as a barrel, or a wheel once its body turns it on its side.

```lua
local barrel = physics3d.newCylinder(0.9, 0.3, {density = 300})
```

### physics3d.newTaperedCylinder(height, topRadius, bottomRadius, options)

Makes a cylinder whose top and bottom have radii of their own, such as a bucket or a cone with a flat top.

```lua
local bucket = physics3d.newTaperedCylinder(0.4, 0.15, 0.12)
```

### physics3d.newConvexHull(points, options)

Makes the smallest convex solid that holds the points, from a `FloatBuffer` of three numbers for each point or a list of `Vec3` values. A hull takes from 4 to 65536 points within 1000 meters of its origin along each axis, which spread along every axis by at least a millimeter and a thousandth of the length of the hull, since the physics places every point on a face of the hull within a millimeter. A hull is the shape of a rock, a bottle or a simplified model.

```lua
local rock = physics3d.newConvexHull({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0.6, 0.6, 0.6}})
```

### physics3d.newMesh(positions, indices)

Makes a triangle mesh from the positions and the optional indices that [`spatial3d.newMeshBvh`](spatial3d.md) takes, or from a `MeshBvh` itself with `physics3d.newMesh(meshBvh)`. The positions are a `FloatBuffer` of three numbers for each vertex or a list of `Vec3` values, and the indices name three vertices for each triangle, counted from 1. Without indices, the positions list every triangle in turn. The corners of a triangle run counter-clockwise on its front. Triangles without area are left out. A mesh is the shape of a level or a building that never moves.

```lua
local ground = physics3d.newMesh({{-50, 0, -50}, {50, 0, -50}, {50, 0, 50}, {-50, 0, 50}}, {1, 3, 2, 1, 4, 3})
```

### physics3d.newHeightField(heights, sampleCount, options)

Makes a terrain of `sampleCount` rows of `sampleCount` heights, from a `FloatBuffer` or a list of numbers. The rows run along `z`, so the height of column `x` and row `z`, both counted from 1, is at `(z - 1) * sampleCount + x`. A sample lies at `offset + scale * (x - 1, height, z - 1)`. The side takes from 3 to 4096 samples.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `scale` | Vec3 or `{x, y, z}` | `{1, 1, 1}` | The meters between samples along `x` and `z`, and the factor of the heights. Each part is above zero. |
| `offset` | Vec3 or `{x, y, z}` | `{0, 0, 0}` | Where the first sample lies. |
| `holes` | list of integers | none | The samples without collision, counted from 1, such as the mouth of a cave. |

```lua
local heights = collections.newFloatBuffer(129 * 129)
local terrain = physics3d.newHeightField(heights, 129, {scale = {2, 40, 2}, offset = {-128, 0, -128}})
```

### physics3d.newPlane(normal, distance, options)

Makes the half of the space under a plane, whose `normal` points out of it and which lies `distance` meters from the origin along its normal. The plane collides within a cube of twice `halfExtent` meters around the origin of its body, `1000` unless the option says otherwise. A plane stands alone and is never the child of a compound.

```lua
local floor = physics3d.newPlane({0, 1, 0}, 0, {halfExtent = 200})
```

### physics3d.newCompound(children, options)

Makes one shape of the shapes of `children`, a list of tables `{shape, position, rotation}` in the space of the compound, such as a chair made of boxes. A compound takes up to 65536 children and no plane. With the option `mutable` set to `true`, each body that takes the compound gets a copy of its own whose children the body adds, removes and moves while it lives, which suits an object that grows or loses parts. A static compound needs at least one child, and a mutable one may start empty.

```lua
local chair = physics3d.newCompound({
    {shape = physics3d.newBox({0.5, 0.05, 0.5}), position = {0, 0.45, 0}},
    {shape = physics3d.newBox({0.5, 0.5, 0.05}), position = {0, 0.7, -0.225}},
})
```

### physics3d.newScaled(shape, scale)

Makes a shape that scales another by a `Vec3` or one number for every axis, which reuses one shape at many sizes. Spheres and capsules take the same scale on every axis, and cylinders the same scale on `x` and `z`. A negative scale mirrors the shape, and a scale of zero on an axis raises an error.

```lua
local tower = physics3d.newScaled(crate, {1, 4, 1})
```

### physics3d.newOffsetCenterOfMass(shape, offset)

Makes a shape that moves the center of mass of another by `offset` without moving the shape, such as the low center of mass that keeps a car on its wheels.

```lua
local car = physics3d.newOffsetCenterOfMass(physics3d.newBox({1.8, 1.2, 4.2}), {0, -0.4, 0})
```

### Asynchronous shapes

The functions `physics3d.newConvexHullAsync`, `physics3d.newMeshAsync` and `physics3d.newHeightFieldAsync` take the arguments of the functions without `Async` and build the shape on the task pool, so a large mesh or terrain never holds up a frame. Each returns a promise that resolves with the shape, or rejects with the sentence of the problem, which `await` returns as its second value.

```lua
async.spawn(function()
    local level, problem = physics3d.newMeshAsync(positions, indices):await()
    if level == nil then
        print(problem)
    end
end)
```

### Shape3D properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `kind` | string | read | `sphere`, `box`, `capsule`, `taperedCapsule`, `cylinder`, `taperedCylinder`, `convexHull`, `mesh`, `heightField`, `plane`, `staticCompound`, `mutableCompound`, `scaled` or `offsetCenterOfMass`. |
| `backend` | string | read | The name of the backend that made the shape. |
| `bounds` | Box | read | The box that holds the shape in its own space. |
| `centerOfMass` | Vec3 | read | The center of mass in the space of the shape. |
| `volume` | number | read | The volume in cubic meters, 0 for a mesh, a heightfield and a plane. |
| `mass` | number | read | The mass in kilograms that the density gives the shape, 0 for a mesh, a heightfield and a plane. |
| `solid` | boolean | read | Whether the shape has a volume, so a dynamic body takes it. |

```lua
local crate = physics3d.newBox({1, 1, 1}, {density = 600})
print(crate.kind, crate.mass, crate.bounds.max.y) -- box 600.0 0.5
```

### Shape errors

| Message | Cause |
| --- | --- |
| `The radius of a sphere must be a finite number above zero.` and the other sentences that name a size of a shape | A size that is zero, negative or not a number. |
| `The convex radius of a box must be from 0 to <largest>, half of its smallest size.` | A convex radius larger than the shape. |
| `The height of a capsule, from the bottom of one cap to the top of the other, must be more than twice its radius. Make a sphere for a shorter one.` | A capsule shorter than its caps. |
| `The points of a convex hull lie too close to one plane or one line to hold a volume that the physics builds. Spread them along every axis by at least a millimeter and a thousandth of the length of the hull.` | A flat hull, or a hull thinner than a millimeter or than a thousandth of its length. |
| `The points of a convex hull lie within 1000 meters of its origin along each axis. Place the points around the origin of the hull and move its body instead.` | A point of a hull farther than 1000 meters from its origin along an axis. |
| `Item <item> of the indices of a mesh names vertex <index>, but the vertex count of the mesh is <count>.` | A mesh whose indices pass its vertices. |
| `A triangle mesh needs at least one triangle with an area, and every triangle of this one has its corners on one line.` | A mesh of flat triangles alone. |
| `A heightfield of <side> samples on each side needs <count> heights, and it has <found>.` | Heights that do not fill the grid. |
| `A plane stands alone and is never the child of a static compound. Give the plane a body of its own.` | A plane among the children of a compound. |
| `The scale does not suit the shape it scales.` and the rest of its sentence | A scale that the kind of the shape does not take. |
| `A triangle mesh holds more triangles than the physics takes in one shape. Split it into several bodies.` | A mesh larger than the backend holds. |
