# Testing

Every engine capability that can run without a real graphics device or platform runtime has GoogleTest tests, and every Lua binding is tested by running Lua through a headless engine. This guide explains how the suite is organized, how the headless host and the engine fixture work, how to run the tests, coverage and the formatter, what the CI workflow checks and how to test a new binding.

## Layout

The suite lives in `engine/tests/` and builds into one executable, `haylen_tests`.

| Path | Contents |
| --- | --- |
| `engine/tests/<module>/` | Tests of one engine module, such as `core/EngineTests.cpp` or `io/IoTests.cpp`. |
| `engine/tests/2d/<module>/` | Tests of the 2D modules, like the engine's own `2d` folders: `animation`, `graphics` (the renderer, fonts, cameras and their Lua binding), `lighting`, `navigation`, `particles`, `physics`, `spatial` and `tiled`. |
| `engine/tests/lua/` | Binding tests of the core, math, input and assets modules, and of the Lua runtime and the binding toolkit. Other modules keep their binding tests next to their C++ tests, such as `Spatial2DLuaTest` in `2d/spatial/Spatial2DTests.cpp`. |
| `engine/tests/support/` | Shared helpers in `haylen::test`: `EngineFixture` with `DrawingScene` and the test data helpers, `TemporaryDirectory`, `TestApplication` and `VarnRuntime`. |
| `engine/tests/data/fonts/` | Subsets of open fonts, with their licenses, that keep only the characters the text, shaping and UI tests draw: Latin serif and CFF faces, Arabic, Hebrew, Devanagari, Thai, Japanese and symbols. CMake passes the folder to the tests as `HAYLEN_TEST_FONTS`, so the engine tests read no file of the samples. |
| `engine/tests/native/` | The file `NativeTest.c`, the plain C library that the native interop tests load through `haylen.native` and Varn's `ffi`. CMake builds it as `native_test` next to `haylen_tests`, where `native.load` finds it by name, and passes its path to the tests as `HAYLEN_NATIVE_TEST_LIBRARY`. The native sample builds the same source for every platform. |
| `engine/tests/CMakeLists.txt` | The `HAYLEN_TEST_SOURCES` list, which names every test file, and the `haylen_tests` target. |

The target `haylen_tests` links `haylen::engine`, `haylen::headless` and GoogleTest's `gtest_main`, and it includes `engine/tests`, so tests include `support/EngineFixture.hpp`. It also adds `engine/src` and the Sokol headers to its own include folders, so a test may include internal headers such as `platform/headless/HeadlessHost.hpp`. Apps never see those folders, because the engine libraries keep them private. The function `gtest_discover_tests` registers every test with CTest under its `Suite.Name`, with `engine/tests` as the working directory and a timeout of 120 seconds, far above the few seconds the slowest test takes, so a test that hangs fails on its own and the rest of the suite still runs. The suite is built only on desktop platforms, and only when `HAYLEN_BUILD_TESTS` is on, which is the default when the engine or the repository root is the top-level project.

Every test file lives in the namespace of the context it tests, such as `haylen::input`, `haylen::storage` or `haylen::ui`, and the shared helpers live in `haylen::test`, one class per file. A test file has no free functions or namespace-scope variables: its helpers and constants are members of its fixtures, and suites that share them derive from the fixture that holds them. Test suites are named after the subject with a `Test` suffix, such as `SceneManagerTest` or `SpatialLuaTest`, and test names state the behavior they verify, such as `RequiresModulesFromThePackageOnly`.

## The headless host

The class `platform::HeadlessHost`, in `engine/src/platform/headless/`, implements the engine's `platform::Host` interface without a window or a GPU, so the real engine loop runs in tests.

