# 🖌️ Rendering

This guide explains how Haylen draws a frame. It is for app developers who want to know where their draws go, what each kind of draw costs and how to keep a busy scene fast. It covers canvases and their order, design units and the viewport, cameras, the draw order, batching, smooth edges, lit canvases, post-processing and the GPU device under all of it. The [`haylen.graphics2d` reference](../lua-api/graphics2d.md) and the [`haylen.graphics` reference](../lua-api/graphics.md) list every function, and the [UI guide](../ui/ui.md) covers the interface that draws on top of the renderer.

## How a frame is drawn

The renderer is `graphics2d::Renderer` (`engine/include/haylen/2d/graphics/Renderer.hpp`). An app does not draw on the GPU right away. It records draw commands into canvases, and the renderer keeps them until the frame ends. Then it sorts them, merges them into as few GPU draw calls as it can, uploads the data of the whole frame once and submits the frame.

A frame goes through these steps:

1. After the app updates, `Engine::frame` calls `Renderer::beginFrame` with the viewport and the `clearColor` of `app.json`.
2. Scenes and plugins record their draws in `render` and then in `renderUi`. The functions `beginWorld`, `beginScreen` and `beginTarget` open a canvas, and each one closes the canvas before it. Every draw turns its data into GPU instances or vertices at once and stores a draw item with its sort key. During a scene transition this step runs twice: once for the scenes before the change and once for the scenes after it. Each run draws inside a capture of its own image, as [scene transitions](#scene-transitions) explains.
3. The method `Renderer::endFrame` submits the frame. The profiler shows it as `submit`. It creates the render targets that lighting, post-processing and metaballs need, casts the shadow maps of the frame on the task pool, sorts each canvas and merges its items into commands. Then it uploads the instance, vertex and index data of the whole frame. It renders the offscreen passes and the captures in the order the frame recorded them, then the screen pass, and commits.
4. The method `Device::collectGarbage` destroys the GPU objects whose last handle went away during the frame.

Canvases only exist inside this window, so draws belong in `render` and `renderUi`, never in `update`. A draw without an open canvas raises `No canvas is active. Call "beginWorld", "beginScreen" or "beginTarget" before drawing.`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawCircle(0, 0, 40, '#FFFFD040')
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Score 120', 40, 40, {size = 48})
    end,
})
```

## The GPU device

The engine never talks to a graphics library directly. It draws through `gpu::Device` (`engine/include/haylen/gpu/Device.hpp`), an interface of the engine for buffers, images, samplers, programs, pipelines, passes and draws. A device of each graphics API implements that interface, and a build carries the device of every graphics API of its platform. The renderer, the Lua API and apps look the same on every device, as the [layers of the architecture guide](../core/architecture.md#layers) explain.

The class `graphics::Device` (`engine/src/graphics/Device.cpp`) owns the GPU device that the host creates. It creates textures, single-channel alpha textures for font atlases and render targets on it. Every method runs on the frame thread, and one engine runs at a time, as the [architecture guide](../core/architecture.md#resource-ownership) explains.

Two layers check the GPU calls. With validation on, which debug builds and the tests turn on, each device turns on the validation of its graphics API, such as the debug layers of Direct3D, the validation layers of Vulkan and the debug output of OpenGL, and sends its messages to the log. The null device of the headless host checks every call against the contract of the GPU device in every build. The null device also reports the texture limits of desktop GPUs, 16384 pixels for 2D textures, so headless runs load full-size art like real devices.

The function `graphics.apiName()`, `graphics::Device::getApiName()` in C++, names the graphics API the device draws through. The names come from `gpu::Device::kApiNames`. The window takes the first API that starts, in the order of `window.graphicsApis` of `app.json` and then in the order of the platform:

| Platform | Graphics APIs in the order of the platform | `graphics.apiName()` |
| --- | --- | --- |
| macOS, iOS, tvOS, Mac Catalyst | Metal | `'metal'` |
| Windows | Direct3D 12, Direct3D 11, Vulkan, OpenGL | `'direct3D12'`, `'direct3D11'`, `'vulkan'`, `'openGl'` |
| Linux | Vulkan, OpenGL | `'vulkan'`, `'openGl'` |
| Android | Vulkan, OpenGL ES 3 | `'vulkan'`, `'openGlEs'` |
| Web | WebGPU, WebGL2 | `'webGpu'`, `'webGl2'` |
| Headless host of tests | None | `'none'` |

The [build guide](../tools/build.md#cmake-options) explains which devices a build carries. Some graphics APIs store the bottom row of a render target first. The engine corrects that on its own, so the texture of a render target samples upright everywhere.

```lua
local graphics = require('haylen.graphics')
local log = require('haylen.log')

log.info('This device draws through ' .. graphics.apiName() .. '.')
```

## The shaders of the engine

The shaders of the engine live in `engine/shaders`. They are written in the [Haylen shader language](shaders.md#the-haylen-shader-language) and compiled by the Haylen shader compiler `haylen-shaderc` when the engine builds. A build keeps only the code of the shader languages of its own graphics APIs. Each source holds the programs of one kind of draw:

| Source | Programs |
| --- | --- |
| `sprite.glsl` | The sprite program, and the batch program for batches of several textures or with sprite data. |
| `text.glsl` | The text program. |
| `mesh.glsl` | The mesh program for meshes, polygons and strokes. |
| `shape.glsl` | The program of [shapes](#shapes-and-smooth-edges). |
| `recolor.glsl` | The program that [recolors sprites by parts](#recoloring-by-parts). |
| `effect.glsl` | The program of [sprite effects](#sprite-effects). |
| `blend.glsl` | The image blend patterns of scene transitions, groups and masks. |
| `backdrop.glsl` | The program that draws the image of a group whose blend mixes with what lies below it. |
| `metaball.glsl` | The surface of metaballs. |
| `light.glsl` | The light pass. |
| `composite.glsl` | The lighting and post-processing composite, and the filter program that shrinks and blurs images for bloom and blur. |

The programs that draw into lit canvases compile a second time with `HAYLEN_LIT`, which makes twenty-two programs in all. Every program shares the shader library in `engine/shaders/include/haylen`, which the custom shaders of apps include too, as the [shader guide](shaders.md#the-shader-library) explains. The renderer makes each program the first time a draw needs it. Some graphics APIs compile a program when the program is made, such as Metal. On those, the GPU device starts compiling every source on the I/O pool when the renderer starts, as [shader load time](shaders.md#load-time) explains.

## Canvases

A canvas is a place that draws go into. Every draw goes into the canvas that is open. There are three kinds:

| Canvas | Started with | Coordinates | Lighting and post-processing |
| --- | --- | --- | --- |
| World | `beginWorld(camera, options)` | World units through the camera. The view covers the camera viewport, or the visible design area without one, divided by the zoom. | Yes. |
| Screen | `beginScreen(options)` | Design units of `viewport.visibleRect()`. | No. |
| Render target | `beginTarget(target, camera, options)` | World units through the camera. The view covers the camera viewport in pixels of the target, or the whole target. | Yes, composited into the target. |

Use a world canvas for the game world and a screen canvas for the HUD and menus. Use a render target canvas for anything you want as a texture, such as a minimap or a mirror.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local mirror = graphics.newRenderTarget(256, 144)
local mirrorCamera = graphics2d.newCamera()
mirrorCamera.position = {128, 72}

scene.push({
    render = function(self)
        graphics2d.beginTarget(mirror, mirrorCamera, {clear = '#FF10202A'})
        graphics2d.drawCircle(128, 72, 40, '#FFFFD040')

        graphics2d.beginWorld(camera)
        graphics2d.drawRect({-200, -100, 400, 200}, '#FF3A7D44')
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.draw(mirror.texture, 20, 20, {pivotX = 0, pivotY = 0})
    end,
})
```

### The order canvases reach the screen

The engine submits canvases in two steps.

First, the offscreen work runs in the order the frame recorded it:

- The metaball fields of every canvas.
- Every render target canvas, cleared to its `clear` color or to transparent.
- The scene pass of every canvas with lighting or post-processing, followed by its light pass when the canvas is lit.
- The post-processing materials of a canvas, all but the last one.
- The composite of a render target canvas into its target.
- Every [capture](#captures), once the canvases before its end have their passes.

Second, the screen pass clears the framebuffer with the `clearColor` of `app.json`. It draws every world and screen canvas that no capture took, by their `order` and then in the order they began. It draws inside the rectangle of the viewport that the [scaling policy](#design-units-and-the-viewport) computes, so letterbox bars show the clear color. A world canvas draws only inside the part of that rectangle that its camera viewport covers. Render targets render first. So a screen canvas that draws the texture of a render target shows what the target recorded in the same frame, whatever the order of the two canvases.

A world canvas with lighting or post-processing reaches the screen as one image that covers its whole viewport. The image blends with the premultiplied alpha of its scene, and its clear color makes it opaque by default. So it hides everything that earlier canvases drew there, unless its clear color is transparent. Begin such a canvas before the canvases that must show on top of it, or give those canvases a higher `order`.

Every such canvas keeps render targets of its own, with the pixel size of its viewport, so split screens and minimaps light each view on its own. It keeps the scene image, and when it is lit, the emission, surface and info images and the light map. It adds a distortion map when it has distortion draws, and two more targets when it has post-processing materials, bloom or blur. Bloom and blur each add a pair of half-size targets, and a pair at every smaller size their radius reaches.

### Clear colors

Only offscreen canvases take a `clear` color. Screen canvases and world canvases without lighting or post-processing draw straight onto the screen, over what earlier canvases drew, so they reject a `clear` option with an error. A render target canvas clears to transparent by default.

### Clips

The functions `graphics2d.pushClip(rect)` and `graphics2d.popClip()` limit the next draws of the open canvas to a rectangle in the coordinates of the canvas. A clip keeps the pixels whose centers lie inside it, and each edge rounds to the nearest pixel boundary on its own. Nested clips intersect, and a new canvas starts without clips.

Every canvas also covers whole pixels of its destination. The viewport of the screen starts and ends on pixel boundaries, and so does the part of the screen that a camera viewport covers. So the canvases of a split screen share their edges without a gap.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.pushClip({100, 100, 300, 80})
        graphics2d.drawText(nil, 'This line stops at the edge of its box', 100, 110, {size = 40})
        graphics2d.popClip()
    end,
})
```

## Design units and the viewport

An app draws and lays out in design units, not in pixels. The `design` section of `app.json` sets the design size, 1920 by 1080 units by default, and a scaling policy, `expand` by default. The viewport maps the design area onto the real framebuffer by that policy, so the same layout fits a phone, a tablet and a desktop window. The [`haylen.viewport` reference](../lua-api/viewport.md#scaling-policies) lists the policies, and `viewport.setScaling` and `viewport.setDesignSize` change them while the app runs.

Three rectangles describe the mapping:

- The visible rectangle, `viewport.visibleRect()`, is the part of the design space that the screen shows, in design units. With `expand` it can start at negative coordinates and reach past the design size. Screen canvases draw in this space, and a camera without a viewport shows this area divided by its zoom.
- The safe rectangle, `viewport.safeRect()`, is the visible area that no notch, rounded corner, system bar or native view covers. Put buttons and important HUD text inside it.
- The pixel rectangle, `viewport.pixelRect()`, is the part of the framebuffer that shows the visible rectangle, on whole pixels. Outside it, the letterbox bars of `fit` and `pixelPerfect` show the clear color.

The engine recomputes the viewport at the start of every frame, so a resize or a rotation shows from the next frame on. Pointer and touch positions arrive in design units already. Inside a world canvas, `graphics2d.canvasUnitSize()` tells how long one design unit of the screen is in world units, so outlines keep one thickness at any zoom.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local viewport = require('haylen.viewport')

scene.push({
    renderUi = function(self)
        local safe = viewport.safeRect()
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Lives 3', safe:left() + 24, safe:top() + 24, {size = 40})
        graphics2d.drawText(nil, 'Pause', safe:right() - 24, safe:top() + 24, {size = 40, anchor = {1, 0}})
    end,
})
```

