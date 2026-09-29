# haylen.lighting2d

`haylen.lighting2d` holds the lights and occluders of lit 2D canvases: point, spot and directional lights with blend modes, masks, layer ranges and shadows, the occluders that cast those shadows and helpers that build them from physics bodies and Tiled maps. It also answers how lit a point is, for stealth and visibility gameplay, and makes torches waver with its flicker function. Canvases, draws and the draw options that shade them come from [haylen.graphics2d](graphics2d.md).

```lua
local lighting2d = require('haylen.lighting2d')
```

## How 2D lighting works

A world or render target canvas becomes lit when it receives `ambientLight`. Its draws then fill four images at once: their colors, their emission, the normal and specular strength of their surface, and the light mask and layer that lights test. Every `graphics2d.drawLight` draws into a light map that starts at the ambient color, in the order the lights were drawn, and the composite multiplies the colors by the light map and adds the emission. A white ambient color shows the scene unchanged, and a dark blue one turns it into night where only the lights show color.

The light map holds light in floating point when the backend can render and blend it, which `graphics2d.hdrLighting()` reports and which holds on Metal, Direct3D 11, desktop OpenGL, WebGPU and WebGL2 with `EXT_color_buffer_float`. Light then goes past 1, so an intensity of 3 or overlapping lights brighten the scene beyond its unlit colors, and a subtracted light can take the light map below 0. Without floating-point render targets the light map saturates at white.

Lights come in three types:

| Type | Shape |
| --- | --- |
| `'point'` | Shines around its position up to its radius through its texture, the radial falloff of `graphics2d.lightTexture()` when it has none. |
| `'spot'` | A point light that only shines inside a cone around its rotation, fully inside `innerAngle` and fading out at `outerAngle`, both full angles in radians. |
| `'directional'` | Covers the whole canvas from its rotation, the direction the light travels, like the sun. Position, radius, texture and scale do not apply. |

Each light combines with the light map by its blend mode: `'add'` brightens it, `'subtract'` darkens it and `'mix'` replaces it by the strength of the light, which suits a colored pool of light over a lit room. A light reaches a draw when the draw's `lightMask` shares a bit with the light's `itemMask` and the draw's layer lies between `layerMin` and `layerMax`. Draws have the light mask 1 and lights the item mask 1 unless they say otherwise, so masks only matter once they change. Masks have 8 bits, and layer ranges tell layers apart from -128 to 127 and count layers beyond them as the nearest end.

