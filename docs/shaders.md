# Shaders

Apps shade their draws with their own fragment shaders, written in the annotated GLSL of [sokol-shdc](https://github.com/floooh/sokol-tools/blob/master/docs/sokol-shdc.md) under `content/shaders/`. `make.py shaders` compiles every source ahead of time into a `.shader` file for every backend the engine runs on, and the engine loads the file as a `Shader` and draws with it through a `Material`, which holds the values of its uniforms and textures. A material shades sprites, batches, nine-slices, rectangles, lines, shapes, meshes and text in every canvas, and runs over the whole image of a world or render target canvas as a post-processing pass.

## A first shader

A shader source includes the shader library, writes a fragment shader that ends with `haylen_output`, and declares one program whose vertex shader is `haylen_vs`:

```glsl
// content/shaders/ripple.glsl
@include haylen/material.glsl

@fs ripple_fs
@include_block haylen_fragment
layout(binding=1) uniform ripple_params {
    float time;
    float strength;
    vec4 tint;
};

void main() {
    vec2 wave = vec2(sin(uv.y * 40.0 + time * 4.0), cos(uv.x * 40.0 + time * 3.0)) * strength;
    vec4 base = haylen_base(uv + wave);
    haylen_output(vec4(base.rgb * tint.rgb, base.a));
}
@end

@program ripple haylen_vs ripple_fs
```

`python3 make.py shaders my-game` compiles it into `content/shaders/ripple.shader`, and `make.py run` and `make.py package` compile the shaders that changed on their own. In Lua, [haylen.assets](lua-api/assets.md) loads the file and [haylen.graphics2d](lua-api/graphics2d.md#material) makes a material of it:

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local ripple = graphics2d.newMaterial(assets.shader('shaders/ripple.shader'), {strength = 0.01, tint = '#FFC0E0FF'})
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        ripple:set('time', haylen.time())
        graphics2d.beginWorld(camera)
        graphics2d.draw(graphics.whiteTexture(), 0, 0, {width = 200, height = 200, color = '#FF4080C0', material = ripple})
        graphics2d.drawText(nil, 'Ripple', 0, 150, {size = 48, anchor = {0.5, 0.5}, material = ripple})
    end,
})
```

## Compiling

`make.py shaders <app>` compiles every `.glsl` file under `content/shaders/` that declares an `@program`, and leaves the others, which the sources include, alone. It runs `sokol-shdc` from `.tools/` with `engine/shaders/include` as its working directory, which is how `@include haylen/material.glsl` finds the shader library, and writes a `.shader` file next to each source, so the files ship in the package like any asset. A source compiles again when its `.shader` file is older than any source of the app or the shader library, and `--force` compiles them all. The app's own `@include` paths are relative to the folder of the source being compiled.

Every source compiles six times, once for each kind of draw and once more of each for lit canvases:

| Program | Defines | Draws |
| --- | --- | --- |
| `sprite`, `sprite_lit` | none, `HAYLEN_LIT` | Sprites, batches, nine-slices, rectangles, lines and post-processing. |
| `text`, `text_lit` | `HAYLEN_TEXT`, `HAYLEN_TEXT HAYLEN_LIT` | Text. |
| `mesh`, `mesh_lit` | `HAYLEN_MESH`, `HAYLEN_MESH HAYLEN_LIT` | Meshes, circles, rings, arcs and polygons. |

Each program compiles for Metal on macOS, iOS and the iOS simulator, HLSL 5 for Direct3D 11, GLSL 4.30 for desktop OpenGL, GLSL 3.00 ES for Android and WebGL2, and WGSL for WebGPU. A compile error stops the command with the file, the line and the program it happened in:

```text
error: sokol-shdc could not compile the sprite program of content/shaders/ripple.glsl:
content/shaders/ripple.glsl:13:0: error: 'wave' : undeclared identifier
```

Browsers cannot run `sokol-shdc`, so the web editor and the web runtime never compile shaders. An app compiles its shaders on a machine with `make.py` and ships the `.shader` files in its package, and an edited source reaches the browser once its `.shader` file is compiled again.

In development, `make.py run` keeps compiling the sources that change while the desktop player runs with `--dev`, and the player reloads every changed `.shader` file in place, so the materials that use it draw with the new programs from the next frame and keep the values that still fit its uniforms. A source with an error prints the error and leaves the last good shader running.

## The shader library

`haylen/material.glsl` holds what every material shares. It defines the vertex shader `haylen_vs`, which places sprites, glyphs and mesh vertices exactly like the engine's own programs, and the block `haylen_fragment`, which a fragment shader includes first:

| Name | Kind | Meaning |
| --- | --- | --- |
| `uv` | `in vec2` | Texture coordinate of the pixel in the texture of the draw. |
| `color` | `in vec4` | Color of the draw, or of the mesh vertex. |
| `sprite_texture`, `sprite_sampler` | texture and sampler at binding 0 | The texture of the draw, the image before the pass in post-processing, and the atlas of text. |
| `haylen_texture(vec2 point)` | `vec4` | The texture of the draw at a point. |
| `haylen_sprite(vec2 point)` | `vec4` | The texture color times the draw color, mixed toward the flash color of the sprite, which is how sprites and meshes draw. |
| `haylen_text(vec2 point)` | `vec4` | The glyph at a point with its fill and outline, which is how text draws. |
| `haylen_base(vec2 point)` | `vec4` | `haylen_text` in the text programs and `haylen_sprite` in the others, the color the draw has without the material. |
| `haylen_world_normal(vec3 tangent)` | `vec2` | Turns a tangent-space normal with y pointing up the image into the world through the flips and rotation of the sprite. |
| `haylen_output(vec4 color)` | function | Writes the result, a straight color, which it multiplies by its alpha for draws whose blend mode needs that, `multiply` and `screen`. In lit canvases it also writes the emission, surface and info images the light pass reads, with a flat normal. |
| `haylen_output_surface(vec4 color, vec2 normal, float specular, float shininess)` | function | Only with `HAYLEN_LIT`: writes the result with a world normal, a specular strength and a shininess divided by 255, for materials that light their own surface. |
| `haylen_surface`, `haylen_info` | `vec4` uniforms | Only with `HAYLEN_LIT`: the lighting of the draw, which `haylen_output` applies. |

A material reads its colors through `haylen_base`, so the same source shades sprites, meshes and text, and it rarely needs to tell the programs apart. Code that only makes sense in one of them checks the defines, such as `#ifdef HAYLEN_LIT`. Lit canvases light a material's draws like any other draw, with its `unshaded`, `emission`, `lightMask` and layer, and with a flat normal unless the material writes its own with `haylen_output_surface`. The library keeps names that start with `haylen_`, `uv`, `color` and the outputs `frag_color`, `frag_emission`, `frag_surface` and `frag_info`, and sokol-shdc rejects a uniform or texture that repeats a name of the program.

