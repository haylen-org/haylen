# 🎨 Graphics

Graphics draws everything an app shows: the GPU device and its graphics APIs, textures and render targets, the 2D renderer with its canvases, cameras and batching, shaders, text in every script and the mapping of design units to the screen.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| Rendering | The renderer: canvases, captures, scene transitions, draw order, batching, shapes with smooth edges, sprites, node trees, lighting, shaders, post-processing, text, nine-slices, vector images, groups, masks and performance. | [`rendering.md`](rendering.md) |
| Shaders | Fragment shaders of an app: the Haylen shader language, compiling for every graphics API, the shader package, the shader library, uniforms, textures, post-processing and the game shaders of the test project. | [`shaders.md`](shaders.md) |
| Text | Fonts, font families and bitmap fonts, shaping, directions and line breaking, rich text with its markup, effects and typewriter reveal. | [`text.md`](text.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.graphics` | Textures from pixels, colors, images or the bytes of image files, with mipmaps, images on the CPU that decode and blit off the frame thread, render targets and whole frames read back without waiting for the GPU, PNG files, render targets, TrueType and bitmap fonts, font families, custom shaders and the graphics API. | [`lua-api/graphics.md`](../lua-api/graphics.md) |
| `haylen.graphics2d` | Canvases, captures, draw order with y sorting and visibility, cameras with viewports, smoothing, drag margins and shake, camera rigs with zones and rails, pixel canvases that scroll smoothly, parallax layers, sprites, sprite batches, tile layers baked by chunks, strokes with joins, caps, textures, widths and dashes, kept meshes that deform in place, groups that fade as one image, masks of any shape, shapes, meshes, text shaped in every script and ordered in both directions, rich text with effects and a typewriter reveal, nine-slices, image blends, metaballs, materials with custom shaders, lit canvases with normal maps, lights and occluders. | [`lua-api/graphics2d.md`](../lua-api/graphics2d.md) |
| `haylen.video` | Video files of the package played into a texture with the decoder of each platform, with play, pause, seek, loop, rate, sound through a bus of the mixer, events and asynchronous loads. | [`lua-api/video.md`](../lua-api/video.md) |
| `haylen.viewport` | The design resolution and scaling policy, which change while the app runs, the visible area, the safe area and the edges that native views reserve. | [`lua-api/viewport.md`](../lua-api/viewport.md) |
