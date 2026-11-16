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
| The mechanism of ports: `HAYLEN_PORT_DIR`, `haylen_port`, `gpu::PortDevice`, the reserved values `Api::Port`, `Language::Port`, `Surface::Kind::Port` and `SystemInfo::Os::Port`, and the `port` targets of shader packages. | The `port.cmake` of the port. |
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
  CONTRACT 1
  PLATFORM console
  CAPABILITIES dynamicLibraries
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
| `CONTRACT` | The version of the contract of ports that the port follows. It must be `HAYLEN_PORT_CONTRACT_VERSION` of `engine/cmake/haylen-port.cmake`, which grows whenever `haylen_port`, `platform::NativeHost`, `platform::Services` or `gpu::PortDevice` change in a way that a port must follow, so a port of an older contract stops with a sentence that names both versions. |
| `PLATFORM` | The name of the platform in lowercase dash-case, which `HAYLEN_PLATFORM` takes and the services of the port report. It cannot be the name of a platform of the engine. |
| `CAPABILITIES` | What the platform allows apps to do, any of `dynamicLibraries`, which loads libraries at run time, `executableMemory`, which writes code into memory, and `processes`, which starts other processes. The SDK of the port records them, and the registry of the tooling checks them against what the backend of a target needs. |
| `RUNTIME_SOURCES` | The sources of the runtime of the platform, which `haylen-runtime` compiles instead of the SDL platform layer, the Android runtime and the services of the engine. |
| `RUNTIME_LIBRARIES` | The libraries that the runtime links, such as the targets of the SDK. |
| `GRAPHICS_APIS` | The graphics APIs of the engine whose devices the port compiles, by their names in `gpu::Device::kApiNames`, such as `openGl`. |
| `DEVICE_SOURCES` | The sources of the device of the API `port`, which `haylen-gpu` compiles. They define `gpu::PortDevice::create`. |
| `DEVICE_LIBRARIES` | The libraries that the device links. |
| `SHADER_LANGUAGES` | The languages of `haylen-shaderc` that the shader tool of the port reads, such as `spirv` or `hlsl5`. The shader packages of the engine carry them. |
| `SHADER_COMMAND` | The command that runs on every shader package of the engine after `haylen-shaderc` wrote it, with the path of the package after its last argument. It writes the code of the device of the port into the package. |
| `HOST_TOOLS` | The programs of the port that run on the build machine, such as its shader tool, which the SDK of the port installs in `bin/`. |
| `APP_FUNCTION` | The CMake function that [`haylen_add_app`](../tools/embedding.md) calls to build an app of the platform, instead of the branches of the platforms of the engine. It receives the target, `SOURCES` with the sources of the app and its bootstrap, `LIBRARIES` with `haylen::runtime` and, for a Lua app, `haylen::player`, `CONTENT` with the folder that the app ships, and `RELEASE` when that folder is a protected release. |
| `APP_MODULE` | The CMake file that defines `APP_FUNCTION`, which the engine includes and the SDK of the port installs. |
| `RELEASE_PROFILE` | The content profile of the protected releases of the platform, in lowercase dash-case, which the targets of the backend of the port name so the tooling builds their releases for it. |
| `PLUGIN_PART` | The name of the part of plugins that the backend of the port reads, in lowercase dash-case, which plugins list in `implementations` and in the `platforms` of their contributions. |
| `SDK` | `ON` when the port builds and installs the SDK of its platform with `HAYLEN_BUILD_SDK`, which stops with a clear sentence otherwise. |

The runtime of a port compiles with the definition `HAYLEN_PORT`, which keeps the platform folders of the repository out of the shared player even where the compiler of the port targets their system, and `haylen-gpu` compiles with `HAYLEN_GPU_PORT` when the port has a device. A port needs `CONTRACT`, `RUNTIME_SOURCES` and at least one graphics API: its own device, the devices of the engine it names, or both. A port with a device of its own needs `SHADER_LANGUAGES` and `SHADER_COMMAND`, and an `APP_FUNCTION` needs its `APP_MODULE`. The configuration stops with a clear sentence when one is missing, when a capability or a host tool is unknown, and when a port without an `APP_FUNCTION` builds an app.

Build the engine with the port through CMake:

```sh
cmake -S path/to/haylen -B build/console -DHAYLEN_PORT_DIR=path/to/haylen-console -DCMAKE_TOOLCHAIN_FILE=path/to/console.toolchain.cmake -DHAYLEN_BUILD_TESTS=OFF
cmake --build build/console --target haylen
```

### The SDK of a port

