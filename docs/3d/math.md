# 📏 3D math

This guide explains the math types of 3D space and how they fit together. They live in `haylen.math`, next to the 2D types, and the [reference of `haylen.math`](../lua-api/math.md) lists every function with its options and errors. The axes, units and angles they follow are on the [coordinates page](coordinates.md).

## Vectors

A `Vec3` carries three numbers, `x`, `y` and `z`. The same type holds two different ideas:

- A point is a place, such as the position of a character or the corner of a box.
- A direction is a way to go, often with a length, such as a velocity, a normal or the offset from one point to another.

The difference between two points is a direction, and a point plus a direction is another point. A `Vec4` adds a fourth number, `w`, which tells the two ideas apart when a matrix moves them: a point has `w` of `1` and moves with a translation, while a direction has `w` of `0` and only turns.

```lua
local m = require('haylen.math')

local player = m.vec3(0, 0, 0)
local enemy = m.vec3(3, 0, -4)
local offset = enemy - player
print(offset:length()) -- 5.0

local asPoint = m.vec4(enemy, 1)
local asDirection = m.vec4(offset, 0)
print(asPoint.w, asDirection.w) -- 1.0 0.0
```

## Dot and cross products

Two products answer most questions about directions. The dot product tells how much two directions agree: for directions of length one it is the cosine of the angle between them, so it is positive in front, zero to the side and negative behind. The cross product returns a direction at a right angle to both, by the right hand, such as the normal of a triangle from two of its edges.

```lua
local m = require('haylen.math')

local facing = m.forward()
local toEnemy = (m.vec3(1, 0, -5) - m.vec3(0, 0, 0)):normalized()
local inFront = facing:dot(toEnemy) > 0

local a, b, c = m.vec3(0, 0, 0), m.vec3(1, 0, 0), m.vec3(0, 0, -1)
local normal = (b - a):cross(c - a):normalized()
print(inFront, normal) -- true Vec3(0.0, 1.0, 0.0)
```

## Directions that must exist

Many operations need a direction: normalizing a vector, projecting onto another vector or aiming along one. A vector of length zero has no direction, and neither has a vector with an infinite component or one that is not a number. The engine never makes up a direction for them. It raises an error that says what was wrong, so a mistake shows where it happens instead of turning into a wrong result later. When a zero vector is a normal case, such as the input of a stick at rest, check it first.

```lua
local m = require('haylen.math')

local function moveDirection(input)
    if input:isZero() then
        return nil
    end
    return input:normalized()
end

print(moveDirection(m.vec3()), moveDirection(m.vec3(0, 0, -2))) -- nil Vec3(0.0, 0.0, -1.0)
print(pcall(m.vec3().normalized, m.vec3())) -- false ...: The vector has no direction, because its length is zero.
```

## Matrices

A matrix moves vectors from one space to another. A `Mat4` holds a whole placement, a turn, a scale and a move, and it also holds what a camera does: the view, which moves the world in front of the camera, and the projection, which squeezes what the camera sees into the box of the screen. Matrices apply from right to left, so `projection * view * model` places a point of a model in the world, then in front of the camera, then on the screen. The functions `m.compose`, `m.lookAt` and `m.perspective` build the three.

```lua
local m = require('haylen.math')

local model = m.compose({2, 0, -6}, m.fromAxisAngle(m.up(), 0.3))
local view = m.lookAt({0, 1.7, 0}, {2, 0, -6})
local projection = m.perspective({fieldOfView = m.radians(60), aspect = 16 / 9})
local onScreen = (projection * view * model):apply({0, 0, 0})
print(onScreen.x, onScreen.y) -- the center of the model, from -1 to 1 across the view
```

A matrix maps a point and a direction differently. The method `apply` moves a point, translation included, while `applyVector` only turns and scales a direction. Normals need the normal matrix of `normalMatrix`, which keeps them at right angles to a surface that a matrix stretches.

An inverse undoes a matrix, which turns a point of the screen back into the world. A matrix that squashes space onto a plane or a line has no inverse, and asking for one raises an error instead of returning numbers that mean nothing. A `Mat3` holds the turn and scale alone, such as the normal matrix or the linear part of a placement.

## Rotations

A `Quat` holds a rotation in four numbers. It combines with another rotation by multiplying, interpolates smoothly with `slerp` and never locks, so it is the form that the engine keeps and passes. Build one from the form that suits the task:

- An axis and an angle, with `m.fromAxisAngle`, such as a wheel that spins around its axle.
- Euler angles, with `m.fromEuler`, three turns in a named order, such as the yaw, pitch and roll of a camera that a person controls.
- Two directions, with `m.rotationBetween`, such as a foot that tilts to the slope under it.
- A direction to look along, with `m.lookRotation`, such as a turret that aims at its target.