Draws shade themselves with the draw options of [graphics2d](graphics2d.md#draw-order). `normalMap` gives a sprite a second texture with the same layout as its texture, whose red and green store the x and y of the surface normal with y pointing up the image, from 0 for -1 to 255 for 1, the convention of most normal map tools. Normal-mapped sprites take light by the angle it reaches them at, where the light `height` lifts it above the canvas, and `specular`, scaled by the alpha of the normal map, and `shininess` shape their highlights. `unshaded` draws keep their own colors whatever the light, and `emission` makes the colors of a draw glow by that strength on top of the light, so `emission = 1` shows them fully even in the dark.

### Shadows

A light with `shadows = true` is blocked by occluders, the outlines that `graphics2d.drawOccluder` draws in the same canvas, like LightOccluder2D in Godot. Every frame the engine casts a 1D shadow map of 1024 texels per shadowed light: the distance of the nearest occluder in every direction around a point or spot light, and for a directional light the depth of the nearest occluder along the light at every point across the canvas, so its shadows run across the whole view. The light pass compares each pixel with its texel, with one sample when `shadowFilter` is `'none'` and 5 or 13 samples with `'pcf5'` and `'pcf13'`, spread wider by `shadowSmoothness`. Shadowed pixels receive `shadowColor`, whose alpha sets how dark the shadow is.

Every edge of an occluder casts shadows unless its `cull` mode skips the edges the light sees wound one way. A polygon whose points go clockwise on screen with `cull = 'counterClockwise'` skips its edges that face the light, so the object stays lit and only what lies behind it is dark. Without culling the object sits in its own shadow. Occluders cast the shadows of lights whose `shadowMask` shares a bit with the occluder `mask`.

Occluders and lights belong to the canvas they are drawn in, and every lit canvas casts the shadows of its own lights from its own occluders. Shadow maps are cast on the CPU in parallel on the task pool, so their cost grows with the shadowed lights times the edges within their reach.

```lua
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local torch = lighting2d.newLight({x = -150, y = 0, radius = 420, color = '#FFFFC080', intensity = 1.4, shadows = true, shadowFilter = 'pcf13', shadowSmoothness = 1})
local crate = lighting2d.newOccluder({points = {-20, -20, 20, -20, 20, 20, -20, 20}, cull = 'counterClockwise'})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF141824'})
        graphics2d.drawRect({-600, -400, 1200, 800}, '#FF6A6A62')
        graphics2d.drawRect({-20, -20, 40, 40}, '#FF5A4A3A', {layer = 1})
        graphics2d.drawOccluder(crate)
        torch.intensity = 1.4 * lighting2d.flicker(haylen.time(), {seed = 2})
        graphics2d.drawLight(torch)
    end,
})
```

## Light

A `Light` holds every setting of one light, so a scene creates it once and draws it every frame with `graphics2d.drawLight`, which also accepts a table with the same keys. Its properties read and write straight into the light:

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `type` | string | `'point'` | `'point'`, `'spot'` or `'directional'`. |
| `x`, `y`, `position` | number, Vec2 | `0` | Center of point and spot lights. |
| `radius` | number | `160` | Reach of point and spot lights. It must be positive. |
| `color` | Color | `'#FFFFFFFF'` | Light color, whose alpha scales the light like the intensity. |
| `intensity` | number | `1` | Multiplies the color. It must be zero or positive and may go above 1. |
| `rotation` | number | `0` | Turns the texture of point lights and points spot and directional lights, where 0 points right and a quarter turn points down. |
| `scaleX`, `scaleY` | number | `1` | Stretch the texture of point and spot lights. They must be positive. |
| `texture` | Texture or nil | nil | Shape of point and spot lights, whose colors tint the light and whose alpha is its strength. `nil` uses the radial falloff. |
| `innerAngle`, `outerAngle` | number | `0.5`, `1` | Full angles in radians of the cone of spot lights. The inner angle may not exceed the outer one, which may not exceed a full turn. |
| `height` | number | `0` | Height above the canvas, which sets the angle light reaches normal-mapped draws at. |
| `enabled` | boolean | `true` | Disabled lights draw nothing. |
| `blend` | string | `'add'` | `'add'`, `'subtract'` or `'mix'`. |
| `itemMask` | integer | `1` | Bits of the light masks of the draws the light reaches, from 0 to 255. |
| `layerMin`, `layerMax` | integer | all layers | The layers of the draws the light reaches. |
| `shadows` | boolean | `false` | Lets occluders block the light. |
| `shadowFilter` | string | `'none'` | `'none'`, `'pcf5'` or `'pcf13'`. |
| `shadowColor` | Color | `'#FF000000'` | Light that shadowed pixels still receive, whose alpha sets how dark shadows are. |
| `shadowSmoothness` | number | `0` | Spreads the filter samples wider, in shadow map texels beyond the first. |
| `shadowMask` | integer | `1` | Bits of the masks of the occluders that cast the light's shadows. |

`graphics2d.drawLight` raises an error for a value out of its range, such as `A light radius must be positive.`, `A light intensity must be zero or positive.` or `A light layer range needs layerMin at most layerMax.`

### light:affects(lightMask, layer)

Tells whether the light reaches draws with the light mask and the layer.

```lua
local lighting2d = require('haylen.lighting2d')

local lamp = lighting2d.newLight({itemMask = 6, layerMin = 1})
print(lamp:affects(2, 1), lamp:affects(1, 1), lamp:affects(2, 0))
```

### light:strengthAt(x, y)

Returns how strongly the light reaches a point, from 0 to 1, through the radial falloff and the cone of point and spot lights. Directional lights reach every point fully, and textured lights count with the radial falloff.

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

Tells whether one of the occluders stands between the light and a point, with the cull modes and masks the shadow map uses. `occluders` is a list of `Occluder` objects or tables with their properties. Lights without shadows never shadow a point, which makes the query suit stealth games that hide the player in shadows.

```lua
local lighting2d = require('haylen.lighting2d')

local lamp = lighting2d.newLight({x = 0, y = 0, radius = 300, shadows = true})
local wall = lighting2d.newOccluder({points = {100, -50, 100, 50}, closed = false})
print(lamp:shadowedAt(200, 0, {wall}), lamp:shadowedAt(50, 0, {wall}))
```

## Occluder

An `Occluder` is an outline that blocks the light of lights with shadows. Its points are local to its position, rotation and scale, so the occluder of a moving object copies the object's position and rotation every frame. `graphics2d.drawOccluder` draws it in a lit canvas, and also accepts a table with the same keys.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `points` | list of Vec2 | empty | The outline, given as a list of vectors or as a flat list of numbers, two per point. Reading it returns a list of vectors. |
| `closed` | boolean | `true` | Joins the last point to the first. Closed occluders need 3 points and open ones 2. |
| `cull` | string | `'disabled'` | `'disabled'`, `'clockwise'` or `'counterClockwise'`, the edges the light sees wound that way on screen that cast no shadow. |
| `mask` | integer | `1` | Bits the shadow masks of lights test. |
| `x`, `y`, `position` | number, Vec2 | `0` | Where the points go. |
| `rotation` | number | `0` | Turns the points in radians. |
| `scaleX`, `scaleY` | number | `1` | Scale the points. |

Drawing an occluder with too few points raises `An occluder needs at least 2 points, and 3 when it is closed.`

### occluder:worldPoints()

Returns the points in world coordinates, through the scale, the rotation and the position.

```lua
local lighting2d = require('haylen.lighting2d')

local door = lighting2d.newOccluder({points = {0, 0, 40, 0}, closed = false, x = 100, y = 50, rotation = math.pi / 2})
for _, point in ipairs(door:worldPoints()) do
    print(point.x, point.y)
end
```

## Functions

### lighting2d.newLight(properties)

Creates a `Light` with the properties of the table over the defaults. Unknown keys raise `Unknown option 'name'.`

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

Creates an `Occluder` with the properties of the table over the defaults. Unknown keys raise `Unknown option 'name'.`, and a flat list of points with an odd count raises `a flat list of points needs two numbers per point`.

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

Returns a list of occluders, one per shape of a [physics2d](physics2d.md) body, in the space of the body at its current position and rotation: polygons, boxes, circles and capsules closed, and segments open. The segments of a chain join into one outline, closed when the chain loops. Occluders of a moving body follow it when they take its position and rotation every frame. The collision of a Tiled map, which `map:buildCollision(world)` turns into bodies, gives occluders for its tile collision shapes this way too.

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

Returns a list of occluders, one per object of the named object layer of a [Tiled](tiled.md) map, or of every object layer when `layer` is `nil`, in world coordinates with the offsets of the layer and its groups: rectangles, tile objects, ellipses, capsules and polygons closed, and polylines open. Points and text have no outline and make no occluder. An unknown layer raises `Unknown object layer: name`.

```lua
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local tiled = require('haylen.tiled')
local assets = require('haylen.assets')
local scene = require('haylen.scene')

local map = tiled.newMap(assets.load('maps/level.tmj'))
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

Returns the `Color` of the light map at a point that the ambient color and a list of lights leave, in order and with their blend modes, as the light pass computes it without normal maps or shadows. `lights` holds `Light` objects or tables with their properties. `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `lightMask` | integer | `1` | Light mask of the draw at the point. |
| `layer` | integer | `0` | Layer of the draw at the point. |

It tells how visible a character standing in the dark is, and combined with `light:shadowedAt` how lit it really is.

```lua
local lighting2d = require('haylen.lighting2d')

local torches = {lighting2d.newLight({x = 0, y = 0, radius = 200}), {x = 150, y = 0, radius = 120, color = '#FFFF8040'}}
local seen = lighting2d.illuminate('#FF101020', torches, 60, 0)
print(seen.r > 0.5 and 'visible' or 'hidden')
```

### lighting2d.flicker(time, options)

Returns an intensity multiplier that wavers like a flame, from smooth noise, between `1 - amount` and 1. The same time, options and seed always give the same value. `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `speed` | number | `8` | How fast the value changes. |
| `amount` | number | `0.15` | How far the value can dim below 1. 0 always returns 1. |
| `seed` | integer | `0` | Noise seed. Give each light its own seed so they do not flicker in step. |

Unknown keys raise `Unknown option 'name'.`

```lua
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF101830'})
        local wave = lighting2d.flicker(haylen.time(), {speed = 6, amount = 0.2, seed = 4})
        graphics2d.drawLight({x = 0, y = 0, radius = 140 * wave, color = '#FFFF9040', intensity = 0.8 * wave})
    end,
})
```
