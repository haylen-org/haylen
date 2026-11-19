# 🎮 Ports

This guide explains how someone with access to the SDK of a platform that the repository does not hold, such as a game console, adds that platform to Haylen. It covers the three layers that a console needs, what the repository can and cannot hold, the folder of a port and its `port.cmake`, the SDK of a port, the engine port and the backend that the tooling installs from outside the repository, the platform runtime, the GPU device with its graphics API and shader language, the shader tools, testing, continuous integration, a port step by step and the parts that every port implements.

## Three layers

A console reaches Haylen through three layers, and each one lives where its license allows:

| Layer | What it holds | Where it lives |
| --- | --- | --- |
| The engine | Only what is general to every platform: the player, the renderers, the Lua API, the contracts of hosts, windows, services and GPU devices, the mechanism of ports and the plugin contract. | This repository. |
| The console port | The runtime, the window, the GPU device, the audio output, the files and the packaging of the console, with its engine port and its backend for the tooling. | A private repository. |
| The console plugin | The services that only the console offers to games: users and sign-in, achievements, the store, saves of each user, the storage quota, the setup of controllers and their glyphs, with a C++ API and a Lua API. | A private repository. |

The engine never holds a piece of one console, so a console plugs in without a rewrite of the engine. The console plugin is a native plugin like any other, with its `plugin.json`, its Lua API and its native part, as the [plugin guide](plugins.md) describes, and the backend of the port reads its contributions for the platform of the console. This repository only documents the two private layers.

## What a port is

A port is a folder outside the repository that adds a platform to the engine. The cache path `HAYLEN_PORT_DIR` of the engine names it, and the engine then builds for the platform of the port instead of the platform of the toolchain. The port brings two things to the engine, and the engine stays as it is:

