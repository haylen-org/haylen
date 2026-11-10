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
| `engine/tests/fuzz/` | The entry points of the fuzzers of the content formats, of input recordings, of MessagePack, of image placeholders, of image files, of SVG documents, of the pages of Ogg files, of TrueType and OpenType fonts and of BMFont files, which the option `HAYLEN_BUILD_FUZZERS` builds. |
| `engine/tests/data/fonts/` | Subsets of open fonts, with their licenses, that keep only the characters that the text, shaping and UI tests draw: Latin serif and CFF faces, Arabic, Hebrew, Devanagari, Thai, Japanese, kana of a variable CFF2 face of Chinese and Japanese, symbols, the color emoji of a `COLR` font of version 1 with the ligatures of their sequences, and the letters of a display font in color layers of version 0. The command `pyftsubset` of fontTools makes each subset from the font of its license, outside the repository, such as `pyftsubset NotoColorEmoji-Regular.ttf --unicodes=U+1F600,U+200D --layout-features='*' --drop-tables+=SVG,vhea,vmtx`. CMake passes the folder to the tests as `HAYLEN_TEST_FONTS`, so the engine tests read no file of the samples. |
| `engine/tests/data/audio/` | Short tones made for the tests in Ogg Vorbis, in MP3 with and without the header that tells its length, and in FLAC, and `notes.ogg`, forty seconds of notes and clicks in Ogg Vorbis over many pages, which the streaming tests decode and seek through. CMake passes the folder as `HAYLEN_TEST_AUDIO`. |
| `engine/tests/data/video/` | A short H.264 and AAC clip made for the tests, which the Windows tests decode through Media Foundation. CMake passes the folder as `HAYLEN_TEST_VIDEO`. |
| `engine/tests/native/` | The file `NativeTest.c`, a plain C library that the native interop tests load through `haylen.native` and Varn's `ffi`. The CMake target `haylen-native-test` builds it as the library `native-test` next to `haylen-tests` and the headless player, where `native.load` finds it by name, copies it next to the desktop player in `bin/haylen`, and passes its path to the tests as `HAYLEN_NATIVE_TEST_LIBRARY`. The Native libraries tests of the test project build the same source for every platform. |
| `engine/tests/gpu/` | The tests of the null device and of `gpu::DeviceFactory`, which `haylen-tests` runs, and the [GPU tests](#the-gpu-tests) of the real devices, which `haylen-gpu-tests` runs: the conformance suite of the contract, the reference scenes of the 2D renderer, their programs in `shaders/conformance.glsl`, their helpers in `support/` and the test apps of Apple devices and of Android in `apple/` and `android/`. |
| `engine/tests/data/gpu/` | The reference images of the scenes of the 2D renderer, one PNG per scene, which hold for every graphics API. |
| `engine/tests/data/port/`, `engine/tests/data/port-app/` | The test port, the smallest [port](../platform/ports.md) of the engine, and the app that [the test port](#the-test-port) plays on it. |
| `engine/tests/platform/sdl/` | The tests of the SDL platform layer: the translation of the events of SDL and the timing of the frames, which read only their own data, so `haylen-tests` compiles their sources with them. |
| `engine/tests/ui/apple/` | The runner of [the UI tests](#the-ui-tests) of apps on the Apple simulators: its XcodeGen spec `project.yml` and the base class `HaylenUITestCase` in `support/`. |
| `engine/tests/CMakeLists.txt` | The list `HAYLEN_TEST_SOURCES`, which names every test file, and the target `haylen-tests`. |

The target `haylen-tests` links `haylen::engine`, `haylen::headless`, SDL for the tests of the SDL platform layer and GoogleTest's `gtest_main`. It includes `engine/tests`, so tests include `support/EngineFixture.hpp`. It also adds `engine/src` to its own include folders, so a test may include internal headers such as `platform/headless/HeadlessHost.hpp`. Apps never see those folders, because the engine libraries keep them private.

The function `gtest_discover_tests` registers every test with CTest under its `Suite.Name`, with `engine/tests` as the working directory. Each test has a timeout of 120 seconds, far above the few seconds that the slowest test takes. So a test that hangs fails on its own, and the rest of the suite still runs.

On Windows the target also compiles `WindowsVideoDecoder` and `WindowsSettings` from the runtime, which need no window. So `WindowsVideoDecoderTest` decodes the clip of `data/video` with Media Foundation, and `WindowsSettingsTest` reads the accessibility settings of the user. The suite builds only on desktop platforms, and only when `HAYLEN_BUILD_TESTS` is on. That is the default when the engine or the root of the repository is the top-level project.

Every test file lives in the namespace of the context it tests, such as `haylen::input`, `haylen::storage` or `haylen::ui`. The shared helpers live in `haylen::test`, one class per file. A test file has no free functions or namespace-scope variables. Its helpers and constants are members of its fixtures, and suites that share them derive from the fixture that holds them. A suite takes the name of its subject with a `Test` suffix, such as `SceneManagerTest` or `SpatialLuaTest`. A test takes the name of the behavior it checks, such as `RequiresModulesFromThePackageOnly`.

## The headless host

The class `platform::HeadlessHost`, in `engine/src/platform/headless/`, implements the `platform::Host` interface of the engine without a window or a GPU. So the real engine loop runs in tests.

- The host draws through `gpu::NullDevice` (`engine/src/gpu/NullDevice.hpp`), the GPU device of the headless host. The renderer, textures, render targets and the UI run and record their statistics without drawing anything.
- The device checks every call against the contract of `gpu::Device` and throws `std::logic_error` on a broken one. So a test fails when the engine uses an object that does not exist, opens a pass inside another, draws outside a pass or without a pipeline, leaves a slot of the program unbound or hands a uniform block the wrong size. It also fails when the engine writes the dynamic images of a frame in more than one call or after its first pass, since a real device stages every image of a frame before it copies any of them.
- It checks the rest of the contract the same way. An image fits its type, layers, mip levels, sample count and format, and a texture brings the pixels of every layer of every level. The attachments of a pass share one size and sample count, a multisampled color resolves into a render target of one sample of its format, and a pipeline draws only in a pass whose attachments match its targets. A draw comes after the bindings and every uniform block of its pipeline, samples no attachment of its own pass and reads only the indices its buffer holds. A texture slot that filters takes only an image of a format that the device filters, and a sampler slot that does not filter takes no sampler with a linear filter, as WebGPU requires. A frame writes each part of a dynamic buffer once, before a pass binds it. Debug groups close in the pass that opened them, the swapchain passes of a frame into one surface follow one another, and the surface goes and comes back in turn between frames.
- Its capabilities say what the engine may use. By default it renders every format, blends into and filters all but the 32-bit floats and resolves multisampled render targets, but blends every color of a pipeline alike, as OpenGL ES does. A test that makes it with other `gpu::Device::Capabilities` sees how the engine behaves on a device that lacks a feature.
- Its pools limit how many images, buffers, samplers, programs and pipelines exist, like the pools of a real device.
- It keeps what the last committed frame did, which a test reads with `fixture.host().getGpu().getLastFrame()`. That is its passes, with whether each drew into the swapchain, its clear color, the write masks of its pipelines and its draws, and the image writes, their bytes and the buffer writes of the frame.
- The method `lose()` of `gpu::NullDevice` loses the device, as a reset of the GPU does, so `isLost` answers `true` and a test follows what the platform does with a lost device.
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
- The writes and reads of the clipboard answer at once with the answer of `setClipboardAnswer(answer)`, `Granted` until a test changes it. The method `getRumbles()` returns every rumble of a gamepad that the engine asked for.
- The method `setPermission(answer)` sets the answer of every check of a permission, and `setPermissionAnswer(answer)` the one of every request, whose kinds `getPermissionRequests()` returns in order. The method `setPermissionAnswersHeld(true)` holds the answers of the requests until `answerPermissions()`, the way a person takes a while to answer the prompt of the system.
- The method `enableDevelopment(folder)` gives the host a `platform::DevelopmentSession`, so the engines created afterwards run in development. With a folder, the session scans it for changes the way the player does. It outlives the engines that a test restarts on the host, like the session of the runtime.
- The method `setNetworkRequirement(sentence)` sets the sentence that the network errors of the apps that start afterwards end with, the way Android reports an app without the permission `INTERNET`.
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
| `DebugClient(host)` | A client of the Debug Adapter Protocol with a debugger of its own, which `connect(L)` attaches to the Lua state of a fixture. It records every message of the debugger and, while the debugger halts the app, takes the steps that `then` queued, each once the requests of the step before were answered, so a test drives halts, steps and evaluations the way an editor does. In `support/DebugClient.hpp`. |
| `CountingPackage(inner)` | Passes a package through and counts the bytes that its readers read and the largest single read, so a test shows which parts of a file an operation touches. With `watch(thread)` it counts the reads of one thread, so a test shows that the thread that mixes audio never reads. In `support/CountingPackage.hpp`. |
| `ContentServer`, `LimitedStoreFiles`, `QuotaStoreFiles`, `CrashingStoreFiles`, `ServiceKeyProvider`, `StorePackSource`, `PrivateFiles` | The places where the content system meets the world, in `support/`: a local server that serves ranges and misbehaves on purpose, store files on a device with little room, store files in a browser that runs out of quota, store files that lose power during a chosen write, which reaches the disk only in part, a key provider that hands out keys on request from a thread of its own, a source of packs like the plugin of a store, and the check that a file opens for its owner alone. |
| `AllocationTracker` | Counts the allocations of the current thread while it lives, with the largest one and the total, through the global allocation functions that the test program replaces. So a test shows that an operation never allocates a whole file or package. With the checked iterators of the debug library of MSVC, which allocate a proxy for every container, it leaves out the allocations of the size of that proxy. On Apple platforms it also leaves out what the GPU driver of the system, the Metal driver of the simulators and the validation layer of Metal allocate for every command buffer, which no device of the engine can avoid, so the GPU tests check the allocations of the engine alone. On Linux it leaves out what the Vulkan loader, its validation layers, the drivers and the compiler of software drivers allocate, which reach the allocation functions of the test program too, from the first caller outside the program and the C++ library. On the web it leaves out the handles that the port of WebGPU makes in the memory of the program for every command encoder, pass and command buffer, which it finds through the functions of the port on the stack of the allocation. In `support/AllocationTracker.hpp`. |
| `EventLoop::yield()` | Gives the event loop of the platform a turn while a test waits, such as for a copy of a render target. In the browser that turn settles the promises of WebGPU, and on other platforms it is a short pause. In `support/EventLoop.hpp`. |
| `Environment::get(name)` | Reads a variable of the environment that the GPU tests take, such as `HAYLEN_TEST_GRAPHICS_API`, and gives an empty string when it is not set. It reads through `_dupenv_s` with MSVC, whose C library deprecates `std::getenv`, so the build stays free of warnings there. In `support/Environment.hpp`. |
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

The command `test` builds `haylen-tests` for this machine and runs CTest with `--output-on-failure` in parallel. It configures `build/<host>-<config>` when needed, as the [build trees](build.md#build-trees) describe. It takes the [build options](build.md#build-options), of which `--config` (`Debug` by default), `--jobs` and `--sanitizers` matter here. With `--platform web` it runs [the error screen check of the web](#the-error-screen-check-of-the-web), [the wake lock check of the web](#the-wake-lock-check-of-the-web), [the fullscreen check of the web](#the-fullscreen-check-of-the-web), [the plugin check of the web](#the-plugin-check-of-the-web) and [the memory check of the web](#the-memory-check-of-the-web) instead, and with `--gpu` on the web, on Android, on the Apple simulators, on Mac Catalyst or on an iOS, tvOS or visionOS device it runs [the GPU tests](#the-gpu-tests) there. With `--port` it builds and plays [the test port](#the-test-port), and with `--ui` it runs [the UI tests](#the-ui-tests) of an app on a simulator.

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

- The fixture `test::GpuFixture` creates the device of its API offscreen with the validation of the API on, through `GpuFixture::makeDevice`, which waits until `gpu::DeviceFactory::prepare` says the API is ready and then calls `gpu::DeviceFactory::create`. The environment variable `HAYLEN_TEST_GPU_SOFTWARE=1` asks the devices for the software rasterizer of their API, such as WARP on Direct3D or the fallback adapter of the browser on WebGPU, so a machine with a GPU runs the tests as a machine of continuous integration without one does. The variable `HAYLEN_TEST_GPU_LOW_POWER=1` asks them for the GPU that uses the least power, such as the integrated GPU of a laptop that has two. A device whose API has no such choice ignores them. The OpenGL APIs draw into the context of a hidden drawable that `test::OffscreenSurface` makes, which the OpenGL device brings to the tests and the benchmarks through `HAYLEN_GPU_OFFSCREEN_SOURCES`, an EGL pixel buffer on Linux and Android, a hidden WGL window on Windows and the canvas of the page on the web. Like the contexts of apps, these contexts ask to be lost when the GPU resets, so a reset fails the test as a lost device instead of leaving it waiting for a frame that the GPU never finishes. The OpenGL device adds its own tests of its state cache, shader programs, upload ring and frames, which run on OpenGL and OpenGL ES wherever the machine makes their contexts. A device that cannot start on the machine skips the test with the reason, such as a machine without Vulkan. When `HAYLEN_TEST_GRAPHICS_API` or `--graphics-api` names the API, a device that cannot start fails the test instead, so a run of continuous integration never passes without the device it counts on.
- Every error that reaches the log while a test runs fails it, through `test::LoggedErrors`. The devices log the messages of the validation of their API as errors, so a call that the API rejects fails the test.
- The conformance suite draws into render targets and reads them back with `GpuFixture::read`, which commits frames until the copy finishes and hands the rows with the top row first. It covers clears, viewports and scissors, kept passes, four color targets, the resolve of multisampled targets, mip levels and layers of render targets, triangles, strips, lines, indexed and instanced draws, uniforms per draw, dynamic buffers, every blend factor and operation, write masks, independent blending, depth, stencil, comparison samplers, every pixel format the device marks, the linear filtering of every format it marks filterable, arrays, volumes, cubes, mip levels, filters, wraps, levels of detail, dynamic images, objects destroyed while their frames run, frames that allocate nothing, GPU timing and programs prepared ahead. It also makes every program and variant of the shader packages of the renderer on each device, so the code the shader compiler writes for its language compiles and fits the bindings of the device.
- The reference scenes run a real engine through `test::EngineFixture` with the option `gpuDevice`, which draws the engine through the device instead of the null device. Each scene draws shapes, sprites, text, lights and shadows, particles, post-processing, render targets or groups whose blends mix with what lies below into a render target, reads it back and compares it with its PNG in `engine/tests/data/gpu` through `test::ImageComparison`, within a small tolerance for the edges that APIs rasterize apart.

The references come from the first device that draws the scenes right, and every other device matches them. To write them, run the GPU tests with `HAYLEN_GPU_REFERENCES=write`, which writes the image of each scene to its reference and names it in the output, then look at every image before you commit it. A run without the variable never writes a reference. A missing reference fails its test, and a scene that differs keeps the image it drew and the difference in `gpu-test-output` of the build tree.

```sh
HAYLEN_GPU_REFERENCES=write HAYLEN_TEST_GRAPHICS_API=metal build/macos-debug/bin/haylen-gpu-tests --gtest_filter='GpuRendererTest.*'
```

On the web, `haylen test --gpu --platform web` builds the GPU tests in `build/web-gpu-tests-debug` and runs them in the page `engine/tests/web/gpu-tests.html` of a headless Chrome, whose WebGPU draws through SwiftShader on a machine without a GPU for it. The browser hands over the adapter of WebGPU and the copies of render targets only in a turn of its event loop. So the tests wait through `test::EventLoop::yield`, which gives the browser that turn through the promise integration of WebAssembly. The page preloads the reference images at their paths on the machine that built it, and the driver `engine/tests/web/gpu-tests.mjs` prints what the tests print and exits with their status.

```sh
haylen test --gpu --platform web
haylen test --gpu --graphics-api webGpu --platform web
haylen test --gpu --graphics-api webGl2 --platform web
```

On Android, `haylen test --gpu --platform android` builds the GPU tests for the ABI of the device in `build/android-gpu-tests-<abi>-debug` as the library of a test app, since a program that adb starts reaches the Vulkan driver of some devices only from an app. The command packages the library and the reference images into the APK of `com.haylen.gputests`, a NativeActivity without Java code. The tool `aapt2` links it against the newest platform of the Android SDK, and `apksigner` signs it with a key that the tree keeps. The command installs the app on the device that `--device` names or on the only one connected. Without any, it starts an emulator of the first virtual device of the Android SDK without a window and shuts it down when the tests end. With `HAYLEN_TEST_GPU_SOFTWARE=1` that emulator draws through SwiftShader, as a machine without a GPU for it needs. The command starts the app with the variables that start with `HAYLEN_` or `GTEST_`, which the app takes from the string array extra `environment` of its intent. It hands those of GoogleTest to it as the arguments they stand for, since GoogleTest reads its variables when the library loads, before the activity starts. The command prints what the tests print, which the app writes to the log of the device under the tag `haylen-gpu-tests`. The app copies the reference images from its assets into its files, runs the tests on a thread of their own, finishes its activity and quits with their status. It finishes the activity first because Android starts the activity of a process that quits while it shows again, with the intent it had. The command starts the app with `am start -S`, which stops a run that still goes on. It writes their results to `results.xml` and keeps the scenes that differ next to it in `gpu-test-output` of its files. The app is debuggable, so the command copies that folder into `gpu-test-output` of the tree through `run-as`. Then it uninstalls the app and fails when a test failed or the app quit before it wrote its results.

```sh
haylen test --gpu --platform android
haylen test --graphics-api openGlEs --platform android --device emulator-5554
```

On the Apple simulators, `haylen test --gpu --platform ios-simulator`, `--platform tvos-simulator` and `--platform visionos-simulator` build the GPU tests for the simulator in `build/<platform>-gpu-tests-debug`, as a plain executable for the architecture of the Mac. They start it inside the simulator with `xcrun simctl spawn`, which hands the tests the variables that start with `HAYLEN_` or `GTEST_` through the prefix `SIMCTL_CHILD_`. A simulator shares the files of the Mac, so the tests read the reference images and keep the scenes that differ in `gpu-test-output` of the tree, at the same paths as on the Mac. The option `--device` takes the name or the id of a simulator. Without it the tests take a booted simulator, or else the first iPhone, Apple TV or Apple Vision Pro. A machine without the simulator runtime of the platform stops with the command that installs it, such as `xcodebuild -downloadPlatform visionOS`. A simulator that was shut down boots without a window and shuts down when the tests end. With `--platform catalyst` the tests build for Mac Catalyst in `build/catalyst-gpu-tests-debug` and run on the Mac itself.

```sh
haylen test --gpu --platform ios-simulator
haylen test --gpu --platform ios-simulator --device "iPad Pro 11-inch (M5)"
haylen test --gpu --platform tvos-simulator
haylen test --gpu --platform catalyst
```

On iOS, tvOS and visionOS devices, `haylen test --gpu --platform ios`, `--platform tvos` and `--platform visionos` with `--device` and the name or the id of a device, which `xcrun devicectl list devices` lists, build the GPU tests in `build/<platform>-gpu-tests-debug` as an app, since a device starts no plain executable. The Xcode generator builds the app `com.haylen.gputests` and signs it for the development team that `HAYLEN_APPLE_TEAM` names. The app carries the reference images in its bundle, shows an empty window and runs the tests once its scene is active, because Metal takes no work from an app in the background. The command installs it with `xcrun devicectl`, launches it with the variables that start with `HAYLEN_` or `GTEST_` through the prefix `DEVICECTL_CHILD_` and prints what the tests print. The app writes its results to `results.xml` and keeps the scenes that differ next to it in `Library/Caches/gpu-test-output` of its container, the one folder that tvOS lets an app write. The class `test::GpuTestFiles` finds both folders, in the bundle and the container there and in the source and build trees elsewhere. When the app quits, the command copies that folder into `gpu-test-output` of the tree, uninstalls the app and fails when a test failed or the app quit before it wrote its results. An iPhone or iPad must be unlocked to launch the app. An Apple TV must be awake and show no screen saver, because tvOS refuses to launch an app while it sleeps and keeps an app that launches behind the screen saver in the background, where the tests wait. The app keeps the idle timer off, so no screen saver starts while the tests run.

```sh
haylen test --gpu --platform ios --device "iPhone"
haylen test --gpu --platform tvos --device "Living Room"
```

## The test port

```sh
haylen test --port
haylen test --port --config Release
```

The option `--port` of `haylen test` checks the mechanism of [ports](../platform/ports.md) on this machine. It configures the engine with `HAYLEN_PORT_DIR` set to `engine/tests/data/port` in `build/<host>-port-<config>`, builds the player `haylen` of that port and plays `engine/tests/data/port-app` on it. The port replaces the platform layer and the services with a runtime that plays the app without a window, and draws through a device of the API `port` built on `gpu::NullDevice`, so every call of the engine meets the checks of the contract. Its shader tool gives every shader package of the engine a `port` target, which the device runs.

The app fails the run with the status `1` when `haylen.platform` is not `'test-port'`, when the operating system of `haylen.system` is not `'port'` or when `haylen.graphicsApi` is not `'port'`. The runtime fails it too when the app does not quit within 600 frames, such as an app stuck on the error screen. Otherwise the app draws ten frames of shapes, sprites and text and quits with the status `0`.

## The error screen check of the web

```sh
haylen test --platform web
haylen test --platform web --graphics-api webGl2
```

A web page writes the clipboard only through the asynchronous clipboard API of the browser, which may refuse it, and a restart from the error screen creates a new GPU device, which WebGPU hands over only in a later turn of the event loop of the page. So `haylen test --platform web` first runs this check in a browser:

1. It builds the release site of the app `engine/tests/data/web-error-screen`, which clears the canvas to magenta and stops with an error when the key E is pressed, and serves it.
2. The driver `engine/tests/web/error-screen-check.mjs` opens the page in a headless Chrome that lets the page write the clipboard. Like the [GPU tests of the web](#the-gpu-tests), every browser of the check draws WebGPU and WebGL2 through SwiftShader, so both start on a machine without a GPU. The driver decodes its screenshots itself, so they never touch the GPU of the page.
3. The page decodes a screenshot of itself through an `OffscreenCanvas` once, as a page that embeds an app and draws into a canvas of its own does, and the app must draw on. Should the browser take the GPU away from the app then, the app must start again on the one replacement of its device and draw its magenta, without the failure that a second loss would bring.
4. The driver stops the app and presses C on the error screen. The clipboard must then hold the report of the error.
5. It presses R, and the app must start again and draw its magenta. Then it stops the app again, finds the restart button in a screenshot of the page and clicks it, with the same result.
6. Then a browser that refuses every write of the clipboard and one without the clipboard API open the page. The copy must warn in the log of the page and leave the clipboard alone, and R must still bring the app back.
7. Every page opens twice, with `?graphicsApi=webGpu` and with `?graphicsApi=webGl2`, and the app must draw through the API that the address names, since each restart makes a new GPU device of that API. With `--graphics-api webGpu` or `--graphics-api webGl2`, every page opens once, with that API, and the pages of the wake lock check, the fullscreen check, the plugin check and the memory check draw through it too. Without it, those pages draw through the first API that starts.
8. Last, a page opens with `?graphicsApi=bogus`, an API the engine does not know. The app must not start, the reason must reach the log of the page and show on the page, over the splash or on the error screen, and the page must throw no exception, so a wrong address never leaves a page that waits forever. That page draws its error screen through the first API that starts, WebGPU where the browser has it, so `--graphics-api webGl2` leaves it out.

Headless Chrome on Linux cannot present a WebGPU canvas, so it takes the device away from such a page and the app ends after the one replacement of its device, as the [WebGPU guide](../graphics/webgpu.md#lost-devices) explains. On Linux, run the checks with `--graphics-api webGl2`, and run those of WebGPU on a Mac, where Chrome draws WebGPU through Metal. The GPU tests of the web present no canvas, so they run on WebGPU on Linux too.

## The wake lock check of the web

A web page keeps the screen on through the Screen Wake Lock API of the browser, which releases the lock whenever the page hides. So `haylen test --platform web` then runs this check in a browser:

1. It builds the release site of the app `engine/tests/data/web-wake-lock`, whose `app.json` keeps the screen on from the start and whose script lets the screen sleep when the space key is pressed, and serves it.
2. The driver `engine/tests/web/wake-lock-check.mjs` opens the page in a headless Chrome and watches the locks that the browser grants and releases through a script that it adds to the page.
3. The page must take the lock, give it up when another tab hides it, take it again when it shows and release it once the app lets the screen sleep.
4. Then a browser that refuses every request and one without the API open the page, which must only warn in its log while the app runs on, without an error.

## The fullscreen check of the web

A web page goes fullscreen through the Fullscreen API of the browser, which allows it only shortly after an input of the person and settles each change later. The page puts the element that holds the canvas in fullscreen, which SDL does not follow, so the page itself tells the engine whether the app is fullscreen. So `haylen test --platform web` then runs this check in a browser:

1. It builds the release site of the app `engine/tests/data/web-fullscreen`, whose keys F, L and R cover the screen, leave fullscreen and ask for both at once, and serves it.
2. The driver `engine/tests/web/fullscreen-check.mjs` opens the page in a headless Chrome and presses the keys through the debugging protocol, which the browser counts as inputs of the person.
3. After F, the promise must settle with `window.fullscreen()` already `true`, and the event `windowFullscreenChanged` must arrive. After L, the promise must settle with `window.fullscreen()` already `false`.
4. After R, the promise of the change to fullscreen must fail with the code `denied`, since the change that leaves fullscreen replaced it, which must settle with `window.fullscreen()` `false`, and the page must end out of fullscreen.

## The plugin check of the web

The context that the web module of a plugin receives answers what the page offers and how a failure reaches the app, which only a browser shows. A plugin names what its web part needs from the page, such as a feature of the permissions policy, and the page fails its calls with the code `unsupported` when it lacks one. A browser governs only some features through its policy, and its policy allows no feature that it does not govern, so such a feature is left to the error of its API. An error of the page, such as a `DOMException`, carries a number as its `code`, which names no failure. So `haylen test --platform web` then runs this check in a browser:

1. It builds the site of the app `engine/tests/data/web-plugins`, whose plugin requires the features `camera` and `web-share`, and serves it.
2. The driver `engine/tests/web/plugin-check.mjs` opens the page in a headless Chrome with a permissions policy that governs `camera` alone and refuses it.
3. The page must lack `camera` alone, and the check fails when it also counts `web-share` as missing.
4. The plugin throws the `DOMException` `NotAllowedError`, whose call must fail with the code `exception` and `NotAllowedError` in `data.type`.
5. The plugin requires a permission whose request ends in `prompt`, as in a browser that asks only during a tap, and the call must fail with the code `denied` and the sentence that says the system did not show its prompt.
6. The plugin requires a permission that it never registered, and the call must fail with the code `unsupported` and the sentence of Apple platforms, which names the plugin and the permission.

## The memory check of the web

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

The executable `haylen-headless` builds with the player on desktop platforms. It runs any app package on the headless host at 60 frames per second of real time, so asynchronous work such as decoding and file reads finishes as it does on a device. It prints the log. It stops when the app quits, when an error stops an app that cannot recover, or after the frames of `--frames`. With `--dev` it runs a package folder in development, like the player. So a file saved while it runs reloads as [hot reload](../core/lua.md#hot-reload) describes, and the log shows what each save did. The apps keep their user data in a folder of the first app under the temporary folder of the system. A test that plays an app through `HeadlessPlayer::run` names a folder of its own in `dataDirectory`, so tests and checkouts that run at the same time never share preferences or storage.

The headless player exits with 1 when an error stopped the app, even one that the app went back from, or when the app did not quit in time. Otherwise it exits with the status that the app quit with, which `haylen.quit({status = 1})` sets and which is 0 by default, as the [reference](../lua-api/haylen.md#haylenquitoptions) describes.

On the headless platform the test project runs every test that supports the platform for 2 seconds each:

- It writes `[CODE] Running`, `[CODE] Passed`, `[CODE] Failed: <message>` or `[CODE] Failed to load: <message>` for each test. A check of the results of a test that settles on a failure fails the test too.
- It goes back from every error with `haylen.recover()`.
- It ends with a summary, and quits with the status 1 when any test failed.

So the headless player exits with 1 when a test raised an error, failed a check or its scene failed to load. A test that needs a person, a device or a native part that the headless host does not load lists `headless` among its unsupported platforms in its manifest.

## The UI tests

The UI tests drive an app on a simulator the way a person does: they tap, drag and use the system UI, such as the controls that arrange the windows of iPadOS. They run on the iOS and iPadOS simulators, on the Apple TV simulator, where a test presses the buttons of the remote through `XCUIRemote` and types on the keyboard of the system, and on the Apple Vision Pro simulator, where a tap arrives as a look and a pinch.

```sh
haylen test --ui samples/tests --platform ios-simulator --device "iPad Pro 13-inch (M5)"
haylen test --ui samples/tests --platform tvos-simulator --device "Apple TV 4K (3rd generation)"
haylen test --ui samples/tests --platform visionos-simulator --device "Apple Vision Pro"
haylen test --ui samples/games/tiny-island --platform visionos-simulator
```

An app keeps its UI tests in Swift in its folder `ui-tests`, which is not part of its package. Each test is an XCUITest case that derives from `HaylenUITestCase`. The GUIs of the app expose their components through the accessibility tree, so a test finds a button or a label by the text it shows:

```swift
import XCTest

class MenuTests: HaylenUITestCase {
    func testATapOnACategoryOpensItsTests() throws {
        tap("Sprites")
        waitFor(element(matching: "SPR-001\\s+Sprite basics"))
        try saveScreenshot("sprites-category")
    }
}
```

The command `haylen test --ui <app>` runs these steps:

- It builds the app for the simulator as `haylen run --build-only` does, in the Debug configuration with the engine artifacts of `--config`, `Debug` by default. A Debug engine checks at every frame on UIKit that the view and its drawable cover the window, through `platform::SurfaceCheck`.
- It generates the runner from `engine/tests/ui/apple/project.yml` with the pinned XcodeGen into `build/<platform>-ui-tests`, with the tests of the app, and builds it with `xcodebuild build-for-testing`.
- It boots the simulator that `--device` names without a window, or else a booted one or the first iPhone, Apple TV or Apple Vision Pro, and installs the app.
- It runs the tests with `xcodebuild test-without-building`, which prints each test and each failure. The runner receives the identifier of the app and the folder of the screenshots through the prefix `TEST_RUNNER_` of xcodebuild. A test that runs longer than 10 minutes stops and fails.
- It relays the log of the app meanwhile and fails when the engine logged an error, even when every test passed, so a layout that leaves the drawable behind the window fails the run.
- It uninstalls the app and the runner, and shuts the simulator down when it booted it.

The class `HaylenUITestCase` starts the app again for each test and gives the tests these helpers:

| Helper | What it does |
| --- | --- |
| `element(_:)`, `element(matching:)` | The first element of the app whose label is the text or matches the regular expression. |
| `waitFor(_:)`, `tap(_:)` | Waits until an element exists, and taps the element with the text on every simulator but the Apple TV, which has no taps. |
| `waitUntil(_:_:)` | Waits until a condition holds, and fails with the message when it never does. |
| `saveScreenshot(_:)` | Saves a picture of the window of the app, never the whole screen, in `ui-tests/<platform>` of the build folder of the app. |
| `setMultitasking(_:)` | Chooses Full Screen Apps, Windowed Apps or Stage Manager in the settings of iPadOS, and returns the mode it replaced, so a test puts it back. |
| `arrangeWindow(_:)` | Picks an arrangement of the window controls of iPadOS, such as `Left`, `Right` or `Fill`. |
| `resizeWindow(by:)` | Drags the bottom right corner of the window of the app, which resizes it. |

The test project checks with them that a tap opens a category, that the software keyboard types, deletes and turns with a field on the iPhone and the iPad, that the keyboard of the Apple TV types into the search of its menu, and that on an iPad the framebuffer follows the window. That test tiles the window to the left half, resizes it there, tiles it to the right half and resizes it in Stage Manager. At each size it reads the framebuffer from the test "Window and device", compares it with the frame of the window times its scale, and checks that the controls of the window leave the button "Back" free. Tiny Island checks that its start plays.

## Sanitizers

```sh
haylen test --sanitizers address
haylen test --sanitizers thread
```

The option `--sanitizers` builds the tests with sanitizers in a tree of its own, `build/<host>-<config>-address` or `build/<host>-<config>-thread`, so switching never builds the plain tree again. It sets `HAYLEN_SANITIZERS`, which MSVC builds ignore.

| Value | Sanitizers | What they instrument |
| --- | --- | --- |
| `address` | AddressSanitizer and UndefinedBehaviorSanitizer | The engine libraries, the tests and the other targets of the engine, which catch memory errors and undefined behavior in the code of the engine. AddressSanitizer also checks the libraries that the engine compiles from the sources of the dependencies it hands the bytes of apps: NanoSVG, HarfBuzz, SheenBidi, libunibreak, Monocypher and miniaudio. |
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

The command configures `build/fuzz` in the Debug configuration with that Clang and `HAYLEN_BUILD_FUZZERS`, which instruments every target for libFuzzer and AddressSanitizer. It builds the nine fuzzers and runs each on inputs of at most 64 KiB. Each fuzzer has a corpus of its own in `build/fuzz/corpus/<fuzzer>`, which keeps the inputs that reached new code for the next run. A crash stops the command with the input that caused it.

| Fuzzer | What it checks | Tests that drive the same code on every run |
| --- | --- | --- |
| `haylen-bitmap-font-fuzzer` | It reads each input as a BMFont file of `text::BitmapFont`, in the binary format when its first byte is odd, which puts the magic of that format before the rest, and in the text format otherwise, and lays out text with what reads. The parser may reject a file only as data that is not a BMFont file. | `BitmapFontTest`, with cut files, unknown blocks, blocks shorter than their fields, huge page counts and pages past 16 bits. |
| `haylen-content-fuzzer` | The parsers of the content formats treat every file as hostile. The first byte of an input picks the chunk record, the shard header, the shard index, the catalog, the manifest or the channel descriptor, and the parser may reject the rest only with a content error. | The hostile input tests in `engine/tests/content/HostileInputTests.cpp`, with random bytes and damaged copies of valid files. |
| `haylen-font-fuzzer` | It reads each input as a TrueType or OpenType font file, or as the first faces of a collection, the two ways the engine draws fonts: it lays out text in several scripts through the distance field atlas of `text::TrueTypeFont`, and it draws the same characters at two sizes through the font loader of Dear ImGui. A file may fail only as data that is not a font, or with a glyph or an atlas too large for the device. | `FontFileTest` and `BackendTest`, with glyphs made of themselves, outlines past the end of their table, character maps that count more segments than they hold, truncated advances and an em square of no units. |
| `haylen-image-fuzzer` | It decodes each input with `graphics::Image::decode` as a PNG, JPEG, GIF, BMP or TGA file, which its first byte picks by putting the signature of that format before the rest, and bleeds and halves what decodes. The decode may fail only as data that is not an image of at most 1024 pixels on a side. | The image tests in `engine/tests/graphics/GraphicsTests.cpp`, with a header that declares a side too long and data that inflates past its image. |
| `haylen-input-fuzzer` | The reader of input recordings may reject an input only as a malformed recording. | `InputRecordingTest`, with truncated, trailing and oversized inputs. |
| `haylen-ogg-fuzzer` | It reads each input as an Ogg file with the page index of streamed Ogg Vorbis sounds: it checks the first packet, reads the length, records the pages as decoding in order does and searches for frames in a new index, which bisects the file. Every page it finds must start inside the file and end at or before the frame. | `OggPageIndexTest`, with cut files, headers that claim the largest pages, copies of headers inside pages and runs of capture patterns. |
| `haylen-messagepack-fuzzer` | It decodes each input with `core::MessagePack` and encodes the result again, which may fail only as bytes that are not valid MessagePack. | `MessagePackTest`, with truncated, trailing, deeply nested and oversized messages. |
| `haylen-placeholder-fuzzer` | It decodes each input as an image placeholder of `graphics::ImagePlaceholder`, which may fail only as a text that is not a placeholder. | `ImagePlaceholderTest`, with empty, cut, padded, foreign and oversized texts. |
| `haylen-vector-image-fuzzer` | It reads each input as an SVG document with `graphics::VectorImage::parse` and rasterizes what reads at 64 pixels on its longest side, which may fail only as a text that is not an SVG document with a size. | `VectorImageTest`, with documents without a size and rasters too large. |

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
- How it reads its arguments, such as the app folder of `run` and the options of `sdk --check-consumers` and `distribution android-key`, how its launchers run it, and how its terminal output marks reserved expressions, paths and URLs with and without color.
- How it provides the pinned shader compiler, XcodeGen and Emscripten, how it builds only the stale engine artifacts, and how the layers of the templates, the desktop projects, `run`, `plugin new` and native libraries behave.
- That every tracked file and folder uses dashes where nothing requires an underscore, and so does every target, project and install component of the CMake files and of the CMake text that `haylen.py` writes.
- That only the SDL platform layer reaches SDL, that only the GPU devices include the headers of their graphics APIs, that no Lua binding knows a host, and that comments and the texts of the interface are written as the project asks.
- That the animated sheets and the images of the samples hold their pivots and have clean edges.
- The documentation, through `DocsTest`: the titles, the languages of code blocks, one line per paragraph, every relative link and its heading, the tables of the indexes, and that no long sentence repeats in another page.

## Continuous integration

The workflow `.github/workflows/ci.yml` runs on every push to a branch and on every pull request, except a change that touches only Markdown files, `docs/` or `extras/`, whose docs the tests of `tools/test_haylen.py` check on every machine before a commit. A newer push to the same branch or pull request cancels the run in progress, so only the run of the newest commit goes on. A run can also start by hand, with `gh workflow run ci.yml --ref main`, which tests the whole branch whatever changed, and such a run has a concurrency group of its own, so no push cancels it and it cancels nothing. The workflow only tests, so no job publishes anything, and tags and releases start no run. The jobs of a run hand the desktop artifacts that one of them built to the others as an artifact of the run, which nothing outside the run reads and which GitHub deletes after a day. A GitHub release of the repository is its source archive alone, since every project compiles the engine from its sources, or builds the prebuilt engine of its apps with `haylen engine` on its own machine.

Every job runs `haylen.py` with `--verbose`, and every build passes `--jobs` with all the processors of the runner. The variable `LINUX_PACKAGES` of the workflow lists the development packages that the engine builds against on Linux: Ninja, the X11, Wayland and input libraries that SDL compiles against and loads when the app starts, and the headers of EGL and OpenGL. The variable `MESA_PACKAGES` lists Mesa lavapipe for Vulkan with its validation layers and Mesa llvmpipe for OpenGL, which draw the GPU tests of Linux.

### The build environment

Every job that builds starts with the action `.github/actions/build-environment`. It installs Python and the Linux packages that the job names, sets up MSVC and Ninja on Windows and restores the caches below. A job with a compiler cache then points the variables `CMAKE_<LANG>_COMPILER_LAUNCHER` at ccache, so every compile of the job goes through it, as the [build guide](build.md#settings-of-every-build) describes. Each cache has a key that changes with everything it depends on, so no cache ever holds a result of other inputs:

| Cache | Folder | Key |
| --- | --- | --- |
| The sources of the dependencies | `.cache/cpm`, which `CPM_SOURCE_CACHE` names | The system and the hash of `engine/cmake/haylen-dependencies.cmake` and of the patches in `engine/cmake/patches`. The `desktop` job of each system saves it, and the other jobs of the system read it and fetch the few sources that only their platform needs. |
| The shader compiler | `.tools/haylen-shaderc-<commit>` | The system, the processor and the hash of `engine/cmake/haylen-shader-compiler.json` and of `engine/cmake/haylen-shader-compiler.cmake`. The `desktop` job of each system and the `macos-intel` job save it, and every other job reads it. |
| XcodeGen | `.tools/xcodegen-<version>` | The system, the processor and the version and SHA-256 of `XCODEGEN_VERSION` and `XCODEGEN_SHA256` in `haylen.py`. The iOS job of `apple`, the one job that generates an Xcode project, saves it. |
| The Emscripten SDK and its ports | `.tools/emsdk` and `.cache/emscripten/ports` | The system, the processor and `EMSDK_VERSION` of `haylen.py`. The `web` job saves it on Linux and the macOS `desktop-port` job on macOS, and the `release` and `web-gpu` jobs read it. The jobs that compile with Emscripten point `EM_CACHE` at `.cache/emscripten`, so the libraries that Emscripten builds stay out of the SDK and out of the cache, while the ports that it downloads land in `.cache/emscripten/ports`. |
| ccache on Windows | `.tools/ccache-<version>-windows-x86_64` | The system, the processor and the inputs `ccache-version` and `ccache-sha256` of the action, the newest release of ccache and the hash that its download must match. The Windows `desktop` job saves it, and the other Windows jobs with a compiler cache read it. |
| The compiler cache | `.ccache` | The name of the cache of the job, the system and the processor, the version of Emscripten for the jobs that build the web, and the run that saved it. A run restores the newest cache of its name. A job that keeps a cache ends with the action `.github/actions/compiler-cache`, also when it fails or a newer push cancels it. Only the runs of the default branch save, pushed or started by hand, and the runs of other branches and of pull requests read the caches of the default branch. A job that compiled nothing, such as one cancelled before its build, keeps the cache it restored. A job that ran to its end drops every object it did not use, a cancelled job keeps them all, and then the action saves the cache under the name of its run and deletes the caches of the same name that earlier runs saved. So the repository keeps one compiler cache of each name, which holds one build of its job. |

The sources of the dependencies and the tools come from the releases of their projects, which a run would otherwise download again in every job. Each of them keeps a cache whose key ends with what pins it, so a new pin downloads its own copy and no cache hides a failure. Only the runs of the default branch save them, and only in the one job that saves each cache, once the job succeeds with a key that no cache holds yet. That job first deletes the copies of earlier keys, so the repository keeps one copy of each cache. Every other job, and every run of another branch or of a pull request, only reads them. The steps of these caches sit in the action `build-environment` itself, each with an id of its own, because the steps of an action that a job uses twice share the state with which the runner saves their caches at the end of the job.

The compiler cache cannot hide a failure. It finds an object by the hash of the compiler, its options and every source and header that the compile reads, so a change to any of them compiles the source again, and every link, test and check runs on every run. The objects take the compression level 6 of zstd, which keeps them about a seventh smaller than the default level. The largest size of each compiler cache, its `cache-size` in the workflow, stays above what one run of its job compiles, since a smaller cache would drop objects in the order the next run asks for them.

The repository keeps at most 10 GB of caches, and GitHub removes the caches that runs read least recently first, so a second copy of any cache would push out the newest cache of another job. That is why only the default branch saves, and why each cache holds one build of its job. One build of every job has to fit, or the caches of the jobs that run last, whose runs read them least recently, would leave before each run and every run would compile those jobs from nothing. So the workflow sets `HAYLEN_DEBUG_INFO` to `LINES`, as the [build guide](build.md#cmake-options) describes, and its Debug trees keep only the file and line of each instruction, which stack traces and sanitizer reports read. Full debug information made up most of each Debug object. The jobs that compile keep a compiler cache: `desktop`, `desktop-port`, `macos-intel`, `fuzz`, `desktop-artifacts`, `web`, `android`, `apple`, `cpp-apps` and `cpp-releases`. The `desktop-apps` jobs compile only the few files of the starter app and its plugin, whose bootstrap holds the keys that each run makes, so they compile without a cache. The desktop artifacts of Linux and Windows build once per run, in the `desktop-artifacts` jobs, and the jobs that release apps take them through the action `.github/actions/desktop-artifacts`, so no other cache holds their objects. The macOS artifacts build in the iOS job and in the macOS `desktop-port` job, the two jobs that need them, since a job of their own would hold up the slices of the iOS job. The `tests-project` job builds the headless player in the same build tree as the Linux `desktop` job, and the `release` job builds the same artifacts as the `web` job, so each reads the compiler cache of that job and saves none. The `coverage`, `sanitizers`, `web-gpu` and `android-gpu` jobs build trees with options of their own that no other job compiles, and they compile without a cache.

### The jobs

| Job | Runner | What it does |
| --- | --- | --- |
| `format` | Ubuntu | Installs clang-format 23.1.3, runs `format --check` and the tests of [the rules of haylen.py](#the-rules-of-haylenpy). |
| `desktop` | macOS, Ubuntu and Windows | Runs `test --config Debug`, the engine tests. Then it runs [the GPU tests](#the-gpu-tests) in the same build tree on every device that a hosted runner can start, each named with `--graphics-api`, so a device that cannot start fails the job. Linux runs `vulkan` on Mesa lavapipe and `openGl` on Mesa llvmpipe through EGL without a display. Windows installs the Graphics Tools of Windows, whose debug layer validates Direct3D, and runs `direct3D12` and `direct3D11` on WARP with `HAYLEN_TEST_GPU_SOFTWARE=1`. Windows runners have no Vulkan driver and only the OpenGL 1.1 of the system, so Vulkan and OpenGL do not run there. macOS runs `metal` on the paravirtual GPU of the runner. |
| `desktop-port` | macOS, Ubuntu and Windows | Runs `test --port --config Debug`, which builds the engine with [the test port](#the-test-port) in a tree of its own and plays its app on it. Then it runs `sdk --config Release --check-consumers package`, which builds the SDK and the consumer project `engine/tests/consumer` that finds it with `find_package`. The macOS job then runs `engine --platform web` and `test --platform web --graphics-api webGpu`, the checks of the pages of the web on WebGPU, which Chrome draws there through Metal, since headless Chrome on Linux cannot present a WebGPU canvas. |
| `macos-intel` | macOS on Intel | Builds the engine for x86-64 and runs the engine tests in Debug. The Intel runners of GitHub are virtual machines without a Metal device, so the GPU tests do not run there and the Metal paths of Intel Macs stay untested in CI. The job prints the report of the displays of the runner, which shows whether it supports Metal. |
| `tests-project` | Ubuntu | Builds `haylen-headless` and the test library `haylen-native-test` in Debug, and runs every test of [the test project](#the-test-project) with `build/linux-debug/bin/haylen-headless samples/tests --frames 60000`. It fails when a test raises an error or its scene fails to load. |
| `coverage` | macOS | Runs `coverage`, whose report table shows in the log. |
| `sanitizers` | Ubuntu | Runs `test --sanitizers address` and `test --sanitizers thread` in two jobs, which build and run the engine tests with AddressSanitizer and UndefinedBehaviorSanitizer, and with ThreadSanitizer. |
| `fuzz` | Ubuntu | Installs Clang with its runtime libraries and runs `fuzz --seconds 60 --llvm /usr/bin`, which builds the fuzzers and runs each for a minute. |
| `web-gpu` | Ubuntu | Runs `test --gpu --platform web` with `--graphics-api webGpu` and `--graphics-api webGl2`, the GPU tests in the headless Chrome of the runner, where WebGPU draws through SwiftShader and WebGL2 through ANGLE on SwiftShader. |
| `desktop-artifacts` | Ubuntu and Windows | Runs `engine --platform desktop`, which builds the player, the content tool and the SDK of the system, and hands them to the jobs of the run that release apps: `web`, `release` and `android` on Linux, and `desktop-apps` and `cpp-releases` on both systems. |
| `web` | Ubuntu | Runs `engine --platform web`, which builds the prebuilt WebGPU and WebGL2 player, then `test --platform web --graphics-api webGl2`, the [error screen check](#the-error-screen-check-of-the-web), the [wake lock check](#the-wake-lock-check-of-the-web), the [fullscreen check](#the-fullscreen-check-of-the-web), the [plugin check](#the-plugin-check-of-the-web) and the [memory check of the web](#the-memory-check-of-the-web) in the Chrome of the runner on WebGL2, since headless Chrome on Linux cannot present a WebGPU canvas. The macOS `desktop-port` job runs the same checks on WebGPU. Their releases take the content tool of the desktop artifacts of the run. |
| `release` | Ubuntu | Builds the protected release of the test project for the web with `prepare samples/tests --platform web --config Release`, with keys made for the job and thrown away with it. Then it inspects the site with `check --config Release`, which fails when it holds Lua text, a file of the package as it is, a zipped package, symbol files or a key in any form. |
| `android-gpu` | Ubuntu | Installs the NDK, the emulator and the Android 37 image for x86-64, lets the emulator use KVM and creates a virtual device. Runs `test --gpu --platform android` with `HAYLEN_TEST_GPU_SOFTWARE=1`, which starts that device without a window, drawing through SwiftShader, and runs the GPU tests on every device of the Android build that starts there. |
| `android` | Ubuntu | Installs Java 25, NDK 30.0.16248370, CMake 4.1.2 and the Android 37.2 platform. Runs `engine --platform android`, which builds the Android libraries into the local Maven repository. Then it creates the starter app with `new build/starter`, gives it an upload key with `distribution android-key build/starter` and runs `run build/starter --platform android --config Release --build-only`. That builds the protected release with the content tool of the desktop artifacts, the library of the app for every ABI and the signed APK, and checks it for anything a release never holds. |
| `apple` | macOS | Builds the slices of `Haylen.xcframework` with `engine --platform apple --slices`, in four jobs, one for each family: iOS with `ios` and `ios-simulator`, tvOS with `tvos` and `tvos-simulator`, visionOS with `xros` and `xros-simulator`, and macOS with `macos` and `ios-maccatalyst`. The iOS job then creates the starter app and runs `run build/starter --platform ios-simulator --config Release --build-only`, which builds and checks its protected release for the iOS simulator from the slices it built. |
| `desktop-apps` | Ubuntu and Windows | Installs `dpkg-dev`, `desktop-file-utils` and the newest CMake on Linux. Creates the starter app with `new build/starter`. Adds to it a plugin that `plugin new build/echo-check` creates from the plugin template, with `plugin add build/echo-check --app build/starter`. Runs `distribution installer build/starter --output build/dist` for the platform, which takes the desktop artifacts of the run and builds the C library of the plugin, the starter app from the Linux or Windows template in the Release configuration, and its `.deb` and AppImage or its installer. Then it checks the installed app with `check --config Release`, and on Linux validates its desktop entry with `desktop-file-validate`. |
| `cpp-apps` | Ubuntu and Windows | Creates the starter C++ app with `new build/starter-cpp --kind cpp` and builds it with `run build/starter-cpp --build-only`, which compiles the engine with the app in the Debug configuration. |
| `cpp-releases` | Ubuntu and Windows | Creates the same app and builds it with `run build/starter-cpp --config Release --build-only`, which ships and checks its protected release with the content tool of the desktop artifacts of the run. |

The engine tests run in the `desktop`, `macos-intel`, `coverage` and `sanitizers` jobs, and the GPU tests in the `desktop`, `web-gpu` and `android-gpu` jobs. The `web`, `android` and `apple` jobs check that the engine artifacts of those platforms build. The `desktop-apps` job checks the desktop artifacts with the Windows and Linux templates, the `cpp-apps` and `cpp-releases` jobs build the C++ template on both systems, and the `release` job checks a protected web release. Work that builds a tree of its own runs in a job of its own, so the longest job sets the time of a run, except where short trees share a job, as the test port and the SDK do, so a run asks for fewer macOS runners, which GitHub has the fewest of.