A port with `SDK ON` installs the SDK of its platform like any other platform, with `-DHAYLEN_BUILD_SDK=ON` and `cmake --install <build> --component haylen-sdk`. The SDK records the port in `share/haylen/haylen-port.json`, installs its host tools in `bin/` and its `APP_MODULE` in `share/haylen/cmake/port/`, and its package config sets `HAYLEN_PORT_PLATFORM`, `HAYLEN_PORT_CAPABILITIES` and `HAYLEN_PORT_APP_FUNCTION` and includes the module, so `haylen_add_app` builds the apps of the port from the SDK alone.

```json
{
  "platform": "console",
  "contract": 1,
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
| `EnginePort` | Builds the engine of the platform and installs its SDK. `describe` returns a `PortDescription` with the name, the contract version, the version of the port, its tools and its `PortTarget`s. `check_environment` finds the tools that one of its targets needs on this machine. `build_sdk` takes an `SdkRequest` with the targets, the configuration, the architectures, the revision of the engine sources and the folder, builds the engine, installs the SDK and returns its `SdkManifest`. | `haylen.engine_ports` |
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

## The platform runtime

The runtime of a port does what every platform layer does, as the [architecture guide](../core/architecture.md#layers) describes:

1. **A host** derived from `platform::NativeHost`. It adds what the window does: the size and scale of the framebuffer, fullscreen, the title, the cursor, the clipboard, closing the window, its focus, `endSplashScreen`, which takes the splash screen of the platform away once the app drew its first complete frame, keeping the screen on, pacing its frames at the frame rate of the app in `applyFrameRate`, sizing its drawable for the pixel density cap of the app in `applyMaxPixelDensity` with the `gpu::Surface::Size` that its device takes, the controllers, the text input of the services, which it hands to the constructor of `platform::NativeHost`, where the engine shares it between its editors and the plain keyboard, `prepareGpuDevice`, which answers `true` once the device of the next app can be created, `createGpuDevice`, which hands over the device that draws into the window, and `pumpHaltedEvents`, which keeps the platform responsive while the [debugger](../tools/debugger.md) halts the frame thread and answers whether the platform asks the frame to finish.
2. **A window** derived from `platform::NativeWindow`, installed with `NativeWindow::install` while it is open, which hands the services its native handles.
3. **A runtime** that holds the entry point of the process. It first calls `CrashGuard::install`, so the handlers of native crashes are in place before the native parts of plugins load, as the [plugin guide](plugins.md) describes. It then calls `Player::create` with the command line and the host, opens the window for the APIs of `Player::getGraphicsApis` until a device starts, calls `Player::initialize`, then `Player::frame` once a frame, hands events of the system to `Player::handleEvent`, replaces a device that `isLost` reports and calls `Player::cleanup` when the app ends.
4. **The services** of the platform: the definitions of the static methods of `platform::Services` in `engine/src/platform/Services.hpp`. They tell the name of the platform, open the package that ships with the app, give the folders of user data and of downloaded content, report what the device is, answer the bridge, run text input and read video. A service the platform does not offer answers the way the contract of its method says, such as a screen that fails with `noHandler`.

The runtime reaches the system of the console through its SDK, or through SDL 3 where the maker of the console provides its port of SDL to licensed developers. A runtime on SDL can start from the SDL platform layer of the engine, `engine/src/platform/sdl/`, as a copy in the private repository, and keep its own changes there.

The system info of a port reports the operating system `'port'`, the value `SystemInfo::Os::Port`, and the platform name of `PLATFORM` tells which platform it is. A console on a TV reports the device kind `tv`.

## The GPU device

### A graphics API of the engine

A platform whose graphics API the engine already has names it in `GRAPHICS_APIS`, and the engine compiles that device as it does on its own platforms. The device still needs a surface it knows:

- The [OpenGL device](../graphics/opengl.md) draws through the `gpu::GlContext` that the platform makes. A port whose console runs OpenGL 4.3 core or OpenGL ES 3.0 implements `gpu::GlContext` in its runtime, hands it over in a surface of the kind `Gl` and needs no device code.
- The [Vulkan device](../graphics/vulkan.md) makes surfaces for the windows of Windows, Linux and Android only, and needs Vulkan 1.1. A console with Vulkan whose surface is of another kind needs a device of its own.
- The [Direct3D 12 device](../graphics/direct3d.md) is written against the Direct3D 12 of Windows and compiles HLSL when programs are made, with `d3dcompiler_47.dll`. A console whose graphics API differs from it keeps an adapted copy as its device of the API `port`.

### A device of its own

A port whose graphics API the engine has no device of defines the class `gpu::PortDevice` of `engine/src/gpu/PortDevice.hpp`:

```cpp
#include "gpu/PortDevice.hpp"

