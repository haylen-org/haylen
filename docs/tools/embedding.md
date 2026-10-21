# 📎 Embedding the engine

This guide is for C++ developers who add Haylen to a CMake project of their own. The folder `engine/` is a standalone CMake project that other projects use as a library. A project adds it in one of three ways: with `add_subdirectory`, with CPM, or with `find_package(haylen)` against a prebuilt SDK. All three give the same targets and the same CMake function. This guide covers the three ways, the SDK, the function `haylen_add_app`, protected releases of C++ apps and apps written only in C++. The [build guide](build.md) covers the toolchains of each platform and the CMake options, and the [Lua guide](../core/lua.md#extending-the-engine-from-c) shows how a C++ project adds Lua modules.

## What a project gets

| Name | Kind | Purpose |
| --- | --- | --- |
| `haylen::engine` | Library target, the alias of `haylen-engine` | The whole C++ API and every Lua binding. Its include folder is `engine/include`, and it carries the C++20 requirement to every target that links it. In browser builds it also carries `-fwasm-exceptions` and `-sSUPPORT_LONGJMP=wasm`, because Lua errors unwind as C++ exceptions and yields jump back to their resume with `longjmp`, both native to WebAssembly. |
| `haylen::runtime` | Library target, the alias of `haylen-runtime` | The player, the native host, the platform services and the backend, which opens the window, runs and draws the frames and holds the entry point. Linking it turns an executable into a Haylen app, whose sources must define `haylen::core::Application::create()`. In a source build it is an object library, so the entry points always reach the final binary. |
| `haylen_add_app` | CMake function | Builds the app of one package for the platform of the build, as [haylen_add_app](#haylen_add_app) explains. |

The engine needs CMake 3.28 or newer and a C++20 compiler, and it downloads its dependencies with CPM. When the engine brings CPM itself, as it does under `add_subdirectory`, it keeps the sources in `CPM_SOURCE_CACHE` when that variable is set, and in `.cache/cpm` of the top-level project otherwise.

When the engine is not the top-level project, its player, tests, benchmarks and tools stay off, because their options default to `PROJECT_IS_TOP_LEVEL`. The [CMake options](build.md#cmake-options) of the build guide shape a source build. The ones a project sets most often are `HAYLEN_GRAPHICS_API`, `HAYLEN_BACKEND_DIR` and `HAYLEN_SHADERC`. With `HAYLEN_SHADERC` empty, the configure downloads the pinned shader compiler, or builds it, as [the shader compiler](build.md#the-shader-compiler) explains.

```sh
cmake -S . -B build -G Ninja -DHAYLEN_GRAPHICS_API=GLCORE -DHAYLEN_SHADERC=$HOME/tools/haylen-shaderc
```

## add_subdirectory

A project that keeps a Haylen checkout, such as a Git submodule, adds the engine folder directly.

```cmake
cmake_minimum_required(VERSION 3.28...4.4)

project(my-app LANGUAGES C CXX)

add_subdirectory(external/haylen/engine haylen)

haylen_add_app(my-app
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
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

haylen_add_app(my-app
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

A local checkout works the same way with `CPMAddPackage(NAME haylen SOURCE_DIR "<checkout>/engine")`, which `samples/cpp/embedding` uses. The engine reuses the CPM that the project already included.

## find_package and the SDK

The SDK is a prebuilt engine for one platform, one backend and one graphics API. A project that uses it needs neither the sources of the engine nor `haylen-shaderc`, because the shaders are compiled into it already. Every static library that the engine links, such as Varn, Lua and Box2D, is merged into one `haylen` library, and the runtime into `haylen-runtime`.

| Installed path | Contents |
| --- | --- |
| `lib/` | The static libraries `haylen` and `haylen-runtime`, such as `libhaylen.a` and `libhaylen-runtime.a`. Linux distributions where `GNUInstallDirs` picks `lib64/` use that folder instead. |
| `lib/cmake/haylen/` | `haylen-config.cmake` and `haylen-config-version.cmake`. |
| `include/` | The public headers of the engine, the generated `haylen/core/Version.hpp`, and the headers of nlohmann/json, Dear ImGui and Lua that they include. It also holds the Dear ImGui configuration of the engine in `haylen/ui/ImGuiConfig.hpp`, with the `haylen/ui/ImGuiAssert.hpp` it includes. |
| `share/haylen/` | The file `haylen-app.cmake` with the function `haylen_add_app` and its helper scripts, the web shell and runtime script, the Apple templates, the Windows manifest, `LuaPlayer.cpp` and, in Apple SDKs, `AppleMain.cpp`. |

A project finds the SDK like any installed package:

```cmake
cmake_minimum_required(VERSION 3.28...4.4)

project(my-app LANGUAGES C CXX)

find_package(haylen REQUIRED)

haylen_add_app(my-app
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/haylen-macos-release
```

The package config imports `haylen::engine` and `haylen::runtime` and defines `haylen_add_app`. It sets four variables to the values that the SDK was built with:

| Variable | Value |
| --- | --- |
| `HAYLEN_PLATFORM` | The platform, such as `macos`. |
| `HAYLEN_BACKEND` | The name of the folder of the backend, such as `sokol`. |
| `HAYLEN_GRAPHICS` | The graphics API, such as `METAL`. |
| `HAYLEN_DESKTOP` | Whether the platform is a desktop. |

Its version file accepts requests for the same major and minor version. Use a web SDK from a project that Emscripten's `emcmake` configures. Android, iOS and tvOS projects add the engine from source through `add_subdirectory`.

### The sdk command

```sh
haylen sdk
haylen sdk --platform web --output dist/haylen-web
haylen sdk --config Release --check-consumers package
```

The command `sdk` builds the SDK of a platform and installs it where `find_package(haylen)` finds it.

| Option | Default | Meaning |
| --- | --- | --- |
| `--platform` | this machine | `macos`, `linux`, `windows`, `web` (WebGPU) or `web-webgl2`. |
| `--config` | `Release` | `Debug`, `Release` or `RelWithDebInfo`. |
| `--output` | `build/sdk/haylen-<platform>-<config>` | The install prefix of the SDK. |
| `--jobs` | the processors minus one | The parallel jobs of the build. |
| `--check-consumers` | off | Then builds a project in each way it lists, as [checking the three ways](#checking-the-three-ways) explains. |

The command configures `engine/` on its own in `build/sdk-build-<platform>-<config>`. It turns `HAYLEN_BUILD_SDK` on and the tests, the player and the benchmarks off. Then it builds the `haylen-sdk` target and installs the `haylen-sdk` component.

### Checking the three ways

The option `--check-consumers` builds `engine/tests/consumer` after the SDK. That is a CMake project of its own with a small C++ app, which adds the engine the way another repository does. It builds once for every way it lists:

| Way | How the project adds the engine |
| --- | --- |
| `subdirectory` | With `add_subdirectory`. |
| `cpm` | With `CPMAddPackage`. |
| `package` | With `find_package`, against the SDK that the command just installed, through `CMAKE_PREFIX_PATH`. |

```sh
haylen sdk --check-consumers
haylen sdk --check-consumers subdirectory cpm
```

Without a list it builds all three. The project reads the way from its cache variable `HAYLEN_CONSUMER_MODE`. Each way builds in `build/consumers/<mode>-<config>`. The `subdirectory` and `cpm` ways share the dependency cache and the shader compiler of the engine builds. The option builds for this machine only, so it takes no other `--platform`. The `desktop` job of [continuous integration](testing.md#continuous-integration) runs the `package` way on macOS, Linux and Windows.

## haylen_add_app

The file `engine/cmake/haylen-app.cmake` defines the function that builds the app of a package for the platform of the build.

```cmake
haylen_add_app(<target> PACKAGE <folder> [SOURCES <files>...] [CPP] [APPLE_PROJECT <folder>] [WEB_SHELL <file>])
```

| Argument | Meaning |
| --- | --- |
| `<target>` | The name of the executable, or of the shared library on Android. |
| `PACKAGE` | The package folder, which holds `app.json`, `source/` and `content/`. Its `app.json` must set `name`, `identifier` and `version`. They become the display name, the bundle identifier and the version of the app, so the app and the runtime always agree. The configure fails when one is missing. |
| `SOURCES` | The source files of the app, such as native handlers of the platform bridge. |
| `CPP` | The sources define `haylen::core::Application::create()`. Without it, the function adds `LuaPlayer.cpp`, whose application runs `source/main.lua`, so a Lua app needs no sources at all. |
| `APPLE_PROJECT` | A folder with `mac/Info.plist.in`, `ios/Info.plist.in`, `ios/LaunchScreen.storyboard`, `tvos/Info.plist.in`, `tvos/LaunchScreen.storyboard` and `PrivacyInfo.xcprivacy`. The default is `engine/platform/apple`, whose privacy manifest declares the APIs with required reasons that the engine calls, as the [privacy manifest](distribution.md#privacy-manifest) lists them. |
| `WEB_SHELL` | The HTML shell of the browser build. The default is `engine/platform/web/shell.html`. |

The function links `haylen::runtime`, which brings the system libraries of the platform. On Apple platforms those are the system frameworks of `HAYLEN_APPLE_FRAMEWORKS_MACOS`, `_IOS`, `_MACCATALYST` and `_TVOS` in `engine/cmake/haylen-app.cmake`. The Apple artifacts publish the same lists as `haylen-frameworks.json` for the projects of Lua apps, so C++ apps and Lua apps link the same frameworks. On Apple platforms the function also adds `AppleMain.cpp`, whose `main` enters the runtime through `haylen_main`, because the Sokol backend leaves `main` to the app there.

The function deploys the package into the app of each platform. Only `app.json`, `source/` and `content/` ship, with `plugin.json` and `source/` of every plugin that `app.json` lists. So platform projects, notes and build files can share the package folder. The function also writes the absolute path of the package to `bin/<target>/package.txt`.

| Platform | What the build makes | Where the runtime opens the package |
| --- | --- | --- |
| Windows, Linux | An executable in `bin/<target>/` of the build tree. The target `<target>-package` creates `bin/<target>/app` after every build and links the entries of the package into it: directory junctions and a hard link on Windows, and symbolic links elsewhere. Edited files show up without a rebuild. Windows builds are apps of the Windows subsystem, and the debugger of Visual Studio starts in their folder. | `app/` next to the executable, or `app.zip` next to it. |
| macOS, iOS, tvOS, Mac Catalyst | An app bundle in `bin/<target>/`, or in `bin/` with the Xcode generator. The files of the package become resources under `Resources/app` through `MACOSX_PACKAGE_LOCATION`, except `.DS_Store`. The list of files is globbed with `CONFIGURE_DEPENDS`, so the next build picks up new files. | `Resources/app` of the bundle, or `Resources/app.zip`. |
| Web | The page `<target>.html` with the shell. The options `--preload-file` pack the package into `<target>.data` under `/app`. | `/app` in the virtual file system, unless the page hands over another package. |
| Android | The shared library `bin/<target>/lib<target>.so`, which `HaylenActivity` loads. The Gradle project of the app copies the package into the APK assets. The command `run-cpp --platform android` prepares that project, `platform/android` of the package folder or a copy of the Android template, as [run-cpp](distribution.md#run-cpp) describes. | The folder `app/` of the APK assets. |

Any app opens the package that the first argument of its command line names instead of the bundled one, as [development mode](distribution.md#development-mode) explains.

### Windows

C++ apps and the `haylen` player embed `engine/platform/windows/haylen.manifest`, the application manifest of the engine. The function `haylen_add_app` adds it to the sources of every Windows target, and the SDK installs it with the other platform files. It selects two things:

- Version 6 of the Common Controls, where the task dialogs of [`haylen.dialogs`](../lua-api/dialogs.md) live.
- UTF-8 as the code page of the process, since the engine keeps every text and path in UTF-8.

Lua apps embed the manifest of their Windows project instead, which selects the same, as the [Windows template](distribution.md#windows) describes. Linux apps load GIO for the theme and GTK 3 for the dialogs at run time, where the system has them, so building needs neither.

### Apple bundles

C++ apps target iOS 16.3 on iPhone and iPad and tvOS 16.3, because the engine needs the C++ library of those releases. They sign automatically. macOS apps target macOS 14.0, the first release with the display link of `NSView` that `sokol_app` draws with. The bundle name, identifier and version come from `name`, `identifier` and `version` in the `app.json` of the package. The templates in `engine/platform/apple` hold the `Info.plist` files, the launch screens and the privacy manifest of the engine, which every bundle carries among its resources.

The function fills placeholders of the templates from `app.json`:

| Placeholder | Template | Value |
| --- | --- | --- |
| `@HAYLEN_ORIENTATIONS@`, `@HAYLEN_ORIENTATIONS_IPAD@` | iOS | The orientations of iPhone and iPad from `orientation`: `landscape` allows both landscape sides, `portrait` allows portrait, and upside down on iPad, and `any` allows all of them. |
| `@HAYLEN_STATUS_BAR_HIDDEN@`, `@HAYLEN_STATUS_BAR_STYLE@` | iOS | The status bar that the app launches with, from `window.statusBar`. |
| `@HAYLEN_UI_ELEMENT@` | macOS, at the end of the dictionary | `LSUIElement` when `window.showInTaskbar` is `false`, so the app never shows a Dock icon. |

Pass `APPLE_PROJECT` to use templates of your own with the same layout and placeholders.

Xcode signs the bundles it builds, while other generators leave only the signature that the linker gives the executable. That signature binds neither the bundle identifier nor the `Info.plist`, and macOS opens the window of an unsigned Mac Catalyst app at the top left of the screen instead of where the app asks. So for Mac Catalyst without the Xcode generator, `haylen_add_app` signs the bundle ad hoc after linking, as Xcode does.

The Apple parts of the plugins of a C++ app reach the bundle through the file that `HAYLEN_PLUGINS_FILE` names, which `run-cpp` writes, as the [plugin guide](../platform/plugins.md#c-apps) describes. Plugins with Swift sources need `enable_language(Swift)` in the project, as the C++ template has.

### Protected releases

The cache variable `HAYLEN_PROTECTED_RELEASE`, off by default, makes every app of the project ship its [protected release](../content/content.md) instead of the package as it is. That is what the release builds of Lua apps do: the Lua modules become bytecode and every asset is encrypted in shards. The app opens them with the bootstrap that the build compiles into it, so a C++ app and a Lua app read their content through the same code. The command `run-cpp --config Release` turns it on, and any other configuration turns it off.

| Variable | Default | Meaning |
| --- | --- | --- |
| `HAYLEN_PROTECTED_RELEASE` | `OFF` | Ships the protected release of each app, with its bootstrap compiled in. |
| `HAYLEN_CONTENT_TOOL` | empty | The content tool of the host, `haylen-content`. Empty uses the `haylen-content` target of the build, which exists when the engine builds its tools for this machine. A project that builds for another platform names the tool of the desktop artifacts, as `run-cpp` does. Without either, the configure fails and says how to get one. |
| `HAYLEN_PREVIOUS_RELEASE` | empty | A folder that holds the last release of the app that shipped. The new release follows it instead of the last release of the build tree, so a clean machine keeps the shards of unchanged content. The option `--previous-release` of `run-cpp` sets it. |

The build runs the content tool on the package with the keys of the app from its [key folder](../content/content.md#key-folders). On the machine of the developer it creates the keys the first time, with a warning to back them up. It never creates them in continuous integration, where `HAYLEN_KEYS_DIR` names the folder of the keys it receives. The release goes into `haylen-release/<target>/` of the build tree, with the bootstrap next to it. Each build reuses the shards of the release before it, and the release builds again when a file of the package changes. The profile follows the platform, `apple`, `android`, `web`, `windows` or `linux`, and the build number follows the version, so 1.2.3 is 1002003.

| Platform | What ships |
| --- | --- |
| Windows, Linux | The release in `app/` next to the executable, copied after each build. |
| macOS, iOS, tvOS, Mac Catalyst | The release in `Resources/app` of the bundle on macOS and Mac Catalyst, or `app` at the root of an iOS or tvOS bundle, copied before the bundle is signed. |
| Web | The manifests and the app domain, preloaded at `/app`, and the whole release in `app/` next to the page. The content streams from there by range into the [content store of the browser](../content/content.md#the-web). |
| Android | The file `bin/<target>/release.txt`, which names the release folder that `run-cpp` places in the assets of the Android project instead of the package. |

An app with a content server in `app.json` ships only what its required content needs, and the shards of named packs stay in `haylen-release/<target>/packs/`, as the [delivery guide](../content/content.md#delivery) explains. A release also builds without symbols, as Lua apps do:

- Android and Linux link with `--gc-sections` and ship stripped. The symbols stay in a `.debug` file in `haylen-release/<target>/`, which the binary names in its debug link.
- Windows links with `/DEBUG /OPT:REF /OPT:ICF` and keeps the PDB of the public symbols in that folder.
- Apple platforms keep a dSYM and strip the app as they build it.
- The web keeps the symbol map of Emscripten in that folder, and minifies the workers next to the page.
- The Android library exports only the entry points of Java and GameActivity.

The command `run-cpp --config Release` inspects the built app before it launches, and stops when it holds Lua text, a file of the package as it is, symbol files or a key in any form. The command `haylen steam <project> --platform <this machine>` builds the same release and lays it out as Steam depots, as [steam](distribution.md#steam) describes.

## An app written in C++

The command `haylen new <folder> --kind cpp` creates a C++ app from the [C++ template](distribution.md#cpp). It is a CMake project that adds the engine from the folder that `HAYLEN_ENGINE_SOURCE` names, or from an installed SDK when that variable is empty. It builds the target `main` from its sources, with the [bridge example](distribution.md#the-bridge-example) of the templates and the Android project and page code that it shares with Lua apps. The command `haylen run-cpp <folder>` builds and runs it on every platform this machine reaches, and passes `HAYLEN_ENGINE_SOURCE` itself.

```cmake
cmake_minimum_required(VERSION 3.28...4.4)
project(App LANGUAGES C CXX)

if(APPLE)
  enable_language(OBJCXX Swift)
endif()

set(HAYLEN_ENGINE_SOURCE "" CACHE PATH "The engine folder of a Haylen checkout. Empty finds an installed SDK with find_package.")
if(HAYLEN_ENGINE_SOURCE)
  add_subdirectory("${HAYLEN_ENGINE_SOURCE}" haylen)
else()
  find_package(haylen REQUIRED)
endif()

set(sources src/App.cpp)
if(APPLE)
  list(APPEND sources src/AppBridge.mm)
  set_source_files_properties(src/AppBridge.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
endif()

haylen_add_app(main CPP
  SOURCES ${sources}
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(EMSCRIPTEN)
  target_link_options(main PRIVATE "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/platform/web/app.js")
endif()
```

The sample `samples/cpp/embedding` is an app project of its own that uses no Lua. Its `CMakeLists.txt` adds the engine from the checkout that `HAYLEN_ENGINE_SOURCE` names, in the way that its cache variable `CPP_EMBEDDING_MODE` picks: `subdirectory`, `cpm` or `package`. It builds the app with the `CPP` option. The command `haylen run-cpp samples/cpp/embedding` builds and runs it from the root of the repository.

```cmake
haylen_add_app(main CPP
  SOURCES src/EmbeddingApp.cpp
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

The sample folder is also its package. It holds only `app.json`, since the application never runs Lua, and `CMakeLists.txt` and `src/` next to it never ship. The file `src/EmbeddingApp.cpp` defines the application in a namespace of the project, with the scene that it pushes nested inside it, and `Application::create()`.

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

- `configure(AppConfig&)` runs before the window exists, and may change the configuration read from `app.json`.
- `start(Engine&)` runs once the engine is ready.
- `stop(Engine&)` runs when the app ends.

The class `core::Engine` gives access to every service, such as `getRenderer2D()`, `getScenes()`, `getAssets()`, `getAudio()`, `getInput()`, `getActions()`, `getTimers()`, `getTweens()`, `getJobs()` and `getPlatform()`. A `core::Scene` has the same callbacks as a Lua scene. A C++ app can still run Lua by delegating to a `lua::Application`, as the [Lua guide](../core/lua.md#extending-the-engine-from-c) shows.

## Plugins of an app

An app that compiles the engine extends it with plugins of its own. They use the same `haylen::plugins::Plugin` interface as the built-in plugins. Its hooks are `getName`, `start`, `stop`, `installLua`, `event`, `beginFrame`, `fixedUpdate`, `update`, `render`, `renderUi`, `renderOverlay` and `endFrame`, all on the frame thread. One more hook, `packageChanged`, hears the package paths that changed while the app runs in development. So a plugin rebuilds what it built from files outside the asset cache.

```cpp
engine.addPlugin(std::make_unique<MyPlugin>());
MyPlugin& plugin = engine.getPlugin<MyPlugin>();
```

The call `engine.addPlugin` adds a plugin. A plugin added while the engine runs, such as from `Application::start`, gets `start` and `installLua` at once and the event `pluginStarted`. So an application that adds a plugin first and then delegates to a `lua::Application` has the Lua module of the plugin before `source/main.lua` runs. The call `engine.getPlugin<MyPlugin>()` finds the plugin again, such as from a Lua binding through `lua::Runtime::getEngine(L)`.

A plugin is the place for native code that the app compiles itself:

- A C++ SDK, pumped from `update`.
- Handlers of the platform bridge, registered in `start` with `engine.getPlatform().registerHandler`.
- A Lua module, installed in `installLua` with the binding toolkit. Its `Binding::preload` adds the module to the Varn runtime of the engine, and refuses a name that a module of Varn or of another plugin already has.

The [Lua guide](../core/lua.md#extending-the-engine-from-c) has a complete plugin with a Lua module and the application that adds it. The [native code guide](../platform/native.md) compares plugins with the platform bridge and with native libraries called through FFI, which need no compiled engine.