- The file `SokolDummy.c` compiles Sokol gfx with its dummy backend, so the renderer, textures, render targets and the UI run and record statistics without drawing anything. The root `haylen` Lua module reports `'dummy'` as its `backend` and `'headless'` as its `platform`.
- The audio mixer has no device and mixes only when a test asks it to. Tests of a mixer with a device give it an `audio::OutputBackend` of their own, which refuses the audio or opens a device that never plays, so no test reaches the audio hardware.
- User data goes to a folder the test chooses, and `getPersistCount()` counts the requests to make it durable.
- Bridge calls are recorded in `getPlatformCalls()` with their JSON parameters and byte buffers instead of reaching native code, and the calls the bridge gave up through a timeout or a cancel in `getCancelledCalls()`.
- The methods `resize(size)`, `setSafeAreaInsets(insets)` and `setGamepad(index, state)` change what the engine sees, and `getTitle()`, `getCursor()`, `isCursorVisible()`, `isMouseLocked()`, `isKeyboardVisible()` and `isQuitRequested()` report what the engine asked for.
- The method `getNativeViews()` returns the `platform::NativeViews` of the headless screen, whose `reserveInsets(key, insets)`, `releaseInsets(key)`, `coverApp()` and `uncoverApp()` a test calls from any thread, the way the native views of plugins do. The engine takes them at the start of the next frame, and they outlive the engines that a test restarts on the host.
- The method `setNativePlugins(ids)` sets the plugins whose native part the headless platform reports, and `getErrorReports()` returns the JSON report of every error that stopped an app, in order.
- The method `setSystemInfo(info)` sets what the headless system reports to the apps that start afterwards, which starts as a desktop of the operating system the tests run on with the cores of the machine, and the graphics device names no GPU on the dummy backend. The methods `setTheme(theme)` and `setBattery(battery)` change the theme and the battery from any thread, the way platform services report them, and the engine publishes the change at its next frame.
- The method `openUrl` records the urls in `getOpenedUrls()` and answers at once that an app took them, or that none did after `setOpensUrls(false)`, and `getVibrations()` returns the seconds of every vibration.
- The method `setNetworkRequirement(sentence)` sets what the network errors of the apps that start afterwards end with, the way Android reports an app without the permission `INTERNET`.
- The method `getDialogRequests()` returns the id, the `DialogRequest` and the folder for copies of every native dialog the engine asked for, and `getCancelledDialogs()` the ids of the dialogs it closed after a cancel, a timeout or the end of its app. A test answers a dialog with `engine.getDialogs().resolve(id, result)`, or from another thread through `DialogRelay::resolve`, the way platform code does.
- The method `getScreenRequests()` returns the `ScreenRequest` of every screen of a plugin that the engine handed to the platform, once it covered the app, and `getCancelledScreens()` the ids of the screens it gave up. A test ends a screen from any thread through `ScreenRelay::finish(id, ok, resultJson, buffers)`, and restores the end of a screen of an earlier process through `ScreenRelay::restore`, the way platform code does. The screen that shows belongs to the process, like the covers of native libraries, so a test ends the screens it opened.

Sokol keeps one global device, so only one engine can exist at a time in a process, and a test never creates two fixtures at once. CTest runs every test in a process of its own, so tests still run in parallel.

## The engine fixture

The class `haylen::test::EngineFixture` is a running engine on the headless host with an in-memory package.

```cpp
explicit EngineFixture(std::map<std::string, std::string> files = {}, std::unique_ptr<core::Application> application = nullptr);
```

The argument `files` maps package paths to their contents. The fixture adds an `app.json` with the name `Test App` and the identifier `dev.haylen.tests` and an empty `source/main.lua` unless the map has them, reads the configuration, creates the engine with a `lua::Application`, or with the given application, and starts it. User data goes to a temporary folder that is removed afterwards.

| Member | Purpose |
| --- | --- |
| `engine()`, `host()`, `package()` | The `core::Engine`, the `platform::HeadlessHost` and the `io::MemoryPackage`, for driving the engine and asserting on its state. |
| `lua()` | The `lua_State*` of the engine. |
| `restart()` | Replaces the engine with a new one that runs the package with a Lua application on the same host, the way the runtime restarts an app, so what the host keeps, such as the covers of the app, outlives it. |
| `frames(count, seconds)` | Runs `count` frames of `seconds` each, one sixtieth of a second by default. |
| `frameUntil(condition, timeout)` | Runs frames, sleeping one millisecond between them so worker threads can finish, until `condition` returns `true` or the timeout, ten seconds by default, passes. It returns whether the condition was met. |
| `lua(source)` | Runs a chunk named `test` and returns its first result converted with `tostring`, or `error: ` followed by the message when it fails. |
| `runLua(source)` | Runs a chunk and throws `std::runtime_error` when it fails, for setup code. |

