# 📖 `haylen.graphics2d`

The module `haylen.graphics2d` draws everything a 2D app shows: sprites, sprite batches, shapes, lines, meshes, text, rich text, nine-slices, vector images, metaballs, lights and occluders, into world, screen and offscreen canvases. It also has cameras, parallax layers, groups, masks, captures and the materials that shade draws with custom shaders. Draw inside the `render` and `renderUi` hooks of a scene, and make cameras, sprites, batches and materials anywhere. Textures, render targets, fonts and shaders come from [`haylen.graphics`](graphics.md), and lights and occluders from [`haylen.lighting2d`](lighting2d.md). The [rendering guide](../graphics/rendering.md) explains canvases, batching and every effect, and the [text guide](../graphics/text.md) explains text and markup.

```lua
local graphics2d = require('haylen.graphics2d')
```

## Values

Functions accept these forms wherever the reference names the type:

| Type | Accepted values |
| --- | --- |
| Color | A `Color` from [`haylen.math`](math.md), a `'#RRGGBB'` or `'#AARRGGBB'` string, or a table `{r, g, b, a}` or `{r = 1, g = 1, b = 1, a = 1}` with components from 0 to 1, where `a` is 1 by default. |
| Vec2 | A `Vec2` from [`haylen.math`](math.md), or a table `{x, y}` or `{x = 0, y = 0}`. |
| Rect | A `Rect` from [`haylen.math`](math.md), or a table `{x, y, width, height}` or `{x = 0, y = 0, width = 0, height = 0}`. |

The y axis points down. Angles are in radians, and a positive rotation turns clockwise on the screen. Pivots are shares of a size, so `pivotX = 0.5, pivotY = 0.5` is the center and `0, 0` is the top-left corner.

A malformed color string raises `invalid color text, expected #RRGGBB or #AARRGGBB`. Option tables reject keys they do not know with `Unknown option "<key>".` and keys that are not strings with `Option tables only accept string keys.`. An unknown name for an enum option raises an argument error that contains `unknown value '<name>'`.

```lua
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({40, 40, 200, 100}, '#FF2050A0')
        graphics2d.drawRect(m.rect(260, 40, 200, 100), {r = 1, g = 0.5, b = 0})
        graphics2d.drawCircle(600, 90, 50, m.color(0.2, 0.8, 0.4, 1))
    end,
})
```

## Draw order

