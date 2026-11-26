# 🍎 Metal

This guide explains the Metal device, the device of the [render hardware interface](rhi.md) on Apple platforms. It covers what it needs from the machine, how it draws a frame, how frames reach the window, how it compiles shaders, what it can do, how to debug it, lost devices and its limits. Apps never talk to the device, since the renderers of the engine draw through the RHI.

## Where it runs

The Metal device is the only device of Apple platforms. A build for macOS, iOS, iPadOS, Mac Catalyst or tvOS compiles it alone, so the default order of those platforms is just `metal`. The device lives in `engine/src/gpu/metal/`, in Objective-C++ with automatic reference counting.

```lua
local haylen = require('haylen')

assert(haylen.graphicsApi == 'metal', 'Apple platforms draw through Metal.')
```

## What the machine needs

The device runs on every GPU of the systems that the engine supports: macOS 14.0, Mac Catalyst 16.4, iOS and iPadOS 16.3 and tvOS 16.3 and later. Those systems run only GPUs of the Apple families and, on macOS, the GPUs of Intel Macs. The platform layer hands the device a `CAMetalLayer`. On macOS and Mac Catalyst the device draws on the GPU that the layer prefers, which is the GPU of the display that shows the window, and elsewhere on the default GPU of the system.

## How it draws a frame