## Cameras and parallax

A camera, `graphics2d::Camera`, decides which part of the world a world or render target canvas shows. Its position is the point the view shows. The method `follow` moves the camera toward a target through these steps:

- The dead zone, an area where the target moves without moving the view.
- The drag margins on each side, with their drag offset.
- The look-ahead, which leads the target by its velocity.
- The position smoothing.
- The limits, with optional limit smoothing.

The method `update` advances the shake, the flash and the rotation smoothing. The frequency, direction and amount of the shake are settings. Every world canvas of the camera draws the flash over what it holds as it closes.

The zoom has two axes and limits. These methods help with common needs:

- The method `zoomAt` zooms around a point of the screen, for pinch and wheel input.
- The method `frame` keeps several targets in view.
- The function `Camera::blend` returns a view between two cameras, for smooth cuts.
- The method `drawDebug` draws the view, the limits and the drag box.

A camera with a `viewport` draws into that rectangle of the screen, and every canvas of the viewport clips to it. This is how one frame draws split screens and minimaps with several cameras.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
camera.limits = {0, 0, 4000, 1200}
camera.positionSmoothing = true
local player = {x = 100, y = 600}

scene.push({
    update = function(self, dt)
        player.x = player.x + 200 * dt
        camera:follow(player.x, player.y, dt)
        camera:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawCircle(player.x, player.y, 24, '#FFFFFFFF')
    end,
})
```

A camera rig, `graphics2d::CameraRig`, directs a camera through the zones of a level and along a rail. A zone is an area that gives the camera its own limits, which default to the area, its own zoom and its own offset while the target is inside it. Where zones overlap, the zone of highest priority wins, and between equal zones the camera stays in the zone the target is already in. The rig eases the limits, zoom and offset into a zone it enters over its blend time, along a smoothstep curve. Limits that arrive from outside every zone ease through the limit smoothing of the camera instead. A rail is a line, such as the track of a side-scrolling level, and the camera follows the point of the rail nearest the target. The rig then moves the camera with `follow`, so the dead zone, the drag margins and the smoothing still apply.

A pixel canvas, `graphics2d::PixelCanvas`, renders pixel art at a low resolution. It draws into a render target one pixel larger on every side, through a copy of the camera whose drawn position is rounded to whole pixels of the canvas. Then it draws the target scaled up, with its source moved by the part of a pixel that the camera lay between. The art keeps whole pixels while the screen scrolls in its own small steps, which smooths the slow scrolling of a canvas scaled several times. A turned view keeps whole pixels without the smooth shift.

A parallax layer, `graphics2d::Parallax`, scrolls a texture at its own rate as the camera moves, which suggests depth. It repeats its texture on either axis over the part of the world the camera sees, scrolls by itself over time and stops following the camera outside its limits. Its offset also moves static batches through `drawStatic`, so baked layers get parallax without baking again. The [camera](../lua-api/graphics2d.md#camera), [camera rig](../lua-api/graphics2d.md#camerarig), [pixel canvas](../lua-api/graphics2d.md#pixelcanvas) and [parallax](../lua-api/graphics2d.md#parallax) references describe every setting.

## Draw order

Every draw carries a `graphics2d::DrawOrder` with its `layer`, `depth`, `sortOffset`, `sortId`, `visibility` and `blend`. Inside a canvas, draws go by layer, lowest first. The function `pushLayerOffset` moves the layer of every draw of a scope, so a group of draws changes layer as one.

The `sort` option of the canvas decides how draws of one layer are ordered:

- The default, `sort = 'layer'`, ignores the depth and the y of draws.
- With `sort = 'depth'`, `Renderer::SortMode::Depth` in C++, draws of one layer also go by `depth`, lowest first.
- With `sort = 'y'`, `Renderer::SortMode::Y` in C++, draws of one layer go by the y they stand on plus their `sortOffset`. Sprites stand on their pivot, and every sprite of a batch sorts on its own. Text stands on its position and rich text on the bottom of its block. Shapes, meshes, nine-slices, static batches, image blends and metaballs stand on their lowest point.

Draws that sort the same go by their `sortId`, lowest first, and draws with the same `sortId` keep the order the app recorded them in. So a canvas that never sets a layer draws in the order of the calls. Some apps walk their objects in an order that changes, such as the keys of a table. They give each object its id as `sortId`, so two objects on one row never swap places.

A draw whose `visibility` bits share none with the `visibilityMask` of its canvas is dropped when it is recorded, so it costs nothing later. Use this to leave details out of a minimap.

Depth sorting is how top-down games draw units that stand in front of each other: the depth of each unit is the y of its feet. Sorting by y does the same without setting any depth:

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local trees = {}
for index = 1, 3 do
    trees[index] = graphics2d.newSprite(graphics.whiteTexture(), {x = index * 120, y = 300 - index * 40, width = 40, height = 120, pivotY = 1, layer = 1})
end

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {sort = 'depth'})
        graphics2d.drawRect({-1000, -1000, 2000, 2000}, '#FF3A7D44')
        for _, tree in ipairs(trees) do
            tree.depth = tree.y
            tree:draw()
        end

        -- The same trees sorted by y, with the ground on a lower layer so it never sorts among them.
        graphics2d.beginWorld(camera, {sort = 'y'})
        graphics2d.drawRect({-1000, -1000, 2000, 2000}, '#FF3A7D44', {layer = -1})
        for _, tree in ipairs(trees) do
            tree:draw()
        end
    end,
})
```

### Blend modes

The blend mode decides how a draw mixes with what is below it. The GPU blends the modes `alpha`, the default, `additive`, `multiply`, `screen`, `premultiplied` and `opaque` on its own. The modes `overlay`, `darken`, `lighten`, `colorDodge`, `colorBurn`, `hardLight`, `softLight`, `difference` and `exclusion` must read the color below, which the blending of the GPU cannot do, so only groups take them. The [blend modes reference](../lua-api/graphics2d.md#blend-modes) describes each one.

A canvas with such a group renders offscreen. Where the group lies among its draws, it stops its pass, copies the image it drew so far and goes on in a new pass that keeps that image. Then the `backdrop` program draws the image of the group and reads the copy. Each such group therefore costs a copy of the canvas and two passes.

Draws write straight colors, the way images and colors are usually made. The blends `multiply` and `screen` need colors multiplied by their alpha, so the shaders multiply them first. So a partly transparent multiply or screen draw weakens its effect instead of brightening what is below. Offscreen images, and the clear colors of their passes, hold premultiplied colors.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawRect({-300, -200, 600, 400}, '#FF3A7D44')
        graphics2d.drawCircle(0, 0, 120, '#80FFC040', {blend = 'additive'})
        graphics2d.beginGroup({blend = 'overlay', layer = 1})
        graphics2d.drawRect({-100, -100, 200, 200}, '#FFFF8040')
        graphics2d.endGroup()
    end,
})
```

## Batching

After it sorts a canvas, the renderer walks its draws and merges neighbours into one GPU draw call when they share all of these:

- The program, such as the sprite, shape or text program.
- The blend mode.
- The clip rectangle.
- The texture.
- The shading: the material with its values and, in lit canvases, the lighting options and the layer of the draw.

The renderer lays the instance data out in draw order before the upload. So any run of neighbours that match becomes one instanced draw, whatever order the app recorded them in. A different texture, blend mode, program, clip or shading between two draws starts a new draw call, and a static batch is always a draw call of its own.

This table shows which program and texture each kind of draw uses, so you can tell which draws can merge:

| Draws | Program | Texture |
| --- | --- | --- |
| `graphics2d.draw`, `sprite:draw()`, `batch:draw()`, `graphics2d.drawNineSlice`, particle emitters | sprite | Their texture. |
| `graphics2d.drawVector` | sprite | The page of the vector atlas that holds the raster. |
| Sprites and batches with a `partMask` | recolor | Their texture, and the mask as a second texture. |
| Sprite batches with several textures, or with sprite data and a material | batch | Their textures, up to 8, which each sprite picks from. |
| Sprites with an `effect` | effect | Their texture. |
| `graphics2d.drawRect` | sprite | The white texture. |
| `graphics2d.drawCircle`, `drawEllipse`, `drawPoints`, `drawRing`, `drawArc`, `drawShape` | shape | The white texture, or the `texture` of a shape. |
| `graphics2d.drawPolygon`, `drawRectOutline`, `drawLine`, `drawPolyline` | mesh | The white texture. |
| `graphics2d.drawMesh`, `graphics2d.drawStroke`, `mesh:draw()` | mesh | Their texture, or the white texture. |
| `graphics2d.drawText` | text for TrueType fonts, sprite for bitmap fonts | The atlas of the font or its pages. |
| `graphics2d.drawRichText`, `richText:draw()`, the text of `haylen.ui` components | text for TrueType glyphs, sprite for bitmap glyphs, images and boxes | The atlas or page of each font, each image and the white texture. |
| `graphics2d.drawStatic` | sprite, from the buffer of the batch | The texture of the batch. |
| `graphics2d.drawLight` | light, one draw call for each run of matching lights, into the light map | The light texture, the surface and info targets and the shadow atlas. |
| `graphics2d.drawMetaballs` | sprite into its field, then metaball, one draw call each | The field. |
| `graphics2d.drawImageBlend` and the shader-based scene transitions | blend, one draw call each | Both images. |
| `haylen.ui` and `haylen.imgui` | mesh, one clip rectangle per ImGui command | The UI textures. |

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local atlas = assets.texture('sprites/props.png')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        -- These three sprites share the texture, the program and the blend, so they make one draw call.
        graphics2d.draw(atlas, 100, 100, {source = {0, 0, 64, 64}})
        graphics2d.draw(atlas, 200, 100, {source = {64, 0, 64, 64}})
        graphics2d.draw(atlas, 300, 100, {source = {128, 0, 64, 64}})
        -- A rectangle uses the white texture, so it starts a second draw call.
        graphics2d.drawRect({100, 200, 300, 20}, '#FF202020')
    end,
})
```

