# 🧭 2D coordinates

This page describes the conventions that every 2D part of Haylen follows: the axes, the spaces that positions live in, the units, the angles, the order of transforms, the order of draws and color. Every 2D module and every app that draws in 2D uses the same conventions, so a position or an angle means the same thing in the renderer, in physics, in Tiled maps and in the UI. The [3D coordinates](../3d/coordinates.md) describe the other dimension, and the [math guide](../core/math.md) explains the types that carry these values.

## Axes

The `x` axis points right and the `y` axis points down, as on a screen. The origin of a world is wherever the app puts it, and a larger `y` is lower on the screen.

```text
   +-------- +x (right)
   |
   |
   |
  +y (down)
```

A `Vec2` holds the two coordinates. The 2D types never mix with the 3D ones, whose `y` points up, since each dimension has types of its own.

```lua
local m = require('haylen.math')

local below = m.vec2(100, 100) + m.vec2(0, 50)
print(below.y > 100) -- true, so the point is lower on the screen
```

## Spaces

A position of 2D lives in one of three spaces:

| Space | What it measures | Where it shows |
| --- | --- | --- |
| Design units | The layout of the screen, 1920 by 1080 units by default, which the `design` section of `haylen.json` sets. | Screen canvases, the UI, and the positions of the mouse and of fingers. |
| World units | The positions of the world that a camera looks at, the units of sprites, bodies, maps and nodes. | World canvases and render target canvases, through their camera. |
| Pixels | The pixels of the framebuffer or of a render target. | Captures, readbacks and the pixel rectangle of the viewport. |

A camera maps world units to design units with its position, rotation and zoom, so a camera without zoom shows one world unit as one design unit. The viewport maps design units to pixels by the scaling policy of the app. The [2D graphics guide](graphics.md#design-units-and-the-viewport) explains the viewport, and the camera converts between the spaces with `camera:screenToWorld` and `camera:worldToScreen`.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')

local camera = graphics2d.newCamera()

local worldX, worldY = camera:screenToWorld(input.mousePosition())
print(worldX, worldY)
```

## Units

World units have no fixed size, so an app picks what one unit means, such as one pixel of its art. Physics needs meters, so each world of `haylen.physics2d` converts world units to meters with its `pixelsPerMeter` scale, 64 by default. Every value that physics takes and returns stays in world units, so a game keeps one set of numbers. The [physics guide](physics.md#units-and-scale) lists every quantity with its unit.

```lua
local physics2d = require('haylen.physics2d')

local world = physics2d.newWorld({pixelsPerMeter = 32})
print(world.pixelsPerMeter) -- 32
```

Time is in seconds everywhere, and masses are in kilograms.

## Angles

Angles are in radians. A positive angle turns from the positive `x` axis toward the positive `y` axis, which is clockwise on the screen. So the angle `m.pi / 2` points down. The functions `m.radians` and `m.degrees` convert between radians and degrees.

```lua
local m = require('haylen.math')

local down = m.fromAngle(m.pi / 2, 10)
print(down, m.degrees(down:angle())) -- close to Vec2(0.0, 10.0) and 90.0
```

The rotation of a sprite, of a camera, of a body and of a node follows the same rule.

## Transforms

A `Transform2D` maps a point `p` to `(a * p.x + c * p.y + tx, b * p.x + d * p.y + ty)`. The operator `*` composes transforms so that the one on the right applies first, so a child inside a parent is `parent * child`. A transform made from a position, a rotation and a scale scales first, then rotates, then moves.

```lua
local m = require('haylen.math')

local parent = m.transform2d({300, 200}, m.pi / 2)
local child = m.transform2d({50, 0})
print((parent * child):apply({0, 0})) -- close to Vec2(300.0, 250.0)
```

## Draw order

The order of 2D draws comes from their layers, not from a depth buffer. Inside a canvas, a lower layer draws first, and a canvas may sort the draws of one layer by their depth or by the `y` they stand on, so a character lower on the screen draws in front. The canvases themselves reach the screen by their `order` and then in the order they began, so a screen canvas that begins after the world canvas draws above it. The [2D graphics guide](graphics.md#draw-order) explains every key of the order.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
graphics2d.beginWorld(camera, {sort = 'y'})
graphics2d.drawRect({0, 0, 40, 80}, '#FF4A7A9A', {layer = 1})
```

## Color

Colors of 2D are written as `'#RRGGBB'` or `'#AARRGGBB'` strings or as `Color` values, encoded in sRGB, the way people pick them in art and design tools. The [color guide](../graphics/color.md) explains encoded colors, linear light and colors brighter than white.

```lua
local m = require('haylen.math')

local sky = m.fromHex(0x87CEEBFF)
print(sky.b > sky.r) -- true
```
