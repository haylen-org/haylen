# haylen.math

`haylen.math` provides the math types that the rest of the engine uses, `Vec2`, `Rect`, `Color` and `Transform`, together with a seeded random generator, gradient and cellular noise, easing curves, scalar helpers, circle and segment tests, ray casts against shapes, polygon utilities, polygon booleans and offsets, marching squares, splines, critically damped springs, shuffle bags, weighted choices and Poisson disk sampling. Use it for gameplay math, procedural generation and anything that passes positions, areas or colors to other modules. Lua's standard `math` library stays available for everything else.

```lua
local m = require('haylen.math')
```

The examples name the module `m` so that the standard `math` library keeps its name.

## Values

### Accepted forms

Functions of every engine module accept plain tables wherever they expect one of these values, so creating the userdata is optional.

| Type | Accepted forms |
| --- | --- |
| `Vec2` | A `Vec2`, `{x = 10, y = 20}` or `{10, 20}`. |
| `Rect` | A `Rect`, `{x = 0, y = 0, width = 100, height = 50}` or `{0, 0, 100, 50}`. |
| `Color` | A `Color`, a `'#AARRGGBB'` or `'#RRGGBB'` string where the `#` is optional, `{r = 1, g = 0.5, b = 0, a = 1}` or `{1, 0.5, 0}`. Components go from `0` to `1`, and alpha defaults to `1` in tables. |

The constructors `m.vec2`, `m.rect` and `m.color` take the same forms as a single argument, which also copies an existing value. A table with a missing or non-numeric component raises `Expected a number in field 'y'.`, naming the field. A value of another type raises an error such as `bad argument #2 to 'drawRect' (Color, color text or table with r, g and b expected, got number)`. Invalid color text raises `invalid color text, expected #RRGGBB or #AARRGGBB`.

```lua
local m = require('haylen.math')

local a = m.vec2(1, 2) + {x = 3, y = 4}
local b = m.vec2(1, 2) + {3, 4}
print(a == b) -- true

local red = m.color('#FFFF0000')
print(red:lerp({0, 0, 1}, 0.5), red == m.color('FF0000')) -- #FF800080 true
```

### Userdata behavior

`Vec2`, `Rect`, `Color`, `Transform`, `Random` and `Noise` values are userdata. Assigning one to another variable shares the same object, so changing a field through one variable changes it for both. Operators and methods always return new values, and engine functions copy the values they receive. Reading a member that does not exist raises `haylen.Vec2 has no member 'z'.`, and assigning one raises `haylen.Vec2 has no writable property 'z'.`, with the type name of the value.

```lua
local m = require('haylen.math')

local spawn = m.vec2(100, 100)
local shared = spawn
shared.x = 150
print(spawn.x) -- 150.0

local copy = m.vec2(spawn:unpack())
copy.x = 0
print(spawn.x) -- 150.0
```

## Constants

### m.pi

The number π as a float.

```lua
local m = require('haylen.math')

local halfTurn = m.pi
print(m.degrees(halfTurn)) -- 180.0
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

## Constructors

### m.vec2(x, y)

Returns a new `Vec2`. Both components default to `0`.

```lua
local m = require('haylen.math')

local origin = m.vec2()
local velocity = m.vec2(120, -80)
print(origin, velocity) -- Vec2(0.0, 0.0) Vec2(120.0, -80.0)
```

### m.vec2(value)

Returns a new `Vec2` from `value`, which takes any form a `Vec2` argument accepts: a `Vec2`, `{x = 10, y = 20}` or `{10, 20}`. Passing a `Vec2` makes an independent copy.

```lua
local m = require('haylen.math')

local spawn = m.vec2({x = 64, y = 128})
local respawn = m.vec2(spawn)
respawn.y = 0
print(spawn, respawn) -- Vec2(64.0, 128.0) Vec2(64.0, 0.0)
```

### m.fromAngle(radians, length)

Returns a new `Vec2` pointing at `radians` from the positive x axis toward the positive y axis, with the given `length`, which defaults to `1`.

```lua
local m = require('haylen.math')

local aim = m.radians(30)
local velocity = m.fromAngle(aim, 400)
print(velocity:length()) -- close to 400
```

### m.rect(x, y, width, height)

Returns a new `Rect` whose top left corner is at `x`, `y`. Every argument defaults to `0`. The y axis points down, so `top` is `y` and `bottom` is `y + height`.

```lua
local m = require('haylen.math')

local door = m.rect(400, 300, 64, 96)
print(door:bottom()) -- 396.0
```

### m.rect(value)

Returns a new `Rect` from `value`, which takes any form a `Rect` argument accepts: a `Rect`, `{x = 0, y = 0, width = 100, height = 50}` or `{0, 0, 100, 50}`. Passing a `Rect` makes an independent copy.

```lua
local m = require('haylen.math')

local level = {bounds = {0, 0, 3200, 1800}}
local camera = m.rect(level.bounds)
print(camera:right()) -- 3200.0
```

### m.fromMinMax(minimum, maximum)

Returns a new `Rect` that spans from the top left corner `minimum` to the bottom right corner `maximum`, both `Vec2`.

```lua
local m = require('haylen.math')

local dragStart, dragEnd = m.vec2(120, 80), m.vec2(360, 240)
local selection = m.fromMinMax(dragStart:min(dragEnd), dragStart:max(dragEnd))
print(selection) -- Rect(120.0, 80.0, 240.0, 160.0)
```

### m.fromCenter(center, size)

Returns a new `Rect` of the given `size` centered on `center`, both `Vec2`.

```lua
local m = require('haylen.math')

local player = {x = 500, y = 300}
local hitbox = m.fromCenter(player, {32, 48})
print(hitbox) -- Rect(484.0, 276.0, 32.0, 48.0)
```

### m.color(r, g, b, a)

Returns a new `Color` from components between `0` and `1`. Every argument defaults to `1`, so `m.color()` is opaque white. When the first argument is not a number, the call converts it instead, as described in the next section.

```lua
local m = require('haylen.math')

local white = m.color()
local translucentBlue = m.color(0.2, 0.4, 1, 0.5)
print(white, translucentBlue) -- #FFFFFFFF #803366FF
```

### m.color(value)

Returns a new `Color` from `value`, which takes any form a `Color` argument accepts. Text is parsed from `'#AARRGGBB'` or `'#RRGGBB'`, where the `#` is optional and six digits mean opaque. A table holds `r`, `g`, `b` and an optional `a`, by name or in that order. Passing a `Color` makes an independent copy. Invalid text raises `bad argument #1 to 'color' (invalid color text, expected #RRGGBB or #AARRGGBB)`.

```lua
local m = require('haylen.math')

local sand = m.color('#FFE8D29B')
local shadow = m.color({0, 0, 0, 0.5})
local tint = m.color({r = 1, g = 0.8, b = 0.8})
print(sand, shadow.a, tint) -- #FFE8D29B 0.5 #FFFFCCCC
```

### m.white()

