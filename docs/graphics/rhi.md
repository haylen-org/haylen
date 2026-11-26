# 🧱 Render hardware interface

This guide explains the render hardware interface of Haylen, the RHI. It covers the layers between a Lua script and the GPU, how a frame flows through them, the contract of `gpu::Device` with its objects, passes, uploads, readbacks and lifetimes, surfaces and lost devices, capabilities, validation and profiling, what each graphics API can do, how an app chooses its API, the driver issues that the devices work around, and the status of every device on every platform. It is for people who work on the engine, write a device or port the engine to a new platform. App developers never touch the RHI, and the [rendering guide](rendering.md) covers what they draw.

## Layers

The engine draws through five layers. Each layer talks only to the one under it.

```text
Lua scripts and C++ apps          draw sprites, shapes, text and the UI
        |
Renderer2D                        graphics2d::Renderer, a future Renderer3D next to it
        |
graphics::Device                  textures, render targets, shaders and readbacks
        |
gpu::Device                       the RHI contract, one device per graphics API
        |
platform layer                    the window, the frames, the input and gpu::Surface
```

| Layer | Where | What it does |
| --- | --- | --- |
| Scripts and apps | `haylen.graphics2d`, `haylen.ui` and the C++ API | Record draws into canvases. They see design units, colors, textures and materials, never a graphics API. |
| Renderer2D | `engine/src/2d/graphics/` | Sorts and batches the draws of a frame, uploads their data once and turns them into passes and draws of the device. |
| Renderer3D | A future `engine/src/3d/graphics/` | Does not exist yet. It will draw through the same `graphics::Device` and `gpu::Device` as the 2D renderer, so both share textures, render targets and the window. |
| `graphics::Device` | `engine/src/graphics/Device.cpp` | Owns the GPU device of the app. It makes textures, font atlases and render targets, keeps the graveyard of released objects and reads render targets back for the renderers of every dimension. |
| `gpu::Device` | `engine/include/haylen/gpu/Device.hpp` | The RHI. Each graphics API has a device in `engine/src/gpu/<api>/` that implements it with the native API alone. |
| Platform layer | `engine/src/platform/sdl/`, `engine/src/platform/worker/`, `engine/src/platform/android/` | Opens the window, hands the device a `gpu::Surface` with the native handles of the window, runs the frames and tells the device when the window changes size, goes away or comes back. |

