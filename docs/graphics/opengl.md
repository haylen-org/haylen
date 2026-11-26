# 🟩 OpenGL

This page covers the OpenGL device of the [render hardware interface](rhi.md): the graphics APIs it draws through, how it gets its context, how it runs shaders, uploads data, draws passes and reads pixels back, what it can do on each platform and its limits.

## The three flavours

The OpenGL device is one device class that serves three graphics APIs. It knows which one it runs, and that choice is its flavour.

| Flavour | Graphics API | Platforms | Shader language |
| --- | --- | --- | --- |
| `openGl` | OpenGL 4.3 core profile | Windows and Linux | `glsl430` |
| `openGlEs` | OpenGL ES 3.0 or later | Android | `glsl300es` |
| `webGl2` | WebGL2 | The web | `glsl300es` |

macOS has only OpenGL 4.1 and ships Metal, so OpenGL is not a graphics API of Haylen on macOS.

## The context

OpenGL draws through a context, and the pixel format of a window is chosen before the window opens. So the platform creates the context with the window and hands it to the device as a `gpu::GlContext` in a surface of the kind `Surface::Kind::Gl`. The device loads every OpenGL function it calls through `GlContext::getProcAddress`, so the engine links no OpenGL library and needs no loader library.

```cpp
gpu::Surface surface{.kind = gpu::Surface::Kind::Gl, .gl = &context, .size = {.width = 1280, .height = 720, .windowWidth = 1280, .windowHeight = 720}, .vsync = true};
std::unique_ptr<gpu::Device> device = gpu::DeviceFactory::create(gpu::Device::Api::OpenGl, surface, {.validation = true});
```

The context stays current on the frame thread for the whole life of the device. The device needs a context, so it cannot start without a surface. The tests of the engine make one with `test::OffscreenGlContext`, which opens an OpenGL or OpenGL ES context on a hidden pixel buffer through EGL on Linux and Android, with Mesa in a container or on the Android emulator, through WGL on Windows, and on the canvas of the page on the web.

When the window goes away, such as an Android app in the background, the platform keeps the context and calls `detachSurface`. A pass into the swapchain then records nothing. `attachSurface` takes the same context back with its new drawable, so no object is lost.

## Shaders

Shader packages carry GLSL code for each flavour. Each uniform block of a package is a uniform block of GLSL in the `std140` layout without a binding, so its bytes are the bytes that every other graphics API reads. When a program links, the device finds each block by its name and binds it to the binding point of its slot, and a block that the linker removed binds nothing.

```glsl
layout(std140) uniform haylen_vs_params {
    mat4 view_projection;
    vec4 premultiplied;
};
uniform highp sampler2D sprite_texture_sprite_sampler;
```

A texture and a sampler that a program reads together become one GLSL sampler, named by a pair of the package. Pair number `n` reads texture unit `n`. The vertex inputs take the locations of the package by their names before the program links.

OpenGL can only compile and link programs in its own context, which belongs to the frame thread. So `prepareProgram` does nothing, and a program links when `makeProgram` makes it.

## Uniform blocks

A uniform block reads either the range of a uniform buffer of the bindings, which `glBindBufferRange` binds at its offset, or the bytes that `applyUniforms` writes into the uniform ring of the frame. The ring holds chunks for each frame in flight at offsets of `GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT`. A frame that needs more room takes another chunk, so the ranges that the frame bound earlier keep their buffer, and bytes that come again for a slot in the frame reuse their range. The state cache skips the binds of a range that a binding point already holds, and a trim lets go of the chunks that the last frames left empty.

## Buffers and dynamic data

Immutable, dynamic and stream buffers take the hints `GL_STATIC_DRAW`, `GL_DYNAMIC_DRAW` and `GL_STREAM_DRAW`. OpenGL orders every write into a buffer with the draws, so a stream buffer is one buffer, and a write into a dynamic or stream buffer and a dynamic image take new data the same way. On OpenGL and OpenGL ES the data goes through an upload ring: one buffer with a region for each frame in flight. The CPU writes the region of the frame, and the GPU copies the data into the buffer with `glCopyBufferSubData` or into the texture from a pixel unpack buffer. A copy between buffers is a `glCopyBufferSubData` too.

- Where the context has buffer storage, the ring stays mapped the whole time, so a write is a plain copy in memory.
- Elsewhere the device maps the part it writes without waiting for the GPU, which is safe because of the fences below.
- WebGL2 cannot map buffers, so the device hands the data to `bufferSubData` and `texSubImage2D`, and the browser stages it.

The ring grows when a frame needs more room, and a frame that writes what the frame before wrote allocates nothing.

## Frames in flight

