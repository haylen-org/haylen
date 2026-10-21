# 📜 API policy

Apps written for Haylen keep working when the engine grows. This page says which parts of the engine are its public API, which parts are internal, and the rules every API follows so that a new capability, a new option or a new field enters without changing what an app already calls. Every change to the engine follows it, and a review of a change checks it.

## What is public and stable

The public API is what an app or a project that extends the engine reaches:

- The Lua modules `haylen` and `haylen.<module>` that `require` returns, with every function, method, property, option, enum name, event name, event payload, error code and error message their [reference pages](../README.md) list.
- The public C++ headers under `engine/include/haylen/`, with the namespaces of their contexts, such as `haylen::graphics2d` and `haylen::ui`, the `haylen::plugins::Plugin` interface, the plugins that apps construct or reference, and the binding toolkit in `engine/include/haylen/lua/` that projects use to install their own Lua modules.
- The app package: the keys of `app.json`, the folders `source/` and `content/`, `source/main.lua` as the entry point, the asset paths relative to `content/` and the formats of the engine data files, such as `.particles`, screens of the UI and themes.
- The contract of native plugins: `plugin.json`, the bridge methods and events named `<id>.<name>`, the error codes of the bridge and the native interfaces `HaylenPlugin`, the Android plugin classes and the JavaScript module of the web part.
- The commands and options of `haylen.py`, the CMake targets `haylen::engine` and `haylen::runtime` and the function `haylen_add_app`, and the folder `haylen/` that the platform projects include with their few visible lines.

A name that a reference page or a guide documents is public. A name that no page documents is not, even when Lua can reach it, such as a metatable in the registry.

## What is internal

Everything under `engine/src/` is internal, also the headers of built-in plugins kept there, the `Host` interface of `engine/src/platform/Host.hpp`, the host of each platform, the backends of `engine/src/backends/` and the wire formats that only the engine and the content tool read. Internal code changes freely, and apps never include or rely on it. The dependencies of the engine, such as Sokol, Box2D, miniaudio, Dear ImGui and Varn, are internal too: apps reach them only through the API of the engine, with the exception of the Varn modules that the engine installs for apps, whose own reference lives in the Varn repository.

## How an API grows

A capability enters as new names, never as a new meaning of an existing name. The rules below keep every API open for growth.

### Options

- A function that may ever need more than its essential inputs takes an options table as its last argument, and a method of C++ takes an options struct with designated initializers and defaults. A new option is a new key with a default that keeps the old behavior, so a call that leaves it out behaves exactly as before.
- Essential inputs are positional and come first, such as the texture and the position of a draw, and nothing positional follows the options table. A positional boolean is kept for the one switch that defines a call, such as `closed` of a polyline or `restart` of `animator:play`, and every other switch is an option.
- An unknown key raises `Unknown option "<key>".`, so a misspelled option never passes silently and a key added later never collides with an app's own key.
- The last table of a draw call holds the draw order keys of [`haylen.graphics2d`](../lua-api/graphics2d.md#draw-order) together with the options of that draw, such as `borderScale` of a nine-slice or `rotation` of a mesh, so a draw gains options without another argument.
- A number the engine hands to Lua never carries a negative zero, so a coordinate at the origin formats as `0` and never as `-0`.
- Hot paths that run per item, such as the primitives of `graphics2d` and the queries of `physics2d` and `spatial2d`, take numbers positionally to avoid tables in a loop, and end with an optional table for everything that may come later.

### Returns

- A function returns one value, or a few values whose meaning and order never change, such as a width and a height, a value and an error message, or a changed flag and a new value in `haylen.imgui`. Anything that describes a thing, such as what holds the focus or a hit of a ray cast, is a table with named fields, so it gains fields without changing the ones apps read.
- Lists are Lua sequences. A list whose items describe things holds tables.
- C++ returns structs for the same reason, and a struct gains members with defaults.

### Events and callbacks

- An event or callback that carries data passes it as one table with named fields, or as at most two values whose order is part of the contract, such as `code` and `reason` of a closed socket. A later version adds fields to the table, never values before existing ones. Engine events of [`haylen.events`](../lua-api/events.md) that carry data pass one table, so `sceneEntered` passes `{scene}` and `guiMounted` passes `{gui}`, and an event without data gains a table when it first carries some.
- Event names and callback names use the same verbs for the same ideas across modules: `onComplete` and `'complete'` when something reaches its end, `onKill` when it leaves the engine, `Started` and `Finished` for the phases of a transition, and `Changed` for a value that changed.
- Every function that registers a listener, starts a task or starts a job takes an `owner`, and without one it takes the default owner, so nothing runs for an owner that is gone, as the [owners of `haylen.events`](../lua-api/events.md#owners) describe.

### Asynchronous calls

- A call that finishes later returns a Varn promise that a coroutine awaits, and the asynchronous variant of a synchronous function ends with `Async`, such as `assets.loadAsync`. A rejection that the app asked for, such as a cancellation, never stops the app. A rejection that the app may act on carries a table next to its message, which `await` returns as its third value, with the message in `error` and the same fields as the other failures of its module, such as the `code` and the IDs of the content failures.
- A promise resolves with one value, which follows the rules of returns, so its result grows like any other.

### Value types

- Enums cross into Lua as their `camelCase` string names from the one name table of the enum, so a new value is a new name and old names keep their meaning.
- Colors, vectors and rectangles take every form the [math reference](../lua-api/math.md) lists, such as `'#AARRGGBB'`, `{x, y}` and `{x, y, width, height}`, everywhere a function takes one.
- Numbers keep their units across modules: seconds for time, radians for angles, design units for positions on the screen and pixels for textures.
- Names that cross into Lua are `camelCase`, types are `PascalCase`, and C++ accessors become Lua properties without their `get` and `set` prefixes, as the naming rules of `AGENTS.md` describe.

### Errors

- An error message is a complete sentence that says what was wrong and what is expected, and the messages that a reference page lists are part of the API. Bridge errors carry a stable `code`, such as `unsupported` or `timeout`, which apps compare instead of the message.

## How deprecation is avoided

The engine has no deprecated names, aliases or compatibility branches. It keeps its API stable by design instead:

- A name says exactly what it is from the start, in the vocabulary that the whole engine shares, so it never needs a better name later.
- Every API is open for growth through the rules above, so new needs become new keys, new fields and new functions instead of new signatures of old ones.
- A review of a public API happens before it ships: a new function, option or event follows this page, has its reference page and has tests of its options, errors and payloads.

When a public API still has to change, the change lands in one commit that updates the engine, its bindings, the tests, the reference pages, the guides, the test project, the samples and the templates together, without an alias, and the reference page describes only the new form.
