# 🔨 Building Haylen

This guide is for people who build the engine itself. It explains the command `haylen`, what each platform needs, the tools it downloads, the build trees, the engine commands, the CMake options, the dependencies with their patches, the desktop player and the web runtime. Other guides cover the rest of the command: the [distribution guide](distribution.md) covers the commands of apps, the [testing guide](testing.md) covers tests and continuous integration, and the [embedding guide](embedding.md) covers projects that add the engine with CMake.

## The command haylen

The script `haylen.py` at the root of the repository is the one entry point for every task of the engine and its apps. It wraps CMake and the tools of each platform. The file `haylen` runs it on macOS and Linux, and `haylen.cmd` runs it on Windows. Both pass every argument on, from any folder and through a link. Put the root of the repository on `PATH`, and every command below is `haylen <command>`. The call `python3 haylen.py <command>` does the same.

```sh
export PATH="$HOME/path/to/haylen:$PATH"
haylen --version
haylen run --help
```

The option `--version` prints `Haylen` and the version that `engine/VERSION` holds. The option `--help`, after `haylen` or after a command, lists the commands or the options of one command. Every command works on the engine, or on an app, a plugin or a project that you name by its folder. The folder may be relative to the current folder or absolute.

| Command | What it does | Guide |
| --- | --- | --- |
| `tools` | Downloads the pinned build tools. | [tools](#tools) |
| `configure` | Generates a build tree of the engine. | [configure](#configure) |
| `build` | Configures the engine when needed and builds it. | [build](#build) |
| `bench` | Builds and runs a benchmark of the engine. | [bench](#bench) |
| `clean` | Removes every build tree, the engine artifacts and the build folders of apps. | [clean](#clean) |
| `test` | Builds and runs the engine tests, or the memory check of the web. | [Testing](testing.md#running-the-tests) |
| `coverage` | Measures the code coverage of the engine. | [Testing](testing.md#coverage) |
| `fuzz` | Runs the fuzzers of the parsers that read untrusted bytes. | [Testing](testing.md#fuzzing) |
| `format` | Formats the C, C++ and Objective-C sources. | [Testing](testing.md#formatting) |
| `sdk` | Builds and installs the engine SDK, and checks that other projects consume the engine. | [Embedding](embedding.md#the-sdk-command) |
| `engine` | Builds the prebuilt engine artifacts that apps use. | [Distribution](distribution.md#engine) |
| `new` | Creates an app. | [Distribution](distribution.md#new) |
| `run` | Runs an app in the desktop player, or builds and launches it for a platform. | [Distribution](distribution.md#run) |
| `run-cpp` | Builds and runs a C++ app project. | [Distribution](distribution.md#run-cpp) |
| `prepare` | Writes the generated folder of the project of a platform. | [Distribution](distribution.md#prepare) |
| `xcodegen` | Generates the `App.xcodeproj` of an app or of the Apple template again. | [Distribution](distribution.md#xcodegen) |
| `check` | Checks a built app against what the engine and its plugins need. | [Distribution](distribution.md#check) |
| `platform` | Creates the project of a platform in an app, or compares it with the template. | [Distribution](distribution.md#platform) |
| `plugin` | Adds, removes, lists or creates plugins. | [Distribution](distribution.md#plugin) |
| `package` | Zips the package of an app. | [Distribution](distribution.md#package) |
| `shaders` | Compiles the shaders of an app. | [Distribution](distribution.md#shaders) |
| `serve` | Serves a folder with the headers that WebAssembly pages need. | [Distribution](distribution.md#serve) |
| `android-key` | Creates the signing key of the Android project of an app. | [Distribution](distribution.md#android-key) |
| `dist` | Builds the installer of a Windows app, or the packages of a Linux app. | [Distribution](distribution.md#dist) |
| `steam` | Builds a desktop release and stages its Steam depots. | [Distribution](distribution.md#steam) |
| `content` | Builds, checks and publishes the protected releases of an app, and manages its keys. | [Content](../content/content.md#the-content-tool) |

### What the commands print

Each step prints a title after `==>`. Successes, warnings after `Warning:` and errors after `Error:` follow. The tools that a command runs, such as CMake, Ninja, `xcodebuild` and Gradle, write their whole output to the log of the command in `build/logs`, such as `build/logs/run.log`. Every run of a command starts its log again. Meanwhile a terminal shows the latest line of the tool in place under the step.

When a tool fails, the command prints its last 60 lines and the path of the log. Add `--verbose`, before or after the command, to see the commands after `$` and the whole output of every tool instead. Some output always prints, because you read it: the log of the app that runs, the results of tests and benchmarks and the coverage report.

An error is one block with the cause and what to do. The output of the tool that failed follows it, when there is one. The command then exits with the code 1, without a Python traceback. Only an unexpected failure prints a traceback. Ctrl+C stops a command with the code 130.

```text
==> Preparing the site build/apps/my-game-70da5b67/web
Serving build/apps/my-game-70da5b67/web at http://127.0.0.1:8000/
Press Ctrl+C to stop the server.
```

On a terminal, titles are bold, commands are dim, successes are green, warnings are yellow and errors are red. Reserved expressions, such as commands, options, keys and values, show in cyan without the double quotes that mark them in plain text. Paths and URLs always print bare, and underlined on a terminal, so the terminal can open them. Color follows the usual rules of terminals:

- The variable `NO_COLOR` with any value turns color off.
- The variable `FORCE_COLOR` turns it on when the output is not a terminal, such as a log of continuous integration.
- Without either, the output has color only on a terminal whose `TERM` is not `dumb`.
- On Windows, `haylen.py` turns on the escape sequences of the console it writes to.

## Requirements

Every machine needs Python 3.12 or newer, CMake 3.28 or newer and a C++20 compiler. With an older Python, `haylen.py` stops at once with a message that names the version it needs. Most builds also need Ninja. The exceptions are iOS, tvOS and macOS with `--xcode`, which use Xcode, and Windows, which falls back to the default generator of CMake when Ninja is not on `PATH`.

What each platform needs on top of that:

| Platform | `--platform` | Host | Graphics API | Generator | Also install |
| --- | --- | --- | --- | --- | --- |
| macOS | `macos` | macOS | Metal | Ninja, or Xcode with `--xcode` | The Xcode Command Line Tools, or Xcode for `--xcode`. |
| Windows | `windows` | Windows | Direct3D 11 | Ninja when it is on `PATH`, otherwise the default of CMake (Visual Studio) | Visual Studio with the C++ workload. Run Ninja builds from a developer command prompt, so MSVC is on `PATH`. |
| Linux | `linux` | Linux | OpenGL core | Ninja | GCC or Clang and the development packages of X11, Xi, Xcursor, Xext, Xrandr and OpenGL, such as `libx11-dev libxi-dev libxcursor-dev libxext-dev libxrandr-dev libgl1-mesa-dev` on Debian and Ubuntu. |
| iOS | `ios` | macOS | Metal | Xcode | Xcode. Apps also need XcodeGen to generate the Apple project again. |
| tvOS | `tvos` | macOS | Metal | Xcode | Xcode, and the tvOS simulator runtime (`xcodebuild -downloadPlatform tvOS`) to run apps on it. |
| Android | `android` | any | OpenGL ES 3 | Ninja | The Android SDK with NDK `30.0.16248370`, CMake `4.1.2`, platform 37.2 and platform-tools, and JDK 17 or newer. |
| Web | `web` | any | WebGPU | Ninja through `emcmake` | `git`, and a browser with WebGPU to run it. |
| Web | `web-webgl2` | any | WebGL2 | Ninja through `emcmake` | `git`, and a browser with WebGL2 to run it. |

The script refuses to configure macOS, iOS, tvOS, Linux and Windows builds on any other host. Android and web builds work from every host.

### Tools that haylen.py brings

The script downloads, or builds, the other tools the first time a command needs them. It keeps them in `.tools/` at the root of the repository. Each tool is pinned to one version, and a new pin downloads its own copy. The downloads of the shader compiler, XcodeGen, bundletool, Inno Setup and the AppImage tools are also pinned by their SHA-256 hashes. A download whose hash differs is deleted and stops the command.

| Tool | Version | Folder | Used by |
| --- | --- | --- | --- |
| `haylen-shaderc` | The release that `engine/cmake/haylen-shader-compiler.json` pins | `.tools/haylen-shaderc-<version>`, or `.tools/haylen-shaderc-<commit>` when it is built here | Every build and the command `shaders`, as [the shader compiler](#the-shader-compiler) explains. |
| Emscripten SDK | 6.0.12 | `.tools/emsdk` | Web builds. |
| Gradle | 9.8.1 | `.tools/gradle-9.8.1` | The Android libraries and Android apps. |
| XcodeGen | 2.46.0 | `.tools/xcodegen-2.46.0` | Apple apps whose `App.xcodeproj` haylen.py generates again. macOS only. |
| bundletool | 1.18.3 | `.tools/bundletool-1.18.3.jar` | Release runs on Android of apps whose packs Google Play delivers, which install in the local testing mode of Google Play. |
| Inno Setup | 7.1.0 | `.tools/inno-setup-7.1.0` | The Windows installer of [`dist`](distribution.md#dist), installed in portable mode, without an uninstaller or entries in the registry. Windows only. |
| appimagetool, the AppImage runtime and patchelf | 1.9.1, 20251108 and 0.19.2 | `.tools/appimage-1.9.1` | The AppImage of [`dist`](distribution.md#dist). Linux only. |

Emscripten comes from its repository through `git`. An SDK of another version fetches the newest list of releases and installs the pinned one. Every web build tree records the version of Emscripten that configured it. A tree of another version builds again from nothing, because Emscripten keeps no binary compatibility between versions.

### The shader compiler

The Haylen shader compiler, `haylen-shaderc`, compiles the shaders of the engine and of apps. It lives in its own repository, [haylen-shader-compiler](https://github.com/haylen-org/haylen-shader-compiler). The file `engine/cmake/haylen-shader-compiler.json` pins one release of it and the commit of that release. It lists a prebuilt binary with its SHA-256 hash for each system and processor, and the source archive of the commit with its hash.

The script `engine/cmake/haylen-shader-compiler.cmake` provides the compiler from those pins, in the same way for `haylen.py` and for CMake:

1. It names the machine by its system and processor, such as `macos-arm64` or `windows-x64`.
2. The release ships binaries for macOS on Apple silicon and Intel, Linux on x64 and ARM64 with glibc 2.28 or later, and Windows on x64, and on ARM64 from Windows 11. On those machines the script downloads the binary and checks its hash.
3. On any other machine, it downloads the source of the commit, checks its hash and builds the compiler with CMake on half of the processors.
4. It keeps the compiler in a folder named after the version, or after the first seven characters of the commit when it built it. A new pin therefore brings its own compiler.
5. Builds that configure at the same time wait for the first one, which moves the finished compiler into place.

The script `haylen.py` runs it with `.tools/` as the folder of the compilers and hands the result to every build tree as `HAYLEN_SHADERC`. Set the environment variable `HAYLEN_SHADER_COMPILER` to a build of the compiler, and `haylen.py` uses that one instead. A CMake project that adds the engine runs the same script when `HAYLEN_SHADERC` is empty, as the [CMake options](#cmake-options) describe.

```sh
HAYLEN_SHADER_COMPILER=~/haylen-shader-compiler/build/haylen-shaderc haylen build
```

## Build trees

Every platform and configuration gets its own build tree at `build/<platform>-<config>`, with the configuration in lowercase, such as `build/macos-debug` or `build/web-webgl2-release`. A build with sanitizers gets a tree of its own at `build/<platform>-<config>-<sanitizers>`, such as `build/macos-debug-thread`. Executables and apps land in `bin/<target>/` inside the tree.

The command `build` configures its tree first when one of these is true:

- The tree has no CMake cache yet.
- The option `--graphics-api` asks for another graphics API than the tree has.
- The shader compiler changed.
- Another version of Emscripten configured a web tree.

CMake cannot change the generator of a tree, so run `clean` before you switch to Xcode with `--xcode`. The other folders of `build/` belong to other commands:

| Folder | What it holds |
| --- | --- |
| `build/engine/` | The build trees of the [engine artifacts](distribution.md#engine-artifacts), whose results go to `build/artifacts/`. |
| `build/apps/<app>-<hash>/` | Everything that haylen.py builds for an app, as [platform projects](distribution.md#platform-projects) explain. |
| `build/cpp/<project>-<hash>/` | The build trees of the C++ app projects of [`run-cpp`](distribution.md#run-cpp). |
| `build/sdk/`, `build/sdk-build-<platform>-<config>/`, `build/consumers/` | The SDK, its build tree and the projects that [`sdk --check-consumers`](embedding.md#checking-the-three-ways) builds. |
| `build/coverage/`, `build/fuzz/`, `build/tests/` | The trees of [coverage](testing.md#coverage), [fuzzing](testing.md#fuzzing) and the [memory check of the web](testing.md#the-memory-check-of-the-web). |
| `build/logs/` | The log of each command. |

## Engine commands

### Build options

The commands `configure`, `build` and `test` take these options:

| Option | Default | Meaning |
| --- | --- | --- |
| `--platform` | this machine | `macos`, `linux`, `windows`, `ios`, `tvos`, `android`, `web` or `web-webgl2`. The command `test` always runs on this machine, except that `web` and `web-webgl2` run the memory check of the web. |
| `--config` | `Debug` | `Debug`, `Release` or `RelWithDebInfo`. |
| `--graphics-api` | the default of the platform | `METAL`, `D3D11`, `GLCORE`, `GLES3` or `WGPU`, the graphics API that the backend draws through. It reaches CMake as `HAYLEN_GRAPHICS_API`. The web platforms always use their own graphics API. |
| `--xcode` | off | Uses the Xcode generator on macOS. |
| `--sanitizers` | none | `address` for AddressSanitizer and UndefinedBehaviorSanitizer, or `thread` for ThreadSanitizer, in a tree of its own. It reaches CMake as `HAYLEN_SANITIZERS`, and MSVC builds ignore it. The [testing guide](testing.md#sanitizers) explains both. |
| `--target` | everything | The CMake target that `build` builds. |
| `--jobs` | the processors minus one | The parallel jobs of the build and the tests. |

The runtime links the system libraries of the default graphics API of each platform. On Windows it also links OpenGL when the graphics API is not `D3D11`. So `--graphics-api GLCORE` on Windows is the one desktop override with everything it needs to link.

### tools

```sh
haylen tools
haylen tools --emsdk --gradle
```

Downloads the shader compiler, or builds it where its release has no binary for the machine, and XcodeGen on macOS. The option `--emsdk` also installs Emscripten, and `--gradle` installs Gradle. The command prints where each tool lives. Every other command fetches what it needs by itself, so use `tools` to prepare a machine or the cache of a CI job.

### configure

```sh
haylen configure --platform ios
```

Generates the build tree of a platform with the [build options](#build-options). Every tree receives `HAYLEN_SHADERC`, `CMAKE_BUILD_TYPE`, `HAYLEN_GRAPHICS_API` and `HAYLEN_SANITIZERS`. Each platform adds its own settings:

- macOS targets macOS 14.0 through `CMAKE_OSX_DEPLOYMENT_TARGET`.
- iOS and tvOS use the Xcode generator with `CMAKE_SYSTEM_NAME` set to `iOS` or `tvOS`, the `arm64` architecture and the 16.3 deployment target. The tests and the player stay off. The [distribution guide](distribution.md#haylenxcframework) explains these minimum versions.
- Android uses the NDK toolchain with `ANDROID_ABI=arm64-v8a`, `ANDROID_PLATFORM=android-27`, the static C++ library and `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES`. The tests stay off. The player is the shared library `libhaylen.so`, which the [Android library](distribution.md#the-android-libraries) packages.
- The web platforms run CMake through `emcmake` with the tests off. They force `WGPU` for `web` and `GLES3` for `web-webgl2`.

Android builds always use the pinned NDK, `ndk/30.0.16248370` inside the SDK that `ANDROID_HOME` or `ANDROID_SDK_ROOT` names. This holds even when the environment names another NDK, because the engine and its dependencies build against its headers.

### build

```sh
haylen build
haylen build --platform web-webgl2 --config Release --target haylen
haylen build --target haylen-headless
```

Configures the tree when needed and runs `cmake --build`. Without `--target` it builds everything the tree defines. That is the engine, the `haylen` player on desktop, web and Android, and on desktop also the headless player, the content tool, the tests and the benchmarks. Useful targets are `haylen`, `haylen-headless`, `haylen-tests`, `haylen-native-test` and `haylen-sprite-benchmark`.

Apps are not part of this workspace. The command `run` runs Lua apps from their folders, and `run-cpp` builds C++ app projects, as the [distribution guide](distribution.md) describes.

### bench

```sh
haylen bench
haylen bench --suite physics
haylen bench --suite lua --jobs 4
```

Builds a benchmark in the Release configuration for this machine and runs it. The option `--suite` picks it, `sprites` by default, and `--jobs` sets the jobs of the build. Every benchmark is a target of the engine, which `HAYLEN_BUILD_BENCHMARKS` adds. The guide of each subject shows the recorded results.

| Suite | Target | What it measures | Results |
| --- | --- | --- | --- |
| `sprites` | `haylen-sprite-benchmark` | Sprites drawn on the GPU of this machine, in phases. | [Rendering guide](../graphics/rendering.md#sprite-benchmark) |
| `core` | `haylen-core-benchmark` | On the headless host, the average time of: a frame of an app at rest and with a Lua scene, events and signals with C++ and Lua listeners, owners that end what they hold, timers that wait and timers that fire every frame, tweens of native values and of Lua tables with and without an owner, jobs that go to a worker and back, `parallelFor`, and JSON assets loaded in the background and on the frame thread. | [Architecture guide](../core/architecture.md#what-the-base-costs) |
| `algorithms` | `haylen-algorithm-benchmark` | The average time of: A\*, weighted A\*, jump point search, flow fields and hierarchical path finding on a 512 by 512 grid, navigation mesh builds and funnel paths, ORCA crowd steps on one thread, on the job system and on the job system while navigation mesh builds keep every worker busy, with the slowest of those steps, AABB and k-d tree queries, and batches of physics ray casts on one thread and on the job system. | The output |
| `procedural` | `haylen-procedural-benchmark` | The average time of: wave function collapse, Poisson disk sampling with a fixed and a varying distance, random scattering, marching squares, the Delaunay triangulation of 100 thousand points and carving destructible terrain. | The output |
| `physics` | `haylen-physics-benchmark` | On the headless host, the average time of: the step of a pile of falling boxes on one, two, four and eight threads, a settled pile with sleeping on and off, a pyramid on one and four threads and with two, four and eight sub-steps, a chain of 300 links, ragdolls that tumble down stairs, fluids of three sizes with the fluid pass alone and the whole step, ray casts and circle queries in a pile, reading the transforms of 5000 bodies with and without interpolation, the debug drawing of a pile, and a pile stepped from Lua with and without its contact and hit callbacks. | [Physics guide](../2d/physics.md#the-physics-benchmark) |
| `ui` | `haylen-ui-benchmark` | On the headless host, collections of a hundred thousand items of three types, flung in a list and in a grid: the CPU time of a frame with the items bound, the binder calls and the cells created while flinging and at rest. | [UI guide](../ui/ui.md#what-a-long-list-costs) |
| `render` | `haylen-render-benchmark` | On the headless host, whole frames of sprites, shapes, text, rich text and interface components, with the draw calls, instances, uploads and allocations of each frame. | [Rendering guide](../graphics/rendering.md#render-benchmark) |
| `particles` | `haylen-particle-benchmark` | On the headless host, the average time of: one emitter of 100 thousand particles on one thread and on the job system, a thousand emitters of a hundred particles, spawning 200 thousand particles per second, the draw calls of both kinds of emitter, and 500 emitters updated from Lua. | [Particles guide](../2d/particles.md#the-particle-benchmark) |
| `content` | `haylen-content-benchmark` | An app of 64 MiB of data that does not compress, 16 MiB of text, 2000 small files, a Lua module of 2 MiB, a JSON map and a picture, written to the temporary folder of the system. It times building the protected release, opening it, reading the large files in order and on four threads, reading the data in order from the app folder too, 2000 random reads of 4 KiB, and finding every small file in the catalog. It times compressing, decompressing, encrypting and decrypting chunks alone. It loads the module from its text in the app folder and as bytecode from the release, and the map, the picture and its raw pixels from the folder and from the release, so the pairs show what protection adds. It also loads the map cooked as CBOR, builds an update of two changed files with the bytes it stores and reuses, plans the download ranges of 100 thousand records, adds 256 records to a local store, and builds, opens and reads a release of 20 thousand files. Each line shows the throughput or the time, with the largest single allocation, so a reader that starts to hold whole files shows. | [Content guide](../content/content.md#performance) |
| `nodes` | `haylen-node-benchmark` | On the headless host, the update of a tree of 100 thousand sprite nodes after every root moved and after one root in a hundred moved, on one thread and on the job system, and its draw with every node in view and with most of them culled. | [`haylen.nodes2d`](../2d/nodes.md#performance) |
| `animation` | `haylen-animation-benchmark` | On the headless host, an animated batch of 100 thousand sprites on one thread and on the job system, and a crowd of 2000 units animated from Lua with an animator each and with one animated batch. | [`haylen.animation2d`](../2d/animation.md#performance) |
| `lua` | `haylen-lua-benchmark` | Runs the Lua sprite benchmark of `engine/bench/lua-benchmark` on the headless host. It prints how long Lua takes to update and draw 10 thousand, 100 thousand and a million sprites kept in tables, drawn in one batch or with a call each, in a float buffer and in a sprite batch. | [Lua guide](../core/lua.md#performance) |

### clean

```sh
haylen clean
```

Removes the whole `build/` folder: every build tree, the engine artifacts, the logs and the build folders of apps with the copies of the templates that haylen.py keeps for them. It keeps `.tools/` and the dependency cache in `.cache/cpm`.

## CMake options

The script `haylen.py` sets these cache variables for you. A project that adds the engine itself sets them directly.

| Option | Default | Meaning |
| --- | --- | --- |
| `HAYLEN_SHADERC` | empty | The path of a build of `haylen-shaderc`, which compiles the shaders of the engine. Empty, the configure provides the pinned compiler as [the shader compiler](#the-shader-compiler) explains, and configures again when the pins change. |
| `HAYLEN_SHADERC_CACHE` | the CPM source cache, or `tools` of the build tree without one | The folder that keeps the compilers that the configure provides, so build trees share them. |
| `HAYLEN_BACKEND_DIR` | `engine/src/backends/sokol` | The folder of the backend, which opens the window, runs and draws the frames and holds the entry point. Its `backend.cmake` adds its sources to the runtime, as the [architecture guide](../core/architecture.md#backends) describes. The backend takes the name of its folder, which `HAYLEN_BACKEND` holds. |
| `HAYLEN_GRAPHICS_API` | `AUTO` | The graphics API that the backend draws through: `AUTO`, `METAL`, `D3D11`, `GLCORE`, `GLES3` or `WGPU`. The value `AUTO` picks Metal on Apple platforms, Direct3D 11 on Windows, OpenGL ES 3 on Android, WebGPU on the web and OpenGL core on Linux. The shaders of the engine carry the shader languages of that graphics API alone. |
| `HAYLEN_BUILD_PLAYER` | on when the engine is the top-level project | Builds the `haylen` player: the executable on desktop and web, and the shared library of `HaylenActivity` on Android. |
| `HAYLEN_BUILD_TESTS` | on when the engine is the top-level project | Builds the tests on desktop. |
| `HAYLEN_BUILD_BENCHMARKS` | on when the engine is the top-level project | Builds every benchmark of [`bench`](#bench) on desktop. |
| `HAYLEN_BUILD_TOOLS` | on when the engine is the top-level project | On desktop hosts, builds the content tool `haylen-content` and its library `haylen-content-tool`. The tests link that library, so they build it too. |
| `HAYLEN_BUILD_SDK` | off | Merges the engine into the SDK that `cmake --install --component haylen-sdk` installs. |
| `HAYLEN_BUILD_FRAMEWORK` | off | On Apple platforms, with `HAYLEN_BUILD_SDK`, merges the SDK and the player into the `libhaylen.a` of one slice of `Haylen.xcframework`, which the `haylen-framework` component installs. |
| `HAYLEN_ENABLE_COVERAGE` | off | Instruments the engine and the tests with LLVM coverage. It needs Clang. |
| `HAYLEN_SANITIZERS` | `OFF` | The sanitizers of desktop builds that do not use MSVC: `OFF`, `ADDRESS` for AddressSanitizer and UndefinedBehaviorSanitizer on the targets of the engine, or `THREAD` for ThreadSanitizer on every C and C++ target of the build, dependencies included. |
| `HAYLEN_BUILD_FUZZERS` | off | Builds the fuzzers `haylen-content-fuzzer`, `haylen-input-fuzzer`, `haylen-messagepack-fuzzer` and `haylen-placeholder-fuzzer`, and instruments every target for libFuzzer and AddressSanitizer. It needs a Clang that links libFuzzer, which Apple Clang does not ship. |

The workspace `CMakeLists.txt` at the root of the repository is the project `haylen-workspace`. It adds `engine/` and turns the player, the tests, the benchmarks and the content tool on. The options of apps, such as `HAYLEN_PROTECTED_RELEASE`, belong to [`haylen_add_app`](embedding.md#haylen_add_app).

### Names of targets and files

Every CMake target, project and install component uses dashes, such as `haylen-engine`, `haylen-runtime`, `haylen-tests` and `haylen-sdk`. So does every library, executable and other file that the build makes, such as `libhaylen-app.so`. An underscore appears only where a name must be an identifier, such as the CMake function `haylen_add_app` and the variable `HAYLEN_PLATFORM`. Projects reach the libraries through the aliases `haylen::engine` and `haylen::runtime`, as the [embedding guide](embedding.md#what-a-project-gets) lists.

### Settings of release binaries

The file `engine/cmake/haylen-release.cmake` sets how every target of the engine and its dependencies compiles:

- Clang and GCC map the source folder of the engine, its build folder and the CPM cache to `engine/`, `build/` and `deps/` in the file names that `__FILE__` writes into binaries, such as those that assertions keep. So no binary names a folder of the machine that built it. MSVC has no such mapping, so a Windows build keeps the full paths of the files whose assertions it compiles.
- Clang and GCC place every function and object in a section of its own, which `--gc-sections` drops when nothing calls it. Apple platforms skip this, because their linker strips dead code by atom.
- On Apple platforms every target compiles with hidden symbols, since an Apple executable exports every global symbol it holds. So an app exports only the symbols of its own code, which [`native.findSymbol`](../lua-api/native.md#nativefindsymbolname) finds.
- A Release build compiles with link-time optimization wherever the compiler supports it, through `CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE`. That is thin LTO with Clang on Apple platforms, Android and the web, LTO with GCC on Linux and whole program optimization with MSVC on Windows.

With link-time optimization, the libraries and SDKs of the artifacts hold the intermediate code of the compiler. The link of each app then optimizes the engine, its dependencies and the app as one program, and drops what the app never reaches. A few cases differ:

- The SDK of Windows compiles to machine code. The intermediate code of MSVC would make its one library larger than the 4 GB that a library of Windows holds. A C++ app that compiles the engine with MSVC keeps whole program optimization.
- GCC and MSVC link that code only when the link asks for it. So `haylen::engine` carries `-flto=auto` or `/LTCG /INCREMENTAL:NO` to every program that links it, even a Debug app that links the prebuilt SDK. Clang links it on its own.
- An SDK holds code for the compiler that built it, so an app links it with the same compiler. That is how `haylen.py` builds both on one machine.
- The slices of `Haylen.xcframework` hold machine code. Both `xcodebuild -create-xcframework` and `lipo` refuse libraries of LLVM bitcode, since Xcode stopped taking bitcode. So a Lua app on Apple platforms links the engine with dead code stripping alone. A C++ app compiles the engine with its own code and links it with LTO there too.
- Zstandard passes an option to the assembler, which the link of GCC drops from code compiled for link-time optimization with a warning. So Zstandard builds as machine code.

### Shaders of the engine

The shaders of the engine live in `engine/shaders`, in the Haylen shader language. The function `haylen_compile_shader` of `engine/cmake/haylen-shaders.cmake` compiles them with `haylen-shaderc` at build time into `.shader` packages, which the engine embeds. A package holds the code of the shader languages of the graphics API of the build alone, which `HAYLEN_SHADER_LANGUAGES` of `engine/cmake/haylen-platform.cmake` lists:

| Graphics API | Shader languages |
| --- | --- |
| Metal | The three Metal languages, for macOS, iOS and the iOS simulator. |
| Direct3D 11 | HLSL 5. |
| OpenGL core | GLSL 4.30. |
| OpenGL ES 3 and WebGL2 | GLSL 3.00 ES. |
| WebGPU | WGSL. |

The programs that draw into lit canvases compile a second time with `HAYLEN_LIT`. Every compile includes `engine/shaders/include`, the shader library that the shaders of apps include too, as the [shader guide](../graphics/shaders.md) explains.

## Dependencies

Dependencies are declared with [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) in `engine/cmake/haylen-dependencies.cmake`. The file `engine/cmake/cpm.cmake` downloads CPM 0.43.2 and checks its SHA-256 hash. CPM keeps the sources in `.cache/cpm` under the top-level source folder, so several build trees share one download. Set `CPM_SOURCE_CACHE`, as a CMake variable or an environment variable, to keep them elsewhere.

A patched package, such as Sokol, keys its place in the cache with `haylen_patched_package_key`. The key is a hash of its archive and of the contents of its patches. So an edited patch downloads and patches the package again in every checkout, while checkouts in other folders share the same package. The patches are configure dependencies of the build tree, so the next build after a patch changes configures again and fetches the package under its new key.

| Package | Version | Notes |
| --- | --- | --- |
| nlohmann/json | 3.12.0 | Added first, so Varn reuses it and the build holds one copy. |
| Varn | commit `7a20070` | The Lua runtime, the event loop, the worker pools and the modules `async`, `http`, `socket`, `json`, `fs`, `zip`, `crypto` and the others. `VARN_TARGET` is `cli` on desktop and Apple platforms, `android` on Android and `wasm` on the web. The `cli` target builds the static core that the engine links. Varn picks the HTTP client of each platform itself: the Apple one on iOS, tvOS and Mac Catalyst, the Android one on Android and `fetch` on the web. Lua, libuv, zlib, libzip, Poco and OpenSSL come in through Varn, each pinned with its hash. ThreadSanitizer builds tell Varn to keep the yields of Lua as exceptions, because the sanitizer cannot follow the `_longjmp` of a yield. |
| Sokol | commit `5e94d12` | Headers only. The Sokol backend compiles the implementation for the chosen graphics API, with the [patches](#patches) of `engine/cmake/patches`. |
| GameActivity | 4.4.2 | Android only, Apache 2.0. The AAR of `androidx.games:games-activity` from Google Maven, pinned by its hash. Its prefab folder holds the static library and the headers of the native side of GameActivity for every ABI, which the runtime links. |
| stb | commit `2c980bb` | Headers only. |
| NanoSVG | commit `239e102` | Headers only. It reads SVG documents into curves and rasterizes vector images. It publishes no releases, so it is pinned to a commit of its default branch. |
| msdfgen | v1.13 | Only its core, which builds the distance fields of font glyphs from their whole outlines, the cubic curves of CFF fonts included. |
| HarfBuzz | 14.6.0 | Old MIT license. Its amalgamated source compiles as `haylen-harfbuzz` with `HB_MINI`, the OpenType shaper without the AAT and legacy shapers. |
| SheenBidi | v3.0.0 | Apache 2.0. The Unicode Bidirectional Algorithm and script runs, compiled from its unity source as `haylen-sheenbidi`. |
| libunibreak | 8.0 | zlib license. Line breaks and grapheme clusters of Unicode 17, compiled as `haylen-unibreak`. |
| BudouX | v0.9.3 | Apache 2.0. Only its Thai phrase model, a JSON file that the engine embeds to break Thai lines. |
| fast_float | v8.3.2 | Headers only. It parses floating point numbers on every platform, since `std::from_chars` for floating point needs iOS, tvOS and macOS 26. |
| Dear ImGui | v1.92.9b | Compiled as `haylen-imgui` with the ImGui configuration of the engine. |
| miniaudio | 0.11.25 | Compiled as `haylen-miniaudio`. |
| Box2D | v3.1.1 | |
| Clipper2 | 2.0.1 | The polygon booleans, offsets and constrained triangulations of `math::Polygon`. |
| zstd | v1.5.7 | A static library for compressed Tiled layers and for the chunks of protected content. |
| Monocypher | 4.0.3 | BSD 2-clause or CC0. The authenticated encryption (XChaCha20-Poly1305), the hashes and key derivation (BLAKE2b) and the manifest signatures (Ed25519) of protected content, compiled from its two sources as `haylen-monocypher`. It is audited, has no dependencies and builds the same on every target, the web included. This release fixes a timing leak of its signatures. |
| GoogleTest | v1.18.0 | Only when the desktop tests build. |

Every package is pinned by the URL of a release archive, or of a commit archive for a project without releases, with its hash:

```cmake
CPMAddPackage(
  NAME box2d
  URL "https://github.com/erincatto/box2d/archive/refs/tags/v3.1.1.tar.gz"
  URL_HASH SHA256=fb6ef914b50f4312d7d921a600eabc12318bb3c55a0b8c0b90608fa4488ef2e4
  OPTIONS "BOX2D_SAMPLES OFF" "BOX2D_BENCHMARKS OFF" "BOX2D_DOCS OFF" "BOX2D_UNIT_TESTS OFF" "BOX2D_VALIDATE OFF"
  SYSTEM YES
)
```

To add or update a package:

1. Point `URL` at the archive of its latest release. Varn is pinned by the archive of one commit, whose contents never change.
2. Compute the hash of that archive, such as with `curl -L <url> | shasum -a 256`.
3. Adopt the new API everywhere the engine uses it.

A package that Varn also uses goes before the Varn block, so the whole build resolves it once. Header-only sources use `DOWNLOAD_ONLY YES` and an interface library, like `haylen-sokol-headers` and `haylen-stb`.

Two details of the packages that come through Varn need care:

- Varn builds OpenSSL with its own `make` as one step of the build. That step runs the jobs that `CMAKE_BUILD_PARALLEL_LEVEL` named when the tree was configured, or one job without it. The script `haylen.py` removes the variable, so a build never runs more jobs than `--jobs` gives it.
- The headers of Poco ask MSVC to link every Poco library by its file name, such as `PocoFoundationmd.lib`. The SDK merges those libraries into `haylen.lib`, so an app that links the SDK would look for files that do not exist. The engine defines `POCO_NO_AUTOMATIC_LIBS` for Poco and every target that uses it, since CMake links the Poco libraries by their targets.

Two more notes hold for miniaudio. It manages the audio session on iOS and tvOS with Objective-C, so its implementation file compiles as Objective-C there. On Android the engine builds it with AAudio as its only backend, which miniaudio uses from Android 8.1 (API 27) on, because the first AAudio release of Android 8.0 has known faults. That is why the [Android library](distribution.md#the-android-libraries) needs API 27.

### Patches

A change to the source of a dependency is a patch in `engine/cmake/patches/` that CPM applies. CPM applies the patches of Sokol in the order of the list in `engine/cmake/haylen-dependencies.cmake`. Each patch below fixes one problem of the pinned commit.

#### sokol-ios-view-size.patch

The module `sokol_app` sizes the framebuffer of iOS apps by the screen. That crops every app whose window is smaller than the screen, such as Mac Catalyst windows and iPad windows. The patch sizes it by the view of the app instead.

#### sokol-desktop-window.patch

The module `sokol_app` opens every desktop window with a title bar, in the taskbar, without a position and able to take the focus. So a frameless, topmost, unfocusable or taskbar-less window of `app.json` would show its default style for its first frame. Its windows of Direct3D 11 and X11 also cannot be transparent. The patch:

- Creates the window with those options and its position before it first shows.
- Adds `sapp_set_window_focusable`, so the focus behavior changes while the app runs.
- Makes transparency work on Direct3D 11 through DirectComposition and on X11 through ARGB visuals.
- Gives X11 windows the class by which desktops find the desktop entry of the app.
- Gives Windows windows the icon of the executable instead of the logo of the system.

#### sokol-android-quit.patch

The module `sokol_app` ends a destroyed Android activity with `exit()`. That runs the static destructors of the process while the rendering threads of Android still use them, so the process aborts with a crash report every time the app closes. It also ignores `sapp_quit()` on Android. The patch calls the cleanup callback of the runtime when the activity is destroyed, even after its window is gone, by binding the GL context without a surface. So the engine stops and releases its resources. Then the activity finishes normally, and the process stays cached like any Android app. The patch also makes `haylen.quit()` finish the activity, and a later activity in the same process starts a new runtime.

#### sokol-apple-classes.patch

On Apple platforms, `sokol_app` creates its own application delegate, which also delegates the scene on iOS, tvOS and Mac Catalyst. It is the only object that receives the launch, the links and user activities that open the app, the shortcut items, the life of the scene and the registration and delivery of remote notifications. Native plugins need all of them. Swizzling the class of `sokol_app` at run time would hide behavior and fight the SDKs that swizzle the delegate themselves.

The patch adds `sapp_desc.apple.delegate_class`, which names the class that `UIApplicationMain` or AppKit creates instead. It lets the scene configuration name the class of the application delegate, so one object delegates both. The runtime names `HaylenSceneDelegate` on iOS, tvOS and Mac Catalyst and `HaylenAppDelegate` on macOS. Both derive from the delegates of `sokol_app`, call them for the events they handle and hand every event to the plugins, as the [plugin guide](../platform/plugins.md#events-of-the-app) describes.

The scene configuration names that class for every scene. So `HaylenSceneDelegate` passes only the scene that holds the window of the app to `sokol_app`, gives the windows of screens a delegate of their own and removes other scenes of the app. That way `sokol_app` never builds a second window on its global state.

UIKit asks the view controller of the window whether the status bar shows and which content color it has. The plain `UIViewController` of `sokol_app` answers neither. So the same patch adds `sapp_desc.apple.view_controller_class`, which names the subclass that holds the Metal view. The runtime names `HaylenViewController`, which answers with the status bar of the app.

#### sokol-android-gameactivity.patch

The module `sokol_app` hosts Android apps in `NativeActivity`, which extends the plain `Activity`. So it can never be the `ComponentActivity` that the Activity Result API and current SDKs need, such as paywalls, payment sheets and biometric prompts. It also takes the window surface, so views of the activity never draw over the app. The patch hosts apps in GameActivity of the AndroidX games libraries 4.4.2, an `AppCompatActivity` whose `SurfaceView` lives in an ordinary view hierarchy. It keeps the render thread of `sokol_app` and its handshakes with the UI thread.

Input:

- GameActivity delivers input on the UI thread. The patch copies it into a queue that the render thread drains. The queue grows when a slow frame fills it, so no release is ever dropped.
- Keys go through a key table with the characters of hardware keyboards.
- The UI thread decides which keys the app takes. Back and the system keys go on to the views and the system, and a release never reaches the app without its press.
- `HaylenActivity` hands every release to the app before the views. So the release of a press that the app took reaches it, even when a view took the focus in between, such as the hidden field that the press started to edit.

Frames:

- From Android 10 on, frames follow the Choreographer through `AChoreographer_postFrameCallback64`, which the patch looks up at run time. Older devices wait for the display refresh in `eglSwapBuffers`.
- The module `sokol_app` runs Android frames only while the window of the activity has the focus. That would stop timers, cancels and timeouts under every dialog, and leave the surface black when the app comes back under one. The patch runs frames while the activity is resumed and has a surface, whatever window has the focus. It reports the changes of that focus as focus events, which make the app `'inactive'`.
- A frame swaps its buffers only when it acquired the swapchain. A frame that drew nothing, such as those of a covered app after its one covered frame, would show a buffer of undefined content. So the surface keeps the last picture, and older devices wait about one display refresh after such a frame.
- The module `sokol_app` runs no frames at all while the activity is paused. That would stop the event loop of the app under every other activity, such as a document picker, a permission prompt or the screen of a plugin, and in the background. The patch ticks a paused activity every 100 milliseconds with a frame that presents nothing. In those frames the app, in the background from the pause on, runs its event loop and draws nothing.
- Once the surface is gone, the GL context stays current without one (`EGL_KHR_surfaceless_context`). So those ticks still create and release GPU resources.

The engine links the static library of GameActivity from the prefab folder of its AAR, which CPM pins by its hash. It links with `-u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode`, since nothing in the library references the JNI entry that GameActivity looks up.

#### sokol-ios-scene-states.patch

The module `sokol_app` suspends iOS, tvOS and Mac Catalyst apps whenever their scene resigns active. That happens under the app switcher, Control Center, a phone call or another app in front of a Mac Catalyst window, and it stops the frames of an app that still shows. The patch reports this as the loss of the focus while the frames go on. It suspends the app only when its scene enters the background.

#### sokol-scroll-units.patch

The module `sokol_app` scales the points that trackpads and other precise devices scroll. It divides them by ten on macOS and by a hundred on the web, without telling them apart from the steps of a wheel. It also counts each of the three lines that browsers report for a step of a wheel as 1.33 steps. So the same gesture scrolls by different amounts on every platform. The patch:

- Reports precise scrolls in the points they move, with `scroll_precise` set.
- Reports the scroll of wheels in steps, one for each step of a mouse wheel on macOS, Windows and Linux, and on the web where the browser reports the wheel in lines, as Firefox does. Chromium and Safari report most wheels in pixels, about 100 for a step, which a page cannot tell apart from a trackpad. There a step of a wheel scrolls like a trackpad that moves as far.
- On macOS, reports the phase of the gesture in `scroll_phase`, from the phase and the momentum phase of the event: the fingers touch the trackpad, move or leave it, or the system adds momentum after them.
- Also sends the scrolls that only change the phase, when the fingers touch or leave the trackpad, which `sokol_app` dropped for having no movement. So the UI knows when to release the content and run its own inertia, as on iOS.

The engine passes the flag on as `Event::preciseScroll` and the phase as `Event::scrollPhase`. The UI scrolls by the points of a trackpad and by the theme metric `scrollLine` for each step of a wheel, as the [UI guide](../ui/ui.md#scrolling-with-wheels-and-trackpads) describes. Android reports the wheels of mice and the scrolling of touchpads as scroll motion events. The engine takes them from GameActivity before `sokol_app`, in steps of a wheel at the place of the pointer.

#### sokol-web-page-events.patch

The module `sokol_app` listens for the keys of the whole page on the window. It also takes the focus moving between the elements of the page, such as from the canvas to a button, for the app losing the focus. So a page that hosts the app next to elements of its own, such as the editor of a website, could keep their keys from the app only by stopping them before they reach those elements. The patch skips a key whose time stamp the runtime names in `Module.sappIgnoredKey`. The runtime does this for the keys of elements that are not the canvas, the page or the fields of the runtime, so those elements still hear them. The patch reports only the focus of the window.

#### sokol-ios-keyboard-presses.patch

The module `sokol_app` turns only the arrows and the Select, Menu and Play/Pause presses of UIKit into keys. So a hardware keyboard of an iPad or an Apple TV would reach the app through those keys alone. The runtime reads every key of a hardware keyboard through `GCKeyboard` on iOS, iPadOS, tvOS and Mac Catalyst. The patch leaves the presses that carry a key of a keyboard to it, so no key arrives twice, and keeps the presses of remotes and game controllers. On tvOS it keeps the Menu press that the Escape key of a keyboard makes, so Escape goes back like the remote.

#### sokol-webgpu-copy-source.patch

The module `sokol_gfx` declares the `copy_src` usage of images, but does not use it yet. So it creates WebGPU textures without the `CopySrc` usage, which `sokol::ImageReadback` of the Sokol backend needs to copy a render target into a buffer that the CPU maps. The engine creates render targets with `copy_src`, and the patch gives the textures with that usage `CopySrc`.

#### sokol-metal-gpu-time.patch

The module `sokol_gfx` measures no time on the GPU. The GPU lane of [profiler traces](../lua-api/debug.md#traces) needs it. The patch reads the start and end times of the GPU from the completed command buffer of every Metal frame. It adds `sg_mtl_gpu_frame_nanoseconds()`, which returns the time of the last completed frame.

#### sokol-validation-shifts.patch

The validation of `sokol_gfx` sets the bit of a view slot by shifting a signed 1. That overflows for the last of its 32 slots, an undefined behavior that the sanitizers report. The patch shifts an unsigned 1.

#### nanosvg-visibility.patch

NanoSVG hides the shapes whose `display` is `none`, but draws those whose `visibility` is `hidden` or `collapse`. A drawing program may leave such a hidden background behind a logo, which the vector images of the engine and the SVG pictures of the UI would then show. The patch hides those shapes too.

### Choices that need no patch

- The module `sokol_app` asks Android for an OpenGL ES 3.1 context, which some devices and some configurations of the Android emulator lack. The shaders of the engine are GLSL ES 3.00, which every version runs. So the runtime asks for the newest version that the device declares in `ro.opengles.version` instead. The emulator creates a context of exactly the version an app asks for, while every context reports the newest version of the device. So `sokol_gfx` would otherwise query the features of that version, such as the limits of OpenGL ES 3.1, in a context that rejects them.
- On Windows and Linux, the Sokol backend compiles `sokol_app` with `SOKOL_NO_ENTRY`, so its entry point, `WinMain` or `main`, returns the exit status of the app. On macOS the application delegate of the backend ends the process with it.
- On Apple platforms the Sokol backend also compiles `sokol_app` with `SOKOL_NO_ENTRY`, so the runtime never defines `main`. The app calls `haylen_main` instead, as [`Haylen.xcframework`](distribution.md#haylenxcframework) shows.

## The desktop player

The `haylen` player is the runtime without a bundled app. It runs any package that its command line names, a folder or a zip:

```sh
build/macos-debug/bin/haylen/haylen --dev ~/apps/my-game
build/macos-debug/bin/haylen/haylen my-game.zip
```

The option `--dev` turns on development mode, as [development mode](distribution.md#development-mode) explains with every option of the command line. The command `run` passes it. Without `--dev`, the player runs the package like a shipped app. Without a package on its command line, the player runs the folder `app/` next to it, or a zip of it named `app.zip`. A package that fails to load keeps the window open and shows the error.

The player, like every desktop app, exits with the status that the app quit with, which [`haylen.quit`](../lua-api/haylen.md#haylenquitoptions) sets. It exits with 0 when its window closes, and with 1 when its engine could not start.

The command `haylen engine --platform desktop` builds the player for apps, as the [desktop artifacts](distribution.md#web-and-desktop) describe.

## Web builds

Web builds run on one thread, like Varn. So they need no `SharedArrayBuffer` and no cross-origin isolation. The command `serve` sends an opener policy only when `--coop` asks for one, as [serve](distribution.md#serve) explains.

Audio reaches the browser through an `AudioWorkletNode`, which the page feeds over its message port, as the [audio guide](../audio/audio.md#sessions-and-interruptions) describes. Browsers offer `AudioWorklet` only to pages served over https or from localhost. The commands `serve`, `run --platform web` and `run-cpp --platform web` serve on `127.0.0.1` by default, so their pages play sound. Apps run on any other page too, without sound, as the [audio guide](../audio/audio.md#without-an-audio-output) explains.

The link options come from `haylen_link_runtime_platform`:

- Memory growth and a stack of 1 MB.
- IDBFS for user data.
- The `emdawnwebgpu` port for WebGPU, and WebGL 2 only for the `web-webgl2` platform.

The engine target adds the native exceptions and the native `longjmp` of WebAssembly, `-fwasm-exceptions` and `-sSUPPORT_LONGJMP=wasm`. Varn compiles Lua as C++, so Lua errors unwind as exceptions and yields jump back to their resume. Every current browser runs both without help from JavaScript.

### Files of a web build

A web target produces `<target>.html`, `<target>.js`, `<target>.wasm` and, for apps, `<target>.data` in `bin/<target>/`. A step after the build, `haylen_setup_web_page`, copies three more files next to them:

- The shell of the target as `<target>.shell.html`. That is the `WEB_SHELL` of `haylen_add_app`, or `engine/platform/web/shell.html` by default.
- `haylen-logo.svg`, the engine logo, which the default shell shows as the icon of the page.
- `haylen-audio-worklet.js`, the AudioWorklet processor of the audio output. The runtime loads it from the folder of its script through `locateFile`.

The prebuilt player of `haylen engine --platform web` is the `haylen` target of the `web` and `web-webgl2` trees, whose page is the [web loader](distribution.md#web-loader). The command `run-cpp --platform web` builds a C++ app target for both graphics APIs and writes this layout:

```text
build/cpp/embedding-<hash>/web/
  index.html              The shell of the target, with engine/platform/web/graphics-api-picker.html in place of {{{ SCRIPT }}}.
  haylen-logo.svg         The icon of the default shell.
  plugins.json            The web parts of the plugins of the app, which the page loads before the app starts.
  webgpu/                 main.js, main.wasm and main.data built for WebGPU, next to haylen-audio-worklet.js and haylen-content-store.js.
  webgl2/                 The same files built for WebGL2.
```

The graphics API picker runs the WebGPU build when `navigator.gpu` returns an adapter, and the WebGL2 build otherwise. It points `Module.locateFile` at the folder of the chosen build, `webgpu` or `webgl2`, and loads `<folder>/<target>.js`. Add `?graphicsApi=webGpu` or `?graphicsApi=webGl2` to the address to force one of them. Those are the names that `graphics.apiName()` gives the two APIs. Add `?package=<url>` to download a zipped package.

The page is the own shell of the target, so everything that a custom `WEB_SHELL` adds works in the bundle too, such as the page handlers of the [platform bridge](../platform/platform-bridge.md#web). So do the handlers of a script that the target links with `--pre-js`, such as `platform/web/app.js` of the C++ template. The command `run-cpp` fails when the shell has no `{{{ SCRIPT }}}` placeholder.

### Page setup

A page defines `Module` before it loads the runtime script. The file `templates/lua/platform/web/loader.js` is the model for the pages of Lua apps, and `engine/platform/web/shell.html` for C++ apps.

| Field | Meaning |
| --- | --- |
| `Module.canvas` | The canvas that the app draws into. It must be a canvas element with an `id`. The runtime follows its size with a `ResizeObserver`, so the layout of the page decides the size. |
| `Module.haylen.packageData` | Optional bytes of a zipped package, as an `ArrayBuffer` or a `Uint8Array`. The runtime writes them to its file system and plays them instead of the bundled package. The web loader downloads `app.zip` with a progress bar and hands it over this way. |
| `Module.haylen.packageUrl` | Optional URL of a zipped package, used when `packageData` is not set. The runtime downloads it before the app starts and plays it instead of the bundled package. A failed download reaches `onError`. The default shell of C++ apps reads it from the `package` parameter of the address. |
| `Module.haylen.release` | Optional `{address, identifier, files}` of a protected release. The `files` are `{name, bytes}` of its manifests and its app domain, which the page downloaded and which go to `/app`. The `address`, such as `app/`, is where the shards of the content stay and download by range. The `identifier` of the app names its [content store](../content/content.md#the-web). The web loader sets it in release builds. |
| `Module.haylen.development` | Optional `true` that runs the app in development, as the player does with `--dev`. Lua modules and assets that the page changes with `applyChanges()` then reload in place, as [hot reload](../core/lua.md#hot-reload) describes. |
| `Module.haylen.developmentServer` | Optional WebSocket address of the development server that `run --platform web` starts, such as `ws://127.0.0.1:8000/haylen/development?token=<token>`. The server pushes every file saved in the app to the page. |
| `Module.instantiateWasm` | Optional function that receives the imports of the module and a callback. It instantiates `haylen.wasm` itself and passes the instance to the callback. So a page that already downloaded the bytes, such as the web loader, keeps Emscripten from fetching the file again. |
| `Module.preRun` | Functions that run before the app starts. Page handlers of the platform bridge register here. |

None of the package fields turns on development mode. Only a page that sets `Module.haylen.development` runs in development. The template loader sets it only for the `development` entry of `config.json` that `run --platform web` writes.

A custom shell passed as `WEB_SHELL` keeps the `{{{ SCRIPT }}}` placeholder. Emscripten inserts the runtime script there, and `run-cpp` inserts the graphics API picker. Before the app starts, the runtime mounts IndexedDB at `/persistent` for user data and loads it. It also reads what the browser tells about the device through `navigator.userAgentData` and the Battery Status API, which answer asynchronously.

### Runtime API

The file `engine/platform/web/haylen-runtime.js` is linked with `--pre-js`. It fills `Module.haylen` with the API that a page or a browser editor uses. None of these calls reloads the page.

| Function | Meaning |
| --- | --- |
| `loadZip(bytes)` | Replaces the running app with a zipped package, given as a `Uint8Array` or an `ArrayBuffer`. A file that is not a valid zip shows the error screen and reaches `onError`, like any package that fails to load. |
| `clearFiles()` | Stops the app and empties its package, for an app that the page builds file by file before `run()`. |
| `setFile(path, content)` | Adds or replaces a file of the package that plays. The `path` is relative to the root of the package, such as `source/main.lua` or `content/ui/logo.png`, and `content` is a string or bytes. The app reads the new file the next time it loads it, and `applyChanges()` applies it at once. An absolute path, or one that leaves the package, throws an `Error` with the reason. |
| `removeFile(path)` | Removes a file of the package that plays and returns whether it existed. An invalid path throws an `Error`. |
| `applyChanges()` | Applies every file that `setFile` and `removeFile` changed since its last call, as one batch at the start of the next frame. A page in development reloads them as [hot reload](../core/lua.md#hot-reload) applies saved files: assets and Lua modules in place, and `app.json` or `source/main.lua` with a restart. Any other page restarts the app with them. |
| `run()` | Starts the package from `source/main.lua`, in place of the running app. |
| `restart()` | Starts the last app again from its package, which loads every script again. |
| `console(line)` | Runs a line of the [developer console](../lua-api/debug.md#console) in the running app. It returns a promise of `{ok, output}`, which resolves once the next frame ran the line. An app outside development without `debug.console` in `app.json` answers with `ok` set to `false` and the reason. |
| `stop()` | Ends the app and leaves an empty canvas until the next start. |
| `setPaused(paused)`, `pause()`, `resume()` | Pause or resume the app. A paused app keeps its last frame and sees the pause as a suspend. |
| `paused()` | Returns whether the app is paused. |
| `register(method, handler)`, `unregister(method)`, `emit(event, payload, options)` | The handlers and events of the platform bridge, with `options.retain` to keep an event for the first listener, as the [platform bridge guide](../platform/platform-bridge.md#web) describes. |
| `createPluginContext(id, config)` | The context of the web module of a plugin, as the [plugin guide](../platform/plugins.md#web-modules) describes. |
| `nativePlugins()` | Returns the ids of the plugins that have a web module. |

The runtime of a release carries the bootstrap of its app and plays only its protected release. So `loadZip`, `clearFiles`, `setFile`, `removeFile`, `applyChanges` and `run` throw an `Error` there and leave the app as it is.

No C++ exception crosses into the page. A function of the runtime that fails while it answers the page throws an `Error` with the reason. Otherwise it logs an engine error that names the function, and the page goes on.

The runtime reports back through callbacks that the page assigns to `Module.haylen`:

| Callback | Arguments |
| --- | --- |
| `onLog(level, message)` | The `level` is `'debug'`, `'info'`, `'warning'` or `'error'`. |
| `onError(error)` | `{message, file, line, traceback, frames}` of the error that stopped the app, with an empty `file` when no script position is known. The `traceback` is the stack as text, one frame per line. The `frames` list the same frames as `{source, line, function, kind}` objects, innermost first, with the kind `lua`, `c` or `main`. |
| `onStarted(app)` | `{name, identifier, version}` from `app.json`. |
| `onStopped()` | The app ended. |
| `onSplashEnded()` | The runtime ended the splash screen, which the page then fades out. |
| `onReloaded(report)` | In development, after every batch of changes that hot reload applied. It is `{restarted: false, mode, modules, assets, resumed, milliseconds}`, with the files of the modules and assets that reloaded in place and whether the app left the error screen. Right before a restart it is `{restarted: true, reason}`. |
| `onStats(stats)` | About once per second: `fps`, `frameMilliseconds`, `averageMilliseconds`, `drawCalls`, `sprites`, `vertices`, `textureSwitches`, `uploadedBytes`, `assets`, `voices`, `scopes` and `audio`. The `scopes` are a list of `{name, milliseconds, calls, depth}` profiler scopes. The `audio` is the output of the app as `{available, state, sampleRate, bufferedMilliseconds, blocks, underruns}`, or only `{available}` while the app runs without an audio output. |

In `onStats`, `available` tells whether the app plays sound, `state` is the state of its `AudioContext` and `bufferedMilliseconds` is the mixed audio that waits ahead of the output. The `blocks` count the blocks mixed so far, and `underruns` the render quanta that the processor played without samples.

Callbacks run in a microtask right after the frame that raised them. So they may call back into the runtime, even to restart the app.

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

The command `haylen build --platform android` builds the engine and the player library `libhaylen.so` for arm64-v8a with the NDK and Ninja, in `build/android-<config>`. That is the quick way to compile the engine for Android. Apps come from the Android library and the Android template:

- The folder `engine/platform/android` is the Gradle project of the Android libraries of Haylen, which [the Android libraries](distribution.md#the-android-libraries) describe. The command `haylen engine --platform android` builds and publishes them.
- The folder `templates/common/platform/android` is the Gradle project of an app, without C++, which [the Android template](distribution.md#android) describes. The command `run --platform android` builds it, as [the Android project](distribution.md#the-android-project) describes.

Gradle finds the Android SDK through `ANDROID_HOME` or a `local.properties` file, as in any Android project. R8 minifies the release builds of apps, and the upload key of the app signs them, which [`android-key`](distribution.md#android-key) creates.

## iOS, tvOS and Mac Catalyst

The command `haylen configure --platform ios`, or `--platform tvos`, generates an Xcode project of the engine at `build/<platform>-<config>/haylen-workspace.xcodeproj`. The command `build` compiles it through `cmake --build`. Apps for iOS, iPadOS, Mac Catalyst, tvOS and macOS come from the XcodeGen project of the app. It starts from `templates/lua/platform/apple` and links the prebuilt `Haylen.xcframework` through the target templates of its generated folder, as [the Apple project](distribution.md#the-apple-project) describes. C++ apps build their Apple bundles with `haylen_add_app`, as the [embedding guide](embedding.md#apple-bundles) describes.
