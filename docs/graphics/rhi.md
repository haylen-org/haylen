# 🧱 Render hardware interface

This guide explains the render hardware interface of Haylen, the RHI. It covers the layers between a Lua script and the GPU, how a frame flows through them, the contract of `gpu::Device` with its objects, passes, uploads, readbacks and lifetimes, surfaces and lost devices, capabilities, validation and profiling, what each graphics API can do, how an app chooses its API, and the status of every device on every platform. It is for people who work on the engine, write a device or port the engine to a new platform. App developers never touch the RHI, and the [rendering guide](rendering.md) covers what they draw.

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
| Platform layer | `engine/src/platform/sdl/`, `engine/src/platform/android/` | Opens the window, hands the device a `gpu::Surface` with the native handles of the window, runs the frames and tells the device when the window changes size, goes away or comes back. |

The [architecture guide](../core/architecture.md#layers) describes the platform layers, the player and the host around them.

### How a frame flows

1. The platform layer starts a frame and calls `Player::frame`, which runs `Engine::frame`.
2. Scripts and plugins record draws in `render` and `renderUi`. The renderer turns each draw into vertices or instances at once.
3. `Renderer::endFrame` sorts the canvases, merges draws into batches and writes the data of the whole frame with `writeBuffer`. It sends the pixels of every changed dynamic texture in one `writeImages` call.
4. The renderer records its passes: offscreen passes for render targets, lights and effects first, then the pass into the swapchain. Each pass applies pipelines, bindings and uniforms and draws.
5. `commit` submits the frame to the GPU and presents the swapchain.
6. `graphics::Device::collectGarbage` hands the objects released during the frame to `destroy`, and `Device::processReads` starts the readbacks that scripts asked for.

### Why scripts never see the API

A script calls functions such as `graphics2d.drawCircle` and `graphics.newRenderTarget`. Those functions take colors, sizes and design units, and the renderer decides what the GPU does. The shader packages carry the code of every shader language the build draws with, so a material works on every device. The renderer reads `gpu::Device::Capabilities` instead of the API, so a missing feature changes how the renderer draws, never what the script calls. The only trace of the API in Lua is its name in [`haylen.graphicsApi`](../lua-api/haylen.md#haylengraphicsapi), which a script may log or show.

```lua
local haylen = require('haylen')
local log = require('haylen.log')

log.info('This frame draws through ' .. haylen.graphicsApi .. '.')
```

## Objects

A device makes five kinds of objects. Each `make` method returns a small handle, a struct with a 32-bit `id`, and `id` 0 names nothing. Devices give each id a generation, so the handle of a destroyed object never finds the object that takes its place later.

| Object | Made with | What it is |
| --- | --- | --- |
| `Buffer` | `makeBuffer(BufferDesc)` | Vertices or indices. An immutable buffer keeps the data it was made with. A dynamic buffer takes new data each frame. |
| `Image` | `makeImage(ImageDesc)` | A flat image, an array, a cube or a volume, with mip levels and a pixel format. Its kind is a texture, a dynamic image, a render target or a depth stencil image. |
| `Sampler` | `makeSampler(SamplerDesc)` | Filters, wraps, levels of detail, anisotropy and an optional compare function. |
| `Program` | `makeProgram(ProgramDesc)` | The vertex and fragment stages in the language of the device, with the bindings that the reflection of the shader package gives. |
| `Pipeline` | `makePipeline(PipelineDesc)` | A program with its vertex layout, primitive, index type, blend and write mask for each color target, depth and stencil tests, culling and sample count. |

A device makes the native state object of a pipeline once, in `makePipeline`, and never makes one while it draws. A pipeline that draws into the swapchain takes the formats and the sample count of the swapchain, so the renderer never needs to know them.

Programs that the API compiles when they are made, such as Metal and Direct3D, start compiling early through `prepareProgram`. The engine calls it as soon as it knows a program, from any thread, and `makeProgram` later takes the result or waits only for what is left.

```cpp
const gpu::Device::Buffer quad = device.makeBuffer({.data = std::as_bytes(std::span(vertices)), .label = "quad"});
const gpu::Device::Image target = device.makeImage({.width = 256, .height = 256, .kind = gpu::Device::ImageKind::RenderTarget, .label = "minimap"});
```

## Passes and draws

A pass draws into attachments. A `PassDesc` that names color attachments draws offscreen, into up to four render targets at a mip level and layer of each, with an optional depth stencil image. A `PassDesc` that names no attachment draws into the swapchain of the window. Each attachment says how it loads, with `Clear`, `Keep` or `DontCare`, and how it stores, with `Store` or `Discard`. Tile GPUs skip memory traffic for `DontCare` and `Discard`. A multisampled color attachment resolves into its `resolve` image when the pass ends.

Inside a pass the renderer calls, in this order for each draw:

1. `applyViewport` and `applyScissor`, in pixels from the top left of the attachment.
2. `applyPipeline`.
3. `applyBindings`, with up to four vertex buffers and their offsets, an index buffer, up to 32 textures and up to 12 samplers.
4. `applyUniforms` for each uniform block of the program, up to eight, which copy their bytes into the frame.
5. `applyStencilReference` and `applyBlendColor` when the pipeline uses them.
6. `draw(first, count, instances)`.

Every device remembers what it sent last, so a binding, a pipeline or a value that does not change costs nothing. Debug groups wrap commands with `pushDebugGroup` and `popDebugGroup`, and GPU debuggers show their names.

## Uploads

Two calls bring data from the CPU into a frame:

- `writeBuffer(buffer, offset, data)` writes part of a dynamic buffer at an offset that is a multiple of four bytes. A frame writes each part once, before a pass binds the buffer, and the buffer holds in that frame only what the frame wrote.
- `writeImages(writes)` replaces the first mip level of dynamic images, every layer at once. A frame makes one call before its first pass, so the device stages every image before it copies any.

Each device moves this data through upload memory of its own for each frame in flight. That memory stays mapped where the API allows it, and it grows only when a frame needs more room, so a frame that writes what the frame before it wrote allocates nothing. The engine tests check this with `test::AllocationTracker`.

## Reading pixels back

`readImage(image, width, height)` starts copying the first mip level and layer of a render target after the frame that drew it was committed. It returns a `Readback`, whose `poll` gives nothing until the GPU finished the copy and then hands over four bytes a pixel, red, green, blue and alpha, whatever the format. The rows start with the first row that the device stores, which `Capabilities::originTopLeft` names. No device waits for the GPU to finish a copy. The [rendering guide](rendering.md#reading-pixels-back) shows how each API copies.

## Frames in flight and lifetimes

The CPU records a frame while the GPU still draws the frames before it. Each device keeps a fixed number of frames in flight behind fences or semaphores, and the next frame waits only when that many frames are still on the GPU.

| Device | Frames in flight |
| --- | --- |
| Metal | 3 |
| Direct3D 12 | 2 |
| Direct3D 11 | 2 |
| Vulkan | 2 |
| OpenGL, OpenGL ES and WebGL2 | 3 |
| WebGPU | The browser orders the frames through its queue. |

`destroy` never frees an object that a frame on the GPU may still use. Metal, Direct3D 12 and Vulkan keep it until the frames that may use it finished. Direct3D 11, OpenGL and WebGPU hand it to their API, which keeps it alive while queued commands use it. The engine itself destroys objects only between frames, through the graveyard of `graphics::Device`, and only on the frame thread.

## Surfaces

The platform layer hands a device a `gpu::Surface`: the kind of window, its native handles, the size of its drawable in pixels, the sample count, whether vertical sync is on and whether the window is transparent. A device made without a surface draws offscreen only, which the GPU tests use.

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

- `resizeSurface(width, height)` follows a new size of the drawable from the next frame. A size of zero, such as a minimized window, makes swapchain passes record nothing.
- `detachSurface` lets go of the window when the platform has none, such as an Android app in the background, and `attachSurface` takes the new window when it comes back.
- A swapchain that its API reports out of date or suboptimal is made again before the next frame.

A transparent window composites the alpha of its pixels as premultiplied where the platform allows it: through the `opaque` property of the layer on Apple platforms, DirectComposition on Windows with Direct3D, the composite alpha of the Vulkan surface and the alpha mode of the WebGPU canvas.

## Lost devices

A device is lost when the GPU resets or is removed, when its driver fails or updates, when the system revokes the GPU, or when an OpenGL or WebGL context is lost. `isLost` then answers `true`, and the device stops waiting for the GPU and recording work. The SDL platform layer checks it before each frame, logs which API stopped, makes a new device of the same API, on a new context for OpenGL, and starts the app again on it. The Android runtime does not check `isLost` yet, so it does not replace a device that is lost while the app runs. It makes a new context of OpenGL ES only when the next app starts and finds the old context lost. A lost surface is not a lost device: the platform calls `detachSurface` and `attachSurface` and the app goes on.

## Capabilities

`getCapabilities()` returns what the device and its GPU can do, read from the API when the device starts. The renderer reads these values and never uses a capability that a device lacks.

| Field | Meaning |
| --- | --- |
| `maxImageSize` | The largest side of a flat image in pixels. |
| `maxArrayLayers` | The most layers of an array image. |
| `maxSampleCount` | The most samples of a multisampled attachment. |
| `originTopLeft` | Whether the first row of a render target is its top row. |
| `independentBlend` | Whether each color target of a pipeline takes a blend and a write mask of its own. Without it, every target takes those of the first. |
| `multisampledRenderTargets` | Whether a pass can resolve a multisampled render target into an image of one sample. |
| `gpuTiming` | Whether `getGpuFrameSeconds` measures the time the GPU took for a frame. |
| `textureFormats` | The pixel formats that images can be made in and sampled. |
| `renderFormats` | The formats that render targets and depth stencil images can be made in. |
| `blendableFormats` | The color formats that pipelines can blend into. |
| `filterableFormats` | The color formats that linear filters can sample. |

For example, the light map of lit canvases is `Rgba16Float` only when that format is in `renderFormats` and `blendableFormats`, as the [lighting section](rendering.md#lighting) explains.

## Validation and profiling

The option `validation` of `gpu::DeviceFactory::Options` turns on the validation of the API. Debug builds of the player and every GPU test turn it on.

| Device | Validation |
| --- | --- |
| Metal | The Metal API validation layer through `MTL_DEBUG_LAYER`, and the execution status of each encoder in the error of a failed command buffer. |
| Direct3D 12 and 11 | The debug layers of Direct3D and DXGI, from the optional feature Graphics Tools of Windows, and on Direct3D 12 the breadcrumbs and page faults of removed devices. |
| Vulkan | The validation layers of the Vulkan SDK when the machine has them. |
| OpenGL and OpenGL ES | The debug output of `KHR_debug`, or `glGetError` after each pass where the context lacks it. |
| WebGL2 | `glGetError` after each pass and frame. |
| WebGPU | The browser validates every call in every build. |

Each device turns the messages of its API into lines of the engine log, errors as errors, and every error in the log fails a GPU test. Every object takes the `label` of its description, and debug groups become the markers of GPU debuggers.

For profiling, `getGpuFrameSeconds()` returns the time the GPU took for the last frame it finished, where `Capabilities::gpuTiming` is true, and `getPools()` reports how many objects of each kind exist. The debug overlay and [`debug.stats()`](../lua-api/debug.md) show both.

## The rules of the null device

`gpu::NullDevice` in `engine/src/gpu/NullDevice.hpp` is the device of the headless host. It draws nothing, and it throws `std::logic_error` on every call that breaks the contract, in every build, so the engine tests fail on a renderer that a real device would reject. In short, it checks that:

- Objects exist when they are used, and images fit their type, size, layers, mip levels, sample count, format and kind.
- Passes open and close in turn, with attachments of one size and sample count, and a pipeline draws only in a pass whose attachments match it.
- A draw comes inside a pass, after a pipeline, its bindings and every uniform block of its program, and reads only indices its buffer holds.
- A texture slot that filters never takes a format the device cannot filter, and a pass never samples its own attachment.
- Dynamic buffers and images take their data before the passes that read them, once a frame.
- Debug groups close inside the pass that opened them, and the surface goes and comes back in turn.
- Pools hold at most the sizes that `NullDevice.hpp` declares, so a leak of GPU objects fails a test.

The [testing guide](../tools/testing.md#the-headless-host) lists every check and how tests read what a frame did.

## The devices and their graphics APIs

| Device | Folder | Graphics API | Shader language | Platforms |
| --- | --- | --- | --- | --- |
| [Metal](metal.md) | `engine/src/gpu/metal/` | Metal | MSL for macOS, iOS and the iOS simulator | macOS, iOS, iPadOS, Mac Catalyst and tvOS |
| [Direct3D 12](direct3d.md) | `engine/src/gpu/d3d12/` | Direct3D 12 | HLSL for shader model 5.0 | Windows |
| [Direct3D 11](direct3d.md) | `engine/src/gpu/d3d11/` | Direct3D 11.1 | HLSL for shader model 5.0 | Windows |
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
| Samples | 8, 4 or 2, the most the GPU takes | Up to 8, the most the GPU gives color and depth | The most the GPU gives color and depth | `GL_MAX_SAMPLES` | `GL_MAX_SAMPLES` | `GL_MAX_SAMPLES` | 4 |
| Sample and draw into formats | Every format, `Depth24Stencil8` as `Depth32FloatStencil8` | From the GPU | From the GPU | Every format | Float targets with extensions | Float targets with extensions | Every format, `Depth32FloatStencil8` with its feature |
| Blend into 32-bit floats | From the A14 and M1 on, and on Intel Macs | From the GPU | From the GPU | Yes | With `GL_EXT_float_blend` | With `GL_EXT_float_blend` | With the feature `float32-blendable` |
| Filter 32-bit floats | Where the GPU supports it | From the GPU | From the GPU | Yes | With `GL_OES_texture_float_linear` | With `GL_OES_texture_float_linear` | With the feature `float32-filterable` |
| Independent blend | Yes | Yes | Where the GPU has the feature | Yes | On OpenGL ES 3.2 or with an extension | No | Yes |
| Multisampled render targets | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| GPU timing | Yes | Yes | Where the queue writes timestamps | Yes | With `GL_EXT_disjoint_timer_query` | With `EXT_disjoint_timer_query_webgl2` | With the feature `timestamp-query` |
| First row | Top | Top | Top | Bottom | Bottom | Bottom | Top |
| Vertical sync off | On macOS and Mac Catalyst | Yes, with tearing where the display allows it | Mailbox or immediate where the surface offers them | Swap interval 0 | Swap interval 0 | The swap interval that SDL hands the page | No, the browser paces the canvas |

The device pages explain each value: [Metal](metal.md#capabilities), [Direct3D](direct3d.md#capabilities), [Vulkan](vulkan.md#capabilities), [OpenGL](opengl.md#capabilities) and [WebGPU](webgpu.md#capabilities).

## Choosing the graphics API

A build compiles the device of every API of its platform, which the CMake list `HAYLEN_GRAPHICS_APIS` names, as the [build guide](../tools/build.md#cmake-options) describes. When the app starts, the platform layer tries the APIs in an order and keeps the first one whose device starts. It logs the API it chose and why each earlier one failed, such as a machine without Vulkan.

| Platform | Default order |
| --- | --- |
| macOS, iOS, iPadOS, Mac Catalyst and tvOS | Metal |
| Windows | Direct3D 12, Direct3D 11, Vulkan, OpenGL |
| Linux | Vulkan, OpenGL |
| Android | Vulkan, OpenGL ES |
| Web | WebGPU, WebGL2 |
| A port with a device of its own | The API `port`, then the APIs of the engine it compiles |

Three things change the order. Each one puts its APIs first, and the APIs of the build that it leaves out follow in the default order:

- The key `window.graphicsApis` of `app.json`, an array of names such as `["vulkan", "openGl"]`. A name that is not a graphics API stops the app with an error, and a name that the build lacks is skipped, so one list serves every platform.
- The option `--graphics-api <name>` of the player, which replaces the list of `app.json`.
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

Some APIs need other window flags than others, such as an OpenGL context or a Metal layer, so the platform layer opens the window again for each API it tries. WebGPU gets its adapter and device from the browser in a later turn of the event loop, so `gpu::DeviceFactory::prepare` starts the request and the platform layer keeps running frames until it is ready.

The options `software` and `lowPower` of `gpu::DeviceFactory::Options` ask a device for the software rasterizer of its API, such as WARP on Direct3D, Mesa lavapipe on Vulkan or the fallback adapter of WebGPU, or for the GPU that uses the least power. The GPU tests set them through `HAYLEN_TEST_GPU_SOFTWARE` and `HAYLEN_TEST_GPU_LOW_POWER`, as the [testing guide](../tools/testing.md#the-gpu-tests) describes. The player takes the GPU that each device prefers.

## Status of the devices

This table separates what is implemented, what compiles and what was tested, on each platform. "Tested" means that the GPU tests, the conformance suite and the reference images of the 2D renderer, passed there, unless the row says otherwise.

| Device | Platform | Implemented | Compiled | Tested |
| --- | --- | --- | --- | --- |
| Metal | macOS | Yes | Yes | On the Apple GPU of a Mac. No Intel Mac. |
| Metal | iOS and iPadOS | Yes | For devices and the simulator | An app ran through SDL on the iOS simulator. The GPU tests did not run, and no physical device ran it. |
| Metal | Mac Catalyst | Yes | Yes | An app ran through SDL. The GPU tests did not run. |
| Metal | tvOS | Yes | For devices and the simulator | An app ran through SDL on the tvOS simulator. No Apple TV ran it. |
| Metal | visionOS | Yes | For devices and the simulator | Not tested. The engine builds no visionOS slice, so visionOS runs the iPad app. |
| Direct3D 12 | Windows | Yes | Yes | On NVIDIA and Intel GPUs and on WARP. No AMD GPU. |
| Direct3D 11 | Windows | Yes | Yes | On NVIDIA and Intel GPUs and on WARP. No AMD GPU. |
| Vulkan | Windows | Yes | Yes | On an NVIDIA GPU. |
| Vulkan | Linux | Yes | Yes | On Mesa lavapipe, the software rasterizer. No hardware GPU. |
| Vulkan | Android | Yes | Yes | On the Android emulator. No physical device. |
| OpenGL | Windows | Yes | Yes | On an Intel GPU. |
| OpenGL | Linux | Yes | Yes | On Mesa llvmpipe, the software rasterizer. |
| OpenGL ES | Android | Yes | Yes | On the Android emulator. No physical device. |
| WebGL2 | Web | Yes | Yes | In headless Chrome. No other browser. |
| WebGPU | Web | Yes | Yes | In headless Chrome on SwiftShader and on the Apple GPU of a Mac. No other browser. |

No device has met a real lost device on any API yet. The paths that replace a lost device run in code, but no GPU reset, removed adapter or lost context drove them. No AMD GPU and no physical iOS, tvOS, visionOS or Android device ran any device.

## Writing a device

A new graphics API of the repository gets a folder in `engine/src/gpu/<api>/` with a `device.cmake`, which adds its sources to `haylen-gpu` and defines `HAYLEN_GPU_<API>`, a value in `gpu::Device::Api` and `kApiNames`, its shader language in `gpu::Device::Language`, a case in `gpu::DeviceFactory`, and a place in the default order of its platforms in `engine/cmake/haylen-gpu.cmake`. The GPU tests then run the whole conformance suite and the reference images on it. A graphics API that the repository cannot hold, such as the API of a game console, comes through a [port](../platform/ports.md) instead.