Returns a new opaque white `Color`.

```lua
local m = require('haylen.math')

local tint = m.white()
tint.a = 0.5
print(tint) -- #80FFFFFF
```

### m.black()

Returns a new opaque black `Color`.

```lua
local m = require('haylen.math')

local fade = m.black():withAlpha(0.75)
print(fade) -- #BF000000
```

### m.transparent()

Returns a new `Color` with every component `0`, which is fully transparent black.

```lua
local m = require('haylen.math')

local fadeIn = m.transparent():lerp(m.white(), 0.5)
print(fadeIn) -- #80808080
```

### m.fromRgba8(red, green, blue, alpha)

Returns a new `Color` from 8-bit channels, each an integer from `0` to `255`. `alpha` defaults to `255`. A value outside that range raises `bad argument #1 to 'fromRgba8' (expected an integer from 0 to 255)`, naming the argument.

```lua
local m = require('haylen.math')

local pixel = {37, 99, 235}
local brand = m.fromRgba8(pixel[1], pixel[2], pixel[3])
print(brand) -- #FF2563EB
```

### m.fromHex(rrggbbaa)

Returns a new `Color` from an integer that packs red, green, blue and alpha in that order, one byte each, such as `0xFF8000FF`. The packing differs from color text, which puts alpha first. A value outside `0` to `0xFFFFFFFF` raises `bad argument #1 to 'fromHex' (expected an integer from 0 to 0xFFFFFFFF)`.

```lua
local m = require('haylen.math')

local orange = m.fromHex(0xFF8000FF)
local glass = m.fromHex(0x3399FF40)
print(orange, glass) -- #FFFF8000 #403399FF
```

### m.hsv(hue, saturation, value, alpha)

Returns a new `Color` from hue, saturation and value. `hue` is measured in turns, so `0` is red, `1/3` is green and `2/3` is blue, and values outside `0` to `1` wrap around. `saturation`, `value` and `alpha` go from `0` to `1`, and `alpha` defaults to `1`.

```lua
local m = require('haylen.math')
local haylen = require('haylen')

local function rainbow()
    return m.hsv(haylen.time() * 0.2, 0.8, 1)
end

print(m.hsv(1 / 3, 1, 1)) -- #FF00FF00
print(rainbow())
```

### m.transform(position, rotation, scale, skew)

Returns a new `Transform` that scales by `scale`, then skews by `skew`, then rotates by `rotation` radians, then moves by `position`. `position` is a `Vec2` and is required. `rotation` defaults to `0`, and `scale` is a `Vec2` that defaults to `{1, 1}`. `skew` is a `Vec2` of angles in radians that defaults to `{0, 0}`. `skew.x` turns the local y axis and `skew.y` turns the local x axis by that angle, in the same direction as `rotation`, which shears the shape.

```lua
local m = require('haylen.math')

local turret = m.transform({400, 300}, m.pi / 2, {2, 2})
local muzzle = turret:apply({16, 0})
print(muzzle) -- close to Vec2(400.0, 332.0)

-- Leaning the y axis by 15 degrees shears a banner sideways.
local banner = m.transform({0, 0}, 0, {1, 1}, {m.radians(15), 0})
print(banner:apply({0, 100})) -- close to Vec2(-25.9, 96.6)
```

### m.identity()

Returns a new `Transform` that leaves every point where it is.

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

Returns a new `Transform` that moves points by the `Vec2` `offset`.

```lua
local m = require('haylen.math')

local shake = m.translation({4, -2})
print(shake:apply({100, 100})) -- Vec2(104.0, 98.0)
```

### m.rotation(radians)

Returns a new `Transform` that rotates points around the origin by `radians`, clockwise on screen.

```lua
local m = require('haylen.math')

local quarterTurn = m.rotation(m.halfPi)
print(quarterTurn:apply({10, 0})) -- close to Vec2(0.0, 10.0)
```

### m.scaling(scale)

Returns a new `Transform` that scales points from the origin by the `Vec2` `scale`.

```lua
local m = require('haylen.math')

local mirror = m.scaling({-1, 1})
print(mirror:apply({30, 40})) -- Vec2(-30.0, 40.0)
```

### m.random(seed)

Returns a new `Random` generator. The same seed produces the same sequence on every platform. `seed` is an integer, and without one the generator uses a fixed default seed, so it repeats the same sequence every run. Pass something like `os.time()` for different results each run.

```lua
local m = require('haylen.math')

local levelRandom = m.random(2024)
local lootRandom = m.random(os.time())
print(levelRandom:integer(1, 6), lootRandom:float())
```

### m.noise(seed)

Returns a new `Noise` generator. `seed` is an integer that defaults to `0`, and each seed gives a different noise field.

```lua
local m = require('haylen.math')

local terrain = m.noise(7)
print(terrain:perlin(3.5, 8.25))
```

## Scalar functions

### m.ease(name, t)

Evaluates an easing curve at `t` and returns the eased value. `t` is clamped to `0` to `1`. The result goes from `0` to `1` and can overshoot for the `back` and `elastic` curves. The names are `'linear'` and the `in`, `out` and `in_out` variants of `sine`, `quad`, `cubic`, `quart`, `quint`, `expo`, `circ`, `back`, `elastic` and `bounce`, such as `'quad_out'`, `'back_in'` or `'elastic_in_out'`. An unknown name raises `bad argument #1 to 'ease' (unknown value 'bouncy')`.

```lua
local m = require('haylen.math')

local startY, endY, duration = 1200, 540, 0.8
local elapsed = 0.4
local y = m.lerp(startY, endY, m.ease('back_out', elapsed / duration))
print(y)
```

### m.clamp(value, minimum, maximum)

Returns `value` limited to the range from `minimum` to `maximum`. `minimum` must not be greater than `maximum`.

```lua
local m = require('haylen.math')

local health = m.clamp(130, 0, 100)
print(health) -- 100.0
```

### m.lerp(from, to, t)

Returns the linear interpolation `from + (to - from) * t`. `t` is not clamped, so values outside `0` to `1` extrapolate.

```lua
local m = require('haylen.math')

print(m.lerp(0, 200, 0.25)) -- 50.0
```

### m.inverseLerp(from, to, value)

Returns where `value` lies between `from` and `to`, as `0` at `from` and `1` at `to`. The result is not clamped, and it is `0` when `from` equals `to`.

```lua
local m = require('haylen.math')

local progress = m.inverseLerp(100, 300, 250)
print(progress) -- 0.75
```

### m.remap(value, fromMin, fromMax, toMin, toMax)

Maps `value` from the range `fromMin` to `fromMax` onto the range `toMin` to `toMax`, without clamping.

```lua
local m = require('haylen.math')

local speed = 7
local pitch = m.remap(speed, 0, 10, 0.8, 1.4)
print(pitch) -- about 1.22
```

### m.smoothstep(edge0, edge1, value)

Returns `0` below `edge0`, `1` above `edge1` and a smooth S curve in between.

