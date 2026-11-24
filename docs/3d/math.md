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
