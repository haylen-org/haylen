<p align="center">
    <a href="https://github.com/haylen-org/haylen" target="_blank" rel="noopener noreferrer">
        <picture>
            <source media="(prefers-color-scheme: dark)" srcset="extras/images/logo-v-dark.svg">
            <img width="200" src="extras/images/logo-v.svg" alt="Haylen">
        </picture>
    </a>
</p>

<p align="center">
    <a href="https://github.com/haylen-org/haylen/actions/workflows/ci.yml"><img src="https://github.com/haylen-org/haylen/actions/workflows/ci.yml/badge.svg" alt="Haylen - CI"></a>
    <a href="https://github.com/haylen-org/haylen/blob/main/LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License: MIT"></a>
    <a href="https://isocpp.org"><img src="https://img.shields.io/badge/C%2B%2B-20-00599C.svg" alt="C++ 20"></a>
    <a href="https://github.com/varn-org/varn"><img src="https://img.shields.io/badge/Lua-Varn-000080.svg" alt="Lua through Varn"></a>
</p>

<p align="center">
    <img src="https://img.shields.io/badge/platform-macOS%20%7C%20Windows%20%7C%20Linux%20%7C%20iOS%20%7C%20tvOS%20%7C%20Android%20%7C%20Web-555555.svg" alt="Supported platforms">
</p>

<p align="center">
The engine for games, multimedia apps and applications, with a fast C++20 core, a complete Lua API and one app package for every platform.
</p>

<br>

## What Haylen is

Haylen is a reusable engine for games, multimedia apps and applications. It is 2D today and organized so that 3D grows next to it. Its core is C++20, and its whole API is exported to Lua through the [Varn](https://github.com/varn-org/varn) runtime, so apps are written in Lua while every capability stays usable from C++.

An app is a package: a folder or a zip file with `app.json`, its Lua modules under `source/` and its assets under `content/`. The same package runs on every platform, the engine is built once into prebuilt artifacts, and the project of each platform belongs to the developer, who edits it like any native project.

## Features

- **2D rendering.** Batched sprites, sprite batches, shapes, meshes, nine-slices, cameras with smoothing and shake, parallax layers, render targets, custom shaders and materials, lit canvases with normal maps, post-processing and y-sorted draw order.
- **Animation and effects.** Frame animations, tweens of fields and engine properties with timelines, particle emitters and effect files, and 2D lights with shadows and occluders.
- **Physics.** Rigid bodies, shapes, joints, contacts, sensors and every kind of ray and shape cast, stepped on several threads and drawn smoothly between fixed steps, with ready-made ropes, bridges, ragdolls, side and top-down vehicles, character movers, grabbers, force fields, one-way platforms, conveyors, explosions, destructible terrain and fluids.
- **Tiled maps.** Maps and worlds in every orientation, animated tiles, parallax, collision, object spawning and ray casts against tiles and objects.
- **World and gameplay.** Path finding on grids, waypoint graphs and navigation meshes, steering and crowds, spatial queries, procedural generation, state machines, behavior trees and utility selectors.
- **UI and themes.** Themed menus, HUDs, dialogs, lists and touch controls with focus navigation that works with every device, layouts that follow the safe area, mirroring for right-to-left languages, and debug panels for tools.
- **Text.** Text shaped in every script and ordered in both directions, distance field fonts, bitmap fonts, rich text with effects, localization with plural forms and native text fields with input methods and on-screen keyboards.
- **Audio.** Sounds, streamed music, buses, effects, positional audio, pause modes and the audio sessions and interruptions of mobile platforms.
- **Input.** Keyboard, mouse, touch, gestures, gamepads and TV remotes behind one action map, with virtual touch controls.
- **Networking.** HTTP, sockets and WebSocket connections that never block the frame, with promises and coroutines.
- **Native plugins.** Plugins that bring a Lua API with its Swift or Objective-C, Kotlin or Java, JavaScript and C or C++ parts, an asynchronous bridge with typed errors, timeouts and cancellation, and native libraries called from Lua.
- **Platform services.** System information, native dialogs, opening URLs, vibration, the app lifecycle, safe areas, orientation, and desktop windows that are frameless, transparent, always on top or click-through.
- **Web runtime.** WebAssembly with WebGPU or WebGL2, packages loaded at runtime, restart and hot reload without reloading the page, and logs, statistics and errors with Lua stack traces forwarded to JavaScript.
- **Tools.** One command line, `haylen.py`, that creates, runs, checks and packages apps, builds the engine for every platform, compiles shaders, signs Android apps, serves web pages and runs the tests, with hot reload and an error screen that keeps the app alive.

## Platforms

macOS, Windows, Linux, iOS and iPadOS, Mac Catalyst, tvOS, Android phones, tablets and TVs from one APK, and the web with WebGPU or WebGL2. visionOS runs the iPad app. The [distribution guide](docs/distribution.md#platform-support) lists the minimum versions and how each platform runs.

## Quick start

Haylen builds with CMake, Ninja, Python and a C++20 compiler, and `haylen.py` downloads the other tools it needs. Apple platforms need Xcode, and Android needs the Android SDK.

```sh
python3 haylen.py new ~/apps/my-app --name "My App" --identifier com.example.myapp
python3 haylen.py run ~/apps/my-app
python3 haylen.py run ~/apps/my-app --platform web
```

The command `new` writes a starter app with a project of every platform, and `run` opens it in the desktop player with hot reload, or builds it for a platform with `--platform`, such as `ios-simulator`, `android` or `web`.

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

- [Lua API reference](docs/lua-api.md), the [Lua guide](docs/lua.md) and the [lifecycle](docs/lifecycle.md)
- [Architecture](docs/architecture.md), [building the engine](docs/build.md), [distributing apps](docs/distribution.md), [protected content](docs/content.md) and [using the engine from C++](docs/embedding.md)
- [Rendering](docs/rendering.md), [shaders](docs/shaders.md), [text](docs/text.md), [UI](docs/ui.md), [text input](docs/text-input.md), [physics](docs/physics.md), [Tiled maps](docs/tiled.md), [audio](docs/audio.md), [input](docs/input.md) and [desktop windows](docs/desktop.md)
- [Networking](docs/networking.md), [plugins](docs/plugins.md), [native code](docs/native.md), the [platform bridge](docs/platform_bridge.md) and [testing](docs/testing.md)

## License

Haylen is released under the license in [LICENSE](LICENSE).
