# 💡 Lighting

This guide explains how to light a 2D scene with the module `haylen.lighting2d`. It is for developers who want night scenes, torches, flashlights, glowing magic or shadows behind walls. It covers lit canvases, the kinds of lights, how a light chooses what it reaches, how surfaces take light, shadows, the light questions that gameplay asks and what lighting costs. The [`haylen.lighting2d` reference](../lua-api/lighting2d.md) lists every function, property and error, and the [rendering guide](../graphics/rendering.md#lighting) describes the passes on the GPU.

## A lit canvas

Lighting happens in a lit canvas. A world canvas or a render target canvas of [`haylen.graphics2d`](../lua-api/graphics2d.md) becomes lit when you give it an `ambientLight` color. Canvases without it draw as usual, so a HUD on the screen canvas stays unlit while the world behind it is dark.

The draws of a lit canvas fill four images at once: their colors, their glow, the surface they show and the masks and layer that lights test, as the [rendering guide](../graphics/rendering.md#lighting) describes. Every draw blends all four with its own blend mode, as OpenGL and WebGL2 must. A draw that covers what lies below, with the blend `'alpha'`, `'premultiplied'` or `'opaque'`, writes its surface, masks and layer where it covers more than half of a pixel. A draw that adds to, multiplies or screens what lies below writes nothing into them, so the lights still reach what it lies over, and an additive glow brightens a lit wall instead of darkening it. A multiplying draw also leaves the glow alone. A lit canvas also keeps a light map, an image of how much light reaches each pixel. The light map starts at the ambient color. Every light you draw then changes it, in the order you draw them. At the end, the canvas multiplies the colors of the scene by the light map and adds the glow of the draws that emit light.

So the ambient color sets the mood. White shows the scene as it is. A dark blue turns it into a night where only the lights show color.

```lua
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local lamp = lighting2d.newLight({x = 0, y = 0, radius = 300, color = '#FFFFD890'})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF182030'})
        graphics2d.drawRect({-600, -400, 1200, 800}, '#FF6A8A5A')
        graphics2d.drawLight(lamp)
    end,
})
```

## Lights

Create a light once with `lighting2d.newLight` and draw it in every frame with `graphics2d.drawLight`. A `Light` keeps all its settings, and you change them through its properties, such as `lamp.x` or `lamp.intensity`. The function `graphics2d.drawLight` also takes a plain table with the same keys, which suits a light that lives for one frame.

There are three types of light:

| Type | What it lights |
| --- | --- |
| `'point'` | A circle around its position, up to its `radius`. It fades from the center through its `texture`, or through a smooth radial falloff without one. |
| `'spot'` | The part of a point light inside a cone that points along its `rotation`. It is full inside `innerAngle` and fades out at `outerAngle`. Both are full angles in radians. |
| `'directional'` | The whole canvas, like the sun. Its `rotation` is the direction the light travels. Position, radius, texture and scale do not apply. |

A light texture gives a point or spot light any shape, such as a window or a lantern pattern. Its colors tint the light and its alpha is the strength. Turn the shape with `rotation` and stretch it with `scaleX` and `scaleY`.

```lua
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local sun = lighting2d.newLight({type = 'directional', rotation = 0.6, color = '#FFFFF0D8', intensity = 0.4})
local flashlight = lighting2d.newLight({type = 'spot', x = -300, y = 0, radius = 700, innerAngle = 0.3, outerAngle = 0.7, intensity = 2})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF303848'})
        graphics2d.drawRect({-600, -400, 1200, 800}, '#FF4E7A3A')
        graphics2d.drawLight(sun)
        graphics2d.drawLight(flashlight)
    end,
})
```

## Blend modes

The `blend` of a light says how it changes the light map:

- `'add'` brightens the light map. It is the default and suits most lights.
- `'subtract'` darkens it, for a spot of darkness or a curse that eats light.
- `'mix'` replaces the light map by the strength of the light. It suits a colored pool of light over a room that is already lit.
- `'max'` keeps the brighter of the light map and the light in each color channel. Torches whose circles overlap then light the overlap no more than the brightest of them, which many tile games want.

## Light beyond white

The light map holds light in floating point when the graphics device can render and blend such images. The function `graphics2d.hdrLighting()` tells whether it can. It can on Metal, Direct3D 11, desktop OpenGL, WebGPU, and WebGL2 with the extension `EXT_color_buffer_float`.

With floating point, light can go past 1. An `intensity` of 3, or lights that overlap, brighten the scene beyond its unlit colors. A subtracted light can also take the light map below 0. Without floating point, the light map stops at white.

## What a light reaches

By default every light reaches every draw. Masks and layers limit that, such as a lamp that lights the floor of its room but not the roof above it.

- Each draw has a `lightMask` and each light an `itemMask`. A light reaches a draw when the two masks share a bit. Both are 1 unless you change them, so masks only matter once you do.
- Each light has a layer range, from `layerMin` to `layerMax`. A light reaches a draw only when the layer of the draw lies in that range.

Light masks and item masks have 8 bits. Shadow masks and occluder masks, described below, have 32 bits. Layer ranges tell the layers from -128 to 127 apart, and count a layer beyond them as the nearest end.

```lua
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local roomLamp = lighting2d.newLight({x = 0, y = 0, radius = 400, itemMask = 2, layerMax = 4})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF101018'})
        graphics2d.drawRect({-300, -200, 600, 400}, '#FF806040', {lightMask = 2, layer = 1})
        graphics2d.drawRect({-300, -260, 600, 60}, '#FF404060', {lightMask = 1, layer = 5})
        graphics2d.drawLight(roomLamp)
    end,
})
```

## How surfaces take light

Draws take light through the draw options of [`haylen.graphics2d`](../lua-api/graphics2d.md#draw-order). Unlit canvases ignore these options.

- The option `normalMap` gives a sprite a second texture with the same layout as its texture. Its red and green hold the x and y of the surface normal, the direction the surface faces, with y pointing up the image. The values go from 0 for -1 to 255 for 1, the convention of most normal map tools. A normal-mapped sprite takes light by the angle the light reaches it at, so bricks and armor show their bumps.
- The `height` of a light lifts it above the canvas, which changes that angle.
- The option `specular` sets how shiny a surface is, scaled by the alpha of the normal map, and `shininess` how tight its highlights are.
- The option `unshaded` keeps the colors of a draw whatever the light.
- The option `emission` makes a draw glow by that strength on top of the light, for eyes, screens and fire. With `emission = 1` a draw shows its full colors even in the dark.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local wall = assets.texture('walls/bricks.png')
local bumps = assets.texture('walls/bricks-normal.png')
local torch = lighting2d.newLight({x = 120, y = -40, radius = 360, height = 40, color = '#FFFFC080'})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF141824'})
        graphics2d.draw(wall, 0, 0, {normalMap = bumps, specular = 0.6, shininess = 24})
        graphics2d.drawCircle(120, -40, 8, '#FFFFD060', {emission = 1})
        graphics2d.drawLight(torch)
    end,
})
```

## Shadows

A light with `shadows = true` is blocked by occluders. An occluder is an outline, a list of points, that you draw with `graphics2d.drawOccluder` in the same canvas as the light. Create it once with `lighting2d.newOccluder`. Its points are local to its position, rotation and scale, so the occluder of a moving object copies the place of the object in every frame.

For each shadowed light, the engine casts a shadow map in every frame. It is a row of 1024 texels. Around a point or spot light, each texel holds the distance to the nearest occluder in one direction. For a directional light, each texel holds the depth of the nearest occluder along the light at one point across the canvas, so its shadows run across the whole view.

These settings shape the shadows:

- The `shadowFilter` softens the edges. With `'none'` each pixel tests the shadow map once. The filters `'pcf5'` and `'pcf13'` test it 5 and 13 times, and `shadowSmoothness` spreads those tests wider.
- The `shadowColor` is the light that shadowed pixels still get. Its alpha sets how dark the shadows are.
- The `shadowMask` of a light and the `mask` of an occluder choose which occluders block which lights. An occluder casts the shadow of a light when the two masks share a bit.

Every edge of an occluder casts a shadow, unless its `cull` mode skips the edges that the light sees wound one way. Take a box whose points go clockwise on the screen. With `cull = 'counterClockwise'` it skips the edges that face the light, so the box stays lit and only what lies behind it is dark. Without culling, the box sits in its own shadow. Some edges have a line that passes through the light, such as the two edges of a corner the light sits on. Such an edge shows the light only its side and casts no shadow, while the other edges still do.

Occluders and lights belong to the canvas you draw them in. Each lit canvas casts the shadows of its own lights from its own occluders.

```lua
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
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
        torch.intensity = 1.4 * lighting2d.flicker(haylen.elapsed(), {seed = 2})
        graphics2d.drawLight(torch)
    end,
})
```

## Occluders from bodies and maps

Drawing the outline of every wall by hand is slow work. Two helpers build occluders from what the level already has:

- `lighting2d.occludersFromBody(world, body)` returns one occluder for each shape of a body of [`haylen.physics2d`](physics.md). For a body that moves, copy its position and rotation to the occluders in every frame.
- `lighting2d.occludersFromMap(map, layer)` returns one occluder for each object of an object layer of a map of [`haylen.tiled`](tiled.md). The tile collision of a map, which `map:buildCollision(world)` turns into bodies, gives occluders through `occludersFromBody` too.

```lua
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')

