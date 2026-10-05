# haylen.graphics2d

The module `haylen.graphics2d` draws everything a 2D app shows: sprites, sprite batches, shapes, meshes, text, rich text, nine-slice frames, metaballs, lights, occluders and parallax layers, into world, screen and offscreen canvases, shades them with custom shader materials, and captures whole frames into render targets. Use it inside the `render` and `renderUi` callbacks of a scene, and use its cameras, sprites, batches and materials anywhere. Textures, render targets, fonts and shaders come from [`haylen.graphics`](graphics.md), and lights and occluders from [`haylen.lighting2d`](lighting2d.md).

```lua
local graphics2d = require('haylen.graphics2d')
```

## Concepts

### Canvases

Every draw goes into the active canvas. A scene starts a canvas with `graphics2d.beginWorld`, `graphics2d.beginScreen` or `graphics2d.beginTarget`, and each new canvas closes the previous one. Drawing without an active canvas raises `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.` Canvases only exist while the engine renders a frame, so draw calls belong in `render` or `renderUi` and never in `update`.

World canvases look through a camera, into the camera viewport or across the whole visible area. Screen canvases use design coordinates, the space configured by `design` in `app.json` and reported by `viewport.visibleRect()`. Render target canvases draw into an offscreen texture. World and render target canvases can be lit and post-processed. The engine submits the whole frame at the end: offscreen work runs first in the order the frame recorded it, render target canvases, the offscreen passes of lit and post-processed canvases and [captures](#captures), then world and screen canvases are composited on the screen by their `order` and, for equal orders, in the order they were begun.

### Captures

The function `graphics2d.beginCapture(target)` sends the world and screen canvases that begin until `graphics2d.endCapture()` into a render target instead of the screen, lit and post-processed canvases included. The visible area covers the whole target, so a target with the pixel size of the viewport holds the frame exactly as the screen would show it, ready to be drawn as a texture, blended or kept. Scene transitions use captures to render the scenes before and after a change into their own images, as the [scene reference](scene.md#both-scenes-stay-alive) explains. A capture renders once the canvases before its end have their offscreen passes, and before any later canvas, so a canvas that draws the captured texture shows the frame it holds.

### Coordinates and angles

The y axis points down. Angles are in radians, and a positive rotation turns clockwise on screen. Pivots are fractions of a size, so `pivotX = 0.5, pivotY = 0.5` is the center and `0, 0` is the top-left corner.

### Values

Functions accept these forms wherever the reference names the type:

| Type | Accepted values |
| --- | --- |
| Color | A `Color` from [`haylen.math`](math.md), a `'#RRGGBB'` or `'#AARRGGBB'` string, or a table `{r, g, b, a}` or `{r = 1, g = 1, b = 1, a = 1}` with components from 0 to 1 and `a` defaulting to 1. |
| Vec2 | A `Vec2` from [`haylen.math`](math.md), or a table `{x, y}` or `{x = 0, y = 0}`. |
| Rect | A `Rect` from [`haylen.math`](math.md), or a table `{x, y, width, height}` or `{x = 0, y = 0, width = 0, height = 0}`. |

A malformed color string raises `invalid color text, expected #RRGGBB or #AARRGGBB`. Option tables reject keys they do not know with `Unknown option "name".` and reject non-string keys with `Option tables only accept string keys.` An enum option given an unknown name raises an argument error that contains `unknown value 'name'`.

### Draw order

Every draw that takes an `order` table, and every option table that lists `layer`, `depth` and `blend`, places and shades the draw in its canvas with these keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `layer` | integer | `0` | Higher layers draw later, on top of lower layers. |
| `depth` | number | `0` | Inside one layer, higher depths draw later when the canvas sorts by depth. Other canvases ignore it. |
| `sortOffset` | number | `0` | Moves the point a canvas that sorts by `'y'` sorts the draw by, such as from the center of a sprite down to its feet. |
| `visibility` | integer | `1` | Bits of the draw. A canvas whose `visibilityMask` shares no bit with them skips the draw. |
| `blend` | string | `'alpha'` | Blend mode of the draw. |
| `material` | Material | none | A [custom shader material](#material) that shades the pixels of the draw. Sprites, batches, static batches, nine-slices, rectangles, lines, shapes, meshes and text take materials, while image blends and metaballs raise `Image blends and metaballs do not take a material.` |
| `partMask` | Texture | none | Recolors sprites and sprite batches by parts, as [recoloring by parts](#recoloring-by-parts) explains. Other draws ignore it. |
| `normalMap` | Texture | none | In lit canvases, the normal map of a sprite, with the layout of its texture. |
| `specular` | number | `0` | In lit canvases, the strength of the highlights of a normal-mapped sprite, scaled by the alpha of its normal map. It must be zero or positive. |
| `shininess` | number | `32` | In lit canvases, how tight the highlights of a normal-mapped sprite are, from 1 to 255. |
| `emission` | number | `0` | In lit canvases, how strongly the colors of the draw glow whatever the light. It must be zero or positive. |
| `lightMask` | integer | `1` | In lit canvases, bits that the item masks of lights test, from 0 to 255. |
| `unshaded` | boolean | `false` | In lit canvases, keeps the colors of the draw whatever the light. |
| `distortion` | number | `0` | Above 0, the draw bends the image of its canvas instead of drawing colors, as [distortion](#distortion) explains. |

Unlit canvases ignore the lighting keys, and [`haylen.lighting2d`](lighting2d.md#how-2d-lighting-works) explains how lit canvases use them. A draw copies the values of its material when it is made, so a material changed between two draws shades each draw with its own values.

The `sort` option of a canvas chooses how draws of one layer are ordered. The value `'layer'` keeps the order in which the app made them, `'depth'` sorts them by `depth`, and `'y'` sorts them by the y they stand on plus their `sortOffset`, so lower draws cover higher ones without setting any depth. Sprites stand on their pivot, and every sprite of a batch sorts on its own. Text stands on its position, rich text on the bottom of its block, and rectangles, lines, shapes, meshes, nine-slices, static batches, image blends and metaballs stand on their lowest point. Draws that sort the same keep the order in which the app made them. The function `graphics2d.pushLayerOffset` shifts the layer of every following draw of the canvas, so a group of draws, such as a character and its shadow, can move between layers together.

Visibility bits hide draws from some canvases: draw the world once for the main camera and again for a minimap canvas whose `visibilityMask` leaves out details. The blend modes are:

| Name | Effect |
| --- | --- |
| `'alpha'` | Regular transparency. |
| `'additive'` | Adds the color to what is below, for glows, fire and sparks. |
| `'multiply'` | Multiplies with what is below, which darkens. |
| `'screen'` | Inverse multiply, which lightens. |
| `'premultiplied'` | Transparency for colors already multiplied by their alpha. |
| `'opaque'` | No blending, the draw replaces what is below. |

Every mode but `'premultiplied'` takes straight colors, the way images and colors are usually made, so a partly transparent multiply or screen draw weakens its effect in proportion to its alpha.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({100, 100, 200, 200}, '#FF2050A0', {layer = 2})
        graphics2d.drawRect({150, 150, 200, 200}, '#80FFC040', {layer = 1, blend = 'additive'})
    end,
})
```

## Functions

### graphics2d.beginWorld(camera, options)

Starts a world canvas that draws in world coordinates through `camera`. The camera's view covers its `viewport`, or the current visible design area without one, divided by its zoom, and the canvas reaches only that part of the screen, which is how split screens and minimaps draw several cameras in one frame. The argument `options` is an optional canvas options table. A viewport without a positive width and height raises `A camera viewport needs a positive width and height.` A world canvas with `ambientLight` or `postProcess` renders offscreen over its `clear` color and is composited as an image that covers what earlier canvases drew. Other world canvases draw directly on the screen, so a `clear` color without `ambientLight` or `postProcess` raises `World canvases only support a clear color with lighting or post-processing.`

Canvas options:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `sort` | string | `'layer'` | The value `'layer'` sorts draws by layer only. The value `'depth'` also sorts by depth inside each layer, and `'y'` sorts by the y each draw stands on, as [Draw order](#draw-order) describes. |
| `order` | integer | `0` | Canvases reach their destination by order, lowest first, and canvases with the same order in the order they began. |
| `visibilityMask` | integer | all bits | Draws whose `visibility` shares no bit with the mask are skipped. |
| `ambientLight` | Color | none | Turns on lighting. The canvas is multiplied by a light map that starts at this color and receives every `graphics2d.drawLight`, and occluders cast the shadows of its lights. |
| `clear` | Color | see meaning | Background of offscreen canvases. Render target canvases default to transparent. World canvases with `ambientLight` or `postProcess` default to the `clearColor` of `app.json`. Screen canvases and other world canvases draw directly on the screen, so they raise an error when given a clear color. |
| `postProcess` | table | none | Full-screen adjustments applied when the canvas is composited onto its destination. |

Post-process options:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `tint` | Color | `'#FFFFFFFF'` | Multiplies every pixel. |
| `saturation` | number | `1` | 0 is grayscale and values above 1 boost color. |
| `brightness` | number | `1` | Multiplies brightness. |
| `contrast` | number | `1` | Scales contrast around middle gray. |
| `vignetteStrength` | number | `0` | How dark the vignette makes the corners, from 0 to 1. |
| `vignetteRadius` | number | `0.6` | Distance from the center where the vignette starts, where 1 reaches the corners. |
| `vignetteSoftness` | number | `0.5` | Width of the vignette falloff. |
| `fade` | Color | `'#00000000'` | Mixes the result toward this color by its alpha, for fades to black or white. |
| `distortion` | number | `24` | How far the [distortion draws](#distortion) move pixels, in units of the destination, where their coverage goes from nothing to full within 8 units. |
| `chromaticAberration` | number | `0` | Splits red and blue apart toward the corners by this many units, like a cheap lens. |
| `pixelate` | number | `0` | Draws the image in square blocks of this many units, where 0 keeps every pixel. |
| `blur` | number | `0` | Blurs the whole image over this radius in units, such as the world behind a pause menu. |
| `bloomStrength` | number | `0` | Adds the parts of the image brighter than the threshold, blurred, times this strength, so lights, fire and neon glow. |
| `bloomThreshold` | number | `0.8` | The brightness, from 0 to 1 and above for floating-point light, that a color needs to bloom. |
| `bloomRadius` | number | `12` | How far bloom spreads, in units. It must be positive. |
| `colorLut` | Texture | none | Grades every color through a lookup texture of square cells side by side, one cell for each step of blue, with red across a cell and green down it, so it is as wide as its height squared, such as 256 by 16 pixels for 16 steps. |
| `colorLutStrength` | number | `1` | How much of the lookup texture mixes in, from 0 to 1. |
| `materials` | list of Material | empty | Custom shader passes that run after the other adjustments, in order, each over the image the step before made. The image is the texture of each pass, which covers the whole canvas. |

Units are design units for world canvases and pixels for render target canvases. The composite applies the steps in this order: pixelate, distortion and chromatic aberration on the image of the canvas, then bloom, saturation, contrast, brightness and tint, the lookup texture, the vignette, the fade and the materials. Bloom and blur cost a pass of the image before them and three passes at half the size each, and the other adjustments cost nothing beyond the composite. Out of range values raise `Post-processing needs a distortion, chromatic aberration, pixelate, blur, bloom strength and bloom threshold of at least 0 and a positive bloom radius.`, `A color lookup texture is as wide as its height squared, such as 256 by 16 pixels.` and `The strength of a color lookup texture must be from 0 to 1.`

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {
            sort = 'depth',
            ambientLight = '#FF404060',
            postProcess = {saturation = 0.8, vignetteStrength = 0.4, bloomStrength = 0.6, bloomThreshold = 0.7, chromaticAberration = 2},
        })
        graphics2d.drawRect({-200, -100, 400, 200}, '#FF3A7D44')
        graphics2d.drawLight({x = 0, y = 0, radius = 180, color = '#FFFFC080'})
    end,
})
```

### graphics2d.beginScreen(options)

Starts a screen canvas that draws in design coordinates, the space of `viewport.visibleRect()`. Use it for HUDs, menus and anything that does not move with the camera. The argument `options` accepts `sort`, `order` and `visibilityMask` from the canvas options. Passing `ambientLight` or `postProcess` raises `Screen canvases do not support lighting or post-processing.`, and passing `clear` raises `Screen canvases do not support a clear color.` because a screen canvas draws on top of what earlier canvases drew.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Score 120', 40, 40, {size = 48})
    end,
})
```

### graphics2d.beginTarget(target, camera, options)

Starts a canvas that draws into the render target `target` through `camera`. The camera's view covers the target size, so a camera at `target.width / 2, target.height / 2` shows the region from 0, 0 to the target size, and a camera `viewport` is a rectangle of the target in pixels. The argument `options` takes the canvas options of `beginWorld`, and `clear` defaults to transparent. With `ambientLight` or `postProcess` the canvas renders lit and post-processed offscreen and then composites into the target, so a minimap, a portrait or a mirror can show a lit scene. Draw the result with `target.texture`. The target holds colors premultiplied by their alpha, so draw its texture with `blend = 'premultiplied'` wherever it is not fully opaque, or its soft edges come out darker. A canvas that draws straight into the target cannot sample its texture in the same canvas, and such a draw raises `A draw cannot sample the render target that its canvas draws into.`

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local minimap = graphics.newRenderTarget(256, 256)
local minimapCamera = graphics2d.newCamera()
minimapCamera.x = 128
minimapCamera.y = 128

scene.push({
    render = function(self)
        graphics2d.beginTarget(minimap, minimapCamera, {clear = '#FF10202A'})
        graphics2d.drawCircle(128, 128, 40, '#FFFFD040')

        graphics2d.beginScreen()
        graphics2d.draw(minimap.texture, 20, 20, {pivotX = 0, pivotY = 0})
    end,
})
```

### graphics2d.draw(texture, x, y, options)

Draws `texture` once with its pivot at `x`, `y`. The argument `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `source` | Rect | whole texture | Region of the texture in pixels. |
| `width`, `height` | number | `0` | Drawn size before scaling. When both are 0 the size of the source is used. |
| `scaleX`, `scaleY` | number | `1` | Multiply the drawn size. |
| `pivotX`, `pivotY` | number | `0.5` | Point of the sprite that sits at `x`, `y` and that rotation turns around. |
| `rotation` | number | `0` | Rotation in radians. |
| `color` | Color | `'#FFFFFFFF'` | Multiplies the texture color. |
| `flash` | Color | `'#00000000'` | Mixes the result toward this color by its alpha, for hit flashes. |
| `flipHorizontal`, `flipVertical` | boolean | `false` | Mirror the image horizontally or vertically. |
| `flipDiagonal` | boolean | `false` | Mirror the image across its diagonal from the top-left corner, which with `flipHorizontal` and `flipVertical` turns it by quarter turns, the way Tiled rotates tiles. |
| `partColors` | table | white parts | The colors of the parts when the draw has a `partMask`, as [recoloring by parts](#recoloring-by-parts) explains. |
| `effect` | table | none | Dissolves the sprite or draws an outline and a glow around it, as [sprite effects](#sprite-effects) explains. |
| `x`, `y` | number | the arguments | Replace the position arguments. |
| `layer`, `depth`, `sortOffset`, `visibility`, `blend`, `material`, `normalMap`, `specular`, `shininess`, `emission`, `lightMask`, `unshaded`, `distortion` | | | [Draw order](#draw-order). |

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local sheet = assets.texture('sprites/knight/idle.png')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.draw(sheet, 400, 300, {source = {0, 0, 192, 192}, scaleX = 2, scaleY = 2, flipHorizontal = true, flash = '#80FFFFFF'})
    end,
})
```

### graphics2d.drawVector(image, x, y, options)

Draws a [`VectorImage`](graphics.md#vectorimage) with its pivot at `x`, `y`, at its own size unless the options give one. The engine rasterizes the image at the scale it covers on the screen, in steps a quarter of an octave apart, so it stays sharp at any size and zoom, and keeps the rasters in a vector atlas, so many images at many sizes batch into few draw calls. The first raster of an image is made at once, and every other one on worker threads while the nearest raster draws in its place, as the [rendering guide](../rendering.md#vector-images) explains. The argument `options` takes the keys of `graphics2d.draw` but `source` and `partColors`, and a `partMask` raises `A vector image draws without a part mask.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local heart = assets.vectorImage('icons/heart.svg')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        for index = 1, 8 do
            local size = 16 * index
            graphics2d.drawVector(heart, 40 + index * index * 12, 200, {width = size, height = size, color = '#FFFF8080', rotation = math.sin(haylen.elapsed()) * 0.2})
        end
    end,
})
```

### graphics2d.drawBatch(texture, sprites, order)

Draws a list of sprite tables that share `texture` as one batch, without keeping them in a `SpriteBatch`. Each sprite table uses the keys described in [`SpriteBatch`](#spritebatch). The argument `order` is an optional draw order table. An empty list draws nothing. Use a `SpriteBatch` instead when most sprites stay the same from frame to frame.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local pixel = graphics.whiteTexture()

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        local dots = {}
        for index = 1, 12 do
            local angle = haylen.elapsed() + index * 0.5236
            dots[index] = {x = 960 + math.cos(angle) * 200, y = 540 + math.sin(angle) * 200, width = 12, height = 12, color = '#FFFFD040'}
        end
        graphics2d.drawBatch(pixel, dots, {layer = 1, blend = 'additive'})
    end,
})
```

### graphics2d.drawBatch(texture, buffer, layout, order)

Draws the sprites a float buffer of [`haylen.collections`](collections.md#float-buffers) holds, without a table for each sprite and without a list of sprites in between. The argument `layout` is a sprite table, with the keys of [`SpriteBatch`](#spritebatch), that every sprite starts from, plus `fields`, the [sprite fields](#sprite-fields) each sprite takes from the buffer in order. The buffer holds as many sprites as it has whole groups of fields. A layout without `fields` raises `A "drawBatch" call with a float buffer needs the fields each sprite takes, such as "fields = {'x', 'y'}".`. The [performance section of the Lua guide](../lua.md#performance) compares it with sprite tables.

```lua
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local count = 5000
local sparks = collections.newFloatBuffer(count * 3)
for index = 1, count do
    sparks:set(index * 3 - 2, math.random(0, 1920), math.random(0, 1080), math.random())
end
local layout = {fields = {'x', 'y', 'alpha'}, width = 3, height = 3, color = '#FFFFD040'}

scene.push({
    update = function(self, dt)
        for slot = 2, count * 3, 3 do
            sparks[slot] = (sparks[slot] + 60 * dt) % 1080
        end
    end,
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawBatch(graphics.whiteTexture(), sparks, layout, {blend = 'additive'})
    end,
})
```

### graphics2d.drawRect(rect, color, order)

Fills `rect` with `color`. The rectangle's `x` and `y` are its top-left corner. The argument `order` is an optional draw order table.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({40, 40, 320, 24}, '#FF202020')
        graphics2d.drawRect({40, 40, 240, 24}, '#FF40C060', {layer = 1})
    end,
})
```

### graphics2d.drawRectOutline(rect, thickness, color, order)

Draws the border of `rect` with lines `thickness` units wide, inside the rectangle.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRectOutline({100, 100, 300, 200}, 4, '#FFFFFFFF')
    end,
})
```

