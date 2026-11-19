<p align="center">
    <a href="https://github.com/haylen-org/haylen" target="_blank" rel="noopener noreferrer"><img width="120" src="extras/images/symbol.svg" alt=""></a>
</p>

<h1 align="center">Haylen</h1>

<p align="center">
    <a href="https://github.com/haylen-org/haylen/actions/workflows/ci.yml"><img src="https://github.com/haylen-org/haylen/actions/workflows/ci.yml/badge.svg" alt="Haylen - CI"></a>
    <a href="https://github.com/haylen-org/haylen/blob/main/LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License: MIT"></a>
    <a href="https://isocpp.org"><img src="https://img.shields.io/badge/C%2B%2B-20-00599C.svg" alt="C++ 20"></a>
    <a href="https://github.com/varn-org/varn"><img src="https://img.shields.io/badge/Lua-Varn-000080.svg" alt="Lua through Varn"></a>
</p>

<p align="center">
The engine for games, multimedia apps and applications, with a fast C++20 core, a complete Lua API and one app package for every platform.
</p>

<br>

## What Haylen is

Haylen is a reusable engine for games, multimedia apps and applications. It is 2D today and organized so that 3D grows next to it. Its core is C++20, and its whole API is exported to Lua through the [Varn](https://github.com/varn-org/varn) runtime, so apps are written in Lua while every capability stays usable from C++.

An app is a package: a folder or a zip file with `haylen.json`, its Lua modules under `source/` and its assets under `content/`. The same package runs on every platform. The engine is built once into an SDK for each target, and apps link it without compiling it. The native project of each platform is generated for every build from `haylen.json`, so an app keeps only its own code, assets and settings.

## Made with AI in mind

Haylen is built so that coding agents write correct apps and extend the engine with confidence. The names of the C++ and Lua APIs follow one vocabulary, every Lua module has a complete reference with a runnable example of every capability, error messages say what was wrong and what is expected, and [`AGENTS.md`](AGENTS.md) holds the rules that every change follows. One command, `haylen`, builds, runs, checks and tests everything, the headless player runs a whole app without a window, and the test project shows every feature with a code, so an agent can verify what it built.

## Platforms

| | Platform | Graphics APIs |
| --- | --- | --- |
| 🍎 | macOS | Metal |
| 🪟 | Windows | Direct3D 12, Direct3D 11, Vulkan and OpenGL |
| 🐧 | Linux | Vulkan and OpenGL |
| 📱 | iOS and iPadOS | Metal |
| 💻 | Mac Catalyst | Metal |
| 📺 | tvOS | Metal |
| 🤖 | Android phones, tablets and TVs from one APK | Vulkan and OpenGL ES |
| 🌐 | Web | WebGPU and WebGL2 |
| 🥽 | visionOS | Metal |

Each build carries every graphics API of its platform and uses the first one that starts on the machine, in the order of the table, while the app or its player can ask for another. Game consoles need a port in a private repository, which the engine is built to take.

The guide to distribution in the documentation lists the minimum versions and how each platform runs.

## Features

- 🎨 **2D rendering:** sprites, batches, shapes with smooth edges, meshes, nine-slices, cameras, parallax, render targets, custom shaders, lighting with shadows and post-processing.
- 🎞️ **Animation:** frame animations, state graphs, skeletons, tweens and timelines, particle effects and chains for procedural animation.
- 🧱 **Physics:** bodies, joints, sensors and casts on several threads, with ropes, ragdolls, vehicles, character movers, destructible terrain and fluids.
- 🗺️ **Tiled maps:** every orientation, animated tiles, parallax, collision, objects and worlds.
- 🧭 **Gameplay:** path finding, navigation meshes, crowds, spatial queries, procedural generation, behavior trees and utility AI.
- 🧩 **UI:** themed components, focus navigation for every device, safe areas, right-to-left layouts and screen readers.
- 🔤 **Text:** every script in both directions, distance field and bitmap fonts, rich text and localization.
- 🔊 **Audio:** sounds, streamed music, buses, effects, positional audio and mobile audio sessions.
- 🎮 **Input:** keyboard, mouse, touch, gestures, gamepads and TV remotes behind one action map.
- 🌍 **Network:** HTTP, sockets and WebSocket connections that never block the frame.
- 📦 **Content:** protected releases, on-demand downloads and updates that apply on the next start.
- 🔌 **Native plugins:** Swift, Objective-C, Kotlin, Java, JavaScript and C++ behind one Lua API.
- 🔁 **Development:** hot reload, an error screen that keeps the app alive and a debug overlay.
- 🛠️ **Tools:** one command that creates, runs, checks, packages and tests apps on every platform.

## Quick start

The command line of Haylen needs only [uv](https://docs.astral.sh/uv), which brings the Python it runs on and its packages. Building the engine needs CMake, Ninja and a C++20 compiler, Apple platforms need Xcode, and Android needs the Android SDK. The command `haylen doctor` shows what this machine has and lacks for every target, and the other tools download when a build first needs them.

The script `haylen` at the root of the repository, or `haylen.cmd` on Windows, runs the tooling through uv. From the root, create an app and run it:

```sh
./haylen new ~/apps/my-app --name "My App" --identifier com.example.myapp
./haylen app run ~/apps/my-app
```

The command `new` writes a starter app with its `haylen.json`, and `app run` plays it in the desktop player with hot reload. The first run of a target compiles the engine into the SDK that the apps of the target link, as `sdk build` does, which takes about ten minutes with a warm compiler cache and up to an hour without one, and later runs reuse it. For another target, name it, such as `ios-simulator`, `android` or `web`:

```sh
./haylen app run ~/apps/my-app --target web
```

An app written in C++ starts the same way and goes through the same targets, with its C++ code compiled against the SDK:

```sh
./haylen new ~/apps/my-cpp-app --kind cpp
./haylen app run ~/apps/my-cpp-app --target ios-simulator
```

Put the root of the repository on `PATH` to run `haylen` from any folder.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Hello, Haylen', 960, 540, {size = 96, anchor = {0.5, 0.5}})
    end,
})
```

## Documentation

The [documentation](docs/README.md) explains how every block of Haylen works, from the architecture to building and shipping apps, and lists the whole Lua API with an example for each capability.

## License

Haylen is free for personal and commercial work, forever, under the [MIT License](LICENSE). Every contribution, whether code, art, sounds or any other content, becomes part of Haylen under the same license, as [CONTRIBUTING.md](CONTRIBUTING.md) explains.
