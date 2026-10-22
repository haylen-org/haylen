# 📖 `haylen.lighting2d`

The module `haylen.lighting2d` holds the lights and occluders of lit 2D canvases, the helpers that build occluders from physics bodies and Tiled maps, and the light queries of gameplay. Canvases, draws and the draw options that shade them come from [`haylen.graphics2d`](graphics2d.md). The [lighting guide](../2d/lighting.md) explains lit canvases, light types, blend modes, masks and shadows.

```lua
local lighting2d = require('haylen.lighting2d')
```

## Functions

### lighting2d.newLight(properties)

Creates a [`Light`](#light) with the [properties](#properties) of the table over the defaults. The table is optional, and a `Light` in its place makes a copy. Unknown keys raise `Unknown option "<key>".`

```lua
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local sun = lighting2d.newLight({type = 'directional', rotation = 0.6, color = '#FFFFF0D8', shadows = true, shadowColor = '#AA000000'})
local lamp = lighting2d.newLight({type = 'spot', x = -300, y = 0, radius = 700, innerAngle = 0.3, outerAngle = 0.7, intensity = 2, blend = 'add'})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF303848'})
        graphics2d.drawRect({-600, -400, 1200, 800}, '#FF4E7A3A')
        graphics2d.drawLight(sun)
        graphics2d.drawLight(lamp)
    end,
})
```

### lighting2d.newOccluder(properties)

Creates an [`Occluder`](#occluder) with the [properties](#properties-1) of the table over the defaults. The table is optional, and an `Occluder` in its place makes a copy. Unknown keys raise `Unknown option "<key>".`, and a flat list of points with an odd count or a value that is not a number raises `A flat list of points needs two numbers per point.`

```lua
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local pillar = lighting2d.newOccluder({points = {{-15, -15}, {15, -15}, {15, 15}, {-15, 15}}, cull = 'counterClockwise'})
local lamp = lighting2d.newLight({x = -200, y = 0, radius = 500, shadows = true})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF101018'})
        graphics2d.drawRect({-600, -400, 1200, 800}, '#FF707070')
        for index = 0, 3 do
            pillar.x, pillar.y = index * 90, (index % 2) * 120 - 60
            graphics2d.drawRect({pillar.x - 15, pillar.y - 15, 30, 30}, '#FF303030', {layer = 1})
            graphics2d.drawOccluder(pillar)
        end
        graphics2d.drawLight(lamp)
    end,
})
```

### lighting2d.occludersFromBody(world, body)

Returns a list of occluders, one for each shape of a body of a [`physics2d`](physics2d.md) world. The occluders sit in the space of the body at its current position and rotation. Polygons, boxes, circles and capsules make closed occluders, and segments open ones. The segments of a chain join into one outline, closed when the chain loops. To follow a moving body, give its occluders its position and rotation in every frame.

```lua
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local crate = world:createBody({x = 0, y = -200})
crate:addBox(40, 40)
local floor = world:createBody({type = 'static', x = 0, y = 150})
floor:addBox(600, 20)
local shapes = lighting2d.occludersFromBody(world, crate)
local camera = graphics2d.newCamera()
local lamp = lighting2d.newLight({x = -250, y = 0, radius = 600, shadows = true, shadowFilter = 'pcf5'})

scene.push({
    update = function(self, dt)
        world:step(dt)
        for _, shape in ipairs(shapes) do
            shape.x, shape.y, shape.rotation = crate.x, crate.y, crate.rotation
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF181820'})
        graphics2d.drawRect({-600, -400, 1200, 800}, '#FF6A6A62')
        for _, shape in ipairs(shapes) do
            graphics2d.drawOccluder(shape)
        end
        graphics2d.drawLight(lamp)
    end,
})
```

### lighting2d.occludersFromMap(map, layer)

Returns a list of occluders, one for each object of the named object layer of a [Tiled](tiled.md) map renderer, or of every object layer when `layer` is `nil`. The occluders are in world coordinates, with the offsets of the layer and its groups. Rectangles, tile objects, ellipses, capsules and polygons make closed occluders, and polylines open ones. A tile object outlines its image where it draws. Points and text have no outline and make no occluder. A name that is not an object layer raises `The map has no object layer named "<name>".`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local scene = require('haylen.scene')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/level.tmj'))
local walls = lighting2d.occludersFromMap(map, 'walls')
local camera = graphics2d.newCamera()
local lamp = lighting2d.newLight({x = 200, y = 150, radius = 500, shadows = true})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF101018'})
        map:draw(camera)
        for _, wall in ipairs(walls) do
            graphics2d.drawOccluder(wall)
        end
        graphics2d.drawLight(lamp)
    end,
})
```

### lighting2d.illuminate(ambient, lights, x, y, options)

Returns the `Color` of the light map at a point, after the ambient color and a list of lights, in order and with their blend modes. It works as the light pass does, without normal maps or shadows. The list `lights` holds `Light` objects or tables with their properties. Combine it with `light:shadowedAt` to tell how lit a character really is. The table `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `lightMask` | integer | `1` | The light mask of the draw at the point. |
| `layer` | integer | `0` | The layer of the draw at the point. |

