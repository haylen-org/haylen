# 🚚 Distributing apps

This guide is for app developers who create, run and ship apps. A Haylen app is a package: `app.json`, the Lua modules under `source/` and the assets under `content/`. The same package runs on every platform. The engine is compiled once into prebuilt artifacts for every platform, so a Lua app never compiles the engine. The project of each platform starts as a copy of a template and then belongs to the developer. The script `haylen.py` writes the values of `app.json`, the package and the plugins of the app into one generated folder of that project, builds the project where it is and launches it.

The guide covers the commands of apps, the engine artifacts, the templates, the platform projects and their generated folder, release builds, the check of a built app, the privacy manifest, native libraries, splash screens, development mode, the web loader and the platforms that Haylen supports. The [plugin guide](../platform/plugins.md) covers plugins, which add native capabilities to apps. The [build guide](build.md) covers the command `haylen` itself and how to build the engine, and the [embedding guide](embedding.md) covers C++ projects that compile the engine through CMake.

## Quick start

```sh
haylen new ~/apps/my-game --name "My Game" --identifier com.example.mygame
haylen run ~/apps/my-game
haylen run ~/apps/my-game --platform ios-simulator
haylen run ~/apps/my-game --platform android
haylen run ~/apps/my-game --platform web
```

The command `new` creates the app from the starter app and copies every platform template into its folder `platform/`, where the projects belong to the developer. The command `run` without `--platform` starts the desktop player of this machine in development mode, which reloads edited scripts and assets. With `--platform` it builds the engine artifacts when they are missing or stale, prepares the project of the platform and builds it. Then it checks what the built app lacks, launches it on a simulator, an emulator, a device, this Mac or a local web server, and streams the output of the app.

A C++ app is a CMake project that compiles the engine. The option `--kind cpp` of `new` creates one, and `run-cpp` builds and runs it:

```sh
haylen new ~/apps/my-cpp-game --kind cpp
haylen run-cpp ~/apps/my-cpp-game
haylen run-cpp ~/apps/my-cpp-game --platform android
```

## Commands

The commands of apps name an app by its folder, relative to the current folder or absolute. A command without an app, or with a path that holds no `app.json`, stops with an error that says what an app folder is. It never runs another app in its place. A folder that holds `CMakeLists.txt` is a C++ app. The commands `run`, `prepare`, `xcodegen` and `dist` build the platform projects of Lua apps, so they stop for a C++ app and name `run-cpp` instead.

Several commands share these options:

| Option | Default | Meaning |
| --- | --- | --- |
| `--config` | `Debug` | The configuration of the project of the platform. `Debug` ships the package as it is, and `Release` ships the protected release and the bootstrap of the app, as [release builds](#release-builds) explain. |
| `--engine-config` | `Release` | The configuration of the engine: of the artifacts that the project links, of the desktop player and of the content tool. Pick `Debug` only to debug the engine itself, because it runs many times slower. |
| `--jobs` | the processors minus one | The parallel jobs of the builds, which `xcodebuild`, Gradle and CMake receive. |
| `--previous-release` | the last release of the build folder | A folder that holds the last release of the app that shipped, such as the files of its Steam depots, its unpacked app or a copy of its release folder. A Release build follows it, so a clean machine such as continuous integration keeps the shards of unchanged content byte for byte, as the [content guide](../content/content.md#building-content) explains. |

### engine

```sh
haylen engine
haylen engine --platform apple --config Release
```

Builds the [engine artifacts](#engine-artifacts) into `build/artifacts/`. The option `--platform` picks `apple`, `android`, `web`, `desktop` or `all`, which is the default. The option `--config` is `Release` by default, and `--jobs` sets the jobs.

Each build records its configuration and a SHA-256 hash of the engine sources in `build/artifacts/manifest.json`. The hash covers every file under `engine/` except the tests, the benchmarks and build outputs. So any change to the engine code, its CMake files, its shaders or its platform projects makes the artifacts stale. The commands `run`, `prepare` and the others build the artifacts of a platform again before they use them, whenever the manifest does not match the current sources or the configuration that `--engine-config` asks for.

On Apple platforms the manifest records each slice of `Haylen.xcframework` on its own. The command `run` builds only the slice that it runs on, such as the slice of the iOS simulator, so a change to the engine builds one slice before a run. The commands `prepare`, `xcodegen` and `engine` build every slice, since a project of the developer may build for any Apple platform.

### new

```sh
haylen new ~/apps/space-race --orientation portrait
haylen new ~/apps/space-race-cpp --kind cpp
```

Creates an app from the [templates](#templates). The folder must not exist, or must be empty.

| Option | Default | Meaning |
| --- | --- | --- |
| `--kind` | `lua` | The kind of the app: `lua`, whose code is Lua, or `cpp`, a CMake project whose C++ code compiles the engine. |
| `--name` | the name of the folder in title case | The display name. |
| `--identifier` | `com.example.<folder>`, with the dashes of the folder name left out | The reverse domain identifier, such as `com.example.spacerace`. |
| `--orientation` | `landscape` | `landscape`, `portrait` or `any`. |

The command copies `templates/common` first, then `templates/lua` or `templates/cpp`, whose files replace the common files of the same path. The app gets its package, its `.gitignore`, `.editorconfig` and `.clang-format`, and the project of every platform template of its kind under `platform/<platform>`:

- A Lua app gets `apple`, `android`, `linux`, `web` and `windows`.
- A C++ app gets `android` and `web`, since its CMake project builds the other platforms itself.

Then the command writes the name, the identifier and the orientation into `app.json`, and gives the window the name as its title. For `portrait` it turns the window and the design resolution upright. The projects belong to the developer, who edits them as the company wants. The script `haylen.py` builds them where they are and writes only their folder `haylen/`, as [platform projects](#platform-projects) describe.

### run

```sh
haylen run ~/apps/my-game
haylen run ~/apps/my-game --platform macos
haylen run ~/apps/my-game --platform catalyst
haylen run ~/apps/my-game --platform ios-simulator --device "iPad Pro 13-inch (M5)"
haylen run ~/apps/my-game --platform tvos-simulator
haylen run ~/apps/my-game --platform android --device emulator-5554
haylen run ~/apps/my-game --platform web --open
haylen run ~/apps/my-game --platform android --config Release --build-only
```

Runs an app. First it compiles the shaders of the app whose sources changed, as [shaders](#shaders) describes. It takes the shared options of [commands](#commands) and these:

| Option | Default | Meaning |
| --- | --- | --- |
| `--platform` | none | `macos`, `catalyst`, `ios`, `ios-simulator`, `tvos`, `tvos-simulator`, `android`, `web`, `windows` or `linux`. Without it, the desktop player runs the app folder in development mode. |
| `--device` | see below | The name or id of a simulator, the id of an Apple device or the serial of an Android device. |
| `--build-only` | off | Builds and checks the app for the platform without launching it, which needs no device, simulator or server. Continuous integration builds the apps of every platform this way. A build-only Android run packs the native libraries of every ABI. |
| `--host` | `127.0.0.1` | The address that the local web server listens on, and the development server of a Debug run on a device, a simulator or this Mac. The address of this machine on the network lets phones and other computers open the page, which then runs without sound, because browsers offer `AudioWorklet` only to pages served over https or from localhost. Apple devices reach the development server on every address of this machine unless this option names one. |
| `--port` | `8000` | The port of the local web server, and of the development server of a Debug run. |
| `--coep`, `--coop`, `--open` | off | The headers and the browser of the web server, as [serve](#serve) describes. |

Without `--platform`, `run` does this:

1. It builds the `haylen` player of this machine in the configuration of `--engine-config`, in the build tree `build/<host>-<config>`.
2. It builds or copies the [native libraries](#native-libraries) of the app for this machine into `native/development` of the [build folder of the app](#platform-projects).
3. It starts the player with `--dev`, with `--native` and that folder when there are libraries, and with the app folder.
4. While the player runs, it compiles every shader source that changes again, and the player then reloads the shader.

With `--platform` it prepares the project of the platform, as [platform projects](#platform-projects) describe, and then builds, checks and launches it:

| Platform | Build | Launch |
| --- | --- | --- |
| `macos` | Generates `App.xcodeproj` again when it may, then runs `xcodebuild` on the `macOS` scheme into the build folder of the app. | Starts the executable of the app bundle. The engine writes its log to the standard output there, so the log stays in the terminal. |
| `catalyst` | Runs `xcodebuild` on the `iOS` scheme for the Mac Catalyst destination. | Starts the executable of the Mac Catalyst bundle and streams its log with `log stream`. |
| `ios-simulator`, `tvos-simulator` | Runs `xcodebuild` on the `iOS` or `tvOS` scheme for the simulator. | Runs `xcrun simctl boot`, `install` and `launch --console-pty`, and streams the log of the app with `log stream` inside the simulator. The simulator is the one that `--device` names, by name or id, or else a booted one, or else the first iPhone or Apple TV available. |
| `ios`, `tvos` | Runs `xcodebuild` for the device, signed with the team that the environment variable `HAYLEN_APPLE_TEAM` names. | Runs `xcrun devicectl device install app` and `device process launch --console` on the device whose id `--device` gives. |
| `android` | Runs the Gradle task `:app:assembleDebug`, or with `--config Release` the release tasks of [release builds](#release-builds), into the build folder of the app. A debug build names the ABI of the device with the Gradle property `haylen.abi`, and the debug build type of the template then packs only the native libraries of that ABI. | Installs the newest APK of the configuration with `adb install -r`, starts its launcher activity with `adb shell am start`, and streams the log of the app process. The device or emulator is the one whose serial `--device` gives, which you may leave out when only one is connected. |
| `web` | Copies the files of the web project into the site, with the prebuilt runtime and `app.zip`, or with `--config Release` the protected release in `app/` and runtimes linked for the app. Then it writes `config.json` next to them. | Serves the site on `--port` with the `--coep` and `--coop` policies, and opens it with `--open`. |
| `windows`, `linux` | CMake builds the project with Ninja, or with the default generator on Windows without Ninja, into `cmake/<platform>-<config>` of the build folder of the app. Then it installs the app into `build/apps/<app>-<hash>/installed/<platform>/`, as [Windows and Linux](#windows-and-linux) describe. | Runs the installed executable, `<executable>.exe` on Windows and `bin/<executable>` on Linux, which plays the folder `app` next to it. |

Before it builds, `run` checks the plugins of the app and their values for the platform, and stops with every problem it finds. After the build it runs [check](#check) and prints what it finds as warnings. In the Debug configuration it launches the app connected to a development server, as [development mode](#development-mode) describes. It streams the output of the app until the app exits or Ctrl+C stops it.

Where the log of the engine goes:

- On desktops, to the standard output.
- On Android, to the log of the process, which `adb logcat --pid` streams, with `haylen` as the tag.
- On iOS, tvOS and Mac Catalyst, to the unified log, with the bundle identifier of the app as the subsystem and `haylen` as the category.

On Mac Catalyst and the simulators, `run` streams those lines next to the standard output and error of the process, with warnings and errors on the standard error. It keeps streaming for one second after the app ends, so the last lines arrive. The log of an app on a device shows in Console.app, while `run` shows the standard output and error of its process.

### run-cpp

```sh
haylen run-cpp ~/apps/my-cpp-game
haylen run-cpp ~/apps/my-cpp-game --platform web
haylen run-cpp ~/apps/my-cpp-game --platform ios-simulator --device "iPhone 17"
haylen run-cpp ~/apps/my-cpp-game --platform catalyst
haylen run-cpp ~/apps/my-cpp-game --platform android --device emulator-5554
```

Builds a C++ project that compiles the engine through CMake and runs it. The project is a folder with a `CMakeLists.txt` that adds the engine and calls `haylen_add_app`, as the [embedding guide](embedding.md) shows. The command configures it in `build/cpp/<project>-<hash>/<platform>-<config>`. The part `<hash>` is the start of the SHA-256 hash of the absolute folder of the project, so projects in folders with the same name never share a build tree. It builds the app target and runs it. It takes the shared options of [commands](#commands) and these:

| Option | Default | Meaning |
| --- | --- | --- |
| `--platform` | this machine | This machine, `web`, `catalyst`, `ios`, `ios-simulator`, `tvos`, `tvos-simulator` or `android`. |
| `--target` | `main` | The target of `haylen_add_app`, which the [C++ template](#cpp) names `main`. |
| `--config` | `Debug` | `Debug`, `Release` or `RelWithDebInfo`. The value `Release` ships the protected release of the app, built with the content tool of the desktop artifacts, and inspects the built app before it launches, as the [embedding guide](embedding.md#protected-releases) describes. |
| `--device` | see below | The name or id of a simulator, the id of an Apple device or the serial of an Android device. |
| `--build-only` | off | Builds and checks the app without launching it, as continuous integration does on Windows and Linux. |
| `--host`, `--port`, `--coep`, `--coop`, `--open` | as in `run` | The local web server of the `web` platform, as [serve](#serve) describes. |

The command hands the project the engine of this checkout in `HAYLEN_ENGINE_SOURCE`, the shader compiler in `HAYLEN_SHADERC` and the dependency cache of the engine builds in `CPM_SOURCE_CACHE`. So a project anywhere on the machine compiles the engine without downloading its dependencies again. C++ projects compile the engine from source, so only Android uses an engine artifact, for its Java classes.

| Platform | Build | Launch |
| --- | --- | --- |
| This machine | Ninja, or the default generator on Windows without Ninja. On a Mac, the macOS minimum of the engine as `CMAKE_OSX_DEPLOYMENT_TARGET`. | Runs the executable. |
| `web` | Emscripten into `build/cpp/<project>-<hash>/web`, with an `index.html` that loads the plugins and the runtime, which draws through WebGPU or WebGL2, as [the files of a web build](build.md#files-of-a-web-build) show. | Serves that folder like `serve`. |
| `ios-simulator`, `tvos-simulator` | The Xcode generator with the SDK of the iOS or tvOS simulator for the architecture of this Mac, which compiles the launch screen and signs the bundle. | Like `run`, on the simulator that `--device` names, a booted one or the first one available. |
| `ios`, `tvos` | The Xcode generator with the SDK of the device, signed with the team of `HAYLEN_APPLE_TEAM`. | Like `run`, on the device that `--device` names. |
| `catalyst` | Ninja with `engine/cmake/haylen-catalyst.toolchain.cmake` for the architecture of this Mac. The function `haylen_add_app` signs the bundle ad hoc, as [Apple bundles](embedding.md#apple-bundles) explain. | Like `run`: starts the executable and streams its log. |
| `android` | Ninja with the NDK, for the ABI of the device that `--device` names or of the only one connected, into `bin/<target>/lib<target>.so`. Then it prepares the Android project of the package that `bin/<target>/package.txt` names: its `platform/android`, or the copy of the template. The library goes into `haylen/jniLibs/<abi>`, and `library` of `haylen/haylen.properties` names the target. So `HaylenActivity` loads the library of the app, and the Lua player of the `haylen` library stays out of the APK. The `haylen` library comes from the artifacts, built with `--engine-config` when they are missing or stale. | Like `run`. |

The Apple and web builds also bring the native parts of the plugins of the app, as the [plugin guide](../platform/plugins.md#c-apps) describes.

### prepare

```sh
haylen prepare ~/apps/my-game --platform ios-simulator
haylen prepare ~/apps/my-game --platform android --config Release
```

Builds the engine artifacts of the platform when they are missing or stale. Then it writes the folder `haylen/` of the project of the platform, and nothing else. That is enough to open the project in Xcode, Android Studio or an editor that opens CMake projects, and to build, run or archive it there. On the web it makes the site. The option `--platform` is required and takes the platforms of `run`. The native libraries of an Apple project are those of the platform that `--platform` names. The command takes the shared options of [commands](#commands). The value `--config Release` writes the protected release and the bootstrap of the app, which an archive for a store needs, as [release builds](#release-builds) describe.

### xcodegen

```sh
haylen xcodegen ~/apps/my-game
haylen xcodegen --template
```

Prepares the Apple project of an app and generates its `App.xcodeproj` again from `project.yml`, with the XcodeGen that haylen.py pins. Run it after you change `project.yml`, as [generating App.xcodeproj](#generating-appxcodeproj) describes. The app is the current folder by default. The option `--platform` picks the Apple platform whose native libraries the project gets, `macos` by default. The option `--config` picks the package, `Debug` by default, and the command takes the other shared options of [commands](#commands).

The option `--template` generates the `App.xcodeproj` of the Apple template of the engine again, after a change to its `project.yml`. It works in a new starter app in `build/templates/lua`, whose folder `haylen/` links no plugins and no native libraries, and copies the result into the template.

### check

```sh
haylen check ~/apps/my-game --platform ios-simulator
haylen check ~/apps/my-game --platform web --config Release --coop same-origin
```

Compares the last build of an app for a platform with what the engine and every plugin of the platform need. It prints each missing requirement with who needs it, why, and the snippet to add with the file to put it in. The command fails when anything is missing, so continuous integration can run it. The option `--platform` is required, `--config` picks the build to check, `Debug` by default, and `--coop` names the opener policy that the server of a web site sends.

The command `run` runs the same check after each build and prints what it finds as warnings. A feature whose requirement is missing logs once and answers `unsupported` instead of stopping the app, as the [plugin guide](../platform/plugins.md#requirements) describes. A Release build is different: what a release never ships stops `run` before it launches, as [release builds](#release-builds) explain.

| Platform | What it reads | What it checks |
| --- | --- | --- |
| Apple | The `Info.plist` of the bundle, the frameworks that `otool -L` lists for the executable, the entitlements that `codesign` reports, the privacy manifest of the bundle and the classes of the executable. With `--config Release`, every file of the bundle. | The system frameworks of the engine and, on iOS and tvOS, the scene manifest. For every plugin: its class, its `frameworks`, its `infoPlist` keys, its `entitlements`, its `privacy` declarations and its resources. A value only has to exist, so a usage description may have other words, while items of arrays and booleans must match. |
| Android | The merged manifest of the APK through `aapt2 dump xmltree`, and its native libraries. With `--config Release`, every entry of the APK. | `HaylenActivity`, the provider that loads plugins in an app with plugins, and the native libraries of the app. For every plugin: the permissions, components and meta-data of the manifest of its module, and in a project of the developer the `files` it places there. A permission that the manifest of the app removes with `tools:node="remove"` is named with the file that removes it. With `--config Release`, also every shard that the APK compresses. |
| Web | The `config.json` of the site and the modules of the plugins. | Every plugin with a web part in `config.json`. With `--coop same-origin`, every plugin whose web module registers screens, since that opener policy cuts popups off from the page. With `--config Release`, also any zipped package. |
| Windows, Linux | The folder of the installed app and, on Windows, the application manifest in the resources of the executable. | The executable, the package and the native libraries where the engine finds them. On Windows, the Common Controls of version 6 and UTF-8 as the code page in the manifest. On Linux, the desktop entry named after the identifier. |

With `--config Release`, every platform also checks for what a release never ships, as [release builds](#release-builds) list.

```text
Warning: The plugin "native-demo" needs "NSCameraUsageDescription" in the "Info.plist" of the app, which the built app lacks.
  Add to platform/apple/ios/Info.plist:
    <key>NSCameraUsageDescription</key>
    <string>The Native Demo plugin asks for the camera to show how a plugin requests a permission.</string>
Warning: The plugin "native-demo" needs the permission "android.permission.POST_NOTIFICATIONS", which the merged manifest of the built app lacks.
  The file platform/android/app/src/main/AndroidManifest.xml removes it with the attribute "tools:node" set to "remove". Delete that entry, or keep it, and the calls that need the permission answer "unsupported".
```

### platform

```sh
haylen platform add ~/apps/my-game android
haylen platform diff ~/apps/my-game --template apple
```

The command `platform add <app> <template>` creates `platform/<template>/` of an app from the template of the platform: the common layer and the layer of the kind of the app. The app then builds that project instead of the copy that haylen.py keeps. It stops when the app already has the project. A template that the kind lacks, such as `apple` for a C++ app, stops the command with the list of the templates that the kind has.

The command `platform diff <app> --template <template>` compares `platform/<template>/` of an app with the same layered template. It prints the files that only one of them has, and a unified diff of every text file that differs. It leaves out `haylen/`, build outputs and the state of Xcode for each person. So the developer adopts what they want of a newer template. It changes nothing.

### plugin

```sh
haylen plugin list
haylen plugin add ~/plugins/camera-scanner --app ~/apps/my-game
haylen plugin add https://github.com/haylen-org/background-assets.git --ref main --app ~/apps/my-game
haylen plugin remove camera-scanner --app ~/apps/my-game
haylen plugin new ~/plugins/my-plugin --with camera,microphone
```

| Command | What it does |
| --- | --- |
| `plugin add <folder or repository> [--ref <ref>] [--app <app>]` | Copies a plugin folder, or the root of a plugin repository at the branch, tag or commit that `--ref` names, into `plugins/<id>/` of the app. It lists the plugin in the `plugins` section of `app.json`, with the defaults of its parameters and an empty text for each required one. |
| `plugin remove <id> [--app <app>]` | Deletes the plugin from `plugins/` and from `app.json`. |
| `plugin list [--app <app>]` | Lists the plugins of the app with their versions, platforms and the problems that keep them from building. |
| `plugin new <folder> [--id <id>] [--with <features>]` | Creates a plugin from `templates/plugin/`, with the camera, microphone, location or banner features of `templates/plugin-features/` that `--with` names. |

The option `--app` is the current folder by default. The [plugin guide](../platform/plugins.md) describes these commands in depth, the `plugins` section of `app.json`, the checks that `run` makes before it builds and the format of a plugin.

### package

```sh
haylen package ~/apps/my-game -o dist/my-game.zip
```

Compiles the shaders of the app whose sources changed. Then it zips `app.json`, `source/` and `content/` of the app, with `plugin.json` and `source/` of every plugin that `app.json` lists, and nothing else of its folder. It skips `.DS_Store` files. The zip goes to `app.zip` in the current folder, or to the file that `-o` or `--output` names. The desktop player runs the zip with `haylen app.zip`, and a web page hands it to the runtime, as [the web loader](#web-loader) describes.

### shaders

```sh
haylen shaders ~/apps/my-game
haylen shaders ~/apps/my-game --force
```

Compiles every source under `content/shaders/` that declares an `@program` into a `.shader` package next to it. The package holds the code of every shader language that the engine runs. The command uses the Haylen shader compiler `haylen-shaderc`, which haylen.py provides as [the shader compiler](build.md#the-shader-compiler) explains. Sources without a program are files that the others include.

A shader compiles again when its `.shader` file is older than any shader source of the app or of the shader library, and `--force` compiles them all. A compile error names the file, the line and the column, and fails the command. The [shader guide](../graphics/shaders.md) explains the sources and the files. The web editor cannot compile shaders, so apps ship their `.shader` files in their package.

### serve

```sh
haylen serve dist/my-game --port 8000
haylen serve dist/my-game --open
haylen serve dist/my-game --host 192.168.1.20
```

Serves a folder at `http://127.0.0.1:<port>/` with a threading Python server. The option `--port` is `8000` by default, and `--open` opens the page in the default browser. The option `--host` listens on another address of this machine instead, such as its address on the network, so phones and other computers open the page. Browsers offer `AudioWorklet` only to pages served over https or from localhost, so those pages run without sound. The commands `run --platform web` and `run-cpp --platform web` take the same options. The server sends these headers:

| Header | Value |
| --- | --- |
| `Cross-Origin-Opener-Policy` | None by default, `same-origin-allow-popups` with `--coop same-origin-allow-popups`, and `same-origin` with `--coop same-origin`. |
| `Cross-Origin-Embedder-Policy` | None by default, `credentialless` with `--coep credentialless`, and `require-corp` with `--coep require-corp`. |
| `Cross-Origin-Resource-Policy` | `cross-origin`. |
| `Access-Control-Allow-Origin` | `*`. |
| `Cache-Control` | `no-cache`. |
| `ETag`, `Accept-Ranges` | A strong validator of each file, and `bytes`. The server answers one byte range of a file with `206`, `Content-Range` and the bytes asked for, as a content delivery network serves the shards of a protected release. A range whose `If-Range` names another validator gets the whole file. |

The web runtime runs on one thread, so its pages need no cross-origin isolation, and the server sends neither policy by default. Each policy would break something that plugins need:

- An opener policy of `same-origin` puts the sign-in and payment popups of plugins into a browsing context group of their own, which cuts them off from the page, as the [plugin guide](../platform/plugins.md#web-screens) explains.
- An embedder policy of `require-corp` blocks the scripts of third parties that plugins load, such as those of ads, analytics and sign-in, which do not send the resource policy it asks for.

The option `--coop same-origin` with `--coep credentialless` or `--coep require-corp` makes a page cross-origin isolated, for pages that use `SharedArrayBuffer` or threads of their own. The option `--coop same-origin-allow-popups` keeps the popups of the page linked to it.

The server names the MIME type of `.wasm` (`application/wasm`), `.js` and `.mjs` (`text/javascript`), `.json`, `.zip`, `.html`, `.css`, `.svg`, `.png` and `.data` itself. A request for a file that also exists as `<file>.br` or `<file>.gz` receives the compressed copy with its `Content-Encoding`, when the browser accepts that encoding.

### android-key

```sh
haylen android-key ~/apps/my-game
haylen android-key ~/apps/my-game --debug
haylen android-key ~/apps/my-game --alias store --password "a long secret" --dname "CN=My Company, O=My Company, C=US"
```

Creates the upload key that signs the release builds of an app. With `--debug` it creates a debug key of the project instead, which signs its debug builds in place of the debug key of the Android SDK. So every machine of a team builds with the same signature.

| Option | Default | Meaning |
| --- | --- | --- |
| `--release` | on | Creates the upload key of release builds. It excludes `--debug`. |
| `--debug` | off | Creates the debug key of the project. |
| `--alias` | `upload` | The alias of the key. |
| `--password` | `upload` | The password of the keystore and of the key, at least 6 characters, which `keytool` asks of every keystore. |
| `--dname` | `CN=Upload, OU=Upload, O=Upload, L=Upload, ST=Upload, C=BR` | The distinguished name of the certificate. |
| `--force` | off | Replaces a key that the project already has. |

The command runs `keytool -genkeypair` for an RSA key of 2048 bits that stays valid for 10000 days, in a PKCS12 keystore. Then it exports the certificate as PEM with `keytool -exportcert -rfc`, which is the file that a store asks for when it registers or resets an upload key. The password reaches `keytool` through the environment, so the printed commands and the list of processes never show it. The command finds `keytool` in the JDK of `JAVA_HOME`, or else on `PATH`.

The key goes into `platform/android/keystore/` of the app as `<type>.jks`, `<type>.pem` and `<type>.properties`, where the type is `release` or `debug`. An app without `platform/android/` gets the project from the template first. The command refuses to replace a key of the project unless `--force` is given, since an app signed with another key cannot update the installed one. The `.gitignore` of the app keeps the folder out of the repository.

The app module of the Android template signs each build type with the key that `keystore/<type>.properties` describes, which names the keystore next to it:

```properties
storeFile=release.jks
storePassword=upload
keyAlias=upload
keyPassword=upload
```

Continuous integration, where the folder does not exist, gives a key through Gradle properties or environment variables instead:

| Gradle property | Environment variable | Value |
| --- | --- | --- |
| `haylen.release.storeFile` | `HAYLEN_RELEASE_STORE_FILE` | The keystore, as an absolute path or a path relative to the Android project. |
| `haylen.release.storePassword` | `HAYLEN_RELEASE_STORE_PASSWORD` | The password of the keystore. |
| `haylen.release.keyAlias` | `HAYLEN_RELEASE_KEY_ALIAS` | The alias of the key. |
| `haylen.release.keyPassword` | `HAYLEN_RELEASE_KEY_PASSWORD` | The password of the key. |

The debug key takes the same names with `debug`, such as `haylen.debug.storeFile` and `HAYLEN_DEBUG_STORE_FILE`. A key needs its four values. The properties file wins over the Gradle properties, and a Gradle property wins over its environment variable. Gradle reads its properties from `gradle.properties` of the project and of the Gradle home, `GRADLE_USER_HOME`, and the Gradle home wins. A release build without an upload key stops before it compiles, with a message that names both ways to add one. A debug build without a key of the project signs with the debug key of the Android SDK.

A workflow keeps the keystore as Base64 in a secret and writes it to a file before the build:

```yaml
env:
  HAYLEN_RELEASE_STORE_FILE: ${{ runner.temp }}/release.jks
  HAYLEN_RELEASE_STORE_PASSWORD: ${{ secrets.ANDROID_KEYSTORE_PWD }}
  HAYLEN_RELEASE_KEY_ALIAS: ${{ secrets.ANDROID_KEY_ALIAS }}
  HAYLEN_RELEASE_KEY_PASSWORD: ${{ secrets.ANDROID_KEY_PWD }}
steps:
  - run: echo "${{ secrets.ANDROID_KEYSTORE }}" | base64 --decode > "$HAYLEN_RELEASE_STORE_FILE"
```

The command `base64 < platform/android/keystore/release.jks` prints the text of the `ANDROID_KEYSTORE` secret. The guide [installers in CI](ci-installers.md) builds the App Bundle with these secrets. The command `apksigner verify --print-certs` of the build tools of the Android SDK shows the certificate that signed an APK.

### dist

```sh
haylen dist ~/apps/my-game --platform windows
haylen dist ~/apps/my-game --platform linux -o ~/releases/my-game
```

Builds what ships a Windows or Linux app, on a machine of that system. The option `--platform` is required and names this machine, `windows` or `linux`. The command takes `--engine-config`, `--jobs` and `--previous-release` too. It works in these steps:

1. It compiles the shaders of the app and prepares the project of the platform in the Release configuration.
2. It builds the project and installs it.
3. It stops on what a release never ships, as [check](#check) does.
4. It runs CPack with the configuration that `CMakeLists.txt` of the project describes.

The packages land in `dist/` of the app folder, which its `.gitignore` keeps out of the repository, or in the folder of `-o` or `--output`.

| Platform | Packages | Tools |
| --- | --- | --- |
| Windows | `<executable>-<version>-setup.exe`, an installer that installs the app for the person or, with an administrator, for every user. It adds a shortcut in the Start menu, an optional one on the desktop, the option to run the app at the end, and an uninstaller in the settings of Windows. [Signing](#signing) covers the installer and the uninstaller. | Inno Setup, which haylen.py installs into `.tools`, as the [build guide](build.md#tools-that-haylenpy-brings) lists. |
| Linux | `<executable>_<version>_<arch>.deb`, which installs into `/usr` with the dependencies that `dpkg-shlibdeps` finds in the executable, and GTK 3 and the sound servers as recommendations. And `<executable>-<version>-<arch>.AppImage`, one file that runs on any distribution without installing. | The DEB generator of CPack, with `dpkg-shlibdeps` of `dpkg-dev`. The AppImage generator of CPack, which needs CMake 4.2 or later, with appimagetool, the AppImage runtime and patchelf, which haylen.py downloads into `.tools`. |

Inno Setup makes the Windows installer for several reasons. It is free, it has had regular releases for more than two decades, it compiles a script from the command line, which CPack writes, and it signs the installer and its uninstaller through the signing command it receives. MSI packages suit software that companies deploy to many machines, while an app for people needs a plain installer.

The `.deb` reaches Debian, Ubuntu and the distributions based on them through their package manager, which resolves the libraries of the system that the app links. The AppImage serves every other distribution with one file that needs no installation, no permission of the administrator and no runtime. The packages link the C library and the graphics libraries of the system that builds them. So they run on distributions as new as that one or newer, and a release that reaches older ones builds on the oldest distribution it supports.

#### Signing

A Windows app without a signature makes SmartScreen warn the person who installs it. So the release builds of the Windows project sign the executable, the installer and the uninstaller with `signtool` of the Windows SDK. They use a SHA-256 digest and an RFC 3161 time stamp, which keeps the signature valid after the certificate expires. The certificate never belongs in the project or the repository, so `signing.cmake` reads it from the environment:

| Environment variable | Value |
| --- | --- |
| `HAYLEN_WINDOWS_CERTIFICATE` | A PFX file with the code signing certificate and its private key. |
| `HAYLEN_WINDOWS_CERTIFICATE_PASSWORD` | The password of the PFX file. |
| `HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT` | Instead of a file, the SHA-1 thumbprint of a certificate in the certificate store of the user, such as one on a hardware token, whose private key never leaves it. |
| `HAYLEN_WINDOWS_TIMESTAMP_URL` | The time stamp server of the authority that issued the certificate. |
| `HAYLEN_WINDOWS_SIGNTOOL` | Another `signtool` than the one on `PATH` or in the newest Windows SDK. |

The project decides when CMake configures it. With `HAYLEN_WINDOWS_CERTIFICATE` or `HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT` it signs. Without either, the app stays unsigned, and the install of every release build says so. A workflow of GitHub Actions keeps the PFX file as Base64 in a secret and writes it to a file before the build:

```yaml
env:
  HAYLEN_WINDOWS_CERTIFICATE: ${{ runner.temp }}\certificate.pfx
  HAYLEN_WINDOWS_CERTIFICATE_PASSWORD: ${{ secrets.WINDOWS_CERTIFICATE_PWD }}
  HAYLEN_WINDOWS_TIMESTAMP_URL: ${{ secrets.WINDOWS_TIMESTAMP_URL }}
  HAYLEN_KEYS_DIR: ${{ runner.temp }}\keys
steps:
  - shell: pwsh
    run: |
      [IO.File]::WriteAllBytes($env:HAYLEN_WINDOWS_CERTIFICATE, [Convert]::FromBase64String("${{ secrets.WINDOWS_CERTIFICATE }}"))
      python haylen.py dist my-game --platform windows
```

The guide [installers in CI](ci-installers.md) gives the whole workflow of every platform and explains each secret. The content keys of the app reach the workflow through `HAYLEN_KEYS_DIR`, as [the content guide](../content/content.md) describes, since every release of an app needs the keys of its earlier releases. The command `signtool verify /pa /v my-game-1.0.0-setup.exe` shows the signature of an installer.

Linux packages carry no signature of their own. The repositories that publish `.deb` packages sign their indexes. The AppImage takes a GnuPG signature from `CPACK_APPIMAGE_SIGN` and `CPACK_APPIMAGE_SIGN_KEY` in the project.

### steam

```sh
haylen steam ~/apps/my-game --platform windows --app-id 480 --depot windows=481 --depot forest=482
```

Builds the release of an app for macOS, Windows or Linux, inspects it as `check --config Release` does, and lays it out for SteamPipe, the upload system of Steam. Steam installs the app with its release, so the app needs no content server.

| Option | Default | Meaning |
| --- | --- | --- |
| `--platform` | required | `macos`, `windows` or `linux`, the platform of the depot of the app. |
| `--app-id` | required | The ID of the app on Steam. |
| `--depot <name>=<id>` | required | The ID of a depot from Steamworks. Repeat it for every depot: the platform is one name, and each pack of the app is another. The command stops before it builds when one is missing. |
| `--branch` | none | The beta branch that the build goes live on, such as `beta`. Without it, the build waits in Steamworks to be set live. |
| `--output` | `build/apps/<app>-<hash>/steam` | The folder of the staging. |
| `--target` | `main` | The `haylen_add_app` target of a C++ app. |

The command also takes `--engine-config`, `--jobs` and `--previous-release`. It writes this layout:

| Folder | What it holds |
| --- | --- |
| `content/<platform>/` | The depot of the app: the installed Windows or Linux app with its release, or the macOS bundle. It holds every shard of the release except those of the [packs](../content/content.md#delivery) of the app, or only the shards that its required content needs when `app.json` names a content server. |
| `content/<pack>/` | The depot of each pack. Its shards install into `packs/` next to the executable, in `lib/<executable>/` on Linux, or next to the macOS bundle, where the runtime finds them. A player who does not own a pack, such as an expansion sold apart, never receives its depot. Its assets stay unavailable, and `assets.loadAsync` of one fails with `storeAssetUnavailable`, which an app shows as content to buy. |
| `scripts/` | The build scripts of `steamcmd`: `app_build.vdf` with the app ID, a description of the app and its version, and the branch of `--branch`, and one `depot_build_<id>.vdf` per depot that maps its whole folder. |

A C++ app stages its depots the same way from the folder of its CMake project. Its Release build on this machine makes the executable with its release next to it, or the macOS bundle, and the shards of each pack, which `content/<pack>/packs/` installs next to the executable or the bundle. Its CMake project builds for the machine it runs on, so `--platform` must name this machine. The depots of each platform are staged on that platform, such as in one job of continuous integration per platform.

Steam compares each depot with the build before it, file by file. A release keeps every unchanged shard byte for byte under the same name, so an update of a few assets uploads and downloads only the shards that changed and the two manifests. A machine without the release before it in its build folder, such as continuous integration, downloads the depots of the live build first. Use `steamcmd +login <account> +app_update <app id> validate +quit`, or the command `download_depot` of the Steam console, and name that folder with `--previous-release`, which finds the release and the shards of its packs in it.

The command `steamcmd +login <account> +run_app_build <steam>/scripts/app_build.vdf +quit` uploads the build. The Steamworks API of the app itself, such as achievements and the ownership of packs, is a native library that the app calls through `haylen.native`, as [native libraries](#native-libraries) describe.

### content

The command `content` builds, verifies, inspects, compares, publishes and compacts the protected releases of an app, and shows or rotates its keys. Its actions are `build`, `verify`, `inspect`, `diff`, `publish`, `compact` and `keys`, which the [content guide](../content/content.md#the-content-tool) describes with their options. Every action also takes `--engine-config`, the configuration of the content tool of the desktop artifacts, `Release` by default, and `--jobs`, the jobs of its build. The option `--platform` of an action takes the platforms of `run` and picks their content profile.

```sh
haylen content keys ~/apps/my-game
haylen content build ~/apps/my-game --platform android
```

## Engine artifacts

The command [`engine`](#engine) writes this layout:

```text
build/artifacts/
  manifest.json                 The engine version and, per platform, the configuration and the source hash of the last build.
  apple/Haylen.xcframework      Static library and headers for macOS, iOS, the iOS simulator, Mac Catalyst, tvOS and the tvOS simulator.
  apple/haylen-frameworks.json  The system frameworks that the engine needs on iOS, Mac Catalyst, tvOS and macOS, which haylen.py links into Apple projects.
  apple/PrivacyInfo.xcprivacy   The APIs with required reasons that the engine calls, which haylen.py merges into the privacy manifest of Apple apps.
  apple/HaylenBridgeAsync.swift The Swift helpers of the bridge and of plugin contexts, which haylen.py writes into the haylen/ folder of Apple projects.
  android/maven/                Maven repository with dev.haylen:haylen, the Android library with the player, and dev.haylen:haylen-plugins, haylen-links and haylen-coroutines.
  android/sdk/<abi>/            The SDK of the engine for each Android ABI, which links the release libraries of apps.
  web/player/                   haylen.js and haylen.wasm of the player, which draws through WebGPU or WebGL2, haylen-audio-worklet.js, the processor of its audio output, and haylen-content-store.js, the worker of its content store.
  web/sdk/                      The SDK of the engine for the web, which links the release runtime of each app.
  desktop/<os>-<arch>/haylen    The player of this machine.
  desktop/<os>-<arch>/haylen-content  The content tool of this machine, which builds, verifies, inspects and publishes protected releases.
  desktop/<os>-<arch>/sdk/      The SDK of the engine for this machine, which the Windows and Linux projects of apps link.
```

The build trees of the artifacts live in `build/engine/` for Apple, Android and the desktop. The web uses the regular build tree of `haylen build`, `build/web-<config>`.

### Haylen.xcframework

The framework holds one static library per slice, `libhaylen.a`, and the public headers. The library merges, with `libtool`, the engine, every dependency (Varn, Lua, Poco, OpenSSL, libuv, Box2D, ImGui, miniaudio and the rest), the runtime and the Lua player. It is built in these steps:

1. Every architecture is a CMake build of `engine/` with `HAYLEN_BUILD_SDK` and `HAYLEN_BUILD_FRAMEWORK`, in `build/engine/apple-<slice>-<arch>-<config>`.
2. The script `haylen-install.cmake` merges the engine and the runtime, as it does for the [SDK](embedding.md#find_package-and-the-sdk).
3. The script `haylen-framework.cmake` merges both with the player. It writes `haylen-frameworks.json` from the lists of system frameworks in `haylen-app.cmake`, the same lists that `haylen_link_runtime_platform` links into C++ apps.
4. The tool `lipo` joins the architectures of a slice, and `xcodebuild -create-xcframework` joins the slices.
5. haylen.py copies `haylen-frameworks.json`, the privacy manifest of the engine and its Swift helpers next to the framework.

| Slice | Architectures | Built with |
| --- | --- | --- |
| `macos` | arm64, x86_64 | The macOS SDK. |
| `ios` | arm64 | `CMAKE_SYSTEM_NAME=iOS` and the `iphoneos` SDK. |
| `ios-simulator` | arm64, x86_64 | `CMAKE_SYSTEM_NAME=iOS` and the `iphonesimulator` SDK. |
| `ios-maccatalyst` | arm64, x86_64 | `engine/cmake/haylen-catalyst.toolchain.cmake`, which compiles for the `macabi` flavor of iOS against the macOS SDK and its `iOSSupport` folder, because CMake has no system name for Mac Catalyst. The engine treats the build as iOS. |
| `tvos` | arm64 | `CMAKE_SYSTEM_NAME=tvOS` and the `appletvos` SDK. |
| `tvos-simulator` | arm64, x86_64 | `CMAKE_SYSTEM_NAME=tvOS` and the `appletvsimulator` SDK. |

The slices target the oldest systems that the code of the engine allows:

- iOS and tvOS 16.3, and Mac Catalyst 16.4, which is macOS 13.3 on the Mac. Those releases have a C++ library whose `std::format` formats floating point numbers, which the engine uses for text and logs. The toolchain itself reaches back to iOS and tvOS 15 and macOS 12, and a build for iOS 15 fails only on `std::format` with floating point.
- macOS 13.3, for the same `std::format` of its C++ library. SDL runs from macOS 10.13 on, and the Metal device from macOS 10.15 on, where Metal sorts GPUs into the families that it asks about, so neither sets the minimum.
- The Apple template sets `IPHONEOS_DEPLOYMENT_TARGET[sdk=macosx*]` to 16.4 for Mac Catalyst, because Xcode would otherwise map iOS 16.3 to macOS 13.2.
- The engine parses floating point numbers with fast_float, because `std::from_chars` for floating point needs iOS, tvOS and macOS 26.

The script `haylen.py` passes the versions as `CMAKE_OSX_DEPLOYMENT_TARGET`, also to the C++ apps it builds for macOS. The Apple template sets them in `project.yml`, and C++ apps built with `haylen_add_app` use the same ones.

The SDL platform layer enters the main callbacks of SDL itself, so the runtime never defines `main` on Apple platforms. The app calls `haylen_main` instead, which `haylen/platform/apple/HaylenMain.h` declares:

```objc
#import "haylen/platform/apple/HaylenMain.h"

int main(int argc, char* argv[]) {
    return haylen_main(argc, argv);
}
```

That `main` is also the place to register native handlers of the platform bridge with `[HaylenBridge registerHandler:handler:]` from `haylen/platform/apple/HaylenBridge.h`, before the runtime starts. Native plugins register theirs through their context, as the [plugin guide](../platform/plugins.md#the-apple-part) describes. Apple apps built with CMake through `haylen_add_app`, and the macOS desktop player, get the same `main` from `engine/src/platform/apple/AppleMain.cpp`.

### The Android libraries

The folder `engine/platform/android` is a Gradle project with four modules. The module `haylen` is the Android library (AAR) with the namespace `dev.haylen`. It holds:

- `HaylenActivity`, a `GameActivity` of the AndroidX games libraries and so an `AppCompatActivity`. It draws the app in a `SurfaceView` of an ordinary view hierarchy and installs the splash screen. It hides the system bars, apart from the status bar that the app shows, and reports the safe area, the on-screen keyboard and the orientation of the screen. It forwards low-memory warnings. It takes the back button through an `OnBackPressedCallback` while the app captures it, and hands its events to the plugins.
- `HaylenLinkActivity`, which hands the links and notifications of the app to the activity.
- `HaylenEditText`, the hidden field that edits the focused text field of the UI.
- `HaylenAudioFocus`, which holds the audio focus in the foreground, `HaylenNetwork`, which follows the network while the app holds the permission `ACCESS_NETWORK_STATE`, `HaylenBattery`, which follows the battery, and `HaylenAccessibility`, which follows the accessibility settings.
- `HaylenAccessibilityTree`, which serves the elements of the UI to TalkBack and the other accessibility services, and `HaylenDialogs` with the native dialogs.
- `HaylenBridge` for the platform bridge, and the host of the Android parts of [plugins](../platform/plugins.md#the-android-part): `HaylenPluginProvider`, which loads them when the process starts, `HaylenPlugin`, `HaylenPluginContext`, `HaylenRequirements`, `HaylenOverlay` with its `HaylenPlacement`, the screens of plugins with `HaylenScreens` and `HaylenScreen`, and the streams `HaylenVideoStream` and `HaylenAudioStream`.
- The splash themes, the R8 rules for the classes that C++ calls by name, and the Kotlin HTTP transport of Varn.

The manifest of `haylen` declares only the OpenGL ES 3 requirement, with no permission and no component. So the permissions, the back policy and the components that only some apps want never reach an app through it. The other modules each depend on `haylen` as an API:

- `haylen-plugins` holds only a manifest, which declares `HaylenPluginProvider` for the modules of plugins.
- `haylen-links` holds only a manifest, which declares the exported `HaylenLinkActivity` for plugins of links and notifications.
- `haylen-coroutines` holds `HaylenCoroutines` and `registerSuspend`, with `kotlinx-coroutines-android`, as the [plugin guide](../platform/plugins.md#engine-libraries) describes.

The library packages the player, which the CMake project of the engine builds with `HAYLEN_BUILD_PLAYER` as the shared library `libhaylen.so`. It covers arm64-v8a, armeabi-v7a, for the 32-bit Android TV devices still in use, and x86_64, for emulators, from Android 8.1 (API 27) on. The NDK r30 links the arm64-v8a and x86_64 libraries with 16 KB page alignment, which Android 15 devices with 16 KB pages need, and `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES` makes it explicit. 32-bit ARM has no 16 KB pages and keeps 4 KB alignment.

The Android library needs API 27 because miniaudio plays through AAudio from Android 8.1 on, as the [build guide](build.md#dependencies) explains. The others need less: GameActivity needs API 21, AppCompat and the activity library need API 23, and the HTTP transport of Varn needs API 24. `HaylenActivity` reaches the system bars through `WindowCompat` and `WindowInsetsControllerCompat` of `androidx.core`, and covers display cutouts from Android 9 on, where cutouts exist. Back reaches `HaylenActivity` only through the `OnBackPressedCallback` that it enables while the app takes back or edits a text field. Android 13 and later call it through the back callbacks of the window, which the manifest of the template turns on with `android:enableOnBackInvokedCallback`. So the system plays its predictive back animation when back leaves the app.

The command `haylen engine --platform android` builds the libraries in these steps:

1. It builds the player and the SDK of the engine for each ABI in `build/engine/android-<abi>-<config>`, one ABI after the other with the jobs of `--jobs`. So the compilers stay within those jobs, and the configures never write into the shared CPM sources at the same time.
2. It gathers the libraries in `build/engine/android-<config>/jniLibs`, and installs the SDK of each ABI into `build/artifacts/android/sdk/<abi>`, which links the release libraries of apps.
3. It runs Gradle with `-PhaylenNativeLibraries` for the libraries, `-PhaylenVarnSourceDir` for the folder where CPM placed Varn and `-PhaylenMavenDir` for the Maven repository. The task `copyVarnTransport` copies `VarnHttp.kt` of Varn into the module, because the engine links Varn into the player library and only the Kotlin HTTP transport of the Android library of Varn runs in the app.
4. It runs the task `publishReleasePublicationToArtifactsRepository` of every module. That publishes `dev.haylen:haylen`, `dev.haylen:haylen-plugins`, `dev.haylen:haylen-links` and `dev.haylen:haylen-coroutines` at the engine version, with their POMs, to `build/artifacts/android/maven`.

The POM of `haylen` lists the dependencies of the library, which Gradle brings into every app:

- The Kotlin standard library.
- `androidx.games:games-activity` 4.4.2, which the Gradle module metadata pins strictly. The Java classes of GameActivity must match its native side in `libhaylen.so`, which links the same version that `engine/cmake/haylen-dependencies.cmake` pins. A change of version updates the pin, the library and the template together.
- `androidx.appcompat:appcompat` 1.8.0, `androidx.activity:activity` 1.13.0 and `androidx.core:core` 1.19.1, as APIs, which `HaylenActivity` and its plugins build on.
- `androidx.core:core-splashscreen` and `androidx.window:window-java`, the Jetpack WindowManager that reports the fold of foldable devices.

The POMs of the other libraries list `haylen`. The POM of `haylen-coroutines` also lists `kotlinx-coroutines-android`, which apps and plugins with suspending handlers get as an API.

### Web and desktop

The web artifacts are the `haylen` player, which carries the devices of WebGPU and WebGL2 and draws through WebGPU, or through WebGL2 where WebGPU does not start. It sits next to `haylen-audio-worklet.js`, the AudioWorklet processor that the runtime loads from the folder of its script, and `haylen-content-store.js`, the worker of its content store. The SDK of the engine for the web, in `web/sdk/`, links the runtime of the release of each app. The player embeds no app. In development the page hands it the package at runtime, so the same `haylen.wasm` runs every app.

The desktop artifacts are three things, which one build tree of the engine builds, `build/engine/desktop-<config>/`:

- The `haylen` player of this machine, which runs any package that its command line names, as the [build guide](build.md#the-desktop-player) describes.
- The content tool `haylen-content`, which builds the protected releases of apps, as the [content guide](../content/content.md#the-content-tool) describes.
- The SDK of the engine for this machine in `sdk/`, which the Windows and Linux projects of apps link in their Debug and Release configurations.

## Templates

```text
templates/
  common/             What every kind of app shares: app.json, the logo, the .gitignore, .editorconfig and .clang-format of an app, and the platform projects that both kinds build.
    platform/
      android/        Gradle project that depends on the haylen library, with the Kotlin handler of the bridge example.
      web/app.js      Page code of the app, with the JavaScript handler of the bridge example.
  lua/                The Lua app: its starter code and the platform projects that only Lua apps build.
    source/main.lua   The starter app, which calls the bridge example.
    platform/
      apple/          XcodeGen project with targets for iOS and iPadOS with Mac Catalyst, tvOS and macOS, with the Swift handler of the bridge example.
      web/            Loading page with the logo, the progress bar, the choice of graphics API and the error screen.
      windows/        CMake project of the Windows executable with its icon, version, manifest, signing and Inno Setup installer.
      linux/          CMake project of the Linux executable with its desktop entry, icons, AppStream metainfo, .deb and AppImage.
  cpp/                The C++ app, a CMake project that compiles the engine.
    CMakeLists.txt    Adds the engine and builds the app with haylen_add_app.
    src/App.cpp       The starter app, which calls the bridge example and answers it on Windows and Linux.
    src/AppBridge.mm  The Objective-C handler of the bridge example on Apple platforms.
  plugin/             The starter plugin of haylen.py plugin new, with the .gitignore, .editorconfig and .clang-format of a plugin repository.
  plugin-features/    The features that plugin new --with adds to the starter plugin, with the parts they share.
```

The templates of apps are layers. The folder `common` holds what every kind of app shares, and `lua` and `cpp` hold what only their kind has. An app or a platform project takes the files of `common` first and then those of its kind. A file of the kind replaces the common file of the same path, and a common file that the kind lacks stays, so no file repeats between the layers. The command `new` copies both layers whole. The commands `platform add` and `platform diff`, and the copies that haylen.py keeps for apps without a project of a platform, take `platform/<template>/` of both layers. The platform templates of a kind are the folders under `platform/` of its layers.

The script `haylen.py` finds the platform templates by folder. A new platform is a folder under `platform/` of a layer and a run target in `haylen.py`. The table `RUN_TARGETS` names, for each `--platform`, the template of its project, the engine artifacts it needs, and the functions that prepare, build and launch it and check what the built app lacks. Apps follow the same shape with their folders `platform/<platform>/`.

### common

The file `app.json` and `content/logo.png`, the Haylen symbol on a transparent background, are the package that both kinds share. The logo is also the splash logo of the app. The Android project and the page code of the web are the platform files that both kinds build the same way.

Next to them are the files that the repository of an app keeps at its root. None of them is part of the package:

- The file `.gitignore` keeps out what an app never commits: the build outputs in `build/` and `dist/`, the CPM cache in `.cache/`, the folder `haylen/` of every platform project, the files of Finder and of the editors, the local settings and caches of Gradle, Kotlin and the Android build of C++ code, and the state of Xcode for each person.
- The file `.editorconfig` sets the style of text files.
- The file `.clang-format` holds the C, C++ and Objective-C style of the engine, for the code of the app, of its platform projects and of its plugins.

### lua

The file `source/main.lua` is a scene with the logo, the name of the app, a line with the engine version and platform, a button, a label that counts the presses, and a line with the answer of the [bridge example](#the-bridge-example). The button starts with the focus, so gamepads and TV remotes can press it. It runs on every platform.

### cpp

The folder of a C++ app is its CMake project and its package at once. That works because `haylen_add_app` ships only `app.json`, `source/`, `content/` and the manifests and Lua of the plugins that `app.json` lists. The command `run-cpp` brings the native parts of those plugins, as the [plugin guide](../platform/plugins.md#c-apps) describes.

Its `CMakeLists.txt` adds the engine folder that `HAYLEN_ENGINE_SOURCE` names, which `run-cpp` passes, or an installed SDK through `find_package` when the variable is empty, as the [embedding guide](embedding.md#an-app-written-in-c) shows. It builds the target `main` with the `CPP` option from `src/App.cpp`. It adds `src/AppBridge.mm` on Apple platforms, and links `platform/web/app.js` into the runtime script with `--pre-js` on the web. The target is not named `app`, because iOS and tvOS bundles keep the package in a folder of that name next to the executable. Its scene draws the logo, the name of the app and the answer of the [bridge example](#the-bridge-example) with the renderer of the engine, and the app runs no Lua.

Android builds through `platform/android` of the common layer, as [run-cpp](#run-cpp) describes, and `haylen_add_app` makes the apps of the other platforms itself. The command `steam` builds the Release of a C++ app with its CMake project for this machine and stages its depots, as it does for a Lua app.

### The bridge example

Both templates ship a working call of the [platform bridge](../platform/platform-bridge.md). The app calls the method `app.greet` with its kind, `Lua` or `C++`, as the name. The native code of the platform answers with a greeting that names its language. It sends the event `app.greeted` with the count of greetings so far, which the app shows next to the answer.

| Platform | Lua app | C++ app |
| --- | --- | --- |
| macOS, iOS, iPadOS, Mac Catalyst, tvOS | Swift, in `platform/apple/source/AppBridge.swift`, which the function `main` of `main.m` registers. | Objective-C, in `src/AppBridge.mm`, which registers in `+load`. |
| Android | Kotlin, in `onCreate` of `App` in `platform/android/app/src/main/kotlin/com/haylen/app/App.kt`. | The same Kotlin. |
| Web | JavaScript, in `Module.preRun` of `platform/web/app.js`. | The same JavaScript, linked with `--pre-js`. |
| Windows, Linux | No native handler, so the call fails with the code `noHandler` and the app shows its message. | C++, in `registerDesktopHandlers` of `src/App.cpp`, whose engine handler answers there. |

The Lua side is `Home:greet` of `source/main.lua`. It listens with `platform.on` and calls with `platform.call` inside `scene.spawn`, both owned by the scene. The C++ side is `HomeScene::enter` of `src/App.cpp`, which listens with `Bridge::on` and calls with `Bridge::call`. The desktop player of `haylen run` holds no native code of the app, so there the call fails with `noHandler` too.

To replace the example with real code:

1. Rename the method and the event in the app and in each handler, and change what they do.
2. Keep the files where they are, since the projects already build and register them.
3. Add more handlers next to them, with the APIs that the [platform bridge guide](../platform/platform-bridge.md) describes for each language.

An app that needs no native code deletes the handlers and the call.

### apple

```text
templates/lua/platform/apple/
  project.yml                     XcodeGen spec, which includes haylen/project.yml and gives each target its template from it.
  App.xcodeproj                   The project generated from project.yml with the haylen/ folder of an app without plugins or native libraries, always committed next to it.
  App.xcconfig                    Includes haylen/Haylen.xcconfig, and the settings after the include win.
  source/main.m                   Calls haylen_main, after the native bridge handlers of the app register.
  source/AppBridge.swift          The Swift handler of the bridge example.
  source/HaylenBridging.h         Makes HaylenBridge.h, HaylenPlugin.h and HaylenNotificationPlugin.h visible to the Swift files of the app and its plugins.
  ios/                            Info.plist, LaunchScreen.storyboard and Assets.xcassets with the app icon.
  tvos/                           Info.plist, LaunchScreen.storyboard and Assets.xcassets with the layered app icons and the top shelf images.
  macos/                          Info.plist and Assets.xcassets with the app icon.
```

The targets are `iOS`, for iPhone and iPad with `SUPPORTS_MACCATALYST` for the Mac Catalyst destination, `tvOS` and `macOS`. Each has a shared scheme of the same name and the target template of its platform, `HaylenIOS`, `HaylenTVOS` or `HaylenMacOS`. The template brings the engine, the package, the splash assets, the completed `Info.plist`, entitlements and privacy manifest, the native libraries and the native parts of the plugins, as [the Apple project](#the-apple-project) describes.

What stays in the template belongs to the developer: the deployment targets, C++20 and the Swift bridging header, the device families, Mac Catalyst and its deployment target of 16.4, the hardened runtime of macOS, the ad hoc signing of builds for this Mac and the app icons.

- The folder `source/` is one that Xcode keeps in sync, so every Objective-C, C++ and Swift file in it builds into every target.
- The module of the targets is `HaylenApp`, so Objective-C++ reaches Swift classes through `HaylenApp-Swift.h`.
- The targets read their product name and bundle identifier from `HAYLEN_PRODUCT_NAME` and `HAYLEN_BUNDLE_IDENTIFIER`, and their version from `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION`, which `haylen/Haylen.xcconfig` sets from `app.json`.
- Builds for this Mac, native or Mac Catalyst, sign with the ad hoc identity. Device builds use the team of `HAYLEN_APPLE_TEAM`.

The `Info.plist` of each platform is complete. It holds `$(HAYLEN_DISPLAY_NAME)`, `$(MARKETING_VERSION)` and the other build settings for the values of `app.json`. Its other keys are visible defaults that the developer changes or deletes:

- The scene manifest. It declares several scenes on iOS, so screens of plugins open windows of their own on Mac Catalyst and iPad while the app still draws in one window, and one scene on tvOS.
- `ITSAppUsesNonExemptEncryption`, the statement on export compliance that App Store Connect asks about.
- `GCSupportsControllerUserInteraction`, the hidden status bar of iOS, and the principal class and minimum system of macOS.
- `UIApplicationSupportsIndirectInputEvents`, through which UIKit hands the mouse and the trackpad of an iPad or a Mac to the app as a pointer with its buttons, which SDL reads as the mouse.

The orientations of iOS come from `app.json`, so the template leaves them out.

The asset catalogs carry the Haylen icons, the gradient symbol on the navy `#07112F` of the brand. The iOS icon is an opaque full square that the system masks. The macOS icons put the symbol on a navy rounded plate with a soft drop shadow, on the grid of macOS icons, with every size drawn on its own. The layered tvOS icons have an opaque navy back layer and the symbol on a transparent front layer. The top shelf images show the horizontal logo with its white wordmark on navy.

### android

```text
templates/common/platform/android/
  settings.gradle.kts         Reads haylen/haylen.properties: the engine repository, the plugin modules in haylen/plugins, the engine version of the modules and their build folder.
  build.gradle.kts            Android Gradle Plugin 9.4.1 for apps, libraries and asset packs, Kotlin 2.4.21 for the Kotlin sources, and the Gradle plugins of the plugins on the build classpath.
  gradle.properties           Gradle settings.
  gradle/wrapper/             Gradle 9.8.1 for Android Studio.
  app/build.gradle.kts        The app module, which depends on dev.haylen:haylen and on the plugin modules, applies the Gradle plugins of the plugins and has no C++.
  app/src/main/AndroidManifest.xml
  app/src/main/kotlin/com/haylen/app/App.kt  The application class, where the native bridge handlers of the app register before the first activity starts, with the handler of the bridge example.
  app/src/main/res/           Adaptive launcher icon with its monochrome layer and Android TV banner.
```

The app module depends on `dev.haylen:haylen` from the Maven repository of the artifacts, which brings GameActivity, AppCompat, the activity library and core. It reads the application id, the version name and code, the label, the screen orientation, the status bar, the native library and the manifest placeholders from `haylen/haylen.properties`. It adds `haylen/assets`, `haylen/res` and `haylen/jniLibs` to its source folders, as [the Android project](#the-android-project) describes. The version code comes from the version, so 1.2.3 becomes 1002003. The SDK levels and R8 are choices of the developer. The signing reads the keys of `keystore/`, as [android-key](#android-key) describes. The `minSdk` of 27 is what the engine library needs.

The manifest declares the permissions `INTERNET`, `ACCESS_NETWORK_STATE` and `VIBRATE` as visible defaults. Network access, the network events and `system.vibrate` need them, and an app deletes those it does not use, since the engine library declares no permission. A feature whose permission is missing logs once what is missing and does nothing or fails, as the [plugin guide](../platform/plugins.md#android-requirements) describes. The `<application>` sets `android:enableOnBackInvokedCallback`, which lets the back callback of `HaylenActivity` play the predictive back animation of the system.

The manifest declares `HaylenActivity` with:

- `singleTop`, so the launcher icon brings back every screen that shows over the app, such as a purchase.
- Every change of the configuration in `android:configChanges`, so a new language, font size or display keeps the app running instead of starting it again.
- The meta-data `android.app.lib_name`, set to the native library that GameActivity loads: `haylen`, the Lua player of the haylen library, for Lua apps, and the library of the app for C++ apps, whose APK leaves the Lua player out.
- The meta-data `dev.haylen.statusBar`, which launches the activity with the status bar of `app.json` until the engine applies it. Without it, the status bar starts hidden.
- The categories `LAUNCHER` and `LEANBACK_LAUNCHER` and the TV banner.

The manifest declares a touchscreen, the screen orientations, Android TV and a gamepad as optional features, so the same APK serves phones, tablets and Android TV. The links and notifications of the app go to `HaylenLinkActivity`, which the `haylen-links` library of the plugins that receive them declares with their intent filters. So an app without such plugins has no exported component besides its launcher activity. The command `run` starts the app with the launcher intent, whose task the launcher icon then brings back as it is.

The resources are vector drawables of the Haylen brand. The adaptive launcher icon has the gradient symbol inside the safe zone of its foreground, over `ic_launcher_background`, the navy `#07112F` of the brand. Its monochrome layer has the silhouette of the symbol, which launchers tint for themed icons. The Android TV banner shows the horizontal logo with its white wordmark on the same navy.

### web

```text
templates/lua/platform/web/
  index.html        The canvas and the splash with the logo, the progress bar and the status line.
  loader.css        Styles of the page and the splash.
  loader.js         Checks the browser, downloads with progress, loads the plugins and starts the runtime.
templates/common/platform/web/
  app.js            Page code of the app, where its bridge handlers register, with the handler of the bridge example.
```

The [web loader](#web-loader) explains what `loader.js` does.

### windows

```text
templates/lua/platform/windows/
  CMakeLists.txt          Includes haylen/haylen.cmake, builds the executable in the Windows subsystem, installs it with the package and the native libraries, signs release builds and describes the installer.
  signing.cmake           Signs a file with signtool and a time stamp, with the certificate that the environment names.
  resources/app.rc        The icon, the version resource and the application manifest of the executable.
  resources/app.manifest  Windows 10 and 11, the Common Controls of version 6, UTF-8 as the code page, per-monitor DPI awareness and no elevation.
  resources/app.ico       The Haylen icon in the sizes Windows shows, from 16 to 256 pixels.
```

The file `CMakeLists.txt` adds the target `app`, which `haylen_app(app)` of `haylen/haylen.cmake` makes the executable of the app. It names it after `HAYLEN_APP_EXECUTABLE` and builds it in the Windows subsystem, without a console.

The resource script `resources/app.rc` reads the values of `app.json` from `haylen/HaylenApp.h` for the version resource, which the properties of the file and the Task Manager show:

| Field | Value |
| --- | --- |
| `CompanyName` | `company` of `app.json`. |
| `FileDescription`, `ProductName` | `name`. |
| `FileVersion`, `ProductVersion` | `version`. |
| `LegalCopyright` | `copyright`. |
| `OriginalFilename` | The executable. |
| `Comments` | `description`. |

The resource script embeds the application manifest itself, so the linkers of MSVC and clang-cl take `/MANIFEST:NO` and add none of their own. The engine needs the Common Controls of version 6 and UTF-8 as the code page of the process, as [the Windows section](embedding.md#windows) of the embedding guide explains. The per-monitor DPI awareness matches the one that the Direct3D 11 runtime asks for. The window of the app and its taskbar button take the first icon of the executable, the one that Explorer shows. The icon is the gradient symbol on a navy plate with rounded corners, every size drawn from the full-size art.

The project installs the executable with the package in `app/` and the native libraries next to it. It signs the executable of a release build when the environment names a certificate, and describes the installer that [dist](#dist) makes with Inno Setup.

### linux

```text
templates/lua/platform/linux/
  CMakeLists.txt              Includes haylen/haylen.cmake, builds the executable, installs it with the desktop entry, the AppStream metainfo and the icons, and describes the .deb and the AppImage.
  resources/app.desktop       The keys of the desktop entry that belong to the developer, such as its categories.
  resources/app.metainfo.xml  The elements of the AppStream metainfo that belong to the developer, such as the licenses and the content rating.
  resources/icons/            The Haylen icon in the hicolor sizes from 16 to 512 pixels, and as SVG for every other size.
```

The file `CMakeLists.txt` adds the target `app`, which `haylen_app(app)` makes the executable of the app. It links the C++ library of the compiler statically, so the app runs on distributions older than the one that built it, as long as their C library is as new. It installs:

- The executable into `lib/<executable>/`, with the package in `app/` and the native libraries in `lib/`, which the `RUNPATH` `$ORIGIN:$ORIGIN/lib` of the executable names.
- A link `bin/<executable>` to the executable, so desktops start the app by name.
- The desktop entry into `share/applications/`, the metainfo into `share/metainfo/` and the icons into `share/icons/hicolor/`, all named after the identifier of `app.json`. The identifier is also the class of the window of the app, so desktops show the name and the icon of the entry for the window.

The executable cannot share the name of those folders. So `haylen.py` stops a Linux build of an app whose folder gives the executable the name `app` or `lib`, and asks for another folder name.

The script `haylen.py` completes `resources/app.desktop` and `resources/app.metainfo.xml` into `haylen/`, as it completes the `Info.plist` of Apple projects. What the developer gives wins, with a warning when it differs from `app.json`, and the values of `app.json` fill what is missing:

| File | Values of `app.json` |
| --- | --- |
| `app.desktop` | `Name` from `name`, `Comment` from `description`, `Exec` from the executable, and `Icon` and `StartupWMClass` from `identifier`. |
| `app.metainfo.xml` | `id` from `identifier`, `name`, `summary` and the first paragraph of `description` from `description`, `developer` with the name of `company` and the identifier without its last part as its id, the `launchable` desktop entry, and the `homepage` URL from `website`. |

The metainfo of the template declares the `CC0-1.0` license for the metadata itself, a proprietary license for the app and an empty content rating, which the developer changes. The desktop entry puts the app among games. The command `appstreamcli validate` checks the metainfo, which needs a `website` in `app.json` to pass, and `desktop-file-validate` checks the desktop entry. A metainfo that lists the releases of the app keeps them in `<releases>`, each with its version and date.

## Platform projects

The project of a platform belongs to the developer. It is `platform/<template>/` of the app when that folder exists, and haylen.py builds it where it is. An app without that folder uses a copy of the layers of the template for its kind, which haylen.py keeps in the build folder of the app, `build/apps/<app>-<hash>/<template>/`. The build folder takes the name of the app folder and the start of the SHA-256 hash of its absolute path. So apps in folders with the same name never share projects. haylen.py makes the copy again whenever the template changes. In a project of the developer, a file that the developer deletes stays deleted, and a newer template reaches the project only when the developer adopts it, which [`platform diff`](#platform) helps with.

The script `haylen.py` writes one folder inside a project, `haylen/`, which the `.gitignore` of the app ignores. It never edits `project.yml`, `project.pbxproj`, the Gradle scripts, the manifests, the CMake scripts, the `Info.plist` files or the entitlements of the developer. The files of the project reach what it writes through a few visible lines, which the developer may delete to take over. Build products go to the build folder of the app, outside the project: Xcode builds into `xcode/`, Gradle into `gradle/` with its project cache in `gradle-cache/`, and CMake into `cmake/<platform>-<config>/`.

The command [`prepare`](#prepare) writes the folder `haylen/`. The command [`run`](#run) prepares the project, generates `App.xcodeproj` again when it may, builds, [checks](#check) the built app and launches it.

### The Apple project

The file `project.yml` includes `haylen/project.yml`, and every target names its template in `templates`. XcodeGen appends the lists of a template to the lists of the target, and a setting of the target wins over the setting of the template. So the developer adds frameworks, sources and settings to a target, or points `INFOPLIST_FILE` at a file of their own and leaves the completed `Info.plist` behind. The file `App.xcconfig` starts with `#include "haylen/Haylen.xcconfig"`, and its settings after the include win, such as a build number from continuous integration in `CURRENT_PROJECT_VERSION`. The sources of each target leave its `Info.plist` out of the resources with `excludes` and list it with `buildPhase: none`, since the target builds the completed copy.

| File in `haylen/` | Contents |
| --- | --- |
| `project.yml` | The target templates `HaylenIOS`, `HaylenTVOS` and `HaylenMacOS`, and the Swift packages of the plugins. Each template links `Haylen.xcframework` and the system frameworks of the engine from the list that the artifacts publish, with IOKit for Mac Catalyst alone. It leaves out a framework that a target of the template links itself, since XcodeGen refuses a dependency listed twice. It copies the package and the privacy manifest, and on iOS and tvOS the splash assets. It sets `INFOPLIST_FILE` to the completed `Info.plist` and `CODE_SIGN_ENTITLEMENTS` to the completed entitlements when there are any. It adds the native parts of the plugins of its platforms, as the [plugin guide](../platform/plugins.md#apple-platforms) describes. The phase `Embed native libraries` joins the templates of the platforms that the app has native libraries for. It copies the libraries of the file list `haylen/native/<platform>-<sdk>.xcfilelist` into the folder `Frameworks` of the bundle and signs them with the identity of the app. Scripts run without the script sandbox in a template with any build phase, which would need every file of a bundle and the temporary files of `codesign` listed one by one. |
| `Haylen.xcconfig` | `HAYLEN_PRODUCT_NAME`, `HAYLEN_DISPLAY_NAME`, `HAYLEN_BUNDLE_IDENTIFIER`, `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION` from `app.json`, and `OTHER_LDFLAGS` with `HAYLEN_NATIVE_LDFLAGS`, which links static native libraries. |
| `ios/Info.plist`, `tvos/Info.plist`, `macos/Info.plist` | The `Info.plist` of the developer, completed. |
| `ios/App.entitlements`, `ios/Catalyst.entitlements`, `tvos/App.entitlements`, `macos/App.entitlements` | The entitlements of the developer at the same path, completed with the `entitlements` of the plugins of iOS, Mac Catalyst, tvOS and macOS, written when either has any. |
| `PrivacyInfo.xcprivacy` | The [privacy manifest](#privacy-manifest) of the app. |
| `Splash.xcassets` | The image `splash-logo` and the color `splash-background` of the [launch screens](#splash-screens). |
| `app/` | The package, which the bundle carries as `Resources/app`, or in the Release configuration the protected release of the app. |
| `HaylenBootstrap.cpp` | The bootstrap of the app, which every target template compiles. In the Release configuration the content tool writes it from the key folder of the app, with its sealed content keys. In the Debug configuration it holds only a check that stops a release build of the development package with the way to prepare the release. |
| `HaylenBridgeAsync.swift` | The [Swift helpers](../platform/plugins.md#swift-helpers) of the bridge and of plugin contexts, which every target template compiles, from the Apple artifacts. |
| `native/`, `HaylenNativeSymbols.mm` | The [native libraries](#native-libraries) of the run platform with the file lists of the embed phases, and the table of the symbols of static libraries. |
| `plugins/<id>/` | The Apple sources and resources of the plugins. |
| `Haylen.xcframework` | A link to the framework of the artifacts. |
| `state.json` | The hashes of the last generation of `App.xcodeproj`, and for a copy of the template, the hash of the template. |

The completed `Info.plist` and entitlements follow one rule: the value of the developer wins, and the keys of `app.json` and of the plugins only fill what is missing.

- Objects merge key by key, and arrays gain the items they lack, so two ad plugins share `SKAdNetworkItems`.
- A value of the developer that differs from the value of a plugin stays, with a warning, such as a usage description in the words of the company.
- Two plugins, or a plugin and `app.json`, that give one key different values stop the build with both named.
- The file `app.json` fills the orientations of iPhone and iPad, and the status bar that the app launches with, as `UIStatusBarHidden` and `UIStatusBarStyle` from `window.statusBar`. For an app whose `window.showInTaskbar` is `false`, it fills `LSUIElement` on macOS, so macOS never shows its Dock icon, not even while it starts.
- No key that haylen.py adds is a decision of the company, such as the statement on export compliance. That stays a default of the template, which the developer keeps or deletes.

### Generating App.xcodeproj

The developer changes the project in `project.yml` and generates `App.xcodeproj` again with `haylen xcodegen <app>`. That prepares `haylen/` and runs the XcodeGen that haylen.py pins. The command `run` generates it on its own when all of these hold:

- `project.yml` includes `haylen/project.yml`.
- The inputs changed since the last generation. The inputs are `project.yml`, `haylen/project.yml`, the XcodeGen version and the files of the plugin sources, such as after the app gained a plugin.
- `project.pbxproj` still has the hash of the last generation, or is an untouched copy of the template in a project without a record.

The file `haylen/state.json` keeps both hashes. A project whose inputs did not change builds as it is, edits made in Xcode included. Sometimes the inputs changed while `project.pbxproj` differs from the last generation, or haylen.py never generated it, such as in a fresh clone of the repository of an app. Then haylen.py generates the project once more in a scratch folder that links the entries of the project. When the result is the same file, haylen.py records it and goes on. Otherwise it stops without touching the project and says what to do:

```text
Error: The project platform/apple/App.xcodeproj changed since its last generation from "project.yml", or was never generated from it, so it stays as it is. Move the changes made in Xcode into "project.yml" and run "haylen xcodegen .", which generates the project again.
```

The command `run` never generates a `project.yml` without the include, and [check](#check) reports what its targets lack.

### The Android project

The Gradle scripts of the developer read `haylen/haylen.properties`:

| Key | Value |
| --- | --- |
| `repository`, `engineVersion` | The local Maven repository of the artifacts and the engine version. The file `settings.gradle.kts` gives every module the version as the extra property `haylenEngineVersion`, with which plugin modules depend on the engine libraries. |
| `name`, `identifier`, `versionName`, `versionCode`, `orientation`, `statusBar` | The identity, version, orientation and status bar of `app.json`. |
| `library` | The native library that `HaylenActivity` loads: `haylen`, the Lua player, for the debug builds of Lua apps, `haylen-app` for their release builds, or the library of a C++ app. The template stops a release build of a project whose library is `haylen`, since that project holds the development package. |
| `plugins`, `gradlePlugins`, `placeholder.<name>` | The plugin modules as `id=folder` entries, their Gradle plugins as `id=version` entries and their manifest placeholders, as the [plugin guide](../platform/plugins.md#android) describes. |
| `assetPacks` | The asset packs of a large release and of store packs, which `settings.gradle.kts` and the app module include, as [release builds](#release-builds) describe. |
| `buildDirectory` | The build folder of the app, where `settings.gradle.kts` sends the build outputs of every module. |

Next to it, haylen.py writes:

- `assets/app/`: the package with `haylen-package-index.json`, the list of its files, because Android cannot list asset folders recursively. In the Release configuration it holds the flat folder of the protected release instead.
- `assets/haylen-plugins.json`: the plugins with an Android part in [load order](../platform/plugins.md#load-order), with the values of their parameters.
- `res/`: the splash resources.
- `jniLibs/<abi>/`: the native libraries.
- `plugins/<id>/`: the library modules of the plugins.

haylen.py writes every value as ASCII with `\uXXXX` escapes, so any text survives. It builds with Gradle, whose outputs and project cache go to the build folder of the app. It installs the newest APK of the configuration, whatever product flavors the project defines.

### The web site

The files of `platform/web/` of the app, or of the layers of the template, go as they are into the site in the build folder of the app, `build/apps/<app>-<hash>/web/`. haylen.py writes next to them:

- `config.json`.
- The package: `app.zip` in development, or the protected release in `app/` with `--config Release`.
- The splash logo, and the web modules of the plugins in `plugins/`.
- The runtime, `haylen.js` and `haylen.wasm`, with the processor of its audio output and the worker of its content store.

Nothing is written into `platform/web/`, and a file that the developer deletes stays out of the site.

### Windows and Linux

The file `CMakeLists.txt` of `platform/windows` or `platform/linux` includes `haylen/haylen.cmake` before its targets, and gives its executable target `haylen_app(<target>)`.

| File in `haylen/` | Contents |
| --- | --- |
| `haylen.cmake` | The values of `app.json` as `HAYLEN_APP_NAME`, `HAYLEN_APP_IDENTIFIER`, `HAYLEN_APP_VERSION`, `HAYLEN_APP_DESCRIPTION`, `HAYLEN_APP_COMPANY`, `HAYLEN_APP_COPYRIGHT`, `HAYLEN_APP_WEBSITE` and `HAYLEN_APP_EXECUTABLE`. The folders `HAYLEN_PACKAGE_DIR` and `HAYLEN_NATIVE_DIR`, and on Linux `HAYLEN_DESKTOP_ENTRY` and `HAYLEN_METAINFO`. The SDK of the desktop artifacts through `find_package(haylen)`. The function `haylen_app(<target>)`, described below. |
| `app/` | The package, or in the Release configuration the protected release of the app. |
| `HaylenBootstrap.cpp` | The bootstrap of the app, which `haylen_app` compiles. In the Release configuration the content tool writes it from the key folder of the app. In the Debug configuration it holds only a check that stops a build where `NDEBUG` is defined, since the project then holds the development package. |
| `native/` | The [native libraries](#native-libraries) of the app for this machine. |
| `HaylenApp.h` | On Windows, the values of `app.json` as strings of the resource script and the version as four numbers, which `resources/app.rc` includes. |
| `app.desktop`, `app.metainfo.xml` | On Linux, the desktop entry and the AppStream metainfo of the project, completed with the values of `app.json`, as [linux](#linux) describes. |

The function `haylen_app(<target>)` gives the target:

- The Lua player of the engine, the bootstrap and `haylen::runtime`, with hidden symbols.
- On Windows, the C runtime of the SDK in every configuration.
- On Linux, the `RUNPATH` `$ORIGIN:$ORIGIN/lib`, with its symbols stripped in Release.
- Links to the package and the native libraries next to the executable of the build tree, so the build tree runs from an IDE as the installed app does.

The executable takes the name of the app folder in lowercase dash-case, such as `my-game` for `My Game`, which every package format accepts. The command `run` configures the project with the configuration of `--config` in `cmake/<platform>-<config>/` of the build folder of the app. It builds it, and installs it again into `installed/<platform>/` of the build folder, which is the app as it ships:

```text
build/apps/my-game-1a2b3c4d/installed/windows/
  my-game.exe             The executable of the app, with its icon, version resource and manifest.
  app/                    The package, or the manifests and shards of the protected release.
  my-glue.dll             The native libraries of the app.

build/apps/my-game-1a2b3c4d/installed/linux/
  bin/my-game             A link to the executable.
  lib/my-game/my-game     The executable of the app.
  lib/my-game/app/        The package, or the manifests and shards of the protected release.
  lib/my-game/lib/        The native libraries of the app.
  share/applications/     The desktop entry, named after the identifier.
  share/metainfo/         The AppStream metainfo.
  share/icons/hicolor/    The icons.
```

A project of the developer that replaces the copy of the template builds in a new tree, since a build tree belongs to one project. The project also builds without haylen.py once `prepare` wrote its folder `haylen/`, from an IDE or from a terminal:

```sh
haylen prepare ~/apps/my-game --platform linux --config Release
cmake -S ~/apps/my-game/platform/linux -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux
cmake --install build/linux --prefix dist/linux
```

## Release builds

The Release configuration of `run` and `prepare` builds what an app ships:

- The package, only as the protected release that the content tool builds into `build/apps/<app>-<hash>/release/<profile>/`, as the [content guide](../content/content.md) describes. The Lua modules become bytecode.
- A binary that compiles in the bootstrap of the app, `HaylenBootstrap.cpp`. The content tool writes it from the key folder of the app, and it seals the content keys, so no key sits in a file of the app.

A release reuses the shards of the release before it and the build cache of the app, so a build after a small change writes only the shards of what changed. The release before it is the last one of the build folder, or the one in the folder that `--previous-release` names. The Debug configuration ships the package as it is, for development.

```sh
haylen run ~/apps/my-game --platform linux --config Release
haylen prepare ~/apps/my-game --platform windows --config Release
haylen check ~/apps/my-game --platform linux --config Release
```

| Platform | Release build |
| --- | --- |
| Windows, Linux | Writes the release of the profile `windows` or `linux` into `haylen/app/` of the project, and the bootstrap into `haylen/HaylenBootstrap.cpp`. The function `haylen_app` compiles it into the executable that links the SDK of the desktop artifacts, with hidden symbols. Linux links it with `--gc-sections` and ships it stripped, with its symbols in a `.debug` file next to the build, which the executable names in its debug link. Windows links it with `/DEBUG /OPT:REF /OPT:ICF`, so the PDB of its public symbols stays next to the build. The project installs the release in `app/` next to the executable, and neither symbol file. |
| Android | Writes the release of the profile `android` into `haylen/assets/app/` of the Android project, a flat folder that Android lists without an index and whose shards the template keeps uncompressed in the APK. It links the library of the app, `libhaylen-app.so`, for every ABI from the SDK of the Android artifacts with the bootstrap. The library exports only the entry points of Java and GameActivity and ships stripped. Its symbols stay in `libhaylen-app.so.debug` of `build/apps/<app>-<hash>/release-library/build-<abi>/` for crash reports. The key `library` of `haylen/haylen.properties` names it, so the activity loads it and the Lua player of the `haylen` library stays out of the APK. The release build signs with the upload key of the app, which `android-key` creates, and the template shrinks the code and the resources of the APK with R8. |
| Web | Places the whole release of the profile `web` in `app/` of the site, where the shards of the content stay and stream by range. It links the runtime of the app with Emscripten, from the web SDK of the artifacts with the bootstrap, in `build/apps/<app>-<hash>/release-web/`. The site holds no `app.zip`, and `config.json` names the manifests and the shards of the app domain, which the page downloads before the app starts. The symbol map of the runtime, `haylen.js.symbols`, stays in its build folder for crash reports and never reaches the site. Every other script of the site goes through the terser of the pinned Emscripten: the classic scripts of the page and its workers keep their top-level names, and the web modules of the plugins minify as modules. |
| macOS, Mac Catalyst, iOS, tvOS | Writes the release of the profile `apple` into `haylen/app/` of the Apple project, which the bundle carries as `Resources/app`, and on macOS as `Contents/Resources/app`. It writes the bootstrap into `haylen/HaylenBootstrap.cpp`, which the targets compile into the app that links `Haylen.xcframework`. Every target of the project shares the release. The generated `Haylen.xcconfig` makes the Release configuration keep the debug information in a `.dSYM` next to the app, and strip the app as it builds, so the release that runs is the one that ships. `App.xcconfig` may override each setting. The engine and its dependencies compile with hidden symbols on Apple platforms, so the app exports only the symbols of its own code. |

After a release build, `run` inspects the built app and stops before it launches when the app holds what a release never may. The command `check --config Release` reports the same as missing requirements, so continuous integration fails on it. A release never holds:

- A `.lua` file, or the `haylen-package-index.json` of a development package.
- A file of the package as it is, under any name. The splash logo is the one exception, since the system shows it before the engine starts.
- Other files in the release folder than its manifests and shards.
- Symbol files, such as `.pdb`, `.dSYM` or `.debug`.
- Any content key or the signing key of the app, as bytes, as hexadecimal text in either case or as Base64 text.

### Store limits

Stores limit the size of an app. Those limits belong to the stores and never to the format, which serves releases of any size, and content beyond them goes on a content server.

- The App Store accepts iOS and tvOS apps of at most 4 GB uncompressed. An Apple release build warns when its release is larger.
- Google Play downloads a base module of at most 500 MB, which also carries the code and the native libraries of the app. On top of it come install-time asset packs of at most 1.5 GB each, all of them together at most 4 GB. A release build warns when its release is larger than the 4 GB.

So Android releases build shards that aim for 256 MiB. An Android release larger than 384 MiB moves its content shards into install-time asset packs, which `prepare` writes into `haylen/asset-packs/` of the project. Each pack holds at most 1 GiB, filled with the shards in the order of their names. The two manifests and the shards of the app domain, the code of the app, stay in the base module. A shard holds at most twice its target and one record. The content tool builds a shard of a previous release that passes that again, such as one of a release built for another profile, so every shard fits its pack.

The template includes the packs through the `assetPacks` entries of `haylen/haylen.properties` in `settings.gradle.kts` and the `assetPacks` of the app module. Google Play installs them with the app, and Android serves their files through the same assets as the base module. So the runtime reads the release from `app/`, whichever module holds a shard. The bundle of `:app:bundleRelease` carries them for Google Play. The command `run` builds `:app:packageReleaseUniversalApk`, the universal APK of the bundle with the asset packs, and installs it on the device. The check of a Release build stops when the built app lacks a shard of its release, such as in a project that leaves the asset packs out.

### Packs that stores deliver

The named packs of `content.delivery` become asset packs of Google Play when a plugin of the app delivers them. Such a plugin declares `assetPacks` in the `android` section of its `plugin.json`, such as the `play-asset-delivery` plugin of [haylen-org](https://github.com/haylen-org/play-asset-delivery):

- `prepare` writes an asset pack for every named pack into `haylen/asset-packs/`, with the shards of the pack at the root of its assets.
- The pack takes the name of the named pack, with every dash and dot as an underscore, and with `pack_` in front of a name that starts with a digit.
- Packs that the plugin lists in `fastFollow` deliver `fast-follow`, and the others `on-demand`.
- A pack larger than the 1.5 GB of a Play asset pack, two packs with the same Play name, or more than 100 asset packs in all stop the build.

Without such a plugin, the named packs stay for the content server. The bundle of `:app:bundleRelease` carries those packs. The command `run` installs it with bundletool in the local testing mode of Google Play, signed with the upload key of the project, so the library of Play delivers each pack from the device as the store would. The check of the Release build stops when the bundle lacks a shard of a pack.

On iOS and tvOS the named packs become asset packs of Background Assets, which the App Store hosts. That happens when a plugin of the app declares `assetPacks` in the `apple` section of its `plugin.json`, such as the `background-assets` plugin of [haylen-org](https://github.com/haylen-org/background-assets):

- `prepare` and `run` in the Release configuration pack each named pack with `xcrun ba-package` into `build/apps/<app>-<hash>/release/apple.asset-packs/<pack>.aar`, with the shards of the pack in a folder named after it.
- Packs that the plugin lists in `prefetch` download with the install and every update, and the others when the app asks for them.
- The developer uploads those archives to App Store Connect with the app.
- Only one plugin of the app may deliver packs from the App Store.

The plugin adds the downloader extension that Background Assets runs, the App Group of the app and its extension, and the keys of the `Info.plist` that hosting by Apple needs. The mock server of Xcode, `xcrun ba-serve`, serves the archives to a simulator or a device for testing, as the README of the plugin describes.

## Privacy manifest

Apps for Apple platforms declare the APIs with required reasons that their code calls. The engine is part of that code, since `Haylen.xcframework` is a static library. The file `engine/platform/apple/PrivacyInfo.xcprivacy` holds the declarations of the engine. The Apple artifacts carry it next to the framework, and C++ apps built with `haylen_add_app` copy it into their bundles.

The script `haylen.py` writes `haylen/PrivacyInfo.xcprivacy` from three sources: the manifest of the engine, the `privacy` keys of the Apple plugins, and `PrivacyInfo.xcprivacy` at the root of the Apple project of the developer, when it exists. The reasons of every API type join the reasons of that type. The tracking domains and the collected data types gain the items they lack. The app tracks when any of them says so.

The undefined symbols of `libhaylen.a` show which of those APIs the engine and its dependencies reach:

| Category | Reason | Callers |
| --- | --- | --- |
| File timestamp, `NSPrivacyAccessedAPICategoryFileTimestamp` | `C617.1`: the metadata of files inside the container of the app. | `stat`, `fstat` and `lstat` from the file layer of libuv, whose modification time Varn's `fs.stat` reports, and from Poco, OpenSSL, libsodium, libzip, pugixml, miniaudio and the log backend. |
| System boot time, `NSPrivacyAccessedAPICategorySystemBootTime` | `35F9.1`: time between events of the app and timers. | `mach_absolute_time` from the clock of SDL, the timers of libuv and the audio of miniaudio. |
| Disk space, `NSPrivacyAccessedAPICategoryDiskSpace` | `E174.1`: whether there is room to write files. | `statfs` from the file systems of libuv and Poco, which report the free space of a disk. |

The engine reads no user defaults, no active keyboards and no system boot time through `systemUptime`. An app that uses these APIs for other reasons adds them in its own `PrivacyInfo.xcprivacy`, such as the file timestamps of documents that the person picks with a dialog, `3B52.1`.

## Native libraries

The `native` section of `app.json` lists the native libraries that an app ships, by the name that `native.load` takes. Each one is prebuilt files for each platform, or a CMake project that haylen.py builds for each platform it lists. The [native code guide](../platform/native.md#packaging-libraries-with-an-app) describes the section. The `native` section of a plugin adds one more library, named after the plugin id, which haylen.py builds and places in the same way.

```json
{
    "native": {
        "steam_api": {"files": {"macos": "platform/apple/native/libsteam_api.dylib", "windows": "platform/windows/steam_api64.dll", "linux": "platform/linux/libsteam_api.so"}},
        "my-glue": {"cmake": "native", "platforms": ["macos", "ios", "android", "windows", "linux"]}
    }
}
```

The script `haylen.py` builds a CMake library in `native/<library>/` of the build folder of the app, once per architecture with the settings of the engine artifacts, and joins the architectures of Apple platforms with `lipo`. Then it places every library of the run platform:

| Platform | Place |
| --- | --- |
| macOS, Mac Catalyst | `haylen/native/<platform>-<sdk>/` of the Apple project, which the phase `Embed native libraries` copies into `Contents/Frameworks` and signs. An `.xcframework` gives the slice of the platform. |
| iOS, tvOS | The same, into `Frameworks`, where a dynamic CMake library becomes a framework with its own `Info.plist`. A static library links through `HAYLEN_NATIVE_LDFLAGS` in `haylen/Haylen.xcconfig`, and haylen.py writes `haylen/HaylenNativeSymbols.mm`, which keeps and registers the symbols that the section lists. |
| Android | `haylen/jniLibs/<abi>/` of the Android project, for arm64-v8a, armeabi-v7a and x86_64, built one ABI after the other with the NDK and `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES`. |
| Windows | `haylen/native/` of the Windows project, which the project installs next to the executable. |
| Linux | `haylen/native/` of the Linux project, which the project installs into `lib/` next to the executable, whose `RUNPATH` is `$ORIGIN:$ORIGIN/lib`. |
| Web | Nothing, because the browser loads no native libraries. |

An Apple project gets the libraries of the platform it was prepared for. So a build for another platform in Xcode runs `prepare` for that platform first.

## Splash screens

The object `splash` of `app.json` sets the launch screen of every platform:

```json
{
    "splash": {"logo": "ui/splash.png", "background": "#FF101418", "darkBackground": "#FF000000", "duration": 1.5, "fadeOut": 0.4}
}
```

| Key | Default | Meaning |
| --- | --- | --- |
| `logo` | the Haylen symbol | An image relative to `content/`. |
| `background` | `clearColor` | A color as `#RRGGBB` or `#AARRGGBB`. By default the launch screen blends into the first frame of the app. |
| `darkBackground` | `background` | The background while the system uses dark colors, which the asset catalog of Apple platforms, the night resources of Android and the media query `prefers-color-scheme` of the web page pick. |
| `duration` | `0` | The least time in seconds that the launch screen stays from the launch of the app. It always stays until the app drew its first frame. |
| `fadeOut` | `0.25` | The seconds the launch screen takes to fade out into the app. |

The runtime ends the launch screen after the first frame that the app drew once the `duration` passed since the runtime started. The platform then fades it out over `fadeOut` seconds. The project changes the rest:

- The `LaunchScreen.storyboard` of the iOS and tvOS targets of the Apple project, whose layout belongs to the developer.
- The theme `Theme.Haylen.Splash` that the manifest of the Android project names, which a theme of the project may replace.
- The page and `loader.css` of the web site.

The app icons live in the asset catalogs of the Apple project and the launcher resources of the Android project, which the developer replaces.

| Platform | Launch screen |
| --- | --- |
| iOS, iPadOS, Mac Catalyst | `ios/LaunchScreen.storyboard`: the image `splash-logo` centered in the safe area over the color `splash-background`, both from the asset catalog. Auto Layout keeps the logo square, at most 200 points and at most half of the safe area in each direction, so it fits every device, orientation and iPad window size. The system takes the launch screen away as soon as the window of the app shows. So `AppleSplash` covers the window with the same storyboard, which `UILaunchStoryboardName` of the `Info.plist` names, until the runtime ends the splash and fades it out. |
| tvOS | `tvos/LaunchScreen.storyboard` with the same layout and a logo of at most 360 points, kept over the window the same way. |
| Android | The SplashScreen API of `androidx.core:core-splashscreen`, with the theme `Theme.Haylen.Splash` of the manifest showing `haylen_splash_icon` over `haylen_splash_background`, then `Theme.Haylen`, whose window background has the same color. The system splash screen ends with the first frame of the activity window, while the Android runtime draws only once the activity resumed and its surface exists. So `HaylenSplash` covers the surface of the app from that first frame with a view of the same background and icon. It stays until the runtime ends the splash through `nativeFramePresented`, and then fades out over the seconds of `nativeFadeOut`. No black frame shows in between. The icon insets the logo so it fits the circle that Android masks it with, in every orientation, on phones, tablets and TVs. |
| Web | The page shows the logo over the background with the progress bar, until the runtime ends the splash through `Module.haylen.onSplashEnded`, which fades it out. |

The script `haylen.py` writes the splash into each project:

- In the Apple project, it writes the logo into the image set `splash-logo` of `haylen/Splash.xcassets`, and the background into its color set `splash-background`, with the dark background as its dark appearance. When the app names no logo, the image set holds `engine/platform/web/haylen-logo.svg`, the SVG of the Haylen symbol, with its vector data preserved. The launch screens of iOS and tvOS find both in the catalogs of their target.
- On Android, it writes the background into `haylen/res/values/haylen_splash.xml` and the dark background into `haylen/res/values-night/haylen_splash.xml`. It copies the logo to `haylen/res/drawable/haylen_splash_logo.<ext>`. These replace the defaults of the library, the Haylen symbol as a vector drawable over `#FF101418`. Android splash logos are PNG, WebP or JPEG images.
- On the web, it copies the logo, or the Haylen symbol, next to the files of the page as `splash.<ext>`, and writes both backgrounds into `config.json`.

## Development mode

Development mode is [hot reload](../core/lua.md#hot-reload): the running app reloads the files saved in its folder. The runtime turns it on only when its command line asks for it:

| Option | Meaning |
| --- | --- |
| `--dev` | Runs in development, watching the package folder. |
| `--dev-server <address>` | Runs in development and connects to the development server at that WebSocket address, which pushes the saved files. |
| `--native <folder>` | Adds a folder that `native.load` searches before the folders of the platform. It may repeat. |
| `--graphics-api <name>` | The graphics API that the window tries first, such as `vulkan`, before the other APIs of the build. It replaces the `window.graphicsApis` of `app.json`, and the web page passes the API that its address names the same way. A name that is not a graphics API stops the player with an error, and an API that the build lacks logs a warning. |
| The first argument that is not an option | The package to play, a folder or a zip. Without one, the runtime plays the package bundled with the app. |

Other options are ignored, because systems add their own, such as the ones that Xcode passes to the macOS apps it launches. Android starts apps without a command line. So a debuggable Android app takes the address of the development server from the extra `dev.haylen.developmentServer` of the intent that launched it, and an app that is not debuggable never reads it. A release build, which carries the bootstrap of its app, ignores `--dev` and `--dev-server` with a warning, since a protected release never runs in development.

Among the commands of `haylen.py`, only `run` turns development on:

- `run` without `--platform` passes `--dev`.
- `run --platform web` in the Debug configuration serves a page that turns on development through `Module.haylen.development`.
- `run` with any other platform in the Debug configuration starts a development server and launches the app connected to it.

So apps built from the templates and launched by hand or by Xcode and Android Studio, shipped apps, the sites that `prepare` makes and the Release configuration never run in development mode.

### The development server

The page of `run --platform web` connects to the development server that `haylen.py` runs on the same port as the site, at `/haylen/development`. It passes a random token of the run, which `config.json` hands to the page, and the server refuses a connection without it. The server works like this:

- It scans the package of the app twice a second with the rules of the player. It leaves out hidden files, backup files and files under `source/` that are not Lua, and waits for the next scan with files that are still being written.
- It compiles the shaders whose sources changed.
- It sends every batch of saved files to every open page as one JSON message, followed by the bytes of each file. A page reloads them in place, and the terminal prints what each page did, such as `The app reloaded "source/scenes/level.lua" in 4 ms.`
- A page that opens or reconnects later gets the files that changed since the package was made. A page of an earlier run of the server gets every file, so it always plays the files on disk.
- A file larger than 16 MiB stays out, with an error that names it. A shader that fails to compile prints its error in the terminal and on the pages, which keep the last good shader.

In the Debug configuration, `run` with any other platform starts the same development server once the app is built. It runs alone on the port of `--port`, 8000 by default, and the app launches with `--dev-server` and its address. So every file saved in the app reloads in it, as it does in the player and in the pages. The manifest of the server holds the files of the package that the build placed in the project, in `haylen/app` of the Apple project and `haylen/assets/app` of the Android project. So the app catches up on what changed while it was built. The server stops when the launch ends, such as when the app quits or Ctrl+C stops the streamed log.

| Platform | How the app reaches the server |
| --- | --- |
| `android` | `adb reverse` forwards the port of the device, emulator or phone to this machine, so the app connects to `127.0.0.1`. The address travels as the extra `dev.haylen.developmentServer` of the launch intent, and the forward goes away when the launch ends. |
| `ios-simulator`, `tvos-simulator`, `macos`, `catalyst`, `windows`, `linux` | The simulator and this machine share the loopback address of this machine. The address follows the arguments of `xcrun simctl launch` or of the executable. |
| `ios`, `tvos` | The device reaches this machine over the network. So the server listens on every address of this machine, or on the one that `--host` names, and the app connects to the address of this machine on the network, which `xcrun devicectl device process launch` passes. The device and this machine must share a network, and the first connection asks the person on the device for access to the local network. |

While the development server of `run` runs in an interactive terminal, with the web platform or any other, the terminal takes commands:

- The line `r` sends the files saved since the last batch at once, for a save that the watcher has not seen yet.
- The line `R` restarts every connected app from its package with a fresh Lua state, which the app reports back.
- Every other line goes to the connected apps as a line of their [developer console](../lua-api/debug.md#console), and the terminal prints the result of each app. So you run commands on a phone or a TV from the computer.

An app that stops or restarts leaves the server, which says so and keeps serving the apps that connect next.

## Web loader

The file `loader.js` runs when the page loads:

1. It creates `Module` with the canvas, so `app.js` can add page handlers to `Module.preRun`.
2. It reads `config.json`, which haylen.py writes. It holds the app name and identifier, whether `window.transparent` is set, the splash logo and backgrounds, the files of the protected release that download before the app starts, or `app.zip` in development, the size of each of them and of `haylen.wasm`, and the plugins with a web part.
3. It sets the title and the background, and downloads `splash.<ext>`, the logo of the app or the Haylen symbol, once for both the logo of the splash and the icon of the page. For a transparent app the page itself has no background, so whatever holds the page, such as the page of an editor that embeds it in a frame, shows through the transparent pixels of the canvas. Only the splash keeps the splash background.
4. It checks for WebAssembly and for WebGPU or WebGL2, and a browser with neither sees a message instead of a blank page. The runtime draws through WebGPU, or through WebGL2 where WebGPU does not start. Add `?graphicsApi=webGpu` or `?graphicsApi=webGl2` to the address to try one of them first, which the loader passes on as `Module.haylen.graphicsApi`. Those are the names that `graphics.apiName()` gives the two APIs.
5. It downloads `haylen.wasm` with the manifests and the app domain of the protected release, or with `app.zip` in development, behind one progress bar. Each download counts the bytes it streams against its `Content-Length`, or against the size in `config.json` when the length is missing or describes compressed bytes. Meanwhile it imports the web module of every plugin from `plugins/<id>/`.
6. It hands the WebAssembly bytes to the runtime through `Module.instantiateWasm`, which compiles them instead of downloading `haylen.wasm` again. It hands the release as `Module.haylen.release`, or the package as `Module.haylen.packageData` in development, then loads `haylen.js`. The runtime opens the [content store](../content/content.md#the-web) of the app before it starts, and the content of the release streams from `app/` as the app needs it. Every file of the page downloads once.
7. Before the app starts, a `Module.preRun` callback holds the start with a run dependency. Meanwhile it calls `load(context)` of every plugin in load order, with the context of `Module.haylen.createPluginContext`, as the [plugin guide](../platform/plugins.md#web) describes. A plugin that fails to import or to load replaces the progress bar with its error, and the app does not start.
8. `Module.haylen.onSplashEnded` fades the splash out once the runtime ends it, and `Module.haylen.onStarted` tells the page that the app runs. `Module.haylen.onError` writes every error to the console. An error that happens before the app starts replaces the progress bar with the message and the Lua stack trace. Later errors show on the error screen of the runtime.

When `config.json` holds the `development` entry, which only `run --platform web` writes, the loader sets `Module.haylen.development`, and the address of the development server with its token in `Module.haylen.developmentServer`. The [build guide](build.md#page-setup) lists every field that a page sets and the whole [runtime API](build.md#runtime-api), such as `Module.haylen.loadZip`, which replaces the running app later.

## Platform support

| Platform | Status | How it runs |
| --- | --- | --- |
| macOS | Supported | macOS 13.3 and later, the first release whose C++ library formats floating point numbers with `std::format`, as [Haylen.xcframework](#haylenxcframework) explains. The desktop player, or the `macOS` target of the Apple template with `--platform macos`. |
| Windows, Linux | Supported | Windows 10 version 1903 and later, the first that applies the UTF-8 code page of the application manifest, through which the engine reads its command line. Linux with X11, or with Wayland, where the window keeps the place and the level that the compositor gives it, and the glibc of the machine that built the app, or later. The desktop player, or the project of the [Windows or Linux template](#windows-and-linux) with `--platform windows` or `--platform linux`. CI builds and tests on those hosts. The application manifest of Windows apps declares Windows 10 and 11. Linux apps load GIO and GTK 3 at run time for the theme and the dialogs, and run without them, as [`haylen.system`](../lua-api/system.md#platforms) and [`haylen.dialogs`](../lua-api/dialogs.md#platforms) describe. |
| iOS, iPadOS | Supported | iOS and iPadOS 16.3 and later. The `iOS` target on iPhone and iPad, with every orientation of `app.json` and every iPad window size. |
| Mac Catalyst | Supported | macOS 13.3 and later. The `iOS` target on the Mac, whose window opens at the size of `app.json` in the points of the Mac, where macOS places it. SDL reads the buttons and the wheel of the mouse through `GCMouse` and the pointer position through a hover gesture on the view of the app, as on an iPad with a mouse or a trackpad, and the runtime reads the keyboard through `GCKeyboard`, as on iOS and tvOS. Typed text comes from the native field of the [text input](../ui/text-input.md) of the runtime. |
| tvOS | Supported | tvOS 16.3 and later. The `tvOS` target on Apple TV and its simulator, with the Siri Remote and game controllers. |
| visionOS | Runs the iPad app | The iOS target runs on Apple Vision Pro as a compatible iPad app. A native visionOS slice is possible now, since SDL runs on visionOS, but the engine does not build one. |
| watchOS | Not possible | The watchOS 27 SDK has no Metal, MetalKit, GameController or AudioToolbox, which the renderer, the input and the audio of the engine need. |
| Android | Supported | Phones, tablets and Android TV from one APK, on arm64-v8a, armeabi-v7a and x86_64, Android 8.1 (API 27) and later. |
| Web | Supported | Desktop and mobile browsers with WebGPU or WebGL2 and the exception handling of WebAssembly, which Chrome 95, Firefox 100 and Safari 15.2 introduced, since Lua unwinds its errors and yields through it. Sound needs `AudioWorklet`, which browsers offer only to pages served over https or from localhost, and apps on other pages run without sound. |
| Game consoles | Through a port | Xbox, Nintendo Switch and PlayStation need a [port](../platform/ports.md) that a developer with access to their SDK writes in a private repository, since those SDKs come under licenses that keep their code out of this repository. The repository holds the mechanism of ports and a test port that runs on a desktop, and no port of a console. |

Each platform draws through the graphics APIs of its build, in the order that the [RHI guide](../graphics/rhi.md#choosing-the-graphics-api) lists: Metal on Apple platforms, Direct3D 12, Direct3D 11, Vulkan and OpenGL on Windows, Vulkan and OpenGL on Linux, Vulkan and OpenGL ES on Android, and WebGPU and WebGL2 on the web. The same guide records where each device was [tested](../graphics/rhi.md#status-of-the-devices) and the limits of each API.
