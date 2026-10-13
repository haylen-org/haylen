# Haylen documentation

Haylen is organized in modules, and this index follows them. Every module has guides that explain how it works and reference pages that list the complete Lua API of each `haylen.<module>` with its options, defaults, errors and a runnable example of every function. Apps are written in Lua, where every capability is a module that `require` returns, such as `require('haylen.graphics2d')`, and every capability is also available to C++, which each guide describes in its C++ section.

Start with the [quick start](../README.md#quick-start), then the [Lua guide](core/lua.md) and the [lifecycle guide](core/lifecycle.md). The [API policy](api-policy.md) says what stays stable as the engine grows.

## Core

The core runs an app: the engine and its plugins, the frame, the threads, the Lua runtime, scenes, events, timers, tweens and jobs, and the values and services that every other module builds on.

| Page | What it covers |
| --- | --- |
| [Architecture](core/architecture.md) | The libraries the engine builds, the source layout, plugins, the host boundary, the frame, threads, resource ownership, app packages and the web runtime. |
| [Lifecycle](core/lifecycle.md) | What happens from start to stop: app states on each platform, the scene lifecycle, the game pause and process modes, autoloads and the owners that end listeners, timers and tasks. |
| [Lua](core/lua.md) | The app package and `app.json`, modules and asset paths, scenes, autoloads, classes, disk access, asynchronous code, errors, hot reload, performance and Lua modules written in C++. |
| [`haylen`](lua-api/haylen.md) | Engine version, platform, graphics backend, the resolved `app.json`, the app clock, time scale, the game pause, app states with the cover of native UI, lifecycle options, autoloads, classes, quitting with an exit status and fatal errors. |
| [`haylen.scene`](lua-api/scene.md) | The stack of scenes and their lifecycle: asynchronous loads with progress, preloads, loading views, transitions that cover, hold and reveal or keep both scenes alive with built-in and custom effects, completion promises, tasks and listeners that end with their scene, process modes and platform events. |
| [`haylen.events`](lua-api/events.md) | The event bus with channels, priorities, filters, owners and queued delivery, and the lifecycle events of the engine. |
| [`haylen.signal`](lua-api/signal.md) | Signals with priorities, one-shot, deferred and owned listeners, and the `Connection` of every listener. |
| [`haylen.timer`](lua-api/timer.md) | Functions that run after a delay or at an interval, with owners and process modes. |
| [`haylen.tween`](lua-api/tween.md) | Tweens of fields and engine properties, ready-made motions, timelines, staggers and full playback control. |
| [`haylen.jobs`](lua-api/jobs.md) | Long Lua work spread over several frames under a time budget. |
| [`haylen.collections`](lua-api/collections.md) | Object pools that recycle Lua values, ring buffers of the newest values and float buffers shared with C++. |
| [`haylen.log`](lua-api/log.md) | Messages with a severity in the engine log. |
| [`haylen.math`](lua-api/math.md) | The types `Vec2`, `Rect`, `Color` and `Transform2D`, random numbers, noise, easing, geometry, ray casts against shapes, polygon booleans, marching squares, splines, springs, shuffle bags, weighted choices and Poisson disk sampling. |
| [`haylen.localization`](lua-api/localization.md) | Translated text with placeholders, plural forms and the direction of each language. |
| [`haylen.storage`](lua-api/storage.md) | Private files of the player, the folder Varn's `fs` shares with them, and named save slots with summaries, on the frame thread or asynchronously on the I/O pool. |
| [`haylen.preferences`](lua-api/preferences.md) | Player preferences that persist between sessions. |
| [`haylen.ai`](lua-api/ai.md) | Finite state machines, behavior trees, utility selectors and influence maps. |
| [`haylen.conversation`](lua-api/conversation.md) | Branching conversations with lines, choices, conditions, variables, commands, variants and saves. |

## Graphics

Graphics draws everything an app shows: the GPU device and its backends, textures and render targets, the 2D renderer with its canvases, cameras and batching, shaders, text in every script and the mapping of design units to the screen.

| Page | What it covers |
| --- | --- |
| [Rendering](graphics/rendering.md) | The renderer: canvases, captures, scene transitions, draw order, batching, shapes with smooth edges, sprites, node trees, lighting, shaders, post-processing, text, nine-slices, vector images, groups, masks and performance. |
| [Shaders](graphics/shaders.md) | Fragment shaders of an app: writing, compiling for every backend, the shader library, uniforms, textures and post-processing. |
| [Text](graphics/text.md) | Fonts, font families and bitmap fonts, shaping, directions and line breaking, rich text with its markup, effects and typewriter reveal. |
| [`haylen.graphics`](lua-api/graphics.md) | Textures from pixels, colors, images or the bytes of image files, with mipmaps, images on the CPU that decode and blit off the frame thread, render targets and whole frames read back without waiting for the GPU, PNG files, render targets, TrueType and bitmap fonts, font families, custom shaders and the GPU backend. |
| [`haylen.graphics2d`](lua-api/graphics2d.md) | Canvases, captures, draw order with y sorting and visibility, cameras with viewports, smoothing, drag margins and shake, camera rigs with zones and rails, pixel canvases that scroll smoothly, parallax layers, sprites, sprite batches, tile layers baked by chunks, strokes with joins, caps, textures, widths and dashes, kept meshes that deform in place, groups that fade as one image, masks of any shape, shapes, meshes, text shaped in every script and ordered in both directions, rich text with effects and a typewriter reveal, nine-slices, image blends, metaballs, materials with custom shaders, lit canvases with normal maps, lights and occluders. |
| [`haylen.viewport`](lua-api/viewport.md) | The design resolution and scaling policy, which change while the app runs, the visible area, the safe area and the edges that native views reserve. |

## 2D

Each 2D module has a guide that explains how it works and when to use what, and a reference page with its complete Lua API. The modules end with `2d`, so 3D modules can grow next to them.

| Page | What it covers |
| --- | --- |
| [Nodes](2d/nodes.md) | Retained trees of composed objects, prefabs and how nodes work with sprites, physics, animation and tweens. |
| [Animation](2d/animation.md) | Frame animations, atlases with frame data, animators, animated batches, state graphs, skeletons and atlases packed at run time. |
| [Particles](2d/particles.md) | How emitters simulate and draw particles, composite effects, trails, the effect library and performance. |
| [Lighting](2d/lighting.md) | Lit canvases, lights, occluders and shadows, light queries for gameplay and performance. |
| [Physics](2d/physics.md) | Worlds, bodies and shapes, units, stepping on several threads, sleeping, the common problems of physics games and their solutions, and performance. |
| [Tiled](2d/tiled.md) | Maps and worlds made with the Tiled editor: authoring, supported features, loading, drawing, reading a map, collision and spawning objects. |
| [Navigation](2d/navigation.md) | Path finding on grids, graphs and navigation meshes, flow fields, Dijkstra maps, steering and crowds, and which to choose. |
| [Spatial](2d/spatial.md) | Spatial structures, grid algorithms, screen picking and overlap worlds, and which to choose. |
| [Procedural](2d/procedural.md) | Deterministic generation of scatterings, caves, dungeons, mazes, tile maps, triangulations and autotiles, on the frame or in the background. |
| [`haylen.nodes2d`](lua-api/nodes2d.md) | Retained trees of 2D nodes with transforms, colors, visibility and layers that subtrees inherit, culled drawing in one call, prefabs, saving to descriptions, nodes that follow bodies and native properties for tweens. |
| [`haylen.animation2d`](lua-api/animation2d.md) | Frame animations from grids and atlases with frame data, animators, animated sprite batches, state graphs, skeletal and cutout animation, and atlases packed at run time. |
| [`haylen.particles2d`](lua-api/particles2d.md) | Particle emitters, composite systems, trails and `.particles` effect files. |
| [`haylen.lighting2d`](lua-api/lighting2d.md) | Point, spot and directional lights with blend modes, masks and shadows, occluders from outlines, physics bodies and Tiled maps, light queries and flame flicker. |
| [`haylen.physics2d`](lua-api/physics2d.md) | Box2D worlds stepped on several threads with interpolation between fixed steps, bodies, shapes with their materials, geometry and outlines for drawing, joints that change while they run and break under load, contacts, sensors, ray casts with filters, piercing, bounces and fans, shape casts, batched ray casts on the job system, screen picking with a radius, path prediction, ray debug drawing and queries, with ropes, bridges, ragdolls, side and top-down vehicles, character movers, grabbers, force fields, one-way platforms, conveyors, explosions, destructible terrain, fractures and fluids. |
| [`haylen.tiled`](lua-api/tiled.md) | Tiled maps and worlds: drawing with y sorting, maps built at run time with terrain painting and saved as Tiled JSON, collision, objects and spawning, ray casts against tiles and objects, and object outlines for navigation meshes. |
| [`haylen.navigation2d`](lua-api/navigation2d.md) | A\*, weighted A\*, jump point search and hierarchical path finding on square, isometric, staggered and hexagonal grids, Dijkstra maps, flow fields, waypoint graphs, navigation meshes with agent radius, steering agents and ORCA crowds, with asynchronous versions. |
| [`haylen.spatial2d`](lua-api/spatial2d.md) | Spatial hashes, quadtrees, dynamic AABB trees and k-d trees with area, circle, point, ray and nearest queries, screen picking, and grid algorithms: ray casts, Bresenham lines and circles, field of view, visibility polygons, flood fills, connected regions and union-find. |
| [`haylen.procedural2d`](lua-api/procedural2d.md) | Scattering over regions, caves, dungeons, mazes, Wave Function Collapse, Delaunay and Voronoi, and autotiling, all deterministic by seed and with asynchronous versions. |

## UI

The UI draws the interface players see and the tools developers use, in design units, inside the safe area and with themes.

| Page | What it covers |
| --- | --- |
| [UI](ui/ui.md) | GUIs, scale, placement, layout, components, events, long lists, text and translations, right-to-left interfaces, themes, touch controls, immediate mode UI and GUIs written in Lua. |
| [Text input](ui/text-input.md) | How text fields edit through the native text input of each platform, with input methods, composition and on-screen keyboards. |
| [`haylen.ui`](lua-api/ui.md) | Themed menus, HUDs, dialogs and touch controls, mirrored for right-to-left languages, with images made at run time from textures, render targets and vector images. |
| [`haylen.imgui`](lua-api/imgui.md) | Dear ImGui windows for debug panels and tools. |

## Audio

Audio mixes sounds and music through buses with effects, follows the pause and the sessions of each platform and places sounds in the world.

| Page | What it covers |
| --- | --- |
| [Audio](audio/audio.md) | Sounds, streams, voices, buses, music, process modes, effects, loudness, positional audio, sessions and interruptions. |
| [`haylen.audio`](lua-api/audio.md) | Sounds from files, bytes or raw samples, music, buses, effects, pause modes, interruptions and positional audio. |

## Input

Input reads every device and gives gameplay named actions, with the timing that action games need.

| Page | What it covers |
| --- | --- |
| [Input](input/input.md) | Frame timing, raw devices, the action map, players, command buffers, recording and replay, virtual controls and how the game and the UI share the devices. |
| [`haylen.input`](lua-api/input.md) | Keyboard, mouse, touch, gestures, gamepads and the action map. |

## Platform

Platform covers the window and the device, the services every platform offers and the ways an app reaches native code.

| Page | What it covers |
| --- | --- |
| [Desktop](platform/desktop.md) | Frameless, transparent, always-on-top and click-through windows, their placement and monitors, menus and quitting. |
| [Platform bridge](platform/platform-bridge.md) | The JSON bridge between Lua and native code: calls, events, bytes, timeouts, screens and the native side on every platform. |
| [Native code](platform/native.md) | Native libraries called from Lua, their callbacks and threads, packaging them with an app and integrating SDKs with a C API. |
| [Plugins](platform/plugins.md) | Native plugins: the package, `plugin.json`, their Lua API, requirements, the Apple, Android and web parts and the demo plugin. |
| [`haylen.window`](lua-api/window.md) | Window size, fullscreen, title, cursor, on-screen keyboard and clipboard, and on desktops frameless, transparent, always-on-top and click-through windows with their frame and monitors. |
| [`haylen.system`](lua-api/system.md) | What the device and its operating system are, the theme and the battery with their changes, opening urls and vibrating. |
| [`haylen.dialogs`](lua-api/dialogs.md) | Native message boxes and pickers of files to open, of a destination to save to and of a folder, which never block the app. |
| [`haylen.platform`](lua-api/platform.md) | The JSON bridge to native code, such as sign-in and purchases, with bytes next to the JSON, typed errors, timeouts, cancellation, calls without answers, retained and batched events, the handles of the plugins of the app and the video and audio streams of their native parts. |
| [`haylen.native`](lua-api/native.md) | Native libraries for Varn's `ffi`, found where the app ships them, their symbols, callbacks from any thread and the interface of the engine for libraries. |

## Network

Network reaches servers and peers from every platform, with every result on the frame thread.

| Page | What it covers |
| --- | --- |
| [Networking](network/networking.md) | What each platform offers, WebSocket clients, HTTP, raw sockets, timeouts, cancellation, owners, backpressure and errors. |
| [`haylen.net`](lua-api/net.md) | WebSocket connections with reconnection and pings. |

## Content

Content brings the files of an app to the device and into memory, protects them in releases and updates them.

| Page | What it covers |
| --- | --- |
| [Loading](content/loading.md) | How the code and assets of an app reach the device and memory in development and in a release: startup, autoloads, assets, preload groups, scenes, delivery, caches and updates. |
| [Content system](content/content.md) | The protected release format, its chunks, cryptography and formats, the content tool, downloads, delivery, updates, the local store and security. |
| [`haylen.assets`](lua-api/assets.md) | Loading package files now or in the background, and preload groups. |
| [`haylen.content`](lua-api/content.md) | Which content plays and which assets are on the device, assets brought down ahead of time, and the updates of the content. |

## Tools

Tools build, test, debug and ship the engine and its apps.

| Page | What it covers |
| --- | --- |
| [Build](tools/build.md) | The command `haylen.py`, the requirements of each platform, build trees, CMake options, dependencies, C++ apps, web and mobile builds and the CI of the repository. |
| [Distribution](tools/distribution.md) | App commands, engine artifacts, templates and platform projects, release builds, native libraries, splash screens, development mode, the web loader and platform support. |
| [Installers in CI](ci-installers.md) | A GitHub Actions workflow that builds an app with signed installers and store packages for every platform, and the secrets it needs. |
| [Embedding](tools/embedding.md) | The engine as a library of another CMake project through `add_subdirectory`, CPM or `find_package`. |
| [Testing](tools/testing.md) | The test suite, the headless host, the engine fixture, Lua binding tests, the test project, sanitizers, fuzzing, coverage and formatting. |
| [API policy](api-policy.md) | What is public and stable, what is internal, and how options, returns and events grow without breaking apps. |
| [`haylen.debug`](lua-api/debug.md) | The debug statistics, object counts, monitors, the frame profiler and the recent log. |
| [`haylen.hotReload`](lua-api/hotReload.md) | Whether the app runs in development, how changed modules reload in place, values a module keeps across its reloads and the `reloaded` hook. |

## Varn modules

The engine runs on the [Varn](https://github.com/varn-org/varn) runtime, so its modules are available to apps too: `async` for coroutines and promises, `http`, `socket`, `json`, `fs`, `zip`, `crypto`, `log`, `platform`, `process`, `datetime`, `xml` and `ffi`, which calls C functions of native libraries that `haylen.native` loads. Their reference lives in the Varn repository. Engine functions that finish later return Varn promises, which a coroutine started with `async.spawn` waits for with `:await()`.
