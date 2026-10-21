# 🛠️ Tools

Tools build, test, debug and ship the engine and its apps.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| API policy | What is public and stable, what is internal, and how options, returns and events grow without breaking apps. | [`api-policy.md`](api-policy.md) |
| Build | The command `haylen.py`, the requirements of each platform, build trees, CMake options, dependencies, C++ apps, web and mobile builds and the CI of the repository. | [`build.md`](build.md) |
| Distribution | App commands, engine artifacts, templates and platform projects, release builds, native libraries, splash screens, development mode, the web loader and platform support. | [`distribution.md`](distribution.md) |
| Embedding | The engine as a library of another CMake project through `add_subdirectory`, CPM or `find_package`. | [`embedding.md`](embedding.md) |
| Installers in CI | A GitHub Actions workflow that builds an app with signed installers and store packages for every platform, and the secrets it needs. | [`ci-installers.md`](ci-installers.md) |
| Testing | The test suite, the headless host, the engine fixture, Lua binding tests, the test project, sanitizers, fuzzing, coverage and formatting. | [`testing.md`](testing.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.debug` | The debug statistics, object counts, monitors, the frame profiler and the recent log. | [`lua-api/debug.md`](../lua-api/debug.md) |
| `haylen.hotReload` | Whether the app runs in development, how changed modules reload in place, values a module keeps across its reloads and the `reloaded` hook. | [`lua-api/hotReload.md`](../lua-api/hotReload.md) |
