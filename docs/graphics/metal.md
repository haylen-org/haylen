# 🍎 Metal

This guide explains the Metal device, the device of the [render hardware interface](rhi.md) on Apple platforms. It covers what it needs from the machine, how it draws a frame, how frames reach the window, how it compiles shaders, what it can do, how to debug it, lost devices and its limits. Apps never talk to the device, since the renderers of the engine draw through the RHI.

## Where it runs

The Metal device is the only device of Apple platforms. A build for macOS, iOS, iPadOS, Mac Catalyst or tvOS compiles it alone, so the default order of those platforms is just `metal`. The device lives in `engine/src/gpu/metal/`, in Objective-C++ with automatic reference counting.

```lua
local haylen = require('haylen')

assert(haylen.graphicsApi == 'metal', 'Apple platforms draw through Metal.')
```

## What the machine needs

The device runs on every GPU of the systems that the engine supports: macOS 13.3, iOS and iPadOS 16.3 and tvOS 16.3 and later. Those systems run only GPUs of the Apple families and, on macOS, the GPUs of Intel Macs. The platform layer hands the device a `CAMetalLayer`. On macOS and Mac Catalyst the device draws on the GPU that the layer prefers, which is the GPU of the display that shows the window, and elsewhere on the default GPU of the system.

## How it draws a frame

The device records each frame into one command buffer. The command buffer holds no references to the objects it uses, since the device keeps every object alive itself until the frame finished. A semaphore keeps at most three frames on the GPU, and a completion handler of each command buffer tells the frame thread that the GPU finished.

- Uniforms and the pixels of dynamic images pass through an upload ring of each frame, a buffer in memory that the CPU and the GPU share. A uniform block of up to 4096 bytes goes to Metal as bytes of the command, which costs less than a range of the ring.
- A dynamic buffer holds a region for each frame in flight. The CPU writes the region of its frame while the GPU reads the regions of the frames before.
- Immutable buffers live in shared memory on GPUs that share the memory of the CPU, and in the memory of the GPU elsewhere.
- Objects made with data copy it through a command buffer of their own, which runs before the frame that uses them.
- The encoder of a pass receives only the state that changes. A buffer that stays bound moves to its new offset alone, and pipelines with the same depth and stencil tests share one state object.
- Released objects wait in a graveyard until the GPU finished every frame that may use them.

## Windows and swapchains

Swapchain passes draw into the drawables of the `CAMetalLayer` of the window, one drawable for each frame in flight. On Apple GPUs the multisampled color and the depth and stencil of the window live in tile memory alone, through memoryless textures, since no pass stores them. A swapchain pass that keeps what the pass before it drew continues in the same encoder, so its color, depth and stencil stay in tile memory too. The iOS and tvOS simulators run on the GPU of the Mac, which has no tile memory, so there these attachments live in ordinary memory.

A transparent window makes the layer not opaque, and the system composites its premultiplied pixels. Vertical sync is off when the layer turns off `displaySyncEnabled`, which only macOS and Mac Catalyst have, so iOS, iPadOS and tvOS always present at the pace of the display. A new size of the drawable takes effect at the next frame, and the attachments of the old size wait in the graveyard for the frames that still draw them.

## Shaders

Shader packages carry three Metal languages: `msl-macos` for macOS, `msl-ios` for devices of iOS, iPadOS and tvOS and for Mac Catalyst, and `msl-ios-simulator` for the simulators. The device compiles the source of each stage into a Metal library when a program is made. That takes time, so `prepareProgram` starts compiling on the I/O pool as soon as the engine knows a program. Programs that run the same source share its library, and a program whose source still compiles waits only for what is left.

Uniform blocks take the buffer indices from 0 of each stage, and vertex buffers take the indices after them. Textures and samplers take the indices that the shader compiler gives them, which count from 0 in each stage.

## Capabilities

| Capability | Metal |
| --- | --- |
| Largest image | 16384 pixels a side, or 8192 on the A7 and A8. |
| Array layers | 2048. |
| Samples | 8, 4 or 2, the most that the GPU takes. |
| Sample and draw into a format | Every format of the contract. `Depth24Stencil8` takes `Depth32FloatStencil8`, the depth and stencil format of every Metal GPU. |
| Blend into a format | Every color format, except the 32-bit floats on Apple GPUs before the A14 and M1. |
| Filter a format | Every color format, and the 32-bit floats only where the GPU reports that it filters them. |
| Separate blend for each color target | Yes. |
| GPU time of a frame | Yes, from the start and end times of the command buffer of the frame. |
| First row of a render target | The top row. |

## Debugging

With validation on, the device sets `MTL_DEBUG_LAYER` before it makes its first device, which loads the API validation layer of Metal, and asks command buffers for the execution status of each encoder. A command buffer that fails names the pass that faulted in the log. Every object takes its label and debug groups become groups of the command buffer, which the frame capture of Xcode shows.

```sh
haylen test --graphics-api metal
```

## Lost devices

The device is lost when the system revokes the access of the app to the GPU, when the GPU fails inside its driver, and on macOS before macOS 27 when the GPU is removed, such as an external GPU that was unplugged. A command buffer that fails with one of those errors marks the device lost, and the platform layer replaces it.

## Limits

- The GPUs of the Apple TV HD and the first Apple TV 4K bind 31 textures in each stage, so the device refuses a program that binds a texture at a higher index there.
- Mac Catalyst apps run the `msl-ios` code, since they run the iOS slice of the engine.
- visionOS runs the iPad app, since the engine builds no visionOS slice.