The support folder provides the other shared helpers.

| Helper | Purpose |
| --- | --- |
| `TestApplication(onStart)` | An application whose `start` runs the given function, for tests that add plugins or push C++ scenes, in `support/TestApplication.hpp`. |
| `DrawingScene(draw)` | A scene whose `render` runs the given function inside a real frame, in `support/EngineFixture.hpp`. |
| `RecordingScene(name, log, transparent, processMode)` | A scene that writes every hook it receives to a shared log as `name:hook`, counts its updates and renders, and loads at once, through a deferral the test completes or with a failure, preloading the asset groups it lists, in `support/RecordingScene.hpp`. |
| `RecordingEffect(switchAt, exitAt)` | A transition effect with the given switch and exit points that records the progress and the images it draws, in `support/RecordingEffect.hpp`. |
| `TemporaryDirectory` | A unique folder under the system temporary directory, removed afterwards, with `getPath()` and `write(relative, content)`, in `support/TemporaryDirectory.hpp`. |
| `bytes(text)`, `pngImage(width, height, rgba)` | Test data: raw bytes and a solid PNG image, in `support/EngineFixture.hpp`. |

Data that only one test file needs is built by its fixture, such as the zip archives of the package tests and the WAV files of the audio tests.

The class `VarnRuntime` in `support/VarnRuntime.hpp` owns a Varn runtime without an engine, which `getRuntime()` returns, and `pumpUntil(condition, timeout)` advances its event loop the way the engine does once per frame. The `JobSystem` tests use it.

## Testing Lua bindings

A binding test runs Lua through the fixture and checks what matters to an app: returned values, engine state, error messages and asynchronous results.

- Compare returned values as strings. The call `fixture.lua("return hash.size")` returns `"3"`, and a float returns `"32.0"`.
- Check validation with the message an app would see, as in `EXPECT_NE(fixture.lua("spatial2d.newHashGrid(-2)").find("positive cell size"), std::string::npos)`.
- Check callbacks that fail through `fixture.engine().getError()`, the `lua::Error` that the error screen shows, with its message, position and frames.
- Advance frames for anything asynchronous. Promises settle and coroutines resume during a frame, so a test waits with `frameUntil`.
- Assert on engine state through `fixture.engine()` and `fixture.host()` when a binding changes the engine, such as the window title or a platform call.

The test `Spatial2DLuaTest.StoresLuaValuesByBounds` in `engine/tests/2d/spatial/Spatial2DTests.cpp` is a compact model for a synchronous module. This test covers `jobs.spawn`, whose promise resolves in a later frame.

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
2. Cover the results, the argument errors, the failures of callbacks and every asynchronous path. Write tests for behavior an app relies on, not tests that restate the implementation.
3. Wrap multi-line Lua sources and multi-line lambdas in `// clang-format off` and `// clang-format on`, as the example does.
4. Run the tests, the coverage report and the formatter as the next sections describe, and document the binding in its `docs/lua-api/<module>.md` page.

## Running the tests

```sh
python3 make.py test
```

The command configures `build/<host>-<config>` when needed, builds `haylen_tests` and runs CTest with `--output-on-failure` in parallel. It accepts `--config` (`Debug` by default), `--jobs` and `--sanitizers`.

## Sanitizers

```sh
python3 make.py test --sanitizers address
python3 make.py test --sanitizers thread
```

The option `--sanitizers` builds the tests with sanitizers in a tree of its own, `build/<host>-<config>-address` or `build/<host>-<config>-thread`, so switching never rebuilds the plain tree. It sets `HAYLEN_SANITIZERS`, which MSVC builds ignore.

