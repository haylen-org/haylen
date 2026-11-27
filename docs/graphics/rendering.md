# 🖌️ Rendering

This guide covers what every renderer of Haylen draws through: the GPU device of each graphics API, the pixel density of the window, textures with their filters, mipmaps and compressed formats, render targets, reading pixels back and the pools of GPU objects. The renderer of 2D, with its canvases, cameras, batching and effects, has the [2D graphics guide](../2d/graphics.md), and the [`haylen.graphics` reference](../lua-api/graphics.md) lists every function of textures and render targets.

## The GPU device

The engine never talks to a graphics library directly. It draws through `gpu::Device` (`engine/include/haylen/gpu/Device.hpp`), the render hardware interface of Haylen, with buffers, images, samplers, programs, pipelines, passes and draws. A device of each graphics API implements it, and a build carries the device of every graphics API of its platform. The renderer, the Lua API and apps look the same on every device. The [RHI guide](rhi.md) describes the contract, the capabilities of each API, the order in which the window tries them and the status of every device, and the pages of [Metal](metal.md), [Direct3D](direct3d.md), [Vulkan](vulkan.md), [OpenGL](opengl.md) and [WebGPU](webgpu.md) describe each device.

The class `graphics::Device` (`engine/src/graphics/Device.cpp`) owns the GPU device that the host creates. It creates textures, single-channel alpha textures for font atlases and render targets on it. Every method runs on the frame thread, and one engine runs at a time, as the [architecture guide](../core/architecture.md#resource-ownership) explains.

Two layers check the GPU calls. With validation on, which debug builds and the tests turn on, each device turns on the validation of its graphics API and sends its messages to the log. The null device of the headless host checks every call against the contract of the GPU device in every build. The null device also reports the texture limits of desktop GPUs, 16384 pixels for 2D textures, so headless runs load full-size art like real devices.

The function `graphics.apiName()`, `graphics::Device::getApiName()` in C++, names the graphics API the device draws through, such as `'metal'`, `'vulkan'` or `'webGpu'`, with the names of `gpu::Device::kApiNames`. The headless host of the tests reports `'none'`, and the device of a [port](../platform/ports.md) reports `'port'`. Some graphics APIs store the bottom row of a render target first. The engine corrects that on its own, so the texture of a render target samples upright everywhere.

```lua
local graphics = require('haylen.graphics')
local log = require('haylen.log')

log.info('This device draws through ' .. graphics.apiName() .. '.')
```

## Pixel density

A dense display draws many framebuffer pixels in each window point: 2 on a Retina Mac, 3 on many phones, and 2 or more on a 4K monitor. Every pixel costs fill rate, so a weak GPU, a phone that runs hot or a page in a browser may spend most of its frame filling pixels that the eye hardly tells apart. The cap of the pixel density, `window.maxPixelDensity` of `haylen.json` or [`window.setMaxPixelDensity`](../lua-api/window.md#windowsetmaxpixeldensitycap), limits the framebuffer pixels in each point. On a denser display the framebuffer shrinks to the cap and is stretched over the window with a linear filter, so the window keeps its size and every size in points and design units stays the same.

The cap trades sharpness for fill rate. Curves keep their fade of one pixel, which is now a pixel of the smaller framebuffer, so edges look softer once stretched, and text and art lose the detail that the display could show. A cap of `2` is hard to tell from the full density on most phones and fills less than half the pixels of a screen of 3 pixels in each point. A cap of `1` draws one pixel in each point, as on a display of standard density.

Each platform stretches the framebuffer where the system can, at no cost to the app:

| Platform and device | Who stretches the framebuffer |
| --- | --- |
| Apple platforms with Metal | The `CAMetalLayer` of the window, whose drawable takes the size of the framebuffer. |
| Windows with Direct3D 12 or 11 | The swapchain of DXGI, whose buffers take the size of the framebuffer, and DirectComposition for a transparent window. |
| Android with Vulkan or OpenGL ES | The native window, whose buffers take the size of the framebuffer. |
| Windows and Linux with Vulkan or OpenGL, and the web with WebGPU or WebGL2 | The GPU device, since the swapchain or the canvas keeps the size of the window. The frame draws into an image of the size of the framebuffer, which the device copies over the window at the end of the frame, a cost of one pass over the pixels of the window. |

The [RHI guide](rhi.md#surfaces) explains how a surface hands the device both sizes. Outputs on other displays always draw every pixel of their display, and captures, render targets and readbacks keep the sizes they ask for.

## Textures and atlases

A texture made with the `linear` filter samples the pixels around each point. So the color of the transparent pixels next to what shows bleeds into its edges, often as a dark or bright fringe. To stop this, every image for a linear texture is bled when it decodes on the worker pool. The transparent pixels around what shows take the color of the nearest pixel that shows. This reaches up to `graphics::Image::kBleedReach` pixels and keeps their alpha at zero.

The texture option `mipmaps` keeps smaller copies of the image, down to one pixel, made when it decodes. A texture drawn smaller than its size samples those copies instead of skipping pixels. The copies average each block of pixels by alpha, so transparent pixels never darken them. A texture with the `linear` filter keeps them by default, because art painted in high definition is often drawn far below its size, such as a character sheet of 256 pixel cells drawn 50 pixels tall, a ship on a map or the thin end of a textured ribbon. Without the smaller copies, each pixel of the screen picks one texel of the full image, so edges turn jagged, thin lines break into dots and fine detail shimmers as it moves. The copies take a third more memory than the image and upload with it. A texture loaded with `assets.texture` makes them on the worker that decodes it, so they cost the frame nothing, while a texture made from an `Image` with `graphics.newTexture` makes them when it is made. A texture with mipmaps and the `linear` filter also filters anisotropically: a pixel that covers many texels along one side and few along the other takes up to the most samples that the device has along the longer side, 16 on almost every GPU, and reads the level of detail of the shorter one. So a texture drawn much smaller in one direction than the other, such as the middle of a nine-slice squeezed into a narrow button, keeps the detail of its other direction, where one sample would blur it to the level of the longer side. A driver whose anisotropic filtering is broken filters without it, as the [driver issues](rhi.md#driver-issues) list. Pixel art keeps the `nearest` filter and no mipmaps by default, so its pixels stay hard. The option `mipmaps = false` turns them off for a linear texture that never draws smaller than its size, such as a background drawn at its own size. Render targets have no mipmaps, and a texture that changes in place keeps them only with `mipmaps = true`, which the GPU generates from its new pixels after each update.

An image file can come from anywhere, such as a picture that a person uploads or an avatar that the app downloads, so the engine treats its bytes as hostile. The function `graphics::Image::decode` reads the size in the header of the file before it allocates a pixel. It refuses a side longer than `Image::kMaxSide`, 16384 pixels, or than the smaller side the caller names, which is the option `maxSize` of `graphics.decodeImage` in Lua. A file of a few bytes that declares a huge image therefore fails at once, without taking the memory of one. The decoder also caps every buffer it allocates at what an image of the declared size needs, so a compressed stream that inflates past its image fails instead of growing a buffer until memory runs out. The decoder reads PNG, JPEG, WebP, TGA, BMP and GIF files and nothing else, and the image keeps the pixels the decoder wrote, without a copy. Fuzzers feed the decoders and the SVG reader with generated input, as the [testing guide](../tools/testing.md#fuzzing) describes.

WebP images decode through the decoder of libwebp on every platform, the web included, lossy, lossless and with alpha, with the same checks of their size before a pixel is allocated. Animated WebP images are not decoded: one fails with a sentence that says to save it as a still image or to play its frames as a sprite sheet, since an image of the engine holds one frame. AVIF images are not read either, and one fails with a sentence that names its format and says to save it as a PNG, JPEG or WebP image.

```lua
local async = require('async')
local graphics = require('haylen.graphics')
local http = require('http')

async.spawn(function()
    local response = http.client.get('https://example.com/avatar.png'):await()
    local avatar, failure = graphics.decodeImageAsync(response.body, {maxSize = 512}):await()
    print(avatar and avatar.width or failure)
end)
```

Modest phones of AWS Device Farm measure what the textures of the engine cost there, the PNG and JPEG files that apps ship. Two sets load as textures with the `linear` filter and their mipmaps: the 51 PNG images of Tiny Island, 6.6 MB in the package with 13.0 million pixels and 69.3 MB of texture memory as RGBA8 with mipmaps, and the 12 JPEG photos of Photo Social, 3.4 MB with 7.1 million pixels and 37.7 MB. An app loads every image of a set at once with `assets.loadAsync`, as a scene loads its art, three times, and the time of a set runs from the first request until every texture is ready. The first load reads cold files and the later ones read files that the system keeps in memory. Reading and decoding alone, every file of a set at once through `assets.bytesAsync` and `graphics.decodeImageAsync`, shows how much of the load is the work of the worker threads, and the rest is the mipmaps and the uploads within the upload budget of each frame. The longest frame tells how much a load holds up the frames, which the upload budget bounds.

| Phone | GPU and API | PNG load, first and then | PNG read and decode | JPEG load, first and then | JPEG read and decode | Longest frame |
| --- | --- | --- | --- | --- | --- | --- |
| Google Pixel 3a, Android 10 | Qualcomm Adreno 615, Vulkan | 411 ms, 304 ms | 37 ms and 96 ms | 203 ms, 236 ms | 35 ms and 98 ms | 24 to 123 ms |
| Samsung Galaxy A14 5G, Android 13 | ARM Mali-G57 MC2, Vulkan | 472 ms, 243 ms | 46 ms and 121 ms | 178 ms, 169 ms | 23 ms and 63 ms | 16 to 220 ms |
| Samsung Galaxy Tab A7 Lite, Android 13 | Imagination PowerVR GE8320, OpenGL ES | 1,149 ms, 771 ms | 117 ms and 152 ms | 469 ms, 392 ms | 36 ms and 88 ms | 38 to 211 ms |

The texture memory comes from the graphics memory of the process that `dumpsys meminfo` reports while the textures live and after they are gone. On the Tab A7 Lite it grows by 73.2 MB for the PNG set and by 38.2 MB for the JPEG set, close to the 69.3 MB and 37.7 MB of RGBA8 with mipmaps, and it falls back once the textures are released. On the Galaxy A14 5G the Vulkan device keeps the memory blocks it allocated for the first set, so the graphics memory of the process grows by 202 MB with the first load and stays there for every later load of either set. The Adreno driver of the Pixel 3a reports no graphics memory of Vulkan to `dumpsys`.

Separate textures split batches, as the [render benchmark](../2d/graphics.md#render-benchmark) shows. The class `animation2d::AtlasBuilder`, `animation2d.newAtlasBuilder` in Lua, packs images, areas of images and the frames of whole atlases with their animations into one texture while the app runs:

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

## Compressed textures

A block compressed texture holds blocks of pixels, such as 4 by 4 pixels in 16 bytes, which the GPU decodes as it samples them. So it takes a quarter of the memory of RGBA8 or less, and each frame reads less memory. Each family of GPUs reads its own formats: desktop GPUs read BC, mobile GPUs read ETC2 and ASTC, Apple GPUs read ETC2 and ASTC and, on every Mac and on the iPads of the M1 on, BC, and browsers offer each family only where the GPU under them has it. The function [`graphics.formatSupport`](../lua-api/graphics.md#graphicsformatsupportformat) tells which formats this device samples, and the [RHI guide](rhi.md#pixel-formats) lists them for each graphics API.

The art of the sample Tiny Island shows what they bring. Its 51 images hold 13 million pixels, which take 69 MB of texture memory as RGBA8 with mipmaps and about 17 MB as ASTC 4x4 or BC7.

The call `graphics.newTexture(width, height, {format = 'bc7', levels = {...}})` makes a texture from the blocks of each mip level as they are, the largest level first. The width and the height hold whole blocks, and a level smaller than a block takes one whole block. A device that lacks the format refuses the texture with a sentence that names the format and the graphics API. The engine never decodes the blocks on the CPU in its place, since that would take the memory and the time that the format saves and would hide that the app shipped a format its target cannot read. So an app ships each texture in a format of every target it runs on, such as BC7 for desktops and ASTC for phones, and picks the one that `graphics.formatSupport` reports. A compressed texture holds no pixels that the CPU reads, so the atlas builder and the mipmaps that average by alpha work on uncompressed images, and a compressed texture brings its mipmaps as levels.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local format = graphics.formatSupport('astc4x4').sample and 'astc4x4' or 'bc7'
local blocks = assets.bytes('textures/stone-' .. format .. '.bin')
local stone = graphics.newTexture(256, 256, {format = format, levels = {blocks}, filter = 'linear'})
print(stone.format, stone.width)
```

## Render targets

A render target, `graphics::RenderTarget`, is an image on the GPU that passes draw into and draws sample later, which `graphics.newRenderTarget` makes and `graphics::Device::createRenderTarget` in C++. Besides the filter and wrap of its textures, its options name its attachments, `graphics::RenderTarget::Options` in C++:

- Up to four colors, each in a pixel format that the device draws into and filters, such as `rgba16Float` for light brighter than white. Each color has its texture, and the first one is `target.texture`.
- An optional depth, with or without stencil, which a renderer of 3D tests its draws against. With `depthTexture` the target keeps it as a texture that a material reads through a texture of the sample type `depth`, such as the depth of a scene for fog or soft particles.
- Several samples for each pixel, which give smooth edges to everything the target draws. Each pass draws into the samples and resolves them into the textures of one sample when it ends. A kept depth of several samples resolves by `depthResolve`: the depth of the first sample, the nearest of them or the farthest of them, as the device offers.
- `memoryless`, which keeps the samples and a depth without a texture only inside each pass. Phones and Apple silicon Macs draw a pass in tiles of fast memory on the chip, so on them such attachments take no memory at all, and the samples of a target of 1920 by 1080 pixels at four samples save about 33 MB. Elsewhere they are ordinary images that each pass clears.

A canvas draws into the first color of its target, into its samples when it has several, and leaves its other colors and its depth as they are, since the draws of 2D write one color and test no depth. So the first color takes a format that the device blends into. The pipelines of the renderer take the format and the sample count of the pass they draw in, and the renderer makes them the first time a pass of that layout needs them. A glass reads what its canvas drew before it, so it never draws into a memoryless target of several samples, whose samples a pass loses as it ends.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local counts = graphics.formatSupport('rgba8').sampleCounts
local map = graphics.newRenderTarget(256, 256, {filter = 'linear', samples = counts[#counts], memoryless = true})
local camera = graphics2d.newCamera()
camera.x = 128
camera.y = 128

scene.push({
    render = function(self)
        graphics2d.beginTarget(map, camera, {clear = '#FF10202A'})
        graphics2d.drawCircle(128, 128, 90, '#FFFFD040')

        graphics2d.beginScreen()
        graphics2d.draw(map.texture, 20, 20, {pivotX = 0, pivotY = 0})
    end,
})
```

The [RHI guide](rhi.md#attachments) explains how each graphics API resolves depth and keeps attachments in its tiles.

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