The [architecture guide](../core/architecture.md#layers) describes the platform layers, the player and the host around them.

### How a frame flows

1. The platform layer starts a frame and calls `Player::frame`, which runs `Engine::frame`.
2. Scripts and plugins record draws in `render` and `renderUi`. The renderer turns each draw into vertices or instances at once.
3. `Renderer::endFrame` sorts the canvases, merges draws into batches and writes the data of the whole frame with `writeBuffer`. It sends the region that changed of every changed dynamic texture in one `writeImages` call.
4. The renderer records its passes: offscreen passes for render targets, lights and effects first, then the pass into the swapchain. Each pass applies pipelines, bindings and uniforms and draws.
5. `commit` submits the frame to the GPU and presents the swapchain.
6. `graphics::Device::collectGarbage` hands the objects released during the frame to `destroy`, and `Device::processReads` starts the readbacks that scripts asked for.
7. `Player::frame` ends with `waitForDisplay`, which returns once the displays can take another frame, and only then does the platform read the events of the next frame, as [frame pacing](#frame-pacing) describes.

### Why scripts never see the API

A script calls functions such as `graphics2d.drawCircle` and `graphics.newRenderTarget`. Those functions take colors, sizes and design units, and the renderer decides what the GPU does. The shader packages carry the code of every shader language the build draws with, so a material works on every device. The renderer reads `gpu::Device::Capabilities` instead of the API, so a missing feature changes how the renderer draws, never what the script calls. The only trace of the API in Lua is its name in [`haylen.graphicsApi`](../lua-api/haylen.md#haylengraphicsapi), which a script may log or show.

```lua
local haylen = require('haylen')
local log = require('haylen.log')

log.info('This frame draws through ' .. haylen.graphicsApi .. '.')
```

## Objects

A device makes five kinds of objects. Each `make` method returns a small handle, a struct with a 64-bit `id`, and `id` 0 names nothing. Every device keeps its objects in a `gpu::HandleTable`, whose ids hold the slot of an object in 24 bits and the generation of the slot in 40 bits, so the handle of a destroyed object never finds the object that takes its place later, however often the slot is taken again.

| Object | Made with | What it is |
| --- | --- | --- |
| `Buffer` | `makeBuffer(BufferDesc)` | Bytes with the usages they serve, such as vertices, indices or uniform blocks, and an update that says how they change, as [buffers](#buffers) explain. |
| `Image` | `makeImage(ImageDesc)` | A flat image, an array, a cube or a volume, with mip levels and a pixel format. Its kind is a texture, a dynamic image, a render target or a depth stencil image. |
| `Sampler` | `makeSampler(SamplerDesc)` | Filters, a wrap for each axis with a border color, levels of detail with a bias, anisotropy and an optional compare function. |
| `Program` | `makeProgram(ProgramDesc)` | The vertex and fragment stages in the language of the device, with the bindings that the reflection of the shader package gives. |
| `Pipeline` | `makePipeline(PipelineDesc)` | A program with its vertex layout, primitive, index type, blend and write mask for each color target, depth and stencil tests, culling and sample count. A pipeline without color targets draws depth alone, such as into a shadow map. |

A device makes the native state object of a pipeline once, in `makePipeline`, and never makes one while it draws. A pipeline that draws into the swapchain takes the formats and the sample count of the swapchain, so the renderer never needs to know them.

Programs that the API compiles when they are made, such as Metal and Direct3D, start compiling early through `prepareProgram`. The engine calls it as soon as it knows a program, from any thread, and `makeProgram` later takes the result or waits only for what is left. The Metal device loads the programs of the renderer from the Metal libraries that the Apple build precompiled, and compiles only the source of the other programs, such as the shaders of the app.

```cpp
const gpu::Device::Buffer quad = device.makeBuffer({.data = std::as_bytes(std::span(vertices)), .usage = {.vertex = true}, .label = "quad"});
const gpu::Device::Image target = device.makeImage({.width = 256, .height = 256, .kind = gpu::Device::ImageKind::RenderTarget, .label = "minimap"});
```

## Buffers

A `BufferDesc` names the size of a buffer, or its first data, the usages it serves and how its contents change. The usages of `BufferUsage` are `vertex`, `index`, `uniform`, `storage`, `indirect`, `copySource` and `copyDestination`, and a buffer has at least one. Storage that shaders read and write needs `Capabilities::storageBuffers`, and the arguments of indirect draws need `Capabilities::indirectBuffers`. The contract keeps to what the strictest graphics API allows, so every device refuses the same buffers and a renderer that runs on one runs on all of them:

- A uniform buffer is no vertex, index, storage or indirect buffer, since Direct3D 11 binds a constant buffer as nothing else.
- An index buffer is no vertex buffer, since WebGL2 binds a buffer of indices to no other target than the copies.
- A stream buffer is no copy source or destination, since each frame holds its own copy of it on Metal, Vulkan and Direct3D 11, which no copy between frames could name.
- An immutable buffer is no copy destination, since Direct3D 11 makes an immutable buffer that nothing writes after it was made.
- A uniform block reads either a uniform buffer of the bindings or the uniforms of `applyUniforms` under one pipeline, never both, since Vulkan and WebGPU pick the buffer of each block when they make the descriptor set or the bind group of the uniforms.

`BufferRules` of `engine/src/gpu/BufferRules.hpp` checks these rules for every device, which throws `gpu::ContractError` of the rule `Buffer`, `Bindings` or `Uniforms`, and `graphics::Device::createBuffer` refuses such a buffer first with a sentence that says what the app asked for. The update of `BufferUpdate` is one of three:

| Update | Contents | Writes |
| --- | --- | --- |
| `Immutable` | The data it was made with, for as long as it lives. | None, and no copy reaches it. |
| `Dynamic` | What the last write or copy left, across frames. | `writeBuffer` and `copyBuffer` of whole words at any point between passes. The passes after a write read the new bytes, and the passes before it the old ones. |
| `Stream` | What the frame wrote, for that frame alone. | `writeBuffer` before the passes of the frame that read it, each part once a frame. It is never the source or destination of a copy. |

The renderer writes the vertices of its draws into stream buffers every frame and keeps the vertices of a baked batch in an immutable buffer. A dynamic buffer suits data that changes now and then, such as the bones of a model or the instances of a field of grass, since it keeps what it holds and takes only the ranges that change.

`copyBuffer(source, sourceOffset, destination, destinationOffset, size)` copies a range of a copy source into a copy destination between passes, at offsets and of a size that are multiples of four bytes. Each device records the writes into dynamic buffers and the copies at their place among the passes of the frame, so a write or a copy between two swapchain passes ends what the device keeps open of the first one, as an offscreen pass does.

A uniform block of a program reads either the bytes that `applyUniforms` gives its slot or a range of a uniform buffer, which the field `uniformBuffers` of the `Bindings` names at that slot as a `BufferRange` of a buffer and an offset. The offset is a multiple of `kUniformBufferAlignment`, 256 bytes, the strictest alignment of the graphics APIs, and the buffer holds the whole block from there. So many draws share one buffer of their uniforms, each at its own offset, and data that stays the same across frames never travels again.

```cpp
const gpu::Device::Buffer lights = device.makeBuffer({.size = 4096, .usage = {.uniform = true}, .update = gpu::Device::BufferUpdate::Dynamic, .label = "lights"});
device.writeBuffer(lights, 0, std::as_bytes(std::span(sun)));
gpu::Device::Bindings bindings;
bindings.vertexBuffers[0] = quad;
bindings.uniformBuffers[2] = {.buffer = lights, .offset = 0};
device.applyBindings(bindings);
```

## Passes and draws

A pass draws into attachments. A `PassDesc` that names color attachments draws offscreen, into up to four render targets at a mip level and layer of each, with an optional depth stencil image. A `PassDesc` that names no attachment draws into the swapchain of the window. Each attachment says how it loads, with `Clear`, `Keep` or `DontCare`, and how it stores, with `Store` or `Discard`. Tile GPUs skip memory traffic for `DontCare` and `Discard`. A multisampled color attachment resolves into its `resolve` image when the pass ends. A pass that cannot begin, such as one into an image that is not a render target, throws `gpu::ContractError` and leaves no pass open, so the next pass begins as usual. A swapchain pass that keeps the contents keeps the depth, the stencil and the multisampled color of the window only when it directly follows the swapchain pass of the same surface, with no debug group pushed or popped and no write into a dynamic buffer or copy of buffers between them. After an offscreen pass, a debug group, such a write or a copy between them, a window of one sample keeps its color alone, and a multisampled window keeps no color, since its multisampled color lives in tile memory for one run of swapchain passes. So the renderer draws the offscreen passes of a frame before its swapchain passes, and a screen whose canvases hold glass draws into an image of its own. The null device refuses a swapchain pass of a multisampled window that keeps its color after such a pass, so a renderer that would lose that color fails in the engine tests. A window system may show the images of a swapchain turned, such as Android on a phone held across its panel, and then the device draws into images that hold the window turned by quarter turns, so the window system shows them without turning them itself. The method `getPassTurns` gives the clockwise quarter turns of the pass that is open, which is 0 in every offscreen pass and on every device but Vulkan. The device turns the viewports and scissors of such a pass, which the renderer gives in the pixels of the window, and the renderer turns the positions that the vertex stages of its draws write in clip space through `gpu::SurfaceTurn`, so every pixel lands where the turn of the images takes it.

Inside a pass the renderer calls, in this order for each draw:

1. `applyViewport` and `applyScissor`, in pixels from the top left of the attachment, or of the window in a swapchain pass whose images are turned.
2. `applyPipeline`.
3. `applyBindings`, with up to four vertex buffers and their offsets, an index buffer, a uniform buffer for each uniform block that reads one, up to 32 textures and up to 12 samplers.
4. `applyUniforms` for each other uniform block of the program, up to eight, which copy their bytes into the frame.
5. `applyStencilReference` and `applyBlendColor` when the pipeline uses them.
6. `draw(first, count, instances)`.

Every device remembers what it sent last, so a binding, a pipeline or a value that does not change costs nothing. Debug groups wrap commands with `pushDebugGroup` and `popDebugGroup`, and GPU debuggers show their names.

## Uploads

Two calls bring data from the CPU into a frame:

- `writeBuffer(buffer, offset, data)` writes part of a stream or dynamic buffer at an offset that is a multiple of four bytes, as [buffers](#buffers) explain. A frame writes each part of a stream buffer once, before a pass binds the buffer, and the buffer holds in that frame only what the frame wrote. A dynamic buffer takes writes of whole words between passes, which each device stages in its upload memory and copies into the buffer at that place among the passes.
- `writeImages(writes)` replaces a region of dynamic images, which are flat with one mip level, and keeps their other pixels. Each write names its image, its `Region` of whole pixels from the first row and column, and the pixels of the region alone, row after row without a gap. A frame makes one call before its first pass, with at most one write for each image, so the device stages every region before it copies any and no copy waits for another. Metal blits each region from its upload ring, Vulkan copies it from its staging ring into the image, which leaves its resting layout with its contents, Direct3D 12 copies it from upload memory at the footprint of its size, Direct3D 11 hands its box to the driver, which discards the old pixels only for a write of the whole image, OpenGL and OpenGL ES copy it from the pixel unpack buffer, and WebGL2 and WebGPU hand it to the browser.

Each device moves this data through upload memory of its own for each frame in flight. That memory stays mapped where the API allows it, and it grows only when a frame needs more room, so a frame that writes what the frame before it wrote allocates nothing, even when the GPU falls behind. A device opens its frame, which waits for the frame in flight that used the same memory, before it stages any data. The engine tests check this with `test::AllocationTracker`.

## Reading pixels and bytes back

`readBuffer(buffer, offset, size)` copies a range of a buffer that is a copy source, of whole words, into memory that the CPU reads, after the frame that wrote it was committed, and its `Readback` hands the bytes over as they are once the GPU finished the copy. `readImage(image, width, height)` starts copying the first mip level and layer of a render target after the frame that drew it was committed. Every device refuses a size larger than the render target. It returns a `Readback`, whose `poll` gives nothing until the GPU finished the copy, then hands over four bytes a pixel, red, green, blue and alpha, whatever the format, and gives nothing again after that. The rows start with the first row that the device stores, which `Capabilities::originTopLeft` names. No device waits for the GPU to finish a copy. The [rendering guide](rendering.md#reading-pixels-back) shows how each API copies.

## Frames in flight and lifetimes

The CPU records a frame while the GPU still draws the frames before it. Each device keeps a fixed number of frames in flight behind fences or semaphores, and the next frame waits only when that many frames are still on the GPU.

| Device | Frames in flight |
| --- | --- |
| Metal | 2 |
| Direct3D 12 | 2 |
| Direct3D 11 | 2 |
| Vulkan | 2 |
| OpenGL and OpenGL ES | 3 |
| WebGL2 | The browser orders the frames of the page. |
| WebGPU | The browser orders the frames through its queue. |

Two frames in flight keep the GPU drawing one frame while the CPU records the next, and a third would only let the input of a frame wait behind one more frame. Direct3D 11 waits for the frames in flight on a fence of Direct3D 11.4, which blocks the thread without spinning.

### Frame pacing

The input of a frame reaches the screen as soon as the display allows when the frame reads that input only once the display can take it. So every device waits for the display in `waitForDisplay`, and never inside a frame after its input. The platform calls it between frames, before it reads the events of the next frame: `Player::frame` ends with it on every platform layer, the main callbacks of SDL, the frame thread of Android and the worker of the web, since each of them reads the events of the next frame after the frame before returned. The wait covers every surface that the frames since the last wait presented, the window and the surfaces beyond it, and a frame that presented nothing leaves nothing to wait for. The readbacks that the engine starts once a frame committed come before the wait, and some devices record them into the next frame, so only a call inside a pass breaks the contract and throws `gpu::ContractError` with `Rule::PassOrder`.

| Device | What `waitForDisplay` waits for |
| --- | --- |
| Metal | The slot of the next frame, once the GPU finished the frame before the last, and the next drawable of every surface that presented. A `CAMetalLayer` keeps two drawables, the one that the display shows and the one that the next frame draws, so the drawable comes once the display took the frame before, and the frame draws into it without waiting. |
| Direct3D 12 and Direct3D 11 | The frame latency waitable object of every swapchain that presented, which DXGI signals once the swapchain can take a frame. The swapchains let one frame wait to be shown, the fewest that DXGI allows, and a wait ends after one second at most, the bound that the documentation of DXGI waits with. |
| Vulkan with `VK_KHR_present_id` and `VK_KHR_present_wait` | The display showing the frame that each swapchain presented last, through `vkWaitForPresentKHR`. A window system that holds the presents of a surface, such as a compositor whose outputs sleep, shows nothing while it holds them, so the wait ends after `FrameCallbackGate::kHeldSeconds`, the time after which a window system counts as holding its frames. |
| Vulkan without them | The fence of the frame before the last, whose slot the next frame takes. |
| OpenGL and OpenGL ES | The fence of the frame before the last, since OpenGL tells nothing of when a display shows a frame. |
| WebGL2 and WebGPU | Nothing. The browser paces the frames of a page by its animation frames, and a page may not wait for the GPU. |

A device whose graphics API offers the frames of the window itself paces the window through `paceWindow(framesPerSecond, due)`, which calls `due` on the frame thread whenever the window can take another frame and returns `true`, so the platform plays a frame at each call. The Metal device does it with the `CAMetalDisplayLink` of the layer of the window from macOS 14, iOS 17 and tvOS 17 on, with a latency of one frame, the least it allows: the display link offers the drawable of the next frame once the display took the frame before, so the frame reads its input with its drawable at hand, and the main thread waits in the run loop instead of in the wait for a drawable. The moment it offers a drawable is then the moment the next frame reads its input. Every other device returns `false`, and the platform plays the frames at its own display links, frame callbacks or animation frames. An empty `due` stops the pacing, as for a window that shows nothing.

The moment the wait ends is the moment the next frame reads its input, so a device whose graphics API reports when the display showed a frame measures the age of that input on the screen, which `getInputAge` gives for the window and [`debug.stats`](../lua-api/debug.md#debugstats) shows as `frame.inputAge`:

| Device | When the display showed a frame |
| --- | --- |
| Metal | The `presentedTime` of the drawable of the window, which its presented handler reads. The drawables of the simulators have no presented handler, so the age is unknown there. |
| Direct3D 12 and Direct3D 11 | The `SyncQPCTime` of the frame statistics of DXGI, for the present whose count they name. |
| Vulkan with `VK_GOOGLE_display_timing` on Linux and Android | The `actualPresentTime` of the past presentation timing of the swapchain, on the monotonic clock. |
| OpenGL, OpenGL ES, WebGL2, WebGPU and Vulkan without it | Unknown, so the age is unknown too. |

The null device counts the waits, and how many of them followed frames that presented, for the engine tests, and takes the age of the input that a test sets with `setInputAge`. The GPU tests measure the age of the input in frames on every device that waits: frames whose GPU work takes far longer than their recording on the CPU present a surface, and once the wait after a frame returns, the GPU finished the frame before it.

`destroy` never frees an object that a frame on the GPU may still use. Metal, Direct3D 12 and Vulkan keep it until the frames that may use it finished. Direct3D 11, OpenGL and WebGPU hand it to their API, which keeps it alive while queued commands use it. The engine itself destroys objects only between frames, through the graveyard of `graphics::Device`, and only on the frame thread.

## Memory held for later

A device keeps some memory for the frames to come, so a frame like the one before it allocates nothing. Its upload memory grows to the largest frame, and the memory allocator of Vulkan keeps an empty block of each memory type after its objects are gone. After a burst of loads, such as a scene that loads its art and then lets it go, that memory would stay with the app. `getSpareBytes()` tells how many bytes the device holds for later beyond what its objects take, and `trim()` gives them back to the system.

A trim runs outside a pass and never waits for the GPU. What the GPU finished with goes at once. The upload memory of a frame in flight goes when that frame finished, as the device starts its next frame there. Between frames, once every frame finished, the objects destroyed since the last frame go at once too, so a trim in the background, where no frame runs, gives everything back.

| Device | What `trim` gives back |
| --- | --- |
| Metal | The objects that the finished frames released, and the upload ring of each frame in flight down to what its last frame wrote. Metal frees every object that the device releases. |
| Direct3D 12 | The objects that the finished frames released, the upload pages of the pool beyond one for each frame in flight, and the blocks of small buffers that hold no buffer. |
| Direct3D 11 | The memory that the driver keeps for the device, such as the copies of the dynamic buffers it hands out, through `IDXGIDevice3::Trim`, after the context let go of every bound object. The rings of the device keep their size. |
| Vulkan | The objects that the finished frames released, the chunks of the staging and uniform rings that the last frames left empty, and the empty memory blocks of the Vulkan Memory Allocator, through `vmaFreeEmptyBlocks` of the [patch](../tools/build.md#vulkan-memory-allocator-empty-blockspatch). |
| OpenGL and OpenGL ES | The upload ring down to what the last frame of each region wrote. OpenGL frees every object that the device deletes. |
| WebGL2 | Nothing, since every upload goes to the browser without a ring of the device. |
| WebGPU | The chunks of the uniform ring that the last frame did not reach, with their bind groups. The browser holds every other upload. |

The engine trims in two moments. `graphics::Device::followSpareMemory`, which runs at the end of every frame, trims once the spare bytes stopped growing for `graphics::Device::kTrimDelay`, two seconds of frames, so what a burst left behind goes back and a frame that draws what the frame before drew trims nothing and allocates nothing. A low memory warning of the system trims at once, after `assets.releaseUnused` and the listeners of `appLowMemory` released what they could. An app trims on its own with [`graphics.trim`](../lua-api/graphics.md#graphicstrim), such as after it left a level, and [`debug.stats`](../lua-api/debug.md#debugstats) reports the spare bytes as `memory.gpuSpare`.

## Surfaces

The platform layer hands a device a `gpu::Surface`: the kind of window, its native handles, its `gpu::Surface::Size`, the sample count, whether vertical sync is on and whether the window is transparent. A device made without a surface draws offscreen only, which the GPU tests use.

The size holds the drawable that the swapchain passes draw into and the window that shows it, both in pixels. They differ while the app [caps its pixel density](rendering.md#pixel-density) on a denser display, and then the drawable is smaller and is stretched over the window with a linear filter:

| Device | How a smaller drawable fills its window |
| --- | --- |
| Metal | The drawable of the `CAMetalLayer` takes the size of the drawable, and the layer stretches it over its bounds. |
| Direct3D 12 and Direct3D 11 | The buffers of the swapchain take the size of the drawable, which DXGI stretches over the window, and the visual of a transparent window scales them in DirectComposition. |
| Vulkan | The passes draw into an image of the size of the drawable for each image of the swapchain, which the frame copies into that image with `vkCmdBlitImage` and a linear filter, since the surface of a window of Windows, X11 or Wayland fixes the size of its images. A surface that takes no copies into its images throws `std::runtime_error`. |
| OpenGL, OpenGL ES and WebGL2 | The passes draw into a framebuffer of the size of the drawable, with the samples of the window, which the device draws over the default framebuffer of the window before it swaps it, since a blit cannot scale into a multisampled framebuffer. |
| WebGPU | The canvas keeps the size of the window, and the passes draw into a texture of the size of the drawable, which the frame draws over the texture of the canvas. |

A platform whose window takes buffers of the size of the drawable, such as the native window of Android, reports the drawable as the window, so no device scales there. The null device checks that a window is never smaller than its drawable and keeps the size it got last.

| Kind | Handles | Devices |
| --- | --- | --- |
| `MetalLayer` | A `CAMetalLayer` | Metal |
| `Win32` | An `HWND` and the `HINSTANCE` | Direct3D 12, Direct3D 11 and Vulkan |
| `Xlib` | An X11 `Window` and its `Display` | Vulkan |
| `Wayland` | A `wl_surface` and its `wl_display` | Vulkan |
| `Android` | An `ANativeWindow` | Vulkan |
| `WebCanvas` | The selector of the canvas | WebGPU |
| `Gl` | A `gpu::GlContext` that owns the drawable | OpenGL, OpenGL ES and WebGL2 |
| `Port` | Handles that only the device of a port reads | The device of a [port](../platform/ports.md) |

The surface changes while the app runs, and the device keeps every object it made through each change:

- `resizeSurface(size)` follows a new size of the drawable and of its window from the next frame. The platform hands a device only a drawable with an area. A window without one, such as a canvas that its page hides with `display: none`, leaves the device with the last surface, the engine draws no frame and lays nothing out until the window has an area again, and a page whose canvas has none at the start holds the start until it shows. The null device refuses a surface without area, so a platform that hands one over fails in the engine tests.
- `detachSurface` lets go of the window when the platform has none, such as an Android app in the background, and `attachSurface` takes the new window when it comes back.
- A swapchain that its API reports out of date or suboptimal is made again before the next frame.

A device may also draw into surfaces beyond the window, such as the windows of the outputs of an app on other displays. `addSurface(surface)` makes a swapchain on another surface, with the color format, the samples and the depth and stencil of the swapchain of the window, so the swapchain pipelines draw into it too, and returns its number from 1. A swapchain pass names that number in the field `surface` of its `PassDesc`, where 0 is the window. `resizeSurface(surface, size)` follows its drawable, `removeSurface(surface)` lets it go once the frames in flight finished with it, and `commit` presents every surface that a pass drew into. Every device draws into several surfaces. Metal makes another drawable of each `CAMetalLayer`. Vulkan makes another `VkSurfaceKHR` and swapchain on the same device and queue, checks that the queue presents to the new surface, and submits each frame once, waiting for the image of every swapchain it draws into and presenting each one. Direct3D 11 and Direct3D 12 make another flip model swapchain of DXGI on the new window, and Direct3D 12 keeps the views of its buffers in its heaps while the surface lives. OpenGL and OpenGL ES take a surface of the kind `Gl` whose `gpu::GlContext` is another drawable of the context of the window, such as the context of the window drawing into the window of the output through SDL or another window surface of the same EGL context on Android, so every object of the context serves every surface, and the device makes the drawable current for the passes that draw into it and swaps it once a pass into another drawable leaves it, or at the commit. Some drivers, such as the software rasterizer of Mesa, keep the back buffer of a drawable only while it is current, so a drawable that waited for the commit would show nothing. WebGPU configures another canvas of the page with the same device. A WebGL2 context belongs to its one canvas and shares nothing with another context, so the web platform shows outputs only through WebGPU. A device whose surface cannot take the format of the window, or whose queue cannot present to it, throws `std::runtime_error`, which ends an output as `unsupported`. So on every device the swapchain passes of a frame into one surface follow one another, with offscreen passes allowed between them. The null device checks that a pass names a surface it has and that the passes into one surface follow one another, and the GPU tests draw into surfaces beyond the window on every device whose machine gives them one: a `CAMetalLayer` on Apple platforms, a hidden window on Windows, a window of X11 on Linux while a display server runs, a canvas of the page on the web, and another drawable of the offscreen context of OpenGL.

A transparent window composites the alpha of its pixels as premultiplied where the platform allows it: through the `opaque` property of the layer on Apple platforms, DirectComposition on Windows with Direct3D, the composite alpha of the Vulkan surface and the alpha mode of the WebGPU canvas.

## Lost devices

A device is lost when the GPU resets or is removed, when its driver fails or updates, when the system revokes the GPU, or when an OpenGL or WebGL context is lost. `isLost` then answers `true`, and the device stops waiting for the GPU and recording work. The SDL platform layer, the worker layer of the web and the Android runtime check it before each frame, log which API stopped, make a new device, on a new context for OpenGL and OpenGL ES, and start the app again on it. The Android runtime also counts an EGL context that reports itself lost as a lost device, and it makes the new device through the same order of APIs, so a Vulkan device that cannot start again gives way to OpenGL ES. A lost device gets one replacement. A replacement that is lost too shows a GPU that keeps no device for the app, so the platform ends the app through `Player::fail`, whose reason reaches the log and the web page, and a window of the desktop closes with the exit status 1. A replacement that cannot be created ends the app the same way, as an engine that cannot start. The class `platform::DeviceRecovery` holds this rule for every platform layer, and its tests lose the null device of the headless host through `gpu::NullDevice::lose`, so they show the replacement that draws on and the reason that ends the app. A lost surface is not a lost device: the platform calls `detachSurface` and `attachSurface` and the app goes on.

## Capabilities

`getCapabilities()` returns what the device and its GPU can do, read from the API when the device starts. The renderer reads these values and never uses a capability that a device lacks.

| Field | Meaning |
| --- | --- |
| `maxImageSize` | The largest side of a flat image in pixels. |
| `maxArrayLayers` | The most layers of an array image. |
| `maxVolumeSize` | The largest side of a volume, along its width, its height and its slices. |
| `maxCubeSize` | The largest side of the square faces of a cube. |
| `originTopLeft` | Whether the first row of a render target is its top row. |
| `depthRange` | The range of depth in clip space, `ZeroToOne` or `NegativeOneToOne` of `math::Mat4::DepthRange`, which the vertex stages write and the projections of the renderers take. |
| `independentBlend` | Whether each color target of a pipeline takes a blend and a write mask of its own. Without it, every target takes those of the first. |
| `gpuTiming` | Whether `getGpuFrameSeconds` measures the time the GPU took for a frame. |
| `maxAnisotropy` | The most samples of anisotropic filtering that a sampler takes, 1 without anisotropic filtering. |
| `maxLodBias` | The largest bias of the level of detail that a sampler adds in either direction, 0 where samplers take no bias. |
| `borderColors` | Whether samplers read a border color through `Wrap::ClampToBorder`. |
| `storageBuffers` | Whether buffers take the usage `storage`, which shaders read and write. |
| `indirectBuffers` | Whether buffers take the usage `indirect`, the arguments of indirect draws. |
| `maxUniformBlockSize` | The most bytes of one uniform block of a program, whether a uniform buffer feeds it or `applyUniforms` takes it. |
| `formats` | What the device does with each pixel format, a `FormatSupport` for each value of `PixelFormat`, which `getFormat(format)` returns. |

A `FormatSupport` holds six values:

| Field | Meaning |
| --- | --- |
| `sample` | Textures of the format can be made and sampled. |
| `filter` | Linear filters sample the format, through texture slots of the type `SampleType::Float`. The other formats sample only through slots of `SampleType::UnfilterableFloat` with samplers of `SamplerType::NonFiltering`. |
| `render` | Render targets, or depth stencil images for a depth format, can be made in the format. |
| `blend` | Pipelines blend into the color format. |
| `storage` | Shaders write the color format as a storage image. |
| `sampleCounts` | The sample counts that render targets and depth stencil images of the format take, as bits whose values are the counts, so `5` holds one sample and four. A color format that takes more samples than one also resolves into an image of one sample. |

For example, the light map of lit canvases is `Rgba16Float` only when that format renders and blends, as the [lighting section](rendering.md#lighting) explains.

## Pixel formats

`PixelFormat` names every format of images, and `kPixelFormatNames` names each one the way Lua writes it. The formats fall into three groups:

- Uncompressed color formats: `R8`, `Rg8`, `Rgba8`, `Rgba8Srgb`, `Bgra8`, `Rgb10A2`, `Rg11B10Float`, `R16Float`, `Rg16Float`, `Rgba16Float`, `R32Float`, `Rg32Float` and `Rgba32Float`. `Rg11B10Float` holds three floats without a sign in 32 bits, which HDR color buffers take at half the memory of `Rgba16Float`.
- Depth formats: `Depth16`, `Depth32Float`, `Depth24Stencil8` and `Depth32FloatStencil8`, which belong to depth stencil images. A draw samples their depth, and a sampler with a compare function compares it, as shadow maps need.
- Block compressed formats, which hold blocks of pixels that the GPU decodes as it samples them: the BC family, `Bc1` to `Bc7` with their `Srgb`, `Snorm` and float forms, the ETC2 and EAC family, `Etc2Rgb8`, `Etc2Rgb8A1`, `Etc2Rgba8`, `EacR11` and `EacRg11` with their `Srgb` and `Snorm` forms, and the ASTC family, every block size from `Astc4x4` to `Astc12x12` in its plain, `Srgb` and `Hdr` forms.

`gpu::ImageInfo` of `engine/src/gpu/ImageInfo.hpp` tells every device and the resources of the engine what a format holds: the pixels and bytes of one block, which is one pixel for an uncompressed format, the bytes of a row of blocks and of a region, and the compression family of a format. Every size of the contract counts in blocks. The rows of a level of a compressed image are rows of blocks, and a level smaller than a block takes one whole block on each side it lacks.

A block compressed image follows rules that hold on every graphics API:

- It is a texture, never a render target, a depth stencil image or a dynamic image.
- It is flat, an array or a cube, never a volume, since most APIs have no compressed volumes.
- Its width and height hold whole blocks, as Direct3D and WebGPU require. Its smaller mip levels round up to whole blocks.
- Its format is one that the device samples. A device that lacks the format of an image refuses it, and the engine never decodes blocks on the CPU in its place. An app ships the formats of each target, as the [texture guide](rendering.md#compressed-textures) explains.

Each device fills the table of formats from its graphics API when it starts.

| Device | Formats |
| --- | --- |
| Metal | Every uncompressed and depth format. The ETC2, EAC and ASTC formats on Apple GPUs, with the HDR profile of ASTC from the A13 and M1 on. The BC formats where the device reports `supportsBCTextureCompression`, which every Mac and the iPads of the M1 on do, from iOS and tvOS 16.4 on. |
| Direct3D 12 and 11 | What `CheckFeatureSupport` and `CheckFormatSupport` report for the DXGI format, the BC formats on every GPU of the feature level the devices need. Direct3D has no ETC2, EAC or ASTC format. |
| Vulkan | What the format properties of the GPU report, with the BC, ETC2 and ASTC families only where the device has the features `textureCompressionBC`, `textureCompressionETC2` and `textureCompressionASTC_LDR`, which it turns on, and the HDR profile of ASTC with the feature of Vulkan 1.3 or of the extension `VK_EXT_texture_compression_astc_hdr`. |
| OpenGL | Every uncompressed format, the BC4 to BC7 formats of OpenGL 4.3, ETC2 and EAC, BC1 to BC3 with `GL_EXT_texture_compression_s3tc` and their sRGB forms with `GL_EXT_texture_sRGB`, and ASTC with `GL_KHR_texture_compression_astc_ldr` and its HDR profile with `GL_KHR_texture_compression_astc_hdr`. |
| OpenGL ES | Every uncompressed format, ETC2 and EAC of OpenGL ES 3.0, ASTC from OpenGL ES 3.2 on or with its extension, and the BC formats with `GL_EXT_texture_compression_s3tc`, `GL_EXT_texture_compression_s3tc_srgb`, `GL_EXT_texture_compression_rgtc` and `GL_EXT_texture_compression_bptc`. |
| WebGL2 | Every uncompressed format, and each compressed family with the extension of WebGL that the browser offers: `WEBGL_compressed_texture_s3tc`, `WEBGL_compressed_texture_s3tc_srgb`, `EXT_texture_compression_rgtc`, `EXT_texture_compression_bptc`, `WEBGL_compressed_texture_etc` and `WEBGL_compressed_texture_astc`. WebGL2 shows a page no HDR profile of ASTC, so those formats are missing. |
| WebGPU | Every uncompressed format, and each compressed family with the features `texture-compression-bc`, `texture-compression-etc2` and `texture-compression-astc`. WebGPU has no HDR profile of ASTC. `Rg11B10Float` renders only with the feature `rg11b10ufloat-renderable`. |

The formats that shaders write as storage images are the ones that the API lists for its storage images or image units: from the format support of the GPU on Direct3D and Vulkan, the uncompressed color formats on Metal with the sRGB one only on Apple GPUs, the formats of the image units of OpenGL 4.3 and OpenGL ES 3.1, and `Rgba8`, `Rgba16Float`, `R32Float`, `Rg32Float` and `Rgba32Float` on WebGPU. OpenGL ES 3.0 and WebGL2 have no storage images.

## Samplers

A `SamplerDesc` names the filters of minification, magnification and mipmaps, a wrap for each axis `U`, `V` and `W`, the levels of detail that lookups stay within from `minLod` to `maxLod`, a `lodBias` that each lookup adds to its level of detail, the most samples of anisotropic filtering in `maxAnisotropy`, an optional compare function that turns the sampler into one that compares depth, as shadow maps read, and the `BorderColor` that `Wrap::ClampToBorder` reads outside the image. `gpu::SamplerRules` of `engine/src/gpu/SamplerRules.hpp` checks every description on every device before it makes a sampler, and a description that breaks a rule throws `ContractError` of the rule `Sampler`:

- `maxAnisotropy` runs from 1 to `Capabilities::maxAnisotropy`, and more than one needs linear filters for minification, magnification and mipmaps, as WebGPU requires.
- `minLod` is 0 or more and at most `maxLod`.
- `lodBias` lies within `Capabilities::maxLodBias` in either direction. Metal, OpenGL ES, WebGL2 and WebGPU have no bias in their samplers, so their `maxLodBias` is 0.
- `Wrap::ClampToBorder` needs `Capabilities::borderColors`. The border colors are transparent black, opaque black and opaque white, the ones that every API with border colors has. WebGL2 and WebGPU read no border color, so on the web no API has them and the gap stays, while Metal reads them on Apple GPUs from the A14 and M1 on and on every Mac.

No device clamps a value of a sampler to what its API takes, since the rules refuse it before.

## Validation and profiling

The option `validation` of `gpu::DeviceFactory::Options` turns on the validation of the API. Debug builds of the player and every GPU test turn it on.

| Device | Validation |
| --- | --- |
| Metal | The Metal API validation layer through `MTL_DEBUG_LAYER`, and the execution status of each encoder in the error of a failed command buffer. |
| Direct3D 12 and 11 | The debug layers of Direct3D and DXGI, from the optional feature Graphics Tools of Windows, and on Direct3D 12 the breadcrumbs and page faults of removed devices. |
| Vulkan | The validation layers of the Vulkan SDK when the machine has them. |
| OpenGL and OpenGL ES | The debug output of `KHR_debug`, or `glGetError` after each pass and frame where the context lacks it. |
| WebGL2 | `glGetError` after each pass and frame. |
| WebGPU | The browser validates every call in every build. |

Each device turns the messages of its API into lines of the engine log, errors as errors, and every error in the log fails a GPU test. Every object takes the `label` of its description, and debug groups become the markers of GPU debuggers.

For profiling, `getGpuFrameSeconds()` returns the time the GPU took for the last frame it finished, where `Capabilities::gpuTiming` is true, and `readPools(pools)` reports how many objects of each kind exist. The debug overlay and [`debug.stats()`](../lua-api/debug.md) show both.

## Frame captures

A frame capture records every command of the device during some frames into a file that the debugger of a capture tool opens, which replays each pass and draw with its pipeline, bindings, buffers and images. Each device names its tool with `getCaptureSupport()`, which returns `{tool, missing}`: the `gpu::Device::CaptureTool` it captures with, from the one table `gpu::Device::kCaptureToolNames`, or `None` with the sentence that says what is missing and how to get it. A capture tool hooks the graphics API before the device exists, so a device finds its tool when it starts and never loads one itself.

| Device | Capture tool | What the app needs |
| --- | --- | --- |
| Metal | The frame capture of Metal, through `MTLCaptureManager`, into a `.gputrace` document that Xcode opens. | The environment variable `MTL_CAPTURE_ENABLED` set to 1 when the process starts, or the key `MetalCaptureEnabled` of the `Info.plist`. Metal reads them as it loads, before any code of the app runs, so the Debug runs of `haylen app run` start the player of macOS and Apple apps with the variable, while shipped apps run without the capture layer, which costs time on every call. |
| Direct3D 12 | The programmatic capture of PIX, into a `.wpix` file, or else RenderDoc. | PIX launched the app for a GPU capture, which loads `WinPixGpuCapturer.dll`, or RenderDoc launched it. |
| Direct3D 11, Vulkan, OpenGL and OpenGL ES | The in-application API of RenderDoc, into an `.rdc` file. | RenderDoc launched the app, through its "Launch Application" panel or `renderdoccmd capture`, which loads `renderdoc.dll` on Windows, `librenderdoc.so` on Linux and its layer `libVkLayer_GLES_RenderDoc.so` on Android. |
| WebGPU and WebGL2 | None. | Browsers give a page no capture tool, so the device always reports `None`. |
| The null device | None, unless a test makes it with a tool, which records when captures begin and end without writing a file. | Nothing. |

`beginCapture(path)` starts a capture between passes, at a path without its extension, which the tool adds, and `endCapture()` ends it after the commit of the last frame and returns the path of the file. RenderDoc adds the number of the first frame to the name, such as `_frame482.rdc`. A capture outside a pass that began while another one records, or without a tool, throws `std::logic_error`, and a tool that refuses or writes no file throws `std::runtime_error` with its reason.

`graphics::Device::captureFrames` counts the frames: it begins a capture at once and ends it after the given number of frames was submitted, then hands the path to its callback on the frame thread. An app that stops during a capture ends it and drops its callback. The [debug reference](../lua-api/debug.md#gpu-captures) shows how apps, the developer console and `haylen app capture` ask for one, and where the files go.

## The rules of the null device

`gpu::NullDevice` in `engine/src/gpu/NullDevice.hpp` is the device of the headless host. It draws nothing, and it throws `gpu::ContractError` of `haylen/gpu/ContractError.hpp` on every call that breaks the contract, in every build, so the engine tests fail on a renderer that a real device would reject. Every device of the engine throws the same error for a call that breaks the contract. Only a bug of the engine or of a port makes such a call, so the message of the error says `Haylen hit an internal error in its graphics device. Report it with the steps that led to it.`, and the rule that the call breaks, such as `Rule::PassOrder` or `Rule::MissingObject`, is a value of the error that tests check with `getRule`, never part of its message. In short, the null device checks that:

- Objects exist when they are used, and images fit their type, size, layers, mip levels, sample count, format and kind.
- An image of a block compressed format is a texture of whole blocks that is no volume, in a format that the device samples, as the [pixel formats](#pixel-formats) say.
- A sampler follows the [rules of samplers](#samplers): anisotropy within the device and with linear filters, levels of detail in order, a bias within the device and border colors only where the device has them.
- Passes open and close in turn, with attachments of one size and sample count, and a pipeline draws only in a pass whose attachments match it.
- A draw comes inside a pass, after a pipeline, its bindings and every uniform block of its program, and reads only indices its buffer holds.
- A buffer follows the [rules of buffers](#buffers): usages that the device has and that serve together, data for an immutable buffer, no copy of a stream buffer, writes, copies and readbacks of whole words inside the buffer, vertex, index and uniform buffers with those usages, and uniform buffers at aligned offsets with room for their block, which no `applyUniforms` of the same slot meets.
- A texture slot that filters never takes a format the device cannot filter, and a pass never samples its own attachments or the images they resolve into.
- Stream buffers and dynamic images take their data before the passes that read them, once a frame.
- Debug groups close inside the pass that opened them, and the surface goes and comes back in turn.
- A readback hands its pixels over once.
- The swapchain passes of a frame into one surface follow one another.
- Pools hold at most the sizes that `NullDevice.hpp` declares, so a leak of GPU objects fails a test.
- A capture begins and ends in turn outside a pass, only on a device that a test made with a capture tool.
- The wait for the display comes outside a pass.
- A trim comes outside a pass. The device holds the memory of the buffers and images it destroyed as spare bytes until a trim gives it back, the way a device keeps its empty memory blocks, and `getTrimCount` tells how often the engine trimmed it.

The [testing guide](../tools/testing.md#the-headless-host) lists every check and how tests read what a frame did.

## The devices and their graphics APIs

| Device | Folder | Graphics API | Shader language | Platforms |
| --- | --- | --- | --- | --- |
| [Metal](metal.md) | `engine/src/gpu/metal/` | Metal | MSL for macOS, iOS and the iOS simulator | macOS, iOS, iPadOS, Mac Catalyst, tvOS and visionOS |
| [Direct3D 12](direct3d.md) | `engine/src/gpu/d3d12/` | Direct3D 12 | HLSL for shader model 5.0 | Windows |
| [Direct3D 11](direct3d.md) | `engine/src/gpu/d3d11/` | Direct3D 11.4, with a driver of WDDM 2.0 or later | HLSL for shader model 5.0 | Windows |
| [Vulkan](vulkan.md) | `engine/src/gpu/vulkan/` | Vulkan 1.1 | SPIR-V | Windows, Linux and Android |
| [OpenGL](opengl.md) | `engine/src/gpu/opengl/` | OpenGL 4.3 core | GLSL 4.30 | Windows and Linux |
| [OpenGL ES](opengl.md) | `engine/src/gpu/opengl/` | OpenGL ES 3.0 or later | GLSL 3.00 ES | Android |
| [WebGL2](opengl.md) | `engine/src/gpu/opengl/` | WebGL2 | GLSL 3.00 ES | The web |
| [WebGPU](webgpu.md) | `engine/src/gpu/webgpu/` | WebGPU | WGSL | The web |

The folder `engine/src/gpu/direct3d/` holds what both Direct3D devices share: the adapters, the formats, the shader compiler and the swapchain.

### Capability matrix

The matrix lists what each device reports. "From the GPU" means that the device asks the API at start, so the value differs between machines.

| Capability | Metal | Direct3D 12 and 11 | Vulkan | OpenGL | OpenGL ES | WebGL2 | WebGPU |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Largest image | 16384, or 8192 on the A7 and A8 GPUs | 16384 | From the GPU | From the GPU | From the GPU | From the GPU | From the adapter |
| Array layers | 2048 | 2048 | From the GPU | From the GPU | From the GPU | From the GPU | From the adapter |
| Largest volume | 2048 | 2048 | From the GPU | From the GPU | From the GPU | From the GPU | From the adapter |
| Largest cube | As large as a flat image | 16384 | From the GPU | From the GPU | From the GPU | From the GPU | As large as a flat image |
| Sample counts of each format | 2, 4 and 8 where the GPU takes them, and the 32-bit floats only where the GPU filters them | The counts of each format that the GPU draws into and resolves | The counts of each format in its image format properties | The counts of each format in `GL_SAMPLES` | The counts of each format in `GL_SAMPLES` | The counts of each format in `GL_SAMPLES` | 1 and 4, and 1 alone for the 32-bit floats |
| Sample and draw into formats | Every format, `Depth24Stencil8` as `Depth32FloatStencil8` | From the GPU | From the GPU | Every format | Float targets with extensions | Float targets with extensions | Every format, `Depth32FloatStencil8` with its feature |
| Blend into 32-bit floats | From the A14 and M1 on, and on Intel Macs | From the GPU | From the GPU | Yes | With `GL_EXT_float_blend` | With `GL_EXT_float_blend` | With the feature `float32-blendable` |
| Filter 32-bit floats | Where the GPU supports it | From the GPU | From the GPU | Yes | With `GL_OES_texture_float_linear` | With `GL_OES_texture_float_linear` | With the feature `float32-filterable` |
| Draw into `Rg11B10Float` | Yes | From the GPU | From the GPU | Yes | With `GL_EXT_color_buffer_float` | With `EXT_color_buffer_float` | With the feature `rg11b10ufloat-renderable` |
| BC formats | Every Mac, and Apple GPUs that report them from 16.4 on | Yes | With `textureCompressionBC` | BC4 to BC7, and BC1 to BC3 with S3TC | With their extensions | With their extensions | With the feature `texture-compression-bc` |
| ETC2 and EAC formats | Apple GPUs | No | With `textureCompressionETC2` | Yes | Yes | With `WEBGL_compressed_texture_etc` | With the feature `texture-compression-etc2` |
| ASTC formats | Apple GPUs | No | With `textureCompressionASTC_LDR` | With `GL_KHR_texture_compression_astc_ldr` | From OpenGL ES 3.2 or with its extension | With `WEBGL_compressed_texture_astc` | With the feature `texture-compression-astc` |
| HDR profile of ASTC | From the A13 and M1 on | No | With the feature of Vulkan 1.3 or its extension | With `GL_KHR_texture_compression_astc_hdr` | With `GL_KHR_texture_compression_astc_hdr` | No | No |
| Storage images | The uncompressed color formats | From the GPU | From the GPU | The formats of the image units | From OpenGL ES 3.1 | No | `Rgba8`, `Rgba16Float`, `R32Float`, `Rg32Float` and `Rgba32Float` |
| Independent blend | Yes | Yes | Where the GPU has the feature | Yes | On OpenGL ES 3.2 or with an extension | No | Yes |
| Anisotropic filtering | 16 | 16 | From the GPU, with the feature `samplerAnisotropy` | From the GPU, with its extension | From the GPU, with its extension | From the GPU, with its extension | 16 |
| Bias of the level of detail | No | From -15.99 to 15.99 | From the GPU, as `maxSamplerLodBias` | From the GPU, as `GL_MAX_TEXTURE_LOD_BIAS` | No | No | No |
| Border colors | From the A14 and M1 on, and on every Mac | Yes | Yes | Yes | On OpenGL ES 3.2 or with `GL_EXT_texture_border_clamp` | No | No |
| Storage buffers | Yes | Yes | Yes | Yes | From OpenGL ES 3.1 | No | Yes |
| Indirect buffers | Every Mac and the A9 and later | Yes | Yes | Yes | From OpenGL ES 3.1 | No | Yes |
| Largest uniform block | The largest buffer of the GPU | 65536 bytes | From the GPU, as `maxUniformBufferRange` | From the GPU, as `GL_MAX_UNIFORM_BLOCK_SIZE` | From the GPU, as `GL_MAX_UNIFORM_BLOCK_SIZE` | From the GPU, as `GL_MAX_UNIFORM_BLOCK_SIZE` | From the adapter, at most 65536 bytes |
| GPU timing | Yes | Yes | Where the queue writes timestamps | Yes | With `GL_EXT_disjoint_timer_query` | With `EXT_disjoint_timer_query_webgl2` | With the feature `timestamp-query` |
| First row | Top | Top | Top | Bottom | Bottom | Bottom | Top |
| Range of depth | 0 to 1 | 0 to 1 | 0 to 1 | 0 to 1 with `glClipControl` of OpenGL 4.5 or `GL_ARB_clip_control`, -1 to 1 without | 0 to 1 with `GL_EXT_clip_control`, -1 to 1 without | 0 to 1 with `EXT_clip_control`, -1 to 1 without | 0 to 1 |
| Frame capture | The frame capture of Metal in the Debug runs of `haylen app run` | PIX or RenderDoc on Direct3D 12, RenderDoc on Direct3D 11, when one launched the app | RenderDoc when it launched the app | RenderDoc when it launched the app | RenderDoc when it launched the app | No | No |
| Vertical sync off | On macOS and Mac Catalyst | Yes, with tearing where the display allows it | Mailbox or immediate where the surface offers them | Swap interval 0 | Swap interval 0 | The swap interval that SDL hands the page | No, the browser paces the canvas |

The device pages explain each value: [Metal](metal.md#capabilities), [Direct3D](direct3d.md#capabilities), [Vulkan](vulkan.md#capabilities), [OpenGL](opengl.md#capabilities) and [WebGPU](webgpu.md#capabilities).

## Choosing the graphics API

A build compiles the device of every API of its platform, which the CMake list `HAYLEN_GRAPHICS_APIS` names, as the [build guide](../tools/build.md#cmake-options) describes. When the app starts, the platform layer tries the APIs in an order and keeps the first one whose device starts. It logs the API it chose and why each earlier one failed, such as a machine without Vulkan.

| Platform | Default order |
| --- | --- |
| macOS, iOS, iPadOS, Mac Catalyst, tvOS and visionOS | Metal |
| Windows | Direct3D 12, Direct3D 11, Vulkan, OpenGL |
| Linux | Vulkan, OpenGL |
| Android | Vulkan, OpenGL ES |
| Web | WebGPU, WebGL2 |
| A port with a device of its own | The API `port`, then the APIs of the engine it compiles |

Three things change the order. Each one puts its APIs first, and the APIs of the build that it leaves out follow in the default order:

- The key `window.graphicsApis` of `haylen.json`, an array of names such as `["vulkan", "openGl"]`. A name that is not a graphics API stops the app with an error, and a name that the build lacks is skipped, so one list serves every platform.
- The option `--graphics-api <name>` of the player, which replaces the list of `haylen.json`.
- On the web, the address of the page, such as `?graphicsApi=webGl2`, which the page hands to the runtime as `Module.haylen.graphicsApi`.

```json
{
  "window": {
    "graphicsApis": ["vulkan", "openGl"]
  }
}
```

```sh
haylen --dev my-app --graphics-api openGl
```

The class `platform::GraphicsApiFallback` walks that order for the SDL platform layer and for the worker of the web. Each API opens the window and starts its device, and the first that starts keeps the window. An API that fails closes the window, logs why and gives way to the next one, and the app ends with a sentence once no API is left. Some APIs need other window flags than others, such as an OpenGL context or a Metal layer, so the window opens again for each API that the fallback tries. WebGPU gets its adapter and device from the browser in a later turn of the event loop, so `gpu::DeviceFactory::prepare` starts the request and the platform layer keeps running frames until it is ready.

The options `software` and `lowPower` of `gpu::DeviceFactory::Options` ask a device for the software rasterizer of its API, such as WARP on Direct3D, Mesa lavapipe on Vulkan or the fallback adapter of WebGPU, or for the GPU that uses the least power. The GPU tests set them through `HAYLEN_TEST_GPU_SOFTWARE` and `HAYLEN_TEST_GPU_LOW_POWER`, as the [testing guide](../tools/testing.md#the-gpu-tests) describes.

### Machines with two GPUs

Many laptops have two GPUs: an integrated GPU that saves power and a discrete GPU that draws faster. The player and the apps draw on the discrete GPU unless the app asks for the other one with the key `window.lowPowerGpu` of `haylen.json`, which sets the option `lowPower` of every device it makes.

```json
{
  "window": {
    "lowPowerGpu": true
  }
}
```

Direct3D, Vulkan and WebGPU list every GPU, and each device takes the one that the option asks for. OpenGL lists no GPU. On Windows, the drivers of NVIDIA and AMD pick the GPU of OpenGL for the whole process from two variables that the executable exports, `NvOptimusEnablement` and `AmdPowerXpressRequestHighPerformance`. Every executable of the engine on Windows exports them: the player, the apps that the backends and `haylen_add_app` build, and the test programs. The drivers read them when the process starts and again when the first context of OpenGL opens, and a 1 in either read takes the discrete GPU. So the executable exports them at 0, and `gpu::DeviceFactory::prepare` sets them before the first context opens: to 1 for an app that wants the discrete GPU, and left at 0 for an app with `lowPowerGpu`, so OpenGL draws on the integrated GPU as well. An executable that a project builds in another way compiles the file `src/gpu/windows/GpuPreferenceExports.cpp` of the engine to get the same choice. The settings of the driver and the graphics settings of Windows, where a person picks the GPU of an app, come before what the app asks for.

## Driver issues

Some drivers have bugs that the tests on their GPUs showed, which the devices work around. The table `gpu::DriverIssues` of `engine/src/gpu/DriverIssues.cpp` lists them, each with the reason that the device logs. An entry names the graphics API, a piece of the name the driver gives its GPU and the versions that have the bug where they are known, as a piece of the version text of the driver or, where the API numbers its drivers, the last version number, so a newer driver that fixed it leaves the entry.

| Driver | Issue | What the device does |
| --- | --- | --- |
| Vulkan of the PowerVR Rogue GE8320, up to the driver 5776728 | Some pipelines fail with `VK_ERROR_OUT_OF_HOST_MEMORY`, and `vkCreateGraphicsPipelines` never returns for others. | The Vulkan device refuses the GPU, so the platform draws through OpenGL ES, the next API of the order, and logs why. |
| OpenGL ES of the Xclipse GPUs, through ANGLE | The driver names `GL_OES_texture_float_linear` but samples the 32-bit float formats with the nearest texel. | The `filter` of the 32-bit float formats in `Capabilities::formats` is `false`. |
| OpenGL ES of the Mali-G72, driver r19p0 | Some draws of a pass that follows another pass whose commands the driver still holds reset the context as guilty, and the frame is lost, so a scene leaves its target untouched in some runs. | Every pass ends with `glFlush`. With it the renderer scenes passed 45 times out of 45 on the Mali-G72 of Android 10, where 23 runs out of 24 failed without it. |
| OpenGL ES of AMD, driver 25.10 | The driver names `GL_EXT_disjoint_timer_query` and measures its queries, but refuses `GL_GPU_DISJOINT_EXT` in `glGetIntegerv`, `glGetInteger64v`, `glGetBooleanv` and `glGetFloatv` with `GL_INVALID_ENUM`, an error that the debug output reports in each frame whose timing the device reads. | The GPU timer reads no disjoint state, as on OpenGL, which has none. A probe on the AMD Radeon PRO V620 found the call, and the tests of the OpenGL ES context passed there with the entry. |
| OpenGL of AMD | The GPU hangs in the first frames that draw with new programs while the compiler threads of the driver still work on the shaders it compiled before, and the GPU reset that follows loses the context. On the AMD Radeon PRO V620, the render benchmark hung in its scene of shapes in every run with an empty shader cache, with the driver 25.10 on Linux, where the host reset the GPU, and with the driver 25.1.1 on Windows, which logged a timeout detection and recovery of the GPU. On Linux it hung even with `glFinish` after each frame, a flush after each pass or the link of each program awaited, and never once the cache held the shaders. | The device asks the context to compile on the thread that asks for it, with `glMaxShaderCompilerThreadsKHR(0)` of `GL_KHR_parallel_shader_compile` or `GL_ARB_parallel_shader_compile`. With it the scene passed every run with an empty cache on both systems, and the first frames took as long to compile as before. |
| Vulkan of the Adreno 610, up to the driver 512.415 | The swapchain takes no timeout in `vkAcquireNextImageKHR` but an endless one, which it waits for whatever the call asks, and logs `non-infinite timeouts not yet implemented` at every other timeout, so every frame of Android 11 wrote a warning to the log. | The swapchain waits for its next image without a timeout. Elsewhere it waits at most one second and leaves a frame without an image when the window system keeps every image that long. |
| Vulkan of the Adreno 630, up to the driver 512.331 | A pass that loads the depth of a format without stencil, discards it and clears the stencil that the format has no plane for reads that depth as cleared. | A pass that keeps the depth of a format without stencil also stores the depth and keeps the stencil. With it the test of kept depth passed on the Adreno 630 of Android 9 with 32-bit and 16-bit depth. Without it the test failed with every other change tried there: the depth resting in its attachment layout or in the general layout, the image without the sampled usage or in transient memory, a pass between them that only keeps the depth, a barrier on all commands before each pass, and a store of the depth alone. |
| OpenGL ES of the Android emulator through its translator on SwiftShader 4.0.0.1, the `-gpu swiftshader_indirect` of the x86_64 images on a Linux host | `glClear`, `glClearBufferfv`, `glReadPixels` and `glBlitFramebuffer` reach the first layer of an array or a volume texture whatever layer `glFramebufferTextureLayer` attached, while draws reach that layer. A probe on the x86_64 image of Android 17 cleared layer 1 of an array into layer 0, read layer 0 back for layer 1 and blitted nothing into any layer, and a draw into layer 1 wrote layer 1. Faces of a cube clear correctly. | A pass clears a color attachment at a later layer of an array or a volume by drawing the clear color over it with a small program of its own, `opengl::DrawnClear`, and clears every other attachment with `glClearBufferfv`. |
| OpenGL ES of SwiftShader 4.0.0.1, directly and through the translator of the Android emulator, the `-gpu swiftshader_indirect` of the x86_64 images on a Linux host | `glClearBufferfv` on a multisampled renderbuffer of `GL_SRGB8_ALPHA8`, `GL_RGB10_A2` or a float format clears one of its samples, so the resolve that `glBlitFramebuffer` makes reads the clear color divided by the count of samples, while a draw reaches every sample and `GL_RGBA8` clears every sample. A probe on SwiftShader 4.0.0.1 on Linux and the GPU tests on the x86_64 image of Android 17 showed it with 2 and 4 samples. | A pass clears every multisampled color attachment by drawing the clear color over it with `opengl::DrawnClear`, which reaches every sample. |
| OpenGL ES of SwiftShader 4.0.0.1, directly and through the translator of the Android emulator, the `-gpu swiftshader_indirect` of the x86_64 images on a Linux host | A draw writes the depth of a texture of `GL_DEPTH_COMPONENT32F` in other places than the lookups of that texture read, so the test `GpuDepthStencilTest.DrawsDepthWithoutColorTargets`, whose plane covers the left half of a shadow map, read every other whole row as covered. A probe of the `libGLESv2` of SwiftShader that the Linux emulator ships, with the calls of the device, showed the rows with and without a color attachment, through the viewport and the scissor, and with `glTexStorage2D` and `glTexImage2D`, while clears, `GL_DEPTH_COMPONENT16`, `GL_DEPTH_COMPONENT24`, `GL_DEPTH24_STENCIL8` and `GL_DEPTH32F_STENCIL8` kept every texel. | A texture of `Depth32Float` is made as `GL_DEPTH32F_STENCIL8`, whose depth a pass attaches and clears alone, so it keeps 32-bit float depth. The same probe read the half that the plane covered with that format. |
| OpenGL of Mesa llvmpipe, up to Mesa 26.0 | Anisotropic filtering takes its samples and its level of detail from the slopes of the coordinate between neighboring pixels and ignores the slopes that `textureGrad` gives the lookup. So a material that reads its texture at points that jump from one pixel to the next, which `haylen_texture` reads at the footprint of the pixel, reads the smallest level instead, and the test `GpuRendererTest.ReadsTexturesAtThePixelFootprintWherePointsJump` read no texel of the full texture on Mesa 26.0.8 in Linux. Mesa 26.1 passes the slopes of the lookup to anisotropic filtering, and the test passed with anisotropic filtering on Mesa 26.2.4. | Samplers filter without anisotropy. OpenGL names no number for its drivers, so the device numbers a driver of Mesa from the version of Mesa in its version text, as Mesa numbers its Vulkan drivers, and a newer Mesa leaves the entry. |

A new entry comes with the evidence of a test on the GPU, a unit test in `DriverIssuesTest` and a row in this table.

The same translator of the Android emulator on SwiftShader 4.0.0.1 compiles a constant array that a shader indexes with a value it computes, such as `const vec3 directions[6]` read at `directions[face]`, into a read of one fixed element, whether the array is global or local and whatever type it holds, while arrays that are not constant and uniform arrays read the element of their index. No shader of the engine indexes a constant array that way, and a shader of an app that must run on that emulator computes the value or keeps the array in a uniform block instead.

The Vulkan driver of the Android 16 (API 36) emulator images crashes inside `vkSetDebugUtilsObjectNameEXT` when the device names an image, which it does only with validation on, in Debug builds and tests. The driver of the Android 17 (API 37) images names every object correctly, so the GPU tests of CI run on an API 37 image, and an app that draws through Vulkan with validation on an older emulator image needs a newer image.

## Status of the devices

This table separates what is implemented, what compiles and what was tested, on each platform. "Tested" means that the GPU tests, the conformance suite and the reference images of the 2D renderer, passed there, unless the row says otherwise.

| Device | Platform | Implemented | Compiled | Tested |
| --- | --- | --- | --- | --- |
| Metal | macOS | Yes | Yes, for Apple silicon and Intel Macs, while CI compiles only Apple silicon | On the Apple GPU of a Mac, with the engine tests, the headless test project, the test port and the SDK check. No Intel Mac, since CI runs only the common platforms, which leave Intel Macs out. |
| Metal | iOS and iPadOS | Yes | For devices and the simulator, with the GPU test app signed for an iPhone | On the iPhone and iPad simulators of iOS 27 with `haylen test --gpu --target ios-simulator`, and on an iPhone 13, an A15 with iOS 27.0.1, with `haylen test --gpu --target ios`: the GPU tests, the whole test project in Debug, turns between landscape and portrait, Tiny Island, an app in portrait and the protected release of the test project. On the iPad Pro 13-inch (M5) simulator, the UI tests of `haylen test --ui samples/tests` resized the window of the Debug test project in Windowed Apps, tiled it to each half and resized it there, and resized it by its corner in Stage Manager, and at every size the framebuffer followed the window and the surface check logged no error. No physical iPad. |
| Metal | Mac Catalyst | Yes | Yes | On the Apple GPU of a Mac with `haylen test --gpu --target catalyst`. |
| Metal | tvOS | Yes | For devices and the simulator | On the Apple TV simulator of tvOS 27 with `haylen test --gpu --target tvos-simulator`, and on the first Apple TV 4K, an A10X of the Apple3 family with tvOS 26.6, with `haylen test --gpu --target tvos`: the GPU tests, a sprite batch whose last texture binds at slot 31, the whole test project in Debug, Tiny Island and the protected release of the test project in Release. |
| Metal | visionOS | Yes | For devices and the simulator, in the `xros` and `xros-simulator` slices of `Haylen.xcframework`, with the test project for the simulator | On the Apple Vision Pro simulator of visionOS 27 with `haylen test --gpu --target visionos-simulator`, where the GPU tests passed, and with the whole test project in Debug, which ran 451 tests without a failure. The UI tests of `haylen test --ui` tapped a category of the menu of the test project, which the simulator delivers as a look and a pinch, and played the start of Tiny Island, both without an error in the log. No physical Apple Vision Pro. |
| Direct3D 12 | Windows | Yes | Yes, with MSVC in one build with the other three APIs of Windows | On an NVIDIA GPU, on an Intel GPU with `HAYLEN_TEST_GPU_LOW_POWER=1` and on WARP with `HAYLEN_TEST_GPU_SOFTWARE=1`. The whole test project passed in a window. On the NVIDIA Tesla T4 of a cloud machine with Windows Server 2022 and the driver 596.86, with the debug layer, the benchmarks and the four stress tours in a window. On the AMD Radeon PRO V620 of a cloud machine, a quarter of the GPU that the host shares, with Windows Server 2025 and the driver 25.1.1 (32.0.12033.5037), with the benchmarks, the sprite benchmark and the four stress tours in a window, but without the debug layer, which the Graphics Tools of Windows did not install in 20 minutes. |
| Direct3D 11 | Windows | Yes | Yes, in the same build | On an NVIDIA GPU, on an Intel GPU and on WARP, as Direct3D 12. The whole test project passed in a window. On the NVIDIA Tesla T4 and on the AMD Radeon PRO V620 of cloud machines, as Direct3D 12. |
| Vulkan | Windows | Yes | Yes, in the same build | On an NVIDIA GPU, and on an Intel GPU with `HAYLEN_TEST_GPU_LOW_POWER=1`. The whole test project passed in a window. On the NVIDIA Tesla T4 and on the AMD Radeon PRO V620 of cloud machines, with the validation layers of the Vulkan SDK, the benchmarks and the four stress tours in a window. |
| Vulkan | Linux | Yes | Yes | On the NVIDIA Tesla T4 of a cloud machine with Ubuntu 24.04 and the driver 595.91, with the validation layers, the engine tests in a Release build, the benchmarks and the four stress tours in an Xvfb window, on the AMD Radeon PRO V620 of a cloud machine with Ubuntu 24.04 and the kernel 6.8, both on Mesa RADV 25.2.8 and on the Vulkan driver of AMD 25.10, with the validation layers, the benchmarks and the four stress tours in a Weston window, and on Mesa lavapipe, the software rasterizer, in a container, with the engine tests, the headless test project and the test port. |
| Vulkan | Android | Yes | Yes | On the Android emulator and on six phones and tablets. The GPU tests run from an installed test app, since a program that `adb shell` starts finds no Vulkan GPU on the Mali-G72 of Android 10. They passed on the PowerVR DXT-48-1536 and the Adreno 830 of Android 16, the Xclipse 530 of Android 14, the PowerVR GE8320 of Android 13 and the Mali-G72 of Android 10. The whole test project ran on Vulkan without a failure on the PowerVR DXT-48-1536, the Adreno 830, the Xclipse 530, the Mali-G72 and the Adreno 630 of Android 9. On the Adreno 630 every GPU test passed but the one of kept depth, which passes with the ops that the device uses there, as the [driver issues](#driver-issues) say. A 32-bit depth with stencil still loses the depth that a pass keeps while it clears the stencil, which the 24-bit depth with stencil keeps, so that format stays a limit of that driver. On the PowerVR GE8320 the driver fails one pipeline of the test project with `VK_ERROR_OUT_OF_HOST_MEMORY` and stops for good inside `vkCreateGraphicsPipelines` at the test `CAM-013`, so the device refuses that driver. Device Farm refuses its Android 8.1 device, the Pixel 2 XL, because its own check finds the minimum SDK of 27 above that of the device, although Android 8.1 is API 27, so Android 8.1 ran on no device. |
| OpenGL | Windows | Yes | Yes, in the same build | On the GPU that the OpenGL driver of the laptop gives a hidden window. The whole test project passed in a window. On the NVIDIA Tesla T4 of a cloud machine, with the benchmarks and the four stress tours in a window, once the basic display adapter of the machine was turned off, since the OpenGL of a window on that adapter is the OpenGL 1.1 of Windows. On the AMD Radeon PRO V620 of a cloud machine with the driver 25.1.1, with the GPU tests, the benchmarks, the sprite benchmark and the four stress tours in a window, and the render benchmark three times with an empty shader cache, once the device compiled on its own thread, as the [driver issues](#driver-issues) say. |
| OpenGL | Linux | Yes | Yes | On the NVIDIA Tesla T4 of a cloud machine through EGL without a display, with the engine benchmarks, and on Mesa llvmpipe, the software rasterizer, through EGL without a display. On the AMD Radeon PRO V620 of a cloud machine with Ubuntu 24.04, on the OpenGL driver of AMD 25.10 through EGL without a display, with the benchmarks and the four stress tours in a Weston window. There every GPU test passed on OpenGL and OpenGL ES, and the render benchmark passed three times with an empty shader cache, once the device compiled on its own thread and read no disjoint state on OpenGL ES, as the [driver issues](#driver-issues) say. Mesa radeonsi did not run, since the OpenGL driver of AMD takes the EGL of the machine once it is installed. |
| OpenGL ES | Android | Yes | Yes | On the Android emulator and on six phones and tablets. Every GPU test of OpenGL ES passed on the x86_64 emulator image of Android 17 with `-gpu swiftshader_indirect` on a Linux host, the emulator of the `android-gpu` job of CI, once passes cleared later layers by drawing and textures of 32-bit float depth took a stencil plane, as the [driver issues](#driver-issues) say. The GPU tests passed on the PowerVR DXT-48-1536 and the Adreno 830 of Android 16, the ANGLE of the Xclipse 530 of Android 14, after the device stopped marking its 32-bit float formats filterable, and the Adreno 630 of Android 9. On the PowerVR GE8320 of Android 13 the whole test project ran on OpenGL ES without a failure. Its compiler drew no glyph while the vertex stage of text ran the flips of sprites with flags that are always zero, and the text scene passes, checked by pixels, since text takes the rectangle of its glyph as it is. On the Mali-G72 of Android 10 the driver r19p0 dropped passes, which a flush at the end of each pass prevents, as the [driver issues](#driver-issues) say. That driver never finishes a timer query that spans such a flush, so the GPU time of OpenGL adds up a query around each pass, and every GPU test of OpenGL ES passed there. |
| WebGL2 | Web | Yes | Yes | In headless Chrome. No other browser. |
| WebGPU | Web | Yes | Yes | In headless Chrome on SwiftShader and on the Apple GPU of a Mac. No other browser. |

No device has met a real lost device on any API yet. The paths that replace a lost device run in code, but no GPU reset, removed adapter or lost context drove them. No physical iPad or Apple Vision Pro ran any device.

## Writing a device

A new graphics API of the repository gets a folder in `engine/src/gpu/<api>/` with a `device.cmake`, which adds its sources to `haylen-gpu` and defines `HAYLEN_GPU_<API>`, a value in `gpu::Device::Api` and `kApiNames`, its shader language in `gpu::Device::Language`, a case in `gpu::DeviceFactory`, a place in the default order of its platforms and its shader languages in `haylen_shader_languages_<api>` of `engine/cmake/haylen-gpu.cmake`, and its name in `TestCommand.GRAPHICS_APIS` of `tools/haylen/cli/commands/test.py`, which `tools/tests/repository/test_graphics_api_names.py` checks. The GPU tests then run the whole conformance suite and the reference images on it. A graphics API that the repository cannot hold, such as the API of a game console, comes through a [port](../platform/ports.md) instead.