### graphics2d.drawLine(x1, y1, x2, y2, thickness, color, order)

Draws a straight line from `x1`, `y1` to `x2`, `y2`, `thickness` units wide and centered on the segment. A line of zero length draws nothing.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawLine(100, 100, 500, 300, 3, '#FFFF4040')
    end,
})
```

### graphics2d.drawCircle(x, y, radius, color, order)

Fills a circle centered on `x`, `y`. Circles, rings, arcs and [shapes](#graphics2ddrawshaperect-options) draw as one quad each, whose edge the shader covers exactly, so it fades over one pixel of the screen or of the render target at any radius, zoom and density, as the [rendering guide](../rendering.md#shapes-and-smooth-edges) explains. A radius of 0 or less draws nothing.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawCircle(300, 300, 80, '#FF4080FF')
        graphics2d.drawCircle(500, 300, 3, '#FFFFFFFF')
    end,
})
```

### graphics2d.drawRing(x, y, radius, thickness, color, order)

Draws a circle outline `thickness` units wide whose middle line lies at `radius` from the center. A ring thicker than twice its radius fills the whole circle, and a thickness of 0 or less draws nothing.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRing(300, 300, 64, 6, '#FFFFD040')
    end,
})
```

### graphics2d.drawArc(x, y, radius, thickness, startAngle, endAngle, color, order)

Draws the part of a ring that goes from `startAngle` to `endAngle`, in radians, with square ends. Angle 0 points right and angles grow clockwise, and a span of a full turn or more draws the whole ring.

```lua
local graphics2d = require('haylen.graphics2d')
local mmath = require('haylen.math')
local scene = require('haylen.scene')

local progress = 0.7

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        local start = -mmath.pi / 2
        graphics2d.drawArc(300, 300, 48, 10, start, start + mmath.tau * progress, '#FF40E0A0')
    end,
})
```

### graphics2d.drawShape(rect, options)

Draws a rectangle with rounded corners, turned around its center, which also draws pills, capsules, circles, rings and arcs, and fills it with a border along its edge in the same quad, so the fill and the border meet without a seam. The argument `rect` is the area of the shape before it turns, and `options` is optional and also takes the [draw order](#draw-order) keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `radius` | number or table | `0` | The radius of every corner, or four radii from the top-left corner clockwise. A radius past half the shorter side rounds the whole end, so a radius of half the height makes a pill. |
| `rotation` | number | `0` | Turns the shape around the center of `rect`, in radians, clockwise. |
| `startAngle` | number | `0` | Where the sector the sweep keeps starts, in radians clockwise from the x axis of the shape after its rotation. |
| `sweep` | number | `mmath.tau` | Keeps the sector that starts at `startAngle` and turns clockwise by this angle, with straight sides from the center, such as the part of a ring that shows progress or a triangle cut from a rectangle around its corners. A full turn keeps the whole shape. |
| `color` | Color | `'#FFFFFFFF'` | The fill. |
| `borderWidth` | number | `0` | The width of the border, which runs inside the edge, at most half the shorter side. |
| `borderColor` | Color | `'#00000000'` | The color of the border. A clear fill with a border draws an outline. |
| `softness` | number | `0` | Widens the fade of the edge to this many units, centered on the edge, such as for a soft shadow. |

The edge fades over one pixel of the destination, or over the softness when it is wider, and a shape thinner than a pixel covers only the share of the pixels it crosses. A negative radius, border width or softness raises `A shape needs corner radii, a border width and a softness of zero or more.`, a radius table of another length raises `The option "radius" of a shape takes one radius for every corner or four, from the top-left corner clockwise.`, and a shape without a positive width and height draws nothing. Shapes draw with the white texture through their own program, so neighbouring shapes merge into one draw call, and a [material](#material) shades them through its `shape` program, where `uv` goes from 0 to 1 across the rectangle of the shape.

```lua
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawShape({200, 208, 320, 120}, {radius = 16, color = '#80000000', softness = 24})
        graphics2d.drawShape({200, 200, 320, 120}, {radius = 16, color = '#FF232938', borderWidth = 3, borderColor = '#FF8FB0FF'})
        graphics2d.drawShape({240, 360, 240, 32}, {radius = 16, rotation = math.sin(haylen.elapsed()) * 0.3, color = '#FFF2B23A'})
        graphics2d.drawShape({600, 200, 120, 120}, {radius = 60, startAngle = -math.pi / 2, sweep = math.pi * 1.5, color = '#FF6FDCA0'})
    end,
})
```

### graphics2d.drawPolygon(points, color, order)

Fills a simple polygon, convex or concave, given as a list of points in either winding. The fill stops half a pixel of the destination inside the outline and a fringe fades it out to half a pixel outside, so its edges cover the pixels they cross like the edges of shapes. Fewer than three points draw nothing.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawPolygon({{200, 100}, {300, 300}, {200, 220}, {100, 300}}, '#FFE0A030')
    end,
})
```

### graphics2d.drawPolyline(points, thickness, color, closed, order)