local world = physics2d.newWorld()
local crate = world:createBody({x = 0, y = -200})
crate:addBox(40, 40)
local shadows = lighting2d.occludersFromBody(world, crate)
local camera = graphics2d.newCamera()
local lamp = lighting2d.newLight({x = -250, y = 0, radius = 600, shadows = true})

scene.push({
    update = function(self, dt)
        world:step(dt)
        for _, shadow in ipairs(shadows) do
            shadow.x, shadow.y, shadow.rotation = crate.x, crate.y, crate.rotation
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF181820'})
        for _, shadow in ipairs(shadows) do
            graphics2d.drawOccluder(shadow)
        end
        graphics2d.drawLight(lamp)
    end,
})
```

## Light in gameplay

The same falloff, cones and shadows also answer questions on the CPU. A stealth game uses them to tell whether a guard can see the player in the dark.

- `light:strengthAt(x, y)` tells how strongly a light reaches a point, from 0 to 1.
- `light:shadowedAt(x, y, occluders)` tells whether occluders hide a point from a light.
- `lighting2d.illuminate(ambient, lights, x, y)` returns the color that the ambient color and a list of lights leave at a point. It works as the light pass does, without normal maps or shadows. Compare it with a threshold to decide whether a character is visible.
- `lighting2d.falloff(distance)` returns the radial falloff curve of lights without a texture, for a light meter that sums nearby lamps.

```lua
local lighting2d = require('haylen.lighting2d')

