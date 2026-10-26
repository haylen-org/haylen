# 🪟 Direct3D

This guide explains the two devices that draw on Windows through Direct3D: the device of Direct3D 12 and the device of Direct3D 11. It covers what each one needs from the machine, how each one draws a frame, how they show frames in a window, how they compile shaders, what they can do, how to debug them and their limits. Apps never talk to these devices. The renderers of the engine draw through the RHI contract, so an app draws the same way on every device.

## Choosing a device

A Windows build compiles both devices. The player tries Direct3D 12 first and Direct3D 11 next, and it moves on to the next graphics API when one cannot start on the machine. An app can change the order with `window.graphicsApis` in `app.json`:

```json
{
  "window": {
    "graphicsApis": ["direct3D11", "direct3D12"]
  }
}
```

Each device tries the GPUs of the machine in turn. It takes the most powerful GPU first, then the other GPUs, and last WARP, the software rasterizer of Windows. A machine without a GPU, such as a virtual machine of continuous integration, still runs both devices on WARP.

## What the machine needs

Both devices run on Windows 10 version 1903 and later, the oldest version that the engine supports, on any GPU of feature level 11_0 or later. Shader model 5.0 is the shader model of feature level 11_0, so older GPUs cannot run the shaders of the engine.

| Need | Direct3D 11 | Direct3D 12 |
| --- | --- | --- |
| GPU | Feature level 11_0 or later. | Feature level 11_0 or later. |
| Runtime | Direct3D 11.1 with constant buffer offsets, which every driver of Windows 10 has. | Root signatures of version 1.1 and render passes, which Windows 10 version 1809 and later have. |
| Shader compiler | `d3dcompiler_47.dll`, which Windows ships in its system folder. | The same compiler. |
| Transparent windows | `dcomp.dll`, which Windows ships in its system folder. | The same library. |

The devices load the shader compiler and DirectComposition when they start, so an executable never fails to open because one of them is missing. A device that cannot start says why in its error, and the player then tries the next graphics API.

## How the Direct3D 12 device draws a frame

The Direct3D 12 device records each frame into two command lists. The transfer list holds the copies into buffers and images and the copies out of render targets. The list of passes holds the passes and their draws. The transfer list runs first, so a pass always sees the data that its frame wrote.

The CPU runs at most two frames ahead of the GPU. A fence marks the end of each frame, and the device reuses the memory of a frame only once the GPU has finished it. Objects that the engine destroys wait for the same fence before they go, so the GPU never reads a destroyed object.

The device keeps the data of each frame in memory that the CPU writes and the GPU reads. That memory stays mapped, so writing uniforms and buffers costs one copy and no call to the driver. Uniform blocks bind as root constant buffer views that point into this memory. Textures and samplers bind as descriptor tables in heaps that the shaders read. When a frame binds the same textures twice, the second draw reads the table of the first one, so redundant bindings cost nothing.

The root signature of each program comes from the reflection of its shader package. Each uniform block becomes a root constant buffer view, and the textures and samplers of each stage become one descriptor table each. Programs with the same layout share one root signature, so switching between their pipelines keeps every binding.

Images rest in the state that shaders read. A pass moves its attachments into the states it draws in and back when it ends, and every pass is a render pass of Direct3D 12. A multisampled attachment resolves as its render pass ends, which lets tile GPUs resolve without writing every sample to memory.

## How the Direct3D 11 device draws a frame

The Direct3D 11 device draws on the immediate context. It writes uniforms into one large dynamic buffer and binds a window of that buffer to each draw, which Direct3D 11.1 allows. Each piece of data goes after the previous one without waiting for the GPU, and when the buffer is full the driver hands the device a fresh copy while the GPU keeps reading the old one. Writes into buffers go through a second buffer of the same kind and a copy on the GPU, so the GPU never waits for the CPU.