### Inside a batch

Sprites, glyphs, nine-slice pieces and metaball splats are instances of one shared quad. Each instance is 48 bytes: position, size, texture coordinates, color, flash color, rotation, four parameter bytes and pivot. The sprite program reads the flip flags from the first parameter byte. The texture coordinates are four numbers of 16 bits from 0 to 1, which address every texel of an atlas exactly. A source rectangle that reaches past its texture, such as a tile repeated over a floor, spreads its coordinates over a range of 2, 4 or more textures, centered on 0 when it starts left of or above the texture. The size of that range takes the free bits of the first parameter byte, so the sampler repeats or mirrors the texture across the quad by its `wrap`. The text program reads the outline, the weight and the softness of a glyph from the first, second and fourth bytes. Both lean the quad around its pivot by the skew in the third byte.

Some programs need more per instance:

- A recolored sprite takes a second record of the same size right after its own. Its first sixteen bytes hold the four part colors, and the recolor program reads both records at the same instance through two vertex buffers.
- A sprite of the batch program takes a second record the same way, with the four numbers of its data and the index of its texture. The index picks one of the textures of the batch, which are all bound at once and sampled with the sampler of the first.
- A shape is one 48-byte record of its own in the same stream, which the shape program reads.
- Polygons and meshes are indexed triangles of 20-byte vertices.

Every instanced program samples its texture only inside the texels of the source rectangle of the quad, half a texel in from its sides. So linear filtering never reads the neighbours of a region of an atlas, or the borders around a stretched nine-slice center. A region one texel wide fills its quad with that texel.

The renderer keeps one streaming buffer each for instances, vertices and indices. It writes each of them once per frame and grows them to one and a half times the need when a frame outgrows them. When the app recorded the draws in their final order, the instances upload as recorded, and otherwise they are first copied into draw order.

Everything a frame records and submits goes into storage that the renderer keeps from frame to frame. That covers the draw items, the instances, the sorted order, the commands, the text painter and the resources a frame keeps alive. Once that storage has grown to what the app draws, a frame of any kind of draw allocates nothing in the renderer.

### Measuring a frame

The function `graphics2d.stats()`, `Renderer::getStats()` in C++, reports the counters of a frame: canvases, passes, draw calls, sprites, uploaded instances, vertices, indices, lights, occluders, shadow maps, texture switches and uploaded bytes. The compact statistics and the debug overlay, which F3 cycles as the [debug reference](../lua-api/debug.md) describes, show them live. The web runtime passes them to the page through `onStats`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        local stats = graphics2d.stats()
        if stats.drawCalls > 200 then
            print('The last frame took ' .. stats.drawCalls .. ' draw calls.')
        end
    end,
})
```

## Shapes and smooth edges

Every curve the renderer draws has an edge that fades over one pixel of its destination. This holds at any size, zoom, rotation, screen density and graphics API, without multisampling. A circle, an ellipse, a point of `drawPoints`, a ring, an arc and every shape of `graphics2d.drawShape`, `Renderer::drawShape` in C++, is one quad around a rectangle with rounded corners or an ellipse. That shape is a `graphics2d::Shape` (`engine/include/haylen/2d/graphics/Shape.hpp`), and the [UI](../ui/ui.md#smooth-edges) draws the curves of its components with it too.

This is how the shape program draws:

- The record of a shape holds its center, half size and rotation, and its softness and margin as half floats. It also holds its fill and border colors, its border width, its sweep, the angle the sweep starts at and the radius of each corner. Its flags say whether it is an ellipse and whether its texture is flipped.
- The vertex stage grows the quad by the margin: one pixel of the destination past the edge plus half the softness. The renderer works the margin out from the view of the canvas and the size of its pass when the shape is drawn.
- The fragment stage measures the exact signed distance from the center of each pixel to the edge. For a rectangle it uses the radius of the corner the pixel lies near. For an ellipse it finds the nearest point of the edge by bisection, so the distance is exact at any ratio of the radii, and a flat ellipse measures its depth across its short side. It cuts the shape to the sector of the sweep.
- It turns that distance into coverage over the size of one pixel, which it takes from the screen-space derivatives of the point. So the fade stays one pixel wide inside render targets, captures, lit canvases and turned or zoomed cameras alike. The softness widens the fade, which is how soft shadows draw.
- The border runs inside the edge and the fill inside the border, both measured from the same distance. So they meet without a seam, and the outer edge of the border is the edge of the shape.
- A thin shape loses the coverage past its far side, and a thin border the coverage past its inner edge. So a hairline ring or a capsule thinner than a pixel covers only the share of the pixels it crosses.
- A shape with a texture stretches it over its rectangle under the fill color. The texture is clamped inside, so the fade past the edge never wraps around a texture that repeats.
- The shader library applies the coverage in `haylen_output`, so a [material](shaders.md) that shades a shape keeps its edge whatever color it writes.

Polygons are tessellated instead, since their edges are straight. The fill stops half a pixel inside the outline, and a fringe of triangles fades it to nothing half a pixel outside. Corners move no further than two half pixels. `PolygonMesh` (`engine/src/2d/graphics/PolygonMesh.cpp`) builds them:

- The outlines of a shape fill by the nonzero rule. `math::Triangulator` (`engine/src/math/Triangulator.cpp`) finds the outline around every outline, so an outline wound opposite to the one around it is a hole. It joins every hole to the outline around it through a bridge to a vertex the hole sees, and clips the joined ring one ear at a time.
- The edge of a hole fades into the hole. An edge that two outlines share, such as the edge between two convex pieces or two chunks of a terrain, gets no fringe. The fills of both outlines reach it, and a corner on it slides along it, so the pieces join without a seam.
- The triangulator, the edges and the vertices keep their storage in the renderer, so a shape drawn every frame allocates nothing once its buffers have grown.

Strokes of `graphics2d.drawStroke`, `Renderer::drawStroke` in C++, are tessellated the same way by `StrokeBuilder` (`engine/src/2d/graphics/StrokeBuilder.cpp`):

- Every cross section of a stroke is four vertices: the outer edge and the core on each side. The core lies half a pixel inside the edge and the outer vertices half a pixel past it. A section joins the one before it with three quads.
- A miter join is one section whose offsets grow longer along the bisector. A bevel or round join and a round cap are runs of sections around one point. Their outer side turns in steps that keep the arc within a quarter pixel, and their inner side stays at the corner of the offset edges. Butt and square caps add a section that fades past the end.
- A stroke thinner than a pixel keeps no core and fades by the share of the pixel it covers.
- Lines, polylines and the outlines of rectangles are strokes too, with butt ends and miter joins. So their edges fade like the edges of strokes, a thin line never breaks into dashes, and a translucent polyline covers each pixel once at its turns.
- Dashes cut the length into separate runs, whose texture coordinates keep counting along the whole stroke.
- A stroke along a `math::Spline` first flattens the curve into points whose lines stay within a quarter pixel of it. It splits each segment where its middle strays from its chord.
- The builder keeps its storage in the renderer, so a stroke drawn every frame allocates nothing once its buffers have grown. The stroke draws on the mesh program with the other meshes of its texture and order.

Kept meshes, `graphics2d::Mesh`, hold their vertices in C++ between draws. A deformation moves them with one call of `mesh:setPositions` from a float buffer, and the draw copies them into the streaming buffer through the transform it is given.

Rectangles, lines and sprites keep hard edges. So rectangles that share an edge tile without a seam, and lines on whole pixels stay crisp.

The `sampleCount` of `app.json` multisamples only the window. Render targets, captures and the images of lit and post-processed canvases have one sample, so no edge of the engine depends on it.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawShape({200, 208, 320, 120}, {radius = 16, color = '#80000000', softness = 24})
        graphics2d.drawShape({200, 200, 320, 120}, {radius = 16, color = '#FF232938', borderWidth = 3, borderColor = '#FF8FB0FF'})
        graphics2d.drawStroke({600, 300, 700, 220, 800, 300}, {width = 0.5, color = '#FFFFFFFF'})
    end,
})
```

## Sprites, sprite batches and static batches

The renderer offers three ways to draw sprites, from the most flexible to the fastest:

| Way | Lua | C++ | Cost per frame |
| --- | --- | --- | --- |
| Single sprite | `graphics2d.draw`, `sprite:draw()` | `Renderer::draw(const Sprite&)` | One call from Lua into the engine, and one instance converted and uploaded. |
| Sprite batch | `graphics2d.newSpriteBatch`, `batch:draw()` | `SpriteBatch::draw`, `Renderer::drawBatch` | Every instance converted and uploaded, with no Lua call per sprite. |
| Static batch | `batch:bake()`, `graphics2d.drawStatic` | `SpriteBatch::bake`, `Renderer::createStaticBatch`, `Renderer::drawStatic` | One draw call, with nothing converted or uploaded. |

