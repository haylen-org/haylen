# ⚙️ Core

The core runs an app: the engine and its plugins, the frame, the threads, the Lua runtime, scenes, events, timers, tweens and jobs, and the values and services that every other module builds on.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| Architecture | The libraries the engine builds, the source layout, plugins, the host boundary, the backends and the GPU device interface, the frame, threads, resource ownership, app packages, the web runtime and how hot reload works. | [`architecture.md`](architecture.md) |
| Lifecycle | What happens from start to stop: app states on each platform, the scene lifecycle, the game pause and process modes, autoloads and the owners that end listeners, timers and tasks. | [`lifecycle.md`](lifecycle.md) |
| Lua | The first steps of a Lua app: the app package and `app.json`, modules and asset paths, scenes, autoloads, classes, owners, disk access, asynchronous code, errors, hot reload, performance and Lua modules written in C++. | [`lua.md`](lua.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen` | Engine version, platform, graphics API, every field of `app.json` with its default, the app clock, time scale, the game pause, app states with the cover of native UI, lifecycle options, autoloads, classes, quitting with an exit status and fatal errors. | [`lua-api/haylen.md`](../lua-api/haylen.md) |
| `haylen.ai` | Finite state machines, behavior trees, utility selectors and influence maps. | [`lua-api/ai.md`](../lua-api/ai.md) |
| `haylen.collections` | Object pools that recycle Lua values, ring buffers of the newest values and float buffers shared with C++. | [`lua-api/collections.md`](../lua-api/collections.md) |
| `haylen.conversation` | Branching conversations with lines, choices, conditions, variables, commands, variants and saves. | [`lua-api/conversation.md`](../lua-api/conversation.md) |
| `haylen.events` | The event bus with channels, priorities, filters, owners and queued delivery, and the lifecycle events of the engine. | [`lua-api/events.md`](../lua-api/events.md) |
| `haylen.jobs` | Long Lua work spread over several frames under a time budget. | [`lua-api/jobs.md`](../lua-api/jobs.md) |
| `haylen.localization` | Translated text with placeholders, plural forms, a fallback language, the best match for the device and pseudo translation, the direction of each language, numbers, dates and lists written the way each language writes them, and assets per language. | [`lua-api/localization.md`](../lua-api/localization.md) |
| `haylen.log` | Messages with a level in the engine log, the level that filters them and where each platform writes them. | [`lua-api/log.md`](../lua-api/log.md) |
| `haylen.math` | The types `Vec2`, `Rect`, `Color` and `Transform2D`, random numbers, noise, easing, geometry, ray casts against shapes, polygon booleans, marching squares, splines, springs, shuffle bags, weighted choices and Poisson disk sampling. | [`lua-api/math.md`](../lua-api/math.md) |
| `haylen.preferences` | Player choices that persist between sessions under dotted keys, with the volumes, full screen and action map of the engine. | [`lua-api/preferences.md`](../lua-api/preferences.md) |
| `haylen.scene` | The stack of scenes and their lifecycle: asynchronous loads with progress, preloads, loading views, transitions that cover, hold and reveal or keep both scenes alive with built-in and custom effects, completion promises, tasks and listeners that end with their scene, process modes and platform events. | [`lua-api/scene.md`](../lua-api/scene.md) |
| `haylen.signal` | Signals with priorities, one-shot, deferred and owned listeners, and the `Connection` of every listener. | [`lua-api/signal.md`](../lua-api/signal.md) |
| `haylen.storage` | Private files of the player, the folder Varn's `fs` shares with them, named save slots with summaries, versions, migrations, backups and cloud copies, and the resume state, on the frame thread or on the I/O pool. | [`lua-api/storage.md`](../lua-api/storage.md) |
| `haylen.timer` | Functions that run after a delay or at an interval, with owners and process modes. | [`lua-api/timer.md`](../lua-api/timer.md) |
| `haylen.tween` | Tweens of fields and engine properties, ready-made motions, timelines, staggers and full playback control. | [`lua-api/tween.md`](../lua-api/tween.md) |