Draws line segments through `points`. When `closed` is `true`, a last segment joins the final point to the first one.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawPolyline({{100, 100}, {300, 120}, {260, 300}}, 2, '#FFFFFFFF', true)
    end,
})
```

### graphics2d.drawMesh(texture, vertices, indices, order)

Draws textured triangles. The argument `texture` can be `nil` for solid colors. Each vertex is a table with `x`, `y`, `u`, `v` and `color`, where `u` and `v` are texture coordinates from 0 to 1 and every field is optional (`color` defaults to white and the others to 0). The argument `indices` lists vertex numbers in groups of three, counting from 1. An index below 1 raises `mesh indices start at 1`, and an index past the last vertex raises `Mesh index refers to a vertex that does not exist.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local texture = assets.texture('effects/dust.png')
local vertices = {
    {x = 100, y = 100, u = 0, v = 0},
    {x = 300, y = 100, u = 1, v = 0},
    {x = 300, y = 200, u = 1, v = 1, color = '#FFFF8080'},
    {x = 100, y = 200, u = 0, v = 1},
}

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawMesh(texture, vertices, {1, 2, 3, 1, 3, 4})
        graphics2d.drawMesh(nil, {{x = 400, y = 100}, {x = 500, y = 100}, {x = 450, y = 180, color = '#FF4040FF'}}, {1, 2, 3}, {layer = 1})
    end,
})
```

### graphics2d.drawText(font, text, x, y, style)

Draws UTF-8 `text` with `font`, a `Font` or a `FontFamily`, or with the engine's default font when `font` is `nil`. A family draws every character its regular face lacks with the first fallback that has it, which is how one string mixes Latin, Arabic, Devanagari or CJK text. A TrueType font is shaped by HarfBuzz and rendered from a signed distance field, so it stays sharp at any size, with the ligatures, joining forms, conjuncts and marks of every script, and a bitmap font draws its own images, pixel for pixel at its native size. Every line is ordered for display by the Unicode bidirectional algorithm, so right-to-left text reads from the right and keeps numbers and Latin words in their own order. The character `\n` starts a new line, and so do `\r`, `\r\n`, which counts as one, and the other paragraph separators of Unicode, such as U+2029. Tabs and other control characters draw nothing and take no room. Lines wrap where the Unicode line breaking rules allow, which includes between Chinese and Japanese characters and between Thai phrases. The [text guide](../text.md#scripts-and-directions) explains shaping, directions and line breaking. The anchor point of the text block sits at `x`, `y`, and rotation turns the block around that point. The argument `style` is optional, and a `size`, `maxWidth` or `lineSpacing` that is not a finite number raises `Text needs a finite size, maximum width and line spacing.`:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `size` | number | `32` | Font size in canvas units, the height of the em square of a TrueType font. |
| `color` | Color | `'#FFFFFFFF'` | Text color. |
| `outlineWidth` | number | `0` | Outline width in canvas units. 0 draws no outline. Every outline of the text draws under every fill, so an outline never covers the letters beside it. Outlines, blurs and glows reach at most the distance field spread of the font, less the smoothing of small text, and bitmap fonts have none. |
| `outlineColor` | Color | `'#FF000000'` | Outline color. |
| `shadowOffset` | Vec2 | `{0, 0}` | Offset of the drop shadow. |
| `shadowColor` | Color | `'#00000000'` | Shadow color. The shadow is drawn only when its alpha is above 0. The shadow of a bitmap font is the silhouette of its glyphs. |
| `shadowBlur` | number | `0` | Softens the shadow over this many canvas units. |
| `align` | string | `'start'` | Alignment of each line inside the block: `'start'` and `'end'`, the sides where the lines of the paragraph begin and end, which are left and right for left-to-right text and the other way around for right-to-left text, `'left'`, `'center'`, `'right'` or `'fill'`, which stretches the spaces of every wrapped line to reach both edges and leaves the last line of a paragraph at its start. |
| `maxWidth` | number | `0` | Wraps whole words at this width. 0 never wraps. |
| `lineSpacing` | number | `1.2` | Distance between lines as a multiple of the line height. |
| `direction` | string | `'auto'` | Direction of every paragraph: `'auto'` takes the direction of its first strong letter, `'leftToRight'` and `'rightToLeft'` force it, which decides the order of mixed runs and the side of `'start'`. |
| `language` | string | `''` | BCP 47 language tag of the text, such as `'ar'`, `'fa'`, `'ur'` or `'sr'`, which the shaper uses to pick the forms a language prefers. |
| `bold`, `italic` | boolean | `false` | With a `FontFamily`, pick its bold and italic faces or synthesize them. A `Font` synthesizes them. |
| `anchor` | Vec2 | `{0, 0}` | Point of the block placed at `x`, `y`, as a fraction of its size. The value `{0.5, 0.5}` centers the text. |
| `rotation` | number | `0` | Rotation in radians. |
| `scale` | Vec2 | `{1, 1}` | Stretches the drawn block from its anchor point on each axis, without laying the text out again. The function `graphics2d.measureText` returns the stretched size for the same style. |
| `pixelSnap` | boolean | `false` | Moves the block so its left edge lands on a whole pixel of the destination and puts the baseline of every line on a whole pixel row, which keeps small text crisp and stops it from shimmering as it moves. It applies when neither the text nor the view of its canvas turns, and it never changes the layout. |
| `layer`, `depth`, `blend` | | | Draw order. |

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Victory!', 960, 300, {
            size = 96,
            color = '#FFFFE070',
            outlineWidth = 4,
            outlineColor = '#FF402000',
            shadowOffset = {6, 6},
            shadowColor = '#80000000',
            anchor = {0.5, 0.5},
        })
        graphics2d.drawText(nil, 'A long message that wraps inside the panel.', 960, 420, {size = 32, maxWidth = 400, align = 'center', anchor = {0.5, 0}})
    end,
})
```

A family with fallbacks for other scripts draws them in the same call:

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local family = graphics.newFontFamily({
    regular = graphics2d.defaultFont(),
    fallbacks = {assets.font('fonts/noto_sans_arabic_regular.ttf'), assets.font('fonts/noto_sans_devanagari_regular.ttf')},
})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(family, 'مرحبا بالعالم، رقم 42 (Harbor)', 1200, 200, {size = 40, maxWidth = 600, language = 'ar'})
        graphics2d.drawText(family, 'Order 7: السعر 42', 100, 300, {size = 40, direction = 'leftToRight'})
        graphics2d.drawText(family, 'नमस्ते दुनिया', 100, 400, {size = 40, language = 'hi'})
    end,
})
```

### graphics2d.measureText(font, text, style)

Returns the width and height of the text block that `graphics2d.drawText` would draw with the same arguments. The argument `font` can be a `Font`, a `FontFamily` or `nil` for the default font. Only the layout keys of `style` and its `scale` change the result.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        local width, height = graphics2d.measureText(nil, 'Paused', {size = 64})
        graphics2d.drawRect({960 - width / 2 - 20, 500 - 10, width + 40, height + 20}, '#C0000000')
        graphics2d.drawText(nil, 'Paused', 960 - width / 2, 500, {size = 64, layer = 1})
    end,
})
```

### graphics2d.newRichText(markup, options)

Creates a [`RichText`](#richtext) from BBCode markup, laid out once and drawn every frame with `text:draw(x, y, options)`. The [text guide](../text.md#markup) lists every tag. The argument `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `family` | FontFamily or Font | default font | The family of text without a `[font]` tag. A `Font` becomes the regular face of a family of its own. |
| `size` | number | `32` | Size of text without a `[size]` tag, in canvas units: the height of the em square of the font. |
| `bold` | boolean | `false` | Styles all the text bold, as if inside `[b]`. |
| `italic` | boolean | `false` | Styles all the text italic, as if inside `[i]`. |
| `color` | Color | `'#FFFFFFFF'` | Color of text without a `[color]` tag. |
| `maxWidth` | number | `0` | Width the paragraphs wrap at. 0 never wraps, and the block is as wide as its widest line. |
| `align` | string | `'start'` | Alignment of paragraphs without their own: `'start'`, `'end'`, `'left'`, `'center'`, `'right'` or `'fill'`, where `'start'` and `'end'` follow the direction of each paragraph. |
| `lineSpacing` | number | `1.2` | Distance between lines as a multiple of their height. |
| `direction` | string | `'auto'` | Direction of paragraphs without a `[p dir]` of their own: `'auto'` takes the direction of the first strong letter of each paragraph, and `'leftToRight'` and `'rightToLeft'` force it. |
| `language` | string | `''` | BCP 47 language tag the text is shaped for, such as `'ar'` or `'hi'`. |
| `scale` | number | `1` | Multiplies every size of the markup and the options. |
| `revealSpeed` | number | `0` | Characters per second the typewriter reveal shows as `text:update` advances. 0 shows everything at once. |
| `underlineLinks` | boolean | `true` | Whether `[url]` text is underlined. |
| `fonts` | table | none | The families or fonts that `[font=name]` tags name, as `{name = family}`. A key that is not a string raises `The "fonts" option maps font names to families, so its keys must be strings.`. |

Markup errors raise `Rich text markup at line L, column C: ...` with the place and the problem, such as `The tag "[b]" is never closed.` or `The markup "[wiggle]" is neither a tag nor a registered text effect.`. Unknown option keys raise `Unknown option "name".`, a `maxWidth` that is not a finite number raises `Rich text needs a finite maximum width.`, and a size, scale or line spacing that is not a positive number raises `Rich text needs a positive size, scale and line spacing.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont()})
local story = graphics2d.newRichText('[b]Welcome[/b], traveler!\nPress [color=gold]Start[/color] to [wave]begin[/wave].', {family = family, size = 36, maxWidth = 600, revealSpeed = 25})
local chapter = graphics2d.newRichText('Chapter [i]One[/i]', {family = family, size = 48, bold = true})

scene.push({
    update = function(self, dt)
        story:update(dt)
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        chapter:draw(660, 320)
        story:draw(660, 400)
    end,
})
```

### graphics2d.drawRichText(markup, x, y, options)

Draws markup once with its top-left corner at `x`, `y`, laying it out on every call. The argument `options` takes the keys of `graphics2d.newRichText`, `tint`, a `Color` that multiplies every color of the text and defaults to white, and the [draw order](#draw-order) keys, and effects follow the time the app has run. The images of `[img]` tags load once and stay loaded while frames keep drawing them, and they go back to the assets after a frame that no longer draws them. Text that stays on screen draws faster as a `RichText`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRichText('[outline=3 color=#402000][size=64]Level [color=gold]3[/color][/size][/outline]', 40, 40, {layer = 5, tint = '#FFFFE0C0'})
    end,
})
```

### graphics2d.measureRichText(markup, options)

Returns the width and height of the block `graphics2d.drawRichText` would draw with the same markup and options.

```lua
local graphics2d = require('haylen.graphics2d')

local width, height = graphics2d.measureRichText('[b]Quest complete[/b]\nYou found [color=gold]12 coins[/color].', {size = 28, maxWidth = 400})
print(width, height)
```

### graphics2d.registerTextEffect(name, effect)

Registers a text effect that runs as `[name]...[/name]` in rich text, in `haylen.graphics2d` and in `ui.richText`. Every frame, `effect(glyph, attributes)` runs once for each glyph inside the tag. The table `glyph` holds `index` (counted from 1 over every character inside the tag, spaces and images included), `character` (counted from 1 in the whole text), `char`, `codePoint`, `x` and `y` (its pen position on the baseline) and `time` (seconds the text has run), and the effect changes `offsetX`, `offsetY`, `color` and `visible`. The table `attributes` holds the attributes of the tag, with numbers as numbers and `[name=value]` as `attributes.value`. Registering a name again replaces the effect. A tag name, a built-in effect name (`wave`, `shake`, `tornado`, `fade`, `rainbow`, `pulse`) or a name with spaces raises an error, and an error inside the effect stops the app like any script error. An effect runs while its text builds the picture of the moment, so changing or measuring that text from inside the effect raises `A text effect cannot change or lay out the rich text it runs on.`.

```lua
local graphics2d = require('haylen.graphics2d')

graphics2d.registerTextEffect('bounce', function(glyph, attributes)
    local height = attributes.height or 8
    glyph.offsetY = glyph.offsetY - math.abs(math.sin(glyph.time * 6 + glyph.index * 0.5)) * height
end)

local title = graphics2d.newRichText('[bounce height=12]Jump![/bounce]', {size = 64})
```

### graphics2d.registerTextIcon(name, texture, options)

Registers an image that `[icon=name]` shows inline, such as an input prompt. The argument `options` takes `source` (a `Rect` of the texture, the whole texture by default) and `width` and `height`. An icon without a size is as tall as its text and keeps the shape of its region, and the tag may set its own `width`, `height`, `color` and `valign`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local prompts = assets.texture('ui/prompts.png', {filter = 'linear'})
graphics2d.registerTextIcon('confirm', prompts, {source = {0, 0, 64, 64}})
local hint = graphics2d.newRichText('Press [icon=confirm] to open the chest.', {size = 28})
```

### graphics2d.textEffectNames()

Returns the names of every registered text effect, the built-in ones included.

```lua
local graphics2d = require('haylen.graphics2d')

print(table.concat(graphics2d.textEffectNames(), ', '))
```

### graphics2d.drawNineSlice(slice, rect, color, order, borderScale)

Draws the nine-slice `slice` so it fills `rect`. Corners keep their size, edges stretch or tile along their length and the center fills the rest, and every piece samples only its own texels, so linear filtering never bleeds the gaps of an atlas into a frame. The argument `color` defaults to white. The argument `borderScale` multiplies the border sizes and defaults to 1, and one that is not positive raises `A nine-slice border scale must be positive.` When `rect` is narrower or shorter than the borders of an axis, they shrink in proportion until they meet, and a `rect` without a positive width and height draws nothing. The [rendering guide](../rendering.md#nine-slices) gives every rule, and [`slice:layout`](#slicelayoutrect-borderscale) returns the quads a draw makes.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local banner = graphics2d.newNineSlice(assets.texture('ui/banner.png'), {borders = {100, 68, 84, 111}})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawNineSlice(banner, {660, 200, 600, 400}, '#FFFFFFFF', {layer = 1}, 0.75)
    end,
})
```

### graphics2d.drawLight(light)

Draws a light into the light map of the active lit canvas, a world or render target canvas with `ambientLight`, over the lights drawn before it. The argument `light` is a [`Light`](lighting2d.md#light) from `lighting2d.newLight`, which a scene creates once, or a table with the same keys, such as `type`, `x`, `y`, `radius`, `color`, `intensity`, `rotation`, `innerAngle`, `outerAngle`, `height`, `blend`, `itemMask`, `layerMin`, `layerMax` and the shadow settings. Drawing a light in any other canvas raises `Lights can only be drawn in a lit canvas, a world or render target canvas with ambient light.`, and a value out of its range raises an error such as `A light intensity must be zero or positive.` Disabled lights draw nothing. Lights drawn one after another with the same blend mode and texture draw in one call, so draw the lights of one kind together. The module [`haylen.lighting2d`](lighting2d.md#how-2d-lighting-works) explains the light types, blend modes, masks and shadows.

```lua
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local beam = lighting2d.newLight({type = 'spot', x = 150, y = 50, radius = 400, rotation = 2.5, innerAngle = 0.3, outerAngle = 0.6, color = '#FF60A0FF', intensity = 1.5})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF202040'})
        graphics2d.drawRect({-400, -300, 800, 600}, '#FF5A8A50')
        graphics2d.drawLight({x = -100, y = 0, radius = 200, color = '#FFFFB060', intensity = 0.85 * lighting2d.flicker(haylen.elapsed())})
        graphics2d.drawLight(beam)
    end,
})
```

### graphics2d.drawOccluder(occluder)

Draws an outline that blocks the lights with shadows of the active lit canvas. The argument `occluder` is an [`Occluder`](lighting2d.md#occluder) from `lighting2d.newOccluder` or a table with its keys `points`, `closed`, `cull`, `mask`, `x`, `y`, `rotation`, `scaleX` and `scaleY`. Drawing an occluder outside a lit canvas raises `Occluders can only be drawn in a lit canvas, a world or render target canvas with ambient light.`, and too few points raise `An occluder needs at least 2 points, and 3 when it is closed.`

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF101018'})
        graphics2d.drawRect({-400, -300, 800, 600}, '#FF707070')
        graphics2d.drawRect({40, -30, 60, 60}, '#FF403028', {layer = 1})
        graphics2d.drawOccluder({points = {40, -30, 100, -30, 100, 30, 40, 30}, cull = 'counterClockwise'})
        graphics2d.drawOccluder({points = {{-200, 100}, {-100, 140}}, closed = false})
        graphics2d.drawLight({x = -150, y = 0, radius = 500, shadows = true, shadowFilter = 'pcf13', shadowSmoothness = 1})
    end,
})
```