Every draw that takes an `order` table, and every options table that lists `layer`, `depth` and `blend`, places and shades the draw in its canvas with these keys. The [rendering guide](../graphics/rendering.md#draw-order) explains how canvases sort them.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `layer` | integer | `0` | Higher layers draw later, on top of lower layers. |
| `depth` | number | `0` | Inside one layer, higher depths draw later when the canvas sorts by depth. Other canvases ignore it. |
| `sortOffset` | number | `0` | Moves the point that a canvas sorted by `'y'` sorts the draw by, such as from the center of a sprite down to its feet. |
| `sortId` | integer | `0` | Orders the draws that sort the same, lowest first, such as characters on one row of a canvas sorted by `'y'`. |
| `visibility` | integer | `1` | Bits of the draw. A canvas whose `visibilityMask` shares no bit with them skips the draw. |
| `blend` | string | `'alpha'` | The [blend mode](#blend-modes) of the draw. |
| `material` | Material | none | A [custom shader material](#material) that shades the pixels of the draw. Sprites, batches, static batches, nine-slices, rectangles, lines, shapes, meshes and text take materials. Image blends and metaballs raise `Image blends and metaballs do not take a material.`. |
| `partMask` | Texture | none | Recolors sprites and sprite batches by parts, as [recoloring by parts](#recoloring-by-parts) explains. Vector images and baked batches raise an error with it, and other draws ignore it. |
| `normalMap` | Texture | none | In lit canvases, the normal map of a sprite, with the layout of its texture. |
| `specular` | number | `0` | In lit canvases, the strength of the highlights of a normal-mapped sprite, scaled by the alpha of its normal map. It must be zero or more. |
| `shininess` | number | `32` | In lit canvases, how tight the highlights of a normal-mapped sprite are, from 1 to 255. |
| `emission` | number | `0` | In lit canvases, how strongly the colors of the draw glow whatever the light. It must be zero or more. |
| `lightMask` | integer | `1` | In lit canvases, bits that the item masks of lights test, from 0 to 255. |
| `unshaded` | boolean | `false` | In lit canvases, keeps the colors of the draw whatever the light. |
| `distortion` | number | `0` | Above 0, the draw bends the image of its canvas instead of drawing colors, as [distortion](#distortion) explains. |

Unlit canvases ignore the lighting keys, and [`haylen.lighting2d`](lighting2d.md) explains how lit canvases use them. A draw copies the values of its material when it is made, so a material changed between two draws shades each draw with its own values. A negative `specular` or `emission` raises `A draw needs a specular strength and an emission of zero or more.`, and a `shininess` out of range raises `A draw shininess must be from 1 to 255.`.

The `sort` option of a canvas chooses how the draws of one layer are ordered:

- The value `'layer'` keeps the order in which the app made them.
- The value `'depth'` sorts them by `depth`.
- The value `'y'` sorts them by the y they stand on plus their `sortOffset`, so lower draws cover higher ones without setting any depth.

Draws that sort the same draw by their `sortId`, lowest first, and draws with the same `sortId` keep the order in which the app made them. Some apps walk their objects in an order that changes from frame to frame, such as the keys of a table with `pairs`. They give each object its own `sortId`, such as its id. The function [`graphics2d.pushLayerOffset`](#graphics2dpushlayeroffsetoffset) shifts the layer of every following draw of the canvas. Visibility bits hide draws from some canvases: draw the world once for the main camera and again for a minimap canvas whose `visibilityMask` leaves out details.

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

### Blend modes

| Name | Effect |
| --- | --- |
| `'alpha'` | Regular transparency. |
| `'additive'` | Adds the color to what is below, for glows, fire and sparks. |
| `'multiply'` | Multiplies with what is below, which darkens. |
| `'screen'` | The inverse of multiply, which lightens. |
| `'premultiplied'` | Transparency for colors already multiplied by their alpha. |
| `'opaque'` | No blending: the draw replaces what is below. |
| `'overlay'` | Multiplies the dark colors below and screens the light ones, which raises the contrast. |
| `'darken'`, `'lighten'` | Keeps the darker or the lighter of the two colors, channel by channel. |
| `'colorDodge'`, `'colorBurn'` | Brightens what is below toward the color, or darkens it toward the color. |
| `'hardLight'`, `'softLight'` | Multiplies or screens by the color itself, harshly or softly, like a spotlight or a diffused light. |
| `'difference'`, `'exclusion'` | Subtracts the darker color from the lighter one, or does it with less contrast. |

Every mode but `'premultiplied'` takes straight colors, the way images and colors are usually made, so a partly transparent multiply or screen draw weakens its effect in proportion to its alpha. The modes from `'overlay'` on mix each color with the color below it, which the blending of the GPU cannot reach. So only [groups](#graphics2dbegingroupoptions) take them, and any other draw with one raises `The blend mode "<name>" mixes with what lies below, which only groups do. Draw inside "beginGroup" with that blend instead.`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({100, 100, 400, 300}, '#FF3A7D44')
        graphics2d.drawCircle(300, 250, 120, '#FF804020', {blend = 'multiply'})
        graphics2d.drawCircle(300, 250, 60, '#FFFFD080', {blend = 'screen'})
    end,
})
```

## Canvases

Every draw goes into the open canvas. A scene opens a world, screen or render target canvas, and each new canvas closes the one before it. Drawing without an open canvas raises `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.`. Canvases exist only while the engine renders a frame, so draws belong in `render` or `renderUi` and never in `update`. The [rendering guide](../graphics/rendering.md#canvases) explains the kinds of canvas and the order they reach the screen.

### graphics2d.beginWorld(camera, options)

Opens a world canvas that draws in world coordinates through `camera`. The view of the camera covers its `viewport`, or the current visible design area without one, divided by its zoom, and the canvas reaches only that part of the screen. This is how split screens and minimaps draw several cameras in one frame. The table `options` is optional and takes the [canvas options](#canvas-options).

A viewport without a positive width and height raises `A camera viewport needs a positive width and height.`. A world canvas with `ambientLight` or `postProcess` renders offscreen over its `clear` color and reaches the screen as an image that covers what earlier canvases drew. Other world canvases draw straight onto the screen, so a `clear` color without `ambientLight` or `postProcess` raises `World canvases only support a clear color with lighting or post-processing.`.

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

### Canvas options

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `sort` | string | `'layer'` | The value `'layer'` sorts draws by layer only. The value `'depth'` also sorts by depth inside each layer, and `'y'` sorts by the y each draw stands on, as the [draw order](#draw-order) describes. |
| `order` | integer | `0` | Canvases reach their destination by order, lowest first, and canvases with the same order in the order they began. |
| `visibilityMask` | integer | all bits | Draws whose `visibility` shares no bit with the mask are skipped. |
| `ambientLight` | Color | none | Turns on lighting. The canvas is multiplied by a light map that starts at this color and receives every `graphics2d.drawLight`, and occluders cast the shadows of its lights. |
| `clear` | Color | see the meaning | The background of offscreen canvases. Render target canvases are transparent by default. World canvases with `ambientLight` or `postProcess` take the `clearColor` of `app.json` by default. Screen canvases and other world canvases draw straight onto the screen, so they raise an error when given a clear color. |
| `postProcess` | table | none | Adjustments of the whole image, applied when the canvas reaches its destination, with the [post-process options](#post-process-options). |

### Post-process options

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `tint` | Color | `'#FFFFFFFF'` | Multiplies every pixel. |
| `saturation` | number | `1` | 0 is gray, and values above 1 boost color. |
| `brightness` | number | `1` | Multiplies the brightness. |
| `contrast` | number | `1` | Scales the contrast around middle gray. |
| `vignetteStrength` | number | `0` | How dark the vignette makes the corners, from 0 to 1. |
| `vignetteRadius` | number | `0.6` | The distance from the center where the vignette starts, where 1 reaches the corners. |
| `vignetteSoftness` | number | `0.5` | The width of the falloff of the vignette. |
| `fade` | Color | `'#00000000'` | Mixes the result toward this color by its alpha, for fades to black or white. |
| `distortion` | number | `24` | How far the [distortion draws](#distortion) move pixels, in units of the destination, where their coverage goes from nothing to full within 8 units. |
| `chromaticAberration` | number | `0` | Splits red and blue apart toward the corners by this many units, like a cheap lens. |
| `pixelate` | number | `0` | Draws the image in square blocks of this many units, where 0 keeps every pixel. |
| `blur` | number | `0` | Blurs the whole image over this radius in units, such as the world behind a pause menu. |
| `bloomStrength` | number | `0` | Adds the parts of the image brighter than the threshold, blurred, times this strength, so lights, fire and neon glow. |
| `bloomThreshold` | number | `0.8` | The brightness, from 0 to 1 and above for floating-point light, that a color needs to bloom. |
| `bloomRadius` | number | `12` | How far bloom spreads, in units. It must be positive. |
| `colorLut` | Texture | none | Grades every color through a lookup texture of square cells side by side, one cell for each step of blue, with red across a cell and green down it. So it is as wide as its height squared, such as 256 by 16 pixels for 16 steps. |
| `colorLutStrength` | number | `1` | How much of the lookup texture mixes in, from 0 to 1. |
| `colorVision` | string | `'normal'` | Shows the image as a deficiency of color vision sees it: `'protanopia'` without red cones, `'deuteranopia'` without green cones or `'tritanopia'` without blue cones. A developer checks that the app still reads, or corrects for it with `correctColorVision`. |
| `correctColorVision` | boolean | `false` | Instead of showing what the `colorVision` deficiency sees, shifts the differences it cannot see into colors it can, for players who have it. |
| `grain` | number | `0` | Adds noise that changes every frame, like the grain of film, from 0 to 1. |
| `dither` | boolean | `false` | Spreads the rounding of every color to 8 bits over neighbouring pixels, so smooth gradients, fog and the falloff of lights show no bands. |
| `materials` | list of Material | empty | Custom shader passes that run after the other adjustments, in order, each over the image that the step before made. The image is the texture of each pass, which covers the whole canvas. |

Units are design units for world canvases and pixels for render target canvases. The [rendering guide](../graphics/rendering.md#post-processing) gives the order of the steps and what each one costs. These errors come from post-processing:

| Error | Cause |
| --- | --- |
| `The color vision "<name>" is unknown. It is "normal", "protanopia", "deuteranopia" or "tritanopia".` | The `colorVision` has another name. |
| `Post-processing needs a distortion, chromatic aberration, pixelate, blur, bloom strength and bloom threshold of at least 0 and a positive bloom radius.` | One of these values is out of range. |
| `A color lookup texture is as wide as its height squared, such as 256 by 16 pixels.` | The `colorLut` has another shape. |
| `The strength of a color lookup texture must be from 0 to 1.` | The `colorLutStrength` is out of range. |
| `The grain of post-processing must be from 0 to 1.` | The `grain` is out of range. |
| `A post-processing material needs a shader.` | An entry of `materials` has no shader. |

### graphics2d.beginScreen(options)

Opens a screen canvas that draws in design coordinates, the space of `viewport.visibleRect()`. Use it for HUDs, menus and anything that does not move with the camera. The table `options` takes `sort`, `order` and `visibilityMask` of the [canvas options](#canvas-options). A screen canvas draws on top of what earlier canvases drew, so `ambientLight` or `postProcess` raises `Screen canvases do not support lighting or post-processing.`, and `clear` raises `Screen canvases do not support a clear color.`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen({order = 10})
        graphics2d.drawText(nil, 'Score 120', 40, 40, {size = 48})
    end,
})
```

### graphics2d.beginTarget(target, camera, options)

Opens a canvas that draws into the render target `target` through `camera`. The view of the camera covers the size of the target, so a camera at `target.width / 2, target.height / 2` shows the region from 0, 0 to the size of the target. A camera `viewport` is a rectangle of the target in pixels. The table `options` takes the [canvas options](#canvas-options), and `clear` is transparent by default. With `ambientLight` or `postProcess`, the canvas renders lit and post-processed offscreen and then composites into the target, so a minimap, a portrait or a mirror can show a lit scene.

Draw the result with `target.texture`. The target holds colors premultiplied by their alpha, so draw its texture with `blend = 'premultiplied'` wherever it is not fully opaque, or its soft edges come out darker. A target that is not valid raises `A render target canvas needs a valid render target.`. A canvas that draws into the target cannot sample its texture, and such a draw raises `A draw cannot sample the render target that its canvas draws into.`.

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

### graphics2d.canvasBounds()

Returns a `Rect` with the area that the open canvas shows, in its own coordinates. For a world canvas this is the part of the world the camera sees. Without an open canvas it returns an empty rectangle at 0, 0.

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

Returns how long one unit of the destination is in the coordinates of the open canvas: a design unit for world canvases and a pixel for render target canvases. For a camera without rotation this is `1 / zoom`. Outlines and markers multiplied by it keep the same thickness at any zoom. Without an open canvas it raises `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.`.

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

Returns `true` when the open canvas is lit: a world or render target canvas with `ambientLight`, which is the only kind of canvas that takes lights and occluders. Effects that bring their own light draw it only then. Without an open canvas it raises `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.`.

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

### graphics2d.pushClip(rect)

Limits the following draws of the open canvas to `rect`, in the coordinates of the canvas. The clip keeps the pixels of the destination whose centers lie inside the rectangle. So a clip whose edges fall between pixels neither shows a row past them nor cuts one inside them. Nested clips intersect with the clip around them. Clips end with `graphics2d.popClip` or when a new canvas begins.

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

Removes the clip that the last `graphics2d.pushClip` added. A call without a matching push raises `The "popClip" call has no matching "pushClip".`.

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

Adds `offset` to the layer of every following draw of the open canvas until `graphics2d.popLayerOffset`. Nested offsets add up, and a new canvas starts without offsets. It lets a group of draws that sets its own layers, such as a character with its shadow and its name, move to another layer as a whole. Without an open canvas it raises `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.`.

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

Removes the offset that the last `graphics2d.pushLayerOffset` added. A call without a matching push raises `The "popLayerOffset" call has no matching "pushLayerOffset".`.

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

## Captures

A capture sends whole canvases into a render target instead of the screen, lit and post-processed canvases included, ready to draw as a texture, blend or keep. Scene transitions use captures to render the scenes before and after a change into images of their own, as the [scene reference](scene.md#effects-that-show-both-scenes) explains. The [rendering guide](../graphics/rendering.md#captures) explains when a capture renders.

### graphics2d.beginCapture(target, options)

Starts a capture: the world and screen canvases that begin until `graphics2d.endCapture` render into the render target `target` instead of the screen. The target is cleared first to the color of the option `clear`, transparent by default, and other keys raise `Unknown option "<key>".`. The visible area covers the whole target, so give it the pixel size of the viewport to keep every pixel.

- Render target canvases begun during a capture still draw into their own targets.
- A capture can begin while another one is open, such as in a scene that a transition captures. The canvases that begin until it ends go into its own target, and the canvases after its end go back into the outer capture.
- A capture left open ends with the frame.

The target holds colors premultiplied by their alpha, so draw its texture with `blend = 'premultiplied'` wherever it is not fully opaque. The canvases of the capture cannot sample its texture, which raises `A draw cannot sample the render target that its canvas draws into.`. A target that is not valid raises `A capture needs a valid render target.`.

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
        graphics2d.beginCapture(frame, {clear = '#FF101820'})
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

Ends the capture that `graphics2d.beginCapture` began, so the following canvases reach the screen again. A call without an open capture raises `The "endCapture" call has no matching "beginCapture".`.

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
        print('Capturing: ' .. tostring(graphics2d.capturing()))
        graphics2d.endCapture()
    end,
})
```

## Camera

A `Camera` shows a part of the world for `graphics2d.beginWorld` and `graphics2d.beginTarget`. Its `position` is the point of the world it shows. The method `camera:follow` moves that position toward a target through the dead zone, the drag margins, the look-ahead, the smoothing and the limits, and `camera:update` advances the shake and the rotation smoothing. The [rendering guide](../graphics/rendering.md#cameras-and-parallax) explains how following works. Vector and rectangle properties return copies, so assign a new value instead of changing a field of the returned one.

The methods measure the view with the screen, the visible design area of `viewport.visibleRect()`, and a `viewport` is a rectangle of that screen in design coordinates. Screen points, such as the pointer positions of [`haylen.input`](input.md), are in the same design coordinates. A render target canvas measures the view with the target instead, so for a camera used with `graphics2d.beginTarget` the methods describe the screen view unless the camera has a viewport.

### graphics2d.newCamera()

Returns a new `Camera` at 0, 0 with zoom 1. Without a viewport, its view always follows the current visible design area, so it stays right when the window is resized.

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

### graphics2d.blendCameras(from, to, amount)

Returns a new `Camera` between the views of `from` and `to`, where an `amount` of 0 gives the view of `from` and 1 the view of `to`. The position, offset, rotation and viewport move in a straight line, and the zoom changes at a steady rate. The result takes the other settings of `to`. Tween `amount` for a smooth cut between two cameras, such as from the player to a boss.

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

### Properties

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `position` | Vec2 | `{0, 0}` | The point of the world the view shows at its anchor. |
| `x`, `y` | number | `0` | The parts of the position. |
| `offset` | Vec2 | `{0, 0}` | Moves the view in world units after the limits, so it can show past them, for a look around or a nudge of a cutscene. |
| `anchor` | string | `'center'` | The value `'center'` puts the position at the center of the view, which zooms and turns around it. The value `'topLeft'` puts it at the top-left corner, and following then ignores the dead zone and the drag margins. |
| `zoom` | Vec2 | `{1, 1}` | The scale of the view on each axis: 2 shows half the area at twice the size. It stays between `minZoom` and `maxZoom`, and an axis that is not a number raises `The camera zoom must be a number.`. |
| `minZoom`, `maxZoom` | number | `0.05`, `20` | The limits of every zoom change. The smallest zoom must be above 0 and not above the largest, or the assignment raises `The smallest zoom must be above 0 and not above the largest zoom.` or `The largest zoom must not be below the smallest zoom.`. |
| `rotation` | number | `0` | The rotation of the view in radians. |
| `ignoreRotation` | boolean | `false` | Keeps the view upright, ignoring `rotation` and the rotation of the shake. |
| `viewport` | Rect or nil | `nil` | The part of the screen the camera draws into, in design coordinates, for split screens and minimaps. The value `nil` covers the whole visible area. |
| `limits` | Rect or nil | `nil` | The world area the view stays inside while following or clamping. A limit smaller than the view centers the view on it. |
| `limitSmoothing` | boolean | `false` | With position smoothing, eases the view into the limits instead of stopping it at them. |
| `positionSmoothing` | boolean | `false` | Eases the position toward the target at `positionSmoothingSpeed`, and the zoom of `camera:frame` too. |
| `positionSmoothingSpeed` | number | `5` | The rate of the position smoothing per second. |
| `rotationSmoothing` | boolean | `false` | Eases the drawn rotation toward `rotation` at `rotationSmoothingSpeed`, the short way around. |
| `rotationSmoothingSpeed` | number | `5` | The rate of the rotation smoothing per second. |
| `deadZone` | Vec2 | `{0, 0}` | The width and height in world units of the area around the view center where the target moves without moving the view. A negative value raises `The dead zone of a camera cannot be negative.`. |
| `dragHorizontal`, `dragVertical` | boolean | `false` | Let the target move inside the drag margins on that axis before the view follows. |
| `dragMargins` | table | `{0.2, 0.2, 0.2, 0.2}` | How far the target moves from the view center on each side, in shares of half the view, as `{left, top, right, bottom}` or `{left = 0.2, ...}`. Reading it returns a table with named fields, and a negative margin raises `The drag margins of a camera cannot be negative.`. |
| `dragOffset` | Vec2 | `{0, 0}` | Where the target rests on an axis that does not drag, from -1 on the right or bottom margin to 1 on the left or top margin, such as ahead of a character that faces right. |
| `lookAheadTime` | number | `0` | Seconds of target velocity that the view leads the target by. |
| `maxLookAhead` | number | `200` | The largest look-ahead distance in world units. |
| `lookAheadSmoothingSpeed` | number | `4` | The rate at which the look-ahead follows the velocity, so a sudden stop does not jerk the view back. |
| `trauma` | number | `0` | The amount of shake, from 0 to 1, which decays over time. Assigning it sets the trauma directly. |
| `maxShakeOffset` | number | `24` | The shake offset in world units at full trauma. |
| `maxShakeAngle` | number | `0.05` | The shake rotation in radians at full trauma. |
| `shakeFrequency` | number | `25` | How fast the shake noise moves. Lower values sway, and higher values rattle. |
| `traumaDecay` | number | `1.5` | The trauma lost per second. |
| `pixelSnap` | boolean | `false` | Rounds the drawn position to whole view pixels at the current zoom, and the center of a view of an odd size down to a whole view pixel, which keeps pixel art from shimmering. |
| `viewSize` | Vec2 | the visible design size | Read only. The size of the viewport, or of the visible design area without one, at zoom 1. |

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera.anchor = 'center'
camera.offset = {0, -40}
camera.rotation = 0.05
camera.ignoreRotation = false
camera.minZoom = 0.5
camera.maxZoom = 3
camera.limits = {0, 0, 3584, 2304}
camera.limitSmoothing = true
camera.positionSmoothing = true
camera.positionSmoothingSpeed = 6
camera.rotationSmoothing = true
camera.rotationSmoothingSpeed = 4
camera.deadZone = {64, 32}
camera.dragVertical = true
camera.dragOffset = {0.5, 0}
camera.lookAheadTime = 0.3
camera.maxLookAhead = 160
camera.lookAheadSmoothingSpeed = 3
camera.maxShakeAngle = 0.03
camera.traumaDecay = 2
camera.pixelSnap = true
print(camera.viewSize.x, camera.viewSize.y)
```

### camera:follow(x, y, dt)

Moves the camera toward the target `x`, `y` for a frame of `dt` seconds. The goal of the view stays put while the target moves inside the dead zone and the drag margins. It leads the target by its velocity with the look-ahead and rests at the drag offset on axes that do not drag. The position eases toward the goal with position smoothing and stays inside the limits. Like `camera:update` and `camera:frame`, it raises `A camera moves forward by a number of seconds that is finite and not negative.` for any other `dt`.

```lua
local graphics2d = require('haylen.graphics2d')
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

Follows the middle of a list of points and zooms until they fit inside the view with `padding` world units around them, within the zoom limits. Use it for games that keep several players, or a player and a boss, in view. The zoom eases like the position when position smoothing is on. An empty list raises `Framing needs at least one point.`.

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

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        camera:update(dt)
    end,
})
```

### camera:addTrauma(amount)

Adds shake that moves in every direction. The trauma stays between 0 and 1, and the shake grows with the square of the trauma, so small hits stay subtle.

```lua
local graphics2d = require('haylen.graphics2d')
local timer = require('haylen.timer')

local camera = graphics2d.newCamera()
camera.maxShakeOffset = 16
camera.shakeFrequency = 18
timer.every(2, function() camera:addTrauma(0.6) end)
```

### camera:shake(amount, dx, dy)

Adds trauma that shakes the view only back and forth along the direction `dx`, `dy`, without turning it, until the trauma settles, such as a recoil along the line of a shot.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local timer = require('haylen.timer')

local camera = graphics2d.newCamera()
timer.every(3, function() camera:shake(0.8, 1, 0) end)

scene.push({
    update = function(self, dt)
        camera:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawRect({-100, -100, 200, 200}, '#FFA04040')
        graphics2d.drawText(nil, string.format('Trauma %.2f', camera.trauma), -100, 120)
    end,
})
```

### camera:snapTo(x, y)

Moves the view to `x`, `y` at once, inside the limits, without smoothing or look-ahead, such as after a teleport or when a level starts.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera.limits = {0, 0, 2000, 1200}
camera:snapTo(-500, -500)
print(camera.x, camera.y)
```

### camera:resetSmoothing()

Puts the view where following wants it now, without easing the position or the rotation.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera.positionSmoothing = true
camera:follow(900, 600, 0.016)
camera:resetSmoothing()
print(camera.x, camera.y)
```

### camera:align()

Recenters following on the last target, as if the target had just been reached, and lets the smoothing ease the view there. Use it when the drag margins should stop holding the view back.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera.positionSmoothing = true
camera.dragHorizontal = true
camera:follow(300, 0, 0.016)
camera:align()
```

### camera:clampToLimits()

Moves the camera so its view stays inside `limits`. Without limits it does nothing.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera.limits = {0, 0, 2000, 1200}
camera.position = {5000, 0}
camera:clampToLimits()
print(camera.x, camera.y)
```

### camera:zoomAt(factor, x, y)

Multiplies the zoom by `factor`, within the zoom limits, and keeps the world point under the screen point `x`, `y` in place. This is how the mouse wheel and pinch gestures zoom toward the pointer. A factor that is not a number raises `The camera zoom must be a number.`.

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

Returns the shake offset of this frame as a `Vec2`, which `camera:update` computes. It is zero without trauma.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera:addTrauma(0.8)
camera:update(0.016)
local offset = camera:shakeOffset()
print(offset.x, offset.y)
```

### camera:flash(color, duration)

Covers the view with `color`, such as a white flash on a hit or a red one on damage. The alpha of the color fades out over `duration` seconds as `camera:update` advances it, fast at first and slowly at the end. Every world canvas of the camera draws it over what it holds, unshaded, and a new flash replaces the one that is fading.

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

### camera:flashColor()

Returns the color that the flash covers the view with now, transparent once it faded.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera:flash('#C0FF0000', 0.5)
camera:update(0.1)
print(camera:flashColor().a)
```

### camera:renderPosition()

Returns the position the view is drawn from as a `Vec2`: the position plus the offset and the shake, rounded to whole view pixels when `pixelSnap` is `true`.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera.offset = {10, 0}
local position = camera:renderPosition()
print(position.x, position.y)
```

### camera:renderRotation()

Returns the rotation the view is drawn with: the rotation, eased by rotation smoothing, plus the rotation of the shake, or 0 with `ignoreRotation`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
camera.rotationSmoothing = true
camera.rotation = 0.3

scene.push({
    update = function(self, dt)
        camera:update(dt)
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, string.format('Turning %.3f', camera:renderRotation()), 40, 40)
    end,
})
```

### camera:viewTransform()

Returns the `Transform2D` from [`haylen.math`](math.md) that maps world coordinates to view coordinates, where 0, 0 is the top-left corner of the view, with the zoom, rotation, offset and shake. Its inverse maps view coordinates back to the world.

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

Returns a `Rect` with the world area that the view covers, with the zoom, rotation, offset and shake.

```lua
local graphics2d = require('haylen.graphics2d')

local camera = graphics2d.newCamera()
camera.zoom = {2, 2}
local visible = camera:visibleBounds()
print(visible.x, visible.y, visible.width, visible.height)
```

### camera:worldToScreen(x, y)

Converts a world position to a screen point in design coordinates and returns `x, y`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    renderUi = function(self)
        local screenX, screenY = camera:worldToScreen(0, 0)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Origin', screenX, screenY)
    end,
})
```

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
    end,
})
```

### camera:drawDebug(order)

Draws the view as the screen shows it, the limits and the box where the target moves without moving the view, with a cross on the last target. It draws in world coordinates, so it belongs in a world canvas, usually one of a camera that is zoomed out or of the camera itself. The table `order` is an optional [draw order](#draw-order).

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
        graphics2d.drawText(nil, 'Player', player.x, player.y - 60, {size = 24, visibility = 2})
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

## CameraRig

A `CameraRig` directs a camera through zones and along a rail, as the [rendering guide](../graphics/rendering.md#cameras-and-parallax) explains. A zone gives the camera its own limits, zoom and offset while the target is inside it, and the camera eases into a zone it enters. A rail is a line that the camera follows the target along.

### graphics2d.newCameraRig(options)

Returns a new `CameraRig`. The table `options` is optional:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `limits` | Rect | none | The limits of the camera outside every zone. |
| `zoom` | number or Vec2 | `1` | The zoom of the camera outside every zone. |
| `blendTime` | number | `0.5` | The seconds the camera takes to ease into a zone it enters. |
| `zones` | list | none | Zones to add, as `rig:addZone` takes them. |
| `rail` | list or FloatBuffer | none | The points of the rail as `x1, y1, x2, y2, ...`. |

A negative blend time or a zoom that is not positive raises `A camera rig needs a blend time of zero or more and a positive zoom.`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
camera.positionSmoothing = true
camera.limitSmoothing = true
local rig = graphics2d.newCameraRig({limits = {0, 0, 4000, 1200}, zones = {
    {name = 'cave', area = {0, 0, 900, 600}, zoom = 1.5},
    {name = 'arena', area = {1800, 0, 1200, 900}, zoom = 0.75, offset = {0, -80}},
}})
local player = {x = 100, y = 300}

scene.push({
    update = function(self, dt)
        player.x = player.x + 120 * dt
        rig:update(camera, player.x, player.y, dt)
        camera:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawCircle(player.x, player.y, 20, '#FFFFD040')
    end,
})
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `zone` | string or nil | read | The name of the zone the target is in. |
| `zoneCount` | integer | read | The number of zones. |
| `blendTime` | number | read and write | The seconds the camera takes to ease into a zone. |

```lua
local graphics2d = require('haylen.graphics2d')

local rig = graphics2d.newCameraRig({zones = {{name = 'cave', area = {0, 0, 900, 600}}}})
rig.blendTime = 0.8
print(rig.zone, rig.zoneCount, rig.blendTime)
```

### rig:addZone(zone)

Adds a zone, in place of the zone of the same name, from a table with these keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | required | The name of the zone. |
| `area` | Rect | required | Where the zone is. |
| `limits` | Rect | the area | The limits of the camera inside the zone. |
| `zoom` | number or Vec2 | the zoom of the rig | The zoom inside the zone. |
| `offset` | Vec2 | `{0, 0}` | The offset of the camera inside the zone. |
| `priority` | number | `0` | Where zones overlap, the zone of higher priority wins. |

A zone without a name or without an area of a positive size raises `A camera zone needs a name and an area of a positive size.`, and a zoom that is not positive raises `The camera zone "<name>" needs a positive zoom.`.

```lua
local graphics2d = require('haylen.graphics2d')

local rig = graphics2d.newCameraRig()
rig:addZone({name = 'shop', area = {2400, 0, 600, 400}, zoom = 2, priority = 1})
```

### rig:removeZone(name)

Removes the zone named `name`.

```lua
local graphics2d = require('haylen.graphics2d')

local rig = graphics2d.newCameraRig({zones = {{name = 'cave', area = {0, 0, 900, 600}}}})
rig:removeZone('cave')
print(rig.zoneCount)
```

### rig:clearZones()

Removes every zone.

```lua
local graphics2d = require('haylen.graphics2d')

local rig = graphics2d.newCameraRig({zones = {{name = 'cave', area = {0, 0, 900, 600}}}})
rig:clearZones()
```

### rig:setRail(points)

Sets the rail from a list `{x1, y1, x2, y2, ...}` or a float buffer, and `nil` takes it away. A single point raises `A camera rail needs two points or more, or none to take it away.`.

```lua
local graphics2d = require('haylen.graphics2d')

local rig = graphics2d.newCameraRig()
rig:setRail({0, 600, 2000, 600, 3000, 300})
rig:setRail(nil)
```

### rig:project(x, y)

Returns the point of the rail nearest to `x`, `y`, or the point itself without a rail.

```lua
local graphics2d = require('haylen.graphics2d')

local rig = graphics2d.newCameraRig({rail = {0, 600, 2000, 600}})
print(rig:project(500, 100))
```

### rig:update(camera, x, y, dt)

Picks the zone of the target `x`, `y`, eases the limits, zoom and offset of `camera` toward it, and follows the target, or its point on the rail, with `camera:follow`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local rig = graphics2d.newCameraRig({rail = {0, 600, 4000, 600}})
local player = {x = 0, y = 500}

scene.push({
    update = function(self, dt)
        player.x = player.x + 100 * dt
        rig:update(camera, player.x, player.y, dt)
    end,
})
```

## PixelCanvas

A `PixelCanvas` renders pixel art at a low resolution and draws it scaled up, scrolling smoothly while every pixel stays whole, as the [rendering guide](../graphics/rendering.md#cameras-and-parallax) explains.

### graphics2d.newPixelCanvas(width, height, options)

Returns a new `PixelCanvas` of `width` by `height` pixels. The option `smooth`, `true` by default, shifts the image by the part of a pixel that the camera lies between. A size under one pixel raises `A pixel canvas needs a width and a height of one pixel or more.`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local pixels = graphics2d.newPixelCanvas(320, 180)
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        camera.x = camera.x + 7 * dt
    end,
    render = function(self)
        pixels:begin(camera, {clear = '#FF101418'})
        graphics2d.drawRect({0, 0, 16, 16}, '#FFFFD040')
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        local area = graphics2d.canvasBounds()
        pixels:draw(area.x, area.y, area.width, area.height)
    end,
})
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width`, `height` | integer | read | The size in pixels. |
| `texture` | Texture | read | The image, one pixel larger on every side. |

```lua
local graphics2d = require('haylen.graphics2d')

local pixels = graphics2d.newPixelCanvas(320, 180, {smooth = false})
print(pixels.width, pixels.height, pixels.texture.width)
```

### canvas:begin(camera, options)

Opens a render target canvas of the image through `camera` at whole pixels, with the options of [`graphics2d.beginTarget`](#graphics2dbegintargettarget-camera-options). The viewport of the camera is left out.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local pixels = graphics2d.newPixelCanvas(160, 90)
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        pixels:begin(camera, {sort = 'y'})
        graphics2d.drawCircle(0, 0, 8, '#FFFFFFFF')
    end,
})
```

### canvas:draw(x, y, width, height, order)

Draws the image over the rectangle `x`, `y`, `width`, `height` of the open canvas, such as the screen, with an optional [draw order](#draw-order).

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local pixels = graphics2d.newPixelCanvas(160, 90)

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        pixels:draw(0, 0, 1920, 1080, {layer = -1})
    end,
})
```

### canvas:fraction()

Returns the part of a pixel, on each axis, that the camera lay between at the last `begin`, which the draw shifts the image by.

```lua
local graphics2d = require('haylen.graphics2d')

