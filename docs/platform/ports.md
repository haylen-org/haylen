# 🎮 Ports

This guide explains how someone with access to the SDK of a platform that the repository does not hold, such as a game console, adds that platform to Haylen. It covers what a port is, what the repository can and cannot hold, the folder of a port and its `port.cmake`, the platform runtime, the GPU device with its graphics API and shader language, the shader tools, building and testing, continuous integration, how a studio gets the SDK of a console, a port step by step, the paths for Xbox, Nintendo Switch and PlayStation 4 and 5, why a desktop cannot test an Xbox port, and what a port of any other platform implements.

## What a port is

A port is a folder outside the repository that adds a platform to the engine. The cache path `HAYLEN_PORT_DIR` of the engine names it, and the engine then builds for the platform of the port instead of the platform of the toolchain. The port brings two things, and the engine stays as it is:

- **A platform runtime**: a host, a window and the services of the platform, with the entry point of the process. It takes the place of the [SDL platform layer and the Android runtime](../core/architecture.md#layers).
- **A GPU device**, when the graphics API of the platform is not one of the engine. The device implements the [render hardware interface](../graphics/rhi.md) and draws through the graphics API `port` with the shader language `port`. A platform whose graphics API the engine already has names that API instead.

Everything above those layers, the player, the engine, the renderers and the whole Lua API, is the same as on every other platform. An app runs on a port without a change, and a script sees the port only through [`haylen.platform`](../lua-api/haylen.md#haylenplatform), which names its platform, and [`haylen.graphicsApi`](../lua-api/haylen.md#haylengraphicsapi), which is `'port'` on the device of a port.

## What the repository holds

The SDKs of game consoles, their headers, their graphics APIs, their shader compilers and their documentation come under licenses that keep them private. So the code that calls them never enters this repository, which is public under the MIT License. The repository holds only what every port builds on:

| In the repository | In the private repository of the port |
| --- | --- |
| The mechanism of ports: `HAYLEN_PORT_DIR`, `haylen_port`, `gpu::PortDevice`, the reserved values `Api::Port`, `Language::Port`, `Surface::Kind::Port` and `SystemInfo::Os::Port`, and the `port` targets of shader packages. | The `port.cmake` of the port. |
| The engine, the player, the renderers, the Lua API and the devices of the public graphics APIs. | The runtime, the host, the window and the services of the console. |
| The test port in `engine/tests/data/port`, which checks the mechanism on a desktop. | The GPU device of the console, its shader tool and its tests. |
| This guide, which names only platforms, SDKs and public APIs. | The build scripts, the private runners of continuous integration and everything else the license of the SDK covers. |

A port never patches the engine in its private repository to work around a missing piece. When a port needs something the mechanism lacks, the mechanism gains it here, in public terms, and every port profits.

## The folder of a port

The folder holds a `port.cmake`, which describes the port with one call of `haylen_port`, and the sources it names. The engine includes `port.cmake` before it picks its platform, so the description decides the platform, the runtime and the devices of the build.

```cmake
haylen_port(
  PLATFORM console
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
)
```

| Argument | What it gives |
| --- | --- |
| `PLATFORM` | The name of the platform in lowercase dash-case, which `HAYLEN_PLATFORM` takes and the services of the port report. It cannot be the name of a platform of the engine. |
| `RUNTIME_SOURCES` | The sources of the runtime of the platform, which `haylen-runtime` compiles instead of the SDL platform layer, the Android runtime and the services of the engine. |
| `RUNTIME_LIBRARIES` | The libraries that the runtime links, such as the targets of the SDK. |
| `GRAPHICS_APIS` | The graphics APIs of the engine whose devices the port compiles, by their names in `gpu::Device::kApiNames`, such as `openGl`. |
| `DEVICE_SOURCES` | The sources of the device of the API `port`, which `haylen-gpu` compiles. They define `gpu::PortDevice::create`. |
| `DEVICE_LIBRARIES` | The libraries that the device links. |
| `SHADER_LANGUAGES` | The languages of `haylen-shaderc` that the shader tool of the port reads, such as `spirv` or `hlsl5`. The shader packages of the engine carry them. |
| `SHADER_COMMAND` | The command that runs on every shader package of the engine after `haylen-shaderc` wrote it, with the path of the package after its last argument. It writes the code of the device of the port into the package. |

The runtime of a port compiles with the definition `HAYLEN_PORT`, which keeps the platform folders of the repository out of the shared player even where the compiler of the port targets their system, and `haylen-gpu` compiles with `HAYLEN_GPU_PORT` when the port has a device. A port needs `RUNTIME_SOURCES` and at least one graphics API: its own device, the devices of the engine it names, or both. A port with a device of its own needs `SHADER_LANGUAGES` and `SHADER_COMMAND`. The configuration stops with a clear sentence when one is missing.

Build the engine with the port through CMake:

```sh
cmake -S path/to/haylen -B build/console -DHAYLEN_PORT_DIR=path/to/haylen-console -DCMAKE_TOOLCHAIN_FILE=path/to/console.toolchain.cmake -DHAYLEN_BUILD_TESTS=OFF
cmake --build build/console --target haylen
```

A port builds the engine and its apps in one CMake project, so the SDK that `HAYLEN_BUILD_SDK` installs and the artifacts of `haylen.py engine` cover the platforms of the repository only.

## The platform runtime

The runtime of a port does what every platform layer does, as the [architecture guide](../core/architecture.md#layers) describes:

1. **A host** derived from `platform::NativeHost`. It adds what the window does: the size and scale of the framebuffer, fullscreen, the title, the cursor, the clipboard, closing the window, its focus, `endSplashScreen`, which takes the splash screen of the platform away once the app drew its first complete frame, keeping the screen on, pacing its frames at the frame rate of the app in `applyFrameRate`, the controllers, `getTextInput`, which hands over the text input of the services, `prepareGpuDevice`, which answers `true` once the device of the next app can be created, `createGpuDevice`, which hands over the device that draws into the window, and `pumpHaltedEvents`, which keeps the platform responsive while the [debugger](../tools/debugger.md) halts the frame thread and answers whether the platform asks the frame to finish.
2. **A window** derived from `platform::NativeWindow`, installed with `NativeWindow::install` while it is open, which hands the services its native handles.
3. **A runtime** that holds the entry point of the process. It calls `Player::create` with the command line and the host, opens the window for the APIs of `Player::getGraphicsApis` until a device starts, calls `Player::initialize`, then `Player::frame` once a frame, hands events of the system to `Player::handleEvent`, replaces a device that `isLost` reports and calls `Player::cleanup` when the app ends.
4. **The services** of the platform: the definitions of the static methods of `platform::Services` in `engine/src/platform/Services.hpp`. They tell the name of the platform, open the package that ships with the app, give the folders of user data and of downloaded content, report what the device is, show dialogs, answer the bridge, run text input and read video. A service the platform does not offer answers the way the contract of its method says, such as a dialog that fails with `unsupported`.

The runtime reaches the system of the console through its SDK, or through SDL 3 where the console maker provides its port of SDL to licensed developers. A runtime on SDL can start from the SDL platform layer of the engine, `engine/src/platform/sdl/`, as a copy in the private repository, and keep its own changes there.

The system info of a port reports the operating system `'port'`, the value `SystemInfo::Os::Port`, and the platform name tells which console it is, such as `'xbox'`. A console on a TV reports the device kind `tv`.

## The GPU device

### A graphics API of the engine

A platform whose graphics API the engine already has names it in `GRAPHICS_APIS`, and the engine compiles that device as it does on its own platforms. The device still needs a surface it knows:

- The [OpenGL device](../graphics/opengl.md) draws through the `gpu::GlContext` that the platform makes. A port whose console runs OpenGL 4.3 core or OpenGL ES 3.0 implements `gpu::GlContext` in its runtime, hands it over in a surface of the kind `Gl` and needs no device code.
- The [Vulkan device](../graphics/vulkan.md) makes surfaces for the windows of Windows, Linux and Android only, and needs Vulkan 1.1. A console with Vulkan whose surface is of another kind needs a device of its own.
- The [Direct3D 12 device](../graphics/direct3d.md) is written against the Direct3D 12 of Windows and compiles HLSL when programs are made, with `d3dcompiler_47.dll`.

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

The enums `gpu::Device::Api` and `gpu::Device::Language` stay closed, with one value for every port: `Port`. A build targets one platform, so it holds at most one port, and one reserved value names its device in every build. The closed enums keep their one table of names each, `kApiNames` for the APIs and the names of the languages in shader packages, so `app.json`, `--graphics-api`, the shader packages and Lua all read the same names. They keep the arrays that the engine indexes by language, and the validation of `window.graphicsApis` stays in the engine library, which runs before any device exists and never links the devices. A registration of APIs at run time would need tables of names that grow while the app runs, which the validation of `app.json` could not read, for a choice that a build makes only once. The name of the console comes from the platform name and from `getAdapterName` of the device instead.

## Shaders

`haylen-shaderc` compiles the shaders of the engine into packages with the code of every language of the build, as the [shader guide](../graphics/shaders.md) explains. The compiler is public and writes only public languages, so a port compiles its own code from one of them:

1. `SHADER_LANGUAGES` names the language its tool reads, such as `spirv` for SPIR-V or `hlsl5` for HLSL text, and every package of the engine carries it.
2. After `haylen-shaderc` writes a package, the build runs `SHADER_COMMAND` on it. The command reads the code of each program, compiles it with the shader compiler of the SDK of the console, through a translator such as SPIRV-Cross where that compiler reads another language, and writes the result into the package as the target `port` of the program.
3. The device then receives the code of the `port` target as bytes in `StageCode::bytecode`, with the descriptor set and the binding of each slot.

The target `port` has the shape of the target `spirv`: its stages name sources of the package that hold the Base64 text of the code, and its `blocks`, `textures` and `samplers` give each slot an object with `set` and `binding`. The test port shows the smallest shader tool, `engine/tests/data/port/port-shaders.cmake`, which copies the `spirv` target as the `port` target.

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

The custom shaders of apps are packages too, which `haylen shaders` compiles. A port runs its shader tool on the `.shader` files of an app before it packs the app for the console, since the device of the port reads only the `port` target.

## Testing a port

The command `haylen test --port` builds the engine with the test port of `engine/tests/data/port` in `build/<host>-port-<config>` and plays the app of `engine/tests/data/port-app` on it. The test port has the platform `test-port`, a runtime that plays the app without a window, services that answer like a console without dialogs or plugins, and a device of the API `port` built on `gpu::NullDevice`, which checks every call of the engine against the contract and runs the code that its shader tool wrote. The app checks the platform, the operating system and the graphics API, draws shapes, sprites and text, and quits with the status `0`. It needs no SDK, so it builds on the desktops of the public repository and keeps the mechanism working for every port.

```sh
haylen test --port
```

A port tests its own device the way the engine tests its devices, with the conformance suite and the reference images of `engine/tests/gpu`, which the [testing guide](../tools/testing.md#the-gpu-tests) describes. The engine builds those tests for desktops, the web and Android, so a port builds them with the toolchain of its console in its private repository and runs them on a development kit.

## Continuous integration

The public continuous integration of Haylen never sees a console SDK, and `haylen test --port` checks the mechanism of ports without one. A port runs its own builds and tests on private runners, such as self-hosted runners of its private repository on machines that hold the SDK and reach a development kit:

1. Check out Haylen at the commit the port supports and the port next to it.
2. Configure the engine with the toolchain of the console and `HAYLEN_PORT_DIR`.
3. Build the player and the GPU tests of the port.
4. Run them on the development kit, and keep logs, captures and reference images in the private repository.

Keep the runners, their secrets and their logs private, since they hold what the license of the SDK covers.

## Getting access to an SDK

Each console maker gives its SDK, its development kits and its documentation only to studios that it approves, under a non-disclosure agreement, the NDA. The steps below are public, and everything after them happens under that agreement:

| Platform | Where a studio applies | What it receives after approval |
| --- | --- | --- |
| Xbox One and Xbox Series X and S | The ID@Xbox program for independent studios, or a publisher that already works with Xbox, through Microsoft Partner Center. | The GDKX, the console parts of the Game Development Kit, development kits, and the Xbox Requirements that a game passes before release. |
| Nintendo Switch and Nintendo Switch 2 | The Nintendo Developer Portal, where a company registers and applies for the platform. | The SDK of the console, development hardware that the studio buys, and the guidelines that the release check of Nintendo follows. |
| PlayStation 4 and PlayStation 5 | The PlayStation Partners program, where a company registers and applies. | The SDK of the console, development and test kits, and the technical requirements that a game passes before release. |

Approval asks for a company and, usually, a game to show. Once a studio holds the agreement of a console, it can also ask the SDL project for its private port of SDL 3 for that console, which the SDL maintainers share after they confirm the agreement.

## A port step by step

These steps are the same for every console. The sections that follow add what each console changes.

1. **Get access.** Apply to the program of the console, sign its agreement and receive the SDK, the development kit and the documentation.
2. **Make the private repository.** Create a private repository for the port, such as `haylen-console`, with `port.cmake`, a `runtime/` folder for the host, the window, the entry point and the services, a `gpu/` folder for the device when the console needs one, a `tools/` folder for the shader command, the CMake toolchain file of the SDK, and the scripts of its continuous integration.
3. **Build the test port first.** Run `haylen test --port` on a desktop to see the mechanism work, and read `engine/tests/data/port`, the smallest complete port, before writing your own.
4. **Write the runtime.** Implement the host, the window, the runtime and the services that [the platform runtime](#the-platform-runtime) lists, on the SDL port of the console or on its SDK alone. Start every service with the answer its contract gives for a platform that lacks it, then fill in the services the console has.
5. **Choose the GPU device.** Name a graphics API of the engine in `GRAPHICS_APIS` when the console runs one, or write `gpu::PortDevice` for its own graphics API, as [the GPU device](#the-gpu-device) explains. Start from `gpu::NullDevice` so apps run before the device draws.
6. **Write the shader command.** Pick the language of `haylen-shaderc` that the shader compiler of the SDK reads, name it in `SHADER_LANGUAGES`, and make `SHADER_COMMAND` write the compiled code into the `port` target, as [the shaders section](#shaders) shows.
7. **Build and play.** Configure the engine with the toolchain and `HAYLEN_PORT_DIR`, build it, and play the app of `engine/tests/data/port-app` on the development kit until it quits with the status `0`.
8. **Run the GPU tests.** Build the conformance suite of `engine/tests/gpu` with the toolchain of the console and run it on the kit until every reference image matches.
9. **Play real apps.** Play the test project of `samples/tests` and the sample games on the kit with a controller, and fix in the port what differs from the other platforms.
10. **Meet the requirements of the console.** Every console has rules that a game passes before release, such as how it suspends and resumes, how users sign in, where it saves, and what it shows when a controller disconnects. The services and the runtime of the port carry them, so apps meet them without code of their own.
11. **Pack the app.** Put the app package, or its protected release, inside the package of the console with the packaging tool of the SDK, and open it through `Services::openBundledPackage`.
12. **Automate it.** Run steps 7 to 9 on private runners for every commit of Haylen that the port follows, as [continuous integration](#continuous-integration) describes.

## Xbox

Xbox games build with the Game Development Kit, the GDK. Microsoft publishes the parts of the GDK for Windows PCs in its public repository `microsoft/GDK`, while the parts for the consoles, the GDKX, reach licensed Xbox developers only.

- **Runtime.** SDL 3 supports the GDK, and its public repository holds the code of the targets `Gaming.Desktop.x64`, `Gaming.Xbox.XboxOne.x64` and `Gaming.Xbox.Scarlett.x64`, while only the GDKX builds the Xbox targets. SDL builds for the GDK only through the Visual Studio solution of its folder `VisualC-GDK`, so the port builds SDL there and names the resulting library in `RUNTIME_LIBRARIES`. A port builds its runtime on that SDL, starting from a copy of `engine/src/platform/sdl/`, with services that reach the GDK for the user, saves and storage. SDL initializes the game runtime and the task queue of the GDK by itself.
- **GPU device.** The consoles draw through their own Direct3D 12, which the GDKX documents. The [Direct3D 12 device](../graphics/direct3d.md) of the engine is the starting point. A port whose console runs it as it is names `direct3D12` in `GRAPHICS_APIS`. A port that needs the headers or the swapchain of the console keeps an adapted copy as its device of the API `port`.
- **Shaders.** The packages carry `hlsl5`, HLSL for shader model 5.0. The Direct3D 12 device of the engine compiles it with `d3dcompiler_47.dll` when a program is made. A console that compiles shaders ahead of time gets them from the shader command of the port, which compiles the HLSL of each package with the compiler of the GDK into the `port` target.
- **Package.** A GDK game ships with `MicrosoftGame.config`, which names its identity, its executable and its logos, and the packaging tools of the GDK pack it with the app package next to the executable.

Steps that differ from the general ones:

1. Join ID@Xbox or work through a publisher, and install the GDK and the GDKX of the same version on Windows with Visual Studio.
2. Build SDL from `VisualC-GDK` for the target of the console, and the engine with the CMake toolchain that the GDKX provides for that target.
3. Name `direct3D12` in `GRAPHICS_APIS` and test whether the device of the engine runs on the console as it is, before writing a device of the API `port`.
4. Deploy and run on the development kit with the tools of the GDK, which also capture GPU frames for the device.

### Testing without a console

A PC game of the public GDK, the target `Gaming.Desktop.x64`, runs on Windows without a console, but it does not test an Xbox port. That target is Windows with the game runtime of the GDK on top: it draws through the Direct3D 12 of Windows, which the [Windows platform](../tools/distribution.md) of the engine already runs, and it never reaches the Direct3D 12 of the consoles, their memory, their packaging or their requirements, which only the GDKX and a development kit provide. A build for that target also needs SDL built through its Visual Studio solution instead of the CMake build of the engine. Developer Mode of a retail Xbox runs UWP apps, not games of the GDK, and the engine has no UWP runtime. So the Xbox path is tested on a development kit, and what a desktop can test without one is the mechanism of ports, through `haylen test --port`.

## Nintendo Switch

Nintendo Switch games build with the SDK of the console, which reaches licensed developers only.

- **Runtime.** The SDL project keeps a port of SDL 3 for Nintendo Switch and Nintendo Switch 2 in a separate repository, free under the zlib license for developers under the NDA of the console. A port builds its runtime on it, or on the SDK alone as a native host, and its services on the SDK.
- **GPU device.** The console has a graphics API of its own, NVN, which its SDK documents. The original Nintendo Switch is also conformant to Vulkan 1.0 and OpenGL 4.5, as the public conformance lists of those APIs record. The fastest start draws through the [OpenGL device](../graphics/opengl.md): the runtime implements `gpu::GlContext` on the context of the console, and the port names `openGl` in `GRAPHICS_APIS`. A device for NVN follows as the device of the API `port`.
- **Shaders.** The OpenGL device runs the `glsl430` code of the packages as they are. A device for NVN takes `spirv` or the GLSL of the packages through the shader command of the port, which compiles it with the shader compiler of the SDK.
- **Package.** The tools of the SDK pack the executable with the app package as the content of the game.

Steps that differ from the general ones:

1. Register on the Nintendo Developer Portal, apply for the platform and buy the development hardware.
2. Ask the SDL project for its port of the console, or plan a native host on the SDK.
3. Start with the OpenGL device and a `gpu::GlContext` of the console, so apps draw from the first build, and measure before writing the device for NVN.
4. Test docked and handheld, since the console changes its screen size and performance between them, and the runtime reports each change as a new size of the window.

## PlayStation

PlayStation games build with the SDK of the console, which reaches registered developers only, and so does the documentation of its graphics API and shader language.

- **Runtime.** The SDL project keeps ports of SDL 3 for PlayStation 4 and PlayStation 5 in separate repositories for developers under the NDA of the console. A port builds its runtime on them or on the SDK alone.
- **GPU device.** The consoles have graphics APIs of their own, which no device of the engine draws through, so the port brings its device of the API `port`.
- **Shaders.** The consoles have a shader language of their own, which public talks describe for the PlayStation 4 as the PlayStation Shader Language. The shader command of the port reads the `hlsl5` or `spirv` code of each package, translates it where the compiler of the SDK reads another language, compiles it and writes the result into the `port` target.
- **Package.** The tools of the SDK pack the executable with the app package as the content of the game.

Steps that differ from the general ones:

1. Register on PlayStation Partners and apply for the consoles, then receive the SDK and the development and test kits.
2. Ask the SDL project for its ports of the consoles, or plan a native host on the SDK.
3. Write the device of the API `port` from the start, keeping `gpu::NullDevice` in its place until it draws, and follow the order of the [RHI guide](../graphics/rhi.md): buffers, textures, programs, pipelines, render passes, readbacks.
4. Keep one port for both consoles when their SDKs allow it, with one `PLATFORM` each, such as `playstation4` and `playstation5`, so both share the runtime and differ only where their SDKs differ.

## Other platforms

The steps above fit any platform. What decides between a port and a platform of the repository is the license of its SDK:

- **A platform whose SDK is public**, such as a new operating system for phones, computers or TVs, becomes a platform of the repository: a platform folder under `engine/src/platform/`, on SDL where SDL supports it, a GPU device under `engine/src/gpu/` when its graphics API is new, a platform template under `templates/`, and a run target in `haylen.py`, as `AGENTS.md` describes.
- **A platform whose SDK is private**, such as a console or a device under an agreement, becomes a port, with everything this guide describes kept in its private repository.

Every port, whatever the platform, implements the same parts of the engine:

| Part | What the port gives |
| --- | --- |
| Entry point and frames | The runtime that calls `Player::create`, `Player::initialize`, `Player::frame`, `Player::handleEvent` and `Player::cleanup`. |
| Window | The host derived from `platform::NativeHost` and the window derived from `platform::NativeWindow`, with the size and scale of the framebuffer, focus, closing, the screen saver and the frame rate. |
| Input | Controllers, and the keyboard, the mouse and touch where the platform has them, handed to the player as events, and the text input of `Services::getTextInput` with the keyboard of the system. |
| Files | The package that ships with the app, the folder of user data, which `Services::persistUserData` commits where the platform saves on request, and the folder of the content store. |
| System | The name of the platform, `Services::getSystemInfo` with the device kind, the safe area of the screen and the system state, such as the theme and the battery. |
| Lifecycle | The app states `active`, `inactive` and `background`, suspend and resume, and low memory, handed to the player as events. |
| Audio | The audio setup that `getAudioSetup` of the host returns, with a backend of its own for the mixer where miniaudio has none for the platform. |
| Graphics | A device of the engine named in `GRAPHICS_APIS`, or `gpu::PortDevice` with the shader command. |
| Optional services | Dialogs, URLs, vibration, video, accessibility, the media session and the bridge of native plugins, each answering with `unsupported` or doing nothing where the platform lacks it. |

## Limits

- Every dependency of the engine must build with the toolchain of the console, such as Varn with Lua and libuv, Poco with its TLS stack, SDL where the port uses it, and the libraries of fonts, images and audio. A dependency that needs a change for a console gets it in the private repository of the port, through a patch of its own.
- Native plugins have parts for Apple platforms, Android, the web and desktops, so a plugin that a console app uses gets its console part in the private repository too.
- The commands of `haylen.py` that build, run and pack apps target the platforms of the repository. A port packs its apps with the tools of the SDK of its console.
