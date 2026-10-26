# 🧪 Testing

This guide is for people who change the engine and need to prove that it works. Every capability of the engine that runs without a real graphics device or platform has GoogleTest tests, and every Lua binding is tested by running Lua through a headless engine. The guide explains how the suite is organized, how the headless host and the engine fixture work, and how to test a new binding. It also covers the commands `test`, `coverage`, `fuzz` and `format`, the test project, the tests of `haylen.py` and continuous integration.

## Layout

The suite lives in `engine/tests/` and builds into one executable, `haylen-tests`.

| Path | Contents |
| --- | --- |
| `engine/tests/<module>/` | The tests of one module of the engine, such as `core/EngineTests.cpp` or `io/IoTests.cpp`. |
| `engine/tests/2d/<module>/` | The tests of the 2D modules, which follow the `2d` folders of the engine: `animation`, `graphics` (the renderer, fonts, cameras and their Lua binding), `lighting`, `navigation`, `particles`, `physics`, `spatial` and `tiled`. |
| `engine/tests/lua/` | The binding tests of the core, math, input and assets modules, and of the Lua runtime and the binding toolkit. Other modules keep their binding tests next to their C++ tests, such as `Spatial2DLuaTest` in `2d/spatial/Spatial2DTests.cpp`. |
| `engine/tests/support/` | The shared helpers in `haylen::test`, which [the engine fixture](#the-engine-fixture) lists. |
| `engine/tests/fuzz/` | The entry points of the fuzzers of the content formats, of input recordings, of MessagePack and of image placeholders, which the option `HAYLEN_BUILD_FUZZERS` builds. |
| `engine/tests/data/fonts/` | Subsets of open fonts, with their licenses, that keep only the characters that the text, shaping and UI tests draw: Latin serif and CFF faces, Arabic, Hebrew, Devanagari, Thai, Japanese and symbols. CMake passes the folder to the tests as `HAYLEN_TEST_FONTS`, so the engine tests read no file of the samples. |
| `engine/tests/data/audio/` | Short tones made for the tests in Ogg Vorbis, in MP3 with and without the header that tells its length, and in FLAC, which the streaming tests decode. CMake passes the folder as `HAYLEN_TEST_AUDIO`. |
| `engine/tests/data/video/` | A short H.264 and AAC clip made for the tests, which the Windows tests decode through Media Foundation. CMake passes the folder as `HAYLEN_TEST_VIDEO`. |
| `engine/tests/native/` | The file `NativeTest.c`, a plain C library that the native interop tests load through `haylen.native` and Varn's `ffi`. The CMake target `haylen-native-test` builds it as the library `native-test` next to `haylen-tests` and the headless player, where `native.load` finds it by name, copies it next to the desktop player in `bin/haylen`, and passes its path to the tests as `HAYLEN_NATIVE_TEST_LIBRARY`. The Native libraries tests of the test project build the same source for every platform. |
| `engine/tests/gpu/` | The tests of the null device and of `gpu::DeviceFactory`, which `haylen-tests` runs, and the [GPU tests](#the-gpu-tests) of the real devices, which `haylen-gpu-tests` runs: the conformance suite of the contract, the reference scenes of the 2D renderer, their programs in `shaders/conformance.glsl` and their helpers in `support/`. |
| `engine/tests/data/gpu/` | The reference images of the scenes of the 2D renderer, one PNG per scene, which hold for every graphics API. |
| `engine/tests/backends/<backend>/` | The tests of a backend, which its `backend.cmake` names in `HAYLEN_BACKEND_TEST_SOURCES`, such as `backends/sokol/EventsTests.cpp`, which tests the translation of the events of `sokol_app`. |
| `engine/tests/CMakeLists.txt` | The list `HAYLEN_TEST_SOURCES`, which names every test file, and the target `haylen-tests`, which also compiles the tests of the backend of the build. |

The target `haylen-tests` links `haylen::engine`, `haylen::headless`, the libraries of the tests of the backend in `HAYLEN_BACKEND_TEST_LIBRARIES` and GoogleTest's `gtest_main`. It includes `engine/tests`, so tests include `support/EngineFixture.hpp`. It also adds `engine/src` to its own include folders, so a test may include internal headers such as `platform/headless/HeadlessHost.hpp`. Apps never see those folders, because the engine libraries keep them private.

The function `gtest_discover_tests` registers every test with CTest under its `Suite.Name`, with `engine/tests` as the working directory. Each test has a timeout of 120 seconds, far above the few seconds that the slowest test takes. So a test that hangs fails on its own, and the rest of the suite still runs.

On Windows the target also compiles `WindowsVideoDecoder` and `WindowsSettings` from the runtime, which need no window. So `WindowsVideoDecoderTest` decodes the clip of `data/video` with Media Foundation, and `WindowsSettingsTest` reads the accessibility settings of the user. The suite builds only on desktop platforms, and only when `HAYLEN_BUILD_TESTS` is on. That is the default when the engine or the root of the repository is the top-level project.

Every test file lives in the namespace of the context it tests, such as `haylen::input`, `haylen::storage` or `haylen::ui`. The shared helpers live in `haylen::test`, one class per file. A test file has no free functions or namespace-scope variables. Its helpers and constants are members of its fixtures, and suites that share them derive from the fixture that holds them. A suite takes the name of its subject with a `Test` suffix, such as `SceneManagerTest` or `SpatialLuaTest`. A test takes the name of the behavior it checks, such as `RequiresModulesFromThePackageOnly`.

## The headless host

The class `platform::HeadlessHost`, in `engine/src/platform/headless/`, implements the `platform::Host` interface of the engine without a window or a GPU. So the real engine loop runs in tests.

- The host draws through `gpu::NullDevice` (`engine/src/gpu/NullDevice.hpp`), the GPU device of the headless host. The renderer, textures, render targets and the UI run and record their statistics without drawing anything.
- The device checks every call against the contract of `gpu::Device` and throws `std::logic_error` on a broken one. So a test fails when the engine uses an object that does not exist, opens a pass inside another, draws outside a pass or without a pipeline, leaves a slot of the program unbound or hands a uniform block the wrong size. It also fails when the engine writes the dynamic images of a frame in more than one call or after its first pass, since a real device stages every image of a frame before it copies any of them.
- It checks the rest of the contract the same way. An image fits its type, layers, mip levels, sample count and format, and a texture brings the pixels of every layer of every level. The attachments of a pass share one size and sample count, a multisampled color resolves into a render target of one sample of its format, and a pipeline draws only in a pass whose attachments match its targets. A draw comes after the bindings and every uniform block of its pipeline, samples no attachment of its own pass and reads only the indices its buffer holds. A texture slot that filters takes only an image of a format that the device filters, and a sampler slot that does not filter takes no sampler with a linear filter, as WebGPU requires. A frame writes each part of a dynamic buffer once, before a pass binds it. Debug groups close in the pass that opened them, and the surface goes and comes back in turn between frames.
- Its capabilities say what the engine may use. By default it renders every format, blends into and filters all but the 32-bit floats and resolves multisampled render targets, but blends every color of a pipeline alike, as OpenGL ES does. A test that makes it with other `gpu::Device::Capabilities` sees how the engine behaves on a device that lacks a feature.
- Its pools limit how many images, buffers, samplers, programs and pipelines exist, like the pools of a real device.
- It keeps what the last committed frame did, which a test reads with `fixture.host().getGpu().getLastFrame()`. That is its passes, with whether each drew into the swapchain, its clear color, the write masks of its pipelines and its draws, and the image writes, their bytes and the buffer writes of the frame.
- A `gpu::NullDevice` that a test makes with a shader language, such as `gpu::Device::Language::Glsl300es`, asks every program for code in that language. So a test sees what a shader package lacks.
- The root module `haylen` reports `'none'` as its `graphicsApi` and `'headless'` as its `platform`.

Audio:

- The audio mixer of the headless host has no device and keeps time. It mixes the time of each frame into silence, so voices, fades and music end as they would on a device. A mixer that a test makes without a device mixes only when the test asks it to.
- Tests of a mixer with a device give it an `audio::OutputBackend` of their own. Such a backend refuses the audio, opens a device that never plays, or keeps the opening waiting the way an audio service that hangs does. So no test reaches the audio hardware.
- The method `setAudioOutput(backend)` gives the mixers of the engines created afterwards a device that opens through such a backend. The option `audioOutput` of the engine fixture sets it.

What the host records and changes:

- User data goes to a folder that the test chooses, and `getPersistCount()` counts the requests to make it durable.
- Bridge calls go to `getPlatformCalls()` with their JSON parameters and byte buffers, instead of reaching native code. The calls that the bridge gave up through a timeout or a cancel go to `getCancelledCalls()`.
- The methods `resize(size)`, `setSafeAreaInsets(insets)`, `setFold(fold)` and `setGamepad(index, state)` change what the engine sees. The methods `getTitle()`, `getCursor()`, `isCursorVisible()`, `isMouseLocked()`, `isKeyboardVisible()` and `isQuitRequested()` report what the engine asked for.
- The method `getNativeViews()` returns the `platform::NativeViews` of the headless screen. A test calls its `reserveInsets(key, insets)`, `releaseInsets(key)`, `coverApp()` and `uncoverApp()` from any thread, the way the native views of plugins do. The engine takes them at the start of the next frame, and they outlive the engines that a test restarts on the host.
- The method `setNativePlugins(ids)` sets the plugins whose native part the headless platform reports. The method `getErrorReports()` returns the JSON report of every error that stopped an app, in order.
- The method `setSystemInfo(info)` sets what the headless system reports to the apps that start afterwards. It starts as a desktop of the operating system that runs the tests, with the cores of the machine, and the null device names no GPU. The methods `setTheme(theme)` and `setBattery(battery)` change the theme and the battery from any thread, the way platform services report them. The engine publishes the change at its next frame.
- The method `openUrl` records the URLs in `getOpenedUrls()` and answers at once that an app took them, or that none did after `setOpensUrls(false)`. The method `getVibrations()` returns the seconds of every vibration.
- The method `enableDevelopment(folder)` gives the host a `platform::DevelopmentSession`, so the engines created afterwards run in development. With a folder, the session scans it for changes the way the player does. It outlives the engines that a test restarts on the host, like the session of the runtime.
- The method `setNetworkRequirement(sentence)` sets the sentence that the network errors of the apps that start afterwards end with, the way Android reports an app without the permission `INTERNET`.
- The method `getDialogRequests()` returns the id, the `DialogRequest` and the folder for copies of every native dialog that the engine asked for. The method `getCancelledDialogs()` returns the ids of the dialogs that it closed after a cancel, a timeout or the end of its app. A test answers a dialog with `engine.getDialogs().resolve(id, result)`, or from another thread through `DialogRelay::resolve`, the way platform code does.
- The method `getScreenRequests()` returns the `ScreenRequest` of every screen of a plugin that the engine handed to the platform, once it covered the app. The method `getCancelledScreens()` returns the ids of the screens it gave up. A test ends a screen from any thread through `ScreenRelay::finish(id, ok, resultJson, buffers)`. It restores the end of a screen of an earlier process through `ScreenRelay::restore`, the way platform code does. The screen that shows belongs to the process, like the covers of native libraries, so a test ends the screens it opened.

Only one engine runs at a time in a process. Creating a second one throws `Only one engine can run at a time. Destroy the engine that runs before creating another.`. So a test never creates two fixtures at once. CTest runs every test in a process of its own, so tests still run in parallel.

## The engine fixture

The class `haylen::test::EngineFixture` is a running engine on the headless host, with a package in memory.

```cpp
explicit EngineFixture(std::map<std::string, std::string> files = {}, std::unique_ptr<core::Application> application = nullptr);
EngineFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application, Options options);
```

The argument `files` maps package paths to their contents. Unless the map has them, the fixture adds an `app.json` with the name `Test App` and the identifier `com.haylen.tests`, and an empty `source/main.lua`. It reads the configuration, creates the engine with a `lua::Application`, or with the given application, and starts it. User data goes to a temporary folder, which the fixture removes afterwards.

The struct `EngineFixture::Options` has three options:

- `development` turns on development on the host before the engine starts, as the player does with `--dev`. A test changes paths with `fixture.package().setFile`, queues them with `fixture.host().getDevelopmentSession()->addChanges(paths)`, and the next frame applies them.
- `developmentServer` names the WebSocket address of a development server that the session connects to. The package then plays under an `io::OverlayPackage` that takes the files the server sends, as the runtime of the web plays it.
- `audioOutput` gives the mixer a device that opens through an `audio::OutputBackend` of the test.

| Member | Purpose |
| --- | --- |
| `engine()`, `host()`, `package()` | The `core::Engine`, the `platform::HeadlessHost` and the `io::MemoryPackage`, to drive the engine and check its state. |
| `lua()` | The `lua_State*` of the engine. |
| `restart()` | Replaces the engine with a new one that runs the package with a Lua application on the same host, as the runtime restarts an app. What the host keeps, such as the covers of the app, outlives it. |
| `frames(count, seconds)` | Runs `count` frames of `seconds` each, one sixtieth of a second by default. |
| `frameUntil(condition, timeout)` | Runs frames until `condition` returns `true` or the timeout passes, ten seconds by default. It sleeps one millisecond between frames, so worker threads can finish. It returns whether the condition was met. |
| `lua(source)` | Runs a chunk named `test` and returns its first result converted with `tostring`, or `error: ` followed by the message when it fails. |
| `runLua(source)` | Runs a chunk and throws `std::runtime_error` when it fails, for setup code. |

The folder `support/` holds the other shared helpers.

| Helper | Purpose |
| --- | --- |
| `TestApplication(onStart)` | An application whose `start` runs the given function, for tests that add plugins or push C++ scenes, in `support/TestApplication.hpp`. |
| `DrawingScene(draw)` | A scene whose `render` runs the given function inside a real frame, in `support/EngineFixture.hpp`. |
| `RecordingScene(name, log, transparent, processMode)` | A scene that writes every hook it receives to a shared log as `name:hook` and counts its updates and renders, in `support/RecordingScene.hpp`. It loads at once, through a deferral that the test completes, or with a failure, and preloads the asset groups it lists. Its function `onHook` runs with the entry of every hook right after the log gets it, so a test can clear the stack or fail the hook in the middle of a change. |
| `RecordingEffect(switchAt, exitAt)` | A transition effect with the given switch and exit points, which records the progress and the images it draws, in `support/RecordingEffect.hpp`. |
| `TemporaryDirectory` | A unique folder under the temporary folder of the system, removed afterwards, with `getPath()` and `write(relative, content)`, in `support/TemporaryDirectory.hpp`. |
| `bytes(text)`, `pngImage(width, height, rgba)`, `randomBytes(size, seed)` | Test data: raw bytes, a PNG image of one color, and bytes that look random but are the same for the same seed, in `support/TestFiles.hpp`. |
| `ReleaseFixture` | Builds the protected release of an app package with fixed test keys through `content::ReleaseBuilder`. Each build goes into a temporary folder of its own, which `getFolder()` names, and reuses the release of the build before. It opens the release as one package with `open()`. In `support/ReleaseFixture.hpp`. |
| `CountingPackage(inner)` | Passes a package through and counts the bytes that its readers read and the largest single read, so a test shows which parts of a file an operation touches. With `watch(thread)` it counts the reads of one thread, so a test shows that the thread that mixes audio never reads. In `support/CountingPackage.hpp`. |
| `ContentServer`, `LimitedStoreFiles`, `QuotaStoreFiles`, `CrashingStoreFiles`, `ServiceKeyProvider`, `StorePackSource`, `PrivateFiles` | The places where the content system meets the world, in `support/`: a local server that serves ranges and misbehaves on purpose, store files on a device with little room, store files in a browser that runs out of quota, store files that lose power during a chosen write, which reaches the disk only in part, a key provider that hands out keys on request from a thread of its own, a source of packs like the plugin of a store, and the check that a file opens for its owner alone. |
| `AllocationTracker` | Counts the allocations of the current thread while it lives, with the largest one and the total, through the global allocation functions that the test program replaces. So a test shows that an operation never allocates a whole file or package. With the checked iterators of the debug library of MSVC, which allocate a proxy for every container, it leaves out the allocations of the size of that proxy. On Apple platforms it also leaves out what the GPU driver of the system and the validation layer of Metal allocate for every command buffer, which no device of the engine can avoid, so the GPU tests check the allocations of the engine alone. On Linux it leaves out what the Vulkan loader, its validation layers, the drivers and the compiler of software drivers allocate, which reach the allocation functions of the test program too, from the first caller outside the program and the C++ library. On the web it leaves out the handles that the port of WebGPU makes in the memory of the program for every command encoder, pass and command buffer, which it finds through the functions of the port on the stack of the allocation. In `support/AllocationTracker.hpp`. |
| `EventLoop::yield()` | Gives the event loop of the platform a turn while a test waits, such as for a copy of a render target. In the browser that turn settles the promises of WebGPU, and on other platforms it is a short pause. In `support/EventLoop.hpp`. |
| `ContentParsers::parse(input)` | Feeds bytes to the parser of the content formats that the first byte picks, and lets only content errors through, for the hostile input tests and the fuzzer, in `support/ContentParsers.hpp`. |
| `VarnRuntime` | Owns a Varn runtime without an engine, which `getRuntime()` returns. Its `pumpUntil(condition, timeout)` advances the event loop the way the engine does once per frame. The `JobSystem` tests use it. In `support/VarnRuntime.hpp`. |

The fixture of a test file builds the data that only that file needs, such as the zip archives of the package tests and the WAV files of the audio tests. Files that code cannot build live in `engine/tests/data/`, such as the fonts and the tones of the audio tests.

## Testing Lua bindings

A binding test runs Lua through the fixture and checks what matters to an app: returned values, engine state, error messages and results that arrive later.

- Compare returned values as strings. The call `fixture.lua("return hash.size")` returns `"3"`, and a float returns `"32.0"`.
- Check validation with the message that an app would see, as in `EXPECT_NE(fixture.lua("spatial2d.newHashGrid(-2)").find("positive cell size"), std::string::npos)`.
- Check callbacks that fail through `fixture.engine().getError()`, the `lua::Error` that the error screen shows, with its message, position and frames.
- Advance frames for anything asynchronous. Promises settle and coroutines resume during a frame, so a test waits with `frameUntil`.
- Check engine state through `fixture.engine()` and `fixture.host()` when a binding changes the engine, such as the window title or a platform call.

The test `Spatial2DLuaTest.StoresLuaValuesByBounds` in `engine/tests/2d/spatial/Spatial2DTests.cpp` is a compact model for a synchronous module. This test covers `jobs.spawn`, whose promise resolves in a later frame:

```cpp
#include <gtest/gtest.h>

#include <string>

#include "support/EngineFixture.hpp"

namespace haylen::core {

TEST(JobsLuaTest, ResolvesSpawnedJobsInLaterFrames) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        local async = require('async')
        local jobs = require('haylen.jobs')
        async.spawn(function()
            total = jobs.spawn(function(limit)
                local sum = 0
                for i = 1, limit do
                    sum = sum + i
                    jobs.checkpoint()
                end
                return sum
            end, 100):await()
        end)
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return total"), "nil");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return total") != "nil"; }));
    EXPECT_EQ(fixture.lua("return total"), "5050");
    EXPECT_NE(fixture.lua("require('haylen.jobs').spawn(42)").find("function expected"), std::string::npos);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::core
```

To add a test for a new binding:

1. Add the test to the file of its module, or create `engine/tests/<module>/<Module>Tests.cpp` and add it to `HAYLEN_TEST_SOURCES` in `engine/tests/CMakeLists.txt`.
2. Cover the results, the argument errors, the failures of callbacks and every asynchronous path. Write tests for behavior that an app relies on, not tests that restate the implementation.
3. Wrap multi-line Lua sources and multi-line lambdas in `// clang-format off` and `// clang-format on`, as the example does.
4. Run the tests, the coverage report and the formatter, as the next sections describe, and document the binding in its page `docs/lua-api/<module>.md`.

## Running the tests

```sh
haylen test
haylen test --config Release --jobs 4
```

The command `test` builds `haylen-tests` for this machine and runs CTest with `--output-on-failure` in parallel. It configures `build/<host>-<config>` when needed, as the [build trees](build.md#build-trees) describe. It takes the [build options](build.md#build-options), of which `--config` (`Debug` by default), `--jobs` and `--sanitizers` matter here. With `--platform web` or `--platform web-webgl2` it runs [the memory check of the web](#the-memory-check-of-the-web) instead, and with `--gpu` on the web or on Android it runs [the GPU tests](#the-gpu-tests) there.

Run one suite or one test through CTest or through the executable:

```sh
ctest --test-dir build/macos-debug -R SpatialLuaTest --output-on-failure
build/macos-debug/bin/haylen-tests --gtest_filter='SpatialLuaTest.*'
```

## The GPU tests

The executable `haylen-gpu-tests` runs on the real GPU devices of the build, while `haylen-tests` runs on the null device. It links `haylen::gpu`, and the tests take each graphics API that the build compiled as a parameter, so each test runs once per device and takes the name of its API, such as `GpuBlendTest.BlendsWithEveryFactor/vulkan`.

```sh
haylen test --gpu
haylen test --graphics-api vulkan
HAYLEN_TEST_GRAPHICS_API=metal build/macos-debug/bin/haylen-gpu-tests --gtest_filter='GpuTextureTest.*'
HAYLEN_TEST_GPU_SOFTWARE=1 build/windows-debug/bin/haylen-gpu-tests.exe
```

The option `--gpu` of `haylen test` builds the executable and runs it on every device of the build, and `--graphics-api` with a name of `gpu::Device::kApiNames` runs it on that device alone, through the environment variable `HAYLEN_TEST_GRAPHICS_API`. CTest does not run these tests, so `haylen test` without them runs only `haylen-tests`.

- The fixture `test::GpuFixture` creates the device of its API offscreen with the validation of the API on, through `GpuFixture::makeDevice`, which waits until `gpu::DeviceFactory::prepare` says the API is ready and then calls `gpu::DeviceFactory::create`. The environment variable `HAYLEN_TEST_GPU_SOFTWARE=1` asks the devices for the software rasterizer of their API, such as WARP on Direct3D or the fallback adapter of the browser on WebGPU, so a machine with a GPU runs the tests as a machine of continuous integration without one does. The variable `HAYLEN_TEST_GPU_LOW_POWER=1` asks them for the GPU that uses the least power, such as the integrated GPU of a laptop that has two. A device whose API has no such choice ignores them. The OpenGL APIs draw into the context of a hidden drawable that `test::OffscreenSurface` makes, which the OpenGL device brings to the tests through `HAYLEN_GPU_TEST_SOURCES`, an EGL pixel buffer on Linux and Android, a hidden WGL window on Windows and the canvas of the page on the web. The OpenGL device adds its own tests of its state cache, shader programs, upload ring and frames, which run on OpenGL and OpenGL ES wherever the machine makes their contexts. A device that cannot start on the machine skips the test with the reason, such as a machine without Vulkan.
- Every error that reaches the log while a test runs fails it, through `test::LoggedErrors`. The devices log the messages of the validation of their API as errors, so a call that the API rejects fails the test.
- The conformance suite draws into render targets and reads them back with `GpuFixture::read`, which commits frames until the copy finishes and hands the rows with the top row first. It covers clears, viewports and scissors, kept passes, four color targets, the resolve of multisampled targets, mip levels and layers of render targets, triangles, strips, lines, indexed and instanced draws, uniforms per draw, dynamic buffers, every blend factor and operation, write masks, independent blending, depth, stencil, comparison samplers, every pixel format the device marks, the linear filtering of every format it marks filterable, arrays, volumes, cubes, mip levels, filters, wraps, levels of detail, dynamic images, objects destroyed while their frames run, frames that allocate nothing, GPU timing and programs prepared ahead. It also makes every program and variant of the shader packages of the renderer on each device, so the code the shader compiler writes for its language compiles and fits the bindings of the device.
- The reference scenes run a real engine through `test::EngineFixture` with the option `gpuDevice`, which draws the engine through the device instead of the null device. Each scene draws shapes, sprites, text, lights and shadows, particles, post-processing or render targets into a render target, reads it back and compares it with its PNG in `engine/tests/data/gpu` through `test::ImageComparison`, within a small tolerance for the edges that APIs rasterize apart.

The references come from the first device that draws the scenes right, and every other device matches them. To write them, run the GPU tests with `HAYLEN_GPU_REFERENCES=write`, which writes the image of each scene to its reference and names it in the output, then look at every image before you commit it. A run without the variable never writes a reference. A missing reference fails its test, and a scene that differs keeps the image it drew and the difference in `gpu-test-output` of the build tree.

```sh
HAYLEN_GPU_REFERENCES=write HAYLEN_TEST_GRAPHICS_API=metal build/macos-debug/bin/haylen-gpu-tests --gtest_filter='GpuRendererTest.*'
```

On the web, `haylen test --gpu --platform web` builds the GPU tests in `build/web-gpu-tests-debug` and runs them in the page `engine/tests/web/gpu-tests.html` of a headless Chrome, whose WebGPU draws through SwiftShader on a machine without a GPU for it. The browser hands over the adapter of WebGPU and the copies of render targets only in a turn of its event loop. So the tests wait through `test::EventLoop::yield`, which gives the browser that turn through the promise integration of WebAssembly. The page preloads the reference images at their paths on the machine that built it, and the driver `engine/tests/web/gpu-tests.mjs` prints what the tests print and exits with their status.

```sh
haylen test --gpu --platform web
haylen test --graphics-api webGpu --platform web
haylen test --graphics-api webGl2 --platform web
```

On Android, `haylen test --gpu --platform android` builds the GPU tests for the ABI of the device in `build/android-gpu-tests-<abi>-debug`, pushes them with the reference images into `/data/local/tmp/haylen-gpu-tests` of the device and runs them there through adb. It takes the device that `--device` names or the only one connected. Without any, it starts an emulator of the first virtual device of the Android SDK without a window and shuts it down when the tests end. The scenes that differ come back into `gpu-test-output` of the tree, and the folder of the device goes away afterwards.

```sh
haylen test --gpu --platform android
haylen test --graphics-api openGlEs --platform android --device emulator-5554
```

## The memory check of the web

```sh
haylen test --platform web
```

A web page reads content through a worker and keeps records in memory within a budget, which only a browser shows. So this check runs one:

1. It copies the app `engine/tests/data/web-memory` into `build/tests/web-memory`.
2. It writes a file of 16 MiB and one of 384 MiB of random bytes into its content, which `app.json` delivers on demand.
3. It builds the release site of the app in the Release configuration and serves it.
4. It opens the page in a headless Chrome: the one that the environment variable `HAYLEN_CHROME` names, or else the one of the machine.
5. The driver `engine/tests/web/memory-check.mjs`, which the Node of the Emscripten SDK runs, follows the page and its workers over the debugging protocol. Meanwhile the app prefetches the large file and reads a small asset. The driver reports the bytes they received, the peak of the memory of WebAssembly, and the array buffers that they keep after a collection.

The check fails when the page did not download the file, when the memory of WebAssembly grew past half of the file, or when more than 64 MiB of array buffers stay. A page that holds what it downloads would do that.

Then it opens the page again in a browser whose page finds no private file system, which the driver removes before the page loads. That page must keep the file of 16 MiB in memory and refuse the large one for want of room, or the check fails. An Emscripten SDK without Node stops the check with a message to install it again with `haylen tools --emsdk`.

## The test project

The app `samples/tests`, Haylen Tests, holds a test of every feature of the engine, from sprites to plugins, in categories with a code per test, as [its README](../../samples/tests/README.md) lists. A person runs it on every platform, with `haylen run samples/tests` on the desktop. Its menu runs one test at a time, or every test one after the other.

The headless player runs it without a window, as every platform runs it, so a broken test shows up without a device. The Native libraries tests load the test library of the engine, which the target `haylen-native-test` builds next to the headless player and the desktop player:

```sh
haylen build --target haylen-headless
haylen build --target haylen-native-test
build/macos-debug/bin/haylen-headless samples/tests --frames 60000
```

The executable `haylen-headless` builds with the player on desktop platforms. It runs any app package on the headless host at 60 frames per second of real time, so asynchronous work such as decoding and file reads finishes as it does on a device. It prints the log. It stops when the app quits, when an error stops an app that cannot recover, or after the frames of `--frames`. With `--dev` it runs a package folder in development, like the player. So a file saved while it runs reloads as [hot reload](../core/lua.md#hot-reload) describes, and the log shows what each save did.

The headless player exits with 1 when an error stopped the app, even one that the app went back from, or when the app did not quit in time. Otherwise it exits with the status that the app quit with, which `haylen.quit({status = 1})` sets and which is 0 by default, as the [reference](../lua-api/haylen.md#haylenquitoptions) describes.

On the headless platform the test project runs every test that supports the platform for 2 seconds each:

- It writes `[CODE] Running`, `[CODE] Passed`, `[CODE] Failed: <message>` or `[CODE] Failed to load: <message>` for each test. A check of the results of a test that settles on a failure fails the test too.
- It goes back from every error with `haylen.recover()`.
- It ends with a summary, and quits with the status 1 when any test failed.

So the headless player exits with 1 when a test raised an error, failed a check or its scene failed to load. A test that needs a person, a device or a native part that the headless host does not load lists `headless` among its unsupported platforms in its manifest.

## Sanitizers

```sh
haylen test --sanitizers address
haylen test --sanitizers thread
```

The option `--sanitizers` builds the tests with sanitizers in a tree of its own, `build/<host>-<config>-address` or `build/<host>-<config>-thread`, so switching never builds the plain tree again. It sets `HAYLEN_SANITIZERS`, which MSVC builds ignore.

| Value | Sanitizers | What they instrument |
| --- | --- | --- |
| `address` | AddressSanitizer and UndefinedBehaviorSanitizer | The engine libraries, the tests and the other targets of the engine, which catch memory errors and undefined behavior in the code of the engine. |
| `thread` | ThreadSanitizer | Every C and C++ target of the build, dependencies included, because ThreadSanitizer sees only the synchronization of the code it instruments, such as the atomics with which libuv wakes its event loop. OpenSSL builds with its own scripts and stays uninstrumented. |

AddressSanitizer and ThreadSanitizer fail the test whose process raised a report. UndefinedBehaviorSanitizer prints its reports and lets the test go on. ThreadSanitizer runs the tests several times slower, and `TSAN_OPTIONS`, such as `TSAN_OPTIONS=halt_on_error=1`, tunes it. ThreadSanitizer cannot follow the `_longjmp` with which a yield of Lua jumps back, so those builds tell Varn to keep the yields of Lua as exceptions.

## Fuzzing

```sh
haylen fuzz
haylen fuzz --seconds 600 --llvm /opt/homebrew/opt/llvm/bin
```

A fuzzer feeds a parser with inputs that libFuzzer generates, to find the inputs that crash it. The command `fuzz` builds the fuzzers of the parsers that read untrusted bytes and runs each one for a while.

| Option | Default | Meaning |
| --- | --- | --- |
| `--seconds` | `60` | How long each fuzzer runs. |
| `--llvm` | the variable `HAYLEN_LLVM`, or else the LLVM of a package manager | The `bin` folder of an LLVM whose Clang links libFuzzer. Without either, the command looks in `/opt/homebrew/opt/llvm/bin`, `/usr/local/opt/llvm/bin` and `/usr/lib/llvm-<version>/bin` for the versions 20, 19 and 18. Apple Clang does not ship libFuzzer. |
| `--jobs` | the processors minus one | The parallel jobs of the build. |

The command configures `build/fuzz` in the Debug configuration with that Clang and `HAYLEN_BUILD_FUZZERS`, which instruments every target for libFuzzer and AddressSanitizer. It builds the four fuzzers and runs each on inputs of at most 64 KiB. Each fuzzer has a corpus of its own in `build/fuzz/corpus/<fuzzer>`, which keeps the inputs that reached new code for the next run. A crash stops the command with the input that caused it.

| Fuzzer | What it checks | Tests that drive the same code on every run |
| --- | --- | --- |
| `haylen-content-fuzzer` | The parsers of the content formats treat every file as hostile. The first byte of an input picks the chunk record, the shard header, the shard index, the catalog, the manifest or the channel descriptor, and the parser may reject the rest only with a content error. | The hostile input tests in `engine/tests/content/HostileInputTests.cpp`, with random bytes and damaged copies of valid files. |
| `haylen-input-fuzzer` | The reader of input recordings may reject an input only as a malformed recording. | `InputRecordingTest`, with truncated, trailing and oversized inputs. |
| `haylen-messagepack-fuzzer` | It decodes each input with `core::MessagePack` and encodes the result again, which may fail only as bytes that are not valid MessagePack. | `MessagePackTest`, with truncated, trailing, deeply nested and oversized messages. |
| `haylen-placeholder-fuzzer` | It decodes each input as an image placeholder of `graphics::ImagePlaceholder`, which may fail only as a text that is not a placeholder. | `ImagePlaceholderTest`, with empty, cut, padded, foreign and oversized texts. |

## Coverage

```sh
haylen coverage
haylen coverage --jobs 4 --sanitizers address
```

Coverage uses the source-based coverage of LLVM, so it needs Clang. The command `coverage` works in these steps:

1. It configures `build/coverage` in the Debug configuration with `HAYLEN_ENABLE_COVERAGE=ON` and without the player.
2. It builds `haylen-tests` and runs every test, which writes one profile per process into `build/coverage/coverage/`.
3. It merges the profiles with `llvm-profdata`, prints the report of `llvm-cov` and writes an HTML report to `build/coverage/coverage/html/index.html`.

The report leaves out the dependencies, the tests themselves, generated files and the platform folders in `engine/src/platform/` that need the SDK of a platform: `apple`, `android`, `web`, `windows` and `linux`. On macOS the LLVM tools come from Xcode through `xcrun`, and elsewhere they must be on `PATH`. The command also takes `--jobs` and `--sanitizers`. Coverage of the engine stays as close to 100 percent as the code allows.

## Formatting

```sh
haylen format
haylen format --check
```

The command `format` runs `clang-format --style=file` on every `.h`, `.hpp`, `.c`, `.cpp`, `.m` and `.mm` file under `engine/include`, `engine/src`, `engine/tests`, `engine/tools`, `samples` and `templates`. It skips build outputs inside those folders, such as the `.cxx`, `.gradle`, `build` and `_deps` folders. It also skips the files of the plugin template whose names hold a token such as `{{NAME}}`, which become code only once `plugin new` replaces their tokens.

Then it lists every multi-line lambda outside a `// clang-format off` and `// clang-format on` region, because `clang-format` cannot lay out such lambdas well. The option `--check` changes nothing. It fails when a file is not formatted or a lambda lacks its markers. The command `clang-format` must be on `PATH`, and continuous integration installs version 23.1.3 with `pip install clang-format==23.1.3`.

## The rules of haylen.py

```sh
python3 -m unittest discover -s tools -p "test_*.py"
```

The module `tools/test_haylen.py` tests the rules that need no build, with the `unittest` module of Python. It covers:

- How `haylen.py` merges the `Info.plist` keys, the entitlements and the privacy manifests of the developer, the plugins and `app.json`, and how `check` decides that a built app holds what a plugin needs.
- How it writes `haylen.properties`, the CMake values, the resource scripts, the status bar and the splash screens, and when `run` generates `App.xcodeproj` again, keeps it or compares it with a trial generation.
- How it reads its arguments, such as the app folder of `run` and the options of `sdk --check-consumers` and `android-key`, how its launchers run it, and how its terminal output marks reserved expressions, paths and URLs with and without color.
- How it provides the pinned shader compiler, XcodeGen and Emscripten, how it builds only the stale engine artifacts, and how the layers of the templates, the desktop projects, `run-cpp`, `plugin new` and native libraries behave.
- That every tracked file and folder uses dashes where nothing requires an underscore, and so does every target, project and install component of the CMake files and of the CMake text that `haylen.py` writes.
- That only the folder of a backend reaches its libraries and headers, that no Lua binding knows a backend or a host, and that comments and the texts of the interface are written as the project asks.
- That the animated sheets and the images of the samples hold their pivots and have clean edges.
- The documentation, through `DocsTest`: the titles, the languages of code blocks, one line per paragraph, every relative link and its heading, the tables of the indexes, and that no long sentence repeats in another page.

## Continuous integration

The workflow `.github/workflows/ci.yml` runs on every push to a branch and on every pull request. Every commit of `main` gets a complete run of its own. On any other branch or pull request, a newer push cancels the run in progress. The workflow only tests, so no job uploads artifacts or publishes anything, and tags and releases start no run. A GitHub release of the repository is its source archive alone, since every project compiles the engine from its sources, or builds the prebuilt engine of its apps with `haylen engine` on its own machine.

Every job runs `haylen.py` with `--verbose`. The variable `CPM_SOURCE_CACHE` points at `.cache/cpm` of the workspace, and the build jobs cache that folder with a key made of the hash of `engine/cmake/haylen-dependencies.cmake`. The `web` and `release` jobs also cache `.tools/emsdk`, with a key made of the hash of `haylen.py`.

| Job | Runner | What it does |
| --- | --- | --- |
| `format` | Ubuntu | Installs clang-format 23.1.3, runs `format --check` and the tests of [the rules of haylen.py](#the-rules-of-haylenpy). |
| `desktop` | macOS, Ubuntu and Windows | Installs Ninja, with the development packages of X11 and OpenGL on Linux and the MSVC environment on Windows. Runs `test --config Debug`, then `sdk --config Release --check-consumers package`, which builds the SDK and the consumer project `engine/tests/consumer` that finds it with `find_package`. |
| `coverage` | macOS | Runs `coverage`, whose report table shows in the log. |
| `web` | Ubuntu | Installs Ninja and the development packages of X11 and OpenGL, which the content tool of the release needs. Runs `engine --platform web`, which builds the prebuilt WebGPU and WebGL2 player, then `test --platform web`, the [memory check of the web](#the-memory-check-of-the-web) in the Chrome of the runner. |
| `android` | Ubuntu | Installs Java 25, NDK 30.0.16248370, CMake 4.1.2, the Android 37.2 platform, Ninja and the development packages of X11 and OpenGL. Runs `engine --platform android`, which builds the Android libraries into the local Maven repository. Then it creates the starter app with `new build/starter`, gives it an upload key with `android-key build/starter` and runs `run build/starter --platform android --config Release --build-only`. That builds the protected release with the content tool of the desktop artifacts, the library of the app for every ABI and the signed APK, and checks it for anything a release never holds. |
| `apple` | macOS | Installs Ninja and runs `engine --platform apple`, which builds `Haylen.xcframework`. Then it creates the starter app and runs `run build/starter --platform ios-simulator --config Release --build-only`, which builds and checks its protected release for the iOS simulator. |
| `desktop-apps` | Ubuntu and Windows | Installs Ninja, with the development packages of X11 and OpenGL, `dpkg-dev`, `desktop-file-utils` and the newest CMake on Linux, and the MSVC environment on Windows. Creates the starter app with `new build/starter`. Adds to it a plugin that `plugin new build/echo-check` creates from the plugin template, with `plugin add build/echo-check --app build/starter`. Runs `dist build/starter --output build/dist` for the platform, which builds the desktop artifacts, the C library of the plugin, the starter app from the Linux or Windows template in the Release configuration, and its `.deb` and AppImage or its installer. Then it checks the installed app with `check --config Release`, and on Linux validates its desktop entry with `desktop-file-validate`. Last it creates the starter C++ app with `new build/starter-cpp --kind cpp`, builds it with `run-cpp build/starter-cpp --build-only`, which compiles the engine with the app, and again with `--config Release`, which ships and checks its protected release. |
| `sanitizers` | Ubuntu | Runs `test --sanitizers address` and `test --sanitizers thread` in two jobs, which build and run the engine tests with AddressSanitizer and UndefinedBehaviorSanitizer, and with ThreadSanitizer. |
| `fuzz` | Ubuntu | Installs Clang with its runtime libraries and runs `fuzz --seconds 60 --llvm /usr/bin`, which builds the fuzzers and runs each for a minute. |
| `tests-project` | Ubuntu | Installs Ninja and the development packages of X11 and OpenGL. Builds `haylen-headless` and the test library `haylen-native-test` in Debug, and runs every test of [the test project](#the-test-project) with `build/linux-debug/bin/haylen-headless samples/tests --frames 60000`. It fails when a test raises an error or its scene fails to load. |
| `release` | Ubuntu | Installs Ninja and the development packages of X11 and OpenGL, which the desktop artifacts with the content tool need. Builds the protected release of the test project for the web with `prepare samples/tests --platform web --config Release`, with keys made for the job and thrown away with it. Then it inspects the site with `check --config Release`, which fails when it holds Lua text, a file of the package as it is, a zipped package, symbol files or a key in any form. |

The engine tests run only in the `desktop` job. The `web`, `android` and `apple` jobs check that the engine artifacts of those platforms build. The `desktop-apps` job checks the desktop artifacts with the Windows and Linux templates and builds the C++ template on both systems, and the `release` job checks a protected web release.
