# ⏱️ Benchmarks

This guide explains how the benchmarks of the engine measure the renderer on the GPU devices of a machine, how to run them on each graphics API and what their numbers mean. The [`bench` command](build.md#bench) of the build guide lists every benchmark and what it measures.

## Two ways to draw

Most benchmarks run the engine on the headless host, the host of the engine tests. By default it draws through the null GPU device, which takes every draw call and upload without a GPU. So the numbers show what the frame thread spends to record, sort, batch and submit a frame, and they are the same on every machine with the same processor.

The benchmarks that draw also run on a real GPU device of the build. The option `--graphics-api` names the graphics API, and the option `--gpu` runs the benchmark once on every API that the build compiled:

```sh
haylen bench --suite render --graphics-api vulkan
haylen bench --suite render --gpu
haylen bench --suite particles --gpu
```

The benchmarks that draw are `render`, `ui`, `particles`, `nodes`, `animation`, `physics` and `sprites`. The others measure the CPU alone, so they take neither option.

## How the headless benchmarks draw on a GPU

A headless benchmark that names an API makes the device of that API without a window. The device draws every frame of the engine offscreen: each pass that would draw into the window draws into a render target of the size of the framebuffer of the headless host, 1920 by 1080 pixels, with one sample. Every other pass, such as a render target or a lit scene, draws as it does in an app. The frames present nothing, and the device runs without the validation of its API, as an app of the Release configuration does. OpenGL draws through an offscreen context, the same one that the [GPU tests](testing.md#the-gpu-tests) use: a pixel buffer of EGL on Linux and a hidden window of WGL on Windows.

So the GPU does the real work of every frame, and the device waits for it as it does in a window, through its fixed number of frames in flight. A device that cannot start on the machine, such as Vulkan without a driver, prints the reason, and `--gpu` goes on with the next API.

The benchmark `sprites` is an app that opens a window of the player instead. The option `--graphics-api` reaches the player, which tries that API first and the others of the platform after it, so the first line of the output names the API that the window took.

## What the numbers mean

The first line of a headless benchmark names the graphics API and the GPU it draws through, such as `Graphics API "vulkan" on "llvmpipe (LLVM 19.1.7, 128 bits)"`, or the null device. Each table then has these columns:

| Column | Meaning |
| --- | --- |
| `Frame ms` | The average CPU time of a whole frame, from its start to the return of the submission. On a real device it includes the time the frame thread waits for the GPU when the GPU falls behind. So a frame bound by the GPU shows the time of the GPU here too. |
| `GPU ms` | The average time the GPU took for a frame, from the timestamps of the API, where the device measures it. A dash means the null device or an API that cannot measure it. The value of each frame arrives a frame or two late, so short measurements read a little of the frames before them. |
| `Average ms` | In the `particles`, `nodes`, `animation` and `physics` benchmarks, the part of the frame that the row measures, such as building the instances of the emitters. Their rows that draw whole frames fill `Frame ms` and `GPU ms` too, and the rows that do not draw show dashes there. |

The `render` benchmark also counts the allocations of the frame thread per frame. On a real device they include what the device and the driver of its API allocate through the allocation functions of C++. The Metal driver of Apple, for example, allocates two small blocks for every command buffer, which no device of the engine can avoid, so a scene that allocates nothing on the null device shows two allocations per frame on Metal.

## Software rasterizers

Machines without a GPU, such as containers and the runners of continuous integration, draw through software rasterizers: Mesa lavapipe for Vulkan and Mesa llvmpipe for OpenGL on Linux, WARP for Direct3D on Windows, and SwiftShader for WebGPU in a headless Chrome. They draw the same pixels as a GPU, but on the processor. So their numbers measure the processor and the rasterizer, not a GPU, and they only compare runs on the same machine. A report of such numbers names the rasterizer, which the first line of the output gives. The timer queries of OpenGL on llvmpipe measure only how long the commands take to reach the rasterizer, which draws them later on its own threads, and the first query of a context reports a time far longer than the run. So on llvmpipe the `GPU ms` of OpenGL means little, and `Frame ms` shows the cost of the frame.

To measure Linux in a container, run the benchmarks of a Linux build there with the Mesa drivers installed, such as the packages `mesa-vulkan-drivers` and `libgl1-mesa-dri` of Ubuntu:

```sh
haylen bench --suite render --gpu
```

## Comparing runs

The first line of each table names the GPU, which matters on a laptop with two GPUs. Direct3D and Vulkan take the most powerful GPU, while OpenGL on Windows draws on the GPU that the driver gives the process, often the integrated one. So the tables of two APIs can come from two GPUs of one machine.

Numbers compare well only on one machine at a time, in the Release build that `bench` makes, with the machine idle and plugged in. Compare `Frame ms` to see what each API costs the frame thread, and `GPU ms` to see what it costs the GPU. A frame whose `Frame ms` stays well above its `GPU ms` is bound by the CPU, and one whose `Frame ms` follows its `GPU ms` is bound by the GPU.
