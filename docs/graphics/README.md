# 🎨 Graphics

Graphics draws everything an app shows: the GPU device and its graphics APIs, textures and render targets, the 2D renderer with its canvases, cameras and batching, shaders, text in every script and the mapping of design units to the screen.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| Direct3D | The devices of Direct3D 12 and Direct3D 11 on Windows: what the machine needs, how each one draws a frame, swapchains, transparent windows, shaders, capabilities, debugging, lost devices and limits. | [`direct3d.md`](direct3d.md) |
| Rendering | How a frame is drawn: the GPU device of the backend, canvases and their order, design units and the viewport, cameras, draw order and blend modes, batching and what breaks a batch, smooth edges, sprites and batches, captures, groups, masks, lit canvases, post-processing, effects, the GPU pools and performance. | [`rendering.md`](rendering.md) |
| Shaders | Fragment shaders of an app: the Haylen shader language, compiling with `haylen-shaderc` for every graphics API, the shader library, sprite data, uniforms and textures, post-processing, the game shaders of the test project, the shader package and load time. | [`shaders.md`](shaders.md) |
| Text | Fonts, bitmap fonts and font families, how glyphs are made and drawn, shaping, directions and line breaking, layout and fitting, rich text with its markup, effects and typewriter reveal, and text in the UI. | [`text.md`](text.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.graphics` | The graphics API and the texture limit, textures from pixels, colors, images or image files, render targets, images on the CPU with their effects, gradients, palettes and placeholders, pixels read back from the GPU, TrueType and bitmap fonts, font families, shaders and vector images. | [`lua-api/graphics.md`](../lua-api/graphics.md) |
| `haylen.graphics2d` | Canvases with their options and post-processing, captures, draw order and blend modes, cameras, camera rigs, pixel canvases and parallax layers, sprites with effects, recoloring and distortion, sprite batches, static batches and tile layers, shapes, lines and strokes, meshes, text and rich text, nine-slices, vector images, groups, masks, image blends, metaballs, lights, materials and the statistics of the renderer. | [`lua-api/graphics2d.md`](../lua-api/graphics2d.md) |
| `haylen.video` | Video files of the package played into a texture with the decoder of each platform, with play, pause, seek, loop, rate, sound through a bus of the mixer, events and asynchronous loads. | [`lua-api/video.md`](../lua-api/video.md) |
| `haylen.viewport` | The design size and the scaling policy, which change while the app runs, the visible area, the safe area, the edges that native views reserve, conversions between design units and pixels, and the simulated safe area of other devices. | [`lua-api/viewport.md`](../lua-api/viewport.md) |
