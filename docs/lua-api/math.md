# 📖 `haylen.math`

The module `haylen.math` holds the math that games and apps need, and the value types that every other module passes around: `Vec2`, `Rect`, `Color` and `Transform2D`. It also has seeded random numbers, noise, easing curves and scalar helpers. For shapes, it tests circles and segments, casts rays, combines and splits polygons and traces outlines with marching squares. It ends with splines, springs, signal filters, shuffle bags, weighted choices and Poisson disk sampling. Use it for gameplay math, for procedural generation and for any position, area, color or transform that goes to another module. This page has no guide of its own, so it starts with a short tour of the value types. The standard `math` library of Lua stays available for everything else.

```lua
local m = require('haylen.math')
```

The examples name the module `m`, so the standard `math` library keeps its name.

## Value types

Four value types carry positions, areas, colors and transforms through the whole engine:

- A [`Vec2`](#vec2) is a 2D vector with `x` and `y`. It is a point, such as a position, or a direction with a length, such as a velocity.
- A [`Rect`](#rect) is a rectangle that is not rotated, with its top left corner at `x`, `y` and a `width` and `height`.
- A [`Color`](#color) has red, green, blue and alpha components from 0 to 1. Alpha is the opacity, 0 for invisible and 1 for solid.
- A [`Transform2D`](#transform2d) moves, rotates, scales and skews points, such as the placement of a sprite or the chain from a child to its parent.

The y axis points down, as on the screen: a larger `y` is lower. Angles are in radians, and a positive angle turns from the positive x axis toward the positive y axis, which is clockwise on the screen. The functions `m.radians` and `m.degrees` convert between radians and degrees.

```lua
local m = require('haylen.math')

local position = m.vec2(100, 50)
local velocity = m.fromAngle(m.radians(90), 30)
local hitbox = m.fromCenter(position, {32, 48})
local tint = m.color('#FF66CCFF')
local placement = m.transform(position, m.radians(45), {2, 2})

position = position + velocity
print(position, hitbox:contains(position), tint.b, placement:apply({0, 0}))
```

### Passing values

Every function of every engine module accepts plain tables wherever it expects one of these values, so making a userdata is optional. Use the form that reads best.

| Type | Accepted forms |
| --- | --- |
| `Vec2` | A `Vec2`, `{x = 10, y = 20}` or `{10, 20}`. |
| `Rect` | A `Rect`, `{x = 0, y = 0, width = 100, height = 50}` or `{0, 0, 100, 50}`. |
| `Color` | A `Color`, `'#AARRGGBB'` or `'#RRGGBB'` text where the `#` is optional, `{r = 1, g = 0.5, b = 0, a = 1}` or `{1, 0.5, 0}`. Components go from `0` to `1`, and alpha is `1` when a table leaves it out. |

The functions `m.vec2`, `m.rect` and `m.color` take the same forms as a single argument, which also copies an existing value. These errors come from wrong values:

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

The values `Vec2`, `Rect`, `Color`, `Transform2D`, `Random` and `Noise2D` are userdata, objects that the engine owns. Assigning one to another variable shares the same object, so a change made through one variable shows through the other. Operators and methods always return new values, and engine functions copy the values they receive. Copy a value with its constructor, such as `m.vec2(other)`.

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

Returns a vector of length `1` in the same direction. A zero vector stays zero.

```lua
local m = require('haylen.math')

print(m.vec2(0, -8):normalized()) -- Vec2(0.0, -1.0)
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

A `Color` has red, green, blue and alpha components from `0` to `1`. The components are straight, not premultiplied: red, green and blue do not already include the alpha. Make a color with `m.color` or one of the functions below.

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

### color:luminance()

Returns the relative luminance of the color in linear light, from 0 for black to 1 for white. It is the measure of brightness that the contrast rules of accessibility read.

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

The operator `a * b` multiplies component by component, which tints one color by another. The operator `a == b` compares all components, and `tostring(color)` gives the same text as `color:toHex`.

```lua
local m = require('haylen.math')

print(m.color('#FFFFFFFF') * m.color(0.5, 0.5, 1)) -- #FF8080FF
print(m.color('#00000000') == m.color(0, 0, 0, 0)) -- true
```

## Transform2D

A `Transform2D` is an affine transform: it moves, rotates, scales and skews points in one step. Make one with `m.transform`, `m.identity`, `m.translation`, `m.rotation` or `m.scaling`. It maps a point `p` to `(a * p.x + c * p.y + tx, b * p.x + d * p.y + ty)`.

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

### m.transform(position, rotation, scale, skew)

Returns a new `Transform2D` that scales by `scale`, then skews by `skew`, then rotates by `rotation` radians, then moves by `position`. The `Vec2` `position` is required. The `rotation` is `0` when left out, and the `Vec2` `scale` is `{1, 1}`. The `skew` is a `Vec2` of angles in radians, `{0, 0}` by default, that shears the shape. The angle `skew.x` turns the local y axis, and `skew.y` turns the local x axis, the same way as `rotation` turns.

```lua
local m = require('haylen.math')

local turret = m.transform({400, 300}, m.pi / 2, {2, 2})
print(turret:apply({16, 0})) -- close to Vec2(400.0, 332.0)

-- Leaning the y axis by 15 degrees shears a banner sideways.
local banner = m.transform({0, 0}, 0, {1, 1}, {m.radians(15), 0})
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

local ship = m.transform({500, 300}, m.pi / 4)
print(ship:apply({40, 0}))
```

### transform:applyVector(vector)

Returns `vector` transformed without the translation, which suits directions and offsets.

```lua
local m = require('haylen.math')

local ship = m.transform({500, 300}, m.pi / 2)
local forward = ship:applyVector({1, 0})
print(m.approximately(forward.y, 1)) -- true
```

### transform:determinant()

Returns `a * d - b * c`, the factor by which the transform scales areas. It is negative when the transform mirrors, and `0` when it squashes everything onto a line or a point.

```lua
local m = require('haylen.math')

print(m.transform({0, 0}, 0.3, {2, -3}):determinant() < 0) -- true
```

### transform:translation()

Returns `tx` and `ty` as a `Vec2`, the point where the transform moves the origin.

```lua
local m = require('haylen.math')

print(m.transform({300, 200}, m.pi / 3):translation()) -- Vec2(300.0, 200.0)
```

### transform:inverse()

Returns the inverse transform, which maps transformed points back. A transform with a determinant of `0`, such as one with a zero scale, returns a transform with every coefficient `0`.

```lua
local m = require('haylen.math')

local body = m.transform({200, 100}, m.pi / 6, {2, 2})
local world = body:apply({10, 5})
local localPoint = body:inverse():apply(world)
print(m.approximately(localPoint.x, 10), m.approximately(localPoint.y, 5)) -- true true
```

### Transform2D operators

The operator `a * b` composes two transforms. The result applies `b` first and then `a`, so `(a * b):apply(p)` equals `a:apply(b:apply(p))`. A child placed inside a parent is `parent * child`.

```lua
local m = require('haylen.math')

local parent = m.transform({300, 200}, m.pi / 2)
local child = m.transform({50, 0})
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
- Fewer than two points raise `A points curve needs at least two points.`, and points out of order raise `The points of a curve must be ordered by x.`
- A function that returns anything but a number raises `An easing function must return a number.`

## EasingCurve

An `EasingCurve` is an easing curve that C++ code made from a function, which cannot become a name or a table. A curve in any other form reaches Lua as a name or a table. Such a value comes from projects that extend the engine in C++, when a binding pushes a curve of `math::EasingCurve::custom` with the binding toolkit that the [Lua guide](../core/lua.md#extending-the-engine-from-c) describes:

```cpp
int CurvesLua::half(lua_State* L) {
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

A `Noise2D` is seeded gradient noise, made with `m.noise`. Noise gives values that change smoothly from one point to the next, without the jumps of random numbers, which suits terrain, clouds, wind and wobble. The methods `noise:perlin`, `noise:simplex` and `noise:fractal` return values from `-1` to `1`. The method `noise:worley` returns distances and a region value, and `noise:warp` returns a point. Features of the noise lie about one unit apart, so scale world coordinates down before sampling, such as `x / 200`.

### m.noise(seed)

Returns a new `Noise2D`. The whole number `seed`, `0` by default, picks the noise field, and each seed gives a different one.

```lua
local m = require('haylen.math')

local terrain = m.noise(7)
print(terrain:perlin(3.5, 8.25))
```

### noise:perlin(x, y)

Returns classic Perlin noise at `x`, `y`.

```lua
local m = require('haylen.math')

local wind = m.noise(3):perlin(12 / 50, 0.5)
print(wind)
```

### noise:simplex(x, y)

Returns simplex noise at `x`, `y`, which has fewer patterns along the axes than Perlin noise.

```lua
local m = require('haylen.math')

local moisture = m.noise(21):simplex(640 / 200, 320 / 200)
print(moisture)
```

### noise:fractal(x, y, octaves, lacunarity, gain)

Returns several layers of simplex noise added together, kept from `-1` to `1`. Each layer adds finer detail, like the small bumps on a large hill. The number of layers is `octaves`, `4` by default. Each layer has `lacunarity` times the frequency of the one before, `2` by default, and `gain` times its strength, `0.5` by default.

```lua
local m = require('haylen.math')

local noise = m.noise(99)
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

local nearest, second, cell = m.noise(5):worley(3.2, 7.5)
local crack = second - nearest < 0.05
print(nearest, crack, cell)
```

### noise:warp(x, y, amplitude, frequency, octaves)

Moves the point by fractal noise and returns the new `x` and `y`. The noise is sampled at `frequency` times the point, `1` by default, with `octaves` layers, `3` by default, and it moves the point by up to `amplitude` on each axis. Sampling any noise at the moved point bends its patterns into swirls, which suits coastlines, marble and smoke.

```lua
local m = require('haylen.math')

local noise = m.noise(8)
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

local heights = m.noise(42):sampleGrid({columns = 256, rows = 256, step = 1 / 32, octaves = 5})
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
    local moisture = m.noise(7):sampleGridAsync({columns = 1024, rows = 1024, kind = 'simplex', step = 0.01}):await()
    print(#moisture)
end)
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

### m.poissonDisk(options)

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
- A `distance` function without a `maximumDistance` of at least `minimumDistance` raises `A varying Poisson distance needs a maximum distance of at least the minimum distance.`
- An attempt count outside `1` to `1000` raises `Poisson sampling needs between 1 and 1000 attempts.`
- A minimum distance so small that the sampling grid of the area would need more than 16777216 cells raises `The minimum distance is too small for the area, whose sampling grid would need more than 16777216 cells.`
- A `distance` function that returns something other than a number raises `A Poisson distance function must return a number, not <type>.`, and one that returns a distance that is infinite or not a number raises `A Poisson distance function must return finite distances.`
- An error raised inside `accept` or `distance` leaves the call.

```lua
local m = require('haylen.math')

local lake = m.rect(600, 400, 300, 200)
local trees = m.poissonDisk({
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

local woods = m.poissonDisk({
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

local noise = m.noise(4)
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

## Spline

A `Spline` is a smooth curve through or near control points. It is measured by its length, so it can be walked at a steady speed or sampled at even distances. Its parameter runs from `0` at the start to `1` at the end of the whole curve. The [strokes of `haylen.graphics2d`](graphics2d.md#graphics2ddrawstrokepoints-options) draw splines.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `kind` | string | read | `'catmullRom'`, `'bezier'` or `'bSpline'`. |
| `closed` | boolean | read | Whether the curve joins its end back to its start. |
| `points` | list of Vec2 | read | The control points. |
| `segmentCount` | integer | read | The number of pieces of the curve. |
| `length` | number | read | The length of the curve in world units. |

```lua
local m = require('haylen.math')

local loop = m.spline({{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {closed = true})
print(loop.kind, loop.closed, #loop.points, loop.segmentCount, loop.length > 400) -- catmullRom true 4 4 true
```

### m.spline(points, options)

Returns a new `Spline` through the list `points`. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | `'catmullRom'` | The kind `'catmullRom'` passes through every point, with centripetal spacing, which avoids loops and sharp tips. The kind `'bezier'` chains cubic pieces whose points go end, control, control, end and so on. The kind `'bSpline'` stays near the points without passing through them, except for the ends of an open curve. |
| `closed` | boolean | `false` | Joins the last point back to the first. A closed curve wraps its parameters and distances around. |

Each kind needs enough points, or it raises one of these errors: `A Catmull-Rom spline needs at least two points when open and three when closed.`, `A Bezier spline needs 3n + 1 points when open and 3n points when closed.` and `A B-spline needs at least two points when open and three when closed.`

```lua
local m = require('haylen.math')

local track = m.spline({{0, 0}, {200, -80}, {400, 0}, {600, -80}}, {kind = 'catmullRom'})
print(track.length)
```

### spline:point(t)

Returns the point of the curve at the parameter `t` as a `Vec2`.

```lua
local m = require('haylen.math')

local arc = m.spline({{0, 0}, {50, -50}, {100, 0}})
print(arc:point(0.5))
```

### spline:tangent(t)

Returns the direction of travel at the parameter `t`, as a `Vec2` of length 1.

```lua
local m = require('haylen.math')

local arc = m.spline({{0, 0}, {50, -50}, {100, 0}})
print(arc:tangent(0))
```

### spline:pointAtDistance(distance)

Returns the point at `distance` world units along the curve. Use it to move things at a steady speed, whatever the spacing of the control points.

```lua
local m = require('haylen.math')
local scene = require('haylen.scene')

local rail = m.spline({{0, 0}, {300, 0}, {300, 300}})
local traveled = 0

scene.push({
    update = function(self, dt)
        traveled = math.min(traveled + 120 * dt, rail.length)
        self.position = rail:pointAtDistance(traveled)
    end,
})
```

### spline:tangentAtDistance(distance)

Returns the direction of travel at `distance` world units along the curve, as a `Vec2` of length 1, such as to turn a cart along its rail.

```lua
local m = require('haylen.math')

local rail = m.spline({{0, 0}, {300, 0}, {300, 300}})
print(m.degrees(rail:tangentAtDistance(rail.length):angle()))
```

### spline:parameterAtDistance(distance)

Returns the parameter at `distance` world units along the curve.

```lua
local m = require('haylen.math')

local rail = m.spline({{0, 0}, {300, 0}, {300, 300}})
print(rail:parameterAtDistance(rail.length / 2))
```

### spline:sample(count)

Returns `count` points spread evenly over the parameter.

```lua
local m = require('haylen.math')

local fence = m.spline({{0, 0}, {100, 40}, {200, 0}, {300, 40}}, {kind = 'bezier'})
print(#fence:sample(10)) -- 10
```

### spline:sampleByDistance(spacing)

Returns points `spacing` world units apart along the curve, plus the end point of an open curve. A spacing that is not finite and above 0, or so small that it would take more than 16777216 steps along the curve, raises `Sampling by distance needs a finite spacing above 0 that takes at most 16777216 steps along the curve.`

```lua
local m = require('haylen.math')

local fence = m.spline({{0, 0}, {100, 40}, {200, 0}, {300, 40}}, {kind = 'bezier'})
for _, post in ipairs(fence:sampleByDistance(25)) do
    print(post)
end
```

### spline:flatten(tolerance)

Returns points along the curve, from its start to its end, whose straight lines stay within `tolerance` world units of it. The points sit closer together where the curve bends and far apart where it runs straight. A closed curve does not repeat its start at the end. Every piece of the curve splits into 4 to 1024 parts, so a tiny tolerance stays bounded. A [stroke along a spline](graphics2d.md#graphics2ddrawstrokepoints-options) flattens it within a quarter pixel on its own, and the points also serve as the outline of a polygon or a physics chain. A tolerance that is not finite and above 0 raises `Flattening a spline needs a finite tolerance above 0.`

```lua
local m = require('haylen.math')

local hill = m.spline({{0, 300}, {200, 100}, {400, 280}, {600, 120}})
local outline = hill:flatten(1)
print(#outline, outline[1], outline[#outline])
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

Filters the values and returns them. The seconds since the last update come first, so the smoothing is the same at any frame rate. The first update returns its values as they are.

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
| `Expected a number in field "<field>".` | A table given as a `Vec2`, a `Rect`, a `Color` or a circle misses a number. |
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
| `Poisson distances must be finite.` | A distance of `m.poissonDisk` is infinite or not a number. |
| `A varying Poisson distance needs a maximum distance of at least the minimum distance.` | A `distance` function comes without a valid `maximumDistance`. |
| `Poisson sampling needs between 1 and 1000 attempts.` | The `attempts` of `m.poissonDisk` are out of range. |
| `The minimum distance is too small for the area, whose sampling grid would need more than 16777216 cells.` | The area of `m.poissonDisk` is too large for its distance. |
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
| `Sampling by distance needs a finite spacing above 0 that takes at most 16777216 steps along the curve.` | The spacing of `spline:sampleByDistance` is out of range. |
| `Flattening a spline needs a finite tolerance above 0.` | The tolerance of `spline:flatten` is not finite and above 0. |
| `A filter has from 1 to 4 dimensions, not <count>.` | The `dimensions` of a filter are out of range. |
| `The filter takes <dimensions> values, not <count>.` | An update of a filter got another count of values. |
| `The cutoff of a low-pass filter is a positive number of hertz.` | The `cutoff` of a low-pass filter is not positive. |
| `The window of a filter holds from 1 to <maximum> updates, not <window>.` | The `window` of a filter is out of range. |
| `The threshold of a Hampel filter is a positive number of spreads.` | The `threshold` of a Hampel filter is not positive. |
| `The process noise and the measurement noise of a Kalman filter are positive numbers.` | A noise of a Kalman filter is not positive. |
| `bad argument #2 to 'shuffleBag' (expected one count per item)` | The `counts` of a shuffle bag have another length. |
| `A shuffle bag needs at least one item.` | A shuffle bag would hold no item. |
| `bad argument #2 to 'weightedChoice' (expected one weight per item)` | The weights of a weighted choice have another length. |
| `A weighted choice needs at least one positive weight.` | The weights of a weighted choice are all zero. |
| `bad argument #1 to 'probability' (expected an item number from 1)` | The position of `choice:probability` is below 1. |