#include "ConsoleDevice.hpp"

namespace haylen::gpu {

std::unique_ptr<Device> PortDevice::create(const std::optional<Surface>& surface, const DeviceFactory::Options& options) {
    return std::make_unique<ConsoleDevice>(surface, options);
}

} // namespace haylen::gpu
```

The device implements the whole contract of `gpu::Device` and reports `Api::Port` from `getApi` and `Language::Port` from `getLanguage`. Its runtime hands it the window in a surface of the kind `Surface::Kind::Port`, whose `window` and `display` hold whatever native handles the device reads. The factory prefers the API `port` to every other API of the build, and the [RHI guide](../graphics/rhi.md) lists what each part of the contract asks of a device. A device can start as `gpu::NullDevice`, which reports the API it is given, so the runtime of a port runs apps before its device draws, as the test port does.

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

The command `haylen test --port` builds the engine with the test port of `engine/tests/data/port` in `build/engine/<host>-port-<config>` and plays the app of `engine/tests/data/port-app` on it. The test port has the platform `test-port`, a runtime that plays the app without a window, services that answer like a console without plugins, and a device of the API `port` built on `gpu::NullDevice`, which checks every call of the engine against the contract and runs the code that its shader tool wrote. The app checks the platform, the operating system and the graphics API, draws shapes, sprites and text, and quits with the status `0`. It needs no SDK, so it builds on the desktops of the public repository and keeps the mechanism working for every port.

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

The same test checks the failures that a port meets, each with its typed error and its sentence: a machine without CMake (`missingToolchain`), a backend of another contract version (`incompatibleExtension`), a plugin without an implementation for `test-port` (`unsupportedTarget`), a capability that the backend does not declare, such as `package` (`missingCapability`), an architecture that the target does not build (`unsupportedTarget`), and an SDK whose slice lacks its files (`incompatibleSdk`). It builds the engine, so the usual run of the tests leaves it out, and the job `port` of continuous integration runs it on Linux, Windows and macOS. The variable `HAYLEN_JOBS` sets the jobs of its builds.

```sh
HAYLEN_JOBS=4 uv run pytest -m port tools/tests/extensions
```

The test port still includes some internal headers of `engine/src` and compiles one internal source. The test `tools/tests/repository/test_port_headers.py` lists them and fails on any new one, so a port depends on fewer of them over time.

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
5. **Choose the GPU device.** Name a graphics API of the engine in `GRAPHICS_APIS` when the console runs one, or write `gpu::PortDevice` for its own graphics API, as [the GPU device](#the-gpu-device) explains. Start from `gpu::NullDevice` so apps run before the device draws.
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
| Entry point and frames | The runtime that calls `Player::create`, `Player::initialize`, `Player::frame`, `Player::handleEvent` and `Player::cleanup`. |
| Window | The host derived from `platform::NativeHost` and the window derived from `platform::NativeWindow`, with the size and scale of the framebuffer, the pixel density cap, focus, closing, the screen saver and the frame rate. |
| Input | Controllers, and the keyboard, the mouse and touch where the platform has them, handed to the player as events, and the text input of `Services::getTextInput` with the keyboard of the system. |
| Files | The package that ships with the app, the folder of user data, which `Services::persistUserData` commits where the platform saves on request, and the folder of the content store. |
| System | The name of the platform, `Services::getSystemInfo` with the device kind, the safe area of the screen and the system state, such as the theme and the battery. |
| Lifecycle | The app states `active`, `inactive` and `background`, suspend and resume, and low memory, handed to the player as events. |
| Audio | The audio setup that `getAudioSetup` of the host returns, with a backend of its own for the mixer where miniaudio has none for the platform. |
| Graphics | A device of the engine named in `GRAPHICS_APIS`, or `gpu::PortDevice` with the shader command. |
| Optional services | Video, accessibility, the media session and the bridge of native plugins, each answering with `unsupported` or doing nothing where the platform lacks it. |
| Tooling | The engine port that builds the SDK and the backend that generates, builds, verifies, runs and packages apps. |

## Limits

- Every dependency of the engine must build with the toolchain of the console, such as Varn with Lua and libuv, Poco with its TLS stack, SDL where the port uses it, and the libraries of fonts, images and audio. A dependency that needs a change for a console gets it in the private repository of the port, through a patch of its own.
- Native plugins have parts for the platforms they list in `implementations`, so a plugin that a console app uses gets its console part in a private repository too, and a plugin that does not list the platform of the port fails the build of that platform.
- The backends of the repository build apps for the platforms of the repository. Apps of a port build, run and pack only through the backend of the port, with the tools of the SDK of its console.