The engine's own shaders in `engine/shaders` include the same library, so a material draws exactly where the engine's program would.

## Uniforms and textures

A material fills the uniform blocks and textures that the fragment shader declares:

- Uniform blocks take the bindings 1 to 6. Binding 0 holds the view projection of the vertex shader and whether the blend of the draw needs premultiplied colors, and binding 7 the lighting of lit canvases. The renderer fills both with the layout of the shader library it was built with, so a shader compiled with another version of the library raises `The shader <name> was compiled with another version of the shader library. Compile it again with make.py shaders.` when a draw first uses it.
- Members are `float`, `vec2`, `vec3`, `vec4`, `int`, `ivec2`, `ivec3`, `ivec4` and `mat4`, and arrays of `vec4`, `ivec4` and `mat4`, the types sokol-shdc allows, laid out with std140 rules.
- Textures take the bindings from 1 up, next to `sprite_texture` at 0, with samplers of their own from binding 1 up or `sprite_sampler`. The sampler of a texture binding is the one the texture was created with, so a texture loaded with `filter = 'linear'` samples smoothly.
- A texture that a material leaves unset draws as white.

`material:set(name, value)` takes the value that fits the type, as the [Material reference](lua-api/graphics2d.md#material) lists, and a draw copies the values of its material when it is made. `shader.uniforms` and `shader.textures` list what a shader offers.

```glsl
// content/shaders/palette.glsl
@include haylen/material.glsl

@fs palette_fs
@include_block haylen_fragment
layout(binding=1) uniform palette_params {
    vec4 colors[4];
    mat4 warp;
    int steps;
};
layout(binding=1) uniform texture2D noise_texture;

void main() {
    vec4 base = haylen_base((warp * vec4(uv, 0.0, 1.0)).xy);
    float grain = texture(sampler2D(noise_texture, sprite_sampler), uv * 4.0).r;
    float level = floor(dot(base.rgb, vec3(0.3, 0.59, 0.11)) * float(steps) + grain * 0.5);
    haylen_output(vec4(colors[int(clamp(level, 0.0, 3.0))].rgb, base.a));
}
@end

@program palette haylen_vs palette_fs
```

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local scene = require('haylen.scene')

local palette = graphics2d.newMaterial(assets.shader('shaders/palette.shader'), {
    colors = {0.1, 0.1, 0.2, 1, 0.3, 0.3, 0.5, 1, 0.6, 0.6, 0.8, 1, 0.9, 0.9, 1, 1},
    warp = m.rotation(0.1),
    steps = 4,
    noise_texture = graphics.newTexture(2, 2, {pixels = string.rep('\255\0\0\255\0\0\0\255', 2), wrap = 'repeat'}),
})
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawCircle(0, 0, 120, '#FFFFFFFF', {material = palette})
    end,
})
```

## Post-processing

The `materials` of a world or render target canvas's `postProcess` run after the composite and the other adjustments, in order. Each one draws the image of the step before over the whole canvas, as `sprite_texture` with `uv` from 0 at the top-left corner to 1 at the bottom-right one, and the last one draws into the destination of the canvas. The sprite program of the material runs these passes, so a post-processing shader reads the image with `haylen_texture(uv)` and writes it with `haylen_output`.

```glsl
// content/shaders/scanlines.glsl
@include haylen/material.glsl