```lua
local m = require('haylen.math')

local distance = 180
local fade = 1 - m.smoothstep(100, 300, distance)
print(fade)
```

### m.moveToward(current, target, maxDelta)

Moves `current` toward `target` by at most `maxDelta` and never overshoots.

```lua
local m = require('haylen.math')

local ship = {speed = 0, maxSpeed = 400, acceleration = 900}

require('haylen.scene').push({
    update = function(self, dt)
        ship.speed = m.moveToward(ship.speed, ship.maxSpeed, ship.acceleration * dt)
    end,
})
```

### m.wrapAngle(radians)

Returns the same angle wrapped into the range from -π to π, excluding π itself.

```lua
local m = require('haylen.math')

local heading, desired = m.radians(170), m.radians(-170)
local turn = m.wrapAngle(desired - heading)
print(m.degrees(turn)) -- close to 20
```

### m.damp(rate, dt)

Returns the interpolation factor `1 - exp(-rate * dt)` for smoothing that behaves the same at any frame rate. A higher `rate` follows the target faster.

```lua
local m = require('haylen.math')

local camera = {x = 0}
local player = {x = 500}

require('haylen.scene').push({
    update = function(self, dt)
        camera.x = m.lerp(camera.x, player.x, m.damp(8, dt))
    end,
})
```

### m.radians(degrees)

Converts degrees to radians.

```lua
local m = require('haylen.math')

local tilt = m.radians(15)
print(tilt)
```

### m.degrees(radians)

Converts radians to degrees.

```lua
local m = require('haylen.math')

local facing = m.vec2(0, 1):angle()
print(m.degrees(facing)) -- 90.0
```

### m.sign(value)

Returns `1` for a positive `value`, `-1` for a negative one and `0` for zero.

```lua
local m = require('haylen.math')

local input = -0.6
local facing = m.sign(input)
print(facing) -- -1.0
```

### m.saturate(value)

Returns `value` limited to the range from `0` to `1`.

```lua
local m = require('haylen.math')

local charge = 1.3
print(m.saturate(charge), m.saturate(-0.2)) -- 1.0 0.0
```

### m.approximately(a, b, epsilon)

Returns `true` when `a` and `b` differ by at most `epsilon` times the larger of `1`, `|a|` and `|b|`. `epsilon` defaults to `0.00001`.

```lua
local m = require('haylen.math')

print(0.1 + 0.2 == 0.3, m.approximately(0.1 + 0.2, 0.3)) -- false true
print(m.approximately(1, 1.05, 0.1)) -- true
```

## Circles and segments

Circle arguments are tables with a `center` point and a `radius`, as `{center = {100, 50}, radius = 20}` or `{{100, 50}, 20}`. Segment arguments are tables with a `start` and an `end` point, as `{start = a, ['end'] = b}` or `{a, b}`. A table without a readable point raises `Expected a point in field 'center'.`, and one without a readable number raises `Expected a number in field 'radius'.`, naming the field.

### m.intersects(circle, other)

Returns `true` when `circle` overlaps `other`, which is another circle or a rectangle. Shapes that only touch count as overlapping. `other` is a circle when it has a `radius` field or holds exactly two values in order, and a rectangle in any `Rect` form otherwise.

```lua
local m = require('haylen.math')

local blast = {center = {400, 300}, radius = 120}
local crate = m.rect(480, 280, 64, 64)
local slime = {{560, 300}, 30}
print(m.intersects(blast, crate), m.intersects(blast, slime)) -- true false
```

### m.intersection(a, b)

Returns the point where segments `a` and `b` cross as a `Vec2`, or `nil` when they do not cross. Parallel segments return `nil`.

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
local cart = m.closestPoint(rail, {250, 320})
print(cart) -- Vec2(250.0, 400.0)
```

### m.distanceToSegment(segment, point)

Returns the distance from `point` to the nearest point of `segment`.

```lua
local m = require('haylen.math')

local river = {{0, 500}, {1920, 500}}
local camp = {x = 600, y = 380}
if m.distanceToSegment(river, camp) < 150 then
    print('the camp hears the river')
end
```

## Polygons and sampling

Polygon functions take a sequence of points, each a `Vec2` or a point table, with the vertices in order around the outline. A point that cannot be read raises an error.

### m.polygonContains(polygon, point)

Returns `true` when `point` is inside `polygon`, using the even-odd rule.

```lua
local m = require('haylen.math')

local pond = {{100, 100}, {300, 80}, {340, 260}, {120, 300}}
print(m.polygonContains(pond, {200, 200})) -- true
print(m.polygonContains(pond, {400, 200})) -- false
```

### m.polygonArea(polygon)

Returns the signed area of `polygon`. It is positive when the vertices run clockwise on screen, where the y axis points down, and negative for the other direction.

```lua
local m = require('haylen.math')

local field = {{0, 0}, {10, 0}, {10, 10}, {0, 10}}
print(m.polygonArea(field)) -- 100.0
print(math.abs(m.polygonArea({{0, 0}, {0, 10}, {10, 10}, {10, 0}}))) -- 100.0
```

### m.polygonCentroid(polygon)

Returns the center of mass of `polygon` as a `Vec2`. For a polygon without area it returns the average of the vertices.

```lua
local m = require('haylen.math')

local island = {{0, 0}, {400, 0}, {400, 200}, {0, 200}}
local center = m.polygonCentroid(island)
print(center) -- Vec2(200.0, 100.0)
```

### m.polygonConvex(polygon)

Returns `true` when `polygon` is convex. Polygons with fewer than three points are not convex.

```lua
local m = require('haylen.math')

local square = {{0, 0}, {10, 0}, {10, 10}, {0, 10}}
local arrow = {{0, 0}, {10, 0}, {5, 3}, {10, 10}, {0, 10}}
print(m.polygonConvex(square), m.polygonConvex(arrow)) -- true false
```

### m.convexHull(points)

Returns the smallest convex polygon that encloses `points`, as a sequence of `Vec2` in order around the outline. Duplicate points are ignored, and with fewer than three distinct points the distinct points are returned.

```lua
local m = require('haylen.math')

