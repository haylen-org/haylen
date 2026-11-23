# ⌨️ Commands

This page is the reference of the command `haylen`: every command with its arguments, its options, its defaults and the result it prints with `--json`. The [build guide](build.md) explains how the command runs, its output and its logs, and the [distribution guide](distribution.md) explains targets, generated projects and what each platform ships.

## The command tree

```text
haylen new <folder> [--kind lua|cpp] [--name] [--identifier] [--orientation]
haylen doctor [--target]
haylen devices
haylen plugin add|update|remove|list|new
haylen project inspect
haylen sdk build|join|list
haylen app prepare|build|verify|run|package|sign|zip|shaders|serve|capture
haylen serve <folder>
haylen content build|verify|inspect|diff|publish|compact|keys
haylen test
haylen engine configure|build|bench|fuzz|coverage|format|clean|tools|consumers
```

Every command also takes `--verbose`, `--json` and `--jobs`, and `haylen` alone takes `--version`. With `--json`, a command prints `{"ok": true, "result": {...}}` when it succeeds and `{"ok": false, "error": {...}}` when it fails, as [options of every command](build.md#options-of-every-command) describes. The sections below list the `result` of each command.

## Shared arguments

Most commands work on one app. They take the app folder, which holds `haylen.json`, as an optional first argument, and the current folder is the app otherwise. The commands of the `plugin` group that work on an app take it with `--app` instead, since their first argument is the plugin.

The commands that resolve an app for a target share these options:

| Option | Meaning |
| --- | --- |
| `--target <name>` | The target, such as `ios-simulator`, `android` or `web`, which `haylen doctor` lists. Without it, a Lua app takes `player` and a C++ app takes the target of this machine: `macos`, `windows` or `linux`. |
| `--config Debug\|Release` | The configuration of the app, `Debug` by default and `Release` for `app package`. A Debug app ships its package as it is and plays in development. A Release app ships its protected release with its bootstrap. |
| `--engine-config Debug\|Release` | The configuration of the engine SDK that the app links, `Release` by default. A Debug SDK is for debugging the engine itself. |
| `--architecture <name>` | An architecture to build, such as `arm64` or `x86_64`. It repeats, and the default architectures of the target apply without it. |

The targets of each backend, with their architectures and the systems they build on, are listed in [targets](distribution.md#targets).

## Apps

### new

```sh
haylen new <folder> [--kind lua|cpp] [--category <category>] [--name <name>] [--identifier <identifier>] [--orientation landscape|portrait|any]
```

Creates an app in a folder that does not exist or is empty: `haylen.json`, the logo under `content/`, the starter code, the bridge handlers of every platform under `native/` and the `.gitignore`, `.editorconfig` and `.clang-format` of an app. A Lua app starts with `source/main.lua`, and a C++ app with `src/App.cpp`, which `cpp.sources` names. The app holds no platform project.

| Option | Default |
| --- | --- |
| `--kind` | `lua`. The value `cpp` makes an app whose C++ code compiles against the engine SDK. |
| `--category` | `utility`. The category that the stores and the app menus show the app in: `game`, `entertainment`, `education`, `productivity`, `utility`, `graphics`, `music`, `video`, `social` or `developer`, as [category](distribution.md#category) maps them. |
| `--name` | The display name, made from the folder name. |
| `--identifier` | `com.example.<folder>`. |
| `--orientation` | `landscape`. |

Result: `folder`, `name`, `identifier`, `kind` and `category`.

### app prepare

```sh
haylen app prepare [app] [--target] [--config] [--engine-config] [--architecture] [--previous-release <folder>]
```

Resolves the app for a target and generates its native project in the build folder of the app, without building it. When no installed SDK of the current engine sources holds an architecture of the target in the configuration of `--engine-config`, it first builds that SDK as `sdk build` does, in a step that says so. Inputs that did not change write nothing. A Release preparation builds the protected release of the app, which follows the release that `--previous-release` names, such as the release that shipped last, which continuous integration keeps, or else the last release of the build folder, so unchanged content keeps its shards. The option takes `--config Release`.

Result: the prepared project, with `identity`, `backend`, `project` (the folder of the generated project), `changed` (whether a file changed) and `state`, the record of `haylen-preparation.json` with `generator`, `backend`, `backendVersion`, `template`, `sdk`, `inputs`, `output` and `extra`.

### app build

```sh
haylen app build [app] [--target] [--config] [--engine-config] [--architecture] [--previous-release <folder>]
```

Prepares the project, builds it with the native tools of the target and verifies the product. The build of the app never compiles the engine. A target whose SDK is missing, was built from other engine sources or has another configuration gets its SDK first, as for `app prepare`, and an SDK that serves no app of the target, such as one that runs only on newer systems than the app, stops the command with the reason. The option `--previous-release` works as for `app prepare`.

Result: `product` and `verification`. The product holds `identity`, `path`, `kind`, `components` (the `target`, `path` and `kind` of each architecture) and `auxiliary` (other files of the build, such as symbol files). The verification holds `passed` and `checks`, each with `name`, `outcome` (`passed`, `warning` or `failed`), `detail` and `source`.

### app verify

```sh
haylen app verify [app] [--target] [--config] [--engine-config] [--architecture]
```

Examines the last build of the app for the target again, without building: the files it made, the architectures it serves, what the platform and the plugins need, and in Release everything that a release never holds. A failed check ends the command with the code 1.

Result: `product` and `verification`, as for `app build`.

### app run

```sh
haylen app run [app] [--target] [--config] [--engine-config] [--architecture] [--previous-release <folder>] [--device <identifier>] [--host <address>] [--port <port>] [--debugger-port <port>] [--coep <policy>] [--coop <policy>] [--open]
```

Checks that the backend of the target runs apps, then builds the SDK that the target lacks, as for `app prepare`, prepares, builds and verifies the app, printing each step, launches it and follows its log until it ends or the key `q` stops it. The option `--previous-release` works as for `app prepare`. A Debug app plays in development with hot reload and the debugger, and a Release app plays as it ships.

| Option | Meaning |
| --- | --- |
| `--device` | The simulator, emulator or device, by an identifier that `haylen devices` lists. The default device of the target otherwise. |
| `--host` | The address that the development server, and the server of a web page, listen on, `127.0.0.1` by default. For an Apple device, the development server listens on the address of this machine on the CoreDevice tunnel of the device unless `--host` names one. |
| `--port` | The port of the development server and of a web page, `8000` by default. |
| `--debugger-port` | The port that an editor attaches its debugger to in Debug, `4711` by default. |
| `--coep` | The `Cross-Origin-Embedder-Policy` of a web page: `require-corp`, `credentialless` or `off`, the default. |
| `--coop` | The `Cross-Origin-Opener-Policy` of a web page: `same-origin`, `same-origin-allow-popups` or `off`, the default. |
| `--open` | Opens a web page in the default browser. |

The keys that act on a running app are listed in [while the app runs](distribution.md#while-the-app-runs).

Result, once the app ends: `product` and `verification`, as for `app build`.

### app package

```sh
haylen app package [app] --target <name> --format <format> [--config] [--engine-config] [--architecture] [--previous-release <folder>] [-o <folder>] [--option <name>=<value>]
```

Checks that the backend of the target packages apps, then builds and verifies the app, in Release by default, and packages it in a format of the backend of its target. The packages go to `dist/` of the app folder unless `-o` names another folder. The release of the build follows the release that `--previous-release` names, as for `app prepare`, such as the depots of the live Steam build.

| Target | Format | Options |
| --- | --- | --- |
| `windows`, `linux` | `installer`: the Inno Setup installer of Windows, or the `.deb` package and the AppImage of Linux. | None. |
| `windows`, `linux` | `steam`: the depots of the app and of its named packs with the build scripts of SteamPipe. | `appId`, `depots` as `<name>=<id>` once per depot, such as `--option depots=windows=481` for one depot, and `branch`. |
| `android` | `bundle`: the Android App Bundle with its asset packs. | None. |
| `android` | `apks`: the APK set of a connected device, built from the bundle in the local testing mode of Google Play and installed. | `device`. |

Result: `format`, `files` (the paths of the packages) and `build` (the build folder of the product).

### app sign

```sh
haylen app sign [app] --target <name> [--kind release|debug] [--option <name>=<value>]
```

Creates a signing key of the app for a target whose backend makes keys. The target `android` writes the keystore, the certificate and the properties of the key into `platform/android/keystore/` of the app, which the `.gitignore` of every app keeps out of its repository.

| Option | Meaning |
| --- | --- |
| `--kind` | `release`, the default, for the upload key of release builds, or `debug` for a debug key of the app. |
| `--option alias=<alias>` | The alias of the key, `upload` by default. |
| `--option password=<password>` | The password of the keystore and the key, `upload` by default. |
| `--option name=<name>` | The distinguished name of the certificate, `CN=Upload, OU=Upload, O=Upload, L=Upload, ST=Upload, C=BR` by default. |
| `--option force=true` | Replaces a key that the app already has. Only `true` replaces it, `false` keeps it, and any other value is an error. |

Result: `files`, the paths that the command wrote.

### app zip

```sh
haylen app zip [app] [-o <file>]
```

Zips the package of the app: `haylen.json`, `source/`, `content/` and the `plugin.json` and `source/` of every plugin under `plugins/<id>/`, wherever the plugin comes from. The zip goes to `app.zip` in the current folder unless `-o` names another file. Any player and the web runtime run it as it is.

Result: `output`, the path of the zip, and `files`, the number of files it holds.

### app shaders

```sh
haylen app shaders [app] [--force]
```

Compiles every shader under `content/shaders/` of the app into a `.shader` package with the code of every shader language of the engine. It compiles only the shaders whose package is out of date, or every one with `--force`.

Result: `sources`, the shader sources, and `compiled`, the packages it wrote.

### app serve

```sh
haylen app serve [app] [--host <address>] [--port <port>]
```

Serves the zipped package of a Lua app and its development server on the local network, until the key `q` or Ctrl+C. A player on another device downloads the package from the address that the command prints and plays it with hot reload. The server listens on every address of this machine unless `--host` names one, on the port `8000` unless `--port` names another.

Result: `address`, the address of the zip.

### app capture

```sh
haylen app capture [app] [--frames <count>]
```

Asks every app that `app run` or `app serve` plays in development in another terminal to capture its next frames on the GPU, one frame unless `--frames` names more, and prints the path of each capture file in the storage of the app.

Result: `results`, what each app answered.

### serve

```sh
haylen serve <folder> [--host <address>] [--port <port>] [--coep <policy>] [--coop <policy>] [--open]
```

Serves a folder, such as a site that `app build --target web` made, with the types, ranges and headers that WebAssembly pages need, until Ctrl+C. The options mean what they mean for `app run`.

Result: `folder` and `url`.

## Plugins and projects

### plugin add

```sh
haylen plugin add <plugin> [--ref <ref>] [--path <folder>] [--reference] [--app <app>]
```

Adds a plugin to an app and lists it in `haylen.json`. The plugin is a folder, or a git repository with the plugin at its root or in the folder that `--path` names, at the branch, tag or commit of `--ref`. Without `--reference` the command copies the plugin into `plugins/<id>/` of the app. With it, `haylen.json` names the folder or the repository as where the plugin comes from, and a repository is pinned to its current commit in `haylen.lock`.

Result: `id` and `version`.

### plugin update

```sh
haylen plugin update <id> [--app <app>]
```

Pins a plugin that comes from a git repository to the commit that its branch or tag names now, in `haylen.lock`.

Result: `id` and `commit`.

### plugin remove

```sh
haylen plugin remove <id> [--app <app>]
```

Removes a plugin from `haylen.json` and `haylen.lock`, and deletes its copy under `plugins/`. A folder or repository that the app referenced stays as it is.

Result: `id`.

### plugin list

```sh
haylen plugin list [--app <app>]
```

Lists the plugins of an app with their versions, the platforms they support, where they come from and what keeps each one from building. A folder under `plugins/` of the app that `haylen.json` does not list shows as not in `haylen.json`, since no build carries it.

Result: `plugins`, each with `id`, `version`, `implementations`, `source`, `problems` and `listed`, whether `haylen.json` lists it.

### plugin new

```sh
haylen plugin new <folder> [--id <id>]
```

Creates a plugin from `templates/plugin` in a folder that does not exist or is empty, with its id in dash-case, the folder name unless `--id` names another. Its native part on every platform answers its method and sends its event from the first build.

Result: `id` and `folder`.

### project inspect

```sh
haylen project inspect [app] [--target] [--config] [--engine-config] [--architecture]
```

Shows the resolved project of the app for a target without generating anything: the target, the backend, the SDK slice of every architecture, the oldest system version with where it comes from and why, the plugins in load order with their parameters, the settings of the sections of `platforms` that apply to the target, each with the key of `haylen.json` it comes from, and every merged contribution with its owners.

Result: `app` (`folder`, `name`, `identifier`, `version`, `build`, `category`, `kind`), `backend`, `identity` and `targets`, one per architecture, each with `target`, `targetName`, `platform`, `kind`, `engineConfiguration`, `sdk`, `minimumSystemVersion` (`value`, `origin`, `justification`), `plugins`, `contributions` (each with `type`, `fields`, `owners` and `origins`), `settings` (each key of each section with its `value` and its `origin`, such as `haylen.json at platforms.apple.team`), `nativeLibraries` and `permissions`.

## Machine and SDKs

### doctor

```sh
haylen doctor [--target <name>]
```

Shows, for every target or for one, its backend with its capabilities, the engine port that builds its engine, its installed SDKs and every tool it needs with its version and path, and what is missing with how to get it. It downloads nothing and accepts no license.

Result: `host` (`system`, `architecture`), `problems` (the backends and engine ports that did not load) and `targets`, each with `target`, `backend`, `capabilities`, `port`, `sdk`, `tools` (each with `name`, `role`, `version`, `path` and `problem`), `problems` and `ready`.

### devices

```sh
haylen devices
```

Lists the simulators, emulators and devices that the backends of this machine launch apps on, with the identifier that `--device` takes and the targets that run on each.

Result: `devices`, each with `backend`, `identifier`, `name`, `kind`, `state` and `targets`, and `problems`, the backends that could not list theirs.

### sdk build

```sh
haylen sdk build --target <name> [--target <name>...] [--config Debug|Release] [--architecture <name>...] [--output <folder>]
```

Builds the engine for one or more targets with the engine port that serves them, and installs the SDK that apps build against. It is the command that compiles the engine for apps, and the app commands that build run it themselves for an SDK that is missing or stale. Every target must belong to one port, the configuration is `Release` by default, and the SDK goes to `build/sdk/<port>-<configuration>/` unless `--output` names another folder. Building a target later adds its slice to an SDK of the same engine sources and configuration.

| Target | SDK |
| --- | --- |
| `player`, `windows`, `linux` | The desktop SDK of this machine, with the desktop player and the content tool. |
| `macos`, `catalyst`, `ios`, `ios-simulator`, `tvos`, `tvos-simulator`, `visionos`, `visionos-simulator` | The Apple SDK, one slice per system and environment. |
| `android` | The Android SDK, one slice per ABI. |
| `web` | The web SDK. |

Result: the manifest `haylen-sdk.json` of the SDK with its `folder`: `format`, `port`, `portVersion`, `engineVersion`, `revision`, `configuration`, `slices`, `hostTools` and `files`.

### sdk join

```sh
haylen sdk join <folder> [<folder>...] [--output <folder>]
```

Joins SDKs that hold different slices into one SDK, such as SDKs that several machines built side by side with `sdk build --output`. The SDKs must come from one engine port, the same engine sources and the same configuration, no two of them may hold the same slice, and the port must declare the capability `join`, which the Apple port does. The joined SDK goes to `build/sdk/<port>-<configuration>/` unless `--output` names another folder, which cannot be one of the SDKs it joins.

```sh
haylen sdk build --target ios --target ios-simulator --architecture arm64 --output build/slices/ios
haylen sdk build --target tvos --target tvos-simulator --architecture arm64 --output build/slices/tvos
haylen sdk join build/slices/ios build/slices/tvos
```

Result: the manifest of the joined SDK with its `folder`, as `sdk build` prints it.

### sdk list

```sh
haylen sdk list
```

Lists the installed SDKs, those under `build/sdk/` and those of the folders that the variable `HAYLEN_SDK_PATH` lists, with their port, configuration, slices and whether they were built from the engine sources of this checkout.

Result: `sdks`, each manifest with its `folder` and `current`.

## Content

The commands of protected releases are explained in the [content guide](../content/content.md). The commands that take `--target` use the content profile of the target, which the targets of one store share.

### content build

```sh
haylen content build [app] --target <name> [--previous-release <folder>]
```

Builds the protected release of the app with the content tool of the desktop SDK of this machine. It follows the last release of the build folder, or the release that `--previous-release` names, so unchanged content keeps its shards.

Result: `profile`, `folder`, `shipped` (the part that ships inside the app) and `packs` (each named pack with its folder).

### content verify

```sh
haylen content verify [app] --target <name> [--release <folder>]
```

Authenticates every manifest and chunk of the last release of the profile, or of the release that `--release` names, with the keys of the app.

Result: `release` and `report`, the lines of the report of the content tool.

### content inspect

```sh
haylen content inspect [app] --target <name> [--release <folder>] [--chunks]
```

Shows the manifests, shards and files of a release, and every chunk of every file with `--chunks`.

Result: `release` and `report`.

### content diff

```sh
haylen content diff [app] <earlier> <later>
```

Compares two release folders: the chunks, shards and files that the later one adds, reuses and drops, and what an installed app downloads to reach it.

Result: `earlier`, `later` and `report`.

### content publish

```sh
haylen content publish [app] --target <name> [--channel <channel>] --output <folder>
```

Publishes the content of the app as the next generation of a channel of a publication tree, which a content server serves as it is. The channel is the one that `content.updates.channel` of `haylen.json` names, `stable` by default, as the engine follows it when the app names none.

Result: `tree`, `channel`, `profile` and `report`.

### content compact

```sh
haylen content compact [app] <tree> [--keep <count>]
```

Removes the manifests and packs of a publication tree that no channel reaches within its last generations, 2 unless `--keep` names another count.

Result: `tree` and `report`.

### content keys

```sh
haylen content keys [app] [--rotate]
```

Shows the key IDs and the public signing key of the app, and creates its keys the first time. With `--rotate` it adds a content key for the next builds. It never prints a key.

Result: `folder`, the key folder, and `report`.

## The engine

### test

```sh
haylen test [--target <target>] [--config Debug|Release|RelWithDebInfo] [--sanitizers address|thread] [--cmake-target <cmake target>] [--shard <index>/<count>] [--gpu] [--graphics-api <api>] [--device <identifier>] [--ui <app>] [--ui-test <class>] [--device-farm <devices>] [--app <folder>] [--port]
```

Builds and runs the engine tests of this machine by default. The [testing guide](testing.md#running-the-tests) explains each mode.

| Option | Meaning |
| --- | --- |
| `--target` | The target that the tests build for and run on, with the names of the app commands and `haylen devices`: `macos`, `windows`, `linux`, `web`, `android`, `ios-simulator`, `tvos-simulator`, `visionos-simulator`, `catalyst`, `ios`, `tvos` or `visionos`, this machine by default. |
| `--config` | The configuration of the build tree, or of the app of `--ui` or `--app`, `Debug` by default. |
| `--sanitizers` | A tree of its own with AddressSanitizer and UndefinedBehaviorSanitizer, or with ThreadSanitizer. |
| `--cmake-target` | A CMake target that builds before CTest runs, which repeats, `haylen-tests` and `haylen-lua-tests` by default. |
| `--shard` | Runs one of the equal parts of the engine tests of this machine, or of the checks of the web with `--target web`, such as `2/4`, so several machines run them side by side and together run each test once. |
| `--gpu` | Runs the GPU tests instead of the engine tests. |
| `--graphics-api` | A name of `gpu::Device::kApiNames` that the GPU tests, or the pages of the checks of the web, use alone. |
| `--device` | The device, simulator or emulator of the target, which `haylen devices` lists. |
| `--ui` | Runs the UI tests of the app of this folder instead, on the simulator, device or emulator of `--target` and `--device`. |
| `--ui-test` | A class of the UI tests of `--ui` to run alone, such as `CameraTests`, which repeats, so a device runs only the tests that need its hardware. Every class runs by default. |
| `--device-farm` | Runs the UI tests of `--ui` on the phones of AWS Device Farm that this list names, separated by commas, with `--target android` or `ios`. |
| `--app` | Builds the app of this folder in the configuration of `--config` and checks instead that it starts: with `--target web` in a page on WebGPU and WebGL2 or on the API of `--graphics-api`, and with `--target android` on the device of `--device` or on an emulator that it starts. |
| `--port` | Builds the engine with the test port and plays its app instead. |

Result: the build tree with what ran, such as `tree`, `configuration`, `sanitizers` and `shard` for the engine tests, `tree` and `graphicsApi` for the GPU tests, with the `output` folder of their images, the `target` or the `abi` and the `device` where they ran on the web, Android and the Apple platforms, `graphicsApi`, `memory` and `shard` for the checks of the web, `app`, `configuration`, `graphicsApis` and `site` for the start check of an app on the web, `app`, `target`, `configuration`, `device` and `graphicsApi` for the start check of an app on Android, `app`, `target`, `simulator` and `screenshots` for the UI tests, and `tree` and `app` for the test port.

### engine configure

```sh
haylen engine configure [--config Debug|Release|RelWithDebInfo] [--sanitizers address|thread] [--xcode]
```

Configures the build tree of the engine of this machine under `build/engine/`, with its tests, benchmarks, tools and player, for developing the engine. With `--xcode` the tree uses the Xcode generator of macOS.

Result: `tree`.

### engine build

```sh
haylen engine build [--config] [--sanitizers] [--xcode] [--cmake-target <cmake target>...]
```

Configures the tree when it needs it and builds every CMake target, or each one that `--cmake-target` names. The option takes CMake targets, so it has a name of its own, and `--target` names the targets of apps and SDKs in every other command.

Result: `tree` and `targets`.

### engine bench

```sh
haylen engine bench [--suite <suite>] [--graphics-api <api>] [--gpu]
```

Builds a benchmark in Release and runs it on this machine. The [benchmarks guide](benchmarks.md) lists the suites and what they measure.

Result: `suite` and `graphicsApis`, the APIs that the benchmark ran on.

### engine fuzz

```sh
haylen engine fuzz [--seconds <seconds>] [--llvm <folder>]
```

Builds the fuzzers of the parsers that read hostile bytes with a Clang that links libFuzzer, and runs each one for 60 seconds unless `--seconds` says otherwise. The option `--llvm`, or the variable `HAYLEN_LLVM`, names the `bin` folder of that LLVM.

Result: `tree`, `fuzzers` and `seconds`.

### engine coverage

```sh
haylen engine coverage [--sanitizers address|thread]
```

Measures the coverage of the engine tests with the source-based coverage of LLVM and writes its report.

Result: `tree` and `report`, the path of the HTML report.

### engine format

```sh
haylen engine format [--check]
```

Formats the C, C++ and Objective-C sources with `clang-format` and the Python of `tools/` with Ruff. With `--check` it changes nothing and fails on what is not formatted, on a lint problem of Ruff and on a multi-line lambda without its `clang-format off` and `on` markers.

Result: `files`, `checked` and `lambdas`, the lambdas without markers.

### engine clean

```sh
haylen engine clean
```

Removes the `build` folder of the checkout: every engine tree, every SDK under `build/sdk/`, the build folders of apps and the logs. The tools in `.tools/` stay.

Result: `removed`, whether there was a folder to remove.

### engine tools

```sh
haylen engine tools [--emscripten]
```

Installs the pinned shader compiler into `.tools/` now instead of at the first build that needs it, and the pinned Emscripten too with `--emscripten`.

Result: the path of each installed tool by its name.

### engine consumers

```sh
haylen engine consumers [--mode subdirectory|cpm|package] [--config Debug|Release] [--sdk <folder>]
```

Builds the consumer project of `engine/tests/consumer`, which adds the engine the way another repository does, through `add_subdirectory`, through CPM, or through `find_package` with an installed SDK. The option `--mode` repeats, and every way runs without it. The configuration is `Release` by default, and `--sdk` names the SDK folder of `package`. By default it is the installed desktop SDK of this machine in that configuration, which `haylen sdk build --target player` builds, and never the SDK of a device platform that holds a slice of this system too, such as the Apple SDK on a Mac.

Result: `consumers`, the ways that built, and `sdk`.