A `SpriteBatch` keeps its sprites in C++ and shares one texture. It can also share up to 8 textures with the same filter, wrap and mipmaps, and each sprite picks one. So sprites of separate images that a layer interleaves still make one draw call without an atlas, as [batches of several textures](../lua-api/graphics2d.md#batches-of-several-textures) explain. The method `batch:set(index, fields)` changes only the fields it is given, so a script touches only the sprites that move. The method `drawBatch` converts batches of 8,192 sprites or more on the worker pool with `JobSystem::parallelFor`, while the frame thread takes its own share. The web build has one thread, so it converts them inline.

The method `batch:bake()` copies the sprites into a GPU buffer that never changes, so bake again to change the copy. The functions `graphics2d.drawStatic(batch, x, y, order)` and `Renderer::drawStatic(batch, order, offset)` shift the whole batch by an offset in world units without touching its data. This lets parallax and scrolling layers reuse one batch. Each static batch holds one buffer of the buffer pool until its last handle goes away.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local stars = graphics2d.newSpriteBatch(graphics.whiteTexture())
for index = 1, 20000 do
    stars:add({x = math.random(-4000, 4000), y = math.random(-2000, 2000), width = 2, height = 2})
end
local sky = stars:bake()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawStatic(sky, camera.x * 0.8, camera.y * 0.8, {layer = -1})
    end,
})
```

## Textures and atlases

A texture made with the `linear` filter samples the pixels around each point. So the color of the transparent pixels next to what shows bleeds into its edges, often as a dark or bright fringe. To stop this, every image for a linear texture is bled when it decodes on the worker pool. The transparent pixels around what shows take the color of the nearest pixel that shows. This reaches up to `graphics::Image::kBleedReach` pixels and keeps their alpha at zero.

The texture option `mipmaps` keeps smaller copies of the image, down to one pixel, made when it decodes. A texture drawn smaller than its size samples those copies instead of skipping pixels. The copies average each block of pixels by alpha, so transparent pixels never darken them. Render targets and textures that change in place have no mipmaps.

Compressed texture formats are left out, because no format decodes on every graphics API of the engine. Desktop GPUs read BC, mobile GPUs read ETC2 and ASTC, and WebGPU and WebGL2 offer each one only where the device has it. A compressed release would need a copy for every family of devices.

Separate textures split batches, as the [render benchmark](#render-benchmark) shows. The class `animation2d::AtlasBuilder`, `animation2d.newAtlasBuilder` in Lua, packs images, areas of images and the frames of whole atlases with their animations into one texture while the app runs:

- It trims transparent borders and remembers where each frame sat.
- Frames that show the same pixels share one place.
- It places the frames tallest first on a skyline, at the lowest place that fits. It tries every power of two width up to the largest size and keeps the one with the smallest square.
- It repeats the edges of each frame into its padding and bleeds the result.

Packing is plain work on `graphics::Image` values, so `builder:buildAsync` runs it on the task pool and only makes the texture on the frame thread. The method `atlas:subset(prefix)` hands each packed atlas back under its own names. Tiny Island packs its characters, props and effects this way when its menu loads. On an M5 Pro, a frame of 2,535 sprites goes from 136 draw calls with 121 texture switches to 44 draw calls with 30. Those remain for the map, the water, the particles and the interface. The frame time moved within the noise of the machine, between 2 and 8 milliseconds of CPU in both cases. The frame thread of that frame spends little on draw calls.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local async = require('async')

async.spawn(function()
    local builder = animation2d.newAtlasBuilder()
    builder:add('hero', assets.loadAsync('characters/hero.png', 'image'):await())
    builder:add('chest', assets.loadAsync('props/chest.png', 'image'):await())
    local atlas = builder:buildAsync({filter = 'linear'}):await()
    print(atlas.texture.width, atlas.texture.height)
end)
```

## Nine-slices

A nine-slice, `graphics2d::NineSlice` (`engine/include/haylen/2d/graphics/NineSlice.hpp`), is a texture with nine source rectangles, row by row from the top-left corner. It draws a frame of any size whose corners keep their shape. The method `NineSlice::fromBorders` cuts one source rectangle with fixed borders, and `NineSlice::fromPieces` takes nine separate rectangles, such as the pieces an atlas packs apart. In Lua, `graphics2d.newNineSlice(texture, {borders = ...})` or `{pieces = ...}` builds one.

The method `NineSlice::layout(area, borderScale, patches)`, `slice:layout(rect, borderScale)` in Lua, cuts an area into the quads of the frame. The method `Renderer::drawNineSlice(slice, area, color, order, borderScale)`, `graphics2d.drawNineSlice(slice, rect, color, options)` in Lua, draws them by these rules:

- Each border is as wide as the widest piece of its side column, or as tall as the tallest piece of its side row. So separate pieces of different sizes, such as the ones an atlas packs, keep their corners whole.
- The corners keep their size times `borderScale`. When the area is narrower or shorter than the scaled borders of an axis, those borders shrink in proportion until they meet, and the pieces between them draw nothing.
- With the `stretch` fill, the edges and the center stretch over their part.
- With the `tile` fill, an edge repeats along its length at the scale its border gives its piece. A piece thinner than its border stretches across the border and keeps its shape along it. The center repeats at the scaled size of its piece.
- Copies are placed by their index, and the last copy of a row or column is cropped from the start of its piece. A last copy thinner than a thousandth of a tile, which is a rounding error of the length, is left out.
- Every quad samples only the texels of its piece, so linear filtering never reads the gaps between the pieces of an atlas. A stretched center one texel wide fills its part with that texel.
- Render target textures that store their bottom row first flip their pieces, so a nine-slice drawn from a render target shows upright. A world canvas with a turned camera turns the whole frame.

All pieces go into one draw item with the texture of the slice, so frames that share a texture batch together. The slices of a sprite atlas take their bounds from the untrimmed canvas of their frame, so a trimmed frame moves its slice by the pixels it dropped. A slice that reaches past the pixels its frame keeps raises `The slice "<name>" reaches past the pixels its frame keeps in the atlas. Keep the slice inside the opaque pixels of the frame or export the atlas without trimming.`. UI themes use nine-slices for their surfaces, as the [UI guide](../ui/ui.md) explains. They draw them through the renderer at their place among the ImGui draws, so they lay out and sample exactly like `drawNineSlice`. The [`haylen.graphics2d` reference](../lua-api/graphics2d.md#nineslice) lists the errors.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local panel = graphics2d.newNineSlice(assets.texture('ui/panel.png', {filter = 'linear'}), {borders = {24, 24, 24, 24}, fill = 'tile'})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawNineSlice(panel, {660, 200, 600, 400}, '#FFFFFFFF', {borderScale = 0.75})
    end,
})
```

## Recoloring by parts

Recoloring by parts lets one set of images make every outfit of a character. A sprite or a sprite batch whose draw order has a `partMask` draws with the recolor program. The base is a white or gray sprite. The mask is a texture with the layout of the base. It marks every part in red, green, blue or yellow, such as the hat, the shirt, the trousers and the boots.

Each sprite has `PartColors` (`haylen/2d/graphics/PartColors.hpp`), with a color for each color of the mask. The program multiplies every part of the base by its color, which keeps the shading of the base:

- Yellow counts as red and green together, so it takes its own color and leaves the rest of each to its part.
- The alpha of the mask fades parts out at their soft edges.
- A part color mixes in by its alpha, so white or transparent keeps a part as it is.
- The color and flash of the sprite apply over the result.

Recolored sprites of one texture and mask share a draw call whatever their colors, since the colors travel in their instances and the GPU does all the work per pixel. In lit canvases they light with a flat normal. The [`haylen.graphics2d` reference](../lua-api/graphics2d.md#recoloring-by-parts) shows the Lua keys and the errors.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local villager = assets.texture('characters/villager.png', {filter = 'linear'})
local mask = assets.texture('characters/villager-mask.png', {filter = 'linear'})
local guard = graphics2d.newSprite(villager, {x = 400, y = 300, partMask = mask, partColors = {red = '#FFC0392B', green = '#FF2E86C1'}})

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        guard:draw()
    end,
})
```

## Sprite effects

A sprite effect, `graphics2d::SpriteEffect` (`SpriteEffect.hpp`), dissolves a sprite into noise with a colored edge and draws an outline and a glow around its visible pixels. A sprite with an active effect draws with the effect program. Like a recolored sprite, it takes a second record after its own in the instance stream. That record holds the colors of the dissolve edge, the outline and the glow. It also holds the dissolve with its edge and the size of its noise, the width of the outline and the size of the glow. The program reads both records at the same instance.

The vertex stage grows the quad by the reach of the outline and the glow. The fragment stage samples the texture only inside the source of the sprite and hides the pixels whose value noise falls under the dissolve. It colors the band just above the dissolve. For the outline it takes the most covered of twelve points around each pixel. For the glow it measures the distance to the sprite: twelve rays look for the first of twelve steps that the sprite covers, and place its edge between that step and the one before by their coverage. The glow fades with the square of that distance over its size, so it changes smoothly from pixel to pixel and shows no rings at any size. Then it draws the sprite over both.

