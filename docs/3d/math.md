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

## Comparing with tolerance

Float numbers round, so two results that should be equal often differ in their last digits. Compare them with `approximately`, which accepts a tolerance relative to the size of the numbers, and keep `==` for values that were copied, not computed.

```lua
local m = require('haylen.math')

local measured = m.vec3(1.0000001, 2, 3)
print(measured == m.vec3(1, 2, 3), measured:approximately({1, 2, 3})) -- false true
```