Euler angles are easy to read but have poles: when the middle turn reaches a quarter turn, the outer and inner turns spin around the same axis and one angle is lost. Keep the rotation of an object as a `Quat`, and turn it into Euler angles only to show or edit them.

```lua
local m = require('haylen.math')

local current = m.quat()
local target = m.lookRotation({1, 0, -1})
for frame = 1, 10 do
    current = current:slerp(target, 0.2)
end
print(current:angleTo(target) < 0.1) -- true
```

## Transforms

A `Transform3D` holds a placement as its three parts: a position, a rotation and a scale. The parts are easy to read, to edit and to animate, so a transform is the form that an object keeps, while a matrix is the form that drawing and the GPU take. A child placed in a parent combines as `parent * child`, which lands the child where the parent puts it.

```lua
local m = require('haylen.math')

local ship = m.transform3d({0, 10, 0}, m.fromAxisAngle(m.up(), 0.5))
local cannon = m.transform3d({1.5, 0.2, -2})
local muzzle = (ship * cannon):apply({0, 0, -1})
print(muzzle) -- the tip of the cannon in the world
```

Parts cannot hold everything a chain of matrices holds. When a parent scales its axes by different amounts and a child turns inside it, the child would be sheared, slanted along an axis, and a position, a rotation and a scale cannot slant. So such a chain multiplies the matrices of `transform:matrix()` instead, which keep the shear. The same holds for undoing a transform: `transform:applyInverse` undoes any scale exactly, while `transform:inverse` needs a scale that is the same on every axis.

A skinned mesh, whose vertices follow the joints of a skeleton, blends the placements of several joints for each vertex. Blending matrices pinches the mesh where a joint twists, such as a wrist, so `DualQuat` holds the rotation and translation of a joint in a form that blends without that loss.

## Shapes and ray casts

The shapes of 3D stand for the room that things take, and each suits its own task:

- A `Box` lines up with the axes. It is the cheapest shape to test, so it bounds models, levels and anything that culling and spatial searches sort.
- An `Obb` is a box that turns with its object, such as a car or a crate, and fits it more tightly than a box along the axes.
- A `Sphere` has no direction at all, so it suits blasts, ranges and objects that roll.
- A `Capsule` is a segment with a radius, the usual body of a character, since it slides along walls and steps over edges.
- A `Triangle` is a piece of a mesh, and a `Plane` is the ground or a wall without an end.

A `Ray3D` asks what lies along a line, such as what the player aims at or how far the ground is below a foot. Its cast returns the nearest hit with its distance, point and normal. A ray never slips between two triangles that share an edge, so a ray cast against a mesh finds the surface even through the seams of its triangles. The overlap tests answer whether two shapes touch, and the closest points tell where they come nearest.

```lua
local m = require('haylen.math')

local foot = m.ray3d({3, 1, -2}, -m.up(), 2)
local ground = m.triangle({0, 0, 0}, {0, 0, -10}, {10, 0, 0})
local hit = foot:cast(ground)
print(hit and hit.distance) -- 1.0, so the foot stands one meter above the ground
```

A `Frustum` is the space that a camera sees, which `m.frustum` builds from the view and the projection of the camera. Culling skips the objects whose bounds lie outside it, which saves the work of drawing them. The test is conservative: it never calls a shape outside that reaches inside, so nothing that the camera sees disappears, and it allows a little room past each plane so a shape that rests on one is never cut by rounding.

## Curves

A curve in 3D gives a smooth way through space from a few points, such as the rail of a coaster, the center of a road, a tube or the track that a camera follows. A `Spline3D` holds one, and its kind decides how the points shape it:

- A linear curve runs straight from point to point, for paths of corners.
- A Catmull-Rom curve passes through every point, which suits paths that a person places by hand. Its `alpha` picks how it spaces itself between points: the centripetal default never loops or cusps inside a segment, while `0` and `1` give the uniform and the chordal curves, and its `tension` tightens it toward straight lines.
- A Bezier curve passes through every third point, or every second for a quadratic one, and bends toward the points between, which is how drawing tools shape curves.
- A B-spline stays inside its points without passing through them and is the smoothest of all.
- A NURBS curve gives each point a weight and takes knots of its own, which draws circles and other conics exactly, the form that design tools exchange.

Every kind becomes a NURBS curve inside, so one exact evaluation gives the points, the velocity and the acceleration of all of them. A curve also measures its own length, so `pointAtDistance` walks it at an even speed whatever its parameter does, and `closestPoint` snaps something to it.

```lua
local m = require('haylen.math')

local track = m.spline3d({{0, 0, 0}, {20, 4, -10}, {40, 0, -10}, {60, 2, 0}})
local cart = track:pointAtDistance(25)
local snapped = track:closestPoint({18, 6, -9})
print(cart, snapped.distance)
```

