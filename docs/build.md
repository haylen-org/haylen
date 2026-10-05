# Building Haylen

`haylen.py` at the repository root is the single entry point for building, running, testing and packaging the engine and its apps on every platform. It wraps CMake, downloads the pinned tools it needs into `.tools/`, and keeps one build tree per platform and configuration under `build/`. This guide covers building the engine, what each platform needs, how dependencies are declared, how `haylen_add_app` deploys a package into a C++ app, the web runtime and continuous integration. The [distribution guide](distribution.md) covers the prebuilt engine artifacts, the platform templates and the commands that create, run and package apps, the [embedding guide](embedding.md) covers consuming the engine from another CMake project, and the [testing guide](testing.md) covers the test suite.

## Requirements

Every host needs Python 3.10 or newer, CMake 3.28 or newer and a C++20 compiler. Ninja is needed by every build except iOS, tvOS, macOS with `--xcode`, and Windows builds, which fall back to the default CMake generator when Ninja is not on `PATH`. Run the script with `python3 haylen.py` on macOS and Linux and with `python haylen.py` on Windows.

`haylen.py` downloads the remaining tools the first time a command needs them:

| Tool | Version | Location | Needed by |
| --- | --- | --- | --- |
| `sokol-shdc` | commit `11d0cf6` of `floooh/sokol-tools-bin` | `.tools/sokol-shdc` | Every build. Prebuilt binaries exist for macOS (arm64 and x64), Linux (arm64 and x64) and Windows (x64). |
| Emscripten SDK | 6.0.10 | `.tools/emsdk` | Web builds. It is cloned with `git`. |
| Gradle | 9.8.0 | `.tools/gradle-9.8.0` | The Android library and Android apps. |
| XcodeGen | 2.46.0, the release zip checked against its pinned SHA-256 hash | `.tools/xcodegen` | Apple apps whose plugins add to the Apple project, which haylen.py generates again. macOS only. |

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

`haylen.py` refuses to configure macOS, iOS, tvOS, Linux and Windows builds on any other host. Android and web builds work from every host.

## Build trees

Every platform and configuration gets its own tree at `build/<platform>-<config>`, with the configuration in lowercase, such as `build/macos-debug` or `build/web-webgl2-release`, and builds with sanitizers get trees of their own at `build/<platform>-<config>-<sanitizers>`, such as `build/macos-debug-thread`. Executables and apps land in `bin/<target>/` inside the tree. `python3 haylen.py clean` removes the whole `build/` folder but keeps `.tools/` and the dependency cache.

