# 🌋 Vulkan

This guide explains the Vulkan device of the [render hardware interface](rhi.md), which draws on Windows, Linux and Android. It covers what it needs from the machine, how it picks a GPU, how it draws a frame, its swapchains, how it runs shaders, what it can do, how to debug it, lost devices and its limits. Apps never talk to the device, since the renderers of the engine draw through the RHI.

## Choosing the device

Builds for Windows, Linux and Android compile the Vulkan device. Linux and Android try it first, and Windows tries it after Direct3D 12 and Direct3D 11. An app that wants Vulkan first names it in `window.graphicsApis` of `app.json`, as [choosing the graphics API](rhi.md#choosing-the-graphics-api) explains:

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

The device records each frame into two command buffers: one for the copies into buffers and images, and one for the passes, which runs after it. At most two frames run on the GPU, each behind a fence, and a frame waits for the fence of the frame that used its slot before.

- Uniforms go into a ring of buffers that stay mapped, and each draw passes the offsets of its blocks as dynamic offsets of one descriptor set, so a draw writes no descriptor.
- A dynamic buffer holds a copy for each frame in flight, and the first write of a frame moves to the next copy.
- The pixels of images go through staging memory that stays mapped, in chunks that each frame reuses once the GPU finished it.
- The descriptor sets of textures and samplers are made once for each choice of images and samplers, so a frame that binds what the frame before bound writes no descriptor.
- Memory comes from the Vulkan Memory Allocator, and released objects wait in a release queue for the frames that may use them.

Passes draw with dynamic rendering where the GPU has it, from Vulkan 1.3 or through its extension. Elsewhere the device makes a render pass for each choice of formats, sample counts and load and store actions, and a framebuffer for each set of views, once. Images rest in one layout, and a pass moves its attachments out of it with barriers and back when it ends.

The viewport of every pass flips the vertical axis, so clip space and the rows of render targets match Metal, Direct3D and WebGPU, and the first row of a render target is its top row.

## Windows and swapchains

The swapchain takes eight bits a channel without the conversion of sRGB formats, since the engine blends in the values it stores. A window with multisampling draws into a multisampled image that resolves into the swapchain image, and the depth and stencil of the window live in memory that tile GPUs never allocate, since no pass stores them. A swapchain pass that keeps what the pass before it drew keeps that depth, stencil and multisampled color too.

| Setting | What the device does |
| --- | --- |
| Vertical sync on | The FIFO present mode, which waits for the refresh of the display. |
| Vertical sync off | The mailbox mode, which replaces the waiting frame without tearing, or else the immediate mode, or else FIFO. |
| Transparent window | The premultiplied composite alpha of the surface, or the alpha that the surface inherits. A surface with neither logs a warning once and stays opaque. |
| New size | The swapchain is made again before the next frame. A swapchain that presents out of date or suboptimal is made again once. |
| Turned display | The images keep the size of the window as it shows, and the compositor turns them where the display turns, as on Android. |
| Window taken back | A surface that the window system took back, such as the window of an Android app in the background, gives no swapchain and the frames draw nothing until the platform hands over a new one. |

## Shaders

Shader packages carry SPIR-V modules, the `spirv` language, with the descriptor set and the binding of each slot. Uniform blocks take dynamic uniform buffers, and textures and samplers take separate descriptors. Making a shader module takes time on some drivers, so `prepareProgram` makes the modules on the I/O pool as soon as the engine knows a program, and `makeProgram` takes them. Programs that bind the same things share their descriptor set layouts and pipeline layout.

## Capabilities

| Capability | Vulkan |
| --- | --- |
| Largest image | `maxImageDimension2D` of the GPU. |
| Array layers | `maxImageArrayLayers` of the GPU. |
| Samples | The most samples that the GPU gives both color and depth attachments. |
| Sample, draw into, blend and filter a format | The format features that the GPU reports for each format. |
| Separate blend for each color target | Where the GPU has the feature `independentBlend`. |
| GPU time of a frame | Timestamp queries, where the queue writes timestamps. |
| First row of a render target | The top row. |

## Debugging

With validation on, the device turns on the validation layers when the machine has them, such as through the Vulkan SDK, and logs a warning that it runs without them otherwise. Errors of the layers and their warnings about broken rules become errors of the engine log, which fail the tests. Every object takes its label and debug groups become labels of the command buffer through `VK_EXT_debug_utils`, which RenderDoc and other GPU debuggers show.

```sh
haylen test --graphics-api vulkan
HAYLEN_TEST_GPU_SOFTWARE=1 haylen test --graphics-api vulkan
```

## Lost devices

The device is lost when a wait, a submit, an acquire or a present returns `VK_ERROR_DEVICE_LOST`. It then records nothing more, and readbacks that were running fail with a clear sentence. A lost surface, `VK_ERROR_SURFACE_LOST_KHR`, only stops the frames until the platform hands over a new window.

## Limits

- A GPU without dynamic rendering draws through render passes, which cost one render pass and one framebuffer for each new combination of attachments the first time it comes.
- Anisotropic filtering needs the feature `samplerAnisotropy`, which the device turns on wherever the GPU has it.
