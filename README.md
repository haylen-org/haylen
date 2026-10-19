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

An app is a package: a folder or a zip file with `app.json`, its Lua modules under `source/` and its assets under `content/`. The same package runs on every platform, the engine is built once into prebuilt artifacts, and the project of each platform belongs to the developer, who edits it like any native project.

## Made with AI in mind

Haylen is built so that coding agents write correct apps and extend the engine with confidence. The names of the C++ and Lua APIs follow one vocabulary, every Lua module has a complete reference with a runnable example of every capability, error messages say what was wrong and what is expected, and [`AGENTS.md`](AGENTS.md) holds the rules that every change follows. One command, `haylen`, builds, runs, checks and tests everything, the headless player runs a whole app without a window, and the test project shows every feature with a code, so an agent can verify what it built.

## Platforms

| | Platform | Renderer |
| --- | --- | --- |
| 🍎 | macOS | Metal |
| 🪟 | Windows | Direct3D 11 |
| 🐧 | Linux | OpenGL |
| 📱 | iOS and iPadOS | Metal |
| 💻 | Mac Catalyst | Metal |
| 📺 | tvOS | Metal |
| 🤖 | Android phones, tablets and TVs from one APK | OpenGL ES 3 |
| 🌐 | Web | WebGPU, or WebGL2 where the browser lacks it |
| 🥽 | visionOS | Metal, running the iPad app |

The [distribution guide](docs/tools/distribution.md#platform-support) lists the minimum versions and how each platform runs.

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

Haylen builds with CMake, Ninja, Python and a C++20 compiler, and `haylen.py` downloads the other tools it needs. Apple platforms need Xcode, and Android needs the Android SDK.

The command `haylen` at the root of the repository runs `haylen.py` on macOS, Linux and Windows, so put the root on `PATH` once:

```sh
export PATH="$HOME/path/to/haylen:$PATH"
```

```sh
haylen new ~/apps/my-app --name "My App" --identifier com.example.myapp
haylen run ~/apps/my-app
haylen run ~/apps/my-app --platform web
```

The command `new` writes a starter app with a project of every platform, and `run` opens it in the desktop player with hot reload, or builds it for a platform with `--platform`, such as `ios-simulator`, `android` or `web`. An app written in C++ starts the same way and compiles the engine itself:

```sh
haylen new ~/apps/my-cpp-app --kind cpp
haylen run-cpp ~/apps/my-cpp-app --platform ios-simulator
```

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

The [documentation index](docs/README.md) follows the modules of the engine, with the guides and the Lua reference of each one:

| Module | Start here |
| --- | --- |
| Core | [Architecture](docs/core/architecture.md), [lifecycle](docs/core/lifecycle.md) and [Lua](docs/core/lua.md) |
| Graphics | [Graphics and text](docs/README.md#graphics) |
| 2D | [Animation, particles, lighting, physics, Tiled maps, navigation and procedural generation](docs/README.md#2d) |
| UI | [Components, themes and focus](docs/README.md#ui) |
| Audio and input | [Audio](docs/README.md#audio) and [input](docs/README.md#input) |
| Platform | [Hosts, plugins and services](docs/README.md#platform) |
| Network and content | [Network](docs/README.md#network) and [content](docs/README.md#content) |
| Tools | [Building, distribution, testing](docs/README.md#tools) and the [API policy](docs/api-policy.md) |

## License

Haylen is free for personal and commercial work, forever, under the [MIT License](LICENSE). Every contribution, whether code, art, sounds or any other content, becomes part of Haylen under the same license, as [CONTRIBUTING.md](CONTRIBUTING.md) explains.