local pixels = graphics2d.newPixelCanvas(160, 90)
print(pixels:fraction())
```

## Parallax

A `Parallax` is a layer that scrolls at its own rate as the camera moves, which suggests depth, for any texture and outside Tiled maps too. It draws its texture at its position moved by its offset, and repeats it on the axes that repeat until it covers the part of the world that the camera shows. Vector and rectangle properties return copies.

### graphics2d.newParallax(texture, properties)

Returns a new `Parallax` that draws `texture`. The table `properties` is optional and holds properties of the layer from the table below, each assigned through its setter, so a misspelled key raises `The type "haylen.Parallax" has no writable property "<key>".`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local stripes = graphics.newRenderTarget(256, 128)
local clouds = graphics2d.newParallax(stripes.texture, {scrollScale = {0.3, 0.3}, repeatX = true, autoscroll = {-20, 0}, color = '#C0FFFFFF'})
local stripeCamera = graphics2d.newCamera()
stripeCamera.position = {128, 64}

scene.push({
    update = function(self, dt)
        camera.x = camera.x + 120 * dt
        clouds:update(dt)
    end,
    render = function(self)
        graphics2d.beginTarget(stripes, stripeCamera, {clear = '#00000000'})
        graphics2d.drawCircle(80, 64, 50, '#FFFFFFFF')
        graphics2d.drawCircle(150, 70, 40, '#FFFFFFFF')

        graphics2d.beginWorld(camera)
        clouds:draw(camera, {layer = -1})
        graphics2d.drawRect({-2000, 100, 4000, 400}, '#FF3A7D44')
    end,
})
```

### Properties

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | the argument | The texture to draw. |
| `source` | Rect | empty | The region of the texture in pixels. An empty rectangle covers the whole texture. |
| `position` | Vec2 | `{0, 0}` | Where the layer sits in the world while the camera shows the world origin. |
| `size` | Vec2 | `{0, 0}` | The drawn size of one copy, or the size of the source when zero. |
| `scrollScale` | Vec2 | `{1, 1}` | How much the layer follows the world on each axis. 1 moves with the world, 0 stays still on the screen, and values between look farther away. Values above 1 look closer than the world. |
| `repeatX`, `repeatY` | boolean | `false` | Repeat the texture on that axis to fill the view. |
| `repeatSize` | Vec2 | `{0, 0}` | The distance between copies, or the drawn size when zero. |
| `autoscroll` | Vec2 | `{0, 0}` | The world units per second that the layer scrolls by itself, such as drifting clouds. |
| `limits` | Rect or nil | `nil` | The camera positions in world units where the layer scrolls. Outside them the layer stays as it was at the edge. |
| `color` | Color | `'#FFFFFFFF'` | Multiplies the texture color. |

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local hills = graphics2d.newParallax(graphics.whiteTexture(), {size = {512, 128}})
hills.source = {0, 0, 1, 1}
hills.position = {0, -200}
hills.scrollScale = {0.2, 1}
hills.repeatX = true
hills.repeatY = false
hills.repeatSize = {600, 128}
hills.limits = {-4000, -1000, 8000, 2000}
hills.color = '#FF6080A0'
print(hills.texture.width, hills.autoscroll.x)
```

### parallax:update(dt)

Advances the autoscroll by `dt` seconds.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local clouds = graphics2d.newParallax(graphics.whiteTexture(), {autoscroll = {-20, 0}})

scene.push({
    update = function(self, dt)
        clouds:update(dt)
    end,
})
```

### parallax:scrolled()

Returns the distance that the autoscroll moved the layer so far, as a `Vec2`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local clouds = graphics2d.newParallax(graphics.whiteTexture(), {autoscroll = {-20, 0}})
clouds:update(2)
print(clouds:scrolled().x)
```

### parallax:offset(camera)

Returns how far the layer is moved from where the world would put it, as `x, y` in world units. The offset also moves anything else with the layer, such as a baked static batch drawn with `graphics2d.drawStatic(batch, x, y)`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local baked = graphics2d.newSpriteBatch(graphics.whiteTexture())
for index = 0, 20 do
    baked:add({x = index * 180, y = 180, width = 12, height = 80, color = '#FF203020'})
end
local fence = baked:bake()
local fenceLayer = graphics2d.newParallax(graphics.whiteTexture(), {scrollScale = {1.3, 1}})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        local x, y = fenceLayer:offset(camera)
        graphics2d.drawStatic(fence, x, y, {layer = 1})
    end,
})
```

### parallax:draw(camera, order)

