# Building Haylen

`make.py` at the repository root is the single entry point for building, running, testing and packaging the engine and its apps on every platform. It wraps CMake, downloads the pinned tools it needs into `.tools/`, and keeps one build tree per platform and configuration under `build/`. This guide covers building the engine, what each platform needs, how dependencies are declared, how `haylen_add_app` deploys a package into a C++ app, the web runtime and the Tiny Island asset tools. The [distribution guide](distribution.md) covers the prebuilt engine artifacts, the platform templates and the commands that create, run and package apps, the [embedding guide](embedding.md) covers consuming the engine from another CMake project, and the [testing guide](testing.md) covers the test suite.

## Requirements

Every host needs Python 3.10 or newer, CMake 3.28 or newer and a C++20 compiler. Ninja is needed by every build except iOS, tvOS, macOS with `--xcode`, and Windows builds, which fall back to the default CMake generator when Ninja is not on `PATH`. Run the script with `python3 make.py` on macOS and Linux and with `python make.py` on Windows.

`make.py` downloads the remaining tools the first time a command needs them:

| Tool | Version | Location | Needed by |
| --- | --- | --- | --- |
| `sokol-shdc` | commit `11d0cf6` of `floooh/sokol-tools-bin` | `.tools/sokol-shdc` | Every build. Prebuilt binaries exist for macOS (arm64 and x64), Linux (arm64 and x64) and Windows (x64). |
| Emscripten SDK | 6.0.10 | `.tools/emsdk` | Web builds. It is cloned with `git`. |
| Gradle | 9.8.0 | `.tools/gradle-9.8.0` | The Android library and Android apps. |
| XcodeGen | 2.46.0, the release zip checked against its pinned SHA-256 hash | `.tools/xcodegen` | Apple apps whose plugins add to the Apple project, which make.py generates again. macOS only. |

What each platform needs on top of that:

| Platform | `--platform` | Host | Graphics backend | Generator | Also install |
| --- | --- | --- | --- | --- | --- |
| macOS | `macos` | macOS | Metal | Ninja, or Xcode with `--xcode` | Xcode Command Line Tools, or Xcode for `--xcode`. |
| Windows | `windows` | Windows | Direct3D 11 | Ninja when it is on `PATH`, otherwise the CMake default (Visual Studio) | Visual Studio with the C++ workload. Ninja builds run from a developer command prompt so MSVC is on `PATH`. |
| Linux | `linux` | Linux | OpenGL core | Ninja | GCC or Clang and the X11, Xi, Xcursor, Xext, Xrandr and OpenGL development packages, for example `libx11-dev libxi-dev libxcursor-dev libxext-dev libxrandr-dev libgl1-mesa-dev` on Debian and Ubuntu. |
| iOS | `ios` | macOS | Metal | Xcode | Xcode. Apps also need XcodeGen only to regenerate the template project. |
| tvOS | `tvos` | macOS | Metal | Xcode | Xcode, and the tvOS simulator runtime (`xcodebuild -downloadPlatform tvOS`) to run apps on it. |
| Android | `android` | any | OpenGL ES 3 | Ninja | Android SDK with NDK `30.0.16248370`, CMake `4.1.2`, platform 37 and platform-tools, and JDK 17. |
| Web | `web` | any | WebGPU | Ninja through `emcmake` | `git`, and a browser with WebGPU to run it. |
| Web | `web-webgl2` | any | WebGL2 | Ninja through `emcmake` | `git`, and a browser with WebGL2 to run it. |

`make.py` refuses to configure macOS, iOS, tvOS, Linux and Windows builds on any other host. Android and web builds work from every host.

## Build trees

Every platform and configuration gets its own tree at `build/<platform>-<config>`, with the configuration in lowercase, such as `build/macos-debug` or `build/web-webgl2-release`, and builds with sanitizers get trees of their own at `build/<platform>-<config>-<sanitizers>`, such as `build/macos-debug-thread`. Executables and apps land in `bin/<target>/` inside the tree. `python3 make.py clean` removes the whole `build/` folder but keeps `.tools/` and the dependency cache.