- **A platform runtime**: a host, a window and the services of the platform, with the entry point of the process. It takes the place of the [SDL platform layer and the Android runtime](../core/architecture.md#layers).
- **A GPU device**, when the graphics API of the platform is not one of the engine. The device implements the [render hardware interface](../graphics/rhi.md) and draws through the graphics API `port` with the shader language `port`. A platform whose graphics API the engine already has names that API instead.

Everything above those layers, the player, the engine, the renderers and the whole Lua API, is the same as on every other platform. An app runs on a port without a change, and a script sees the port only through [`haylen.platform`](../lua-api/haylen.md#haylenplatform), which names its platform, and [`haylen.graphicsApi`](../lua-api/haylen.md#haylengraphicsapi), which is `'port'` on the device of a port.

The tooling reaches a port through two more pieces, which a Python package of the port installs: an engine port, which builds the SDK of the platform, and a backend, which builds apps against that SDK. [The tooling of a port](#the-tooling-of-a-port) describes them.

## What the repository holds

The SDKs of game consoles, their headers, their graphics APIs, their shader compilers and their documentation come under licenses that keep them private. So the code that calls them never enters this repository, which is public under the MIT License. The repository holds only what every port builds on:

| In the repository | In the private repository of the port |
| --- | --- |
| The mechanism of ports: `HAYLEN_PORT_DIR`, `haylen_port`, the public API of ports in `haylen/port`, the reserved values `Api::Port`, `Language::Port`, `Surface::Kind::Port` and `SystemInfo::Os::Port`, and the `port` targets of shader packages. | The `port.cmake` of the port. |
| The contracts of the tooling: `EnginePort`, `PlatformBackend`, the entry point groups `haylen.engine_ports` and `haylen.backends`, and the SDK manifest. | The engine port and the backend of the console, in a Python package. |
| The engine, the player, the renderers, the Lua API and the devices of the public graphics APIs. | The runtime, the host, the window and the services of the console. |
| The test port in `engine/tests/data/port`, which checks the mechanism on a desktop. | The GPU device of the console, its shader tool and its tests. |
| The plugin contract, with its C++ and Lua APIs. | The console plugin. |
| This guide, which names no console and no private SDK. | The build scripts, the private runners of continuous integration and everything else the license of the SDK covers. |

A port never patches the engine in its private repository to work around a missing piece. When a port needs something the mechanism lacks, the mechanism gains it here, in public terms, and every port profits.

## The folder of a port

The folder holds a `port.cmake`, which describes the port with one call of `haylen_port`, and the sources it names. The engine includes `port.cmake` before it picks its platform, so the description decides the platform, the runtime and the devices of the build.

```cmake
haylen_port(
  CONTRACT 6
  PLATFORM console
  SYSTEM_API posix
  THREADS ON
  CAPABILITIES dynamicLibraries
  VARN_DRIVERS SOCKET=EXTERNAL HTTP_CLIENT=EXTERNAL FS=STD
  VARN_DRIVER_SOURCES
    "SOCKET=${CMAKE_CURRENT_LIST_DIR}/varn/ConsoleSockets.cpp"
    "HTTP_CLIENT=${CMAKE_CURRENT_LIST_DIR}/varn/ConsoleHttpClient.cpp"
  RUNTIME_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/runtime/ConsoleHost.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/runtime/ConsoleRuntime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/runtime/ConsoleServices.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/runtime/ConsoleWindow.cpp"
  RUNTIME_LIBRARIES console-sdk::system
  DEVICE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/gpu/PortDevice.cpp"
  DEVICE_LIBRARIES console-sdk::graphics
  SHADER_LANGUAGES spirv
  SHADER_COMMAND "${CMAKE_CURRENT_LIST_DIR}/tools/port-shaders" --sdk "$ENV{CONSOLE_SDK}"
  HOST_TOOLS "${CMAKE_CURRENT_LIST_DIR}/tools/port-shaders"
  APP_FUNCTION console_add_app
  APP_MODULE "${CMAKE_CURRENT_LIST_DIR}/cmake/console-app.cmake"
  RELEASE_PROFILE console
  PLUGIN_PART console
  SDK ON
)
```

| Argument | What it gives |
| --- | --- |
| `CONTRACT` | The version of the contract of ports that the port follows. It must be `port::Contract::kVersion` of the [public API of ports](#the-public-api-of-ports), which `engine/cmake/haylen-port.cmake` reads into `HAYLEN_PORT_CONTRACT_VERSION` and which grows whenever that API or `haylen_port` change in a way that a port must follow, so a port of an older contract stops with a sentence that names both versions. |
| `PLATFORM` | The name of the platform in lowercase dash-case, which `HAYLEN_PLATFORM` takes and the services of the port report. It cannot be the name of a platform of the engine. |
| `SYSTEM_API` | The interfaces of the system that the engine compiles its system code against, `posix` or `windows`: the files of the content store with their flush to the disk, the local time, the handlers of native crashes and, with `dynamicLibraries`, the loader of native libraries. The portable code of the engine never asks which system it compiles for, so these are the only parts that a port takes from a system, and everything else of its platform comes through the public API of ports. |
| `THREADS` | `ON` by default, so the worker pools of Varn run the jobs of the engine, its physics steps and the opening of its audio output on threads of their own, whatever the event loop. `OFF` when the platform runs no thread besides the one that plays the frames, so all of that runs on that thread, which needs the event loop `PUMP`. |
| `CAPABILITIES` | What the platform allows apps to do, any of `dynamicLibraries`, which loads libraries at run time, `executableMemory`, which writes code into memory, and `processes`, which starts other processes. The engine answers each one the platform lacks where an app reaches it: without `dynamicLibraries`, `native.load` finds only the libraries linked into the app, which the backend links statically and registers by their names, without `processes`, `io.popen` and `os.execute` fail with a sentence that says so, and `port::Contract::allows` tells native code. Without `executableMemory`, neither `haylen.native` nor the module `ffi` of Varn, which the engine builds with `VARN_FFI_CLOSURES` off, makes a callback that native code calls, since their closures write code into memory, and both fail with the same sentence. Without `processes` the module `process` of Varn takes its driver that runs nothing. The SDK of the port records the capabilities, and the registry of the tooling checks them against what the backend of a target needs. |
| `VARN_DRIVERS` | The drivers of the Lua runtime Varn that the platform takes, as `<NAME>=<DRIVER>` for `EVENT_LOOP`, `SOCKET`, `HTTP_CLIENT`, `HTTP_SERVER`, `CRYPTO`, `PROCESS`, `FFI`, `FS`, `LOG`, `JSON` and `XML`, and `TLS=ON` for TLS. The engine builds the target `port` of Varn, whose every driver that the port does not name is the one that needs nothing of the system, such as no sockets, the portable crypto and the event loop `PUMP`, which [the event loop of a port](#the-event-loop-of-a-port) describes. `EVENT_LOOP=LIBUV` takes the loop of libuv, which needs `THREADS ON` and a toolchain that builds libuv. The WebSockets of the engine, which the development server reaches apps through, run on the sockets and the TLS of Poco on threads of their own, which `TLS=ON` builds with either event loop, and a platform without it opens none. |
| `VARN_DRIVER_SOURCES` | The sources of each driver `EXTERNAL` of `VARN_DRIVERS`, as `<NAME>=<path>`, which implement the public header of the module of Varn that every driver of it implements, such as `varn/socket/SocketTransport.h`, so a port brings the sockets, the HTTP transport, the crypto, the processes, the native calls or the files of its system. A transport that completes work through a callback of the system while the loop is pumped counts it in `varn::runtime::HostWork`. |
| `RUNTIME_SOURCES` | The sources of the runtime of the platform, which `haylen-runtime` compiles instead of the SDL platform layer, the Android runtime and the services of the engine. |
| `RUNTIME_LIBRARIES` | The libraries that the runtime links, such as the targets of the SDK. |
| `GRAPHICS_APIS` | The graphics APIs of the engine whose devices the port compiles, by their names in `gpu::Device::kApiNames`, such as `openGl`. |
| `DEVICE_SOURCES` | The sources of the device of the API `port`, which `haylen-gpu` compiles. They define `port::PortDevice::create`. |
| `DEVICE_LIBRARIES` | The libraries that the device links. |
| `SHADER_LANGUAGES` | The languages of `haylen-shaderc` that the shader tool of the port reads, such as `spirv` or `hlsl5`. The shader packages of the engine carry them. |
| `SHADER_COMMAND` | The command that runs on every shader package of the engine after `haylen-shaderc` wrote it, with the path of the package after its last argument. It writes the code of the device of the port into the package. |
| `HOST_TOOLS` | The programs of the port that run on the build machine, such as its shader tool, which the SDK of the port installs in `bin/`. |
| `APP_FUNCTION` | The CMake function that [`haylen_add_app`](../tools/embedding.md) calls to build an app of the platform, instead of the branches of the platforms of the engine. It receives the target, `SOURCES` with the sources of the app and its bootstrap, `LIBRARIES` with `haylen::runtime` and, for a Lua app, `haylen::player`, `CONTENT` with the folder that the app ships, and `RELEASE` when that folder is a protected release. |
| `APP_MODULE` | The CMake file that defines `APP_FUNCTION`, which the engine includes and the SDK of the port installs. |
| `RELEASE_PROFILE` | The content profile of the protected releases of the platform, in lowercase dash-case, which the targets of the backend of the port name so the tooling builds their releases for it. |
| `PLUGIN_PART` | The name of the part of plugins that the backend of the port reads, in lowercase dash-case, which plugins list in `implementations` and in the `platforms` of their contributions. |
| `SDK` | `ON` when the port builds and installs the SDK of its platform with `HAYLEN_BUILD_SDK`, which stops with a clear sentence otherwise. |

The portable code of the engine never asks which system it compiles for, so the platform folders of the repository stay out of the build of a port even where the compiler of the port targets their system, and `haylen-gpu` compiles with `HAYLEN_GPU_PORT` when the port has a device. A port needs `CONTRACT`, `SYSTEM_API`, `RUNTIME_SOURCES` and at least one graphics API: its own device, the devices of the engine it names, or both. A port with a device of its own needs `SHADER_LANGUAGES` and `SHADER_COMMAND`, and an `APP_FUNCTION` needs its `APP_MODULE`. The configuration stops with a clear sentence when one is missing, when a capability or a host tool is unknown, and when a port without an `APP_FUNCTION` builds an app.

Build the engine with the port through CMake:

```sh
cmake -S path/to/haylen -B build/console -DHAYLEN_PORT_DIR=path/to/haylen-console -DCMAKE_TOOLCHAIN_FILE=path/to/console.toolchain.cmake -DHAYLEN_BUILD_TESTS=OFF
cmake --build build/console --target haylen
```

### The event loop of a port

Varn runs the promises, timers and callbacks of an app on its event loop. The engine polls that loop once per frame within a budget, so the loop never waits on the system inside a frame, and every port drives it from its own run loop through `port::Player::frame`. A port gets the `PUMP` loop by default, which needs nothing of the system: the frames pump it, and the network drivers of the port are external drivers that complete their work through `varn::runtime::HostWork` or post it to the loop. The event loop and the threads are independent. With `THREADS ON`, the worker pools of Varn run the jobs of the engine on threads of their own, and a worker that finishes a job hands what follows to the loop, which the next frame runs. With `THREADS OFF`, the frames run the jobs inside the pump, as on the web. So no port needs an event loop of its own, and a console with threads runs the jobs, the physics and the opening of the audio output off the thread of the frames like any other platform.

### The SDK of a port

A port with `SDK ON` installs the SDK of its platform like any other platform, with `-DHAYLEN_BUILD_SDK=ON` and `cmake --install <build> --component haylen-sdk`. The SDK records the port in `share/haylen/haylen-port.json`, installs its host tools in `bin/` and its `APP_MODULE` in `share/haylen/cmake/port/`, and its package config sets `HAYLEN_PORT_PLATFORM`, `HAYLEN_PORT_CAPABILITIES` and `HAYLEN_PORT_APP_FUNCTION` and includes the module, so `haylen_add_app` builds the apps of the port from the SDK alone.

```json
{
  "platform": "console",
  "contract": 6,
  "capabilities": ["dynamicLibraries"],
  "hostTools": ["port-shaders"],
  "appFunction": "console_add_app",
  "releaseProfile": "console",
  "pluginPart": "console"
}
```

The engine port of the console reads that file into `haylen-sdk.json`, the manifest that every SDK of Haylen holds and that the tooling selects SDKs by: the port, its version, the engine version, the revision of the engine sources, the configuration, and one slice for each system and environment with its architectures, binary interface, toolchain, C++ runtime, oldest system version, graphics APIs, capabilities and files, besides the host tools with the system and architecture they run on. An app build never compiles the engine, so a missing slice, a slice of other engine sources or another configuration stops it with the `haylen sdk build` command to run.

## The tooling of a port

The command line `haylen` finds every platform through two contracts of `tools/haylen/platforms`, which a port implements in a Python package of its private repository:

| Contract | What it does | Entry point group |
| --- | --- | --- |
| `EnginePort` | Builds the engine of the platform and installs its SDK. `describe` returns a `PortDescription` with the name, the contract version, the version of the port, its tools and its `PortTarget`s. `check_environment` finds the tools that one of its targets needs on this machine. `build_sdk` takes an `SdkRequest` with the targets, the configuration, the architectures, the revision of the engine sources and the folder, builds the engine, installs the SDK and returns its `SdkManifest`. The `capabilities` of the description name its optional operations: `join` offers `join_sdks`, which `haylen sdk join` calls with an `SdkJoinRequest` to join SDKs of the port that hold different slices into one. | `haylen.engine_ports` |
| `PlatformBackend` | Builds apps against that SDK. `describe` returns a `PlatformDescription` with its targets, capabilities, tools, contribution types and the sections of `platforms` in `haylen.json` it reads. `check_environment`, `prepare`, `build` and `verify` turn a resolved product into a generated project, a built product and a verification report, and `devices`, `run`, `package` and `sign` are optional capabilities. | `haylen.backends` |

A `PortTarget` declares its name, its system, its environment (`device`, `simulator` or `host`), its architectures with their binary interfaces, its toolchain, its C++ runtime, its oldest system version, its graphics APIs, its shader languages and the capabilities of the system that the engine uses there. A backend target declares the same system, environment, binary interfaces and toolchain, and the capabilities it needs. The registry matches a backend target with the port target that builds its engine by those properties and capabilities, never by their names, so the two pieces of a port can name their targets as they like. Installing one never implies the other, and `haylen doctor --target <name>` shows both with what this machine lacks.

Each contract has its own version, `Contract.ENGINE_PORT` and `Contract.BACKEND`, apart from the version of the engine and of its SDKs. An extension that declares another version stops every command that needs it with a sentence that names both versions, and the registry never tries another extension in its place. The core of the tooling never imports a backend or a port by name, so the internal ones of `tools/haylen/platforms/<name>/` register through the same entry points as a private one.

The Python package of a port declares its two classes in its `pyproject.toml`:

```toml
[project]
name = "haylen-console"
version = "1.0.0"
requires-python = ">=3.15"

[project.entry-points."haylen.engine_ports"]
console = "haylen_console.console_port:ConsolePort"

[project.entry-points."haylen.backends"]
console = "haylen_console.console_backend:ConsoleBackend"
```

The package goes into the Python environment that runs the tooling, such as a uv project of the private repository that depends on the Haylen checkout through a path and runs `uv run haylen`. Installing it runs its code with the trust of the developer who installs it. Its targets then take part in every command, such as `haylen sdk build --target console` and `haylen app run --target console`, and its backend generates the project of the platform with the tool that the SDK of the console requires, which may be CMake with `haylen_add_app` or another build system.

## The public API of ports

A port reaches the engine through the headers of `engine/include/haylen/port/`, in the namespace `haylen::port`, and the other public headers of the engine, which every SDK installs. It never includes a header of `engine/src` or compiles a source of the engine, so a port builds against an SDK as well as against a checkout. The API is stable in the sense of the [API policy](../tools/api-policy.md#the-api-of-ports), and `port::Contract::kVersion` names its version, which the `CONTRACT` of `haylen_port` must match.

| Header | What it gives a port |
| --- | --- |
| `Contract.hpp` | `port::Contract`, the version of the contract of ports. |
| `Host.hpp` | `port::Host`, the window that the port implements: the size and scale of its framebuffer, fullscreen, the title, the cursor, the mouse lock, the clipboard, the text input, closing, whether an app may quit itself, the splash, the halts of the debugger, the controllers, the screen saver, the frame rate, the pixel density cap and the GPU device. |
| `Player.hpp` | `port::Player`, which installs the services and plays apps for the runtime of the port: `installServices`, `create`, `getGraphicsApis`, `initialize`, `frame`, `handleEvent`, `postEvent`, `cleanup`, `getGpuDevice` and `getExitStatus`. |
| `Services.hpp` and the services | `port::Services`, the services of the platform by capability, which [the services of a port](#the-services-of-a-port) lists. |
| `SystemReports.hpp` | `port::SystemReports`, what the system tells the engine from any thread: the theme, the battery, the power saving mode, the time zone, the languages, the accessibility settings, memory warnings, the links that open the app and the actions of the screen reader. |
| `BridgeReplies.hpp` | `port::BridgeReplies`, the answers, events and screen endings of the native parts of plugins, from any thread. |
| `GpuDevices.hpp` | `port::GpuDevices`, which makes the device of a graphics API of the build, names an API and makes the checking device of `createChecking`, which draws nothing and checks every call against the contract of `gpu::Device`. |
| `PortDevice.hpp` | `port::PortDevice`, the device of the API `port` that a port with a graphics API of its own defines. |

The value types that the API takes are public too, such as `platform::Event`, `platform::SystemInfo`, `platform::Display`, `platform::WindowStyle`, `platform::SystemFont`, `platform::PixelDensityCap`, `gpu::Surface` and `gpu::DeviceOptions`.

## The platform runtime

The runtime of a port does what every platform layer does, as the [architecture guide](../core/architecture.md#layers) describes:

1. **A window** derived from `port::Host`, which the engine wraps in its own native host, so the development session, the style of the window, the share of the text input between the editors and the plain keyboard and every other part that the engine holds of a host stay the same on every platform.
2. **The services** of the platform, a `port::Services`, which the runtime installs once with `port::Player::installServices` before anything else of the engine runs.
3. **A runtime** that holds the entry point of the process. It installs the services, calls `port::Player::create` with the command line and its window, opens the window for the APIs of `port::Player::getGraphicsApis` until a device starts, calls `port::Player::initialize`, then `port::Player::frame` once a frame, hands events of the system to `port::Player::handleEvent`, replaces a device that `isLost` reports and calls `port::Player::cleanup` when the app ends. A port maps the lifecycle of its system onto the events of the engine: a suspend of the console is `Event::Type::Suspended` and its resume `Event::Type::Resumed`, which move the app between the states that the [lifecycle guide](../core/lifecycle.md) describes.

The runtime reaches the system of the console through its SDK, or through SDL 3 where the maker of the console provides its port of SDL to licensed developers. A runtime on SDL can start from the SDL platform layer of the engine, `engine/src/platform/sdl/`, as a copy in the private repository that it moves onto the public API, and keep its own changes there.

The system info of a port reports the operating system `'port'`, the value `SystemInfo::Os::Port`, and the platform name of `PLATFORM` tells which platform it is. A console on a TV reports the device kind `tv`.

### The services of a port

Every platform, of the engine or of a port, registers the services of the capabilities it has, and the engine answers in one place for each one it lacks, so an empty implementation never reads as one that worked. One object may implement several services, which the fields of `port::Services` then share.

| Service | What it covers | What the engine answers without it |
| --- | --- | --- |
| `PlatformService` | The name of the platform, starting and stopping the following of the system, the package that ships with the app, the folder of user data, committing it, the system info and whether the device has a pointer. | Required. |
| `DisplayService` | The displays of the device, the display of the window, the displays that a browser hides until the person lets the page see every screen, the request for them and the frame of the window. | Required. |
| `DesktopService` | A desktop of windows: the style, background, frame, passthrough and drag of the window, following it and its native handles for native libraries. | A window that fills its screen, with null native handles. |
| `ScreenService` | The safe area, the orientation, the status bar, the gestures of the system, a fold and cutouts. | A landscape screen, safe everywhere, without a status bar, gestures, fold or cutouts. |
| `BridgeService` | The native parts of plugins in the language of the platform, their calls, their screens and the failures that stopped the app. | Calls and screens that fail with the code `noHandler`. |
| `NetworkService` | Why the platform blocked a connection and what the project lacks for network access. | Nothing to explain. |
| `LinkService` | The links of a platform that starts a new process of the app for every link. | No process forwards a link. |
| `MediaService` | The now-playing controls and the audio of an app in the background. | No controls, and frames that keep running in the background. |
| `AccessibilityService` | The screen reader. | A tree that nobody wants. |
| `VideoService` | The decoder of video files. | No decoder, so a video fails with the sentence of the video system. |
| `FontService` | The fonts of the system. | No font of the system. |
| `AudioOutput` | The output that plays the mix of each app, as [the audio output of a port](#the-audio-output-of-a-port) describes. | A mixer that keeps time without sound, as a device that nobody hears would play. |

The platforms of the engine register their services the same way, so the table is also the map of what each platform offers.

### The audio output of a port

The audio output is the service that plays sound. The engine mixes every voice of an app into blocks of interleaved 32-bit float samples, and a `port::AudioOutput` takes them to the speakers of the console. The mixer calls `open` with the sample rate, the channels and the latency the app asks for. The output answers with a `Playback` that reports its own format: its sample rate, its channels, its period in frames and the periods it keeps ahead of the speakers. The engine converts the mix to that format, and apps read the latency from the periods. The mixer opens the output on a thread of its own, so `open` may wait for the system, and throws `std::runtime_error` with a sentence when the console refuses sound, which the log shows while the app goes on without sound.

The playback reads the mix through the `Stream` it was given, from the one thread it plays on, between `start` and `stop`. That thread is the audio thread of the system, or the run loop of the port on a platform without one, which reads a period after each frame. A playback that stops working later says why through `getFailure`, and one that waits for a gesture of the person before it plays says so through `isLocked`.

```cpp
class ConsolePlayback final : public port::AudioOutput::Playback {
  public:
    explicit ConsolePlayback(port::AudioOutput::Stream& source) : stream(source) {}

    port::AudioOutput::Format getFormat() const noexcept override {
        return {.sampleRate = 48000, .channels = 2, .periodFrames = 256, .periods = 3};
    }
    void start() noexcept override; // The audio thread of the console calls `stream.render` for each period.
    void stop() noexcept override;
    bool isLocked() const noexcept override {
        return false;
    }
    std::optional<std::string> getFailure() const override {
        return std::nullopt;
    }

  private:
    port::AudioOutput::Stream& stream;
};
```

The test port registers an output whose run loop reads one period after each frame, at a rate and a period of its own, and its app checks that the output plays with the latency of that output.

## The GPU device

### A graphics API of the engine

A platform whose graphics API the engine already has names it in `GRAPHICS_APIS`, and the engine compiles that device as it does on its own platforms. The device still needs a surface it knows:

- The [OpenGL device](../graphics/opengl.md) draws through the `gpu::GlContext` that the platform makes. A port whose console runs OpenGL 4.3 core or OpenGL ES 3.0 implements `gpu::GlContext` in its runtime, hands it over in a surface of the kind `Gl` and needs no device code.
- The [Vulkan device](../graphics/vulkan.md) makes surfaces for the windows of Windows, Linux and Android only, and needs Vulkan 1.1. A console with Vulkan whose surface is of another kind needs a device of its own.
- The [Direct3D 12 device](../graphics/direct3d.md) is written against the Direct3D 12 of Windows and compiles HLSL when programs are made, with `d3dcompiler_47.dll`. A console whose graphics API differs from it keeps an adapted copy as its device of the API `port`.

### A device of its own

A port whose graphics API the engine has no device of defines the class `port::PortDevice` of `haylen/port/PortDevice.hpp`:

```cpp
#include "haylen/port/PortDevice.hpp"

#include "ConsoleDevice.hpp"

namespace haylen::port {

std::unique_ptr<gpu::Device> PortDevice::create(const std::optional<gpu::Surface>& surface, const gpu::DeviceOptions& options) {
    return std::make_unique<ConsoleDevice>(surface, options);
}

} // namespace haylen::port
```

The device implements the whole contract of `gpu::Device` and reports `Api::Port` from `getApi` and `Language::Port` from `getLanguage`. Its runtime hands it the window in a surface of the kind `Surface::Kind::Port`, whose `window` and `display` hold whatever native handles the device reads. The factory prefers the API `port` to every other API of the build, and the [RHI guide](../graphics/rhi.md) lists what each part of the contract asks of a device. A device can start as the checking device of `port::GpuDevices::createChecking`, which reports the API it is given, so the runtime of a port runs apps before its device draws, as the test port does.

### Why the API and the language are reserved values

The enums `gpu::Device::Api` and `gpu::Device::Language` stay closed, with one value for every port: `Port`. A build targets one platform, so it holds at most one port, and one reserved value names its device in every build. The closed enums keep their one table of names each, `kApiNames` for the APIs and the names of the languages in shader packages, so `haylen.json`, `--graphics-api`, the shader packages and Lua all read the same names. They keep the arrays that the engine indexes by language, and the validation of `window.graphicsApis` stays in the engine library, which runs before any device exists and never links the devices. A registration of APIs at run time would need tables of names that grow while the app runs, which the validation of `haylen.json` could not read, for a choice that a build makes only once. The name of the platform comes from the platform name and from `getAdapterName` of the device instead.

## Shaders

`haylen-shaderc` compiles the shaders of the engine into packages with the code of every language of the build, as the [shader guide](../graphics/shaders.md) explains. The compiler is public and writes only public languages, so a port compiles its own code from one of them:

1. `SHADER_LANGUAGES` names the language its tool reads, such as `spirv` for SPIR-V or `hlsl5` for HLSL text, and every package of the engine carries it.
2. After `haylen-shaderc` writes a package, the build runs `SHADER_COMMAND` on it. The command reads the code of each program, compiles it with the shader compiler of the SDK of the console, through a translator such as SPIRV-Cross where that compiler reads another language, and writes the result into the package as the target `port` of the program.
3. The device then receives the code of the `port` target as bytes in `StageCode::bytecode`, with the descriptor set and the binding of each slot.

The target `port` has the shape of the target `spirv`: its stages name sources of the package that hold the Base64 text of the code, and its `blocks`, `textures` and `samplers` give each slot an object with `set` and `binding`. The test port shows the smallest shader tool, `engine/tests/data/port/port-shaders.cmake`, which copies the `spirv` target as the `port` target.

The shaders of apps take the `port` target the same way. The SDK of a port lists its shader tool among its host tools with the name `port-shaders`, whatever the suffix of its file, as the test port does with `port-shaders.cmake`. When the SDK of the target of an app holds that tool for this machine, the tooling runs it on every `.shader` package of the app after `haylen-shaderc` writes it, with the path of the package as its last argument and a CMake script through `cmake -P`, and it compiles a package that lacks the target again, so `app prepare`, `app build`, `app run` and the development server give every package of a port app its `port` target.

```json
{
  "targets": {
    "port": {
      "vertex": {"source": 4, "entry": "main"},
      "fragment": {"source": 5, "entry": "main"},
      "blocks": {"0": {"set": 0, "binding": 0}},
      "textures": {"0": {"set": 1, "binding": 0}},
      "samplers": {"0": {"set": 1, "binding": 1}}
    }
  }
}
```

The custom shaders of apps are packages too, which `haylen app shaders` compiles. The backend of a port runs the shader tool of the SDK of the port, one of its host tools, on the `.shader` files of an app before it packs the app for the console, since the device of the port reads only the `port` target.

## Testing a port

The command `haylen test --port` builds the engine with the test port of `engine/tests/data/port` in `build/engine/<host>-port-<config>` and plays the app of `engine/tests/data/port-app` on it. The test port has the platform `test-port`, a runtime that plays the app without a window, services that answer like a console without plugins, and a device of the API `port` built on `gpu::NullDevice`, which checks every call of the engine against the contract and runs the code that its shader tool wrote. The test port runs threads with the `PUMP` loop of Varn, which its frames pump, and builds the target `port` of Varn with the drivers of the desktop that need no network and an external driver of processes of its own, which starts nothing. Its services register an audio output, which its run loop reads after each frame. The app checks the platform, the operating system and the graphics API, that `io.popen` refuses to start a program, since the test port does not allow `processes`, that `ffi.cast` of a Lua function refuses to make a callback, since it does not allow `executableMemory`, and that the module `process` runs on the external driver. It checks in a trace that its jobs run on a worker of the pools rather than on the thread of the frames, draws shapes, sprites and text until its sound plays through the output of the port with the latency of that output, and quits with the status `0`. The command `haylen test --port --sanitizers thread` plays the same app under ThreadSanitizer. The runtime fails a run that played no sound. It needs no SDK, so it builds on the desktops of the public repository and keeps the mechanism working for every port.

```sh
haylen test --port
```

The tests of the tooling prove the other half with the fake backend and the fake engine port of `tools/tests/support`. The fake port names its target differently from the target of the fake backend, so the tests show that the registry matches them by what they describe, refuses a port that lacks a capability of the backend, refuses an extension of another contract version and resolves products for a platform that the core never names. Run them with `uv run pytest tools/tests`, as the [testing guide](../tools/testing.md) describes.

### The proof of an external port

The proof of an external port checks both halves together, installed the way a private repository installs them. The folder `engine/tests/data/port` is also the Python package `haylen-test-port`, whose engine port `testport.port_engine:PortEngine` builds the SDK of the test port with `HAYLEN_PORT_DIR` and `HAYLEN_BUILD_SDK=ON`. The folder `tools/tests/data/test-port-backend` holds the package `haylen-test-port-backend`, whose backend `testportbackend.port_backend:PortBackend` declares the target `test-port`, the contribution type `testPortPart` and the section `platforms.test-port`. Neither the command line nor the core names them.

The test `tools/tests/extensions/test_external_port.py` copies both packages to a temporary folder outside the repository. It makes an environment with uv that holds the tooling of the checkout in editable mode, the engine port in editable mode, since it reads its CMake port from the folder of its project, and the backend as a wheel. Then it runs the `haylen` of that environment:

1. `haylen doctor --target test-port` finds the backend and the engine port and matches them by their system, environment, toolchain and binary interfaces, while their target names differ.
2. `haylen sdk build --target test-port --config Debug` builds the SDK of the test port. Its slice takes the capabilities of `share/haylen/haylen-port.json`, and its shader tool becomes a host tool.
3. `haylen app build`, `app verify` and `app run` build the app of `tools/tests/data/test-port-app`. Its CMake project calls `haylen_add_app`, which calls the app function of the port, and links the part of its plugin, a C++ source that Lua calls through `native.load`. The verification checks that the executable holds the content of the app and registers the part. The app checks the platform, the operating system, the graphics API, a file of its content and the answer of its plugin, plays without a window and quits with the status `0`.
4. The files of the command line, the core and the internal backends are the same before and after, and none of them names `test-port`.

The same test checks the failures that a port meets, each with its typed error and its sentence: a machine without CMake (`missingToolchain`), a backend of another contract version (`incompatibleExtension`), a plugin without an implementation for `test-port` (`unsupportedTarget`), a capability that the backend does not declare, such as `package` (`missingCapability`), an architecture that the target does not build (`unsupportedTarget`), and an SDK whose slice lacks its files (`incompatibleSdk`). It builds the engine, so the usual run of the tests leaves it out, and the job `port` of continuous integration runs it on Linux. The variable `HAYLEN_JOBS` sets the jobs of its builds.

```sh
HAYLEN_JOBS=4 uv run pytest -m port tools/tests/extensions
```

The test port includes only public headers of the engine and compiles no source of the engine, which the test `tools/tests/repository/test_port_headers.py` checks, so it proves that a port builds on the public API of ports alone.

A port tests its own device the way the engine tests its devices, with the conformance suite and the reference images of `engine/tests/gpu`, which the testing guide also describes. The engine builds those tests for desktops, the web and Android, so a port builds them with the toolchain of its console in its private repository and runs them on a development kit.

## Continuous integration

The public continuous integration of Haylen never sees the SDK of a console, and its job `port` runs [the proof of an external port](#the-proof-of-an-external-port), which checks the mechanism of ports and their tooling without one. A port runs its own builds and tests on private runners, such as self-hosted runners of its private repository on machines that hold the SDK and reach a development kit:

1. Check out Haylen at the commit the port supports and the port next to it.
2. Install the Python package of the port into the environment of the tooling, and build the SDK of the console with `haylen sdk build`.
3. Build the player and the GPU tests of the port with the toolchain of the console and `HAYLEN_PORT_DIR`.
4. Run them on the development kit, and keep logs, captures and reference images in the private repository.

Keep the runners, their secrets and their logs private, since they hold what the license of the SDK covers.

## Getting access to an SDK

The maker of each console gives its SDK, its development kits and its documentation only to studios that it approves, under a non-disclosure agreement, the NDA. A studio applies to the developer program of the console, usually as a company with a game to show, and receives the SDK, the development hardware and the requirements that a game passes before release once it is approved. Everything after that happens under the agreement, in the private repositories of the port and the plugin. A studio that holds the agreement of a console can also ask the SDL project for its private port of SDL 3 for that console, which the SDL maintainers share after they confirm the agreement.

## A port step by step

These steps are the same for every console:

1. **Get access.** Apply to the program of the console, sign its agreement and receive the SDK, the development kit and the documentation.
2. **Make the private repositories.** Create one for the port, such as `haylen-console`, with `port.cmake`, a `runtime/` folder for the host, the window, the entry point and the services, a `gpu/` folder for the device when the console needs one, a `tools/` folder for the shader command, the CMake toolchain file of the SDK, the Python package of its engine port and its backend, and the scripts of its continuous integration. Create another one for the console plugin.
3. **Build the test port first.** Run `haylen test --port` on a desktop to see the mechanism work, and read `engine/tests/data/port`, the smallest complete port, before writing your own.
4. **Write the runtime.** Implement the host, the window, the runtime and the services that [the platform runtime](#the-platform-runtime) lists, on the SDL port of the console or on its SDK alone. Start every service with the answer its contract gives for a platform that lacks it, then fill in the services the console has.
5. **Choose the GPU device.** Name a graphics API of the engine in `GRAPHICS_APIS` when the console runs one, or write `port::PortDevice` for its own graphics API, as [the GPU device](#the-gpu-device) explains. Start from `gpu::NullDevice` so apps run before the device draws.
6. **Write the shader command.** Pick the language of `haylen-shaderc` that the shader compiler of the SDK reads, name it in `SHADER_LANGUAGES`, and make `SHADER_COMMAND` write the compiled code into the `port` target, as [the shaders section](#shaders) shows.
7. **Write the engine port and the backend.** Implement `EnginePort` so `haylen sdk build` installs the SDK of the console with its manifest, and `PlatformBackend` so `haylen app build` generates and builds the project of an app with the tool of the SDK, as [the tooling of a port](#the-tooling-of-a-port) explains, and check both with `haylen doctor`.
8. **Build and play.** Build the SDK, then play the app of `engine/tests/data/port-app` on the development kit until it quits with the status `0`.
9. **Run the GPU tests.** Build the conformance suite of `engine/tests/gpu` with the toolchain of the console and run it on the kit until every reference image matches.
10. **Play real apps.** Play the test project of `samples/tests` and the sample games on the kit with a controller, and fix in the port what differs from the other platforms.
11. **Write the console plugin.** Bring users and sign-in, achievements, the store, the saves of each user, the storage quota and the setup of controllers with their glyphs through the plugin contract, so apps reach them from Lua and C++.
12. **Meet the requirements of the console.** Every console has rules that a game passes before release, such as how it suspends and resumes, how users sign in, where it saves, and what it shows when a controller disconnects. The runtime and the plugin carry them, so apps meet them without code of their own.
13. **Pack the app.** The `package` capability of the backend puts the app package, or its protected release, inside the package of the console with the packaging tool of the SDK, and the runtime opens it through `Services::openBundledPackage`.
14. **Automate it.** Run steps 8 to 10 on private runners for every commit of Haylen that the port follows, as [continuous integration](#continuous-integration) describes.

## Other platforms

The steps above fit any platform. What decides between a port and a platform of the repository is the license of its SDK:

- **A platform whose SDK is public**, such as a new operating system for phones, computers or TVs, becomes a platform of the repository: a platform folder under `engine/src/platform/`, on SDL where SDL supports it, a GPU device under `engine/src/gpu/` when its graphics API is new, and a backend and an engine port under `tools/haylen/platforms/<name>/` with their template and their entry points in `pyproject.toml`, as `AGENTS.md` describes.
- **A platform whose SDK is private**, such as a console or a device under an agreement, becomes a port, with everything this guide describes kept in its private repositories.

Every port, whatever the platform, implements the same parts of the engine:

| Part | What the port gives |
| --- | --- |
| Entry point and frames | The runtime that installs the services and calls `port::Player::create`, `port::Player::initialize`, `port::Player::frame`, `port::Player::handleEvent` and `port::Player::cleanup`. |
| Window | The window derived from `port::Host`, with the size and scale of the framebuffer, the pixel density cap, focus, closing, the screen saver and the frame rate. |
| Input | Controllers, and the keyboard, the mouse and touch where the platform has them, handed to the player as events, and the text input of `port::Host::getTextInput` with the keyboard of the system. |
| Files | The package that ships with the app, the folder of user data, which `PlatformService::persistUserData` commits where the platform saves on request, and the folder of the content store. |
| System | The name of the platform, `PlatformService::getSystemInfo` with the device kind, the displays, the safe area of the screen where it has one and the reports of `port::SystemReports`, such as the theme, the battery and the languages. |
| Lifecycle | The app states `active`, `inactive` and `background`, suspend and resume, and low memory, handed to the player as events. |
| Audio | The `port::AudioOutput` of the services, which plays the mix of each app on the audio system of the console. |
| Graphics | A device of the engine named in `GRAPHICS_APIS`, or `port::PortDevice` with the shader command. |
| Optional services | The services of [the services of a port](#the-services-of-a-port) that the platform has, such as video, accessibility, the media session and the bridge of native plugins, while the engine answers for each one it lacks. |
| Tooling | The engine port that builds the SDK and the backend that generates, builds, verifies, runs and packages apps. |

## Limits

- Every dependency of the engine must build with the toolchain of the console, such as Varn with Lua, libuv for a port that takes its loop, Poco with its TLS stack, SDL where the port uses it, and the libraries of fonts, images and audio. A dependency that needs a change for a console gets it in the private repository of the port, through a patch of its own.
- Native plugins have parts for the platforms they list in `implementations`, so a plugin that a console app uses gets its console part in a private repository too, and a plugin that does not list the platform of the port fails the build of that platform.
- The backends of the repository build apps for the platforms of the repository. Apps of a port build, run and pack only through the backend of the port, with the tools of the SDK of its console.