Draws the layer in the open canvas, moved by how `camera` sees it, with the repeated axes covering the whole area that the canvas shows, a render target canvas included. The table `order` is an optional [draw order](#draw-order).

A layer without a texture raises `A parallax layer needs a texture to draw.`, and a repeated copy without a size raises `A parallax layer needs a positive size to repeat.`. A position or scroll that is not finite raises `A parallax layer needs a finite position and scroll.`. Copies so small that more than 65536 of them would fill the area raise `A parallax layer would draw more than 65536 copies to fill the view. Give it a larger size or repeat size.`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local hills = graphics.newRenderTarget(512, 256)
local far = graphics2d.newParallax(hills.texture, {position = {0, -200}, scrollScale = {0.2, 1}, repeatX = true, color = '#FF6080A0'})
local near = graphics2d.newParallax(hills.texture, {position = {0, -60}, scrollScale = {0.6, 1}, repeatX = true, repeatSize = {700, 256}})
local hillCamera = graphics2d.newCamera()
hillCamera.position = {256, 128}

scene.push({
    update = function(self, dt)
        camera.x = camera.x + 150 * dt
    end,
    render = function(self)
        graphics2d.beginTarget(hills, hillCamera, {clear = '#00000000'})
        graphics2d.drawCircle(256, 300, 220, '#FFFFFFFF')

        graphics2d.beginWorld(camera)
        far:draw(camera, {layer = -2})
        near:draw(camera, {layer = -1})
        graphics2d.drawRect({camera.x - 2000, 150, 4000, 400}, '#FF3A7D44')
    end,
})
```

## Sprite

A sprite is one textured quad. Draw a texture once with `graphics2d.draw`, or keep a `Sprite` value that describes the quad and draws it with `sprite:draw()`.

### graphics2d.draw(texture, x, y, options)

Draws `texture` once with its pivot at `x`, `y`. The table `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `source` | Rect | the whole texture | The region of the texture in pixels. |
| `width`, `height` | number | `0` | The drawn size before scaling. When both are 0, the size of the source is used. |
| `scaleX`, `scaleY` | number | `1` | Multiply the drawn size. |
| `pivotX`, `pivotY` | number | `0.5` | The point of the sprite that sits at `x`, `y` and that the rotation turns around. |
| `rotation` | number | `0` | The rotation in radians. |
| `tilt` | number | `0` | Lays the turned sprite on a plane that leans away from the viewer by this angle in radians, which shortens its height on the screen by the cosine of the angle. So a circle on the ground spins as an ellipse instead of turning the ellipse. |
| `color` | Color | `'#FFFFFFFF'` | Multiplies the texture color. |
| `flash` | Color | `'#00000000'` | Mixes the result toward this color by its alpha, for hit flashes. |
| `flipHorizontal`, `flipVertical` | boolean | `false` | Mirror the image horizontally or vertically. |
| `flipDiagonal` | boolean | `false` | Mirrors the image across its diagonal from the top-left corner. With `flipHorizontal` and `flipVertical` it turns the image by quarter turns, the way Tiled turns tiles. |
| `partColors` | table | white parts | The colors of the parts when the draw has a `partMask`, as [recoloring by parts](#recoloring-by-parts) explains. |
| `effect` | table | none | Dissolves the sprite or draws an outline and a glow around it, as [sprite effects](#sprite-effects) explains. |
| `x`, `y` | number | the arguments | Replace the position arguments. |
| `layer`, `depth`, `sortOffset`, `sortId`, `visibility`, `blend`, `material`, `normalMap`, `specular`, `shininess`, `emission`, `lightMask`, `unshaded`, `distortion` | | | The [draw order](#draw-order). |

A texture that is not valid raises `Cannot draw a sprite without a texture.`.

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

A tilted sprite lies on the ground under a character, such as a magic circle that keeps turning flat:

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local circle = assets.texture('effects/magic-circle.png', {filter = 'linear'})
local turned = 0

scene.push({
    update = function(self, dt)
        turned = turned + dt
    end,
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.draw(circle, 400, 420, {width = 240, height = 240, rotation = turned, tilt = 1.1, blend = 'additive'})
    end,
})
```

### graphics2d.newSprite(texture, properties)

Returns a new `Sprite` of `texture`. The table `properties` is optional and holds properties of the sprite from the table below, each assigned through its setter, so a misspelled key raises `The type "haylen.Sprite" has no writable property "<key>".`.

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

### Properties

Vector and color properties return copies, so assign a new value instead of changing a field of the returned value.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | the argument | The texture to draw. |
| `x`, `y` | number | `0` | The position of the pivot. |
| `position` | Vec2 | `{0, 0}` | The same position as a vector. |
| `width`, `height` | number | `0` | The drawn size before scaling. When both are 0, the size of the source is used. |
| `scaleX`, `scaleY` | number | `1` | Multiply the drawn size. |
| `pivotX`, `pivotY` | number | `0.5` | The pivot as a share of the size. |
| `rotation` | number | `0` | The rotation in radians. |
| `tilt` | number | `0` | Lays the turned sprite on a plane that leans away from the viewer, as the `tilt` of `graphics2d.draw` does. |
| `color` | Color | `'#FFFFFFFF'` | Multiplies the texture color. |
| `flash` | Color | `'#00000000'` | Mixes the result toward this color by its alpha. |
| `source` | Rect | empty | The region of the texture in pixels. An empty rectangle covers the whole texture. |
| `flipHorizontal`, `flipVertical` | boolean | `false` | Mirror the image. |
| `flipDiagonal` | boolean | `false` | Mirrors the image across its diagonal from the top-left corner. |
| `layer`, `depth`, `sortOffset`, `sortId`, `visibility`, `blend` | | `0`, `0`, `0`, `0`, `1`, `'alpha'` | The [draw order](#draw-order). |
| `material` | Material or nil | `nil` | The custom shader of the sprite. |
| `partMask` | Texture or nil | `nil` | The mask that [recolors](#recoloring-by-parts) the parts of the sprite. |
| `partColors` | table | white parts | The colors of the parts, as `{red = Color, green = Color, blue = Color, yellow = Color}` after the colors of the mask, each white when the table leaves it out. Reading it returns all four. |
| `effect` | table | no effect | The [sprite effect](#sprite-effects), assigned as a table with any of its keys. Reading it returns every key. |
| `normalMap`, `specular`, `shininess`, `emission`, `lightMask`, `unshaded` | | `nil`, `0`, `32`, `0`, `1`, `false` | The lighting in lit canvases, as the [draw order](#draw-order) describes. |
| `distortion` | number | `0` | Bends the image of the canvas instead of drawing colors, as [distortion](#distortion) explains. |

Reading or writing any other key raises `The type "haylen.Sprite" has no member "<key>".` or `The type "haylen.Sprite" has no writable property "<key>".`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local hero = graphics2d.newSprite(assets.texture('sprites/hero.png', {filter = 'linear'}))
hero.position = {500, 300}
hero.width, hero.height = 96, 96
hero.scaleX, hero.scaleY = 1.5, 1.5
hero.pivotX, hero.pivotY = 0.5, 1
hero.tilt = 0
hero.flipVertical = false
hero.flipDiagonal = false
hero.depth = hero.y
hero.sortOffset = 0
hero.sortId = 7
hero.visibility = 1
hero.blend = 'alpha'
hero.material = nil
hero.partMask = nil
hero.normalMap = assets.texture('sprites/hero-normal.png', {filter = 'linear'})
hero.specular = 0.5
hero.shininess = 48
hero.emission = 0
hero.lightMask = 1
hero.unshaded = false
hero.distortion = 0
print(hero.x, hero.texture.width, hero.partColors.red:toHex(), hero.effect.dissolve)
```

### sprite:draw()

Draws the sprite in the open canvas with its own draw order.

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

## Sprite effects

The `effect` of a sprite dissolves it into noise with a colored edge, and draws an outline and a glow around its visible pixels. It all runs on the GPU, so sprites of one texture share a draw call whatever their effects. The quad of the sprite grows by the outline and the glow, which spill past its frame. The [rendering guide](../graphics/rendering.md#sprite-effects) explains how the effect draws. The table takes these keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `dissolve` | number | `0` | How much of the sprite has dissolved, from 0 for none to 1 for all of it. The outline and the glow fade with it. |
| `dissolveEdge` | number | `0.08` | The share of the noise along the dissolving border that takes the dissolve color, from 0 to 1. |
| `dissolveSize` | number | `6` | The size of the cells of the noise in pixels of the texture, from 1, which dissolves pixel by pixel, to 64. |
| `dissolveColor` | Color | `'#00000000'` | The color of the dissolving border, such as glowing embers. |
| `outlineWidth` | number | `0` | The width of the outline in pixels of the texture, from 0 to 64. |
| `outlineColor` | Color | `'#FFFFFFFF'` | The color of the outline. |
| `glowSize` | number | `0` | How far the glow reaches in pixels of the texture, from 0 to 64. |
| `glowColor` | Color | `'#FFFFFFFF'` | The color of the glow, whose alpha sets its strength. |

The `color` and `flash` of the sprite apply to the sprite itself, before the outline and the glow. Sprite batches and baked batches draw without effects. These errors come from effects:

| Error | Cause |
| --- | --- |
| `A sprite effect needs a dissolve and a dissolve edge from 0 to 1, a dissolve size from 1 to 64 and an outline width and a glow size from 0 to 64.` | A value is out of range. |
| `A sprite takes a part mask or an effect, not both.` | The sprite also has a `partMask`. |
| `A sprite with an effect does not take a material.` | The sprite also has a `material`. |

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
        hero.effect = {dissolve = dissolve, dissolveColor = '#FFFF8020', dissolveEdge = 0.1, dissolveSize = 4}
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        hero:draw()
        graphics2d.draw(hero.texture, 200, 0, {effect = {outlineWidth = 3, outlineColor = '#FFFFFFFF', glowSize = 16, glowColor = '#C040C0FF'}})
    end,
})
```

## Recoloring by parts

A white or gray sprite drawn with a `partMask` takes a color for each part that the mask marks in red, green, blue or yellow. It keeps its shading, so one set of images makes every outfit of a character. The [rendering guide](../graphics/rendering.md#recoloring-by-parts) explains how the colors mix. The `partColors` of a sprite, or of each sprite of a batch, name the color of each mask color, as `{red = Color, green = Color, blue = Color, yellow = Color}`.

The keys `partMask` and `partColors` work with `sprite:draw()`, `graphics2d.draw`, `batch:draw` and `graphics2d.drawBatch` with sprite tables. A material with a part mask raises `A draw with a part mask does not take a material.`, `graphics2d.drawStatic` raises `A baked sprite batch draws without a part mask. Draw a sprite batch to recolor its sprites.`, and `graphics2d.drawVector` raises `A vector image draws without a part mask.`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local base = assets.texture('characters/villager.png', {filter = 'linear'})
local mask = assets.texture('characters/villager-mask.png', {filter = 'linear'})
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

## Distortion

A draw with a `distortion` above 0 bends the image of its canvas instead of drawing colors. Its coverage is the alpha of its texture times its color and the distortion. The coverage adds up in the distortion map of the canvas, and the composite moves every pixel down the slope of the map. The `distortion` of the [post-process options](#post-process-options) sets how far, where the coverage goes from nothing to full within 8 units. This makes these effects:

- A ring pushes the image outward at its outer edge and inward at its inner edge, like a shock wave.
- A soft dot bends the image around its edge, like a lens.
- Soft blobs that rise and fade shimmer like heat above a fire. The [rendering guide](../graphics/rendering.md#distortion) explains the map.

Only world and render target canvases with lighting or post-processing have the map, and other canvases skip distortion draws. Only sprites, sprite batches, nine-slices, rectangles, lines, shapes and meshes make them, and particle emitters take the same `distortion` key. Other draws raise `Only sprites, sprite batches, nine-slices, shapes and meshes draw as distortion.`, and `graphics2d.drawStatic` raises `A baked sprite batch draws without a distortion. Draw a sprite batch to distort with its sprites.`.

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

## SpriteBatch

A `SpriteBatch` holds many sprites that share one texture, or up to 8 textures, and draws them in one call. The sprites live in C++, and scripts change only the sprites that move. Indices count from 1. The [rendering guide](../graphics/rendering.md#sprites-sprite-batches-and-static-batches) compares batches with single sprites and static batches.

### graphics2d.newSpriteBatch(texture)

Returns a new `SpriteBatch` of `texture`. The argument may also be a list of up to 8 textures, whose sprites still draw in one call, as [batches of several textures](#batches-of-several-textures) explain. An empty list raises `A sprite batch needs a texture.`.

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

### Sprite tables

Each sprite of a batch is a table with these keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | The position of the pivot. |
| `width`, `height` | number | `0` | The drawn size. When both are 0, the size of the source is used, which is the whole texture without a source, like `graphics2d.draw`. |
| `source` | Rect | the whole texture | The region of the texture in pixels. |
| `pivotX`, `pivotY` | number | `0.5` | The pivot as a share of the size. |
| `rotation` | number | `0` | The rotation in radians. |
| `tilt` | number | `0` | Lays the turned sprite on a plane that leans away from the viewer, as the `tilt` of `graphics2d.draw` does. |
| `color` | Color | `'#FFFFFFFF'` | Multiplies the texture color. |
| `flash` | Color | `'#00000000'` | Mixes the result toward this color by its alpha. |
| `flipHorizontal`, `flipVertical` | boolean | `false` | Mirror the image. |
| `flipDiagonal` | boolean | `false` | Mirrors the image across its diagonal from the top-left corner. |
| `partColors` | table | white parts | The colors of the parts of the sprite when the batch draws with a `partMask`, as [recoloring by parts](#recoloring-by-parts) explains. A batch keeps room for part colors only once a sprite has some. |
| `texture` | integer | `1` | The texture of the batch that the sprite draws from, counted from 1. A sprite without a size takes the size of its texture. |
| `data` | table | `{0, 0, 0, 0}` | Up to four numbers that the material of the draw reads as `haylen_data`, as [batches of several textures](#batches-of-several-textures) explain. A batch keeps room for the texture and the data only once a sprite has some. |

### Sprite fields

Float buffers name the numbers of a sprite with these fields:

| Fields | Meaning |
| --- | --- |
| `x`, `y`, `width`, `height`, `rotation`, `tilt`, `pivotX`, `pivotY` | The keys of the same names of a sprite table. |
| `red`, `green`, `blue`, `alpha` | The channels of the color, from 0 to 1. |
| `sourceX`, `sourceY`, `sourceWidth`, `sourceHeight` | The source rectangle in pixels of the texture. |
| `texture` | The texture of the batch, counted from 1. |
| `data1`, `data2`, `data3`, `data4` | The numbers of the data. |

An unknown name raises an error that ends with `(unknown value '<name>')`, and an empty list raises `A sprite layout needs at least one field.`.

### graphics2d.drawBatch(texture, sprites, order)

Draws a list of sprite tables that share `texture` as one batch, without keeping them in a `SpriteBatch`. The argument `texture` may also be a list of textures, which each sprite picks from with its `texture` key. Each sprite table takes the keys of [sprite tables](#sprite-tables). The table `order` is an optional [draw order](#draw-order). An empty list draws nothing. Use a `SpriteBatch` instead when most sprites stay the same from frame to frame.

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

Draws the sprites that a float buffer of [`haylen.collections`](collections.md#float-buffers) holds, without a table for each sprite. The argument `texture` may also be a list of textures, which the `texture` field of each sprite picks from. The table `layout` is a sprite table, with the keys of [sprite tables](#sprite-tables) but `partColors`, that every sprite starts from. It adds `fields`, the [sprite fields](#sprite-fields) that each sprite takes from the buffer, in order. The buffer holds as many sprites as it has whole groups of fields. A layout without `fields` raises `A "drawBatch" call with a float buffer needs the fields each sprite takes, such as "fields = {'x', 'y'}".`. The [performance section of the Lua guide](../core/lua.md#performance) compares it with sprite tables.

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

### batch:add(sprite)

Appends a [sprite table](#sprite-tables) and returns its index.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
local index = batch:add({x = 10, y = 10, width = 4, height = 4, color = '#FFFF0000'})
```

### batch:set(index, fields)

Changes the sprite at `index`. Only the keys present in `fields` change, and the rest keep their values. An index outside the batch raises a `bad argument` error that ends with `sprite index out of range`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
local index = batch:add({x = 0, y = 0, width = 8, height = 8})
batch:set(index, {x = 120, rotation = 0.5})
```

### batch:get(index)

Returns a copy of the sprite at `index` as a sprite table with every key of [sprite tables](#sprite-tables). A sprite added without a size reports the size it draws with. An index outside the batch raises a `bad argument` error that ends with `sprite index out of range`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
local index = batch:add({x = 10, y = 20, width = 8, height = 8, flipHorizontal = true})
local sprite = batch:get(index)
print(sprite.x, sprite.width, sprite.flipHorizontal, sprite.color:toHex())
```

### batch:reserve(count)

Makes room for `count` sprites at once, so a batch filled in a loop does not grow step by step. A batch holds at most 16777216 sprites, and a larger `count`, here or in `batch:resize`, raises `A sprite batch holds at most 16777216 sprites.`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:reserve(1000)
for index = 1, 1000 do
    batch:add({x = index, y = 0, width = 1, height = 1})
end
```

### batch:resize(count, sprite)

Sets the number of sprites to `count` in one call. New sprites copy the sprite table `sprite`, which takes the keys of [sprite tables](#sprite-tables) but `partColors`, `texture` and `data`, or take the size of the texture without one. Sprites past `count` go away. It is the fast way to fill a large batch that a float buffer then moves. A negative count raises a `bad argument` error that ends with `the count cannot be negative`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:resize(10000, {width = 4, height = 4, color = '#FF80C0FF'})
print(batch:size()) -- 10000
```

### batch:remove(index)

Removes the sprite at `index`, and the sprites after it move down by one index. An index outside the batch raises a `bad argument` error that ends with `sprite index out of range`.

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

### batch:writeFields(buffer, fields, first)

Copies the [sprite fields](#sprite-fields) named in the list `fields` from a float buffer into the sprites, from the sprite `first`, 1 by default, on. Each sprite takes one value for each field, in order, and the copy stops at the end of the buffer or of the batch. The other fields of the sprites keep their values, so a batch built once moves every frame with one call. A `first` below 1 raises an error that contains `sprites are numbered from one`. A `first` more than one past the last sprite raises `Sprite batch index is out of range.`, and a `first` just past the last sprite copies nothing.

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

Copies the named fields of the sprites into a float buffer, the other way around from `batch:writeFields`, with the same rules for `first`.

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

Draws every sprite in the open canvas as one batch. The table `order` is an optional [draw order](#draw-order). Large batches convert their sprites on worker threads.

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

Copies the sprites into a [`StaticSpriteBatch`](#staticspritebatch) on the GPU, which draws every frame without uploading sprite data again. Later changes to the batch do not reach the baked copy. An empty batch raises `A static batch needs a texture and at least one sprite.`. A batch with several textures or with sprite data raises `A sprite batch with several textures or sprite data does not bake. Draw it as a sprite batch instead.`.

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

## Batches of several textures

A sprite batch draws from up to 8 textures in one draw call. So sprites of separate images that mix in one layer, such as heroes, rocks and trees that a world sorted by y interleaves, need no atlas to share a batch. The functions `graphics2d.newSpriteBatch` and `graphics2d.drawBatch` take a list of textures. Each sprite names the one it draws from with its `texture` key, counted from 1, or with the `texture` field of a float buffer. A sprite without a size takes the size of its own texture.

Every sprite also has `data`, four numbers that the [material](#material) of the draw reads in its shader as `haylen_data`. So one material and one draw call give every sprite its own phase, strength, color or frame, as the [shader guide](../graphics/shaders.md#sprite-data) describes. A batch with several textures, or with data and a material, draws with the batch program, which reads a second record for each sprite. The data of a batch of one texture without a material changes nothing and costs nothing.

The textures of a batch draw with one sampler, so they share their filter, wrap and mipmaps. A batch with several textures draws without a part mask, a normal map or a distortion, and it does not bake. These errors stop such a batch:

| Error | Cause |
| --- | --- |
| `A sprite batch draws from at most 8 textures.` | The list holds more than 8 textures. |
| `The textures of a sprite batch share their filter, wrap and mipmaps, since they draw with one sampler.` | The textures sample differently. |
| `A sprite draws from the texture <number> of its batch, which has <count> textures.` | A sprite table names a texture past the list. |
| `A sprite of a batch names a texture past the <count> textures of the batch.` | A float buffer names a texture past the list. |
| `The data of a sprite holds up to 4 numbers, and this one has <count>.` | The `data` of a sprite table holds more than four numbers. |
| `A sprite batch with several textures draws without a part mask.` | The order has a `partMask`. |
| `A sprite batch with several textures draws without a normal map.` | The order has a `normalMap` in a lit canvas. |
| `A sprite batch with several textures or sprite data draws without a distortion.` | The order has a `distortion`. |

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

-- The shader `content/shaders/sway.glsl` bends each sprite by `haylen_data.x` as its phase and `haylen_data.y` as its strength.
local sway = graphics2d.newMaterial(assets.shader('shaders/sway.shader'))
local images = {assets.texture('meadow/grass.png'), assets.texture('meadow/flower.png'), assets.texture('meadow/bush.png')}
local meadow = graphics2d.newSpriteBatch(images)
for index = 1, 600 do
    meadow:add({x = math.random(0, 1920), y = math.random(0, 1080), pivotY = 1, texture = index % 3 + 1, data = {math.random() * 6.28, 0.05}})
end

scene.push({
    render = function(self)
        sway:set('time', haylen.elapsed())
        graphics2d.beginScreen()
        meadow:draw({material = sway})
    end,
})
```

## StaticSpriteBatch

A `StaticSpriteBatch` is the copy on the GPU that `batch:bake()` makes. It draws in one draw call without uploading anything, and its offset moves it without baking again.

### graphics2d.drawStatic(batch, x, y, order)

Draws a static batch. The numbers `x` and `y` shift the whole batch without baking it again and are 0 by default, which lets parallax layers reuse one batch. The table `order` is the fourth argument, so pass the offsets before it. A draw order in place of `x` raises a `number expected` error.

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

### staticBatch:size()

Returns the number of sprites in the batch.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:add({x = 100, y = 100, width = 50, height = 50})
print(batch:bake():size())
```

### staticBatch:bounds()

Returns a `Rect` that covers every sprite before rotation.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:add({x = 100, y = 100, width = 50, height = 50})
print(batch:bake():bounds().width)
```

### staticBatch:texture()

Returns the `Texture` the batch draws with.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')

local batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
batch:add({x = 100, y = 100, width = 50, height = 50})
print(batch:bake():texture() == graphics.whiteTexture())
```

## TileLayer

A `TileLayer` shows a frame in each of its cells: a region of one of its textures named by an id from 1, where 0 is an empty cell. Cells are kept in chunks of `chunkSize` by `chunkSize` cells in a hash map, so negative cells work, and a sparse or endless world costs memory only where cells were set. Each chunk bakes into one static batch for each texture its cells use, the first time it draws after one of its cells changed. So changing a cell bakes only its chunk again, and a draw leaves out the chunks outside the area the open canvas shows. Chunks draw row by row from the top. Maps made in the Tiled editor use [`haylen.tiled`](tiled.md) instead, which adds orientations, animated tiles, objects and collision.

### graphics2d.newTileLayer(textures, options)

Returns a new `TileLayer`, a grid of cells that code fills, such as a generated cave or a streamed world. The argument `textures` is a texture or a list of them, and `options` is an optional table:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `cellWidth`, `cellHeight` | number | `32` | The size of a cell in world units, which must be positive. |
| `chunkSize` | integer | `32` | The cells along each side of a chunk, from 1 to 256. |

Other keys raise `Unknown option "<key>".`, no texture raises `A tile layer needs at least one texture.`, and a size outside the ranges raises `A tile layer needs a positive cell size and a chunk size from 1 to 256 cells.`.

```lua
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local layer = graphics2d.newTileLayer(assets.texture('tiles/cave.png'), {cellWidth = 16, cellHeight = 16, chunkSize = 32})
layer:defineFrame(1, {source = {0, 0, 16, 16}})
layer:defineFrame(2, {source = {16, 0, 16, 16}})

local ids = collections.newFloatBuffer(256 * 256)
for index = 1, #ids do
    ids[index] = math.random() < 0.4 and 1 or 2
end
layer:setCells(-128, -128, 256, ids)

local camera = graphics2d.newCamera()
scene.push({
    update = function(self, dt)
        camera.x = camera.x + 60 * dt
        if math.random() < 0.1 then
            layer:setCell(math.random(-128, 127), math.random(-128, 127), 0)
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        layer:draw({layer = 1})
    end,
})
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `chunkCount` | integer | read | The chunks that hold at least one cell. |
| `batchCount` | integer | read | The static batches that the baked chunks hold. |

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local layer = graphics2d.newTileLayer(assets.texture('tiles/cave.png'))
layer:defineFrame(1, {source = {0, 0, 32, 32}})
layer:setCell(0, 0, 1)
print(layer.chunkCount, layer.batchCount)
```

### layer:defineFrame(id, frame)

Defines or redefines the frame `id`, from 1 to 1048576, with a table of these keys:

| Key | Default | Meaning |
| --- | --- | --- |
| `texture` | `1` | The position of a texture in the list of the layer. |
| `source` | required | The `Rect` of the frame in that texture. |
| `width`, `height` | the size of `source` | The size it draws at. |
| `offsetX`, `offsetY` | `0` | Move the frame from the top-left corner of its cell, so a tall tree stands on its cell. |

Redefining a frame bakes the chunks that show it again. An id outside the range raises `A frame of a tile layer has an id from 1 to 1048576, not <id>.`. A texture below 1 raises a `bad argument` error that ends with `textures are numbered from one`, and a texture past the list raises `A frame of a tile layer uses one of its <count> textures.`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local layer = graphics2d.newTileLayer({assets.texture('tiles/ground.png'), assets.texture('tiles/trees.png')})
layer:defineFrame(1, {source = {0, 0, 32, 32}})
layer:defineFrame(2, {texture = 2, source = {0, 0, 32, 64}, offsetY = -32})
```

### layer:setCell(column, row, id)

Sets the frame of a cell, where 0 empties it. A chunk that loses its last cell is removed. A frame that was never defined raises `The tile layer has no frame <id>.`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local layer = graphics2d.newTileLayer(assets.texture('tiles/cave.png'))
layer:defineFrame(1, {source = {0, 0, 32, 32}})
layer:setCell(-3, 5, 1)
layer:setCell(-3, 5, 0)
```

### layer:cell(column, row)

Returns the frame of a cell, 0 for an empty one.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local layer = graphics2d.newTileLayer(assets.texture('tiles/cave.png'))
layer:defineFrame(1, {source = {0, 0, 32, 32}})
layer:setCell(2, 2, 1)
print(layer:cell(2, 2), layer:cell(9, 9))
```

### layer:setCells(column, row, width, buffer)

Sets the cells of a rectangle whose top-left cell is `column`, `row` from a [`FloatBuffer`](collections.md#float-buffers) of frame ids, in rows of `width` cells. It fills a large area with one call across the Lua boundary. A buffer that is not whole rows raises `The frames of a rectangle of a tile layer come in whole rows of its width.`.

```lua
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')

local layer = graphics2d.newTileLayer(assets.texture('tiles/cave.png'))
layer:defineFrame(1, {source = {0, 0, 32, 32}})
local ids = collections.newFloatBuffer(16, 1)
layer:setCells(0, 0, 4, ids)
```

### layer:clear()

Empties every cell.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local layer = graphics2d.newTileLayer(assets.texture('tiles/cave.png'))
layer:clear()
```

### layer:draw(order, offsetX, offsetY)

Draws the chunks in view into the open canvas with an optional [draw order](#draw-order), moved by an offset in world units, which also gives the layer parallax.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local layer = graphics2d.newTileLayer(assets.texture('tiles/cave.png'))
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        layer:draw({layer = -1}, camera.x * 0.5, 0)
    end,
})
```

## Shapes and lines

Rectangles and lines draw with the white texture and keep hard edges. Circles, ellipses, points, rings, arcs and shapes draw as one quad each, whose edge the shader covers exactly. So the edge fades over one pixel of the screen or of the render target at any radius, zoom and density. Polygons and strokes fade their edges over a fringe one pixel wide. The [rendering guide](../graphics/rendering.md#shapes-and-smooth-edges) explains how. Every function here takes an optional [draw order](#draw-order).

### graphics2d.drawRect(rect, color, order)

Fills `rect` with `color`. The `x` and `y` of the rectangle are its top-left corner.

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

Fills a circle centered on `x`, `y`. A radius of 0 or less draws nothing.

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

### graphics2d.drawEllipse(x, y, radiusX, radiusY, color, order)

Fills an ellipse centered on `x`, `y` with the horizontal radius `radiusX` and the vertical radius `radiusY`. Its edge fades over one pixel like the edge of a circle. A radius of 0 or less draws nothing. A [shape](#graphics2ddrawshaperect-options) of the kind `'ellipse'` also turns and takes a border, a sector, a softness and a texture.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawEllipse(300, 300, 120, 50, '#FF4080FF')
        graphics2d.drawEllipse(300, 420, 40, 12, '#60000000')
    end,
})
```

### graphics2d.drawPoints(points, options)

Fills a circle at every point of `points`, such as the stars of a sky, the dots of a chart or the sparks of a simulation. The points are a flat list `{x1, y1, x2, y2, ...}` or a [`FloatBuffer`](collections.md#float-buffers) of the same pairs. Every point is one quad with the smooth edge of a circle, and all of them go into one draw call with the other shapes. The table `options` is optional and also takes the keys of the [draw order](#draw-order):

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `radius` | number | `1` | The radius of every point. A radius of 0 or less draws nothing. |
| `color` | Color | white | The color of every point. |
| `colors` | list of Color | none | One color for each point, in place of `color`. |
| `count` | integer | every point | The number of points to draw from the start, so a buffer of a fixed size holds points that grow. |

Points with both `color` and `colors` raise `Points take either one "color" or "colors" for each point.`, and `colors` of another count raise `Points take one color for each of their <count> points.`. A `count` past the points given raises `The points draw <count> of the <total> they were given.`. The points raise the same errors as the points of [`graphics2d.drawStroke`](#graphics2ddrawstrokepoints-options).

```lua
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local scene = require('haylen.scene')

local random = m.random(7)
local stars = collections.newFloatBuffer(400)
for index = 1, 400, 2 do
    stars[index], stars[index + 1] = random:range(0, 1280), random:range(0, 720)
end

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawPoints(stars, {radius = 1.5, color = '#C0FFFFFF'})
        graphics2d.drawPoints({200, 500, 260, 440, 320, 470}, {radius = 6, colors = {'#FFFF6060', '#FF60FF90', '#FF6090FF'}})
    end,
})
```

### graphics2d.drawRing(x, y, radius, thickness, color, order)

Draws a circle outline `thickness` units wide, whose middle line lies at `radius` from the center. A ring thicker than twice its radius fills the whole circle, and a thickness of 0 or less draws nothing.

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

Draws the part of a ring that goes from `startAngle` to `endAngle`, in radians, with square ends. Angle 0 points right, and angles grow clockwise. A span of a full turn or more draws the whole ring.

```lua
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local scene = require('haylen.scene')

local progress = 0.7

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        local start = -m.pi / 2
        graphics2d.drawArc(300, 300, 48, 10, start, start + m.tau * progress, '#FF40E0A0')
    end,
})
```

### graphics2d.drawShape(rect, options)

Draws a rectangle with rounded corners or an ellipse, turned around its center, which also draws pills, capsules, circles, rings and arcs. It fills the shape and draws a border along its edge in the same quad, so the fill and the border meet without a seam. The argument `rect` is the area of the shape before it turns, and `options` is optional and also takes the keys of the [draw order](#draw-order):

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kind` | string | `'rectangle'` | The kind `'rectangle'` rounds its corners by `radius`, and `'ellipse'` fills `rect` with an ellipse and leaves `radius` unused. |
| `radius` | number or table | `0` | The radius of every corner, or four radii from the top-left corner clockwise. A radius past half the shorter side rounds the whole end, so a radius of half the height makes a pill. |
| `rotation` | number | `0` | Turns the shape around the center of `rect`, in radians, clockwise. |
| `startAngle` | number | `0` | Where the sector that the sweep keeps starts, in radians clockwise from the x axis of the shape after its rotation. |
| `sweep` | number | `m.tau` | Keeps the sector that starts at `startAngle` and turns clockwise by this angle, with straight sides from the center, such as the part of a ring that shows progress or a triangle cut from a rectangle around its corners. A full turn keeps the whole shape. |
| `color` | Color | `'#FFFFFFFF'` | The fill. |
| `texture` | Texture | none | An image stretched over `rect` under the fill color, such as a portrait inside a circle or a [gradient](graphics.md#gradients), which turns with the shape. |
| `borderWidth` | number | `0` | The width of the border, which runs inside the edge, at most half the shorter side. |
| `borderColor` | Color | `'#00000000'` | The color of the border. A clear fill with a border draws an outline. |
| `softness` | number | `0` | Widens the fade of the edge to this many units, centered on the edge, such as for a soft shadow. |

The edge fades over one pixel of the destination, or over the softness when it is wider. A shape thinner than a pixel covers only the share of the pixels it crosses. A shape without a positive width and height draws nothing. The border of an ellipse follows its edge closely: it stays as wide as `borderWidth` on a round ellipse and within a few percent of it on a flat one. Shapes draw with the white texture through their own program, so neighbouring shapes merge into one draw call, and a shape with a `texture` draws in a call of that texture. A [material](#material) shades them through its `shape` program, where `uv` goes from 0 to 1 across the rectangle of the shape.

A negative radius, border width or softness raises `A shape needs corner radii, a border width and a softness of zero or more.`. A radius table of another length raises `The option "radius" of a shape takes one radius for every corner or four, from the top-left corner clockwise.`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local portrait = assets.texture('images/portrait.png', {filter = 'linear'})

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawShape({200, 208, 320, 120}, {radius = 16, color = '#80000000', softness = 24})
        graphics2d.drawShape({200, 200, 320, 120}, {radius = 16, color = '#FF232938', borderWidth = 3, borderColor = '#FF8FB0FF'})
        graphics2d.drawShape({240, 360, 240, 32}, {radius = 16, rotation = math.sin(haylen.elapsed()) * 0.3, color = '#FFF2B23A'})
        graphics2d.drawShape({600, 200, 120, 120}, {radius = 60, startAngle = -math.pi / 2, sweep = math.pi * 1.5, color = '#FF6FDCA0'})
        graphics2d.drawShape({600, 360, 160, 100}, {kind = 'ellipse', rotation = 0.3, color = '#00000000', borderWidth = 4, borderColor = '#FFF2B23A'})
        graphics2d.drawShape({800, 200, 96, 96}, {kind = 'ellipse', texture = portrait, borderWidth = 3, borderColor = '#FFFFFFFF'})
    end,
})
```

### graphics2d.drawPolygon(points, color, order)

Fills a simple polygon, convex or concave, given as a list of points in either winding. The fill stops half a pixel of the destination inside the outline, and a fringe fades it out to half a pixel outside. So its edges cover the pixels they cross like the edges of shapes. Fewer than three points draw nothing.

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

Draws line segments through `points`, `thickness` units wide. When `closed` is `true`, a last segment joins the final point to the first one.

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

### graphics2d.drawStroke(points, options)

Draws a line through `points`, with joins, caps, a width and a color for each point, a texture along its length and dashes. The points are a flat list `{x1, y1, x2, y2, ...}` or a [`FloatBuffer`](collections.md#float-buffers) of the same pairs. Points that repeat the point before them are dropped. In place of the points it takes a [`Spline`](math.md#spline), such as a Bézier curve. It flattens the spline so the curve stays within a quarter pixel of the destination at any zoom, closed when the spline is, with one `width` and one `color` and without `count`. The table `options` is optional and also takes the keys of the [draw order](#draw-order):

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `count` | integer | every point | The number of points to draw from the start, so a buffer of a fixed size holds a line that grows. |
| `width` | number | `1` | The width of the stroke. |
| `widths` | list or FloatBuffer | none | One width for each point, which replaces `width` and blends along each segment, such as a brush stroke or a tapered rope. |
| `color` | Color | white | The color of the stroke. |
| `colors` | list of Color | none | One color for each point, blended along each segment. |
| `join` | string | `'miter'` | How segments meet: `'miter'`, `'round'` or `'bevel'`. |
| `cap` | string | `'butt'` | How the ends look: `'butt'`, `'round'` or `'square'`, which reaches half the width past the end. |
| `miterLimit` | number | `4` | A miter longer than this many widths becomes a bevel. It must be 1 or more. |
| `closed` | boolean | `false` | Joins the last point to the first, without caps. A stroke of two points never closes. |
| `texture` | Texture | white | The image drawn along the stroke, from its left edge at the top of the image to its right edge at the bottom. |
| `textureMode` | string | `'stretch'` | The value `'stretch'` spans the image once over the whole length, and `'tile'` repeats it every `textureLength`, which takes a texture whose `wrap` is `'repeat'`. |
| `textureLength` | number | `0` | The length that one copy of the image covers in tile mode, or the width times the aspect of the image when 0. |
| `dashes` | list or FloatBuffer | none | Lengths drawn and left out in turn, repeated along the stroke, where a list of an odd count repeats twice. A dash of length 0 draws the dot of a round or square cap. |
| `dashOffset` | number | `0` | The distance into the dash pattern where the stroke starts. |

These errors come from strokes:

| Error | Cause |
| --- | --- |
| `A stroke along a spline draws the whole curve, without "count".` | A spline has a `count`. |
| `A stroke along a spline takes one width and one color, without widths or colors for each point.` | A spline has `widths` or `colors`. |
| `A stroke needs at least two points.` | Fewer than two points draw. |
| `The stroke draws <count> points of the <total> it was given.` | The `count` is past the points given. |
| `The points are x and y pairs, and <count> numbers leave one alone.` | The list holds an odd count of numbers. |
| `The points are a list of numbers or a float buffer.` | The points are another type. The same message names the `widths` or the `dashes`. |
| `The points are a list of numbers, and entry <index> is not a number.` | An entry of the list is not a number. The same message names the `widths` or the `dashes`. |
| `A stroke has one width for each of its <count> points, or none.` | The `widths` have another count. |
| `A stroke has one color for each of its <count> points, or none.` | The `colors` have another count. |
| `A stroke needs widths and a texture length of zero or more and a miter limit of 1 or more.` | A width or the texture length is negative, or the miter limit is under 1. |
| `A dash pattern needs lengths of zero or more that add up to more than zero, and a finite offset.` | The pattern has no length. |
| `A dash pattern makes more than 65536 dashes along the stroke.` | The pattern cuts the stroke too finely. |

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local scene = require('haylen.scene')

local rope = assets.texture('effects/rope.png', {wrap = 'repeat'})
local curve = m.spline({{100, 640}, {300, 520}, {500, 700}, {700, 580}}, {kind = 'bezier'})

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawStroke({100, 500, 300, 420, 500, 520, 700, 440}, {width = 12, join = 'round', cap = 'round', color = '#FF40C0FF'})
        graphics2d.drawStroke({100, 200, 400, 120, 700, 200}, {width = 16, texture = rope, textureMode = 'tile', layer = 1})
        graphics2d.drawStroke({100, 300, 700, 300}, {widths = {2, 20}, colors = {'#FFFFFF00', '#FFFFFFFF'}, dashes = {24, 12}, cap = 'round'})
        graphics2d.drawStroke(curve, {width = 6, cap = 'round', color = '#FFF2B23A'})
    end,
})
```

## Mesh

A mesh is a set of textured triangles. Draw triangles once with `graphics2d.drawMesh`, or keep a `Mesh` with its texture, its vertices and its triangles between draws. A deformation reads the rest positions once with `mesh:readPositions`. Then it writes the moved ones every frame with `mesh:setPositions`, one call each across the Lua boundary. A draw copies the vertices into the streaming buffer of the renderer without building anything again.

### graphics2d.drawMesh(texture, vertices, indices, order)

Draws textured triangles. The argument `texture` can be `nil` for solid colors. Each vertex is a table with `x`, `y`, `u`, `v` and `color`, where `u` and `v` are texture coordinates from 0 to 1. Every field is optional: `color` is white by default and the others 0. The list `indices` names vertices in groups of three, counted from 1. The table `order` is an optional [draw order](#draw-order). An index below 1 raises a `bad argument` error that ends with `mesh indices start at 1`, and an index past the last vertex raises `Mesh index refers to a vertex that does not exist.`.

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

### graphics2d.newMesh(texture, vertices, indices)

Returns a new `Mesh` that keeps its triangles in C++ between draws, with the vertices and indices that [`graphics2d.drawMesh`](#graphics2ddrawmeshtexture-vertices-indices-order) takes. The argument `texture` can be `nil` for solid colors. A count of indices that is not a multiple of three raises `A mesh lists its triangles as three indices each, and <count> is not a multiple of three.`, and an index past the last vertex raises `Mesh index refers to a vertex that does not exist.`.

```lua
local graphics2d = require('haylen.graphics2d')

local triangle = graphics2d.newMesh(nil, {{x = 0, y = 0}, {x = 100, y = 0}, {x = 50, y = 80, color = '#FF4040FF'}}, {1, 2, 3})
print(triangle.vertexCount, triangle.indexCount)
```

### graphics2d.newGridMesh(texture, columns, rows, options)

Returns a new `Mesh` of `columns` by `rows` cells, the start of a free-form deformation such as a waving flag or a squashed jelly. The table `options` is optional:

| Option | Default | Meaning |
| --- | --- | --- |
| `x`, `y`, `width`, `height` | the size of the texture at the origin | The area the grid covers. |
| `source` | the whole texture | The `Rect` of the texture that the grid shows. |

Vertices count row by row from the top left, `columns + 1` to a row. Fewer than one column or row, or more than 1048576 cells, raise `A grid mesh needs at least one column and one row and at most 1048576 cells.`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local flag = graphics2d.newGridMesh(assets.texture('ui/flag.png'), 16, 4, {x = 0, y = 0, width = 256, height = 128})
print(flag.vertexCount) -- 85
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `vertexCount` | integer | read | The number of vertices. |
| `indexCount` | integer | read | The number of indices, three to a triangle. |
| `bounds` | Rect | read | The rectangle around every vertex. |
| `texture` | Texture or nil | read and write | The image the mesh draws, `nil` for solid colors. |

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local flag = graphics2d.newGridMesh(assets.texture('ui/flag.png'), 8, 2)
flag.texture = assets.texture('ui/flag-torn.png')
print(flag.vertexCount, flag.indexCount, flag.bounds.width)
```

### mesh:setPositions(positions, first)

Moves the vertices from the vertex `first`, 1 by default, to the x and y pairs of `positions`, a [`FloatBuffer`](collections.md#float-buffers) or a list of numbers. Pairs past the last vertex raise `The positions of a mesh come in x and y pairs for the vertices it has, <count>, from the first one given.`, and a `first` below 1 raises a `bad argument` error that ends with `vertices count from 1`.

```lua
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local flag = graphics2d.newGridMesh(assets.texture('ui/flag.png'), 16, 4, {x = 0, y = 0, width = 256, height = 128})
local rest = collections.newFloatBuffer(flag.vertexCount * 2)
flag:readPositions(rest)
local moved = collections.newFloatBuffer(#rest)
local time = 0

scene.push({
    update = function(self, dt)
        time = time + dt
        for index = 1, #rest, 2 do
            local x = rest[index]
            moved[index] = x
            moved[index + 1] = rest[index + 1] + math.sin(time * 4 + x * 0.05) * x * 0.06
        end
        flag:setPositions(moved)
    end,
    render = function(self)
        graphics2d.beginScreen()
        flag:draw(200, 150, {layer = 1})
    end,
})
```

### mesh:readPositions(buffer, first)

Copies the positions of as many vertices as `buffer` holds pairs into it, from the vertex `first`, 1 by default, with the same errors as `mesh:setPositions`.

```lua
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')

local triangle = graphics2d.newMesh(nil, {{x = 0, y = 0}, {x = 100, y = 0}, {x = 50, y = 80}}, {1, 2, 3})
local positions = collections.newFloatBuffer(6)
triangle:readPositions(positions)
print(positions[5], positions[6]) -- 50.0 80.0
```

### mesh:setVertex(index, vertex)

Changes the fields that the table `vertex` gives of one vertex, from `x`, `y`, `u`, `v` and `color`. A vertex the mesh lacks raises a `bad argument` error that ends with `the mesh has no such vertex`.

```lua
local graphics2d = require('haylen.graphics2d')

local triangle = graphics2d.newMesh(nil, {{x = 0, y = 0}, {x = 100, y = 0}, {x = 50, y = 80}}, {1, 2, 3})
triangle:setVertex(3, {y = 120, color = '#FFFF8040'})
```

### mesh:vertex(index)

Returns a table with every field of one vertex: `x`, `y`, `u`, `v` and `color`. A vertex the mesh lacks raises a `bad argument` error that ends with `the mesh has no such vertex`.

```lua
local graphics2d = require('haylen.graphics2d')

local triangle = graphics2d.newMesh(nil, {{x = 0, y = 0}, {x = 100, y = 0}, {x = 50, y = 80}}, {1, 2, 3})
local top = triangle:vertex(3)
print(top.x, top.y, top.color:toHex())
```

### mesh:setIndices(indices)

Replaces the triangles with a list of vertex numbers counted from 1, three to a triangle, with the errors of `graphics2d.newMesh`.

```lua
local graphics2d = require('haylen.graphics2d')

local quad = graphics2d.newMesh(nil, {{x = 0, y = 0}, {x = 100, y = 0}, {x = 100, y = 100}, {x = 0, y = 100}}, {1, 2, 3})
quad:setIndices({1, 2, 3, 1, 3, 4})
```

### mesh:draw(x, y, options)

Draws the mesh into the open canvas with its vertices moved to `x`, `y`, which are 0 by default, without changing the mesh. The table `options` is optional and takes `rotation` in radians, `scaleX` and `scaleY`, which are 0, 1 and 1 by default, and the keys of the [draw order](#draw-order).

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local triangle = graphics2d.newMesh(nil, {{x = -50, y = 40}, {x = 50, y = 40}, {x = 0, y = -60}}, {1, 2, 3})

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        triangle:draw(400, 300, {rotation = 0.4, scaleX = 2, scaleY = 2, layer = 1})
    end,
})
```

## Text

Plain text draws with a font or a font family, shaped in every script and ordered in its direction, as the [text guide](../graphics/text.md#scripts-and-directions) explains.

### graphics2d.drawText(font, text, x, y, style)

Draws the UTF-8 `text` with `font`, a `Font` or a `FontFamily`, or with the default font of the engine when `font` is `nil`. The anchor point of the text block sits at `x`, `y`, and the rotation turns the block around that point. The table `style` is optional and takes the [text style](#text-style) and the keys of the [draw order](#draw-order).

- A family draws every character its regular face lacks with the first fallback that has it, which is how one string mixes Latin, Arabic, Devanagari or CJK text.
- A TrueType font is shaped by HarfBuzz and drawn from a signed distance field, so it stays sharp at any size, with the ligatures, joining forms, conjuncts and marks of every script. A bitmap font draws its own images, pixel for pixel at its native size.
- Every line is ordered for display by the Unicode bidirectional algorithm, so right-to-left text reads from the right and keeps numbers and Latin words in their own order.
- The character `\n` starts a new line, and so do `\r`, `\r\n`, which counts as one, and the other paragraph separators of Unicode, such as U+2029. Tabs and other control characters draw nothing and take no room.
- Lines wrap where the Unicode line breaking rules allow, which includes between Chinese and Japanese characters and between Thai phrases.

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
        -- A name that runs long shrinks to fit its plate, and a long story shrinks until it fits its box.
        graphics2d.drawText(nil, 'Captain Marigold of the Northern Sea', 960, 560, {size = 48, maxWidth = 360, wrap = false, minScale = 0.5, anchor = {0.5, 0.5}})
        graphics2d.drawText(nil, 'The tide rose over the reef while the crew counted the coins of the old wreck.', 960, 640, {size = 40, maxWidth = 420, maxHeight = 120, minScale = 0.4, anchor = {0.5, 0}})
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
    fallbacks = {assets.font('fonts/noto-sans-arabic-regular.ttf'), assets.font('fonts/noto-sans-devanagari-regular.ttf')},
})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(family, 'مرحبا بالعالم، رقم 42 (Harbor)', 1200, 200, {size = 40, maxWidth = 600, language = 'ar'})
        graphics2d.drawText(family, 'Order 7: السعر 42', 100, 300, {size = 40, direction = 'leftToRight'})
        graphics2d.drawText(family, 'नमस्ते दुनिया', 100, 400, {size = 40, language = 'hi', bold = true, italic = true, underline = true, strike = false, rotation = 0.05, scale = {1.1, 1}, lineSpacing = 1.4, outlineWidth = 0, shadowBlur = 0, pixelSnap = false})
    end,
})
```

### Text style

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `size` | number | `32` | The font size in canvas units: the height of the em square of a TrueType font. |
| `color` | Color | `'#FFFFFFFF'` | The text color. |
| `outlineWidth` | number | `0` | The outline width in canvas units, where 0 draws no outline. Every outline of the text draws under every fill, so an outline never covers the letters beside it. Outlines, blurs and glows reach at most the spread of the distance field of the font, less the smoothing of small text, and bitmap fonts have none. |
| `outlineColor` | Color | `'#FF000000'` | The outline color. |
| `shadowOffset` | Vec2 | `{0, 0}` | The offset of the drop shadow. |
| `shadowColor` | Color | `'#00000000'` | The shadow color. The shadow draws only when its alpha is above 0. The shadow of a bitmap font is the silhouette of its glyphs. |
| `shadowBlur` | number | `0` | Softens the shadow over this many canvas units. |
| `align` | string | `'start'` | The alignment of each line inside the block. The values `'start'` and `'end'` are the sides where the lines of the paragraph begin and end: left and right for left-to-right text and the other way around for right-to-left text. The values `'left'`, `'center'` and `'right'` are fixed sides, and `'fill'` stretches the spaces of every wrapped line to reach both edges and leaves the last line of a paragraph at its start. |
| `maxWidth` | number | `0` | Wraps whole words at this width, where 0 never wraps. |
| `wrap` | boolean | `true` | Whether lines wrap at `maxWidth`. Without wrapping every line stays whole, and `maxWidth` only bounds the box that `minScale` fits the text into. |
| `maxHeight` | number | `0` | The height of the box that `minScale` fits the text into, where 0 bounds no height. |
| `minScale` | number above 0, at most 1 | `1` | Draws text that does not fit its box smaller, down to this share of `size`, at the largest size where it fits: wrapped lines until they fit `maxHeight`, and text that does not wrap until its widest line fits `maxWidth` and its lines fit `maxHeight`. Text that fits at `size` keeps it, and text that never fits draws at the smallest size. The fitted layout is cached like any other, so text that stays the same fits once. |
| `lineSpacing` | number | `1.2` | The distance between lines as a multiple of the line height. |
| `direction` | string | `'auto'` | The direction of every paragraph: `'auto'` takes the direction of its first strong letter, and `'leftToRight'` and `'rightToLeft'` force it. It decides the order of mixed runs and the side of `'start'`. |
| `language` | string | `''` | The BCP 47 language tag of the text, such as `'ar'`, `'fa'`, `'ur'` or `'sr'`, which the shaper uses to pick the forms a language prefers. |
| `bold`, `italic` | boolean | `false` | With a `FontFamily`, pick its bold and italic faces or synthesize them. A `Font` synthesizes them. |
| `underline`, `strike` | boolean | `false` | Draw a line under the text or through it, in its color, across every line it wraps into. |
| `anchor` | Vec2 | `{0, 0}` | The point of the block placed at `x`, `y`, as a share of its size. The value `{0.5, 0.5}` centers the text. |
| `rotation` | number | `0` | The rotation in radians. |
| `scale` | Vec2 | `{1, 1}` | Stretches the drawn block from its anchor point on each axis, without laying the text out again. The function `graphics2d.measureText` returns the stretched size for the same style. |
| `pixelSnap` | boolean | `false` | Moves every line so its left edge lands on a whole pixel of the destination, centered and right-aligned lines included, and puts its baseline on a whole pixel row. This keeps small text crisp and stops it from shimmering as it moves. It applies when neither the text nor the view of its canvas turns, and it never changes the layout. |
| `ligatures` | boolean | `true` | Joins letters such as f and i into one glyph where the font has a ligature for them, which suits prose. The value `false` keeps a glyph for every letter, which code, file names and the names of functions need. |

A `size`, `maxWidth`, `maxHeight`, `minScale` or `lineSpacing` that is not a finite number raises `Text needs a finite size, maximum width, maximum height, minimum scale and line spacing.`. A `minScale` out of range raises `The "minScale" of a text style is a share of its size above 0 and at most 1.`.

### graphics2d.measureText(font, text, style)

Returns the width and height of the text block that `graphics2d.drawText` would draw with the same arguments. The argument `font` can be a `Font`, a `FontFamily` or `nil` for the default font. Only the layout keys of `style` and its `scale` change the result, and its draw order keys change nothing.

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

### graphics2d.defaultFont()

Returns the `Font` that the engine uses when a text function receives `nil` as its font.

```lua
local graphics2d = require('haylen.graphics2d')

local font = graphics2d.defaultFont()
print(font:lineHeight(24))
```

## RichText

A `RichText` is BBCode markup laid out with a font family. It caches its layout for its width and scale, runs its effects and its typewriter reveal as `text:update(dt)` advances, and draws the moment it has reached. The [text guide](../graphics/text.md#markup) lists every tag and explains the effects and the reveal.

### graphics2d.newRichText(markup, options)

Returns a new `RichText` from `markup`, laid out once and drawn every frame with `text:draw(x, y, options)`. The table `options` is optional and takes the [rich text options](#rich-text-options).

Markup errors raise `Rich text markup at line <line>, column <column>: <problem>`, such as `The tag "[b]" is never closed.` or `The markup "[wiggle]" is neither a tag nor a registered text effect.`, as the [text guide](../graphics/text.md#markup) lists. Unknown option keys raise `Unknown option "<key>".`. A `maxWidth` that is not a finite number raises `Rich text needs a finite maximum width.`, a size, scale or line spacing that is not a positive number raises `Rich text needs a positive size, scale and line spacing.`, and a negative reveal speed raises `Rich text cannot reveal at a negative speed.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local family = graphics.newFontFamily({regular = graphics2d.defaultFont()})
local story = graphics2d.newRichText('[b]Welcome[/b], traveler!\nPress [color=gold]Start[/color] to [wave]begin[/wave].', {family = family, size = 36, maxWidth = 600, revealSpeed = 25})
local chapter = graphics2d.newRichText('Chapter [i]One[/i] in [font=title]gold[/font]', {family = family, size = 48, bold = true, fonts = {title = assets.font('fonts/title.ttf')}})

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

### Rich text options

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `family` | FontFamily or Font | the default font | The family of text without a `[font]` tag. A `Font` becomes the regular face of a family of its own. |
| `size` | number | `32` | The size of text without a `[size]` tag, in canvas units: the height of the em square of the font. |
| `bold` | boolean | `false` | Styles all the text bold, as if inside `[b]`. |
| `italic` | boolean | `false` | Styles all the text italic, as if inside `[i]`. |
| `color` | Color | `'#FFFFFFFF'` | The color of text without a `[color]` tag. |
| `maxWidth` | number | `0` | The width the paragraphs wrap at. 0 never wraps, and the block is as wide as its widest line. |
| `align` | string | `'start'` | The alignment of paragraphs without their own: `'start'`, `'end'`, `'left'`, `'center'`, `'right'` or `'fill'`, where `'start'` and `'end'` follow the direction of each paragraph. |
| `lineSpacing` | number | `1.2` | The distance between lines as a multiple of their height. |
| `direction` | string | `'auto'` | The direction of paragraphs without a `[p dir]` of their own: `'auto'` takes the direction of the first strong letter of each paragraph, and `'leftToRight'` and `'rightToLeft'` force it. |
| `language` | string | `''` | The BCP 47 language tag that the text is shaped for, such as `'ar'` or `'hi'`. |
| `scale` | number | `1` | Multiplies every size of the markup and the options. |
| `revealSpeed` | number | `0` | The characters per second that the typewriter reveal shows as `text:update` advances. 0 shows everything at once. |
| `underlineLinks` | boolean | `true` | Whether `[url]` text is underlined. |
| `ligatures` | boolean | `true` | Joins letters into the ligatures of the font, like the text style option. The text of `[code]` never takes ligatures. |
| `fonts` | table | none | The families or fonts that `[font=name]` tags name, as `{name = family}`. A key that is not a string raises `The "fonts" option maps font names to families, so its keys must be strings.`. |

### graphics2d.drawRichText(markup, x, y, options)

Draws `markup` with the top-left corner of its block at `x`, `y`. The table `options` takes the [rich text options](#rich-text-options), `tint`, a `Color` that multiplies every color of the text and is white by default, and the keys of the [draw order](#draw-order). Its effects follow the time the app has run.

The text plugin keeps the layout of every markup and options that a frame draws or measures, compared by their values. So text drawn the same way every frame lays out once and its frames allocate nothing. At the end of each frame it forgets the layouts that the frame did not use. The images of `[img]` tags load once and stay loaded while frames keep drawing them, and they go back to the assets after a frame that no longer draws them. Text whose markup changes often, such as a number that counts every frame, lays out at each change, and a `RichText` changed with `text.markup` costs the same.

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

Returns the width and height of the block that `graphics2d.drawRichText` would draw with the same markup and options. A block that wraps at a `maxWidth` is as wide as `maxWidth`, so its paragraphs align inside it, and the boxes of its lines in [`text:frame()`](#textframe) end where their text ends.

```lua
local graphics2d = require('haylen.graphics2d')

local width, height = graphics2d.measureRichText('[b]Quest complete[/b]\nYou found [color=gold]12 coins[/color].', {size = 28, maxWidth = 400})
print(width, height)
```

### graphics2d.registerTextEffect(name, effect)

Registers a text effect that runs as `[name]...[/name]` in rich text, in `haylen.graphics2d` and in `ui.richText`. Every frame, `effect(glyph, attributes)` runs once for each glyph inside the tag. The table `glyph` holds these fields:

| Field | Meaning |
| --- | --- |
| `index` | The place of the glyph, counted from 1 over every character inside the tag, spaces and images included. |
| `character` | The place of its character, counted from 1 in the whole text. |
| `char`, `codePoint` | The character as a string and as a code point. |
| `x`, `y` | The pen position of the glyph on the baseline. |
| `time` | The seconds the text has run. |
| `offsetX`, `offsetY`, `color`, `visible` | The fields that the effect changes. |

The table `attributes` holds the attributes of the tag, with numbers as numbers, and `[name=value]` as `attributes.value`.

Registering a name again replaces the effect. An error inside the effect stops the app like any script error. An effect runs while its text builds the picture of the moment, so changing or measuring that text from inside the effect raises `A text effect cannot change or lay out the rich text it runs on.`. These names raise an error:

| Error | Cause |
| --- | --- |
| `A text effect name must be a single word, not "<name>".` | The name has spaces or is empty. |
| `The name "<name>" belongs to a rich text tag, so no effect can take it.` | The name is a tag, such as `b` or `color`. |
| `The name "<name>" belongs to a built-in text effect, so no other effect can take it.` | The name is `wave`, `shake`, `tornado`, `fade`, `rainbow` or `pulse`. |
| `The text effect "<name>" needs a function.` | The effect is not a function. |

```lua
local graphics2d = require('haylen.graphics2d')

graphics2d.registerTextEffect('bounce', function(glyph, attributes)
    local height = attributes.height or 8
    glyph.offsetY = glyph.offsetY - math.abs(math.sin(glyph.time * 6 + glyph.index * 0.5)) * height
end)

local title = graphics2d.newRichText('[bounce height=12]Jump![/bounce]', {size = 64})
```

### graphics2d.registerTextIcon(name, texture, options)

Registers an image that `[icon=name]` shows inline, such as an input prompt. The table `options` takes `source`, a `Rect` of the texture that is the whole texture by default, and `width` and `height`. An icon without a size is as tall as its text and keeps the shape of its region, and the tag may set its own `width`, `height`, `color` and `valign`. An empty name raises `A text icon needs a name.`, and a texture that is not valid raises `The text icon "<name>" needs a texture.`.

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

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `markup` | string | read and write | The markup. Setting it lays the text out again and starts its effects and reveal over. |
| `maxWidth` | number | read and write | The width the paragraphs wrap at, 0 for none. |
| `scale` | number | read and write | Multiplies every size of the text. A scale that is not positive raises `Rich text needs a positive scale.`. |
| `family` | FontFamily | read and write | The family of text without a `[font]` tag. Setting a `Font` makes it the regular face of a family of its own. |
| `bold`, `italic` | boolean | read and write | Whether all the text is bold or italic, as if inside `[b]` or `[i]`. |
| `color` | Color | read and write | The color of text without a `[color]` tag. |
| `align` | string | read and write | The alignment of paragraphs without their own, like the `align` option. |
| `direction` | string | read and write | The direction of paragraphs without a `[p dir]` of their own, like the `direction` option. |
| `language` | string | read and write | The BCP 47 language tag the text is shaped for. |
| `lineSpacing` | number | read and write | The distance between lines as a multiple of their height. |
| `revealSpeed` | number | read and write | The characters per second that the typewriter reveal shows, 0 for everything at once. |
| `underlineLinks` | boolean | read and write | Whether `[url]` text is underlined. |
| `ligatures` | boolean | read and write | Whether letters outside `[code]` join into the ligatures of the font. |
| `visibleCharacters` | integer | read and write | How many characters show. Setting it moves the reveal there, and a negative count shows everything. |
| `visibleRatio` | number | read and write | The share of characters that show, from 0 to 1. Values outside that range count as its ends, and NaN raises `The visible ratio of rich text must be a number.`. |
| `characterCount` | integer | read | The characters of the text: one per cluster, which is a letter with its marks, a conjunct or a ligature, and one per image or icon. |
| `revealing` | boolean | read | Whether the reveal still has characters to show. |
| `time` | number | read | The seconds the text has run, which drive its effects. |

The option properties take the values of the [rich text options](#rich-text-options). Setting one lays the text out again and starts its reveal over, while its effects keep their time. The base `size` and the `fonts` of `[font]` tags stay the ones the text was made with, and `scale` resizes the whole text.

```lua
local graphics2d = require('haylen.graphics2d')

local line = graphics2d.newRichText('Hello [b]there[/b]', {revealSpeed = 20})
line:update(0.25)
print(line.visibleCharacters, line.characterCount, line.revealing, line.time)
line.visibleRatio = 1
line.markup = 'Goodbye'
line.maxWidth = 400
line.scale = 1.5
line.family = graphics2d.defaultFont()
line.bold = true
line.italic = true
line.color = '#FFFFD070'
line.align = 'center'
line.direction = 'leftToRight'
line.language = 'en'
line.lineSpacing = 1.3
line.revealSpeed = 0
line.underlineLinks = false
```

### text:update(dt)

Moves the effects and the reveal `dt` seconds forward. A `dt` that is negative or not finite raises `Rich text moves forward by a number of seconds that is finite and not negative.`. A `[pause=seconds]` tag holds the reveal before the next character, and `[speed=factor]` reveals the text inside it faster or slower. An image that was still loading takes its room once it arrives, at the next update.

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

Draws the text with the top-left corner of its block at `x`, `y`. The table `options` is optional and takes `scale`, a `Vec2` that stretches the block from that corner without laying it out again, which suits pulses and pops. It also takes `tint`, a `Color` that multiplies every color of the text, and the keys of the [draw order](#draw-order). A canvas sorted by y sorts the text by the bottom of its block.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local sign = graphics2d.newRichText('[center][b]Village[/b]\n[size=70%]Population 42[/size][/center]', {size = 28, maxWidth = 240})
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
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

### text:linkAt(x, y)

Returns the payload of the `[url]` link under a point of the text block, relative to its top-left corner, or `nil`. A link without a payload, such as `[url]https://haylen.dev[/url]`, uses its text.

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

### text:hintAt(x, y)

Returns the text of the `[hint]` under a point of the text block, relative to its top-left corner, or `nil`.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local help = graphics2d.newRichText('Ask the [hint=The old keeper of the lighthouse]keeper[/hint].', {size = 28})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        help:draw(100, 100)
        local x, y = input.mousePosition()
        local hint = help:hintAt(x - 100, y - 100)
        if hint then
            graphics2d.drawText(nil, hint, x + 16, y + 16, {size = 20})
        end
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

Returns the text as it draws at this moment, with its effects applied and the characters that the reveal has not reached hidden. The table has `size` (`Vec2`), `lineCount` and these lists:

| Field | Entries |
| --- | --- |
| `glyphs` | `char` (the first code point of its character), `index` (the glyph index in its font), `character` (counted from 1), `rect` (the quad), `baseline`, `color`, `visible`, `size` (the text size), `syntheticBold` and `syntheticItalic`, in the order the lines show them. |
| `boxes` | `kind` (`'background'`, `'underline'`, `'strike'`, `'rule'`, `'cellBackground'` or `'cellBorder'`), `rect`, `color` and `visible`. |
| `images` | `rect`, `texture` and `visible` of every image and icon. |
| `links` | `rect` and `link` of every piece of a link, several when it wraps. |
| `hints` | `rect` and `hint` of every piece of a hint. |
| `characters` | `rect`, `first` and `last`, the code points of the text without markup that it draws, counted from 1, where paragraphs end with a line break, and `rightToLeft` of every character in reading order. |
| `lines` | `rect`, `baseline` and `rightToLeft` of every line. |

```lua
local graphics2d = require('haylen.graphics2d')

local laid = graphics2d.newRichText('[b]Bold[/b] and [url=more]a link[/url]'):frame()
for _, glyph in ipairs(laid.glyphs) do
    print(glyph.char, glyph.rect.x, glyph.syntheticBold)
end
print(#laid.links, laid.links[1].link)
```

## NineSlice

A `NineSlice` is a frame made of nine regions of a texture, which draws at any size while its corners keep their shape. Make one with `graphics2d.newNineSlice` or with `atlas:slice(name)` of [`haylen.animation2d`](animation2d.md), and draw it with `graphics2d.drawNineSlice`. The [rendering guide](../graphics/rendering.md#nine-slices) gives every rule of how the pieces lay out. Vector and rectangle properties return copies, so assign a new value instead of changing a returned one.

### graphics2d.newNineSlice(texture, options)

Returns a new `NineSlice` of `texture`. The table `options` is required. Give either `borders`, which cut `source` into nine regions, or `pieces`, which lists the nine regions directly:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `source` | Rect | the whole texture | The region of the texture that holds the frame, used with `borders`. |
| `borders` | table | required without `pieces` | The border sizes in pixels, as `{left, top, right, bottom}`. |
| `pieces` | table | none | Nine `Rect` values, row by row from the top-left corner to the bottom-right corner. |
| `fill` | string | `'stretch'` | The value `'stretch'` stretches the edges and the center. The value `'tile'` repeats them at their pixel size times the `borderScale` of the draw, at most 4096 times along a row or a column, so a frame squeezed far below its borders stretches its copies instead. |

These errors come from a wrong nine-slice:

| Error | Cause |
| --- | --- |
| `a nine-slice needs exactly nine pieces` | The `pieces` do not hold nine rectangles. It is the reason of a `bad argument` error. |
| `borders need left, top, right and bottom` | The `borders` do not hold four numbers. It is the reason of a `bad argument` error. |
| `The borders of a nine-slice cannot be negative.` | A border is negative. |
| `The borders of a nine-slice must fit inside its source rectangle, with the left and right borders at most its width and the top and bottom borders at most its height.` | The borders are larger than the source. |
| `unknown value '<name>'` | The `fill` is not `'stretch'` or `'tile'`. |

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

### graphics2d.drawNineSlice(slice, rect, color, options)

Draws the nine-slice `slice` so it fills `rect`. The corners keep their size, the edges stretch or repeat along their length and the center fills the rest. Every piece samples only its own texels, so linear filtering never bleeds the gaps of an atlas into a frame. The argument `color` is white by default. The table `options` is optional and takes the keys of the [draw order](#draw-order) and `borderScale`, which multiplies the border sizes and is 1 by default. When `rect` is narrower or shorter than the borders of an axis, they shrink in proportion until they meet, and a `rect` without a positive width and height draws nothing. A `borderScale` that is not positive raises `A nine-slice border scale must be positive.`, and a nine-slice without a texture raises `Cannot draw a nine-slice without a texture.`. The method [`slice:layout`](#slicelayoutrect-borderscale) returns the quads a draw makes.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local banner = graphics2d.newNineSlice(assets.texture('ui/banner.png'), {borders = {100, 68, 84, 111}})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawNineSlice(banner, {660, 200, 600, 400}, '#FFFFFFFF', {layer = 1, borderScale = 0.75})
    end,
})
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `texture` | Texture | read and write | The texture the regions come from. |
| `pieces` | table | read and write | The nine source `Rect` values, row by row from the top-left corner. Writing needs exactly nine, or it raises a `bad argument` error that ends with `a nine-slice needs exactly nine pieces`. |
| `fill` | string | read and write | `'stretch'` or `'tile'`. |
| `borders` | table | read | The border sizes as `{left, top, right, bottom}`, in the form `graphics2d.newNineSlice` takes: the width of the widest piece of each side column and the height of the tallest piece of each side row. |
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

Returns the quads that `graphics2d.drawNineSlice` draws for `rect` and `borderScale`, which is 1 by default, as a list of `{area = Rect, source = Rect}`. Here `area` is the part of `rect` that a quad covers, and `source` the region of the texture it shows, in pixels. An app that draws frames its own way, such as a GUI library of its own or a frame of meshes, takes the same layout. A `rect` without a positive width and height returns an empty list, and a scale that is not positive raises `A nine-slice border scale must be positive.`.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local panel = graphics2d.newNineSlice(assets.texture('ui/panel.png', {filter = 'linear'}), {borders = {16, 16, 16, 16}, fill = 'tile'})
for _, patch in ipairs(panel:layout({0, 0, 300, 120}, 0.5)) do
    print(patch.area.x, patch.area.width, patch.source.x, patch.source.width)
end
```

## Vector images

A [`VectorImage`](graphics.md#vectorimage) of `haylen.graphics` draws sharp at any size and zoom. The [rendering guide](../graphics/rendering.md#vector-images) explains how the engine rasterizes it at the scale it covers and keeps the rasters in a vector atlas.

### graphics2d.drawVector(image, x, y, options)

Draws a vector image with its pivot at `x`, `y`, at its own size unless the options give one. The table `options` takes the keys of [`graphics2d.draw`](#graphics2ddrawtexture-x-y-options) but `source`, `partColors` and `effect`. A `partMask` raises `A vector image draws without a part mask.`, and an image without a document raises `Cannot draw a vector image without a document.`.

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

## Groups and masks

A group draws everything between its two calls into an image, which then draws as one. So the parts of a character, a panel or an effect that overlap fade together instead of showing each other through. A mask draws its content only inside, or outside, a shape drawn with any draws. It goes through the coverage of the shape, so soft and smooth edges stay soft. The [rendering guide](../graphics/rendering.md#groups-and-masks) explains what they cost. Groups and masks nest, and a canvas that ends, or a frame, closes the groups and masks it left open.

### graphics2d.beginGroup(options)

Starts a group in the open canvas. The group sorts its draws among themselves like a canvas of its own. It draws its image at the place its options give among the draws of the canvas. The image is unlit and has no post-processing of its own, even in a canvas that has them. The clips of the canvas apply to the image and not to the draws inside, which start without clips. The table `options` is optional and also takes the keys of the [draw order](#draw-order) but `material`:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `opacity` | number | `1` | The opacity of the whole image, from 0 to 1. |

The image composites with the `blend` of the options, plain transparency by default. A blend from `'overlay'` on mixes the image with what the canvas drew below the group, as a layer of a drawing program does. A canvas with ambient light cannot do that and raises `A group that blends with what lies below draws in a canvas without ambient light.`. A group with `'opaque'` replaces everything below it with its image, transparent parts included. An opacity outside 0 to 1 raises `A group needs an opacity from 0 to 1.`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local fade = 1

scene.push({
    update = function(self, dt)
        fade = math.max(0, fade - dt * 0.2)
    end,
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.beginGroup({opacity = fade, layer = 1})
        graphics2d.drawCircle(300, 120, 50, '#FFFF8040')
        graphics2d.drawCircle(340, 120, 50, '#FFFF8040')
        graphics2d.endGroup()
    end,
})
```

### graphics2d.endGroup()

Ends the group that the last `graphics2d.beginGroup` began and draws its image. A call without an open group raises `The "endGroup" call has no matching "beginGroup".`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({100, 100, 400, 300}, '#FF3A7D44')
        graphics2d.beginGroup({blend = 'overlay'})
        graphics2d.drawCircle(300, 250, 120, '#FFFFC080')
        graphics2d.endGroup()
    end,
})
```

### graphics2d.pushMask(drawShape, options)

Calls the function `drawShape` at once to draw the shape of a mask with any draws, such as circles, sprites, text or meshes. Then everything until `graphics2d.popMask()` draws only inside that shape, or outside it with `mode = 'outside'`, as for round avatars, reveals, fog of war and shaped panels. The masked draws composite as one image like a group. The table `options` is optional and takes `mode`, `'inside'` by default or `'outside'`, and the keys of the [draw order](#draw-order) but `material`. In C++, `Renderer::pushMask(mode, order)` begins the shape, `Renderer::endMaskShape()` ends it and `Renderer::popMask()` ends the content.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local portrait = assets.texture('ui/portrait.png')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.pushMask(function()
            graphics2d.drawCircle(120, 120, 64, '#FFFFFFFF')
        end)
        graphics2d.draw(portrait, 120, 120)
        graphics2d.popMask()
    end,
})
```

### graphics2d.popMask()

Ends the content of the mask that the last `graphics2d.pushMask` began and draws it. A call without an open mask raises `The "popMask" call has no matching "pushMask" whose shape ended with "endMaskShape".`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({0, 0, 1920, 1080}, '#FF3A7D44')
        -- Fog of war covers everything outside the circle the player sees.
        graphics2d.pushMask(function()
            graphics2d.drawCircle(960, 540, 300, '#FFFFFFFF')
        end, {mode = 'outside', layer = 5})
        graphics2d.drawRect({0, 0, 1920, 1080}, '#E0000000')
        graphics2d.popMask()
    end,
})
```

## Image blends

An image blend mixes two textures through a pattern, such as the shader-based scene transitions do. Custom transitions and effects can use it too.

### graphics2d.drawImageBlend(from, to, rect, options)

Draws the textures `from` and `to` over `rect`, mixed through a pattern that `progress` advances from `from` at 0 to `to` at 1. The textures of render targets sample upright on every graphics API. Both textures are required, and one that is missing raises `An image blend needs a texture to blend from and one to blend to.`. The table `options` also takes the keys of the [draw order](#draw-order) but `material`:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `pattern` | string | `'dissolve'` | The pattern `'dissolve'` switches cells in random order with a soft edge, `'pixelate'` breaks `from` into growing blocks and brings `to` back out of them, and `'radial'` sweeps a hand clockwise from twelve o'clock. The pattern `'iris'` closes a circle on `from` to the color and opens it on `to`, and `'pageTurn'` curls `from` away like a page. The pattern `'mask'` shows `from` where `to` is opaque, or where it is transparent when `reversed`, going from `from` whole at 0 to masked at 1. |
| `progress` | number | `0` | From 0 to 1. |
| `center` | Vec2 | `{0.5, 0.5}` | The center of `'radial'` and `'iris'`, from `{0, 0}` at the top-left of the rectangle to `{1, 1}` at its bottom-right. |
| `cellSize` | number | `1` | The size of the dissolve cells in pixels of `from`. |
| `blockSize` | number | `48` | The size of the largest pixelate blocks in pixels of `from`. |
| `color` | Color | `'#FF000000'` | The color the iris passes through. |
| `reversed` | boolean | `false` | Sweeps the radial pattern counterclockwise, and shows `from` outside the mask of the `'mask'` pattern. |
| `angle` | number | `0` | The direction in radians that the page turn moves toward, where 0 points right and `math.pi / 2` points down. |

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local day = graphics.newRenderTarget(320, 180)
local night = graphics.newRenderTarget(320, 180)
local camera = graphics2d.newCamera()
camera.position = {160, 90}
local time = 0

scene.push({
    update = function(self, dt)
        time = time + dt
    end,
    render = function(self)
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

## Metaballs

Metaballs merge nearby soft circles into one smooth shape, such as a liquid made of particles. The [rendering guide](../graphics/rendering.md#metaballs) explains how the field draws.

### graphics2d.drawMetaballs(points, radius, options)

Adds up a soft circle of `radius` around every point into a field, and draws the surface where the field reaches the threshold. The argument `points` is a flat list of positions, `{x1, y1, x2, y2, ...}`, so thousands of points need no table each, and the `positions` of a [`physics2d`](physics2d.md) fluid fill it directly. A lone circle shows exactly its radius at the default threshold, and two circles join when they come closer than about two and a half radii. The field of each call is a render target at half the resolution of the canvas, drawn with one instanced pass. The table `options` is optional and also takes the keys of the [draw order](#draw-order) but `material`:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `color` | Color | `'#FFFFFFFF'` | The fill of the surface. |
| `outlineColor` | Color | `'#00000000'` | The color of the band of the field just above the threshold. |
| `outlineWidth` | number | `0` | The width of that band in field units, where a lone circle is 1 at its center. |
| `threshold` | number | `0.5` | The field value where the surface starts, between 0 and 1. Lower values make the shapes rounder and merge them sooner. |

A radius that is not positive raises `A metaball radius must be positive.`, and a threshold outside 0 to 1 raises `A metaball threshold must be between 0 and 1.`. A negative outline width, or one that takes the threshold past 1, raises `A metaball outline width must be zero or more and keep the threshold plus the width below 1.`. An odd number of values raises a `bad argument` error that ends with `metaball positions need two numbers per point`.

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
        graphics2d.drawMetaballs(drops, 14, {color = '#E02080FF', outlineColor = '#FFA0E0FF', outlineWidth = 0.15, threshold = 0.5, layer = 1})
    end,
})
```

## Lights

Lights and occluders draw only in lit canvases, world or render target canvases with `ambientLight`. The module [`haylen.lighting2d`](lighting2d.md) explains the kinds of light, blend modes, masks and shadows, and the [rendering guide](../graphics/rendering.md#lighting) explains the passes.

### graphics2d.drawLight(light)

Draws a light into the light map of the open lit canvas, over the lights drawn before it. The argument `light` is a [`Light`](lighting2d.md#light) from `lighting2d.newLight`, which a scene makes once, or a table with the same keys. The keys are such as `type`, `x`, `y`, `radius`, `color`, `intensity`, `rotation`, `innerAngle`, `outerAngle`, `height`, `blend`, `itemMask`, `layerMin`, `layerMax` and the shadow settings. Disabled lights draw nothing. Lights drawn one after another with the same blend mode and texture draw in one call, so draw the lights of one kind together.

Drawing a light in any other canvas raises `Lights can only be drawn in a lit canvas, a world or render target canvas with ambient light.`. A value out of its range raises an error such as `A light intensity must be zero or positive.`.

```lua
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
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

Draws an outline that blocks the lights with shadows of the open lit canvas. The argument `occluder` is an [`Occluder`](lighting2d.md#occluder) from `lighting2d.newOccluder`, or a table with its keys `points`, `closed`, `cull`, `mask`, `x`, `y`, `rotation`, `scaleX` and `scaleY`. Drawing an occluder outside a lit canvas raises `Occluders can only be drawn in a lit canvas, a world or render target canvas with ambient light.`, and too few points raise `An occluder needs at least 2 points, and 3 when it is closed.`.

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

### graphics2d.lightTexture()

Returns the radial falloff `Texture`, 128 by 128 pixels, that `graphics2d.drawLight` uses when a light has no texture of its own. It is white, with an alpha that fades from the center to the edge, which also suits glows and soft shadows.

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

Tells whether light maps hold light in floating point, which lets lights brighter than 1 brighten the scene beyond its unlit colors. It is `true` where the GPU device renders and blends floating-point targets, and otherwise light stops at 1.

```lua
local graphics2d = require('haylen.graphics2d')

local glare = graphics2d.hdrLighting() and 3 or 1
print('The sun shines with intensity ' .. glare)
```

## Material

A `Material` shades draws with a custom shader and holds values for the uniforms and textures of the shader, which it sets by the names the shader declares them with. The `material` key of a [draw order](#draw-order), of a sprite and of the `materials` of a post-process applies it. A draw copies the values when it is made. The [shader guide](../graphics/shaders.md) explains how to write and compile shaders. Materials compare equal with `==` when they are the same material.

### graphics2d.newMaterial(shader, uniforms)

Returns a new `Material` that shades draws with `shader`, a `Shader` that [`haylen.assets`](assets.md#assetsshaderpath) loads from a `.shader` file compiled by `haylen.py shaders`. The table `uniforms` is optional and holds first values by name, which `material:set` takes one by one. A key of it that is not a string raises a `bad argument` error that ends with `uniform names must be strings`. A shader without the programs of the shader library raises `The shader "<name>" has no "sprite" program. Compile it with "haylen.py shaders" from a source that includes "haylen/material.glsl".`, and no shader raises `A material needs a shader.`.

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

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `shader` | Shader | read | The shader of the material. |

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local shine = graphics2d.newMaterial(assets.shader('shaders/shine.shader'))
print(shine.shader.name, shine == shine)
```

### material:set(name, value)

Sets a uniform or a texture of the shader by name. The value fits the type of the uniform:

| Uniform | Value |
| --- | --- |
| `float`, `int` | A number. Integers round to the nearest one. |
| `vec2`, `ivec2` | A `Vec2`, or a table `{x, y}` or `{x = 0, y = 0}`. |
| `vec3` | A `Color`, whose red, green and blue fill it, or a list of 3 numbers. |
| `vec4` | A `Color`, a color string or a list of 4 numbers. |
| `mat4` | A `Transform2D`, which moves x and y like the transform, or a list of 16 numbers in column order. |
| arrays | A list of every number of every element, such as 12 numbers for `vec4 palette[3]`. |
| texture | A `Texture`, or `nil` for a white texture. |

An unknown name raises `The shader "<name>" has no uniform named "<key>".`. A value that does not fit raises an error that names the type and the count of numbers the uniform takes, such as `The uniform "tint" is a "vec4", which takes 4 numbers, not 1.` or `The uniform "palette" is an array of 3 "vec4", which takes 12 numbers, not 4.`.

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

Returns the value of a uniform or a texture. Uniforms that were never set read as zeros.

| Uniform | Value |
| --- | --- |
| `float`, `int` | A number. |
| `vec2`, `ivec2` | A `Vec2`. |
| `vec4` | A `Color`. |
| Any other uniform, and every array | A list of numbers. |
| texture | The `Texture`, or `nil` when none is set. |

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local shine = graphics2d.newMaterial(assets.shader('shaders/shine.shader'), {speed = 3})
print(shine:get('speed'), shine:get('glint'), shine.shader.name)
```

## Statistics and inspection

### graphics2d.stats()

Returns a table with the counters of the renderer for the current frame. The draw counters grow while the frame is recorded, and the GPU counters are filled when the frame is submitted. So read them in `update` to see the totals of the frame before.

| Key | Meaning |
| --- | --- |
| `canvases` | Canvases begun. |
| `passes` | GPU render passes. |
| `drawCalls` | GPU draw calls. |
| `sprites` | Quads submitted: sprites, batch sprites, glyphs and particles. |
| `instances` | Sprite instances uploaded to the GPU. |
| `vertices` | Mesh and polygon vertices uploaded. |
| `indices` | Mesh and polygon indices uploaded. |
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

Returns the textured quads and blocks of text that the open canvas holds so far, in the order they were drawn. Each entry is a table with these fields:

| Field | Meaning |
| --- | --- |
| `x`, `y`, `width`, `height` | The bounds in the coordinates of the canvas. |
| `text` | The value `true` for a block of text. |
| `label` | The path of the texture of a sprite, on the first quad of each draw. |

- Every sprite and every quad of a batch or a nine-slice is one entry, a baked batch is one entry, and every text draw is one block.
- Plain shapes, such as rectangles and lines, the sprites and batches drawn with `graphics.whiteTexture()` and the draws with a distortion are left out.
- The bounds of a block of text reach a little past its letters, by the spread of the distance field of its glyphs.

It suits checks and tools that need to know where things draw, such as the bounds that the debug drawings outline. Without an open canvas it raises `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.`.

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

## Errors

These messages come from the functions and methods of the module, besides the errors of options that [values](#values) lists.

| Message | Cause |
| --- | --- |
| `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.` | A draw or a canvas query ran without an open canvas. |
| `A camera viewport needs a positive width and height.` | The viewport of the camera of a canvas is empty. |
| `World canvases only support a clear color with lighting or post-processing.` | A world canvas without `ambientLight` or `postProcess` got a `clear` color. |
| `Screen canvases do not support lighting or post-processing.` | A screen canvas got `ambientLight` or `postProcess`. |
| `Screen canvases do not support a clear color.` | A screen canvas got a `clear` color. |
| `A render target canvas needs a valid render target.` | The target of `graphics2d.beginTarget` is not valid. |
| `A capture needs a valid render target.` | The target of `graphics2d.beginCapture` is not valid. |
| `A draw cannot sample the render target that its canvas draws into.` | A draw used the texture of its own target. |
| `The "popClip" call has no matching "pushClip".` | The function `graphics2d.popClip` had no clip to remove. |
| `The "popLayerOffset" call has no matching "pushLayerOffset".` | The function `graphics2d.popLayerOffset` had no offset to remove. |
| `The "endCapture" call has no matching "beginCapture".` | The function `graphics2d.endCapture` had no open capture. |
| `The "endGroup" call has no matching "beginGroup".` | The function `graphics2d.endGroup` had no open group. |
| `The "popMask" call has no matching "pushMask" whose shape ended with "endMaskShape".` | The function `graphics2d.popMask` had no open mask. |
| `A group needs an opacity from 0 to 1.` | The `opacity` of a group is out of range. |
| `A group that blends with what lies below draws in a canvas without ambient light.` | A group with a blend from `'overlay'` on is in a lit canvas. |
| `The blend mode "<name>" mixes with what lies below, which only groups do. Draw inside "beginGroup" with that blend instead.` | A draw that is not a group has a blend from `'overlay'` on. |
| `A draw needs a specular strength and an emission of zero or more.` | The `specular` or `emission` of a draw is negative. |
| `A draw shininess must be from 1 to 255.` | The `shininess` of a draw is out of range. |
| `Image blends and metaballs do not take a material.` | An image blend or metaballs got a `material`. |
| `A draw with a part mask does not take a material.` | A recolored draw got a `material`. |
| `A sprite takes a part mask or an effect, not both.` | A sprite has both. |
| `A sprite with an effect does not take a material.` | A sprite with an effect got a `material`. |
| `A sprite effect needs a dissolve and a dissolve edge from 0 to 1, a dissolve size from 1 to 64 and an outline width and a glow size from 0 to 64.` | A value of a sprite effect is out of range. |
| `Only sprites, sprite batches, nine-slices, shapes and meshes draw as distortion.` | Another draw got a `distortion`. |
| `A baked sprite batch draws without a distortion. Draw a sprite batch to distort with its sprites.` | The function `graphics2d.drawStatic` got a `distortion`. |
| `A baked sprite batch draws without a part mask. Draw a sprite batch to recolor its sprites.` | The function `graphics2d.drawStatic` got a `partMask`. |
| `A vector image draws without a part mask.` | The function `graphics2d.drawVector` got a `partMask`. |
| `Cannot draw a sprite without a texture.` | A sprite has no valid texture. |
| `Cannot draw a nine-slice without a texture.` | A nine-slice has no texture. |
| `Cannot draw a vector image without a document.` | A vector image has no document. |
| `An image blend needs a texture to blend from and one to blend to.` | A texture of `graphics2d.drawImageBlend` is missing. |
| `A static batch needs a texture and at least one sprite.` | The method `batch:bake` ran on an empty batch. |
| `A sprite batch needs a texture.` | The list of `graphics2d.newSpriteBatch` is empty. |
| `A sprite batch holds at most 16777216 sprites.` | A batch would hold more sprites. |
| `Sprite batch index is out of range.` | The `first` of `batch:writeFields` or `batch:readFields` is more than one past the last sprite. |
| `A sprite batch with several textures or sprite data does not bake. Draw it as a sprite batch instead.` | The method `batch:bake` ran on such a batch. |
| `A sprite layout needs at least one field.` | A list of sprite fields is empty. |
| `A "drawBatch" call with a float buffer needs the fields each sprite takes, such as "fields = {'x', 'y'}".` | The layout of `graphics2d.drawBatch` lacks `fields`. |
| `A shape needs corner radii, a border width and a softness of zero or more.` | A value of `graphics2d.drawShape` is negative. |
| `The option "radius" of a shape takes one radius for every corner or four, from the top-left corner clockwise.` | The `radius` table of a shape has another length. |
| `Points take either one "color" or "colors" for each point.` | The function `graphics2d.drawPoints` got both. |
| `Points take one color for each of their <count> points.` | The `colors` of `graphics2d.drawPoints` have another count. |
| `The points draw <count> of the <total> they were given.` | The `count` of `graphics2d.drawPoints` is past the points. |
| `Mesh index refers to a vertex that does not exist.` | An index of a mesh is past its vertices. |
| `A mesh lists its triangles as three indices each, and <count> is not a multiple of three.` | The indices of a kept mesh are not whole triangles. |
| `A grid mesh needs at least one column and one row and at most 1048576 cells.` | The size of `graphics2d.newGridMesh` is out of range. |
| `The positions of a mesh come in x and y pairs for the vertices it has, <count>, from the first one given.` | The positions of `mesh:setPositions` or `mesh:readPositions` reach past the vertices. |
| `Text needs a finite size, maximum width, maximum height, minimum scale and line spacing.` | A value of a text style is not finite. |
| `The "minScale" of a text style is a share of its size above 0 and at most 1.` | The `minScale` of a text style is out of range. |
| `Rich text needs a finite maximum width.` | The `maxWidth` of rich text is not finite. |
| `Rich text needs a positive size, scale and line spacing.` | A size, scale or line spacing of rich text is not positive. |
| `Rich text needs a positive scale.` | The property `text.scale` got a value that is not positive. |
| `Rich text cannot reveal at a negative speed.` | The `revealSpeed` of rich text is negative. |
| `Rich text moves forward by a number of seconds that is finite and not negative.` | The `dt` of `text:update` is wrong. |
| `The visible ratio of rich text must be a number.` | The property `text.visibleRatio` got NaN. |
| `A text effect cannot change or lay out the rich text it runs on.` | An effect changed or measured its own text. |
| `The "fonts" option maps font names to families, so its keys must be strings.` | A key of the `fonts` option is not a string. |
| `A nine-slice border scale must be positive.` | The `borderScale` of a nine-slice is not positive. |
| `The borders of a nine-slice cannot be negative.` | A border of `graphics2d.newNineSlice` is negative. |
| `The borders of a nine-slice must fit inside its source rectangle, with the left and right borders at most its width and the top and bottom borders at most its height.` | The borders are larger than the source. |
| `A metaball radius must be positive.` | The radius of `graphics2d.drawMetaballs` is not positive. |
| `A metaball threshold must be between 0 and 1.` | The `threshold` is out of range. |
| `A metaball outline width must be zero or more and keep the threshold plus the width below 1.` | The `outlineWidth` is out of range. |
| `Lights can only be drawn in a lit canvas, a world or render target canvas with ambient light.` | The function `graphics2d.drawLight` ran in another canvas. |
| `Occluders can only be drawn in a lit canvas, a world or render target canvas with ambient light.` | The function `graphics2d.drawOccluder` ran in another canvas. |
| `A material needs a shader.` | The function `graphics2d.newMaterial` got no shader. |
| `A post-processing material needs a shader.` | An entry of the `materials` of a post-process has no shader. |
| `The shader "<name>" has no "sprite" program. Compile it with "haylen.py shaders" from a source that includes "haylen/material.glsl".` | The shader of a material lacks the programs of the shader library. |
| `The shader "<name>" has no uniform named "<key>".` | The methods `material:set` or `material:get` named another uniform. |
| `The shader "<name>" was compiled with another version of the shader library. Compile it again with "haylen.py shaders".` | A draw first used a shader compiled with another library. |
| `The camera zoom must be a number.` | A zoom of a camera is not a number. |
| `The smallest zoom must be above 0 and not above the largest zoom.` | The `minZoom` of a camera is out of range. |
| `The largest zoom must not be below the smallest zoom.` | The `maxZoom` of a camera is out of range. |
| `The dead zone of a camera cannot be negative.` | The `deadZone` of a camera is negative. |
| `The drag margins of a camera cannot be negative.` | A drag margin of a camera is negative. |
| `A camera moves forward by a number of seconds that is finite and not negative.` | The `dt` of `camera:follow`, `camera:frame` or `camera:update` is wrong. |
| `Framing needs at least one point.` | The list of `camera:frame` is empty. |
| `A camera rig needs a blend time of zero or more and a positive zoom.` | An option of `graphics2d.newCameraRig` is out of range. |
| `A camera zone needs a name and an area of a positive size.` | A zone lacks a name or an area. |
| `The camera zone "<name>" needs a positive zoom.` | The zoom of a zone is not positive. |
| `A camera rail needs two points or more, or none to take it away.` | The rail of a rig has one point. |
| `A pixel canvas needs a width and a height of one pixel or more.` | The size of `graphics2d.newPixelCanvas` is under one pixel. |
| `A parallax layer needs a texture to draw.` | A parallax layer has no texture. |
| `A parallax layer needs a positive size to repeat.` | A repeated parallax layer has no size. |
| `A parallax layer needs a finite position and scroll.` | The position or scroll of a parallax layer is not finite. |
| `A parallax layer would draw more than 65536 copies to fill the view. Give it a larger size or repeat size.` | The copies of a parallax layer are too small. |
| `A tile layer needs at least one texture.` | The function `graphics2d.newTileLayer` got no texture. |
| `A tile layer needs a positive cell size and a chunk size from 1 to 256 cells.` | An option of `graphics2d.newTileLayer` is out of range. |
| `A frame of a tile layer has an id from 1 to 1048576, not <id>.` | The id of `layer:defineFrame` is out of range. |
| `A frame of a tile layer uses one of its <count> textures.` | The texture of a frame is past the list. |
| `The tile layer has no frame <id>.` | A cell names a frame that was never defined. |
| `The frames of a rectangle of a tile layer come in whole rows of its width.` | The buffer of `layer:setCells` is not whole rows. |
| `A stroke needs at least two points.` | A stroke has fewer than two points. |
| `The type "haylen.<Type>" has no member "<key>".` | A script read a key that the value does not have. |
| `The type "haylen.<Type>" has no writable property "<key>".` | A script wrote a key that the value does not have or that is read only. |
