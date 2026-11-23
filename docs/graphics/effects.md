# 🌫️ Effects

This guide covers the visual effects that run in real time while an app draws: glass and backdrop blur, dithered fades, distortion, sprite effects and post-processing. It explains what each one is for, how glass reads what lies behind it, what the effects cost on a phone, and how to write an effect of your own as a `.shader` file and plug it into a draw, a canvas or a glass. The [`haylen.graphics2d` reference](../lua-api/graphics2d.md) lists every option, and the [shader guide](shaders.md) covers the shader language.

## Choosing an effect

Every effect below runs on the GPU in the frame that draws it, and the same Lua code runs on every graphics API.

| Effect | What it is for | How to use it |
| --- | --- | --- |
| Glass | A surface that shows what lies behind it blurred, tinted, lit along its rim and bent at its edge, such as a bar, a card, a sheet or a dialog. | [`graphics2d.drawGlass`](../lua-api/graphics2d.md#graphics2ddrawglassrect-options), or `glass = true` on a panel, a card or a dialog of [`haylen.ui`](../lua-api/ui.md#uipanelproperties). |
| Backdrop blur | A region that blurs whatever lies behind it, without the rest of the glass. | `graphics2d.drawGlass` with only a `blur`. |
| Dithered fades | Fades and masks that show as a pattern of whole pixels, as in pixel art. | The `dither` of [`beginGroup`](../lua-api/graphics2d.md#graphics2dbegingroupoptions) and [`pushMask`](../lua-api/graphics2d.md#graphics2dpushmaskdrawshape-options). |
| Distortion | Draws that bend the image of a canvas, such as heat haze and shock waves. | The `distortion` of a draw, as the [rendering guide](rendering.md#distortion) explains. |
| Sprite effects | Dissolve, outlines and glows on one sprite. | The [`effect`](../lua-api/graphics2d.md#sprite-effects) of a sprite. |
| Post-processing | Bloom, blur, color grading, vignettes and the rest over a whole canvas. | The [`postProcess`](../lua-api/graphics2d.md#post-process-options) of a canvas. |
| Materials | Any effect of your own on a draw, a canvas or a glass. | A `.shader` file and [`graphics2d.newMaterial`](../lua-api/graphics2d.md#material), as [your own effects](#your-own-effects) shows. |
| Image filters | Effects on an image before it becomes a texture, off the frame. | [`image:applyAsync`](../lua-api/graphics.md#imageapplyasynceffects), for art that changes rarely. |

## Glass

Glass reads the pixels that lie behind its shape at the moment it draws. Behind means what its canvas drew before it, by the draw order, and what every canvas before that canvas drew into the same destination this frame. So a glass bar in the interface blurs the game world below it, and a glass card drawn after another glass card shows the first one blurred.

A glass goes through these steps when the frame is submitted:

1. It ends the pass of its destination, so the pixels behind it can be read. The glass draws after it share that end of the pass while nothing drawn since then reaches the pixels they read and they read no pixel that a glass before them reads, such as a bar at the top of a screen and a bar at the bottom, so one end of the pass serves them all.
2. It shrinks the region it needs to half its size, then to smaller sizes, each half the one before, until the blur spans at most four texels.
3. At the smallest size, it blurs the region across and then down with nine taps of a Gaussian curve.
4. It grows the blurred region back size by size into an image with the size of its destination.
5. It goes on drawing in a new pass that keeps what the destination holds. Its shape reads that image at its own pixels, moved outward near the edge by the refraction, then grades, tints and lights it.

The region is the area of the shape, the refraction around it and a margin that the blur needs. Every step writes only that region and lets the GPU discard the rest of its image, which a tile GPU then never loads, so a glass costs the same few passes whatever its radius, and a wider blur adds one more small pass each time it doubles. A blur never loops over its radius in a pixel.

The screen is the one destination that cannot be read back. A frame whose screen canvases hold glass draws them into an image with the size of the window instead, and that image reaches the swapchain in one copy at the end of the frame. A frame without glass on the screen draws as before and pays nothing. Render target canvases, captures and canvases with post-processing already draw into images, so their glass reads them directly. A glass inside a group reads only what the group drew before it, since the group draws into an image of its own.

```lua
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawRect({-960, -540, 1920, 1080}, '#FF1B2A41')
        for index = 0, 7 do
            local x = -700 + index * 200
            graphics2d.drawCircle(x, math.sin(haylen.elapsed() * 2 + index) * 220, 90, '#FFE9C46A')
        end
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        -- Frosted glass: blur, richer colors, a white haze, a rim of light and a bend at the edge.
        graphics2d.drawGlass({160, 80, 1600, 160}, {radius = 80, blur = 24, saturation = 1.6, tint = '#33FFFFFF', rim = 0.6, refraction = 12})
        -- A plain backdrop blur.
        graphics2d.drawGlass({160, 760, 700, 240}, {radius = 32, blur = 40})
    end,
})
```

### Glass in the interface

A `panel`, a `card` and a `dialog` of `haylen.ui` take the property `glass`. With `glass = true`, the surface draws as glass with the look of the theme, in place of its flat color or its theme image. The theme color `glass` is the tint, and the theme metrics `glassBlur`, `glassSaturation`, `glassRim` and `glassRefraction` set the rest, as the [theme reference](../lua-api/ui.md#theme-metrics) lists. The border of the theme draws along the edge, and the transforms of the nodes around the surface scale the glass with it.

The interface follows the accessibility settings of the system. While the person asks for reduced transparency, the theme makes its glass color opaque, and every glass surface draws as a plain surface of that color, which reads nothing behind it and costs nothing more than a flat surface. The high contrast themes keep their glass color almost opaque for the same reason.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    align = 'center',
    gap = 24,
    ui.panel{glass = true, width = 900, ui.label{text = 'Now playing'}},
    ui.card{glass = true, width = 900, ui.label{text = 'Up next'}},
    ui.dialog{glass = true, title = 'Paused', message = 'The game waits behind the glass.', buttons = {{id = 'resume', text = 'Resume'}}},
}, {placement = 'screen'})
```

## Dithered fades

A group with a `dither` shows its opacity as a pattern of square cells instead of blending it, and a mask with a `dither` shows the coverage of its shape the same way. Each cell shows whole or not at all. The cells follow an ordered pattern of four by four, so a half opacity turns on every other cell, and a smooth fall-off turns into a pattern that grows denser toward the solid part. The cells are measured in units of the canvas and stay put on what the canvas shows, so the pattern moves with the world rather than crawling over it as the camera moves. A cell of one art pixel keeps pixel art crisp.

A soft shape makes a soft dither. The example dissolves a tree into a pattern around a character behind it, with a circle whose `softness` fades over most of its radius:

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local tree = assets.texture('forest/tree.png')
local hero = {x = 0, y = 40}

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.pushMask(function()
            graphics2d.drawShape({hero.x - 40, hero.y - 60, 80, 80}, {radius = 40, softness = 60})
        end, {mode = 'outside', dither = 1})
        graphics2d.draw(tree, 0, 0, {pivotY = 1})
        graphics2d.popMask()
    end,
})
```

## Your own effects

An effect of your own is a fragment shader in the [Haylen shader language](shaders.md#the-haylen-shader-language). You write it under `content/shaders/`, `haylen app shaders` compiles it into a `.shader` file for every graphics API, and a material made of it plugs into one of three places:

| Place | What the shader reads | How to plug it in |
| --- | --- | --- |
| A draw | The texture and color of the draw, through `haylen_base`. | The `material` of the draw order of any sprite, shape, mesh or text. |
| A canvas | The image of the whole canvas, through `haylen_texture(uv)`. | The `materials` of the `postProcess` of a world or render target canvas. |
| A glass | What lies behind the glass, through `haylen_backdrop(shift)`. | The `material` of `graphics2d.drawGlass`. |

### An effect on a glass

The shader of a glass runs as the `shape` program of its material, so the edge of the shape stays smooth whatever it writes. The function `haylen_backdrop(shift)` returns the color that lies behind the pixel, moved by a shift in units of the canvas, and the name `haylen_output_point` holds the point of the canvas where the pixel lies. The glass hands its material the sharp image behind it when its `blur` is 0, and the blurred one otherwise. A material reads at most `refraction` units past the edge of the shape, so an effect that reads farther sets `refraction` to how far it reads.

This effect turns what lies behind a glass into a mosaic of square cells, like the privacy glass of a shower door, with thin lines between the cells:

```glsl
// The file `content/shaders/mosaic.glsl`.
@include haylen/material.glsl

@fs mosaic_fs
@include_block haylen_fragment
layout(binding=1) uniform mosaic_params {
    float cell;
    float lines;
};

void main() {
    vec2 point = haylen_output_point;
    vec2 middle = (floor(point / cell) + 0.5) * cell;
    vec4 behind = haylen_backdrop(middle - point);
    vec2 inside = abs(fract(point / cell) - 0.5) * 2.0;
    float line = smoothstep(1.0 - lines, 1.0, max(inside.x, inside.y));
    vec3 shaded = mix(behind.rgb / max(behind.a, 0.0001), vec3(1.0), line * 0.35);
    haylen_output(vec4(shaded, 1.0) * color);
}
@end

@program mosaic haylen_vs mosaic_fs
```

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local mosaic = graphics2d.newMaterial(assets.shader('shaders/mosaic.shader'), {cell = 24, lines = 0.08})

scene.push({
    renderUi = function(self)
        graphics2d.beginScreen()
        -- The mosaic reads up to half a cell past each pixel, which the refraction keeps ready past the edge.
        graphics2d.drawGlass({600, 300, 720, 480}, {radius = 32, blur = 0, refraction = 12, material = mosaic})
    end,
})
```

The same shader shades only glass, since `haylen_backdrop` exists only in the `shape` program. A material that should also shade other draws checks `#ifdef HAYLEN_SHAPE` and reads `haylen_base` elsewhere.

## Cost and budget

A glass costs passes over its own region, never over the whole screen, and one copy of the screen when the screen holds glass. The table shows the time the GPU spent on a frame of the test `EFX-008` of the test project, with five surfaces of glass over a moving landscape, a bar, a card and a lens in the world and a panel and a card of the interface, against the same frame without them. It was measured with `debug.frame().gpuMilliseconds` on an Apple M5 Pro while other builds ran on the machine. A frame at 60 frames per second has 16.7 milliseconds.

| Device | Pixels | Without glass | With glass | Five surfaces |
| --- | --- | --- | --- | --- |
| The player on macOS, Metal | 2560 by 1504 | 3.37 ms | 4.65 ms | 1.28 ms |
| Chrome on macOS, WebGPU | 2000 by 1125 | 1.85 ms | 2.49 ms | 0.64 ms |

The [render benchmark](rendering.md#render-benchmark) measures the same in Release on that machine, with 400 circles under a bar, a card and a sheet of glass of 1920 by 1080 pixels. The three surfaces take the GPU time of a frame from 0.11 ms to 0.49 ms on Metal, add 28 draw calls, and add 2 microseconds of the frame thread, which allocates nothing for them. The Metal driver allocates 28 small blocks more for the passes of the glass in each frame, which the engine cannot avoid.

So one surface costs about a quarter of a millisecond on that GPU at that size. The GPU of a phone is several times slower, and its screen holds about as many pixels, so a phone pays about a millisecond for each surface of glass, which keeps one or two of them, such as a tab bar and a sheet, well inside a frame. The work of the frame thread grows by a few hundredths of a millisecond for each glass. WebGL2 measures no GPU time in Chrome, and its frame thread showed the same small growth.

To keep glass cheap on a phone:

- Use few glass surfaces at once. Each one ends the pass of its destination, which a phone pays for as a store and a load of the screen in its tile memory.
- Keep the blur where it looks right rather than small, since a wider blur shrinks further and adds only a small pass.
- Draw glass after the content it covers and before the content on top of it, so one glass serves the whole bar or card.
- Let reduced transparency turn glass off, as the interface does on its own.