### graphics2d.drawMetaballs(points, radius, options)

Adds up a soft circle of `radius` around every point into a field and draws the surface where the field reaches the threshold, which merges nearby circles into one smooth shape, such as a liquid made of particles. The argument `points` is a flat list of positions, `{x1, y1, x2, y2, ...}`, so thousands of points need no table each, and the `positions` of a [`physics2d`](physics2d.md) fluid fill it directly. A lone circle shows exactly its radius at the default threshold, and two circles join when they come closer than about two and a half radii. The field of each call is a render target at half the resolution of the canvas, drawn with one instanced pass. The argument `options` is optional and also takes the [draw order](#draw-order) keys, apart from `material`:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `color` | Color | `'#FFFFFFFF'` | Fill of the surface. |
| `outlineColor` | Color | `'#00000000'` | Color of the band of the field just above the threshold. |
| `outlineWidth` | number | `0` | Width of that band in field units, where a lone circle is 1 at its center. |
| `threshold` | number | `0.5` | Field value where the surface starts, between 0 and 1. Lower values make the shapes rounder and merge them sooner. |

A radius that is not positive raises `A metaball radius must be positive.`, a threshold outside 0 to 1 raises `A metaball threshold must be between 0 and 1.`, a negative outline width, or one that takes the threshold past 1, raises `A metaball outline width must be zero or more and keep the threshold plus the width below 1.`, and an odd number of values raises `metaball positions need two numbers per point`.

```lua
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local drops = {}

scene.push({
    render = function(self)
        local time = haylen.elapsed()
        for index = 0, 199 do
            drops[index * 2 + 1] = math.cos(index * 2.4 + time) * math.sqrt(index) * 12
            drops[index * 2 + 2] = math.sin(index * 2.4 + time) * math.sqrt(index) * 9
        end
        graphics2d.beginWorld(camera)
        graphics2d.drawMetaballs(drops, 14, {color = '#E02080FF', outlineColor = '#FFA0E0FF', outlineWidth = 0.15, layer = 1})
    end,
})
```

### graphics2d.drawStatic(batch, x, y, order)

Draws a static batch made by `batch:bake()`. The arguments `x` and `y` shift the whole batch without rebaking it and default to 0, which lets parallax layers reuse one batch. The table `order` is the fourth argument, so pass the offsets before it. A draw order in place of `x` raises a `number expected` error.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local clouds = graphics2d.newSpriteBatch(assets.texture('effects/dust.png'))
for index = 0, 9 do
    clouds:add({x = index * 180, y = 100, width = 64, height = 64, source = {0, 0, 64, 64}})
end
local baked = clouds:bake()
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawStatic(baked, camera.x * 0.5, 0, {layer = -1})
    end,
})
```

### graphics2d.pushClip(rect)

Restricts the following draws of the active canvas to `rect`, given in the canvas coordinates. Nested clips intersect with the clip around them. Clips end with `graphics2d.popClip` or when a new canvas begins.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.pushClip({100, 100, 300, 80})
        graphics2d.drawText(nil, 'This line is cut at the edge of the box', 100, 110, {size = 40})
        graphics2d.popClip()
    end,
})
```

### graphics2d.popClip()

Removes the clip added by the last `graphics2d.pushClip`. Calling it without a matching push raises `The "popClip" call has no matching "pushClip".`

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.pushClip({0, 0, 400, 400})
        graphics2d.drawCircle(400, 400, 200, '#FF4080FF')
        graphics2d.popClip()
        graphics2d.drawCircle(800, 400, 100, '#FFFF8040')
    end,
})
```

### graphics2d.pushLayerOffset(offset)

Adds `offset` to the layer of every following draw of the active canvas until `graphics2d.popLayerOffset`. Nested offsets add up, and a new canvas starts without offsets. It lets a group of draws that sets its own layers, such as a character with its shadow and its name, move to another layer as a whole. Calling it without an active canvas raises `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.`

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local function drawUnit(x, y)
    graphics2d.drawCircle(x, y + 30, 24, '#60000000', {layer = 0})
    graphics2d.drawRect({x - 16, y - 32, 32, 64}, '#FF4080FF', {layer = 1})
end

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        drawUnit(300, 300)
        -- The selected unit draws above every other unit, shadow included.
        graphics2d.pushLayerOffset(10)
        drawUnit(320, 310)
        graphics2d.popLayerOffset()
    end,
})
```

### graphics2d.popLayerOffset()

Removes the offset added by the last `graphics2d.pushLayerOffset`. Calling it without a matching push raises `The "popLayerOffset" call has no matching "pushLayerOffset".`

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.pushLayerOffset(5)
        graphics2d.drawRect({100, 100, 200, 200}, '#FFFF8040')
        graphics2d.popLayerOffset()
        graphics2d.drawRect({150, 150, 200, 200}, '#FF4080FF', {layer = 4})
    end,
})
```

### graphics2d.beginCapture(target, clear)

Starts a [capture](#captures): the world and screen canvases that begin until `graphics2d.endCapture` render into the render target `target`, cleared to `clear` first, transparent by default, instead of reaching the screen. The visible area covers the whole target, so give it the pixel size of the viewport to keep every pixel. Render target canvases begun during a capture still draw into their own targets. A capture can begin while another one is open, such as in a scene that a transition captures: the canvases that begin until it ends go into its own target, and the canvases after its end go back into the outer capture. A capture left open ends with the frame. The target holds colors premultiplied by their alpha, so draw its texture with `blend = 'premultiplied'` wherever it is not fully opaque, and the canvases of the capture cannot sample its texture, which raises `A draw cannot sample the render target that its canvas draws into.` An invalid target raises `A capture needs a valid render target.`

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local viewport = require('haylen.viewport')

local camera = graphics2d.newCamera()
local pixels = viewport.pixelRect()
local frame = graphics.newRenderTarget(math.floor(pixels.width), math.floor(pixels.height))

scene.push({
    render = function(self)
        graphics2d.beginCapture(frame, '#FF101820')
        graphics2d.beginWorld(camera, {ambientLight = '#FF404060'})
        graphics2d.drawRect({-300, -200, 600, 400}, '#FF3A7D44')
        graphics2d.drawLight({x = 0, y = 0, radius = 300, color = '#FFFFD080'})
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Captured', 60, 60, {size = 64})
        graphics2d.endCapture()

        -- The whole frame is one texture now, drawn here small and tilted.
        graphics2d.beginScreen()
        local area = graphics2d.canvasBounds()
        graphics2d.draw(frame.texture, area.x + area.width / 2, area.y + area.height / 2, {width = area.width / 2, height = area.height / 2, rotation = 0.1})
    end,
})
```

### graphics2d.endCapture()

Ends the capture begun by `graphics2d.beginCapture`, so the following canvases reach the screen again. Calling it without an open capture raises `The "endCapture" call has no matching "beginCapture".`

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local mirror = graphics.newRenderTarget(480, 270)

scene.push({
    render = function(self)
        graphics2d.beginCapture(mirror)
        graphics2d.beginScreen()
        graphics2d.drawCircle(960, 540, 300, '#FFFF8040')
        graphics2d.endCapture()

        graphics2d.beginScreen()
        graphics2d.draw(mirror.texture, 0, 0, {pivotX = 0, pivotY = 0, flipHorizontal = true})
    end,
})
```

### graphics2d.capturing()

Returns `true` while a capture is open.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local target = graphics.newRenderTarget(64, 64)

scene.push({
    render = function(self)
        graphics2d.beginCapture(target)
        print('capturing', graphics2d.capturing())
        graphics2d.endCapture()
        print('capturing', graphics2d.capturing())
    end,
})
```

### graphics2d.drawImageBlend(from, to, rect, options)

Draws the textures `from` and `to` over `rect`, mixed through a pattern that `progress` advances from `from` at 0 to `to` at 1. The shader-based scene transitions use it, and custom transitions and effects can too. Render target textures sample upright on every backend. Both textures are required, and `options` also takes the keys of [Draw order](#draw-order):

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `pattern` | string | `'dissolve'` | The pattern `'dissolve'` switches cells in random order with a soft edge, `'pixelate'` breaks `from` into growing blocks and brings `to` back out of them, `'radial'` sweeps a hand clockwise from twelve o'clock, `'iris'` closes a circle on `from` to the color and opens it on `to`, and `'pageTurn'` curls `from` away like a page. |
| `progress` | number | `0` | From 0 to 1. |
| `center` | Vec2 | `{0.5, 0.5}` | Center of `'radial'` and `'iris'`, from `{0, 0}` at the top-left of the rectangle to `{1, 1}` at its bottom-right. |
| `cellSize` | number | `1` | Size of the dissolve cells in pixels of `from`. |
| `blockSize` | number | `48` | Size of the largest pixelate blocks in pixels of `from`. |
| `color` | Color | `'#FF000000'` | The color the iris passes through. |
| `reversed` | boolean | `false` | Sweeps the radial pattern counterclockwise. |
| `angle` | number | `0` | Direction in radians the page turn moves toward, where 0 points right and `math.pi / 2` points down. |

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local day = graphics.newRenderTarget(320, 180)
local night = graphics.newRenderTarget(320, 180)
local time = 0

scene.push({
    update = function(self, dt)
        time = time + dt
    end,
    render = function(self)
        local camera = graphics2d.newCamera()
        camera.position = {160, 90}
        graphics2d.beginTarget(day, camera, {clear = '#FF87CEEB'})
        graphics2d.drawCircle(100, 60, 30, '#FFFFE060')
        graphics2d.beginTarget(night, camera, {clear = '#FF101830'})
        graphics2d.drawCircle(220, 50, 20, '#FFE0E0FF')

        graphics2d.beginScreen()
        local progress = (math.sin(time) + 1) / 2
        graphics2d.drawImageBlend(day.texture, night.texture, {100, 100, 640, 360}, {pattern = 'radial', progress = progress})
        graphics2d.drawImageBlend(day.texture, night.texture, {800, 100, 640, 360}, {pattern = 'dissolve', progress = progress, cellSize = 4})
        graphics2d.drawImageBlend(day.texture, night.texture, {100, 560, 640, 360}, {pattern = 'iris', progress = progress, center = {0.3, 0.4}, color = '#FFFFFFFF'})
        graphics2d.drawImageBlend(day.texture, night.texture, {800, 560, 640, 360}, {pattern = 'pageTurn', progress = progress, angle = math.pi})
    end,
})
```

### graphics2d.newSprite(texture, properties)

Creates a `Sprite`, a reusable description of one textured quad. The argument `properties` is an optional table of `Sprite` properties, each assigned through the property setter, so a misspelled key raises `The type "haylen.Sprite" has no writable property "name".`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local hero = graphics2d.newSprite(assets.texture('sprites/knight/idle.png'), {
    x = 300,
    y = 200,
    source = {0, 0, 192, 192},
    layer = 2,
})

scene.push({
    update = function(self, dt)
        hero.rotation = hero.rotation + dt
    end,
    render = function(self)
        graphics2d.beginScreen()
        hero:draw()
    end,
})
```

### graphics2d.newSpriteBatch(texture)

Creates a `SpriteBatch` that draws many sprites of one texture in a single batch. The sprite data stays in C++, and scripts change only the sprites that move.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local batch = graphics2d.newSpriteBatch(assets.texture('effects/dust.png'))
for index = 1, 100 do
    batch:add({x = (index % 10) * 70, y = (index // 10) * 70, width = 64, height = 64, source = {0, 0, 64, 64}})
end

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        batch:draw({layer = 1})
    end,
})
```

### graphics2d.newNineSlice(texture, options)

Creates a `NineSlice`. Give either `borders`, which cut `source` into nine regions, or `pieces`, which lists the nine regions directly:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `source` | Rect | whole texture | Region of the texture that holds the frame. Used with `borders`. |
| `borders` | table | required without `pieces` | Border sizes in pixels, as `{left, top, right, bottom}`. |
| `pieces` | table | none | Nine `Rect` values, row by row from the top-left corner to the bottom-right corner. |
| `fill` | string | `'stretch'` | The value `'stretch'` stretches the edges and the center. The value `'tile'` repeats them at their pixel size times the `borderScale` of the draw. |

Errors: `a nine-slice needs exactly nine pieces`, `borders need left, top, right and bottom`, `The borders of a nine-slice cannot be negative.`, `The borders of a nine-slice must fit inside its source rectangle, with the left and right borders at most its width and the top and bottom borders at most its height.`, and a `fill` other than `'stretch'` or `'tile'` raises an error that contains `unknown value 'name'`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local texture = assets.texture('ui/button.png')
local button = graphics2d.newNineSlice(texture, {borders = {45, 47, 45, 47}})
local tiled = graphics2d.newNineSlice(texture, {source = {0, 0, texture.width, texture.height}, borders = {45, 47, 45, 47}, fill = 'tile'})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawNineSlice(button, {100, 100, 400, 120})
        graphics2d.drawNineSlice(tiled, {100, 300, 400, 120})
    end,
})
```

### graphics2d.newCamera()

Creates a `Camera` at 0, 0 with zoom 1. Without a viewport its view always follows the current visible design area, so it stays right when the window is resized.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
camera.zoom = {2, 2}

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawCircle(0, 0, 20, '#FFFFFFFF')
    end,
})
```

### graphics2d.newParallax(texture, properties)

Creates a [`Parallax`](#parallax) layer that draws `texture`. The argument `properties` is an optional table of `Parallax` properties, each assigned through the property setter, so a misspelled key raises `The type "haylen.Parallax" has no writable property "name".`

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local stripes = graphics.newRenderTarget(256, 128)
local clouds = graphics2d.newParallax(stripes.texture, {scrollScale = {0.3, 0.3}, repeatX = true, autoscroll = {-20, 0}, color = '#C0FFFFFF'})

scene.push({
    update = function(self, dt)
        camera.x = camera.x + 120 * dt
        clouds:update(dt)
    end,
    render = function(self)
        local stripeCamera = graphics2d.newCamera()
        stripeCamera.position = {128, 64}
        graphics2d.beginTarget(stripes, stripeCamera, {clear = '#00000000'})
        graphics2d.drawCircle(80, 64, 50, '#FFFFFFFF')
        graphics2d.drawCircle(150, 70, 40, '#FFFFFFFF')

        graphics2d.beginWorld(camera)
        clouds:draw(camera, {layer = -1})
        graphics2d.drawRect({-2000, 100, 4000, 400}, '#FF3A7D44')
    end,
})
```

