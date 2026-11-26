# 📖 `haylen.physics3d`

The module `haylen.physics3d` makes 3D physics worlds in meters, which Jolt Physics simulates unless a plugin selects another backend. This page lists the functions of the module and every member of its worlds and shapes. The [3D physics guide](../3d/physics.md) explains worlds, steps, threads, limits, shapes and backends.

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

## PhysicsWorld3D

A `PhysicsWorld3D`, named `haylen.PhysicsWorld3D` in messages, holds the bodies of a 3D simulation and moves them when the app steps it. Every member of a destroyed world, but `destroyed` and `destroy`, raises `The 3D physics world was destroyed.`.

### world:step(seconds)

Advances the world by the time in seconds, which is zero or more. Step a world in the `fixedUpdate` of a scene with the time it receives, so the simulation keeps the same step whatever the frame rate. A negative time, or one that is not a number, raises `A step of a 3D physics world takes a time of zero seconds or more.`.

```lua
function scene:fixedUpdate(seconds)
    self.world:step(seconds)
end
```

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
| `backend` | string | read | The name of the backend that simulates the world. |
| `bodyCount` | integer | read | The bodies of the world. |
| `activeBodyCount` | integer | read | The bodies that are awake. |
| `destroyed` | boolean | read | Whether the world was destroyed. |

A value written out of its range raises the same sentence as the option, and the world keeps the value it had.

```lua
world.gravity = {0, -3.71, 0}
world.velocitySteps = 14
```

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

Makes the smallest convex solid that holds the points, from a `FloatBuffer` of three numbers for each point or a list of `Vec3` values. A hull takes from 4 to 65536 points that spread along every axis. A hull is the shape of a rock, a bottle or a simplified model.

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
| `The points of a convex hull lie on one plane or one line, so they hold no volume. Give it points that spread along every axis.` | A flat hull. |
| `Item <item> of the indices of a mesh names vertex <index>, but the vertex count of the mesh is <count>.` | A mesh whose indices pass its vertices. |
| `A triangle mesh needs at least one triangle with an area, and every triangle of this one has its corners on one line.` | A mesh of flat triangles alone. |
| `A heightfield of <side> samples on each side needs <count> heights, and it has <found>.` | Heights that do not fill the grid. |
| `A plane stands alone and is never the child of a static compound. Give the plane a body of its own.` | A plane among the children of a compound. |
| `The scale does not suit the shape it scales.` and the rest of its sentence | A scale that the kind of the shape does not take. |
| `A triangle mesh holds more triangles than the physics takes in one shape. Split it into several bodies.` | A mesh larger than the backend holds. |