Sprites of one texture share a draw call whatever their effects, and lit canvases light them like sprites. The [sprite effects reference](../lua-api/graphics2d.md#sprite-effects) lists the keys.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local hero = assets.texture('sprites/hero.png', {filter = 'linear'})

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.draw(hero, 400, 300, {effect = {outlineWidth = 3, outlineColor = '#FFFFFFFF', glowSize = 16, glowColor = '#C040C0FF'}})
    end,
})
```

## Vector images

A vector image, `graphics::VectorImage` (`haylen/graphics/VectorImage.hpp`), is an SVG document that NanoSVG reads into curves. It keeps paths and basic shapes with their fills and strokes, line joins, caps and dashes, linear and radial gradients, transforms, groups, opacity and the view box. A gradient lays itself out in the bounding box of its shape, the default of SVG, where `x2="1"` is the far side of the box, or in user space with `gradientUnits="userSpaceOnUse"`. The color `currentColor` paints white, so the color of a draw tints the parts that use it, the way icons follow the color of their text. Clip paths, masks, `use`, style sheets and text are left out.

The asset type `vectorImage` reads `.svg` files on the task pool, and `graphics.newVectorImage(text)` reads a document from Lua. The method `VectorImage::rasterize(scale)` turns an image into straight RGBA pixels on any thread, with the colors of its edges spread into the transparent pixels around them.

The function `graphics2d.drawVector`, `Renderer::drawVector(image, sprite)` in C++, draws an image in the place, size and look of a sprite. It works out the scale that the image covers on the screen: the size of the draw in pixels of its destination over the size of the image. Then it draws the raster of the step that covers that scale from the vector atlas of the renderer:

- Scales come in steps a quarter of an octave apart, about 19 percent. An image drawn at a size between two steps takes the larger raster and never blurs. An image that grows or zooms takes a sharper raster as it reaches each step.
- The first raster of an image is made at once on the frame thread, so the image shows from its first draw. Every other step is made on the task pool, and until it arrives the nearest step that the atlas holds draws in its place.
- Rasters live in pages of 1024 by 1024 pixels, one dynamic texture each, packed in shelves with a pixel between them. So the draws of many images at many sizes share a few textures and batch into few draw calls. A raster larger than a page takes a page of its own.
- Once the atlas holds eight pages, the page that drew longest ago starts over with a new texture, and its images make their rasters again when they draw.
- A page whose rasters changed uploads once per frame, before the passes.

The UI draws the SVG pictures of its components through `ui::ImageLibrary` (`engine/src/ui/ImageLibrary.hpp`) instead of the atlas, because ImGui draws each picture as a texture of its own. It rasterizes a document in the same steps for the pixels a component covers, every raster on the task pool with the nearest raster standing in. It keeps each raster as a texture. While the rasters pass its budget, it releases the ones that no frame drew in the last two frames, the longest unused first, as the [UI guide](../ui/ui.md#component-catalog) describes.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local heart = assets.vectorImage('icons/heart.svg')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawVector(heart, 100, 100, {width = 24, height = 24, color = '#FFFF8080'})
        graphics2d.drawVector(heart, 300, 100, {width = 240, height = 240, color = '#FFFF8080'})
    end,
})
```

## Text

Text draws in batches like sprites. Plain text of one TrueType font is one run of glyph quads of the text program, with the atlas of the font as its texture. Each font has its own atlas, so switching fonts between draws starts a new draw call, and a bitmap font draws its pages through the sprite program. Rich text draws in layers through a text painter, and the glyphs of one font share a draw call. Outlines and shadows add more quads. The [text guide](text.md#how-glyphs-are-drawn) explains how fonts make their glyphs, how the text program draws them and how layouts are cached.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        -- Both lines use the default font, so they share one draw call.
        graphics2d.drawText(nil, 'Gold 120', 40, 40, {size = 40})
        graphics2d.drawText(nil, 'Wood 35', 40, 90, {size = 40})
    end,
})
```

## Groups and masks

A group draws several draws as one image, so parts that overlap fade together instead of showing through each other. A mask draws content only inside, or outside, a shape drawn with any draws. Use `graphics2d.beginGroup` and `graphics2d.endGroup` for groups, and `graphics2d.pushMask` and `graphics2d.popMask` for masks. In C++ they are `Renderer::beginGroup` and `Renderer::endGroup`, and `Renderer::pushMask`, `Renderer::endMaskShape` and `Renderer::popMask`.

A group, and the shape and the content of a mask, each draw into an image of their own. That image is a render target canvas with the view and the options of the canvas they draw over, and with the pixel size of its pass. It has no clear color, no lighting and no post-processing. The images come from a pool that frames reuse in order, and a frame gives back the ones past what it used.

When a group or mask ends, its draws and metaballs move ahead of the draws of the canvas it drew over, together with those of the groups inside it. Its canvas moves ahead of that canvas too. So the draws of every canvas stay in one run, and its image renders before anything samples it. The canvas then opens again with the clips and layer offsets it had. It draws the image as one draw of the image blend program: a quad whose corners follow the view however the camera turns or zooms.

The `mask` pattern of `Renderer::drawImageBlend` multiplies the content by the alpha of the shape, or by what the shape leaves uncovered. So the edge of a mask is as smooth as the shape drawn into it. The opacity of a group scales the whole image, whose colors stay premultiplied. Masks need no stencil buffer and no multisampling, and they cost one extra pass each for the shape and for the content.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local portrait = assets.texture('ui/portrait.png', {filter = 'linear'})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.pushMask(function()
            graphics2d.drawCircle(120, 120, 64, '#FFFFFFFF')
        end)
        graphics2d.draw(portrait, 120, 120)
        graphics2d.popMask()
    end,
})
```

## Captures

A capture sends whole canvases into a render target instead of the screen. The functions `graphics2d.beginCapture(target, {clear = color})` and `graphics2d.endCapture()`, `Renderer::beginCapture(target, clear)` and `endCapture()` in C++, send the world and screen canvases that begin between them into the target. Lighting, post-processing, clips and viewports work as they do on the screen.

The visible area covers the whole target. So a target with the pixel size of the viewport, `viewport.pixelRect()`, holds the frame exactly as the screen would show it, ready to draw as a texture, blend or keep. The capture pass clears the target and draws its canvases by order. It runs once the canvases recorded before its end have their offscreen passes, and before any canvas recorded after it. So a later canvas that draws the captured texture shows this frame.

Render target canvases begun during a capture still draw into their own targets. A capture can begin inside another one, such as in a scene that a transition renders into its image. The inner capture renders before the outer one, and the outer one takes the canvases after the end of the inner one. A capture left open ends with the frame.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local viewport = require('haylen.viewport')

local pixels = viewport.pixelRect()
local frame = graphics.newRenderTarget(math.floor(pixels.width), math.floor(pixels.height))

scene.push({
    render = function(self)
        graphics2d.beginCapture(frame)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Captured', 60, 60, {size = 64})
        graphics2d.endCapture()

        graphics2d.beginScreen()
        graphics2d.draw(frame.texture, 960, 540, {width = 960, height = 540, rotation = 0.1, blend = 'premultiplied'})
    end,
})
```

## Scene transitions

A transition with an effect renders both sides of a change of the scene stack. The method `SceneManager::getViews` returns the views of the frame. Without a transition, the visible scenes draw straight onto the screen. During a transition there are three views:

- The outgoing image, a render target with the pixel size of the viewport, which holds the scenes before the change.
- The incoming image, a render target of the same size, which holds the scenes after the change.
- The screen, where the effect draws both images.

The class `Engine` renders every view inside a capture of its image. The plugins and the loading view draw in the view of the current scenes, and the view of the effect draws the effect before its own scenes.

An effect that covers the screen works in three phases:

1. It renders the current scenes into the outgoing image until full cover, where the stack changes.
2. While the next scene loads, the hold draws the effect at its switch point with the last outgoing frame and no scene at all. Only the loading view and the plugins draw on top, so the wait costs almost nothing.
3. The reveal renders the scenes after the change into the incoming image.

An effect that shows both scenes starts once the next scene has loaded. The scenes that leave the stack keep rendering into the outgoing image until its exit point. There they exit before that frame renders, so nothing draws them after they exit, and the image keeps their last frame.

The built-in effects of `graphics2d::SceneTransition` draw the images with sprites, perspective meshes and clipped polygons. The effects `dissolve`, `pixelate`, the radial wipes, `iris` and `pageTurn` use the patterns of `Renderer::drawImageBlend`, one full-rectangle draw of the `blend.glsl` program that reads both images.

The images hold colors premultiplied by their alpha, since the scenes render over the clear color, which a [transparent window](../platform/desktop.md#transparency) keeps transparent. So the built-in effects draw them with the `premultiplied` blend mode, which looks the same as `alpha` over an opaque clear color. Custom effects that should compose with the desktop do the same. The [scene reference](../lua-api/scene.md#changes) lists the effects and the Lua API, and custom effects in Lua or C++ receive both images too. In C++, a transition takes a `graphics2d::SceneTransition` or any `core::TransitionEffect`:

```cpp
#include <memory>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SceneTransition.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/core/TransitionEffect.hpp"

// Shows the incoming scene through a growing band in the middle of the outgoing one.
class BandTransition final : public haylen::core::TransitionEffect {
  public:
    [[nodiscard]] float getSwitchProgress() const noexcept override {
        return 0.0F;
    }
    [[nodiscard]] float getExitProgress() const noexcept override {
        return 1.0F;
    }
    void render(haylen::graphics2d::Renderer& renderer, const Frames& frames, float progress) override {
        renderer.beginScreen();
        const haylen::math::Rect area = renderer.getCanvasBounds();
        renderer.draw({.texture = frames.outgoing, .position = area.getMin(), .size = area.getSize(), .pivot = {}});
        const float height = area.height * progress;
        const haylen::math::Vec2 pixels = frames.incoming.getSize();
        renderer.draw({.texture = frames.incoming, .source = {0.0F, pixels.y * (1.0F - progress) * 0.5F, pixels.x, pixels.y * progress}, .position = {area.x, area.getCenter().y - height * 0.5F}, .size = {area.width, height}, .pivot = {}});
    }
};