The device remembers what the context holds: the pipeline, the shaders, the states, the buffers, the textures and the samplers. A call that would set what the context already holds is skipped. Event queries keep the CPU at most two frames ahead of the GPU.

## Windows and swapchains

Both devices show frames through a flip model swapchain of three buffers. A window with vertical sync waits for the refresh of the display. A window without vertical sync presents at once and lets the display tear where it supports it, so frames reach the screen as soon as they are ready.

A flip model swapchain holds one sample per pixel. A window with multisampling draws into a multisampled image of its own, and the device resolves that image into the swapchain when it presents the frame.

A transparent window shows what lies behind it through the alpha of its pixels. Only a swapchain for composition keeps that alpha, so the devices show a transparent window through DirectComposition, which composites the frames over the desktop with premultiplied alpha. The window must have no redirection bitmap, which the platform asks for with the extended style `WS_EX_NOREDIRECTIONBITMAP`, or Windows draws an opaque background behind the frames.

A window that changes size resizes the swapchain before its next frame. A minimized window has no drawable, so its passes into the swapchain record nothing until the window comes back. The platform can let go of the window and give a device a new one, and the device keeps every object it made.

## Shaders

Shader packages carry HLSL for shader model 5.0, the `hlsl5` language, which both devices run. The devices compile it with `D3DCompile` when a program is made. Uniform blocks use the `b` registers, textures the `t` registers and samplers the `s` registers, and vertex inputs use the semantic `TEXCOORD` with the location as its index.

Compiling a shader takes time, so the engine asks the device to prepare programs as soon as it knows them. The device then compiles them on the I/O pool, and making the program later takes the result or waits only for what is left.

## Capabilities

| Capability | Direct3D 11 | Direct3D 12 |
| --- | --- | --- |
| Largest image | 16384 pixels a side. | 16384 pixels a side. |
| Layers of an array | 2048. | 2048. |
| Samples | The most that the GPU gives both 8-bit color and depth, up to 8. | The same. |
| Blend into a color format | Every color format that the GPU blends into, which the device reads from the driver. | The same. |
| Multisampled render targets with a resolve | Yes. | Yes, as part of the render pass. |
| Separate blend for each color target | Yes. | Yes. |
| GPU time of a frame | Timestamp queries inside a disjoint query. | Timestamp queries that the fence of the frame hands over. |
| First row of a render target | The top row. | The top row. |

## Debugging

The devices validate everything they do when validation is on, which debug builds and tests turn on. Validation turns on the debug layers of Direct3D and DXGI and moves every message they find into the log of the engine. Errors become errors of the log, which fail the tests, and warnings become warnings. The debug layers come with the optional feature Graphics Tools of Windows. On a machine without it, the device logs a warning that says how to add it and runs without validation.

On Direct3D 12, validation also turns on the breadcrumbs and page faults of removed devices. When the GPU stops working, the device logs the last operation each command list finished and the address of a page fault.

Every buffer, image, sampler, shader, pipeline and pass takes the label the engine gives it, and debug groups become markers. GPU debuggers such as PIX show those names in their captures.

## Lost devices

A device is lost when the GPU is removed or reset, or when its driver fails or updates. The device notices it when it presents a frame or checks the GPU at the end of each frame, and it logs the reason that Windows gives. The platform then makes a new device and starts the app again. A lost device never blocks: every wait for the GPU ends at once.

## Limits

- Dynamic images take their pixels for their first mip level, every layer at once.
- A multisampled attachment resolves into the first mip level and the first layer of its resolve image.
- A readback copies the first mip level and the first layer of a render target of one sample.
- The heap of samplers that shaders read holds 2048 descriptors, the most that Direct3D 12 allows, and a frame reuses the table of each set of samplers it binds, so a frame runs out of room only if it binds more than 2048 different samplers.
- Each buffer and image of the Direct3D 12 device has memory of its own, so making many small objects in one frame costs more than on Direct3D 11.
