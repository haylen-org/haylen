# Embedding the engine

The `engine/` folder is a standalone CMake project that other projects consume as a library. A project adds it with `add_subdirectory`, with CPM or with `find_package(haylen)` against a prebuilt SDK, and all three ways give it the same targets and the same CMake function. This guide covers the three ways, `haylen_add_app` and an app written only in C++. The [build guide](build.md) covers the platform toolchains, and the [Lua guide](lua.md#extending-the-engine-from-c) shows how a C++ project adds Lua modules.

## What a project gets

| Name | Kind | Purpose |
| --- | --- | --- |
| `haylen::engine` | Library target | The whole C++ API and every Lua binding. Its include folder is `engine/include`, and it carries the C++20 requirement to every target that links it. In browser builds it also carries `-fexceptions`, because Lua errors unwind as C++ exceptions. |
| `haylen::runtime` | Library target | The Sokol app entry point, the platform host and the platform services. Linking it turns an executable into a Haylen app, whose sources must define `haylen::core::Application::create()`. In a source build it is an object library, so the entry points always reach the final binary. |
| `haylen_add_app` | CMake function | Builds the app of one package for the current platform. |

When the engine is not the top-level project, its player, tests and benchmarks are off, because `HAYLEN_BUILD_PLAYER`, `HAYLEN_BUILD_TESTS` and `HAYLEN_BUILD_BENCHMARKS` default to `PROJECT_IS_TOP_LEVEL`. These cache variables also shape a source build.

| Variable | Default | Meaning |
| --- | --- | --- |
| `HAYLEN_SOKOL_SHDC` | Empty | Path to the `sokol-shdc` executable that compiles the engine shaders. A source build fails without it. `python3 make.py tools` downloads the pinned version into `.tools/` of a Haylen checkout. |
| `HAYLEN_RENDER_BACKEND` | `AUTO` | `AUTO`, `METAL`, `D3D11`, `GLCORE`, `GLES3` or `WGPU`. `AUTO` picks Metal on Apple platforms, D3D11 on Windows, GLES3 on Android, WebGPU in the browser and OpenGL Core on Linux. |
| `HAYLEN_ENABLE_SANITIZERS` | `OFF` | AddressSanitizer and UndefinedBehaviorSanitizer on desktop builds that do not use MSVC. |
| `HAYLEN_BUILD_SDK` | `OFF` | Adds the `haylen_sdk` target and the install rules of the SDK. |

The engine needs CMake 3.28 or newer and a C++20 compiler, and it downloads its dependencies with CPM. When the engine bootstraps CPM itself, as it does under `add_subdirectory`, it keeps the sources in `CPM_SOURCE_CACHE` when that variable is set and in `.cache/cpm` of the top-level project otherwise.

## add_subdirectory

A project that keeps a Haylen checkout, for example as a Git submodule, adds the engine folder directly.

```cmake
cmake_minimum_required(VERSION 3.28...4.4)

project(my_app LANGUAGES C CXX)

add_subdirectory(external/haylen/engine haylen)

haylen_add_app(my-app
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

```sh
cmake -S . -B build -G Ninja -DHAYLEN_SOKOL_SHDC=/path/to/sokol-shdc
cmake --build build
```

## CPM

With [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake), the engine is a package like any other. The engine sits in the `engine` folder of the repository, so a package fetched from Git names that folder with `SOURCE_SUBDIR`.

```cmake
cmake_minimum_required(VERSION 3.28...4.4)

project(my_app LANGUAGES C CXX)

include(cmake/CPM.cmake)
CPMAddPackage(
  NAME haylen
  GIT_REPOSITORY <repository url>
  GIT_TAG <tag or commit>
  SOURCE_SUBDIR engine
)

haylen_add_app(my-app
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

A local checkout works the same way with `CPMAddPackage(NAME haylen SOURCE_DIR "<checkout>/engine")`, which is how `samples/cpp/embedding` does it. The engine reuses the CPM that the project already included, and `HAYLEN_SOKOL_SHDC` is passed on the command line as in the previous section.

## find_package and the SDK

The SDK is a prebuilt engine for one platform and one graphics backend. It needs neither the engine sources nor `sokol-shdc`, because the shaders are already compiled into it.

```sh
python3 make.py sdk --platform macos --config Release
```

| Option | Default | Meaning |
| --- | --- | --- |
| `--platform` | The host | `macos`, `linux`, `windows`, `web` (WebGPU) or `web-webgl2`. |
| `--config` | `Release` | `Debug`, `Release` or `RelWithDebInfo`. |
| `--output` | `build/sdk/haylen-<platform>-<config>` | Install prefix of the SDK. |
| `--jobs` | The processor count minus one | Parallel build jobs. |

The command configures `engine/` on its own in `build/sdk-build-<platform>-<config>` with `HAYLEN_BUILD_SDK=ON`, builds the `haylen_sdk` target and installs the `haylen_sdk` component. Every static library the engine links, such as Varn, Lua and Box2D, is merged into one `haylen` library, and the runtime into `haylen_runtime`.

| Installed path | Contents |
| --- | --- |
| `lib/` | The `haylen` and `haylen_runtime` static libraries, such as `libhaylen.a` and `libhaylen_runtime.a`. Linux distributions where GNUInstallDirs picks `lib64/` use that folder instead. |
| `lib/cmake/haylen/` | `haylen-config.cmake` and `haylen-config-version.cmake`. |
| `include/` | The public engine headers, the generated `haylen/core/Version.hpp`, the nlohmann/json, Dear ImGui and Lua headers that they include, and the Dear ImGui configuration of the engine in `haylen/ui/ImGuiConfig.hpp` with the `haylen/ui/ImGuiAssert.hpp` it includes. |
| `share/haylen/` | `haylen_add_app` and its helper script, the web shell and runtime script, the Apple templates, `LuaPlayer.cpp` and, in Apple SDKs, `AppleMain.cpp`. |

A project then finds the SDK like any installed package.

```cmake
cmake_minimum_required(VERSION 3.28...4.4)

project(my_app LANGUAGES C CXX)

find_package(haylen REQUIRED)

haylen_add_app(my-app
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/haylen-macos-release
```

The package config imports `haylen::engine` and `haylen::runtime`, defines `haylen_add_app` and sets `HAYLEN_PLATFORM`, `HAYLEN_BACKEND` and `HAYLEN_DESKTOP` to the values the SDK was built with. Its version file accepts requests for the same major and minor version. A web SDK is used from a project configured with Emscripten's `emcmake`. Android, iOS and tvOS projects add the engine from source through `add_subdirectory`, as the [build guide](build.md) describes.

## haylen_add_app

`engine/cmake/haylen-app.cmake` defines the function that builds an app.

```cmake
haylen_add_app(<target> PACKAGE <folder> [SOURCES <files>...] [CPP] [APPLE_PROJECT <folder>] [WEB_SHELL <file>])
```

| Argument | Meaning |
| --- | --- |
| `<target>` | Name of the executable, or of the shared library on Android. |
| `PACKAGE` | The package folder, which holds `app.json`, `source/` and `content/`. Its `app.json` must set `name`, `identifier` and `version`, which become the display name, the bundle identifier and the version of the app, so the app and the runtime always agree. Configuration fails when one is missing. |
| `SOURCES` | Source files of the app. |
| `CPP` | The sources define `haylen::core::Application::create()`. Without it the function adds `LuaPlayer.cpp`, whose application runs `source/main.lua`, so a Lua app needs no sources at all. |
| `APPLE_PROJECT` | Folder with `mac/Info.plist.in`, `ios/Info.plist.in`, `ios/LaunchScreen.storyboard`, `tvos/Info.plist.in` and `tvos/LaunchScreen.storyboard`. The default is `engine/platform/apple`. |
| `WEB_SHELL` | HTML shell of the browser build. The default is `engine/platform/web/shell.html`. |

The function links `haylen::runtime` and deploys the package the way the Axmol template deploys content. On Apple platforms it also adds `AppleMain.cpp`, whose `main` enters the runtime through `haylen_main`, because `sokol_app` leaves `main` to the app there. Only `app.json`, `source/` and `content/` are deployed, so platform projects, notes and build files can share the package folder.

| Platform | Result |
| --- | --- |
| Windows and Linux | An executable in `bin/<target>/` of the build tree, and a `SYNC_PACKAGE-<target>` target that creates an `app` folder next to it with links to `app.json`, `source/` and `content/` of the package folder, so edited files show up without a rebuild. Windows builds are GUI apps whose Visual Studio debugger starts in that folder. |
| macOS, iOS and tvOS | An app bundle in `bin/<target>/` with the package files under `Resources/app`. iOS and tvOS target version 17.0. |
| Web | `<target>.html` with the shell, and the package preloaded at `/app` in the virtual file system. |
| Android | A shared library that the Gradle app packages. The Gradle script `engine/platform/android/haylen-app.gradle` copies the package into the APK assets. |

## An app written in C++

`samples/cpp/embedding` is an app project of its own that uses no Lua. Its `CMakeLists.txt` adds the engine in the mode chosen by the `CPP_EMBEDDING_MODE` cache variable (`subdirectory`, `cpm` or `package`) from the checkout named by `HAYLEN_ENGINE_SOURCE`, and builds the app with the `CPP` option.

```cmake
haylen_add_app(embedding CPP
  SOURCES src/EmbeddingApp.cpp
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

The sample folder is also its package, which holds only `app.json` since the application never runs Lua, so `CMakeLists.txt` and `src/` next to it are never deployed. `src/EmbeddingApp.cpp` defines the application in a namespace of the project, with the scene it pushes nested inside it, and `Application::create()`.

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

`core::Application` has three hooks. `configure(AppConfig&)` runs before the window exists and may change the configuration read from `app.json`, `start(Engine&)` runs once the engine is ready, and `stop(Engine&)` runs when the app ends. `core::Engine` gives access to every service, such as `getRenderer2D()`, `getScenes()`, `getAssets()`, `getAudio()`, `getInput()`, `getActions()`, `getTimers()`, `getTweens()`, `getJobs()` and `getPlatform()`, and `core::Scene` has the same callbacks as a Lua scene. A C++ app can still run Lua by delegating to a `lua::Application`, as the [Lua guide](lua.md#extending-the-engine-from-c) shows.

## Checking the three modes

`make.py embedding` builds `samples/cpp/embedding` the way another repository would consume the engine.

```sh
python3 make.py embedding --mode subdirectory
python3 make.py embedding --mode cpm
python3 make.py embedding --mode package
```

Each mode builds in `build/embedding-<mode>-<config>`, with `--config` defaulting to `Debug`, and places the app in `bin/embedding/` of that tree. The `package` mode first runs `make.py sdk` for the host with the same configuration and points `CMAKE_PREFIX_PATH` at the result. The CI workflow runs `python make.py embedding --mode package --config Release` on macOS, Linux and Windows, as the [testing guide](testing.md#continuous-integration) describes.
