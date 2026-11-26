# 🌋 Vulkan

This guide explains the Vulkan device of the [render hardware interface](rhi.md), which draws on Windows, Linux and Android. It covers what it needs from the machine, how it picks a GPU, how it draws a frame, its swapchains, how it runs shaders, what it can do, how to debug it, lost devices and its limits. Apps never talk to the device, since the renderers of the engine draw through the RHI.

## Choosing the device

Builds for Windows, Linux and Android compile the Vulkan device. Linux and Android try it first, and Windows tries it after Direct3D 12 and Direct3D 11. An app that wants Vulkan first names it in `window.graphicsApis` of `haylen.json`, as [choosing the graphics API](rhi.md#choosing-the-graphics-api) explains:

```json
{
  "window": {
    "graphicsApis": ["vulkan"]
  }
}
```

The device sorts the GPUs of the machine and takes the first one that runs Vulkan 1.1, has a queue that draws and presents to the window. Discrete GPUs come first, then integrated, virtual and software ones. The option `lowPower` of `gpu::DeviceFactory::Options` puts integrated GPUs first, and the option `software` puts the rasterizers that run on the CPU first, such as Mesa lavapipe.

## What the machine needs

The device opens the Vulkan loader of the system when it starts, through volk, so neither building nor starting the engine needs the Vulkan SDK. A machine without a loader, or with a loader older than Vulkan 1.1, makes the device fail with a clear sentence, and the platform layer tries the next graphics API.

| Need | Why |
| --- | --- |
| Vulkan 1.1 | The instance, the physical devices and the features that the device reads. |
| A queue that draws and presents | The device records uploads, passes and presents on one queue. |
| A surface of the window system | `VK_KHR_win32_surface` on Windows, `VK_KHR_xlib_surface` or `VK_KHR_wayland_surface` on Linux and `VK_KHR_android_surface` on Android. |

## How it draws a frame

The device records each frame into two command buffers: one for the copies into buffers and images, and one for the passes, which runs after it. At most two frames run on the GPU, each behind a fence, and a frame waits for the fence of the frame that used its slot before. A driver with `VK_KHR_present_id` and `VK_KHR_present_wait` gives every present an id, and the wait for the display between frames waits until the display showed the frame that each swapchain presented last. Without them, that wait takes the fence of the slot of the next frame instead, so a frame never waits for the GPU after it read its input, as [frame pacing](rhi.md#frame-pacing) describes.

- Uniforms go into a ring of buffers that stay mapped, and each draw passes the offsets of its blocks as dynamic offsets of one descriptor set, so a draw writes no descriptor.
- A stream buffer holds a mapped copy for each frame in flight, and the first write of a frame moves to the next copy. An immutable or dynamic buffer lives in GPU memory, and its first data is written directly or staged. The writes into a dynamic buffer and the copies between buffers are staged copies in the command buffer of the passes, with buffer memory barriers around them, which end a swapchain rendering left open. A readback of a buffer copies it through the readback queue of the images, in a submission of its own after the committed frames.
- The pixels of images go through staging memory that stays mapped, in chunks that each frame reuses once the GPU finished it.
- The descriptor sets of textures and samplers are made once for each choice of images and samplers, so a frame that binds what the frame before bound writes no descriptor.
- The descriptor set of the uniform blocks of a layout points, for each block, at a buffer of the uniform ring or at the uniform buffer of the bindings, whose offset is the dynamic offset of the block. A set goes with any of its buffers, when the ring destroys one after a burst or in a trim or when the engine destroys a uniform buffer, so a buffer that takes the same handle later never finds it. `readPools` reports the descriptor sets that the device holds as `descriptorSets`.
- Memory comes from the Vulkan Memory Allocator, and released objects wait in a release queue for the frames that may use them.

Passes draw with dynamic rendering where the GPU has it, from Vulkan 1.3 or through its extension. Elsewhere the device makes a render pass for each choice of formats, sample counts and load and store actions, and a framebuffer for each set of views, once. Images rest in one layout, and a pass moves its attachments out of it with barriers and back when it ends.

The viewport of every pass flips the vertical axis, so clip space and the rows of render targets match Metal, Direct3D and WebGPU, and the first row of a render target is its top row.

## Windows and swapchains

The swapchain takes eight bits a channel without the conversion of sRGB formats, since the engine blends in the values it stores. A window with multisampling draws into a multisampled image that resolves into the swapchain image, and the depth and stencil of the window live in memory that tile GPUs never allocate, since no pass stores them. A swapchain pass that keeps the contents and directly follows the swapchain pass of the same surface keeps that depth, stencil and multisampled color too, and after any other pass or a debug group between them they start anew, since a debug group between passes ends the render pass that the two would share, so a multisampled window keeps no color across an offscreen pass, as the [render hardware interface](rhi.md#passes-and-draws) says. An acquire of an image of the swapchain waits for at most a second, and a frame that gets none in that time draws nothing into the window, which a compositor that keeps every image of a hidden window can cause.

| Setting | What the device does |
| --- | --- |
| Vertical sync on | The FIFO present mode, which waits for the refresh of the display. |
| Vertical sync off | The mailbox mode, which replaces the waiting frame without tearing, or else the immediate mode, or else FIFO, which the log tells once, since the frames then wait for the refresh of the display. The method `vulkan::Device::getPresentMode(surface)` gives the mode that a surface presents with. |
| Transparent window | The premultiplied composite alpha of the surface, or the alpha that the surface inherits. A surface with neither logs a warning once and stays opaque. |
| New size | The swapchain is made again before the next frame. A swapchain that presents out of date or suboptimal is made again once. |
| Turned display | The swapchain takes the turn of the display as its pre-transform, with the images as wide as the window is tall for a quarter turn, as on an Android phone held across its panel. The window system then shows the images as they are, so it never composites the window with the GPU and every present matches the surface, which would otherwise answer suboptimal and make the swapchain again in every frame. The passes into the swapchain draw turned: the device turns their viewports and scissors, and the renderer turns the positions of their draws in clip space by `getPassTurns`, as the [render hardware interface](rhi.md#passes-and-draws) says. |
| Window taken back | A surface that the window system took back, such as the window of an Android app in the background, gives no swapchain and the frames draw nothing until the platform hands over a new one. |

## Shaders

Shader packages carry SPIR-V modules, the `spirv` language, with the descriptor set and the binding of each slot. Uniform blocks take dynamic uniform buffers, and textures and samplers take separate descriptors. Making a shader module takes time on some drivers, so `prepareProgram` makes the modules on the I/O pool as soon as the engine knows a program, and `makeProgram` takes them. Programs that bind the same things share their descriptor set layouts and pipeline layout.

## Capabilities

| Capability | Vulkan |
| --- | --- |
| Largest image | `maxImageDimension2D` of the GPU. |
| Array layers | `maxImageArrayLayers` of the GPU. |
| Largest volume and cube | `maxImageDimension3D` and `maxImageDimensionCube` of the GPU. |
| Samples | The counts of 2, 4 and 8 that the image format properties of the GPU give the attachments of each format. |
| Sample, draw into, blend and filter a format | The format features that the GPU reports for each format, with every use that the device gives the images of each kind: a render target is drawn into, sampled, copied into and read back. |
| Separate blend for each color target | Where the GPU has the feature `independentBlend`. |
| GPU time of a frame | Timestamp queries, where the queue writes timestamps. |
| First row of a render target | The top row. |
| Cube arrays | Where the GPU has the feature `imageCubeArray`, which the device turns on. Cubes and cube arrays are made cube compatible, so array views of their faces work. |
| Views of images | Image views of the range, and every image rests in one layout while a pass moves only the mip level and the layer that it draws into, so a pass samples a view of the other levels. |
| Mip generation | A chain of `vkCmdBlitImage` with a linear filter, level by level, for the formats whose optimal tiling has blit source, blit destination and linear filtering. |

## Debugging

With validation on, the device turns on the validation layers when the machine has them, such as through the Vulkan SDK, together with the messenger of `VK_EXT_debug_utils` that carries their messages, and logs a warning that it runs without them otherwise. A messenger that the instance refuses stops the device with the reason. Errors of the layers and their warnings about broken rules become errors of the engine log, which fail the tests. Every object takes its label and debug groups become labels of the command buffer through `VK_EXT_debug_utils`, which RenderDoc and other GPU debuggers show.

```sh
haylen test --graphics-api vulkan
HAYLEN_TEST_GPU_SOFTWARE=1 haylen test --graphics-api vulkan
```

## Lost devices

The device is lost when a wait, a submit, an acquire or a present returns `VK_ERROR_DEVICE_LOST`. It then records nothing more, and readbacks that were running fail with a clear sentence. A lost surface, `VK_ERROR_SURFACE_LOST_KHR`, only stops the frames until the platform hands over a new window.

## Limits

- A GPU without dynamic rendering draws through render passes, which cost one render pass and one framebuffer for each new combination of attachments the first time it comes.
- Anisotropic filtering needs the feature `samplerAnisotropy`, which the device turns on wherever the GPU has it.