```lua
local lighting2d = require('haylen.lighting2d')

local torches = {lighting2d.newLight({x = 0, y = 0, radius = 200}), {x = 150, y = 0, radius = 120, color = '#FFFF8040'}}
local seen = lighting2d.illuminate('#FF101020', torches, 60, 0, {layer = 2})
print(seen.r > 0.5 and 'visible' or 'hidden')
```

### lighting2d.falloff(distance)

Returns the radial falloff of lights without a texture, the curve of `graphics2d.lightTexture()`. The `distance` is a fraction of the radius. The result is 1 at the center and fades smoothly to 0 at the radius and beyond. Gameplay code uses it to weigh light the way the light pass does.

```lua
local lighting2d = require('haylen.lighting2d')

local lamp = lighting2d.newLight({x = 0, y = 0, radius = 200})
local distance = 80
print(lighting2d.falloff(distance / lamp.radius), lamp:strengthAt(distance, 0))
```

### lighting2d.flicker(time, options)

Returns an intensity multiplier that wavers like a flame, between `1 - amount` and 1, from smooth noise. The same time, options and seed always give the same value. Unknown keys raise `Unknown option "<key>".` The table `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `speed` | number | `8` | How fast the value changes. |
| `amount` | number | `0.15` | How far the value can dim below 1. With 0 it always returns 1. |
| `seed` | integer | `0` | The seed of the noise. Give each light its own seed so they do not flicker in step. |

```lua
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF101830'})
        local wave = lighting2d.flicker(haylen.elapsed(), {speed = 6, amount = 0.2, seed = 4})
        graphics2d.drawLight({x = 0, y = 0, radius = 140 * wave, color = '#FFFF9040', intensity = 0.8 * wave})
    end,
})
```

## Light

A `Light` holds every setting of one light. Create it once with `lighting2d.newLight` and draw it in every frame with [`graphics2d.drawLight`](graphics2d.md), which also takes a table with the same keys. Its properties read and write straight into the light.

### Properties

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `type` | string | `'point'` | `'point'`, `'spot'` or `'directional'`. |
| `x`, `y`, `position` | number, Vec2 | `0` | The center of point and spot lights. |
| `radius` | number | `160` | The reach of point and spot lights. It must be positive. |
| `color` | Color | `'#FFFFFFFF'` | The color of the light. Its alpha scales the light like the intensity. |
| `intensity` | number | `1` | Multiplies the color. It must be zero or positive and may go above 1. |
| `rotation` | number | `0` | Turns the texture of point lights and points spot and directional lights. 0 points right and a quarter turn points down. |
| `scaleX`, `scaleY` | number | `1` | Stretch the texture of point and spot lights. They must be positive. |
| `texture` | Texture or nil | `nil` | The shape of point and spot lights. Its colors tint the light and its alpha is the strength. With `nil` the light uses the radial falloff. |
| `innerAngle`, `outerAngle` | number | `0.5`, `1` | The full angles in radians of the cone of spot lights. The inner angle may not exceed the outer one, and the outer one may not exceed a full turn. |
| `height` | number | `0` | The height above the canvas, which sets the angle light reaches normal-mapped draws at. It must be zero or positive. |
| `enabled` | boolean | `true` | A disabled light draws nothing. |
| `blend` | string | `'add'` | `'add'`, `'subtract'`, `'mix'` or `'max'`. |
| `itemMask` | integer | `1` | The bits of the light masks of the draws the light reaches, from 0 to 255. |
| `layerMin`, `layerMax` | integer | all layers | The layers of the draws the light reaches. |
| `shadows` | boolean | `false` | Lets occluders block the light. |
| `shadowFilter` | string | `'none'` | How wide the edge of a shadow fades: over 1 texel of the shadow map with `'none'`, over 5 with `'pcf5'` and over 13 with `'pcf13'`, and never over less than a pixel. |
| `shadowColor` | Color | `'#FF000000'` | The light that shadowed pixels still receive. Its alpha sets how dark shadows are. |
| `shadowSmoothness` | number | `0` | Widens the window of the shadow filter by this many times its width more, so `'pcf5'` with a smoothness of 1 fades the edge over 10 texels of the shadow map. It must be zero or positive. |
| `shadowMask` | integer | `1` | The bits of the masks of the occluders that cast the shadows of the light. |

```lua
local assets = require('haylen.assets')
local lighting2d = require('haylen.lighting2d')