`build` configures a tree when it has no CMake cache yet or when `--backend` differs from the backend the tree was configured with. Run `clean` before switching the generator with `--xcode`, because CMake cannot change the generator of a tree. The prebuilt engine artifacts that apps use have trees of their own, described in the [distribution guide](distribution.md#engine-artifacts).

## Commands

| Command | Purpose |
| --- | --- |
| `tools` | Download the pinned build tools. |
| `configure` | Generate a build tree of the engine. |
| `build` | Configure the engine when needed and build it. |
| `test` | Build and run the engine tests on the host. |
| `engine` | Build the prebuilt engine artifacts that apps use. See the [distribution guide](distribution.md#engine). |
| `new` | Create an app with the starter code and every platform template. See the [distribution guide](distribution.md#new). |
| `run` | Run an app in the desktop player with hot reload, or build it from the templates for a platform. See the [distribution guide](distribution.md#run). |
| `run-cpp` | Build and run a C++ app project. See the [distribution guide](distribution.md#run-cpp). |
| `package` | Zip the package of an app. |
| `shaders` | Compile the shaders of an app. See the [shader guide](shaders.md). |
| `serve` | Serve a folder with the headers WebAssembly pages need. See the [distribution guide](distribution.md#serve). |
| `coverage` | Measure engine code coverage. |
| `format` | Format the C, C++ and Objective-C++ sources. |
| `bench` | Build and run the sprite benchmark in Release. |
| `sdk` | Build the engine SDK and install it for `find_package(haylen)`. |
| `embedding` | Build the C++ embedding sample. |
| `assets` | Import the Tiny Swords pack into Tiny Island. |
| `map` | Generate the Tiny Island Tiled map. |
| `clean` | Remove every build tree. |

### Build options

`configure`, `build` and `test` share these options:

| Option | Default | Meaning |
| --- | --- | --- |
| `--platform` | the host | `macos`, `linux`, `windows`, `ios`, `tvos`, `android`, `web` or `web-webgl2`. `test` always uses the host. |
| `--config` | `Debug` | `Debug`, `Release` or `RelWithDebInfo`. |
| `--backend` | the platform default | `METAL`, `D3D11`, `GLCORE`, `GLES3` or `WGPU`, passed as `HAYLEN_RENDER_BACKEND`. The web platforms always choose their own backend. |
| `--xcode` | off | Use the Xcode generator on macOS. |
| `--sanitizers` | none | `address` for AddressSanitizer and UndefinedBehaviorSanitizer or `thread` for ThreadSanitizer, in a tree of its own and passed as `HAYLEN_SANITIZERS`. MSVC builds ignore it. The [testing guide](testing.md#sanitizers) explains both. |
| `--target` | everything | CMake target that `build` builds. |
| `--jobs` | CPU count minus one | Parallel build and test jobs. |

The runtime links the system libraries of each platform's default backend, plus OpenGL on Windows when the backend is not `D3D11`. That makes `--backend GLCORE` on Windows the one desktop override with a complete link setup.

### tools

```sh
python3 make.py tools [--emsdk] [--gradle]
```

Downloads `sokol-shdc`, XcodeGen 2.46.0 on macOS, and Emscripten 6.0.10 or Gradle 9.8.0 when asked, and prints where each one lives. Every other command downloads what it needs by itself, so `tools` is mainly useful to warm up a machine or a CI cache.

### configure

```sh
python3 make.py configure --platform ios
```

Generates the build tree with the options above. Every tree receives `-DHAYLEN_SOKOL_SHDC` and `-DCMAKE_BUILD_TYPE`. The platforms add their own settings:

- iOS and tvOS use the Xcode generator with `CMAKE_SYSTEM_NAME` set to `iOS` or `tvOS`, the `arm64` architecture, the 16.3 deployment target, and tests and the player turned off. macOS trees target macOS 13.3. The [distribution guide](distribution.md#haylenxcframework) explains these minimums.
- Android uses the NDK toolchain with `ANDROID_ABI=arm64-v8a`, `ANDROID_PLATFORM=android-27` and the static C++ library, with the tests turned off. The player is the shared library `libhaylen.so`, the one the [Android library](distribution.md#the-android-library) packages.
- The web platforms run CMake through `emcmake`, turn the tests off and force `WGPU` for `web` and `GLES3` for `web-webgl2`.

`make.py` builds with the pinned NDK, `ndk/30.0.16248370` inside the SDK that `ANDROID_HOME` or `ANDROID_SDK_ROOT` names, even when the environment names another NDK, because the engine and its dependencies build against its headers.

### build

```sh
python3 make.py build
python3 make.py build --platform web-webgl2 --config Release --target tiny-island
```

Configures the tree when needed and runs `cmake --build`. Without `--target` it builds everything the tree defines: the engine, the `haylen` player on desktop, web and Android, and the tests and the sprite benchmark on desktop. Useful targets are `haylen`, `haylen_tests` and `haylen-sprite-benchmark`. Apps are not part of the workspace: `make.py run` runs Lua apps from their folders and `make.py run-cpp` builds C++ app projects, as the [distribution guide](distribution.md) describes.

### test

```sh
python3 make.py test
python3 make.py test --config Release --sanitizers address
```

Builds `haylen_tests` for the host and runs it through `ctest` with `--output-on-failure` in parallel. See the [testing guide](testing.md).

### coverage

```sh
python3 make.py coverage
```

Configures `build/coverage` in Debug with `HAYLEN_ENABLE_COVERAGE=ON` and without samples and the player, builds and runs the tests, merges the profiles with `llvm-profdata`, prints the `llvm-cov report` table and writes an HTML report to `build/coverage/coverage/html/index.html`. The report leaves out dependencies, tests, generated files and the platform backends in `engine/src/platform/{apple,android,web,windows,linux,sokol}`, which need a real device. Coverage uses LLVM source-based coverage, so it needs Clang. On macOS the LLVM tools come from Xcode through `xcrun`, and elsewhere they must be on `PATH`. It accepts `--jobs` and `--sanitizers`.

### format

```sh
python3 make.py format
python3 make.py format --check
```

Runs `clang-format --style=file` on every `.h`, `.hpp`, `.c`, `.cpp`, `.m` and `.mm` file under `engine/include`, `engine/src`, `engine/tests`, `samples` and `templates`, then lists every multi-line lambda outside a `// clang-format off` and `// clang-format on` region. `--check` rewrites nothing and fails on formatting differences or unguarded lambdas. `clang-format` must be on `PATH`, and CI pins version 23.1.1.

### bench

```sh
python3 make.py bench
python3 make.py bench --suite algorithms
python3 make.py bench --suite procedural
python3 make.py bench --suite lua
```

Builds `haylen-sprite-benchmark` in Release for the host and runs it on the local GPU. It accepts `--jobs`. The [rendering guide](rendering.md#sprite-benchmark) describes the phases and the recorded results. `--suite algorithms` builds and runs `haylen-algorithm-benchmark` instead, which times A*, weighted A*, jump point search, flow fields and hierarchical path finding on a 512 by 512 grid, navigation mesh builds and funnel paths, ORCA crowd steps on one thread, on the job system and on the job system while navmesh builds keep every worker busy, with the slowest of those steps, AABB and k-d tree queries, and batches of physics ray casts on one thread and on the job system, and prints the average time of each. `--suite procedural` builds and runs `haylen-procedural-benchmark`, which times wave function collapse, Poisson disk sampling with a fixed and a varying distance, random scattering, marching squares, Delaunay triangulation of 100 thousand points and carving destructible terrain, and prints the average time of each. `--suite lua` builds `haylen-lua-benchmark` and runs the Lua bunnymark of `engine/bench/lua-benchmark` on the headless host, which prints how long Lua takes to update and draw 10 thousand, 100 thousand and a million sprites kept in tables, in a float buffer and in a sprite batch, as the [performance section of the Lua guide](lua.md#performance) explains.

### sdk

```sh
python3 make.py sdk
python3 make.py sdk --platform web --output dist/haylen-web
```

Configures `engine/` on its own in `build/sdk-build-<platform>-<config>` with `HAYLEN_BUILD_SDK=ON`, builds the `haylen_sdk` target and installs the `haylen_sdk` component to `build/sdk/haylen-<platform>-<config>` or to `--output`. `--platform` accepts `macos`, `linux`, `windows`, `web` and `web-webgl2`, and `--config` defaults to `Release`. The [embedding guide](embedding.md) explains what the SDK contains and how a project finds it.

### embedding

```sh
python3 make.py embedding --mode cpm
```

Builds `samples/cpp/embedding`, a CMake project of its own that adds the engine the way another repository would, into `build/embedding-<mode>-<config>`. `--mode` is `subdirectory` (default), `cpm` or `package`. The `package` mode builds the host SDK first and points `CMAKE_PREFIX_PATH` at it. `--config` defaults to `Debug`.

### assets, map, package and clean

```sh
python3 make.py assets ~/Downloads/"Tiny Swords (Free Pack).zip"
python3 make.py map
python3 make.py package games/tiny-island -o build/tiny-island.zip
python3 make.py clean
```

- `assets <archive>` imports the Tiny Swords pack, as described in [Tiny Island assets and map](#tiny-island-assets-and-map).
- `map` generates the Tiny Island map.
- `package <app> [-o <output>]` zips the `app.json`, `source/` and `content/` of an app folder or sample, and nothing else in it, into `app.zip` or the file `-o` names. It fails when the folder has no `app.json`, and it skips `.DS_Store` files.
- `clean` removes `build/`, including the engine artifacts and the assembled apps.

## CMake options

`make.py` sets these options for you, and projects that add the engine themselves set them directly.

| Option | Default | Meaning |
| --- | --- | --- |
| `HAYLEN_SOKOL_SHDC` | empty | Path to `sokol-shdc`. Configuring fails without it. |
| `HAYLEN_RENDER_BACKEND` | `AUTO` | `AUTO`, `METAL`, `D3D11`, `GLCORE`, `GLES3` or `WGPU`. `AUTO` picks Metal on Apple platforms, Direct3D 11 on Windows, GLES3 on Android, WebGPU on the web and OpenGL core on Linux. |
| `HAYLEN_BUILD_PLAYER` | on when the engine is the top-level project | Build the `haylen` player: the executable on desktop and web, and the shared library of `HaylenActivity` on Android. |
| `HAYLEN_BUILD_TESTS` | on when the engine is the top-level project | Build the tests on desktop. |
| `HAYLEN_BUILD_BENCHMARKS` | on when the engine is the top-level project | Build the sprite benchmark on desktop. |
| `HAYLEN_BUILD_SDK` | off | Merge the engine into the SDK that `cmake --install --component haylen_sdk` installs. |
| `HAYLEN_BUILD_FRAMEWORK` | off | Apple only, with `HAYLEN_BUILD_SDK`: merge the SDK and the player into the `libhaylen.a` of one `Haylen.xcframework` slice, installed by the `haylen_framework` component. |
| `HAYLEN_ENABLE_COVERAGE` | off | Instrument the engine and tests with LLVM coverage. Requires Clang. |
| `HAYLEN_SANITIZERS` | `OFF` | Sanitizers of desktop builds that do not use MSVC: `OFF`, `ADDRESS` for AddressSanitizer and UndefinedBehaviorSanitizer on the engine targets, or `THREAD` for ThreadSanitizer on every C and C++ target of the build, dependencies included. |

The workspace `CMakeLists.txt` at the repository root adds `engine/` and turns the player, the tests and the benchmark on. Shaders in `engine/shaders` are compiled by `sokol-shdc` at build time for GLSL 4.30, GLSL 3.00 ES, HLSL 5, Metal for macOS, iOS and the simulator and WGSL, and the runtime picks the variant of the active backend. The programs that draw into lit canvases compile a second time with `HAYLEN_LIT`, and every compile runs from `engine/shaders/include`, the shader library that app shaders include too.

## Dependencies

Dependencies are declared with [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) in `engine/cmake/haylen-dependencies.cmake`. `engine/cmake/cpm.cmake` downloads CPM 0.43.2 and checks its SHA-256 hash. Sources are cached in `.cache/cpm` under the top-level source folder unless `CPM_SOURCE_CACHE` is set as a CMake variable or an environment variable, so several build trees share one download.

| Package | Version | Notes |
| --- | --- | --- |
| nlohmann/json | 3.12.0 | Added first, so Varn reuses it. |
| libuv | 1.53.0 | Added before Varn on every platform except the web, so Varn reuses this release. |
| Varn | commit `ed5bba7` | Lua runtime, event loop, worker pools and the `async`, `http`, `socket`, `json`, `fs`, `zip`, `crypto` and other modules. `VARN_TARGET` is `cli` on desktop and Apple platforms, `android` on Android and `wasm` on the web, and iOS and tvOS use its Apple HTTP client driver. zlib, libzip, Poco, OpenSSL and Lua come in through Varn. Varn builds OpenSSL with its own `make` as one step of the build, which the engine keeps to a single job with `OPENSSL_ENABLE_PARALLEL` off, so a build never runs more jobs than it was given. |
| Sokol | commit `2e75443` | Headers only. The runtime compiles the implementation for the chosen backend. |
| stb | commit `2c980bb` | Headers only. |
| msdfgen | v1.13 | Only its core, which builds the distance fields of font glyphs from their whole outlines. |
| HarfBuzz | 14.5.0 | Old MIT license. The amalgamated source compiles as `haylen_harfbuzz` with `HB_MINI`, the OpenType shaper without the AAT and legacy shapers. |
| SheenBidi | v3.0.0 | Apache 2.0. The Unicode Bidirectional Algorithm and script runs, compiled from its unity source as `haylen_sheenbidi`. |
| libunibreak | 8.0 | zlib license. Line breaking and grapheme clusters of Unicode 17, compiled as `haylen_unibreak`. |
| BudouX | v0.9.3 | Apache 2.0. Only its Thai phrase model, a JSON file the engine embeds for breaking Thai lines. |
| fast_float | v8.3.0 | Headers only. Parses floating point numbers on every platform, where `std::from_chars` for floating point needs iOS, tvOS and macOS 26. |
| Dear ImGui | v1.92.9b | Compiled as `haylen_imgui` with the engine's ImGui configuration. |
| miniaudio | 0.11.25 | Compiled as `haylen_miniaudio`. |
| Box2D | v3.1.1 | |
| zstd | v1.5.7 | Static library for compressed Tiled layers. |
| GoogleTest | v1.18.0 | Only when desktop tests are built. |

Every package is pinned by the URL of a release archive, or of a commit archive for projects without releases, together with its hash:

```cmake
CPMAddPackage(
  NAME box2d
  URL "https://github.com/erincatto/box2d/archive/refs/tags/v3.1.1.tar.gz"
  URL_HASH SHA256=fb6ef914b50f4312d7d921a600eabc12318bb3c55a0b8c0b90608fa4488ef2e4
  OPTIONS "BOX2D_SAMPLES OFF" "BOX2D_BENCHMARKS OFF" "BOX2D_DOCS OFF" "BOX2D_UNIT_TESTS OFF" "BOX2D_VALIDATE OFF"
  SYSTEM YES
)
```

To add or update a package, point `URL` at the latest release archive (Varn is pinned by the archive of one commit, whose contents never change), compute the hash of that archive (for example with `curl -L <url> | shasum -a 256`), and adopt the new API everywhere the engine uses it. A package that Varn also uses goes before the Varn block, so it is resolved once for the whole build. Header-only sources use `DOWNLOAD_ONLY YES` and an interface library, like `haylen_sokol_headers` and `haylen_stb`.

## C++ apps

An app package is a folder with `app.json`, the Lua modules under `source/` and the assets under `content/`, as the [Lua guide](lua.md) describes. Lua apps run from the prebuilt engine through the platform templates, as the [distribution guide](distribution.md) describes. C++ apps compile the engine with their own CMake project, and `haylen_add_app` in `engine/cmake/haylen-app.cmake` turns their package into an app for the platform being built, deploying it into the app of each platform. Only `app.json`, `source/` and `content/` are deployed, so the platform projects, notes and build files of an app can share its folder.

```cmake
haylen_add_app(<target> PACKAGE <folder> [SOURCES <files>...] [CPP] [APPLE_PROJECT <folder>] [WEB_SHELL <file>])
```

| Argument | Meaning |
| --- | --- |
| `PACKAGE` | Package folder. Its `app.json` must set `name`, `identifier` and `version`, which become the display name, bundle identifier and version of the app. Configuring fails when one is missing. |
| `SOURCES` | Extra sources compiled into the app, such as native platform handlers. |
| `CPP` | The app is written in C++, and `SOURCES` define `haylen::core::Application::create`. Without it the app runs `source/main.lua`. |
| `APPLE_PROJECT` | Folder with `mac/Info.plist.in`, `ios/Info.plist.in`, `ios/LaunchScreen.storyboard`, `tvos/Info.plist.in` and `tvos/LaunchScreen.storyboard`. Defaults to `engine/platform/apple`. |
| `WEB_SHELL` | HTML shell of the web page. Defaults to `engine/platform/web/shell.html`. |

The C++ sample `samples/cpp/embedding` is such a project, and `python3 make.py run-cpp cpp/embedding` builds and runs it:

```cmake
haylen_add_app(embedding CPP
  SOURCES src/EmbeddingApp.cpp
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

How each platform carries the package, and where the runtime opens it:

| Platform | Deployment | Opened from |
| --- | --- | --- |
| Windows, Linux | The `SYNC_PACKAGE-<target>` target creates `bin/<target>/app` after every build and links its `app.json`, `source` and `content` to the package folder, with directory junctions and a hard link on Windows and symbolic links elsewhere. Edited files show up without a rebuild. | `app/` next to the executable, or `app.zip` next to it. |
| macOS, iOS, tvOS | `app.json` and every file under `source/` and `content/` become bundle resources under `Resources/app/<subfolder>` through `MACOSX_PACKAGE_LOCATION`, except `.DS_Store`. The file list is globbed with `CONFIGURE_DEPENDS`, so new files are picked up by the next build. The app enters the runtime through `engine/src/platform/apple/AppleMain.cpp`, which calls `haylen_main`. | `Resources/app` of the bundle, or `Resources/app.zip`. |
| Web | `--preload-file` options pack `app.json`, `source/` and `content/` into `<target>.data` under `/app`. | `/app` in the virtual file system, unless the page hands over another package. |
| Android | `haylen_add_app` builds the shared library `bin/<target>/lib<target>.so` and writes the absolute path of the package folder to `bin/<target>/package.txt`. The Gradle project of the app packages both: `make.py run-cpp --platform android` assembles it from the [Android template](distribution.md#android), with the library in `jniLibs`, `HaylenActivity` loading it and the package under `assets/app` with `haylen-package-index.json`. | The `app/` folder of the APK assets. |

Every runtime app, not only the player, runs the package named by the first command-line argument that is not an option instead of the bundled one, and `--dev` turns on the development mode described in the next section.

To ship a Windows or Linux app, copy `app.json`, `source/` and `content/` into an `app` folder next to the executable, or zip them with `make.py package` and ship the zip as `app.zip`.

## The desktop player

The `haylen` player is the runtime without a bundled app. It runs any package given on the command line:

```sh
build/macos-debug/bin/haylen/haylen --dev samples/games/tiny-island
build/macos-debug/bin/haylen/haylen tiny-island.zip
```

`--dev` turns on development mode, which `make.py run` passes. When the package is a folder, the player then watches its `app.json`, `source/` and `content/`: a changed texture updates in place, other changed assets leave the cache so the next load reads them again, and a changed file under `source/` or a changed `app.json` restarts the app, even from the error screen. Without `--dev` the player runs the package like a shipped app. A package that fails to load keeps the window open and shows the error. `make.py engine --platform desktop` copies the player to `build/artifacts/desktop/<os>-<arch>/`.

## Web builds

Web builds are single-threaded like Varn, so they need no `SharedArrayBuffer` and no COOP or COEP headers, although `make.py serve` sends them so pages that use threads work too. Audio reaches the browser through an `AudioWorkletNode` that the page feeds over its message port, as the [audio guide](audio.md#sessions-and-interruptions) describes. Pages play sound only when they are served over https or from localhost, where browsers offer `AudioWorklet`, which `make.py serve`, `make.py run --platform web` and `make.py run-cpp --platform web` do on `127.0.0.1` by default. Apps run on any other page too, without sound, as the [audio guide](audio.md#without-an-audio-output) explains. The link options come from `haylen_link_runtime_platform`: memory growth, a 1 MB stack, IDBFS for user data, exception support because Varn compiles Lua as C++, the `emdawnwebgpu` port for WebGPU, and WebGL 2 only for the `web-webgl2` platform.

A single web target produces `<target>.html`, `<target>.js`, `<target>.wasm` and, for apps, `<target>.data` in `bin/<target>/`. A post-build step of `haylen_setup_web_page` also copies the target's shell, the `WEB_SHELL` of `haylen_add_app` or `engine/platform/web/shell.html` by default, next to them as `<target>.shell.html`, together with `haylen-logo.svg`, the engine logo that the default shell shows as the icon of the page, and `haylen-audio-worklet.js`, the AudioWorklet processor of the audio output, which the runtime loads from the folder of its script through `locateFile`. The prebuilt player of `make.py engine --platform web` is the `haylen` target of the `web` and `web-webgl2` trees, whose page is the [web template](distribution.md#web-loader). `make.py run-cpp --platform web` builds a C++ app target for both backends and writes this layout:

```text
build/cpp/embedding-<hash>/web/
  index.html              The target's shell, with engine/platform/web/backend-picker.html in place of {{{ SCRIPT }}}.
  haylen-logo.svg         The icon of the default shell.
  webgpu/                 embedding.js, embedding.wasm and embedding.data built for WebGPU, next to haylen-audio-worklet.js.
  webgl2/                 The same files built for WebGL2.
```

The backend picker runs the WebGPU build when `navigator.gpu` returns an adapter and the WebGL2 build otherwise, by pointing `Module.locateFile` at the chosen folder and loading `<backend>/<target>.js`. `?backend=webgpu` or `?backend=webgl2` forces one of them, and `?package=<url>` names a zipped package to download. Because the page is the target's own shell, everything a custom `WEB_SHELL` adds, such as page handlers for the [platform bridge](platform_bridge.md#web), works in the bundle as well. `run-cpp` fails when the shell has no `{{{ SCRIPT }}}` placeholder.

### Page setup

A page defines `Module` before it loads the runtime script. `templates/platform/web/loader.js` is the reference for pages of Lua apps and `engine/platform/web/shell.html` for C++ apps:

| Field | Meaning |
| --- | --- |
| `Module.canvas` | The canvas the app draws into. It must be a canvas element with an `id`. The runtime follows its size with a `ResizeObserver`, so the page layout decides the size. |
| `Module.haylen.packageData` | Optional bytes of a zipped package, as an `ArrayBuffer` or a `Uint8Array`, which the runtime plays instead of the bundled package. The web loader downloads `app.zip` with a progress bar and hands it over this way. |
| `Module.haylen.packageUrl` | Optional URL of a zipped package, used when `packageData` is not set. The runtime downloads it before the app starts and runs it instead of the bundled package, and a failed download reaches `onError`. |
| `Module.wasmBinary` | Optional bytes of `haylen.wasm` that the page already downloaded, which Emscripten instantiates instead of fetching the file. |
| `Module.preRun` | Functions that run before the app starts. Page handlers of the platform bridge are registered here. |

A page never turns on development mode. A custom shell passed as `WEB_SHELL` keeps the `{{{ SCRIPT }}}` placeholder where Emscripten inserts the runtime script, and where `run-cpp` inserts the backend picker. The runtime mounts IndexedDB at `/persistent` for user data and loads it before the app starts.

### Runtime API

`engine/platform/web/haylen-runtime.js` is linked with `--pre-js` and fills `Module.haylen` with the API a page or a browser editor uses. None of these calls reloads the page.

| Function | Meaning |
| --- | --- |
| `loadZip(bytes)` | Replaces the running app with a zipped package given as a `Uint8Array` or an `ArrayBuffer`. A file that is not a valid zip shows the error screen and reaches `onError`, like any package that fails to load. |
| `clearFiles()` | Empties the in-memory editor package. |
| `setFile(path, content)` | Adds or replaces a file of the editor package. `path` is relative to the package root, such as `source/main.lua` or `content/ui/logo.png`, and `content` is a string or bytes. An absolute path or one that leaves the package throws an `Error` with the reason. |
| `removeFile(path)` | Removes a file of the editor package and returns whether it existed. An invalid path throws an `Error`. |
| `run()` | Starts the editor package from `source/main.lua`, replacing the running app. |
| `restart()` | Starts the last playing app again from its package, which reloads every script. |
| `stop()` | Ends the app and leaves an empty canvas until the next start. |
| `setPaused(paused)`, `pause()`, `resume()` | Pauses or resumes the app. A paused app keeps its last frame and sees the pause as a suspend. |
| `paused()` | Returns whether the app is paused. |
| `reloadAsset(path)` | Reloads the cached assets read from a changed file, given relative to `content/`, and returns whether any were loaded. A file that no longer decodes throws an `Error` with the reason, and the app keeps the assets it had. |
| `register(method, handler)`, `unregister(method)`, `emit(event, payload, options)` | Platform bridge handlers and events, retained for the first listener with `options.retain`, described in the [platform bridge guide](platform_bridge.md#web). |
| `createPluginContext(id, config)` | The context of the web module of a plugin, described in the [plugin guide](plugins.md#web-modules). |

The runtime reports back through callbacks the page assigns to `Module.haylen`:

| Callback | Arguments |
| --- | --- |
| `onLog(level, message)` | `level` is `'debug'`, `'info'`, `'warning'` or `'error'`. |
| `onError(error)` | `{message, file, line, traceback, frames}` of the error that stopped the app, with an empty `file` when no script position is known. `traceback` is the stack as text, one frame per line, and `frames` lists the same frames as `{source, line, function, kind}` objects, innermost first, with the kind `lua`, `c` or `main`. |
| `onStarted(app)` | `{name, identifier, version}` from `app.json`. |
| `onStopped()` | The app ended. |
| `onStats(stats)` | About once per second: `fps`, `frameMilliseconds`, `averageMilliseconds`, `drawCalls`, `sprites`, `vertices`, `textureSwitches`, `uploadedBytes`, `assets`, `voices`, `scopes`, a list of `{name, milliseconds, calls, depth}` profiler scopes, and `audio`, the output of the app as `{available, state, sampleRate, bufferedMilliseconds, blocks, underruns}`, or only `{available}` while the app runs without an audio output. `available` tells whether the app plays sound, `state` is the state of its `AudioContext`, `bufferedMilliseconds` the mixed audio waiting ahead of the output, `blocks` the blocks mixed so far and `underruns` the render quanta the processor played without samples. |

Callbacks run in a microtask right after the frame that raised them, so they may call back into the runtime, even to restart the app.

```js
Module.haylen.onError = (error) => console.error(error.message + "\n" + error.traceback);
Module.haylen.onStarted = (app) => console.log("Running " + app.name);

async function play(files) {
    Module.haylen.clearFiles();
    for (const [path, content] of Object.entries(files)) {
        Module.haylen.setFile(path, content);
    }
    Module.haylen.run();
}
```

## Android

`python3 make.py build --platform android` builds the engine and the player library `libhaylen.so` for arm64-v8a with the NDK and Ninja in `build/android-<config>`, which is the quick way to compile the engine for Android. Apps come from the Android library module and the Android template:

- `engine/platform/android` is the Gradle project of the `haylen` Android library (namespace `dev.haylen`). It holds `HaylenActivity`, `HaylenBridge`, the splash themes, the R8 rules for the classes C++ calls by name, the `INTERNET` and `VIBRATE` permissions, the OpenGL ES 3 requirement and `android:enableOnBackInvokedCallback`, which the back callback of `HaylenActivity` needs, and it packages the `libhaylen.so` of arm64-v8a, armeabi-v7a and x86_64 from `-PhaylenNativeLibraries`, which `make.py` builds with the engine CMake project. Its `copyVarnTransport` task copies Varn's `VarnHttp.kt` from `-PhaylenVarnSourceDir` into the module, because the engine links Varn into the player library and only the Kotlin HTTP transport of Varn's Android library runs in the app. `make.py engine --platform android` passes the libraries, that folder and the Maven repository as `-PhaylenMavenDir`, and publishes the library there.
- `templates/platform/android` is the Gradle project of an app, without C++, which depends on `dev.haylen:haylen` from that repository. `make.py run --platform android` assembles it with the package of an app and installs it, as the [distribution guide](distribution.md#android) describes.

Gradle finds the Android SDK through `ANDROID_HOME` or a `local.properties` file, as in any Android project. Release builds of apps are minified with R8 and signed with the debug key.

## iOS, tvOS and Mac Catalyst

`python3 make.py configure --platform ios` or `--platform tvos` generates an Xcode project of the engine at `build/<platform>-<config>/haylen_workspace.xcodeproj`, and `make.py build` compiles it through `cmake --build`. Apps for iOS, iPadOS, Mac Catalyst, tvOS and macOS come from the XcodeGen project in `templates/platform/apple`, which links the prebuilt `Haylen.xcframework`, as the [distribution guide](distribution.md#apple) describes.

C++ apps built with `haylen_add_app` target iOS 16.3 on iPhone and iPad and tvOS 16.3, with automatic code signing, because the engine needs the C++ library of those releases, and macOS apps macOS 13.3. The bundle name, identifier and version come from `name`, `identifier` and `version` in the `app.json` of the package. The templates in `engine/platform/apple` hold the `Info.plist` files and launch screens. The iOS template lists the supported orientations through `@HAYLEN_ORIENTATIONS@` and `@HAYLEN_ORIENTATIONS_IPAD@`, which `haylen_add_app` fills from `orientation` in `app.json`: `landscape` allows both landscape sides, `portrait` allows portrait (and upside down on iPad) and `any` allows all of them. The macOS template ends its dictionary with `@HAYLEN_UI_ELEMENT@`, which `haylen_add_app` fills with `LSUIElement` when `window.showInTaskbar` is `false` in `app.json`, so the app never shows a Dock icon. Pass `APPLE_PROJECT` to `haylen_add_app` to use templates of your own with the same layout and placeholders. macOS apps use the same mechanism with `engine/platform/apple/mac/Info.plist.in`.

## Tiny Island assets and map

The Tiny Swords art by Pixel Frog is free to use in games but not to redistribute, so the repository ignores `samples/games/tiny-island/content/tiny_swords/` and `make.py` imports it from the original zip:

```sh
python3 make.py assets ~/Downloads/"Tiny Swords (Free Pack).zip"
```

`assets` runs `tools/import_tiny_swords.py` with `samples/games/tiny-island/content/tiny_swords` as the destination, which it deletes first. The importer:

- copies every PNG of the pack's UI elements, store banners, particle effects, terrain, buildings and units into `ui/`, `ui/store/`, `effects/`, `terrain/`, `buildings/` and `units/`, with every path component in `snake_case` and colored folders such as `Blue Units` shortened to `blue`,
- packs the UI sheets whose nine-slice pieces are spread apart (banners, papers, the wood table, big buttons, bars and ribbons) into tight images under `ui/sliced/`, with the borders of each one in `ui/sliced.json`,
- writes light gray copies of the bar fills as `ui/sliced/big_bar_fill_light.png` and `ui/sliced/small_bar_fill_light.png`, so a UI theme can tint every bar.

```sh
python3 make.py map
```

`map` runs `tools/generate_island_map.py --package samples/games/tiny-island`, which needs the imported art. It writes `content/maps/island.tmj` with the tilesets `terrain.tsj`, `foam.tsj`, `shadow.tsj` and `decorations.tsj`, all as Tiled JSON that opens and edits in Tiled. The island shape comes from seeded noise, so the same seed always gives the same map. `make.py map` uses the default seed, and the tool takes `--seed` when run directly:

```sh
python3 tools/generate_island_map.py --package samples/games/tiny-island --seed 7
```

The [Tiled guide](tiled.md) describes the layers and objects the game reads.

## Continuous integration

`.github/workflows/ci.yml` runs every command a change must pass:

| Job | Runs |
| --- | --- |
| `format` | `make.py format --check` with clang-format 23.1.1. |
| `desktop` | `make.py test --config Debug` and `make.py embedding --mode package --config Release` on macOS, Ubuntu and Windows. |
| `coverage` | `make.py coverage` on macOS, with the HTML report as an artifact. |
| `web` | `make.py engine --platform web`, with the prebuilt WebGPU and WebGL2 player as an artifact. |
| `android` | `make.py engine --platform android`, with the Maven repository of the Android library as an artifact. |
| `apple` | `make.py engine --platform apple` on macOS, with `Haylen.xcframework` as an artifact. |

The jobs cache `.cache/cpm`, keyed by the hash of `engine/cmake/haylen-dependencies.cmake`, and the web job also caches `.tools/emsdk`.
