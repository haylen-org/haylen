# 🛠️ Tools

Tools build, test, debug and ship the engine and its apps.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| API policy | What is public and stable, what is internal, and how options, returns and events grow without breaking apps. | [`api-policy.md`](api-policy.md) |
| Benchmarks | How the benchmarks draw on the GPU device of each graphics API, offscreen or in a window, how to run them on every API of a machine and what their frame and GPU times mean. | [`benchmarks.md`](benchmarks.md) |
| Build | The command `haylen.py` and its output, the requirements and tools of each platform, build trees, the engine commands, CMake options, dependencies and their patches, the desktop player and the web runtime. | [`build.md`](build.md) |
| Distribution | The commands of apps, engine artifacts, templates and platform projects, release builds and their checks, native libraries, splash screens, development mode, the web loader and platform support. | [`distribution.md`](distribution.md) |
| Embedding | The engine as a library of another CMake project through `add_subdirectory`, CPM or `find_package`, the SDK, `haylen_add_app` and apps written in C++. | [`embedding.md`](embedding.md) |
| Installers in CI | A GitHub Actions workflow that builds an app with signed installers and store packages for every platform, and the secrets it needs. | [`ci-installers.md`](ci-installers.md) |
| Testing | The test suite, the headless host, the engine fixture, Lua binding tests, the test project, sanitizers, fuzzing, coverage, formatting and continuous integration. | [`testing.md`](testing.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.debug` | The debug statistics and drawings, object counts, monitors, the frame profiler, the recent log, traces, breadcrumbs and native crash reports, the developer console and the inspector. | [`lua-api/debug.md`](../lua-api/debug.md) |
| `haylen.hotReload` | Whether the app runs in development, how changed modules reload in place, values a module keeps across its reloads and the `reloaded` hook. | [`lua-api/hotReload.md`](../lua-api/hotReload.md) |
