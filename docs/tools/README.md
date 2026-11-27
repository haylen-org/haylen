# 🛠️ Tools

Tools build, test, debug and ship the engine and its apps. The [reference of the commands](../reference/commands.md) lists every command of `haylen` with its options and its result.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| API policy | What is public and stable, what is internal, and how options, returns and events grow without breaking apps. | [`api-policy.md`](api-policy.md) |
| Benchmarks | How the benchmarks draw on the GPU device of each graphics API, offscreen or in a window, how to run them on every API of a machine and what their frame and GPU times mean. | [`benchmarks.md`](benchmarks.md) |
| Build | The command `haylen` and its Python package, the requirements and tools of each target, engine SDKs and engine ports, build trees, the engine commands, CMake options, dependencies and their patches, the desktop player and the web runtime. | [`build.md`](build.md) |
| Debugger | The Lua debugger that editors attach to with the Debug Adapter Protocol: breakpoints, log points, steps across coroutines, halts on uncaught errors, values, evaluations and what runs while the app halts. | [`debugger.md`](debugger.md) |
| Developer tools | The tools that look inside a running app: statistics with their window and monitors, the profiler and traces, debug drawings, the inspector with the pick on the screen, the developer console of the terminal, GPU captures and the keys of `haylen app run`. | [`developer-tools.md`](developer-tools.md) |
| Distribution | Targets and their backends, the generated projects of each platform, the `platforms` sections of `haylen.json` and how values merge, signing, release builds, packages and store packs, native libraries, splash screens, development mode, the web loader and platform support. | [`distribution.md`](distribution.md) |
| Embedding | The engine as a library of another CMake project through `add_subdirectory`, CPM or `find_package`, the SDK, `haylen_add_app` and apps written in C++. | [`embedding.md`](embedding.md) |
| Hardware testing | Where to run every test from the command line without continuous integration: the machines you have, simulators, emulators, real devices, and GPUs and phones rented on Amazon Web Services and Microsoft Azure, with their costs and the rules that keep them low. | [`hardware-testing.md`](hardware-testing.md) |
| Installers in CI | A GitHub Actions workflow that builds an app with signed installers and store packages for every platform, and the secrets it needs. | [`ci-installers.md`](ci-installers.md) |
| Testing | The engine test suite, the headless host, the engine fixture, Lua binding tests, the test project, the tests of the tooling, sanitizers, fuzzing, coverage, formatting and continuous integration. | [`testing.md`](testing.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.debug` | The debug statistics and drawings, object counts, monitors, the frame profiler, the recent log, traces, breadcrumbs and native crash reports, the developer console, the inspector and the breakpoints of the debugger. | [`lua-api/debug.md`](../lua-api/debug.md) |
| `haylen.hotReload` | Whether the app runs in development, how changed modules reload in place, values a module keeps across its reloads and the `reloaded` hook. | [`lua-api/hot-reload.md`](../lua-api/hot-reload.md) |
