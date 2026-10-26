# ✨ Shaders

This guide explains how an app shades its draws with fragment shaders of its own. It is for developers who want effects that the built-in options lack, such as a ripple, a palette swap or a screen filter. It covers the Haylen shader language, compiling for every graphics API, the shader library, uniforms and textures, post-processing, the game shaders of the test project and what loading a shader costs. The [`Material` reference](../lua-api/graphics2d.md#material) and the [`Shader` reference](../lua-api/graphics.md#shader) list the Lua API.

## How custom shaders work

A custom shader goes through four steps:

1. You write a source in the [Haylen shader language](#the-haylen-shader-language), annotated GLSL, under `content/shaders/`.
2. The command `haylen.py shaders` compiles every source ahead of time with the Haylen shader compiler into a `.shader` package. The package holds the code of every shader language the engine runs.
3. The engine loads the package as a `Shader`.
4. A `Material` draws with the shader and holds the values of its uniforms and textures.

A material shades sprites, batches, nine-slices, rectangles, lines, circles, shapes, polygons, meshes and text in every canvas. It also runs over the whole image of a world or render target canvas as a [post-processing](#post-processing) pass.

## A first shader

A source includes the shader library and writes a fragment shader that ends with `haylen_output`. It declares one program whose vertex shader is `haylen_vs`:

```glsl
// The file `content/shaders/ripple.glsl`.
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

The command `haylen shaders my-game` compiles it into `content/shaders/ripple.shader`. In Lua, [`haylen.assets`](../lua-api/assets.md#assetsshaderpath) loads the file and [`graphics2d.newMaterial`](../lua-api/graphics2d.md#material) makes a material of it:

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
        ripple:set('time', haylen.elapsed())
        graphics2d.beginWorld(camera)
        graphics2d.draw(graphics.whiteTexture(), 0, 0, {width = 200, height = 200, color = '#FF4080C0', material = ripple})
        graphics2d.drawText(nil, 'Ripple', 0, 150, {size = 48, anchor = {0.5, 0.5}, material = ripple})
    end,
})
```

## The Haylen shader language

A source is GLSL 4.50 in the dialect that SPIR-V compilers read. It uses separate `texture2D` and `sampler` objects, and every uniform block, texture and sampler has a `layout(binding=N)`. A few tags split the source into stages, pair the stages into programs and share code between stages and files. A tag is a line whose first word starts with `@`:

| Tag | What it does |
| --- | --- |
| `@vs <name>`, `@fs <name>` | Starts a vertex stage or a fragment stage. Its GLSL ends at `@end` and has a `void main()` as its entry point. |
| `@block <name>` | Starts a block of code, closed by `@end`, which stages and other blocks paste in with `@include_block <name>`. |
| `@include <path>` | Reads the stages, blocks and programs of another file, once however many files include it. |
| `@program <name> <vertex stage> <fragment stage>` | Pairs a vertex stage and a fragment stage into a program. |
| `@sample_type <texture> <type>` | Gives a texture that holds floats the sample type `unfilterable_float` or `depth`. |
| `@sampler_type <sampler> <type>` | Gives a sampler the type `nonfiltering`. |

The compiler gives inputs and outputs their locations, so a stage declares them without `layout(location=N)`. Every input of the fragment stage reads the output of the vertex stage with the same name. A texture and a sampler combine where the stage samples, such as `texture(sampler2D(sprite_texture, sprite_sampler), uv)`. The repository of the compiler, [haylen-shader-compiler](https://github.com/haylen-org/haylen-shader-compiler), holds the complete reference of the [language](https://github.com/haylen-org/haylen-shader-compiler/blob/main/docs/language.md), its errors and its command line, and of the [package format](https://github.com/haylen-org/haylen-shader-compiler/blob/main/docs/format.md).

## Compiling

The command `haylen.py shaders <app>` compiles every `.glsl` file under `content/shaders/` that declares an `@program`. It leaves the other files alone, since the sources include them. The commands `run`, `prepare`, `package` and `dist` of `haylen.py` compile the shaders that changed on their own.

The command runs `haylen-shaderc`, the Haylen shader compiler. Unless the environment variable `HAYLEN_SHADER_COMPILER` names a build of it, `haylen.py` uses the release that `engine/cmake/haylen-shader-compiler.json` pins and keeps it in `.tools/`. It downloads the binary of that release for the machine and checks its SHA-256. Only on a machine that the release ships no binary for does it build the compiler from the source of the pinned commit, as the [build guide](../tools/build.md#tools) describes.

The command passes `engine/shaders/include` with `--include`, which is how `@include haylen/material.glsl` finds the shader library. It writes a `.shader` file next to each source, so the files ship in the package like any asset. A source compiles again when its `.shader` file is older than any source of the app or of the shader library, and `--force` compiles them all. An `@include` path of the app resolves against the folder of the file that includes it first, and then against the shader library.

```sh
haylen shaders my-game
haylen shaders my-game --force
```

A source declares exactly one `@program`, whose vertex shader is `haylen_vs`. Otherwise the command stops with `The shader <path> must declare exactly one "@program" whose vertex shader is "haylen_vs" from "haylen/material.glsl".`. That program compiles ten times: as variants with the defines of each kind of draw, and once more of each for lit canvases.

| Program | Defines | Draws |
| --- | --- | --- |
| `sprite`, `sprite_lit` | none, `HAYLEN_LIT` | Sprites, batches, nine-slices, rectangles, lines and post-processing. |
| `batch`, `batch_lit` | `HAYLEN_BATCH`, `HAYLEN_BATCH HAYLEN_LIT` | Sprite batches with several textures or with [sprite data](#sprite-data). |
| `text`, `text_lit` | `HAYLEN_TEXT`, `HAYLEN_TEXT HAYLEN_LIT` | Text. |
| `mesh`, `mesh_lit` | `HAYLEN_MESH`, `HAYLEN_MESH HAYLEN_LIT` | Meshes and polygons. |
| `shape`, `shape_lit` | `HAYLEN_SHAPE`, `HAYLEN_SHAPE HAYLEN_LIT` | Circles, rings, arcs and shapes, whose `uv` goes from 0 to 1 across the rectangle of the shape. |

Each program compiles for every target of the compiler, so one package runs on every graphics API:

| Target | Graphics API |
| --- | --- |
| `msl-macos`, `msl-ios`, `msl-ios-simulator` | Metal on macOS, iOS and the iOS simulator. |
| `hlsl5` | Direct3D 11. |
| `glsl430` | Desktop OpenGL. |
| `glsl300es` | Android and WebGL2. |
| `wgsl` | WebGPU. |

A compile error stops the command with the file, the line and the column of every error:

```text
Error: The shader my-game/content/shaders/ripple.glsl does not compile, and the Haylen shader compiler names the lines below.
/home/me/my-game/content/shaders/ripple.glsl:14:34: error: Undeclared identifier ("wave").
```

Browsers cannot run the shader compiler, so the web editor and the web runtime never compile shaders. An app compiles its shaders on a machine with `haylen.py` and ships the `.shader` files in its package. An edited source reaches the browser once its `.shader` file is compiled again.

In development, `haylen.py run` keeps compiling the sources that change while the desktop player runs with `--dev`. The player reloads every changed `.shader` file in place. The materials that use it draw with the new programs from the next frame, and keep the values that still fit its uniforms. A source with an error prints the error and leaves the last good shader running.

## The shader library

The file `haylen/material.glsl` holds what every material shares:

- The vertex shader `haylen_vs`, which places sprites, glyphs, mesh vertices and shapes exactly like the programs of the engine.
- The block `haylen_vertex`, with the body of `haylen_vs` as the function `haylen_vertex_main()`. A vertex shader with attributes of its own includes it and calls it.
- The block `haylen_fragment`, which a fragment shader includes first. It declares these names:

| Name | Kind | Meaning |
| --- | --- | --- |
| `uv` | `in vec2` | Texture coordinate of the pixel in the texture of the draw. |
| `color` | `in vec4` | Color of the draw, or of the mesh vertex. |
| `sprite_texture`, `sprite_sampler` | texture and sampler at binding 0 | The texture of the draw, the image before the pass in post-processing, and the atlas of text. |
| `haylen_texture(vec2 point)` | `vec4` | The texture of the draw at a point. A sprite of a batch with several textures picks its own texture. |
| `haylen_texture_grad(vec2 point, vec2 slopeX, vec2 slopeY)` | `vec4` | The texture of the draw at a point, with the slopes of the point across a pixel, `dFdx` and `dFdy`, which pick its mip level. A loop that samples the texture takes the slopes once before it starts and passes them to each step, so the compilers of every graphics API keep the loop instead of unrolling it, which keeps the shader small and quick to compile. |
| `haylen_source` | `in vec4` | The source rectangle of the draw in texture coordinates, with its smallest corner in `xy` and its largest in `zw`. A source that reaches past its texture runs below 0 or past 1. Meshes, strokes and shapes have the whole texture. |
| `haylen_data` | `in vec4` | The four numbers of the [sprite data](#sprite-data) of a sprite of a batch, and zeros in the other draws. |
| `haylen_inside(vec2 point)` | `vec2` | The point moved inside the texels of the source rectangle of the draw, half a texel in from its sides. So filtering never reads the texels around the region of a sprite, such as its neighbours in an atlas. Meshes keep their point, and shapes keep it inside the texture. |
| `haylen_sprite(vec2 point)` | `vec4` | The texture color inside the source of the draw times the draw color, mixed toward the flash color of the sprite, which is how sprites and meshes draw. A shape paints its border color over it in the band along its edge. |
| `haylen_text(vec2 point)` | `vec4` | The glyph at a point with its fill and outline, which is how text draws. Its weight, outline and softness, with the smoothing of the edge on screen, stop a sixteenth of the field short of the border of the quad of the glyph and shrink together beyond it. |
| `haylen_base(vec2 point)` | `vec4` | The function `haylen_text` in the text programs and `haylen_sprite` in the others: the color the draw has without the material. |
| `haylen_world_normal(vec3 tangent)` | `vec2` | Turns a tangent-space normal, with y pointing up the image, into the world through the flips and rotation of the sprite. |
| `haylen_output(vec4 color)` | function | Writes the result, a straight color. It multiplies the color by its alpha for draws whose blend mode needs that, `multiply` and `screen`. A shape fades the alpha by how much of the pixel it covers, so its edge stays smooth whatever color the material writes. In lit canvases it also writes the emission, surface and info images that the light pass reads, with a flat normal. |
| `haylen_output_surface(vec4 color, vec2 normal, float specular, float shininess)` | function | Only with `HAYLEN_LIT`: writes the result with a world normal, a specular strength and a shininess divided by 255, for materials that light their own surface. |
| `haylen_surface`, `haylen_info` | `vec4` uniforms | Only with `HAYLEN_LIT`: the lighting of the draw, which `haylen_output` applies. |

A material reads its colors through `haylen_base`, so the same source shades sprites, meshes, shapes and text, and it rarely needs to tell the programs apart. Code that only makes sense in one of them checks the defines, such as `#ifdef HAYLEN_LIT`. Lit canvases light the draws of a material like any other draw, with its `unshaded`, `emission`, `lightMask` and layer. They use a flat normal unless the material writes its own with `haylen_output_surface`.

The library keeps the names that start with `haylen_`, the names `uv` and `color`, and the outputs `frag_color`, `frag_emission`, `frag_surface` and `frag_info`. The compiler rejects a uniform or texture that repeats a name of the program. The shaders of the engine in `engine/shaders` include the same library, so a material draws exactly where the program of the engine would.

## Sprite data

Each sprite of a [sprite batch](../lua-api/graphics2d.md#batches-of-several-textures) carries four numbers of its own, its `data`. A material reads them as `haylen_data` in the `batch` programs. So sprites that share one material and one draw call still sway, glow or tint on their own, instead of each one taking a draw call with its own uniform values. The other programs read zeros, so the same source shades every kind of draw. A batch draws with the `batch` programs when it has several textures, or data and a material.

```glsl
// The file `content/shaders/sway.glsl`.
@include haylen/material.glsl

@fs sway_fs
@include_block haylen_fragment
layout(binding=1) uniform sway_params {
    float time;
};

void main() {
    float height = (uv.y - haylen_source.y) / max(haylen_source.w - haylen_source.y, 0.0001);
    float bend = sin(time * 2.0 + haylen_data.x) * haylen_data.y * (1.0 - height);
    vec4 base = haylen_base(uv + vec2(bend * (haylen_source.z - haylen_source.x), 0.0));
    haylen_output(vec4(base.rgb * (1.0 + haylen_data.z), base.a));
}
@end

@program sway haylen_vs sway_fs
```

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local sway = graphics2d.newMaterial(assets.shader('shaders/sway.shader'))
local grass = graphics2d.newSpriteBatch(assets.texture('meadow/grass.png'))
for index = 1, 400 do
    grass:add({x = math.random(0, 1920), y = math.random(0, 1080), pivotY = 1, data = {math.random() * 6.28, 0.08, math.random() * 0.2}})
end

scene.push({
    render = function(self)
        sway:set('time', haylen.elapsed())
        graphics2d.beginScreen()
        grass:draw({material = sway})
    end,
})
```

## Uniforms and textures

A material fills the uniform blocks and textures that the fragment shader declares, by these rules:

- Uniform blocks take the bindings 1 to 6. Binding 0 holds the view projection of the vertex shader and whether the blend of the draw needs premultiplied colors, and binding 7 holds the lighting of lit canvases. The renderer fills both with the layout of the shader library it was built with. So a shader compiled with another version of the library raises `The shader "<name>" was compiled with another version of the shader library. Compile it again with "haylen.py shaders".` when a draw first uses it.
- Members are `float`, `vec2`, `vec3`, `vec4`, `int`, `ivec2`, `ivec3`, `ivec4` and `mat4`, and arrays of any of them. The std140 rules give every element of an array a whole vector of 16 bytes, so an array of `float` takes 16 bytes an element. The engine places the numbers that a material sets for an array at those strides.
- A block holds floats or integers, not both, because the GLSL targets read each block as one array of `vec4` or `ivec4`. So integer uniforms go in a block of their own.
- Textures take the bindings from 1 to 24, next to `sprite_texture` at 0, with samplers of their own from binding 1 up or with `sprite_sampler`. The bindings 25 to 31 hold the textures of a batch after its first, in the `batch` programs.
- The sampler of a texture binding is the one the texture was created with, so a texture loaded with `filter = 'linear'` samples smoothly.
- A texture that a material leaves unset draws as white.

The method `material:set(name, value)` takes the value that fits the type, as the [`Material` reference](../lua-api/graphics2d.md#materialsetname-value) lists. A draw copies the values of its material when it is made. The properties `shader.uniforms` and `shader.textures` list what a shader offers.

```glsl
// The file `content/shaders/palette.glsl`.
@include haylen/material.glsl

@fs palette_fs
@include_block haylen_fragment
layout(binding=1) uniform palette_params {
    vec4 colors[4];
    mat4 warp;
};
layout(binding=2) uniform palette_steps {
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

The engine already offers bloom, blur, chromatic aberration, pixelation, distortion, a color lookup texture, the vignette and the other adjustments of `postProcess` without a shader, as the [rendering guide](rendering.md#post-processing) explains. Materials add the passes it lacks.

The `materials` of the `postProcess` of a world or render target canvas run after the composite and the other adjustments, in order. Each one draws the image of the step before over the whole canvas, as `sprite_texture`, with `uv` from 0 at the top-left corner to 1 at the bottom-right one. The last one draws into the destination of the canvas. The sprite program of the material runs these passes, so a post-processing shader reads the image with `haylen_texture(uv)` and writes it with `haylen_output`.

```glsl
// The file `content/shaders/scanlines.glsl`.
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

## Game shaders

The test project keeps a library of forty-five material shaders of games under `samples/tests/content/shaders/library/`. Test `SHD-008` shows them by category, with a slider for each of their numbers. Each one is an ordinary source that an app copies into its own `content/shaders/` together with `common.glsl` and compiles with `haylen.py shaders`. The file `common.glsl` holds the hashes, noise and helpers they share. Among them, `library_glow` spreads the coverage of a shape like the bloom of a bright light, for the glows of neon, plasma and teleports.

| Category | Shaders | Drawn on |
| --- | --- | --- |
| Neon and energy | Neon glow, neon trail, laser beam, plasma bolt, electric arc, energy shield, charge fill, neon sign, teleport | Bullets with a transparent margin for their glow, a trail for the neon trail, a straight beam that sweeps for the laser and the lightning, a square for the shield, and sprites. |
| Elements | Fire, water ripple, lava, frozen, poison, burn away | Rectangles of the white texture for fire and lava, a picture, and sprites. |
| Status and looks | Stone, gold, shine sweep, ghost, invisibility, silhouette, damage blink, drop shadow, inner glow, rim light, spectrum, gradient map | Sprites. |
| Styles | Posterize, sepia, night vision, thermal, X-ray, dither, halftone, sketch, emboss, color split, glitch, VHS | Pictures. |
| Motion and deformation | Flag wave, jelly, swirl, shockwave, heat haze, melt | Pictures and sprites. |

These shaders follow a few rules that help any shader of an app:

- They measure the draw through `haylen_source`, as a share from 0 to 1 of its source rectangle on each axis. So they shade a region of an atlas like a whole texture.
- A stroke draws as a mesh. With the default `textureMode` of `'stretch'`, its texture coordinates go from 0 to 1 along its length and from 0 to 1 across its width. The trail shaders read these.
- The shaders that reach past the pixels of a sprite, such as the glows, outlines and drop shadow, draw inside its quad. So the image needs a transparent margin as wide as what they draw.
- The ones that read the pixels around them take a `texel` uniform, the size of a pixel of the texture. The app sets it from the width and height of the texture.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local hero = assets.texture('sprites/hero.png', {filter = 'linear'})
local glow = graphics2d.newMaterial(assets.shader('shaders/inner-glow.shader'), {texel = {1 / hero.width, 1 / hero.height}})
```

## The .shader file

A `.shader` file is a Haylen shader package: compact JSON that `haylen-shaderc` writes and nothing else edits, in version 2 of the [package format](https://github.com/haylen-org/haylen-shader-compiler/blob/main/docs/format.md).

| Key | Meaning |
| --- | --- |
| `format`, `version` | `"haylen-shader"` and `2`. |
| `name` | The name of the source file without its folder and its extension. |
| `sources` | The source of every stage of every program for every target, each text once. |
| `programs` | Each of the ten programs under its name, with its `attributes`, its uniform `blocks` with their `members`, its `textures`, its `samplers`, the texture sampler `pairs` that its stages sample with, the number of color `outputs`, and its `targets`. The members have names, types, element counts and byte offsets that follow the std140 rules. For each of `msl-macos`, `msl-ios`, `msl-ios-simulator`, `hlsl5`, `glsl430`, `glsl300es`, `wgsl` and `spirv`, the targets give the source and entry point of each stage and the binding of each uniform block, texture and sampler in that language. The `spirv` target holds each module as the Base64 text of its words and gives each binding with its descriptor set: the uniform blocks in set 0 at the binding of their slot, the textures in set 1 at the binding of their slot and the samplers in set 1 at 64 plus their slot. |

The engine reads the file on a worker thread. The first time a draw uses a program, the engine creates it on the frame thread for the shader language of the GPU device. It creates the pipelines for each blend mode and kind of target the same way. As it reads the file, the engine hands the sources of that language of every program to the GPU device. On Metal the device starts compiling them on the I/O pool, as [load time](#load-time) explains.

The shaders of the engine are packages of the same format. The engine compiles them when it builds, and embeds only the shader languages of the graphics APIs of the build. The shaders of apps carry all eight targets, so one package runs on every platform.

Reading the file also checks it, so a damaged or hand-edited file fails at once instead of when a draw uses it. The file must follow these rules:

- It is a version 2 package.
- Every attribute, uniform block, texture and sampler fits the binding slots of every GPU device, and no two uniform blocks, textures or samplers of a program share a slot. Every binding number is in range.
- Every uniform block takes a multiple of 16 bytes, and every uniform of a block fits inside it.
- Every stage names a source of the file, and every pair names a texture and a sampler that its program declares.
- A program writes one to four colors.
- The names of stages, texture types, sample types, sampler types and targets are ones the compiler writes.
- The GLSL targets read each block as an array of `vec4` or `ivec4` that fills it exactly.
- Every program reads each block of the material with the slot and size that the file gives it.

A file that breaks a rule raises `The shader file is malformed: ` followed by the problem, from `assets.shader` or from the hot reload, which keeps the last good shader. Examples of problems are `It is not a version 2 Haylen shader package. Compile its source again with the Haylen shader compiler.`, `The texture slot 40 is out of range.` and `The uniform "tint" does not fit in the block "params".`.

Two errors come when a draw first uses a program. A program without code for the language of the GPU device raises `The program "<name>/<program>" has no code for the "<language>" language of this GPU device. Compile its shader again with the Haylen shader compiler.`. A GPU program that the graphics API rejects raises `The graphics device could not create the shader "<name>/<program>".`.

```lua
local assets = require('haylen.assets')
local async = require('async')

async.spawn(function()
    local shader, failure = assets.loadAsync('shaders/ripple.shader'):await()
    print(shader and shader.name or failure)
end)
```

## Load time

A shader costs time in three steps:

1. Reading its `.shader` file.
2. Compiling the program for the shader language of the GPU device.
3. Making a pipeline for each blend mode and kind of target it draws with.

Reading a file of the shader samples took about 2 ms in a Release build, 7 ms for the first one, on a worker thread when the shader loads asynchronously. The debug log names every program and pipeline with the milliseconds it took, such as `The graphics device made the shader "ripple/sprite" in 0.10 ms.`, once `log.setLevel('debug')` shows debug lines. These are the costs measured with the seven materials of the shader samples on an Apple M5 Pro:

| Graphics API | Compiling a program the first time | Compiling it again | Making a pipeline |
| --- | --- | --- | --- |
| Metal (macOS, iOS, tvOS) | 220 ms, 110 ms per stage, which the system keeps in its compiler cache across runs | 0.1 ms | 5 to 8 ms the first time, 43 ms for the very first pipeline of the process |
| WebGPU (Chrome) | Under 0.1 ms on the frame thread, since the browser compiles in its GPU process | Under 0.1 ms | Under 0.6 ms |
| WebGL2 (Chrome on Metal) | 15 to 60 ms, compiled and linked on the frame thread when the program is made | The same | None, since WebGL2 links the program with its state |

Metal compiles the source of a program when the program is made, and seven programs made one after another wait 2.3 seconds in all. So the engine starts compiling every source of the language of the GPU device on the I/O pool as soon as a `.shader` file is read, on whatever thread reads it. The renderer does the same for its own programs when it starts, and makes each one the first time a draw needs it. The system compiles the requests side by side and keeps each result, so a program made while its sources still compile waits only for what is left. Making the seven programs right after their files were read takes 315 ms. The compile overlaps whatever the app does until its first draw, such as the rest of its loading.

A scene that loads its shaders in its `load` hook, with `assets.load`, `assets.loadAsync` or a [preload group](../lua-api/assets.md#preload-groups), lets them compile while it loads and its transition plays. On WebGL2 a program compiles when the first draw needs it, 15 to 60 ms each in Chrome and 224 ms for the 13 shaders of the test project. So a scene with many materials draws them once while it loads, such as behind its transition, to compile them before it shows.

```lua
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local level = {}

function level:load(context)
    -- The shaders compile on the I/O pool while the scene loads and its transition plays.
    self.ripple = graphics2d.newMaterial(assets.loadAsync('shaders/ripple.shader'):await())
    self.sway = graphics2d.newMaterial(assets.loadAsync('shaders/sway.shader'):await())
end

return level
```

## C++

In C++, `assets::Manager::shader(path)` loads a `graphics::Shader` (`engine/include/haylen/graphics/Shader.hpp`). A `graphics2d::Material` (`engine/include/haylen/2d/graphics/Material.hpp`) sets its values and goes into the `material` of a `graphics2d::DrawOrder` or the `materials` of a `graphics2d::PostProcess`:

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
