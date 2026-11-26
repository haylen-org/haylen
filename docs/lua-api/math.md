# 📖 `haylen.math`

The module `haylen.math` holds the math that games and apps need, and the value types that every other module passes around: `Vec2`, `Vec3`, `Vec4`, `Mat3`, `Mat4`, `Quat`, `Plane`, `Transform3D`, `DualQuat`, `Rect`, `Color` and `Transform2D`. It also has seeded random numbers, noise, easing curves and scalar helpers. For shapes, it tests circles and segments, casts rays, combines and splits polygons and traces outlines with marching squares. It ends with splines, springs, signal filters, shuffle bags, weighted choices and Poisson disk sampling. Use it for gameplay math, for procedural generation and for any position, area, color or transform that goes to another module. This page has no guide of its own, so it starts with a short tour of the value types. The standard `math` library of Lua stays available for everything else.

```lua
local m = require('haylen.math')
```

The examples name the module `m`, so the standard `math` library keeps its name.

## Value types

These value types carry positions, areas, colors and transforms through the whole engine:

- A [`Vec2`](#vec2) is a 2D vector with `x` and `y`. It is a point, such as a position, or a direction with a length, such as a velocity.
- A [`Vec3`](#vec3) is a 3D vector with `x`, `y` and `z`, and a [`Vec4`](#vec4) adds `w`. The [3D math guide](../3d/math.md) explains how they carry points and directions.
- A [`Mat3`](#mat3) or a [`Mat4`](#mat4) is a matrix that places, views or projects 3D space, a [`Quat`](#quat) is a 3D rotation and a [`Plane`](#plane) is an infinite flat surface. A [`Spherical`](#spherical) or a [`Cylindrical`](#cylindrical) holds a point by its angles around the up axis, and [`SphericalHarmonics`](#sphericalharmonics) hold the light that arrives from every direction. A [`Transform3D`](#transform3d) places an object in 3D, and a [`DualQuat`](#dualquat) blends the joints of a skinned mesh. The [3D shapes](#3d-shapes) are rays, segments, boxes, spheres, oriented boxes, capsules, triangles and frustums.
- A [`Rect`](#rect) is a rectangle that is not rotated, with its top left corner at `x`, `y` and a `width` and `height`.
- A [`Color`](#color) has red, green, blue and alpha components from 0 to 1. Alpha is the opacity, 0 for invisible and 1 for solid.
- A [`Transform2D`](#transform2d) moves, rotates, scales and skews points, such as the placement of a sprite or the chain from a child to its parent.

In 2D, the y axis points down, as on the screen: a larger `y` is lower. In 3D, the y axis points up, as [the 3D coordinates guide](../3d/coordinates.md) shows. Angles are in radians, and a positive angle turns from the positive x axis toward the positive y axis, which is clockwise on the screen. The functions `m.radians` and `m.degrees` convert between radians and degrees.

```lua
local m = require('haylen.math')

local position = m.vec2(100, 50)
local velocity = m.fromAngle(m.radians(90), 30)
local hitbox = m.fromCenter(position, {32, 48})
local tint = m.color('#FF66CCFF')
local placement = m.transform2d(position, m.radians(45), {2, 2})

position = position + velocity
print(position, hitbox:contains(position), tint.b, placement:apply({0, 0}))
```

### Passing values

Every function of every engine module accepts plain tables wherever it expects one of these values, so making a userdata is optional. Use the form that reads best.

| Type | Accepted forms |
| --- | --- |
| `Vec2` | A `Vec2`, `{x = 10, y = 20}` or `{10, 20}`. |
| `Vec3` | A `Vec3`, `{x = 1, y = 2, z = 3}` or `{1, 2, 3}`. |
| `Vec4` | A `Vec4`, `{x = 1, y = 2, z = 3, w = 4}` or `{1, 2, 3, 4}`. |
| `Mat3`, `Mat4` | A matrix of the type, or a table of its 9 or 16 numbers by columns. |
| `Quat` | A `Quat`, `{x = 0, y = 0, z = 0, w = 1}` or `{0, 0, 0, 1}`. |
| `Plane` | A `Plane`, `{normal = {0, 1, 0}, distance = 2}` or `{{0, 1, 0}, 2}`. |
| `Rect` | A `Rect`, `{x = 0, y = 0, width = 100, height = 50}` or `{0, 0, 100, 50}`. |
| `Color` | A `Color`, `'#AARRGGBB'` or `'#RRGGBB'` text where the `#` is optional, `{r = 1, g = 0.5, b = 0, a = 1}` or `{1, 0.5, 0}`. Components go from `0` to `1`, and alpha is `1` when a table leaves it out. |

The functions `m.vec2`, `m.vec3`, `m.vec4`, `m.mat3`, `m.mat4`, `m.quat`, `m.rect` and `m.color` take the same forms as a single argument, which also copies an existing value. These errors come from wrong values:

- A table with a missing component or one that is not a number raises `Expected a number in field "y".`, with the name of the field.
- A value of another type raises an error such as `bad argument #2 to 'drawRect' (Color, color text or table with r, g and b expected, got number)`.
- Color text that is not valid raises an error that ends with `invalid color text, expected #RRGGBB or #AARRGGBB`.

```lua
local m = require('haylen.math')

local a = m.vec2(1, 2) + {x = 3, y = 4}
local b = m.vec2(1, 2) + {3, 4}
print(a == b) -- true

local red = m.color('#FFFF0000')
print(red:lerp({0, 0, 1}, 0.5), red == m.color('FF0000')) -- #FF800080 true
```

### Shared values

The values `Vec2`, `Vec3`, `Vec4`, `Mat3`, `Mat4`, `Quat`, `Plane`, `Spherical`, `Cylindrical`, `SphericalHarmonics`, `Transform3D`, `DualQuat`, `Rect`, `Color`, `Transform2D`, `Random` and `Noise2D` are userdata, objects that the engine owns. Assigning one to another variable shares the same object, so a change made through one variable shows through the other. Operators and methods always return new values, and engine functions copy the values they receive. Copy a value with its constructor, such as `m.vec2(other)`.

Reading a member that a value does not have raises `The type "haylen.Vec2" has no member "z".`, and assigning one raises `The type "haylen.Vec2" has no writable property "z".`, with the type name of the value.

```lua
local m = require('haylen.math')

local spawn = m.vec2(100, 100)
local shared = spawn
shared.x = 150
print(spawn.x) -- 150.0

local copy = m.vec2(spawn)
copy.x = 0
print(spawn.x) -- 150.0
```

## Constants

### m.pi

The number π as a float, half a turn in radians.

```lua
local m = require('haylen.math')

print(m.degrees(m.pi)) -- 180.0
```

### m.tau

The number 2π, one full turn in radians.

```lua
local m = require('haylen.math')

local spokes = 8
for index = 0, spokes - 1 do
    local angle = index * m.tau / spokes
    print(math.cos(angle), math.sin(angle))
end
```

### m.halfPi

The number π/2, a quarter turn in radians.

```lua
local m = require('haylen.math')

local down = m.fromAngle(m.halfPi)
print(m.approximately(down.y, 1)) -- true
```

### m.standardGravity

Standard gravity, `9.80665` meters per second squared, the pull of the Earth at sea level. The weights of 2D physics measure against it, the accelerometers of gamepads read it on a device that lies still, and 3D measures its default gravity with it, as the [coordinates page](../3d/coordinates.md#units) describes.

```lua
local m = require('haylen.math')

local weight = 80 * m.standardGravity
print(weight) -- the weight of 80 kilograms in newtons, about 784.5
```

## Vec2

A `Vec2` is a 2D vector with `x` and `y` components. Make one with `m.vec2` or `m.fromAngle`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `x` | number | read and write | The horizontal component, larger to the right. |
| `y` | number | read and write | The vertical component, larger downward. |

```lua
local m = require('haylen.math')

local position = m.vec2(10, 20)
position.x = position.x + 5
position.y = position.y - 5
print(position.x, position.y) -- 15.0 15.0
```

### m.vec2(x, y)

Returns a new `Vec2`. Both components are `0` when left out.

```lua
local m = require('haylen.math')

local origin = m.vec2()
local velocity = m.vec2(120, -80)
print(origin, velocity) -- Vec2(0.0, 0.0) Vec2(120.0, -80.0)
```

### m.vec2(value)

Returns a new `Vec2` from any form that a `Vec2` argument accepts: a `Vec2`, `{x = 10, y = 20}` or `{10, 20}`. A `Vec2` gives an independent copy.

```lua
local m = require('haylen.math')

local spawn = m.vec2({x = 64, y = 128})
local respawn = m.vec2(spawn)
respawn.y = 0
print(spawn, respawn) -- Vec2(64.0, 128.0) Vec2(64.0, 0.0)
```

### m.fromAngle(radians, length)

Returns a new `Vec2` that points at the angle `radians`, with the given `length`, `1` by default.

```lua
local m = require('haylen.math')

local velocity = m.fromAngle(m.radians(30), 400)
print(velocity:length()) -- close to 400
```

### vec:length()

Returns the length of the vector.

```lua
local m = require('haylen.math')

print(m.vec2(3, 4):length()) -- 5.0
```

### vec:lengthSquared()

Returns the length times itself. It is cheaper than `vec:length` and enough to compare distances.

```lua
local m = require('haylen.math')

local offset = m.vec2(30, 40)
local radius = 60
print(offset:lengthSquared() < radius * radius) -- true
```

### vec:angle()

Returns the angle of the vector in radians, from -π to π.

```lua
local m = require('haylen.math')

local toTarget = m.vec2(0, 50)
print(m.degrees(toTarget:angle())) -- 90.0
```

### vec:rotated(radians)

Returns the vector turned by `radians`, clockwise on the screen for a positive angle.

```lua
local m = require('haylen.math')

local down = m.vec2(1, 0):rotated(m.pi / 2)
print(m.approximately(down.x, 0), m.approximately(down.y, 1)) -- true true
```

### vec:perpendicular()

Returns the vector `(-y, x)`, the vector turned by a quarter turn.

```lua
local m = require('haylen.math')

local wall = m.vec2(10, 0)
print(wall:perpendicular()) -- Vec2(-0.0, 10.0)
```

### vec:clampedLength(maximum)

Returns the vector shortened to the length `maximum` when it is longer, or the same vector otherwise.

```lua
local m = require('haylen.math')

local velocity = m.vec2(300, 400)
print(velocity:clampedLength(250):length()) -- 250.0
```

### vec:isZero()

Returns `true` when both components are exactly `0`.

```lua
local m = require('haylen.math')

local input = m.vec2(0, 0)
if input:isZero() then
    print('Standing still.')
end
```

### vec:normalized()

Returns a vector of length `1` in the same direction. A vector of length zero has no direction and raises `The vector has no direction, because its length is zero.`, and one with a component that is not a finite number raises `The vector has no direction, because one of its components is not a finite number.`. When a zero vector is a normal case, such as a stick at rest, check `vec:isZero()` first.

```lua
local m = require('haylen.math')

print(m.vec2(0, -8):normalized()) -- Vec2(0.0, -1.0)

local stick = m.vec2(0, 0)
local heading = not stick:isZero() and stick:normalized() or nil
print(heading) -- nil
```

### vec:dot(other)

Returns the dot product with `other`, `x * other.x + y * other.y`. For two vectors of length 1, it is 1 when they point the same way, 0 when they are at a right angle and -1 when they point opposite ways.

```lua
local m = require('haylen.math')

local facing = m.vec2(1, 0)
local toEnemy = m.vec2(5, 1):normalized()
print(facing:dot(toEnemy) > 0.7) -- true
```

### vec:cross(other)

Returns the 2D cross product, `x * other.y - y * other.x`. It is positive when `other` points clockwise from the vector on the screen, and negative when it points the other way.

```lua
local m = require('haylen.math')

local heading = m.vec2(1, 0)
print(heading:cross({0, 1})) -- 1.0
```

### vec:distance(other)

Returns the distance to the point `other`.

```lua
local m = require('haylen.math')

local player = m.vec2(0, 0)
print(player:distance({30, 40})) -- 50.0
```

### vec:distanceSquared(other)

Returns the distance to `other` times itself. It is cheaper than `vec:distance` and enough to compare distances.

```lua
local m = require('haylen.math')

local tower = m.vec2(400, 400)
local range = 150
print(tower:distanceSquared({500, 480}) <= range * range) -- true
```

### vec:min(other)

Returns a `Vec2` with the smaller `x` and the smaller `y` of the two vectors.

```lua
local m = require('haylen.math')

print(m.vec2(10, 80):min({40, 20})) -- Vec2(10.0, 20.0)
```

### vec:max(other)

Returns a `Vec2` with the larger `x` and the larger `y` of the two vectors.

```lua
local m = require('haylen.math')

print(m.vec2(10, 80):max({40, 20})) -- Vec2(40.0, 80.0)
```

### vec:floor()

Returns the vector with both components rounded down.

```lua
local m = require('haylen.math')

local tileSize = 64
local tile = (m.vec2(200, 70) / tileSize):floor()
print(tile) -- Vec2(3.0, 1.0)
```

### vec:round()

Returns the vector with both components rounded to the nearest whole number, with halves rounded away from zero. Rounding positions before drawing keeps pixel art sharp.

```lua
local m = require('haylen.math')

print(m.vec2(10.5, -3.5):round()) -- Vec2(11.0, -4.0)
```

### vec:lerp(to, t)

Returns the point at `t` of the way toward `to`, a linear interpolation. The value `t` is not clamped, so values outside `0` to `1` go past the ends.

```lua
local m = require('haylen.math')

print(m.vec2(0, 0):lerp({100, 50}, 0.5)) -- Vec2(50.0, 25.0)
```

### vec:unpack()

Returns `x` and `y` as two numbers.

```lua
local m = require('haylen.math')

local x, y = m.vec2(12, 34):unpack()
print(x + y) -- 46.0
```

### Vec2 operators

The operators `a + b` and `a - b` add and subtract component by component. The operators `a * b` and `a / b` multiply and divide component by component when both are vectors, and `v * s`, `s * v` and `v / s` scale by a number. The operator `-v` negates, `a == b` compares both components and `tostring(v)` gives `Vec2(x, y)`. The operand that is not a `Vec2` may be a point table.

```lua
local m = require('haylen.math')

local position = m.vec2(100, 100)
local velocity = m.vec2(30, -10)
position = position + velocity * 0.5
print(position) -- Vec2(115.0, 95.0)
print(-velocity, velocity / 10, m.vec2(2, 3) * {4, 5}) -- Vec2(-30.0, 10.0) Vec2(3.0, -1.0) Vec2(8.0, 15.0)
print(position == m.vec2(115, 95)) -- true
```

## Vec3

A `Vec3` is a 3D vector with `x`, `y` and `z` components. It is a point, such as a position in a 3D world, or a direction with a length, such as a velocity. The 3D space of the engine is right-handed with `y` up, and a camera looks along negative `z`, as [the 3D coordinates guide](../3d/coordinates.md) shows. Make one with `m.vec3`, `m.right`, `m.up` or `m.forward`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `x` | number | read and write | The component along the right axis. |
| `y` | number | read and write | The component along the up axis. |
| `z` | number | read and write | The component along the axis that points back toward a camera. |

```lua
local m = require('haylen.math')

local position = m.vec3(1, 2, 3)
position.y = position.y + 0.5
print(position) -- Vec3(1.0, 2.5, 3.0)
```

### m.vec3(x, y, z)

Returns a new `Vec3`. Every component is `0` when left out.

```lua
local m = require('haylen.math')

print(m.vec3(), m.vec3(4, 0, -2)) -- Vec3(0.0, 0.0, 0.0) Vec3(4.0, 0.0, -2.0)
```

### m.vec3(value)

Returns a new `Vec3` from any form that a `Vec3` argument accepts: a `Vec3`, `{x = 1, y = 2, z = 3}` or `{1, 2, 3}`. A `Vec3` gives an independent copy.

```lua
local m = require('haylen.math')

local spawn = m.vec3({x = 0, y = 1, z = -5})
local copy = m.vec3(spawn)
copy.z = 0
print(spawn.z, copy.z) -- -5.0 0.0
```

### m.right()

Returns the direction `Vec3(1, 0, 0)`, the right axis of the 3D space.

```lua
local m = require('haylen.math')

local strafe = m.right() * 2
print(strafe) -- Vec3(2.0, 0.0, 0.0)
```

### m.up()

Returns the direction `Vec3(0, 1, 0)`, the up axis of the 3D space, which gravity pulls against.

```lua
local m = require('haylen.math')

local jump = m.up() * 4.5
print(jump) -- Vec3(0.0, 4.5, 0.0)
```

### m.forward()

Returns the direction `Vec3(0, 0, -1)`, where a camera without rotation looks.

```lua
local m = require('haylen.math')

local step = m.forward() * 1.5
print(step) -- Vec3(0.0, 0.0, -1.5)
```

### vec3:length()

Returns the length of the vector.

```lua
local m = require('haylen.math')

print(m.vec3(2, 3, 6):length()) -- 7.0
```

### vec3:lengthSquared()

Returns the length times itself. It is cheaper than `vec3:length` and enough to compare lengths.

```lua
local m = require('haylen.math')

local speed = m.vec3(3, 0, 4)
print(speed:lengthSquared() > 4 * 4) -- true
```

### vec3:isZero()

Returns `true` when all three components are exactly `0`.

```lua
local m = require('haylen.math')

print(m.vec3():isZero(), m.vec3(0, 0, 0.1):isZero()) -- true false
```

### vec3:isFinite()

Returns `true` when no component is infinite or not a number.

```lua
local m = require('haylen.math')

print(m.vec3(1, 2, 3):isFinite(), m.vec3(1 / 0, 0, 0):isFinite()) -- true false
```

### vec3:normalized()

Returns a vector of length `1` in the same direction. It keeps the direction of vectors that are very short or very long too. A vector without a direction raises an error: `The vector has no direction, because its length is zero.` for a zero vector, and `The vector has no direction, because one of its components is not a finite number.` for a vector with an infinite component or one that is not a number. Check `vec3:isZero()` first where a zero vector can happen, such as an input that rests.

```lua
local m = require('haylen.math')

local toTarget = m.vec3(10, 0, -10) - m.vec3(0, 0, 0)
print(toTarget:normalized()) -- Vec3(0.70710..., 0.0, -0.70710...)
```

### vec3:dot(other)

Returns the dot product with `other`, `x * other.x + y * other.y + z * other.z`. For two vectors of length 1, it is 1 when they point the same way, 0 when they are at a right angle and -1 when they point opposite ways.

```lua
local m = require('haylen.math')

local facing = m.forward()
local toEnemy = m.vec3(1, 0, -4):normalized()
print(facing:dot(toEnemy) > 0.9) -- true
```

### vec3:cross(other)

Returns the cross product with `other`, a vector at a right angle to both. It follows the right hand, so the cross product of `m.right()` and `m.up()` points back toward the camera, opposite to `m.forward()`. Its length is the area of the parallelogram the two vectors span.

```lua
local m = require('haylen.math')

local edge1 = m.vec3(1, 0, 0)
local edge2 = m.vec3(0, 0, -1)
print(edge1:cross(edge2)) -- Vec3(0.0, 1.0, 0.0)
```

### vec3:distance(other)

Returns the distance to the point `other`.

```lua
local m = require('haylen.math')

print(m.vec3(0, 0, 0):distance({2, 3, 6})) -- 7.0
```

### vec3:distanceSquared(other)

Returns the distance to `other` times itself. It is cheaper than `vec3:distance` and enough to compare distances.

```lua
local m = require('haylen.math')

local range = 5
print(m.vec3(0, 0, 0):distanceSquared({3, 0, 3}) <= range * range) -- true
```

### vec3:project(onto)

Returns the part of the vector that runs along `onto`. The vector `onto` needs a direction, so a zero vector raises `A vector projects only onto a vector with a direction, and this one has a zero or infinite length.`

```lua
local m = require('haylen.math')

local velocity = m.vec3(3, 4, 5)
print(velocity:project(m.up())) -- Vec3(0.0, 4.0, 0.0)
```

### vec3:projectOnPlane(normal)

Returns the part of the vector that lies in the plane through the origin with the given `normal`, such as a velocity that slides along a wall. The normal needs a direction, with the error of `vec3:project`.

```lua
local m = require('haylen.math')

local velocity = m.vec3(3, -4, 5)
print(velocity:projectOnPlane(m.up())) -- Vec3(3.0, 0.0, 5.0)
```

### vec3:reflect(normal)

Returns the direction mirrored by a surface with the given `normal`, which has a length of `1`, such as a ball that bounces off a wall.

```lua
local m = require('haylen.math')

local falling = m.vec3(1, -1, 0)
print(falling:reflect(m.up())) -- Vec3(1.0, 1.0, 0.0)
```

### vec3:refract(normal, ratio)

Returns the direction that bends through a surface, such as light that enters water, or `nil` when the light reflects totally inside the surface. The vector and `normal` have a length of `1`, the normal faces the incoming direction, and `ratio` is the refractive index of the side the light leaves divided by that of the side it enters.

```lua
local m = require('haylen.math')

local ray = m.vec3(1, -1, 0):normalized()
print(ray:refract(m.up(), 1 / 1.33)) -- bends toward the normal
print(ray:refract(m.up(), 1.5)) -- nil
```

### vec3:lerp(to, t)

Returns the point at `t` of the way toward `to`, a linear interpolation. It returns the vector itself at `0` and `to` at `1`, and `t` is not clamped.

```lua
local m = require('haylen.math')

print(m.vec3(0, 0, 0):lerp({10, 20, 30}, 0.5)) -- Vec3(5.0, 10.0, 15.0)
```

### vec3:min(other)

Returns a `Vec3` with the smaller of each component of the two vectors.

```lua
local m = require('haylen.math')

print(m.vec3(1, 5, 0):min({2, 3, -1})) -- Vec3(1.0, 3.0, -1.0)
```

### vec3:max(other)

Returns a `Vec3` with the larger of each component of the two vectors.

```lua
local m = require('haylen.math')

print(m.vec3(1, 5, 0):max({2, 3, -1})) -- Vec3(2.0, 5.0, 0.0)
```

### vec3:approximately(other, epsilon)

Returns `true` when every component is close to the one of `other`, within `epsilon` times the larger of `1` and the size of the components, `0.00001` by default. A component that is not a number is never close to anything.

```lua
local m = require('haylen.math')

local a = m.vec3(2.000001, 4, 8)
print(a == m.vec3(2, 4, 8), a:approximately({2, 4, 8})) -- false true
```

### vec3:unpack()

Returns `x`, `y` and `z` as three numbers.

```lua
local m = require('haylen.math')

local x, y, z = m.vec3(1, 2, 3):unpack()
print(x + y + z) -- 6.0
```

### Vec3 operators

The operators of a `Vec3` work like those of a [`Vec2`](#vec2-operators): `a + b`, `a - b`, `a * b` and `a / b` work component by component, `v * s`, `s * v` and `v / s` scale by a number, `-v` negates, `a == b` compares every component and `tostring(v)` gives `Vec3(x, y, z)`. The operand that is not a `Vec3` may be a table, and every operator returns a new value.

```lua
local m = require('haylen.math')

local position = m.vec3(0, 1, 0)
local velocity = m.vec3(2, 2, -4)
position = position + velocity * 0.5
print(position, -velocity, position == m.vec3(1, 2, -2)) -- Vec3(1.0, 2.0, -2.0) Vec3(-2.0, -2.0, 4.0) true
```

## Vec4

A `Vec4` is a 4D vector with `x`, `y`, `z` and `w` components. It holds a point of 3D space in homogeneous coordinates, with `w` of `1` for a point and `0` for a direction, or any four numbers that go together, such as a value for a shader. Make one with `m.vec4`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `x`, `y`, `z`, `w` | number | read and write | The four components. |

```lua
local m = require('haylen.math')

local point = m.vec4(m.vec3(1, 2, 3), 1)
point.w = 0
print(point) -- Vec4(1.0, 2.0, 3.0, 0.0)
```

### m.vec4(x, y, z, w)

Returns a new `Vec4`. Every component is `0` when left out.

```lua
local m = require('haylen.math')

print(m.vec4(1, 2, 3, 4)) -- Vec4(1.0, 2.0, 3.0, 4.0)
```

### m.vec4(value)

Returns a new `Vec4` from any form that a `Vec4` argument accepts: a `Vec4`, `{x = 1, y = 2, z = 3, w = 4}` or `{1, 2, 3, 4}`. A `Vec4` gives an independent copy.

```lua
local m = require('haylen.math')

print(m.vec4({1, 2, 3, 4})) -- Vec4(1.0, 2.0, 3.0, 4.0)
```

### m.vec4(xyz, w)

Returns a new `Vec4` with the components of the `Vec3` or table `xyz` and the number `w`.

```lua
local m = require('haylen.math')

local direction = m.vec4(m.forward(), 0)
print(direction) -- Vec4(0.0, 0.0, -1.0, 0.0)
```

### vec4:xyz()

Returns `x`, `y` and `z` as a `Vec3`.

```lua
local m = require('haylen.math')

print(m.vec4(1, 2, 3, 1):xyz()) -- Vec3(1.0, 2.0, 3.0)
```

### vec4:length()

Returns the length of the vector.

```lua
local m = require('haylen.math')

print(m.vec4(1, 1, 1, 1):length()) -- 2.0
```

### vec4:lengthSquared()

Returns the length times itself.

```lua
local m = require('haylen.math')

print(m.vec4(1, 2, 2, 4):lengthSquared()) -- 25.0
```

### vec4:isZero()

Returns `true` when all four components are exactly `0`.

```lua
local m = require('haylen.math')

print(m.vec4():isZero()) -- true
```

### vec4:isFinite()

Returns `true` when no component is infinite or not a number.

```lua
local m = require('haylen.math')

print(m.vec4(0, 0, 0, 0 / 0):isFinite()) -- false
```

### vec4:normalized()

Returns a vector of length `1` in the same direction, with the errors of [`vec3:normalized`](#vec3normalized) for a vector without a direction.

```lua
local m = require('haylen.math')

print(m.vec4(0, 0, 0, 3):normalized()) -- Vec4(0.0, 0.0, 0.0, 1.0)
```

### vec4:dot(other)

Returns the dot product with `other`, the sum of the products of the four pairs of components.

```lua
local m = require('haylen.math')

print(m.vec4(1, 2, 3, 4):dot({1, 1, 1, 1})) -- 10.0
```

### vec4:distance(other)

Returns the distance to `other`.

```lua
local m = require('haylen.math')

print(m.vec4():distance({1, 1, 1, 1})) -- 2.0
```

### vec4:distanceSquared(other)

Returns the distance to `other` times itself.

```lua
local m = require('haylen.math')

print(m.vec4():distanceSquared({1, 1, 1, 1})) -- 4.0
```

### vec4:lerp(to, t)

Returns the vector at `t` of the way toward `to`, without clamping `t`.

```lua
local m = require('haylen.math')

print(m.vec4():lerp({2, 4, 6, 8}, 0.5)) -- Vec4(1.0, 2.0, 3.0, 4.0)
```

### vec4:min(other)

Returns a `Vec4` with the smaller of each component of the two vectors.

```lua
local m = require('haylen.math')

print(m.vec4(1, 5, 0, 9):min({2, 3, 1, -9})) -- Vec4(1.0, 3.0, 0.0, -9.0)
```

### vec4:max(other)

Returns a `Vec4` with the larger of each component of the two vectors.

```lua
local m = require('haylen.math')

print(m.vec4(1, 5, 0, 9):max({2, 3, 1, -9})) -- Vec4(2.0, 5.0, 1.0, 9.0)
```

### vec4:approximately(other, epsilon)

Returns `true` when every component is close to the one of `other`, like [`vec3:approximately`](#vec3approximatelyother-epsilon).

```lua
local m = require('haylen.math')

print(m.vec4(1, 2, 3, 4):approximately({1, 2, 3, 4.000001})) -- true
```

### vec4:unpack()

Returns `x`, `y`, `z` and `w` as four numbers.

```lua
local m = require('haylen.math')

local x, y, z, w = m.vec4(1, 2, 3, 4):unpack()
print(x + y + z + w) -- 10.0
```

### Vec4 operators

The operators of a `Vec4` work like those of a [`Vec3`](#vec3-operators), and `tostring(v)` gives `Vec4(x, y, z, w)`.

```lua
local m = require('haylen.math')

local a = m.vec4(1, 2, 3, 4)
print((a + {1, 1, 1, 1}) * 2, a / 2) -- Vec4(4.0, 6.0, 8.0, 10.0) Vec4(0.5, 1.0, 1.5, 2.0)
```

## Mat3

A `Mat3` is a 3x3 matrix, such as the rotation and scale of a 3D placement or the matrix that turns its normals. It is stored by columns and multiplies a column vector on its right. Make one with `m.mat3`, or get one from `mat4:linear()`, `mat4:normalMatrix()` or `quat:matrix()`. The [3D math guide](../3d/math.md#matrices) explains how matrices chain.

```lua
local m = require('haylen.math')

local stretch = m.mat3(2, 0, 0, 0, 1, 0, 0, 0, 1)
print(stretch * {1, 1, 1}) -- Vec3(2.0, 1.0, 1.0)
```

### m.mat3()

Returns the identity matrix, which leaves every vector as it is.

```lua
local m = require('haylen.math')

print(m.mat3() * {1, 2, 3}) -- Vec3(1.0, 2.0, 3.0)
```

### m.mat3(...)

Returns the matrix of nine numbers listed by columns, so the first three numbers are the first column.

```lua
local m = require('haylen.math')

local matrix = m.mat3(1, 2, 3, 4, 5, 6, 7, 8, 10)
print(matrix:column(1), matrix:row(1)) -- Vec3(1.0, 2.0, 3.0) Vec3(1.0, 4.0, 7.0)
```

### m.mat3(value)

Returns a new `Mat3` from a `Mat3`, which it copies, or from a table of its nine numbers by columns. A table with another count of values raises `A matrix table lists its 9 numbers by columns, and this one holds <count> values.`

```lua
local m = require('haylen.math')

local copy = m.mat3({2, 0, 0, 0, 2, 0, 0, 0, 2})
print(copy:determinant()) -- 8.0
```

### mat3:get(row, column)

Returns the number at `row` and `column`, both from `1` to `3`. Another index raises `A row or a column of this matrix is a whole number from 1 to 3, not <index>.`

```lua
local m = require('haylen.math')

print(m.mat3(1, 2, 3, 4, 5, 6, 7, 8, 10):get(1, 2)) -- 4.0
```

### mat3:column(index)

Returns the column at `index`, from `1` to `3`, as a `Vec3`.

```lua
local m = require('haylen.math')

print(m.mat3():column(2)) -- Vec3(0.0, 1.0, 0.0)
```

### mat3:row(index)

Returns the row at `index`, from `1` to `3`, as a `Vec3`.

```lua
local m = require('haylen.math')

print(m.mat3(1, 2, 3, 4, 5, 6, 7, 8, 10):row(3)) -- Vec3(3.0, 6.0, 10.0)
```

### mat3:transposed()

Returns the matrix with its rows and columns swapped. The transposed matrix of a rotation is its inverse.

```lua
local m = require('haylen.math')

print(m.mat3(1, 2, 3, 4, 5, 6, 7, 8, 10):transposed():row(1)) -- Vec3(1.0, 2.0, 3.0)
```

### mat3:determinant()

Returns the determinant, the factor by which the matrix scales volumes. It is negative for a matrix that mirrors space and zero for one that squashes it flat.

```lua
local m = require('haylen.math')

print(m.mat3(-1, 0, 0, 0, 1, 0, 0, 0, 1):determinant()) -- -1.0
```

### mat3:isInvertible()

Returns `true` when the matrix has an inverse. A matrix that squashes space onto a plane or a line has none, even when rounding leaves its determinant a little above zero.

```lua
local m = require('haylen.math')

print(m.mat3(1, 2, 3, 2, 4, 6, 0, 0, 1):isInvertible()) -- false
```

### mat3:inverse()

Returns the matrix that undoes this one. A matrix without an inverse raises `The matrix has no inverse, because it squashes space flat and its determinant is zero.`

```lua
local m = require('haylen.math')

local matrix = m.mat3(2, 0, 0, 0, 4, 0, 0, 0, 8)
print(matrix:inverse() * {2, 4, 8}) -- Vec3(1.0, 1.0, 1.0)
```

### mat3:rotation()

Returns the rotation of a rotation matrix as a `Quat`. The matrix must be a pure rotation, whose columns have a length of `1` and stand at right angles.

```lua
local m = require('haylen.math')

local turn = m.fromAxisAngle(m.up(), 1)
print(turn:matrix():rotation():approximately(turn)) -- true
```

### mat3:approximately(other, epsilon)

Returns `true` when every number is close to the one of `other`, like [`vec3:approximately`](#vec3approximatelyother-epsilon).

```lua
local m = require('haylen.math')

local matrix = m.mat3(1, 2, 3, 4, 5, 6, 7, 8, 10)
print((matrix * matrix:inverse()):approximately(m.mat3())) -- true
```

### mat3:unpack()

Returns the nine numbers by columns.

```lua
local m = require('haylen.math')

print(select('#', m.mat3():unpack())) -- 9
```

### Mat3 operators

The operator `a * b` multiplies two matrices, and `a * v` maps a `Vec3` or a table of three numbers. A table of nine numbers counts as a matrix. The operator `a == b` compares every number, and `tostring(a)` lists the columns.

```lua
local m = require('haylen.math')

local double = m.mat3(2, 0, 0, 0, 2, 0, 0, 0, 2)
print((double * double) * {1, 1, 1}, double == m.mat3(double)) -- Vec3(4.0, 4.0, 4.0) true
```

## Mat4

A `Mat4` is a 4x4 matrix, such as the placement of an object in the world, the view of a camera or its projection. It is stored by columns and multiplies a column vector on its right, so the matrices of a draw chain as `projection * view * model`. Make one with `m.mat4`, `m.compose`, `m.lookAt` or a projection function. The [3D math guide](../3d/math.md#matrices) explains when to use which.

```lua
local m = require('haylen.math')

local model = m.compose({0, 1, -5})
local view = m.lookAt({0, 2, 5}, {0, 1, -5})
local projection = m.perspective({fieldOfView = m.radians(60), aspect = 16 / 9})
local clip = projection * view * model
print(clip:apply({0, 0, 0})) -- the center of the object on the screen
```

### m.mat4()

Returns the identity matrix.

```lua
local m = require('haylen.math')

print(m.mat4():apply({1, 2, 3})) -- Vec3(1.0, 2.0, 3.0)
```

### m.mat4(...)

Returns the matrix of sixteen numbers listed by columns, so the last four numbers are the translation and the fourth number of `w`.

```lua
local m = require('haylen.math')

local move = m.mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 0, 0, 1)
print(move:translation()) -- Vec3(5.0, 0.0, 0.0)
```

### m.mat4(value)

Returns a new `Mat4` from a `Mat4`, which it copies, or from a table of its sixteen numbers by columns, with the error of [`m.mat3(value)`](#mmat3value) for another count.

```lua
local m = require('haylen.math')

local copy = m.mat4(m.compose({1, 2, 3}))
print(copy:translation()) -- Vec3(1.0, 2.0, 3.0)
```

### m.compose(translation, rotation, scale)

Returns the matrix that scales by `scale`, then turns by the `Quat` `rotation`, then moves by `translation`. The rotation is none and the scale is `{1, 1, 1}` when left out. A negative scale mirrors.

```lua
local m = require('haylen.math')

local placement = m.compose({0, 1, 0}, m.fromAxisAngle(m.up(), m.halfPi), {2, 2, 2})
print(placement:apply({1, 0, 0})) -- close to Vec3(0.0, 1.0, -2.0)
```

### m.lookAt(eye, target, up)

Returns the view matrix of a camera at `eye` that looks at `target`, with its up axis as close to `up` as it can, `m.up()` by default. The view puts the camera at the origin and the target along negative `z`. The same point for both raises `The eye and the target of a view are the same point, so the view has no direction.`, and an `up` along the view raises the error of [`m.lookRotation`](#mlookrotationdirection-up).

```lua
local m = require('haylen.math')

local view = m.lookAt({0, 0, 10}, {0, 0, 0})
print(view:apply({0, 0, 0})) -- Vec3(0.0, 0.0, -10.0)
```

### m.perspective(options)

Returns a perspective projection, where far things look smaller. The options are all optional:

| Option | Default | Meaning |
| --- | --- | --- |
| `fieldOfView` | `m.pi / 3` | The vertical angle of the view in radians, above `0` and below `m.pi`. |
| `aspect` | `1` | The width of the view divided by its height. |
| `nearPlane` | `0.1` | The distance of the near plane, above `0`. |
| `farPlane` | `math.huge` | The distance of the far plane, beyond the near plane, or `math.huge` for a view without a far end. |
| `depthRange` | `'zeroToOne'` | The range of depth of the graphics API, `'zeroToOne'` or `'negativeOneToOne'`. |
| `reversedDepth` | `true` | Whether the near plane gets the far end of the range and the far plane the near end. |

The [3D coordinates guide](../3d/coordinates.md#depth) explains the range and the direction of depth. A field of view out of range raises `The field of view of a perspective must be an angle above 0 and below pi radians.`, an aspect that is not above `0` raises `The aspect of a perspective must be a finite number above 0.`, a near plane that is not above `0` raises `The near plane of a perspective must be at a finite distance above 0.`, and a far plane that is not beyond it raises `The far plane of a perspective must lie beyond its near plane.`

```lua
local m = require('haylen.math')

local projection = m.perspective({fieldOfView = m.radians(70), aspect = 16 / 9, nearPlane = 0.1})
print(projection:apply({0, 0, -0.1}).z) -- 1.0, the near plane
```

### m.asymmetricPerspective(options)

Returns a perspective projection whose edges sit at their own angles, such as the view of one eye of a headset. The options `left`, `right`, `bottom` and `top` are the angles of the edges in radians from the direction of the view, negative to the left and below, `-m.pi / 4` and `m.pi / 4` by default. The other options are those of [`m.perspective`](#mperspectiveoptions) without `fieldOfView` and `aspect`. Angles out of a quarter turn, or a left edge that is not left of the right edge or a bottom edge that is not below the top edge, raise `The angles of an asymmetric perspective must lie within a quarter turn of the direction it looks along, with left below right and bottom below top.`

```lua
local m = require('haylen.math')

local eye = m.asymmetricPerspective({left = -0.9, right = 0.6, bottom = -0.8, top = 0.8, nearPlane = 0.05})
print(eye:apply({math.tan(0.6), 0, -1}).x) -- 1.0, the right edge
```

### m.orthographic(options)

Returns an orthographic projection, where things keep their size at any distance, such as a map or the view of a sun for shadows. The options `left`, `right`, `bottom` and `top` are the edges of the view, `-1` and `1` by default, and `nearPlane` and `farPlane` are the distances of its planes along the view, `0` and `100` by default. The options `depthRange` and `reversedDepth` are those of [`m.perspective`](#mperspectiveoptions). Bounds that are not finite numbers, or edges or planes that meet, raise `The bounds of an orthographic projection must be finite numbers, with left apart from right, bottom apart from top and the near plane apart from the far plane.`

```lua
local m = require('haylen.math')

local map = m.orthographic({left = 0, right = 100, bottom = 0, top = 50, nearPlane = 1, farPlane = 20})
print(map:apply({100, 50, -1})) -- Vec3(1.0, 1.0, 1.0)
```

### m.depthRangeConversion(from, to)

Returns the matrix that turns the clip space of the range of depth `from` into that of `to`, when it multiplies a projection on its left. Both are `'zeroToOne'` or `'negativeOneToOne'`.

```lua
local m = require('haylen.math')

local direct = m.perspective({nearPlane = 1, farPlane = 10, reversedDepth = false})
local openGl = m.depthRangeConversion('zeroToOne', 'negativeOneToOne') * direct
print(openGl:apply({0, 0, -1}).z) -- -1.0
```

### mat4:get(row, column)

Returns the number at `row` and `column`, both from `1` to `4`, with the error of [`mat3:get`](#mat3getrow-column) for another index.

```lua
local m = require('haylen.math')

print(m.compose({5, 0, 0}):get(1, 4)) -- 5.0
```

### mat4:column(index)

Returns the column at `index`, from `1` to `4`, as a `Vec4`.

```lua
local m = require('haylen.math')

print(m.compose({5, 6, 7}):column(4)) -- Vec4(5.0, 6.0, 7.0, 1.0)
```

### mat4:row(index)

Returns the row at `index`, from `1` to `4`, as a `Vec4`.

```lua
local m = require('haylen.math')

print(m.mat4():row(4)) -- Vec4(0.0, 0.0, 0.0, 1.0)
```

### mat4:translation()

Returns the translation, the first three numbers of the last column, as a `Vec3`.

```lua
local m = require('haylen.math')

print(m.compose({1, 2, 3}):translation()) -- Vec3(1.0, 2.0, 3.0)
```

### mat4:linear()

Returns the upper left 3x3 part as a `Mat3`, which holds the rotation, the scale and the shear of a placement.

```lua
local m = require('haylen.math')

print(m.compose({1, 2, 3}, nil, {2, 2, 2}):linear():determinant()) -- 8.0
```

### mat4:apply(point)

Returns the point that the matrix maps `point` to, divided by its `w`. For a placement it moves the point. For a projection it gives the normalized device coordinates, from `-1` to `1` across the view and in the range of depth along it.

```lua
local m = require('haylen.math')

print(m.compose({0, 0, -5}):apply({1, 0, 0})) -- Vec3(1.0, 0.0, -5.0)
```

### mat4:applyVector(vector)

Returns the direction that the matrix maps `vector` to. It turns and scales but never translates, so it suits velocities and offsets.

```lua
local m = require('haylen.math')

print(m.compose({0, 0, -5}):applyVector({1, 0, 0})) -- Vec3(1.0, 0.0, 0.0)
```

### mat4:transposed()

Returns the matrix with its rows and columns swapped.

```lua
local m = require('haylen.math')

print(m.compose({5, 0, 0}):transposed():row(4)) -- Vec4(5.0, 0.0, 0.0, 1.0)
```

### mat4:determinant()

Returns the determinant of the matrix.

```lua
local m = require('haylen.math')

print(m.compose({0, 0, 0}, nil, {1, 2, 3}):determinant()) -- 6.0
```

### mat4:isInvertible()

Returns `true` when the matrix has an inverse. An affine matrix has one when its linear part does, like [`mat3:isInvertible`](#mat3isinvertible), and any other matrix when its determinant is not zero.

```lua
local m = require('haylen.math')

print(m.compose({0, 0, 0}, nil, {1, 0, 1}):isInvertible()) -- false
```

### mat4:isAffine()

Returns `true` when the last row is `0, 0, 0, 1`, as in every placement and view. A perspective projection is not affine.

```lua
local m = require('haylen.math')

print(m.compose({1, 2, 3}):isAffine(), m.perspective():isAffine()) -- true false
```

### mat4:inverse()

Returns the matrix that undoes this one, with the error of [`mat3:inverse`](#mat3inverse) for a matrix without an inverse. The inverse of `projection * view` turns a point of the screen back into the world.

```lua
local m = require('haylen.math')

local projection = m.perspective({nearPlane = 0.1, farPlane = 100})
local point = projection:apply({1, 2, -10})
print(projection:inverse():apply(point)) -- close to Vec3(1.0, 2.0, -10.0)
```

### mat4:affineInverse()

Returns the inverse of an affine matrix with fewer operations than `mat4:inverse`. A matrix that is not affine raises `The matrix is not affine, because its last row is not 0, 0, 0 and 1.`, and a singular one the error of `mat4:inverse`.

```lua
local m = require('haylen.math')

local placement = m.compose({1, 2, 3}, m.fromAxisAngle(m.up(), 0.5))
print((placement:affineInverse() * placement):approximately(m.mat4())) -- true
```

### mat4:normalMatrix()

Returns the `Mat3` that maps the normals of a surface that the matrix places, which keeps them at right angles to the surface when the scale is not the same on every axis. A singular linear part raises the error of `mat4:inverse`.

```lua
local m = require('haylen.math')

local stretched = m.compose({0, 0, 0}, nil, {2, 1, 1})
print(stretched:normalMatrix() * {1, 0, 0}) -- Vec3(0.5, 0.0, 0.0)
```

### mat4:decompose()

Returns a table with the `translation` and `scale` as `Vec3` values and the `rotation` as a `Quat`, which `m.compose` puts back together. A mirrored matrix gets a negative scale on `x`. A matrix that is not affine raises `Only an affine matrix, whose last row is 0, 0, 0 and 1, splits into translation, rotation and scale.`, and one that scales an axis to zero raises `The matrix scales an axis to zero or to infinity, so it holds no rotation.`

```lua
local m = require('haylen.math')

local parts = m.compose({4, 5, 6}, m.fromAxisAngle(m.up(), 0.5), {1, 2, 3}):decompose()
print(parts.translation, parts.scale, parts.rotation:angle()) -- Vec3(4.0, 5.0, 6.0), then close to Vec3(1.0, 2.0, 3.0) and 0.5
```

### mat4:obliqueNearPlane(plane, options)

Returns the projection with its near plane replaced by the `Plane` `plane` of the space of the camera, so nothing between the camera and the plane draws, such as what stands behind a mirror or a portal. The plane must face away from the camera, with the camera behind it, and cross the view before its far end. The options `depthRange` and `reversedDepth` are those the projection was built with, `'zeroToOne'` and `true` by default. A plane that faces the camera raises `An oblique near plane must face away from the camera, so the camera must lie behind the plane.`, and one that misses the view raises `An oblique near plane must cross the view before its far end.`

```lua
local m = require('haylen.math')

local projection = m.perspective({nearPlane = 0.1, farPlane = 100})
local mirror = m.planeFromPoint({0, 0, -1}, {0, 0, -5})
local clipped = projection:obliqueNearPlane(mirror)
print(clipped:apply({0, 0, -5}).z) -- 1.0, the new near plane
```

### mat4:approximately(other, epsilon)

Returns `true` when every number is close to the one of `other`, like [`vec3:approximately`](#vec3approximatelyother-epsilon).

```lua
local m = require('haylen.math')

local placement = m.compose({5, 0, 0}, m.fromAxisAngle({1, 1, 0}, 0.4))
print((placement * placement:inverse()):approximately(m.mat4(), 0.0001)) -- true
```

### mat4:unpack()

Returns the sixteen numbers by columns.

```lua
local m = require('haylen.math')

print(select('#', m.mat4():unpack())) -- 16
```

### Mat4 operators

The operator `a * b` multiplies two matrices, so `b` applies first, and `a * v` maps a `Vec4` or a table of four numbers without dividing by `w`. A table of sixteen numbers counts as a matrix. The operator `a == b` compares every number, and `tostring(a)` lists the columns.

```lua
local m = require('haylen.math')

local move = m.compose({1, 0, 0})
print((move * move):translation(), move * m.vec4(0, 0, 0, 1)) -- Vec3(2.0, 0.0, 0.0) Vec4(1.0, 0.0, 0.0, 1.0)
```

## Quat

A `Quat` is a rotation in 3D, a unit quaternion with the components `x`, `y`, `z` and `w`, where `w` is the real part and the identity is `(0, 0, 0, 1)`. A quaternion and its opposite hold the same rotation. Make one with `m.quat`, `m.fromAxisAngle`, `m.fromEuler`, `m.rotationBetween` or `m.lookRotation`. The [3D math guide](../3d/math.md#rotations) explains when to use rotations, Euler angles and matrices.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `x`, `y`, `z`, `w` | number | read and write | The four components, which keep a length of `1` for a rotation. |

```lua
local m = require('haylen.math')

local turn = m.fromAxisAngle(m.up(), m.halfPi)
print(turn:rotate(m.right())) -- close to Vec3(0.0, 0.0, -1.0)
```

### m.quat()

Returns the identity rotation, which does not turn.

```lua
local m = require('haylen.math')

print(m.quat()) -- Quat(0.0, 0.0, 0.0, 1.0)
```

### m.quat(x, y, z, w)

Returns the quaternion with the four components as they are, which must have a length of `1` to be a rotation. Use `quat:normalized()` on components that come from elsewhere.

```lua
local m = require('haylen.math')

local saved = m.quat(0, 0.7071, 0, 0.7071):normalized()
print(saved:angle()) -- close to m.halfPi
```

### m.quat(value)

Returns a new `Quat` from a `Quat`, which it copies, or a table with `x`, `y`, `z` and `w` or the four values in order.

```lua
local m = require('haylen.math')

print(m.quat({0, 0, 0, 1}) == m.quat()) -- true
```

### m.fromAxisAngle(axis, angle)

Returns the rotation by `angle` radians around `axis`, which turns counter-clockwise when the axis points at the viewer. An axis without a direction raises the errors of [`vec3:normalized`](#vec3normalized).

```lua
local m = require('haylen.math')

local spin = m.fromAxisAngle(m.up(), m.radians(45))
print(spin:angle()) -- 0.785...
```

### m.fromEuler(angles, order)

Returns the rotation of the Euler angles `angles`, a `Vec3` of turns in radians around `x`, `y` and `z`. The `order` names the axes from the outer turn to the inner one, `'xyz'`, `'xzy'`, `'yxz'`, `'yzx'`, `'zxy'` or `'zyx'`, `'yxz'` by default. So `'yxz'` turns an object about its own `y` axis first, then about its own `x` axis and last about its own `z` axis, which is the yaw, the pitch and the roll of a camera.

```lua
local m = require('haylen.math')

local yaw, pitch = m.radians(30), m.radians(-10)
local camera = m.fromEuler({pitch, yaw, 0})
print(camera:rotate(m.forward())) -- looks to the left and a little down
```

### m.rotationBetween(from, to)

Returns the shortest rotation that turns the direction `from` onto the direction `to`. For opposite directions it turns half a turn around an axis at a right angle to `from`. A direction of length zero raises the errors of [`vec3:normalized`](#vec3normalized).

```lua
local m = require('haylen.math')

local tilt = m.rotationBetween(m.up(), {1, 1, 0})
print(tilt:rotate(m.up())) -- Vec3(0.70710..., 0.70710..., 0.0)
```

### m.lookRotation(direction, up)

Returns the rotation that turns `m.forward()` onto `direction` and keeps the up axis as close to `up` as it can, `m.up()` by default, like a camera or a head that looks along `direction`. A direction of length zero raises `A rotation that looks along a direction needs a direction with a finite length above zero.`, and an `up` of length zero or along the direction raises `The up direction needs a finite length above zero and must not run along the direction to look along, or it cannot choose how the view turns around that direction.`

```lua
local m = require('haylen.math')

local facing = m.lookRotation({1, 0, -1})
print(facing:rotate(m.forward())) -- Vec3(0.70710..., 0.0, -0.70710...)
```

### quat:rotate(vector)

Returns `vector` turned by the rotation.

```lua
local m = require('haylen.math')

print(m.fromAxisAngle(m.right(), m.halfPi):rotate(m.up())) -- close to Vec3(0.0, 0.0, 1.0)
```

### quat:length()

Returns the length of the quaternion, `1` for a rotation.

```lua
local m = require('haylen.math')

print(m.quat(0, 0, 0, 2):length()) -- 2.0
```

### quat:normalized()

Returns the quaternion with a length of `1`, the rotation it stands for. A quaternion of length zero or with a component that is not finite raises `The quaternion holds no rotation, because its length is zero or not a finite number.`

```lua
local m = require('haylen.math')

print(m.quat(0, 0, 0, 2):normalized()) -- Quat(0.0, 0.0, 0.0, 1.0)
```

### quat:inverse()

Returns the rotation that undoes this one. A quaternion of length zero raises `The quaternion has no inverse, because its length is zero or not a finite number.`

```lua
local m = require('haylen.math')

local turn = m.fromAxisAngle(m.up(), 1)
print((turn * turn:inverse()):approximately(m.quat())) -- true
```

### quat:angle()

Returns how far the rotation turns, from `0` to `m.pi` radians.

```lua
local m = require('haylen.math')

print(m.fromAxisAngle(m.up(), 1):angle()) -- 1.0
```

### quat:axis()

Returns the axis that the rotation turns around by `quat:angle()`, with a length of `1`. A rotation that does not turn raises `The rotation does not turn, so it has no axis.`

```lua
local m = require('haylen.math')

print(m.fromAxisAngle({0, 2, 0}, 1):axis()) -- Vec3(0.0, 1.0, 0.0)
```

### quat:toEuler(order)

Returns the Euler angles of the rotation in the `order` of [`m.fromEuler`](#mfromeulerangles-order), `'yxz'` by default, as a `Vec3` of radians. The middle angle stays between `-m.halfPi` and `m.halfPi`. At a quarter turn of the middle angle, where the outer and inner turns spin around the same axis, the inner angle is `0` and the outer angle takes the whole turn.

```lua
local m = require('haylen.math')

local camera = m.fromEuler({-0.2, 0.5, 0})
print(camera:toEuler()) -- close to Vec3(-0.2, 0.5, 0.0)
```

### quat:matrix()

Returns the rotation as a `Mat3`.

```lua
local m = require('haylen.math')

print(m.fromAxisAngle(m.up(), m.halfPi):matrix():column(1)) -- close to Vec3(0.0, 0.0, -1.0)
```

### quat:swingTwist(axis)

Returns a table with the `twist`, the part of the rotation that turns around `axis`, and the `swing`, the part that turns the axis itself, so `swing * twist` is the rotation. It limits the joints of a skeleton, such as the twist of a forearm around its bone. An axis without a direction raises the errors of [`vec3:normalized`](#vec3normalized).

```lua
local m = require('haylen.math')

local wrist = m.fromAxisAngle(m.right(), 0.4) * m.fromAxisAngle(m.up(), 1.2)
local parts = wrist:swingTwist(m.up())
print(parts.twist:angle(), parts.swing:angle()) -- close to 1.2 and 0.4
```

### quat:dot(other)

Returns the dot product of the four components, which is `1` or `-1` for the same rotation and nears `0` as the rotations differ by half a turn.

```lua
local m = require('haylen.math')

print(m.quat():dot(m.quat())) -- 1.0
```

### quat:angleTo(other)

Returns the angle of the rotation that turns this rotation onto `other`, from `0` to `m.pi` radians.

```lua
local m = require('haylen.math')

print(m.quat():angleTo(m.fromAxisAngle(m.up(), 0.5))) -- 0.5
```

### quat:slerp(to, t)

Returns the rotation at `t` of the way toward `to`, along the shorter arc at a constant speed. It returns the rotation itself at `0` and `to`, or its opposite form on the shorter arc, at `1`.

```lua
local m = require('haylen.math')

local open = m.fromAxisAngle(m.up(), m.halfPi)
print(m.quat():slerp(open, 0.5):angle()) -- close to m.pi / 4
```

### quat:nlerp(to, t)

Returns the rotation at `t` of the way toward `to`, along the shorter arc. It is faster than `quat:slerp`, and its speed varies a little along the arc, which suits small steps such as smoothing every frame.

```lua
local m = require('haylen.math')

local current = m.quat()
local target = m.fromAxisAngle(m.up(), 1)
current = current:nlerp(target, 0.1)
print(current:angle() > 0) -- true
```

### quat:approximately(other, epsilon)

Returns `true` when the two quaternions hold close rotations, with the tolerance of [`vec3:approximately`](#vec3approximatelyother-epsilon). A quaternion and its opposite compare equal.

```lua
local m = require('haylen.math')

local turn = m.fromAxisAngle(m.up(), 1)
print(turn:approximately(m.quat(-turn.x, -turn.y, -turn.z, -turn.w))) -- true
```

### quat:unpack()

Returns `x`, `y`, `z` and `w` as four numbers.

```lua
local m = require('haylen.math')

local x, y, z, w = m.quat():unpack()
print(w) -- 1.0
```

### Quat operators

The operator `a * b` combines two rotations, so `b` turns first and `a` after it. The operator `a == b` compares the four components, and `tostring(q)` gives `Quat(x, y, z, w)`.

```lua
local m = require('haylen.math')

local yaw = m.fromAxisAngle(m.up(), m.halfPi)
local pitch = m.fromAxisAngle(m.right(), 0.3)
print((yaw * pitch):rotate(m.forward())) -- pitched first, then turned
```

## Plane

A `Plane` is an infinite flat surface of the points `p` with `normal:dot(p) == distance`. Its `normal` has a length of `1` and points to its front side. Make one with `m.plane`, `m.planeFromPoint` or `m.planeFromPoints`. Functions that take a plane also accept `{normal = {0, 1, 0}, distance = 2}` or `{{0, 1, 0}, 2}`, whose normal they scale to a length of `1`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `normal` | `Vec3` | read and write | The direction the front side faces, with a length of `1`. A normal of another length raises `The normal of a plane must have a length of one. Make the plane with "plane" of "haylen.math", which scales its normal.` |
| `distance` | number | read and write | How far the plane lies from the origin along its normal. |

```lua
local m = require('haylen.math')

local water = m.plane(m.up(), 0.5)
print(water:signedDistance({0, 2, 0})) -- 1.5
```

### m.plane(normal, distance)

Returns the plane of the points `p` with `normal:dot(p) == distance`, and scales both so the normal has a length of `1`. A normal without a direction raises the errors of [`vec3:normalized`](#vec3normalized).

```lua
local m = require('haylen.math')

print(m.plane({0, 2, 0}, 4)) -- Plane((0.0, 1.0, 0.0), 2.0)
```

### m.planeFromPoint(normal, point)

Returns the plane through `point` whose front side faces along `normal`.

```lua
local m = require('haylen.math')

local wall = m.planeFromPoint({-1, 0, 0}, {10, 0, 0})
print(wall:signedDistance({0, 0, 0})) -- 10.0
```

### m.planeFromPoints(a, b, c)

Returns the plane through three points, whose front side sees them run counter-clockwise. Points on one line raise `The three points lie on one line, so they span no plane.`

```lua
local m = require('haylen.math')

local ground = m.planeFromPoints({0, 1, 0}, {0, 1, 1}, {1, 1, 0})
print(ground) -- Plane((0.0, 1.0, 0.0), 1.0)
```

### plane:signedDistance(point)

Returns how far `point` lies in front of the plane, negative behind it.

```lua
local m = require('haylen.math')

print(m.plane(m.up(), 0):signedDistance({0, -3, 0})) -- -3.0
```

### plane:closestPoint(point)

Returns the point of the plane closest to `point`.

```lua
local m = require('haylen.math')

print(m.plane(m.up(), 2):closestPoint({1, 5, 1})) -- Vec3(1.0, 2.0, 1.0)
```

### plane:coefficients()

Returns the four numbers `a`, `b`, `c` and `d` of the plane `a * x + b * y + c * z + d = 0` as a `Vec4`, as a shader takes a plane.

```lua
local m = require('haylen.math')

print(m.plane(m.up(), 2):coefficients()) -- Vec4(0.0, 1.0, 0.0, -2.0)
```

### Plane operators

The operator `a == b` compares the normal and the distance, and `tostring(p)` gives `Plane((x, y, z), distance)`.

```lua
local m = require('haylen.math')

print(m.plane({0, 2, 0}, 4) == m.plane(m.up(), 2)) -- true
```

## 3D shapes

The 3D shapes are userdata of their own types: `Ray3D`, `Segment3D`, `Box`, `Sphere`, `Obb`, `Capsule`, `Triangle` and `Frustum`, next to [`Plane`](#plane). A ray casts against any of the solid shapes and returns a hit table with the `distance` from its origin, the `point` and the `normal` of the surface, which faces the ray, or `nil` when it misses. Boxes, oriented boxes, spheres and capsules are solid, so a ray that starts inside one hits it at distance `0` with a zero normal, and triangles and planes are hit from both sides. The [3D math guide](../3d/math.md#shapes-and-ray-casts) explains which shape suits which task.

```lua
local m = require('haylen.math')

local ray = m.ray3d({0, 1.7, 0}, m.forward())
local hit = ray:cast(m.box({-1, 0, -6}, {1, 2, -4}))
print(hit.distance, hit.normal) -- 4.0 Vec3(0.0, 0.0, 1.0)
```

## Ray3D

A `Ray3D` is a half-line from its `origin` along its `direction`, which stops after its `length`.

```lua
local m = require('haylen.math')

print(m.ray3d({0, 0, 0}, m.forward())) -- a Ray3D
```

### m.ray3d(origin, direction, length)

Returns a `Ray3D` from `origin` along `direction`, which it scales to a length of `1`, and that stops after `length`, `math.huge` by default. A direction of length zero raises the errors of [`vec3:normalized`](#vec3normalized). The fields `origin`, `direction` and `length` read and write, and a direction of another length than `1` raises `The direction of a ray must have a length of one. Make the ray with "ray3d" of "haylen.math", which scales its direction.`

```lua
local m = require('haylen.math')

local sight = m.ray3d({0, 1.7, 0}, {0, 0, -5}, 50)
print(sight.direction, sight.length) -- Vec3(0.0, 0.0, -1.0) 50.0
```

### m.ray3dBetween(from, to)

Returns the `Ray3D` from the point `from` to the point `to`, which stops at `to`.

```lua
local m = require('haylen.math')

local shot = m.ray3dBetween({0, 1, 0}, {0, 1, -8})
print(shot.length) -- 8.0
```

### m.ray3dFromView(viewProjection, point, options)

Returns the `Ray3D` that a camera sees through `point` of its view, such as what lies under the pointer or a finger. The `Mat4` `viewProjection` is the projection times the view of the camera, and `point` is a `Vec2` in normalized device coordinates, from `-1` to `1` from the left edge to the right edge and from the bottom edge to the top edge. The ray starts on the near plane and has no end, so it also reaches past a far plane at infinity. The options `depthRange` and `reversedDepth` are those the projection was built with, `'zeroToOne'` and `true` by default, and an unknown key raises `Unknown option "<key>".`. A matrix without an inverse raises the error of [`mat4:inverse`](#mat4inverse), a point that is not finite raises `The point of a view must have coordinates that are finite numbers.`, and a matrix that puts the point at infinity raises `The matrix is not the projection and view of a camera, because it puts the point of the view at no place on the near plane.`

```lua
local m = require('haylen.math')

local camera = m.perspective({aspect = 16 / 9}) * m.lookAt({0, 5, 10}, {0, 0, 0})
local ray = m.ray3dFromView(camera, {0, 0})
local hit = ray:cast(m.plane(m.up(), 0))
print(hit.point) -- close to Vec3(0.0, 0.0, 0.0), the point that the center of the view looks at
```

### ray:at(distance)

Returns the point at `distance` along the ray.

```lua
local m = require('haylen.math')

print(m.ray3d({0, 0, 0}, m.forward()):at(3)) -- Vec3(0.0, 0.0, -3.0)
```

### ray:endPoint()

Returns the point where the ray stops.

```lua
local m = require('haylen.math')

print(m.ray3dBetween({0, 0, 0}, {2, 0, 0}):endPoint()) -- Vec3(2.0, 0.0, 0.0)
```

### ray:cast(shape)

Returns the hit table of the ray on a `Triangle`, a `Box`, an `Obb`, a `Sphere`, a `Plane` or a `Capsule`, or `nil` when the ray misses it before it stops. A triangle never lets a ray slip through the edge it shares with the next triangle. Another value raises `A ray casts against a Triangle, a Box, an Obb, a Sphere, a Plane or a Capsule, not a <type>.`

```lua
local m = require('haylen.math')

local down = m.ray3d({2, 10, 3}, {0, -1, 0})
local hit = down:cast(m.triangle({0, 0, 0}, {0, 0, 10}, {10, 0, 0}))
print(hit.point, hit.normal) -- Vec3(2.0, 0.0, 3.0) Vec3(0.0, 1.0, 0.0)
```

## Segment3D

A `Segment3D` is the straight piece of a line between two points.

```lua
local m = require('haylen.math')

print(m.segment3d({0, 0, 0}, {1, 0, 0})) -- a Segment3D
```

### m.segment3d(start, end)

Returns the `Segment3D` between two points, whose fields `start` and `end` read and write. Lua reads the field `end` as `segment['end']`, since `end` is a word of the language.

```lua
local m = require('haylen.math')

local bone = m.segment3d({0, 0, 0}, {0, 0.4, 0})
print(bone:length(), bone['end']) -- 0.4 Vec3(0.0, 0.4, 0.0)
```

### segment:length()

Returns the distance between the two ends.

```lua
local m = require('haylen.math')

print(m.segment3d({0, 0, 0}, {3, 4, 0}):length()) -- 5.0
```

### segment:closestPoint(point)

Returns the point of the segment closest to `point`.

```lua
local m = require('haylen.math')

print(m.segment3d({0, 0, 0}, {10, 0, 0}):closestPoint({4, 3, 0})) -- Vec3(4.0, 0.0, 0.0)
```

### segment:closestPoints(other)

Returns the point of this segment and the point of `other` that lie closest to each other.

```lua
local m = require('haylen.math')

local a, b = m.segment3d({0, 0, 0}, {10, 0, 0}):closestPoints(m.segment3d({5, 3, -1}, {5, 3, 1}))
print(a, b) -- Vec3(5.0, 0.0, 0.0) Vec3(5.0, 3.0, 0.0)
```

## Box

A `Box` is a box whose faces line up with the axes, from the corner `min` to the corner `max`.

```lua
local m = require('haylen.math')

print(m.box({0, 0, 0}, {1, 1, 1})) -- a Box
```

### m.box(min, max)

Returns the `Box` with its faces along the axes from the corner `min` to the corner `max`, whose fields `min` and `max` read and write. Its faces count as part of it.

```lua
local m = require('haylen.math')

local room = m.box({0, 0, 0}, {5, 3, 4})
print(room:size()) -- Vec3(5.0, 3.0, 4.0)
```

### m.boxFromCenter(center, size)

Returns the `Box` of `size` around `center`.

```lua
local m = require('haylen.math')

print(m.boxFromCenter({0, 1, 0}, {2, 2, 2}).min) -- Vec3(-1.0, 0.0, -1.0)
```

### m.boxBounds(points)

Returns the smallest `Box` that holds every point of the list. An empty list raises `The bounds of points need at least one point.`

```lua
local m = require('haylen.math')

print(m.boxBounds({{1, 2, 3}, {-1, 5, 0}}).max) -- Vec3(1.0, 5.0, 3.0)
```

### box:center()

Returns the point in the middle of the box.

```lua
local m = require('haylen.math')

print(m.box({0, 0, 0}, {2, 4, 6}):center()) -- Vec3(1.0, 2.0, 3.0)
```

### box:size()

Returns the size of the box along each axis.

```lua
local m = require('haylen.math')

print(m.box({0, 0, 0}, {2, 4, 6}):size()) -- Vec3(2.0, 4.0, 6.0)
```

### box:halfExtents()

Returns half the size of the box along each axis, the reach of its faces from its center.

```lua
local m = require('haylen.math')

print(m.box({0, 0, 0}, {2, 4, 6}):halfExtents()) -- Vec3(1.0, 2.0, 3.0)
```

### box:isEmpty()

Returns `true` when `max` lies below `min` on an axis.

```lua
local m = require('haylen.math')

print(m.box({0, 0, 0}, {-1, 1, 1}):isEmpty()) -- true
```

### box:contains(value)

Returns `true` when the box holds the point or the whole `Box` `value`.

```lua
local m = require('haylen.math')

local room = m.box({0, 0, 0}, {5, 3, 4})
print(room:contains({1, 1, 1}), room:contains(m.box({4, 0, 0}, {6, 1, 1}))) -- true false
```

### box:intersects(shape)

Returns `true` when the box overlaps or touches the `Box` or `Sphere` `shape`.

```lua
local m = require('haylen.math')

local crate = m.box({0, 0, 0}, {1, 1, 1})
print(crate:intersects(m.box({1, 0, 0}, {2, 1, 1})), crate:intersects(m.sphere({3, 0, 0}, 1))) -- true false
```

### box:merged(value)

Returns the smallest box that holds this box and the point or `Box` `value`.

```lua
local m = require('haylen.math')

print(m.box({0, 0, 0}, {1, 1, 1}):merged({3, -1, 0}).min) -- Vec3(0.0, -1.0, 0.0)
```

### box:expanded(amount)

Returns the box grown by `amount` on every side, or shrunk by a negative amount.

```lua
local m = require('haylen.math')

print(m.box({0, 0, 0}, {1, 1, 1}):expanded(0.5):size()) -- Vec3(2.0, 2.0, 2.0)
```

### box:clamp(point)

Returns the point of the box closest to `point`.

```lua
local m = require('haylen.math')

print(m.box({0, 0, 0}, {1, 1, 1}):clamp({5, 0.5, -2})) -- Vec3(1.0, 0.5, 0.0)
```

### box:transformed(placement)

Returns the smallest box along the axes that holds this box after the affine `Mat4` `placement` places it, such as the bounds of a model in the world. A matrix that is not affine raises `A box is placed only by an affine matrix, whose last row is 0, 0, 0 and 1.`

```lua
local m = require('haylen.math')

local model = m.box({-1, 0, -1}, {1, 2, 1})
print(model:transformed(m.compose({10, 0, 0})).min) -- Vec3(9.0, 0.0, -1.0)
```

### Box operators

The operator `a == b` compares both corners.

```lua
local m = require('haylen.math')

print(m.boxFromCenter({0, 0, 0}, {2, 2, 2}) == m.box({-1, -1, -1}, {1, 1, 1})) -- true
```

## Sphere

A `Sphere` is the ball of the points within `radius` of `center`.

```lua
local m = require('haylen.math')

print(m.sphere({0, 0, 0}, 1)) -- a Sphere
```

### m.sphere(center, radius)

Returns the `Sphere` of `radius` around `center`, whose fields `center` and `radius` read and write. A radius below `0` or not finite raises `A radius must be a finite number of 0 or more.`

```lua
local m = require('haylen.math')

local blast = m.sphere({0, 0, -5}, 3)
print(blast:contains({1, 0, -5})) -- true
```

### sphere:contains(point)

Returns `true` when `point` lies in the sphere.

```lua
local m = require('haylen.math')

print(m.sphere({0, 0, 0}, 1):contains({0.5, 0.5, 0})) -- true
```

### sphere:intersects(shape)

Returns `true` when the sphere overlaps or touches the `Sphere` or `Box` `shape`.

```lua
local m = require('haylen.math')

print(m.sphere({0, 0, 0}, 1):intersects(m.sphere({1.5, 0, 0}, 0.6))) -- true
```

### sphere:bounds()

Returns the `Box` around the sphere.

```lua
local m = require('haylen.math')

print(m.sphere({0, 0, 0}, 1):bounds().max) -- Vec3(1.0, 1.0, 1.0)
```

## Obb

An `Obb` is an oriented box, a box that turns with its object.

```lua
local m = require('haylen.math')

print(m.obb({0, 0, 0}, nil, {1, 1, 1})) -- a Obb
```

### m.obb(center, rotation, halfExtents)

Returns the `Obb`, an oriented box of `halfExtents` around `center`, turned by the `Quat` `rotation`, none when `nil`. Its fields `center`, `rotation` and `halfExtents` read and write, and half extents below `0` or not finite raise `The half extents of an oriented box must be finite numbers of 0 or more.`

```lua
local m = require('haylen.math')

local car = m.obb({0, 0.7, 0}, m.fromAxisAngle(m.up(), 0.4), {1, 0.7, 2.2})
print(car:contains({0, 0.7, 2})) -- true
```

### m.obbFromTransform(box, transform)

Returns the `Obb` that the `Transform3D` `transform` makes of the `Box` `box`, such as a box of a model placed in the world. A negative scale counts by its size.

```lua
local m = require('haylen.math')

local placed = m.obbFromTransform(m.box({-1, -1, -1}, {1, 1, 1}), m.transform3d({5, 0, 0}, nil, {2, 1, 1}))
print(placed.center, placed.halfExtents) -- Vec3(5.0, 0.0, 0.0) Vec3(2.0, 1.0, 1.0)
```

### obb:axes()

Returns the three axes of the box as the columns of a `Mat3`.

```lua
local m = require('haylen.math')

print(m.obb({0, 0, 0}, nil, {1, 1, 1}):axes():column(1)) -- Vec3(1.0, 0.0, 0.0)
```

### obb:contains(point)

Returns `true` when `point` lies in the box.

```lua
local m = require('haylen.math')

print(m.obb({0, 0, 0}, m.fromAxisAngle(m.up(), m.pi / 4), {1, 1, 1}):contains({1.3, 0, 0})) -- true
```

### obb:closestPoint(point)

Returns the point of the box closest to `point`.

```lua
local m = require('haylen.math')

print(m.obb({0, 0, 0}, nil, {1, 1, 1}):closestPoint({5, 0.5, -7})) -- Vec3(1.0, 0.5, -1.0)
```

### obb:bounds()

Returns the smallest `Box` along the axes around the box.

```lua
local m = require('haylen.math')

print(m.obb({0, 0, 0}, m.fromAxisAngle(m.up(), m.pi / 4), {1, 1, 1}):bounds().max) -- close to Vec3(1.414, 1.0, 1.414)
```

### obb:intersects(other)

Returns `true` when the box overlaps the `Obb` `other`, by the test of the fifteen axes that can separate two boxes.

```lua
local m = require('haylen.math')

local a = m.obb({0, 0, 0}, nil, {1, 1, 1})
local b = m.obb({2.3, 0, 0}, m.fromAxisAngle(m.up(), m.pi / 4), {1, 1, 1})
print(a:intersects(b)) -- true
```

## Capsule

A `Capsule` is the room within a radius of a segment.

```lua
local m = require('haylen.math')

print(m.capsule({0, 0, 0}, {0, 2, 0}, 0.5)) -- a Capsule
```

### m.capsule(start, end, radius)

Returns the `Capsule`, the points within `radius` of the segment from `start` to `end`, such as the body of a character. Its fields `start`, `end` and `radius` read and write, with the error of [`m.sphere`](#mspherecenter-radius) for a radius out of range.

```lua
local m = require('haylen.math')

local body = m.capsule({0, 0.3, 0}, {0, 1.5, 0}, 0.3)
print(body:contains({0.2, 1, 0})) -- true
```

### capsule:axis()

Returns the segment at the middle of the capsule as a `Segment3D`.

```lua
local m = require('haylen.math')

print(m.capsule({0, 0, 0}, {0, 2, 0}, 0.5):axis():length()) -- 2.0
```

### capsule:contains(point)

Returns `true` when `point` lies in the capsule.

```lua
local m = require('haylen.math')

print(m.capsule({0, 0, 0}, {0, 2, 0}, 0.5):contains({0, 2.4, 0})) -- true
```

### capsule:bounds()

Returns the `Box` around the capsule.

```lua
local m = require('haylen.math')

print(m.capsule({0, 0, 0}, {0, 2, 0}, 0.5):bounds().max) -- Vec3(0.5, 2.5, 0.5)
```

## Triangle

A `Triangle` is a triangle of three corners, the piece that meshes are made of.

```lua
local m = require('haylen.math')

print(m.triangle({0, 0, 0}, {1, 0, 0}, {0, 0, -1})) -- a Triangle
```

### m.triangle(a, b, c)

Returns the `Triangle` with the corners `a`, `b` and `c`, whose front side sees them run counter-clockwise. Its fields `a`, `b` and `c` read and write.

```lua
local m = require('haylen.math')

local floor = m.triangle({0, 0, 0}, {0, 0, 4}, {4, 0, 0})
print(floor:normal()) -- Vec3(0.0, 1.0, 0.0)
```

### triangle:area()

Returns the area of the triangle.

```lua
local m = require('haylen.math')

print(m.triangle({0, 0, 0}, {4, 0, 0}, {0, 0, -4}):area()) -- 8.0
```

### triangle:normal()

Returns the unit normal of the front side. A triangle without area raises `The triangle has no area, so it has no normal.`

```lua
local m = require('haylen.math')

print(m.triangle({0, 0, 0}, {4, 0, 0}, {0, 0, -4}):normal()) -- Vec3(0.0, 1.0, 0.0)
```

### triangle:closestPoint(point)

Returns the point of the triangle closest to `point`, on its face, an edge or a corner.

```lua
local m = require('haylen.math')

print(m.triangle({0, 0, 0}, {4, 0, 0}, {0, 0, -4}):closestPoint({1, 5, -1})) -- Vec3(1.0, 0.0, -1.0)
```

## Frustum

A `Frustum` is the space that a camera sees, bounded by the planes of its view.

```lua
local m = require('haylen.math')

print(m.frustum(m.perspective({farPlane = 100}))) -- a Frustum
```

### m.frustum(viewProjection, depthRange)

Returns the `Frustum`, the space that a camera sees, from its `projection * view` matrix and the range of depth that the projection was built with, `'zeroToOne'` by default. A projection without a far end gives a frustum of five planes.

```lua
local m = require('haylen.math')

local camera = m.perspective({aspect = 16 / 9, nearPlane = 0.1, farPlane = 200}) * m.lookAt({0, 2, 10}, {0, 0, 0})
local frustum = m.frustum(camera)
print(frustum:classify(m.sphere({0, 0, 0}, 1))) -- inside
```

### frustum:planes()

Returns the list of the planes of the frustum, whose normals point inside: the left, right, bottom and top planes and the planes of depth.

```lua
local m = require('haylen.math')

print(#m.frustum(m.perspective({farPlane = 100})):planes()) -- 6
```

### frustum:contains(point, tolerance)

Returns `true` when `point` lies inside, or within `tolerance` of a plane outside it, `0.0001` by default, so a point on a face is never lost to rounding.

```lua
local m = require('haylen.math')

print(m.frustum(m.perspective({farPlane = 100})):contains({0, 0, -10})) -- true
```

### frustum:classify(shape, tolerance)

Returns `'inside'`, `'intersecting'` or `'outside'` for the `Sphere` or `Box` `shape`, with the `tolerance` of `frustum:contains`. It never calls a shape outside that reaches inside, so culling by it never hides what the camera sees, while a box near a corner may count as intersecting though it lies outside.

```lua
local m = require('haylen.math')

local frustum = m.frustum(m.perspective({nearPlane = 0.5, farPlane = 100}) * m.lookAt({0, 0, 10}, {0, 0, 0}))
for _, rock in ipairs({m.sphere({0, 0, 0}, 1), m.sphere({0, 0, 50}, 1)}) do
    if frustum:classify(rock) ~= 'outside' then
        print('Draw the rock at', rock.center)
    end
end
```

### frustum:cull(boxes, tolerance)

Returns the positions, from `1` and in order, of the boxes of the list `boxes` that are not wholly outside, with the `tolerance` of `frustum:contains`. It keeps exactly the boxes that `frustum:classify` does not call `'outside'`, and it tests them in one call with the vector instructions of the processor, which the [3D math guide](../3d/math.md#batches) describes.

```lua
local m = require('haylen.math')

local frustum = m.frustum(m.perspective({nearPlane = 0.5, farPlane = 100}) * m.lookAt({0, 0, 10}, {0, 0, 0}))
local crates = {m.boxFromCenter({0, 0, 0}, {1, 1, 1}), m.boxFromCenter({0, 0, -200}, {1, 1, 1})}
print(table.concat(frustum:cull(crates), ', ')) -- 1
```

## Spherical

A `Spherical` holds a point by its distance from the origin and two angles, such as the place of a camera that orbits a target or a direction on the sky. The polar angle runs down from the up axis and the azimuth turns around it, as the [coordinates page](../3d/coordinates.md#spherical-and-cylindrical-coordinates) shows. Make one with `m.spherical`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `radius` | number | read and write | The distance from the origin. |
| `polar` | number | read and write | The angle in radians down from the up axis, `0` straight up and `math.pi` straight down. |
| `azimuth` | number | read and write | The angle in radians around the up axis, `0` toward positive `z`, growing counterclockwise seen from above. |

### m.spherical(radius, polar, azimuth)

Returns a new `Spherical` with the three numbers, or with all of them `0` when it gets none.

```lua
local m = require('haylen.math')

local orbit = m.spherical(10, m.radians(60), 0)
orbit.azimuth = orbit.azimuth + 0.5
print(orbit:vector())
```

### m.spherical(point)

Returns the `Spherical` of a point in any form a `Vec3` argument accepts. A point on the up axis has an azimuth of `0`, and the origin also has a polar angle of `0`. The azimuth runs from `-math.pi` to `math.pi`.

```lua
local m = require('haylen.math')

local where = m.spherical({0, 0, 3})
print(where.radius, where.azimuth) -- 3.0 0.0
```

### spherical:vector()

Returns the point as a `Vec3`.

```lua
local m = require('haylen.math')

print(m.spherical(2, math.pi / 2, math.pi / 2):vector()) -- close to Vec3(2.0, 0.0, 0.0)
```

## Cylindrical

A `Cylindrical` holds a point by its distance from the up axis, its angle around that axis and its height, such as a place on a tower, a carousel or a spiral path. Make one with `m.cylindrical`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `radius` | number | read and write | The distance from the up axis. |
| `azimuth` | number | read and write | The angle in radians around the up axis, as the azimuth of a `Spherical`. |
| `height` | number | read and write | The height along the up axis. |

### m.cylindrical(radius, azimuth, height)

Returns a new `Cylindrical` with the three numbers, or with all of them `0` when it gets none.

```lua
local m = require('haylen.math')

local seat = m.cylindrical(4, 0, 1)
seat.azimuth = seat.azimuth + m.radians(30)
print(seat:vector())
```

### m.cylindrical(point)

Returns the `Cylindrical` of a point in any form a `Vec3` argument accepts. A point on the axis has an azimuth of `0`.

```lua
local m = require('haylen.math')

print(m.cylindrical({0, -4, 0})) -- Cylindrical(0.0, 0.0, -4.0)
```

### cylindrical:vector()

Returns the point as a `Vec3`.

```lua
local m = require('haylen.math')

print(m.cylindrical(3, math.pi / 2, 2):vector()) -- close to Vec3(3.0, 2.0, 0.0)
```

## SphericalHarmonics

A `SphericalHarmonics` holds the light that reaches a point from every direction in nine coefficients, such as the sky of a scene or a light probe, which the [3D math guide](../3d/math.md#spherical-harmonics) explains. Each coefficient is a `Vec3` with the red, green and blue of linear light in `x`, `y` and `z`, and the nine follow these basis functions of a unit direction `(x, y, z)`:

| Coefficient | Basis function |
| --- | --- |
| `1` | `0.282095` |
| `2` | `0.488603 y` |
| `3` | `0.488603 z` |
| `4` | `0.488603 x` |
| `5` | `1.092548 xy` |
| `6` | `1.092548 yz` |
| `7` | `0.315392 (3z² - 1)` |
| `8` | `1.092548 xz` |
| `9` | `0.546274 (x² - y²)` |

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `coefficients` | list of `Vec3` | read | A copy of the nine coefficients. |

Harmonics add with `+` and scale by a number with `*` on either side, and `==` compares every coefficient.

### m.sphericalHarmonics(coefficients)

Returns new harmonics from a list of nine `Vec3` values, or without any light when it gets none. Another count raises `Spherical harmonics take 9 coefficients, not <count>.`.

```lua
local m = require('haylen.math')

local sky = m.sphericalHarmonics()
sky:projectDirection(m.up(), {0.6, 0.8, 1.2}, 4 * math.pi)
print(#sky.coefficients) -- 9
```

### m.sphericalHarmonicsFromCubemap(buffer, size)

Returns the harmonics of a cube of linear light in a `FloatBuffer` of [`haylen.collections`](collections.md), each face `size` by `size` texels of red, green and blue. The faces follow each other in the order positive x, negative x, positive y, negative y, positive z and negative z, with the rows of each face from the top, as graphics APIs lay out cube textures. Each texel counts with the solid angle it covers. A size of `0` or above `16384` raises `The side of a cubemap must be from 1 to 16384 texels.`, and a buffer of another size raises `A cubemap of side <size> holds <count> floats, the red, green and blue of each texel of its 6 faces, not <size>.`.

```lua
local m = require('haylen.math')
local collections = require('haylen.collections')

local gray = collections.newFloatBuffer(6 * 8 * 8 * 3, 0.5)
local sky = m.sphericalHarmonicsFromCubemap(gray, 8)
print(sky:evaluate(m.up())) -- close to the gray of every texel
```

### m.sphericalHarmonicsFromPanorama(buffer, width, height)

Returns the harmonics of a panorama of linear light in a `FloatBuffer`, `width` by `height` texels of red, green and blue from the top row down. Its rows run from straight up to straight down, and its middle column looks forward, along negative `z`, turning right toward its right edge. A side of `0` or above `16384` raises `The width and the height of a panorama must be from 1 to 16384 texels.`, and a buffer of another size raises `A panorama of <width> by <height> texels holds <count> floats, the red, green and blue of each texel, not <size>.`.

```lua
local m = require('haylen.math')
local collections = require('haylen.collections')

local night = collections.newFloatBuffer(64 * 32 * 3, 0.02)
local ambient = m.sphericalHarmonicsFromPanorama(night, 64, 32):convolveLambert()
print(ambient:evaluate(m.up()))
```

### harmonics:projectDirection(direction, color, weight)

Adds the light of `color` that arrives from `direction` into the harmonics in place, scaled by `weight`, `1` unless it names another. A weight of `1` suits the light of one direction, such as the sun, and a weight of the solid angle that a sample covers suits samples spread over the sky. A direction of zero length raises the error of `vec3:normalized()`.

```lua
local m = require('haylen.math')

local probe = m.sphericalHarmonics()
probe:projectDirection({0.3, 1, -0.2}, {3, 2.8, 2.5})
print(probe.coefficients[1])
```

### harmonics:evaluate(direction)

Returns the light that arrives from a direction as a `Color` of linear light with an alpha of `1`. A direction of zero length raises the error of `vec3:normalized()`.

```lua
local m = require('haylen.math')

local sky = m.sphericalHarmonics()
sky:projectDirection(m.up(), {1, 1, 1})
print(sky:evaluate(m.up()).r > sky:evaluate(-m.up()).r) -- true
```

### harmonics:convolveLambert()

Returns the harmonics of the irradiance, the light that a surface facing each direction receives from the half of the sphere in front of it. A matte white surface sends back the irradiance divided by `math.pi`. Nine coefficients cannot hold a sharp light exactly, so the irradiance of one direction comes out as `1.0625` times its light on a surface that faces it and `0.0625` on a surface that faces away.

```lua
local m = require('haylen.math')

local sun = m.sphericalHarmonics()
sun:projectDirection(m.up(), {1, 1, 1})
print(string.format('%.4f', sun:convolveLambert():evaluate(m.up()).r)) -- 1.0625
```

### harmonics:lerp(to, t)

Returns harmonics that blend from these to `to` by `t`, such as the light between two probes.

```lua
local m = require('haylen.math')

local day, dusk = m.sphericalHarmonics(), m.sphericalHarmonics()
day:projectDirection(m.up(), {2, 2, 2})
dusk:projectDirection({1, 0.2, 0}, {1.5, 0.6, 0.3})
print(day:lerp(dusk, 0.5):evaluate(m.up()))
```

## Transform3D

A `Transform3D` places an object in 3D with a `position`, a `rotation` and a `scale`. It scales a point first, then turns it, then moves it. A child placed in a parent combines with `parent * child`. Make one with `m.transform3d`, and turn it into a matrix with `transform:matrix()`. The [3D math guide](../3d/math.md#transforms) explains when to keep a transform and when a matrix.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `position` | `Vec3` | read and write | Where the object is. |
| `x`, `y`, `z` | number | read and write | The components of the position, which read and write without making a `Vec3`. |
| `rotation` | `Quat` | read and write | How the object is turned. |
| `scale` | `Vec3` | read and write | The size of the object on each of its axes, negative to mirror it. |

```lua
local m = require('haylen.math')

local crate = m.transform3d({0, 0.5, -3})
crate.x = crate.x + 1
crate.rotation = m.fromAxisAngle(m.up(), 0.3)
print(crate.position) -- Vec3(1.0, 0.5, -3.0)
```

### m.transform3d(position, rotation, scale)

Returns a new `Transform3D`. The position is `{0, 0, 0}`, the rotation is none and the scale is `{1, 1, 1}` when left out.

```lua
local m = require('haylen.math')

local tree = m.transform3d({4, 0, -2}, m.fromAxisAngle(m.up(), 1.2), {1.5, 1.5, 1.5})
print(tree:apply({0, 2, 0})) -- the top of the tree in the world
```

### m.transform3d(value)

Returns a copy of a `Transform3D`, or the parts of a `Mat4`, with the errors of [`mat4:decompose`](#mat4decompose).

```lua
local m = require('haylen.math')

local placement = m.transform3d(m.compose({1, 2, 3}, nil, {2, 2, 2}))
print(placement.position, placement.scale) -- Vec3(1.0, 2.0, 3.0) Vec3(2.0, 2.0, 2.0)
```

### transform:apply(point)

Returns where the transform puts `point`.

```lua
local m = require('haylen.math')

print(m.transform3d({1, 2, 3}):apply({4, 0, 0})) -- Vec3(5.0, 2.0, 3.0)
```

### transform:applyVector(vector)

Returns the direction `vector` scaled and turned, without the move.

```lua
local m = require('haylen.math')

print(m.transform3d({1, 2, 3}, nil, {2, 2, 2}):applyVector({1, 0, 0})) -- Vec3(2.0, 0.0, 0.0)
```

### transform:applyNormal(normal)

Returns the normal of a surface that the transform places, with a length of `1`. It stays at a right angle to the surface when the scale differs between axes. A scale of zero on an axis raises `The transform scales an axis to zero or to infinity, so nothing undoes it.`

```lua
local m = require('haylen.math')

local squashed = m.transform3d({0, 0, 0}, nil, {4, 1, 1})
print(squashed:applyNormal({1, -1, 0})) -- tilted toward x less than the surface
```

### transform:applyInverse(point)

Returns the point that the transform puts at `point`, such as a point of the world in the space of the object. It undoes any scale exactly, with the error of `transform:applyNormal` for a scale of zero.

```lua
local m = require('haylen.math')

local door = m.transform3d({5, 0, 0}, m.fromAxisAngle(m.up(), m.halfPi))
print(door:applyInverse({5, 0, -1})) -- close to Vec3(1.0, 0.0, 0.0)
```

### transform:applyInverseVector(vector)

Returns the direction that the transform turns and scales into `vector`, with the error of `transform:applyNormal` for a scale of zero.

```lua
local m = require('haylen.math')

print(m.transform3d({0, 0, 0}, nil, {2, 2, 2}):applyInverseVector({2, 0, 0})) -- Vec3(1.0, 0.0, 0.0)
```

### transform:inverse()

Returns the transform that undoes this one. Its scale must be the same on every axis, since the undoing of a scale that differs between axes after a rotation is no transform of this form. Such a scale raises `A transform whose scale differs between axes has no inverse of the same form. Invert its matrix instead.`, and a scale of zero raises the error of `transform:applyNormal`.

```lua
local m = require('haylen.math')

local placement = m.transform3d({1, 2, 3}, m.fromAxisAngle(m.up(), 0.5), {2, 2, 2})
print((placement * placement:inverse()):approximately(m.transform3d())) -- true
```

### transform:matrix()

Returns the transform as a `Mat4`, which chains exactly with other matrices.

```lua
local m = require('haylen.math')

print(m.transform3d({1, 2, 3}):matrix():translation()) -- Vec3(1.0, 2.0, 3.0)
```

### transform:lerp(to, t)

Returns the transform at `t` of the way toward `to`: the position and the scale in a straight line and the rotation along the shorter arc.

```lua
local m = require('haylen.math')

local closed = m.transform3d()
local open = m.transform3d({0, 0, 0}, m.fromAxisAngle(m.up(), m.halfPi))
print(closed:lerp(open, 0.5).rotation:angle()) -- close to m.pi / 4
```

### transform:approximately(other, epsilon)

Returns `true` when the positions, rotations and scales are close, with the tolerance of [`vec3:approximately`](#vec3approximatelyother-epsilon).

```lua
local m = require('haylen.math')

print(m.transform3d({1, 0, 0}):approximately(m.transform3d({1.000001, 0, 0}))) -- true
```

### Transform3D operators

The operator `parent * child` returns the transform of `child` in the space of `parent`, as a hierarchy of objects does. The position of the child always lands where the parent puts it. The rotations and scales combine exactly while the scale of the parent is the same on every axis or the child does not turn, and otherwise multiply their matrices, which keep the shear that a transform cannot hold. The operator `a == b` compares every part, and `tostring(t)` lists the position, the rotation and the scale.

```lua
local m = require('haylen.math')

local car = m.transform3d({10, 0, 0}, m.fromAxisAngle(m.up(), m.halfPi))
local wheel = m.transform3d({1, -0.3, 0})
print((car * wheel).position) -- close to Vec3(10.0, -0.3, -1.0)
```

## DualQuat

A `DualQuat` holds a rotation and a translation without scale in one dual quaternion. Blending dual quaternions keeps the volume of a skinned mesh at twisted joints, such as a wrist or a shoulder, where blending matrices pinches it. Make one with `m.dualQuat` and blend several with `m.blendDualQuats`.

```lua
local m = require('haylen.math')

local elbow = m.dualQuat(m.fromAxisAngle(m.right(), 0.6), {0, 1.2, 0})
print(elbow:apply({0, 0.3, 0})) -- a point of the forearm in the space of the arm
```

### m.dualQuat(rotation, translation)

Returns the dual quaternion that turns by the `Quat` `rotation` and then moves by `translation`. Both are none when left out.

```lua
local m = require('haylen.math')

print(m.dualQuat(nil, {1, 2, 3}):translation()) -- Vec3(1.0, 2.0, 3.0)
```

### m.blendDualQuats(dualQuats, weights)

Returns the blend of a list of dual quaternions by a list of weights, one for each, along the shorter arc of each, as the joints that move a vertex blend. Lists of different lengths raise `A blend of dual quaternions needs one weight for each dual quaternion.`, an empty list raises `A blend of dual quaternions needs at least one dual quaternion.`, a weight that is negative or not finite raises `The weights of a blend of dual quaternions must be finite and not negative.`, and weights that add up to zero raise `The weights of a blend of dual quaternions add up to zero, so they blend nothing.`

```lua
local m = require('haylen.math')

local straight = m.dualQuat()
local twisted = m.dualQuat(m.fromAxisAngle(m.right(), 2.8))
local wrist = m.blendDualQuats({straight, twisted}, {0.5, 0.5})
print(wrist:apply({0, 1, 0}):length()) -- close to 1.0, so the skin keeps its distance from the bone
```

### dualQuat:rotation()

Returns the rotation as a `Quat`.

```lua
local m = require('haylen.math')

print(m.dualQuat(m.fromAxisAngle(m.up(), 1)):rotation():angle()) -- 1.0
```

### dualQuat:translation()

Returns the translation as a `Vec3`.

```lua
local m = require('haylen.math')

print(m.dualQuat(m.fromAxisAngle(m.up(), 1), {0, 2, 0}):translation()) -- close to Vec3(0.0, 2.0, 0.0)
```

### dualQuat:apply(point)

Returns `point` turned and then moved.

```lua
local m = require('haylen.math')

print(m.dualQuat(m.fromAxisAngle(m.up(), m.halfPi), {1, 2, 3}):apply(m.right())) -- close to Vec3(1.0, 2.0, 2.0)
```

### dualQuat:normalized()

Returns the dual quaternion with a real part of length `1`, which keeps its translation. A real part of length zero raises `The dual quaternion holds no rotation, because the length of its real part is zero or not a finite number.`

```lua
local m = require('haylen.math')

print(m.dualQuat(nil, {1, 0, 0}):normalized():translation()) -- Vec3(1.0, 0.0, 0.0)
```

### dualQuat:matrix()

Returns the dual quaternion as a `Mat4`.

```lua
local m = require('haylen.math')

print(m.dualQuat(nil, {1, 2, 3}):matrix():translation()) -- Vec3(1.0, 2.0, 3.0)
```

### dualQuat:approximately(other, epsilon)

Returns `true` when the rotations and the translations are close, with the tolerance of [`vec3:approximately`](#vec3approximatelyother-epsilon).

```lua
local m = require('haylen.math')

print(m.dualQuat(nil, {1, 0, 0}):approximately(m.dualQuat(nil, {1.000001, 0, 0}))) -- true
```

### DualQuat operators

The operator `a * b` returns the dual quaternion that applies `b` first and `a` after it, and `tostring(d)` lists the rotation and the translation.

```lua
local m = require('haylen.math')

local shoulder = m.dualQuat(m.fromAxisAngle(m.right(), 0.4), {0, 1.5, 0})
local elbow = m.dualQuat(m.fromAxisAngle(m.right(), 0.6), {0, 0.3, 0})
print((shoulder * elbow):apply({0, 0.25, 0})) -- the wrist in the space of the body
```

## Rect

A `Rect` is a rectangle that is not rotated, with its top left corner at `x`, `y`. Make one with `m.rect`, `m.fromMinMax`, `m.fromCenter` or [`m.bounds`](#mboundspoints).

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | read and write | The top left corner. |
| `width`, `height` | number | read and write | The size. |

```lua
local m = require('haylen.math')

local hitbox = m.rect(0, 0, 32, 48)
hitbox.x, hitbox.y = 200, 150
hitbox.width = hitbox.width * 2
print(hitbox) -- Rect(200.0, 150.0, 64.0, 48.0)
```

### m.rect(x, y, width, height)

Returns a new `Rect`. Every argument is `0` when left out. The y axis points down, so the top edge is `y` and the bottom edge is `y + height`.

```lua
local m = require('haylen.math')

local door = m.rect(400, 300, 64, 96)
print(door:bottom()) -- 396.0
```

### m.rect(value)

Returns a new `Rect` from any form that a `Rect` argument accepts: a `Rect`, `{x = 0, y = 0, width = 100, height = 50}` or `{0, 0, 100, 50}`. A `Rect` gives an independent copy.

```lua
local m = require('haylen.math')

local level = {bounds = {0, 0, 3200, 1800}}
local camera = m.rect(level.bounds)
print(camera:right()) -- 3200.0
```

### m.fromMinMax(minimum, maximum)

Returns a new `Rect` from its top left corner `minimum` to its bottom right corner `maximum`, both `Vec2`.

```lua
local m = require('haylen.math')

local dragStart, dragEnd = m.vec2(360, 240), m.vec2(120, 80)
local selection = m.fromMinMax(dragStart:min(dragEnd), dragStart:max(dragEnd))
print(selection) -- Rect(120.0, 80.0, 240.0, 160.0)
```

### m.fromCenter(center, size)

Returns a new `Rect` of the given `size` around the point `center`, both `Vec2`.

```lua
local m = require('haylen.math')

local player = {x = 500, y = 300}
local hitbox = m.fromCenter(player, {32, 48})
print(hitbox) -- Rect(484.0, 276.0, 32.0, 48.0)
```

### rect:left()

Returns the left edge, which is `x`.

```lua
local m = require('haylen.math')

local wall = m.rect(100, 0, 20, 600)
local player = {x = 90}
if player.x < wall:left() then
    print('Left of the wall.')
end
```

### rect:right()

Returns the right edge, which is `x + width`.

```lua
local m = require('haylen.math')

print(m.rect(100, 400, 300, 20):right()) -- 400.0
```

### rect:top()

Returns the top edge, which is `y`.

```lua
local m = require('haylen.math')

local platform = m.rect(100, 400, 300, 20)
local feet = {x = 150, y = 400}
print(feet.y == platform:top()) -- true
```

### rect:bottom()

Returns the bottom edge, which is `y + height`, since the y axis points down.

```lua
local m = require('haylen.math')

print(m.rect(100, 400, 300, 20):bottom()) -- 420.0
```

### rect:position()

Returns the top left corner as a `Vec2`.

```lua
local m = require('haylen.math')

print(m.rect(40, 60, 300, 200):position()) -- Vec2(40.0, 60.0)
```

### rect:min()

Returns the top left corner, the one with the smallest coordinates, as a `Vec2`.

```lua
local m = require('haylen.math')

print(m.rect(10, 20, 30, 40):min()) -- Vec2(10.0, 20.0)
```

### rect:max()

Returns the bottom right corner, the one with the largest coordinates, as a `Vec2`.

```lua
local m = require('haylen.math')

print(m.rect(10, 20, 30, 40):max()) -- Vec2(40.0, 60.0)
```

### rect:center()

Returns the center as a `Vec2`.

```lua
local m = require('haylen.math')

print(m.rect(0, 0, 200, 100):center()) -- Vec2(100.0, 50.0)
```

### rect:size()

Returns the width and the height as a `Vec2`.

```lua
local m = require('haylen.math')

local size = m.rect(10, 10, 640, 360):size()
print(size.x / size.y)
```

### rect:area()

Returns `width * height`.

```lua
local m = require('haylen.math')

print(m.rect(0, 0, 40, 25):area()) -- 1000.0
```

### rect:empty()

Returns `true` when the width or the height is zero or negative.

```lua
local m = require('haylen.math')

local overlap = m.rect(0, 0, 10, 10):intersection({20, 20, 5, 5})
print(overlap:empty()) -- true
```

### rect:contains(value)

Returns `true` when `value`, a point or a rectangle, lies inside. A point is inside when it lies on the left or top edge, or between the edges. A rectangle is inside when all of it lies within. A table counts as a rectangle when it has a `width` field, so pass a rectangle written as a list, such as `{0, 0, 10, 10}`, through `m.rect` first.

```lua
local m = require('haylen.math')

local room = m.rect(0, 0, 800, 600)
print(room:contains({400, 300})) -- true
print(room:contains(m.rect(700, 500, 200, 200))) -- false
print(room:contains({x = 10, y = 10, width = 50, height = 50})) -- true
```

### rect:intersects(other)

Returns `true` when the two rectangles overlap. Rectangles that only touch along an edge do not intersect.

```lua
local m = require('haylen.math')

local player = m.rect(100, 100, 32, 32)
local spikes = m.rect(120, 120, 64, 16)
if player:intersects(spikes) then
    print('Ouch.')
end
```

### rect:intersection(other)

Returns the area where the two rectangles overlap as a `Rect`, or `Rect(0, 0, 0, 0)` when they do not overlap.

```lua
local m = require('haylen.math')

local screen = m.rect(0, 0, 1920, 1080)
local window = m.rect(1800, 1000, 400, 300)
print(screen:intersection(window)) -- Rect(1800.0, 1000.0, 120.0, 80.0)
```

### rect:merged(other)

Returns the smallest rectangle that holds both rectangles.

```lua
local m = require('haylen.math')

local bounds = m.rect(0, 0, 10, 10)
for _, box in ipairs({m.rect(50, 20, 10, 10), m.rect(-30, 5, 10, 10)}) do
    bounds = bounds:merged(box)
end
print(bounds) -- Rect(-30.0, 0.0, 90.0, 30.0)
```

### rect:expanded(amount)

Returns the rectangle grown by `amount` on every side. A negative amount shrinks it.

```lua
local m = require('haylen.math')

local button = m.rect(100, 100, 200, 60)
print(button:expanded(16)) -- Rect(84.0, 84.0, 232.0, 92.0)
```

### rect:inset(insets)

Returns the rectangle shrunk by `insets` from its edges. The argument is one number for every side, or a table with `left`, `top`, `right` and `bottom`, by name or in that order. The width and the height never go below `0`. A value of another type raises `bad argument #1 to 'inset' (number or table with left, top, right and bottom expected, got string)`.

```lua
local m = require('haylen.math')

local dialog = m.rect(100, 100, 400, 300)
print(dialog:inset(20)) -- Rect(120.0, 120.0, 360.0, 260.0)
print(dialog:inset({left = 20, top = 60, right = 20, bottom = 20})) -- Rect(120.0, 160.0, 360.0, 220.0)
```

### rect:translated(offset)

Returns the rectangle moved by the `Vec2` `offset`, with the same size.

```lua
local m = require('haylen.math')

print(m.rect(200, 400, 128, 16):translated({0, -50})) -- Rect(200.0, 350.0, 128.0, 16.0)
```

### rect:clamp(point)

Returns `point` moved to the nearest position inside the rectangle, edges included, as a `Vec2`.

```lua
local m = require('haylen.math')

local arena = m.rect(0, 0, 1920, 1080)
print(arena:clamp({2000, -40})) -- Vec2(1920.0, 0.0)
```

### Rect operators

The operator `a == b` compares all four fields, and `tostring(rect)` gives `Rect(x, y, width, height)`.

```lua
local m = require('haylen.math')

print(m.rect(1, 2, 3, 4) == m.rect(1, 2, 3, 4)) -- true
print(tostring(m.rect(1, 2, 3, 4))) -- Rect(1.0, 2.0, 3.0, 4.0)
```

## Color

A `Color` has red, green, blue and alpha components from `0` to `1`. The components are straight, not premultiplied: red, green and blue do not already include the alpha. Red, green and blue are encoded with the curve of sRGB, the form that color pickers and images use, unless a function says that it takes or returns linear light. Red, green and blue may go above `1` for an HDR color, which is brighter than the white of the screen. Make a color with `m.color` or one of the functions below. The [color guide](../graphics/color.md) explains encoded colors, linear light and HDR.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `r`, `g`, `b` | number | read and write | Red, green and blue. |
| `a` | number | read and write | Alpha, the opacity. |

```lua
local m = require('haylen.math')

local tint = m.color('#FFFFFFFF')
tint.g, tint.b = 0.6, 0.6
tint.a = 0.5
print(tint) -- #80FF9999
```

### m.color(r, g, b, a)

Returns a new `Color` from components between `0` and `1`. Every argument is `1` when left out, so `m.color()` is solid white. When the first argument is not a number, the call reads it as a whole color instead, as the next section describes.

```lua
local m = require('haylen.math')

local white = m.color()
local glass = m.color(0.2, 0.4, 1, 0.5)
print(white, glass) -- #FFFFFFFF #803366FF
```

### m.color(value)

Returns a new `Color` from any form that a `Color` argument accepts. Text is read as `'#AARRGGBB'` or `'#RRGGBB'`, where the `#` is optional and six digits mean solid. A table holds `r`, `g`, `b` and an optional `a`, by name or in that order. A `Color` gives an independent copy. Text that is not valid raises `bad argument #1 to 'color' (invalid color text, expected #RRGGBB or #AARRGGBB)`.

```lua
local m = require('haylen.math')

local sand = m.color('#FFE8D29B')
local shadow = m.color({0, 0, 0, 0.5})
local tint = m.color({r = 1, g = 0.8, b = 0.8})
print(sand, shadow.a, tint) -- #FFE8D29B 0.5 #FFFFCCCC
```

### m.white()

Returns a new solid white `Color`.

```lua
local m = require('haylen.math')

local tint = m.white()
tint.a = 0.5
print(tint) -- #80FFFFFF
```

### m.black()

Returns a new solid black `Color`.

```lua
local m = require('haylen.math')

print(m.black():withAlpha(0.75)) -- #BF000000
```

### m.transparent()

Returns a new `Color` with every component `0`, invisible black.

```lua
local m = require('haylen.math')

print(m.transparent():lerp(m.white(), 0.5)) -- #80808080
```

### m.fromRgba8(red, green, blue, alpha)

Returns a new `Color` from 8-bit channels, each a whole number from `0` to `255`, as image editors show them. The argument `alpha` is `255` when left out. A value outside that range raises `bad argument #1 to 'fromRgba8' (expected an integer from 0 to 255)`, with the position of the argument.

```lua
local m = require('haylen.math')

print(m.fromRgba8(37, 99, 235)) -- #FF2563EB
```

### m.fromHex(rrggbbaa)

Returns a new `Color` from a whole number that packs red, green, blue and alpha in that order, one byte each, such as `0xFF8000FF`. Mind the order: color text puts alpha first, and this number puts it last. A value outside `0` to `0xFFFFFFFF` raises `bad argument #1 to 'fromHex' (expected an integer from 0 to 0xFFFFFFFF)`.

```lua
local m = require('haylen.math')

local orange = m.fromHex(0xFF8000FF)
local glass = m.fromHex(0x3399FF40)
print(orange, glass) -- #FFFF8000 #403399FF
```

### m.fromHsv(hue, saturation, value, alpha)

Returns a new `Color` from hue, saturation and value. The hue is measured in turns: `0` is red, `1/3` is green and `2/3` is blue, and values outside `0` to `1` wrap around. The saturation, the value and `alpha` go from `0` to `1`, and `alpha` is `1` when left out.

```lua
local haylen = require('haylen')
local m = require('haylen.math')

local function rainbow()
    return m.fromHsv(haylen.elapsed() * 0.2, 0.8, 1)
end

print(m.fromHsv(1 / 3, 1, 1)) -- #FF00FF00
print(rainbow())
```

### m.fromLinear(color)

Returns a new encoded `Color` from a color of linear light, the inverse of `color:linear`. Alpha stays as it is, and channels above `1` stay above `1`.

```lua
local m = require('haylen.math')

local mixed = m.color('#FFFF0000'):linear():lerp(m.color('#FF00FF00'):linear(), 0.5)
print(m.fromLinear(mixed)) -- #FFBCBC00, brighter than the #FF808000 of mixing the encoded colors
```

### m.fromTemperature(kelvin)

Returns a new `Color` of the light of a hot body at `kelvin`, such as about `1900` for a candle, `2700` for a warm bulb, `6500` for daylight and `10000` for a clear blue sky. The largest channel is `1`, and a channel that falls outside the colors of sRGB is `0`. A temperature outside `1667` to `25000` raises `A color temperature must be from 1667 to 25000 kelvin.`.

```lua
local m = require('haylen.math')

local lamp = m.fromTemperature(2700)
print(lamp) -- #FFFFAD59
```

### m.contrast(first, second)

Returns the contrast ratio of two colors, from 1 for the same color to 21 for black on white. The contrast rules of accessibility ask for at least 4.5 between text and its background, and 3 for large text and icons.

```lua
local m = require('haylen.math')

print(m.contrast('#FF767676', m.white()) >= 4.5) -- true
```

### color:withAlpha(alpha)

Returns the same color with another alpha.

```lua
local m = require('haylen.math')

print(m.color('#FFFFC107'):withAlpha(0.25)) -- #40FFC107
```

### color:premultiplied()

Returns the color with red, green and blue multiplied by alpha, the form that premultiplied blending expects.

```lua
local m = require('haylen.math')

local ready = m.color(1, 0.5, 0, 0.5):premultiplied()
print(ready.r, ready.g, ready.a) -- 0.5 0.25 0.5
```

### color:lerp(to, t)

Returns the color at `t` of the way toward `to`, component by component. The value `t` is not clamped.

```lua
local m = require('haylen.math')

local day = m.color('#FFFFFFFF')
local night = m.color('#FF2A3B6E')
print(day:lerp(night, 0.5))
```

### color:lerpHsv(to, t)

Returns the color at `t` of the way toward `to` in hue, saturation, value and alpha. The hue takes the shorter way around the circle of hues. So red to blue passes through magenta, instead of the dark purple of `color:lerp`, and a gray takes the hue of the other color, so only its saturation changes. The value `t` is not clamped.

```lua
local m = require('haylen.math')

local red = m.color('#FFFF0000')
local blue = m.color('#FF0000FF')
print(red:lerp(blue, 0.5)) -- #FF800080
print(red:lerpHsv(blue, 0.5)) -- #FFFF00FF
print(m.color('#FFFFFFFF'):lerpHsv(red, 0.5)) -- #FFFF8080
```

### color:toHsv()

Returns four numbers: the hue, the saturation, the value and the alpha of the color. The hue is in turns, from `0` up to but not including `1`, as `m.fromHsv` reads it. A gray has a hue and a saturation of `0`.

```lua
local m = require('haylen.math')

local hue, saturation, value, alpha = m.color('#FF3399FF'):toHsv()
print(m.fromHsv(hue + 0.5, saturation, value, alpha)) -- #FFFF9933
```

### color:toHex()

Returns the color as `'#AARRGGBB'` text with capital digits, which `m.color` and every color argument read back.

```lua
local m = require('haylen.math')
local storage = require('haylen.storage')

local favorite = m.fromHsv(0.6, 0.7, 0.9)
storage.writeJson('settings/theme.json', {accent = favorite:toHex()})
```

### color:linear()

Returns a new `Color` with red, green and blue decoded into linear light, where amounts of light add up, so mixing, blurring and lighting give the brightness the eye expects. Alpha stays as it is. The curve continues above `1` and mirrors below `0`, so HDR and wide colors keep their values through `m.fromLinear`.

```lua
local m = require('haylen.math')

local gray = m.color('#FF808080'):linear()
print(string.format('%.3f', gray.r)) -- 0.216
```

### color:luminance()

Returns the relative luminance of the color in linear light, from 0 for black to 1 for white, and above 1 for an HDR color. It is the measure of brightness that the contrast rules of accessibility read.

```lua
local m = require('haylen.math')

print(m.color('#FF777777'):luminance()) -- about 0.18
```

### color:dark()

Returns `true` when white reads better on the color than black, which makes the color dark. A header of a dark color takes light text and light icons.

```lua
local m = require('haylen.math')
local window = require('haylen.window')

local header = m.color('#FF1E2A44')
window.setStatusBar(header:dark() and 'light' or 'dark')
```

### color:contrastingColor()

Returns black or white, whichever has the higher contrast on the color, with the alpha of the color. It is the color for text and icons that sit on this color.

```lua
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local scene = require('haylen.scene')

local badge = m.color('#FFFFD54F')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({40, 40, 220, 60}, badge)
        graphics2d.drawText(nil, 'New level', 150, 70, {size = 28, color = badge:contrastingColor(), anchor = {0.5, 0.5}})
    end,
})
```

### Color operators

The operator `a * b` multiplies component by component, which tints one color by another. A number on either side of `*` scales red, green and blue and keeps the alpha, which makes a light brighter than white. The operator `a == b` compares all components, and `tostring(color)` gives the same text as `color:toHex`, which clamps every component to `0` and `1`.

```lua
local m = require('haylen.math')

print(m.color('#FFFFFFFF') * m.color(0.5, 0.5, 1)) -- #FF8080FF
print(m.color('#00000000') == m.color(0, 0, 0, 0)) -- true

local glow = m.fromTemperature(3000):linear() * 4
print(string.format('%.1f', glow.r)) -- 4.0
```

## Matrix batches

The table `m.matrixBatch` multiplies and applies lists of matrices in one call, such as the world matrices of many objects under one parent or the points of a shape, with the vector instructions of the processor. Each result matches the matrix math of `Mat4` within rounding. The [3D math guide](../3d/math.md#batches) explains when a batch pays off.

### m.matrixBatch.multiply(parent, matrices)

Returns a new list with `parent * matrix` for each `Mat4` of the list `matrices`, in order.

```lua
local m = require('haylen.math')

local ship = m.compose({0, 10, 0}, m.fromAxisAngle(m.up(), 0.5))
local parts = {m.compose({1, 0, 0}), m.compose({-1, 0, 0}), m.compose({0, 0, -3})}
local worlds = m.matrixBatch.multiply(ship, parts)
print(worlds[3]:translation())
```

### m.matrixBatch.multiplyPairs(lhs, rhs)

Returns a new list with `lhs[i] * rhs[i]` for each pair of the two lists of `Mat4` values. Lists of different lengths raise `The two lists of matrices must hold as many matrices each, but they hold <count> and <count>.`.

```lua
local m = require('haylen.math')

local parents = {m.compose({0, 0, -5}), m.compose({4, 0, -5})}
local locals = {m.compose({0, 1, 0}), m.compose({0, 2, 0})}
print(m.matrixBatch.multiplyPairs(parents, locals)[2]:translation()) -- Vec3(4.0, 2.0, -5.0)
```

### m.matrixBatch.transformPoints(matrix, points)

Returns a new list with `matrix:apply(point)` for each `Vec3` of the list `points`, which divides by the `w` the point maps to, as `mat4:apply` does.

```lua
local m = require('haylen.math')

local lift = m.compose({0, 2, 0})
print(m.matrixBatch.transformPoints(lift, {{0, 0, 0}, {1, 0, 0}})[2]) -- Vec3(1.0, 2.0, 0.0)
```

### m.matrixBatch.instructions

The vector instructions that the batches run on in this build: `'SSE2'` on x64 processors, `'Neon'` on ARM processors, `'WebAssembly SIMD'` on the web, or `'scalar'` on a processor without them.

```lua
local m = require('haylen.math')

print(m.matrixBatch.instructions) -- Neon on a phone or a Mac with Apple silicon
```

## Color spaces

The table `m.colorSpace` turns colors of linear light into the signal of a screen and moves them between the primaries of color spaces, the three colors whose mixes make every other color of a space. The [color guide](../graphics/color.md#screens-and-their-color-spaces) explains when each one matters. A space is one of these names:

| Name | Meaning |
| --- | --- |
| `'srgb'` | The space of almost every screen and image, with the curve of sRGB. |
| `'displayP3'` | The wider space of many phones, tablets and Macs, with the primaries of Display P3 and the curve of sRGB. |
| `'hdr10'` | The space of HDR screens, with the primaries of Rec. 2020 and the perceptual quantizer, which carries light up to 10000 nits. |

Any other name raises the error of a bad argument.

| Field | Type | Meaning |
| --- | --- | --- |
| `referenceWhite` | number | `203`, the nits that the white of a color with channels of `1` takes on an HDR screen by default. |
| `peakNits` | number | `10000`, the brightest light that the signal of `'hdr10'` carries. |

### m.colorSpace.encode(color, space, options)

Returns a new `Color`, the signal of the screen of `space` for `color`, a color of linear light with the primaries of sRGB, the light that a scene draws. The spaces `'srgb'` and `'displayP3'` encode it with the curve of sRGB, and `'hdr10'` places its white at the reference white and encodes it with the perceptual quantizer. Alpha stays as it is. A color outside the gamut of the space has channels below `0` or above `1`, and `'hdr10'` clamps them to its range.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `referenceWhite` | number | `203` | The nits of the white of the scene on an HDR screen, which only `'hdr10'` reads. |

A reference white that is not above `0` and at most `10000` raises `The reference white must be a brightness above 0 and at most 10000 nits.`.

```lua
local m = require('haylen.math')

local sunlit = m.color(1, 0.9, 0.7):linear() * 3
local signal = m.colorSpace.encode(sunlit, 'hdr10', {referenceWhite = 250})
print(string.format('%.2f', signal.r)) -- 0.71
```

### m.colorSpace.decode(signal, space, options)

Returns a new `Color` of linear light with the primaries of sRGB from the signal of the screen of `space`, undoing `m.colorSpace.encode` with the same options.

```lua
local m = require('haylen.math')

local back = m.colorSpace.decode(m.colorSpace.encode({0.5, 0.2, 2}, 'hdr10'), 'hdr10')
print(string.format('%.2f %.2f %.2f', back.r, back.g, back.b)) -- 0.50 0.20 2.00
```

### m.colorSpace.convert(color, from, to)

Returns a new `Color` of linear light moved from the primaries of the space `from` to those of the space `to`, keeping its alpha. A color that the space `to` cannot show has channels below `0` or above `1`.

```lua
local m = require('haylen.math')

local red = m.colorSpace.convert({1, 0, 0}, 'displayP3', 'srgb')
print(red.r > 1, red.g < 0) -- true true, since the red of Display P3 lies outside sRGB
```

### m.colorSpace.toXyz(color, space)

Returns a `Vec3` with the CIE XYZ coordinates of a color of linear light in `space`, the coordinates that every color space shares. Its `y` is the luminance, and the white of every space has `y` of `1`.

```lua
local m = require('haylen.math')

print(string.format('%.3f', m.colorSpace.toXyz(m.white(), 'srgb').y)) -- 1.000
```

### m.colorSpace.fromXyz(xyz, space)

Returns a new `Color` of linear light in `space` from CIE XYZ coordinates, given like any `Vec3`, with an alpha of `1`.

```lua
local m = require('haylen.math')

local xyz = m.colorSpace.toXyz({0.2, 0.4, 0.6}, 'displayP3')
print(m.colorSpace.fromXyz(xyz, 'hdr10'))
```

### m.colorSpace.luminance(color, space)

Returns the luminance of a color of linear light in `space`, where its white is `1`.

```lua
local m = require('haylen.math')

print(string.format('%.4f', m.colorSpace.luminance({0, 1, 0}, 'srgb'))) -- 0.7152
```

### m.colorSpace.encodeSrgb(value)

Returns one channel of linear light encoded with the curve of sRGB. The curve mirrors below `0` and continues above `1`.

```lua
local m = require('haylen.math')

print(string.format('%.4f', m.colorSpace.encodeSrgb(0.5))) -- 0.7354
```

### m.colorSpace.decodeSrgb(value)

Returns one channel encoded with the curve of sRGB decoded into linear light, the inverse of `m.colorSpace.encodeSrgb`.

```lua
local m = require('haylen.math')

print(string.format('%.4f', m.colorSpace.decodeSrgb(0.5))) -- 0.2140
```

### m.colorSpace.encodePq(nits)

Returns the signal of the perceptual quantizer of HDR10, from `0` to `1`, for light in nits. Light below `0` or above `10000` takes the nearest end.

```lua
local m = require('haylen.math')

print(string.format('%.4f', m.colorSpace.encodePq(1000))) -- 0.7518
```

### m.colorSpace.decodePq(signal)

Returns the light in nits of a signal of the perceptual quantizer, the inverse of `m.colorSpace.encodePq`.

```lua
local m = require('haylen.math')

print(string.format('%.0f', m.colorSpace.decodePq(1))) -- 10000
```

## Photometry

The table `m.photometry` converts the physical units of light, which keep the brightness of lights and glowing surfaces true to the real world. The [color guide](../graphics/color.md#physical-light) explains each unit.

### m.photometry.pointIntensity(lumens)

Returns the candelas of a point light that gives `lumens` evenly in every direction, such as a bare bulb.

```lua
local m = require('haylen.math')

print(string.format('%.1f', m.photometry.pointIntensity(800))) -- 63.7
```

### m.photometry.spotIntensity(lumens, outerAngle)

Returns the candelas of a spot light that gives `lumens` evenly into a cone whose edge lies `outerAngle` radians from its axis. A narrower cone sends the same light into less room, so it is brighter. An angle that is not above `0` and at most pi raises `The cone of a spot light needs an outer angle above 0 and at most pi radians.`.

```lua
local m = require('haylen.math')

local flashlight = m.photometry.spotIntensity(300, m.radians(15))
print(string.format('%.0f', flashlight)) -- 1401
```

### m.photometry.illuminance(candelas, distance)

Returns the lux that a light of `candelas` gives a surface that faces it `distance` meters away. The light falls with the square of the distance. A distance that is not above `0` raises `The distance to a light must be above 0 meters.`.

```lua
local m = require('haylen.math')

print(string.format('%.1f', m.photometry.illuminance(64, 2))) -- 16.0
```

### m.photometry.ev100FromCamera(aperture, shutterTime, iso)

Returns the exposure value at ISO 100 of a camera with the f-number `aperture`, the shutter open for `shutterTime` seconds and the sensitivity `iso`. A setting that is not above `0` raises a sentence that names it, such as `The shutter time of a camera must be above 0 seconds.`.

```lua
local m = require('haylen.math')

print(string.format('%.2f', m.photometry.ev100FromCamera(16, 1 / 100, 100))) -- 14.64, a sunny day
```

### m.photometry.ev100FromLuminance(nits)

Returns the exposure value at ISO 100 that shows a scene whose average luminance is `nits` as middle gray, the way the automatic exposure of a camera works. A luminance that is not above `0` raises `The average luminance of a scene must be above 0 nits.`.

```lua
local m = require('haylen.math')

print(string.format('%.1f', m.photometry.ev100FromLuminance(4000))) -- 15.0
```

### m.photometry.exposure(ev100)

Returns the factor that turns light in nits into the values that a screen shows, for an exposure value at ISO 100, so the brightest light that the camera keeps reaches `1`. Each step of the exposure value halves the factor. An emissive surface of a given number of nits draws with that number times the factor.

```lua
local m = require('haylen.math')

local exposure = m.photometry.exposure(m.photometry.ev100FromCamera(16, 1 / 100, 100))
print(string.format('%.2f', 1500 * exposure)) -- 0.05, so a sign of 1500 nits looks dim in the sun
```

## Transform2D

A `Transform2D` is an affine transform: it moves, rotates, scales and skews points in one step. Make one with `m.transform2d`, `m.identity`, `m.translation`, `m.rotation` or `m.scaling`. It maps a point `p` to `(a * p.x + c * p.y + tx, b * p.x + d * p.y + ty)`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `a`, `b` | number | read and write | Where the local x axis points. |
| `c`, `d` | number | read and write | Where the local y axis points. |
| `tx`, `ty` | number | read and write | The translation, where the origin goes. |

```lua
local m = require('haylen.math')

local flip = m.identity()
flip.a = -1
flip.tx = 64
print(flip:apply({10, 5})) -- Vec2(54.0, 5.0)
```

### m.transform2d(position, rotation, scale, skew)

Returns a new `Transform2D` that scales by `scale`, then skews by `skew`, then rotates by `rotation` radians, then moves by `position`. The `Vec2` `position` is required. The `rotation` is `0` when left out, and the `Vec2` `scale` is `{1, 1}`. The `skew` is a `Vec2` of angles in radians, `{0, 0}` by default, that shears the shape. The angle `skew.x` turns the local y axis, and `skew.y` turns the local x axis, the same way as `rotation` turns.

```lua
local m = require('haylen.math')

local turret = m.transform2d({400, 300}, m.pi / 2, {2, 2})
print(turret:apply({16, 0})) -- close to Vec2(400.0, 332.0)

-- Leaning the y axis by 15 degrees shears a banner sideways.
local banner = m.transform2d({0, 0}, 0, {1, 1}, {m.radians(15), 0})
print(banner:apply({0, 100})) -- close to Vec2(-25.9, 96.6)
```

### m.identity()

Returns a new `Transform2D` that leaves every point where it is.

```lua
local m = require('haylen.math')

local chain = {m.translation({100, 0}), m.rotation(m.halfPi)}
local world = m.identity()
for _, link in ipairs(chain) do
    world = world * link
end
print(world:apply({10, 0})) -- close to Vec2(100.0, 10.0)
```

### m.translation(offset)

Returns a new `Transform2D` that moves points by the `Vec2` `offset`.

```lua
local m = require('haylen.math')

print(m.translation({4, -2}):apply({100, 100})) -- Vec2(104.0, 98.0)
```

### m.rotation(radians)

Returns a new `Transform2D` that turns points around the origin by `radians`, clockwise on the screen.

```lua
local m = require('haylen.math')

print(m.rotation(m.halfPi):apply({10, 0})) -- close to Vec2(0.0, 10.0)
```

### m.scaling(scale)

Returns a new `Transform2D` that scales points from the origin by the `Vec2` `scale`.

```lua
local m = require('haylen.math')

print(m.scaling({-1, 1}):apply({30, 40})) -- Vec2(-30.0, 40.0)
```

### transform:apply(point)

Returns `point` transformed, translation included, as a `Vec2`.

```lua
local m = require('haylen.math')

local ship = m.transform2d({500, 300}, m.pi / 4)
print(ship:apply({40, 0}))
```

### transform:applyVector(vector)

Returns `vector` transformed without the translation, which suits directions and offsets.

```lua
local m = require('haylen.math')

local ship = m.transform2d({500, 300}, m.pi / 2)
local forward = ship:applyVector({1, 0})
print(m.approximately(forward.y, 1)) -- true
```

### transform:determinant()

Returns `a * d - b * c`, the factor by which the transform scales areas. It is negative when the transform mirrors, and `0` when it squashes everything onto a line or a point.

```lua
local m = require('haylen.math')

print(m.transform2d({0, 0}, 0.3, {2, -3}):determinant() < 0) -- true
```

### transform:translation()

Returns `tx` and `ty` as a `Vec2`, the point where the transform moves the origin.

```lua
local m = require('haylen.math')

print(m.transform2d({300, 200}, m.pi / 3):translation()) -- Vec2(300.0, 200.0)
```

### transform:isInvertible()

Returns `true` when the transform has an inverse. A transform that squashes the plane onto a line or a point, such as one with a zero scale, has none. The test compares the determinant with the lengths of the axes, so a tiny scale that keeps the shape still has an inverse.

```lua
local m = require('haylen.math')

print(m.transform2d({0, 0}, 0, {0.001, 0.001}):isInvertible(), m.scaling({0, 1}):isInvertible()) -- true false
```

### transform:inverse()

Returns the inverse transform, which maps transformed points back. A transform without an inverse, as `transform:isInvertible` tells, raises `The transform has no inverse, because it squashes the plane flat and its determinant is zero.`.

```lua
local m = require('haylen.math')

local body = m.transform2d({200, 100}, m.pi / 6, {2, 2})
local world = body:apply({10, 5})
local localPoint = body:inverse():apply(world)
print(m.approximately(localPoint.x, 10), m.approximately(localPoint.y, 5)) -- true true
```

### Transform2D operators

The operator `a * b` composes two transforms. The result applies `b` first and then `a`, so `(a * b):apply(p)` equals `a:apply(b:apply(p))`. A child placed inside a parent is `parent * child`.

```lua
local m = require('haylen.math')

local parent = m.transform2d({300, 200}, m.pi / 2)
local child = m.transform2d({50, 0})
print((parent * child):apply({0, 0})) -- close to Vec2(300.0, 250.0)
```

## Scalar functions

### m.clamp(value, minimum, maximum)

Returns `value` kept within `minimum` and `maximum`. A `minimum` above `maximum` raises `bad argument #3 to 'clamp' (expected a maximum of at least the minimum)`.

```lua
local m = require('haylen.math')

print(m.clamp(130, 0, 100)) -- 100.0
```

### m.lerp(from, to, t)

Returns `from + (to - from) * t`, the value at `t` of the way from `from` to `to`. The value `t` is not clamped, so values outside `0` to `1` go past the ends.

```lua
local m = require('haylen.math')

print(m.lerp(0, 200, 0.25)) -- 50.0
```

### m.inverseLerp(from, to, value)

Returns where `value` lies between `from` and `to`: `0` at `from` and `1` at `to`. The result is not clamped, and it is `0` when `from` equals `to`.

```lua
local m = require('haylen.math')

print(m.inverseLerp(100, 300, 250)) -- 0.75
```

### m.remap(value, fromMin, fromMax, toMin, toMax)

Maps `value` from the range `fromMin` to `fromMax` onto the range `toMin` to `toMax`, without clamping.

```lua
local m = require('haylen.math')

local speed = 7
print(m.remap(speed, 0, 10, 0.8, 1.4)) -- about 1.22
```

### m.smoothstep(edge0, edge1, value)

Returns `0` below `edge0`, `1` above `edge1` and a smooth S curve between them. Equal edges make a step: `0` below the edge and `1` from the edge on.

```lua
local m = require('haylen.math')

local distance = 180
print(1 - m.smoothstep(100, 300, distance))
```

### m.moveToward(current, target, maxDelta)

Moves `current` toward `target` by at most `maxDelta`, and never past it.

```lua
local m = require('haylen.math')
local scene = require('haylen.scene')

local ship = {speed = 0, maxSpeed = 400, acceleration = 900}

scene.push({
    update = function(self, dt)
        ship.speed = m.moveToward(ship.speed, ship.maxSpeed, ship.acceleration * dt)
    end,
})
```

### m.wrapAngle(radians)

Returns the same angle within -π and π, π itself left out. The difference of two headings wrapped this way is the shortest turn between them.

```lua
local m = require('haylen.math')

local turn = m.wrapAngle(m.radians(-170) - m.radians(170))
print(m.degrees(turn)) -- close to 20
```

### m.dampFactor(rate, dt)

Returns `1 - exp(-rate * dt)`, the share of the way to move this frame, so that smoothing behaves the same at any frame rate. A higher `rate` follows the target faster.

```lua
local m = require('haylen.math')
local scene = require('haylen.scene')

local camera = {x = 0}
local player = {x = 500}

scene.push({
    update = function(self, dt)
        camera.x = m.lerp(camera.x, player.x, m.dampFactor(8, dt))
    end,
})
```

### m.radians(degrees)

Converts degrees to radians.

```lua
local m = require('haylen.math')

print(m.radians(180) == m.pi) -- true
```

### m.degrees(radians)

Converts radians to degrees.

```lua
local m = require('haylen.math')

print(m.degrees(m.vec2(0, 1):angle())) -- 90.0
```

### m.sign(value)

Returns `1` for a positive `value`, `-1` for a negative one and `0` for zero.

```lua
local m = require('haylen.math')

print(m.sign(-0.6), m.sign(0)) -- -1.0 0.0
```

### m.saturate(value)

Returns `value` kept within `0` and `1`.

```lua
local m = require('haylen.math')

print(m.saturate(1.3), m.saturate(-0.2)) -- 1.0 0.0
```

### m.approximately(a, b, epsilon)

Returns `true` when `a` and `b` differ by at most `epsilon` times the largest of `1`, `|a|` and `|b|`. The `epsilon` is `0.00001` when left out. Use it instead of `==` for numbers that come out of float math.

```lua
local m = require('haylen.math')

print(0.1 + 0.2 == 0.3, m.approximately(0.1 + 0.2, 0.3)) -- false true
print(m.approximately(1, 1.05, 0.1)) -- true
```

## Easing

An easing curve maps progress, from `0` at the start to `1` at the end, to eased progress. It makes motion start slowly, stop softly or bounce. The tweens of [`haylen.tween`](tween.md), the transitions of scenes and the curves of particles take the same curves that `m.ease` takes.

### m.ease(curve, t)

Returns the value of an easing curve at `t`. The value `t` is clamped to `0` to `1`. The result goes from `0` to `1`, and it can go past those values for the `back` and `elastic` curves and for points and Bézier handles outside that range. The argument `curve` takes one of these forms:

| Form | Example | Curve |
| --- | --- | --- |
| Name | `'quadOut'` | The name `'linear'`, or the `In`, `Out` and `InOut` versions of `sine`, `quad`, `cubic`, `quart`, `quint`, `expo`, `circ`, `back`, `elastic` and `bounce`, such as `'quadOut'`, `'backIn'` or `'elasticInOut'`. A table `{curve = 'quadOut'}` names the same curve. |
| Back | `{curve = 'backOut', overshoot = 3}` | A `back` curve with its overshoot, `1.70158` by default. |
| Elastic | `{curve = 'elasticOut', amplitude = 1.5, period = 0.4}` | An `elastic` curve with its amplitude and period, `1` and `0.3` by default. Either key alone keeps the default of the other. |
| Steps | `{steps = 4, position = 'end'}` | Jumps in `steps` equal steps, like CSS `steps()`, with `position` `'start'`, `'end'` (the default), `'both'` or `'none'`. |
| Bézier | `{cubicBezier = {0.25, 0.1, 0.25, 1}}` | The cubic Bézier curve `{x1, y1, x2, y2}`, like CSS `cubic-bezier()`, with both x values from `0` to `1`. |
| Points | `{points = {0, 1.5, 1}}` | Straight lines through numbers spread evenly from `t = 0` to `t = 1`, or through points such as `{{0, 0}, {0.8, 1.2}, {1, 1}}` whose x grows from one point to the next. Before the first point and after the last one, the curve keeps their values. |
| Function | `function(t) return t * t end` | Any function from progress to eased progress. It receives `t` clamped to `0` to `1` and must return a number. |
| EasingCurve | `curve` | A curve that C++ code made from a function, as [`EasingCurve`](#easingcurve) describes. |

Engine functions that return a curve, such as the `config` of a particle emitter, return it in these forms: a name or a table with the keys above.

```lua
local m = require('haylen.math')

local startY, endY, duration, elapsed = 1200, 540, 0.8, 0.4
print(m.lerp(startY, endY, m.ease('backOut', elapsed / duration)))

print(m.ease({curve = 'backOut', overshoot = 3}, 0.5))
print(m.ease({curve = 'elasticOut', amplitude = 1.5, period = 0.4}, 0.5))
print(m.ease({steps = 4}, 0.6)) -- 0.5
print(m.ease({cubicBezier = {0.25, 0.1, 0.25, 1}}, 0.5))
print(m.ease({points = {0, 1.5, 1}}, 0.25)) -- 0.75
print(m.ease({points = {{0, 0}, {0.5, 1}, {1, 1}}}, 0.75)) -- 1.0
print(m.ease(function(t) return t * t end, 0.5)) -- 0.25
```

These errors come from wrong curves:

- An unknown name raises `bad argument #1 to 'ease' (unknown value 'bouncy')`.
- A value of another type raises `bad argument #1 to 'ease' (easing curve name, function or table expected, got boolean)`.
- An unknown key raises `Unknown option "<key>".`
- An `overshoot` on another curve raises `An overshoot only applies to the back curves.`
- An `amplitude` or a `period` on another curve raises `An amplitude and a period only apply to the elastic curves.`, and a period that is not positive raises `An elastic curve needs a positive period.`
- A step count outside `1` to `16777216` raises `A steps curve needs between 1 and 16777216 steps.`
- A `cubicBezier` list without four numbers raises `A "cubicBezier" curve needs the four numbers "x1", "y1", "x2" and "y2".`, and an x outside `0` to `1` raises `The x coordinates of a cubic Bézier curve must be between 0 and 1.`
- A `points` that is not a list raises `The "points" of an easing curve are a list of heights or of {x, y} points, not a <type>.`, fewer than two points raise `A points curve needs at least two points.`, and points out of order raise `The points of a curve must be ordered by x.`
- A function that returns anything but a number raises `An easing function must return a number.`, and a finite number past the floats of the engine raises `An easing function must return a number from -3.4028234663853e+38 to 3.4028234663853e+38.`

## EasingCurve

An `EasingCurve` is an easing curve that C++ code made from a function, which cannot become a name or a table. A curve in any other form reaches Lua as a name or a table. Such a value comes from projects that extend the engine in C++, when a binding pushes a curve of `math::EasingCurve::custom` with the binding toolkit that the [Lua guide](../core/lua.md#extending-the-engine-from-c) describes:

```cpp
int CurvesBinding::half(lua_State* L) {
    haylen::lua::Stack::push(L, haylen::math::EasingCurve::custom([](float t) { return t * 0.5F; }));
    return 1;
}
```

Call the value like the function it stands for: `curve(t)` returns the eased value of `t`, so the curve above returns `0.25` for `half(0.5)`. Every option and function that takes a curve accepts it, such as `m.ease` and the `ease` of a tween, so it passes from one to another as it is. It has no methods or properties. Code that takes a curve from elsewhere reads it with `m.ease`, which works for every form.

```lua
local m = require('haylen.math')

-- The curve may be a name, a table, a function or an EasingCurve.
local function sample(curve)
    local values = {}
    for step = 0, 4 do
        values[#values + 1] = m.ease(curve, step / 4)
    end
    return values
end

print(table.concat(sample('quadIn'), ' ')) -- 0.0 0.0625 0.25 0.5625 1.0
```

## Random

A `Random` is a seeded generator of random numbers, made with `m.random`. It uses the xoshiro256\*\* algorithm, so the same seed gives the same numbers in the same order on every platform. Use one for anything that must repeat, such as a level from a seed or a replay.

### m.random(seed)

Returns a new `Random`. The argument `seed` is a whole number. Without one, the generator uses a fixed seed, so it repeats the same numbers on every run. Pass something like `os.time()` for different numbers on each run.

```lua
local m = require('haylen.math')

local levelRandom = m.random(2024)
local lootRandom = m.random(os.time())
print(levelRandom:integer(1, 6), lootRandom:nextFloat())
```

### random:nextFloat()

Returns a number from `0` up to but not including `1`.

```lua
local m = require('haylen.math')

local random = m.random(5)
if random:nextFloat() < 0.1 then
    print('Critical hit.')
end
```

### random:range(minimum, maximum)

Returns a number from `minimum` up to but not including `maximum`.

```lua
local m = require('haylen.math')

local random = m.random(9)
print(random:range(0, m.tau), random:range(80, 120))
```

### random:integer(minimum, maximum)

Returns a whole number from `minimum` to `maximum`, both included. When `maximum` is not above `minimum`, it returns `minimum`. Both arguments are whole numbers that fit in 32 bits, from `-2147483648` to `2147483647`. A number with a fraction raises `bad argument #1 to 'integer' (number has no integer representation)`, and a number outside that range raises `bad argument #1 to 'integer' (integer out of range)`, with the position of the argument.

```lua
local m = require('haylen.math')

local dice = m.random(42)
print(dice:integer(1, 6) + dice:integer(1, 6))
```

### random:chance(probability)

Returns `true` with the given probability, from `0` for never to `1` for always.

```lua
local m = require('haylen.math')

local random = m.random(3)
if random:chance(0.25) then
    print('The chest holds a rare item.')
end
```

### random:inSphere(radius)

Returns a `Vec3` spread evenly through the ball of `radius` around the origin, `1` when left out, such as the debris of a blast.

```lua
local m = require('haylen.math')

local random = m.random(5)
print(random:inSphere(3):length() <= 3) -- true
```

### random:onSphere(radius)

Returns a `Vec3` spread evenly over the sphere of `radius` around the origin, `1` when left out, such as a random direction when the radius is `1`.

```lua
local m = require('haylen.math')

local random = m.random(5)
print(string.format('%.1f', random:onSphere(2):length())) -- 2.0
```

### random:inCone(axis, angle)

Returns a direction of length `1` spread evenly over the directions within `angle` radians of `axis`, such as the sparks of a fountain or the spread of a shotgun. An axis of zero length raises `The vector has no direction, because its length is zero.`, and an angle outside `0` to pi raises `A random cone needs an angle from 0 to pi radians.`.

```lua
local m = require('haylen.math')

local random = m.random(5)
local spark = random:inCone(m.up(), m.radians(20))
print(spark:dot(m.up()) >= math.cos(m.radians(20))) -- true
```

### random:inBox(box)

Returns a `Vec3` spread evenly through the `Box` `box`, such as a spawn point inside a room.

```lua
local m = require('haylen.math')

local random = m.random(5)
local spawn = random:inBox(m.box({-5, 0, -5}, {5, 3, 5}))
print(spawn.y >= 0 and spawn.y <= 3) -- true
```

### random:weightedIndex(weights)

Picks a position of the list `weights` and returns it, counted from 1. Each position has a chance in proportion to its weight, and a weight of zero is never picked. An empty list raises `bad argument #1 to 'weightedIndex' (expected at least one weight)`, a weight that is negative or not finite raises `Weights must be finite and not negative.`, and weights that are all zero raise `A weighted pick needs at least one positive weight.` For the same weights picked many times, a [`WeightedChoice`](#weightedchoice) is faster.

```lua
local m = require('haylen.math')

local random = m.random(11)
local loot = {'coin', 'potion', 'sword'}
print(loot[random:weightedIndex({70, 25, 5})])
```

### random:shuffle(list)

Shuffles the sequence `list` in place and returns it.

```lua
local m = require('haylen.math')

local deck = m.random(8):shuffle({'ace', 'king', 'queen', 'jack'})
print(table.concat(deck, ', '))
```

### random:reseed(seed)

Starts the generator again from the whole number `seed`, so it repeats the numbers of a new generator with that seed.

```lua
local m = require('haylen.math')

local random = m.random(1)
local first = random:nextFloat()
random:reseed(1)
print(random:nextFloat() == first) -- true
```

### random:state()

Returns the whole state of the generator as a string of 32 bytes. Keep it in a replay, a save or the data of an [input recording](input.md#recordings), and the generator draws the same numbers again from that point.

```lua
local m = require('haylen.math')

local random = m.random(7)
local saved = random:state()
local first = random:integer(1, 6)
random:setState(saved)
print(random:integer(1, 6) == first) -- true
```

### random:setState(state)

Restores a state that `random:state()` returned. A string of another length raises `A random state needs 32 bytes.`, and 32 zero bytes raise `A random state cannot be all zeros.`

```lua
local m = require('haylen.math')

local random = m.random(3)
local copy = m.random(0)
copy:setState(random:state())
print(copy:nextFloat() == random:nextFloat()) -- true
```

## Noise2D

A `Noise2D` is seeded gradient noise in 2D, made with `m.noise2d`. Noise gives values that change smoothly from one point to the next, without the jumps of random numbers, which suits terrain, clouds, wind and wobble. The methods `noise:perlin`, `noise:simplex` and `noise:fractal` return values from `-1` to `1`. The method `noise:worley` returns distances and a region value, and `noise:warp` returns a point. Features of the noise lie about one unit apart, so scale world coordinates down before sampling, such as `x / 200`. The noise repeats every 256 units, so coordinates of any size sample it. At a coordinate that is infinite or not a number, `noise:perlin`, `noise:simplex` and `noise:fractal` return a value that is not a number.

### m.noise2d(seed)

Returns a new `Noise2D`. The whole number `seed`, `0` by default, picks the noise field, and each seed gives a different one.

```lua
local m = require('haylen.math')

local terrain = m.noise2d(7)
print(terrain:perlin(3.5, 8.25))
```

### noise:perlin(x, y)

Returns classic Perlin noise at `x`, `y`.

```lua
local m = require('haylen.math')

local wind = m.noise2d(3):perlin(12 / 50, 0.5)
print(wind)
```

### noise:simplex(x, y)

Returns simplex noise at `x`, `y`, which has fewer patterns along the axes than Perlin noise.

```lua
local m = require('haylen.math')

local moisture = m.noise2d(21):simplex(640 / 200, 320 / 200)
print(moisture)
```

### noise:fractal(x, y, octaves, lacunarity, gain)

Returns several layers of simplex noise added together, kept from `-1` to `1`. Each layer adds finer detail, like the small bumps on a large hill. The number of layers is `octaves`, a whole number from `1` to `16`, `4` by default, and another count raises `The octaves must be a whole number from 1 to 16.`. Each layer has `lacunarity` times the frequency of the one before, `2` by default, and `gain` times its strength, `0.5` by default.

```lua
local m = require('haylen.math')

local noise = m.noise2d(99)
local tiles = {}
for y = 1, 32 do
    for x = 1, 32 do
        local height = noise:fractal(x / 16, y / 16, 5, 2, 0.5)
        tiles[#tiles + 1] = height > 0.1 and 'grass' or 'water'
    end
end
print(tiles[1])
```

### noise:worley(x, y)

Returns cellular noise, also called Worley noise, at `x`, `y` as three numbers. Each unit cell holds one feature point, placed at random. The first number is the distance to the nearest feature point, and the second the distance to the second nearest one, both in cell units. The third is a value from `0` to `1` that tells the region of the nearest point apart. The nearest distance draws stones and cells, the difference of both distances draws cracks, and the region value colors each cell.

```lua
local m = require('haylen.math')

local nearest, second, cell = m.noise2d(5):worley(3.2, 7.5)
local crack = second - nearest < 0.05
print(nearest, crack, cell)
```

### noise:warp(x, y, amplitude, frequency, octaves)

Moves the point by fractal noise and returns the new `x` and `y`. The noise is sampled at `frequency` times the point, `1` by default, with `octaves` layers, from `1` to `16` like those of `noise:fractal` and `3` by default, and it moves the point by up to `amplitude` on each axis. Sampling any noise at the moved point bends its patterns into swirls, which suits coastlines, marble and smoke.

```lua
local m = require('haylen.math')

local noise = m.noise2d(8)
local wx, wy = noise:warp(120, 45, 30, 0.01)
print(noise:fractal(wx / 200, wy / 200))
```

### noise:sampleGrid(options)

Samples a whole grid of noise in one call. It returns the values in a [`FloatBuffer`](collections.md#collectionsnewfloatbuffersize-value), row after row. The value of the column `column` and the row `row`, both from 0, sits at the position `row * columns + column + 1`. The workers of the task pool sample bands of rows at the same time, and no Lua runs per sample. So a map of a quarter million tiles costs one call instead of a quarter million.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `columns`, `rows` | integer from 1 to 16384 | required | The size of the grid, at most 16777216 samples in all. |
| `kind` | string | `'fractal'` | One of `'perlin'`, `'simplex'`, `'fractal'` or `'worley'`. A `'worley'` grid samples the nearest distance. |
| `x`, `y` | number | `0` | Where the first sample lies. |
| `step` | number or Vec2 | `1` | How far apart the samples lie, the same on both axes or `{x, y}`. |
| `octaves` | integer from 1 to 16 | `4` | The layers of a fractal grid. |
| `lacunarity`, `gain` | number | `2`, `0.5` | The frequency and strength factors of each layer of a fractal grid. |
| `into` | FloatBuffer | a new one | A buffer of at least `columns * rows` floats to write into, so a grid sampled every frame allocates nothing. |

These errors come from wrong options: `The option "columns" is required.`, `The option "columns" must be a whole number from 1 to 16384.` and the same two for `rows`, `The grid holds <count> samples, more than the limit of 16777216.`, `The option "kind" must be "perlin", "simplex", "fractal" or "worley".`, `The option "octaves" must be a whole number from 1 to 16.`, `The buffer of "into" holds <size> floats, fewer than the <count> samples of the grid.` and `Unknown option "<key>".`

```lua
local m = require('haylen.math')

local heights = m.noise2d(42):sampleGrid({columns = 256, rows = 256, step = 1 / 32, octaves = 5})
local water = 0
for index = 1, #heights do
    if heights[index] < 0 then
        water = water + 1
    end
end
print(water)
```

### noise:sampleGridAsync(options)

Samples a grid like `noise:sampleGrid` on the task pool while the frames go on. It returns a promise of a new `FloatBuffer`. It takes the options of `noise:sampleGrid` except `into`, and it raises the same errors at once, in the call.

```lua
local async = require('async')
local m = require('haylen.math')

async.spawn(function()
    local moisture = m.noise2d(7):sampleGridAsync({columns = 1024, rows = 1024, kind = 'simplex', step = 0.01}):await()
    print(#moisture)
end)
```

## Noise3D

A `Noise3D` is seeded noise in three and four dimensions, made with `m.noise3d`, for caves, clouds, volumes and the textures of 3D surfaces. The fourth dimension often runs with time, so a 3D pattern changes in place instead of sliding. Every method takes `x`, `y` and `z`, and one more number `w` after them makes the sample 4D. The methods `noise:perlin`, `noise:simplex`, `noise:value` and `noise:fractal` return values from `-1` to `1`, and the same seed gives the same values on every platform. Features lie about one unit apart, so scale world coordinates before sampling, such as `x / 50`. The [3D math guide](../3d/math.md#noise-and-random-points) explains each kind.

### m.noise3d(seed)

Returns a new `Noise3D`. The whole number `seed`, `0` by default, picks the noise field.

```lua
local m = require('haylen.math')

local caves = m.noise3d(42)
print(caves:simplex(1.5, 2.25, -3))
```

### noise:perlin(x, y, z, w)

Returns gradient noise of Perlin at the point, `0` on every whole coordinate. The argument `w` is optional and makes the sample 4D.

```lua
local m = require('haylen.math')

local noise = m.noise3d(1)
print(noise:perlin(2, 3, 4), noise:perlin(0.5, 0.5, 0.5, 0.25))
```

### noise:simplex(x, y, z, w)

Returns simplex noise at the point, which has no lines along the axes and costs less than Perlin noise, more so in 4D. The argument `w` is optional.

```lua
local m = require('haylen.math')
local haylen = require('haylen')

local clouds = m.noise3d(3)
print(clouds:simplex(1.2, 0.5, 3.4, haylen.elapsed() * 0.1))
```

### noise:value(x, y, z, w)

Returns value noise at the point, which blends random values on the corners of the lattice and looks softer and blockier than gradient noise. The argument `w` is optional.

```lua
local m = require('haylen.math')

print(m.noise3d(3):value(1.5, 2.5, 3.5))
```

### noise:cellular(x, y, z, w)

Returns three numbers of Worley noise: the distance to the nearest feature point, the distance to the second nearest, and a value from `0` up to `1` that tells the cell of the nearest point apart, for stone, cells and crystals. The argument `w` is optional.

```lua
local m = require('haylen.math')

local nearest, second, cell = m.noise3d(8):cellular(1.5, 2.5, 3.5)
print(second - nearest, cell) -- small near the walls between cells
```

### noise:fractal(x, y, z, w, options)

Returns octaves of a noise summed at rising frequencies and falling amplitudes, scaled back to `-1` to `1`. The argument `w` is optional, so the options follow `z` or `w`. Octaves outside `1` to `16` raise `The octaves must be a whole number from 1 to 16.`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `basis` | string | `'simplex'` | The noise of each octave: `'perlin'`, `'simplex'` or `'value'`. |
| `fractal` | string | `'fbm'` | How each octave shapes its noise: `'fbm'` as it is, `'billow'` folded into round billows, `'ridged'` folded into sharp ridges, or `'pingPong'` bounced back and forth. |
| `octaves` | integer | `4` | The number of octaves. |
| `lacunarity` | number | `2` | How much the frequency grows from one octave to the next. |
| `gain` | number | `0.5` | How much the amplitude falls from one octave to the next. |
| `pingPongStrength` | number | `2` | How many times `'pingPong'` bounces across the range. |

```lua
local m = require('haylen.math')

local noise = m.noise3d(4)
local mountains = noise:fractal(10 / 64, 0, 20 / 64, {fractal = 'ridged', octaves = 6})
print(mountains)
```

### noise:warp(x, y, z, amplitude, frequency, octaves)

Returns three numbers, the point moved by fractal simplex noise sampled at `frequency`, `1` by default, up to `amplitude` away on each axis, with `octaves`, `3` by default. Sampling any noise at the moved point bends its patterns, such as marble or swirling smoke.

```lua
local m = require('haylen.math')

local noise = m.noise3d(6)
local x, y, z = noise:warp(1, 2, 3, 0.5, 0.25)
print(noise:simplex(x, y, z))
```

## Circles and segments

A circle argument is a table with a `center` point and a `radius`, written `{center = {100, 50}, radius = 20}` or `{{100, 50}, 20}`. A segment argument is a table with a `start` point and an `end` point, written `{start = a, ['end'] = b}` or `{a, b}`. A table without a point that can be read raises `Expected a point in field "center".`, and one without a number that can be read raises `Expected a number in field "radius".`, with the name of the field.

### m.intersects(circle, other)

Returns `true` when `circle` overlaps `other`, another circle or a rectangle. Shapes that only touch count as overlapping. The argument `other` is a circle when it has a `radius` field or holds exactly two values in order, and a rectangle in any `Rect` form otherwise.

```lua
local m = require('haylen.math')

local blast = {center = {400, 300}, radius = 120}
local crate = m.rect(480, 280, 64, 64)
local slime = {{560, 300}, 30}
print(m.intersects(blast, crate), m.intersects(blast, slime)) -- true false
```

### m.intersection(a, b)

Returns the point where the segments `a` and `b` cross as a `Vec2`, or `nil` when they do not cross. Parallel segments return `nil`.

```lua
local m = require('haylen.math')

local laser = {{0, 0}, {800, 600}}
local wall = {{400, 0}, {400, 1000}}
print(m.intersection(laser, wall)) -- Vec2(400.0, 300.0)
print(m.intersection(laser, {{0, 100}, {100, 200}})) -- nil
```

### m.closestPoint(segment, point)

Returns the point of `segment` nearest to `point`, as a `Vec2`.

```lua
local m = require('haylen.math')

local rail = {{100, 400}, {700, 400}}
print(m.closestPoint(rail, {250, 320})) -- Vec2(250.0, 400.0)
```

### m.distanceToSegment(segment, point)

Returns the distance from `point` to the nearest point of `segment`.

```lua
local m = require('haylen.math')

local river = {{0, 500}, {1920, 500}}
local camp = {x = 600, y = 380}
if m.distanceToSegment(river, camp) < 150 then
    print('The camp hears the river.')
end
```

## Polygons and sampling

The polygon functions take a list of points, each a `Vec2` or a point table, with the corners in order around the outline. A point that cannot be read raises an error.

### m.polygonContains(polygon, point)

Returns `true` when `point` lies inside `polygon`, by the even-odd rule: a point is inside when a line from it to far away crosses the outline an odd number of times.

```lua
local m = require('haylen.math')

local pond = {{100, 100}, {300, 80}, {340, 260}, {120, 300}}
print(m.polygonContains(pond, {200, 200}), m.polygonContains(pond, {400, 200})) -- true false
```

### m.polygonSignedArea(polygon)

Returns the area of `polygon` with a sign. It is positive when the corners run clockwise on the screen, where the y axis points down, and negative when they run the other way. The sign tells the winding of an outline, such as an outline from a hole.

```lua
local m = require('haylen.math')

print(m.polygonSignedArea({{0, 0}, {10, 0}, {10, 10}, {0, 10}})) -- 100.0
print(m.polygonSignedArea({{0, 0}, {0, 10}, {10, 10}, {10, 0}})) -- -100.0
```

### m.polygonCentroid(polygon)

Returns the center of mass of `polygon` as a `Vec2`. For a polygon without area, it returns the average of the corners.

```lua
local m = require('haylen.math')

print(m.polygonCentroid({{0, 0}, {400, 0}, {400, 200}, {0, 200}})) -- Vec2(200.0, 100.0)
```

### m.polygonConvex(polygon)

Returns `true` when `polygon` is convex, in either winding. A convex polygon has no dents: a line between any two of its points stays inside. Polygons with fewer than three points are not convex, and neither is a star such as a pentagram, whose outline crosses itself even though it turns the same way at every corner.

```lua
local m = require('haylen.math')

local square = {{0, 0}, {10, 0}, {10, 10}, {0, 10}}
local arrow = {{0, 0}, {10, 0}, {5, 3}, {10, 10}, {0, 10}}
print(m.polygonConvex(square), m.polygonConvex(arrow)) -- true false

local star = {}
for point = 0, 4 do
    star[#star + 1] = m.fromAngle(point * 2 * m.tau / 5, 10)
end
print(m.polygonConvex(star)) -- false
```

### m.convexHull(points)

Returns the smallest convex polygon around `points`, as a list of `Vec2` in order around the outline. It leaves out duplicate points and points with a coordinate that is not a number. With fewer than three distinct points, it returns the distinct points.

```lua
local m = require('haylen.math')

local rocks = {{0, 0}, {10, 0}, {5, 5}, {10, 10}, {0, 10}}
print(#m.convexHull(rocks)) -- 4
```

### m.triangulate(polygon)

Splits a simple polygon, one whose outline never crosses itself, into triangles. It returns a flat list of corner positions counted from 1, three per triangle. The polygon may have dents and either winding, and every triangle comes out with the winding that `m.polygonSignedArea` reports as positive. Fewer than three points give an empty list, and an outline that crosses itself may leave part of its area without triangles.

It also takes a shape of the [`m.polygon`](#polygon-operations) functions: a list of outlines, where an outline wound opposite to the outline around it is a hole. Then the corner positions count the points of all the outlines one after the other, and no triangle covers a hole. A table that is not a list of points or outlines raises the `bad argument` error of Lua.

```lua
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local scene = require('haylen.scene')

local shape = {{0, 0}, {100, 0}, {50, 30}, {100, 100}, {0, 100}}
local indices = m.triangulate(shape)

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        for index = 1, #indices, 3 do
            graphics2d.drawPolygon({shape[indices[index]], shape[indices[index + 1]], shape[indices[index + 2]]}, '#FF4CAF50')
        end
    end,
})
```

### m.bounds(points)

Returns the smallest `Rect` that holds every point of the list `points`. An empty list returns `Rect(0, 0, 0, 0)`.

```lua
local m = require('haylen.math')

print(m.bounds({{120, 300}, {180, 260}, {90, 340}})) -- Rect(90.0, 260.0, 90.0, 80.0)
```

### m.poissonDisk2d(options)

Returns random points spread evenly inside an area, as a list of `Vec2`. No two points are closer than a minimum distance, so the points never clump. Use it to scatter trees, rocks or spawn points. The same options and seed always give the same points. For points with types, regions and exclusions, use [`procedural.scatter`](procedural2d.md#proceduralscatteroptions) of `haylen.procedural2d`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `area` | Rect | empty | The area to fill. An empty area gives no points. |
| `minimumDistance` | number | `64` | The smallest distance between two points. A value of `0` or less gives no points. |
| `maximumDistance` | number | `0` | The largest distance that a `distance` function may ask for. It must be at least `minimumDistance` when `distance` is set. |
| `distance` | function | none | Called with each candidate `Vec2`, it returns the spacing wanted there, such as from a density map. The result is clamped between `minimumDistance` and `maximumDistance`, and two points stay at least the larger of their own spacings apart. |
| `attempts` | integer | `30` | The candidates tried around each point before it stops growing, from `1` to `1000`. More attempts fill the area more tightly. |
| `random` | Random | none | The generator to draw from, which advances as it draws. |
| `seed` | integer | `0` | The seed of a generator of its own, used when `random` is absent. |
| `accept` | function | none | Called with each candidate `Vec2`, it keeps only the candidates for which it returns a true value. |

These errors come from wrong options:

- An unknown key raises `Unknown option "<key>".`
- A distance that is infinite or not a number raises `Poisson distances must be finite.`
- An `area` with an edge that is infinite or not a number raises `Poisson sampling needs an area whose edges are finite.`
- A `distance` function without a `maximumDistance` of at least `minimumDistance` raises `A varying Poisson distance needs a maximum distance of at least the minimum distance.`
- An attempt count outside `1` to `1000` raises `Poisson sampling needs between 1 and 1000 attempts.`
- A minimum distance so small that the sampling grid of the area would need more than 16777216 cells raises `The minimum distance is too small for the area, whose sampling grid would need more than 16777216 cells.`
- A `distance` function that returns something other than a number raises `A Poisson distance function must return a number, not <type>.`, and one that returns a distance that is infinite or not a number raises `A Poisson distance function must return finite distances.`
- An error raised inside `accept` or `distance` leaves the call.

```lua
local m = require('haylen.math')

local lake = m.rect(600, 400, 300, 200)
local trees = m.poissonDisk2d({
    area = {0, 0, 1920, 1080},
    minimumDistance = 90,
    seed = 12,
    accept = function(point)
        return not lake:contains(point)
    end,
})
print(#trees .. ' trees planted.')
```

A distance function thins the points out where it asks for more room, here from dense woods on the left to scattered trees on the right.

```lua
local m = require('haylen.math')

local woods = m.poissonDisk2d({
    area = {0, 0, 1920, 1080},
    minimumDistance = 40,
    maximumDistance = 160,
    seed = 3,
    distance = function(point)
        return m.lerp(40, 160, point.x / 1920)
    end,
})
print(#woods)
```

### m.poissonDisk3d(options)

Returns a list of `Vec3` points spread evenly through a box, no two closer than `minimumDistance`, such as trees, rocks or stars. The points come in a random order, and the same seed gives the same points. An empty box or a distance that is not above `0` gives an empty list. Attempts outside `1` to `1000` raise `Poisson sampling needs between 1 and 1000 attempts.`, a distance or a side of the box that is not finite raises `Poisson sampling in 3D needs a finite distance and a box whose sides are finite.`, and a distance so small that the sampling grid would need more than `16777216` cells raises `The minimum distance is too small for the box, whose sampling grid would need more than 16777216 cells.`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `volume` | `Box` | the point at the origin | The box that the points fill. |
| `minimumDistance` | number | `1` | The smallest distance between two points. |
| `attempts` | integer | `30` | How many places each point tries for a neighbor before it gives up. |
| `random` | `Random` | none | The generator that draws the points, in place of `seed`. |
| `seed` | integer | `0` | The seed of the points when no `random` is given. |
| `accept` | function | none | Receives each candidate `Vec3` and returns `false` to leave it out, such as points under the ground. |

```lua
local m = require('haylen.math')

local stars = m.poissonDisk3d({volume = m.box({-50, -50, -50}, {50, 50, 50}), minimumDistance = 8, seed = 3})
print(#stars > 100) -- true
```

## Ray casts

A ray cast traces a straight line from one point to another and reports where it first meets a shape. These ray casts need no physics world. A point is a `Vec2` or a table `{x, y}`. A cast returns a hit table, or `nil` when the ray meets nothing.

| Field | Type | Meaning |
| --- | --- | --- |
| `x`, `y` | number | The point of the hit. |
| `normalX`, `normalY` | number | The normal of the surface at the hit, a vector of length 1 that faces the side the ray came from. It is zero when the ray starts inside a solid shape. |
| `distance` | number | The distance from the start of the ray to the hit. |
| `fraction` | number | The distance divided by the length of the ray, from 0 at the start to 1 at the end. |
| `index` | integer | For polygons, chains and lists of segments, the edge or segment that was hit, counted from 1. |

Circles, rectangles and polygons are solid, so a ray that starts inside one hits it at distance 0 with a zero normal. Segments and chains are hit from both sides. The ray casts of [`haylen.spatial2d`](spatial2d.md), [`haylen.navigation2d`](navigation2d.md) and [`haylen.tiled`](tiled.md) return the same tables, with fields of their own.

### m.raycastSegment(from, to, segment)

Casts a ray against one segment, given as `{start, end}` or `{{x1, y1}, {x2, y2}}`.

```lua
local m = require('haylen.math')

local hit = m.raycastSegment({0, 0}, {100, 0}, {{50, -10}, {50, 10}})
print(hit.x, hit.normalX, hit.distance, hit.fraction) -- 50.0 -1.0 50.0 0.5
```

### m.raycastRect(from, to, rect)

Casts a ray against a solid rectangle.

```lua
local m = require('haylen.math')

local door = m.rect(200, 100, 20, 80)
local hit = m.raycastRect({0, 140}, {400, 140}, door)
if hit then
    print('The bolt stops at ' .. hit.x .. '.')
end
```

### m.raycastCircle(from, to, circle)

Casts a ray against a solid circle, given as `{center = point, radius = r}` or `{point, r}`.

```lua
local m = require('haylen.math')

local hit = m.raycastCircle({0, 0}, {600, 0}, {center = {300, 0}, radius = 40})
print(hit.x, hit.normalX) -- 260.0 -1.0
```

### m.raycastPolygon(from, to, polygon)

Casts a ray against a solid simple polygon in either winding. The `index` of the hit is the edge that starts at that corner of the polygon.

```lua
local m = require('haylen.math')

local rock = {{100, -20}, {140, 0}, {100, 20}, {80, 0}}
local hit = m.raycastPolygon({0, 0}, {300, 0}, rock)
print(hit.x, hit.index)
```

### m.raycastChain(from, to, points, loop)

Casts a ray against the segments that join each point to the next. With `loop` set to `true`, the last point also joins the first. The `index` of the hit is the first point of the segment that was hit.

```lua
local m = require('haylen.math')

local ridge = {{0, 100}, {100, 60}, {200, 90}, {300, 40}}
local hit = m.raycastChain({150, -100}, {150, 300}, ridge)
print(hit.y, hit.index)
```

### m.raycastSegments(from, to, segments)

Returns the closest hit among a list of segments, such as the walls of a level. The `index` of the hit is the position of the segment in the list.

```lua
local m = require('haylen.math')

local walls = {{{60, 0}, {60, 100}}, {{30, 0}, {30, 100}}, {{90, 0}, {90, 100}}}
local hit = m.raycastSegments({0, 50}, {200, 50}, walls)
print(hit.index, hit.x) -- 2 30.0
```

### m.raycastSegmentsAll(from, to, segments, limit)

Returns every segment that the ray crosses, as hits in order of distance. The optional `limit` keeps only the first hits, which makes a shot that pierces a number of targets.

```lua
local m = require('haylen.math')

local targets = {{{60, 0}, {60, 100}}, {{30, 0}, {30, 100}}, {{90, 0}, {90, 100}}}
for _, hit in ipairs(m.raycastSegmentsAll({0, 50}, {200, 50}, targets, 2)) do
    print('Pierced target ' .. hit.index .. ' at ' .. hit.x .. '.')
end
```

### m.bounceRay(origin, direction, length, bounces, segments)

Follows a ray of the given `length` from `origin` along `direction`, bouncing off the segments like a laser between mirrors, for at most `bounces` bounces. It returns two things: the list of the hits where it bounced, and then the `x` and `y` where the path ends. The `distance` and `fraction` of each hit measure the whole path up to that bounce. A zero direction raises `bad argument #2 to 'bounceRay' (the direction must not be zero)`, and a negative bounce count raises `bad argument #4 to 'bounceRay' (the bounce count must not be negative)`.

```lua
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local scene = require('haylen.scene')

local mirrors = {{{400, -300}, {400, 300}}, {{-400, -300}, {-400, 300}}, {{-400, -300}, {400, -300}}}
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        local hits, endX, endY = m.bounceRay({0, 0}, {1, -0.4}, 3000, 6, mirrors)
        local x, y = 0, 0
        for _, hit in ipairs(hits) do
            graphics2d.drawLine(x, y, hit.x, hit.y, 2, '#FFFF3030')
            x, y = hit.x, hit.y
        end
        graphics2d.drawLine(x, y, endX, endY, 2, '#FFFF3030')
    end,
})
```

### m.rayFan(origin, angle, spread, count, length, segments)

Casts `count` rays of the given `length`, spread evenly across `spread` radians around `angle`, like a cone of vision. It returns one entry per ray, in order of increasing angle: the hit of that ray, or `false` when it hit nothing.

```lua
local m = require('haylen.math')

local walls = {{{200, -100}, {200, 100}}}
for index, hit in ipairs(m.rayFan({0, 0}, 0, m.pi / 2, 9, 400, walls)) do
    print(index, hit and hit.distance or 'clear')
end
```

## Polygon operations

The table `m.polygon` combines, grows, simplifies and splits shapes. A shape is a list of outlines, each a list of points, and a single outline also works as a shape. An outline alone is filled in either winding. An outline wound the other way from the outline around it is a hole. Results always wind outer outlines so that `m.polygonSignedArea` is positive and holes so that it is negative, and they keep three decimal places.

### m.polygon.unite(shape, other)

Returns the union of both shapes, the area that either covers. Without `other`, it merges the overlapping outlines of `shape`.

```lua
local m = require('haylen.math')

local room = {{0, 0}, {100, 0}, {100, 60}, {0, 60}}
local hall = {{80, 20}, {200, 20}, {200, 40}, {80, 40}}
local floor = m.polygon.unite(room, hall)
print(#floor, m.polygon.area(floor)) -- 1 8000.0
```

### m.polygon.subtract(shape, other)

Returns `shape` without the area of `other`, which can leave holes.

```lua
local m = require('haylen.math')

local wall = {{0, 0}, {200, 0}, {200, 100}, {0, 100}}
local door = {{80, 40}, {120, 40}, {120, 100}, {80, 100}}
print(m.polygon.area(m.polygon.subtract(wall, door))) -- 17600.0
```

### m.polygon.intersect(shape, other)

Returns the area that both shapes cover.

```lua
local m = require('haylen.math')

local light = {{0, 0}, {100, 0}, {100, 100}, {0, 100}}
local room = {{50, 50}, {150, 50}, {150, 150}, {50, 150}}
print(m.polygon.area(m.polygon.intersect(light, room))) -- 2500.0
```

### m.polygon.exclude(shape, other)

Returns the area that exactly one of the shapes covers.

```lua
local m = require('haylen.math')

local a = {{0, 0}, {10, 0}, {10, 10}, {0, 10}}
local b = {{5, 5}, {15, 5}, {15, 15}, {5, 15}}
print(m.polygon.area(m.polygon.exclude(a, b))) -- 150.0
```

### m.polygon.offset(shape, distance, options)

Grows the shape by `distance`, or shrinks it when `distance` is negative, and returns the result. Shrinking can split a shape or make it vanish. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `join` | string | `'round'` | The style of the corners: `'round'`, `'miter'`, `'square'` or `'bevel'`. Round corners follow the arc within one percent of the distance. |
| `miterLimit` | number | `2` | How far a `'miter'` corner may reach, in multiples of the distance, before it is cut square. |

```lua
local m = require('haylen.math')

local island = {{0, 0}, {300, 0}, {300, 200}, {0, 200}}
local beach = m.polygon.offset(island, 16)
local shallows = m.polygon.subtract(m.polygon.offset(island, 48, {join = 'miter'}), beach)
print(#shallows)
```

### m.polygon.simplify(points, tolerance, closed)

Drops the points of an outline or a line that lie closer than `tolerance` to the simpler line, with the Ramer-Douglas-Peucker algorithm, and returns the points that stay. The argument `closed` is `true` when left out, and a closed outline keeps at least three points. An open line keeps both ends.

```lua
local m = require('haylen.math')

local path = {{0, 0}, {50, 1}, {100, 0}, {100, 100}}
print(#m.polygon.simplify(path, 2, false)) -- 3
```

### m.polygon.decompose(shape, maxVertices)

Splits a shape, holes included, into convex pieces of at most `maxVertices` corners, `8` by default, the limit of a polygon of Box2D. The pieces come from a constrained Delaunay triangulation whose triangles merge while they stay convex, and each one winds with a positive area. The method `body:addPolygon` of [`haylen.physics2d`](physics2d.md) uses it for outlines with dents. A negative `maxVertices` raises `bad argument #2 to 'decompose' (expected a non-negative integer)`, one below `3` raises `Convex pieces need at least three vertices.`, and a shape that cannot be split into triangles raises `The shape could not be triangulated.`

```lua
local m = require('haylen.math')

local arrow = {{0, 0}, {60, 0}, {30, 20}, {60, 60}, {0, 60}}
for _, piece in ipairs(m.polygon.decompose(arrow)) do
    print(#piece, m.polygonConvex(piece))
end
```

### m.polygon.area(shape)

Returns the filled area of a shape, where holes count against it.

```lua
local m = require('haylen.math')

local frame = {{{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {{25, 25}, {25, 75}, {75, 75}, {75, 25}}}
print(m.polygon.area(frame)) -- 7500.0
```

## Marching squares

The table `m.marchingSquares` traces the outlines of the areas of a grid that reach a threshold, such as the land of a height map or the solid pixels of a bitmap. The outlines are closed lists of `Vec2` with the same winding as the results of `m.polygon`, so they go straight into `m.polygon.simplify`, `body:addChain` of `haylen.physics2d` and `graphics2d.drawPolygon`. A grid too large to trace raises `The field is too large to trace.`

### m.marchingSquares.trace(values, width, height, options)

Traces a field of `width` times `height` numbers stored row by row. Sample `(x, y)` lies at `origin + (x, y) * spacing`. The outlines pass between samples where the values cross the threshold, and areas that reach the edge of the grid close along the outermost samples. A width or height of `0` gives no outlines, and a count of values that does not match the size raises `The field needs width times height values.` The table `options` is optional.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `threshold` | number | `0.5` | Values at or above it are inside. |
| `spacing` | number | `1` | The distance between neighboring samples. |
| `origin` | Vec2 | `{0, 0}` | The position of the first sample. |

```lua
local m = require('haylen.math')

local noise = m.noise2d(4)
local width, height = 64, 36
local heights = {}
for y = 0, height - 1 do
    for x = 0, width - 1 do
        heights[#heights + 1] = noise:fractal(x / 16, y / 16)
    end
end
local coasts = m.marchingSquares.trace(heights, width, height, {threshold = 0, spacing = 30})
print(#coasts .. ' islands and lakes.')
```

### m.marchingSquares.traceBitmap(pixels, width, height, options)

Traces the pixels that are `true` or not zero. Pixel `(x, y)` covers the square from `origin + (x, y) * spacing` to `origin + (x + 1, y + 1) * spacing`, and the outlines cut diagonally across the corners of pixels. The table `options` is optional and takes `spacing` and `origin`, as `m.marchingSquares.trace` does. A width or height of `0` gives no outlines, and a count of pixels that does not match the size raises `The bitmap needs width times height pixels.`

```lua
local m = require('haylen.math')

local pixels = {
    1, 1, 1, 0,
    1, 0, 1, 0,
    1, 1, 1, 1,
}
local outlines = m.marchingSquares.traceBitmap(pixels, 4, 3, {spacing = 16})
print(#outlines) -- 2
```

## Spline2D

A `Spline2D` is a smooth curve in 2D through or near control points. It is measured by its length, so it can be walked at a steady speed or sampled at even distances. Its parameter runs from `0` at the start to `1` at the end of the whole curve. The [strokes of `haylen.graphics2d`](graphics2d.md#graphics2ddrawstrokepoints-options) draw splines.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `kind` | string | read | `'catmullRom'`, `'bezier'` or `'bSpline'`. |
| `closed` | boolean | read | Whether the curve joins its end back to its start. |
| `points` | list of Vec2 | read | The control points. |
| `segmentCount` | integer | read | The number of pieces of the curve. |
| `length` | number | read | The length of the curve in world units. |

```lua
local m = require('haylen.math')

local loop = m.spline2d({{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {closed = true})
print(loop.kind, loop.closed, #loop.points, loop.segmentCount, loop.length > 400) -- catmullRom true 4 4 true
```

### m.spline2d(points, options)

Returns a new `Spline2D` through the list `points`. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | `'catmullRom'` | The kind `'catmullRom'` passes through every point, with centripetal spacing, which avoids loops and sharp tips. The kind `'bezier'` chains cubic pieces whose points go end, control, control, end and so on. The kind `'bSpline'` stays near the points without passing through them, except for the ends of an open curve. |
| `closed` | boolean | `false` | Joins the last point back to the first. A closed curve wraps its parameters and distances around. |

Each kind needs enough points, or it raises one of these errors: `A Catmull-Rom spline needs at least two points when open and three when closed.`, `A Bezier spline needs 3n + 1 points when open and 3n points when closed.` and `A B-spline needs at least two points when open and three when closed.`

```lua
local m = require('haylen.math')

local track = m.spline2d({{0, 0}, {200, -80}, {400, 0}, {600, -80}}, {kind = 'catmullRom'})
print(track.length)
```

### spline:point(t)

Returns the point of the curve at the parameter `t` as a `Vec2`.

```lua
local m = require('haylen.math')

local arc = m.spline2d({{0, 0}, {50, -50}, {100, 0}})
print(arc:point(0.5))
```

### spline:tangent(t)

Returns the direction of travel at the parameter `t`, as a `Vec2` of length 1. Where the curve stands still, such as on points that repeat, it has no direction and raises `The spline stands still at this parameter, so it has no direction.`.

```lua
local m = require('haylen.math')

local arc = m.spline2d({{0, 0}, {50, -50}, {100, 0}})
print(arc:tangent(0))
```

### spline:pointAtDistance(distance)

Returns the point at `distance` world units along the curve. Use it to move things at a steady speed, whatever the spacing of the control points.

```lua
local m = require('haylen.math')
local scene = require('haylen.scene')

local rail = m.spline2d({{0, 0}, {300, 0}, {300, 300}})
local traveled = 0

scene.push({
    update = function(self, dt)
        traveled = math.min(traveled + 120 * dt, rail.length)
        self.position = rail:pointAtDistance(traveled)
    end,
})
```

### spline:tangentAtDistance(distance)

Returns the direction of travel at `distance` world units along the curve, as a `Vec2` of length 1, such as to turn a cart along its rail. It raises the error of `spline:tangent` where the curve stands still.

```lua
local m = require('haylen.math')

local rail = m.spline2d({{0, 0}, {300, 0}, {300, 300}})
print(m.degrees(rail:tangentAtDistance(rail.length):angle()))
```

### spline:parameterAtDistance(distance)

Returns the parameter at `distance` world units along the curve.

```lua
local m = require('haylen.math')

local rail = m.spline2d({{0, 0}, {300, 0}, {300, 300}})
print(rail:parameterAtDistance(rail.length / 2))
```

### spline:sample(count)

Returns `count` points spread evenly over the parameter. A `count` above 16777216 raises `Sampling a spline takes at most 16777216 points.`

```lua
local m = require('haylen.math')

local fence = m.spline2d({{0, 0}, {100, 40}, {200, 0}, {300, 40}}, {kind = 'bezier'})
print(#fence:sample(10)) -- 10
```

### spline:sampleByDistance(spacing)

Returns points `spacing` world units apart along the curve, plus the end point of an open curve. A spacing that is not finite and above 0, or so small that it would take more than 16777216 steps along the curve, raises `Sampling by distance needs a finite spacing above 0 that takes at most 16777216 steps along the curve.`

```lua
local m = require('haylen.math')

local fence = m.spline2d({{0, 0}, {100, 40}, {200, 0}, {300, 40}}, {kind = 'bezier'})
for _, post in ipairs(fence:sampleByDistance(25)) do
    print(post)
end
```

### spline:flatten(tolerance)

Returns points along the curve, from its start to its end, whose straight lines stay within `tolerance` world units of it. The points sit closer together where the curve bends and far apart where it runs straight. A closed curve does not repeat its start at the end. Every piece of the curve splits into 4 to 1024 parts, so a tiny tolerance stays bounded. A [stroke along a spline](graphics2d.md#graphics2ddrawstrokepoints-options) flattens it within a quarter pixel on its own, and the points also serve as the outline of a polygon or a physics chain. A tolerance that is not finite and above 0 raises `Flattening a spline needs a finite tolerance above 0.`

```lua
local m = require('haylen.math')

local hill = m.spline2d({{0, 300}, {200, 100}, {400, 280}, {600, 120}})
local outline = hill:flatten(1)
print(#outline, outline[1], outline[#outline])
```

## Spline3D

A `Spline3D` is a smooth curve in 3D built from control points, such as a rail, a road, a tube or the track of a camera. Its parameter `t` runs from `0` at the start to `1` at the end of the whole curve, and distances run in world units from its start. The [3D math guide](../3d/math.md#curves) explains the kinds of curve and how frames follow them. Make one with `m.spline3d`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `kind` | string | read | The kind of curve, one of the kinds below. |
| `closed` | boolean | read | Whether the curve returns to its start. |
| `points` | list of `Vec3` | read | A copy of the points the curve was made from. |
| `length` | number | read | The length of the whole curve. |
| `segmentCount` | integer | read | The number of pieces between knots, each a polynomial or a ratio of two. |
| `degree` | integer | read | The degree of the pieces, `1` for linear curves, `2` for quadratic ones and `3` for the other kinds unless a NURBS curve names its own. |
| `weights` | list of numbers | read | The weight of each point of the NURBS curve that holds the spline, `1` for every kind but `'nurbs'`. |
| `knots` | list of numbers | read | The knots of the NURBS curve that holds the spline. |

```lua
local m = require('haylen.math')

local track = m.spline3d({{0, 0, 0}, {10, 2, -5}, {20, 0, -5}, {30, 1, 0}})
print(track.kind, track.segmentCount, track.length > 30) -- catmullRom 3 true
```

### m.spline3d(points, options)

Returns a new `Spline3D` through or around `points`, a list of `Vec3` values in any form a `Vec3` argument accepts. The kinds are these:

| Kind | Meaning |
| --- | --- |
| `'linear'` | Straight segments from point to point. |
| `'catmullRom'` | A smooth curve through every point, the default. |
| `'quadraticBezier'` | Segments whose points go end, control, end and so on, `2n + 1` points when open and `2n` when closed. |
| `'bezier'` | Cubic segments whose points go end, control, control, end and so on, `3n + 1` points when open and `3n` when closed. |
| `'bSpline'` | A smoother curve that stays inside the points without passing through them, except for the ends of an open curve. |
| `'nurbs'` | A curve of any degree with a weight for each point and knots, which draws circles and other conics exactly. |

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | `'catmullRom'` | The kind of curve. |
| `closed` | boolean | `false` | Whether the curve returns to its first point. A `'nurbs'` curve closes through its own points and knots, and `closed` then says that it ends where it starts, so its parameter and distances wrap around. |
| `alpha` | number | `0.5` | For `'catmullRom'` alone: `0` for the uniform parameterization, `0.5` for the centripetal one, which never loops or cusps inside a segment, and `1` for the chordal one. |
| `tension` | number | `0` | For `'catmullRom'` alone: from `0` to `1`, how much the tangents shorten, until the curve runs straight between its points at `1`. |
| `weights` | list of numbers | every weight `1` | For `'nurbs'` alone: a weight above `0` for each point, which pulls the curve toward it. |
| `knots` | list of numbers | clamped uniform knots | For `'nurbs'` alone: as many knots as the points plus the degree plus `1`, never decreasing. The default reaches both end points. |
| `degree` | integer | `3` | For `'nurbs'` alone: the degree from `1` to `9`, which needs at least one point more. |

A count of points that the kind cannot take raises a sentence that says the count it needs, such as `A Bezier spline needs 3n + 1 points when open and 3n points when closed.`. An option for another kind raises `Only a NURBS spline takes weights, knots and a degree.` or `Only a Catmull-Rom spline takes an alpha and a tension.`, an alpha or a tension out of range raises `A Catmull-Rom spline needs an alpha and a tension from 0 to 1.`, knots, weights or a degree that do not describe a NURBS curve raise a sentence that names what is wrong, such as `The NURBS spline needs at least one point more than its degree.`, and a closed NURBS curve that ends away from its start raises `A closed NURBS spline must end where it starts.`.

```lua
local m = require('haylen.math')

local road = m.spline3d({{0, 0, 0}, {20, 0, -5}, {40, 3, 0}}, {alpha = 0.5, tension = 0.2})
local arc = m.spline3d({{1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, {kind = 'nurbs', degree = 2, weights = {1, math.sqrt(0.5), 1}})
print(string.format('%.4f', arc:point(0.3):length())) -- 1.0000, every point lies on the unit circle
```

### m.ellipse3d(center, xAxis, yAxis, options)

Returns a new `Spline3D` that draws an ellipse exactly, or a part of one, such as an orbit, the rim of a dish or an oval track. The point at the angle `a` is `center + xAxis * cos(a) + yAxis * sin(a)`, so `xAxis` reaches from the center to the point at angle `0` and `yAxis` to the point a quarter turn later. Two axes of the same length at right angles draw a circle. The curve is a `'nurbs'` curve of degree `2`, and a whole turn makes it `closed`. Its parameter does not follow the angle evenly, so walk it by distance for an even speed.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `startAngle` | number | `0` | The angle in radians where the curve starts. |
| `endAngle` | number | `2 * math.pi` | The angle in radians where the curve ends, at most a whole turn from the start either way. |

Axes that are not finite or that run along one line raise `The center and the two axes of an ellipse must be finite, and the axes must run in different directions so they span a plane.`, and angles that are equal, more than a whole turn apart or not finite raise `The angles of an ellipse must be finite and apart by more than 0 and at most a whole turn.`.

```lua
local m = require('haylen.math')

local orbit = m.ellipse3d({0, 0, 0}, {12, 0, 0}, {0, 0, -8})
local moon = orbit:pointAtDistance(30)
print(orbit.closed, moon)
```

### m.arc3d(center, start, normal, angle)

Returns a new `Spline3D` that draws an arc of a circle exactly, such as the bend of a road or the swing of a door. The arc turns `start` around the axis through `center` along `normal` by `angle` radians, counterclockwise when the normal points at the viewer, and a negative angle turns it the other way. A start off the plane of the center turns at its own height around the axis. A whole turn makes a closed circle. A center or a start that is not finite or a normal without a direction raises `The center and the start of an arc must be finite, and its normal must have a finite length above 0.`, an angle of `0` or beyond a whole turn raises `The angle of an arc must be a finite number of radians other than 0 and at most a whole turn.`, and a start on the axis raises `The start of an arc lies on its axis, so the arc has no radius.`.

```lua
local m = require('haylen.math')

local straight = m.spline3d({{-20, 0, 10}, {0, 0, 10}}, {kind = 'linear'})
local bend = m.arc3d({0, 0, 0}, {0, 0, 10}, m.up(), math.pi / 2)
local road = m.curvePath3d({straight, bend})
print(road.joins[1]) -- tangent
```

### m.arcThrough3d(start, through, end)

Returns a new `Spline3D` that draws the arc of the circle through three points, from `start` through `through` to `end`, such as an arch that a person places by its ends and its top. Points that are not finite or that lie on one line raise `The three points of an arc must be finite and must not lie on one line, or no circle passes through them.`.

```lua
local m = require('haylen.math')

local arch = m.arcThrough3d({-4, 0, 0}, {0, 4, 0}, {4, 0, 0})
print(string.format('%.1f', arch.length)) -- 12.6, half of a circle of radius 4
```

### spline:point(t)

Returns the point at the parameter `t` as a `Vec3`. A closed curve wraps `t` around, and an open one clamps it to `0` and `1`.

```lua
local m = require('haylen.math')

print(m.spline3d({{0, 0, 0}, {0, 0, -10}}, {kind = 'linear'}):point(0.5)) -- Vec3(0.0, 0.0, -5.0)
```

### spline:derivatives(t)

Returns a table with the `point` at `t` and its `first` and `second` derivatives by the parameter, the velocity and the acceleration of a walk from `0` to `1` over the whole curve.

```lua
local m = require('haylen.math')

local d = m.spline3d({{0, 0, 0}, {0, 0, -10}}):derivatives(0.5)
print(d.point, d.first) -- Vec3(0.0, 0.0, -5.0) Vec3(0.0, 0.0, -10.0)
```

### spline:tangent(t)

Returns the direction of travel at `t` as a `Vec3` of length `1`. Where the curve stops for an instant, the tangent follows the way it leaves. Where it stands still, such as on points that repeat, it raises `The spline stands still at this parameter, so it has no direction.`.

```lua
local m = require('haylen.math')

print(m.spline3d({{0, 0, 0}, {0, 0, -10}}):tangent(0.2)) -- Vec3(0.0, 0.0, -1.0)
```

### spline:pointAtDistance(distance)

Returns the point `distance` world units along the curve, which walks it at an even speed. A closed curve wraps the distance around.

```lua
local m = require('haylen.math')

local rail = m.spline3d({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}}, {kind = 'linear'})
print(rail:pointAtDistance(15)) -- Vec3(10.0, 0.0, -5.0)
```

### spline:tangentAtDistance(distance)

Returns the direction of travel `distance` world units along the curve, such as to turn a cart on its rail. It raises the error of `spline:tangent` where the curve stands still.

```lua
local m = require('haylen.math')

local rail = m.spline3d({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}}, {kind = 'linear'})
print(rail:tangentAtDistance(15)) -- Vec3(0.0, 0.0, -1.0)
```

### spline:parameterAtDistance(distance)

Returns the parameter `t` at `distance` world units along the curve.

```lua
local m = require('haylen.math')

local rail = m.spline3d({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}}, {kind = 'linear'})
print(rail:parameterAtDistance(10)) -- 0.5
```

### spline:closestPoint(point)

Returns a table with the parameter `t`, the `point` of the curve closest to `point` and the `distance` between them, such as to snap a vehicle to its track.

```lua
local m = require('haylen.math')

local rail = m.spline3d({{0, 0, 0}, {10, 0, 0}}, {kind = 'linear'})
local closest = rail:closestPoint({4, 2, 0})
print(string.format('%.1f %.1f', closest.t, closest.distance)) -- 0.4 2.0
```

### spline:sample(count)

Returns `count` points spread evenly over the parameter, both ends included. More than `16777216` points raise `Sampling a spline takes at most 16777216 points.`.

```lua
local m = require('haylen.math')

print(#m.spline3d({{0, 0, 0}, {10, 0, 0}}):sample(5)) -- 5
```

### spline:sampleEvenly(count)

Returns `count` points spread evenly by distance along the curve, both ends included, such as the posts of a fence. It takes as many points as `spline:sample`.

```lua
local m = require('haylen.math')

local posts = m.spline3d({{0, 0, 0}, {10, 0, -2}, {20, 0, 0}}):sampleEvenly(6)
print(#posts) -- 6
```

### spline:sampleByDistance(spacing)

Returns points `spacing` world units apart along the curve from its start, plus the end point of an open curve. A spacing that is not finite and above `0`, or one so small that it takes more than `16777216` steps, raises `Sampling by distance needs a finite spacing above 0 that takes at most 16777216 steps along the curve.`.

```lua
local m = require('haylen.math')

print(#m.spline3d({{0, 0, 0}, {20, 0, 0}}, {kind = 'linear'}):sampleByDistance(4)) -- 6
```

### spline:frames(count, options)

Returns a list of `count` frames spread evenly by distance, both ends included. Each frame is a table with the `position`, the `tangent`, the `normal` and the `binormal`, three directions at right angles. Parallel transport carries the normal along the curve, so the frames never twist more than the curve does, and a closed curve spreads the twist of its loop over its frames so the last one meets the first. The first normal comes from `up`, the up of the world unless the option names another, or from the axis of the world most across the tangent when `up` runs along it. Fewer than `2` frames raise `The frames of a curve need at least 2 frames, one at each end.`, an `up` of zero length raises the error of `vec3:normalized()`, and a curve that stands still raises the error of `spline:tangent`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `up` | `Vec3` | `m.up()` | The direction that the first normal leans toward. |

```lua
local m = require('haylen.math')

local tube = m.spline3d({{0, 0, 0}, {5, 2, -5}, {10, 0, -10}})
for _, frame in ipairs(tube:frames(16)) do
    local ring = frame.position + frame.normal * 0.5
    print(ring)
end
```

## CurvePath3D

A `CurvePath3D` walks several `Spline3D` curves one after another, such as a road of straight pieces and bends. Distances run from the start of the first curve, and the parameter `t` runs from `0` at the start to `1` at the end of the path by distance. Make one with `m.curvePath3d`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `length` | number | read | The length of all the curves together. |
| `closed` | boolean | read | Whether the last curve returns to the start of the first. |
| `curveCount` | integer | read | The number of curves. |
| `joins` | list of strings | read | How smoothly each pair of curves joins, in order, the last one between the last curve and the first for a closed path: `'position'` when only their ends meet, `'tangent'` when their directions agree too, and `'velocity'` when their speeds also agree. |

### m.curvePath3d(curves, options)

Returns a new `CurvePath3D` from a list of `Spline3D` values, which it copies. Each curve must start where the one before it ends, within a small tolerance, and a closed path also needs the last curve to end where the first starts. An empty list raises `A curve path needs at least one curve.`, and a curve that starts away from the end of the one before raises a sentence that starts with `The curves of a path must join` and says how far apart they are.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `closed` | boolean | `false` | Whether the path returns to its start. |

```lua
local m = require('haylen.math')

local straight = m.spline3d({{0, 0, 0}, {10, 0, 0}}, {kind = 'linear'})
local bend = m.spline3d({{10, 0, 0}, {15, 0, 0}, {15, 0, -5}, {15, 0, -10}}, {kind = 'bezier'})
local road = m.curvePath3d({straight, bend})
print(road.curveCount, table.concat(road.joins, ', ')) -- 2 tangent
```

### path:point(t)

Returns the point at the share `t` of the length of the path.

```lua
local m = require('haylen.math')

local road = m.curvePath3d({m.spline3d({{0, 0, 0}, {10, 0, 0}}, {kind = 'linear'})})
print(road:point(0.5)) -- Vec3(5.0, 0.0, 0.0)
```

### path:tangent(t)

Returns the direction of travel at the share `t` of the length, and raises the error of `spline:tangent` where the path stands still.

```lua
local m = require('haylen.math')

local road = m.curvePath3d({m.spline3d({{0, 0, 0}, {10, 0, 0}}, {kind = 'linear'})})
print(road:tangent(0.5)) -- Vec3(1.0, 0.0, 0.0)
```

### path:pointAtDistance(distance)

Returns the point `distance` world units along the path, wrapped around a closed path.

```lua
local m = require('haylen.math')

local first = m.spline3d({{0, 0, 0}, {10, 0, 0}}, {kind = 'linear'})
local second = m.spline3d({{10, 0, 0}, {10, 0, -5}}, {kind = 'linear'})
print(m.curvePath3d({first, second}):pointAtDistance(12)) -- Vec3(10.0, 0.0, -2.0)
```

### path:tangentAtDistance(distance)

Returns the direction of travel `distance` world units along the path.

```lua
local m = require('haylen.math')

local first = m.spline3d({{0, 0, 0}, {10, 0, 0}}, {kind = 'linear'})
local second = m.spline3d({{10, 0, 0}, {10, 0, -5}}, {kind = 'linear'})
print(m.curvePath3d({first, second}):tangentAtDistance(12)) -- Vec3(0.0, 0.0, -1.0)
```

### path:closestPoint(point)

Returns a table with the share `t` of the length at the closest point, the `point` and the `distance` to it, over every curve of the path.

```lua
local m = require('haylen.math')

local road = m.curvePath3d({m.spline3d({{0, 0, 0}, {10, 0, 0}}, {kind = 'linear'})})
print(road:closestPoint({5, 1, 0}).distance) -- 1.0
```

### path:sampleEvenly(count)

Returns `count` points spread evenly by distance along the path, both ends included.

```lua
local m = require('haylen.math')

local road = m.curvePath3d({m.spline3d({{0, 0, 0}, {10, 0, 0}}, {kind = 'linear'})})
print(#road:sampleEvenly(4)) -- 4
```

### path:sampleByDistance(spacing)

Returns points `spacing` world units apart along the path from its start, plus the end point of an open path, with the errors of `spline:sampleByDistance`.

```lua
local m = require('haylen.math')

local road = m.curvePath3d({m.spline3d({{0, 0, 0}, {10, 0, 0}}, {kind = 'linear'})})
print(#road:sampleByDistance(5)) -- 3
```

### path:frames(count, options)

Returns `count` frames spread evenly by distance along the path, which carry their normal across the joins, with the options and errors of `spline:frames`.

```lua
local m = require('haylen.math')

local road = m.curvePath3d({m.spline3d({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}})})
print(#road:frames(8)) -- 8
```

## ParametricCurve3D

A `ParametricCurve3D` is a curve that a formula of its parameter defines, such as a helix, a spring, a spiral staircase, a torus knot or a Lissajous figure. Its parameter `t` runs from `0` at the start to `1` at the end, and like a `Spline3D` it measures its length, walks by distance, finds its closest point and gives frames for tubes. The [3D math guide](../3d/math.md#curves) explains when to use one. Make one with `m.helix`, `m.torusKnot` or `m.lissajous3d`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `length` | number | read | The length of the whole curve. |
| `closed` | boolean | read | Whether the curve ends where it starts, so its parameter and distances wrap around. Torus knots and Lissajous curves are closed. |

### m.helix(options)

Returns a new `ParametricCurve3D` that winds around the up axis, such as a spring, a spiral staircase or the path of a falling leaf. It starts at the point `radius` along the right axis and turns counterclockwise seen from above, rising `pitch` with each turn. An `endRadius` that differs from the radius widens or narrows it into a spiral, and a `pitch` of `0` keeps it flat.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `radius` | number | `1` | The distance from the axis at the start, `0` or more. |
| `endRadius` | number | `radius` | The distance from the axis at the end, `0` or more. |
| `pitch` | number | `1` | How far the curve rises with each turn, negative to go down. |
| `turns` | number | `1` | How many turns it makes, negative to wind the other way, at most `16384` either way. |

A radius below `0` raises `The radius and the end radius of a helix must be finite numbers of 0 or more.`, a pitch that is not finite raises `The pitch of a helix must be a finite number.`, and turns of `0` or too many raise `A helix needs a finite number of turns other than 0 and at most 16384 either way.`.

```lua
local m = require('haylen.math')

local stairs = m.helix({radius = 3, pitch = 4, turns = 2})
for index, step in ipairs(stairs:sampleEvenly(24)) do
    print(index, step)
end
```

### m.torusKnot(options)

Returns a new closed `ParametricCurve3D` that winds `p` times around the up axis and `q` times through the tube of a ring, such as a knotted rail of a coaster or an ornament. The default is the trefoil, the simplest knot.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `p` | integer | `2` | The turns around the up axis, from `1` to `1024`. |
| `q` | integer | `3` | The turns through the tube, from `1` to `1024`, with no common divisor with `p` above `1`. |
| `radius` | number | `1` | The radius of the ring, from the axis to the middle of the tube. |
| `tubeRadius` | number | `0.4` | The radius of the tube, above `0` and below `radius`. |

Windings that share a divisor raise `The windings p and q of a torus knot must be whole numbers from 1 to 1024 with no common divisor above 1, or the knot runs over itself.`, and a tube that does not fit the ring raises `A torus knot needs a finite tube radius above 0 and below its radius.`.

```lua
local m = require('haylen.math')

local knot = m.torusKnot({p = 3, q = 5, radius = 6, tubeRadius = 2})
local frames = knot:frames(200)
print(knot.closed, #frames) -- true 200
```

### m.lissajous3d(options)

Returns a new closed `ParametricCurve3D` that swings along each axis as `amplitude * sin(2 * pi * frequency * t + phase)`, such as the flight of a firefly or a path that a camera drifts along.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `amplitude` | `Vec3` | `{1, 1, 1}` | How far the curve swings along each axis. |
| `frequency` | `Vec3` | `{1, 2, 3}` | How many swings it makes along each axis, whole numbers from `1` to `1024`. |
| `phase` | `Vec3` | `{0, 0, 0}` | Where each swing starts, in radians. |

An amplitude or a phase that is not finite raises `The amplitude and the phase of a Lissajous curve must be finite.`, and a frequency that is not whole raises `The frequencies of a Lissajous curve must be whole numbers from 1 to 1024.`.

```lua
local m = require('haylen.math')

local firefly = m.lissajous3d({amplitude = {4, 1, 4}, frequency = {3, 5, 2}})
print(firefly:pointAtDistance(10))
```

### curve:point(t)

Returns the point at the parameter `t` as a `Vec3`. A closed curve wraps `t` around, and an open one clamps it to `0` and `1`.

```lua
local m = require('haylen.math')

print(m.helix({radius = 2, pitch = 1}):point(0.5):approximately({-2, 0.5, 0})) -- true
```

### curve:derivatives(t)

Returns a table with the `point` at `t` and its `first` and `second` derivatives by the parameter, as `spline:derivatives` does.

```lua
local m = require('haylen.math')

local d = m.helix({radius = 2, pitch = 1}):derivatives(0)
print(d.first.y) -- 1.0, the rise over the whole walk
```

### curve:tangent(t)

Returns the direction of travel at `t` as a `Vec3` of length `1`. Where the curve stands still, such as a helix without a radius or a pitch, it raises `The curve stands still at this parameter, so it has no direction.`.

```lua
local m = require('haylen.math')

print(m.helix({radius = 0, pitch = 3}):tangent(0.5).y) -- 1.0
```

### curve:pointAtDistance(distance)

Returns the point `distance` world units along the curve, which walks it at an even speed. A closed curve wraps the distance around.

```lua
local m = require('haylen.math')

local spring = m.helix({radius = 1, pitch = 0.2, turns = 10})
print(spring:pointAtDistance(spring.length / 2))
```

### curve:tangentAtDistance(distance)

Returns the direction of travel `distance` world units along the curve, with the error of `curve:tangent`.

```lua
local m = require('haylen.math')

local knot = m.torusKnot()
print(knot:tangentAtDistance(1))
```

### curve:parameterAtDistance(distance)

Returns the parameter `t` at `distance` world units along the curve.

```lua
local m = require('haylen.math')

local spring = m.helix({radius = 1, pitch = 0, turns = 1})
print(string.format('%.2f', spring:parameterAtDistance(math.pi))) -- 0.50
```

### curve:closestPoint(point)

Returns a table with the parameter `t`, the `point` of the curve closest to `point` and the `distance` between them.

```lua
local m = require('haylen.math')

local closest = m.helix({radius = 2, pitch = 0}):closestPoint({3, 0, 0})
print(string.format('%.1f', closest.distance)) -- 1.0
```

### curve:sample(count)

Returns `count` points spread evenly over the parameter, both ends included, which the engine evaluates in one call. More than `16777216` points raise `Sampling a spline takes at most 16777216 points.`.

```lua
local m = require('haylen.math')

print(#m.torusKnot():sample(256)) -- 256
```

### curve:sampleEvenly(count)

Returns `count` points spread evenly by distance along the curve, both ends included, with the limit of `curve:sample`.

```lua
local m = require('haylen.math')

local lights = m.helix({radius = 4, pitch = 2, turns = 3}):sampleEvenly(30)
print(#lights) -- 30
```

### curve:sampleByDistance(spacing)

Returns points `spacing` world units apart along the curve from its start, plus the end point of an open curve, with the errors of `spline:sampleByDistance`.

```lua
local m = require('haylen.math')

print(#m.helix({radius = 1, pitch = 0}):sampleByDistance(1) > 0) -- true
```

### curve:frames(count, options)

Returns `count` frames spread evenly by distance, with the options and errors of `spline:frames`. A closed curve spreads the twist of its loop over its frames, so a tube around a knot meets its own start.

```lua
local m = require('haylen.math')

for _, frame in ipairs(m.torusKnot():frames(64)) do
    print(frame.position + frame.normal * 0.1)
end
```

## NurbsSurface

A `NurbsSurface` is a smooth surface over a grid of control points, such as a hull, a patch of terrain or a cloth. It takes two parameters from `0` to `1`: `u` across the columns of the grid and `v` along its rows. Make one with `m.nurbsSurface`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `columns` | integer | read | The points in each row of the grid. |
| `rows` | integer | read | The rows of the grid. |

### m.nurbsSurface(points, options)

Returns a new `NurbsSurface` from `points`, a list of `Vec3` values row after row. Points that do not fill whole rows raise `A NURBS surface needs its points in whole rows of a number of columns above 0.`, and weights, knots or degrees that do not fit raise a sentence that names the direction, such as `The NURBS surface across its columns needs at least one point more than its degree.`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `columns` | integer | none | The points in each row. |
| `weights` | list of numbers | every weight `1` | A weight above `0` for each point. |
| `knotsU` | list of numbers | clamped uniform knots | The knots across the columns. |
| `knotsV` | list of numbers | clamped uniform knots | The knots along the rows. |
| `degreeU` | integer | `3` | The degree across the columns, from `1` to `9`. |
| `degreeV` | integer | `3` | The degree along the rows, from `1` to `9`. |

```lua
local m = require('haylen.math')

local sheet = m.nurbsSurface({{0, 0, 0}, {2, 0, 0}, {0, 0, -2}, {2, 0, -2}}, {columns = 2, degreeU = 1, degreeV = 1})
print(sheet:point(0.5, 0.25)) -- Vec3(1.0, 0.0, -0.5)
```

### surface:point(u, v)

Returns the point of the surface at `u` and `v`, each clamped to `0` and `1`.

```lua
local m = require('haylen.math')

local sheet = m.nurbsSurface({{0, 0, 0}, {2, 0, 0}, {0, 0, -2}, {2, 0, -2}}, {columns = 2, degreeU = 1, degreeV = 1})
print(sheet:point(1, 1)) -- Vec3(2.0, 0.0, -2.0)
```

### surface:derivatives(u, v)

Returns a table with the `point` at `u` and `v` and its derivatives `alongU` and `alongV`, which lie in the plane that touches the surface there.

```lua
local m = require('haylen.math')

local sheet = m.nurbsSurface({{0, 0, 0}, {2, 0, 0}, {0, 0, -2}, {2, 0, -2}}, {columns = 2, degreeU = 1, degreeV = 1})
print(sheet:derivatives(0.5, 0.5).alongU) -- Vec3(2.0, 0.0, 0.0)
```

### surface:normal(u, v)

Returns the normal of the surface at `u` and `v`, the derivative along `u` crossed with the one along `v`, as a `Vec3` of length `1`. Where the surface folds onto a line or a point it raises `The NURBS surface folds onto a line or a point at these parameters, so it has no normal.`.

```lua
local m = require('haylen.math')

local sheet = m.nurbsSurface({{0, 0, 0}, {2, 0, 0}, {0, 0, -2}, {2, 0, -2}}, {columns = 2, degreeU = 1, degreeV = 1})
print(sheet:normal(0.5, 0.5)) -- Vec3(0.0, 1.0, 0.0)
```

## NurbsVolume

A `NurbsVolume` is a smooth block of space over a grid of control points in three directions, such as a lattice that bends a model. It takes three parameters from `0` to `1`: `u` across the columns, `v` along the rows and `w` through the layers. Make one with `m.nurbsVolume`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `columns` | integer | read | The points in each row. |
| `rows` | integer | read | The rows in each layer. |
| `layers` | integer | read | The layers of the block. |

### m.nurbsVolume(points, options)

Returns a new `NurbsVolume` from `points`, a list of `Vec3` values layer after layer and row after row. Points that do not fill whole layers raise `A NURBS volume needs its points in whole layers of a number of columns and rows above 0.`, and weights, knots or degrees that do not fit raise a sentence that names the direction.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `columns` | integer | none | The points in each row. |
| `rows` | integer | none | The rows in each layer. |
| `weights` | list of numbers | every weight `1` | A weight above `0` for each point. |
| `knotsU`, `knotsV`, `knotsW` | list of numbers | clamped uniform knots | The knots of each direction. |
| `degreeU`, `degreeV`, `degreeW` | integer | `3` | The degree of each direction, from `1` to `9`. |

```lua
local m = require('haylen.math')

local corners = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}, {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}}
local block = m.nurbsVolume(corners, {columns = 2, rows = 2, degreeU = 1, degreeV = 1, degreeW = 1})
print(block.layers) -- 2
```

### volume:point(u, v, w)

Returns the point of the volume at `u`, `v` and `w`, each clamped to `0` and `1`.

```lua
local m = require('haylen.math')

local corners = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}, {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}}
local block = m.nurbsVolume(corners, {columns = 2, rows = 2, degreeU = 1, degreeV = 1, degreeW = 1})
print(block:point(0.25, 0.5, 0.75)) -- Vec3(0.25, 0.5, 0.75)
```

## Spring

A `Spring` follows a moving target as fast as it can without going past it, a critically damped spring. Its smooth time is about how long the value takes to reach a target that stands still. It suits cameras, health bars and anything that should settle smoothly.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `value` | number | read and write | The current value. |
| `velocity` | number | read and write | How fast the value changes, per second. |
| `smoothTime` | number | read and write | About how many seconds the value takes to reach a target that stands still. |

```lua
local m = require('haylen.math')

local bar = m.spring(0)
bar.value = 50
bar.smoothTime = 0.25
print(bar.value, bar.velocity, bar.smoothTime) -- 50.0 0.0 0.25
```

### m.spring(value, smoothTime)

Returns a new `Spring` that starts at `value`, `0` by default, with `smoothTime` in seconds, `0.2` by default.

```lua
local m = require('haylen.math')

local zoom = m.spring(1, 0.3)
print(zoom.value) -- 1.0
```

### spring:update(target, dt)

Moves the value toward `target` over `dt` seconds and returns it. The result barely depends on the frame rate.

```lua
local m = require('haylen.math')
local scene = require('haylen.scene')

local health = m.spring(100, 0.25)

scene.push({
    update = function(self, dt)
        self.shownHealth = health:update(40, dt)
    end,
})
```

### m.smoothDamp(current, target, velocity, smoothTime, dt)

Moves a number or a `Vec2` toward `target` like a spring and returns the new value and the new velocity. Use it for values that live elsewhere, such as the position of a camera, and keep the velocity between calls.

```lua
local m = require('haylen.math')
local scene = require('haylen.scene')

local position, velocity = m.vec2(0, 0), m.vec2()

scene.push({
    update = function(self, dt)
        position, velocity = m.smoothDamp(position, {400, 300}, velocity, 0.2, dt)
    end,
})
```

## Signal filters

A signal filter smooths values that change every frame, such as the position of a finger, the speed of a car or the readings of a sensor. Each filter holds from 1 to 4 values at once, its dimensions, 1 by default. Its `update` takes as many numbers and returns them filtered. A filter of two dimensions also takes and returns a `Vec2`. The filters set up their memory when they are made, so an update never allocates.

| Filter | Best for |
| --- | --- |
| [`LowPassFilter`](#lowpassfilter) | Steady smoothing at any frame rate, such as a camera that follows a target or a gauge. |
| [`MovingAverage`](#movingaverage) | The average of the last readings, such as the frames per second or a speed shown to the player. |
| [`MedianFilter`](#medianfilter) | Readings with single wrong values, while steps stay sharp. |
| [`HampelFilter`](#hampelfilter) | Removing spikes while every other value passes exactly. |
| [`KalmanFilter`](#kalmanfilter) | Noisy measurements of a value that drifts, such as the tracked position of a finger or a hand. |

These errors apply to every filter:

- Dimensions outside 1 to 4 raise `A filter has from 1 to 4 dimensions, not <count>.`
- An update with a count of values that differs from the dimensions raises `The filter takes <dimensions> values, not <count>.`
- An update with a value that is infinite or not a number raises `The values of a filter must be finite numbers.`, and the filter stays as it was, since such a value would spoil every later result.
- An unknown option raises `Unknown option "<key>".`

## LowPassFilter

A `LowPassFilter` lets slow changes through and damps fast ones. Make one with `m.lowPassFilter`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `cutoff` | number | read and write | The frequency in hertz. Changes slower than it pass, and faster ones are damped. A higher cutoff follows faster. |
| `dimensions` | integer | read | The number of values the filter holds. |

### m.lowPassFilter(options)

Returns a new `LowPassFilter`. The table `options` is optional and takes `cutoff`, `5` by default, and `dimensions`, `1` by default. A cutoff that is not positive raises `The cutoff of a low-pass filter is a positive number of hertz.`

```lua
local m = require('haylen.math')

local gauge = m.lowPassFilter({cutoff = 2})
print(gauge.cutoff, gauge.dimensions) -- 2.0 1
```

### lowPass:update(dt, ...)

Filters the values and returns them. The seconds since the last update come first, so the smoothing is the same at any frame rate, and seconds below 0 count as 0. The first update returns its values as they are. A `dt` that is not a number raises `The seconds of a low-pass filter update must be a number.`

```lua
local input = require('haylen.input')
local m = require('haylen.math')
local scene = require('haylen.scene')

-- A cursor that follows the mouse smoothly.
local smooth = m.lowPassFilter({cutoff = 8, dimensions = 2})

scene.push({
    update = function(self, dt)
        self.x, self.y = smooth:update(dt, input.mousePosition())
    end,
})
```

### lowPass:reset()

Forgets the past values, so the next update starts over.

```lua
local m = require('haylen.math')

local gauge = m.lowPassFilter()
gauge:update(0.1, 10)
gauge:reset()
print(gauge:update(0.1, 50)) -- 50.0
```

## MovingAverage

A `MovingAverage` returns the average of the last updates, its window. It follows a steady change with a delay of half the window. Until the window fills, it averages the updates so far. Make one with `m.movingAverage`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `window` | integer | read | How many updates it averages. |
| `dimensions` | integer | read | The number of values the filter holds. |

### m.movingAverage(options)

Returns a new `MovingAverage`. The table `options` is optional and takes `window`, `10` by default and from 1 to 1024, and `dimensions`, `1` by default. A window out of range raises `The window of a filter holds from 1 to 1024 updates, not <window>.`

```lua
local m = require('haylen.math')

local average = m.movingAverage({window = 4})
print(average.window, average.dimensions) -- 4 1
```

### average:update(...)

Adds the values to the window and returns the average of the window.

```lua
local m = require('haylen.math')
local scene = require('haylen.scene')

local frames = m.movingAverage({window = 60})

scene.push({
    update = function(self, dt)
        self.fps = 1 / frames:update(dt)
    end,
})
```

### average:reset()

Empties the window.

```lua
local m = require('haylen.math')

local average = m.movingAverage({window = 3})
average:update(10)
average:reset()
print(average:update(4)) -- 4.0
```

## MedianFilter

A `MedianFilter` returns the median of the last updates, the middle value once they are sorted. It drops single wrong readings and keeps steps sharp, where an average would smear both. Make one with `m.medianFilter`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `window` | integer | read | How many updates it looks at. |
| `dimensions` | integer | read | The number of values the filter holds. |

### m.medianFilter(options)

Returns a new `MedianFilter`. The table `options` is optional and takes `window`, `5` by default and from 1 to 255, and `dimensions`, `1` by default. A window out of range raises `The window of a filter holds from 1 to 255 updates, not <window>.`

```lua
local m = require('haylen.math')

local distance = m.medianFilter({window = 3})
print(distance.window) -- 3
```

### median:update(...)

Adds the values to the window and returns the median of the window.

```lua
local m = require('haylen.math')

local distance = m.medianFilter({window = 3})
print(distance:update(1.0), distance:update(9.0), distance:update(1.5)) -- 1.0 5.0 1.5
```

### median:reset()

Empties the window.

```lua
local m = require('haylen.math')

local distance = m.medianFilter()
distance:update(100)
distance:reset()
print(distance:update(2)) -- 2.0
```

## HampelFilter

A `HampelFilter` lets every value through as it is, except a spike. A value is a spike when it lies farther from the median of the last updates than `threshold` times their spread. The filter then returns the median instead. Make one with `m.hampelFilter`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `outlier` | boolean | read | Whether the last update replaced a spike. |
| `window` | integer | read | How many updates it looks at. |
| `threshold` | number | read | How many spreads away from the median a spike lies. |
| `dimensions` | integer | read | The number of values the filter holds. |

### m.hampelFilter(options)

Returns a new `HampelFilter`. The table `options` is optional and takes `window`, `7` by default and from 1 to 255, `threshold`, `3` by default, and `dimensions`, `1` by default. A window out of range raises `The window of a filter holds from 1 to 255 updates, not <window>.`, and a threshold that is not positive raises `The threshold of a Hampel filter is a positive number of spreads.`

```lua
local m = require('haylen.math')

local speed = m.hampelFilter({window = 5, threshold = 3})
print(speed.window, speed.threshold) -- 5 3.0
```

### hampel:update(...)

Adds the values to the window and returns them, or the median of the window in place of a spike.

```lua
local m = require('haylen.math')

local speed = m.hampelFilter({window = 5, threshold = 3})
for _, reading in ipairs({20, 21, 20.5, 21.2, 300, 20.8}) do
    -- The 300 becomes the median, and the rest pass as they are.
    print(speed:update(reading), speed.outlier)
end
```

### hampel:reset()

Empties the window.

```lua
local m = require('haylen.math')

local speed = m.hampelFilter()
speed:update(20)
speed:reset()
print(speed:update(80), speed.outlier) -- 80.0 false
```

## KalmanFilter

A `KalmanFilter` estimates values that drift, from measurements with noise. It weighs each measurement against its own estimate by how sure it is of each. Make one with `m.kalmanFilter`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `uncertainty` | number | read | How unsure the filter is of its estimate of the first value. It falls as measurements arrive. |
| `dimensions` | integer | read | The number of values the filter holds. |

### m.kalmanFilter(options)

Returns a new `KalmanFilter`. The table `options` is optional. A larger `processNoise`, `0.01` by default, follows real changes faster. A larger `measurementNoise`, `1` by default, smooths more. The option `dimensions` is `1` by default. A noise that is not positive raises `The process noise and the measurement noise of a Kalman filter are positive numbers.`

```lua
local m = require('haylen.math')

local finger = m.kalmanFilter({processNoise = 0.5, measurementNoise = 20, dimensions = 2})
print(finger.dimensions) -- 2
```

### kalman:update(...)

Takes the new measurements and returns the estimates. The first update returns its values as they are.

```lua
local input = require('haylen.input')
local m = require('haylen.math')
local scene = require('haylen.scene')

-- The first finger on the screen, tracked without the jitter of its readings.
local finger = m.kalmanFilter({processNoise = 0.5, measurementNoise = 20, dimensions = 2})

scene.push({
    update = function(self, dt)
        local touch = input.touches()[1]
        if touch then
            self.x, self.y = finger:update(touch.x, touch.y)
        end
    end,
})
```

### kalman:reset()

Forgets the estimates, so the next update starts over.

```lua
local m = require('haylen.math')

local sensor = m.kalmanFilter()
sensor:update(5)
sensor:reset()
print(sensor:update(12)) -- 12.0
```

## ShuffleBag

A `ShuffleBag` deals items in a random order without repeats until the bag runs out, then fills itself again. Every item keeps its share, and streaks stay short. Use it for loot drops, playlists and enemy waves that should feel fair.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `remaining` | integer | read | The items left before the next refill. |
| `size` | integer | read | The items of a full bag. |

### m.shuffleBag(items, options)

Returns a new `ShuffleBag` of the list `items`. The table `options` is optional. In it, item `i` goes into the bag `counts[i]` times, once by default, and `seed` seeds the generator of the bag, or it takes the default seed of `m.random`. A `counts` list of another length raises `bad argument #2 to 'shuffleBag' (expected one count per item)`, and a bag without items raises `A shuffle bag needs at least one item.`

```lua
local m = require('haylen.math')

local drops = m.shuffleBag({'coin', 'gem', 'heart'}, {counts = {6, 1, 3}, seed = 7})
print(drops.size, drops.remaining) -- 10 10
```

### bag:next(random)

Deals the next item, and fills the bag again first when it is empty. A `Random` given as `random` draws instead of the generator of the bag.

```lua
local m = require('haylen.math')

local drops = m.shuffleBag({'coin', 'gem', 'heart'}, {counts = {6, 1, 3}})
for i = 1, 10 do
    print(drops:next())
end
```

### bag:refill()

Puts every dealt item back.

```lua
local m = require('haylen.math')

local bag = m.shuffleBag({'a', 'b'})
bag:next()
bag:refill()
print(bag.remaining) -- 2
```

## WeightedChoice

A `WeightedChoice` picks items in proportion to fixed weights, in the same short time however many items it has, with the alias method of Vose. It pays off when the same weights serve many picks. For weights that change between picks, use [`random:weightedIndex`](#randomweightedindexweights).

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of items. |

### m.weightedChoice(items, weights, seed)

Returns a new `WeightedChoice` of the list `items` and a list of `weights` of the same length. The whole number `seed` seeds the generator of the choice, or it takes the default seed of `m.random`. A list of weights of another length raises `bad argument #2 to 'weightedChoice' (expected one weight per item)`. A weight that is negative or not finite raises `Weights must be finite and not negative.`, and weights that are all zero raise `A weighted choice needs at least one positive weight.`

```lua
local m = require('haylen.math')

local loot = m.weightedChoice({'common', 'rare', 'legendary'}, {90, 9, 1}, 42)
print(loot.size) -- 3
```

### choice:pick(random)

Returns an item. A `Random` given as `random` draws instead of the generator of the choice.

```lua
local m = require('haylen.math')

local loot = m.weightedChoice({'common', 'rare', 'legendary'}, {90, 9, 1})
print(loot:pick(), loot:pick(m.random(5)))
```

### choice:probability(item)

Returns the chance of the item at the position `item`, counted from 1, or `0` for a position beyond the items. A position below 1 raises `bad argument #1 to 'probability' (expected an item number from 1)`.

```lua
local m = require('haylen.math')

local loot = m.weightedChoice({'common', 'rare', 'legendary'}, {6, 1, 1})
print(loot:probability(3), loot:probability(4)) -- 0.125 0.0
```

## Errors

These messages come from the functions and methods of the module. Messages that start with `bad argument` come from Lua, which adds the position and the name of the function.

| Message | Cause |
| --- | --- |
| `Expected a number in field "<field>".` | A table given as a `Vec2`, a `Vec3`, a `Vec4`, a `Quat`, a `Plane`, a `Rect`, a `Color` or a circle misses a number. |
| `bad argument #<n> to '<function>' (Vec3 or table with x, y and z expected, got <type>)` | A `Vec3` argument has another type. |
| `The vector has no direction, because its length is zero.` | A zero vector was normalized. |
| `The vector has no direction, because one of its components is not a finite number.` | A vector with an infinite component or one that is not a number was normalized. |
| `A vector projects only onto a vector with a direction, and this one has a zero or infinite length.` | The vector of `vec3:project` or the normal of `vec3:projectOnPlane` has no direction. |
| `A matrix table lists its <count> numbers by columns, and this one holds <count> values.` | A table given as a `Mat3` or a `Mat4` has another count of values. |
| `The value <place> of a matrix table must be a number.` | A table given as a matrix holds something else. |
| `A row or a column of this matrix is a whole number from 1 to <count>, not <index>.` | The index of `get`, `column` or `row` of a matrix is out of range. |
| `The matrix has no inverse, because it squashes space flat and its determinant is zero.` | A singular matrix was inverted. |
| `The matrix is not affine, because its last row is not 0, 0, 0 and 1.` | The matrix of `mat4:affineInverse` is not affine. |
| `Only an affine matrix, whose last row is 0, 0, 0 and 1, splits into translation, rotation and scale.` | The matrix of `mat4:decompose` is not affine. |
| `The matrix scales an axis to zero or to infinity, so it holds no rotation.` | The matrix of `mat4:decompose` scales an axis to zero. |
| `The eye and the target of a view are the same point, so the view has no direction.` | The eye and the target of `m.lookAt` are equal. |
| `The field of view of a perspective must be an angle above 0 and below pi radians.` | The `fieldOfView` of `m.perspective` is out of range. |
| `The aspect of a perspective must be a finite number above 0.` | The `aspect` of `m.perspective` is not above 0. |
| `The near plane of <projection> must be at a finite distance above 0.` | The `nearPlane` of a perspective is not above 0. |
| `The far plane of <projection> must lie beyond its near plane.` | The `farPlane` of a perspective is not beyond its `nearPlane`. |
| `The angles of an asymmetric perspective must lie within a quarter turn of the direction it looks along, with left below right and bottom below top.` | The angles of `m.asymmetricPerspective` are out of range or out of order. |
| `The bounds of an orthographic projection must be finite numbers, with left apart from right, bottom apart from top and the near plane apart from the far plane.` | The bounds of `m.orthographic` are not finite or meet. |
| `An oblique near plane must face away from the camera, so the camera must lie behind the plane.` | The plane of `mat4:obliqueNearPlane` faces the camera. |
| `An oblique near plane must cross the view before its far end.` | The plane of `mat4:obliqueNearPlane` misses the view. |
| `The quaternion holds no rotation, because its length is zero or not a finite number.` | A quaternion of length zero was normalized. |
| `The quaternion has no inverse, because its length is zero or not a finite number.` | A quaternion of length zero was inverted. |
| `The rotation does not turn, so it has no axis.` | The rotation of `quat:axis` is the identity. |
| `A rotation that looks along a direction needs a direction with a finite length above zero.` | The direction of `m.lookRotation` or `m.lookAt` has no length. |
| `The up direction needs a finite length above zero and must not run along the direction to look along, or it cannot choose how the view turns around that direction.` | The `up` of `m.lookRotation` or `m.lookAt` is zero or runs along the view. |
| `The three points lie on one line, so they span no plane.` | The points of `m.planeFromPoints` lie on one line. |
| `The normal of a plane must have a length of one. Make the plane with "plane" of "haylen.math", which scales its normal.` | The `normal` of a plane was set to a vector of another length. |
| `The direction of a ray must have a length of one. Make the ray with "ray3d" of "haylen.math", which scales its direction.` | The `direction` of a ray was set to a vector of another length. |
| `A ray casts against a Triangle, a Box, an Obb, a Sphere, a Plane or a Capsule, not a <type>.` | The shape of `ray:cast` has another type. |
| `The bounds of points need at least one point.` | The list of `m.boxBounds` is empty. |
| `A box is placed only by an affine matrix, whose last row is 0, 0, 0 and 1.` | The matrix of `box:transformed` is not affine. |
| `A radius must be a finite number of 0 or more.` | The radius of a sphere or a capsule is negative or not finite. |
| `The half extents of an oriented box must be finite numbers of 0 or more.` | The half extents of an `Obb` are negative or not finite. |
| `The triangle has no area, so it has no normal.` | The triangle of `triangle:normal` has no area. |
| `The transform scales an axis to zero or to infinity, so nothing undoes it.` | A transform with a scale of zero was undone or placed a normal. |
| `A transform whose scale differs between axes has no inverse of the same form. Invert its matrix instead.` | The transform of `transform:inverse` has a scale that differs between axes. |
| `A blend of dual quaternions needs one weight for each dual quaternion.` | The lists of `m.blendDualQuats` have different lengths. |
| `A blend of dual quaternions needs at least one dual quaternion.` | The list of `m.blendDualQuats` is empty. |
| `The weights of a blend of dual quaternions must be finite and not negative.` | A weight of `m.blendDualQuats` is negative or not finite. |
| `The weights of a blend of dual quaternions add up to zero, so they blend nothing.` | The weights of `m.blendDualQuats` are all zero. |
| `The dual quaternion holds no rotation, because the length of its real part is zero or not a finite number.` | A dual quaternion of length zero was normalized. |
| `Expected a point in field "<field>".` | A table given as a circle or a segment misses a point. |
| `bad argument #<n> to '<function>' (Color, color text or table with r, g and b expected, got <type>)` | A color argument has another type. |
| `bad argument #1 to 'color' (invalid color text, expected #RRGGBB or #AARRGGBB)` | Color text is not valid. |
| `The type "haylen.<Type>" has no member "<name>".` | The code read a member that a value does not have. |
| `The type "haylen.<Type>" has no writable property "<name>".` | The code assigned a member that a value does not have. |
| `bad argument #1 to 'fromRgba8' (expected an integer from 0 to 255)` | A channel of `m.fromRgba8` is out of range. |
| `bad argument #1 to 'fromHex' (expected an integer from 0 to 0xFFFFFFFF)` | The number of `m.fromHex` is out of range. |
| `bad argument #1 to 'inset' (number or table with left, top, right and bottom expected, got <type>)` | The insets of `rect:inset` have another type. |
| `bad argument #3 to 'clamp' (expected a maximum of at least the minimum)` | The minimum of `m.clamp` is above its maximum. |
| `Unknown option "<key>".` | An options table or a curve table has a key that it does not take. |
| `bad argument #1 to 'ease' (unknown value '<name>')` | The curve name of `m.ease` is unknown. |
| `bad argument #1 to 'ease' (easing curve name, function or table expected, got <type>)` | The curve of `m.ease` has another type. |
| `An overshoot only applies to the back curves.` | A curve table has an `overshoot` on another curve. |
| `An amplitude and a period only apply to the elastic curves.` | A curve table has an `amplitude` or `period` on another curve. |
| `An elastic curve needs a positive period.` | The `period` of an elastic curve is not positive. |
| `A steps curve needs between 1 and 16777216 steps.` | The `steps` of a curve are out of range. |
| `A "cubicBezier" curve needs the four numbers "x1", "y1", "x2" and "y2".` | The `cubicBezier` list does not hold four numbers. |
| `The x coordinates of a cubic Bézier curve must be between 0 and 1.` | An x handle of a Bézier curve is out of range. |
| `The "points" of an easing curve are a list of heights or of {x, y} points, not a <type>.` | The `points` of a curve are not a table. |
| `A points curve needs at least two points.` | The `points` of a curve hold fewer than two points. |
| `The points of a curve must be ordered by x.` | The `points` of a curve are out of order. |
| `An easing function must return a number.` | A curve function returned something else. |
| `bad argument #1 to 'integer' (number has no integer representation)` | An argument of `random:integer` has a fraction. |
| `bad argument #1 to 'integer' (integer out of range)` | An argument of `random:integer` does not fit in 32 bits. |
| `bad argument #1 to 'weightedIndex' (expected at least one weight)` | The weights of `random:weightedIndex` are empty. |
| `Weights must be finite and not negative.` | A weight of `random:weightedIndex` or `m.weightedChoice` is negative or not finite. |
| `A weighted pick needs at least one positive weight.` | The weights of `random:weightedIndex` are all zero. |
| `A random state needs 32 bytes.` | The state of `random:setState` has another length. |
| `A random state cannot be all zeros.` | The state of `random:setState` holds only zero bytes. |
| `The option "<option>" is required.` | The `columns` or `rows` of a noise grid are missing. |
| `The option "<option>" must be a whole number from 1 to 16384.` | The `columns` or `rows` of a noise grid are out of range. |
| `The grid holds <count> samples, more than the limit of 16777216.` | A noise grid is too large. |
| `The option "kind" must be "perlin", "simplex", "fractal" or "worley".` | The `kind` of a noise grid is another value. |
| `The option "octaves" must be a whole number from 1 to 16.` | The `octaves` of a noise grid are out of range. |
| `The buffer of "into" holds <size> floats, fewer than the <count> samples of the grid.` | The `into` buffer of a noise grid is too small. |
| `Poisson distances must be finite.` | A distance of `m.poissonDisk2d` is infinite or not a number. |
| `Poisson sampling needs an area whose edges are finite.` | The `area` of `m.poissonDisk2d` has an edge that is infinite or not a number. |
| `A varying Poisson distance needs a maximum distance of at least the minimum distance.` | A `distance` function comes without a valid `maximumDistance`. |
| `Poisson sampling needs between 1 and 1000 attempts.` | The `attempts` of `m.poissonDisk2d` are out of range. |
| `The minimum distance is too small for the area, whose sampling grid would need more than 16777216 cells.` | The area of `m.poissonDisk2d` is too large for its distance. |
| `A Poisson distance function must return a number, not <type>.` | A `distance` function returned something else. |
| `A Poisson distance function must return finite distances.` | A `distance` function returned an infinite distance or one that is not a number. |
| `bad argument #2 to 'bounceRay' (the direction must not be zero)` | The direction of `m.bounceRay` is zero. |
| `bad argument #4 to 'bounceRay' (the bounce count must not be negative)` | The bounce count of `m.bounceRay` is negative. |
| `bad argument #2 to 'decompose' (expected a non-negative integer)` | The `maxVertices` of `m.polygon.decompose` is negative. |
| `Convex pieces need at least three vertices.` | The `maxVertices` of `m.polygon.decompose` is below 3. |
| `The shape could not be triangulated.` | The shape of `m.polygon.decompose` cannot be split into triangles. |
| `The field needs width times height values.` | The values of `m.marchingSquares.trace` do not match the size. |
| `The bitmap needs width times height pixels.` | The pixels of `m.marchingSquares.traceBitmap` do not match the size. |
| `The field is too large to trace.` | A field of marching squares is too large. |
| `A Catmull-Rom spline needs at least two points when open and three when closed.` | A `'catmullRom'` spline has too few points. |
| `A Bezier spline needs 3n + 1 points when open and 3n points when closed.` | A `'bezier'` spline has a count of points that does not fit. |
| `A B-spline needs at least two points when open and three when closed.` | A `'bSpline'` spline has too few points. |
| `The two lists of matrices must hold as many matrices each, but they hold <count> and <count>.` | The lists of `m.matrixBatch.multiplyPairs` have different lengths. |
| `Spherical harmonics take 9 coefficients, not <count>.` | The list of `m.sphericalHarmonics` holds another count. |
| `The side of a cubemap must be from 1 to 16384 texels.` | The `size` of `m.sphericalHarmonicsFromCubemap` is out of range. |
| `A cubemap of side <size> holds <count> floats, the red, green and blue of each texel of its 6 faces, not <size>.` | The buffer of `m.sphericalHarmonicsFromCubemap` has another size. |
| `The width and the height of a panorama must be from 1 to 16384 texels.` | A side of `m.sphericalHarmonicsFromPanorama` is out of range. |
| `A panorama of <width> by <height> texels holds <count> floats, the red, green and blue of each texel, not <size>.` | The buffer of `m.sphericalHarmonicsFromPanorama` has another size. |
| `A closed NURBS spline must end where it starts.` | A `'nurbs'` spline is `closed` but ends away from its start. |
| `The center and the two axes of an ellipse must be finite, and the axes must run in different directions so they span a plane.` | The axes of `m.ellipse3d` run along one line or are not finite. |
| `The angles of an ellipse must be finite and apart by more than 0 and at most a whole turn.` | The angles of `m.ellipse3d` are equal, too far apart or not finite. |
| `The center and the start of an arc must be finite, and its normal must have a finite length above 0.` | A value of `m.arc3d` is not finite, or its normal has no direction. |
| `The angle of an arc must be a finite number of radians other than 0 and at most a whole turn.` | The angle of `m.arc3d` is out of range. |
| `The start of an arc lies on its axis, so the arc has no radius.` | The start of `m.arc3d` lies on the line through its center along its normal. |
| `The three points of an arc must be finite and must not lie on one line, or no circle passes through them.` | The points of `m.arcThrough3d` lie on one line or are not finite. |
| `The radius and the end radius of a helix must be finite numbers of 0 or more.` | A radius of `m.helix` is negative or not finite. |
| `The pitch of a helix must be a finite number.` | The `pitch` of `m.helix` is not finite. |
| `A helix needs a finite number of turns other than 0 and at most 16384 either way.` | The `turns` of `m.helix` are out of range. |
| `The windings p and q of a torus knot must be whole numbers from 1 to 1024 with no common divisor above 1, or the knot runs over itself.` | The `p` and `q` of `m.torusKnot` are out of range or share a divisor. |
| `A torus knot needs a finite tube radius above 0 and below its radius.` | The tube of `m.torusKnot` does not fit its ring. |
| `The amplitude and the phase of a Lissajous curve must be finite.` | The `amplitude` or the `phase` of `m.lissajous3d` is not finite. |
| `The frequencies of a Lissajous curve must be whole numbers from 1 to 1024.` | A `frequency` of `m.lissajous3d` is out of range or not whole. |
| `The curve stands still at this parameter, so it has no direction.` | The tangent of a `ParametricCurve3D` was asked where it does not move. |
| `Sampling a spline takes at most 16777216 points.` | The `count` of `spline:sample` is above 16777216. |
| `Sampling by distance needs a finite spacing above 0 that takes at most 16777216 steps along the curve.` | The spacing of `spline:sampleByDistance` is out of range. |
| `Flattening a spline needs a finite tolerance above 0.` | The tolerance of `spline:flatten` is not finite and above 0. |
| `A filter has from 1 to 4 dimensions, not <count>.` | The `dimensions` of a filter are out of range. |
| `The filter takes <dimensions> values, not <count>.` | An update of a filter got another count of values. |
| `The values of a filter must be finite numbers.` | An update of a filter got a value that is infinite or not a number. |
| `The seconds of a low-pass filter update must be a number.` | The `dt` of `lowPass:update` is not a number. |
| `The cutoff of a low-pass filter is a positive number of hertz.` | The `cutoff` of a low-pass filter is not positive. |
| `The window of a filter holds from 1 to <maximum> updates, not <window>.` | The `window` of a filter is out of range. |
| `The threshold of a Hampel filter is a positive number of spreads.` | The `threshold` of a Hampel filter is not positive. |
| `The process noise and the measurement noise of a Kalman filter are positive numbers.` | A noise of a Kalman filter is not positive. |
| `bad argument #2 to 'shuffleBag' (expected one count per item)` | The `counts` of a shuffle bag have another length. |
| `A shuffle bag needs at least one item.` | A shuffle bag would hold no item. |
| `bad argument #2 to 'weightedChoice' (expected one weight per item)` | The weights of a weighted choice have another length. |
| `A weighted choice needs at least one positive weight.` | The weights of a weighted choice are all zero. |
| `bad argument #1 to 'probability' (expected an item number from 1)` | The position of `choice:probability` is below 1. |
