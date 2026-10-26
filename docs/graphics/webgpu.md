# 🕸️ WebGPU

This guide explains the WebGPU device of the [render hardware interface](rhi.md), which draws web apps in browsers that offer WebGPU. It covers how the device starts, how it draws a frame, the canvas it draws into, its shaders, what it can do, how to debug it, lost devices and its limits. Apps never talk to the device, since the renderers of the engine draw through the RHI.

## Choosing the device

Web builds compile the WebGPU device and the [WebGL2 device](opengl.md). The page tries WebGPU first and WebGL2 when WebGPU cannot start, such as in a browser without WebGPU. The address of the page of the web template picks the first API to try, and the loader hands it to the runtime as `Module.haylen.graphicsApi`:

```text
http://localhost:8080/?graphicsApi=webGl2
```

The device draws through the WebGPU C API of the port of Dawn for Emscripten, which the web build compiles against with `--use-port=emdawnwebgpu`. That port calls the WebGPU of the browser, so the page runs on the GPU process of the browser.

## Starting the device

A browser hands the adapter and the device of WebGPU over in a later turn of its event loop, never at once. So the platform layer calls `gpu::DeviceFactory::prepare` before the window opens. The first call starts the request, and the platform keeps running frames while it answers `false`. Once the browser answered, `gpu::DeviceFactory::create` makes the device on the connection the request made, or throws the reason the request failed, and the platform tries WebGL2.

The request follows the options of `gpu::DeviceFactory::Options`: `lowPower` asks for the adapter of low power instead of high performance, and `software` asks for the fallback adapter, such as SwiftShader in Chrome. The device asks for every limit that the adapter reaches, since the shaders of the engine bind more textures than the defaults of WebGPU allow, and for the optional features it uses where the adapter has them.

## How it draws a frame

The uniform blocks of a program live in bind group 0, with dynamic offsets into a ring of uniform buffers. The draws of a frame write their uniforms into memory of the CPU, and the frame sends each buffer of the ring with one queue write when it is submitted. Textures and samplers live in bind group 1, and the device keeps each group by the images and samplers it holds, so a frame that binds what an earlier frame bound creates no group. Groups that no frame used for a while go away, and so do the groups of a destroyed image or sampler.

Queue writes carry buffers and images to the GPU in the order of the frames, so a write never touches what an earlier frame still reads. WebGPU frees a destroyed object only once the GPU finished the frames that use it, so the device hands what the engine destroyed to WebGPU once the frame that may still record it is submitted. Buffers made with data start mapped, which hands their contents over without a copy.

## The canvas

The device draws into the canvas of the page, in the format that the browser prefers for it. A window with multisampling draws into a multisampled texture that resolves into the canvas, and the depth and stencil of the frames stay between the passes of a frame, so a pass that keeps the color keeps them too.

The browser presents the canvas when the frame returns to its event loop, at the pace of the display, so frames never run without vertical sync on WebGPU. A transparent app configures the canvas with premultiplied alpha, and the page shows through the transparent pixels. Configuring the canvas at a new size sets the size of its drawing buffer, and the attachments of the frames follow it.

## Shaders

Shader packages carry WGSL, the `wgsl` language, with uniform blocks in bind group 0 and textures and samplers in bind group 1. Programs that run the same source share its module. The browser compiles a module in its GPU process once it is made, so `prepareProgram` makes the modules while the app loads, and the layouts of bind groups are shared by the programs whose bindings match.

## Capabilities

| Capability | WebGPU |
| --- | --- |
| Largest image | `maxTextureDimension2D` of the adapter. |
| Array layers | `maxTextureArrayLayers` of the adapter. |
| Samples | 4, the count that WebGPU guarantees for multisampling. |
| Sample and draw into a format | Every format of the contract, and `Depth32FloatStencil8` only with the feature `depth32float-stencil8`. |
| Blend into a format | Every color format, and the 32-bit floats only with the feature `float32-blendable`. |
| Filter a format | Every color format, and the 32-bit floats only with the feature `float32-filterable`. |
| Separate blend for each color target | Yes. |
| GPU time of a frame | Timestamp queries at the start and end of every pass, with the feature `timestamp-query`. The time of a frame adds up its passes, and a frame whose buffer is still mapped goes unmeasured. |
| First row of a render target | The top row. |

## Debugging

The browser validates every call of WebGPU in every build. Errors that no scope captures reach the log of the engine as errors, and every object takes its label, which the developer tools of the browser show. The GPU tests run in a page of a headless Chrome:

```sh
haylen test --gpu --platform web
haylen test --graphics-api webGpu --platform web
```

## Lost devices

The browser loses the device of a page when the GPU process restarts or the GPU resets. The device reports it through `isLost`, a loss for an unknown reason reaches the log as an error, and the platform layer asks the browser for a new adapter and device through `gpu::DeviceFactory::prepare` before it starts the app again.

## Limits

- Frames always follow the display, since the browser presents the canvas.
- Queue writes take sizes in multiples of four bytes, so the device pads the last bytes of data of another size inside the buffer.
- A nearest filter samples one texel, since WebGPU allows anisotropic filtering only with linear filters.
- The device runs on the one thread of the web build, like the rest of the runtime.
