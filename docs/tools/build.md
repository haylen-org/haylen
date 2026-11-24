# 🔨 Building Haylen

This guide is for people who build the engine itself. It explains the command `haylen` and the Python package behind it, what each target needs, the tools that the command downloads, the engine SDKs and the engine ports that build them, the build trees, the engine commands, the CMake options, the dependencies with their patches, the desktop player and the web runtime. Other guides cover the rest: the [command reference](commands.md) lists every command with its options and its result, the [distribution guide](distribution.md) covers apps and their targets, the [testing guide](testing.md) covers tests and continuous integration, and the [embedding guide](embedding.md) covers projects that add the engine with CMake.

## The command haylen

The command `haylen` is the one entry point for every task of the engine and its apps. It is the Python package `tools/haylen`, which [uv](https://docs.astral.sh/uv) runs. The file `pyproject.toml` at the root of the repository declares the package, its one dependency, its entry point and its development tools, `uv.lock` pins every package to an exact version and hash, and `.python-version` names the Python that the package runs on. So uv is the only thing the tooling needs on a machine. It installs that Python and the locked packages into `.venv` the first time a command runs, and never touches the Python of the system.

The file `haylen` runs the command on macOS and Linux, and `haylen.cmd` runs it on Windows. Each one only runs `uv run --project <root> --frozen haylen` with every argument, from any folder and through a link, and stops with a sentence that says how to install uv when it is missing. Put the root of the repository on `PATH`, and every command below is `haylen <command>`.

```sh
export PATH="$HOME/path/to/haylen:$PATH"
haylen --version
haylen doctor
haylen app run --help
```

### Options of every command

Every command takes three options, before or after its words:

| Option | Meaning |
| --- | --- |
| `--verbose` | Prints the commands after `$` and the whole output of every tool, which the command otherwise keeps in its log. |
| `--json` | Prints the result of the command as one JSON document on the standard output, and its steps and the output of the tools that it prints, such as with `--verbose`, on the error output, so the standard output holds the document alone, for continuous integration and editors. |
| `--jobs` | The parallel jobs of the builds, one less than the processors of this machine by default. |

With `--json`, a command that succeeds prints `{"ok": true, "result": {...}}`, whose result the [command reference](commands.md) describes for each command. A command that fails prints `{"ok": false, "error": {...}}` and exits with the code 1. The error holds its `code`, such as `incompatibleSdk`, `missingToolchain`, `unsupportedTarget`, `invalidManifest`, `invalidContribution` or `buildFailed`, its `message`, and, when it has them, its `origin`, the manifest or plugin it comes from, and its `details`, the output of the tool that failed.

```sh
haylen --json sdk list
```

### What the commands print

Each step prints a title after `==>`. Successes, warnings after `Warning:` and errors after `Error:` follow. The tools that a command runs, such as CMake, Ninja, `xcodebuild` and Gradle, write their whole output to the log of the command, which `build/logs` names after the words of the command, such as `build/logs/app-run.log`. Every run of a command starts its log again. Meanwhile a terminal shows the latest line of the tool in place under the step, in a dim color. A line longer than the terminal is wide loses its middle to an ellipsis, so its start and its end, such as the file that the tool works on, stay visible. The cut falls between whole graphemes, the characters that a person sees, so an accent or an emoji that joins several characters never splits around the ellipsis.

When a tool fails, the command prints its last 60 lines and the path of the log. Some output always prints, because you read it: the log of the app that runs, the results of tests and benchmarks and the coverage report. While an app runs, `app run` shows its summary and the keys that act on it, as the [distribution guide](distribution.md) describes.

An error is one block with the cause and what to do. The output of the tool that failed follows it, when there is one. The command then exits with the code 1, without a Python traceback. Only an unexpected failure prints a traceback. Ctrl+C stops a command with the code 130.

```text
==> Preparing the site build/apps/my-game-70da5b67/web-debug/project
==> Serving build/apps/my-game-70da5b67/web-debug/products at http://127.0.0.1:8000/
```

A command that lists or sums up something prints groups and tables. A group is a bold title with its count after it, and a blank line sets it apart. A table has a row of dim titles and its columns line up. An empty cell shows a dash. A sign in front of a state or a result tells it at a glance, and every success starts with a check.

| Sign | Color | Meaning |
| --- | --- | --- |
| `●` | Green | Ready, booted or connected. |
| `○` | Dim | Off, or not built yet, such as a simulator that is shut down or a target without its SDK. |
| `×` | Yellow | Out of reach, such as a disconnected device or a target of another host. |
| `✓` | Green | Passed or done. |
| `✗` | Red | Failed, or a problem. |

An output whose encoding lacks these symbols, such as a console of an old code page of Windows, shows `*`, `o`, `x`, `+` and `x` in their place, and `-` for an empty cell.

A table that is wider than the terminal shrinks some of its columns. A name wraps between its words, and a path loses its middle to an ellipsis. An identifier that you copy, such as the one that `--device` takes, never shrinks. When the table still does not fit, the identifier moves to a line of its own under its row. Output to a file or a pipe keeps every column whole.

```text
Android  2
  Name            Kind      System      State    Identifier
  moto g(8) plus  Device    Android 10  ● Ready  0061296928
  Pixel_10a       Emulator  Android 17  ● Ready  emulator-5554
```

On a terminal, titles are bold after a blue `==>`, commands are dim, successes start with a green check, warnings are yellow and errors are red. Reserved expressions, such as commands, options, keys and values, show in a soft teal without the double quotes that mark them in plain text. Only URLs are underlined, since terminals open them. Paths print bare, with no style. Color follows the usual rules of terminals:

- The variable `NO_COLOR` with any value turns color off. The groups, tables and signs keep their structure as plain text.
- The variable `FORCE_COLOR` turns it on when the output is not a terminal, such as a log of continuous integration.
- Without either, the output has color only on a terminal whose `TERM` is not `dumb`.
- On Windows, the command turns on the escape sequences of the console it writes to.

### The package of the tooling

The package `tools/haylen` holds layers that depend on each other in one direction:

| Folder | What it holds |
| --- | --- |
| `cli/` | The command line: the tree of commands, their options and the printing of results. |
| `core/` | The manifest `haylen.json`, the resolution of an app into an immutable project for one target, the pipeline of prepare, build, verify and run, and the typed errors. |
| `plugins/` | The plugin manifest, the resolver of plugins, their contributions and `haylen.lock`. |
| `platforms/` | The contracts `PlatformBackend` and `EnginePort`, the registry that finds them, and one folder per backend with its template, `apple`, `android`, `desktop`, `player` and `web`. |
| `sdk/` | The manifests of installed SDKs and the choice of the slice of each target. |
| `native/`, `assets/`, `content/`, `development/` | The CMake trees and native libraries, the shaders of apps, protected releases, and the development server of runs. |
| `engine/` | The trees, tests, benchmarks, fuzzers, coverage and format of the engine itself. |
| `toolchains/` | Each tool that a build needs, how to find it and how to download it when the tooling brings it. |
| `scaffold/`, `schemas/` | The app and plugin that `new` and `plugin new` create, and the JSON Schemas of `haylen.json`, `plugin.json` and `haylen-sdk.json`. |

The core never imports a backend. Backends and engine ports register through the entry point groups `haylen.backends` and `haylen.engine_ports` of `pyproject.toml`, the ones of the repository like any other, and the registry matches them by what they describe, as the [guide to ports](../platform/ports.md) explains. The rules of the code of the package, and the tests that check them, are in the [testing guide](testing.md#the-tests-of-the-tooling).

## Requirements

Every machine needs uv, CMake 3.28 or newer, Ninja and a C++20 compiler. The command `haylen doctor` shows, for every target or for the one that `--target` names, its backend, the engine port that builds its engine, its installed SDKs and every tool it needs with its version, and says what is missing and how to get it. It downloads nothing and accepts no license.

```sh
haylen doctor
haylen doctor --target android
```

What each target needs on top of that:

| Targets | Host | Graphics APIs | Also install |
| --- | --- | --- | --- |
| `player` | macOS, Windows, Linux | Those of the host | What the desktop SDK of the host needs, below. |
| `macos`, `catalyst`, `ios`, `ios-simulator`, `tvos`, `tvos-simulator`, `visionos`, `visionos-simulator` | macOS | Metal | Xcode 27.1 or later with its Metal toolchain (`xcodebuild -downloadComponent MetalToolchain`), and the simulator runtime of a platform to run apps on it, such as `xcodebuild -downloadPlatform tvOS`. |
| `windows` | Windows | Direct3D 12, Direct3D 11, Vulkan, OpenGL | Visual Studio with the C++ workload, Git for Windows, whose `patch` applies the patches of the dependencies, and a Perl for Windows with NASM, with which OpenSSL configures and assembles its build, such as Strawberry Perl, whose `perl/bin` and `c/bin` folders go on `PATH`. Run the commands from a developer command prompt, so MSVC is on `PATH`. |
| `linux` | Linux | Vulkan, OpenGL | GCC or Clang and the development packages of X11, Xi, Xcursor, Xext, Xrandr, OpenGL, EGL, Wayland, xkbcommon, D-Bus, IBus and udev, such as `libx11-dev libxi-dev libxcursor-dev libxext-dev libxrandr-dev libgl1-mesa-dev libegl-dev libwayland-dev libxkbcommon-dev libdbus-1-dev libibus-1.0-dev libudev-dev` on Debian and Ubuntu. SDL builds its Wayland support, the input methods of IBus and Fcitx and the hotplug of controllers through udev with them. |
| `android` | any | Vulkan, OpenGL ES 3 | The Android SDK that `ANDROID_HOME` or `ANDROID_SDK_ROOT` names, with NDK `30.0.16248370`, platform 37.2 and platform-tools, and JDK 17 or newer. |
| `web` | any | WebGPU, WebGL2 | `git`, and a browser with WebGPU or WebGL2 to run it. |

A backend builds only on the hosts it declares, so the targets of macOS, Windows and Linux build on their own system, while Android and the web build from every host. Apple builds of the engine precompile the shaders of the renderer with the Metal toolchain, a component of Xcode that Xcode downloads apart, so the build of an Apple SDK stops with the command that installs it when it is missing.

### Tools that the tooling brings

The tooling downloads, or builds, the other tools the first time a command needs them, through the classes of `tools/haylen/toolchains` and, for the tools of the installers, of the desktop backend in `tools/haylen/platforms/desktop`. It keeps them in `.tools/` at the root of the repository. Each tool is pinned to one version in its class, and a new pin downloads its own copy. The downloads of the shader compiler, Gradle, XcodeGen, bundletool, Inno Setup and the AppImage tools are also pinned by their SHA-256 hashes. A download whose hash differs is deleted and stops the command. A zip unpacks into a folder next to its own, which moves into place only once it is complete, so a command that stops while it unpacks leaves no tool that looks installed.

| Tool | Version | Folder | Used by |
| --- | --- | --- | --- |
| `haylen-shaderc` | The release that `engine/cmake/haylen-shader-compiler.json` pins | `.tools/haylen-shaderc-<commit>` | Every build and the command `app shaders`, as [the shader compiler](#the-shader-compiler) explains. |
| Emscripten SDK | 7.0.0 | `.tools/emsdk` | The web SDK, the runtimes that web apps link and the checks of the web. |
| Gradle | 9.9.1 | `.tools/gradle-9.9.1` | The Android libraries of the Android SDK and the Android projects of apps. |
| XcodeGen | 2.46.0 | `.tools/xcodegen-2.46.0` | The Xcode projects that the Apple backend generates, and the runner of the UI tests. macOS only. |
| bundletool | 1.18.3 | `.tools/bundletool-1.18.3.jar` | The format `apks` of `app package` on Android, which installs a bundle in the local testing mode of Google Play. |
| Inno Setup | 7.1.1 | `.tools/inno-setup-7.1.1` | The Windows installer of `app package --format installer`, installed in portable mode, without an uninstaller or entries in the registry. Windows only. |
| appimagetool, the AppImage runtime and patchelf | 1.9.1, 20251108 and 0.19.3 | `.tools/appimage-1.9.1` | The AppImage of `app package --format installer`. Linux only. |

The Android SDK and NDK, Java, LLVM and Chrome are tools of the machine, which the tooling finds and never downloads: the SDK through `ANDROID_HOME` or `ANDROID_SDK_ROOT`, the pinned NDK inside it, Java through `JAVA_HOME` or `PATH`, the LLVM of fuzzing through `--llvm` or `HAYLEN_LLVM`, and the Chrome of the checks of the web through `HAYLEN_CHROME` or the browsers of the machine.

Emscripten comes from its repository through `git`. An SDK of another version fetches the newest list of releases and installs the pinned one. Every web build tree records the version of Emscripten that configured it. A tree of another version builds again from nothing, because Emscripten keeps no binary compatibility between versions.

### The shader compiler

The Haylen shader compiler, `haylen-shaderc`, compiles the shaders of the engine and of apps. It lives in its own repository, [haylen-shader-compiler](https://github.com/haylen-org/haylen-shader-compiler). The compiler keeps one version, `1.0.0`, with one release of that name, which every change of the compiler rebuilds at its one commit. The file `engine/cmake/haylen-shader-compiler.json` pins that release and the commit it was built from. It lists a prebuilt binary with its SHA-256 hash for each system and processor, and the source archive of the commit with its hash.

The script `engine/cmake/haylen-shader-compiler.cmake` provides the compiler from those pins, in the same way for the tooling and for CMake:

1. It names the machine by its system and processor, such as `macos-arm64` or `windows-x64`.
2. The release ships binaries for macOS on Apple silicon and Intel, Linux on x64 and ARM64 with glibc 2.28 or later, and Windows on x64, and on ARM64 from Windows 11. On those machines the script downloads the binary and checks its hash.
3. On any other machine, it downloads the source of the commit, checks its hash and builds the compiler with CMake on half of the processors.
4. It keeps the compiler in a folder named after the first seven characters of the commit. The compiler keeps one version and rebuilds its release at every change, so a new pin brings its own compiler under the same version.
5. Builds that configure at the same time wait for the first one, which moves the finished compiler into place.

The tooling runs it with `.tools/` as the folder of the compilers and hands the result to every build tree as `HAYLEN_SHADERC`. Set the environment variable `HAYLEN_SHADER_COMPILER` to a build of the compiler, and the tooling uses that one instead. A CMake project that adds the engine runs the same script when `HAYLEN_SHADERC` is empty, as the [CMake options](#cmake-options) describe.

```sh
HAYLEN_SHADER_COMPILER=~/haylen-shader-compiler/build/haylen-shaderc haylen engine build
```

## Engine SDKs

Apps never compile the engine. They build against an engine SDK, a prebuilt engine for some targets with a manifest that says exactly what it holds, and `haylen sdk build` compiles the engine for apps, which the app commands that build also run for an SDK they lack. It asks the engine port that builds the engine for the targets it names, finds the tools that port needs, and installs the SDK into `build/sdk/<port>-<configuration>`, such as `build/sdk/apple-release`.

```sh
haylen sdk build --target player
haylen sdk build --target ios --target ios-simulator
haylen sdk build --target android --architecture arm64-v8a
haylen sdk build --target web --config Debug
haylen sdk list
```

The option `--target` repeats, and every target it names must belong to one engine port, so the SDK of each port builds with a command of its own. The option `--config` is `Release` by default, `--architecture` limits the build to some architectures of the targets, and `--output` installs into another folder, such as one that the variable `HAYLEN_SDK_PATH` names for apps of other checkouts. The command `haylen sdk list` lists the installed SDKs with their port, configuration and slices, and whether the engine sources of this checkout built them.

### What an SDK holds

The engine port writes `haylen-sdk.json` into the folder of the SDK, which the schema `tools/haylen/schemas/sdk.schema.json` checks:

| Field | Meaning |
| --- | --- |
| `format` | The version of the format of the manifest, `1`. |
| `port`, `portVersion` | The engine port that built the SDK and its version. |
| `engineVersion` | The version of `engine/VERSION`. |
| `revision` | A hash of the engine sources that built the SDK, which changes with any file of `engine/` that reaches an SDK. The tests and the benchmarks never reach one, so a change to `engine/tests` or `engine/bench` keeps every SDK current. |
| `configuration` | `Debug` or `Release`. |
| `slices` | One slice per system and environment, each with its `system`, `environment` (`host`, `device` or `simulator`), `architectures`, `abi`, `toolchain`, the C++ `runtime` it links, its `minimumSystemVersion`, its `graphicsApis`, the `capabilities` of the system that the engine uses there, such as `dynamicLibraries`, and its `files` by name. |
| `hostTools` | The programs that run on the machine that builds apps, never on the target, each with its `name`, `system`, `architecture` and `path`, such as the content tool `haylen-content`. |
| `files` | The files that every slice shares, by name. |

A port keeps the slices of its folder that the same engine sources built in the same configuration, so building one slice later adds it to the SDK. An SDK of other sources or another configuration leaves the folder before the first new slice installs.

| Engine port | Targets | What its SDK holds |
| --- | --- | --- |
| `desktop` | `player`, `windows`, `linux` | One slice of this machine, such as `macos-arm64`, with the CMake package that `find_package(haylen)` imports, its four libraries, and the desktop player `haylen` and the content tool `haylen-content` as host tools. It builds only for the machine it runs on. |
| `apple` | `macos`, `catalyst`, `ios`, `ios-simulator`, `tvos`, `tvos-simulator`, `visionos`, `visionos-simulator` | `Haylen.xcframework` with the engine and the runtime of every slice in its `libhaylen.a`, `HaylenLua.xcframework` with the Lua layer and the Lua player of every slice in its `libhaylen-lua.a`, both signed ad hoc, the headers, the CMake package of native libraries, `haylen-apple.json` with the settings of Apple projects, the privacy manifest of the engine and the Swift helpers of the bridge. |
| `android` | `android` | One slice per ABI, with the CMake package that the native code of apps links and its four libraries under `abis/` and the player library `libhaylen.so` under `jniLibs/`, and the Android libraries of the engine in the local Maven repository `maven/`, whose `haylen` library carries the player of every ABI of the SDK. |
| `web` | `web` | The web player, which runs any package that its page hands it, with the processor of its audio output, the worker of its content store and the page side and the worker of the [Worker mode](#hosting-the-runtime), and the Emscripten CMake package with its four libraries, which release runtimes and C++ apps link. |

The library of the engine of every SDK holds nothing of Lua, and the Lua layer is a library of its own, which only the apps that run Lua link. The SDKs of CMake packages, the desktop, Android and web SDKs, hold four libraries, which `haylen-sdk.json` lists under the `libraries` of each slice with their files:

| Library | What it holds | Who links it |
| --- | --- | --- |
| `haylen` | The engine with its dependencies, imported as `haylen::engine`. | Every app. |
| `haylen-runtime` | The runtime with the platform services, the platform layer and the GPU devices, imported as `haylen::runtime`. | Every app. |
| `haylen-lua` | The Lua layer with the Lua runtime of Varn, imported as `haylen::lua`. | Lua apps, through the player, and C++ apps whose `cpp.lua` is `true`. |
| `haylen-player` | The object of the Lua player, which defines the application of a Lua app, imported as `haylen::player`. | Lua apps. |

The Apple slices list two libraries, since they merge the runtime into `haylen`, the `libhaylen.a` of `Haylen.xcframework`, and the Lua player into `haylen-lua`, the `libhaylen-lua.a` of `HaylenLua.xcframework`. The player is a member of its own in that library, and a link takes it only when nothing else defines `haylen::core::Application::create`. So a C++ app that runs Lua with an application of its own links the same framework as a Lua app.

Each target finds its slice after the requirements of its plugins are known and before its project is generated. A slice serves a target only when its system, environment, architecture, binary interface, toolchain and configuration match, the current engine sources built it, it runs on the oldest system that the app supports and it holds the graphics APIs that the app asks for. When no installed SDK of the current engine sources holds an architecture of the target in the configuration of the engine, the commands that prepare or build the app first build its SDK through the engine port, as `haylen sdk build` does. That step says why and how long a first build takes: about ten minutes with a warm compiler cache and up to an hour without one, while a build of changed sources compiles only what changed. The commands that only read a product, `project inspect` and `app verify`, never build. They stop with the error `incompatibleSdk`, which names the `haylen sdk build` command to run. Every command stops the same way for an SDK of the current sources that serves no app of the target, such as one that runs only on newer systems than the app. The command `app build` takes the SDK of `--engine-config`, `Release` by default.

A host tool of another system or architecture never runs. Protected releases take the content tool of the desktop SDK of this machine, whose engine port declares that its SDK carries it in the `host_tools` of its `PortDescription`. When no installed SDK of the current engine sources holds the tool, the commands that prepare or build a Release of any target first build that SDK in the configuration of the engine. They do it in a step of its own that says why, as they build the SDK of the target. The command `app verify` and the commands of `haylen content` stop with the `haylen sdk build` command to run instead, such as `haylen sdk build --target player` on macOS.

## Build trees

The tooling keeps every build tree of the engine under `build/engine/`, apart from the trees of apps:

| Tree | What builds there |
| --- | --- |
| `build/engine/<host>-<config>` | The engine of this machine with its tests, benchmarks, tools and players, such as `build/engine/macos-debug`, which [engine configure](#engine-configure) and `haylen test` use. A tree with sanitizers or the Xcode generator gets its own, such as `macos-debug-thread` or `macos-debug-xcode`. |
| `build/engine/<host>-port-<config>` | The engine with the test port, which `haylen test --port` builds, with a tree of its own for each sanitizer, such as `<host>-port-<config>-thread`. |
| `build/engine/coverage`, `build/engine/fuzz` | The trees of coverage and fuzzing. |
| `build/engine/web-gpu-tests-<config>`, `build/engine/android-gpu-tests-<abi>-<config>`, `build/engine/<platform>-gpu-tests-<config>` | The GPU tests of the web, of Android and of the Apple platforms. |
| `build/engine/<platform>-ui-tests`, `build/engine/android-ui-tests` | The runner of the UI tests on an Apple simulator or device, and the test APK of the UI tests on Android. |
| `build/engine/consumers/<mode>-<config>` | The consumer project of [engine consumers](#engine-consumers). |
| `build/engine/desktop-<config>`, `build/engine/web-<config>`, `build/engine/apple-<slice>-<architecture>-<config>`, `build/engine/android-<abi>-<config>` | The trees of the engine ports, which `haylen sdk build` reuses. The Apple port joins the architectures of a slice in `build/engine/apple-<slice>-<config>`, and the Android port builds its Android libraries with Gradle in `build/engine/android-libraries-<config>`. |

The `haylen` player and every app of [`haylen_add_app`](#haylen_add_app) land in `bin/<target>/` inside a tree, and the other executables, such as the tests, the headless player, the content tool and the benchmarks, land in `bin/`. A tree configures again when CMake never configured it, its last configure failed, or another pin of the shader compiler or another uv configured it, so its shaders and its tests never use an old tool. CMake cannot change the generator of a tree, which is why the Xcode generator gets a tree of its own.

The other folders of `build/` belong to apps, SDKs and logs:

| Folder | What it holds |
| --- | --- |
| `build/sdk/<port>-<config>/` | The SDKs that [engine SDKs](#engine-sdks) describe. |
| `build/apps/<app>-<hash>/` | Everything that the tooling builds for an app: one folder per product, such as `ios-simulator-debug`, with its generated project, its build trees, its products and its staged package, as the [distribution guide](distribution.md) explains. |
| `build/plugin-repositories/` | The checkouts of the plugins that apps take from git repositories, at the commits that `haylen.lock` pins. |
| `build/tests/` | The copy of the app of the memory check of the web. |
| `build/logs/` | The log of each command. |

## Engine commands

The group `engine` builds the engine of this machine for developing it. Apps never build against these trees. The commands `engine fuzz`, `engine coverage` and `engine format` are in the [testing guide](testing.md), and every option of every command is in the [command reference](commands.md).

### engine configure

```sh
haylen engine configure
haylen engine configure --config Release --sanitizers address
haylen engine configure --xcode
```

Configures the tree of this machine, with `--config` (`Debug`, `Release` or `RelWithDebInfo`, `Debug` by default), `--sanitizers` (`address` or `thread`) and `--xcode`, which uses the Xcode generator on macOS for debugging the engine in Xcode. Every tree receives `HAYLEN_SHADERC`, `CMAKE_BUILD_TYPE`, `HAYLEN_SANITIZERS` and `HAYLEN_UV`, the uv that the engine tests run the tooling with, and a macOS tree targets macOS 13.3 through `CMAKE_OSX_DEPLOYMENT_TARGET`.

Every build carries the GPU device of each graphics API of its platform, which `HAYLEN_GRAPHICS_APIS` lists, and the app chooses one when it runs, as the [RHI guide](../graphics/rhi.md#choosing-the-graphics-api) describes.

### engine build

```sh
haylen engine build
haylen engine build --cmake-target haylen-headless --cmake-target haylen-native-test
haylen engine build --config Release --cmake-target haylen
```

Configures the tree when it needs it and runs `cmake --build`. It takes the options of `engine configure`, and `--cmake-target`, which repeats, builds some CMake targets alone. Without it, it builds everything the tree defines: the engine, the `haylen` player, the headless player, the content tool, the tests and the benchmarks. Useful targets are `haylen`, `haylen-headless`, `haylen-tests`, `haylen-gpu-tests`, `haylen-native-test` and `haylen-sprite-benchmark`.

### engine bench

```sh
haylen engine bench
haylen engine bench --suite physics
haylen engine bench --suite lua --jobs 4
haylen engine bench --suite render --graphics-api vulkan
haylen engine bench --suite render --gpu
```

Builds a benchmark in the Release configuration for this machine and runs it. The option `--suite` picks it, `sprites` by default, and `--jobs` sets the jobs of the build. Every benchmark is a target of the engine, which `HAYLEN_BUILD_BENCHMARKS` adds. The guide of each subject shows the recorded results.

The benchmarks that draw, `sprites`, `render`, `ui`, `particles`, `nodes`, `animation` and `physics`, take `--graphics-api` with a name of `gpu::Device::kApiNames` to draw through the device of that API, and `--gpu` to run once on every API that the build compiled, skipping the ones that cannot start on the machine. The [benchmarks guide](benchmarks.md) explains how they draw on a GPU and what their numbers mean.

| Suite | Target | What it measures | Results |
| --- | --- | --- | --- |
| `sprites` | `haylen-sprite-benchmark` | Sprites drawn on the GPU of this machine, in phases. | [Rendering guide](../graphics/rendering.md#sprite-benchmark) |
| `core` | `haylen-core-benchmark` | On the headless host, the average time of: a frame of an app at rest and with a Lua scene, events and signals with C++ and Lua listeners, owners that end what they hold, timers that wait and timers that fire every frame, tweens of native values and of Lua tables with and without an owner, jobs that go to a worker and back, `parallelFor`, and JSON assets loaded in the background and on the frame thread. | [Architecture guide](../core/architecture.md#what-the-base-costs) |
| `algorithms` | `haylen-algorithm-benchmark` | The average time of: A\*, weighted A\*, jump point search, flow fields and hierarchical path finding on a 512 by 512 grid, navigation mesh builds and funnel paths, ORCA crowd steps on one thread, on the job system and on the job system while navigation mesh builds keep every worker busy, with the slowest of those steps, AABB and k-d tree queries, batches of physics ray casts on one thread and on the job system, and the batches of matrices, points and frustum culling of `haylen.math` next to the same work done one value at a time, with the vector instructions they ran on. | The output |
| `procedural` | `haylen-procedural-benchmark` | The average time of: wave function collapse, Poisson disk sampling with a fixed and a varying distance, random scattering, marching squares, the Delaunay triangulation of 100 thousand points and carving destructible terrain. | The output |
| `physics` | `haylen-physics-benchmark` | On the headless host, the average time of: the step of a pile of falling boxes on one, two, four and eight threads, a settled pile with sleeping on and off, a pyramid on one and four threads and with two, four and eight sub-steps, a chain of 300 links, ragdolls that tumble down stairs, fluids of three sizes with the fluid pass alone and the whole step, ray casts and circle queries in a pile, reading the transforms of 5000 bodies with and without interpolation, the debug drawing of a pile, and a pile stepped from Lua with and without its contact and hit callbacks. | [Physics guide](../2d/physics.md#the-physics-benchmark) |
| `ui` | `haylen-ui-benchmark` | On the headless host, collections of a hundred thousand items of three types, flung in a list and in a grid: the CPU time of a frame with the items bound, the binder calls and the cells created while flinging and at rest. | [UI guide](../ui/ui.md#what-a-long-list-costs) |
| `render` | `haylen-render-benchmark` | On the headless host, whole frames of sprites, shapes, text, rich text and interface components, with the draw calls, instances, uploads and allocations of each frame. | [Rendering guide](../graphics/rendering.md#render-benchmark) |
| `particles` | `haylen-particle-benchmark` | On the headless host, the average time of: one emitter of 100 thousand particles on one thread and on the job system, a thousand emitters of a hundred particles, spawning 200 thousand particles per second, the draw calls of both kinds of emitter, and 500 emitters updated from Lua. | [Particles guide](../2d/particles.md#the-particle-benchmark) |
| `content` | `haylen-content-benchmark` | An app of 64 MiB of data that does not compress, 16 MiB of text, 2000 small files, a Lua module of 2 MiB, a JSON map and a picture, written to the temporary folder of the system. It times building the protected release, opening it, reading the large files in order and on four threads, reading the data in order from the app folder too, 2000 random reads of 4 KiB, and finding every small file in the catalog. It times compressing, decompressing, encrypting and decrypting chunks alone. It loads the module from its text in the app folder and as bytecode from the release, and the map, the picture and its raw pixels from the folder and from the release, so the pairs show what protection adds. It also loads the map cooked as CBOR, builds an update of two changed files with the bytes it stores and reuses, plans the download ranges of 100 thousand records, adds 256 records to a local store, and builds, opens and reads a release of 20 thousand files. Each line shows the throughput or the time, with the largest single allocation, so a reader that starts to hold whole files shows. | [Content guide](../content/content.md#performance) |
| `nodes` | `haylen-node-benchmark` | On the headless host, the update of a tree of 100 thousand sprite nodes after every root moved and after one root in a hundred moved, on one thread and on the job system, and its draw with every node in view and with most of them culled. | [`haylen.nodes2d`](../2d/nodes.md#performance) |
| `animation` | `haylen-animation-benchmark` | On the headless host, an animated batch of 100 thousand sprites on one thread and on the job system, and a crowd of 2000 units animated from Lua with an animator each and with one animated batch. | [`haylen.animation2d`](../2d/animation.md#performance) |
| `lua` | `haylen-lua-benchmark` | Runs the Lua sprite benchmark of `engine/bench/lua-benchmark` on the headless host. It prints how long Lua takes to update and draw 10 thousand, 100 thousand and a million sprites kept in tables, drawn in one batch or with a call each, in a float buffer and in a sprite batch. | [Lua guide](../core/lua.md#performance) |

### engine tools

```sh
haylen engine tools
haylen engine tools --emscripten
```

Installs the pinned shader compiler into `.tools`, or builds it where its release has no binary for the machine, and with `--emscripten` the pinned Emscripten too. The command prints where each tool lives. Every other command fetches what it needs by itself, so use `engine tools` to prepare a machine or the cache of a CI job.

### engine consumers

```sh
haylen engine consumers
haylen engine consumers --mode subdirectory --mode cpm
haylen engine consumers --mode package --sdk build/sdk/desktop-release
```

Builds `engine/tests/consumer`, a CMake project of its own with a small C++ app, which adds the engine the way another repository does, as [checking the three ways](embedding.md#checking-the-three-ways) explains.

### engine clean

```sh
haylen engine clean
```

Removes the whole `build/` folder: every build tree of the engine, every SDK, the build folders of apps and the logs. It keeps `.tools/` and the dependency cache in `.cache/cpm`. A file that stays, such as one that a running app or an editor holds open on Windows, stops the command with its path. Every other command that makes a folder again, such as a staging folder, a generated project or an installed copy, stops the same way, so no step builds on what the folder held before.

## CMake options

The tooling sets these cache variables for you. A project that adds the engine itself sets them directly.

| Option | Default | Meaning |
| --- | --- | --- |
| `HAYLEN_SHADERC` | empty | The path of a build of `haylen-shaderc`, which compiles the shaders of the engine. Empty, the configure provides the pinned compiler as [the shader compiler](#the-shader-compiler) explains, and configures again when the pins change. |
| `HAYLEN_SHADERC_CACHE` | the CPM source cache, or `tools` of the build tree without one | The folder that keeps the compilers that the configure provides, so build trees share them. |
| `HAYLEN_GRAPHICS_APIS` | every API of the platform | The graphics APIs whose GPU devices the static library `haylen-gpu` compiles, as a CMake list of the names of `gpu::Device::kApiNames`. The default is `metal` on Apple platforms, `direct3D12;direct3D11;vulkan;openGl` on Windows, `vulkan;openGl` on Linux, `vulkan;openGlEs` on Android and `webGpu;webGl2` on the web, and a name of another platform is an error. The engine includes the `device.cmake` of the folder of each device under `engine/src/gpu`, and a name whose folder holds no `device.cmake` stops the configuration. The class `gpu::DeviceFactory` makes the devices in the default order of the platforms, whatever the order of the list, and the shaders of the engine carry the shader languages of the APIs of the list. |
| `HAYLEN_PORT_DIR` | empty | The folder of a [port](../platform/ports.md) of the engine to a platform that the repository does not hold, outside the repository. Its `port.cmake` describes the platform, the runtime that replaces the platform layer and the services, and the GPU device of the API `port` with its shader tool. The engine then builds for the platform of the port, and `HAYLEN_GRAPHICS_APIS` defaults to `port` and the APIs of the engine that the port names. |
| `HAYLEN_BUILD_PLAYER` | on when the engine is the top-level project | Builds the `haylen` player: the executable on desktop, on the web and on the platform of a port, and the shared library of `HaylenActivity` on Android. |
| `HAYLEN_BUILD_TESTS` | on when the engine is the top-level project | Builds the tests on desktop, and the GPU tests alone on the web, Android, iOS and tvOS. |
| `HAYLEN_BUILD_BENCHMARKS` | on when the engine is the top-level project | Builds every benchmark of [`engine bench`](#engine-bench) on desktop. |
| `HAYLEN_BUILD_TOOLS` | on when the engine is the top-level project | On desktop hosts, builds the content tool `haylen-content` and its library `haylen-content-tool`. The tests link that library, so they build it too. |
| `HAYLEN_BUILD_SDK` | off | Merges the engine into the SDK that `cmake --install --component haylen-sdk` installs, which the engine ports turn on. |
| `HAYLEN_BUILD_FRAMEWORK` | off | On Apple platforms, with `HAYLEN_BUILD_SDK`, merges the engine and the runtime into the `libhaylen.a` of one slice of `Haylen.xcframework` and the Lua layer with the Lua player into the `libhaylen-lua.a` of `HaylenLua.xcframework`, which the `haylen-framework` component installs with `haylen-apple.json` and the CMake package of native libraries. |
| `HAYLEN_ENABLE_COVERAGE` | off | Instruments the engine and the tests with LLVM coverage. It needs Clang. |
| `HAYLEN_SANITIZERS` | `OFF` | The sanitizers of builds for this machine that do not use MSVC, a desktop or a port that builds with the compiler of this machine, such as the test port, and any other build stops with a sentence: `OFF`, `ADDRESS` for AddressSanitizer and UndefinedBehaviorSanitizer on the targets of the engine and AddressSanitizer on the libraries it compiles from the sources of the dependencies that read the bytes of apps, or `THREAD` for ThreadSanitizer on every C and C++ target of the build, dependencies included. |
| `HAYLEN_DEBUG_INFO` | `FULL`, or the variable `HAYLEN_DEBUG_INFO` of the environment | The debug information of the Debug and RelWithDebInfo configurations: `FULL` for a debugger, or `LINES` for the file and line of each instruction alone, which stack traces and sanitizer reports read, in objects about a third of the size. GCC takes `-g1` for it and Clang `-gline-tables-only`. MSVC has no format with lines alone, so its objects carry no debug information with `LINES`. The [CI workflow](testing.md#the-compiler-cache) sets `LINES`. |
| `HAYLEN_WARNINGS_AS_ERRORS` | off, or the variable `HAYLEN_WARNINGS_AS_ERRORS` of the environment | Fails the build of the targets of the engine on any compiler warning, through the `COMPILE_WARNING_AS_ERROR` property of CMake, which `haylen_enable_warnings` sets on each of them. The code of the dependencies never fails the build, as [warnings of dependencies](#warnings-of-dependencies) explains. The [CI workflow](testing.md#continuous-integration) turns it on for every build. |
| `HAYLEN_BUILD_FUZZERS` | off | Builds the fuzzers `haylen-bitmap-font-fuzzer`, `haylen-content-fuzzer`, `haylen-font-fuzzer`, `haylen-image-fuzzer`, `haylen-input-fuzzer`, `haylen-messagepack-fuzzer`, `haylen-ogg-fuzzer`, `haylen-placeholder-fuzzer` and `haylen-vector-image-fuzzer`, and instruments every target for libFuzzer and AddressSanitizer. It needs a Clang that links libFuzzer, which Apple Clang does not ship. |

The workspace `CMakeLists.txt` at the root of the repository is the project `haylen-workspace`. It adds `engine/` and turns the player, the tests, the benchmarks and the content tool on. The function [`haylen_add_app`](#haylen_add_app) builds the binaries of apps.

### Names of targets and files

Every CMake target, project and install component uses dashes, such as `haylen-engine`, `haylen-runtime`, `haylen-tests` and `haylen-sdk`. So does every library, executable and other file that the build makes, such as `libhaylen-app.so`. An underscore appears only where a name must be an identifier, such as the CMake function `haylen_add_app` and the variable `HAYLEN_PLATFORM`. Projects reach the libraries through the aliases `haylen::engine` and `haylen::runtime`, as the [embedding guide](embedding.md#what-a-project-gets) lists.

### Settings of every build

The file `engine/CMakeLists.txt` sets two things for the engine and its dependencies in every configuration:

- The targets compile without the scan for C++ modules, through `CMAKE_CXX_SCAN_FOR_MODULES`, since neither the engine nor its dependencies use modules. So each source compiles in one step, without a second pass of the preprocessor before it.
- MSVC writes the debug information of Debug and RelWithDebInfo builds into each object, through `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT`, instead of one PDB file that every compile of a target writes. So parallel compiles never wait for that file, and a compiler cache can keep the objects. The linker still writes the PDB of each program.

The tooling configures the CMake trees of apps the same way: the generated projects of the desktops, the web and Android, and the native libraries of apps and plugins. They compile without the scan for C++ modules too, so app code and native libraries do not use modules. With the scan on, CMake 4.5 and newer give every target the module `std` of the standard library that it finds. For the compiler of the Android NDK, that is the library of the host, which the NDK cannot compile.

A compiler cache, such as ccache, speeds up builds that compile the same sources again. CMake runs every compile through the launcher that the environment variables `CMAKE_C_COMPILER_LAUNCHER`, `CMAKE_CXX_COMPILER_LAUNCHER`, `CMAKE_OBJC_COMPILER_LAUNCHER` and `CMAKE_OBJCXX_COMPILER_LAUNCHER` name when it configures a build tree, so the commands of `haylen` use a cache once they name it:

```sh
export CMAKE_C_COMPILER_LAUNCHER=ccache CMAKE_CXX_COMPILER_LAUNCHER=ccache CMAKE_OBJC_COMPILER_LAUNCHER=ccache CMAKE_OBJCXX_COMPILER_LAUNCHER=ccache
haylen test
```

### Settings of release binaries

The file `engine/cmake/haylen-release.cmake` sets how every target of the engine and its dependencies compiles:

- Clang and GCC map the source folder of the engine, its build folder and the CPM cache to `engine/`, `build/` and `deps/` in the file names that `__FILE__` writes into binaries, such as those that assertions keep. So no binary names a folder of the machine that built it. MSVC has no such mapping, so a Windows build keeps the full paths of the files whose assertions it compiles.
- Clang and GCC place every function and object in a section of its own, which `--gc-sections` drops when nothing calls it. Apple platforms skip this, because their linker strips dead code by atom.
- On Apple platforms the engine and the dependencies that CMake builds compile with hidden symbols, since an Apple executable exports every global symbol it holds. Varn hands that visibility to OpenSSL, which builds with its own build system, and turns off the attribute with which Poco marks the classes of its API for export, so neither keeps a visible symbol but the few routines that OpenSSL writes in assembly, such as `OPENSSL_cleanse` and `CRYPTO_memcmp`, which mark themselves global. The build of every slice of the Apple SDK checks this on its merged `libhaylen.a` and stops with the symbols it found when an object of OpenSSL that the compiler made, or anything of Poco, keeps a visible symbol. It tells the objects of the assembly of OpenSSL by their Mach-O header, which lacks the flag `SUBSECTIONS_VIA_SYMBOLS` that the compiler gives every object it makes. Only the instantiations of templates that the C++ library marks visible keep their symbols visible. Hidden symbols leave the exports of an app but stay global inside its link, so a static library that defines the same symbols, such as another copy of OpenSSL, does not link next to the engine. The code of an app keeps the default visibility, and the reference of [`native.findSymbol`](../lua-api/native.md#nativefindsymbolname) says what a search finds of an app on each platform and in each configuration.
- A Release build compiles with link-time optimization wherever the compiler supports it, through `CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE`. That is thin LTO with Clang on Apple platforms, Android and the web, LTO with GCC on Linux and whole program optimization with MSVC on Windows.

With link-time optimization, the libraries of the SDKs hold the intermediate code of the compiler. The link of each app then optimizes the engine, its dependencies and the app as one program, and drops what the app never reaches. A few cases differ:

- The SDK of Windows compiles to machine code. The intermediate code of MSVC would make its one library larger than the 4 GB that a library of Windows holds. A project that compiles the engine from its sources with MSVC keeps whole program optimization.
- GCC and MSVC link that code only when the link asks for it. So `haylen::engine` carries `-flto=auto` or `/LTCG /INCREMENTAL:NO` to every program that links it, even a Debug app that links the prebuilt SDK. Clang links it on its own.
- An SDK holds code for the compiler that built it, so an app links it with the same compiler. Each slice records its `toolchain`, and the SDK of a target serves only the toolchain of that target.
- The slices of `Haylen.xcframework` hold machine code. Both `xcodebuild -create-xcframework` and `lipo` refuse libraries of LLVM bitcode, since Xcode stopped taking bitcode. So every app on Apple platforms, Lua or C++, links the engine with dead code stripping alone.
- Zstandard passes an option to the assembler, which the link of GCC drops from code compiled for link-time optimization with a warning. So Zstandard builds as machine code.

### Shaders of the engine

The shaders of the engine live in `engine/shaders`, in the Haylen shader language. The function `haylen_compile_shader` of `engine/cmake/haylen-shaders.cmake` compiles them with `haylen-shaderc` at build time into `.shader` packages, which the engine embeds. A package holds the code of the shader languages of every API of `HAYLEN_GRAPHICS_APIS`, which `engine/cmake/haylen-gpu.cmake` lists in `HAYLEN_SHADER_LANGUAGES`:

| Graphics API | Shader languages |
| --- | --- |
| Metal | The three Metal languages, for macOS, iOS and the iOS simulator. |
| Direct3D 11 and 12 | HLSL 5. |
| Vulkan | SPIR-V. |
| OpenGL core | GLSL 4.30. |
| OpenGL ES 3 and WebGL2 | GLSL 3.00 ES. |
| WebGPU | WGSL. |
| The API `port` of a port | The languages that its `SHADER_LANGUAGES` names, and the `port` target that its `SHADER_COMMAND` writes into each package after `haylen-shaderc`, as the [guide to ports](../platform/ports.md#shaders) explains. |

The programs that draw into lit canvases compile a second time with `HAYLEN_LIT`. Every compile includes `engine/shaders/include`, the shader library that the shaders of apps include too, as the [shader guide](../graphics/shaders.md) explains.

Apple builds also precompile the Metal code of these packages. The function `haylen_precompile_metal_libraries` of `engine/cmake/haylen-metal-libraries.cmake` compiles each Metal source of the language of the build with `xcrun metal` into a Metal library, for the kind of device of the build at its minimum system version, and on iOS, tvOS and visionOS also for the simulator. The libraries go into `haylen-gpu`, keyed by the SHA-256 of their source, and the Metal device loads the programs of the renderer from them, as [load time](../graphics/shaders.md#load-time) explains. A shader package that changes compiles its libraries again with it.

## Dependencies

Dependencies are declared with [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) in `engine/cmake/haylen-dependencies.cmake`. The file `engine/cmake/cpm.cmake` downloads CPM 0.43.2 and checks its SHA-256 hash. CPM keeps the sources in `.cache/cpm` under the top-level source folder, so several build trees share one download. Set `CPM_SOURCE_CACHE`, as a CMake variable or an environment variable, to keep them elsewhere.

A patched package, such as SDL, keys its place in the cache with `haylen_patched_package_key`. The key is a hash of its archive and of the contents of its patches. So an edited patch downloads and patches the package again in every checkout, while checkouts in other folders share the same package. The patches are configure dependencies of the build tree, so the next build after a patch changes configures again and fetches the package under its new key.

| Package | Version | Notes |
| --- | --- | --- |
| nlohmann/json | 3.12.0 | Added first, so Varn reuses it and the build holds one copy. |
| Varn | commit `02d9e45` | The scheduler without Lua, with the event loop, the worker pools, the log, data views, the HTTP client and TLS in the library `varn-runtime`, and the Lua runtime on it with the modules `async`, `http`, `socket`, `json`, `fs`, `zip`, `crypto` and the others in the library `varn-core`. `VARN_TARGET` is `cli` on desktop and Apple platforms, `android` on Android, `wasm` on the web and `port` for a port, which builds the drivers that the `VARN_DRIVERS` of its `haylen_port` names with the event loop `PUMP` unless it names another, sets `VARN_THREADS` from its `THREADS`, so the worker pools keep their threads under the pumped loop, and turns `VARN_FFI_CLOSURES` off for a port that does not allow `executableMemory`, so the module `ffi` makes no callback there. The `cli` target builds the two static libraries, and the engine links `varn-core`, which links `varn-runtime`. Varn picks the HTTP client of each platform itself: the Apple one on iOS, tvOS and Mac Catalyst, the Android one on Android and `fetch` on the web. Lua, libuv, zlib, libzip, Poco and OpenSSL come in through Varn, each pinned with its hash, and OpenSSL, which compiles with its own build system, takes the compiler launcher of the build, so a compiler cache keeps its objects too: in its configure on most platforms, through a target that inherits the configuration of Android, whose compiler that configuration derives, and on the command line of `nmake` under MSVC, whose makefile quotes the compiler as one program. ThreadSanitizer builds tell Varn to keep the yields of Lua as exceptions, because the sanitizer cannot follow the `_longjmp` of a yield. Poco forces the library suffixes of its own naming into the CMake cache, such as `md` for the release libraries of MSVC, and Varn gives the cache back the suffixes it had once Poco is configured, so every later library, such as the native libraries that `native.load` finds by name, keeps the name of its target. |
| SDL | 3.4.20 | zlib license. The release archive of SDL 3, with the [patches](#patches) of `engine/cmake/patches`. It builds as the static library `SDL3-static` with only its video, event and gamepad subsystems, since the engine plays sound through miniaudio and draws through its own devices. It opens the window, runs the frames and reads the input and the controllers on every platform but Android and the platforms of ports, which bring their own platform layer, and UIKit builds leave its gamepads out, since the engine reads the controllers of UIKit through GameController. Linux builds keep X11, Wayland and udev, whose libraries SDL loads when the app starts. |
| GameActivity | 4.4.2 | Android only, Apache 2.0. The AAR of `androidx.games:games-activity` from Google Maven, pinned by its hash. Its prefab folder holds the static library and the headers of the native side of GameActivity for every ABI, which the runtime links. The runtime defines the entry `GameActivity_onCreate` itself, so the runtime library of the Android SDK leaves out the native app glue of that static library, which defines the entry again for apps of `android_main`, and its merge fails unless exactly one member defines the entry. |
| stb | commit `d14d46f` | Headers only. The engine compiles `stb_image` and `stb_image_write` alone, which decode and encode images, and none of its readers of fonts. |
| NanoSVG | commit `239e102` | Headers only. It reads SVG documents into curves and rasterizes vector images. It publishes no releases, so it is pinned to a commit of its default branch. |
| msdfgen | v1.13 | Only its core, which builds the distance fields of font glyphs from their whole outlines, the cubic curves of CFF fonts included. |
| HarfBuzz | 15.0.0 | Old MIT license. Its amalgamated source compiles as `haylen-harfbuzz` with `HB_MINI`, the OpenType shaper without the AAT and legacy shapers, together with its rasterizers of outlines and of color glyphs. It reads every font file, which it checks before it uses any of its tables, for the distance fields and for Dear ImGui. The definitions `HB_NO_BITMAP` and `HB_NO_SVG` leave out the readers of bitmap and SVG glyphs, which the engine does not draw. |
| SheenBidi | v3.0.0 | Apache 2.0. The Unicode Bidirectional Algorithm and script runs, compiled from its unity source as `haylen-sheenbidi`. |
| libunibreak | 8.0 | zlib license. Line breaks and grapheme clusters of Unicode 17, compiled as `haylen-unibreak`. |
| BudouX | v0.10.2 | Apache 2.0. Only its Thai phrase model, a JSON file that the engine embeds to break Thai lines. |
| fast_float | v8.3.3 | Headers only. It parses floating point numbers on every platform, since `std::from_chars` for floating point needs iOS, tvOS and macOS 26. |
| Dear ImGui | v1.93.0 | Compiled as `haylen-imgui` with the ImGui configuration of the engine, which leaves out its own reader of fonts with `IMGUI_DISABLE_STB_TRUETYPE`, since the engine hands ImGui a font loader that reads through HarfBuzz. |
| miniaudio | 0.11.26 | Compiled as `haylen-miniaudio`. |
| Box2D | v3.2.0 | With the [patches](#patches) of `engine/cmake/patches`. It picks its SIMD instructions when the app starts, AVX2 where the processor has it and SSE2 otherwise on x64, Neon on 64-bit ARM, plain math on 32-bit ARM and the SIMD of WebAssembly on the web. |
| Clipper2 | 2.0.1 | The polygon booleans, offsets and constrained triangulations of `math::Polygon`. A patch includes the header `iterator` in its core header, which uses `std::back_inserter` and which the standard library of Emscripten does not bring in through another header. |
| zstd | v1.5.7 | A static library for compressed Tiled layers and for the chunks of protected content. |
| Monocypher | 4.0.3 | BSD 2-clause or CC0. The authenticated encryption (XChaCha20-Poly1305), the hashes and key derivation (BLAKE2b) and the manifest signatures (Ed25519) of protected content, compiled from its two sources as `haylen-monocypher`. It is audited, has no dependencies and builds the same on every target, the web included. This release fixes a timing leak of its signatures. |
| Vulkan-Headers | v1.4.367 | Headers only, when `HAYLEN_GRAPHICS_APIS` holds `vulkan`. The Vulkan device compiles against them, so its build needs no Vulkan SDK. |
| volk | 1.4.350 | When `HAYLEN_GRAPHICS_APIS` holds `vulkan`, compiled as `haylen-volk`. It opens the Vulkan loader of the system when the app starts. |
| Vulkan Memory Allocator | v3.5.0 | When `HAYLEN_GRAPHICS_APIS` holds `vulkan`, compiled as `haylen-vulkan-memory-allocator`. It allocates the memory of the Vulkan device and finds the functions of Vulkan through volk. |
| GoogleTest | v1.18.0 | Only when the tests build, on desktop or for the GPU tests of the web, Android, iOS and tvOS. |

Every package is pinned by the URL of a release archive, or of a commit archive for a project without releases, with its hash:

```cmake
CPMAddPackage(
  NAME clipper2
  URL "https://github.com/AngusJohnson/Clipper2/archive/refs/tags/Clipper2_2.0.1.tar.gz"
  URL_HASH SHA256=2a3693aceab4aed3e39b743e038d87701acc53cf05ed7b2013aab3e0aec5287e
  SOURCE_SUBDIR CPP
  OPTIONS "CLIPPER2_UTILS OFF" "CLIPPER2_EXAMPLES OFF" "CLIPPER2_TESTS OFF" "CLIPPER2_USINGZ OFF"
  SYSTEM YES
)
```

To add or update a package:

1. Point `URL` at the archive of its latest release. Varn is pinned by the archive of one commit, whose contents never change.
2. Compute the hash of that archive, such as with `curl -L <url> | shasum -a 256`.
3. Adopt the new API everywhere the engine uses it.

A package that Varn also uses goes before the Varn block, so the whole build resolves it once. Header-only sources use `DOWNLOAD_ONLY YES` and an interface library, like `haylen-stb`.

Two details of the packages that come through Varn need care:

- Varn builds OpenSSL with its own `make` as one step of the build. That step runs the jobs that `CMAKE_BUILD_PARALLEL_LEVEL` named when the tree was configured, or one job without it. The tooling removes the variable, so a build never runs more jobs than `--jobs` gives it.
- The headers of Poco ask MSVC to link every Poco library by its file name, such as `PocoFoundationmd.lib`. The SDK merges those libraries into `haylen.lib`, so an app that links the SDK would look for files that do not exist. The engine defines `POCO_NO_AUTOMATIC_LIBS` for Poco and every target that uses it, since CMake links the Poco libraries by their targets.

Three more notes hold for miniaudio. It manages the audio session on iOS and tvOS with Objective-C, so its implementation file compiles as Objective-C there. The Vorbis decoder in its `extras` folder, which its implementation file compiles, also decodes streamed Ogg Vorbis sounds: the engine pushes the pages of the file into it and keeps its own index of them to seek, as the [audio guide](../audio/audio.md#seeking-in-streamed-sounds) explains. On Android the engine builds it with AAudio as its only backend, which miniaudio uses from Android 8.1 (API 27) on, because the first AAudio release of Android 8.0 has known faults. That is why the Android libraries of the engine need API 27.

### Warnings of dependencies

The engine builds without warnings, and the code of a dependency is the one exception, since only that dependency can act on it. Its warnings are ignored where they appear and nowhere else, so no warning of engine code is ever covered:

- The targets of a dependency that bring warning flags of their own, those of Varn, SDL, Box2D, Clipper2, Zstandard and googletest, build through `haylen_disable_warnings`, which drops those flags from the options and the flags of the target and turns warnings off for that target alone.
- The headers of every dependency reach the targets of the engine as system headers, through the `SYSTEM` of CMake, which Clang and GCC read as `-isystem` and MSVC as `/external:I` with `/external:W0`.
- A warning that the code of a dependency still raises, at the link or in the code generator, is turned off by the narrowest flag of that link or around that code, as the table lists.

| Dependency | Warning | Where it is off and why |
| --- | --- | --- |
| Lua | `-Wstringop-overflow` and `-Wstringop-overread` of GCC | The link with link-time optimization of every program that links `haylen::engine`, through `-Wno-stringop-overflow` and `-Wno-stringop-overread` in `engine/CMakeLists.txt`. That link runs the late checks of GCC again on code that it inlines across files. The function `add_s` of `lstrlib.c` copies the whole match of `string.gsub` through `luaL_addlstring`, and GCC cannot prove there that the match ends after its start, so it reports a copy of up to `SIZE_MAX - 1` bytes that no call makes. Every file of the engine compiles with those checks on. |
| Clipper2 | C4702 of MSVC, unreachable code | The headers of Clipper2, which the engine includes only through `engine/src/math/Clipper2.hpp`, between `#pragma warning(push)` and `#pragma warning(pop)`. The code generator of MSVC reports the clamp that the inline `CheckPrecisionRange` of Clipper2 keeps after a call that always throws, and `/external:W0` does not cover the warnings of the code generator. MSVC keeps the state of such a warning that a function definition starts with, so the functions of the engine keep the warning. |

### Patches

A change to the source of a dependency is a patch in `engine/cmake/patches/` that CPM applies. CPM applies the patches of each package in the order of its list in `engine/cmake/haylen-dependencies.cmake`. On Windows the patches take the `patch` that Git for Windows installs in its `usr/bin` folder, because many machines put the older `patch` of Strawberry Perl first on the path, which crashes on the patches of SDL. The cache path `PATCH_EXECUTABLE` names another one. Each patch below fixes one problem of the pinned commit.

#### sdl-uikit-presses.patch

SDL turns the keys of a hardware keyboard on iOS, iPadOS, Mac Catalyst and tvOS into keys only while GameController reads no keyboard, and it turns the arrows and escape of a keyboard into key commands of its view controller too. The runtime reads every key of a hardware keyboard through `GCKeyboard`, so those keys would arrive twice, and while a keyboard is connected SDL drops the presses of remotes and game controllers altogether. SDL also turns the swipes on the touch surface of the Apple TV remote into arrows, while the runtime reads that surface as the stick of the remote. The patch:

- Leaves the presses that carry a key of a keyboard to GameController, and turns the other presses into keys whether a keyboard is connected or not. On tvOS it keeps the Menu press that the Escape key of a keyboard makes, so Escape goes back like the remote.
- Removes the key commands of the arrows and escape.
- Removes the swipe gestures of the Apple TV remote.
- Reads whether the joystick subsystem opened the Siri Remote only where SDL builds that subsystem, which the UIKit builds of the engine leave out.

#### sdl-uikit-status-bar.patch

SDL hides and shows the status bar through `UIApplication` whenever it creates a window or the window changes its border or fullscreen. Those calls do nothing while view controllers decide the status bar, which is the default of UIKit and the way the runtime applies [`window.setStatusBar`](../lua-api/window.md#windowsetstatusbarvalue), and iOS 27 logs a misuse at each of them. The patch removes those calls on iOS, iPadOS and Mac Catalyst, so the view controller of the window alone decides the status bar.

#### sdl-uikit-scene-launch.patch

SDL connects the scene of a UIKit app on iOS, iPadOS, Mac Catalyst, tvOS and visionOS without the window of the app. It covers the scene with a launch window of its own, which shows the launch storyboard, and it starts the app only after the connection, so the window of the app opens later and takes over from that launch window. The scene then shows a black frame, and the logo of the launch window could stand elsewhere than the logo of the cover of the runtime. The patch starts the app while the scene connects and leaves the launch window out. The engine runs SDL through its main callbacks, whose start returns once the app opened its window, so the connection waits only for that start, as the connection of any UIKit app waits for its window. The window of the app with the cover of the launch screen is then in the first frame of the scene, and the system fades it in over its own launch screen.

#### sdl-uikit-tvos-mouse.patch

SDL reads mice through GameController on iOS, iPadOS and tvOS only when the `Info.plist` sets `UIApplicationSupportsIndirectInputEvents`, which makes iOS tell the touches of a pointer apart from the touches of a finger. That key exists only on iOS and iPadOS, and tvOS has no such touches, so every tvOS app logged at launch that the key is missing. The patch reads mice on tvOS without the key.

#### sdl-uikit-frame-rate.patch

SDL calls the main callbacks of iOS, iPadOS, Mac Catalyst, tvOS and visionOS from a display link, which it sets once to the highest rate of the display. The main callbacks of the other platforms follow the hint `SDL_HINT_MAIN_CALLBACK_RATE`, but that display link ignored it, so an app that aims for 30 or 60 frames per second on a display of 120 hertz woke 120 times a second and kept the display at its highest rate. The patch makes the display link follow the hint through `preferredFrameRateRange`, so the display slows down to the rate of the app, and runs it at the highest rate of the display without one. The engine sets the hint from the [frame rate](../core/architecture.md#frame-rate) of the app.

#### sdl-web-page-keys.patch

SDL listens for the keys of the whole page on the window of the browser. So a page that hosts the app next to elements of its own, such as the editor of a website, could keep their keys from the app only by stopping them before they reach those elements. The patch skips a key whose time stamp the runtime names in `Module.haylenIgnoredKey`. The runtime does this for the keys of elements that are not the canvas, the page or the fields of the runtime, so those elements still hear them.

#### sdl-windows-composition.patch

The Direct3D devices compose a transparent window of Windows with the desktop through a swapchain of DirectComposition with premultiplied alpha. SDL creates every window with a redirection surface, which covers the visual of that swapchain. The patch creates the transparent windows that draw through neither OpenGL nor Vulkan with `WS_EX_NOREDIRECTIONBITMAP`, so the visual shows. The windows of OpenGL keep the redirection surface, through which SDL makes them transparent.

#### sdl-x11-quick-clicks.patch

On X11, SDL hears every press and release of a mouse twice, once from the device itself and once from the virtual pointer that merges every device, and it counts on the state of the buttons to drop the second copy. The X server may deliver the press and the release of a quick click from the device before those of the virtual pointer. So a tap of a touchpad, a click that an assistive tool or a remote desktop makes through the XTEST extension, or a mouse that releases its button at once reached the app as a double click. The patch remembers the last press and the last release that SDL took from a device, and drops the event of the virtual pointer with the same source, button and time, while a press of the virtual pointer whose device event SDL skipped still reaches the app.

#### box2d-material-contacts.patch

Box2D recycles a contact between two shapes that moved less than about 5 centimeters or 11 degrees against each other since its last update. A recycled contact keeps its points and also the friction, restitution, rolling resistance and tangent speed it mixed from the shapes then. So a crate at rest on a belt that starts to run, or on a ramp that turns to ice, never felt the new material. The patch makes `b2Shape_SetFriction`, `b2Shape_SetRestitution` and `b2Shape_SetSurfaceMaterial` clear the recycling of every contact of the shape, so the next step updates those contacts with the new material.

#### box2d-point-in-polygon.patch

Box2D tells whether a point lies in a polygon from the distance between them, which it finds by search. For a point on a diagonal of the polygon, such as the center of a square, the direction of that search fades to round-off, and the search stops with a tiny distance instead of zero. So `b2Shape_TestPoint` reported the center of a box as outside it, and `world:queryPoint` missed the box. The patch first checks the point against every edge of the polygon, which finds every point inside it, and leaves the distance to rounded polygons whose rounding the point may lie in.

#### nanosvg-visibility.patch

NanoSVG hides the shapes whose `display` is `none`, but draws those whose `visibility` is `hidden` or `collapse`. A drawing program may leave such a hidden background behind a logo, which the vector images of the engine and the SVG pictures of the UI would then show. The patch hides those shapes too.

#### nanosvg-bounding-box-gradients.patch

A gradient of SVG lays itself out in the bounding box of its shape, unless it sets `gradientUnits="userSpaceOnUse"`. There a coordinate without a unit, such as `x2="1"`, is a fraction of the box. NanoSVG reads such a coordinate as a length of the document. So a gradient from `0` to `1` shrinks to one unit at the corner of the document, and the shape takes the color of one end of the gradient. The patch reads the coordinates, percentages and radii of these gradients in a unit square. Then it stretches that square over the bounding box of the shape after the `gradientTransform`, as SVG defines it. So a radial gradient on a wide shape becomes an ellipse.

#### nanosvg-style-bounds.patch

NanoSVG reads a style, such as the attribute `style` or the rules of a `<style>` element, one declaration at a time between semicolons, and trims the spaces at its ends. A declaration that holds nothing but spaces, such as the rule `.empty { }`, ended one byte past its text, so NanoSVG read memory after the end of the style, which the fuzzer of the vector images found. The patch trims each declaration and each name inside its own bytes.

#### clipper2-iterator.patch

The core header of Clipper2 uses `std::back_inserter` without including the header `iterator`, which the standard library of Emscripten does not bring in through another header. The patch includes it.

### Choices that need no patch

- The Android runtime asks EGL for the newest OpenGL ES version that the device declares in `ro.opengles.version`. The emulator creates a context of exactly the version an app asks for, while every context reports the newest version of the device. So a context of an older version would report features, such as the limits of OpenGL ES 3.1, that it rejects.
- The SDL platform layer enters the main callbacks of SDL with `SDL_RunApp` and `SDL_EnterAppMainCallbacks`, without the `SDL_main.h` header that renames `main`. So its entry points, `WinMain` on Windows and `main` on Linux and the web, return the exit status of the app, and the runtime never defines `main` on Apple platforms. The app calls `haylen_main` there, from the `main.m` of the Apple template of the backend.

## haylen_add_app

The file `engine/cmake/haylen-app.cmake` defines the one CMake function that builds the binary of an app, Lua or C++, against the engine of an installed SDK or of a source tree:

```cmake
haylen_add_app(<target> LUA|CPP [WITH_LUA] [SOURCES <files>...] CONTENT <folder> [BOOTSTRAP <file>] [WEB_SHELL <file>])
```

The option `LUA` links the Lua player `haylen::player`, and `CPP` compiles the `SOURCES` that define `haylen::core::Application::create`, with the Lua layer `haylen::lua` only when `WITH_LUA` names it. The folder `CONTENT` is what the app ships, which the tooling prepares, and `BOOTSTRAP` is the release bootstrap that the content tool writes. The function builds an executable on desktops, the shared library of the activity on Android and a page on the web, and calls the `APP_FUNCTION` of a port, while Xcode builds the apps of Apple platforms. The generated projects of the desktop, Android and web backends call it, and the [embedding guide](embedding.md#haylen_add_app) describes every argument and what it makes on each platform.

## The desktop player

The `haylen` player is the runtime without a bundled app. It runs any package that its command line names, a folder or a zip:

```sh
build/engine/macos-debug/bin/haylen/haylen --dev ~/apps/my-game
build/engine/macos-debug/bin/haylen/haylen my-game.zip
```

The option `--dev` turns on development mode, as the [distribution guide](distribution.md) explains with every option of the command line. The target `player` of `haylen app run` passes `--dev-server` instead, the address of its development server. Without either, the player runs the package like a shipped app. Without a package on its command line, the player runs the folder `app/` next to it, or a zip of it named `app.zip`. A package that fails to load keeps the window open and shows the error.

The player, like every desktop app, exits with the status that the app quit with, which [`haylen.quit`](../lua-api/haylen.md#haylenquitoptions) sets. It exits with 0 when its window closes, and with 1 when its engine could not start.

The desktop SDK carries the player of apps as a host tool, which `haylen sdk build --target player` builds on macOS, or with the target of the system on Windows and Linux, and which the target `player` of every Lua app shares.

## Web builds

Web builds run on one thread, like Varn. So they need no `SharedArrayBuffer` and no cross-origin isolation. The command `serve` sends an opener policy only when `--coop` asks for one, as [serve](commands.md#serve) explains.

Audio reaches the browser through an `AudioWorkletNode`, which the page feeds over its message port, as the [audio guide](../audio/audio.md#sessions-and-interruptions) describes. Browsers offer `AudioWorklet` only to pages served over https or from localhost. The commands `serve` and `app run --target web` serve on `127.0.0.1` by default, so their pages play sound. Apps run on any other page too, without sound, as the [audio guide](../audio/audio.md#without-an-audio-output) explains.

The link options of a web runtime come from `haylen_link_runtime_platform` and from the GPU devices:

- Memory growth and a stack of 1 MB.
- IDBFS for user data.
- The scripts of the [runtime API](#runtime-api), which `--pre-js` links into the runtime script.
- The `emdawnwebgpu` port for WebGPU, and WebGL 2 alone through `-sMIN_WEBGL_VERSION=2` and `-sMAX_WEBGL_VERSION=2`, which the GPU devices of `haylen-gpu` hand on.

The engine target adds the native exceptions and the native `longjmp` of WebAssembly, `-fwasm-exceptions` and `-sSUPPORT_LONGJMP=wasm`. Varn compiles Lua as C++, so Lua errors unwind as exceptions and yields jump back to their resume. Every current browser runs both without help from JavaScript.

### Files of a web build

A web target produces `<target>.html`, `<target>.js`, `<target>.wasm` and, for apps, `<target>.data` in `bin/<target>/`. A step after the build, `haylen_setup_web_page`, copies more files next to them:

- The shell of the target as `<target>.shell.html`. That is the `WEB_SHELL` of `haylen_add_app`, or `engine/platform/web/shell.html` by default.
- `haylen-logo.svg`, the engine logo, which the default shell shows on its splash and as the icon of the page.
- `haylen-audio-worklet.js`, the AudioWorklet processor of the audio output. The runtime loads it from the folder of its script through `locateFile`.
- `haylen-content-store.js`, the worker of the [content store](../content/content.md#the-web) of a protected release.
- `haylen-page.js` and `haylen-worker.js`, the page side of the runtime and the worker that runs the module in the [Worker mode](#hosting-the-runtime), which a page of that mode loads from next to the runtime script.

The web SDK holds the prebuilt web player, `haylen.js` and `haylen.wasm` with `haylen-audio-worklet.js`, `haylen-content-store.js`, `haylen-page.js` and `haylen-worker.js`. The web backend generates the site of an app around it from the page of `tools/haylen/platforms/web/template`, the loader that the [distribution guide](distribution.md) describes. A Debug build of a Lua app takes the prebuilt player as it is, while a Release build or a C++ app links a runtime of its own, the target `haylen` of the CMake project `runtime/` of the site, with `haylen_add_app` against the web SDK. The site takes the runtime files that its page loads and leaves out the shell of the target.

The runtime draws through WebGPU, or through WebGL2 where WebGPU does not start. The default shell of `haylen_add_app`, for a CMake project that builds a page itself, shows the splash of the [web loader](distribution.md#changing-the-splash) over the background `#101418`: the engine logo, a progress bar and the loading text with the percentage, which `onLoadProgress` of the runtime feeds while `<target>.wasm`, `<target>.data` and the package of the address download. The logo is an SVG inside the page, so the first paint shows it before anything downloads, and a copy of the shell without the element `#splash-logo` shows the bar and the loading text alone. A size that the server hides, as a compressed response does, turns the bar into a sweep without a percentage. The splash fades out with `onSplashEnded`, and an error before the app starts replaces the bar with its message. The variables at the top of its style restyle it, in a copy that `WEB_SHELL` names. The default shell reads two parameters of the address: `?graphicsApi=webGpu` or `?graphicsApi=webGl2` tries one of the two APIs first, by the names that `graphics.apiName()` gives them, and `?package=<url>` downloads a zipped package.

A custom `WEB_SHELL` keeps the `{{{ SCRIPT }}}` placeholder, where Emscripten inserts the runtime script.

### Page setup

A page defines `Module` before it loads the runtime script. The loader `tools/haylen/platforms/web/template/loader.js` of the web backend is the model for the pages of apps, and `engine/platform/web/shell.html` for a CMake project that builds a page itself.

| Field | Meaning |
| --- | --- |
| `Module.canvas` | The canvas that the app draws into. It must be a canvas element with an `id`. The runtime follows its size with a `ResizeObserver`, so the layout of the page decides the size. A canvas without area, such as one that the page hides with `display: none` or inside a hidden frame, keeps the app running without drawing or laying out, and the app draws at the right size once the canvas shows again. A canvas without area when the page loads holds the start of the app until it shows. In the Worker mode the page hands the canvas to the worker with `transferControlToOffscreen`, so nothing else draws into it. |
| `Module.haylen.hosting` | Optional `worker`, which runs the module in a dedicated worker, as [hosting the runtime](#hosting-the-runtime) describes. The page then loads `haylen-page.js` instead of the runtime script. |
| `Module.haylen.runtimeUrl` | The address of the runtime script in the Worker mode, `haylen.js` next to the page by default. The worker `haylen-worker.js`, the audio processor and the worker of the content store load from next to it. |
| `Module.haylen.packageData` | Optional bytes of a zipped package, as an `ArrayBuffer` or a `Uint8Array`. The runtime writes them to its file system and plays them instead of the bundled package. The web loader downloads `app.zip` with a progress bar and hands it over this way. |
| `Module.haylen.packageUrl` | Optional URL of a zipped package, used when `packageData` is not set. The runtime downloads it before the app starts and plays it instead of the bundled package. A failed download reaches `onError`. The default shell of C++ apps reads it from the `package` parameter of the address. |
| `Module.haylen.release` | Optional `{address, identifier, files}` of a protected release. The `files` are `{name, bytes}` of its manifests and its app domain, which the page downloaded and which go to `/app`. The `address`, such as `app/`, is where the shards of the content stay and download by range. The `identifier` of the app names its [content store](../content/content.md#the-web). The web loader sets it in release builds. |
| `Module.haylen.graphicsApi` | Optional `webGpu` or `webGl2`, the graphics API that the window tries first, as the player does with `--graphics-api`. Without it the runtime draws through WebGPU, or through WebGL2 where WebGPU does not start. The web loader and the default shell of C++ apps set it from the `graphicsApi` parameter of the address. |
| `Module.haylen.arguments` | Optional list of strings, the launch arguments of the app, which [`haylen.launchArguments`](../lua-api/haylen.md#haylenlauncharguments) returns. The web loader and the default shell of C++ apps set it from each `argument` parameter of the address, such as `?argument=--test&argument=PLG-016`. |
| `Module.haylen.development` | Optional `true` that runs the app in development, as the player does with `--dev`. Lua modules and assets that the page changes with `applyChanges()` then reload in place, as [hot reload](../core/lua.md#hot-reload) describes. |
| `Module.haylen.projectBudget` | Optional `{files, bytes}`, the most files and bytes that the package of a project may hold, 4096 files and 268435456 bytes by default, as [editing the package](#editing-the-package) describes. |
| `Module.haylen.storageNamespace` | Optional name that keeps the user data of the page in an IndexedDB database of its own, `/persistent/<name>`, as [hosting the runtime](#hosting-the-runtime) describes. Without it the user data lives in `/persistent`. One tab of the site uses a database at a time, as [the user data of a tab](#the-user-data-of-a-tab) describes. |
| `Module.haylen.developmentServer` | Optional WebSocket address of the development server that `app run --target web` starts, such as `ws://127.0.0.1:8000/haylen/development?token=<token>`. The server pushes every file saved in the app to the page. |
| `Module.instantiateWasm` | Optional function that receives the imports of the module and a callback. It instantiates `haylen.wasm` itself and passes the instance to the callback. So a page that already downloaded the bytes, such as the web loader, keeps Emscripten from fetching the file again. |
| `Module.preRun` | Functions that run before the app starts. Page handlers of the platform bridge register here. |

None of the package fields turns on development mode. Only a page that sets `Module.haylen.development` runs in development. The template loader sets it only for the `development` entry of `config.json` that `app run --target web` writes.

Before the app starts, the runtime mounts IndexedDB at `/persistent`, or at the folder of the storage namespace, for user data and loads it. It also reads what the browser tells about the device through `navigator.userAgentData` and the Battery Status API, which answer asynchronously.

### Runtime API

Two scripts make the runtime. The engine side, `engine/platform/web/haylen-runtime.js`, holds what needs the memory and the exports of the module: the calls into the engine, the editor contract, the WebSockets of the app, the content store, the downloads, the user data, the mixer of the audio outputs, the pictures of videos and the pushes of streams. The page side, `engine/platform/web/haylen-page.js`, holds what needs the document: the input, the text fields, the overlay, the mirror of screen readers, the clipboard, fullscreen, the pointer and wake locks, the video elements, the windows of outputs, the audio contexts and the web modules of plugins. Both are linked with `--pre-js`, the engine side first, and in the Worker mode the page loads the page side alone. Together they fill `Module.haylen` with the API that a page uses: the handlers and events of the platform bridge, the contexts of plugins, the callbacks of the app and the editor contract of the next section. None of these calls reloads the page.

| Function | Meaning |
| --- | --- |
| `register(method, handler)`, `unregister(method)`, `emit(event, payload, options)` | The handlers and events of the platform bridge, with `options.retain` to keep an event for the first listener, as the [platform bridge guide](../platform/platform-bridge.md#web) describes. |
| `openLink(url)` | Hands the app a link that opened the page, such as a link of a URL scheme of a plugin that the address of the page stands in for. The app hears it as [`linkOpened`](../lua-api/events.md#app-and-plugins), the link that started the page once `source/main.lua` ran, and a link that arrives while an app that the app launched plays ends that app, as on the other platforms. |
| `createPluginContext(id, config)` | The context of the web module of a plugin, as the [plugin guide](../platform/plugins.md#web-modules) describes. |
| `nativePlugins()` | Returns the ids of the plugins that have a web module. |

No C++ exception crosses into the page. A function of the runtime that fails while it answers the page throws an `Error` with the reason. Otherwise it logs an engine error that names the function, and the page goes on.

The runtime reports back through callbacks that the page assigns to `Module.haylen`:

| Callback | Arguments |
| --- | --- |
| `onLog(level, message)` | The `level` is `'debug'`, `'info'`, `'warning'` or `'error'`. |
| `onError(error)` | The [error record](#error-records) of the error that stopped the app. |
| `onStarted(app)` | `{name, identifier, version, build}` from `haylen.json`. |
| `onStopped()` | The app ended. |
| `onLoadProgress(progress)` | `{loaded, total}` in bytes while the runtime downloads what the app needs before it starts: the WebAssembly module, unless the page instantiates it with `Module.instantiateWasm`, the files that the build preloads and the package of `packageUrl`. The `total` is 0 while the size of a download is unknown, as for a compressed response, which tells only its compressed length. |
| `onSplashEnded(splash)` | `{fadeOut}`, the seconds of the fade out of `haylen.json`. The runtime ended the splash screen, which the page then fades out over those seconds. |
| `onTurnDevice(screen)` | `{turn, orientation}`. A phone or a tablet holds the page in an orientation that the app does not take, the one of `window.lockOrientation` or else of `haylen.json`, while the browser keeps the screen from turning, so `turn` is `true` and the page shows its turn screen over the canvas with the `orientation` the app wants, `'landscape'` or `'portrait'`. It is called again with `turn` set to `false` as soon as the device turns. The app runs on under the screen. Desktop browsers never call it, since their windows do not turn. |
| `onReloaded(report)` | In development, after every batch of changes that hot reload applied, the [reload report](#reload-reports). |
| `onStats(stats)` | About once per second: `fps`, `frameMilliseconds`, `averageMilliseconds`, `drawCalls`, `sprites`, `vertices`, `textureSwitches`, `uploadedBytes`, `assets`, `voices`, `scopes` and `audio`. The `scopes` are a list of `{name, milliseconds, calls, depth}` profiler scopes. The `audio` is the output of the app as `{available, state, sampleRate, bufferedMilliseconds, blocks, underruns}`, or only `{available}` while the app runs without an audio output. |

In `onStats`, `available` tells whether the app plays sound, `state` is the state of its `AudioContext` and `bufferedMilliseconds` is the mixed audio that waits ahead of the output. The `blocks` count the blocks mixed so far, and `underruns` the render quanta that the processor played without samples.

Callbacks run in a microtask right after the frame that raised them. So they may call back into the runtime, even to run the app again.

### The editor contract

An editor, such as a playground of a website, edits a project and runs it with `Module.haylen.editor`. The playground runs Lua, JSON and assets against the precompiled engine of the page, so it never compiles C++ or native plugins.

The contract has a version, `Module.haylen.editor.protocol`, which is `1`. A later version that changes a request or a record raises it. Every request goes through one function:

```js
const answer = await Module.haylen.editor.request(method, params);
```

Each request gets an id, which its answer carries as `request`, and a promise. The promise resolves once the request completed, and rejects with an `Error` whose `code` is a stable code and whose message is a sentence for the person. The codes are `invalidRequest`, `invalidPath`, `missingFile`, `projectBudget`, `package` for bytes that are no zipped package, `notEditable` for the runtime of a release, `notRunning` when no app plays, `unsupported`, `disposed`, `terminated` for a worker that the editor terminated and `failed`. A request made before the runtime started waits for it.

| Request | Params | Answer |
| --- | --- | --- |
| `hello` | | `{protocol, capabilities, storage}` |
| `edit` | `{operations}` | `{revision}` once the package changed |
| `clearFiles` | | `{revision}` once the app stopped and the package is empty |
| `applyChanges` | | `{revision, report}` once the frame that applied the changes ended |
| `run` | | `{revision, app, error}` once the app started |
| `loadZip` | `{bytes}` | `{revision, app, error}` once the app of the zipped package started |
| `stop` | | `{revision}` once the app ended |
| `pause`, `resume` | | `{paused}` |
| `console` | `{line}` | `{ok, output}` once the next frame ran the line |
| `inspect` | `{path}` | `{path, entries}` |
| `dispose` | | `{disposed}` once the runtime ended |

The `capabilities` say what this runtime offers, each as a boolean: `packageEditing`, which the runtime of a release lacks, `developmentReload` for a page in development, `pause`, `console`, `inspection`, `cancellation` for the [execution budget](../core/lua.md#the-execution-budget) of a page in development, `storageNamespaces` and `workerHosting`, which is `true` when the runtime runs in a dedicated worker, as [hosting the runtime](#hosting-the-runtime) explains. The `storage` is `{namespace, available, reason}`: the storage namespace of the page, or `null` for the default database, whether this tab holds its user data, and the sentence that tells why not, or `null`, as [the user data of a tab](#the-user-data-of-a-tab) describes.

#### Editing the package

The request `edit` applies a batch of operations to the package that plays, all of them or none:

- `{op: "write", path, content}` adds or replaces a file, with `content` as a string, which goes as UTF-8, or as bytes, an `ArrayBuffer` or a view of one.
- `{op: "remove", path}` removes a file.
- `{op: "rename", from, to}` moves a file to another path, replacing a file there.

Paths are relative to the root of the package, such as `source/main.lua` or `content/ui/logo.png`. The batch checks every operation before it changes anything. A path that is absolute or leaves the package fails it with `invalidPath`, and a remove or a rename of a file that the package does not hold at that point of the batch fails it with `missingFile`. A batch after which the package would hold more files or bytes than `Module.haylen.projectBudget` allows fails with `projectBudget` and a sentence that names both counts, and so does a zipped package of `loadZip`, so a project never fills the memory of the page. Every batch that applied makes a new revision of the package, a number that grows by one, which its answer carries. The app reads an edited file the next time it loads it, and `applyChanges` applies the files of every batch since the last `applyChanges` at once.

The request `clearFiles` stops the app and empties the package, for a project that the editor writes file by file before `run`. The request `loadZip` replaces the package with a zipped one and runs it, and bytes that are no zip archive fail it with `package` and leave the app as it is.

#### Applying changes

The request `applyChanges` hands the changed files to the app as one batch at the start of the next frame. Its answer comes once that frame ended and carries the revision that the batch applied. A page in development reloads them the way [hot reload](../core/lua.md#hot-reload) applies saved files, and the `report` is the [reload report](#reload-reports) of the batch, which tells for every file whether it applied in place or restarts the app and why, or `null` when no file changed since the last `applyChanges`. The engine restarts the app itself for a file that needs it, so an editor never restarts it on its own and never guesses from the extension of a file. Any other page restarts the app, and the `report` is `{restarted: true, reason, files}` with every file restarting for that reason. A paused app applies no changes, so the answer comes once it resumed, and a request without a playing app fails with `notRunning`.

A module that fails to reload keeps running the code it had, while the package keeps the new file, so the report rejects the file with its error and the error screen shows it. Nothing rolls the file back. The next batch that fixes the module, or that writes the exact text that the module runs, resumes the app. An error of a lifecycle step, or the execution budget, is no error a reload fixes, so the next batch restarts the app.

#### Running, pausing and stopping

The request `run` starts the package from `source/main.lua`, in place of the running app, with a fresh Lua state, and its answer comes once the app started. The `app` of the answer is `{name, identifier, version, build}` from `haylen.json`, and its `error` is the [error record](#error-records) of an error that stopped the start, or `null`. A package whose `haylen.json` is invalid answers with the error and no app. Running again needs no reload of the page, and the audio contexts and the graphics device of the page stay for the next app. A run leaves nothing of the app before it: its voices and their processors end, its timers, tasks and pending work end with its Lua state, and the page keeps one audio context for each sample rate, which the person already unlocked, so the next app plays at once, in WebKit too.

The request `pause` keeps the last frame on screen and the app sees the pause as a suspend, as a hidden page does. A paused app runs no frame and applies no changes until `resume`. A `run` always leaves the app running. The request `stop` ends the app and leaves an empty canvas until the next `run`.

#### Inspecting the app

The request `inspect` lists the values of the running app at a path of the [inspector](../lua-api/debug.md#inspector), without changing anything: an empty path lists the roots `watched`, `scenes`, `autoloads` and `globals`, and a path such as `scenes.1` lists the keys of that value. Each entry is `{key, type, value, expandable}`, where `value` is the value as short text and `expandable` tells whether the path of the entry lists more. The inspector reads tables raw, so it never runs code of the app. A request outside development fails with `unsupported` unless `debug.console` of `haylen.json` turns the console on, and a path that leads nowhere fails with `invalidPath`.

#### Ending the runtime

The request `dispose` ends the runtime for good: the app stops, the player ends with its window and its graphics device, every audio context and its processors close, and the user data reaches its database. Its answer comes once all of that ended, so the page can then remove the frame that hosts the runtime. Every later request fails with `disposed`.

### Error records

Every error that stops an app reaches `onError` as one record, and the answers of `run` and the reload reports carry the same records:

| Field | Meaning |
| --- | --- |
| `code` | What kind of error it is: `syntax` for a Lua file that does not compile, `runtime` for code that raised an error, `package` for a package that cannot open or whose `haylen.json` is invalid, `asset` for an asset that could not load, `executionBudget` for code that ran longer than the [execution budget](../core/lua.md#the-execution-budget), and `memory` for an app that ran out of memory, of its memory budget in development or of the system. |
| `phase` | Where the app stood: `startup` while it starts, from its plugins to `source/main.lua`, `lifecycle` in a step of its lifecycle, such as a scene `enter`, `frame` in code of a frame, such as an update, a timer or a listener, and `reload` in a reload of development. |
| `message` | The sentence for the person, without the position in front of it. |
| `file`, `line` | The project-relative path of the file, such as `source/player.lua`, and the line, or an empty `file` and line 0 when no script position is known. Lua tells lines only, so a record has no column. |
| `callback` | For an error of the execution budget, the step of the engine that ran too long, such as `start`, `update`, `fixedUpdate`, `render`, `scripts` for the callbacks of promises and tasks, `event` or `reload`. |
| `traceback` | The stack as text, one frame per line. |
| `frames` | The same frames as `{source, line, function, kind}` objects, innermost first, with the kind `script`, `native` or `main`. |

The machine fields never change with the language of the person, while `message` and `traceback` are text for the person.

### Reload reports

A report of hot reload is `{restarted: false, files, modules, restored, assets, rejected, resumed, milliseconds}`. The `files` hold one `{path, change, reason}` for every file of the batch, whose `change` says what the batch did with it and whose `reason` is the sentence that tells why:

| Change | Meaning |
| --- | --- |
| `inPlace` | The file applied while the app runs on with its state: a module that reloaded, a module whose file holds again the code that runs, a removed module that no code requires, which left the app, or an asset that changed in place, such as a texture, a shader, JSON, a font or a sound. |
| `nextLoad` | Nothing of the app uses the file now, such as a Lua file that no module loaded yet or an asset that nothing holds, or the file holds assets of a type that cannot change in place, which announced `assetChanged`. The next load reads it. |
| `ignored` | Hot reload never reads the file, such as a hidden file, a backup file or a file under `source/` that is not Lua. |
| `rejected` | The file could not apply, and `rejected` holds its error record. |
| `restart` | The app restarts for the file, such as for `haylen.json`, `source/main.lua`, a plugin manifest, a removed module that code of the app requires or a module that cannot change in place, or the restart that another file asked for applies it too. |

The `modules` and `assets` are the files that reloaded in place, the `restored` are modules whose file holds again the code that runs after a reload of them failed, and the `rejected` are `{path, error}` for every file that could not apply, with its [error record](#error-records). The `resumed` tells whether the app left the error screen. Right before a restart the report is `{restarted: true, reason, files}`, whose `reason` is the reason of the restart.

```js
const editor = Module.haylen.editor;
Module.haylen.onError = (error) => console.error(error.code + " " + error.file + ":" + error.line + " " + error.message);

async function play(files) {
    await editor.request("clearFiles");
    const operations = Object.entries(files).map(([path, content]) => ({ op: "write", path, content }));
    const { revision } = await editor.request("edit", { operations });
    const started = await editor.request("run");
    console.log("Revision " + revision + " runs " + started.app.name);
}

async function save(path, content) {
    await editor.request("edit", { operations: [{ op: "write", path, content }] });
    const { report } = await editor.request("applyChanges");
    for (const file of report.files) {
        console.log(file.path + " " + file.change + ": " + file.reason);
    }
    return report.restarted ? [] : report.rejected;
}
```

### Hosting the runtime

One runtime plays one app at a time, and a page holds one runtime, since the runtime keeps its state in the global `Module` of the page. Two players coexist only in two frames or windows of their own, each with its own runtime. A page that names `Module.haylen.storageNamespace` before the runtime loads, such as `project-42`, keeps the user data of its apps in the IndexedDB database `/persistent/project-42`, apart from every other namespace and from pages without one, and the editor deletes the data of a project with `indexedDB.deleteDatabase("/persistent/project-42")` while no runtime uses it. A namespace is 1 to 64 letters, digits, dots, dashes and underscores, and it starts with a letter or a digit.

#### The user data of a tab

Two tabs that wrote one database would overwrite each other's changes without a word, since each keeps a copy of the files in its memory and writes the whole copy back. So one tab of the site holds a database at a time. Before the app starts, the runtime asks the browser for the lock of its database through the Web Locks API of the browser, `navigator.locks`, named after its folder, such as `haylen-user-data:/persistent/project-42`, and holds it until the runtime ends with `dispose` or `terminate()` or the page goes away. A page whose lock another tab holds still runs its apps, but keeps no user data: every call of [`haylen.storage`](../lua-api/storage.md#a-tab-without-its-user-data), save slots and preferences included, raises the sentence `Another tab of this site uses the storage namespace "project-42", so this page keeps no user data. Close the other tab and load this page again.`, the log of the page warns with it, and `hello` answers `storage.available` as `false` with that sentence as `storage.reason`. So an editor tells the person instead of letting two tabs diverge. The browser gives the Web Locks API only to secure pages, those of `https` and of `localhost`, so a page without it warns in its log that it cannot tell whether another tab writes the same user data, and keeps its user data as before.

An editor that must keep answering whatever the code of a project does runs the runtime in the Worker mode, in a dedicated worker that it ends when the code of a project stops answering. A page asks for the Worker mode with `Module.haylen.hosting = "worker"` and loads `haylen-page.js`, the page side of the runtime, instead of the runtime script:

```html
<canvas id="canvas"></canvas>
<script>
    var Module = {
        canvas: document.getElementById("canvas"),
        haylen: { hosting: "worker", runtimeUrl: "player/haylen.js", development: true, storageNamespace: "project-42" },
    };
</script>
<script src="player/haylen-page.js"></script>
```

Like a page that holds the module, a page of the Worker mode runs the functions of `Module.preRun` first and holds the start until every `Module.addRunDependency` was released with `Module.removeRunDependency`, and it hands the worker the bytes of `Module.wasmBinary` when it downloaded the module itself. The page side reads the device, starts `haylen-worker.js` next to the runtime script as a dedicated worker and hands it the canvas as an `OffscreenCanvas` with the options of the page. The worker loads the runtime script, whose engine side draws into the canvas through WebGPU or WebGL2 and runs the frames on the animation frames of the worker, through the worker layer of `engine/src/platform/worker/`. The two sides talk through messages:

- The page posts the input over the canvas: the keys that reach the app by their `code`, the mouse and the pens and fingers of pointer events, the scrolls and the focus of the page, and the controllers of the Gamepad API in its standard mapping at every animation frame of the page.
- The page posts the state that the engine reads during a frame whenever it changes: the size of the canvas box and the pixels of the screen in each CSS pixel, the safe area, the fold, the orientation, fullscreen, the text of the clipboard, the screens and the plugins with a web module.
- The page posts the requests of the editor, which the worker answers with the contract of the runtime on the page, and the calls of its page side into the engine, such as the answers of plugins and the edits of text fields. What the page posts before the worker started waits for it.
- The worker posts the calls of the engine into the page, such as the text fields to edit, the tree of screen readers, fullscreen, the locks, the clipboard, the cursor, the title and the calls of the platform bridge, and a heartbeat four times a second while it answers.
- Audio, videos and outputs stay on the page. The page owns the audio context of each output and hands its processor one end of a `MessageChannel` and the worker the other, so the mixer of the worker posts its blocks straight to the processor. A video plays in an element of the page, which posts each new picture to the worker as an `ImageBitmap` with its sound, and an output on another screen opens a window of the page whose canvas the worker draws into as another `OffscreenCanvas`.

Everything else stays in the worker: the user data in IndexedDB, the WebSockets of the app, the downloads and the content store. In the Worker mode `hello` reports `workerHosting` as `true`, and the editor gains two functions:

| Function | Meaning |
| --- | --- |
| `Module.haylen.editor.heartbeat()` | The milliseconds since the worker last answered, which grow while code of the app holds the worker, such as code whose [execution budget](../core/lua.md#the-execution-budget) has not run out yet, code outside development, which has no budget, or a native function that runs long. |
| `Module.haylen.editor.terminate()` | Ends the worker at once, rejects every request that waits with the code `terminated` and releases what the page holds for the runtime: its audio contexts, text fields, videos, outputs and locks. It resolves once the audio contexts closed, and the page then starts a new runtime for the next project. |

A `dispose` ends the worker too, once the user data reached its database. The Worker mode has limits that come from the browser:

- The memory of the module stays in the worker, so a `VarnDataView` of the app never reaches the web module of a plugin, which runs on the page, and a call or a screen that carries one fails with the code `unsupported`. A plugin moves bytes instead, which cross as copies. The page could see the memory only if the module kept it in a `SharedArrayBuffer`, which needs a build with shared memory and a page that is cross-origin isolated, and the web build keeps its memory to itself, as it runs on one thread.
- The page cannot answer during a frame of the worker, so an audio output opens before the page knows whether the browser gives it a context. A context or a processor that the browser refuses reaches the engine as soon as the page knows, through `haylen_web_audio_failed`, and the app runs without sound, with the reason in its log and `audio.outputState()` at `unavailable`, as on the page. The `push` of the audio stream of a plugin returns the frames that fit in the room that the worker reported last.
- A browser opens popups, fullscreen, the pointer lock and the windows of outputs only shortly after an input of the person, so the request of the app reaches the page while that input still counts.
- The worker needs the animation frames of workers and the `OffscreenCanvas` of WebGPU or WebGL2. Chromium and Safari offer both. A browser without the animation frames of workers reports the reason to `onError` instead of starting.

A runtime on the page, or in a frame of the same origin, shares the thread of the editor, so code of the app that holds that thread freezes the editor too. In development the [execution budget](../core/lua.md#the-execution-budget) stops Lua code that runs longer than `lua.executionBudget`, finalizers included, and the [bounds of native work](../core/lua.md#native-work-and-its-bounds) keep a native function of the engine from running away. Those bounds stop the app, but only after its time ran out, and nothing bounds code outside development. So an editor that needs isolation uses the Worker mode, whose worker runs on a thread of its own in every browser and which `terminate()` ends at once. A frame of another site, such as a sandbox on a domain of its own, also runs in a process of its own in the browsers that isolate sites, Chromium and Firefox, but not in every browser, and the editor then talks to it with `postMessage`.

## Android

The engine port `android` builds the engine and the player library `libhaylen.so` of every ABI with the pinned NDK and Ninja, one ABI after the other in `build/engine/android-<abi>-<config>`, and publishes the Android libraries of Haylen from the Gradle project `engine/platform/android` into the Maven repository of the SDK, as the [distribution guide](distribution.md) describes:

```sh
haylen sdk build --target android
haylen sdk build --target android --architecture arm64-v8a --config Debug
```

Every Android build uses the pinned NDK, `ndk/30.0.16248370` inside the SDK that `ANDROID_HOME` or `ANDROID_SDK_ROOT` names, with `ANDROID_PLATFORM=android-27`, the static C++ library and `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES`. This holds even when the environment names another NDK, because the engine and its dependencies build against its headers.

The Android backend generates the Gradle project of every app from `tools/haylen/platforms/android/template`. Gradle owns the build of the app, and its external native build compiles the native code of the app for each ABI against the SDK of that ABI. Gradle finds the Android SDK through the `local.properties` file that the backend writes from `ANDROID_HOME`. R8 minifies the release builds of apps, and the upload key of the app signs them, which `haylen app sign --target android` creates.

## Apple platforms

The engine port `apple` builds one slice of `Haylen.xcframework` and `HaylenLua.xcframework` per system and environment, each architecture in its own tree `build/engine/apple-<slice>-<architecture>-<config>`, joined with `lipo`:

```sh
haylen sdk build --target ios --target ios-simulator
haylen sdk build --target macos --target catalyst --architecture arm64 --architecture x86_64
```

Slices of the same engine sources and configuration stay in the frameworks, so building the slices of another family later adds them. The Apple backend generates the XcodeGen project of every app, Lua or C++, from `tools/haylen/platforms/apple/template`, which links the frameworks of the SDK, as the [distribution guide](distribution.md) describes. To debug the engine itself in Xcode, configure the tree of this Mac with `haylen engine configure --xcode`.
