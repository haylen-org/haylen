# Distributing apps

A Haylen app is a package: `app.json`, the Lua modules under `source/` and the assets under `content/`. The engine is compiled once into prebuilt artifacts for every platform, and ready-made platform projects in `templates/platform/` only wait for a package. `make.py` puts the two together: it assembles the template of a platform with the package of an app, writes the name, identifier, version, orientation and splash screen of `app.json` into the project, builds it and launches it. A Lua app never compiles the engine, and the same package runs on every platform.

This guide covers the commands, the engine artifacts, the templates, the way an app is assembled, platform overrides, native libraries, splash screens, the web loader, the local web server and the platforms Haylen supports. The [plugin guide](plugins.md) covers plugins, which add native capabilities to apps, the [build guide](build.md) covers building the engine itself and the [embedding guide](embedding.md) covers C++ projects that compile the engine through CMake.

## Quick start

```sh
python3 make.py new ~/apps/my-game --name "My Game" --identifier com.example.mygame
python3 make.py run ~/apps/my-game
python3 make.py run ~/apps/my-game --platform ios-simulator
python3 make.py run ~/apps/my-game --platform android
python3 make.py run ~/apps/my-game --platform web
```

`new` creates the app from the starter app and copies every platform template into its `platform/` folder. `run` without `--platform` starts the desktop player of this machine in development mode, which reloads edited scripts and assets. With `--platform` it builds the engine artifacts when they are missing or stale, assembles the platform project under `build/apps/`, builds it and launches it on a simulator, an emulator, a device, this Mac or a local web server, and streams the output of the app.

## Commands

| Command | Purpose |
| --- | --- |
| `engine [--platform apple\|android\|web\|desktop\|all] [--config]` | Builds the prebuilt engine artifacts into `build/artifacts/`. |
| `new <folder> [--name] [--identifier] [--orientation]` | Creates an app from `templates/app` with a copy of every platform template of `templates/platform` under `platform/`. |
| `run [app] [--platform] [--device] [--config] [--engine-config]` | Runs an app folder or a sample, in the desktop player by default or built for a platform. |
| `run-cpp <project> [--platform] [--target] [--device] [--config] [--engine-config]` | Builds a C++ project that compiles the engine through CMake and runs it on this machine, in the browser, on Mac Catalyst, iOS, tvOS, their simulators or Android. |
| `package <app> [-o app.zip]` | Zips `app.json`, `source/` and `content/` of an app. |
| `shaders <app> [--force]` | Compiles the shaders under `content/shaders/` of an app into `.shader` files. |
| `serve <folder> [--host] [--port] [--coep] [--coop] [--open]` | Serves a folder with the headers WebAssembly pages need. |
| `plugin add <folder\|repository> [--ref] [--app]` | Copies a plugin folder, or a plugin repository at a branch, tag or commit, into `plugins/` of an app and lists it in its `app.json`. |
| `plugin remove <id> [--app]` | Deletes a plugin from an app and from its `app.json`. |
| `plugin list [--app]` | Lists the plugins of an app with their status. |
| `plugin new <folder> [--id]` | Creates a plugin from `templates/plugin/`. |

`app` is an app folder or the path of a sample from `samples/`, so `python3 make.py run games/tiny-island` and `python3 make.py run samples/games/tiny-island` are the same. Without an app, `run` runs Tiny Island.

### engine

```sh
python3 make.py engine
python3 make.py engine --platform apple --config Release
```

Builds the artifacts of one platform, or of every platform with `--platform all`, which is the default. `--config` is `Release` by default. Each build records the configuration and a SHA-256 hash of the engine sources in `build/artifacts/manifest.json`, and `run` rebuilds the artifacts of a platform before it uses them whenever the manifest does not match the current sources or the configuration that `--engine-config` asks for. The hash covers every file under `engine/` except the tests, the benchmark and build outputs, so any change to the engine code, its CMake files, its shaders or its platform projects makes the artifacts stale.

### new

```sh
python3 make.py new ~/apps/space-race --orientation portrait
```

Copies `templates/app` into the folder, writes the name, identifier and orientation into its `app.json`, turns the window and design resolution upright for `portrait`, and copies every folder of `templates/platform`, today `apple`, `android` and `web`, into `platform/<platform>` of the app. The developer owns those copies: `run` lays them over the templates of the engine, so every change made to them wins. The name defaults to the folder name in title case, the identifier to `com.example.<folder>` and the orientation to `landscape`. The folder must not exist or must be empty.

### run

```sh
python3 make.py run samples/games/tiny-island
python3 make.py run games/tiny-island --platform macos
python3 make.py run games/tiny-island --platform catalyst
python3 make.py run games/tiny-island --platform ios-simulator --device "iPad Pro 13-inch (M5)"
python3 make.py run games/tiny-island --platform tvos-simulator
python3 make.py run games/tiny-island --platform android --device emulator-5554
python3 make.py run games/tiny-island --platform web --coep off --open
```