A tube, a ribbon or a camera on a rail needs more than the direction of travel: it needs a frame, which also says where up is along the way. The frames of `spline:frames` carry their normal along the curve by parallel transport, which turns it only as much as the curve turns, so a tube never twists on its own. A path of several curves, `CurvePath3D`, walks them one after another as one length and tells how smoothly each pair joins, so a road made of straight pieces and bends keeps one distance from start to end.

```lua
local m = require('haylen.math')

local straight = m.spline3d({{0, 0, 0}, {10, 0, 0}}, {kind = 'linear'})
local bend = m.spline3d({{10, 0, 0}, {15, 0, 0}, {15, 0, -5}, {15, 0, -10}}, {kind = 'bezier'})
local road = m.curvePath3d({straight, bend})
for _, frame in ipairs(road:frames(12)) do
    print(frame.position, frame.normal)
end
```

Circles and ellipses are conics, so a NURBS curve draws them exactly, and `m.ellipse3d`, `m.arc3d` and `m.arcThrough3d` build those curves from a center and axes, from a start turned around a normal or from three points. They are splines like any other, so a road joins its straight pieces and its bends into one path whose joins keep their direction.

Some curves come from a formula rather than from points, such as the coil of a spring, a spiral staircase or a knot. A `ParametricCurve3D` holds one, from `m.helix`, `m.torusKnot` or `m.lissajous3d`, and it measures, walks and frames itself as a spline does. Its points come out in one call with `curve:sample`, which suits building the mesh of a tube or placing many lights along a coil. Code in C++ makes a parametric curve of any formula that gives the point and its derivatives.

```lua
local m = require('haylen.math')

local coil = m.helix({radius = 0.5, pitch = 0.1, turns = 12})
local ring = m.arc3d({0, 0, 0}, {3, 0, 0}, m.up(), 2 * math.pi)
print(#coil:sample(400), ring.closed) -- 400 true
```

NURBS also make surfaces and volumes. A `NurbsSurface` spreads a grid of points into a smooth sheet with a normal at every place, such as a hull or a patch of terrain, and a `NurbsVolume` bends the space of a block, such as a lattice that deforms a model inside it.

## Noise and random points

Noise gives values that change smoothly from one point to the next, which shapes caves, clouds, terrain and the textures of 3D surfaces. A `Noise3D` samples it in three dimensions, or in four, where the fourth often runs with time so a pattern changes in place. Perlin and simplex noise are gradient noise, smooth hills of values, and value noise is softer and blockier. Worley noise, from `noise:cellular`, measures the distance to scattered points, which draws cells, stones and crystals. A fractal sums octaves of a noise at rising frequencies: plain fBm for rolling terrain, billows for clouds, ridges for mountain crests and ping-pong for bands. Warping the point before sampling bends any pattern into marble or smoke.

```lua
local m = require('haylen.math')

local noise = m.noise3d(12)
local function isRock(x, y, z)
    return noise:fractal(x / 16, y / 16, z / 16, {octaves = 4}) > 0.1
end

print(isRock(3, -8, 20))
```

A `Random` also draws points of 3D shapes: inside or on a sphere, in a cone of directions and inside a box. A random direction is a point on a sphere of radius `1`, and the spread of a shot or the sparks of a fountain is a cone. To spread things evenly without clumps, `m.poissonDisk3d` places points that keep a distance from each other through a box, which suits trees, rocks and stars.

```lua
local m = require('haylen.math')

local random = m.random(9)
local sparks = {}
for index = 1, 20 do
    sparks[index] = random:inCone(m.up(), m.radians(25)) * random:range(4, 8)
end

local rocks = m.poissonDisk3d({volume = m.box({0, 0, 0}, {40, 2, 40}), minimumDistance = 5, random = random})
print(#sparks, #rocks > 0)
```

## Planes

A `Plane` splits space into a front side and a back side, such as the ground, a wall or the surface of water. Its signed distance tells on which side a point lies and how far, and its closest point drops a point onto it. A plane in the space of a camera also clips a projection with `obliqueNearPlane`, which draws only what lies behind a mirror or a portal.

```lua
local m = require('haylen.math')

local water = m.plane(m.up(), 0.5)
local swimmer = m.vec3(3, 0.2, -1)
print(water:signedDistance(swimmer) < 0, water:closestPoint(swimmer)) -- true Vec3(3.0, 0.5, -1.0)
```

## Comparing with tolerance

Float numbers round, so two results that should be equal often differ in their last digits. Compare them with `approximately`, which accepts a tolerance relative to the size of the numbers, and keep `==` for values that were copied, not computed.

```lua
local m = require('haylen.math')

local measured = m.vec3(1.0000001, 2, 3)
print(measured == m.vec3(1, 2, 3), measured:approximately({1, 2, 3})) -- false true
```