void openLevel(haylen::core::Engine& engine, std::shared_ptr<haylen::core::Scene> level, std::shared_ptr<haylen::core::Scene> map) {
    using haylen::graphics2d::SceneTransition;
    engine.getScenes().replace(std::move(level), {.transition = {.duration = 0.6F, .effect = std::make_shared<SceneTransition>(SceneTransition::Options{.kind = SceneTransition::Kind::PageTurn, .direction = SceneTransition::Direction::Left})}});
    engine.getScenes().push(std::move(map), {.transition = {.duration = 0.4F, .effect = std::make_shared<BandTransition>()}});
}
```

## Lighting

A world or render target canvas becomes lit when it receives `ambientLight`. The [lighting guide](../2d/lighting.md) explains lights, occluders and what a light reaches, and this section explains the passes on the GPU.

The scene pass of a lit canvas renders into four targets with the pixel size of the canvas at once. It uses pipelines with four color attachments and the lit programs, which write through `haylen_output` of the shader library:

| Target | Holds |
| --- | --- |
| Scene | The colors of the draws, cleared to the `clear` color of the canvas. Unshaded draws leave black here. |
| Emission | The colors of unshaded draws, and the colors of draws with `emission` times that strength. |
| Surface | The world normal of normal-mapped sprites in red and green, and their specular strength in blue. |
| Info | The light mask, the layer shifted by 128 and the shininess of the draw divided by 255. A draw writes it only where it covers more than half of the pixel, so these values never blend between two draws. |

Draws with the `alpha`, `premultiplied` and `opaque` blends write the surface and the info. Draws with `additive`, `multiply` and `screen`, such as glows and shadows, leave them alone, and multiplying draws leave the emission alone too. OpenGL and WebGL2 blend every color attachment of a pass with the blend of the first, so every pipeline of a lit pass gives its four targets the blend of the draw, which `gpu::Device` requires of all its pipelines. The lit block of the shader library tells `haylen_output` how the blend covers the targets, and a draw writes zero into a target it must leave alone, which no blend of a lit pass changes the target with. A premultiplied draw premultiplies the surface and the info it covers.

The light pass then draws every light of the canvas into the light map, which starts at the ambient color, in the order the lights were drawn:

- Each light is one quad. Point and spot lights take a quad twice their radius wide, turned with them, and directional lights take the view of the canvas.
- Every light is an instance of 144 bytes in a buffer that the frame writes once. Neighbouring lights that share a blend mode and a texture draw as the instances of one call, so hundreds of lights cost a few draw calls.
- The light shader reads the info target at its pixel and skips the pixels whose light mask or layer the light does not reach. It applies the falloff of the light texture and the cone of spot lights. For normal-mapped pixels it applies the angle to the light and the highlight of their specular strength. Then it applies the shadow map.
- The pipeline blend of the light adds, subtracts or mixes the result into the light map.

The composite multiplies the scene by the light map, adds the emission and applies the post-processing.

The light map is `RGBA16F` when the GPU device can render into it and blend it, so light goes past 1 and below 0. Otherwise it uses the offscreen format, where light stops at 1. The functions `graphics2d.hdrLighting()` and `Renderer::isHdrLighting()` tell which one the device uses. A light without a texture uses the falloff texture of the engine, 128 by 128 pixels, which `graphics2d.lightTexture()` and `Renderer::getLightTexture()` return. Its alpha is `(1 - smoothstep(0, 1, d))^2`, where `d` is the distance from the center as a share of the radius, and `lighting2d::Light::falloff` computes the same on the CPU.

The functions `graphics2d.drawLight` and `Renderer::drawLight(const lighting2d::Light&)` draw lights, and `graphics2d.drawOccluder` and `Renderer::drawOccluder(const lighting2d::Occluder&)` draw occluders. Both work only in a lit canvas.

Shadows come from occluders, which become edges in world space when they are drawn:

- Before the passes, every light with shadows gets one row of a shadow atlas. The atlas is an `R32F` image 1024 texels wide that the frame writes once.
- The class `lighting2d::ShadowMap` casts the rows on the task pool, from the edges of the canvas of each light.
- A point or spot light stores in each texel the distance to the nearest edge in that direction, as a share of its radius. A directional light stores, at each point across the view of the canvas, the depth along the light of the nearest edge.
- Every texel that an edge touches takes it, so the outlines of occluders that overlap leave no gaps.
- The light shader reads one texel, or 5 or 13 texels spaced by the smoothness for the PCF filters. It counts a pixel as shadowed where it lies more than 1.5 world units behind the stored depth.
- The method `lighting2d::Light::isShadowedAt` answers the same question exactly on the CPU, for gameplay.

Every lit canvas owns its targets. They are created before the first pass of the frame, and created again when the pixel size of the canvas changes.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {ambientLight = '#FF202040'})
        graphics2d.drawRect({-400, -300, 800, 600}, '#FF707070')
        graphics2d.drawRect({40, -30, 60, 60}, '#FF403028', {layer = 1})
        graphics2d.drawOccluder({points = {40, -30, 100, -30, 100, 30, 40, 30}})
        graphics2d.drawLight({x = -150, y = 0, radius = 500, color = '#FFFFC080', shadows = true})
    end,
})
```

## Post-processing

A world or render target canvas with `postProcess` renders offscreen like a lit canvas, without the light pass when it has no ambient light. The composite then applies the adjustments of `graphics2d::PostProcess` (`engine/include/haylen/2d/graphics/PostProcess.hpp`) in this order:

1. The `pixelate`, which samples the image at the middle of square blocks.
2. The distortion map, which moves every pixel down the slope of the coverage of the [distortion draws](#distortion).
3. The `chromaticAberration`, which reads red and blue apart toward the corners.
4. The light map and the emission, when the canvas is lit, at each of those points.
5. The bloom, added from its own image.
6. The `saturation` around the luminance, then the `contrast` around middle gray scaled by the alpha of the pixel, then the `brightness`.
7. The `tint`, multiplied into every pixel.
8. The `colorLut`, a lookup texture of square cells that grades every color, mixed in by `colorLutStrength`.
9. The `colorVision`, which shows or corrects a deficiency of color vision.
10. The vignette, which darkens by `vignetteStrength` from `vignetteRadius` over `vignetteSoftness`.
11. The `fade`, which mixes toward its color by its alpha, so it covers the canvas with that color as if it were opaque.
12. The `grain`, noise of interleaved gradients that moves every frame, and the `dither`, the same noise of one step of 8 bits. The dither spreads the rounding of smooth gradients, so they show no bands.
13. The `materials`, in order. The composite writes into a post target. Every material but the last draws the image before it into the other post target, and the last draws into the destination of the canvas. Each one is one quad of the sprite program of the material.

Without bloom and blur, the composite is one pass. Bloom and blur split it into two stages:

1. The first stage writes the image of steps 1 to 4 into a post target. The filter program shrinks that image to half its size, keeping for bloom only what passes `bloomThreshold`.
2. It goes on shrinking it, each time to half the size, until `bloomRadius` or `blur` spans at most four texels. There it blurs the image, across and then down, with nine Gaussian taps a texel or less apart. So a wide radius leaves no gaps between its taps, which would show as rings around bright shapes.
3. It grows the blurred image back, size by size, to half the size of the canvas. Every step averages four taps around each texel, which keeps the blur smooth.
4. The second stage reads the blurred image instead of the sharp one when the canvas blurs, adds the bloom and finishes the steps from 5 on.

So bloom and blur each cost a full-size pass, three half-size passes and two smaller passes for every size the radius needs, and the other adjustments cost nothing beyond the composite. Distances are in units of the destination: design units for world canvases and pixels for render target canvases. The [canvas options reference](../lua-api/graphics2d.md#post-process-options) lists every option with its default.

Colors stay premultiplied by their alpha through the composite. The composite keeps the alpha of the scene, raises it where emission glows and toward 1 with the fade, and never lets a color pass its alpha. A canvas over an opaque clear color looks the same either way, and a [transparent window](../platform/desktop.md#transparency) composes correctly with the desktop. A window that is not transparent at the moment clears alpha to 1 in its screen pass, and draws with pipelines that never write alpha. So nothing an app draws lets the desktop through an opaque window. Screen canvases raise an error when they are given lighting or post-processing.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {postProcess = {bloomStrength = 0.6, bloomThreshold = 0.7, vignetteStrength = 0.4, dither = true}})
        graphics2d.drawRect({-300, -200, 600, 400}, '#FF203040')
        graphics2d.drawCircle(0, 0, 60, '#FFFFF0C0')
    end,
})
```

## Distortion

A draw whose order has a `distortion` above 0 bends the image of its canvas instead of drawing colors, like a lens, a shock wave or the heat above a fire. It goes into the distortion map of its canvas. The renderer multiplies the alpha of its instances or vertices by the distortion and draws it with additive blending into a target the size of the canvas. That target is cleared to nothing in a pass before the scene pass.

The map then shrinks to a quarter of its size in two steps, each averaging four texels, so its texels hold the mean coverage around them. The composite reads that smaller map four units to each side of every pixel. Because the map is averaged first, a texture that a distortion draw shrinks far without mipmaps still bends the image smoothly, instead of scattering its pixels. The slope of the coverage across those 8 units moves the pixel by up to `PostProcess::distortion` units. So the edges of a soft draw bend the image, while the flat middle of a draw leaves it in place. Only composited canvases have the map. Other canvases skip distortion draws when they are recorded, so effects that distort stay harmless in a canvas without post-processing. The [distortion reference](../lua-api/graphics2d.md#distortion) lists which draws take it.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local ring = assets.texture('effects/ring.png', {filter = 'linear'})

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {postProcess = {distortion = 32}})
        graphics2d.drawRect({-300, -200, 600, 400}, '#FF3A7D44')
        graphics2d.draw(ring, 0, 0, {width = 300, height = 300, distortion = 1})
    end,
})
```

## Metaballs

Metaballs draw liquids and blobs from many points: nearby circles merge into one smooth shape. The function `graphics2d.drawMetaballs`, `Renderer::drawMetaballs(points, radius, style, order)` in C++, draws them in two steps:

1. Each call gets a field target at half the resolution of its canvas. Before the canvas renders, one instanced draw of the sprite program adds a soft circle for every point into the field, with additive blending. The kernel is `(1 - d^2)^2` over 1.85 times the radius, which reaches 0.5 exactly at the radius.
2. The draw item of the call then draws a quad over the area of the points, which reads the field with linear filtering. It shows the surface where the field reaches the threshold, and the outline in the band above it. Both are smoothed over the screen-space change of the field.

The field saturates at 1 in the offscreen format, which the threshold never needs to reach. The item sorts and blends like any draw, and lit canvases light the surface.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local drops = {-40, 0, 0, 10, 40, -5, 80, 20}

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawMetaballs(drops, 30, {color = '#E02080FF', outlineColor = '#FFA0E0FF', outlineWidth = 0.15})
    end,
})
```

## Custom shaders

Materials shade draws with the shaders that apps write, as the [shader guide](shaders.md) explains. A draw with a material in its `DrawOrder` keeps a copy of the values the material had. Neighbouring draws that share a material and its values still merge into one draw call.

The pipelines of a material live with its shader, one for each program, blend mode and kind of target. They are created the first time a draw needs them, and released when the shader reloads or goes away. Some GPU devices compile programs when they make them, such as Metal. There, every program of a shader starts compiling on the I/O pool as soon as its file is read. So the first draws wait only for what is left of the compile, as [shader load time](shaders.md#load-time) measures.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local ripple = graphics2d.newMaterial(assets.shader('shaders/ripple.shader'), {strength = 0.01})
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        -- Both rectangles use the same material with the same values, so they make one draw call.
        graphics2d.drawRect({-200, -100, 180, 200}, '#FF3A7D44', {material = ripple})
        graphics2d.drawRect({20, -100, 180, 200}, '#FF3A7D44', {material = ripple})
    end,
})
```

