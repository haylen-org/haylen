# 🧾 `haylen-sdk.json`

This page lists every field of `haylen-sdk.json`, the manifest that an engine port writes into the folder of every engine SDK it installs, which the schema `tools/haylen/schemas/haylen-sdk.schema.json` checks. The tooling reads it to pick the slice of each target, and nothing else writes it, so a manifest that is outdated or damaged is built again with `haylen sdk build`. The [build guide](../tools/build.md#engine-sdks) explains what an SDK is and what each engine port puts in it.

## Fields

| Field | Meaning |
| --- | --- |
| `format` | The version of the format of the manifest, `1`. |
| `port`, `portVersion` | The engine port that built the SDK and its version. |
| `engineVersion` | The version of `engine/VERSION`. |
| `revision` | A hash of the engine sources that built the SDK, which changes with any file of `engine/` that reaches an SDK. The tests and the benchmarks never reach one, so a change to `engine/tests` or `engine/bench` keeps every SDK current. |
| `configuration` | `Debug` or `Release`. |
| `slices` | One slice per system and environment, each with its `system`, `environment` (`host`, `device` or `simulator`), `architectures`, `abi`, `toolchain`, the C++ `runtime` it links, its `minimumSystemVersion`, its `graphicsApis`, the `capabilities` of the system that the engine uses there, such as `dynamicLibraries`, and its `files` by name. |
| `hostTools` | The programs that run on the machine that builds apps, never on the target, each with its `name`, `system`, `architecture` and `path`, such as the content tool `haylen-content`. |
| `files` | The files that every slice shares, by name. |