The device records each frame into one command buffer. The command buffer holds no references to the objects it uses, since the device keeps every object alive itself until the frame finished. A semaphore keeps at most two frames on the GPU, one that the GPU draws while the CPU records the next, and a completion handler of each command buffer tells the frame thread that the GPU finished. The wait for the display between frames claims the slot of the next frame and takes the next drawable of every surface that presented, so a frame never waits for the GPU or for a drawable after it read its input, as [frame pacing](rhi.md#frame-pacing) describes.

- Uniforms and the pixels of dynamic images pass through an upload ring of each frame, a buffer in memory that the CPU and the GPU share. A uniform block of up to 4096 bytes goes to Metal as bytes of the command, which costs less than a range of the ring.
- A stream buffer holds a region for each frame in flight in shared memory. The CPU writes the region of its frame while the GPU reads the regions of the frames before.
- A dynamic buffer lives in private memory. Its writes go through the upload ring of the frame and a blit encoder between the render encoders, and a copy between buffers is a blit there too, so the hazard tracking of Metal orders them with the passes around them. Each one ends the swapchain encoder that a swapchain pass left open. An immutable buffer lives in shared memory on GPUs with unified memory and in private memory elsewhere. Metal buffers carry no usage flags, so every buffer serves every usage that the contract allows it.
- A uniform block that reads a uniform buffer binds that buffer at the buffer index of the block with the offset of its range. A readback of a buffer copies it on a command buffer of its own after the committed frames.
- Immutable buffers live in shared memory on GPUs that share the memory of the CPU, and in the memory of the GPU elsewhere.
- Objects made with data copy it through a command buffer of their own, which runs before the frame that uses them.
- The encoder of a pass receives only the state that changes. A buffer that stays bound moves to its new offset alone, and pipelines with the same depth and stencil tests share one state object.
- Released objects wait in a graveyard until the GPU finished every frame that may use them.

## Windows and swapchains

Swapchain passes draw into the drawables of the `CAMetalLayer` of the window, which keeps two of them: the one that the display shows and the one that the next frame draws. A drawable that the wait for the display took stays for the next frame that draws, and one of a size that the surface left goes back unshown. From macOS 14, iOS 17 and tvOS 17 on, the `CAMetalDisplayLink` of the layer of the window paces the frames of the window and offers its drawables, through `paceWindow`, as [frame pacing](rhi.md#frame-pacing) describes. On Apple GPUs the multisampled color and the depth and stencil of the window live in tile memory alone, through memoryless textures, since no pass stores them. A swapchain pass that keeps what the pass before it drew continues in the same encoder, so its color, depth and stencil stay in tile memory too. The iOS and tvOS simulators run on the GPU of the Mac, which has no tile memory, so there these attachments live in ordinary memory.

A transparent window makes the layer not opaque, and the system composites its premultiplied pixels. Vertical sync is off when the layer turns off `displaySyncEnabled`, which only macOS and Mac Catalyst have, so iOS, iPadOS and tvOS always present at the pace of the display. A new size of the drawable takes effect at the next frame, and the attachments of the old size wait in the graveyard for the frames that still draw them.

Debug builds of iOS, iPadOS, Mac Catalyst, tvOS and visionOS check at every frame that the view of the window covers the window and that its drawable has the pixels of the view, its bounds times its scale. When they part, `platform::SurfaceCheck` logs an error once, so a layout that leaves the view or the drawable behind the window shows in the log.

## Shaders

Shader packages carry three Metal languages: `msl-macos` for macOS, `msl-ios` for devices of iOS, iPadOS and tvOS and for Mac Catalyst, and `msl-ios-simulator` for the simulators. The device compiles the source of each stage into a Metal library when a program is made. That takes time, so `prepareProgram` starts compiling on the I/O pool as soon as the engine knows a program. Programs that run the same source share its library, and a program whose source still compiles waits only for what is left.

Uniform blocks take the buffer indices from 0 of each stage, and vertex buffers take the indices after them. Textures and samplers take the indices that the shader compiler gives them, which count from 0 in each stage.

## Capabilities

| Capability | Metal |
| --- | --- |
| Largest image | 16384 pixels a side, or 8192 on the A7 and A8. |
| Array layers | 2048. |
| Largest volume and cube | 2048 pixels along each side of a volume, and cubes as large as flat images. |
| Samples | 2, 4 and 8 where the GPU takes them, for every format, except the 32-bit floats, which resolve only on a GPU that filters them, as the feature set tables of Metal list. |
| Sample and draw into a format | Every format of the contract. `Depth24Stencil8` takes `Depth32FloatStencil8`, the depth and stencil format of every Metal GPU. |
| Blend into a format | Every color format, except the 32-bit floats on Apple GPUs before the A14 and M1. |
| Filter a format | Every color format, and the 32-bit floats only where the GPU reports that it filters them. |
| Separate blend for each color target | Yes. |
| GPU time of a frame | Yes, from the start and end times of the command buffer of the frame. |
| First row of a render target | The top row. |
| Cube arrays | Every Mac GPU and the Apple GPUs of the A11 and later, the families Apple4 and Mac2, which have cube texture arrays. The A8 to A10X of the families Apple2 and Apple3 have none. |
| Views of images | Texture views of the same pixel format, which change the type without a usage of their own, counting the faces of cubes as slices. |
| Mip generation | `generateMipmapsForTexture:` of a blit encoder of the command buffer of the frame, for every color format that the GPU renders and filters. |
| Depth resolves | The resolve texture, level and slice of the depth attachment with the filters `Sample0`, `Min` and `Max` of `MTLMultisampleDepthResolveFilter`, on the A9 and later, the family Apple3, and on every Mac, which the row of depth resolves of the feature set tables lists. The A8 and the simulators, which Apple documents as GPUs of the family Apple2, resolve no depth. |
| Transient attachments | Memoryless textures, `MTLStorageModeMemoryless`, which only render, on every Apple GPU, Apple silicon Macs and Mac Catalyst included. Intel Macs and the simulators, which keep depth and multisampled textures in private storage, make private textures. |

## Debugging

With validation on, the device sets `MTL_DEBUG_LAYER` before it makes its first device, which loads the API validation layer of Metal, and asks command buffers for the execution status of each encoder. A command buffer that fails names the pass that faulted in the log. Every object takes its label and debug groups become groups of the command buffer, which the frame capture of Xcode shows.

```sh
haylen test --graphics-api metal
```

## Lost devices

The device is lost when the system revokes the access of the app to the GPU, when the GPU fails inside its driver, and on macOS before macOS 27 when the GPU is removed, such as an external GPU that was unplugged. A command buffer that fails with one of those errors marks the device lost, and the platform layer replaces it.

## Limits

- The GPUs of the Apple TV HD and the first Apple TV 4K bind 31 textures in each stage, so the device refuses a program that binds a texture at a higher index there. The shader compiler numbers the textures of each stage from zero, so the sprite batch programs, whose last texture takes slot 31, bind it at index 7, and they draw on the A10X of the first Apple TV 4K.
- Mac Catalyst apps run the `msl-ios` code, since they run the iOS slice of the engine.
- visionOS runs the `msl-ios` code on Apple Vision Pro and the `msl-ios-simulator` code in its simulator, from the visionOS slices of the engine.
