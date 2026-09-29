# Haylen — project guide

Haylen is a reusable engine for games, multimedia apps and applications. It is 2D and organized so that 3D can grow next to it without renaming anything. Its core is C++20, and its whole API is exported to Lua through [Varn](https://github.com/varn-org/varn). Lua is the primary way to write apps, and every capability stays fully usable from C++. The repository holds the engine, the platform templates, the tools and the sample apps.

This file is binding for every change. It describes the project as it is and the rules every feature and fix follows, so read it before working and follow it exactly instead of re-deriving conventions. When a rule or a design decision changes, update this file in the same change and describe the new state in the present tense. `PROJECT.md` holds the master plan, the owner's requests, the decisions and the feature checklist.

## Core principles

- No workarounds, hacks, hidden fallbacks, dead code, legacy code, backward-compatibility branches or speculative abstractions. Implement the current design cleanly and directly.
- Replace or refactor code that no longer fits instead of carrying it forward, whatever the size of the change. There are no aliases for old names and no checks for how something used to work.
- Only change what makes sense to change. Do not add code, tests, comments or files just to show work, and do not guard against situations that cannot happen.
- Every change is finished only when it is implemented, exported to Lua, tested and documented.
- Performance matters everywhere. Hot paths avoid allocations and per-item calls across the Lua boundary, and heavy work leaves the frame thread.
- The engine holds general capabilities. The mechanics of a particular game belong in apps and samples, never in the engine.
- Code, comments and repository documentation are written in English. `PROJECT.md` is the exception and is written in Portuguese for the project owner.
- Never mention other engines or their tools anywhere in the repository: not in code, comments, docs, test names, commit messages or the project plan, and not even as a comparison or an inspiration. Describe what Haylen does in its own terms.
- The repository never records its own history. Code, comments, docs, `README.md`, `PROJECT.md` and this file describe what exists now, without earlier names, earlier designs, what something was renamed from or dated change notes. Git holds the history.
- This file never cites versions of libraries or tools. The pinned versions live in `engine/cmake/haylen-dependencies.cmake`, the templates and the CI workflow.

## Working rules

- Every request of the owner goes into the checklist of `PROJECT.md` before the work starts, so nothing is lost, and an item is checked only when it is implemented, tested where possible and documented.
- Commit and push to `main` after each finished block of work, once the build, the tests and the format check pass. A block that is committed builds and passes its tests on its own, so separate unrelated changes into separate commits and verify a commit from a clean checkout when the working tree holds other work.
- A commit message is one short lowercase sentence with a type prefix: `feature: ...`, `fix: ...`, `refactor: ...`, `perf: ...`, `test: ...`, `docs: ...`, `build: ...` or `chore: ...`, for example `feature: add scene loading lifecycle`. It has no body, no co-author and no other trailer, and never names Claude or anyone else.
- Before every commit, review what is staged (`git status` and `git diff --cached --stat`) and make sure nothing private or temporary goes in: build outputs, caches, generated projects under `build/`, local tool folders, logs, screenshots, secrets, API keys, tokens, signing keys and keystores, `local.properties`, `.env` files, machine-specific paths or settings, and personal data. Anything like that belongs in `.gitignore`, never in the repository.
- Screenshots used to check a change capture only the app window or the page, never the whole screen.
- Features that touch rendering, input, audio, text input, windows or platform code are run on the platforms they affect (desktop player, iOS and tvOS simulators, Mac Catalyst, Android emulator, browser), and a sample shows each of them working.

## What the engine is

- **Library first.** `engine/` is a standalone CMake project that other projects consume with `add_subdirectory`, CPM or `find_package(haylen)`. It exports the `haylen::engine` library, the `haylen::runtime` host library and the `haylen_add_app` CMake function.
- **Lua through Varn.** The engine links Varn's C++ core (`varn_core`). One `varn::runtime::Runtime` owns the Lua state, the event loop and the worker pools. Engine modules are installed into that Lua state through `package.preload`, next to Varn's own modules (`async`, `http`, `socket`, `json`, `fs`, `zip`, `crypto`, `log`, `platform`, `process`, `datetime`, `xml`, `ffi`).
- **Everything in C++ is exported to Lua.** Every public C++ capability has a Lua binding in the same change, with Lua tests and a reference page under `docs/lua-api/`. This is a rule, not a goal.
- **App packages.** The runtime runs an app package, which is either a folder or a `.zip` file containing `app.json`, the Lua modules under `source/` and the assets under `content/`. Nothing else in the folder belongs to the package, so platform projects, notes and build files can sit next to it. `app.json` configures the identity, window, design resolution, orientation, splash screen, lifecycle, audio session, debug options, autoloads and native libraries before any Lua runs, and unknown keys and invalid values are errors with clear messages. `source/main.lua` is the entry point.
- **Asset paths.** Every asset path passed to the engine is relative to the package `content/` folder and never includes the `content/` prefix. `require("scenes.menu")` resolves Lua modules from the `source/` folder, so it loads `source/scenes/menu.lua`.
- **Prebuilt engine, template apps.** Lua apps never compile the engine. `make.py engine` builds it once into prebuilt artifacts under `build/artifacts/` (`Haylen.xcframework`, the `haylen` AAR in a local Maven repository, the WebGPU and WebGL2 web players and the desktop player), and `make.py run --platform` assembles an app from the platform templates in `templates/platform/`, the app's own `platform/<template>/` overrides and its package. C++ apps compile the engine through CMake, where `haylen_add_app` deploys the package into the app of each platform.
- **The player.** `haylen --dev <folder|zip>` runs any app package on desktop with hot reload. Only `--dev` turns on development behavior, so apps built from the templates, shipped apps and web pages never run in development mode.
- **Plugins.** Every subsystem is a plugin: a self-contained unit with a name, a uniform lifecycle (`start`, `installLua`, `event`, `beginFrame`, `fixedUpdate`, `update`, `render`, `renderUi`, `endFrame`, `stop`) and its own Lua module installation. Built-in subsystems use exactly the same plugin interface that other projects use to extend the engine, and plugins never reach into each other's internals.
- **Native access.** Apps reach anything native through native plugins, which package a Lua API with its Swift or Objective-C, Kotlin or Java, JavaScript and C or C++ implementations and their SDK dependencies. Underneath, plugins talk through the asynchronous platform bridge, whose JSON calls native handlers answer with typed errors, timeouts and cancellation. Apps can also call native libraries from Lua through `haylen.native` and Varn's `ffi` (for example the flat C APIs of SDKs such as Steam or Epic Online Services), and apps that compile the engine add C++ engine plugins. Answers and native callbacks always reach Lua on the frame thread. The engine library never bundles or calls a third-party SDK itself, because SDKs such as AdMob or Firebase live in native plugins.
- **Every kind of desktop app.** Besides normal windows, the engine supports frameless, transparent, always-on-top and click-through windows that apps move themselves, for apps that live on the desktop or above the taskbar, and every mode can change while the app runs.
- **Errors never stop the runtime.** An error in app code shows the error screen with the message, the file and line, the source around it and the Lua stack, which the user can scroll, copy and restart from. The web page receives the same error, and hot reload restarts the app from the error screen.
- **Web editor ready.** A future website edits Lua code and assets in the browser and runs the app with the WebAssembly runtime (WebGPU or WebGL2). The web runtime therefore loads packages supplied at runtime (a zip or a set of files in memory), restarts an app without reloading the page, reloads changed scripts and assets, forwards logs, statistics and errors with Lua stack traces to JavaScript, and shows the error screen instead of stopping when a script fails. The website itself lives in another repository.

## Platforms

- The engine runs on macOS, Windows, Linux, iOS and iPadOS, Mac Catalyst, tvOS, Android (phones, tablets and Android TV from one APK) and the web (WebGPU with WebGL2 as the alternative the page picks). visionOS runs the iPad app, and watchOS is not possible because its SDK lacks the graphics, input and audio frameworks the engine needs. `docs/distribution.md` records the support table and the minimum versions.
- Minimum operating system versions are as low as the toolchain and the dependencies allow, and each limit is explained in `docs/distribution.md`.
- Every feature works on every platform, or the docs say exactly where it does not and why. Input covers mouse, touch, keyboard, gamepads and TV remotes, and text input uses the native text field of each platform so input methods, composition and on-screen keyboards work everywhere.
- Mobile apps follow the platform lifecycle: app states (`active`, `inactive`, `background`), audio sessions and interruptions, low-memory warnings and orientation. UI positions itself in the safe area the engine reports.
- The Apple template is an XcodeGen `project.yml` with the generated `App.xcodeproj` always committed next to it. After editing `project.yml`, run `xcodegen generate` in its folder and commit both.
- The Android template is a Gradle app without C++ that depends on the haylen AAR. The web template is a loading page with the app or engine logo, a progress bar, the backend choice and the error screen.
- Splash screens follow `app.json` and work in landscape and portrait on every device.

## Async and threading

- The frame thread owns the GPU, the Lua state, scenes, UI, audio control and physics. It never blocks.
- Varn's event loop is advanced with `Runtime::poll()` once per frame. Promises resolve, coroutines resume, timers fire and socket and HTTP callbacks run inside that call, on the frame thread.
- Long-running CPU work (image and audio decoding, map parsing, path finding, procedural generation, data-parallel updates) runs on Varn's `taskPool()` through `core::JobSystem`. Blocking I/O runs on `ioPool()`. Results return to the frame thread through the event loop and, for Lua, through a Varn `Promise` that a coroutine can `:await()`. Slow Lua APIs offer an asynchronous version that returns a promise.
- Lua code never runs on worker threads. Long Lua computations run as `haylen.jobs` coroutines that share a time budget each frame.
- Never iterate a container while the loop body can run Lua or a user callback. Snapshot first, then iterate the snapshot.
- A job posted to a pool or to the loop never lets an exception escape. The job system hands failures to the frame thread, where they become engine errors.
- GPU resource creation and destruction stay on the frame thread. Handles released on any thread hand their GPU objects to the device, which destroys them after the frame is submitted.
- Static objects that other threads may still use at process exit are created with `new` and never destroyed.
- The web build is single-threaded like Varn. Pool jobs run when `poll()` drains them, and `JobSystem::parallelFor` runs inline there, so pages need no cross-origin isolation.

## Lifecycle and ownership

- One `core::Engine` owns every service of one running app. Restarting an app destroys the engine between frames and creates a new one with a fresh Lua state, and the engine shuts down in a fixed order that releases Lua values before the Lua state closes.
- Scene changes run as one pipeline: the transition covers the current scene, the next scene loads asynchronously in its `load` hook (with progress, optionally behind a loading view), enters, and the transition reveals it. Effects that show both scenes at once load first. Transitions are optional and scenes pass through the states `created`, `loading`, `loaded`, `entering`, `active`, `covered`, `exiting`, `exited` and `unloaded`.
- Everything a scene or another owner creates (timers, tweens, event listeners, signal connections, async tasks, UI documents) belongs to it and ends with it, so no callback or coroutine ever runs for an owner that is gone. APIs that register callbacks take an `owner`.
- The game pause gates scenes, autoloads, timers, tweens and audio buses through process modes (`inherit`, `pausable`, `whenPaused`, `always`, `disabled`), and the app states halt the app in the background as the lifecycle options decide.
- Engine and app events travel on the `EventBus` with one lifecycle vocabulary for the app, scenes, windows, input devices, network and objects, and signals connect objects directly. Both support priorities, one-shot and deferred delivery, and connections that end with their owner.
- Autoloads are app-wide singletons that live for the whole app and receive the lifecycle callbacks. The ones `app.json` lists load in order before `source/main.lua` runs, and `haylen.autoload` adds more while the app runs.

## Naming

- Types, enums and enum values use `PascalCase`. Functions, methods, variables, parameters and members use `camelCase`. Constants use `k` + `PascalCase`.
- Members have plain names with no prefix or suffix: never `m_`, `_` or a trailing underscore.
- Accessors: getters use `get` (`getPosition`), boolean queries use `is`, `has` or `can` (`isVisible`, `hasFocus`), and setters use `set` (`setPosition`). Container-like classes keep the standard library names (`size`, `empty`, `begin`, `end`, `clear`). Plain data structs without invariants expose public fields instead of accessors.
- A parameter or local variable never reuses the name of a member of its class, so nothing shadows a member (setters take `value`).
- Names say what something is. A plugin, class or module named after a verb or a vague word (like a plugin called `save`) is renamed to the thing it owns.
- The product name is Haylen, written `haylen` in identifiers, targets, modules and files. Things the engine runs are apps, never games: `app.json`, `haylen_add_app`, `core::Application`.
- Lua names use the same words as the C++ API: modules are `haylen.<module>`, functions, methods, fields and option keys use `camelCase` and types use `PascalCase`. Accessor prefixes do not reach Lua: a C++ `getX`/`setX` pair is the Lua property `x` (`sprite.x`), and a read-only getter is a Lua property or method without the prefix (`world:bodyCount()`).
- Every name the Lua API takes or returns as a string is `camelCase` too: enum values, which are the C++ enum value with a lowercase first letter (`ProcessMode::WhenPaused` is `'whenPaused'`), event names, input control names, cursor names, easing names and asset type names. `app.json` keys and the keys of engine data files are `camelCase` as well.
- Lua modules of 2D subsystems end with `2d`, such as `haylen.graphics2d`, `haylen.physics2d` and `haylen.animation2d`, so a future 3D module never conflicts. Dimension-agnostic modules keep plain names, such as `haylen.graphics` for textures, render targets, fonts and shaders, and `haylen.tiled` keeps its name because Tiled maps are 2D by nature.
- C and C++ source files (`.h`, `.hpp`, `.c`, `.cpp`, `.mm`) use `PascalCase` named after their class, for example `FrameClock.hpp` and `Renderer.cpp`. Lua files use `dash-case`, for example `main-menu.lua`, and every package keeps `source/main.lua` as its entry point. Lua modules are required with the same name, for example `require("scenes.main-menu")`.
- CMake module files use `dash-case`, for example `haylen-dependencies.cmake`. Sample folders use `dash-case`, for example `samples/games/tiny-island`.
- All folders are lowercase. Asset files and folders use lowercase `snake_case`.

## Code organization

- Every context has its own namespace, matching its folder: `haylen::core`, `haylen::math`, `haylen::io`, `haylen::assets`, `haylen::graphics`, `haylen::text`, `haylen::input`, `haylen::audio`, `haylen::ui`, `haylen::platform`, `haylen::localization`, `haylen::storage`, `haylen::ai`, `haylen::debug`, `haylen::net`, `haylen::lua` and `haylen::plugins`. Folders under `2d/` map to namespaces with the `2d` suffix, the same names as their Lua modules: `haylen::graphics2d`, `haylen::animation2d`, `haylen::particles2d`, `haylen::lighting2d`, `haylen::physics2d`, `haylen::navigation2d`, `haylen::spatial2d`, `haylen::procedural2d`, plus `haylen::tiled`. Nothing lives directly in the `haylen` namespace.
- A type name does not repeat its namespace when a precise name exists: `haylen::physics2d::World`, `haylen::physics2d::Body`, `haylen::tiled::Map`, `haylen::graphics2d::Camera`, `haylen::text::Layout`, `haylen::lua::Promise`. Types in the shared `haylen::plugins` namespace name their dimension (`Physics2DPlugin`).
- One class per file. A header and its source file hold one class and are named after it. Types owned by a single class (its options, events, results and enums) are nested in that class. A type shared by several classes gets its own file.
- There are no free functions. Every function is a method of a class: utilities are static methods (`math::Easing::apply`, `math::Geometry::intersects`), helpers of a source file are private methods of its class, operators are hidden friends defined inside the class, and each Lua binding is a class whose Lua entry points are static methods. The only exceptions are entry points whose names the platform dictates (`main`, `sokol_main`, `ANativeActivity_onCreate`, `JNI_OnLoad` and `Java_*` JNI functions, Emscripten exports and C callbacks with fixed signatures), and each one only forwards to a class.
- There are no namespace-scope variables or constants in source files. They are static members of the file's class.
- Every enum has exactly one table of its string names, and every Lua binding and file parser uses that table.
- Plugins live in `engine/include/haylen/plugins/` and `engine/src/plugins/`, in the `haylen::plugins` namespace, together with the `Plugin` interface. A plugin wires its subsystem into the engine and installs its Lua module, and the subsystem itself stays in its context folder. Plugins that apps construct or reference have public headers, and the others keep their headers in `engine/src/plugins/`. `plugins::BuiltInPlugins` registers the built-in plugins in dependency order.
- Everything specific to 2D lives in a `2d` folder (`engine/include/haylen/2d/`, `engine/src/2d/` with the Lua bindings of the 2D modules, and `engine/tests/2d/`), and dimension-agnostic code never depends on it.

## Lua bindings

- Each context keeps its Lua binding class next to it, such as `engine/src/2d/physics/Physics2DLua.cpp`, built with the public toolkit in `engine/include/haylen/lua/` (`Binding`, `ClassBuilder`, `Userdata`, `Stack`, `Converter`, `EnumNames`, `Table`, `Promise`, `Reference`, `TypeConverter` and `Runtime`). Projects that extend the engine use the same toolkit.
- Options are passed as tables whose unknown keys raise `Unknown option '<key>'.`, and enums are passed as their string names.
- Error messages are complete sentences that start with a capital letter and end with a period, and they say what was wrong and what is expected, such as `No canvas is active. Call beginWorld, beginScreen or beginTarget before drawing.`. The only exceptions are the short reasons of `luaL_argerror` and `luaL_typeerror`, which Lua wraps into its own `bad argument` sentence.
- Userdata keep their Lua callbacks in their user value, so a callback that refers back to its owner never keeps it alive. C++ code that keeps a Lua value alive uses `lua::Reference` and releases it in `stop`.
- Hot paths offer fast Lua access: number fields next to vector properties, native properties that tweens animate in C++, float buffers shared with C++ and bulk APIs that move many objects in one call.
- Lua chunks are always loaded as text (`"t"` mode), never as bytecode.

## Repository layout

```text
CMakeLists.txt            Root project: engine, player and tests.
make.py                   Single build entry point for every platform and task: engine builds and artifacts, creating, running and packaging apps, samples, shaders, serving web pages, tests, coverage, sanitizers, formatting and benchmarks.
PROJECT.md                Master plan, owner's requests and feature checklist (Portuguese).
engine/
  CMakeLists.txt          Standalone engine project, consumable by other projects.
  cmake/                  Engine CMake modules: CPM bootstrap, dependencies and their patches, haylen_add_app, content deployment, SDK install, warnings, the xcframework slice merge and the Mac Catalyst toolchain.
  include/haylen/         Public C++ API. Dimension-agnostic contexts: core, math, io, assets, graphics, text, input, audio, ui, platform, localization, storage, ai, debug, net, lua and plugins.
  include/haylen/2d/      Public 2D API: graphics, animation, particles, lighting, physics, tiled, navigation, spatial, procedural.
  src/                    Implementation mirroring include/. src/lua/ holds the binding toolkit, src/plugins/ the built-in plugins and src/platform/ the host boundary, the native interop and one folder per platform (sokol, headless, apple, android, web, windows, linux, desktop). The `haylen` player is src/platform/sokol/LuaPlayer.cpp.
  shaders/                sokol-shdc shader sources and the shader library app shaders include.
  platform/android/       Gradle project of the haylen Android library (activity with the splash screen, bridge, gamepad, insets, text input and the player library).
  platform/web/           JavaScript runtime, audio worklet and bridge, and the HTML shell and backend picker of C++ web apps.
  platform/apple/         Info.plist and launch screen templates of Apple apps built with CMake.
  bench/                  The benchmark apps (sprites, algorithms, procedural, Lua).
  tests/                  GoogleTest suite with its support helpers, test fonts and the native test library.
samples/
  <category>/<sample>/    Samples grouped by category (games, graphics, gameplay, interface, system, cpp). A Lua sample folder is the app package plus its README and platform overrides.
templates/
  app/                    The starter app package of make.py new.
  plugin/                 The skeleton of a native plugin that make.py plugin new copies.
  platform/<platform>/    One project template per platform, found by folder: a new platform is a folder here and its run target in make.py.
    apple/                XcodeGen project.yml with the generated App.xcodeproj next to it: iOS and iPadOS with Mac Catalyst, tvOS and macOS.
    android/              Gradle app project without C++ that depends on the haylen AAR.
    web/                  Loading page with the app or engine logo, the progress bar, the backend choice and the error screen.
plugins/                  The official native plugins, one folder per plugin id, which make.py plugin add copies into apps.
tools/                    Python tools (Tiny Swords importer, island map generator, PNG reader and writer).
docs/                     Guides and the Lua API reference.
extras/images/            Brand images: the vertical and horizontal logos, the symbol, and the logo variants with a white wordmark for dark backgrounds.
```

## Native plugins

- A native plugin is a folder named after its dash-case id with `plugin.json`, a `README.md` with the complete Lua API and the setup per platform, the Lua API in `source/` (`init.lua` is `require('<id>')`), and one folder per implementation: `apple/` (Swift or Objective-C), `android/` (a Gradle library module in Kotlin or Java), `web/` (an ES module) and `native/` (a CMake project of a C or C++ library for desktops). `docs/plugins.md` documents every field of `plugin.json`.
- `plugin.json` declares the platforms, the plugins it requires, typed parameters with their platforms, defaults and descriptions, and what each platform brings: Swift packages pinned to one exact version, frameworks, Info.plist keys, entitlements, resources and build scripts on Apple, the module, Gradle plugins, manifest placeholders and files on Android, the module on the web and the native library. `${parameter}` in those values is replaced with the app's value.
- Apps list their plugins with the parameter values in the `plugins` section of `app.json`, and keep the plugin folders in `plugins/<id>/`, copied by `make.py plugin add`. A value that differs per platform uses parameters with different names (`iosAppId`, `androidAppId`), so nothing resolves platform values at runtime. Missing parameters take the defaults of `plugin.json` on every platform.
- The package carries `plugins/<id>/plugin.json` and `plugins/<id>/source/` of every listed plugin. Nothing else of a plugin reaches the package.
- make.py validates every manifest and the app's values before it builds, and assembles each platform: on Apple it writes `plugins.json`, which the template's `project.yml` includes, merges Info.plist keys and entitlements and regenerates the project with the pinned XcodeGen when plugins need it, on Android it drives the template through `haylen.*` keys of `gradle.properties`, on the web it copies the modules and lists them in `config.json`, and on desktops it builds the native library like the `native` section of `app.json`.
- Native plugin methods are named `<id>.<method>` and events `<id>.<event>`, with camelCase method and event names, and every official plugin offers the same Lua API on every platform it supports. Calls a platform cannot serve fail with the code `unsupported` and a clear message.
- Official plugins live in `plugins/` of the repository and follow every rule of this file.

## Architecture rules

- Engine code lives under `engine` and never depends on samples, templates or apps.
- Apps consume only public engine headers from `engine/include` and the Lua API.
- The portable engine library never calls Sokol app or OS APIs. It reaches them only through the internal `Host` interface in `engine/src/platform/Host.hpp`. `haylen_runtime` implements the host for real platforms and `haylen_headless` implements it for tests.
- Rendering targets Sokol through the Haylen graphics API. Apps never include Sokol, Box2D, miniaudio, Dear ImGui backend, Varn internals, JNI, UIKit, AppKit or Emscripten headers.
- Platform services go through `platform::Bridge` using JSON requests and asynchronous callbacks delivered on the frame thread.
- All gameplay input goes through the action map, and UI input goes through focus navigation that works with every device.
- UI positions itself in the engine-provided safe area.
- Physics goes through the Haylen Box2D wrapper, whose handles detect a destroyed world.
- Tiled runtime code targets current Tiled JSON files and does not read obsolete formats.

## Dependencies

- Dependencies are declared with CPM in `engine/cmake/haylen-dependencies.cmake`, pinned with a SHA-256 hash to their latest release or, when a project has no releases, to its latest default-branch commit.
- Packages the engine shares with Varn (nlohmann/json, libuv) are declared before Varn, so Varn reuses them and the whole build resolves a single copy of each shared library.
- A change to a dependency's source is a patch in `engine/cmake/patches/` that CPM applies, documented in `docs/distribution.md` or `docs/build.md` with the reason. The patch fixes the real problem and stays as small as possible.
- When a dependency is updated, adopt its current API everywhere and drop patches it no longer needs. Do not keep code paths for the previous version.

## Samples

- Samples are Lua apps grouped by category under `samples/`: `games/` for complete games, `graphics/`, `gameplay/`, `interface/` and `system/` for feature samples, and `cpp/` for C++ projects. Commands take the path from `samples/`, such as `python3 make.py run games/tiny-island`, and `python3 make.py samples` lists them.
- A Lua sample holds `app.json`, `README.md`, `source/` and `content/`, plus `platform/<template>/` with only what it adds to a platform template. Only `app.json`, `source/` and `content/` are deployed.
- A feature sample covers one feature set. `source/main.lua` pushes the menu scene of `source/scenes/menu.lua`, `source/tests.lua` lists the tests, and each test is one scene under `source/tests/` with a header, on-screen hints, a Back button in the safe area and Escape, gamepad B and the TV menu button going back to the menu.
- Samples use only documented Lua APIs, work with mouse, touch, keyboard, gamepads and TV remotes, run on every platform and clean up everything they create through owners. When a sample needs something the engine lacks, the engine gains the capability instead of the sample working around it.
- Sample art is drawn with primitives, generated or taken from CC0 packs listed in `content/CREDITS.md`.

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
- The build is warning-free on every platform with the warnings of `engine/cmake/haylen-warnings.cmake`.
- clang-format does not format project lambdas acceptably. Wrap non-trivial lambdas in `// clang-format off` and `// clang-format on` markers, and format the lambda manually. Multi-line Lua sources in tests get the same markers.
- Run `python3 make.py format` before finishing a change. It applies `.clang-format` to every C, C++, Objective-C and Objective-C++ file of the engine, the samples and the templates, and lists multi-line lambdas that are missing their `clang-format off` and `on` markers. `python3 make.py format --check` fails on either problem and runs in CI.

## Comment standard

- Every comment is a complete sentence that starts with a capital letter and ends with a period.
- If a sentence needs to begin with a lowercase identifier, keep its exact spelling and rewrite the sentence so the identifier does not start it.
- A comment above a function, method, class, or module explains what callers need to know, not its internal implementation.
- Comments are rare and appear only where the code cannot say something by itself.
- Keep comments concise and natural.
- Do not split one sentence across multiple lines.
- Do not continue one sentence on the next line.
- End the current sentence with a period before starting another sentence on the next line.
- Do not join sentences with semicolons, in comments or in documentation.
- Avoid verbose, fragmented, or narrative comments.
- Code and comments are written in English.

## Tests

- Every engine capability has GoogleTest tests when the logic can run without a real graphics device or platform runtime. The headless host (Sokol dummy backend, a mixer without a device, Varn runtime) makes the renderer, UI, Lua bindings and the engine loop testable, and `test::EngineFixture` runs a real engine on an in-memory package.
- Lua bindings are tested by running Lua through the headless engine and asserting on engine state, returned values, error messages and asynchronous results.
- Every fix comes with a regression test that fails without it.
- Tests verify behavior that matters. Do not write redundant tests, generated tests or tests that only restate the implementation.
- Test files follow the code rules: they live in the namespace of the context they test, keep helpers in fixtures or support classes, and name suites after their subject with a `Test` suffix and tests after the behavior they verify.
- Test data lives in `engine/tests/data/`, so engine tests never read files of the samples.
- Run `python3 make.py test`. Changes to threading, lifetimes or memory handling also pass `python3 make.py test --sanitizers thread` and `--sanitizers address`.
- Engine coverage is kept as close to 100 percent as the code allows. Run `python3 make.py coverage`.

## Documentation

- Every change updates the docs it affects in the same change.
- Every Lua module has a reference page in `docs/lua-api/<module>.md` with the complete API, the options, defaults and error messages, and a runnable example of every capability, indexed by `docs/lua-api.md`.
- The guides in `docs/` explain the architecture, the lifecycle, the build, distribution, embedding, testing and each system in depth. `docs/testing.md` describes the test support, `docs/distribution.md` the commands, templates and platform support, and `docs/architecture.md` the libraries, source layout, plugins, frame and threads.
- Markdown prose is never hard-wrapped. One paragraph is one line.
- The main `README.md` is presentation and quick start only.