local window = lighting2d.newLight({texture = assets.texture('lights/window.png'), radius = 240, scaleX = 1.5, scaleY = 0.8, height = 30})
window.position = {320, 180}
window.layerMax = 4
window.shadows = true
window.shadowSmoothness = 2
window.shadowMask = 3
window.enabled = false
print(window.type, window.texture, window.layerMin)
```

### Errors

The function `graphics2d.drawLight` checks a light before it draws it, and so do `light:strengthAt`, `light:apply`, `light:shadowedAt` and `lighting2d.illuminate`. A value out of its range raises one of these errors:

- `A light radius must be positive.`
- `A light intensity must be zero or positive.`
- `A light scale must be positive.`
- `A spot light needs an outer angle up to a full turn and an inner angle from 0 to the outer angle.`
- `A light height must be zero or positive.`
- `A light layer range needs "layerMin" at most "layerMax".`
- `A light shadow smoothness must be zero or positive.`

### light:affects(lightMask, layer)

Returns whether the light reaches draws with the light mask and the layer.

```lua
local lighting2d = require('haylen.lighting2d')

local lamp = lighting2d.newLight({itemMask = 6, layerMin = 1})
print(lamp:affects(2, 1), lamp:affects(1, 1), lamp:affects(2, 0))
```

### light:strengthAt(x, y)

Returns how strongly the light reaches a point, from 0 to 1, through the radial falloff and the cone of point and spot lights. A directional light reaches every point fully, and a textured light counts with the radial falloff.

```lua
local lighting2d = require('haylen.lighting2d')

local spot = lighting2d.newLight({type = 'spot', x = 0, y = 0, radius = 300, rotation = 0, innerAngle = 0.4, outerAngle = 0.8})
print(spot:strengthAt(100, 0), spot:strengthAt(0, 100))
```

### light:apply(color, x, y)

Returns the light map value after the light draws over `color` at a point, with its blend mode and its falloff, without normal maps or shadows.

```lua
local lighting2d = require('haylen.lighting2d')

local sun = lighting2d.newLight({type = 'directional', intensity = 0.5})
print(sun:apply('#FF404040', 0, 0))
```

### light:shadowedAt(x, y, occluders)

Returns whether one of the occluders stands between the light and a point, with the cull modes and masks of the shadow map. The list `occluders` holds `Occluder` objects or tables with their properties. A light without shadows never shadows a point. An occluder with too few points raises `An occluder needs at least 2 points, and 3 when it is closed.`

```lua
local lighting2d = require('haylen.lighting2d')

local lamp = lighting2d.newLight({x = 0, y = 0, radius = 300, shadows = true})
local wall = lighting2d.newOccluder({points = {100, -50, 100, 50}, closed = false})
print(lamp:shadowedAt(200, 0, {wall}), lamp:shadowedAt(50, 0, {wall}))
```

## Occluder

An `Occluder` is an outline that blocks the lights with shadows. Its points are local to its position, rotation and scale. The function [`graphics2d.drawOccluder`](graphics2d.md) draws it in a lit canvas, and also takes a table with the same keys. Drawing an occluder with too few points raises `An occluder needs at least 2 points, and 3 when it is closed.`

### Properties

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `points` | list of Vec2 | empty | The outline, as a list of vectors or as a flat list of numbers, two per point. Reading it returns a list of vectors. |
| `closed` | boolean | `true` | Joins the last point to the first. A closed occluder needs 3 points and an open one 2. |
| `cull` | string | `'disabled'` | `'disabled'`, `'clockwise'` or `'counterClockwise'`: the edges that the light sees wound that way on the screen cast no shadow. |
| `mask` | integer | `1` | The bits that the shadow masks of lights test. |
| `x`, `y`, `position` | number, Vec2 | `0` | Where the points go. |
| `rotation` | number | `0` | Turns the points, in radians. |
| `scaleX`, `scaleY` | number | `1` | Scale the points. |

```lua
local lighting2d = require('haylen.lighting2d')

local gate = lighting2d.newOccluder({points = {{0, 0}, {60, 0}, {60, 10}, {0, 10}}, mask = 2})
gate.position = {200, 100}
gate.scaleX, gate.scaleY = 2, 1
gate.cull = 'clockwise'
print(#gate.points, gate.closed, gate.mask)
```

### occluder:worldPoints()

Returns the points in world coordinates, through the scale, the rotation and the position.

```lua
local lighting2d = require('haylen.lighting2d')

local door = lighting2d.newOccluder({points = {0, 0, 40, 0}, closed = false, x = 100, y = 50, rotation = math.pi / 2})
for _, point in ipairs(door:worldPoints()) do
    print(point.x, point.y)
end
```