local rocks = {{0, 0}, {10, 0}, {5, 5}, {10, 10}, {0, 10}}
local outline = m.convexHull(rocks)
print(#outline) -- 4
```

### m.triangulate(polygon)

Splits a simple polygon into triangles and returns a flat sequence of 1-based vertex indices, three per triangle. The polygon may be concave and may use either winding, and every triangle comes out with the winding that `m.polygonArea` reports as positive. Fewer than three points give an empty table.

```lua
local m = require('haylen.math')
local graphics2d = require('haylen.graphics2d')

local shape = {{0, 0}, {100, 0}, {50, 30}, {100, 100}, {0, 100}}
local indices = m.triangulate(shape)

require('haylen.scene').push({
    render = function(self)
        graphics2d.beginScreen()
        for index = 1, #indices, 3 do
            local a, b, c = shape[indices[index]], shape[indices[index + 1]], shape[indices[index + 2]]
            graphics2d.drawPolygon({a, b, c}, '#FF4CAF50')
        end
    end,
})
```

### m.bounds(points)

Returns the smallest `Rect` that contains every point of the sequence `points`. An empty sequence returns `Rect(0, 0, 0, 0)`.

```lua
local m = require('haylen.math')

local squad = {{120, 300}, {180, 260}, {90, 340}}
print(m.bounds(squad)) -- Rect(90.0, 260.0, 90.0, 80.0)
```

### m.poissonDisk(options)

Returns evenly spread random points inside an area, where no two points are closer than a minimum distance, as a sequence of `Vec2`. Use it to scatter trees, rocks or spawn points without clumps. Unknown keys raise `Unknown option '<key>'.`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `area` | Rect | Empty | Area to fill. An empty area returns no points. |
| `minimumDistance` | number | `64` | Smallest distance between two points. A value that is not positive returns no points, and a value so small for the area that sampling would need more than 16777216 grid cells raises an error. |
| `maximumDistance` | number | `0` | Largest distance a `distance` function may ask for. It must be at least `minimumDistance` when `distance` is set. |
| `distance` | function | `nil` | Called with each candidate `Vec2` and returns the spacing wanted there, such as from a density or noise map. Results are clamped between `minimumDistance` and `maximumDistance`, and two points stay at least the larger of their own spacings apart. |
| `attempts` | integer | `30` | Candidates tried around each point before it stops growing. Higher values fill the area more tightly. |
| `random` | Random | `nil` | Generator to draw from, which advances as points are sampled. |
| `seed` | integer | `0` | Seed of a private generator, used when `random` is absent. |
| `accept` | function | `nil` | Called with each candidate `Vec2`, and only candidates for which it returns a true value are kept. |

The same options and seed always produce the same points. An error raised inside `accept` or `distance` reaches the caller.

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
print(#trees .. ' trees planted')
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

Ray casts without a physics world trace a straight line from one point to another and report where it first meets a shape. Points accept a `Vec2` or a table `{x, y}`, and a cast returns a hit table, or `nil` when the ray meets nothing.

| Field | Type | Meaning |
| --- | --- | --- |
| `x`, `y` | number | The hit point. |
| `normalX`, `normalY` | number | The unit normal of the surface at the hit, facing the side the ray came from. It is zero when the ray starts inside a solid shape. |
| `distance` | number | The distance from the start of the ray to the hit. |
| `fraction` | number | The distance over the length of the ray, from 0 at the start to 1 at the end. |
| `index` | integer | For polygons, chains and segment lists, the 1-based edge or segment that was hit. |

Circles, rectangles and polygons are solid, so a ray that starts inside one hits it at distance 0 with a zero normal. Segments and chains are hit from both sides. The ray casts of [haylen.spatial2d](spatial2d.md), [haylen.navigation2d](navigation2d.md) and [haylen.tiled](tiled.md) return the same tables with fields of their own.

### m.raycastSegment(from, to, segment)

Casts a ray against a segment, given as `{start, end}` or `{{x1, y1}, {x2, y2}}`.

```lua
local m = require('haylen.math')

local hit = m.raycastSegment({0, 0}, {100, 0}, {{50, -10}, {50, 10}})
print(hit.x, hit.normalX, hit.distance, hit.fraction)
```

### m.raycastRect(from, to, rect)

Casts a ray against a solid rectangle.

```lua
local m = require('haylen.math')

local door = m.rect(200, 100, 20, 80)
local hit = m.raycastRect({0, 140}, {400, 140}, door)
if hit then
    print('the bolt stops at ' .. hit.x)
end
```

### m.raycastCircle(from, to, circle)

Casts a ray against a solid circle, given as `{center = point, radius = r}` or `{point, r}`.

```lua
local m = require('haylen.math')

local shield = {center = {300, 0}, radius = 40}
local hit = m.raycastCircle({0, 0}, {600, 0}, shield)
print(hit.x, hit.normalX)
```

### m.raycastPolygon(from, to, polygon)

Casts a ray against a solid simple polygon in either winding. The hit index is the edge that starts at that point of the polygon.

```lua
local m = require('haylen.math')

local rock = {{100, -20}, {140, 0}, {100, 20}, {80, 0}}
local hit = m.raycastPolygon({0, 0}, {300, 0}, rock)
print(hit.x, hit.index)
```

### m.raycastChain(from, to, points, loop)

Casts a ray against the segments that join consecutive points. With `loop` set to true the last point also joins the first. The hit index is the first point of the segment that was hit.

```lua
local m = require('haylen.math')

local ridge = {{0, 100}, {100, 60}, {200, 90}, {300, 40}}
local hit = m.raycastChain({150, -100}, {150, 300}, ridge)
print(hit.y, hit.index)
```

### m.raycastSegments(from, to, segments)

Returns the closest hit among a list of segments, such as the walls of a level. The hit index is the position of the segment in the list.

```lua
local m = require('haylen.math')

local walls = {{{60, 0}, {60, 100}}, {{30, 0}, {30, 100}}, {{90, 0}, {90, 100}}}
local hit = m.raycastSegments({0, 50}, {200, 50}, walls)
print(hit.index, hit.x)
```

### m.raycastSegmentsAll(from, to, segments, limit)

Returns every segment the ray crosses, as hits sorted by distance. The optional `limit` keeps the first hits only, which makes a shot that pierces a number of targets.

```lua
local m = require('haylen.math')

local targets = {{{60, 0}, {60, 100}}, {{30, 0}, {30, 100}}, {{90, 0}, {90, 100}}}
for _, hit in ipairs(m.raycastSegmentsAll({0, 50}, {200, 50}, targets, 2)) do
    print('pierced target ' .. hit.index .. ' at ' .. hit.x)
end
```

### m.bounceRay(origin, direction, length, bounces, segments)

Follows a ray of the given length that bounces off the segments, like a laser between mirrors, for at most `bounces` reflections. Returns the list of bounce hits and the `x`, `y` where the path ends. The distance and fraction of each hit measure the whole path up to that bounce. A zero direction or a negative bounce count raises an error.

```lua
local m = require('haylen.math')
local graphics2d = require('haylen.graphics2d')
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

Casts `count` rays of the given length, spread evenly across `spread` radians centered on `angle`, like a cone of vision, and returns one entry per ray in order of increasing angle: the hit of that ray, or `false` when it hit nothing.

```lua
local m = require('haylen.math')

local walls = {{{200, -100}, {200, 100}}}
local fan = m.rayFan({0, 0}, 0, m.pi / 2, 9, 400, walls)
for index, hit in ipairs(fan) do
    print(index, hit and hit.distance or 'clear')
end
```

## Vec2

A 2D vector or point with `x` and `y` fields.

### vec.x

Readable and writable horizontal component.

```lua
local m = require('haylen.math')

local position = m.vec2(10, 20)
position.x = position.x + 5
print(position.x) -- 15.0
```

### vec.y

Readable and writable vertical component. The y axis points down in design space.

```lua
local m = require('haylen.math')

local position = m.vec2(10, 20)
position.y = position.y - 5
print(position.y) -- 15.0
```

### vec:length()

Returns the length of the vector.

```lua
local m = require('haylen.math')

print(m.vec2(3, 4):length()) -- 5.0
```

### vec:lengthSquared()

Returns the squared length, which is cheaper than `length` and enough for comparing distances.

```lua
local m = require('haylen.math')

local offset = m.vec2(30, 40)
local radius = 60
print(offset:lengthSquared() < radius * radius) -- true
```

### vec:angle()

Returns the angle of the vector in radians, measured from the positive x axis toward the positive y axis, between -π and π.

```lua
local m = require('haylen.math')

local toTarget = m.vec2(0, 50)
print(m.degrees(toTarget:angle())) -- 90.0
```

### vec:rotated(radians)

Returns the vector rotated by `radians`, from the positive x axis toward the positive y axis, which is clockwise on screen.

```lua
local m = require('haylen.math')

local forward = m.vec2(1, 0)
local down = forward:rotated(m.pi / 2)
print(m.approximately(down.x, 0), m.approximately(down.y, 1)) -- true true
```

### vec:perpendicular()

Returns the vector `(-y, x)`, which is the vector rotated by a quarter turn.

```lua
local m = require('haylen.math')

local wall = m.vec2(10, 0)
print(wall:perpendicular()) -- Vec2(-0.0, 10.0)
```

### vec:clampedLength(maximum)

Returns the vector shortened to `maximum` when it is longer, or unchanged otherwise.

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
    print('standing still')
end
```

### vec:normalized()

Returns a vector with the same direction and length `1`. A zero vector stays zero.

```lua
local m = require('haylen.math')

local direction = m.vec2(0, -8):normalized()
print(direction) -- Vec2(0.0, -1.0)
```

### vec:dot(other)

Returns the dot product with `other`.

```lua
local m = require('haylen.math')

local facing = m.vec2(1, 0)
local toEnemy = m.vec2(5, 1):normalized()
print(facing:dot(toEnemy) > 0.7) -- true
```

### vec:cross(other)

Returns the 2D cross product `x * other.y - y * other.x`. It is positive when `other` points clockwise from the vector on screen.

```lua
local m = require('haylen.math')

local heading = m.vec2(1, 0)
print(heading:cross({0, 1})) -- 1.0
```

### vec:distance(other)

Returns the distance to `other`.

```lua
local m = require('haylen.math')

local player = m.vec2(0, 0)
print(player:distance({30, 40})) -- 50.0
```

### vec:distanceSquared(other)

Returns the squared distance to `other`, which is cheaper than `distance` and enough for comparing distances.

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

Returns the vector with both components rounded to the nearest integer, with halves rounded away from zero. Rounding positions before drawing keeps pixel art sharp.

```lua
local m = require('haylen.math')

print(m.vec2(10.5, -3.5):round()) -- Vec2(11.0, -4.0)
```

### vec:lerp(to, t)

Returns the linear interpolation toward `to`. `t` is not clamped.

```lua
local m = require('haylen.math')

local from = m.vec2(0, 0)
print(from:lerp({100, 50}, 0.5)) -- Vec2(50.0, 25.0)
```

### vec:unpack()

Returns `x` and `y` as two numbers.

```lua
local m = require('haylen.math')

local x, y = m.vec2(12, 34):unpack()
print(x + y) -- 46.0
```

### Vec2 operators

`a + b` and `a - b` add and subtract component-wise. `a * b` and `a / b` multiply and divide component-wise when both are vectors, and `v * s`, `s * v` and `v / s` scale by a number. `-v` negates, `a == b` compares both components and `tostring(v)` gives `Vec2(x, y)`. The operand that is not a `Vec2` may be a point table.

```lua
local m = require('haylen.math')

local position = m.vec2(100, 100)
local velocity = m.vec2(30, -10)
local dt = 0.5
position = position + velocity * dt
print(position) -- Vec2(115.0, 95.0)
print(-velocity, velocity / 10, m.vec2(2, 3) * {4, 5}) -- Vec2(-30.0, 10.0) Vec2(3.0, -1.0) Vec2(8.0, 15.0)
print(position == m.vec2(115, 95)) -- true
```

## Rect

An axis-aligned rectangle with its top left corner at `x`, `y`.

### rect.x, rect.y, rect.width, rect.height

Readable and writable position and size.

```lua
local m = require('haylen.math')

local hitbox = m.rect(0, 0, 32, 48)
hitbox.x, hitbox.y = 200, 150
hitbox.width = hitbox.width * 2
print(hitbox) -- Rect(200.0, 150.0, 64.0, 48.0)
```

### rect:left()

Returns the left edge, which is `x`.

```lua
local m = require('haylen.math')

local wall = m.rect(100, 0, 20, 600)
local player = {x = 90}
if player.x < wall:left() then
    print('left of the wall')
end
```

### rect:right()

Returns the right edge, which is `x + width`.

```lua
local m = require('haylen.math')

local platform = m.rect(100, 400, 300, 20)
print(platform:right()) -- 400.0
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

local platform = m.rect(100, 400, 300, 20)
print(platform:bottom()) -- 420.0
```

### rect:position()

Returns the top left corner as a `Vec2`.

```lua
local m = require('haylen.math')

local panel = m.rect(40, 60, 300, 200)
print(panel:position()) -- Vec2(40.0, 60.0)
```

### rect:min()

Returns the top left corner, which has the smallest coordinates, as a `Vec2`.

```lua
local m = require('haylen.math')

print(m.rect(10, 20, 30, 40):min()) -- Vec2(10.0, 20.0)
```

### rect:max()

Returns the bottom right corner, which has the largest coordinates, as a `Vec2`.

```lua
local m = require('haylen.math')

print(m.rect(10, 20, 30, 40):max()) -- Vec2(40.0, 60.0)
```

### rect:center()

Returns the center point as a `Vec2`.

```lua
local m = require('haylen.math')

print(m.rect(0, 0, 200, 100):center()) -- Vec2(100.0, 50.0)
```

### rect:size()

Returns the width and height as a `Vec2`.

```lua
local m = require('haylen.math')

local size = m.rect(10, 10, 640, 360):size()
print(size.x / size.y)
```

### rect:area()

Returns `width * height`.

```lua
local m = require('haylen.math')

local field = m.rect(0, 0, 40, 25)
print(field:area()) -- 1000.0
```

### rect:empty()

Returns `true` when the width or the height is zero or negative.

```lua
local m = require('haylen.math')

local overlap = m.rect(0, 0, 10, 10):intersection({20, 20, 5, 5})
print(overlap:empty()) -- true
```

### rect:contains(value)

Returns `true` when `value` lies inside the rectangle. `value` is a point or a rectangle. A point is inside when it is on the left or top edge or strictly between the edges, and a rectangle is inside when it lies entirely within. A table counts as a rectangle when it has a `width` field, so pass positional rectangles as `Rect` values.

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
    print('ouch')
end
```

### rect:intersection(other)

Returns the overlapping area as a `Rect`, or `Rect(0, 0, 0, 0)` when the rectangles do not overlap.

```lua
local m = require('haylen.math')

local screen = m.rect(0, 0, 1920, 1080)
local window = m.rect(1800, 1000, 400, 300)
print(screen:intersection(window)) -- Rect(1800.0, 1000.0, 120.0, 80.0)
```

### rect:merged(other)

Returns the smallest rectangle that contains both rectangles.

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
local touchArea = button:expanded(16)
print(touchArea) -- Rect(84.0, 84.0, 232.0, 92.0)
```

### rect:inset(insets)

Returns the rectangle shrunk by `insets` from its edges. `insets` is one number for every side, or a table with `left`, `top`, `right` and `bottom` by name or in that order. The width and the height never go below `0`. A value of another type raises `bad argument #1 to 'inset' (number or table with left, top, right and bottom expected, got string)`.

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

local platform = m.rect(200, 400, 128, 16)
print(platform:translated({0, -50})) -- Rect(200.0, 350.0, 128.0, 16.0)
```

### rect:clamp(point)

Returns `point` moved to the nearest position inside the rectangle, edges included, as a `Vec2`.

```lua
local m = require('haylen.math')

local arena = m.rect(0, 0, 1920, 1080)
local player = m.vec2(2000, -40)
print(arena:clamp(player)) -- Vec2(1920.0, 0.0)
```

### Rect operators

`a == b` compares all four fields, and `tostring(rect)` gives `Rect(x, y, width, height)`.

```lua
local m = require('haylen.math')

print(m.rect(1, 2, 3, 4) == m.rect(1, 2, 3, 4)) -- true
print(tostring(m.rect(1, 2, 3, 4))) -- Rect(1.0, 2.0, 3.0, 4.0)
```

## Color

A color with red, green, blue and alpha components between `0` and `1`, not premultiplied.

### color.r, color.g, color.b, color.a

Readable and writable components.

```lua
local m = require('haylen.math')

local tint = m.color('#FFFFFFFF')
tint.g, tint.b = 0.6, 0.6
tint.a = 0.5
print(tint) -- #80FF9999
```

### color:withAlpha(alpha)

Returns the same color with a different alpha.

```lua
local m = require('haylen.math')

local gold = m.color('#FFFFC107')
local ghostGold = gold:withAlpha(0.25)
print(ghostGold) -- #40FFC107
```

### color:premultiplied()

Returns the color with red, green and blue multiplied by alpha, the form that premultiplied blending expects.

```lua
local m = require('haylen.math')

local glow = m.color(1, 0.5, 0, 0.5)
local ready = glow:premultiplied()
print(ready.r, ready.g, ready.a) -- 0.5 0.25 0.5
```

### color:lerp(to, t)

Returns the component-wise interpolation toward `to`. `t` is not clamped.

```lua
local m = require('haylen.math')

local day = m.color('#FFFFFFFF')
local night = m.color('#FF2A3B6E')
print(day:lerp(night, 0.5))
```

### color:toHex()

Returns the color as `'#AARRGGBB'` text with uppercase digits, which `m.color` and every color argument read back.

```lua
local m = require('haylen.math')
local storage = require('haylen.storage')

local favorite = m.hsv(0.6, 0.7, 0.9)
storage.writeJson('settings/theme.json', {accent = favorite:toHex()})
```

### Color operators

`a * b` multiplies component-wise, which tints one color by another. `a == b` compares all components, and `tostring(color)` gives the same text as `toHex`.

```lua
local m = require('haylen.math')

local sprite = m.color('#FFFFFFFF')
local shade = m.color(0.5, 0.5, 1)
print(sprite * shade) -- #FF8080FF
print(m.color('#00000000') == m.color(0, 0, 0, 0)) -- true
```

## Transform

An affine 2D transform built with `m.transform`, `m.identity`, `m.translation`, `m.rotation` or `m.scaling`. It maps a point `p` to `(a * p.x + c * p.y + tx, b * p.x + d * p.y + ty)`.

### transform.a, transform.b, transform.c, transform.d, transform.tx, transform.ty

Readable and writable coefficients. `a` and `b` are where the local x axis points, `c` and `d` are where the local y axis points, and `tx` and `ty` are the translation.

```lua
local m = require('haylen.math')

local flip = m.identity()
flip.a = -1
flip.tx = 64
print(flip:apply({10, 5})) -- Vec2(54.0, 5.0)
```

### transform:apply(point)

Returns `point` transformed by the full transform, including the translation, as a `Vec2`.

```lua
local m = require('haylen.math')

local ship = m.transform({500, 300}, m.pi / 4)
local cannon = ship:apply({40, 0})
print(cannon)
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

Returns `a * d - b * c`, the factor by which the transform scales areas. It is negative when the transform mirrors and `0` when it collapses space.

```lua
local m = require('haylen.math')

local sprite = m.transform({0, 0}, 0.3, {2, -3})
print(sprite:determinant() < 0) -- true
```

### transform:translationPart()

Returns `tx` and `ty` as a `Vec2`, which is where the transform moves the origin.

```lua
local m = require('haylen.math')

local parent = m.transform({300, 200}, m.pi / 3)
print(parent:translationPart()) -- Vec2(300.0, 200.0)
```

### transform:inverse()

Returns the inverse transform, which maps transformed points back. A transform that collapses space, such as one with a zero scale, returns a transform with every coefficient zero.

```lua
local m = require('haylen.math')

local body = m.transform({200, 100}, m.pi / 6, {2, 2})
local world = body:apply({10, 5})
local localPoint = body:inverse():apply(world)
print(m.approximately(localPoint.x, 10), m.approximately(localPoint.y, 5)) -- true true
```

### Transform operators

`a * b` composes two transforms. The result applies `b` first and then `a`, so `(a * b):apply(p)` equals `a:apply(b:apply(p))`.

```lua
local m = require('haylen.math')

local parent = m.transform({300, 200}, m.pi / 2)
local child = m.transform({50, 0})
local world = parent * child
print(world:apply({0, 0})) -- close to Vec2(300.0, 250.0)
```

## Random

A deterministic xoshiro256** generator created with `m.random`.

### random:float()

Returns a number from `0` up to but not including `1`.

```lua
local m = require('haylen.math')

local random = m.random(5)
if random:float() < 0.1 then
    print('critical hit')
end
```

### random:range(minimum, maximum)

Returns a number from `minimum` up to but not including `maximum`.

```lua
local m = require('haylen.math')

local random = m.random(9)
local angle = random:range(0, m.tau)
local speed = random:range(80, 120)
print(angle, speed)
```

### random:integer(minimum, maximum)

Returns an integer from `minimum` to `maximum`, both included. When `maximum` is not greater than `minimum` it returns `minimum`. Both arguments must be integers, and a number with a fraction raises `number has no integer representation`.

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
    print('the chest holds a rare item')
end
```

### random:pick(weights)

Picks an index of the `weights` sequence with a probability proportional to each weight and returns it, starting at `1`. An empty sequence raises `bad argument #1 to 'pick' (expected at least one weight)`.

```lua
local m = require('haylen.math')

local random = m.random(11)
local loot = {'coin', 'potion', 'sword'}
local weights = {70, 25, 5}
print(loot[random:pick(weights)])
```

### random:shuffle(list)

Shuffles the sequence `list` in place and returns it.

```lua
local m = require('haylen.math')

local random = m.random(8)
local deck = random:shuffle({'ace', 'king', 'queen', 'jack'})
print(table.concat(deck, ', '))
```

### random:reseed(seed)

Restarts the generator from the integer `seed`, so it repeats the sequence of a new generator with that seed.

```lua
local m = require('haylen.math')

local random = m.random(1)
local first = random:float()
random:reseed(1)
print(random:float() == first) -- true
```

## Noise

Seeded gradient noise created with `m.noise`. Every function returns values between `-1` and `1` that change smoothly with the coordinates, and features are about one unit apart, so scale world coordinates down before sampling.

### noise:perlin(x, y)

Returns classic Perlin noise at `x`, `y`.

```lua
local m = require('haylen.math')

local noise = m.noise(3)
local wind = noise:perlin(12 / 50, 0.5)
print(wind)
```

### noise:simplex(x, y)

Returns simplex noise at `x`, `y`, which has fewer directional artifacts than Perlin noise.

```lua
local m = require('haylen.math')

local noise = m.noise(21)
local moisture = noise:simplex(640 / 200, 320 / 200)
print(moisture)
```

### noise:fractal(x, y, octaves, lacunarity, gain)

Returns several layers of simplex noise added together, normalized to stay between `-1` and `1`. `octaves` is the number of layers and defaults to `4`. `lacunarity` multiplies the frequency of each layer and defaults to `2`. `gain` multiplies the strength of each layer and defaults to `0.5`.

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

Returns cellular (Worley) noise at `x`, `y` as three numbers: the distance to the nearest feature point, the distance to the second nearest one, both in cell units, and a value from `0` to `1` that tells the region of the nearest point apart. Each unit cell holds one jittered feature point. The nearest distance draws stones and cells, the difference of both distances draws cracks, and the region value colors each cell.

```lua
local m = require('haylen.math')

local noise = m.noise(5)
local nearest, second, cell = noise:worley(3.2, 7.5)
local crack = second - nearest < 0.05
print(nearest, crack, cell)
```

### noise:warp(x, y, amplitude, frequency, octaves)

Moves the point by fractal noise sampled at `frequency`, up to `amplitude` away on each axis, and returns the new `x` and `y`. Sampling any noise at the warped point bends its patterns into swirls, which suits coastlines, marble and smoke. `frequency` defaults to `1` and `octaves` to `3`.

```lua
local m = require('haylen.math')

local noise = m.noise(8)
local wx, wy = noise:warp(120, 45, 30, 0.01)
local height = noise:fractal(wx / 200, wy / 200)
print(height)
```

## Polygon operations

`m.polygon` combines, grows, simplifies and splits shapes with Clipper2. A shape is a list of outlines, each a list of points, and any single outline is also accepted as a shape. One outline alone is filled in either winding, and an outline wound opposite to the outline around it is a hole. Results always wind outer outlines so that `m.polygonArea` is positive and holes so that it is negative, and they keep three decimal places.

### m.polygon.unite(shape, other)

Returns the union of both shapes, or of the outlines of `shape` alone when `other` is absent, which merges overlapping outlines.

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

Grows the shape by `distance`, or shrinks it when `distance` is negative, and returns the result. Shrinking can split a shape or make it vanish. Unknown keys of `options` raise `Unknown option '<key>'.`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `join` | string | `'round'` | Corner style: `'round'`, `'miter'`, `'square'` or `'bevel'`. Round corners follow the arc within one percent of the distance. |
| `miterLimit` | number | `2` | How far a miter corner may reach, in multiples of the distance, before it is squared off. |

```lua
local m = require('haylen.math')

local island = {{0, 0}, {300, 0}, {300, 200}, {0, 200}}
local beach = m.polygon.offset(island, 16)
local shallows = m.polygon.subtract(m.polygon.offset(island, 48, {join = 'miter'}), beach)
print(#shallows)
```

### m.polygon.simplify(points, tolerance, closed)

Drops the points of an outline or line that lie closer than `tolerance` to the simplified line, with the Ramer-Douglas-Peucker algorithm, and returns the points that stay. `closed` defaults to `true`, and closed outlines keep at least three points. Open lines keep both ends.

```lua
local m = require('haylen.math')

local path = {{0, 0}, {50, 1}, {100, 0}, {100, 100}}
print(#m.polygon.simplify(path, 2, false)) -- 3
```

### m.polygon.decompose(shape, maxVertices)

Splits a shape, holes included, into convex pieces of at most `maxVertices` points, which defaults to `8`, the limit of a Box2D polygon. The pieces come from a constrained Delaunay triangulation whose triangles merge while they stay convex, and each one winds with a positive area. `body:addPolygon` of `haylen.physics2d` uses it for concave outlines.

```lua
local m = require('haylen.math')

local arrow = {{0, 0}, {60, 0}, {30, 20}, {60, 60}, {0, 60}}
for _, piece in ipairs(m.polygon.decompose(arrow)) do
    print(#piece, m.polygonConvex(piece))
end
```

### m.polygon.area(shape)

Returns the filled area of a shape, where holes subtract.

```lua
local m = require('haylen.math')

local frame = {{{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {{25, 25}, {25, 75}, {75, 75}, {75, 25}}}
print(m.polygon.area(frame)) -- 7500.0
```

## Marching squares

`m.marchingSquares` traces the outlines of the areas of a grid that reach a threshold, such as the land of a height map or the solid pixels of a bitmap. Outlines are closed lists of `Vec2` that follow the same winding as the results of `m.polygon`, so they feed `m.polygon.simplify`, `body:addChain` and `graphics2d.drawPolygon` directly.

### m.marchingSquares.trace(values, width, height, options)

Traces a field of `width` times `height` numbers stored row by row. Sample `(x, y)` lies at `origin + (x, y) * spacing`, outlines pass between samples where the values cross the threshold, and areas that reach the edge of the grid close along the outermost samples. A value count that does not match the size raises `The field needs width times height values.`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `threshold` | number | `0.5` | Values at or above it are inside. |
| `spacing` | number | `1` | Distance between neighboring samples. |
| `origin` | Vec2 | `{0, 0}` | Position of the first sample. |

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
print(#coasts .. ' islands and lakes')
```

### m.marchingSquares.traceBitmap(pixels, width, height, options)

Traces the pixels that are `true` or not zero. Pixel `(x, y)` covers the square from `origin + (x, y) * spacing` to `origin + (x + 1, y + 1) * spacing`, and outlines cut diagonally across pixel corners. The options take `spacing` and `origin` as in `trace`.

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

A smooth curve through or near control points, measured by arc length so it can be walked at a steady speed or sampled at even distances. Parameters run from `0` at the start to `1` at the end of the whole curve.

### m.spline(points, options)

Creates a `Spline` from a list of points. A kind that needs other point counts raises an error such as `A Bezier spline needs 3n + 1 points when open and 3n points when closed.`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | `'catmullRom'` | `'catmullRom'` passes through every point with centripetal parameterization. `'bezier'` chains cubic segments whose points go end, control, control, end and so on. `'bspline'` stays near the points without passing through them, except for the ends of open curves. |
| `closed` | boolean | `false` | Joins the last point back to the first. Closed curves wrap parameters and distances around. |

```lua
local m = require('haylen.math')

local track = m.spline({{0, 0}, {200, -80}, {400, 0}, {600, -80}}, {kind = 'catmullRom'})
print(track.length)
```

### spline.kind, spline.closed, spline.points, spline.segmentCount, spline.length

Read-only properties with the kind name, whether the curve is closed, its control points, its number of segments and its length in world units.

```lua
local m = require('haylen.math')

local loop = m.spline({{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {closed = true})
print(loop.kind, loop.closed, #loop.points, loop.segmentCount, loop.length > 400)
```

### spline:point(t), spline:tangent(t)

Returns the `Vec2` at parameter `t` and the unit direction of travel there.

```lua
local m = require('haylen.math')

local arc = m.spline({{0, 0}, {50, -50}, {100, 0}})
print(arc:point(0.5), arc:tangent(0))
```

### spline:pointAtDistance(distance), spline:tangentAtDistance(distance), spline:parameterAtDistance(distance)

Returns the point, the unit direction or the parameter at `distance` world units along the curve, which moves things at a steady speed whatever the spacing of the control points.

```lua
local m = require('haylen.math')

local rail = m.spline({{0, 0}, {300, 0}, {300, 300}})
local traveled = 0
require('haylen.scene').push({
    update = function(self, dt)
        traveled = math.min(traveled + 120 * dt, rail.length)
        local position = rail:pointAtDistance(traveled)
        local heading = rail:tangentAtDistance(traveled):angle()
        print(position, heading)
    end,
})
```

### spline:sample(count), spline:sampleByDistance(spacing)

Returns `count` points spread evenly over the parameter, or points `spacing` apart along the curve plus the end point of an open curve. A spacing that is not positive raises `Sampling by distance needs a positive spacing.`

```lua
local m = require('haylen.math')

local fence = m.spline({{0, 0}, {100, 40}, {200, 0}, {300, 40}}, {kind = 'bezier'})
for _, post in ipairs(fence:sampleByDistance(25)) do
    print(post)
end
print(#fence:sample(10)) -- 10
```

## Spring

A critically damped spring that follows a moving target as fast as possible without overshooting it, like Unity's `SmoothDamp`. The smooth time is roughly how long the value takes to reach a target that stands still. It suits cameras, health bars and anything that should settle smoothly.

### m.spring(value, smoothTime)

Creates a `Spring` that starts at `value`, which defaults to `0`, with `smoothTime` in seconds, which defaults to `0.2`.

```lua
local m = require('haylen.math')

local zoom = m.spring(1, 0.3)
print(zoom.value)
```

### spring.value, spring.velocity, spring.smoothTime

Read-write properties with the current value, its speed per second and the smooth time.

```lua
local m = require('haylen.math')

local bar = m.spring(0)
bar.value = 50
bar.smoothTime = 0.1
print(bar.value, bar.velocity, bar.smoothTime)
```

### spring:update(target, dt)

Moves the value toward `target` over `dt` seconds and returns it. The result barely depends on the frame rate.

```lua
local m = require('haylen.math')

local health = m.spring(100, 0.25)
require('haylen.scene').push({
    update = function(self, dt)
        local shown = health:update(40, dt)
        print(shown)
    end,
})
```

### m.smoothDamp(current, target, velocity, smoothTime, dt)

Advances any number or `Vec2` toward `target` and returns the new value and the new velocity, for values that live elsewhere, such as a camera position.

```lua
local m = require('haylen.math')

local position, velocity = m.vec2(0, 0), m.vec2()
require('haylen.scene').push({
    update = function(self, dt)
        position, velocity = m.smoothDamp(position, {400, 300}, velocity, 0.2, dt)
    end,
})
```

## ShuffleBag

Deals items in random order without repeats until the bag runs out, then refills it, so every item keeps its share and streaks stay short. Use it for loot drops, music playlists and enemy waves that should feel fair.

### m.shuffleBag(items, options)

Creates a `ShuffleBag` of a list of items. Item `i` goes into the bag `counts[i]` times, once by default. The bag owns a generator seeded with `seed`, or with the default seed of `m.random`. A counts list of another length raises `expected one count per item`, and a bag without items raises `A shuffle bag needs at least one item.`

```lua
local m = require('haylen.math')

local drops = m.shuffleBag({'coin', 'gem', 'heart'}, {counts = {6, 1, 3}, seed = 7})
print(drops.size, drops.remaining) -- 10 10
```

### bag:next(random)

Deals the next item, refilling the bag first when it is empty. A `Random` passed as `random` is used instead of the bag's own generator.

```lua
local m = require('haylen.math')

local drops = m.shuffleBag({'coin', 'gem', 'heart'}, {counts = {6, 1, 3}})
for i = 1, 10 do
    print(drops:next())
end
```

### bag:refill(), bag.remaining, bag.size

`refill` puts every dealt item back. `remaining` is the number of items left before the next refill and `size` the number of items of a full bag.

```lua
local m = require('haylen.math')

local bag = m.shuffleBag({'a', 'b'})
bag:next()
bag:refill()
print(bag.remaining) -- 2
```

## WeightedChoice

Picks items in proportion to fixed weights in constant time with Vose's alias method, which pays off when the same weights serve many picks. `random:pick` suits weights that change between picks.

### m.weightedChoice(items, weights, seed)

Creates a `WeightedChoice` of a list of items and a list of weights of the same length. The choice owns a generator seeded with `seed`, or with the default seed of `m.random`. Weights must be finite and not negative, with at least one positive weight.

```lua
local m = require('haylen.math')

local loot = m.weightedChoice({'common', 'rare', 'legendary'}, {90, 9, 1}, 42)
print(loot.size) -- 3
```

### choice:pick(random), choice:probability(item), choice.size

`pick` returns an item, drawing from `random` when a `Random` is passed. `probability` returns the chance of the item at 1-based position `item`, and `size` the number of items.

```lua
local m = require('haylen.math')

local loot = m.weightedChoice({'common', 'rare', 'legendary'}, {90, 9, 1})
print(loot:pick(), loot:probability(3))
```