Before anything else, `run` compiles the shaders of the app whose sources changed, as [shaders](#shaders) describes. Without `--platform`, `run` builds the `haylen` player in the build tree of this machine (`build/<host>-<config>`), builds or copies the [native libraries](#native-libraries) of the app for this machine into `native/development` of the [build folder of the app](#assembling-an-app), and starts the player with `--dev`, `--native` and that folder when there are libraries, and the app folder. While the player runs it compiles every shader source that changes again, which the player then reloads. With `--platform` it assembles the app as the next sections describe and then:

| Platform | Build | Launch |
| --- | --- | --- |
| `macos` | `xcodebuild` of the `macOS` scheme of `App.xcodeproj`. | Starts the executable of the app bundle, whose log the engine writes to the standard output, so it stays in the terminal. |
| `catalyst` | `xcodebuild` of the `iOS` scheme for the Mac Catalyst destination. | Starts the executable of the Mac Catalyst bundle and streams its log with `log stream`. |
| `ios-simulator`, `tvos-simulator` | `xcodebuild` of the `iOS` or `tvOS` scheme for the simulator. | `xcrun simctl boot`, `install` and `launch --console-pty`, on the simulator named by `--device` (a name or an id), on a booted one, or on the first iPhone or Apple TV available, and streams the log of the app with `log stream` inside the simulator. |
| `ios`, `tvos` | `xcodebuild` for the device, signed with the team in the `HAYLEN_APPLE_TEAM` environment variable. | `xcrun devicectl device install app` and `device process launch --console` on the device id given with `--device`. |
| `android` | Gradle `:app:assembleDebug`, or `assembleRelease` with `--config Release`. | `adb install -r`, `adb shell am start` and the log of the app process, on the device or emulator serial given with `--device`, which may be left out when only one is connected. |
| `web` | Copies the prebuilt runtime and writes `app.zip` and `config.json`. | Serves the site on `--port` (8000 by default) with the `--coep` and `--coop` policies, and opens it with `--open`. |
| `windows`, `linux` | Copies the desktop player artifact next to the package, the `platform/windows` or `platform/linux` folder of the app over it, and the native libraries of the app next to the player on Windows and into `lib/` on Linux. | Runs the player, named after the app, which plays the `app` folder next to it. |

`--config` is the configuration of the platform project (`Debug` by default), and `--engine-config` the configuration of the engine artifacts it links (`Release` by default). `xcodebuild` and Gradle build with the jobs of `--jobs`. `run` streams the output of the app until it exits or Ctrl+C stops it. The engine logs to the standard output on desktops, to the log of the process on Android, which `adb logcat --pid` streams, and to the unified log on iOS, tvOS and Mac Catalyst, under the `dev.varn.engine` subsystem. On Mac Catalyst and the simulators `run` streams those lines next to the standard output and error of the process, with warnings and errors on stderr, and keeps streaming for a second after the app ends so its last lines arrive. The log of an app on a device shows in Console.app, while `run` shows the standard output and error of its process.

### run-cpp

```sh
python3 make.py run-cpp cpp/embedding
python3 make.py run-cpp samples/cpp/embedding --platform web
python3 make.py run-cpp cpp/embedding --platform ios-simulator --device "iPhone 17"
python3 make.py run-cpp cpp/embedding --platform catalyst
python3 make.py run-cpp cpp/embedding --platform android --device emulator-5554
```

Configures a CMake project that calls `haylen_add_app`, such as `samples/cpp/embedding`, in `build/cpp/<project>-<hash>/<platform>-<config>`, builds its app target and runs it. `<hash>` is the start of the SHA-256 hash of the absolute project folder, so projects in folders with the same name never share a build tree. The target is named after the project folder unless `--target` names another one. C++ projects compile the engine from source, so only Android uses an engine artifact, for its Java classes.

| Platform | Build | Launch |
| --- | --- | --- |
| This machine | Ninja, or the default generator on Windows without Ninja. | Runs the executable. |
| `web` | Emscripten for WebGPU and for WebGL2 into `build/cpp/<project>-<hash>/web`, with an `index.html` that picks the backend the browser supports. | Serves that folder like `serve`. |
| `ios-simulator`, `tvos-simulator` | The Xcode generator with the iOS or tvOS simulator SDK for the architecture of this Mac, which compiles the launch screen and signs the bundle. | Like `run`, on the simulator named by `--device`, a booted one or the first one available. |
| `ios`, `tvos` | The Xcode generator with the device SDK, signed with the team in `HAYLEN_APPLE_TEAM`. | Like `run`, on the device named by `--device`. |
| `catalyst` | Ninja with `engine/cmake/haylen-catalyst.toolchain.cmake` for the architecture of this Mac. `haylen_add_app` signs the bundle ad hoc after linking, as Xcode does, because macOS opens the window of an unsigned Mac Catalyst app at the top left of the screen instead of where the app asks. | Like `run`: starts the executable and streams its log. |
| `android` | Ninja with the NDK for the ABI of the device named by `--device`, or of the only one connected, into `bin/<target>/lib<target>.so`. Then it assembles the Android template with the package that `bin/<target>/package.txt` names, the library in `jniLibs/<abi>` and `haylen.library` set to the target, which makes `HaylenActivity` load the library of the app and leaves the Lua player of the haylen library out of the APK. The haylen library comes from the artifacts, built with `--engine-config` when they are missing or stale. | Like `run`. |

### package

```sh
python3 make.py package games/tiny-island -o build/tiny-island.zip
```

Compiles the shaders of the app whose sources changed, then zips `app.json`, `source/` and `content/` of the app, with `plugin.json` and `source/` of every plugin that `app.json` lists, and nothing else in its folder, into `app.zip` or the file `-o` names. The desktop player runs the zip with `haylen app.zip`, and a web page hands it to the runtime as described in [the web loader](#web-loader).

### shaders

```sh
python3 make.py shaders tiny-island
python3 make.py shaders ~/apps/my-game --force
```

Compiles every source under `content/shaders/` that declares an `@program` into a `.shader` file next to it, for every backend the engine runs on, with `sokol-shdc` from `.tools/`. Sources without a program are files the others include. A shader is compiled again when its `.shader` file is older than any source of the app or the shader library, and `--force` compiles them all. A compile error names the file, the line and the program, and fails the command. The [shader guide](shaders.md) explains the sources and the files. The web editor cannot compile shaders, so apps ship their `.shader` files in their package.

### serve

```sh
python3 make.py serve dist/my-game --port 8000
python3 make.py serve dist/my-game --coep off --open
python3 make.py serve dist/my-game --host 192.168.1.20
```

Serves a folder at `http://127.0.0.1:<port>/` with a threading Python server. `--host` listens on another address of this machine instead, such as its LAN address, so phones and other computers open the page. Browsers offer `AudioWorklet` only to pages served over https or from localhost, so those pages run without sound. `run --platform web` and `run-cpp --platform web` take the same options. The server sends:

| Header | Value |
| --- | --- |
| `Cross-Origin-Opener-Policy` | None by default, `same-origin-allow-popups` with `--coop same-origin-allow-popups`, and `same-origin` with `--coop same-origin` |
| `Cross-Origin-Embedder-Policy` | `require-corp` by default, `credentialless` with `--coep credentialless`, and none with `--coep off` |
| `Cross-Origin-Resource-Policy` | `cross-origin` |
| `Access-Control-Allow-Origin` | `*` |
| `Cache-Control` | `no-cache` |

The web runtime is single-threaded, so its pages need no cross-origin isolation, and the server sends no opener policy by default: `same-origin` would put the sign-in and payment popups of plugins into a browsing context group of their own, which cuts them off from the page, as the [plugin guide](plugins.md#web-screens) explains. `--coop same-origin` together with a `--coep` policy makes a page cross-origin isolated, for pages that use `SharedArrayBuffer` or threads of their own, and `--coop same-origin-allow-popups` keeps the popups of the page linked to it. Pages that load third-party scripts, such as the Google sign-in library of Tiny Island, need `--coep credentialless` or `--coep off`, because those scripts do not send the resource policy that `require-corp` asks for. The server names the MIME type of `.wasm` (`application/wasm`), `.js` and `.mjs` (`text/javascript`), `.json`, `.zip`, `.html`, `.css`, `.svg` and `.png` explicitly. A request for a file that also exists as `<file>.br` or `<file>.gz` receives the compressed copy with its `Content-Encoding` when the browser accepts that encoding.

### plugin

```sh
python3 make.py plugin list
python3 make.py plugin add admob --app ~/apps/my-game
python3 make.py plugin list --app ~/apps/my-game
python3 make.py plugin remove admob --app ~/apps/my-game
python3 make.py plugin new ~/plugins/my-plugin
```

`plugin add` copies a plugin folder, or the root of a plugin repository at the branch, tag or commit that `--ref` names, into `plugins/<id>/` of the app and lists it in the `plugins` section of `app.json` with the defaults of its parameters and an empty text for each required one. `plugin remove` deletes both, `plugin list` lists the plugins of an app with the problems that keep them from building, and `plugin new` creates a plugin from `templates/plugin/`. `--app` defaults to the current folder. The [plugin guide](plugins.md) describes the commands, the `plugins` section of `app.json`, the checks that `run` makes before it builds and the plugin format.

## Engine artifacts

`make.py engine` writes this layout:

```text
build/artifacts/
  manifest.json                 Engine version and, per platform, the configuration and the source hash of the last build.
  apple/Haylen.xcframework      Static library and headers for macOS, iOS, the iOS simulator, Mac Catalyst, tvOS and the tvOS simulator.
  android/maven/                Maven repository with dev.haylen:haylen, the Android library with the player, and dev.haylen:haylen-plugins, haylen-links and haylen-coroutines.
  web/webgpu/                   haylen.js and haylen.wasm of the player for WebGPU, and haylen-audio-worklet.js, the processor of its audio output.
  web/webgl2/                   The same files for WebGL2.
  desktop/<os>-<arch>/haylen    The player of this machine.
```

The intermediate build trees live in `build/engine/` for Apple and Android, and in the regular build trees of `make.py build` (`build/web-<config>`, `build/web-webgl2-<config>` and `build/<host>-<config>`) for the web and the desktop.

### Haylen.xcframework

The framework holds one static library per slice, `libhaylen.a`, with the engine, every dependency (Varn, Lua, Poco, OpenSSL, libuv, Box2D, ImGui, miniaudio and the rest), the runtime and the Lua player merged by `libtool`, and the public headers. Every architecture is a CMake build of `engine/` with `HAYLEN_BUILD_SDK` and `HAYLEN_BUILD_FRAMEWORK` in `build/engine/apple-<slice>-<arch>-<config>`: `haylen-install.cmake` merges the engine and the runtime as it does for the [SDK](embedding.md), and `haylen-framework.cmake` merges both with the player. `lipo` joins the architectures of a slice and `xcodebuild -create-xcframework` joins the slices.

| Slice | Architectures | Built with |
| --- | --- | --- |
| `macos` | arm64, x86_64 | The macOS SDK. |
| `ios` | arm64 | `CMAKE_SYSTEM_NAME=iOS` and the `iphoneos` SDK. |
| `ios-simulator` | arm64, x86_64 | `CMAKE_SYSTEM_NAME=iOS` and the `iphonesimulator` SDK. |
| `ios-maccatalyst` | arm64, x86_64 | `engine/cmake/haylen-catalyst.toolchain.cmake`, which compiles for the `macabi` flavor of iOS against the macOS SDK and its `iOSSupport` folder, because CMake has no system name for Mac Catalyst. The engine treats the build as iOS. |
| `tvos` | arm64 | `CMAKE_SYSTEM_NAME=tvOS` and the `appletvos` SDK. |
| `tvos-simulator` | arm64, x86_64 | `CMAKE_SYSTEM_NAME=tvOS` and the `appletvsimulator` SDK. |

The slices target the oldest systems the engine code allows: iOS and tvOS 16.3, macOS 13.3 and Mac Catalyst 16.4, which is macOS 13.3 on the Mac, the releases whose C++ library formats floating point numbers with `std::format`, which the engine uses for text and logs. The Apple template sets `IPHONEOS_DEPLOYMENT_TARGET[sdk=macosx*]` to 16.4 for Mac Catalyst, because Xcode would otherwise map iOS 16.3 to macOS 13.2. The toolchain itself reaches back to iOS and tvOS 15 and macOS 12, and a build for iOS 15 fails only on `std::format` with floating point. The engine parses floating point numbers with fast_float, because `std::from_chars` for floating point needs iOS, tvOS and macOS 26. `make.py` passes the versions as `CMAKE_OSX_DEPLOYMENT_TARGET`, the Apple template sets them in `project.yml`, and C++ apps built with `haylen_add_app` use the same ones.

On Apple platforms `sokol_app` is compiled with `SOKOL_NO_ENTRY`, so the runtime never defines `main`. The app calls `haylen_main` instead, declared in `haylen/platform/apple/HaylenMain.h`:

```objc
#import "haylen/platform/apple/HaylenMain.h"

int main(int argc, char* argv[]) {
    return haylen_main(argc, argv);
}
```

That `main` is also the place to register native platform bridge handlers with `[HaylenBridge registerHandler:handler:]` from `haylen/platform/apple/HaylenBridge.h`, before the runtime starts, while native plugins register theirs through their context, as the [plugin guide](plugins.md#the-apple-part) describes. Apple apps built with CMake through `haylen_add_app`, and the macOS desktop player, get the same `main` from `engine/src/platform/apple/AppleMain.cpp`.

### The Android libraries

`engine/platform/android` is a Gradle project with four modules. The module `haylen` is the Android library (AAR) with the namespace `dev.haylen`. It holds `HaylenActivity`, a `GameActivity` of the AndroidX games libraries and so an `AppCompatActivity`, which draws the app in a `SurfaceView` of an ordinary view hierarchy, installs the splash screen, hides the system bars, reports the safe area, the on-screen keyboard and the screen orientation, forwards low-memory warnings, takes the back button through an `OnBackPressedCallback` while the app captures it and hands its events to the plugins, `HaylenLinkActivity`, which hands the links and notifications of the app to it, `HaylenEditText`, the hidden field that edits the focused text field of the UI, `HaylenAudioFocus`, which holds the audio focus in the foreground, `HaylenNetwork`, which follows the network while the app holds the permission `ACCESS_NETWORK_STATE`, `HaylenBridge` for the platform bridge, the host of the Android parts of [plugins](plugins.md#the-android-part) with `HaylenPluginProvider`, which loads them when the process starts, `HaylenPlugin`, `HaylenPluginContext`, `HaylenRequirements` and `HaylenOverlay` with its `HaylenPlacement`, the splash themes and the Kotlin HTTP transport of Varn, which Gradle copies from the Varn sources. Its manifest declares only the OpenGL ES 3 requirement, so the permissions, the back policy and the components that only some apps want never reach an app through it: the modules `haylen-plugins` and `haylen-links` declare `HaylenPluginProvider` and the exported `HaylenLinkActivity` in manifests of their own, which reach the apps whose plugins depend on them, and `haylen-coroutines` holds `HaylenCoroutines` and `registerSuspend` with `kotlinx-coroutines-android`, as the [plugin guide](plugins.md#engine-libraries) describes. It packages the player, which the engine CMake project builds with `HAYLEN_BUILD_PLAYER` as the shared library `libhaylen.so`, for arm64-v8a, armeabi-v7a (the 32-bit Android TV devices still in use) and x86_64 (emulators), from Android 8.1 (API 27) on. The NDK r30 links the arm64-v8a and x86_64 libraries with 16 KB page alignment, which Android 15 devices with 16 KB pages need, and `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES` makes it explicit. 32-bit ARM has no 16 KB pages and keeps 4 KB alignment.

`make.py engine --platform android` builds the player of each ABI in `build/engine/android-<abi>-<config>`, one ABI after the other with the jobs of `--jobs`, so the compilers stay within them and the configures never write into the shared CPM sources at the same time, and gathers the libraries in `build/engine/android-<config>/jniLibs`. It passes them to Gradle with the folder where CPM placed Varn, then runs the `publishReleasePublicationToArtifactsRepository` task of every module, which publishes `dev.haylen:haylen`, `dev.haylen:haylen-plugins`, `dev.haylen:haylen-links` and `dev.haylen:haylen-coroutines` at the engine version with their POMs to `build/artifacts/android/maven`. The POM of `haylen` lists the dependencies of the library, which Gradle brings into every app: the Kotlin standard library, `androidx.games:games-activity` 4.4.2, which the Gradle module metadata pins strictly because the player links the native side of the same version, `androidx.appcompat:appcompat`, `androidx.activity:activity` and `androidx.core:core`, which `HaylenActivity` and its plugins build on, and `androidx.core:core-splashscreen`. The POMs of the other libraries list `haylen`, and the one of `haylen-coroutines` also `kotlinx-coroutines-android`, which apps and plugins with suspending handlers get as an API.

### Web and desktop

The web artifacts are the `haylen` player built for WebGPU and for WebGL2, each next to `haylen-audio-worklet.js`, the AudioWorklet processor that the runtime loads from the folder of its script. It embeds no app: the page hands it the package at runtime, so the same `haylen.wasm` runs every app and only `app.zip` changes from one app to another. The desktop artifact is the `haylen` player of this machine, which Windows and Linux apps ship next to their package.

## Templates

```text
templates/
  app/                The starter app package of make.py new.
  plugin/             The starter plugin of make.py plugin new.
  platform/
    apple/            XcodeGen project with targets for iOS and iPadOS with Mac Catalyst, tvOS and macOS.
    android/          Gradle project that depends on the haylen library.
    web/              Loading page with the logo, the progress bar, the backend choice and the error screen.
```

make.py finds the platform templates by folder. A new platform is a folder under `templates/platform/` and a run target in `make.py`, the `RUN_TARGETS` table that names, for each `--platform`, the template it assembles, the engine artifacts it needs and the function that builds and launches it. Apps mirror the same shape with their `platform/<platform>/` folders.

### app

`app.json`, `source/main.lua` and `content/logo.png`, the Haylen symbol on a transparent background, which is also the splash logo of the app: a scene with the logo, the name of the app, a line with the engine version and platform, a button and a label that counts the presses. The button starts with the focus, so gamepads and TV remotes can press it. It runs on every platform.

### apple

```text
templates/platform/apple/
  project.yml                 XcodeGen spec. Run .tools/xcodegen/bin/xcodegen generate in the folder after changing it.
  plugins.json                The XcodeGen include of project.yml that make.py writes for the plugins of an app, empty in the template.
  App.xcodeproj               The project generated from project.yml, always committed next to it.
  App.xcconfig                Written by make.py: product name, bundle identifier, version, build number, the link settings of static native libraries and the entitlements files of plugins.
  source/main.mm              Calls haylen_main.
  source/HaylenBridgeAsync.swift  Registers Swift handlers of the bridge and of plugin contexts written as async functions with Codable parameters and results, and sends events with Encodable payloads.
  source/HaylenBridging.h     Makes HaylenBridge.h and HaylenPlugin.h visible to the Swift files of the app and its plugins.
  ios/                        Info.plist written by make.py, LaunchScreen.storyboard and Assets.xcassets with the app icon and the splash assets.
  tvos/                       Info.plist, LaunchScreen.storyboard and Assets.xcassets with the layered app icons, the top shelf images and the splash assets.
  macos/                      Info.plist and Assets.xcassets with the app icon.
```

The targets are `iOS` (iPhone and iPad, with `SUPPORTS_MACCATALYST` for the Mac Catalyst destination), `tvOS` and `macOS`, each with a shared scheme of the same name. They link `Haylen.xcframework` and the system frameworks the engine needs, including `Network.framework` for the network events and `UserNotifications.framework` for the notification center delegate that the runtime owns for plugins, and copy the `app` folder to `Resources/app` as a folder reference. `source/` is a folder that Xcode keeps in sync, so every Objective-C, C++ and Swift file in it builds into every target, and the module of the targets is `HaylenApp`, so Objective-C++ reaches Swift classes through `HaylenApp-Swift.h`. The last phase of each target, `Embed native libraries`, copies the libraries that the file list `native/<target>-<platform>.xcfilelist` names into the `Frameworks` folder of the bundle and signs them with the identity of the app. It runs without the script sandbox, which would need every file of a bundle and the temporary files of `codesign` listed one by one. The targets read their product name and bundle identifier from `HAYLEN_PRODUCT_NAME` and `HAYLEN_BUNDLE_IDENTIFIER` of `App.xcconfig`, and their version from `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION`, so only plugins change the project per app: when the plugins of an app add sources, Swift packages, system frameworks, resources or build scripts, make.py writes them into `plugins.json` and generates the project again with the XcodeGen it pins, 2.46.0, as the [plugin guide](plugins.md#apple-platforms) describes. The Info.plist of each platform, which make.py writes from `app.json` and the plugins, carries the display name, the orientations of iPhone and iPad, the keys the runtime relies on and the keys and classes of the plugins. Builds for this Mac, native or Mac Catalyst, sign with the ad hoc identity, while device builds use the team of `HAYLEN_APPLE_TEAM`.

The asset catalogs carry the Haylen icons, the gradient symbol on the navy `#07112F` of the brand. The iOS icon is an opaque full square that the system masks. The macOS icons put the symbol on a navy rounded plate with a soft drop shadow, on the grid of macOS icons, with every size drawn on its own. The layered tvOS icons have an opaque navy back layer and the symbol on a transparent front layer, and the top shelf images show the horizontal logo with its white wordmark on navy. An app replaces any of them with a file of the same path under `platform/apple/`.

### android

```text
templates/platform/android/
  settings.gradle.kts         Repositories: the engine repository of haylen.repository, Google and Maven Central, and the plugin modules of haylen.plugins.
  build.gradle.kts            Android Gradle Plugin 9.4.1 for apps and libraries, and the Gradle plugins of haylen.gradlePlugins on the build classpath.
  gradle.properties           Gradle settings and the haylen keys that make.py writes from app.json and the plugins.
  gradle/wrapper/             Gradle 9.8.0 for Android Studio.
  app/build.gradle.kts        The app module, which depends on dev.haylen:haylen, which brings GameActivity, AppCompat, the activity library and core, and on the plugin modules, applies the Gradle plugins of the plugins and has no C++.
  app/app.gradle              Settings of the app itself, empty in the template.
  app/src/main/AndroidManifest.xml
  app/src/main/res/           Adaptive launcher icon with its monochrome layer and Android TV banner.
```

The module reads the application id, version name and code, label, screen orientation and native library from `haylen.identifier`, `haylen.versionName`, `haylen.versionCode`, `haylen.name`, `haylen.orientation` and `haylen.library` of `gradle.properties`. The version code comes from the version, with 1.2.3 becoming 1002003. The manifest declares the permissions `INTERNET`, `ACCESS_NETWORK_STATE` and `VIBRATE` as visible defaults, which network access, the network events and `system.vibrate` need and which an app deletes when it does not use them, since the engine library declares no permission: a feature whose permission is missing logs once what is missing and does nothing or fails, as the [plugin guide](plugins.md#android-requirements) describes. The `<application>` sets `android:enableOnBackInvokedCallback`, which lets the back callback of `HaylenActivity` play the predictive back animation of the system. The manifest declares `HaylenActivity` single top, so the launcher icon brings back every screen that shows over the app, such as a purchase, with the `android.app.lib_name` meta-data set to `haylen.library`, which GameActivity loads, `haylen` for Lua apps, the Lua player of the haylen library, and the library of the app for C++ apps, whose APK leaves the Lua player out, the `LAUNCHER` and `LEANBACK_LAUNCHER` categories, the TV banner, and a touchscreen, the screen orientations, Android TV and a gamepad as optional features, so the same APK serves phones, tablets and Android TV. The links and notifications of the app go to `HaylenLinkActivity`, which the `haylen-links` library of the plugins that receive them declares with their intent filters, so an app without such plugins has no exported component besides its launcher activity, and `make.py run` starts the app with the launcher intent, whose task the launcher icon then brings back as it is. make.py copies the package into `app/src/main/assets/app` with `haylen-package-index.json`, the list of its files, because Android cannot list asset folders recursively. The plugin modules of an app come from `haylen.plugins`, their Gradle plugins from `haylen.gradlePlugins` and their manifest placeholders from the `haylen.placeholder.<name>` keys, which the [plugin guide](plugins.md#android) describes, so the files of the template never change per app. make.py escapes every value it writes to `gradle.properties`, which Gradle reads as ISO 8859-1, so any text survives.

The resources are vector drawables of the Haylen brand. The adaptive launcher icon has the gradient symbol inside the safe zone of its foreground, over `ic_launcher_background`, the navy `#07112F` of the brand, and a monochrome layer with the silhouette of the symbol, which launchers tint for themed icons. The Android TV banner shows the horizontal logo with its white wordmark on the same navy. An app replaces any of them with a file of the same path under `platform/android/`.

### web

```text
templates/platform/web/
  index.html        The canvas and the splash with the logo, the progress bar and the status line.
  loader.css        Styles of the page and the splash.
  loader.js         Checks the browser, picks the backend, downloads with progress, loads the plugins and starts the runtime.
  app.js            Page code of the app, empty in the template.
  haylen-logo.svg   The Haylen symbol, the splash logo and page icon of apps that name no splash logo.
```

## Assembling an app

Every app has a build folder of its own, `build/apps/<app>-<hash>/`, named after the app folder and the start of the SHA-256 hash of its absolute path, so apps in folders with the same name never share platform projects or native library builds. `run` recreates `<platform>/` in that folder for every run:

1. It deletes the folder and copies the template of the platform from `templates/platform/`: `apple` for `macos`, `catalyst`, `ios`, `ios-simulator`, `tvos` and `tvos-simulator`, `android`, or `web`. Windows and Linux have no template and start from an empty folder.
2. It copies `platform/<template>/` of the app over it, or `platform/windows/` and `platform/linux/` on those platforms. A file at the same path replaces the one of the template and a new file is added.
3. It injects the package: `app/` next to the Xcode project, `app/src/main/assets/app/` with its index on Android, and `app.zip` on the web.
4. It builds or copies the native libraries of the app and of its plugins and places them as [native libraries](#native-libraries) describes.
5. It writes the generated settings: `App.xcconfig`, the three `Info.plist` files, the entitlements of the plugins and the splash assets on Apple platforms, the `haylen` keys of `gradle.properties`, `haylen.library` among them, and the splash resources on Android, and `config.json` with the splash logo, the transparency and the plugins on the web. The macOS `Info.plist` of an app whose `window.showInTaskbar` is `false` has `LSUIElement`, so macOS never shows its Dock icon, not even while it starts. It links `Haylen.xcframework` into the Apple project and copies the WebGPU and WebGL2 runtimes into the site.
6. It adds the native parts of the plugins: their sources, packages, frameworks, resources and build scripts to the Apple project, which it then generates again, their library modules and files to the Android project, and their web modules to the site, as the [plugin guide](plugins.md#using-plugins) describes.

Before the first step, `run` checks the plugins of the app and their values for the platform, and stops with every problem it finds. The generated settings are written after the copies of the templates, so they always follow `app.json`, even over a copy of the templates in `platform/`.

## Platform overrides

An app keeps only what is specific to it in `platform/<template>/`, and each template has one file meant for app code:

| Template | App file | What it holds |
| --- | --- | --- |
| `apple` | `source/` | `main.mm`, which registers native `HaylenBridge` handlers before `haylen_main`, and any other Objective-C, C++ or Swift file of the app, which every target builds. |
| `android` | `app/app.gradle` | Dependencies, resources and the Application class of the app, set through the `haylenApplication` manifest placeholder, next to its Java or Kotlin sources under `app/src/main/`. |
| `web` | `app.js` | Page code that runs after the loader creates `Module` and before the runtime starts, such as `Module.preRun.push(() => Module.haylen.register(...))`. |

Tiny Island keeps only its Google sign-in there:

```text
samples/games/tiny-island/platform/
  android/app/app.gradle                                  Credential Manager dependencies, the web client id as a string resource and TinyIslandApplication as the Application class.
  android/app/src/main/java/dev/haylen/tinyisland/        TinyIslandApplication, which registers GoogleSignInPlugin for auth.google.signIn.
  web/app.js                                              auth.google.signIn with Google Identity Services.
```

The Android client id comes from the `googleServerClientId` Gradle property, for example from `~/.gradle/gradle.properties`, and the web client id from the constant at the top of `app.js`. The Google script needs `--coep off` or `--coep credentialless`.

## Native libraries

The `native` section of `app.json` lists the native libraries an app ships, by the name `native.load` takes, as prebuilt files for each platform or as a CMake project that make.py builds for each platform it lists. The [native code guide](native.md#packaging-libraries-with-an-app) describes the section. The `native` section of a plugin adds one more library, named after the plugin id with underscores for its dashes, which make.py builds and places in the same way.

```json
{
    "native": {
        "steam_api": {"files": {"macos": "platform/apple/native/libsteam_api.dylib", "windows": "platform/windows/steam_api64.dll", "linux": "platform/linux/libsteam_api.so"}},
        "my_glue": {"cmake": "native", "platforms": ["macos", "ios", "android", "windows", "linux"]}
    }
}
```

make.py builds a CMake library in `native/<library>/` of the [build folder of the app](#assembling-an-app), once per architecture with the settings of the engine artifacts, and joins the architectures of Apple platforms with `lipo`. Then it places every library of the run platform:

| Platform | Place |
| --- | --- |
| macOS and Mac Catalyst | `native/<target>-<platform>/` of the Apple project, which the `Embed native libraries` phase copies into `Contents/Frameworks` and signs. An `.xcframework` gives the slice of the platform. |
| iOS and tvOS | The same, into `Frameworks`, where a dynamic CMake library becomes a framework with its own `Info.plist`. A static library is linked through `OTHER_LDFLAGS` in `App.xcconfig`, and make.py writes `source/HaylenNativeSymbols.mm`, which keeps and registers the symbols the section lists. |
| Android | `app/src/main/jniLibs/<abi>/` for arm64-v8a, armeabi-v7a and x86_64, built one ABI after the other with the NDK and `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES`. |
| Windows | Next to the player. |
| Linux | `lib/` next to the player, whose `RUNPATH` is `$ORIGIN:$ORIGIN/lib`. |
| Web | Nothing, because the browser loads no native libraries. |

## Splash screens

The `splash` object of `app.json` sets the launch screen of every platform:

```json
{
    "splash": {"logo": "ui/splash.png", "background": "#FF101418"}
}
```

`logo` is an image relative to `content/`, and without it the platforms show the Haylen symbol. `background` is a color as `#RRGGBB` or `#AARRGGBB` and defaults to `clearColor`, so the launch screen blends into the first frame of the app.

| Platform | Launch screen |
| --- | --- |
| iOS, iPadOS, Mac Catalyst | `ios/LaunchScreen.storyboard`: the `splash_logo` image centered in the safe area over the `splash_background` color, both from the asset catalog. Auto Layout keeps the logo square, at most 200 points and at most half of the safe area in each direction, so it fits every device, orientation and iPad window size. |
| tvOS | `tvos/LaunchScreen.storyboard` with the same layout and a logo of at most 360 points. |
| Android | The SplashScreen API of `androidx.core:core-splashscreen`, with the `Theme.Haylen.Splash` theme of the manifest showing `haylen_splash_icon` over `haylen_splash_background`, then `Theme.Haylen`, whose window background has the same color. The system splash screen ends with the first frame of the activity window, while `sokol_app` draws only once that window shows and has the focus, so `HaylenSplash` covers the surface of the app from that first frame with a view of the same background and icon, which stays until the player reports its first frame through `nativeFramePresented`. No black frame shows in between. The icon insets the logo so it fits the circle that Android masks it with, in every orientation, on phones, tablets and TVs. |
| Web | The page shows the logo over the background with the progress bar until the app starts. |

make.py writes the logo into the `splash_logo` image set of the iOS and tvOS asset catalogs, as `templates/platform/web/haylen-logo.svg`, the SVG of the Haylen symbol, with its vector data preserved when the app names none, and the background into the `splash_background` color set. On Android it writes the background into `res/values/haylen_splash.xml` and copies the logo to `res/drawable/haylen_splash_logo.<ext>`, which replace the defaults of the library, the Haylen symbol as a vector drawable over `#FF101418`. Android splash logos are PNG, WebP or JPEG images.

## Development mode

The runtime turns on development behavior, which today is hot reload of the package folder, only when its command line has `--dev`. `--native <folder>`, which may repeat, adds a folder that `native.load` searches before the folders of the platform. The first command-line argument that is not an option names the package to play, a folder or a zip, and without one the runtime plays the package bundled with the app. Other options are ignored, because systems add their own, such as the ones Xcode passes to the macOS apps it launches. `make.py run` without `--platform` is the only place that passes `--dev`, so apps built from the templates, shipped apps and web pages never run in development mode.

## Web loader

`loader.js` runs when the page loads:

1. It creates `Module` with the canvas, so `app.js` can add page handlers to `Module.preRun`.
2. It reads `config.json`, which make.py writes with the app name, whether `window.transparent` is set, the splash logo and background, the size of `app.zip` and of the two `haylen.wasm` files, and the plugins with a web part. It sets the title, the background, and the logo and the page icon, which are the logo of the app or `haylen-logo.svg`. For a transparent app the page itself has no background, so whatever holds the page, such as the page of an editor that embeds it in a frame, shows through the transparent pixels of the canvas, and only the splash keeps the splash background.
3. It checks for WebAssembly and picks the backend: WebGPU when `navigator.gpu` returns an adapter, and WebGL2 otherwise. `?backend=webgpu` or `?backend=webgl2` forces one when the browser supports it. A browser with neither sees a message instead of a blank page.
4. It downloads `<backend>/haylen.wasm` and `app.zip` together with one progress bar. Each download counts the bytes it streams against its `Content-Length`, or against the size in `config.json` when the length is missing or describes compressed bytes. Meanwhile it imports the web module of every plugin from `plugins/<id>/`.
5. It hands the WebAssembly bytes to the runtime as `Module.wasmBinary` and the package as `Module.haylen.packageData`, then loads `<backend>/haylen.js`.
6. Before the app starts, a `Module.preRun` callback holds the start with a run dependency while it calls `load(context)` of every plugin in load order, with the context of `Module.haylen.createPluginContext`, as the [plugin guide](plugins.md#web) describes. A plugin that fails to import or to load replaces the progress bar with its error, and the app does not start.
7. `Module.haylen.onStarted` hides the splash once the app runs. `Module.haylen.onError` writes every error to the console, and one that happens before the app starts replaces the progress bar with the message and the Lua stack trace. Later errors show on the error screen of the runtime.

`Module.haylen.packageData` takes the bytes of a zipped package, as an `ArrayBuffer` or a `Uint8Array`, which the runtime writes to its file system and plays instead of the bundled package. `Module.haylen.packageUrl` remains for pages that let the runtime download the package itself, and `Module.haylen.loadZip` replaces the running app later. None of them turns on development mode. The [build guide](build.md#runtime-api) lists the whole runtime API.

## Platform support

| Platform | Status | How it runs |
| --- | --- | --- |
| macOS | Supported | macOS 13.3 and later. The desktop player, or the `macOS` target of the Apple template with `--platform macos`. |
| Windows, Linux | Supported | The desktop player, or the player artifact next to the package with `--platform windows` or `--platform linux`. Built and tested on those hosts by CI. Windows apps embed the [application manifest](build.md#c-apps) of the engine, and Linux apps load GIO and GTK 3 at run time for the theme and the dialogs, and run without them as [haylen.system](lua-api/system.md#platforms) and [haylen.dialogs](lua-api/dialogs.md#platforms) describe. |
| iOS, iPadOS | Supported | iOS and iPadOS 16.3 and later. The `iOS` target on iPhone and iPad, with every orientation of `app.json` and every iPad window size. |
| Mac Catalyst | Supported | macOS 13.3 and later. The `iOS` target on the Mac, whose window opens at the size of `app.json` in the points of the Mac, where macOS places it. Mac Catalyst passes only touches to `sokol_app`, and the left mouse button arrives as a touch, so the runtime reads the keyboard through `GCKeyboard`, the right and middle buttons and the wheel through `GCMouse`, and the pointer position through a hover gesture on the view of the app. Text entry through the keyboard needs the text input bridge of the runtime. |
| tvOS | Supported | tvOS 16.3 and later. The `tvOS` target on Apple TV and its simulator, with the Siri Remote and game controllers. |
| visionOS | Runs the iPad app | The iOS target runs on Apple Vision Pro as a compatible iPad app. A native visionOS slice is not possible yet: `sokol_app` reads `UIScreen` through `windowScene.screen` in eight places, which the visionOS SDK marks unavailable, so its implementation does not compile for visionOS. |
| watchOS | Not possible | The watchOS 27 SDK has no Metal, MetalKit, GameController or AudioToolbox, which the renderer, the input and the audio of the engine need. |
| Android | Supported | Phones, tablets and Android TV from one APK, on arm64-v8a, armeabi-v7a and x86_64, Android 8.1 (API 27) and later. |
| Web | Supported | Desktop and mobile browsers with WebGPU or WebGL2. Sound needs `AudioWorklet`, which browsers offer only to pages served over https or from localhost, and apps on other pages run without sound. |

## Notes on dependencies

- miniaudio manages the audio session on iOS and tvOS with Objective-C, so its implementation file compiles as Objective-C there.
- `sokol_app` asks Android for an OpenGL ES 3.1 context, which the Android emulator on a Mac and some devices lack, while the shaders of the engine are GLSL ES 3.00, so the runtime asks every OpenGL ES build for version 3.0.
- The headers of Poco ask MSVC to link every Poco library by its file name, such as `PocoFoundationmd.lib`, while the SDK merges those libraries into `haylen.lib`, so apps that link the SDK would look for files that do not exist. The engine defines `POCO_NO_AUTOMATIC_LIBS` for Poco and every target that uses it, since CMake links the Poco libraries by their targets.
- The engine builds miniaudio with AAudio as its only Android backend, and miniaudio uses AAudio from Android 8.1 (API 27) on, because the first AAudio release of Android 8.0 has known faults. The Android library and template therefore set `minSdk` to 27 and the native code builds for API 27. The AndroidX libraries they use need API 21 and Varn's HTTP transport needs API 24. `HaylenActivity` and `HaylenSplash` reach the system bars through `WindowCompat` and `WindowInsetsControllerCompat` of `androidx.core`, and cover display cutouts from Android 9 on, where cutouts exist. Android 13 and later deliver the back button only to an `OnBackInvokedCallback`, so the library manifest sets `android:enableOnBackInvokedCallback` and `HaylenActivity` registers its callback only while the app takes back or edits a text field, which lets the system play its predictive back animation when back leaves the app. Earlier versions send the back key to the native activity, where the engine takes it or leaves it to Android. `sokol_app` picks its Android frame loop when it is compiled: builds for API 29 and later follow the Choreographer, and builds for API 27 wait for the display refresh in `eglSwapBuffers` on every device, which keeps the frame rate at the display rate with slightly less even frame pacing.
- `sokol_app` sizes the framebuffer of iOS apps by the screen, which crops every app whose window is smaller than the screen, as Mac Catalyst windows and iPad windows are. `engine/cmake/patches/sokol-ios-view-size.patch`, which CPM applies to the pinned commit, sizes it by the view of the app instead.
- `sokol_app` creates its own application delegate on Apple platforms, which on iOS, tvOS and Mac Catalyst also delegates the scene, and it is the only object that receives the launch, the links and user activities that open the app, the shortcut items, the scene life cycle and the registration and delivery of remote notifications. Native plugins need all of them, and swizzling the class of `sokol_app` at run time would hide behavior and fight the SDKs that swizzle the delegate themselves. `engine/cmake/patches/sokol-apple-delegate.patch` adds `sapp_desc.apple.delegate_class`, which names the class that `UIApplicationMain` or AppKit creates instead, and lets the scene configuration name the class of the application delegate, so one object delegates both. The runtime names `HaylenSceneDelegate` on iOS, tvOS and Mac Catalyst and `HaylenAppDelegate` on macOS, which derive from the delegates of `sokol_app`, call them for the events they handle and hand every event to the plugins, as the [plugin guide](plugins.md#events-of-the-app) describes.
- `sokol_app` ends a destroyed Android activity with `exit()`, which runs the static destructors of the process while the rendering threads of Android still use them and aborts the process with a crash report every time the app closes, and it ignores `sapp_quit()` on Android. `engine/cmake/patches/sokol-android-quit.patch` calls the cleanup callback of the runtime when the activity is destroyed, even after its window is gone, by binding the GL context without a surface, so the engine stops and releases its resources, then lets the activity finish normally and the process stay cached like any Android app. It also makes `haylen.quit()` finish the activity. A later activity in the same process starts a new runtime.