local lamps = {lighting2d.newLight({x = 0, y = 0, radius = 200, shadows = true}), lighting2d.newLight({x = 150, y = 0, radius = 120})}
local pillar = lighting2d.newOccluder({points = {40, -20, 60, -20, 60, 20, 40, 20}})
local playerX, playerY = 90, 0
local seen = lighting2d.illuminate('#FF101020', lamps, playerX, playerY)
local hidden = lamps[1]:shadowedAt(playerX, playerY, {pillar})
print(seen.r > 0.5 and not hidden and 'The guard sees you.' or 'You are hidden.')
```

## Flickering flames

The function `lighting2d.flicker(time, options)` returns a number between `1 - amount` and 1 that wavers like a flame. Multiply the intensity or the radius of a torch by it. It comes from smooth noise and gives the same value for the same time, options and seed, so a replay shows the same flames. Give each torch its own `seed` so they do not flicker in step.

```lua
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local candles = {lighting2d.newLight({x = -100, radius = 120, color = '#FFFF9040'}), lighting2d.newLight({x = 100, radius = 120, color = '#FFFF9040'})}

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF101830'})
        for index, candle in ipairs(candles) do
            candle.intensity = lighting2d.flicker(haylen.elapsed(), {speed = 6, amount = 0.2, seed = index})
            graphics2d.drawLight(candle)
        end
    end,
})
```

Particle emitters of [`haylen.particles2d`](particles.md) carry lights of their own, which flicker the same way and draw only in lit canvases.

## Performance

- Lights draw as instances of one buffer that the frame writes once. Neighbouring lights of one blend mode and texture cost one draw call, so hundreds of lights cost a few. Draw the lights of one kind together.
- Shadow maps are cast on the CPU, in parallel on the task pool, into one atlas that the frame uploads once. Their cost grows with the number of shadowed lights times the occluder edges within their reach. Leave `shadows` off for small lights that do not need them.
- Each lit canvas keeps its own render targets at the pixel size of the canvas. Light one world canvas and draw the HUD unlit.

## From C++

The C++ types are `haylen::lighting2d::Light`, `Occluder` and `LightFlicker` in `haylen/2d/lighting/`. The methods `Renderer::drawLight` and `Renderer::drawOccluder` of `haylen::graphics2d` draw them into a lit canvas, and `Light::isShadowedAt` answers the shadow question on the CPU.

```cpp
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"

class Cellar final : public haylen::core::Scene {
  public:
    void render(haylen::core::Engine& engine) override {
        haylen::graphics2d::Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(camera, {.ambientLight = haylen::math::Color::fromHex(0x141824FF)});
        renderer.drawLight(lamp);
    }

  private:
    haylen::graphics2d::Camera camera;
    haylen::lighting2d::Light lamp{.radius = 300.0F, .shadows = true};
};
```