| Value | Sanitizers | What they instrument |
| --- | --- | --- |
| `address` | AddressSanitizer and UndefinedBehaviorSanitizer | The engine libraries, the tests and the other targets of the engine, which catch memory errors and undefined behavior in engine code. |
| `thread` | ThreadSanitizer | Every C and C++ target of the build, dependencies included, because ThreadSanitizer sees only the synchronization of the code it instruments, such as the atomics with which libuv wakes its event loop. OpenSSL builds with its own scripts and stays uninstrumented. |

AddressSanitizer and ThreadSanitizer fail the test whose process raised a report, while UndefinedBehaviorSanitizer prints its reports and lets the test go on. ThreadSanitizer runs the tests several times slower, and `TSAN_OPTIONS`, such as `TSAN_OPTIONS=halt_on_error=1`, tunes it.

One suite or test runs through CTest or through the executable.

```sh
ctest --test-dir build/macos-debug -R SpatialLuaTest --output-on-failure
build/macos-debug/bin/haylen_tests --gtest_filter='SpatialLuaTest.*'
```

## Coverage

```sh
python3 make.py coverage
```

Coverage uses LLVM source-based coverage and needs Clang. The command configures `build/coverage` with `HAYLEN_ENABLE_COVERAGE=ON` and without the samples and the player, builds `haylen_tests` and runs every test, which writes one profile per process into `build/coverage/coverage/`. Then it merges the profiles with `llvm-profdata`, prints the `llvm-cov` report and writes an HTML report to `build/coverage/coverage/html/index.html`. The report leaves out dependencies, the tests themselves, generated files and the platform backends in `engine/src/platform/` that need a platform SDK (`apple`, `android`, `web`, `windows`, `linux` and `sokol`). On macOS the LLVM tools come from `xcrun`, and elsewhere they must be on `PATH`. Engine coverage stays as close to 100 percent as the code allows.

## Formatting

```sh
python3 make.py format
python3 make.py format --check
```

The command `format` applies `.clang-format` to every `.h`, `.hpp`, `.c`, `.cpp`, `.m` and `.mm` file under `engine/include`, `engine/src`, `engine/tests`, `samples` and `templates`, and then lists the multi-line lambdas that are not between `// clang-format off` and `// clang-format on`. The option `--check` changes nothing and fails when a file is not formatted or a lambda is missing its markers. The command `clang-format` must be on `PATH`, and CI installs version 23.1.1 with `pip install clang-format==23.1.1`.

## Continuous integration

The workflow `.github/workflows/ci.yml` runs on every push and pull request, and a newer push to the same branch or pull request cancels the run in progress. It sets `CPM_SOURCE_CACHE` to `.cache/cpm` in the workspace, and the build jobs cache that folder, keyed by the hash of `engine/cmake/haylen-dependencies.cmake`.

| Job | Runner | What it does |
| --- | --- | --- |
| `format` | Ubuntu | Installs clang-format 23.1.1 and runs `python make.py format --check`. |
| `desktop` | macOS, Ubuntu and Windows | Installs Ninja with the X11 and OpenGL development packages on Linux, and Ninja with the MSVC environment on Windows. Runs `python make.py test --config Debug`, then `python make.py embedding --mode package --config Release`, which builds the SDK and an app that finds it with `find_package`. |
| `coverage` | macOS | Runs `python make.py coverage` and uploads the HTML report as the `coverage` artifact. |
| `web` | Ubuntu | Caches the Emscripten SDK in `.tools/emsdk`, runs `python make.py engine --platform web` and uploads the prebuilt WebGPU and WebGL2 player as the `haylen-web` artifact. |
| `android` | Ubuntu | Installs Java 17, NDK 30.0.16248370, CMake 4.1.2 and the Android 37 platform, runs `python make.py engine --platform android` and uploads the Maven repository of the Android libraries as the `haylen-android` artifact. |
| `apple` | macOS | Installs Ninja, runs `python make.py engine --platform apple` and uploads `Haylen.xcframework` as the `haylen-apple` artifact. |

The unit tests run only in the `desktop` job, and the web, Android and Apple jobs check that the engine artifacts of those platforms build. The [build guide](build.md) explains the platform builds, and the [embedding guide](embedding.md) explains the SDK.