## Node trees

A node tree, `nodes2d::NodeTree` (`engine/include/haylen/2d/nodes/NodeTree.hpp`), keeps composed objects as a tree that lives between frames, as the [nodes guide](../2d/nodes.md) describes. Its draw walks the nodes in tree order and leaves out the nodes whose world bounds miss the area the canvas shows. It gathers neighbouring sprite nodes of one texture and draw order into one `drawBatch`. So a tree of many sprites of an atlas costs a few draw items however deep it is. Text, frames and particles draw through the usual functions of the renderer. Frames draw as one batch of their pieces, each turned with its node.

```lua
local graphics2d = require('haylen.graphics2d')
local nodes2d = require('haylen.nodes2d')
local scene = require('haylen.scene')

local tree = nodes2d.newTree()
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        tree:draw()
    end,
})
```

## Tile layers and Tiled maps

A tile layer, `graphics2d::TileLayer`, `graphics2d.newTileLayer` in Lua, draws a grid of cells that code builds, such as a generated cave or a streamed world. It keeps its cells in chunks of a fixed size, in a hash map keyed by the coordinates of the chunk. So a world can grow in every direction and costs nothing where it has no cells.

A chunk bakes into one `StaticSpriteBatch` for each texture its cells use, on the first draw after one of its cells changed. A draw sorts the chunks row by row from the top, only when chunks come or go. It culls them with `Renderer::getCanvasBounds` and records one static draw for each baked batch in view. Setting cells allocates only when a chunk is created, and a frame that draws what the frame before drew allocates nothing.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local cave = graphics2d.newTileLayer(assets.texture('tiles/cave.png'), {cellWidth = 16, cellHeight = 16})
cave:defineFrame(1, {source = {0, 0, 16, 16}})
for column = -50, 50 do
    cave:setCell(column, 4, 1)
end
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        cave:draw()
    end,
})
```

Maps of the Tiled editor draw through `tiled::MapRenderer::draw(renderer, view, order)`, which Lua calls as `map:draw(camera, order)`, as the [Tiled guide](../2d/tiled.md#drawing) explains. These are the parts that matter for the renderer:

- A tile layer bakes into static batches the first time it draws, one for each region of 32 by 32 cells and each run of tiles that share a texture. The tint and opacity of the layer are baked into the colors of the instances.
- With a camera, a region draws only when its bounds, moved by the parallax of the layer, meet the area the open canvas shows. That is the visible design area for world canvases and the size of the target for render target canvases. Tile objects are culled one by one, image layers that repeat draw only the copies that cover the visible area, and other image layers draw whole. Without a camera nothing is culled.
- Parallax moves a layer by the distance between the camera position and the parallax origin of the map, times one minus the parallax factor, through the offset of `drawStatic`. So parallax never bakes again.
- The methods that build maps at run time, such as `map:fill`, drop the baked batches of their layer.
- Every layer uses the given draw order with its blend replaced by the blend mode of the layer. Later layers cover earlier ones because they are recorded later.
- The option `MapRenderer::DrawOptions::ysort`, `{ysort = true}` in Lua, bakes tile layers into rows of cells that stand on the same y. That is a row of an orthogonal map or a diagonal of an isometric one, split every 32 columns for culling. Object layers then draw object by object. Each row and object gets the y it stands on as its depth and as its sort point. The `ysort` bake is kept apart from the regular one, so a map can draw one layer both ways.

## Debug drawings

The method `Renderer::addCanvasOverlay(overlay)` runs a function in every canvas just before it closes, with the clips and layer offsets of the canvas cleared. So the overlay draws over what the canvas holds, in its own coordinates, and `Renderer::getCanvasKind()` tells the kind of canvas it runs in. The method `Renderer::visitDrawn(visitor)` hands it every textured quad and block of text the canvas holds, which Lua reads with `graphics2d.drawn()`.

The [debug drawings](../lua-api/debug.md#drawings) of the engine draw through these methods. They draw the shapes of physics worlds in world and render target canvases. The method `Renderer::drawBounds(font, color, labelSize)` outlines every textured quad and text block that the open canvas holds, and names each sprite after the path of the texture it was loaded from.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Score 120', 40, 40, {size = 48})
        for _, item in ipairs(graphics2d.drawn()) do
            graphics2d.drawRectOutline({item.x, item.y, item.width, item.height}, 1, '#FFFF4040')
        end
    end,
})
```

## Reading pixels back

The function `graphics.readTarget`, `graphics::Device::readTarget` in C++, copies the pixels of a render target to the CPU without making the frame thread wait for the GPU. The copy starts in `Device::processReads`, which the engine calls after it submits each frame, so it reads the target as that frame rendered it. The GPU device copies the image with `readImage`.

Each device copies the image through its graphics API:

- Metal blits the texture into a shared buffer, on a command buffer of its own, and marks the copy done in the completion handler.
- Direct3D 12 and Vulkan copy into a buffer that the CPU reads, behind a fence that the next frames test without waiting.
- Direct3D 11 copies into a staging texture and maps it with `D3D11_MAP_FLAG_DO_NOT_WAIT`.
- OpenGL, OpenGL ES 3 and WebGL2 read into a pixel buffer behind a fence, which the next frames test without waiting.
- WebGPU copies into a buffer whose mapping calls back.

Each later frame asks the copies whether they finished. A finished copy goes to the task pool, which turns its premultiplied colors into straight ones and puts its top row first. The function `graphics.captureScreen`, `core::Engine::captureScreen` in C++, renders the next frame into an image with the pixel size of the viewport. It draws that image over the screen and reads it back the same way. So a capture holds everything the screen shows.

```lua
local async = require('async')
local graphics = require('haylen.graphics')

async.spawn(function()
    local shot = graphics.captureScreen():await()
    print(shot.width, shot.height)
end)
```

## GPU object pools

The GPU device keeps the objects of each kind in a pool, and `debug.stats().pools` shows how many objects each pool holds and the room it has. The pools of the real devices grow as the app makes objects, while the null device of the headless host bounds them with the sizes of `engine/src/gpu/NullDevice.hpp`, so the tests catch objects that pile up:

| Pool | Size | Taken by |
| --- | --- | --- |
| `images` | 4096 | Every texture, render target and font atlas. The white, light and metaball textures of the renderer and its shadow atlas. The pages of the vector atlas, the targets of each canvas with lighting or post-processing, and the field of each metaball draw. |
| `buffers` | 4096 | Every static batch, including each baked region run of a Tiled layer, plus the quad of the renderer and its three streaming buffers. |
| `samplers` | 256 | One for each mix of filter, wrap and mipmaps that a texture uses, which every texture with the same mix shares. |
| `programs` | 512 | The twenty-two programs of the renderer, each made the first time a draw needs it, plus every program of a custom shader that a draw has used, up to ten per shader: its sprite, batch, text, mesh and shape programs and their lit versions. |
| `pipelines` | 2048 | One for each program, blend mode and kind of target the renderer has drawn with, for its own programs and for the programs of every custom shader. |

Creating an object in a full pool throws. In Lua this raises an error, which shows the error screen unless the app catches it:

| Error | Cause |
| --- | --- |
| `The graphics device has no room for another texture. At most 4096 textures and render targets can exist at once.` | The image pool is full. |
| `The graphics device has no room for another buffer. At most 4096 baked sprite batches and draw buffers can exist at once.` | The buffer pool is full. |
| `The graphics device has no room for another sampler. At most 256 samplers can exist at once.` | The sampler pool is full. |
| `The graphics device has no room for another shader. At most 512 shader programs can exist at once.` | The shader pool is full. |
| `The graphics device has no room for another pipeline. At most 2048 pipelines can exist at once.` | The pipeline pool is full. |
| `The graphics device could not create a <width>x<height> texture.` | The graphics API rejected the texture. |
| `The graphics device could not create a buffer of <size> bytes.` | The graphics API rejected the buffer. |
| `The graphics device could not create the shader "<label>".` | The graphics API rejected a shader program. The label of a custom shader is its name and program, such as `ripple/sprite_lit`. |
| `The graphics device could not create the pipeline "<label>".` | The graphics API rejected a pipeline. |
| `Texture dimensions must be positive.` and `Texture dimensions exceed the device limit.` | A texture or render target size outside the limits of the device, which `graphics.maxTextureSize()` and `Device::getMaxTextureSize()` report. |

A slot is freed when the last handle to its object goes away. The object is destroyed at the end of that frame, once no queued draw can use it. Lua handles go away when the garbage collector finalizes them. Assets loaded through a preload group stay alive until `assets.unloadGroup` releases the group, as the [assets reference](../lua-api/assets.md) explains.

```lua
local debug = require('haylen.debug')

for name, pool in pairs(debug.stats().pools) do
    print(name, pool.used, pool.size)
end
```

## Performance guidance

