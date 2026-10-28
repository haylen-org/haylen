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

A device is lost when the GPU resets or is removed, when its driver fails or updates, when the system revokes the GPU, or when an OpenGL or WebGL context is lost. `isLost` then answers `true`, and the device stops waiting for the GPU and recording work. The SDL platform layer and the Android runtime check it before each frame, log which API stopped, make a new device, on a new context for OpenGL and OpenGL ES, and start the app again on it. The Android runtime also counts an EGL context that reports itself lost as a lost device, and it makes the new device through the same order of APIs, so a Vulkan device that cannot start again gives way to OpenGL ES. A lost surface is not a lost device: the platform calls `detachSurface` and `attachSurface` and the app goes on.

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

The options `software` and `lowPower` of `gpu::DeviceFactory::Options` ask a device for the software rasterizer of its API, such as WARP on Direct3D, Mesa lavapipe on Vulkan or the fallback adapter of WebGPU, or for the GPU that uses the least power. The GPU tests set them through `HAYLEN_TEST_GPU_SOFTWARE` and `HAYLEN_TEST_GPU_LOW_POWER`, as the [testing guide](../tools/testing.md#the-gpu-tests) describes.

### Machines with two GPUs

Many laptops have two GPUs: an integrated GPU that saves power and a discrete GPU that draws faster. The player and the apps draw on the discrete GPU unless the app asks for the other one with the key `window.lowPowerGpu` of `app.json`, which sets the option `lowPower` of every device it makes.

```json
{
  "window": {
    "lowPowerGpu": true
  }
}
```

Direct3D, Vulkan and WebGPU list every GPU, and each device takes the one that the option asks for. OpenGL lists no GPU. On Windows, the drivers of NVIDIA and AMD pick the GPU of OpenGL for the whole process from two variables that the executable exports, `NvOptimusEnablement` and `AmdPowerXpressRequestHighPerformance`. Every executable of the engine on Windows exports them: the player, the apps that `haylen.py` and `haylen_add_app` build, and the test programs. The drivers read them when the process starts and again when the first context of OpenGL opens, and a 1 in either read takes the discrete GPU. So the executable exports them at 0, and `gpu::DeviceFactory::prepare` sets them before the first context opens: to 1 for an app that wants the discrete GPU, and left at 0 for an app with `lowPowerGpu`, so OpenGL draws on the integrated GPU as well. An executable that a project builds in another way compiles the file `src/gpu/GpuPreferenceExports.cpp` of the engine to get the same choice. The settings of the driver and the graphics settings of Windows, where a person picks the GPU of an app, come before what the app asks for.

## Driver issues

Some drivers have bugs that the tests on their GPUs showed, which the devices work around. The table `gpu::DriverIssues` of `engine/src/gpu/DriverIssues.cpp` lists them, each with the reason that the device logs. An entry names the graphics API, a piece of the name the driver gives its GPU and the versions that have the bug where they are known, as a piece of the version text of the driver or, where the API numbers its drivers, the last version number, so a newer driver that fixed it leaves the entry.

| Driver | Issue | What the device does |
| --- | --- | --- |
| Vulkan of the PowerVR Rogue GE8320, up to the driver 5776728 | Some pipelines fail with `VK_ERROR_OUT_OF_HOST_MEMORY`, and `vkCreateGraphicsPipelines` never returns for others. | The Vulkan device refuses the GPU, so the platform draws through OpenGL ES, the next API of the order, and logs why. |
| OpenGL ES of the Xclipse GPUs, through ANGLE | The driver names `GL_OES_texture_float_linear` but samples the 32-bit float formats with the nearest texel. | `Capabilities::filterableFormats` leaves the 32-bit float formats out. |
| OpenGL ES of the Mali-G72, driver r19p0 | Some draws of a pass that follows another pass whose commands the driver still holds reset the context as guilty, and the frame is lost, so a scene leaves its target untouched in some runs. | Every pass ends with `glFlush`. With it the renderer scenes passed 45 times out of 45 on the Mali-G72 of Android 10, where 23 runs out of 24 failed without it. |
| Vulkan of the Adreno 630, up to the driver 512.331 | A pass that loads the depth of a format without stencil, discards it and clears the stencil that the format has no plane for reads that depth as cleared. | A pass that keeps the depth of a format without stencil also stores the depth and keeps the stencil. With it the test of kept depth passed on the Adreno 630 of Android 9 with 32-bit and 16-bit depth. Without it the test failed with every other change tried there: the depth resting in its attachment layout or in the general layout, the image without the sampled usage or in transient memory, a pass between them that only keeps the depth, a barrier on all commands before each pass, and a store of the depth alone. |

A new entry comes with the evidence of a test on the GPU, a unit test in `DriverIssuesTest` and a row in this table.

Two problems that the AMD Radeon PRO V620 showed have no entry yet, since their cause is not known and no general workaround exists:

- On the OpenGL ES 3.2 context of the OpenGL driver of AMD 25.10 on Linux, the test `OpenGlDeviceTest.AllocatesNothingForAFrameThatRepeatsTheOneBeforeIt` gets three errors from the debug output in the frame that reuses the GPU timer queries of the first frame, and the log of those errors allocates. The driver gives no detail, since the context of the test is not a debug context, and a replay of the same calls through a Wayland window reports no error. The desktop OpenGL context passes the same test.
- The render benchmark stopped in its scene of shapes on the OpenGL driver of AMD: on Linux once in three runs, and on Windows in its one run with the driver 25.1.1. The same scene finished on Vulkan, Direct3D 11 and Direct3D 12, the renderer test of shapes passed on OpenGL, and the stress tours, which draw shapes in a window, finished on OpenGL on both systems.

## Status of the devices

This table separates what is implemented, what compiles and what was tested, on each platform. "Tested" means that the GPU tests, the conformance suite and the reference images of the 2D renderer, passed there, unless the row says otherwise.

| Device | Platform | Implemented | Compiled | Tested |
| --- | --- | --- | --- | --- |
| Metal | macOS | Yes | Yes, also for Intel Macs in the `macos-intel` job of CI | On the Apple GPU of a Mac, with the engine tests, the headless test project, the test port and the SDK check. No Intel Mac, since the Intel runners of GitHub have no Metal device, so CI runs only the engine tests there. |
| Metal | iOS and iPadOS | Yes | For devices and the simulator, with the GPU test app signed for an iPhone | On the iPhone and iPad simulators of iOS 27 with `haylen test --gpu --platform ios-simulator`, and on an iPhone 13, an A15 with iOS 27.0.1, with `haylen test --gpu --platform ios`: the GPU tests, the whole test project in Debug, turns between landscape and portrait, Tiny Island, an app in portrait and the protected release of the test project. No physical iPad. |
| Metal | Mac Catalyst | Yes | Yes | On the Apple GPU of a Mac with `haylen test --gpu --platform catalyst`. |
| Metal | tvOS | Yes | For devices and the simulator | On the Apple TV simulator of tvOS 27 with `haylen test --gpu --platform tvos-simulator`, and on the first Apple TV 4K, an A10X of the Apple3 family with tvOS 26.6, with `haylen test --gpu --platform tvos`: the GPU tests, a sprite batch whose last texture binds at slot 31, the whole test project in Debug, Tiny Island and the protected release of the test project in Release. |
| Metal | visionOS | Yes | For devices and the simulator, in the `xros` and `xros-simulator` slices of `Haylen.xcframework`, with the test project for the simulator | On the Apple Vision Pro simulator of visionOS 27 with `haylen test --gpu --platform visionos-simulator`, where the GPU tests passed, and with the whole test project in Debug, which ran 451 tests without a failure. No physical Apple Vision Pro, and no input reached the simulator, so gaze and pinch were not checked. |
| Direct3D 12 | Windows | Yes | Yes, with MSVC in one build with the other three APIs of Windows | On an NVIDIA GPU, on an Intel GPU with `HAYLEN_TEST_GPU_LOW_POWER=1` and on WARP with `HAYLEN_TEST_GPU_SOFTWARE=1`. The whole test project passed in a window. On the NVIDIA Tesla T4 of a cloud machine with Windows Server 2022 and the driver 596.86, with the debug layer, the benchmarks and the four stress tours in a window. On the AMD Radeon PRO V620 of a cloud machine, a quarter of the GPU that the host shares, with Windows Server 2025 and the driver 25.1.1 (32.0.12033.5037), with the benchmarks, the sprite benchmark and the four stress tours in a window, but without the debug layer, which the Graphics Tools of Windows did not install in 20 minutes. |
| Direct3D 11 | Windows | Yes | Yes, in the same build | On an NVIDIA GPU, on an Intel GPU and on WARP, as Direct3D 12. The whole test project passed in a window. On the NVIDIA Tesla T4 and on the AMD Radeon PRO V620 of cloud machines, as Direct3D 12. |
| Vulkan | Windows | Yes | Yes, in the same build | On an NVIDIA GPU, and on an Intel GPU with `HAYLEN_TEST_GPU_LOW_POWER=1`. The whole test project passed in a window. On the NVIDIA Tesla T4 and on the AMD Radeon PRO V620 of cloud machines, with the validation layers of the Vulkan SDK, the benchmarks and the four stress tours in a window. |
| Vulkan | Linux | Yes | Yes | On the NVIDIA Tesla T4 of a cloud machine with Ubuntu 24.04 and the driver 595.91, with the validation layers, the engine tests in a Release build, the benchmarks and the four stress tours in an Xvfb window, on the AMD Radeon PRO V620 of a cloud machine with Ubuntu 24.04 and the kernel 6.8, both on Mesa RADV 25.2.8 and on the Vulkan driver of AMD 25.10, with the validation layers, the benchmarks and the four stress tours in a Weston window, and on Mesa lavapipe, the software rasterizer, in a container, with the engine tests, the headless test project and the test port. |
| Vulkan | Android | Yes | Yes | On the Android emulator and on six phones and tablets. The GPU tests run from an installed test app, since a program that `adb shell` starts finds no Vulkan GPU on the Mali-G72 of Android 10. They passed on the PowerVR DXT-48-1536 and the Adreno 830 of Android 16, the Xclipse 530 of Android 14, the PowerVR GE8320 of Android 13 and the Mali-G72 of Android 10. The whole test project ran on Vulkan without a failure on the PowerVR DXT-48-1536, the Adreno 830, the Xclipse 530, the Mali-G72 and the Adreno 630 of Android 9. On the Adreno 630 every GPU test passed but the one of kept depth, which passes with the ops that the device now uses there, as the [driver issues](#driver-issues) say. A 32-bit depth with stencil still loses the depth that a pass keeps while it clears the stencil, which the 24-bit depth with stencil keeps, so that format stays a limit of that driver. On the PowerVR GE8320 the driver fails one pipeline of the test project with `VK_ERROR_OUT_OF_HOST_MEMORY` and stops for good inside `vkCreateGraphicsPipelines` at the test `CAM-013`, so the device refuses that driver. Device Farm refuses its Android 8.1 device, the Pixel 2 XL, because its own check finds the minimum SDK of 27 above that of the device, although Android 8.1 is API 27, so Android 8.1 ran on no device. |
| OpenGL | Windows | Yes | Yes, in the same build | On the GPU that the OpenGL driver of the laptop gives a hidden window. The whole test project passed in a window. On the NVIDIA Tesla T4 of a cloud machine, with the benchmarks and the four stress tours in a window, once the basic display adapter of the machine was turned off, since the OpenGL of a window on that adapter is the OpenGL 1.1 of Windows. On the AMD Radeon PRO V620 of a cloud machine, with the benchmarks, the sprite benchmark and the four stress tours in a window, where the render benchmark stopped in its scene of shapes, as the [driver issues](#driver-issues) say. |
| OpenGL | Linux | Yes | Yes | On the NVIDIA Tesla T4 of a cloud machine through EGL without a display, with the engine benchmarks, and on Mesa llvmpipe, the software rasterizer, through EGL without a display. On the AMD Radeon PRO V620 of a cloud machine with Ubuntu 24.04, on the OpenGL driver of AMD 25.10 through EGL without a display, with the benchmarks and the four stress tours in a Weston window. There every GPU test passed but one of the OpenGL ES context, and the render benchmark stopped once in three runs, as the [driver issues](#driver-issues) say. Mesa radeonsi did not run, since the OpenGL driver of AMD takes the EGL of the machine once it is installed. |
| OpenGL ES | Android | Yes | Yes | On the Android emulator and on six phones and tablets. The GPU tests passed on the PowerVR DXT-48-1536 and the Adreno 830 of Android 16, the ANGLE of the Xclipse 530 of Android 14, after the device stopped marking its 32-bit float formats filterable, and the Adreno 630 of Android 9. On the PowerVR GE8320 of Android 13 the whole test project ran on OpenGL ES without a failure. Its compiler drew no glyph while the vertex stage of text ran the flips of sprites with flags that are always zero, and the text scene passes, checked by pixels, since text takes the rectangle of its glyph as it is. On the Mali-G72 of Android 10 the driver r19p0 dropped passes, which a flush at the end of each pass prevents, as the [driver issues](#driver-issues) say. That driver never finishes a timer query that spans such a flush, so the GPU time of OpenGL adds up a query around each pass, and every GPU test of OpenGL ES passed there. |
| WebGL2 | Web | Yes | Yes | In headless Chrome. No other browser. |
| WebGPU | Web | Yes | Yes | In headless Chrome on SwiftShader and on the Apple GPU of a Mac. No other browser. |

No device has met a real lost device on any API yet. The paths that replace a lost device run in code, but no GPU reset, removed adapter or lost context drove them. No physical iPad or Apple Vision Pro ran any device.

## Writing a device

A new graphics API of the repository gets a folder in `engine/src/gpu/<api>/` with a `device.cmake`, which adds its sources to `haylen-gpu` and defines `HAYLEN_GPU_<API>`, a value in `gpu::Device::Api` and `kApiNames`, its shader language in `gpu::Device::Language`, a case in `gpu::DeviceFactory`, and a place in the default order of its platforms in `engine/cmake/haylen-gpu.cmake`. The GPU tests then run the whole conformance suite and the reference images on it. A graphics API that the repository cannot hold, such as the API of a game console, comes through a [port](../platform/ports.md) instead.
