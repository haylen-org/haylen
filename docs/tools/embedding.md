# 📎 Embedding the engine

This guide is for C++ developers who add Haylen to a CMake project of their own. The folder `engine/` is a standalone CMake project that other projects use as a library. A project adds it in one of three ways: with `add_subdirectory`, with CPM, or with `find_package(haylen)` against a prebuilt SDK. All three give the targets `haylen::engine`, `haylen::runtime` and `haylen::player` and the same CMake function, and the SDK adds `haylen::native`. This guide covers the three ways, the SDK, the function `haylen_add_app`, protected releases of C++ apps and apps written only in C++. The examples build a C++ app whose package the project stages in `package/` of its build tree, as the tooling stages the package of every app. The [build guide](build.md) covers the toolchains of each platform and the CMake options, and the [Lua guide](../core/lua.md#extending-the-engine-from-c) shows how a C++ project adds Lua modules.

## What a project gets

| Name | Kind | Purpose |
| --- | --- | --- |
| `haylen::engine` | Library target, the alias of `haylen-engine` | The whole C++ API and every Lua binding. Its include folder is `engine/include`, and it carries the C++20 requirement to every target that links it. In browser builds it also carries `-fwasm-exceptions` and `-sSUPPORT_LONGJMP=wasm`, because Lua errors unwind as C++ exceptions and yields jump back to their resume with `longjmp`, both native to WebAssembly. |
| `haylen::runtime` | Library target, the alias of `haylen-runtime` | The player, the native host, the platform services, the platform layer, which opens the window, runs the frames and holds the entry point, and the GPU devices, which draw the frames. Linking it turns an executable into a Haylen app, whose sources must define `haylen::core::Application::create()`. In a source build it is an object library, so the entry points always reach the final binary. |
| `haylen::player` | Object library target, the alias of `haylen-player` | The Lua player, the object that defines `haylen::core::Application::create()` with the application that runs `source/main.lua`. A Lua app links it, and a C++ app defines the function in its own sources instead, so every app holds exactly one. |
| `haylen::native` | Interface target of the SDK | The headers of the native API, `haylen/platform/native/HaylenNative.h`, with nlohmann/json, for the native libraries of apps and plugins, which link no engine. |
| `haylen_add_app` | CMake function | Builds the binary of an app for the platform of the build, as [haylen_add_app](#haylen_add_app) explains. |

The engine needs CMake 3.28 or newer and a C++20 compiler, and it downloads its dependencies with CPM. When the engine brings CPM itself, as it does under `add_subdirectory`, it keeps the sources in `CPM_SOURCE_CACHE` when that variable is set, and in `.cache/cpm` of the top-level project otherwise.

When the engine is not the top-level project, its player, tests, benchmarks and tools stay off, because their options default to `PROJECT_IS_TOP_LEVEL`. The [CMake options](build.md#cmake-options) of the build guide shape a source build. The ones a project sets most often are `HAYLEN_GRAPHICS_APIS` and `HAYLEN_SHADERC`. With `HAYLEN_SHADERC` empty, the configure downloads the pinned shader compiler, or builds it, as [the shader compiler](build.md#the-shader-compiler) explains.

```sh
cmake -S . -B build -G Ninja -DHAYLEN_GRAPHICS_APIS=vulkan -DHAYLEN_SHADERC=$HOME/tools/haylen-shaderc
```

## add_subdirectory

A project that keeps a Haylen checkout, such as a Git submodule, adds the engine folder directly.

```cmake
cmake_minimum_required(VERSION 3.28...4.4)

project(my-app LANGUAGES C CXX)

add_subdirectory(external/haylen/engine haylen)

haylen_add_app(my-app CPP
  SOURCES src/App.cpp
  CONTENT "${CMAKE_CURRENT_BINARY_DIR}/package"
)
```

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

## CPM

With [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake), the engine is a package like any other. The engine sits in the `engine` folder of the repository, so a package fetched from Git names that folder with `SOURCE_SUBDIR`.

```cmake
cmake_minimum_required(VERSION 3.28...4.4)

project(my-app LANGUAGES C CXX)

include(cmake/CPM.cmake)
CPMAddPackage(
  NAME haylen
  GIT_REPOSITORY https://github.com/haylen-org/haylen.git
  GIT_TAG v0.0.1
  SOURCE_SUBDIR engine
)

haylen_add_app(my-app CPP
  SOURCES src/App.cpp
  CONTENT "${CMAKE_CURRENT_BINARY_DIR}/package"
)
```

A local checkout works the same way with `CPMAddPackage(NAME haylen SOURCE_DIR "<checkout>/engine")`, which `samples/cpp/embedding` uses. The engine reuses the CPM that the project already included.

## find_package and the SDK

The SDK is a prebuilt engine for one platform, with the GPU devices of its graphics APIs. A project that uses it needs neither the sources of the engine nor `haylen-shaderc`, because the shaders are compiled into it already. Every static library that the engine links, such as Varn, Lua and Box2D, is merged into one `haylen` library, the runtime into `haylen-runtime`, and the Lua player is one object, `haylen-player`.

| Installed path | Contents |
| --- | --- |
| `lib/` | The static libraries `haylen` and `haylen-runtime`, such as `libhaylen.a` and `libhaylen-runtime.a`, and the object of the Lua player, `haylen-player.o`, or `haylen-player.obj` with MSVC. Linux distributions where `GNUInstallDirs` picks `lib64/` use that folder instead. |
| `lib/cmake/haylen/` | `haylen-config.cmake` and `haylen-config-version.cmake`. |
| `include/` | The public headers of the engine, the generated `haylen/core/Version.hpp`, the headers of nlohmann/json, Dear ImGui and Lua that they include, and the headers of the data view of Varn, `varn/varn.h` and `varn/dataview/`, with `VarnDataView.h` on Apple platforms. It also holds the Dear ImGui configuration of the engine in `haylen/ui/ImGuiConfig.hpp`, with the `haylen/ui/ImGuiAssert.hpp` it includes. |
| `share/haylen/` | The folder `cmake/` with `haylen-app.cmake`, which defines `haylen_add_app`, and its helper script, the folder `platform/` with the web shell and the scripts of the web runtime, the Apple platform files and the Windows manifest, the `GpuPreferenceExports.cpp` that Windows apps compile in and, on macOS, `AppleMain.cpp`. The SDK of a [port](../platform/ports.md) adds `haylen-port.json` and the CMake file of its app function. |

A project finds the SDK like any installed package:

```cmake
cmake_minimum_required(VERSION 3.28...4.4)

project(my-app LANGUAGES C CXX)

find_package(haylen REQUIRED)

haylen_add_app(my-app CPP
  SOURCES src/App.cpp
  CONTENT "${CMAKE_CURRENT_BINARY_DIR}/package"
)
```

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/haylen/build/sdk/desktop-release/macos-arm64"
```

The package config imports `haylen::engine`, `haylen::runtime`, `haylen::player` and `haylen::native` and defines `haylen_add_app`. It sets these variables to the values that the SDK was built with:

| Variable | Value |
| --- | --- |
| `HAYLEN_PLATFORM` | The platform, such as `macos`. |
| `HAYLEN_GRAPHICS_APIS` | The graphics APIs whose devices the SDK carries, such as `vulkan;openGl`. |
| `HAYLEN_DESKTOP` | Whether the platform is a desktop. |
| `HAYLEN_PORT_PLATFORM`, `HAYLEN_PORT_CAPABILITIES`, `HAYLEN_PORT_APP_FUNCTION` | The platform, the capabilities and the app function of the port that built the SDK, or empty for a platform of the repository. |

Its version file accepts requests for the same major and minor version.

### Building the SDK

The command `haylen sdk build` builds every SDK, as the [build guide](build.md#engine-sdks) describes. Each slice of an SDK holds its CMake package under its own folder, which the `cmake` file of the slice in `haylen-sdk.json` names, so a project points `CMAKE_PREFIX_PATH` at the folder of its slice:

| Command | Folder of the CMake package |
| --- | --- |
| `haylen sdk build --target player` on macOS, `--target windows` on Windows, `--target linux` on Linux | `build/sdk/desktop-<config>/<system>-<architecture>`, such as `macos-arm64`. |
| `haylen sdk build --target web` | `build/sdk/web-<config>/web-wasm32`, for a project that Emscripten's `emcmake` configures. |
| `haylen sdk build --target android` | `build/sdk/android-<config>/abis/<abi>`, for a project that the NDK toolchain configures. |

The Apple SDK holds `Haylen.xcframework` for the Xcode projects of apps, and only the CMake package of native libraries. Xcode builds every app of iOS, Mac Catalyst, tvOS and visionOS from the Apple project of the tooling, so a CMake project builds none of them, as [haylen_add_app](#haylen_add_app) explains.

### Checking the three ways

The command `haylen engine consumers` builds `engine/tests/consumer`. That is a CMake project of its own with a small C++ app, which adds the engine the way another repository does. It builds once for every way that `--mode` names, or for all three without it:

| Way | How the project adds the engine |
| --- | --- |
| `subdirectory` | With `add_subdirectory`. |
| `cpm` | With `CPMAddPackage`. |
| `package` | With `find_package`, through `haylen_DIR` set to the CMake package of the slice of this machine, in the SDK that `--sdk` names, or else the installed SDK of the current engine sources that holds a slice of this machine. |

```sh
haylen engine consumers
haylen engine consumers --mode subdirectory --mode cpm
haylen engine consumers --mode package --config Release
```

The project reads the way from its cache variable `HAYLEN_CONSUMER_MODE`. Each way builds in `build/engine/consumers/<mode>-<config>`, in the `Release` configuration by default, which the SDK of `package` matches. The `subdirectory` and `cpm` ways share the dependency cache and the shader compiler of the engine builds. The command builds for this machine only. The `linux-apps` and `windows-apps` jobs of [continuous integration](testing.md#continuous-integration) run the `package` way on Linux and Windows.

## haylen_add_app

The file `engine/cmake/haylen-app.cmake` defines the function that builds the binary of an app for the platform of the build.

```cmake
haylen_add_app(<target> LUA|CPP [SOURCES <files>...] CONTENT <folder> [BOOTSTRAP <file>] [WEB_SHELL <file>])
```

| Argument | Meaning |
| --- | --- |
| `<target>` | The name of the executable, or of the shared library on Android. |
| `LUA` | The app runs its Lua modules in the player of the engine. The function links `haylen::player`, whose application runs `source/main.lua`, so a Lua app takes no sources. |
| `CPP` | The app compiles `SOURCES`, which define `haylen::core::Application::create()`. The function never links the player, so every app holds exactly one application. |
| `SOURCES` | The source files of a C++ app, such as its application and the native handlers of the platform bridge. |
| `CONTENT` | The folder that the app ships: the staged package of a Debug build, which holds `haylen.json`, `source/`, `content/` and `plugin.json` and `source/` of every plugin, or the protected release of a Release build. The tooling prepares it, and a project that builds the app itself stages it next to its build, as `samples/cpp/embedding` does. |
| `BOOTSTRAP` | The bootstrap source that the content tool writes for a protected release, which the binary compiles in to open the release. It makes the build a release build without symbols, as [protected releases](#protected-releases) describes. |
| `WEB_SHELL` | The HTML shell of the browser build. The default is `engine/platform/web/shell.html`, whose splash shows the engine logo, a progress bar and the loading text with its percentage. A copy of it with other values in the variables at the top of its style restyles the splash, as the [build guide](build.md#files-of-a-web-build) describes. |

The configure stops with a clear sentence for an unknown argument, both or neither of `LUA` and `CPP`, sources of a Lua app, a C++ app without sources, and a `CONTENT` or `BOOTSTRAP` that does not exist.

The function links `haylen::runtime`, which brings the system libraries of the platform. On macOS those are the system frameworks of `HAYLEN_APPLE_FRAMEWORKS_MACOS` in `engine/cmake/haylen-app.cmake`, and the Apple SDK publishes the lists of every Apple platform in `haylen-apple.json` for the Xcode projects of apps. On macOS the function also adds `AppleMain.cpp`, whose `main` enters the runtime through `haylen_main`, because the SDL platform layer leaves `main` to the app there.

Xcode builds the apps of macOS, iOS, tvOS, visionOS and Mac Catalyst from the Apple project of the tooling, so the function builds only plain macOS executables on Apple platforms, such as those of the CMake projects that consume the engine, and stops with a sentence that says so for any other Apple platform.

| Platform | What the build makes | Where the runtime opens the content |
| --- | --- | --- |
| Windows, Linux, macOS | An executable in `bin/<target>/` of the build tree. After every build, `bin/<target>/app` links to the staged package of a Debug build, as a junction on Windows and a symbolic link elsewhere, so files that the tooling stages again show up without a build, and holds a copy of the protected release of a Release build. Windows builds are apps of the Windows subsystem, and the debugger of Visual Studio starts in their folder. | `app/` next to the executable. |
| Web | The page `<target>.html` with the shell. A Debug build preloads the package at `/app` with `--preload-file` into `<target>.data`, and a release build copies its release into `app/` next to the page, whose page downloads the manifests and the app domain before the app starts and streams the rest by range into the [content store of the browser](../content/content.md#the-web). | `/app` in the virtual file system, unless the page hands over another package. |
| Android | The shared library `bin/<target>/lib<target>.so`, which `HaylenActivity` loads. The Gradle project of the app takes the content into the APK assets. | The folder `app/` of the APK assets. |
| A port | What the function that `APP_FUNCTION` of the [port](../platform/ports.md) names builds, from the sources, the libraries and the content of the app. | Where the runtime of the port opens it. |

A Debug build opens the package that the first argument of its command line names instead of the bundled one, while a build with a `BOOTSTRAP` plays only its own release, as the [distribution guide](distribution.md#development-mode) explains.

### Windows

C++ apps and the `haylen` player embed `engine/platform/windows/haylen.manifest`, the application manifest of the engine. The function `haylen_add_app` adds it to the sources of every Windows target, and the SDK installs it with the other platform files. It selects two things:

- Version 6 of the Common Controls, which gives the task dialogs and the controls that native libraries show, such as those of [plugins](../platform/plugins.md), the current look of Windows.
- UTF-8 as the code page of the process, since the engine keeps every text and path in UTF-8.

The Windows project that the desktop backend generates embeds the same manifest through `haylen_add_app`, for Lua and C++ apps alike. Linux apps load GIO for the theme at run time, where the system has it, so building needs none.

### Protected releases

A Release build ships the [protected release](../content/content.md) of the app instead of the package as it is. The tooling builds it with the content tool of the host and the keys of the app from its [key folder](../content/content.md#key-folders), writes the bootstrap of the app next to it and hands both to the function, the release in `CONTENT` and the bootstrap in `BOOTSTRAP`. So a C++ app and a Lua app read their content through the same code, and CMake never builds a release itself.

A build with a `BOOTSTRAP` also builds without symbols, which stay in `symbols/<target>/` of the build tree for crash reports:

- Android and Linux link with `--gc-sections` and ship stripped. The symbols stay in a `.debug` file, which the binary names in its debug link.
- Windows links with `/DEBUG /OPT:REF /OPT:ICF` and keeps the PDB of the public symbols in that folder.
- macOS links with dead code stripping. The executable leaves its symbols in a dSYM, `<target>.dSYM`, and ships stripped.
- The web keeps the symbol map of Emscripten in that folder, and minifies the processor of the audio output and the worker of the content store next to the page.
- The Android library exports only the entry points of Java and GameActivity.

## An app written in C++

A C++ app is an app like any other, whose `haylen.json` names its kind and its sources. The command `haylen new <folder> --kind cpp` creates one with `src/App.cpp` and the bridge handlers of every platform under `native/`, and with no CMake project or platform project of its own. Every backend generates the project of its target and compiles the sources of the app against the SDK of that target, so a C++ app goes through the same backends, plugins and checks as a Lua app, and its builds never compile the engine.

```json
{
    "name": "My Game",
    "identifier": "com.example.mygame",
    "kind": "cpp",
    "cpp": {
        "sources": ["src"],
        "includeDirectories": ["include"],
        "definitions": ["MY_GAME_TRACE=1"]
    }
}
```

The key `sources` lists the folders and files of C, C++, Objective-C and Objective-C++ sources, relative to the app folder, which define `haylen::core::Application::create()`. The keys `includeDirectories` and `definitions` are optional. Without `--target`, `haylen app run <folder>` builds a C++ app for this machine, with the target `macos`, `windows` or `linux`, and `--target` builds it for any other target, as the [distribution guide](distribution.md) describes.

```sh
haylen new ~/apps/my-game --kind cpp
haylen app run ~/apps/my-game
haylen app run ~/apps/my-game --target ios-simulator
```

The sample `samples/cpp/embedding` shows both sides of a C++ app. Its `haylen.json` names it `cpp` with its `src/` sources, so `haylen app run samples/cpp/embedding` builds it through the backends like any app. Its `CMakeLists.txt` is a CMake project of its own, which the tooling never reads, and which adds the engine from the checkout that `HAYLEN_ENGINE_SOURCE` names, in the way that its cache variable `CPP_EMBEDDING_MODE` picks: `subdirectory`, `cpm` or `package`. That project builds the app with the `CPP` option of `haylen_add_app`:

```cmake
set(package "${CMAKE_CURRENT_BINARY_DIR}/package")
file(REMOVE_RECURSE "${package}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/haylen.json" "${CMAKE_CURRENT_SOURCE_DIR}/content" DESTINATION "${package}")

haylen_add_app(main CPP
  SOURCES src/EmbeddingApp.cpp
  CONTENT "${package}"
)
```

```sh
cmake -S samples/cpp/embedding -B build/embedding -G Ninja
cmake --build build/embedding
```

The project stages its package in `package/` of its build tree, with `haylen.json` and `content/` alone, since the application never runs Lua, so `CMakeLists.txt` and `src/` never ship. The file `src/EmbeddingApp.cpp` defines the application in a namespace of the project, with the scene that it pushes nested inside it, and `Application::create()`. Its scene draws the symbol of Haylen with a comet of dots that circles it, under a title. A smaller application of the same shape draws a square that circles the middle of the safe area:

```cpp
namespace embedding {

// An app of its own project that runs the whole engine: its scene draws a square that circles the middle of the safe area.
class EmbeddingApp final : public haylen::core::Application {
  public:
    void start(haylen::core::Engine& engine) override {
        engine.getScenes().push(std::make_shared<OrbitScene>());
    }

  private:
    class OrbitScene final : public haylen::core::Scene {
      public:
        void update(haylen::core::Engine&, float deltaSeconds) override {
            angle += deltaSeconds;
        }

        void renderUi(haylen::core::Engine& engine) override {
            haylen::graphics2d::Renderer& renderer = engine.getRenderer2D();
            const haylen::math::Rect safe = engine.getViewport().getSafeRect();
            const haylen::math::Vec2 center = safe.getCenter() + haylen::math::Vec2{std::cos(angle), std::sin(angle)} * 200.0F;
            renderer.beginScreen();
            renderer.drawRect(haylen::math::Rect::fromCenter(center, {96.0F, 96.0F}), haylen::math::Color::fromHex(0xF2B84BFFU));
            renderer.drawText(*engine.getDefaultFont(), "Haylen from a C++ project", safe.getMin() + haylen::math::Vec2{48.0F, 48.0F}, {.size = 48.0F});
        }

      private:
        float angle = 0.0F;
    };
};

} // namespace embedding

std::unique_ptr<haylen::core::Application> haylen::core::Application::create() {
    return std::make_unique<embedding::EmbeddingApp>();
}
```

The class `core::Application` has three hooks:

- `configure(AppConfig&)` runs before the window exists, and may change the configuration read from `haylen.json`.
- `start(Engine&)` runs once the engine is ready.
- `stop(Engine&)` runs when the app ends.

The class `core::Engine` gives access to every service, such as `getRenderer2D()`, `getScenes()`, `getAssets()`, `getAudio()`, `getInput()`, `getActions()`, `getTimers()`, `getTweens()`, `getJobs()` and `getPlatform()`. A `core::Scene` has the same callbacks as a Lua scene. A C++ app can still run Lua by delegating to a `lua::Application`, as the [Lua guide](../core/lua.md#extending-the-engine-from-c) shows.

## Plugins of an app

A C++ app extends the engine with plugins of its own. They use the same `haylen::plugins::Plugin` interface as the built-in plugins. Its hooks are `getName`, `start`, `stop`, `installLua`, `event`, `beginFrame`, `fixedUpdate`, `update`, `render`, `renderUi`, `renderOverlay` and `endFrame`, all on the frame thread. One more hook, `packageChanged`, hears the package paths that changed while the app runs in development. So a plugin rebuilds what it built from files outside the asset cache. Two queries answer the engine: `isCapturingBack` tells whether the plugin takes the next press of the back button, and `isFrameComplete` tells whether the frame that just rendered drew everything the plugin had to draw, so the platform shows the app for the first time with a complete frame.

```cpp
engine.addPlugin(std::make_unique<MyPlugin>());
MyPlugin& plugin = engine.getPlugin<MyPlugin>();
```

The call `engine.addPlugin` adds a plugin. A plugin added while the engine runs, such as from `Application::start`, gets `start` and `installLua` at once and the event `pluginStarted`. So an application that adds a plugin first and then delegates to a `lua::Application` has the Lua module of the plugin before `source/main.lua` runs. The call `engine.getPlugin<MyPlugin>()` finds the plugin again, such as from a Lua binding through `lua::Runtime::getEngine(L)`.

A plugin is the place for native code that a C++ app compiles itself:

- A C++ SDK, pumped from `update`.
- Handlers of the platform bridge, registered in `start` with `engine.getPlatform().registerHandler`.
- A Lua module, installed in `installLua` with the binding toolkit. Its `Binding::preload` adds the module to the Varn runtime of the engine, and refuses a name that a module of Varn or of another plugin already has.

The [Lua guide](../core/lua.md#extending-the-engine-from-c) has a complete plugin with a Lua module and the application that adds it. The [native code guide](../platform/native.md) compares plugins with the platform bridge and with native libraries called through FFI, which a Lua app reaches too.