@fs scanlines_fs
@include_block haylen_fragment
layout(binding=1) uniform scanlines_params {
    float strength;
    float lines;
    float spread;
};

void main() {
    vec2 offset = (uv - 0.5) * spread;
    vec3 split = vec3(haylen_texture(uv + offset).r, haylen_texture(uv).g, haylen_texture(uv - offset).b);
    float line = 1.0 - strength * step(0.5, fract(uv.y * lines));
    haylen_output(vec4(split * line, 1.0));
}
@end

@program scanlines haylen_vs scanlines_fs
```

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local screen = graphics2d.newMaterial(assets.shader('shaders/scanlines.shader'), {strength = 0.35, lines = 180, spread = 0.02})
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera, {clear = '#FF203040', postProcess = {vignetteStrength = 0.4, materials = {screen}}})
        graphics2d.drawRect({-200, -100, 400, 200}, '#FF3A7D44')
    end,
})
```

## The .shader file

A `.shader` file is JSON that `make.py shaders` writes and nothing else edits:

| Key | Meaning |
| --- | --- |
| `format`, `version` | `"haylen-shader"` and `1`. |
| `name` | The name of the program. |
| `blocks` | The uniform blocks of the shader's own, each with its name, binding slot, size and members, whose names, types, element counts and byte offsets come from the reflection of sokol-shdc. |
| `textures` | The textures of the shader's own, each with its name and binding slot. |
| `sources` | The source of every stage of every program for every language, each text once. |
| `programs` | For each of the six programs and each language, the entry points, the source of each stage and the bindings of its attributes, uniform blocks, textures and samplers, as the shader description of Sokol names them. |

The engine reads the file on a worker thread, and on the frame thread creates the GPU program of the active backend the first time a draw uses it, and the pipelines for each blend mode and kind of target.

Reading the file also checks it, so a damaged or hand-edited file fails at once instead of when a draw uses it. Every attribute, uniform block, block member, texture, sampler and texture sampler pair must fit the binding slots of the GPU, every stage must name a source of the file, every pair must name a texture and a sampler its program declares, the names of stages, texture types, sample types, sampler types and attribute types must be ones `sokol-shdc` writes, the GLSL members of a block must fill it exactly, every uniform of `blocks` must fit inside its block, and every program must read each block with the size `blocks` gives it. A file that breaks a rule raises `The shader file is malformed: ` followed by the problem, such as `The texture slot 40 is out of range.` or `The uniform tint does not fit in the block params.`, from `assets.shader` or from the hot reload, which keeps the last good shader. A GPU program the backend rejects raises `The graphics device could not create the shader <name>/<program>.` when a draw first uses it.

## C++

In C++, `assets::Manager::shader(path)` loads a `graphics::Shader` (`engine/include/haylen/graphics/Shader.hpp`), and `graphics2d::Material` (`engine/include/haylen/2d/graphics/Material.hpp`) sets its values and goes into the `material` of a `graphics2d::DrawOrder` or the `materials` of a `graphics2d::PostProcess`:

```cpp
#include "haylen/2d/graphics/Material.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"

class RippleScene final : public haylen::core::Scene {
  public:
    explicit RippleScene(haylen::core::Engine& engine) : ripple(engine.getAssets().shader("shaders/ripple.shader")) {
        ripple.set("strength", 0.01F);
        ripple.set("tint", haylen::math::Color{0.75F, 0.88F, 1.0F, 1.0F});
    }

    void render(haylen::core::Engine& engine) override {
        haylen::graphics2d::Renderer& renderer = engine.getRenderer2D();
        ripple.set("time", static_cast<float>(engine.getClock().getElapsed()));
        renderer.beginWorld(camera, {.postProcess = haylen::graphics2d::PostProcess{.materials = {ripple}}});
        renderer.drawRect({-100.0F, -100.0F, 200.0F, 200.0F}, haylen::math::Color::white(), {.material = ripple});
    }

  private:
    haylen::graphics2d::Material ripple;
    haylen::graphics2d::Camera camera;
};
```