### graphics2d.blendCameras(from, to, amount)

Returns a new `Camera` between the views of `from` and `to`, where `amount` 0 gives the view of `from` and 1 the view of `to`. Position, offset, rotation and viewport move in a straight line, and the zoom changes at a steady rate. The result takes the other settings of `to`. Tween `amount` for a smooth cut between two cameras, such as from the player to a boss.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local tween = require('haylen.tween')

local player = graphics2d.newCamera()
local boss = graphics2d.newCamera()
boss.position = {1200, 300}
boss.zoom = {0.5, 0.5}
local cut = {amount = 0}
tween.to(cut, 2, {amount = 1}, {ease = 'quadInOut'})

scene.push({
    render = function(self)
        graphics2d.beginWorld(graphics2d.blendCameras(player, boss, cut.amount))
        graphics2d.drawCircle(0, 0, 40, '#FF40A0FF')
        graphics2d.drawCircle(1200, 300, 120, '#FFFF4040')
    end,
})
```

### graphics2d.canvasBounds()

Returns a `Rect` with the area the active canvas shows, in its own coordinates. For a world canvas this is the part of the world the camera sees. Without an active canvas it returns an empty rectangle at 0, 0.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        local bounds = graphics2d.canvasBounds()
        graphics2d.drawRectOutline(bounds, 8, '#FFFF0000')
    end,
})
```

### graphics2d.canvasUnitSize()

Returns the length in the coordinates of the active canvas of one unit of its destination, a design unit for world canvases and a pixel for render target canvases, which is `1 / zoom` for a camera without rotation. Outlines and markers multiplied by it keep the same thickness at any zoom. Calling it without an active canvas raises `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.`

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
camera.zoom = {3, 3}

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawRect({-50, -50, 100, 100}, '#FF3A7D44')
        -- Two design units thick whatever the zoom.
        graphics2d.drawRectOutline({-50, -50, 100, 100}, 2 * graphics2d.canvasUnitSize(), '#FFFFFFFF')
    end,
})
```

### graphics2d.canvasLit()

Returns `true` when the active canvas is lit, a world or render target canvas with `ambientLight`, which is the only kind of canvas that takes lights and occluders. Effects that bring their own light draw it only then. Calling it without an active canvas raises `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.`

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local night = true

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, night and {ambientLight = '#FF202040'} or nil)
        graphics2d.drawCircle(0, 0, 40, '#FFFFA040')
        if graphics2d.canvasLit() then
            graphics2d.drawLight({x = 0, y = 0, radius = 200, color = '#FFFFB060'})
        end
    end,
})
```

### graphics2d.stats()

Returns a table with the renderer counters of the current frame. The draw counters grow while the frame is recorded, and the GPU counters are filled when the frame is submitted, so read them in `update` to see the totals of the previous frame.

| Key | Meaning |
| --- | --- |
| `canvases` | Canvases begun. |
| `passes` | GPU render passes. |
| `drawCalls` | GPU draw calls. |
| `sprites` | Quads submitted: sprites, batch sprites, glyphs and particles. |
| `instances` | Sprite instances uploaded to the GPU. |
| `vertices` | Mesh and shape vertices uploaded. |
| `indices` | Mesh and shape indices uploaded. |
| `lights` | Lights drawn. |
| `occluders` | Occluders drawn. |
| `shadows` | Shadow maps cast, one per light with shadows. |
| `textureSwitches` | Texture changes between draw calls. |
| `uploadedBytes` | Bytes uploaded to GPU buffers and to the textures that change in place, such as the atlases of fonts and of the UI and dynamic textures. |

```lua
local graphics2d = require('haylen.graphics2d')
local log = require('haylen.log')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        local stats = graphics2d.stats()
        log.debug(stats.drawCalls .. ' draw calls, ' .. stats.sprites .. ' sprites')
    end,
})
```

### graphics2d.drawn()

Returns the textured quads and blocks of text that the open canvas holds so far, in the order they were drawn, as a list of tables with `x`, `y`, `width` and `height`, the bounds in the coordinates of the canvas, `text`, which is `true` for a block of text, and `label`, the path of the texture of a sprite on the first quad of each draw. Every sprite and every quad of a batch or a nine-slice is one entry, a baked batch is one entry, and every text draw is one block, while plain shapes such as rectangles and lines and the draws with a distortion are left out. The bounds of a block of text reach a little past its letters, by the spread of the distance field of its glyphs. It suits checks and tools that need to know where things draw, such as the bounds the debug drawings outline. It needs an open canvas, and without one it raises `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.`.

```lua
local graphics2d = require('haylen.graphics2d')
local log = require('haylen.log')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Score 120', 40, 40, {size = 48})
        for _, item in ipairs(graphics2d.drawn()) do
            log.debug(string.format('%s at %.0f, %.0f', item.text and 'Text' or item.label, item.x, item.y))
        end
    end,
})
```

### graphics2d.lightTexture()

Returns the 128 by 128 radial falloff `Texture` that `graphics2d.drawLight` uses when a light has no texture of its own. It is white with an alpha that fades from the center to the edge, which also suits glows and soft shadows.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local glow = graphics2d.newSprite(graphics2d.lightTexture(), {x = 960, y = 540, width = 400, height = 400, color = '#80FFD080', blend = 'additive'})

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        glow:draw()
    end,
})
```

### graphics2d.hdrLighting()

Tells whether light maps hold light in floating point, which lets lights brighter than 1 brighten the scene beyond its unlit colors. It is `true` where the backend renders and blends floating-point targets, and otherwise light saturates at 1.

```lua
local graphics2d = require('haylen.graphics2d')

local glare = graphics2d.hdrLighting() and 3 or 1
print('the sun shines with intensity ' .. glare)
```

### graphics2d.newMaterial(shader, uniforms)

Creates a [`Material`](#material) that shades draws with `shader`, a `Shader` that [`haylen.assets`](assets.md) loads from a `.shader` file compiled by `haylen.py shaders`, as the [shader guide](../shaders.md) explains. The argument `uniforms` is an optional table of initial values by name, which `material:set` takes one by one. A shader without the programs of the shader library raises `The shader "name" has no "sprite" program.`

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local ripple = graphics2d.newMaterial(assets.shader('shaders/ripple.shader'), {strength = 0.02, tint = '#FFC0E0FF'})
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        ripple:set('time', haylen.elapsed())
        graphics2d.beginWorld(camera, {clear = '#FF203040', postProcess = {materials = {ripple}}})
        graphics2d.drawRect({-200, -100, 400, 200}, '#FF3A7D44')
        graphics2d.draw(graphics.whiteTexture(), 0, 0, {width = 64, height = 64, material = ripple})
    end,
})
```

### graphics2d.defaultFont()

Returns the `Font` the engine uses when a text function receives `nil` as its font.

```lua
local graphics2d = require('haylen.graphics2d')

local font = graphics2d.defaultFont()
print(font:lineHeight(24))
```

## Sprite