- Draw what shares a texture next to each other in draw order. Pack sprites into atlases, which [`haylen.animation2d`](../lua-api/animation2d.md) and [`haylen.assets`](../lua-api/assets.md) load from sprite atlas JSON files, and give ground, units, effects and the HUD layers of their own.
- Depth sorting mixes textures by depth, so every change of texture between neighbours costs a draw call. Put everything that sorts together on one atlas.
- Rectangles and lines batch with each other on the white texture. Circles, rings, arcs and shapes batch with each other in the shape program, and polygons use the mesh program. Alternating these kinds of draw, or textured sprites, alternates draw calls. A circle costs one 48-byte record whatever its radius, so many small circles, such as particles, cost no more than sprites.
- Draw many moving sprites of one texture with a sprite batch rather than one `graphics2d.draw` per sprite, and change only the sprites that move with `batch:set`. On an Apple M5 Pro in Release, a `graphics2d.draw` call costs the frame thread about a quarter of a microsecond, mostly to read its option table. A sprite table that `drawBatch` reads costs about 85 nanoseconds, and a sprite of a float buffer or a sprite batch about 10 nanoseconds, as the [Lua benchmark](../core/lua.md#performance) measures. Option tables cost in proportion to the keys they hold, so pass only the keys that differ from the defaults.
- Bake what never moves and draw it with `drawStatic`, and move it with the offset instead of baking again.
- The renderer submits every recorded draw and culls nothing on its own, apart from tile layers, node trees and Tiled maps. In large worlds, skip what is off the screen with `camera:visibleBounds()` or `graphics2d.canvasBounds()`, and use [`haylen.spatial2d`](../lua-api/spatial2d.md) to find what is visible.
- A post-processed canvas costs an offscreen pass, a composite over its viewport and a render target, plus a pass and a target per material. A lit canvas adds a scene pass that writes four targets, a light pass with one quad per light and a floating-point light map. The light pass draws in one call every run of lights that share a blend mode and a texture, so draw the lights of one kind next to each other. One such canvas per frame covers most apps.
- Every light with shadows casts its shadow map on the CPU from the occluder edges within its reach, spread over the task pool. Keep occluders simple, with one edge per face of an object, and give shadows only to the lights that need them.
- Text makes new glyphs on the frame thread and uploads the atlas again. The renderer uploads a changed texture whole and at most once per frame. So a font atlas or a page of the vector atlas that gained rasters uploads entirely, once in that frame, and frames that add nothing upload nothing. A dynamic texture keeps a copy of its new pixels until the upload. It keeps that memory while it changes every frame, such as a video, so its updates allocate nothing, and gives it back after a frame without a change. Measure the strings a scene shows while it loads, with `graphics2d.measureText` or `Font::measure`, so the glyphs exist before the first frame. Shadows double the glyph quads, and each font breaks the batches of the others.
- Clip rectangles split batches, so scroll areas and clipped panels cost a draw call each.
- The method `map:setTile` bakes the region of its cell again. Grids that change cells every frame draw best as a `TileLayer`, whose chunks are smaller and bake only when they change. Or keep the cells that change in a small layer of their own.
- Record draws in their final order, for example layer by layer from the lowest, so the renderer uploads instances without copying them into draw order.
- Measure in Release builds. Debug builds run without optimizations and with the validation of the graphics API.
- Positions are 32-bit floats on their way to the GPU. They keep a step of a hundredth of a unit up to about 100,000 units from the origin. Near a million the step is a sixteenth of a unit, and sprites start to jitter. A larger world keeps a local origin near the player, as the chunks of a streamed world do. When the player crosses into another chunk, it moves the origin, the camera and every object by the same amount. Physics has the same limit, so its bodies move with the origin.

## Sprite benchmark

```sh
haylen bench
```

The command `bench` builds the `haylen-sprite-benchmark` app in Release for the host and runs it on the local GPU. The app is `engine/bench/SpriteBenchmark.cpp`, written in C++. Desktop builds add it with `haylen_add_app(... CPP ...)` when `HAYLEN_BUILD_BENCHMARKS` is on, with the package `engine/bench/sprite-benchmark`. Its `app.json` opens a 1280 by 720 high-DPI window with vsync off and a 1920 by 1080 design area with the `fit` policy. Its `content/noise.json` describes the images of the last phase.

The benchmark runs these phases:

- Six phases draw 100,000, 1,000,000 and 2,000,000 sprites, each first dynamic and then baked. Every phase draws sprites of 3 by 3 units from a tinted 4 by 4 white texture, in a grid dense enough to cover the design area. It runs 30 warm-up frames and 240 measured frames.
- Dynamic phases move every sprite every frame with `JobSystem::parallelFor` and draw them with one `drawBatch`. So all instance data is converted and uploaded every frame, 96 MB at 2,000,000 sprites of 48 bytes.
- Baked phases draw one static batch made with `createStaticBatch`, and shift it every frame through the offset of `drawStatic`.
- A seventh phase moves 100,000 sprites while it loads 48 textures of 1024 by 1024 pixels in the background. An asset type of the benchmark makes the pixels of each image on a worker. It makes the texture on the frame thread within the upload budget of the asset manager, as a picture of an app loads. The phase lasts until every texture is ready, so it shows what finishing GPU resources costs a frame.

The app quits after the last phase. Each phase logs one line:

| Column | Meaning |
| --- | --- |
| `sprites` | Sprites drawn per frame. |
| `kind` | `dynamic`, `baked`, or `dynamic` with the textures that load. |
| `ms` | Wall time between frames. It includes the GPU and, on displays that pace frames themselves like macOS does, the display. |
| `max ms` | The longest of those times, the worst hitch of the phase. |
| `fps` | Frames per second from `ms`. |
| `Msprites/s` | Millions of sprites drawn per second. |
| `work ms` | CPU time of the frame outside the `submit` step, where the renderer uploads and waits for the next drawable. |
| `frames` | Frames measured, 240, or until every texture of the last phase is ready. |

These results are recorded in `PROJECT.md`, measured on an Apple M5 Pro with Metal in Release:

| Phase | Frame rate | CPU per frame (`work ms`) |
| --- | --- | --- |
| 2,000,000 animated sprites | 105 fps | 4.5 ms |
| 2,000,000 baked sprites | 120 Hz, the refresh limit of the display | 0.02 ms |
| 100,000 animated sprites while 48 textures of 1024 pixels load | 115 fps, 12.5 ms at the slowest frame, every texture ready after 9 frames | 0.55 ms, against 0.36 ms without the loads |

## Render benchmark

```sh
haylen bench --suite render
```

The suite `render` builds `haylen-render-benchmark` in Release, from `engine/bench/RenderBenchmark.cpp`. It runs whole engine frames on the headless host, whose null GPU device takes the draw calls and uploads without a GPU. So it measures what the frame thread spends to record, sort, batch and submit. Each scene runs 30 warm-up frames and 300 measured frames of a 1920 by 1080 design area:

- Ten thousand sprites in runs of four textures, and then alternating them.
- A thousand circles, rounded panels and lines.
- 200 labels that stay the same, and 200 labels whose numbers change every frame.
- 20 rich text blocks with a wave effect.
- A screen of 40 interface components.

For each scene it prints the CPU time of a frame, the draw calls, instances and uploaded kilobytes of the last frame, and the allocations of the frame thread per frame. It counts them by replacing the allocation functions of the program. These are the numbers of an Apple M5 Pro in Release:

| Scene | Frame ms | Draw calls | Instances | Uploaded KB | Allocations | Allocated KB |
| --- | --- | --- | --- | --- | --- | --- |
| 10,000 sprites in runs of 4 textures | 0.275 | 4 | 10,000 | 468.8 | 0 | 0 |
| 10,000 sprites alternating 4 textures | 0.438 | 10,000 | 10,000 | 468.8 | 0 | 0 |
| 10,000 sprites alternating 4 frames of a runtime atlas | 0.289 | 1 | 10,000 | 468.8 | 0 | 0 |
| 3,000 circles, panels and lines | 0.133 | 2,000 | 3,000 | 140.6 | 0 | 0 |
| 200 labels that stay | 0.074 | 1 | 4,892 | 229.3 | 0 | 0 |
| 200 labels that change every frame | 0.741 | 1 | 4,090 | 191.7 | 7,000 | 3,204 |
| 20 rich text blocks with a wave | 0.032 | 1 | 1,440 | 67.5 | 0 | 0 |
| A screen of 40 interface components | 0.026 | 48 | 514 | 24.1 | 0 | 0 |

Labels that stay find their layouts in the cache of their font, so they cost what their glyph quads cost. Text that changes every frame lays out again, which is the one drawing that allocates. So a readout of numbers is cheapest when it changes a few times per second, as the compact debug statistics do. Alternating textures turns every sprite into a draw call, while the same sprites cut from one atlas that `animation2d::AtlasBuilder` packs at run time draw in one. Alternating circles and panels with lines splits the batches of the shape program, as the [performance guidance](#performance-guidance) explains. The suite also packs 400 images of 8 to 72 pixels a side into one linear atlas, trimmed, padded, extruded and bled. That takes 5.8 ms on one worker and gives an atlas of 1024 by 685 pixels.

## Using the renderer from C++

C++ apps and plugins reach the same renderer through `engine.getRenderer2D()`. This scene bakes a row of ground tiles once, draws it in a lit world canvas with a light, and draws a label in a screen canvas:

```cpp
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteBatch.hpp"
#include "haylen/2d/graphics/StaticSpriteBatch.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/text/Font.hpp"

class NightScene final : public haylen::core::Scene {
  public:
    void enter(haylen::core::Engine& engine) override {
        haylen::graphics2d::SpriteBatch row(engine.getGraphics().getWhiteTexture());
        for (int column = 0; column < 64; ++column) {
            row.add({.position = {static_cast<float>(column) * 32.0F, 0.0F}, .size = {30.0F, 30.0F}, .pivot = {0.0F, 0.0F}, .color = haylen::math::Color::fromHex(0x3A7D44FFU)});
        }
        ground = row.bake(engine.getRenderer2D());
    }

    void render(haylen::core::Engine& engine) override {
        haylen::graphics2d::Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(camera, {.sort = haylen::graphics2d::Renderer::SortMode::Depth, .ambientLight = haylen::math::Color::fromHex(0x303050FFU)});
        renderer.drawStatic(ground);
        renderer.drawLight({.position = {400.0F, 16.0F}, .radius = 240.0F, .color = haylen::math::Color::fromHex(0xFFB060FFU)});
    }

    void renderUi(haylen::core::Engine& engine) override {
        engine.getRenderer2D().beginScreen();
        engine.getRenderer2D().drawText(*engine.getDefaultFont(), "Night 3", {48.0F, 48.0F}, {.size = 48.0F});
    }

  private:
    haylen::graphics2d::Camera camera;
    haylen::graphics2d::StaticSpriteBatch ground;
};
```

The call `engine.getScenes().push(std::make_shared<NightScene>())` shows it. The [embedding guide](../tools/embedding.md) explains how a C++ app is built.
