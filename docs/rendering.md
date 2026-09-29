# Rendering

Haylen draws everything through `graphics2d::Renderer` (`engine/include/haylen/2d/graphics/Renderer.hpp`), which sits on `graphics::Device` and Sokol. Apps record draw commands into canvases while the engine renders a frame, and the renderer sorts them, merges them into as few draw calls as possible, uploads the frame's data once and submits the whole frame at the end. This guide explains that model, what each kind of draw costs, the limits of the GPU object pools and how to keep millions of sprites fast. The Lua functions are documented in the [haylen.graphics2d reference](lua-api/graphics2d.md) and the [haylen.graphics reference](lua-api/graphics.md), and the UI built on top of the renderer is covered in the [UI guide](ui.md).

## A frame

1. After the app updates, `Engine::frame` calls `Renderer::beginFrame` with the viewport and the `clearColor` of `app.json`.
2. Scenes and plugins record draws in `render`, then in `renderUi`. During a scene transition this happens once for the scenes before the change and once for the scenes after it, each inside a capture of its own image, and then the transition effect draws the two images, as [Scene transitions](#scene-transitions) explains. `beginWorld`, `beginScreen` and `beginTarget` open a canvas and close the previous one. Every draw converts its data to GPU instances or vertices right away and stores a draw item with its sort key.
3. `Renderer::endFrame`, which the profiler reports as `submit`, creates the render targets that lighting, post-processing and metaballs need, casts the shadow maps of the frame on the task pool, sorts each canvas and merges its items into commands, uploads the instance, vertex and index data of the whole frame, renders the offscreen passes and captures in the order the frame recorded them, renders the screen pass and commits.
4. `Device::collectGarbage` destroys the GPU objects whose last handle was released during the frame.

Draws only exist inside that window, so they belong in `render` and `renderUi`. Drawing without an open canvas raises `No canvas is active. Call beginWorld, beginScreen or beginTarget before drawing.`

## Graphics device and backends

`graphics::Device` (`engine/src/graphics/Device.cpp`) owns the Sokol graphics context and creates textures, single-channel alpha textures for font atlases, and render targets. Every method runs on the frame thread, and Sokol keeps one global device, so a process holds one engine at a time and a second device raises `Only one graphics device can exist at a time.` Builds without `NDEBUG` turn Sokol's validation layer on, which checks every GPU call.

| Platform | Backend | `graphics.backendName()` |
| --- | --- | --- |
| macOS, iOS, tvOS | Metal | `'metal'` |
| Windows | Direct3D 11 | `'d3d11'` |
| Linux | OpenGL core | `'glcore'` |
| Android | OpenGL ES 3 | `'gles3'` |
| Web, `web` platform | WebGPU | `'webgpu'` |
| Web, `web-webgl2` platform | WebGL2 | `'gles3'` |
| Engine tests | Sokol dummy backend | `'dummy'` |

The [build guide](build.md#build-options) explains how a build picks its backend. The engine patches the dummy backend to report the texture limits of desktop GPUs, 16384 pixels for 2D textures, so headless runs load full-size art like the real backends. The shaders in `engine/shaders` are compiled by `sokol-shdc` for every backend: `sprite.glsl` holds the sprite program, `text.glsl` the text program, `mesh.glsl` the mesh program, `blend.glsl` the image blend patterns of the scene transitions, `metaball.glsl` the surface of metaballs, `light.glsl` the light pass and `composite.glsl` the lighting and post-processing composite. The programs that draw into lit canvases compile a second time with `HAYLEN_LIT`, and every program shares the shader library in `engine/shaders/include/haylen`, which the custom shaders of apps include too, as the [shader guide](shaders.md) explains. Backends whose render targets start at the bottom row are corrected automatically, so render target textures sample upright everywhere.

## Canvases

| Canvas | Started with | Coordinates | Lighting and post-processing |
| --- | --- | --- | --- |
| World | `beginWorld(camera, options)` | World units through the camera. The view covers the camera viewport, or the visible design area without one, divided by the zoom. | Yes. |
| Screen | `beginScreen(options)` | Design units of `viewport.visibleRect()`. | No. |
| Render target | `beginTarget(target, camera, options)` | World units through the camera, with the view covering the camera viewport in target pixels or the whole target. | Yes, composited into the target. |

The engine submits canvases in two steps. Offscreen work runs first, in the order the frame recorded it: the metaball fields of every canvas, every render target canvas, cleared to its `clear` color or transparent, the scene pass of every canvas with lighting or post-processing, followed by its light pass when the canvas is lit, the post-processing materials but the last, the composite of a render target canvas into its target, and every [capture](#captures), once the canvases before its end have their passes. Then the screen pass clears the framebuffer with the `clearColor` of `app.json` and draws every world and screen canvas that no capture took, by their `order` and then in the order they began, inside the viewport rectangle that the [scaling policy](lua-api/viewport.md) computes, so letterbox bars show the clear color. A world canvas draws only inside the part of that rectangle that its camera viewport covers. A screen canvas that draws a render target's texture therefore shows what the target recorded in the same frame, whatever the order of the two canvases.

A world canvas with lighting or post-processing reaches the screen as an image that covers its whole viewport and blends with the premultiplied alpha of its scene, which its clear color makes opaque by default, so it hides everything earlier canvases drew there unless its clear color is transparent. Begin it before the canvases that must appear on top of it, or give those canvases a higher `order`. Every such canvas keeps its own render targets with the pixel size of its viewport, so split screens and minimaps light each view on its own: the scene, and when lit the emission, surface and info images and the light map, and two more targets when it has post-processing materials.

## Captures

`Renderer::beginCapture(target, clear)` and `endCapture()`, `graphics2d.beginCapture` and `graphics2d.endCapture` in Lua, send the world and screen canvases begun in between into a render target instead of the screen, with lighting, post-processing, clips and viewports working as on the screen. The visible area covers the whole target, so a target with the pixel size of the viewport, `viewport.pixelRect()`, holds the frame exactly as the screen would show it. The capture pass clears the target and draws its canvases by order, and runs once the canvases recorded before its end have their offscreen passes and before any canvas recorded after it, so a later canvas that draws the captured texture shows this frame. Render target canvases begun during a capture still draw into their own targets, a capture can begin inside another one, such as in a scene that a transition renders into its image, and it renders before the outer capture, which takes the canvases after its end, and a capture left open ends with the frame.

## Scene transitions

A transition with an effect renders both sides of a change of the scene stack. `SceneManager::getViews` returns the views of the frame: without a transition, the visible scenes straight onto the screen, and during one, the scenes before the change in the outgoing image, the scenes after it in the incoming image, two render targets with the pixel size of the viewport, and the screen where the effect draws both images. `Engine` renders every view inside a capture of its image, with the drawing of the plugins and the loading view in the view of the current scenes, and the view of the effect draws the effect before its own scenes.

An effect that covers the screen renders the current scenes into the outgoing image until full cover, where the stack changes. While the next scene loads, the hold draws the effect at its switch point with the last outgoing frame and no scene at all, only the loading view and the plugins on top, so waiting costs almost nothing, and the reveal renders the scenes after the change into the incoming image. An effect that shows both scenes starts once the next scene loaded, and the scenes that leave the stack keep rendering into the outgoing image until its exit point, where they exit before that frame renders, so nothing draws them after they exit and the image keeps their last frame. The built-in effects of `graphics2d::SceneTransition` draw the images with sprites, perspective meshes and clipped polygons, and `dissolve`, `pixelate`, the radial wipes, `iris` and `pageTurn` use the patterns of `Renderer::drawImageBlend`, one full-rectangle draw of the `blend.glsl` program that reads both images. The images hold colors premultiplied by their alpha, since the scenes render over the clear color, which a [transparent window](desktop.md#transparency) keeps transparent, so the built-in effects draw them with the `premultiplied` blend mode, which looks the same as `alpha` over an opaque clear color, and custom effects that should compose with the desktop do the same. The [scene reference](lua-api/scene.md#changes) lists the effects and the Lua API, and custom effects in Lua or C++ receive both images too. In C++, a transition takes a `graphics2d::SceneTransition` or any `core::TransitionEffect`:

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

Only offscreen canvases take a `clear` color: screen canvases and world canvases without lighting or post-processing draw straight onto the screen, and they reject a `clear` option with an error.

`pushClip(rect)` and `popClip()` restrict the following draws of the active canvas to a rectangle in the canvas's coordinates. Nested clips intersect, and a new canvas starts without clips.

## Draw order

Every draw carries a `graphics2d::DrawOrder` of `layer`, `depth`, `sortOffset`, `visibility` and `blend`. Inside a canvas, draws are ordered by layer, lowest first, and `pushLayerOffset` shifts the layer of the draws of a scope. A canvas started with `sort = 'depth'` (`Renderer::SortMode::Depth`) also orders by depth inside each layer, lowest first. A canvas started with `sort = 'y'` (`Renderer::SortMode::Y`) orders by the y each draw stands on plus its `sortOffset`: sprites stand on their pivot, every sprite of a batch sorts on its own, text stands on its position, rich text on the bottom of its block, and shapes, meshes, nine-slices, static batches, image blends and metaballs stand on their lowest point. The default `sort = 'layer'` ignores both. Draws with the same key keep the order in which the app recorded them, so a canvas that never sets a layer draws in submission order. A draw whose `visibility` bits share none with the `visibilityMask` of its canvas is dropped when it is recorded, which costs nothing later.

Depth sorting is how top-down games draw units that stand in front of each other, with the depth set to the y position of each unit's feet, and y sorting does the same without setting any depth:

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

The blend modes are `alpha` (the default), `additive`, `multiply`, `screen`, `premultiplied` and `opaque`, and the [reference](lua-api/graphics2d.md#draw-order) describes each one. Draws write straight colors, which the shaders multiply by their alpha for `multiply` and `screen`, because those blends need premultiplied colors, so a partly transparent multiply or screen draw weakens its effect instead of brightening what is below. Offscreen images and the clear colors of their passes hold premultiplied colors.

## Batching

After sorting, the renderer walks the draws of each canvas and merges neighbours into one GPU draw call when they share the program, the blend mode, the clip rectangle, the texture and the shading: the material with its values and, in lit canvases, the lighting options of the draw and its layer. Instance data is laid out in draw order before the upload, so any run of compatible neighbours becomes a single instanced draw, whatever order the app recorded them in. A different texture, blend mode, program, clip or shading between two draws starts a new draw call, and a static batch is always a draw call of its own.

| Draws | Program | Texture |
| --- | --- | --- |
| `graphics2d.draw`, `sprite:draw()`, `batch:draw()`, `graphics2d.drawNineSlice`, particle emitters | sprite | Their texture. |
| `graphics2d.drawRect`, `drawRectOutline`, `drawLine`, `drawPolyline` | sprite | The white texture. |
| `graphics2d.drawCircle`, `drawRing`, `drawArc`, `drawPolygon` | mesh | The white texture. |
| `graphics2d.drawMesh` | mesh | Its texture, or the white texture. |
| `graphics2d.drawText` | text for TrueType fonts, sprite for bitmap fonts | The font's atlas or its pages. |
| `graphics2d.drawRichText`, `richText:draw()`, the text of `haylen.ui` components | text for TrueType glyphs, sprite for bitmap glyphs, images and boxes | The atlas or page of each font, each image and the white texture. |
| `graphics2d.drawStatic` | sprite, from the batch's own buffer | The batch texture. |
| `graphics2d.drawLight` | light, one draw call each into the light map | The light texture, the surface and info targets and the shadow atlas. |
| `graphics2d.drawMetaballs` | sprite into its field, then metaball, one draw call each | The field. |
| `graphics2d.drawImageBlend` and the shader-based scene transitions | blend, one draw call each | Both images. |
| `haylen.ui` and `haylen.imgui` | mesh, one clip rectangle per ImGui command | The UI textures. |

Sprites, glyphs, nine-slice pieces and metaball splats are 48-byte instances of one shared quad: position, size, texture coordinates, color, flash color, rotation, four parameter bytes and pivot. The sprite program reads the flip flags from the first parameter byte, the text program reads the outline, the weight and the softness of a glyph from the first, second and fourth, and both lean the quad around its pivot by the skew in the third. Shapes and meshes are indexed triangles of 20-byte vertices. The renderer keeps one streaming buffer each for instances, vertices and indices, writes each of them once per frame and grows them to one and a half times the need when a frame outgrows them. When the draws were recorded in their final order, the instances upload as recorded, and otherwise they are first copied into draw order.

`graphics2d.stats()` in Lua and `Renderer::getStats()` in C++ report the counters of a frame: canvases, passes, draw calls, sprites, uploaded instances, vertices, indices, lights, occluders, shadow maps, texture switches and uploaded bytes. The compact statistics and the debug overlay, cycled with F3 and described in the [debug reference](lua-api/debug.md), show them live, and the web runtime passes them to the page through `onStats`.

## Sprites, sprite batches and static batches

The renderer offers three ways to draw sprites, from the most flexible to the fastest:

| Way | Lua | C++ | Per-frame cost |
| --- | --- | --- | --- |
| Immediate sprite | `graphics2d.draw`, `sprite:draw()` | `Renderer::draw(const Sprite&)` | One call from Lua into the engine, one instance converted and uploaded. |
| Sprite batch | `graphics2d.newSpriteBatch`, `batch:draw()` | `SpriteBatch::draw`, `Renderer::drawBatch` | Every instance converted and uploaded, with no Lua call per sprite. |
| Static batch | `batch:bake()`, `graphics2d.drawStatic` | `SpriteBatch::bake`, `Renderer::createStaticBatch`, `Renderer::drawStatic` | One draw call, nothing converted or uploaded. |

A `SpriteBatch` keeps its sprites in C++ and shares one texture, and `batch:set(index, fields)` changes only the fields given, so a script touches only the sprites that move. `drawBatch` converts batches of 8,192 sprites or more on the worker pool with `JobSystem::parallelFor` while the frame thread takes its share, and the web build, which is single-threaded, converts them inline.

`batch:bake()` copies the sprites into an immutable GPU buffer. The baked copy never changes, so rebake to change it. `graphics2d.drawStatic(batch, x, y, order)` and `Renderer::drawStatic(batch, order, offset)` shift the whole batch by an offset in world units without touching its data, which lets parallax and scrolling layers reuse one batch. Each static batch holds one buffer from the buffer pool until its last handle goes away.

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

## Lighting

A world or render target canvas becomes lit when it receives `ambientLight`. Its scene pass renders into four targets of the pixel size of the canvas at once, with pipelines of four color attachments and the lit programs, which write through `haylen_output` of the shader library:

| Target | Holds |
| --- | --- |
| Scene | The colors of the draws, cleared to the canvas `clear` color. Unshaded draws leave black here. |
| Emission | The colors of unshaded draws and the colors of draws with `emission`, times that strength. |
| Surface | The world normal of normal-mapped sprites in red and green, and their specular strength in blue. |
| Info | The light mask, the layer shifted by 128 and the shininess of the draw divided by 255, written only where a draw covers more than half of the pixel so they never blend between two draws. |

Alpha, premultiplied and opaque draws write the surface and the info, while additive, multiply and screen draws, such as glows and shadows, leave them alone, and multiplying draws leave the emission alone too. The light pass then draws every light of the canvas into the light map, cleared to the ambient color, in the order the lights were drawn, as one quad each: a quad of twice the radius around point and spot lights, turned with them, and the view of the canvas for directional lights. The light shader reads the info target at its pixel, skips pixels whose light mask or layer the light does not reach, applies the falloff of the light texture and the cone of spot lights, the angle to the light for normal-mapped pixels and the highlight of their specular strength, and the shadow map, and adds, subtracts or mixes the result with the pipeline blend of the light. The composite multiplies the scene by the light map, adds the emission and applies the post-processing.

The light map is `RGBA16F` when the backend can render and blend it, which `Renderer::isHdrLighting()` and `graphics2d.hdrLighting()` report, so light goes past 1 and below 0, and the offscreen format otherwise, where light saturates at 1. `graphics2d.drawLight` and `Renderer::drawLight(const lighting2d::Light&)` draw lights and `graphics2d.drawOccluder` and `Renderer::drawOccluder(const lighting2d::Occluder&)` occluders, and both raise `Lights can only be drawn in a lit canvas, a world or render target canvas with ambient light.` or its occluder version in any other canvas. Without a texture, a light uses the engine's 128 by 128 radial falloff, which `Renderer::getLightTexture()` returns, `(1 - smoothstep(0, 1, d))^2` over the distance `d` from the center as a fraction of the radius, which `lighting2d::Light::falloff` computes on the CPU.

Occluders become world-space edges when they are drawn. Before the passes, every light with shadows gets one row of a shadow atlas, an `R32F` image 1024 texels wide that the frame writes once with `sg_write_image_transient`, and `lighting2d::ShadowMap` casts the rows on the task pool from the edges of the light's canvas. A point or spot light stores in each texel the distance, as a fraction of its radius, to the nearest edge in that direction, and a directional light stores the depth along the light of the nearest edge at each point across the view of the canvas. Every texel an edge touches takes it, so the silhouettes of overlapping occluders leave no gaps. The light shader reads one texel, or 5 or 13 texels spaced by the smoothness for the PCF filters, and counts a pixel shadowed where it lies more than 1.5 world units behind the stored depth. `lighting2d::Light::isShadowedAt` answers the same question exactly on the CPU for gameplay.

Every lit canvas owns its targets, created before the first pass of the frame and created again when the pixel size of the canvas changes. The [lighting reference](lua-api/lighting2d.md) describes lights, occluders and their Lua API.

## Custom shaders

Materials shade draws with the shaders apps write, as the [shader guide](shaders.md) explains. A draw with a material in its `DrawOrder` keeps a copy of the values the material had, and neighbouring draws that share a material and its values still merge into one draw call. The pipelines of a material live with its shader, one per program, blend mode and kind of target, created the first time a draw needs them and released when the shader reloads or goes away.

## Metaballs

`Renderer::drawMetaballs(points, radius, style, order)`, `graphics2d.drawMetaballs` in Lua, draws liquids and blobs from many points. Each call gets a field target at half the resolution of its canvas, and before the canvas renders, one instanced draw of the sprite program adds a soft circle for every point into it with additive blending, the kernel `(1 - d^2)^2` over 1.85 times the radius, which reaches 0.5 exactly at the radius. The draw item of the call then draws a quad over the area of the points that reads the field with linear filtering and shows the surface where it reaches the threshold, with the outline in the band above it, smoothed over the screen-space change of the field. The field saturates at 1 in the offscreen format, which the threshold never needs to reach. The item sorts and blends like any draw, and lit canvases light the surface.

## Post-processing

A world or render target canvas with `postProcess` renders offscreen like a lit canvas, without the light pass when it has no ambient light, and the composite applies the adjustments of `graphics2d::PostProcess` (`engine/include/haylen/2d/graphics/PostProcess.hpp`) in this order:

1. The light map, when the canvas is lit.
2. `saturation` around the luminance, then `contrast` around middle gray scaled by the alpha of the pixel, then `brightness`.
3. `tint`, multiplied into every pixel.
4. The vignette, darkening by `vignetteStrength` from `vignetteRadius` over `vignetteSoftness`.
5. `fade`, mixing toward its color by its alpha, which covers the canvas with that color as if it were opaque.
6. The `materials`, in order. The composite writes into a post target, every material but the last draws the image before it into the other post target, and the last draws into the destination of the canvas, each as one quad of the sprite program of the material.

Colors stay premultiplied by their alpha through the composite, which keeps the alpha of the scene, raises it where emission glows and toward 1 with the fade, and never lets a color exceed its alpha. A canvas over an opaque clear color looks the same either way, and a [transparent window](desktop.md#transparency) composes correctly with the desktop. The screen pass of every window that is not transparent at the moment clears alpha to 1 and draws with pipelines that never write alpha, so nothing an app draws lets the desktop through an opaque window. Screen canvases raise an error when given lighting or post-processing.

## Text

Fonts (`engine/include/haylen/text/`) come in two kinds behind one `text::Font` interface, which lays plain text out the same way for both: shaped glyphs at a native size and texture pages. `text::TrueTypeFont` shapes text with HarfBuzz and renders TrueType and OpenType fonts through a signed distance field, so one atlas serves every text size. Glyphs are rasterized by their index in the font the first time a string uses them: `stb_truetype` reads their outlines, lines, quadratic curves and the cubic curves of CFF fonts, and the core of msdfgen turns each outline into its exact distance field at the font's `bakeSize` (48 by default, the size of the em square) with a distance `spread` of 8 pixels. The fields are packed in shelves into a single-channel atlas that starts at `atlasSize` (512) pixels square. A full atlas doubles its shorter side and keeps every glyph where it was, up to the device's maximum texture size, beyond which layout raises `The font atlas exceeded the maximum texture size.` A single glyph whose field could not fit even the largest atlas raises `The glyph is larger than the largest font atlas the device can hold.` before its field is computed. The atlas is a dynamic texture: a draw that added glyphs hands the atlas to `Device::updateTexture`, and the renderer sends it once per frame before its passes, whatever the number of draws that added glyphs, since Sokol only updates whole images of textures that live across frames. An atlas that grew becomes a new texture, so text drawn earlier in the frame keeps the image its coordinates were made for. The UI atlas of Dear ImGui changes in place the same way. `text::BitmapFont` draws BMFont files and grids of equal cells from their own RGBA pages, pixel for pixel at their native size, through the sprite program.

The text program smooths the edge of the distance field with the screen-space derivative, which keeps edges crisp when scaled, and reads four values of every glyph from its instance. The weight moves the edge outward, which is how synthetic bold grows the strokes of a face a family lacks. The outline is a second threshold of the same field, filled with the flash color, and the fill covers it with premultiplied colors, so every edge blends its coverage once. The softness widens the edge into a blur, for soft shadows and glows. The skew leans the quad around the baseline in the vertex shader, which is how synthetic italic slants. Outline, weight and softness together reach at most the spread, and a glyph whose values reach further shrinks them together. A shadow draws every glyph a second time before the text, offset, in the shadow color and softened by its blur, and the shadow of a bitmap glyph is its silhouette. Layout decodes UTF-8, splits the text into runs of one font, script and direction, shapes every run, ends paragraphs at every paragraph separator, such as `\n`, `\r\n` or U+2029, and lines at mandatory breaks, wraps by the Unicode line breaking rules when `maxWidth` is set, orders every line for display by the bidirectional algorithm, aligns lines to their start, end, left, center or right or fills them, and places the block by its `anchor` with an optional rotation. The [text guide](text.md) describes shaping, directions and line breaking, and fonts cache their recent layouts, so text drawn every frame is shaped once.

Rich text (`text::RichText`, described in the [text guide](text.md)) parses BBCode markup into a document, lays it out with a font family into glyphs, filled boxes, images and hit areas, and caches the layout by width and scale. Every frame its effects and its typewriter reveal work on a copy. `Renderer::drawRichText` hands that frame to the text painter, which turns it into instances in layers: backgrounds, glows, shadows, images, glyphs, and then underlines and strikes. Consecutive instances of one program and texture share a draw item, so a paragraph in one font is one draw call, and each fallback font, bitmap page or image adds its own. The UI draws the text of every component the same way from a draw list callback, labels and buttons with `Renderer::drawText` and `richText` nodes with `drawRichText`, at their place among the ImGui meshes and inside the clip of their window.

The engine embeds Roboto Medium from Dear ImGui as its default font, which `graphics2d.defaultFont()` and `Engine::getDefaultFont()` return. `assets.font(path, options)` loads other fonts, TrueType or BMFont by the extension, and a larger `bakeSize` keeps corners sharper at large sizes at the price of atlas space. Each font has its own atlas texture, so switching fonts between draws starts a new draw call. The options are described in the [assets reference](lua-api/assets.md).

## Nine-slices

A `NineSlice` (`engine/include/haylen/2d/graphics/NineSlice.hpp`) is a texture with nine source rectangles, row by row from the top-left corner. `NineSlice::fromBorders` cuts one source rectangle with fixed borders, and `NineSlice::fromPieces` takes nine separate rectangles, like the packed Tiny Swords pieces that `make.py assets` writes to `ui/sliced.json`. In Lua, `graphics2d.newNineSlice(texture, {borders = ...})` or `{pieces = ...}` builds one.

`drawNineSlice(slice, area, color, order, borderScale)` keeps the corners at their size times `borderScale`, and shrinks the borders proportionally when the area is smaller than them. With the `stretch` fill the edges and the center stretch. With the `tile` fill they repeat at their scaled size and the last tile of each row and column is cropped. All pieces go into one draw item with the slice's texture, so frames that share a texture batch together. UI themes use nine-slices for their surfaces, as the [UI guide](ui.md) explains.

## Tiled maps

`tiled::MapRenderer::draw(renderer, view, order)`, which Lua calls as `map:draw(camera, order)`, draws every visible layer in map order into the active canvas. The [Tiled guide](tiled.md) covers the supported features, and drawing works like this:

- The first time a tile layer draws, it is baked into static batches, one per region of 32 by 32 cells and per run of tiles that share a texture. Layer tint and opacity are baked into the instance colors.
- Animated tiles stay out of the baked batches and are drawn every frame with the frame that matches the map time, which `map:update(dt)` advances.
- With a camera, a region is drawn only when its bounds, shifted by the layer's parallax, intersect the area the active canvas shows, which is the visible design area for world canvases and the target size for render target canvases. Tile objects are culled one by one, image layers that repeat draw only the copies that cover the visible area, and other image layers draw whole. Without a camera nothing is culled.
- Parallax shifts a layer by the distance between the camera position and the map's parallax origin, times one minus the parallax factor, through the offset of `drawStatic`, so parallax never rebakes.
- `map:setTile` drops the baked batches of its layer, and the whole layer bakes again on its next draw.
- Every layer uses the given draw order with its blend replaced by the layer's blend mode. Later layers cover earlier ones because they are recorded later, and `map:drawLayer(name, camera, order)` lets the app draw its own sprites between layers.
- `MapRenderer::DrawOptions::ysort`, `{ysort = true}` in Lua, bakes tile layers into rows of cells that stand on the same y, a row of an orthogonal map or a diagonal of an isometric one, split every 32 columns for culling, and draws object layers object by object. Each row and object gets the y it stands on as its depth and as its sort point, so entities in the same layer of a canvas that sorts by y or by depth pass behind and in front of map objects. The ysort bake is kept apart from the regular one, so a map can draw one layer both ways.

## Cameras and parallax

`graphics2d::Camera` works like this: its position is the point the view shows, and `follow` moves it toward a target through the dead zone, the per-side drag margins with their drag offset, the look-ahead from the target velocity, position smoothing and limits with optional limit smoothing. `update` advances the trauma shake, whose frequency, direction and amount are settings, and the rotation smoothing. The zoom has two axes and limits, `zoomAt` zooms around a screen point for pinch and wheel input, `frame` keeps several targets in view, `Camera::blend` returns a view between two cameras for smooth cuts, and `drawDebug` draws the view, the limits and the drag box. A camera with a `viewport` draws into that rectangle of the screen, which is how one frame draws split screens and minimaps with several cameras, and every canvas of a viewport clips to it.

`graphics2d::Parallax` scrolls a texture at its own rate as the camera moves, repeats it on either axis over the part of the world the camera sees, scrolls by itself over time and stops following the camera outside its limits. Its offset also moves static batches with `drawStatic`, so baked layers get parallax without rebaking. The [camera](lua-api/graphics2d.md#camera) and [parallax](lua-api/graphics2d.md#parallax) references describe every setting.

## GPU object pools

Sokol preallocates its resource pools, and the `Gpu` class of `engine/src/graphics/Gpu.hpp` sets the sizes that bound how many GPU objects exist at once:

| Pool | Size | Taken by |
| --- | --- | --- |
| Images (`kImagePoolSize`) | 4096 | Every texture, render target and font atlas, the renderer's white, light and metaball textures and shadow atlas, the targets of each canvas with lighting or post-processing, and the field of each metaball draw. |
| Views (`kViewPoolSize`) | 8192 | One per texture and two per render target, so the image pool always runs out first. |
| Buffers (`kBufferPoolSize`) | 4096 | Every static batch, including each baked region run of a Tiled layer, plus the renderer's quad and its three streaming buffers. |
| Shaders (`kShaderPoolSize`) | 512 | The renderer's twelve programs, plus every program of a custom shader that a draw has used, up to six per shader: its sprite, text and mesh programs and their lit versions. |
| Pipelines (`kPipelinePoolSize`) | 2048 | One for each program, blend mode and kind of target that the renderer has drawn with, for its own programs and for the programs of every custom shader. |

Creating an object in a full pool throws, which in Lua raises an error that shows the error screen unless the app catches it:

| Error | Cause |
| --- | --- |
| `The graphics device has no room for another texture. At most 4096 textures and render targets can exist at once.` | The image pool is full. |
| `The graphics device has no room for another buffer. At most 4096 baked sprite batches and draw buffers can exist at once.` | The buffer pool is full. |
| `The graphics device has no room for another shader. At most 512 shader programs can exist at once.` | The shader pool is full. |
| `The graphics device has no room for another pipeline. At most 2048 pipelines can exist at once.` | The pipeline pool is full. |
| `The graphics device could not create a <width>x<height> texture.` | The backend rejected the texture. |
| `The graphics device could not create a buffer of <size> bytes.` | The backend rejected the buffer. |
| `The graphics device could not create the shader <label>.` | The backend rejected a shader program, where a custom shader's label is its name and program, such as `ripple/sprite_lit`. |
| `The graphics device could not create the pipeline <label>.` | The backend rejected a pipeline. |
| `Texture dimensions must be positive.` and `Texture dimensions exceed the device limit.` | A texture or render target size outside the device limits, which `Device::getMaxTextureSize()` and `graphics.maxTextureSize()` report. |

A slot is freed when the last handle to its object goes away, and the object is destroyed at the end of that frame, once no queued draw can use it. Lua handles go away when the garbage collector finalizes them, and assets loaded through a preload group stay alive until `assets.unloadGroup` releases the group, as the [assets reference](lua-api/assets.md) explains.

## Performance guidance

- Draw what shares a texture next to each other in draw order. Pack sprites into atlases, which [haylen.animation2d](lua-api/animation2d.md) and [haylen.assets](lua-api/assets.md) load from TexturePacker and Aseprite files, and give ground, units, effects and HUD layers of their own.
- Depth sorting interleaves textures by depth, so every change of texture between neighbours costs a draw call. Put everything that sorts together on one atlas.
- Rectangles and lines batch with each other on the white texture, while circles, rings, arcs and polygons use the mesh program. Alternating shapes and textured sprites alternates draw calls.
- Draw many moving sprites of one texture with a sprite batch rather than one `graphics2d.draw` per sprite, and change only the sprites that move with `batch:set`.
- Bake what never moves and draw it with `drawStatic`, moving it with the offset instead of rebaking.
- The renderer submits every recorded draw and does no culling of its own, apart from what Tiled maps do. Skip off-screen objects in large worlds with `camera:visibleBounds()` or `graphics2d.canvasBounds()`, and with [haylen.spatial2d](lua-api/spatial2d.md) to find what is visible.
- A post-processed canvas costs an offscreen pass, a full-viewport composite and a render target, plus a pass and a target per material. A lit canvas adds a scene pass that writes four targets, a light pass with one quad per light and a floating-point light map. One such canvas per frame covers most apps.
- Every light with shadows casts its shadow map on the CPU from the occluder edges within its reach, spread over the task pool. Keep occluders simple, with one edge per face of an object, and give shadows only to the lights that need them.
- Text rasterizes new glyphs on the frame thread and uploads the atlas again. Measure the strings a scene shows while it loads, with `graphics2d.measureText` or `Font::measure`, so the glyphs exist before the first frame. Shadows double the glyph quads, and each font breaks batches with the others.
- Clip rectangles split batches, so scroll areas and clipped panels cost a draw call each.
- `map:setTile` rebakes the whole layer. Keep tiles that change often in a small layer of their own.
- Recording draws in their final order, for example layer by layer from the lowest, lets the renderer upload instances without copying them into draw order.
- Measure in Release builds, because Debug builds run without optimizations and with the Sokol validation layer.

## Sprite benchmark

```sh
python3 make.py bench
```

`bench` builds the `haylen-sprite-benchmark` app in Release for the host and runs it on the local GPU. The app is `engine/bench/SpriteBenchmark.cpp`, written in C++ and added with `haylen_add_app(... CPP ...)` on desktop builds when `HAYLEN_BUILD_BENCHMARKS` is on, with the package `engine/bench/sprite-benchmark`. Its `app.json` opens a 1280 by 720 high-DPI window with vsync off and a 1920 by 1080 design area with the `fit` policy.

The benchmark runs six phases: 100,000, 1,000,000 and 2,000,000 sprites, each first dynamic and then baked. Every phase draws 3 by 3 unit sprites of a tinted 4 by 4 white texture, in a grid dense enough to cover the design area, for 30 warm-up frames and 240 measured frames. Dynamic phases move every sprite every frame with `JobSystem::parallelFor` and draw them with one `drawBatch`, so all instance data is converted and uploaded every frame, 96 MB at 2,000,000 sprites of 48 bytes. Baked phases draw one static batch made with `createStaticBatch` and shift it every frame through the `drawStatic` offset. The app quits after the last phase.

Each phase logs one line:

| Column | Meaning |
| --- | --- |
| `sprites` | Sprites drawn per frame. |
| `kind` | `dynamic` or `baked`. |
| `ms` | Wall time between frames, which includes the GPU and, on displays that pace frames themselves like macOS does, the display. |
| `fps` | Frames per second from `ms`. |
| `Msprites/s` | Millions of sprites drawn per second. |
| `work ms` | CPU time of the frame outside the `submit` step, where the renderer uploads and waits for the next drawable. |

The results recorded in `PROJECT.md`, measured on an Apple M5 Pro with Metal in Release:

| Phase | Frame rate | CPU per frame (`work ms`) |
| --- | --- | --- |
| 2,000,000 animated sprites | 105 fps | 4.5 ms |
| 2,000,000 baked sprites | 120 Hz, the refresh limit of the display | 0.02 ms |

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

`engine.getScenes().push(std::make_shared<NightScene>())` shows it. The [embedding guide](embedding.md) explains how a C++ app is built.