`build` configures a tree when it has no CMake cache yet or when `--backend` differs from the backend the tree was configured with. Run `clean` before switching the generator with `--xcode`, because CMake cannot change the generator of a tree. The prebuilt engine artifacts that apps use have trees of their own, described in the [distribution guide](distribution.md#engine-artifacts).

## Commands

Every command is about the engine or about an app, plugin or project that the developer names by its folder, relative to the current folder or absolute.

| Command | Purpose |
| --- | --- |
| `tools` | Download the pinned build tools. |
| `configure` | Generate a build tree of the engine. |
| `build` | Configure the engine when needed and build it. |
| `test` | Build and run the engine tests on the host. |
| `engine` | Build the prebuilt engine artifacts that apps use. See the [distribution guide](distribution.md#engine). |
| `new` | Create an app with the starter code and every platform template. See the [distribution guide](distribution.md#new). |
| `plugin` | Add plugins to an app, remove them, list them or create a plugin. See the [distribution guide](distribution.md#plugin). |
| `platform` | Create the project of a platform in an app or compare it with the template. See the [distribution guide](distribution.md#platform). |
| `run` | Run an app in the desktop player with hot reload, or build it from its projects for a platform. See the [distribution guide](distribution.md#run). |
| `prepare` | Write the generated folder of the project of a platform. See the [distribution guide](distribution.md#prepare). |
| `xcodegen` | Generate the `App.xcodeproj` of an app or of the Apple template again. See the [distribution guide](distribution.md#xcodegen). |
| `check` | Check the last build of an app against what the engine and its plugins need. See the [distribution guide](distribution.md#check). |
| `android-key` | Create the upload key or the debug key of the Android project of an app. See the [distribution guide](distribution.md#android-key). |
| `run-cpp` | Build and run a C++ app project. See the [distribution guide](distribution.md#run-cpp). |
| `package` | Zip the package of an app. |
| `shaders` | Compile the shaders of an app. See the [shader guide](shaders.md). |
| `serve` | Serve a folder with the headers WebAssembly pages need. See the [distribution guide](distribution.md#serve). |
| `coverage` | Measure engine code coverage. |
| `format` | Format the C, C++ and Objective-C++ sources. |
| `bench` | Build and run a benchmark of the engine in Release. |
| `sdk` | Build the engine SDK, install it for `find_package(haylen)` and check that other projects consume the engine. |
| `clean` | Remove every build tree, the engine artifacts and the build folders of apps. |

### Output

`haylen.py` prints the title of each step after `==>`, the commands it runs after `$`, then successes, warnings after `Warning:` and errors after `Error:`. An error is one block with the cause and what to do, followed by the output of the tool that failed when there is one, and the command exits with the code 1 without a Python traceback, which only an unexpected failure prints. On a terminal, titles are bold, commands are dim, successes are green, warnings are yellow and errors are red, and reserved expressions, such as commands, options, keys and values, show in cyan without the double quotes that mark them in plain text. Paths and URLs always print bare, underlined on a terminal, so terminals can open them:

```text
==> Preparing the site build/apps/my-game-70da5b67/web
Serving build/apps/my-game-70da5b67/web at http://127.0.0.1:8000/
Press Ctrl+C to stop the server.
```

Color follows the conventions of terminals: the variable `NO_COLOR` with any value turns it off, `FORCE_COLOR` turns it on when the output is no terminal, such as a log of continuous integration, and without either the output has color only on a terminal whose `TERM` is not `dumb`. On Windows, `haylen.py` turns on the escape sequences of the console it writes to.

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
python3 haylen.py tools [--emsdk] [--gradle]
```

Downloads `sokol-shdc`, XcodeGen 2.46.0 on macOS, and Emscripten 6.0.10 or Gradle 9.8.0 when asked, and prints where each one lives. Every other command downloads what it needs by itself, so `tools` is mainly useful to warm up a machine or a CI cache.

### configure

```sh
python3 haylen.py configure --platform ios
```

Generates the build tree with the options above. Every tree receives `-DHAYLEN_SOKOL_SHDC` and `-DCMAKE_BUILD_TYPE`. The platforms add their own settings:

- iOS and tvOS use the Xcode generator with `CMAKE_SYSTEM_NAME` set to `iOS` or `tvOS`, the `arm64` architecture, the 16.3 deployment target, and tests and the player turned off. macOS trees target macOS 14.0. The [distribution guide](distribution.md#haylenxcframework) explains these minimums.
- Android uses the NDK toolchain with `ANDROID_ABI=arm64-v8a`, `ANDROID_PLATFORM=android-27` and the static C++ library, with the tests turned off. The player is the shared library `libhaylen.so`, the one the [Android library](distribution.md#the-android-libraries) packages.
- The web platforms run CMake through `emcmake`, turn the tests off and force `WGPU` for `web` and `GLES3` for `web-webgl2`.

`haylen.py` builds with the pinned NDK, `ndk/30.0.16248370` inside the SDK that `ANDROID_HOME` or `ANDROID_SDK_ROOT` names, even when the environment names another NDK, because the engine and its dependencies build against its headers.

### build

```sh
python3 haylen.py build
python3 haylen.py build --platform web-webgl2 --config Release --target haylen
```

Configures the tree when needed and runs `cmake --build`. Without `--target` it builds everything the tree defines: the engine, the `haylen` player on desktop, web and Android, and the tests and the sprite benchmark on desktop. Useful targets are `haylen`, `haylen_tests` and `haylen-sprite-benchmark`. Apps are not part of the workspace: `haylen.py run` runs Lua apps from their folders and `haylen.py run-cpp` builds C++ app projects, as the [distribution guide](distribution.md) describes.

### test

```sh
python3 haylen.py test
python3 haylen.py test --config Release --sanitizers address
```

Builds `haylen_tests` for the host and runs it through `ctest` with `--output-on-failure` in parallel. See the [testing guide](testing.md).

### coverage

```sh
python3 haylen.py coverage
```

Configures `build/coverage` in Debug with `HAYLEN_ENABLE_COVERAGE=ON` and without samples and the player, builds and runs the tests, merges the profiles with `llvm-profdata`, prints the `llvm-cov report` table and writes an HTML report to `build/coverage/coverage/html/index.html`. The report leaves out dependencies, tests, generated files and the platform backends in `engine/src/platform/{apple,android,web,windows,linux,sokol}`, which need a real device. Coverage uses LLVM source-based coverage, so it needs Clang. On macOS the LLVM tools come from Xcode through `xcrun`, and elsewhere they must be on `PATH`. It accepts `--jobs` and `--sanitizers`.

### format

```sh
python3 haylen.py format
python3 haylen.py format --check
```

Runs `clang-format --style=file` on every `.h`, `.hpp`, `.c`, `.cpp`, `.m` and `.mm` file under `engine/include`, `engine/src`, `engine/tests`, `samples` and `templates`, then lists every multi-line lambda outside a `// clang-format off` and `// clang-format on` region. `--check` rewrites nothing and fails on formatting differences or unguarded lambdas. `clang-format` must be on `PATH`, and CI pins version 23.1.1.

### bench

```sh
python3 haylen.py bench
python3 haylen.py bench --suite algorithms
python3 haylen.py bench --suite procedural
python3 haylen.py bench --suite physics
python3 haylen.py bench --suite ui
python3 haylen.py bench --suite particles
python3 haylen.py bench --suite lua
```

Builds `haylen-sprite-benchmark` in Release for the host and runs it on the local GPU. It accepts `--jobs`. The [rendering guide](rendering.md#sprite-benchmark) describes the phases and the recorded results. `--suite algorithms` builds and runs `haylen-algorithm-benchmark` instead, which times A*, weighted A*, jump point search, flow fields and hierarchical path finding on a 512 by 512 grid, navigation mesh builds and funnel paths, ORCA crowd steps on one thread, on the job system and on the job system while navmesh builds keep every worker busy, with the slowest of those steps, AABB and k-d tree queries, and batches of physics ray casts on one thread and on the job system, and prints the average time of each. `--suite procedural` builds and runs `haylen-procedural-benchmark`, which times wave function collapse, Poisson disk sampling with a fixed and a varying distance, random scattering, marching squares, Delaunay triangulation of 100 thousand points and carving destructible terrain, and prints the average time of each. The suite `physics` builds and runs `haylen-physics-benchmark` on the headless host, which times the step of a pile of falling boxes on one, two, four and eight threads, a settled pile with sleeping on and off, a pyramid on one and four threads and with two, four and eight sub-steps, a chain of 300 links, ragdolls tumbling down stairs, fluids of three sizes with the fluid pass alone and the whole step, ray casts and circle queries in a pile, reading the transforms of 5000 bodies with and without interpolation, the debug drawing of a pile and a pile stepped from Lua with and without its contact and hit callbacks, and prints the average time of each, as the [physics guide](physics.md#the-physics-benchmark) explains. The suite `ui` builds and runs `haylen-ui-benchmark`, which flings collections of a hundred thousand items of three types in a list and in a grid on the headless host and prints the CPU time of a frame with the items bound, the binder calls and the cells created while flinging and at rest, as the [long lists section of the UI guide](ui.md#what-a-long-list-costs) shows. The suite `particles` builds and runs `haylen-particle-benchmark` on the headless host, which times the update of one emitter of 100 thousand particles on one thread and on the job system, a thousand emitters of a hundred particles, spawning 200 thousand particles per second, the draw calls of both kinds of emitter and 500 emitters updated from Lua, and prints the average time of each, as the [particles guide](particles.md#the-particle-benchmark) explains. `--suite lua` builds `haylen-lua-benchmark` and runs the Lua bunnymark of `engine/bench/lua-benchmark` on the headless host, which prints how long Lua takes to update and draw 10 thousand, 100 thousand and a million sprites kept in tables, drawn in one batch or with a call each, in a float buffer and in a sprite batch, as the [performance section of the Lua guide](lua.md#performance) explains.

### sdk

```sh
python3 haylen.py sdk
python3 haylen.py sdk --platform web --output dist/haylen-web
python3 haylen.py sdk --check-consumers
python3 haylen.py sdk --config Release --check-consumers package
```

Configures `engine/` on its own in `build/sdk-build-<platform>-<config>` with `HAYLEN_BUILD_SDK=ON`, builds the `haylen_sdk` target and installs the `haylen_sdk` component to `build/sdk/haylen-<platform>-<config>` or to `--output`. `--platform` accepts `macos`, `linux`, `windows`, `web` and `web-webgl2`, and `--config` defaults to `Release`. The [embedding guide](embedding.md) explains what the SDK contains and how a project finds it.

`--check-consumers` then builds `engine/tests/consumer`, a CMake project of its own with a small C++ app, the way another repository consumes the engine, once for every way it lists: `subdirectory` adds the engine with `add_subdirectory`, `cpm` with `CPMAddPackage` and `package` finds the installed SDK with `find_package`. Without a list it builds all three. Each way builds in `build/consumers/<mode>-<config>` with the dependency cache of the engine, and the option checks the host only, so it takes no other `--platform`.

### package and clean

```sh
python3 haylen.py package ~/apps/my-game -o dist/my-game.zip
python3 haylen.py clean
```

- `package <app> [-o <output>]` zips the `app.json`, `source/` and `content/` of an app folder, and nothing else in it, into `app.zip` or the file `-o` names. It fails when the folder has no `app.json`, and it skips `.DS_Store` files.
- `clean` removes `build/`, including the engine artifacts and the build folders of apps, with the copies of the templates that haylen.py keeps for them.

## CMake options

`haylen.py` sets these options for you, and projects that add the engine themselves set them directly.

| Option | Default | Meaning |
| --- | --- | --- |
| `HAYLEN_SOKOL_SHDC` | empty | Path to `sokol-shdc`. Configuring fails without it. |
| `HAYLEN_RENDER_BACKEND` | `AUTO` | `AUTO`, `METAL`, `D3D11`, `GLCORE`, `GLES3` or `WGPU`. `AUTO` picks Metal on Apple platforms, Direct3D 11 on Windows, GLES3 on Android, WebGPU on the web and OpenGL core on Linux. |
| `HAYLEN_BUILD_PLAYER` | on when the engine is the top-level project | Build the `haylen` player: the executable on desktop and web, and the shared library of `HaylenActivity` on Android. |
| `HAYLEN_BUILD_TESTS` | on when the engine is the top-level project | Build the tests on desktop. |
| `HAYLEN_BUILD_BENCHMARKS` | on when the engine is the top-level project | Build the benchmarks of `haylen.py bench` on desktop: the sprite benchmark and the algorithm, procedural, physics and Lua benchmarks. |
| `HAYLEN_BUILD_SDK` | off | Merge the engine into the SDK that `cmake --install --component haylen_sdk` installs. |
| `HAYLEN_BUILD_FRAMEWORK` | off | Apple only, with `HAYLEN_BUILD_SDK`: merge the SDK and the player into the `libhaylen.a` of one `Haylen.xcframework` slice, installed by the `haylen_framework` component. |
| `HAYLEN_ENABLE_COVERAGE` | off | Instrument the engine and tests with LLVM coverage. Requires Clang. |
| `HAYLEN_SANITIZERS` | `OFF` | Sanitizers of desktop builds that do not use MSVC: `OFF`, `ADDRESS` for AddressSanitizer and UndefinedBehaviorSanitizer on the engine targets, or `THREAD` for ThreadSanitizer on every C and C++ target of the build, dependencies included. |
| `HAYLEN_BUILD_TOOLS` | on at the top level | Desktop hosts only: build the content tool `haylen-content` and its library `haylen_content_tool`, which the tests link too. |
| `HAYLEN_BUILD_FUZZERS` | off | Build `haylen_content_fuzzer`, the libFuzzer target of the parsers of the content formats, and instrument every target for it with AddressSanitizer. Requires a Clang that links libFuzzer, which Apple Clang does not ship. |

The workspace `CMakeLists.txt` at the repository root adds `engine/` and turns the player, the tests and the benchmark on. Shaders in `engine/shaders` are compiled by `sokol-shdc` at build time for GLSL 4.30, GLSL 3.00 ES, HLSL 5, Metal for macOS, iOS and the simulator and WGSL, and the runtime picks the variant of the active backend. The programs that draw into lit canvases compile a second time with `HAYLEN_LIT`, and every compile runs from `engine/shaders/include`, the shader library that app shaders include too.

## Dependencies

Dependencies are declared with [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) in `engine/cmake/haylen-dependencies.cmake`. `engine/cmake/cpm.cmake` downloads CPM 0.43.2 and checks its SHA-256 hash. Sources are cached in `.cache/cpm` under the top-level source folder unless `CPM_SOURCE_CACHE` is set as a CMake variable or an environment variable, so several build trees share one download. A patched package, such as Sokol, keys its place in the cache with `haylen_patched_package_key`, a hash of its archive and of the contents of its patches, so an edited patch downloads and patches the package again in every checkout, and checkouts in other folders share the same package. The patches are configure dependencies of the build tree, so the next build after a patch changes reconfigures and fetches the package under its new key.

| Package | Version | Notes |
| --- | --- | --- |
| nlohmann/json | 3.12.0 | Added first, so Varn reuses it. |
| Varn | commit `0248f79` | Lua runtime, event loop, worker pools and the `async`, `http`, `socket`, `json`, `fs`, `zip`, `crypto` and other modules. `VARN_TARGET` is `cli` on desktop and Apple platforms, `android` on Android and `wasm` on the web. The `cli` target builds the static core that the engine links, and Varn picks the HTTP client driver of each platform itself: the Apple driver on iOS, tvOS and Mac Catalyst, the Android driver on Android and the fetch driver on the web. Lua, libuv, zlib, libzip, Poco and OpenSSL come in through Varn, each pinned with its hash. Varn builds OpenSSL with its own `make` as one step of the build, with the jobs that `CMAKE_BUILD_PARALLEL_LEVEL` names when the tree is configured and one job without it, and `haylen.py` leaves the variable out, so a build never runs more jobs than it was given. |
| Sokol | commit `2e75443` | Headers only. The runtime compiles the implementation for the chosen backend, with the patches in `engine/cmake/patches` that the [distribution guide](distribution.md#notes-on-dependencies) describes. |
| GameActivity | 4.4.2 | Android only, Apache 2.0. The AAR of `androidx.games:games-activity` from Google Maven, pinned by its hash, whose prefab folder holds the static library and the headers of the native side of GameActivity for every ABI, which the runtime links. |
| stb | commit `2c980bb` | Headers only. |
| NanoSVG | commit `239e102` | Headers only. It reads SVG documents into curves and rasterizes vector images, and publishes no releases, so it is pinned to a commit of its default branch. |
| msdfgen | v1.13 | Only its core, which builds the distance fields of font glyphs from their whole outlines. |
| HarfBuzz | 14.5.0 | Old MIT license. The amalgamated source compiles as `haylen_harfbuzz` with `HB_MINI`, the OpenType shaper without the AAT and legacy shapers. |
| SheenBidi | v3.0.0 | Apache 2.0. The Unicode Bidirectional Algorithm and script runs, compiled from its unity source as `haylen_sheenbidi`. |
| libunibreak | 8.0 | zlib license. Line breaking and grapheme clusters of Unicode 17, compiled as `haylen_unibreak`. |
| BudouX | v0.9.3 | Apache 2.0. Only its Thai phrase model, a JSON file the engine embeds for breaking Thai lines. |
| fast_float | v8.3.0 | Headers only. Parses floating point numbers on every platform, where `std::from_chars` for floating point needs iOS, tvOS and macOS 26. |
| Dear ImGui | v1.92.9b | Compiled as `haylen_imgui` with the engine's ImGui configuration. |
| miniaudio | 0.11.25 | Compiled as `haylen_miniaudio`. |
| Box2D | v3.1.1 | |
| zstd | v1.5.7 | Static library for compressed Tiled layers and for the chunks of protected content. |
| Monocypher | 4.0.3 | BSD 2-clause or CC0. The authenticated encryption (XChaCha20-Poly1305), hashing and key derivation (BLAKE2b) and manifest signatures (Ed25519) of protected content, compiled from its two sources as `haylen_monocypher`. It is audited, has no dependencies and builds the same on every target, the web included, and this release fixes a timing leak of its signatures. |
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

The C++ sample `samples/cpp/embedding` is such a project, and `python3 haylen.py run-cpp samples/cpp/embedding` builds and runs it from the root of the repository:

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
| Android | `haylen_add_app` builds the shared library `bin/<target>/lib<target>.so` and writes the absolute path of the package folder to `bin/<target>/package.txt`. The Gradle project of the app packages both: `haylen.py run-cpp --platform android` prepares the [Android project](distribution.md#the-android-project) of the package, with the library in `haylen/jniLibs`, `HaylenActivity` loading it and the package under `haylen/assets/app` with `haylen-package-index.json`. | The `app/` folder of the APK assets. |

Every runtime app, not only the player, runs the package named by the first command-line argument that is not an option instead of the bundled one, and `--dev` turns on the development mode described in the next section.

A Lua app ships on Windows and Linux through the release build of `haylen.py run --config Release`, which links an executable of the app and places its protected release in `app/` next to it, as the [distribution guide](distribution.md#release-builds) describes. The player also runs a package folder in `app/` next to it, or a zip of it as `app.zip`, which development uses.

Windows apps and the `haylen` player embed `engine/platform/windows/haylen.manifest`, the application manifest of the engine, which `haylen_add_app` adds to the sources of every Windows target. It selects version 6 of the Common Controls, where the task dialogs of [haylen.dialogs](lua-api/dialogs.md) live, and UTF-8 as the code page of the process, since the engine keeps every text and path in UTF-8. The SDK installs it with the other platform files. Linux apps load GIO for the theme and GTK 3 for the dialogs at run time where the system has them, so building needs neither.

## The desktop player

The `haylen` player is the runtime without a bundled app. It runs any package given on the command line:

```sh
build/macos-debug/bin/haylen/haylen --dev ~/apps/my-game
build/macos-debug/bin/haylen/haylen my-game.zip
```

`--dev` turns on development mode, which `haylen.py run` passes. When the package is a folder, the player then watches its `app.json`, `source/` and `content/`: a changed texture updates in place, other changed assets leave the cache so the next load reads them again, a changed Lua module reloads in place with the state of the app, and a changed `app.json` or `source/main.lua` restarts the app, even from the error screen or after an `app.json` that failed to load. Hidden and backup files never count, and changes saved while the app restarts reach the next app. Without `--dev` the player runs the package like a shipped app. A package that fails to load keeps the window open and shows the error. `haylen.py engine --platform desktop` builds the player in `build/engine/desktop-<config>/` and copies it to `build/artifacts/desktop/<os>-<arch>/`, next to the content tool `haylen-content`, which builds the protected releases of apps as the [content guide](content.md#the-content-tool) describes, and the SDK of the engine in `sdk/`, which links the release executables of Windows and Linux apps.

## Web builds

Web builds are single-threaded like Varn, so they need no `SharedArrayBuffer` and no cross-origin isolation, and `haylen.py serve` sends a COOP header only when `--coop` asks for one, since `same-origin` cuts the page off from the sign-in and payment popups of plugins, as the [distribution guide](distribution.md#serve) explains. Audio reaches the browser through an `AudioWorkletNode` that the page feeds over its message port, as the [audio guide](audio.md#sessions-and-interruptions) describes. Pages play sound only when they are served over https or from localhost, where browsers offer `AudioWorklet`, which `haylen.py serve`, `haylen.py run --platform web` and `haylen.py run-cpp --platform web` do on `127.0.0.1` by default. Apps run on any other page too, without sound, as the [audio guide](audio.md#without-an-audio-output) explains. The link options come from `haylen_link_runtime_platform`: memory growth, a 1 MB stack, IDBFS for user data, exception support because Varn compiles Lua as C++, the `emdawnwebgpu` port for WebGPU, and WebGL 2 only for the `web-webgl2` platform.

A single web target produces `<target>.html`, `<target>.js`, `<target>.wasm` and, for apps, `<target>.data` in `bin/<target>/`. A post-build step of `haylen_setup_web_page` also copies the target's shell, the `WEB_SHELL` of `haylen_add_app` or `engine/platform/web/shell.html` by default, next to them as `<target>.shell.html`, together with `haylen-logo.svg`, the engine logo that the default shell shows as the icon of the page, and `haylen-audio-worklet.js`, the AudioWorklet processor of the audio output, which the runtime loads from the folder of its script through `locateFile`. The prebuilt player of `haylen.py engine --platform web` is the `haylen` target of the `web` and `web-webgl2` trees, whose page is the [web template](distribution.md#web-loader). `haylen.py run-cpp --platform web` builds a C++ app target for both backends and writes this layout:

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
| `Module.haylen.development` | Optional `true` that runs the app in development, the way the player does with `--dev`: Lua modules and assets that the page changes with `applyChanges()` reload in place, as [hot reload](lua.md#hot-reload) describes. |
| `Module.haylen.developmentServer` | Optional WebSocket address of the development server that `haylen.py run --platform web` starts, such as `ws://127.0.0.1:8000/haylen/development?token=<token>`, which pushes every file saved in the app to the page. |
| `Module.instantiateWasm` | Optional function that receives the imports of the module and a callback, instantiates `haylen.wasm` itself and passes the instance to the callback, so a page that already downloaded the bytes, such as the web loader, keeps Emscripten from fetching the file again. |
| `Module.preRun` | Functions that run before the app starts. Page handlers of the platform bridge are registered here. |

Only a page that sets `Module.haylen.development` runs in development, which the template loader does only for the `development` entry of `config.json` that `haylen.py run --platform web` writes. A custom shell passed as `WEB_SHELL` keeps the `{{{ SCRIPT }}}` placeholder where Emscripten inserts the runtime script, and where `run-cpp` inserts the backend picker. The runtime mounts IndexedDB at `/persistent` for user data and loads it before the app starts, together with what the browser tells about the device through `navigator.userAgentData` and the Battery Status API, which answer asynchronously.

### Runtime API

`engine/platform/web/haylen-runtime.js` is linked with `--pre-js` and fills `Module.haylen` with the API a page or a browser editor uses. None of these calls reloads the page.

| Function | Meaning |
| --- | --- |
| `loadZip(bytes)` | Replaces the running app with a zipped package given as a `Uint8Array` or an `ArrayBuffer`. A file that is not a valid zip shows the error screen and reaches `onError`, like any package that fails to load. |
| `clearFiles()` | Stops the app and empties its package, for an app that the page builds file by file before `run()`. |
| `setFile(path, content)` | Adds or replaces a file of the package that plays. `path` is relative to the package root, such as `source/main.lua` or `content/ui/logo.png`, and `content` is a string or bytes. The app reads the new file the next time it loads it, and `applyChanges()` applies it at once. An absolute path or one that leaves the package throws an `Error` with the reason. |
| `removeFile(path)` | Removes a file of the package that plays and returns whether it existed. An invalid path throws an `Error`. |
| `applyChanges()` | Applies every file that `setFile` and `removeFile` changed since its last call, as one batch at the start of the next frame. A page in development reloads them the way [hot reload](lua.md#hot-reload) applies saved files: assets and Lua modules in place, and `app.json` or `source/main.lua` with a restart. Any other page restarts the app with them. |
| `run()` | Starts the package from `source/main.lua`, replacing the running app. |
| `restart()` | Starts the last playing app again from its package, which reloads every script. |
| `stop()` | Ends the app and leaves an empty canvas until the next start. |
| `setPaused(paused)`, `pause()`, `resume()` | Pauses or resumes the app. A paused app keeps its last frame and sees the pause as a suspend. |
| `paused()` | Returns whether the app is paused. |
| `register(method, handler)`, `unregister(method)`, `emit(event, payload, options)` | Platform bridge handlers and events, retained for the first listener with `options.retain`, described in the [platform bridge guide](platform_bridge.md#web). |
| `createPluginContext(id, config)` | The context of the web module of a plugin, described in the [plugin guide](plugins.md#web-modules). |

The runtime reports back through callbacks the page assigns to `Module.haylen`:

| Callback | Arguments |
| --- | --- |
| `onLog(level, message)` | `level` is `'debug'`, `'info'`, `'warning'` or `'error'`. |
| `onError(error)` | `{message, file, line, traceback, frames}` of the error that stopped the app, with an empty `file` when no script position is known. `traceback` is the stack as text, one frame per line, and `frames` lists the same frames as `{source, line, function, kind}` objects, innermost first, with the kind `lua`, `c` or `main`. |
| `onStarted(app)` | `{name, identifier, version}` from `app.json`. |
| `onStopped()` | The app ended. |
| `onReloaded(report)` | In development, after every batch of changes that hot reload applied: `{restarted: false, mode, modules, assets, resumed, milliseconds}`, with the files of the modules and the assets that reloaded in place and whether the app left the error screen, or `{restarted: true, reason}` right before a restart. |
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

`python3 haylen.py build --platform android` builds the engine and the player library `libhaylen.so` for arm64-v8a with the NDK and Ninja in `build/android-<config>`, which is the quick way to compile the engine for Android. Apps come from the Android library module and the Android template:

- `engine/platform/android` is the Gradle project of the Haylen Android libraries. Its module `haylen` is the `haylen` library (namespace `dev.haylen`), which holds `HaylenActivity`, a GameActivity, `HaylenBridge`, the plugin API, `HaylenRequirements`, `HaylenPluginProvider`, `HaylenLinkActivity`, the AppCompat splash themes and the R8 rules for the classes C++ calls by name. Its manifest declares only the OpenGL ES 3 requirement, with no permission and no component, and it packages the `libhaylen.so` of arm64-v8a, armeabi-v7a and x86_64 from `-PhaylenNativeLibraries`, which `haylen.py` builds with the engine CMake project. Its `copyVarnTransport` task copies Varn's `VarnHttp.kt` from `-PhaylenVarnSourceDir` into the module, because the engine links Varn into the player library and only the Kotlin HTTP transport of Varn's Android library runs in the app. The modules `haylen-plugins` and `haylen-links` hold only a manifest, which declares `HaylenPluginProvider` for plugin modules and the exported `HaylenLinkActivity` for plugins of links and notifications, and `haylen-coroutines` holds `HaylenCoroutines` with its dependency on `kotlinx-coroutines-android`. Each of them depends on `haylen` as an API. `haylen.py engine --platform android` passes the libraries, that folder and the Maven repository as `-PhaylenMavenDir`, and the root `build.gradle.kts` publishes every module there as `dev.haylen:<module>` at the engine version.
- The Java classes of GameActivity must match its native side in `libhaylen.so`, so the library depends on `androidx.games:games-activity` 4.4.2, strictly, the version that `engine/cmake/haylen-dependencies.cmake` pins, and on the AndroidX libraries that GameActivity leaves to apps, AppCompat 1.8.0, the activity library 1.13.0 and core 1.19.1, as APIs. A version change updates the pin, the library and the template together.
- `templates/platform/android` is the Gradle project of an app, without C++, which depends on `dev.haylen:haylen` from that repository and gets GameActivity, AppCompat, the activity library and core from it. Its manifest declares the permissions `INTERNET`, `ACCESS_NETWORK_STATE` and `VIBRATE` of the network, the network events and vibration, which an app deletes when it does not use them, and `android:enableOnBackInvokedCallback`, which lets the back callback of `HaylenActivity` play the predictive back animation of the system. `haylen.py run --platform android` writes the package and the settings of an app into its folder `haylen/`, builds it where it is and installs it, as the [distribution guide](distribution.md#the-android-project) describes.

Gradle finds the Android SDK through `ANDROID_HOME` or a `local.properties` file, as in any Android project. Release builds of apps are minified with R8 and signed with the upload key of the app, which `haylen.py android-key` creates, as the [distribution guide](distribution.md#android-key) describes.

## iOS, tvOS and Mac Catalyst

`python3 haylen.py configure --platform ios` or `--platform tvos` generates an Xcode project of the engine at `build/<platform>-<config>/haylen_workspace.xcodeproj`, and `haylen.py build` compiles it through `cmake --build`. Apps for iOS, iPadOS, Mac Catalyst, tvOS and macOS come from the XcodeGen project of the app, which starts from `templates/platform/apple` and links the prebuilt `Haylen.xcframework` through the target templates of its generated folder, as the [distribution guide](distribution.md#the-apple-project) describes.

C++ apps built with `haylen_add_app` target iOS 16.3 on iPhone and iPad and tvOS 16.3, with automatic code signing, because the engine needs the C++ library of those releases, and macOS apps macOS 14.0, the first release with the display link of `NSView` that `sokol_app` draws with. The bundle name, identifier and version come from `name`, `identifier` and `version` in the `app.json` of the package. The templates in `engine/platform/apple` hold the `Info.plist` files, the launch screens and the privacy manifest of the engine, which every bundle carries among its resources. The iOS template lists the supported orientations through `@HAYLEN_ORIENTATIONS@` and `@HAYLEN_ORIENTATIONS_IPAD@`, which `haylen_add_app` fills from `orientation` in `app.json`: `landscape` allows both landscape sides, `portrait` allows portrait (and upside down on iPad) and `any` allows all of them. The macOS template ends its dictionary with `@HAYLEN_UI_ELEMENT@`, which `haylen_add_app` fills with `LSUIElement` when `window.showInTaskbar` is `false` in `app.json`, so the app never shows a Dock icon. Pass `APPLE_PROJECT` to `haylen_add_app` to use templates of your own with the same layout and placeholders. macOS apps use the same mechanism with `engine/platform/apple/mac/Info.plist.in`.

## Continuous integration

`.github/workflows/ci.yml` runs every command a change must pass, on every push to a branch and every pull request. It only tests: no job publishes artifacts or release assets, and tags and releases start no run. A GitHub release of the repository is its source archive alone, since every project compiles the engine from the sources, or builds the prebuilt engine of its apps with `haylen.py engine` on its own machine.

| Job | Runs |
| --- | --- |
| `format` | `haylen.py format --check` with clang-format 23.1.1, and the tests of `tools/test_haylen.py`. |
| `desktop` | `haylen.py test --config Debug` and `haylen.py sdk --config Release --check-consumers package`, which builds the SDK and a project that finds it with `find_package`, on macOS, Ubuntu and Windows. |
| `coverage` | `haylen.py coverage` on macOS, whose report table shows in the log. |
| `web` | `haylen.py engine --platform web`, which builds the prebuilt WebGPU and WebGL2 player. |
| `android` | `haylen.py engine --platform android`, which builds the Android libraries into the local Maven repository. |
| `apple` | `haylen.py engine --platform apple` on macOS, which builds `Haylen.xcframework`. |

The jobs cache `.cache/cpm`, keyed by the hash of `engine/cmake/haylen-dependencies.cmake`, and the web job also caches `.tools/emsdk`.
