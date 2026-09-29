# Haylen — project guide

Haylen is a reusable engine for games, multimedia apps and applications, 2D today and ready to grow 3D, with a fast C++20 core whose whole API is exported to Lua through [Varn](https://github.com/varn-org/varn). Lua is the primary way to write apps, and every capability stays fully usable from C++, the way Godot works with GDScript and C++. The repository also contains sample apps under `samples/`.

This file is binding. Follow it exactly instead of re-deriving conventions each session. `PROJECT.md` holds the master plan, the requirements, the decisions and the feature checklist. Keep it accurate: an item is checked only when it is implemented, tested where possible and documented.

## Core principles

- No workarounds, hidden fallbacks, dead code, legacy code, backward-compatibility branches or speculative abstractions. Implement the current design cleanly and directly.
- Do not preserve legacy behavior. Refactor or replace obsolete code instead of carrying it forward. There are no checks for how something used to work.
- Only change what makes sense to change. Do not add code, tests, comments or files just to show work.
- Code, comments and repository documentation are written in English. `PROJECT.md` is the exception and is written in Portuguese for the project owner.
- Commit and push to `main` after each finished block of work, once the build, the tests and the format check pass.
- A commit message is one short lowercase sentence with a type prefix: `feature: ...`, `fix: ...`, `refactor: ...`, `perf: ...`, `test: ...`, `docs: ...`, `build: ...` or `chore: ...`, for example `feature: add scene loading lifecycle`. It has no body, no co-author and no other trailer, and never names Claude or anyone else.
- Before every commit, review what is staged (`git status` and `git diff --cached --stat`) and make sure nothing private or temporary goes in: build outputs, caches, generated projects under build/, local tool folders, logs, screenshots, secrets, API keys, tokens, signing keys and keystores, `local.properties`, `.env` files, machine-specific paths or settings, and personal data. Anything like that belongs in `.gitignore`, never in the repository.

## What the engine is

- **Library first.** `engine/` is a standalone CMake project that other projects consume with `add_subdirectory`, CPM or `find_package(haylen)`. It exports the `haylen::engine` library, the `haylen::runtime` host library and the `haylen_add_app` CMake function.
- **Lua through Varn.** The engine links Varn's C++ core (`varn_core`). One `varn::runtime::Runtime` owns the Lua state, the event loop and the worker pools. Engine modules are installed into that Lua state through `package.preload`, next to Varn's own modules (`async`, `http`, `socket`, `json`, `fs`, `zip`, `crypto`, `log`, `platform`, `process`, `datetime`, `xml`, `ffi`).
- **Everything in C++ is exported to Lua.** Every public C++ capability has a Lua binding in the same change, with Lua tests and a reference page under `docs/lua-api/`. This is a rule, not a goal.
- **App packages.** The runtime runs an app package, which is either a folder or a `.zip` file containing `app.json`, the Lua modules under `source/` and the assets under `content/`. Nothing else in the folder belongs to the package, so platform projects, notes and build files can sit next to it. `app.json` configures the window and design resolution before any Lua runs. `source/main.lua` is the entry point.
- **Asset paths.** Every asset path passed to the engine is relative to the package `content/` folder and never includes the `content/` prefix. `require("scenes.menu")` resolves Lua modules from the `source/` folder, so it loads `source/scenes/menu.lua`.
- **The player.** `haylen --dev <folder|zip>` runs any app package on desktop with hot reload, and only `--dev` turns on development behavior. Lua apps never compile the engine: `make.py engine` builds it once into prebuilt artifacts (`Haylen.xcframework`, the `haylen` AAR, the WebGPU and WebGL2 player and the desktop player), and `make.py run --platform` assembles an app from the platform templates in `templates/platform/`, like the Axmol templates. C++ apps compile the engine through CMake, where `haylen_add_app` deploys the package like the Axmol template deploys content.
- **Plugins.** Every subsystem is a plugin: a self-contained unit with a name, a uniform lifecycle (`start`, `fixedUpdate`, `update`, `render`, `renderUi`, `stop`, lifecycle events) and its own Lua module installation. Built-in subsystems use exactly the same plugin interface that other projects use to extend the engine. Plugins live in the `plugins` folder and namespace and never reach into each other's internals.
- **Web editor ready.** A future website will edit Lua code and assets in the browser and run the app with the WebAssembly runtime (WebGL2 or WebGPU). The web runtime therefore loads packages supplied at runtime (a zip or a set of files in memory), restarts an app without reloading the page, reloads changed scripts and assets, forwards logs and errors with Lua stack traces to JavaScript, and shows an error screen instead of stopping when a script fails. The website itself lives in another repository.

## Async and threading

- The frame thread owns the GPU, the Lua state, scenes, UI and physics. It never blocks.
- Varn's event loop is advanced with `Runtime::poll()` once per frame. Promises resolve, coroutines resume, timers fire and socket and HTTP callbacks run inside that call, on the frame thread.
- Long-running CPU work (image and audio decoding, map parsing, data-parallel updates) runs on Varn's `taskPool()`. Blocking I/O runs on `ioPool()`. Results return to the frame thread through the event loop and, for Lua, through a Varn `Promise` that a coroutine can `:await()`.
- Never iterate a container while the loop body can run Lua or a user callback. Snapshot first, then iterate the snapshot.
- A job posted to a pool or to the loop never lets an exception escape.
- The web build is single-threaded like Varn. Pool jobs run when `poll()` drains them, and `JobSystem::parallelFor` runs inline there.
- Scene changes run as one pipeline: the transition covers the current scene, the next scene loads asynchronously in its `load` hook (with progress, optionally behind a loading view), enters, and the transition reveals it. Effects that show both scenes at once load first. Everything a scene creates (timers, tweens, event listeners, async tasks, UI documents) belongs to it and ends with its `unload`, so no callback or coroutine ever runs for a scene that is gone.

## Naming

- Types, enums and enum values use `PascalCase`. Functions, methods, variables, parameters and members use `camelCase`. Constants use `k` + `PascalCase`.
- Members have plain names with no prefix or suffix: never `m_`, `_` or a trailing underscore.
- Accessors are named like Axmol's: getters use `get` (`getPosition`), boolean queries use `is`, `has` or `can` (`isVisible`, `hasFocus`), and setters use `set` (`setPosition`). Container-like classes keep the standard library names (`size`, `empty`, `begin`, `end`, `clear`). Plain data structs without invariants expose public fields instead of accessors.
- A parameter or local variable never reuses the name of a member of its class, so nothing shadows a member (setters take `value`).
- Lua names use the same words as the C++ API: modules are `haylen.<module>`, functions and methods use `camelCase` and types use `PascalCase`. Accessor prefixes do not reach Lua: a C++ `getX`/`setX` pair is the Lua property `x` (`sprite.x`), and a read-only getter is a Lua property or method without the prefix (`world:bodyCount()`).
- Lua modules of 2D subsystems end with `2d`, such as `haylen.graphics2d`, `haylen.physics2d` and `haylen.animation2d`, so a future 3D module never conflicts. Dimension-agnostic modules keep plain names, such as `haylen.graphics` for textures, render targets and fonts, and `haylen.tiled` keeps its name because Tiled maps are 2D by nature.
- C and C++ source files (`.h`, `.hpp`, `.c`, `.cpp`, `.mm`) use `PascalCase` named after their class, for example `FrameClock.hpp` and `Renderer.cpp`. Lua files use `dash-case`, for example `main-menu.lua`, and every package keeps `source/main.lua` as its entry point. Lua modules are required with the same name, for example `require("scenes.main-menu")`.
- CMake module files use `dash-case`, for example `haylen-dependencies.cmake`. Sample folders use `dash-case`, for example `samples/games/tiny-island`.
- All folders are lowercase. Asset files and folders use lowercase `snake_case`.

## Code organization

- Every context has its own namespace, matching its folder, like Axmol: `haylen::core`, `haylen::math`, `haylen::io`, `haylen::assets`, `haylen::graphics`, `haylen::text`, `haylen::input`, `haylen::audio`, `haylen::ui`, `haylen::platform`, `haylen::localization`, `haylen::storage`, `haylen::ai`, `haylen::debug`, `haylen::net`, `haylen::lua` and `haylen::plugins`. Folders under `2d/` map to namespaces with the `2d` suffix, the same names as their Lua modules: `haylen::graphics2d`, `haylen::animation2d`, `haylen::particles2d`, `haylen::lighting2d`, `haylen::physics2d`, `haylen::navigation2d`, `haylen::spatial2d`, plus `haylen::tiled`. Nothing lives directly in the `haylen` namespace.
- A type name does not repeat its namespace when a precise name exists: `haylen::physics2d::World`, `haylen::physics2d::Body`, `haylen::tiled::Map`, `haylen::graphics2d::Camera`, `haylen::lua::Promise`. Types in the shared `haylen::plugins` namespace name their dimension (`Physics2DPlugin`).
- One class per file. A header and its source file hold one class and are named after it. Types owned by a single class (its options, events, results and enums) are nested in that class. A type shared by several classes gets its own file.
- There are no free functions. Every function is a method of a class: utilities are static methods (`math::Easing::apply`, `math::Geometry::intersects`), helpers of a source file are private methods of its class, operators are hidden friends defined inside the class, and each Lua binding is a class whose Lua entry points are static methods. The only exceptions are entry points whose names the platform dictates (`main`, `sokol_main`, `ANativeActivity_onCreate`, `JNI_OnLoad` and `Java_*` JNI functions, Emscripten exports and C callbacks with fixed signatures), and each one only forwards to a class.
- There are no namespace-scope variables or constants in source files. They are static members of the file's class.
- Plugins live in `engine/include/haylen/plugins/` and `engine/src/plugins/`, in the `haylen::plugins` namespace, together with the `Plugin` interface. A plugin wires its subsystem into the engine and installs its Lua module, and the subsystem itself stays in its context folder.
- Names say what something is. A plugin, class or module named after a verb or a vague word (like a plugin called `save`) is renamed to the thing it owns.

## Repository layout

```text
CMakeLists.txt            Root project: engine, player and tests.
make.py                   Single build entry point for every platform and task: engine builds and artifacts, creating, running and packaging apps, and serving web pages.
PROJECT.md                Master plan and feature checklist (Portuguese).
engine/
  CMakeLists.txt          Standalone engine project, consumable by other projects.
  cmake/                  Engine CMake modules: CPM bootstrap, dependencies and their patches, haylen_add_app, content deployment, SDK install, the xcframework slice merge and the Mac Catalyst toolchain.
  include/haylen/         Public C++ API. Dimension-agnostic contexts: core, math, io, assets, graphics, text, input, audio, ui, platform, localization, storage, ai, debug, net, lua and plugins.
  include/haylen/2d/      Public 2D API: graphics, animation, particles, lighting, physics, tiled, navigation, spatial.
  src/                    Implementation mirroring include/. Each context keeps its Lua binding class next to it (for example src/ui/UiLua.cpp), src/plugins/ holds the built-in plugins that are not public, src/lua/ holds the binding toolkit and src/platform/<os>/ holds the hosts. The `haylen` player is src/platform/sokol/LuaPlayer.cpp.
  shaders/                sokol-shdc shader sources.
  platform/android/       Gradle project of the haylen Android library (activity with the splash screen, bridge, gamepad, insets and the player library).
  platform/web/           JavaScript runtime and bridge, and the HTML shell and backend picker of C++ web apps.
  platform/apple/         Info.plist and launch screen templates of Apple apps built with CMake.
  bench/                  The benchmark apps (sprites, algorithms, procedural, Lua).
  tests/                  GoogleTest suite, including Lua binding tests.
samples/
  <category>/<sample>/    Samples grouped by category (games, graphics, gameplay, interface, system, cpp). Commands take the path from samples/, such as `python3 make.py run games/tiny-island`, and `python3 make.py samples` lists them. A Lua sample folder is the app package: only app.json, source/ and content/ are deployed.
    app.json              Window, design resolution and identity of the app.
    source/               main.lua and the other Lua modules. Feature samples add tests.lua, scenes/menu.lua and one scene per test under tests/.
    content/              Assets.
    platform/<template>/  Only what the sample adds to a platform template, laid over it when make.py builds the sample.
templates/
  app/                    The starter app package of make.py new.
  platform/<platform>/    One project template per platform, found by folder: a new platform is a folder here and its run target in make.py.
    apple/                XcodeGen project.yml with the generated App.xcodeproj always next to it: iOS and iPadOS with Mac Catalyst, tvOS and macOS.
    android/              Gradle app project without C++ that depends on the haylen AAR.
    web/                  Loading page with the app or engine logo, the progress bar, the backend choice and the error screen.
tools/                    Python tools (Tiny Swords importer, island map generator, PNG reader and writer).
docs/                     Guides and the Lua API reference.
```

Everything specific to 2D lives in a `2d` folder (`engine/include/haylen/2d/`, `engine/src/2d/` with the Lua bindings of the 2D modules, and `engine/tests/2d/`), like Axmol's `core/2d`.

## Architecture rules

- Engine code lives under `engine` and never depends on samples or apps.
- Apps consume only public engine headers from `engine/include` and the Lua API.
- The portable engine library never calls Sokol app or OS APIs. It reaches them only through the internal `Host` interface in `engine/src/platform/Host.hpp`. `haylen_runtime` implements the host for real platforms and `haylen_headless` implements it for tests.
- Rendering targets Sokol through the Haylen graphics API. Apps never include Sokol, Box2D, miniaudio, ImGui backend, Varn internals, JNI, UIKit, AppKit or Emscripten headers.
- Platform services go through `platform::Bridge` using JSON requests and asynchronous callbacks delivered on the frame thread.
- All gameplay input goes through the action map.
- UI positions itself in the engine-provided safe area.
- GPU resource creation and destruction stay on the frame thread.
- Physics goes through the Haylen Box2D wrapper.
- Tiled runtime code targets current Tiled JSON files and does not add compatibility code for obsolete formats.
- Lua chunks are always loaded as text (`"t"` mode), never as bytecode.

## Dependencies

- Dependencies are declared with CPM in `engine/cmake/haylen-dependencies.cmake`, pinned with a SHA-256 hash to their latest release or, when a project has no releases, to its latest default-branch commit.
- Packages the engine shares with Varn (nlohmann/json, libuv) are declared before Varn, so Varn reuses them and the whole build resolves a single copy of each shared library.
- When a dependency is updated, adopt its current API everywhere. Do not keep code paths for the previous version.

## Mandatory formatting standard

- Preserve the visual, structural, and architectural style already present in the project.
- Keep code compact, professional, and consistent.
- Avoid excessive vertical whitespace.
- Use only the spacing required to separate reading contexts.
- Organize complex methods and functions into logical blocks that are easy to scan.
- Separate blocks with different responsibilities with one blank line.
- Do not visually glue multiple conditions, validations, loops, state mutations, and returns together.
- A function's flow must be understandable at a glance.
- Complex functions must have a visually clear beginning, middle, and end.
- Important blocks inside genuinely complex functions may have a short comment that explains intent.
- Comments explain context or intent. They do not restate the code.
- Do not add obvious, decorative, artificial, or redundant comments.
- Do not add artificial section comments such as helpers, validators, or public methods.
- Extract focused functions when one function is accumulating unrelated responsibilities.
- Do not extract functions only to reduce the visible line count.
- Avoid abstractions that make the main flow harder to understand.
- Avoid unnecessary nesting.
- Prefer early returns when they simplify control flow.
- Do not use an unnecessary else after return.
- Keep includes direct, minimal, and ordered.
- Do not keep dead code, legacy code, generic fallbacks, or unexpected implicit behavior.
- Avoid macros, unsafe casts, and raw owning pointers when a safer project-consistent alternative exists.
- Use const, references, RAII, and smart pointers when they improve ownership and safety.
- Keep headers, implementation files, namespaces, types, names, and responsibilities consistent.
- The code must look like product code written by an experienced C++ engineer.
- Do not add comments in headers merely to describe methods, sections, or members.
- clang-format does not format project lambdas acceptably. Wrap non-trivial lambdas in `// clang-format off` and `// clang-format on` markers, and format the lambda manually.
- Run `python3 make.py format` before finishing a change. It applies `.clang-format` (the same style as the owner's other projects) to every C, C++ and Objective-C++ file of the engine, the samples and the templates, and lists multi-line lambdas that are missing their `clang-format off` and `on` markers. `python3 make.py format --check` fails on either problem and runs in CI.

## Comment standard

- Every comment is a complete sentence that starts with a capital letter and ends with a period.
- If a sentence needs to begin with a lowercase identifier, keep its exact spelling and rewrite the sentence so the identifier does not start it.
- A comment above a function, method, class, or module explains what callers need to know, not its internal implementation.
- Keep comments concise and natural.
- Do not split one sentence across multiple lines.
- Do not continue one sentence on the next line.
- End the current sentence with a period before starting another sentence on the next line.
- Do not join sentences with semicolons, in comments or in documentation.
- Avoid verbose, fragmented, or narrative comments.
- Code and comments are written in English.

## Tests

- Every engine capability has GoogleTest tests when the logic can run without a real graphics device or platform runtime. The headless host (Sokol dummy backend, null audio device, Varn runtime) makes renderer, UI, Lua bindings and the engine loop testable.
- Lua bindings are tested by running Lua through the headless engine and asserting on engine state and returned values.
- Tests verify behavior that matters. Do not write redundant tests, generated tests or tests that only restate the implementation.
- Engine coverage is kept as close to 100 percent as the code allows. Run `python3 make.py test` and `python3 make.py coverage`.

## Documentation

- Markdown prose is never hard-wrapped. One paragraph is one line.
- Every Lua module has a reference page in `docs/lua-api/<module>.md` with the complete API and a runnable example of every capability, indexed by `docs/lua-api.md`.
- The main `README.md` is presentation and quick start only.