A `Sprite` is a value that describes one quad and draws it with `sprite:draw()`. The function `graphics2d.newSprite` creates it. Vector and color properties return copies, so assign a new value instead of changing a field of the returned value.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | the constructor argument | Texture to draw. |
| `x`, `y` | number | `0` | Position of the pivot. |
| `position` | Vec2 | `{0, 0}` | The same position as a vector. |
| `width`, `height` | number | `0` | Drawn size before scaling. When both are 0 the size of the source is used. |
| `scaleX`, `scaleY` | number | `1` | Multiply the drawn size. |
| `pivotX`, `pivotY` | number | `0.5` | Pivot as a fraction of the size. |
| `rotation` | number | `0` | Rotation in radians. |
| `color` | Color | `'#FFFFFFFF'` | Multiplies the texture color. |
| `flash` | Color | `'#00000000'` | Mixes the result toward this color by its alpha. |
| `source` | Rect | empty | Region of the texture in pixels. An empty rectangle covers the whole texture. |
| `flipHorizontal`, `flipVertical` | boolean | `false` | Mirror the image. |
| `flipDiagonal` | boolean | `false` | Mirror the image across its diagonal from the top-left corner. |
| `layer`, `depth`, `sortOffset`, `visibility`, `blend` | | `0`, `0`, `0`, `1`, `'alpha'` | [Draw order](#draw-order). |
| `material` | Material or nil | nil | Custom shader of the sprite. |
| `partMask` | Texture or nil | nil | The mask that [recolors](#recoloring-by-parts) the parts of the sprite. |
| `partColors` | table | white parts | The colors of the parts, as `{red = Color, green = Color, blue = Color, yellow = Color}` after the colors of the mask, each white when the table leaves it out. Reading it returns all four. |
| `effect` | table | no effect | The [sprite effect](#sprite-effects), assigned as a table with any of its keys. Reading it returns every key. |
| `normalMap`, `specular`, `shininess`, `emission`, `lightMask`, `unshaded` | | nil, `0`, `32`, `0`, `1`, `false` | Lighting in lit canvases, as the [draw order](#draw-order) describes. |
| `distortion` | number | `0` | Bends the image of the canvas instead of drawing colors, as [distortion](#distortion) explains. |

Reading or writing any other key raises `The type "haylen.Sprite" has no member "name".` or `The type "haylen.Sprite" has no writable property "name".`

### sprite:draw()

Draws the sprite in the active canvas with its own draw order.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local sprite = graphics2d.newSprite(assets.texture('sprites/knight/idle.png'), {source = {0, 0, 192, 192}})
sprite.position = {500, 300}
sprite.flipHorizontal = true
sprite.color = '#FFFFC0C0'

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        sprite:draw()
    end,
})
```

## SpriteBatch

A `SpriteBatch` holds many sprites that share one texture. Indices count from 1. Each sprite is a table with these keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Position of the pivot. |
| `width`, `height` | number | `0` | Drawn size. When both are 0 the size of the source is used, which is the whole texture without a source, like `graphics2d.draw`. |
| `source` | Rect | whole texture | Region of the texture in pixels. |
| `pivotX`, `pivotY` | number | `0.5` | Pivot as a fraction of the size. |
| `rotation` | number | `0` | Rotation in radians. |
| `color` | Color | `'#FFFFFFFF'` | Multiplies the texture color. |
| `flash` | Color | `'#00000000'` | Mixes the result toward this color by its alpha. |
| `flipHorizontal`, `flipVertical` | boolean | `false` | Mirror the image. |
| `flipDiagonal` | boolean | `false` | Mirror the image across its diagonal from the top-left corner. |
| `partColors` | table | white parts | The colors of the parts of the sprite when the batch draws with a `partMask`, as [recoloring by parts](#recoloring-by-parts) explains. A batch keeps room for part colors only once a sprite has some. |

### Sprite fields

Float buffers name the numbers of a sprite with these fields: `x`, `y`, `width`, `height`, `rotation`, `pivotX`, `pivotY`, the color channels `red`, `green`, `blue` and `alpha` from 0 to 1, and the source rectangle `sourceX`, `sourceY`, `sourceWidth` and `sourceHeight` in texture pixels. An unknown name raises an error that ends with `(unknown value '<name>')`, and an empty list raises `A sprite layout needs at least one field.`.

### batch:add(sprite)

Appends a sprite and returns its index.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
local index = batch:add({x = 10, y = 10, width = 4, height = 4, color = '#FFFF0000'})
```

### batch:set(index, fields)

Changes the sprite at `index`. Only the keys present in `fields` change, the rest keep their values. An index outside the batch raises `sprite index out of range`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
local index = batch:add({x = 0, y = 0, width = 8, height = 8})
batch:set(index, {x = 120, rotation = 0.5})
```

### batch:get(index)

Returns a copy of the sprite at `index` as a sprite table with every key of the table above. A sprite added without a size reports the size it draws with. An index outside the batch raises `sprite index out of range`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
local index = batch:add({x = 10, y = 20, width = 8, height = 8, flipHorizontal = true})
local sprite = batch:get(index)
print(sprite.x, sprite.width, sprite.flipHorizontal, sprite.color:toHex())
```

### batch:reserve(count)

Makes room for `count` sprites at once, so a batch filled in a loop does not grow step by step.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:reserve(1000)
for index = 1, 1000 do
    batch:add({x = index, y = 0, width = 1, height = 1})
end
```

### batch:remove(index)

Removes the sprite at `index`. The sprites after it move down by one index. An index outside the batch raises `sprite index out of range`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:add({x = 0, y = 0, width = 8, height = 8})
batch:add({x = 20, y = 0, width = 8, height = 8})
batch:remove(1)
print(batch:size())
```

### batch:clear()

Removes every sprite.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:add({x = 0, y = 0, width = 8, height = 8})
batch:clear()
```

### batch:size()

Returns the number of sprites.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
print(batch:size())
```

### batch:resize(count, sprite)

Sets the number of sprites to `count` in one call. New sprites copy the sprite table `sprite`, or take the size of the texture without one, and sprites past `count` go away. It is the fast way to fill a large batch that a float buffer then moves.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:resize(10000, {width = 4, height = 4, color = '#FF80C0FF'})
print(batch:size()) -- 10000
```

### batch:writeFields(buffer, fields, first)

Copies the [sprite fields](#sprite-fields) named in the list `fields` from a float buffer into the sprites, from the sprite `first`, which defaults to 1, on. Each sprite takes one value for each field, in order, and the copy stops at the end of the buffer or of the batch. The other fields of the sprites keep their values, so a batch built once moves every frame with one call. A `first` past the end raises `Sprite batch index is out of range.`.

```lua
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local count = 20000
local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:resize(count, {width = 2, height = 2})
local positions = {}
for index = 1, count * 2 do
    positions[index] = math.random(0, 1080)
end
local buffer = collections.newFloatBuffer(count * 2)
local fields = {'x', 'y'}

scene.push({
    update = function(self, dt)
        for slot = 2, count * 2, 2 do
            positions[slot] = (positions[slot] + 30 * dt) % 1080
        end
        buffer:set(1, positions)
        batch:writeFields(buffer, fields)
    end,
    render = function(self)
        graphics2d.beginScreen()
        batch:draw()
    end,
})
```

### batch:readFields(buffer, fields, first)

Copies the named fields of the sprites into a float buffer, the other way around from `writeFields`.

```lua
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:add({x = 10, y = 20, width = 4, height = 4})
local buffer = collections.newFloatBuffer(2)
batch:readFields(buffer, {'x', 'y'})
print(buffer[1], buffer[2]) -- 10.0 20.0
```

### batch:draw(order)

Draws every sprite in the active canvas as one batch. The argument `order` is an optional draw order table. Large batches convert their sprites on worker threads.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local stars = graphics2d.newSpriteBatch(graphics.whiteTexture())
for index = 1, 500 do
    stars:add({x = math.random(0, 1920), y = math.random(0, 1080), width = 2, height = 2})
end

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        stars:draw({blend = 'additive'})
    end,
})
```

### batch:bake()

Copies the sprites into an immutable `StaticSpriteBatch` on the GPU, which draws every frame without uploading sprite data again. Later changes to the batch do not affect the baked copy. An empty batch raises `A static batch needs a texture and at least one sprite.`

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local floor = graphics2d.newSpriteBatch(graphics.whiteTexture())
for column = 0, 31 do
    floor:add({x = column * 64, y = 900, width = 62, height = 62, pivotX = 0, pivotY = 0, color = '#FF6A5030'})
end
local baked = floor:bake()

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawStatic(baked)
    end,
})
```

## Sprite effects

The `effect` of a sprite dissolves it into noise with a colored edge and draws an outline and a glow around its visible pixels, all on the GPU, so sprites of one texture share a draw call whatever their effects. The quad of the sprite grows by the outline and the glow, which spill past its frame. The table takes these keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `dissolve` | number | `0` | How much of the sprite has dissolved, from 0 for none to 1 for all of it. The outline and the glow fade with it. |
| `dissolveEdge` | number | `0.08` | The share of the noise along the dissolving border that takes the dissolve color, from 0 to 1. |
| `dissolveSize` | number | `6` | The size of the cells of the noise in pixels of the texture, from 1, which dissolves pixel by pixel, to 64. |
| `dissolveColor` | Color | `'#00000000'` | Color of the dissolving border, such as glowing embers. |
| `outlineWidth` | number | `0` | Width of the outline in pixels of the texture, from 0 to 64. |
| `outlineColor` | Color | `'#FFFFFFFF'` | Color of the outline. |
| `glowSize` | number | `0` | How far the glow reaches in pixels of the texture, from 0 to 64. |
| `glowColor` | Color | `'#FFFFFFFF'` | Color of the glow, whose alpha sets its strength. |

The `color` and `flash` of the sprite apply to the sprite itself, before the outline and the glow. Values out of range raise `A sprite effect needs a dissolve and a dissolve edge from 0 to 1, a dissolve size from 1 to 64 and an outline width and a glow size from 0 to 64.`, an effect with a `partMask` raises `A sprite takes a part mask or an effect, not both.`, and one with a material raises `A sprite with an effect does not take a material.` Sprite batches and baked batches draw without effects.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local hero = graphics2d.newSprite(assets.texture('sprites/hero.png', {filter = 'linear'}), {x = -200, y = 0})
local dissolve = 0

scene.push({
    update = function(self, dt)
        dissolve = (dissolve + dt * 0.4) % 1
        hero.effect = {dissolve = dissolve, dissolveColor = '#FFFF8020', dissolveEdge = 0.1}
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        hero:draw()
        graphics2d.draw(hero.texture, 200, 0, {effect = {outlineWidth = 3, outlineColor = '#FFFFFFFF', glowSize = 16, glowColor = '#C040C0FF'}})
    end,
})
```

## Distortion

A draw with a `distortion` above 0 bends the image of its canvas instead of drawing colors. Its coverage, the alpha of its texture times its color and the distortion, adds up in the distortion map of the canvas, and the composite moves every pixel down the slope of the map, by the `distortion` of the post-processing options where the coverage goes from nothing to full within 8 units. So a ring pushes the image outward at its outer edge and inward at its inner edge like a shock wave, a soft dot bends the image around its edge like a lens, and soft blobs that rise and fade shimmer like heat above a fire. Only world and render target canvases with lighting or post-processing have the map, and other canvases skip distortion draws, which only sprites, sprite batches, nine-slices, rectangles, lines, shapes and meshes make. Other draws raise `Only sprites, sprite batches, nine-slices, shapes and meshes draw as distortion.`, and `graphics2d.drawStatic` raises `A baked sprite batch draws without a distortion. Draw a sprite batch to distort with its sprites.` Particle emitters take the same `distortion` key.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local ring = assets.texture('effects/ring.png', {filter = 'linear'})
local background = assets.texture('backgrounds/town.png', {filter = 'linear'})
local time = 0

scene.push({
    update = function(self, dt)
        time = (time + dt) % 1
    end,
    render = function(self)
        graphics2d.beginWorld(camera, {postProcess = {distortion = 32}})
        graphics2d.draw(background, 0, 0)
        local size = 100 + time * 900
        graphics2d.draw(ring, 0, 0, {width = size, height = size, color = {1, 1, 1, 1 - time}, distortion = 1})
    end,
})
```

## Recoloring by parts

A white or greyscale sprite drawn with a `partMask` takes a color for each part the mask marks, which keeps the shading of the sprite, so one set of images makes every outfit of a character. The mask has the layout of the texture of the sprite and paints each part in red, green, blue or yellow, such as the hat, the shirt, the trousers and the boots, and the `partColors` of the sprite, or of each sprite of a batch, name the color of each mask color. A part takes its color multiplied by the shading of the sprite and mixed in by the alpha of the color, so white or a transparent color keeps a part as it is, yellow counts as red and green together, the alpha of the mask fades parts at their soft edges, and the `color` and `flash` of the sprite apply over the result. The GPU does all the work per pixel, and sprites of one texture and mask share a draw call whatever their colors. The keys `partMask` and `partColors` work with `sprite:draw()`, `graphics2d.draw`, `batch:draw` and `graphics2d.drawBatch` with sprite tables, while a material raises `A draw with a part mask does not take a material.` and `graphics2d.drawStatic` raises `A baked sprite batch draws without a part mask. Draw a sprite batch to recolor its sprites.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local base = assets.texture('characters/villager.png', {filter = 'linear'})
local mask = assets.texture('characters/villager_mask.png', {filter = 'linear'})
local crowd = graphics2d.newSpriteBatch(base)
local outfits = {
    {red = '#FFC0392B', green = '#FF2E86C1', blue = '#FF1E8449', yellow = '#FF6E2C00'},
    {red = '#FFF4D03F', green = '#FF8E44AD', blue = '#FF34495E', yellow = '#FF784212'},
}
for index = 1, 40 do
    crowd:add({x = 60 + (index % 10) * 90, y = 120 + (index // 10) * 140, partColors = outfits[index % #outfits + 1]})
end
local hero = graphics2d.newSprite(base, {x = 960, y = 700, partMask = mask, partColors = {red = '#FFFFFFFF', green = '#FF000000'}})

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        crowd:draw({partMask = mask})
        hero:draw()
    end,
})
```

## StaticSpriteBatch

A `StaticSpriteBatch` is the GPU copy made by `batch:bake()` and drawn with `graphics2d.drawStatic`.

### staticBatch:size()

Returns the number of sprites in the batch.

### staticBatch:bounds()

Returns a `Rect` that covers every sprite before rotation.

### staticBatch:texture()

Returns the `Texture` the batch draws with.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:add({x = 100, y = 100, width = 50, height = 50})
local baked = batch:bake()
print(baked:size(), baked:bounds().width, baked:texture() == graphics.whiteTexture())

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawStatic(baked)
    end,
})
```

## Camera

A `Camera` shows a part of the world for `graphics2d.beginWorld` and `graphics2d.beginTarget`. The function `graphics2d.newCamera` creates it. Its `position` is the point of the world it shows, and `camera:follow` moves that position toward a target through the dead zone, the drag margins, the look-ahead, the smoothing and the limits, while `camera:update` advances the shake and the rotation smoothing. Vector and rectangle properties return copies, so assign a new value instead of changing a field of the returned one.

The methods measure the view with the screen, the visible design area of `viewport.visibleRect()`, and a `viewport` is a rectangle of that screen in design coordinates. Screen points, such as the pointer positions of [`haylen.input`](input.md), are in the same design coordinates.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `position` | Vec2 | `{0, 0}` | The point of the world the view shows at its anchor. |
| `x`, `y` | number | `0` | The position components. |
| `offset` | Vec2 | `{0, 0}` | Moves the view in world units after the limits, so it can show past them, for a look around or a cutscene nudge. |
| `anchor` | string | `'center'` | The value `'center'` puts the position at the center of the view, which zooms and rotates around it. The value `'topLeft'` puts it at the top-left corner, and following then ignores the dead zone and the drag margins. |
| `zoom` | Vec2 | `{1, 1}` | View scale on each axis. 2 shows half the area at twice the size. It stays between `minZoom` and `maxZoom`, and an axis that is not a number raises `The camera zoom must be a number.` |
| `minZoom`, `maxZoom` | number | `0.05`, `20` | Zoom limits of every zoom change. The smallest zoom must be above 0 and not above the largest, otherwise the assignment raises `The smallest zoom must be above 0 and not above the largest zoom.` or `The largest zoom must not be below the smallest zoom.` |
| `rotation` | number | `0` | View rotation in radians. |
| `ignoreRotation` | boolean | `false` | Keeps the view upright, ignoring `rotation` and the rotation of the shake. |
| `viewport` | Rect or nil | `nil` | The part of the screen the camera draws into, in design coordinates, for split screens and minimaps. The value `nil` covers the whole visible area. |
| `limits` | Rect or nil | `nil` | World area the view stays inside while following or clamping. A limit smaller than the view centers the view on it. |
| `limitSmoothing` | boolean | `false` | With position smoothing, eases the view into the limits instead of stopping it at them. |
| `positionSmoothing` | boolean | `false` | Eases the position toward the target at `positionSmoothingSpeed`, and the zoom of `camera:frame` too. |
| `positionSmoothingSpeed` | number | `5` | Rate of the position smoothing per second. |
| `rotationSmoothing` | boolean | `false` | Eases the drawn rotation toward `rotation` at `rotationSmoothingSpeed`, the short way around. |
| `rotationSmoothingSpeed` | number | `5` | Rate of the rotation smoothing per second. |
| `deadZone` | Vec2 | `{0, 0}` | Width and height in world units of the area around the view center where the target moves without moving the view. |
| `dragHorizontal`, `dragVertical` | boolean | `false` | Let the target move inside the drag margins on that axis before the view follows. |
| `dragMargins` | table | `{0.2, 0.2, 0.2, 0.2}` | How far the target moves from the view center on each side, in fractions of half the view, as `{left, top, right, bottom}` or `{left = 0.2, ...}`. Reading it returns a table with named fields. |
| `dragOffset` | Vec2 | `{0, 0}` | Where the target rests on an axis that does not drag, from -1 on the right or bottom margin to 1 on the left or top margin, such as ahead of a character that faces right. |
| `lookAheadTime` | number | `0` | Seconds of target velocity the view leads the target by. |
| `maxLookAhead` | number | `200` | Largest look-ahead distance in world units. |
| `lookAheadSmoothingSpeed` | number | `4` | Rate at which the look-ahead follows the velocity, so a sudden stop does not jerk the view back. |
| `trauma` | number | `0` | Shake amount from 0 to 1, which decays over time. Assigning it sets the trauma directly. |
| `maxShakeOffset` | number | `24` | Shake offset in world units at full trauma. |
| `maxShakeAngle` | number | `0.05` | Shake rotation in radians at full trauma. |
| `shakeFrequency` | number | `25` | How fast the shake noise moves. Lower values sway, higher values rattle. |
| `traumaDecay` | number | `1.5` | Trauma lost per second. |
| `pixelSnap` | boolean | `false` | Rounds the drawn position to whole view pixels at the current zoom, which keeps pixel art from shimmering. |
| `viewSize` | Vec2 | visible design size | Read only. The size of the viewport, or of the visible design area without one, at zoom 1. |

A render target canvas measures the view with the target instead, so for a camera used with `graphics2d.beginTarget` the methods describe the screen view unless the camera has a viewport.

### camera:follow(x, y, dt)

Moves the camera toward the target `x`, `y` for a frame of `dt` seconds. The goal of the view stays put while the target moves inside the dead zone and the drag margins, leads the target by its velocity with the look-ahead, rests at the drag offset on axes that do not drag, and the position eases toward the goal with position smoothing and stays inside the limits.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
camera.limits = {0, 0, 3584, 2304}
camera.positionSmoothing = true
camera.positionSmoothingSpeed = 6
camera.dragHorizontal = true
camera.dragMargins = {0.3, 0.2, 0.3, 0.2}
camera.lookAheadTime = 0.3
local player = {x = 400, y = 300}
camera:snapTo(player.x, player.y)

scene.push({
    update = function(self, dt)
        player.x = player.x + 200 * dt
        camera:follow(player.x, player.y, dt)
        camera:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawCircle(player.x, player.y, 24, '#FFFFFFFF')
        camera:drawDebug({layer = 10})
    end,
})
```

### camera:frame(points, padding, dt)

Follows the middle of a list of points and zooms until they fit inside the view with `padding` world units around them, within the zoom limits, for games that keep several players or a player and a boss in view. The zoom eases like the position when position smoothing is on. An empty list raises `Framing needs at least one point.`

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
camera.positionSmoothing = true
camera.minZoom = 0.25
camera.maxZoom = 2
local players = {{x = 0, y = 0}, {x = 600, y = 200}}
local time = 0

scene.push({
    update = function(self, dt)
        time = time + dt
        players[2].x = 600 + math.sin(time) * 900
        camera:frame({{players[1].x, players[1].y}, {players[2].x, players[2].y}}, 120, dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        for _, player in ipairs(players) do
            graphics2d.drawCircle(player.x, player.y, 32, '#FF40A0FF')
        end
    end,
})
```

### camera:update(dt)

Decays the trauma by `traumaDecay` per second, computes the shake of this frame, fades the flash and eases the drawn rotation with rotation smoothing. Call it once per frame.

### camera:addTrauma(amount)

Adds shake that moves in every direction. Trauma stays between 0 and 1, and the shake grows with the square of the trauma, so small hits stay subtle.

### camera:shake(amount, dx, dy)

Adds trauma that shakes the view only back and forth along the direction `dx`, `dy`, without turning it, until the trauma settles, such as a recoil along the line of a shot.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local timer = require('haylen.timer')

local camera = graphics2d.newCamera()
camera.maxShakeOffset = 16
camera.shakeFrequency = 18
timer.every(2, function() camera:addTrauma(0.6) end)
timer.every(3, function() camera:shake(0.8, 1, 0) end)

scene.push({
    update = function(self, dt)
        camera:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawRect({-100, -100, 200, 200}, '#FFA04040')
        graphics2d.drawText(nil, string.format('trauma %.2f', camera.trauma), -100, 120)
    end,
})
```

### camera:snapTo(x, y)

Moves the view to `x`, `y` at once, inside the limits, without smoothing or look-ahead, such as after a teleport or when a level starts.

### camera:resetSmoothing()

Puts the view where following wants it now, without easing the position or the rotation.

### camera:align()

Recenters following on the last target, as if the target had just been reached, and lets the smoothing ease the view there, such as when the drag margins should stop holding the view back.

### camera:clampToLimits()

Moves the camera so its view stays inside `limits`. Without limits it does nothing.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera.limits = {0, 0, 2000, 1200}
camera.positionSmoothing = true
camera:follow(900, 600, 0.016)
camera:resetSmoothing()
camera:align()
camera:snapTo(-500, -500)
print(camera.x, camera.y)
camera.position = {5000, 0}
camera:clampToLimits()
print(camera.x, camera.y)
```

### camera:zoomAt(factor, x, y)

Multiplies the zoom by `factor`, within the zoom limits, and keeps the world point under the screen point `x`, `y` in place, which is how the mouse wheel and pinch gestures zoom toward the pointer. A factor that is not a number raises `The camera zoom must be a number.`

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
camera.minZoom = 0.5
camera.maxZoom = 4

scene.push({
    event = function(self, event)
        if event.type == 'mouseScroll' then
            local x, y = input.mousePosition()
            camera:zoomAt(event.scrollY > 0 and 1.1 or 1 / 1.1, x, y)
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawRect({-400, -300, 800, 600}, '#FF3A7D44')
    end,
})
```

### camera:shakeOffset()

Returns the shake offset of this frame as a `Vec2`, computed by `camera:update`. It is zero without trauma.

### camera:flash(color, duration)

Covers the view with `color`, whose alpha fades out over `duration` seconds as `camera:update` advances it, fast at first and slowly at the end, such as a white flash on a hit or a red one on damage. Every world canvas of the camera draws it over what it holds, unshaded, and a new flash replaces the one that is fading.

### camera:flashColor()

Returns the color the flash covers the view with now, transparent once it faded.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        if input.keyPressed('space') then
            camera:flash('#C0FFFFFF', 0.3)
            camera:addTrauma(0.5)
        end
        camera:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawRect({-200, -100, 400, 200}, '#FF3A7D44')
    end,
})
```

### camera:renderPosition()

Returns the position the view is drawn from as a `Vec2`: the position plus the offset and the shake, rounded to whole view pixels when `pixelSnap` is `true`.

### camera:renderRotation()

Returns the rotation the view is drawn with: the rotation, eased with rotation smoothing, plus the shake rotation, or 0 with `ignoreRotation`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
camera.rotationSmoothing = true
camera.rotation = 0.3
camera:addTrauma(0.8)

scene.push({
    update = function(self, dt)
        camera:update(dt)
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        local offset = camera:shakeOffset()
        local position = camera:renderPosition()
        graphics2d.drawText(nil, string.format('shake %.1f, %.1f at %.1f, %.1f turning %.3f', offset.x, offset.y, position.x, position.y, camera:renderRotation()), 40, 40)
    end,
})
```

### camera:viewTransform()

Returns the `Transform2D` from [`haylen.math`](math.md) that maps world coordinates to view coordinates, where 0, 0 is the top-left corner of the view, including zoom, rotation, offset and shake. Its inverse maps view coordinates back to the world.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera.position = {100, 50}
camera.zoom = {2, 2}
local transform = camera:viewTransform()
local view = transform:apply({100, 50})
local world = transform:inverse():apply(view)
print(view.x, view.y, world.x, world.y)
```

### camera:visibleBounds()

Returns a `Rect` with the world area the view covers, including zoom, rotation, offset and shake.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera.zoom = {2, 2}
local visible = camera:visibleBounds()
print(visible.x, visible.y, visible.width, visible.height)
```

### camera:worldToScreen(x, y)

Converts a world position to a screen point in design coordinates and returns `x, y`.

### camera:screenToWorld(x, y)

Converts a screen point in design coordinates, such as the pointer position, to a world position and returns `x, y`.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        local worldX, worldY = camera:screenToWorld(input.mousePosition())
        graphics2d.drawCircle(worldX, worldY, 8, '#FFFFFF00')

        local screenX, screenY = camera:worldToScreen(0, 0)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Origin', screenX, screenY)
    end,
})
```

### camera:drawDebug(order)

Draws the view as the screen shows it, the limits and the box where the target moves without moving the view, with a cross on the last target, in world coordinates, so it belongs in a world canvas, usually one of a zoomed-out camera or of the camera itself. The argument `order` is an optional [Draw order](#draw-order).

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
camera.limits = {-600, -400, 1200, 800}
camera.deadZone = {200, 120}
local overview = graphics2d.newCamera()
overview.zoom = {0.4, 0.4}
overview.viewport = {1500, 20, 400, 240}
local time = 0

scene.push({
    update = function(self, dt)
        time = time + dt
        camera:follow(math.cos(time) * 500, math.sin(time) * 300, dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawRect({-600, -400, 1200, 800}, '#FF3A7D44')
        graphics2d.beginWorld(overview, {order = 1})
        graphics2d.drawRect({-600, -400, 1200, 800}, '#FF3A7D44')
        camera:drawDebug()
    end,
})
```

### Split screen and minimaps

Every camera with a viewport draws into its own part of the screen, lit and post-processed canvases included, so one frame draws the world once per camera. Visibility bits leave details out of a minimap.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local viewport = require('haylen.viewport')

local left = graphics2d.newCamera()
local right = graphics2d.newCamera()
local minimap = graphics2d.newCamera()
minimap.zoom = {0.1, 0.1}
local players = {{x = -300, y = 0}, {x = 300, y = 100}}

local function drawWorld()
    graphics2d.drawRect({-3000, -2000, 6000, 4000}, '#FF3A7D44')
    for _, player in ipairs(players) do
        graphics2d.drawCircle(player.x, player.y, 32, '#FFFFFFFF')
        graphics2d.drawText(nil, 'player', player.x, player.y - 60, {size = 24, visibility = 2})
    end
end

scene.push({
    update = function(self, dt)
        local area = viewport.visibleRect()
        left.viewport = {area.x, area.y, area.width / 2, area.height}
        right.viewport = {area.x + area.width / 2, area.y, area.width / 2, area.height}
        minimap.viewport = {area.x + area.width / 2 - 150, area.y + 20, 300, 180}
        left:follow(players[1].x, players[1].y, dt)
        right:follow(players[2].x, players[2].y, dt)
        players[1].x = players[1].x - 60 * dt
    end,
    render = function(self)
        graphics2d.beginWorld(left, {ambientLight = '#FFB0B0C0'})
        drawWorld()
        graphics2d.drawLight({x = players[1].x, y = players[1].y, radius = 300})
        graphics2d.beginWorld(right)
        drawWorld()
        graphics2d.beginWorld(minimap, {order = 1, visibilityMask = 1})
        drawWorld()
    end,
})
```

## Parallax

A `Parallax` is a layer that scrolls at its own rate as the camera moves, which suggests depth, for any texture and outside Tiled maps too. The function `graphics2d.newParallax` creates it. It draws its texture at its position moved by its offset, and repeats it on the axes that repeat until it covers the part of the world the camera shows. Vector and rectangle properties return copies.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | the constructor argument | Texture to draw. |
| `source` | Rect | empty | Region of the texture in pixels. An empty rectangle covers the whole texture. |
| `position` | Vec2 | `{0, 0}` | Where the layer sits in the world while the camera shows the world origin. |
| `size` | Vec2 | `{0, 0}` | Drawn size of one copy, or the size of the source when zero. |
| `scrollScale` | Vec2 | `{1, 1}` | How much the layer follows the world on each axis: 1 moves with the world, 0 stays still on the screen, and values between look farther away. Values above 1 look closer than the world. |
| `repeatX`, `repeatY` | boolean | `false` | Repeat the texture on that axis to fill the view. |
| `repeatSize` | Vec2 | `{0, 0}` | Distance between copies, or the drawn size when zero. |
| `autoscroll` | Vec2 | `{0, 0}` | World units per second the layer scrolls by itself, such as drifting clouds. |
| `limits` | Rect or nil | `nil` | Camera positions in world units where the layer scrolls. Outside them the layer stays as it was at the edge. |
| `color` | Color | `'#FFFFFFFF'` | Multiplies the texture color. |

### parallax:update(dt)

Advances the autoscroll by `dt` seconds.

### parallax:scrolled()

Returns the distance the autoscroll moved the layer so far as a `Vec2`.

### parallax:offset(camera)

Returns how far the layer is moved from where the world would put it, as `x, y` in world units, which also moves anything else with the layer, such as a baked static batch drawn with `graphics2d.drawStatic(batch, x, y)`.

### parallax:draw(camera, order)

Draws the layer in the active canvas, moved by how `camera` sees it, with the repeated axes covering the whole area the canvas shows, a render target canvas included. The argument `order` is an optional [Draw order](#draw-order). A layer without a texture raises `A parallax layer needs a texture to draw.`, and a repeated copy without a size raises `A parallax layer needs a positive size to repeat.`

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local hills = graphics.newRenderTarget(512, 256)
local far = graphics2d.newParallax(hills.texture, {position = {0, -200}, scrollScale = {0.2, 1}, repeatX = true, color = '#FF6080A0'})
local near = graphics2d.newParallax(hills.texture, {position = {0, -60}, scrollScale = {0.6, 1}, repeatX = true, repeatSize = {700, 256}})
local baked = graphics2d.newSpriteBatch(graphics.whiteTexture())
for index = 0, 20 do
    baked:add({x = index * 180, y = 180, width = 12, height = 80, color = '#FF203020'})
end
local fence = baked:bake()
local fenceLayer = graphics2d.newParallax(graphics.whiteTexture(), {scrollScale = {1.3, 1}})

scene.push({
    update = function(self, dt)
        camera.x = camera.x + 150 * dt
        far:update(dt)
    end,
    render = function(self)
        local hillCamera = graphics2d.newCamera()
        hillCamera.position = {256, 128}
        graphics2d.beginTarget(hills, hillCamera, {clear = '#00000000'})
        graphics2d.drawCircle(256, 300, 220, '#FFFFFFFF')

        graphics2d.beginWorld(camera)
        far:draw(camera, {layer = -2})
        near:draw(camera, {layer = -1})
        graphics2d.drawRect({camera.x - 2000, 150, 4000, 400}, '#FF3A7D44')
        local x, y = fenceLayer:offset(camera)
        graphics2d.drawStatic(fence, x, y, {layer = 1})
    end,
})
```

## NineSlice

A `NineSlice` is a scalable frame made of nine texture regions. The functions `graphics2d.newNineSlice` and `atlas:slice(name)` from [`haylen.animation2d`](animation2d.md) create it, and `graphics2d.drawNineSlice` draws it. Vector and rectangle properties return copies, so assign a new value instead of changing a returned one.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | read and write | Texture the regions come from. |
| `pieces` | table | read and write | The nine source `Rect` values, row by row from the top-left corner. Writing needs exactly nine, otherwise it raises `a nine-slice needs exactly nine pieces`. |
| `fill` | string | read and write | `'stretch'` or `'tile'`. |
| `borders` | table | read | Border sizes as `{left, top, right, bottom}`, in the form `graphics2d.newNineSlice` accepts: the width of the widest piece of each side column and the height of the tallest piece of each side row. |
| `valid` | boolean | read | The value is `true` when the nine-slice has a texture. |

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local panel = graphics2d.newNineSlice(assets.texture('ui/banner.png'), {borders = {100, 68, 84, 111}})
print(table.concat(panel.borders, ', '), panel.fill, #panel.pieces, panel.pieces[5].width)
panel.fill = 'tile'
local wide = graphics2d.newNineSlice(panel.texture, {borders = panel.borders, fill = panel.fill})
print(wide.valid)
```

### slice:layout(rect, borderScale)

Returns the quads that `graphics2d.drawNineSlice` draws for `rect` and `borderScale`, which defaults to 1, as a list of `{area = Rect, source = Rect}`, where `area` is the part of `rect` a quad covers and `source` the region of the texture it shows in pixels. An app that draws frames its own way, such as a custom GUI library or a frame of meshes, takes the same layout. A `rect` without a positive width and height returns an empty list, and a scale that is not positive raises `A nine-slice border scale must be positive.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local panel = graphics2d.newNineSlice(assets.texture('ui/panel.png', {filter = 'linear'}), {borders = {16, 16, 16, 16}, fill = 'tile'})
for _, patch in ipairs(panel:layout({0, 0, 300, 120}, 0.5)) do
    print(patch.area.x, patch.area.width, patch.source.x, patch.source.width)
end
```

## Material

A `Material` shades draws with a custom shader and holds values for the uniforms and textures of the shader, which it sets by the names the shader declares them with. The function `graphics2d.newMaterial` creates it, and the `material` key of a [draw order](#draw-order), of a sprite and of the `materials` of a post-process applies it. A draw copies the values when it is made. The [shader guide](../shaders.md) explains how to write and compile shaders.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `shader` | Shader | read | The shader of the material. |

Materials compare equal when they are the same material.

### material:set(name, value)

Sets a uniform or a texture of the shader by name. The value fits the type of the uniform:

| Uniform | Value |
| --- | --- |
| `float`, `int` | A number. Integers round to the nearest one. |
| `vec2`, `ivec2` | A `Vec2`, a table `{x, y}` or `{x = 0, y = 0}`. |
| `vec3` | A `Color`, whose red, green and blue fill it, or a list of 3 numbers. |
| `vec4` | A `Color`, a color string or a list of 4 numbers. |
| `mat4` | A `Transform2D`, which moves x and y like the transform, or a list of 16 numbers in column order. |
| arrays | A list of every number of every element, such as 12 numbers for `vec4 palette[3]`. |
| texture | A `Texture`, or `nil` for a white texture. |

An unknown name raises `The shader "name" has no uniform named "key".`, and a value that does not fit raises an error that names the type and the count of numbers the uniform takes, such as `The uniform "tint" is a "vec4", which takes 4 numbers, not 1.`

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')

local shine = graphics2d.newMaterial(assets.shader('shaders/shine.shader'))
shine:set('speed', 2)
shine:set('direction', m.vec2(1, 0.5))
shine:set('glint', '#FFFFE080')
shine:set('warp', m.rotation(0.2))
```

### material:get(name)

Returns the value of a uniform or a texture: a number for `float` and `int` uniforms, a `Vec2` for `vec2` and `ivec2`, a `Color` for `vec4`, a list of numbers for every other uniform and array, and the `Texture`, or `nil`, of a texture. Uniforms that were never set read as zeros.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local shine = graphics2d.newMaterial(assets.shader('shaders/shine.shader'), {speed = 3})
print(shine:get('speed'), shine:get('glint'), shine.shader.name)
```

## RichText

A `RichText` is BBCode markup laid out with a font family, created by `graphics2d.newRichText`. It caches its layout for its width and scale, runs its effects and its typewriter reveal as `text:update(dt)` advances, and draws the moment it has reached. The [text guide](../text.md) explains the markup, the effects and the reveal.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `markup` | string | read-write | The markup. Setting it lays the text out again and starts its effects and reveal over. |
| `maxWidth` | number | read-write | The width the paragraphs wrap at, 0 for none. |
| `scale` | number | read-write | Multiplies every size of the text. |
| `family` | FontFamily | read-write | The family of text without a `[font]` tag. Setting a `Font` makes it the regular face of a family of its own. |
| `bold`, `italic` | boolean | read-write | Whether all the text is bold or italic, as if inside `[b]` or `[i]`. |
| `color` | Color | read-write | The color of text without a `[color]` tag. |
| `align` | string | read-write | The alignment of paragraphs without their own, like the `align` option. |
| `direction` | string | read-write | The direction of paragraphs without a `[p dir]` of their own, like the `direction` option. |
| `language` | string | read-write | The BCP 47 language tag the text is shaped for. |
| `lineSpacing` | number | read-write | The distance between lines as a multiple of their height. |
| `revealSpeed` | number | read-write | Characters per second the typewriter reveal shows, 0 for everything at once. |
| `underlineLinks` | boolean | read-write | Whether `[url]` text is underlined. |
| `visibleCharacters` | integer | read-write | How many characters show. Setting it moves the reveal there, and a negative count shows everything. |
| `visibleRatio` | number | read-write | The share of characters that show, from 0 to 1. |
| `characterCount` | integer | read | The characters of the text: one per cluster, which is a letter with its marks, a conjunct or a ligature, and one per image or icon. |
| `revealing` | boolean | read | Whether the reveal still has characters to show. |
| `time` | number | read | Seconds the text has run, which drives its effects. |

The option properties take the values of the options of `graphics2d.newRichText`, and setting one lays the text out again and starts its reveal over, while its effects keep their time. The base `size` and the `fonts` of `[font]` tags stay the ones the text was made with, and `scale` resizes the whole text.

```lua
local graphics2d = require('haylen.graphics2d')

local line = graphics2d.newRichText('Hello [b]there[/b]', {revealSpeed = 20})
line:update(0.25)
print(line.visibleCharacters, line.characterCount, line.revealing)
line.visibleRatio = 1
line.markup = 'Goodbye'
line.color = '#FFFFD070'
line.italic = true
line.revealSpeed = 0
```

### text:update(dt)

Moves the effects and the reveal `dt` seconds forward. A `[pause=seconds]` tag holds the reveal before the next character, and `[speed=factor]` reveals the text inside it faster or slower. An image that was still loading takes its room once it arrives, at the next update.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local dialogue = graphics2d.newRichText('Wait for it...[pause=1] [shake]Boo![/shake]', {size = 40, revealSpeed = 15})

scene.push({
    update = function(self, dt)
        dialogue:update(dt)
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        dialogue:draw(100, 100)
    end,
})
```

### text:draw(x, y, options)

Draws the text with the top-left corner of its block at `x`, `y`. The argument `options` is optional and takes `scale`, a `Vec2` that stretches the block from that corner without laying it out again, which suits pulses and pops, `tint`, a `Color` that multiplies every color of the text, and the [draw order](#draw-order) keys. A y-sorted canvas sorts it by the bottom of its block.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local sign = graphics2d.newRichText('[center][b]Village[/b]\n[size=70%]Population 42[/size][/center]', {size = 28, maxWidth = 240})

scene.push({
    render = function(self)
        graphics2d.beginWorld(graphics2d.newCamera())
        sign:draw(-120, -200, {layer = 3, scale = {1.1, 1.1}, tint = '#FFFFF0D0'})
    end,
})
```

### text:size(maxWidth)

Returns the width and height of the text block. With `maxWidth`, it returns the size the block would take wrapped at that width, without changing the width of the text, which is how a panel measures text before it places it. The layouts of the last few widths stay cached.

```lua
local graphics2d = require('haylen.graphics2d')

local label = graphics2d.newRichText('[size=48]Score[/size] 1200 points this round')
local width, height = label:size()
local narrowWidth, narrowHeight = label:size(200)
print(width, height, narrowWidth, narrowHeight)
```

### text:linkAt(x, y) and text:hintAt(x, y)

Return the payload of the `[url]` link or the text of the `[hint]` under a point of the text block, relative to its top-left corner, or `nil`. A link without a payload, `[url]https://haylen.dev[/url]`, uses its text.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local help = graphics2d.newRichText('Read the [url=manual]manual[/url] or the [hint=Frequently asked questions]FAQ[/hint].', {size = 28})

scene.push({
    update = function(self, dt)
        local x, y = input.mousePosition()
        if input.mousePressed('left') then
            print(help:linkAt(x - 100, y - 100))
        end
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        help:draw(100, 100)
    end,
})
```

### text:setVisibleCharacters(count)

Shows the first `count` characters, like setting `visibleCharacters`, where a negative count shows everything. A reveal that runs continues from there.

```lua
local graphics2d = require('haylen.graphics2d')

local page = graphics2d.newRichText('A long page of the story.', {revealSpeed = 30})
page:setVisibleCharacters(-1)
print(page.revealing)
```

### text:frame()

Returns the text as it draws at this moment, with its effects applied and the characters the reveal has not reached hidden, as a table with `size` (`Vec2`), `lineCount` and these lists:

| Field | Entries |
| --- | --- |
| `glyphs` | `char` (the first code point of its character), `index` (the glyph index in its font), `character` (counted from 1), `rect` (the quad), `baseline`, `color`, `visible`, `size` (the text size), `syntheticBold` and `syntheticItalic`, in the order the lines show them. |
| `boxes` | `kind` (`'background'`, `'underline'`, `'strike'`, `'rule'`, `'cellBackground'` or `'cellBorder'`), `rect`, `color` and `visible`. |
| `images` | `rect`, `texture` and `visible` of every image and icon. |
| `links` | `rect` and `link` of every piece of a link, several when it wraps. |
| `hints` | `rect` and `hint` of every piece of a hint. |
| `characters` | `rect`, `first` and `last` (the code points of the text without markup it draws, counted from 1, where paragraphs end with a line break) and `rightToLeft` of every character in reading order. |
| `lines` | `rect`, `baseline` and `rightToLeft` of every line. |

```lua
local graphics2d = require('haylen.graphics2d')

local laid = graphics2d.newRichText('[b]Bold[/b] and [url=more]a link[/url]'):frame()
for _, glyph in ipairs(laid.glyphs) do
    print(glyph.char, glyph.rect.x, glyph.syntheticBold)
end
print(#laid.links, laid.links[1].link)
```
