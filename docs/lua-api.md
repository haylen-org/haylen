# Lua API

Apps for Haylen are written in Lua. Every engine capability is a module that `require` returns, such as `require('haylen.graphics2d')`, and every module page lists its complete API with a runnable example of each function. The [Lua guide](lua.md) explains the package layout, scenes, classes, disk access and async code, the [lifecycle guide](lifecycle.md) explains app states, the pause, process modes and autoloads, and the other guides in this folder explain each system in depth.

## Engine and app flow

| Module | Purpose |
| --- | --- |
| [haylen](lua-api/haylen.md) | Engine version, platform, graphics backend, the resolved `app.json`, the app clock, time scale, the game pause, app states, lifecycle options, autoloads, classes, quitting and fatal errors. |
| [haylen.scene](lua-api/scene.md) | The stack of scenes and their lifecycle: asynchronous loads with progress, preloads, loading views, transitions that cover, hold and reveal or keep both scenes alive with built-in and custom effects, completion promises, tasks and listeners that end with their scene, process modes and platform events. |
| [haylen.events](lua-api/events.md) | The event bus with channels, priorities, filters, owners and queued delivery, and the lifecycle events of the engine. |
| [haylen.signal](lua-api/signal.md) | Signals with priorities, one-shot, deferred and owned listeners, and the `Connection` of every listener. |
| [haylen.timer](lua-api/timer.md) | Functions that run after a delay or at an interval, with owners and process modes. |
| [haylen.tween](lua-api/tween.md) | Tweens of fields and engine properties, ready-made motions, timelines, staggers and full playback control. |
| [haylen.jobs](lua-api/jobs.md) | Long Lua work spread over several frames under a time budget. |
| [haylen.collections](lua-api/collections.md) | Object pools that recycle Lua values, ring buffers of the newest values and float buffers shared with C++. |
| [haylen.log](lua-api/log.md) | Messages with a severity in the engine log. |

## Graphics and animation

| Module | Purpose |
| --- | --- |
| [haylen.graphics](lua-api/graphics.md) | Textures, render targets, TrueType and bitmap fonts, font families, custom shaders and the GPU backend. |
| [haylen.graphics2d](lua-api/graphics2d.md) | Canvases, captures, draw order with y sorting and visibility, cameras with viewports, smoothing, drag margins and shake, parallax layers, sprites, sprite batches, shapes, meshes, text, rich text with effects and a typewriter reveal, nine-slices, image blends, metaballs, materials with custom shaders, lit canvases with normal maps, lights and occluders. |
| [haylen.animation2d](lua-api/animation2d.md) | Frame animations from grids and atlases, and animators that play them on sprites. |
| [haylen.particles2d](lua-api/particles2d.md) | Particle emitters and `.particles` effect files. |
| [haylen.lighting2d](lua-api/lighting2d.md) | Point, spot and directional lights with blend modes, masks and shadows, occluders from outlines, physics bodies and Tiled maps, light queries and flame flicker. |
| [haylen.viewport](lua-api/viewport.md) | The design resolution, the visible area and the safe area. |
| [haylen.window](lua-api/window.md) | Window size, fullscreen, title, cursor, on-screen keyboard and clipboard. |

## World

| Module | Purpose |
| --- | --- |
| [haylen.tiled](lua-api/tiled.md) | Tiled maps and worlds: drawing with y sorting, collision, objects and spawning, ray casts against tiles and objects, and object outlines for navigation meshes. |
| [haylen.physics2d](lua-api/physics2d.md) | Box2D bodies, shapes with their geometry and outlines for drawing, joints, contacts, sensors, ray casts with filters, piercing, bounces and fans, shape casts, batched ray casts on the job system, screen picking, ray debug drawing and queries, with ropes, bridges, ragdolls, vehicles, one-way platforms, conveyors, explosions, destructible terrain, fractures and fluids. |
| [haylen.navigation2d](lua-api/navigation2d.md) | A\*, weighted A\*, jump point search and hierarchical path finding on square, isometric, staggered and hexagonal grids, Dijkstra maps, flow fields, waypoint graphs, navigation meshes with agent radius, steering agents and ORCA crowds, with asynchronous versions. |
| [haylen.spatial2d](lua-api/spatial2d.md) | Spatial hashes, quadtrees, dynamic AABB trees and k-d trees with area, circle, point, ray and nearest queries, screen picking, and grid algorithms: ray casts, Bresenham lines and circles, field of view, visibility polygons, flood fills, connected regions and union-find. |
| [haylen.procedural2d](lua-api/procedural2d.md) | Scattering over regions, caves, dungeons, mazes, Wave Function Collapse, Delaunay and Voronoi, and autotiling, all deterministic by seed and with asynchronous versions. |
| [haylen.ai](lua-api/ai.md) | Finite state machines, behavior trees, utility selectors and influence maps. |
| [haylen.math](lua-api/math.md) | `Vec2`, `Rect`, `Color`, `Transform`, random numbers, noise, easing, geometry, ray casts against shapes, polygon booleans, marching squares, splines, springs, shuffle bags, weighted choices and Poisson disk sampling. |

## Player interaction

| Module | Purpose |
| --- | --- |
| [haylen.input](lua-api/input.md) | Keyboard, mouse, touch, gestures, gamepads and the action map. |
| [haylen.ui](lua-api/ui.md) | Themed menus, HUDs, dialogs and touch controls. |
| [haylen.imgui](lua-api/imgui.md) | Dear ImGui windows for debug panels and tools. |
| [haylen.audio](lua-api/audio.md) | Sounds, music, buses, effects, pause modes, interruptions and positional audio. |
| [haylen.localization](lua-api/localization.md) | Translated text with placeholders and plural forms. |

## Data and services

| Module | Purpose |
| --- | --- |
| [haylen.assets](lua-api/assets.md) | Loading package files now or in the background, and preload groups. |
| [haylen.storage](lua-api/storage.md) | Private files of the player, the folder Varn's `fs` shares with them, and named save slots with summaries. |
| [haylen.preferences](lua-api/preferences.md) | Player preferences that persist between sessions. |
| [haylen.platform](lua-api/platform.md) | The JSON bridge to native code, such as sign-in and device information. |
| [haylen.net](lua-api/net.md) | WebSocket connections with reconnection and pings. |
| [haylen.debug](lua-api/debug.md) | The debug statistics, object counts, monitors, the frame profiler and the recent log. |

## Varn modules

The engine runs on the [Varn](https://github.com/varn-org/varn) runtime, so its modules are available to apps too: `async` for coroutines and promises, `http`, `socket`, `json`, `fs`, `zip`, `crypto`, `log`, `platform`, `process`, `datetime` and `xml`. Their reference lives in the Varn repository. Engine functions that finish later return Varn promises, which a coroutine started with `async.spawn` waits for with `:await()`.