On OpenGL and OpenGL ES, three frames can be in flight. At the end of each frame the device places a fence. Before a frame writes its region of the upload ring, the device waits for the fence of the frame that used that region last. The wait for the display between frames waits for the fence of the frame before the last, so the CPU runs at most one frame ahead of the GPU and a frame never waits for a fence after it read its input, as [frame pacing](rhi.md#frame-pacing) describes. A frame that the GPU did not finish within ten seconds loses the device, since a context without robustness never reports a GPU that stopped. WebGL2 has no upload ring and places no fences, since the browser orders the frames of the page.

OpenGL keeps an object alive while a queued command still uses it, so `destroy` deletes objects at once and the GPU still finishes the frames that use them.

## Passes

An offscreen pass draws into a framebuffer object. The device makes one framebuffer for each set of attachments the first time the set comes, and keeps it until one of its images is destroyed.

- `Load::Clear` clears an attachment with `glClearBuffer`, with every channel open and no scissor.
- `Load::DontCare` and `Store::Discard` invalidate the attachment with `glInvalidateFramebuffer`, so tile GPUs neither load nor store it.
- A multisampled render target is a renderbuffer. It resolves into its image of one sample with `glBlitFramebuffer` when the pass ends.
- A pass with several color attachments draws into all of them with `glDrawBuffers`.

The depth and stencil of the window are invalidated before each swap, since no frame reads them back.

## Redundant state

Every call that changes OpenGL state goes through a state cache. The cache knows what the context holds, so the device never sends a call that would set the same value again. Each pipeline has its own vertex array object with the layout of its inputs. A draw that binds the same buffers at the same offsets sends nothing, and a new offset sends one call on contexts with vertex attribute binding, OpenGL 4.3 and OpenGL ES 3.1, or one call for each input of that buffer elsewhere.

## Reading pixels back

`readBuffer` copies a range of a buffer into another buffer behind a fence, which the device polls the same way, and hands the bytes over with `glGetBufferSubData` or, on OpenGL ES, through a map. A device that finds its context lost ends every pending copy with an error. `readImage` copies a render target into a pixel buffer object with `glReadPixels` and places a fence after it. `Readback::poll` checks the fence without waiting, which WebGL2 allows on the main thread, and copies the pixels out once the GPU passed it. The rows come from the bottom row up, since the first row of an OpenGL render target is its bottom row. Float targets come back as bytes from 0 to 255.

## Capabilities

| Capability | Where the device has it |
| --- | --- |
| Blend per target | On OpenGL, on OpenGL ES 3.2 and with `GL_OES_draw_buffers_indexed` or `GL_EXT_draw_buffers_indexed`. WebGL2 applies the blend and write mask of the first target to all of them. |
| Float render targets | Always on OpenGL. OpenGL ES and WebGL2 draw into 16-bit float targets with `GL_EXT_color_buffer_half_float` or `GL_EXT_color_buffer_float`, and into 32-bit float targets with `GL_EXT_color_buffer_float`. Blending into 32-bit float targets needs `GL_EXT_float_blend`, and filtering 32-bit float textures linearly needs `GL_OES_texture_float_linear`. |
| GPU time | On OpenGL, on OpenGL ES with `GL_EXT_disjoint_timer_query` and on WebGL2 with `EXT_disjoint_timer_query_webgl2`, as the sum of a query around each pass of the frame, so no query spans the flush that ends a pass. A frame whose timing the GPU disturbed is dropped, except on a driver of the [driver issues](rhi.md#driver-issues) that refuses to say so. |
| Lost context | Reported by `isLost` through `glGetGraphicsResetStatus` where the context has robustness, after a frame that the GPU did not finish within ten seconds, and on WebGL2 through the lost state of the WebGL context. |
| Multisampling | Render targets and depth stencil images of the counts of 2, 4 and 8 that `glGetInternalformativ` lists in `GL_SAMPLES` for the renderbuffers of each format. Multisampled images are resolved, never sampled. |
| Origin | The first row of a render target is its bottom row, so `originTopLeft` is `false`. Viewports and scissors still take rectangles from the top left. |
| Range of depth | The vertex stages write depth from 0 to 1 in clip space, as on every graphics API, and the device takes that range with `glClipControl`, of OpenGL 4.5, `GL_ARB_clip_control` or `GL_EXT_clip_control`, so reversed depth keeps its precision. A driver without clip control maps depth from -1 to 1, which `depthRange` reports as `NegativeOneToOne`, and the projections built for that range correct it. |
| Validation | With validation on, `KHR_debug` sends the messages of the driver to the log, errors as errors, and names objects and debug groups for GPU debuggers. Contexts without it, such as WebGL2, report errors through `glGetError` at the end of each pass and frame. |
| Cube arrays | OpenGL 4.3 and OpenGL ES 3.2, whose GLSL ES 3.20 the shader compiler writes for a program that samples a cube array, as the [RHI guide](rhi.md#cube-arrays-on-opengl-es) explains. WebGL2 has none. |
| Views of images | `glTextureView` of OpenGL 4.3 over the immutable storage that every image of one sample has, and on OpenGL ES with `GL_EXT_texture_view` or `GL_OES_texture_view`. WebGL2 has none. |
| Depth into later layers | `glFramebufferTextureLayer` of the depth attachment, everywhere but the OpenGL ES translator of the Android emulator on SwiftShader 4.0.0.1, whose draws into a later layer of a depth texture land in another layer, as the [driver issues](rhi.md#driver-issues) list. |
| Mip generation | `glGenerateMipmap`, for the formats that the context renders into and filters. |

## Limits

- `Bgra8` images are stored as RGBA. OpenGL takes their pixels in blue first order, and OpenGL ES and WebGL2 swap red and blue while staging them.
- The integer vertex formats `UByte4`, `Short2` and `UInt` feed integer inputs of shaders, and the normalized formats feed float inputs.
- WebGL2 has no debug groups or object names, so `pushDebugGroup` and `popDebugGroup` do nothing there.
- Programs sample at most 32 textures, the units that every flavour guarantees.
