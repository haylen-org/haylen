# 🚚 Distributing apps

This guide is for app developers who create, run and ship apps. A Haylen app is a folder with `haylen.json`, its code and its assets. The same app builds for every platform through one pipeline: the tooling resolves the app for a target, a backend generates the native project of that target in the build folder, the native tools of the platform build it against a prebuilt engine SDK, and the tooling verifies the result before it launches or packages it. The app never holds a platform project, and no app build compiles the engine.

The guide covers the targets and their backends, the pipeline, the generated projects of each platform and what an app customizes in them, signing, release builds, packages, store packs, native libraries, the engine SDK of each platform, splash screens, development mode, the web loader and the platforms that Haylen supports. The [command reference](commands.md) lists every command with its options and its result, the [plugin guide](../platform/plugins.md) covers plugins, and the [build guide](build.md) covers how the engine SDKs are built.

## Quick start

```sh
haylen sdk build --target player
haylen new ~/apps/my-game --name "My Game" --identifier com.example.mygame
haylen app run ~/apps/my-game
haylen sdk build --target ios-simulator
haylen app run ~/apps/my-game --target ios-simulator
```

The command `new` writes the starter app. The command `sdk build` builds the engine SDK of a target once, which every app of the machine then shares. The commands that build an app also build a missing SDK themselves, so `sdk build` only builds it ahead of time. The command `app run` prepares, builds, verifies and launches the app: without `--target` a Lua app plays in the desktop player of this machine in development mode, which reloads edited scripts and assets, and with `--target` it builds for that platform and launches the app on a simulator, an emulator, a device, this computer or a local web server.

An app written in C++ starts the same way, with `--kind cpp`, and goes through the same backends:

```sh
haylen new ~/apps/my-cpp-game --kind cpp
haylen sdk build --target android
haylen app run ~/apps/my-cpp-game --target android
```

## Targets

A target is a name that the commands take with `--target`. Each target belongs to one backend, which declares it with its system, the place where the app runs, its architectures with their binary interfaces, its toolchain and the kinds of app it builds. The command `haylen doctor` lists every target with its backend, its engine port, its SDK and the tools this machine has and lacks for it.

| Target | Backend | Runs on | Kinds | Builds on |
| --- | --- | --- | --- | --- |
| `player` | `player` | The desktop player of this machine, in development. | Lua | macOS, Windows, Linux |
| `macos` | `apple` | This Mac and other Macs. | Lua, C++ | macOS |
| `catalyst` | `apple` | The iPad app as a Mac app, through Mac Catalyst. | Lua, C++ | macOS |
| `ios`, `ios-simulator` | `apple` | iPhone and iPad, or their simulators. | Lua, C++ | macOS |
| `tvos`, `tvos-simulator` | `apple` | Apple TV, or its simulator. | Lua, C++ | macOS |
| `visionos`, `visionos-simulator` | `apple` | Apple Vision Pro, or its simulator. | Lua, C++ | macOS |
| `android` | `android` | Phones, tablets and Android TV, from one APK. | Lua, C++ | macOS, Windows, Linux |
| `web` | `web` | Browsers, through WebGPU or WebGL2. | Lua, C++ | macOS, Windows, Linux |
| `windows` | `desktop` | Windows. | Lua, C++ | Windows |
| `linux` | `desktop` | Linux. | Lua, C++ | Linux |

Without `--target`, a command takes the default target of the kind of the app on this machine: `player` for a Lua app on every desktop, and for a C++ app `macos` on a Mac, `windows` on Windows and `linux` on Linux. The `player` target compiles nothing and has no Release configuration, since the release of an app is built for the target of each platform.

Each target builds its default architectures, the one of this Mac for what runs on it or on its simulators, `arm64` for Apple devices, the one of this machine for Windows and Linux, `x86_64` or `arm64`, and `arm64-v8a`, `armeabi-v7a` and `x86_64` together for Android. The option `--architecture` names others, and it repeats. Every architecture of a request is resolved on its own, with its own SDK slice, and the backend joins them into one product, such as one APK with every ABI.

A backend and an engine port are separate extensions of the tooling, which register through the entry point groups `haylen.backends` and `haylen.engine_ports`. The registry matches a target with the port that builds its engine by their system, environment, toolchain, binary interfaces and capabilities, never by their names, as the [build guide](build.md) explains. The core of the tooling names no platform, so a new platform is a backend and an engine port that someone installs, without a change to the commands.

## The pipeline

Every app, Lua or C++, goes through the same steps on every target:

1. **Resolve.** The tooling reads `haylen.json`, picks the target, resolves the plugins for its platform in load order, merges their contributions with the settings of the app, settles the oldest system version, chooses the SDK slice of every architecture and freezes the result. Every error of this step names the manifest or the plugin it comes from.
2. **Prepare.** The backend generates the native project from the resolved product, writing only the files that changed.
3. **Build.** The native tools of the platform build the project: Xcode on Apple platforms, Gradle with its external CMake build on Android, CMake on Windows and Linux, and Emscripten with CMake on the web. The engine always comes from the SDK.
4. **Verify.** A generic check proves that the product exists and has a component for every target, and the backend adds what only its platform knows, such as architectures, frameworks, manifest entries, the needs of every plugin and, in a release, what a release never ships.
5. **Run.** The backend launches the app, in development with the Debug configuration.

The commands `app prepare`, `app build`, `app verify` and `app run` stop after the step they name, and `app run` says each step as it goes. The command `project inspect` shows the result of the first step: every value with its origin and the reason it has it, the plugins with their parameters, the merged contributions with their owners and the SDK slice of every architecture. The [command reference](commands.md) lists their options.

```sh
haylen project inspect ~/apps/my-game --target android
haylen app prepare ~/apps/my-game --target ios-simulator
haylen app build ~/apps/my-game --target web --config Release
haylen app verify ~/apps/my-game --target web --config Release
```

The option `--config` picks the configuration of the app: `Debug` ships the package as it is, and `Release` ships the protected release with the bootstrap of the app, as [release builds](#release-builds) explain. The option `--engine-config` picks the configuration of the engine SDK, `Release` by default, and `Debug` only to debug the engine itself, because it runs many times slower. Before the preparation, the shaders of the app whose sources changed compile again, as the command `app shaders` does.

The build of an app never compiles the engine, which always comes from an installed SDK. When no installed SDK of the current engine sources holds an architecture of the target in the configuration of the engine, because none was built or it was built from other sources or in another configuration, the commands that prepare or build the app first build that SDK through the engine port, as `haylen sdk build` does, in a step of its own that says so and how long a first build takes. The commands `project inspect` and `app verify` never build, and they stop with the `haylen sdk build` command that makes the SDK. An SDK of the current sources that serves no app of the target, such as one that runs only on newer systems than the app, stops every command with the reason.

## Platform projects

The project of a platform is generated, deterministic and disposable. The developer never edits it, and nothing of it lives in the app folder. Each product of an app has a folder of its own under `build/apps/<app>-<hash>/<product>/`, where `<app>` is the name of the app folder, `<hash>` the start of the SHA-256 hash of its absolute path, so apps in folders with the same name never share builds, and `<product>` the target and the configuration, such as `ios-simulator-debug`, with the architectures added when a request names others than the default ones.

| Folder | What it holds |
| --- | --- |
| `project/` | The generated project, with `haylen-preparation.json`, the record of the preparation. |
| `build/` | The trees and caches of the native tools, such as the derived data of Xcode, the outputs of Gradle and the CMake trees. |
| `products/` | What the build made, such as the installed desktop app, the site of the web or the APK. |
| `package/` | The staged copy of the package, with every plugin at `plugins/<id>/`, for the targets that play it as it is. |

The record of the preparation names the version of the tooling, the backend and its version, the digest of its template, the SDK and the digest of every input: the resolved product, the files of the package, the folders of the plugins, the C++ sources of the app and the files that the sections of `platforms` name. A preparation whose record matches writes nothing, and a preparation of the same inputs writes the same files, so deleting `project/` and preparing again gives the same project. A Release preparation builds the protected release again every time, since the keys of the app are no input of the record.

### What an app customizes

Everything that an app changes in a generated project is a key of the `platforms` section of `haylen.json`, or a file of the app repository that such a key names. Each section belongs to the backend that declares it, and a section that no installed backend reads stops the resolution with the list of the sections there are.

```json
{
    "platforms": {
        "apple": {"sources": ["native/apple"], "team": "ABCDE12345", "privacy": {"NSPrivacyTracking": false}},
        "ios": {"infoPlist": {"NSCameraUsageDescription": "The camera scans the cards of the game."}, "minimumSystemVersion": "17.0"},
        "android": {"sources": ["native/android"], "applicationClass": "com.example.mygame.App", "manifest": {"removedPermissions": ["android.permission.VIBRATE"]}},
        "web": {"scripts": ["native/web/app.js"], "files": {"loader.css": "native/web/loader.css"}},
        "windows": {"files": {"resources/app.ico": "art/app.ico"}}
    }
}
```

| Section | Platforms | Keys |
| --- | --- | --- |
| `apple` | Every Apple platform. | `sources`: folders and files of Swift, Objective-C, Objective-C++, C and C++ that the app target compiles, such as the bridge handlers. `team`: the development team that signs the apps of devices. `buildSettings`: Xcode build settings of the app target, besides the ones that the backend owns. `privacy`: keys of the privacy manifest. |
| `ios`, `catalyst`, `tvos`, `visionos`, `macos` | That platform. | `infoPlist`: keys of the `Info.plist`. `entitlements`: the entitlements that the app is signed with. |
| `android` | Android. | `sources`: the Kotlin and Java folders of the app module. `applicationClass`: the class of the application, where the bridge handlers register. `manifest`: `permissions` to add, `removedPermissions` that the merged manifest leaves out, and `applicationAttributes` of the `<application>` element. |
| `web` | The web. | `scripts`: the page scripts of the app, which load after the loader. |
| `windows`, `linux` | That platform. | Only the keys that every section takes. |

Every section also takes two keys that the core reads:

- `minimumSystemVersion`: the oldest system version of the platform that the app supports, in the section named after the platform, such as `platforms.ios` or `platforms.android`, where Android takes an API level such as `"29"`. A section that serves several platforms, such as `apple`, refuses it with a sentence that names the sections of its platforms.
- `files`: paths of the generated project mapped to files or folders of the app, which take the place of the template file or folder at that path. The Apple backend accepts only paths that its template holds, such as `Info.plist`, `LaunchScreen.storyboard` or `Assets.xcassets`, while the other backends also add new files, such as a drawable of Android.

### How values merge

Each value of a generated project has one owner, and the resolution decides them in a fixed order instead of the order in which it read the files:

- The app defines its identity and the settings that its sections allow. The name, the identifier, the version, the build number, the category and the orientation come from `haylen.json` alone.
- Plugins declare what they need, as typed contributions of their `plugin.json`, as the [plugin guide](../platform/plugins.md) describes.
- The SDK and the engine port declare constraints that no setting of the app cancels, such as the oldest system that the engine runs on, the architectures of each slice and the graphics APIs a slice holds.
- The backend translates the result into the project of its platform.

Values that belong to one owner are validated, and values that several owners give merge by the rule of their type. The keys `infoPlist`, `entitlements` and `privacy` of the app are contributions of the app, which merge with those of the plugins: objects join key by key, lists gain the items they lack, so two ad plugins share `SKAdNetworkItems`, and two owners that give one key different values stop the resolution with both named. Contributions that accumulate, such as the frameworks, the Maven repositories or the URL schemes, join by their identity, and two that share an identity with different values stop it too. No value silently replaces another.

The oldest system version follows the same order. A version that the app sets is final, and a plugin that needs a newer one stops the resolution with both versions, instead of moving the audience of the app. Without a version of the app, the newest version that a plugin needs counts. The SDK slice must run on that version, and when the app names none the slice gives it.

The command `project inspect` shows every merged value with its owners and origins, and the minimum system version with where it comes from and why.

## Running an app

```sh
haylen app run ~/apps/my-game
haylen app run ~/apps/my-game --target macos
haylen app run ~/apps/my-game --target ios-simulator --device "iPhone 17e"
haylen app run ~/apps/my-game --target android --device emulator-5554
haylen app run ~/apps/my-game --target web --open
haylen app run ~/apps/my-game --target linux --config Release
```

The command `app run` builds and verifies the app as `app build` does, stops when the verification fails, and then hands the app to its backend, which launches it:

| Target | Launch |
| --- | --- |
| `player` | Starts the desktop player of the SDK of this machine with the staged package and the folder of the native libraries of the app. |
| `macos` | Starts the executable of the app bundle, whose log goes to its standard output. |
| `catalyst` | Starts the executable of the Mac Catalyst bundle and streams its log from the unified log. |
| `ios-simulator`, `tvos-simulator`, `visionos-simulator` | Boots the simulator that `--device` names without a window, or else a booted one, or else the first one of the family, installs the app with `xcrun simctl install` and launches it with `launch --console-pty`, while the log of the app streams from the unified log of the simulator. A simulator that the run booted shuts down when it ends. |
| `ios`, `tvos`, `visionos` | Installs the app with `xcrun devicectl device install app` on the device that `--device` names, and launches it with `device process launch --console` and the variable `OS_ACTIVITY_DT_MODE`, which makes the process mirror its unified log to its error output. The run shows the records of the engine from that output and leaves out those of the system, such as the ones of UIKit. |
| `android` | Installs the APK, or the bundle of a release whose asset packs Google Play delivers after the install, on the device or emulator that `--device` names, or on the only one connected, starts it with its launcher intent and streams the log of its process. |
| `web` | Serves the site on `--host` and `--port` with the `--coep` and `--coop` policies, and opens it with `--open`. |
| `windows`, `linux` | Registers the URL schemes of the app for the installed app of the products folder, then runs its executable. |

The command `haylen devices` lists the identifiers that `--device` takes. Where the log of the engine goes:

- On desktops, to the standard output.
- On Android, to the log of the process, with `haylen` as the tag. The run streams it with `adb logcat`, filtered to every line of the engine, printed as on a desktop, to the lines of the plugins of the app, which log under their id as the tag, and to the crashes of its Java code under `AndroidRuntime`. The rest of the process, such as the warnings of the graphics driver, stays out, and `adb logcat` still shows it.
- On iOS, tvOS, visionOS and Mac Catalyst, to the unified log, with the bundle identifier of the app as the subsystem and `haylen` as the category. A device gives the Mac no stream of its unified log, so the run of a device shows the records that the process mirrors to its error output, and Console.app shows the whole log of the device.

#### While the app runs

Once the app runs, `app run` prints a summary of the app, the device and the configuration, in development also how saved files reach the app and the port of `--debugger-port` that an editor attaches its [debugger](debugger.md) to, and the keys that act on the app, one per line. The log of the app follows.

```text
==> Preparing the project of "My Game" for "ios-simulator" in the "Debug" configuration
==> Building "My Game" for "ios-simulator"
==> Launching "My Game" on "ios-simulator"

  App            My Game ("com.example.mygame")
  Device         iPhone 17e, simulator
  Configuration  "Debug", with the engine in "Release"
  Hot reload     Every file saved in the app reloads in it
  Debugger       Attach an editor to port 4711 of this machine

  r  Reload what changed
  R  Restart the app
  d  Open the developer console, which runs commands in the app
  s  Show the statistics of the app, compact, full or off in turn
  i  Show or hide the inspector of the app
  p  Turn the debug drawings of the app on or off
  g  Capture a frame of the GPU
  c  Clear the screen
  q  Quit
  h  List the keys
```

| Key | What it does |
| --- | --- |
| `r` | Sends the files saved since the last batch at once, for a save that the development server has not seen yet. |
| `R` | Restarts every connected app from its package with a fresh Lua state, which the app reports back. |
| `d` | Opens the [developer console](../lua-api/debug.md#console), as the next section describes. |
| `s` | Moves the [statistics](../lua-api/debug.md#statistics) of every connected app to the next mode: the compact numbers in a corner, then the statistics window, then nothing. It runs the console command `stats`. |
| `i` | Shows or hides the [inspector](../lua-api/debug.md#inspector) of every connected app, a window that lists and edits the values of its scenes, its autoloads, its watched values and its globals. It runs the console command `inspector`. |
| `p` | Turns the [debug drawings](../lua-api/debug.md#drawings) of every connected app on or off, such as the shapes of physics and the bounds of sprites. It runs the console command `draw`. |
| `g` | Asks every connected app for a capture of its next frame with the capture tool of its graphics API, through the console command `gpuCapture`, and the terminal prints the path of each capture file once the app wrote it, as [GPU captures](../lua-api/debug.md#gpu-captures) describes. The command `app capture` does the same from another terminal. |
| `c` | Clears the screen and its scrollback. |
| `q` | Quits the app and ends the command: it terminates the app on a simulator or on Android, stops the server of a web page and ends the process of the app elsewhere. |
| `h` | Lists the keys again. |

A key acts at once, without Enter, in an interactive terminal. The keys `r`, `R`, `d`, `s`, `i`, `p` and `g` need the development server, so the Release configuration offers only `c`, `q` and `h`. They reach the app wherever it runs, so a phone, a TV or a browser shows its statistics, its inspector and its drawings and captures its GPU frames without a keyboard. The terminal prints the answer of each app, such as `The statistics are "compact".`. Without an interactive terminal, such as in continuous integration, the summary still prints and no key is read, so a run whose output goes to a file streams the log of the app there too. A signal that ends the command, `SIGTERM`, `SIGHUP` or `SIGALRM`, such as the one of a bound on its time, stops the app as the key `q` does before the command ends, so no app runs on without its log.

##### The developer console

The key `d` opens the developer console of the running app in the terminal. A line with the prompt `>` stays at the bottom of the terminal, and the log of the app prints above it, so the two never mix. The console takes every key until Escape, or Enter on an empty line, gives the keys back to the run. While the line is empty, a dim hint on it says how to list the commands and how to leave.

```text
The console runs each line in the app. Type "help" to list the commands. Tab completes, the arrows go through the lines typed before, and Escape or Enter on an empty line goes back to the keys.
> timeScale 0.25
The time scale is 0.25.
>
```

| Key | What it does in the console |
| --- | --- |
| Enter | Runs the line in every connected app. The answer of each app prints above the line, broken between words at the width of the terminal, and an error prints after `Error:`. On an empty line it closes the console and gives the keys back to the run. |
| Tab | Completes the word before the cursor: the name of a command, or `true` and `false` for a boolean argument. When several commands start the same way, it completes what they share and lists them. |
| Up and Down | Go through the lines typed before in this run. |
| Left, Right, Home and End | Move the cursor. Ctrl+A and Ctrl+E also go to the start and the end. |
| Backspace and Delete | Remove the character before or after the cursor. Ctrl+U removes the whole line. |
| Escape | Closes the console and gives the keys back to the run. |

Type `help` to list the commands of the app, with how to type each one in a column and what it does beside it, broken between words at the width of the terminal and aligned under the column, and `help stats` to show one command. The terminal answers `help` itself from the commands that the app sent, so the list prints once however many apps are connected. The [console reference](../lua-api/debug.md#console) lists the commands of the engine, such as `timeScale 0.25`, `pause`, `restart`, `inspect watched.player` and `lua player.speed`, and shows how an app adds its own, such as `gold 50`. The console asks the app for its commands as it opens and after every line, so the completions follow the commands that the app adds and removes as it runs.

## Version and build number

Every platform takes two numbers from `haylen.json`. The `version`, such as `1.2.0`, is the version that people see in the stores. The `build` is a whole number that grows with every upload of the app, such as 1, 2 and 3, and it starts at 1 when `haylen.json` gives none. The `build` is the only source of every build number of the app, so nothing derives one from the version, and no setting of a section sets one of its own.

| Platform | `version` | `build` |
| --- | --- | --- |
| Apple | `MARKETING_VERSION`, which is `CFBundleShortVersionString`. | `CURRENT_PROJECT_VERSION`, which is `CFBundleVersion`. |
| Android | `versionName`. | `versionCode`. |
| Windows | The first three numbers of the version resource and its texts. | The fourth number of the version resource. |
| Linux | The version of the `.deb` package and the AppImage. | Nothing, since the desktop entry and the metainfo carry no build number. |
| Protected releases | Nothing. | The app build of the release and of the content that `content publish` publishes. |

Google Play refuses an upload whose `versionCode` is not higher than the last one, and takes at most 2100000000, so the `build` is a number from 1 to 2100000000. The App Store refuses a build number that it already received for the same version. A content channel serves the build that published it and every later build, as the [content guide](../content/content.md#publication-trees) explains. So raise the `build` before every upload, and keep the `version` for what people see.

```json
{
    "name": "Sky Garden",
    "identifier": "com.example.skygarden",
    "version": "1.2.0",
    "build": 14,
    "category": "game"
}
```

## Category

Every app names its `category` in `haylen.json`, such as `"game"` or `"utility"`. The stores and the app menus of the systems show the app among the apps of its kind. The key has no default, since only the app knows what it is, so a `haylen.json` without it stops every preparation. The command `haylen new` writes `"utility"` unless `--category` names another.

```json
{
    "name": "Star Atlas",
    "identifier": "com.example.staratlas",
    "version": "1.0.0",
    "category": "education"
}
```

Each backend maps the category to the one of its platform. On Linux the desktop entry lists one main category of the freedesktop menu, with an additional category where one fits, the AppStream metainfo lists the same, and the `.deb` package takes a section of the Debian archive. On macOS and Mac Catalyst the `Info.plist` takes `LSApplicationCategoryType`, which the Mac App Store and the Finder read. The key belongs to the engine with `haylen.json` as its origin, so an `infoPlist` of the app that gives it another value stops the preparation, as [how values merge](#how-values-merge) describes. On Android the `<application>` takes `android:appCategory` where Android has a matching value, and nothing where it has none, and an app that wants another value of Android sets it in `applicationAttributes` of `platforms.android.manifest`.

| `category` | Linux desktop entry and metainfo | Debian section | macOS and Mac Catalyst | Android |
| --- | --- | --- | --- | --- |
| `game` | `Game` | `games` | `public.app-category.games` | `game` |
| `entertainment` | `AudioVideo` | `misc` | `public.app-category.entertainment` | None |
| `education` | `Education` | `education` | `public.app-category.education` | None |
| `productivity` | `Office` | `misc` | `public.app-category.productivity` | `productivity` |
| `utility` | `Utility` | `utils` | `public.app-category.utilities` | None |
| `graphics` | `Graphics` | `graphics` | `public.app-category.graphics-design` | `image` |
| `music` | `AudioVideo` and `Audio` | `sound` | `public.app-category.music` | `audio` |
| `video` | `AudioVideo` and `Video` | `video` | `public.app-category.video` | `video` |
| `social` | `Network` and `Chat` | `net` | `public.app-category.social-networking` | `social` |
| `developer` | `Development` | `devel` | `public.app-category.developer-tools` | None |

The other platforms take no category from the app. The `Info.plist` of iPhones, iPads, the Apple TV and the Vision Pro has none, since App Store Connect sets the category of those apps. The web site writes no web app manifest, the Inno Setup installer of Windows has no category, and Steam and Google Play take the category of an app in Steamworks and the Play Console.

## The Apple project

The Apple backend serves macOS, Mac Catalyst, iOS and iPadOS, tvOS and visionOS, on their devices and simulators, for Lua and C++ apps alike. It generates an XcodeGen spec with one app target, `App`, into `project/` of the product, and XcodeGen writes `App.xcodeproj` into `build/` of the product whenever the spec changed or the project is missing. Then `xcodebuild` builds the scheme `App` for the destination of the target, with its derived data in `build/derived/`.

| File in `project/` | Contents |
| --- | --- |
| `project.yml` | The spec of the app target and of a target per app extension. The app target links `Haylen.xcframework` of the Apple SDK, with `HaylenPlayer.xcframework` for a Lua app, and the system frameworks of the engine that the SDK lists for the platform. It compiles the Apple sources of the plugins and the `sources` of the `apple` section, and for a C++ app the `cpp.sources` of `haylen.json`, which Xcode reads where they lie. It adds the frameworks, Swift packages, binary frameworks, resources, build scripts and app extensions of the plugins, and sets the deployment target to the oldest system version of the resolution. |
| `App.xcconfig` | The identity of the app: `HAYLEN_PRODUCT_NAME`, `HAYLEN_DISPLAY_NAME`, `HAYLEN_BUNDLE_IDENTIFIER`, `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION`, which every target and the values of plugins name as `$(NAME)`. |
| `main.m` | Calls `haylen_main`, after the bridge handlers of the app and its plugins registered when their classes loaded. |
| `HaylenBridging.h` | Makes `HaylenBridge.h`, `HaylenPlugin.h` and `HaylenNotificationPlugin.h` visible to the Swift sources of the app and its plugins. The module of the target is `HaylenApp`, so Objective-C reaches Swift classes through `HaylenApp-Swift.h`. |
| `Info.plist` | The `Info.plist` of the template, or of the app through `files`, joined with the keys of the engine and the `infoPlist` contributions of the plugins and the app. |
| `App.entitlements` | The `entitlements` contributions of the plugins and the app, written when there are any. |
| `PrivacyInfo.xcprivacy` | The [privacy manifest](#privacy-manifest) of the app. |
| `Assets.xcassets` | The app icon of the template, or of the app through `files`. |
| `LaunchScreen.storyboard`, `Splash.xcassets` | On iOS, iPadOS, Mac Catalyst and tvOS, the launch screen and its image `splash-logo` and color `splash-background`, as [splash screens](#splash-screens) describe. |
| `app/` | The package, which the bundle carries as its resource `app`, or in the Release configuration the part of the protected release that ships with the app. |
| `HaylenBootstrap.cpp` | The bootstrap of the app. In the Release configuration the content tool writes it from the key folder of the app, with its sealed content keys. In the Debug configuration it holds only a check that stops a release build of the development package. |
| `HaylenBridgeAsync.swift` | The [Swift helpers](../platform/plugins.md#swift-helpers) of the bridge and of plugin contexts, from the Apple SDK. |
| `HaylenNativeSymbols.mm` | The table of the symbols of the static [native libraries](#native-libraries), when the app links any. |
| `extensions/<name>/` | The `Info.plist` and entitlements of each app extension of the plugins. |

The build settings of the app target come from the backend: C++20, the bridging header, the device families of the platform, the ad hoc signing of what runs on this Mac, the hardened runtime of macOS and the architectures of the product. The `buildSettings` of the `apple` section come last, so they win over them, except the settings that the backend owns, whose values come from the SDK, the target or `haylen.json`: the architectures, the SDK, the device families and the Mac Catalyst support of the target, `OTHER_LDFLAGS`, the signing, the `Info.plist` file, the deployment targets that `minimumSystemVersion` settles and the name, the bundle identifier and the versions of the app. A key of those stops the preparation with a sentence that names it and where its value comes from, so no setting cancels a constraint of the SDK or the backend. Device builds sign automatically with the development team of `platforms.apple.team`, or else with the team that the variable `HAYLEN_APPLE_TEAM` names, such as in continuous integration, and a device build without either stops with a sentence that names both.

The `Info.plist` of each platform in the template is complete. It holds `$(HAYLEN_DISPLAY_NAME)`, `$(MARKETING_VERSION)` and the other build settings for the values of `haylen.json`, and visible defaults such as the scene manifest, `ITSAppUsesNonExemptEncryption`, the statement on export compliance that App Store Connect asks about, and `UIApplicationSupportsIndirectInputEvents`, through which UIKit hands the mouse and the trackpad of an iPad or a Mac to the app as a pointer. The keys of the engine join them:

- Every orientation of iPhone and iPad. The runtime allows only the ones of `haylen.json`, since a foldable iPhone shows an app whose `Info.plist` lists fewer in a box of the size of its outer screen on its inner screen.
- The status bar that the app launches with, as `UIStatusBarHidden` and `UIStatusBarStyle` from `window.statusBar`.
- `CADisableMinimumFrameDurationOnPhone`, without which an iPhone shows no app above 60 frames per second, whatever its display refreshes at and whatever `window.frameRate` asks.
- The background mode `audio` for an app that sets `audio.background`.
- `LSUIElement` on macOS for an app whose `window.showInTaskbar` is `false`, so macOS never shows its Dock icon, not even while it starts.
- `LSApplicationCategoryType` on macOS and Mac Catalyst from `category`, as [category](#category) lists.

The join follows the rule of [how values merge](#how-values-merge): a key that the template, the engine, a plugin and the app give with different values stops the preparation with every origin, so an app that words a key in its own way puts it in its own `Info.plist` through `files` and leaves it out of the plugins, or gives the plugin the words through its parameters. The verification reads the built bundle: its architectures, the frameworks it links, the classes of the plugins, the `Info.plist` keys, the entitlements it is signed with, the privacy declarations and the resources of the plugins.

The asset catalogs of the template carry the Haylen icons, the gradient symbol on white, the same art that the repositories of Haylen show. The iOS icon is an opaque full square that the system masks. The macOS icons put the symbol on a white rounded plate with a light gray edge, on the grid of macOS icons, with every size drawn on its own. The layered tvOS icons have an opaque white back layer and the symbol on a transparent front layer. The top shelf images show the horizontal logo with its navy wordmark on white.

## The Android project

The Android backend serves the target `android`, which builds one APK for phones, tablets and Android TV with every ABI of the target, `arm64-v8a`, `armeabi-v7a` and `x86_64`, or with the ones that `--architecture` names. It resolves one project per ABI, each with the slice of its ABI in the Android SDK, and generates the whole Gradle project of the app into `build/apps/<app>-<hash>/android-<config>/project/`. The developer customizes it only through the section `platforms.android` of `haylen.json`:

| Key | Value |
| --- | --- |
| `sources` | The Kotlin and Java folders of the app, such as `native/android`, which compile into the app module where they lie. |
| `applicationClass` | The application class of the app, such as `com.example.game.App`, where the native bridge handlers of the app register before the first activity starts. |
| `manifest` | The `permissions` that the manifest adds, the `removedPermissions` that the merged manifest leaves out, and the `applicationAttributes` of the `<application>` element by their names without the `android:` prefix, such as `{"largeHeap": "true"}`. |
| `minimumSystemVersion` | The oldest API level of the app, the `minSdk`. |
| `files` | Files and folders of the app by their paths in the project, which replace the files of the template there, such as `{"app/src/main/res/drawable/banner.xml": "art/banner.xml"}` for the Android TV banner. |

The generated project holds:

- The scripts of the template, with the identity, the `version`, the `build`, the orientation and the status bar of `haylen.json`, the ABIs of the product, the NDK of the engine, which strips the debug information of the native libraries from the APK, and the parts of the plugins: their modules in `plugins/<id>/`, their Maven repositories, their Gradle plugins, their manifest placeholders and the files they place, as the [plugin guide](../platform/plugins.md#android) describes.
- `app/src/main/assets/app/`: the package with `haylen-package-index.json`, the list of its files, because Android cannot list asset folders recursively. In the Release configuration it holds the protected release instead, a flat folder that Android lists itself.
- `app/src/main/assets/haylen-plugins.json`: the plugins with a native Android part in [load order](../platform/plugins.md#load-order), with the values of their parameters.
- `app/src/main/res/`: the splash resources of `haylen.json`.
- `app/src/main/jniLibs/<abi>/`: the prebuilt native libraries of the app and its plugins.
- `native/CMakeLists.txt`: the native code of the app, which the external native build of Gradle compiles for every ABI against the CMake package of the Android SDK of that ABI. It builds `haylen-app`, the library of the app that the activity loads, from the sources of the `cpp` section of a C++ app, or from the Lua player with the bootstrap of a Lua release, and the native libraries of the app and its plugins that build from a CMake project. A debug build of a Lua app compiles no app library, since it plays the package with the player of the `haylen` library.
- `asset-packs/<name>/`: the asset packs of a release, as [release builds](#release-builds) describe.
- `local.properties`: the Android SDK and the CMake of this machine, which the external native build takes instead of an older CMake of the Android SDK.

The values reach the scripts as Kotlin strings, the manifest as XML text and the properties as ASCII with `\uXXXX` escapes, so any text survives. Gradle builds the project with its outputs, its project cache and the trees of its native build in `build/apps/<app>-<hash>/android-<config>/build/`, and the APK, with the bundle of a release whose packs Google Play delivers, goes to `products/` next to it under the identifier of the app. Gradle receives the signing keys of the app through its environment, so no password reaches a file of the project or a command line.

The app module depends on `dev.haylen:haylen` from the Maven repository of the Android SDK, which brings GameActivity, AppCompat, the activity library and core. The `minSdk` is the API level that the resolution settles, and at least the 27 that the engine library needs. Release builds sign with the upload key of the app, as [signing](#signing) describes, and R8 shrinks their code and resources.

The manifest declares the permissions `INTERNET`, `ACCESS_NETWORK_STATE`, `VIBRATE` and `ACCESS_LOCAL_NETWORK` by default. Network access, the network events, the rumble of gamepads and the devices of the local network need them, since the engine library declares no permission. An app that does not use a feature names its permission in `removedPermissions` of `platforms.android.manifest`, and the merged manifest then leaves it out whichever library brings it. A feature whose permission is missing logs once what is missing and does nothing or fails, as the [plugin guide](../platform/plugins.md#android-requirements) describes. The `<application>` sets `android:enableOnBackInvokedCallback`, which lets the back callback of `HaylenActivity` play the predictive back animation of the system.

The manifest declares `HaylenActivity` with:

- `singleTop`, so the launcher icon brings back every screen that shows over the app, such as a purchase.
- Every change of the configuration in `android:configChanges`, so a new language, font size or display keeps the app running instead of starting it again.
- The meta-data `android.app.lib_name`, set to the native library that GameActivity loads: `haylen`, the Lua player of the haylen library, for the debug builds of Lua apps, and `haylen-app`, the library of the app, for C++ apps and the release builds of Lua apps, whose APK leaves the Lua player out.
- The meta-data `dev.haylen.statusBar`, which launches the activity with the status bar of `haylen.json` until the engine applies it. Without it, the status bar starts hidden.
- The categories `LAUNCHER` and `LEANBACK_LAUNCHER` and the TV banner.

The manifest declares a touchscreen, the screen orientations, Android TV and a gamepad as optional features, so the same APK serves phones, tablets and Android TV. The links and notifications of the app go to `HaylenLinkActivity`, which the `haylen-links` library of the plugins that receive them declares with their intent filters. So an app without such plugins has no exported component besides its launcher activity.

The resources are vector drawables of the Haylen brand. The adaptive launcher icon has the gradient symbol inside the safe zone of its foreground, over `ic_launcher_background`, which is white. Its monochrome layer has the silhouette of the symbol, which launchers tint for themed icons. The Android TV banner shows the horizontal logo with its navy wordmark on the same white.

## The web site

The web backend generates the project of the site into `project/` of the product:

- The page files of the template, `index.html`, `loader.js` and `loader.css`, with the files of `platforms.web.files` in their place or next to them.
- The page scripts of `platforms.web.scripts` under `scripts/`, which `index.html` loads after the loader where the template marks it.
- The web module of every plugin with the files of its folder under `plugins/<id>/`.
- The splash logo and `config.json`.
- In the Debug configuration of a Lua app, `app.zip`, the zipped package that the prebuilt web player of the web SDK plays. Nothing compiles then.
- In the Release configuration and for a C++ app, the CMake project of a runtime of its own in `runtime/`, which `haylen_add_app` links against the web SDK with the pinned Emscripten, with the protected release and the bootstrap of the app for a release.

The build writes the site into `products/`: the page files, the runtime with the processor of its audio output and the worker of its content store, the part of a release that ships with the page in `app/`, and `config.json` with the size of every download, so the loader shows progress when the server sends no length. The verification checks the page with its runtime and every plugin with a web module in `config.json`, and in the Release configuration anything that a release never holds. The run serves the site, in development with the development server on the same port.

## Windows and Linux

The desktop backend generates the CMake project of a Windows or Linux app from the template of its platform, builds it with CMake against the desktop SDK of this machine in `build/` of the product and installs the app into `products/`. It builds only on a machine of the system of its target. Lua and C++ apps share the project, and only the call of `haylen_add_app` differs, as the [embedding guide](embedding.md) describes.

| File in `project/` | Contents |
| --- | --- |
| `CMakeLists.txt` | The project of the template, which includes `app.cmake`, installs the app and describes its packages for CPack. |
| `app.cmake` | The values of `haylen.json` as `HAYLEN_APP_NAME`, `HAYLEN_APP_IDENTIFIER`, `HAYLEN_APP_VERSION`, `HAYLEN_APP_DESCRIPTION`, `HAYLEN_APP_COMPANY`, `HAYLEN_APP_COPYRIGHT`, `HAYLEN_APP_WEBSITE` and `HAYLEN_APP_EXECUTABLE`, the content and the native libraries that the app ships in `HAYLEN_APP_CONTENT` and `HAYLEN_APP_NATIVE`, the desktop SDK through `find_package(haylen)`, and the target `main` that `haylen_add_app` builds. |
| `HaylenBootstrap.cpp` | In the Release configuration, the bootstrap of the app, which the executable compiles in. |
| `resources/` | On Windows, the icon, the resource script, the application manifest and `HaylenApp.h`, the values of `haylen.json` as strings of the resource script. On Linux, the desktop entry, the AppStream metainfo and the icons. |
| `signing.cmake` | On Windows, the signing of the executable, the installer and the uninstaller, as [signing](#signing) describes. |
| `links.iss` | On Windows, for an app whose plugins declare URL schemes, the registry entries that the installer adds for them. |

The executable takes the name of the app folder in lowercase dash-case, such as `my-game` for `My Game`, which every package format accepts. The installed app is the app as it ships:

```text
build/apps/my-game-1a2b3c4d/windows-debug/products/
  my-game.exe             The executable of the app, with its icon, version resource and manifest.
  app/                    The package, or the manifests and shards of the protected release.
  my-glue.dll             The native libraries of the app.

build/apps/my-game-1a2b3c4d/linux-debug/products/
  bin/my-game             A link to the executable.
  lib/my-game/my-game     The executable of the app.
  lib/my-game/app/        The package, or the manifests and shards of the protected release.
  lib/my-game/lib/        The native libraries of the app.
  share/applications/     The desktop entry, named after the identifier.
  share/metainfo/         The AppStream metainfo.
  share/icons/hicolor/    The icons.
```

### Windows

The resource script `resources/app.rc` reads the values of `haylen.json` from `HaylenApp.h` for the version resource, which the properties of the file and the Task Manager show:

| Field | Value |
| --- | --- |
| `CompanyName` | `company` of `haylen.json`. |
| `FileDescription`, `ProductName` | `name`. |
| `FILEVERSION`, `PRODUCTVERSION` | The numbers of `version` followed by `build`, so the version `1.2` of build 7 is `1.2.0.7`. The resource holds numbers up to 65535, and a larger one stops the preparation with a message that names it. |
| `FileVersion`, `ProductVersion` | `version`. |
| `LegalCopyright` | `copyright`. |
| `InternalName`, `OriginalFilename` | The executable. |
| `Comments` | `description`. |

The application manifest of the template declares Windows 10 and 11, per-monitor DPI awareness and no elevation, and the build merges it with the manifest of the engine, which selects the Common Controls of version 6 and UTF-8 as the code page. The engine needs the Common Controls of version 6 and UTF-8 as the code page of the process, as [the Windows section](embedding.md#windows) of the embedding guide explains. The executable builds in the Windows subsystem, without a console, and links the C runtime of the configuration of the engine SDK. The window of the app and its taskbar button take the first icon of the executable, the one that Explorer shows. The icon is the gradient symbol on a white plate with rounded corners and a light gray edge, every size drawn from the full-size art.

### Linux

The executable links the C++ library of the compiler statically, so the app runs on distributions older than the one that built it, as long as their C library is as new. The project installs:

- The executable into `lib/<executable>/`, with the package in `app/` and the native libraries in `lib/`, which the `RUNPATH` `$ORIGIN:$ORIGIN/lib` of the executable names.
- A link `bin/<executable>` to the executable, so desktops start the app by name.
- The desktop entry into `share/applications/`, the metainfo into `share/metainfo/` and the icons into `share/icons/hicolor/`, all named after the identifier of `haylen.json`. The identifier is also the class of the window of the app, so desktops show the name and the icon of the entry for the window.

The executable cannot share the name of those folders, so a Linux build of an app whose folder gives the executable the name `app` or `lib` stops and asks for another folder name.

The backend completes `resources/app.desktop` and `resources/app.metainfo.xml` of the template, or the files of the app that `platforms.linux.files` puts in their place. What the file gives wins, with a warning when it differs from `haylen.json`, and the values of `haylen.json` fill what is missing:

| File | Values of `haylen.json` |
| --- | --- |
| `app.desktop` | `Name` from `name`, `Comment` from `description`, `Categories` from `category`, `Exec` from the executable, `Icon` and `StartupWMClass` from `identifier`, and the URL schemes of the plugins in `MimeType`. |
| `app.metainfo.xml` | `id` from `identifier`, `name`, and `summary` and a `description` of one paragraph from `description`, `developer` with the name of `company` and the identifier without its last part as its id, the `launchable` desktop entry, the `categories` from `category`, and the `homepage` URL from `website`. |

The metainfo of the template declares the `CC0-1.0` license for the metadata itself, a proprietary license for the app and an empty content rating, which the app changes through its own file. The `.deb` package takes the section of the Debian archive that `category` maps to, as [category](#category) lists. The command `appstreamcli validate` checks the metainfo, which needs a `website` in `haylen.json` to pass, and `desktop-file-validate` checks the desktop entry.

### Links on Windows and Linux

An app opens from a link of a URL scheme that the `urlScheme` contributions of its plugins declare, as the [plugin guide](../platform/plugins.md#desktop-contributions) describes. Windows and Linux start a new process of the app for every link, with the link after the option `--link`. That process hands the link to the instance of the app that runs already and ends, so the app that runs hears the link as [`linkOpened`](../lua-api/events.md#app-and-plugins), and a process without a running instance plays the app with the link. The shell of Windows reads a link as an address before it starts the app, so a link without a path gains a slash before its query, and `my-game://open?level=3` reaches the app as `my-game://open/?level=3`. An app therefore reads the parts of its links, such as the host and the query, instead of comparing their text.

| Platform | How the scheme reaches the system | How the link reaches the running app |
| --- | --- | --- |
| Windows | The installer registers each scheme under `Software\Classes` of the person, or of every user for an install for every user, with the command `"<app>.exe" --link "%1"`, and removes it with the app. The command `app run` registers the same keys under `HKEY_CURRENT_USER` for the installed app of the products folder, so links work while the app is being made. | The named pipe of the app in the session of the person, `\\.\pipe\haylen-links-<identifier>-<session>`, which the first instance of the app serves. |
| Linux | The desktop entry lists each scheme as `x-scheme-handler/<scheme>` in `MimeType` and starts `<app> --link %u`, which a package installs. The command `app run` writes the entry into `~/.local/share/applications` for the installed app of the products folder and makes the app the handler of each scheme with `xdg-mime`. | The method `Open` of `org.freedesktop.Application` on the session bus, under the name of the identifier of the app and its object path, such as `/com/haylen/runner`, which GIO reaches. Without GIO or a session bus every process keeps its own link. |

## Signing

Each platform signs its apps with keys that never live in the app repository or a generated project.

| Platform | Key | Where it comes from |
| --- | --- | --- |
| Apple devices | The development team of Xcode. | `platforms.apple.team`, or the variable `HAYLEN_APPLE_TEAM`. What runs on this Mac and on the simulators signs ad hoc. |
| Android | The upload key of release builds, and an optional debug key. | `platform/android/keystore/` of the app, which `haylen app sign` writes, or the Gradle properties and variables of continuous integration. |
| Windows | A code signing certificate. | The variables `HAYLEN_WINDOWS_CERTIFICATE` and its password, or `HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT`. |
| Linux | None of its own. | The repositories that publish `.deb` packages sign their indexes. |

### Android keys

```sh
haylen app sign ~/apps/my-game --target android
haylen app sign ~/apps/my-game --target android --kind debug
haylen app sign ~/apps/my-game --target android --option alias=store --option password="a long secret" --option name="CN=My Company, O=My Company, C=US"
```

The command `app sign` creates the upload key that signs the release builds of an app. With `--kind debug` it creates a debug key of the app instead, which signs its debug builds in place of the debug key of the Android SDK, so every machine of a team builds with the same signature. Its options are `--option` pairs:

| Option | Default | Meaning |
| --- | --- | --- |
| `alias` | `upload` | The alias of the key. |
| `password` | `upload` | The password of the keystore and of the key, at least 6 characters, which `keytool` asks of every keystore. |
| `name` | `CN=Upload, OU=Upload, O=Upload, L=Upload, ST=Upload, C=BR` | The distinguished name of the certificate. |
| `force` | none | The value `true` replaces a key that the app already has. |

The command runs `keytool -genkeypair` for an RSA key in a PKCS12 keystore, then exports the certificate as PEM, which is the file that a store asks for when it registers or resets an upload key. The command finds `keytool` in the JDK of `JAVA_HOME`, or else on `PATH`. The key goes into `platform/android/keystore/` of the app as `<kind>.jks`, `<kind>.pem` and `<kind>.properties`, and only the owner of the properties file, which holds the passwords, can read it. The command refuses to replace a key of the app without `--option force=true`, since an app signed with another key cannot update the installed one. The `.gitignore` of the app keeps the folder out of the repository.

The Android backend signs each build type with the key that `platform/android/keystore/<kind>.properties` describes, which names the keystore next to it, and hands the key to Gradle through the environment of the build:

```properties
storeFile=release.jks
storePassword=upload
keyAlias=upload
keyPassword=upload
```

Continuous integration, where the folder does not exist, gives a key through Gradle properties or environment variables instead:

| Gradle property | Environment variable | Value |
| --- | --- | --- |
| `haylen.release.storeFile` | `HAYLEN_RELEASE_STORE_FILE` | The keystore, as an absolute path or a path relative to the app folder. |
| `haylen.release.storePassword` | `HAYLEN_RELEASE_STORE_PASSWORD` | The password of the keystore. |
| `haylen.release.keyAlias` | `HAYLEN_RELEASE_KEY_ALIAS` | The alias of the key. |
| `haylen.release.keyPassword` | `HAYLEN_RELEASE_KEY_PASSWORD` | The password of the key. |

The debug key takes the same names with `debug`, such as `haylen.debug.storeFile` and `HAYLEN_DEBUG_STORE_FILE`. A key needs its four values. The properties file wins over the Gradle properties, and a Gradle property wins over its environment variable. The Gradle properties come from `gradle.properties` of the Gradle home, `GRADLE_USER_HOME`, since the generated project holds none of the developer. A release build without an upload key stops before Gradle runs, with a message that names both ways to add one. A debug build without a key of the app signs with the debug key of the Android SDK.

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

The command `base64 < platform/android/keystore/release.jks` prints the text of the `ANDROID_KEYSTORE` secret. The command `apksigner verify --print-certs` of the build tools of the Android SDK shows the certificate that signed an APK.

### Windows certificates

A Windows app without a signature makes SmartScreen warn the person who installs it. So the Release installs of the Windows project sign the executable with `signtool` of the Windows SDK, and the installer of CPack signs the installer and its uninstaller through the same script, `signing.cmake`. They use a SHA-256 digest and an RFC 3161 time stamp, which keeps the signature valid after the certificate expires. The certificate never belongs in the project or the repository, so the script reads it from the environment:

| Environment variable | Value |
| --- | --- |
| `HAYLEN_WINDOWS_CERTIFICATE` | A PFX file with the code signing certificate and its private key. |
| `HAYLEN_WINDOWS_CERTIFICATE_PASSWORD` | The password of the PFX file. |
| `HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT` | Instead of a file, the SHA-1 thumbprint of a certificate in the certificate store of the user, such as one on a hardware token, whose private key never leaves it. |
| `HAYLEN_WINDOWS_TIMESTAMP_URL` | The time stamp server of the authority that issued the certificate, which a signature needs. |
| `HAYLEN_WINDOWS_SIGNTOOL` | Another `signtool` than the one on `PATH` or in the newest Windows SDK. |

The project decides when CMake configures it. With `HAYLEN_WINDOWS_CERTIFICATE` or `HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT` it signs, and a signature that fails stops the install. Without either, the app stays unsigned, and the install of every Release build says so. A workflow of GitHub Actions keeps the PFX file as Base64 in a secret and writes it to a file before the build:

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
      ./haylen.cmd app package my-game --target windows --format installer
```

The guide [installers in CI](ci-installers.md) gives the whole workflow of every platform and explains each secret. The content keys of the app reach the workflow through `HAYLEN_KEYS_DIR`, as [the content guide](../content/content.md) describes, since every release of an app needs the keys of its earlier releases. The command `signtool verify /pa /v my-game-1.0.0-setup.exe` shows the signature of an installer.

## Release builds

The Release configuration builds what an app ships:

- The package, only as the protected release that the content tool of the desktop SDK builds into `build/apps/<app>-<hash>/release/<profile>/`, as the [content guide](../content/content.md) describes. The Lua modules become bytecode.
- A binary that compiles in the bootstrap of the app, `HaylenBootstrap.cpp`. The content tool writes it from the key folder of the app, and it seals the content keys, so no key sits in a file of the app.

A release follows the last release of its profile in the build folder, so a build after a small change writes only the shards of what changed. The option `--previous-release` of `app prepare`, `app build`, `app run` and `app package` makes the release of the build follow the one in another folder instead, such as the release that shipped last, which continuous integration keeps, and `content build --previous-release` builds such a release without the app. The Debug configuration ships the package as it is, for development.

```sh
haylen app build ~/apps/my-game --target linux --config Release
haylen app verify ~/apps/my-game --target linux --config Release
haylen app run ~/apps/my-game --target android --config Release
```

| Platform | Release build |
| --- | --- |
| Windows, Linux | Writes the release of the profile `windows` or `linux` and the bootstrap, which `haylen_add_app` compiles into the executable against the desktop SDK, with hidden symbols. Linux links it with `--gc-sections` and ships it stripped, with its symbols in a `.debug` file in the symbols folder of the build tree. Windows keeps the PDB of its public symbols there. The project installs the release in `app/` next to the executable, and neither symbol file. |
| Android | Writes the release of the profile `android` into `app/src/main/assets/app/` of the generated project, a flat folder that Android lists without an index and whose shards the app module keeps uncompressed in the APK, and the bootstrap into `native/`. The external native build of Gradle links the library of the app, `libhaylen-app.so`, with `haylen_add_app` for every ABI against the Android SDK with the bootstrap. The library exports only the entry points of Java and GameActivity and ships stripped, while its symbols stay in the native build trees for crash reports. The manifest names it, so the activity loads it and the Lua player of the `haylen` library stays out of the APK. The release build signs with the upload key of the app, and R8 shrinks the code and the resources of the APK. |
| Web | Places the release of the profile `web` in `app/` of the site, where the shards of the content stay and stream by range. It links the runtime of the app with Emscripten from the web SDK with the bootstrap. The site holds no `app.zip`, and `config.json` names the manifests and the shards of the app domain, which the page downloads before the app starts. The symbol map of the runtime stays in its build folder for crash reports and never reaches the site. Every other script of the site goes through the terser of the pinned Emscripten: the loader and the page scripts of the app keep their top-level names, and the web modules of the plugins minify as modules. |
| macOS, Mac Catalyst, iOS, tvOS, visionOS | Writes the release of the profile `apple` into `app/` of the project, which the bundle carries as its resource `app`, and the bootstrap, which the app target compiles into the app that links `Haylen.xcframework`. The Release configuration keeps the debug information in a `.dSYM` next to the app and strips the app as it builds, so the release that runs is the one that ships. The engine and its dependencies compile with hidden symbols on Apple platforms, so the app exports only the symbols of its own code. |

The verification of a Release build fails when the app holds what a release never may, so `app run` stops before it launches and `app verify` fails in continuous integration. A release never holds:

- A `.lua` file, or the `haylen-package-index.json` of a development package.
- A file of the package as it is, under any name. The splash logo is the one exception, since the system shows it before the engine starts.
- Other files in the release folder than its manifests and shards.
- Symbol files, such as `.pdb`, `.dSYM` or `.debug`.
- Any content key or the signing key of the app, as bytes, as hexadecimal text in either case or as Base64 text.

### Store limits

Stores limit the size of an app. Those limits belong to the stores and never to the format, which serves releases of any size, and content beyond them goes on a content server.

- The App Store accepts iOS and tvOS apps of at most 4 GB uncompressed. An Apple release build warns when its release is larger.
- Google Play downloads a base module of at most 500 MB, which also carries the code and the native libraries of the app. On top of it come install-time asset packs of at most 1.5 GB each, all of them together at most 4 GB. A release build warns when its release is larger than the 4 GB.

So Android releases build shards that aim for 256 MiB. An Android release larger than 384 MiB moves its content shards into install-time asset packs, which the preparation writes into `asset-packs/` of the generated project. Each pack holds at most 1 GiB, filled with the shards in the order of their names. The two manifests and the shards of the app domain, the code of the app, stay in the base module. A shard holds at most twice its target and one record. The content tool builds a shard of a previous release that passes that again, such as one of a release built for another profile, so every shard fits its pack.

The generated `settings.gradle.kts` includes the packs, and the `assetPacks` of the app module name them. Google Play installs them with the app, and Android serves their files through the same assets as the base module. So the runtime reads the release from `app/`, whichever module holds a shard. The build of such a release makes the universal APK of the bundle with the asset packs, which `app run` installs on the device. The verification of a Release build fails when the built app lacks a shard of its release.

### Packs that stores deliver

The named packs of `content.delivery` become asset packs of Google Play when a plugin of the app delivers them. Such a plugin, such as one that brings Play Asset Delivery, gives the contribution `assetPacks` for `android` in its `plugin.json`:

- The preparation writes an asset pack for every named pack into `asset-packs/` of the generated project, with the shards of the pack at the root of its assets.
- The pack takes the name of the named pack, with every dash and dot as an underscore, and with `pack_` in front of a name that starts with a digit.
- Packs that the plugin lists in `fastFollow` deliver `fast-follow`, and the others `on-demand`.
- A pack larger than the 1.5 GB of a Play asset pack, two packs with the same Play name, or more than 100 asset packs in all stop the build.

Without such a plugin, the named packs stay for the content server. The build of such a release makes the bundle next to the APK. The command `app run`, and `app package` with the format `apks`, install it with bundletool in the local testing mode of Google Play, signed with the upload key of the app, so the library of Play delivers each pack on the device as the store would. The format `bundle` of `app package` copies the bundle for the upload to Google Play. The verification of the Release build fails when the bundle lacks a shard of a pack.

On iOS, Mac Catalyst, tvOS, visionOS and macOS the named packs become asset packs of Background Assets, which the App Store hosts. That happens when a plugin of the app, such as one that brings Background Assets, gives the contribution `assetPacks` for the Apple platforms:

- A Release build packs each named pack with `xcrun ba-package` into `products/asset-packs/<pack>.aar` of the product, with the shards of the pack in a folder named after it.
- Packs that the plugin lists in `prefetch` download with the install and every update, and the others when the app asks for them.
- The developer uploads those archives to App Store Connect with the app.
- Only one plugin of the app may deliver packs, since the type `assetPacks` takes a single owner.

The plugin adds the downloader extension that Background Assets runs, the App Group of the app and its extension, and the keys of the `Info.plist` that hosting by Apple needs. The mock server of Xcode, `xcrun ba-serve`, serves the archives to a simulator or a device for testing, as the README of the plugin describes.

## Packages

```sh
haylen app package ~/apps/my-game --target windows --format installer
haylen app package ~/apps/my-game --target linux --format installer -o ~/releases/my-game
haylen app package ~/apps/my-game --target android --format bundle
haylen app package ~/apps/my-game --target android --format apks --option device=emulator-5554
```

The command `app package` builds an app in the Release configuration, verifies it and packages it in a format of the backend of its target, into `dist/` of the app folder, which its `.gitignore` keeps out of the repository, or into the folder of `-o`. The formats belong to the backends:

| Target | Format | Package |
| --- | --- | --- |
| `windows` | `installer` | `<executable>-<version>-setup.exe`, an Inno Setup installer that installs the app for the person or, with an administrator, for every user. It adds a shortcut in the Start menu, an optional one on the desktop, the option to run the app at the end, and an uninstaller in the settings of Windows. The tooling installs the pinned Inno Setup into `.tools` on the first package. |
| `linux` | `installer` | `<executable>_<version>_<arch>.deb`, which installs into `/usr` with the dependencies that `dpkg-shlibdeps` finds in the executable, and GIO and the sound servers as recommendations. And `<executable>-<version>-<arch>.AppImage`, one file that runs on any distribution without installing, with the AppImage tools that the tooling downloads into `.tools`. |
| `windows`, `linux` | `steam` | The depots and build scripts of SteamPipe, the upload system of Steam, as the next section describes. |
| `android` | `bundle` | The Android App Bundle for the upload to Google Play, with its asset packs. |
| `android` | `apks` | The APK set of a connected device, built from the bundle in the local testing mode of Google Play and installed on the device that the option `device` names. |

Inno Setup makes the Windows installer for several reasons. It is free, it has had regular releases for more than two decades, it compiles a script from the command line, which CPack writes, and it signs the installer and its uninstaller through the signing command it receives. MSI packages suit software that companies deploy to many machines, while an app for people needs a plain installer.

The `.deb` reaches Debian, Ubuntu and the distributions based on them through their package manager, which resolves the libraries of the system that the app links. The AppImage serves every other distribution with one file that needs no installation, no permission of the administrator and no runtime. The packages link the C library and the graphics libraries of the system that builds them. So they run on distributions as new as that one or newer, and a release that reaches older ones builds on the oldest distribution it supports.

The Apple backend has no package format. An archive for App Store Connect comes from Xcode, from the `App.xcodeproj` that a Release preparation generates in the build folder of the product.

### Steam

```sh
haylen app package ~/apps/my-game --target windows --format steam --option appId=480 --option depots=windows=481 --option depots=forest=482
```

The format `steam` lays a Release build of a Windows or Linux app out for SteamPipe. Steam installs the app with its release, so the app needs no content server. Its options are `--option` pairs:

| Option | Meaning |
| --- | --- |
| `appId` | The numeric ID of the app on Steam. |
| `depots` | The ID of a depot from Steamworks as `<name>=<id>`, which repeats: the platform is one name, and each pack of the app is another. |
| `branch` | The beta branch that the build goes live on, such as `beta`. Without it, the build waits in Steamworks to be set live. |

| Folder | What it holds |
| --- | --- |
| `content/<platform>/` | The depot of the app: the installed Windows or Linux app with its release. It holds every shard of the release except those of the [packs](../content/content.md#delivery) of the app, or only the shards that its required content needs when `haylen.json` names a content server. |
| `content/<pack>/` | The depot of each pack. Its shards install into `packs/` next to the executable, in `lib/<executable>/` on Linux, where the runtime finds them. A player who does not own a pack, such as an expansion sold apart, never receives its depot. Its assets stay unavailable, and `assets.loadAsync` of one fails with `storeAssetUnavailable`, which an app shows as content to buy. |
| `scripts/` | The build scripts of `steamcmd`: `app-build.vdf` with the app ID, a description of the app and its version, and the branch, and one `depot-build-<id>.vdf` per depot that maps its whole folder. |

The depots of each platform are staged on that platform, such as in one job of continuous integration per platform. Steam compares each depot with the build before it, file by file. A release keeps every unchanged shard byte for byte under the same name, so an update of a few assets uploads and downloads only the shards that changed and the two manifests. A machine without the release before it in its build folder, such as continuous integration, downloads the depots of the live build first, with `steamcmd +login <account> +app_update <app id> validate +quit` or the command `download_depot` of the Steam console, and names that folder with `--previous-release` of `app package`, which finds the release and the shards of its packs in it.

The command `steamcmd +login <account> +run_app_build <output>/scripts/app-build.vdf +quit` uploads the build. The Steamworks API of the app itself, such as achievements and the ownership of packs, is a native library that the app calls through `haylen.native`, as [native libraries](#native-libraries) describe.

## Privacy manifest

Apps for Apple platforms declare the APIs with required reasons that their code calls. The engine is part of that code, since `Haylen.xcframework` is a static library. The file `engine/platform/apple/PrivacyInfo.xcprivacy` holds the declarations of the engine, which the Apple SDK carries next to its frameworks.

The Apple backend writes `PrivacyInfo.xcprivacy` of the generated project from three sources: the manifest of the engine, the `privacy` contributions of the plugins and the `privacy` key of the `apple` section of `haylen.json`. The reasons of every API type join into one entry of that type, as the App Store expects. The tracking domains and the collected data types gain the items they lack, and two owners that disagree on tracking stop the preparation with both named.

The undefined symbols of `libhaylen.a` show which of those APIs the engine and its dependencies reach:

| Category | Reason | Callers |
| --- | --- | --- |
| File timestamp, `NSPrivacyAccessedAPICategoryFileTimestamp` | `C617.1`: the metadata of files inside the container of the app. | `stat`, `fstat` and `lstat` from the file layer of libuv, whose modification time Varn's `fs.stat` reports, and from Poco, OpenSSL, libzip, pugixml, miniaudio, SDL, the log backend and the content store of the engine. |
| System boot time, `NSPrivacyAccessedAPICategorySystemBootTime` | `35F9.1`: time between events of the app and timers. | `mach_absolute_time` from the clock of SDL, the timers of libuv and the audio of miniaudio. |
| Disk space, `NSPrivacyAccessedAPICategoryDiskSpace` | `E174.1`: whether there is room to write files. | `statfs` from the file systems of libuv and Poco, which report the free space of a disk. |

The engine reads no user defaults, no active keyboards and no system boot time through `systemUptime`. An app that uses these APIs for other reasons adds them in `platforms.apple.privacy`, such as the file timestamps of documents that the person picks with a dialog, `3B52.1`.

## Native libraries

The `native` section of `haylen.json` lists the native libraries that an app ships, by the name that `native.load` takes. Each one is prebuilt files for each platform, or a CMake project that the backend of each platform it lists builds. The [native code guide](../platform/native.md#packaging-libraries-with-an-app) describes the section. The `nativeLibrary` contribution of a plugin adds one more library, named after the plugin id, which the backends build and place in the same way. A library ships only to the platforms it names, so a library for iPhones and iPads that also serves Mac Catalyst and visionOS lists `catalyst` and `visionos` too.

```json
{
    "native": {
        "steam_api": {"files": {"macos": "native/macos/libsteam_api.dylib", "windows": "native/windows/steam_api64.dll", "linux": "native/linux/libsteam_api.so"}},
        "my-glue": {"cmake": "native/glue", "platforms": ["macos", "ios", "android", "windows", "linux"]}
    }
}
```

A CMake library builds against the CMake package of the SDK of the target, whose target `haylen::native` gives it `HaylenNative.h` and nlohmann/json:

| Platform | Place |
| --- | --- |
| The player | The folder in `build/native/` of the product, which the player searches first through `--native`. |
| macOS, Mac Catalyst | The backend builds a CMake library for every architecture of the product and joins them with `lipo`, or takes the file or the slice of an `.xcframework`, and the app embeds it in `Contents/Frameworks`, signed with the identity of the app. |
| iOS, tvOS, visionOS | The same, into `Frameworks`, where a dynamic library becomes a framework with its own `Info.plist`. A static library links into the app, and `HaylenNativeSymbols.mm` keeps and registers the symbols that the entry lists. |
| Android | Prebuilt files go into `app/src/main/jniLibs/<abi>/`, and the external native build of Gradle builds a CMake library for every ABI. |
| Windows | The folder of the native libraries of the product, which the project installs next to the executable. |
| Linux | The same, installed into `lib/` next to the executable, whose `RUNPATH` is `$ORIGIN:$ORIGIN/lib`. |
| Web | Nothing, because the browser loads no native libraries. |

## Engine SDKs of each platform

Apps build against the engine SDK of their target, which `haylen sdk build` installs into `build/sdk/<port>-<configuration>/` with its manifest `haylen-sdk.json`, as the [build guide](build.md) describes. This section says what each SDK carries for apps.

### Haylen.xcframework

The command `haylen sdk build` with the Apple targets builds the Apple SDK into `build/sdk/apple-<config>/`. It holds two frameworks with one static library per slice. The library `libhaylen.a` of `Haylen.xcframework` merges, with `libtool`, the engine, every dependency (Varn, Lua, Poco, OpenSSL, libuv, Box2D, ImGui, miniaudio and the rest) and the runtime, next to the public headers. The library `libhaylen-player.a` of `HaylenPlayer.xcframework` holds the Lua player alone, which defines `haylen::core::Application::create` for Lua apps, so Lua apps link both frameworks and C++ apps, which define the function in their own sources, link only `Haylen.xcframework`. The SDK is built in these steps:

1. Every architecture is a CMake build of `engine/` with `HAYLEN_BUILD_SDK` and `HAYLEN_BUILD_FRAMEWORK`, in `build/engine/apple-<slice>-<arch>-<config>`.
2. The script `haylen-install.cmake` merges the engine and the runtime, as it does for the [SDK](embedding.md#find_package-and-the-sdk).
3. The script `haylen-framework.cmake` merges both into `libhaylen.a` and archives the player into `libhaylen-player.a`. It writes `haylen-apple.json` with the lists of system frameworks in `haylen-app.cmake`, the same lists that `haylen_link_runtime_platform` links into CMake apps, and the preprocessor definitions that C++ sources compile with against the headers.
4. The tool `lipo` joins the architectures of a slice, and `xcodebuild -create-xcframework` joins the slices of each framework. A slice that the SDK folder holds from the same engine sources and configuration stays, so building one slice later adds it.
5. The tool `codesign` signs both frameworks ad hoc with `--sign -` and `--timestamp=none`, and `codesign --verify --strict` checks the signatures, so Xcode takes them as signed and a broken signature stops the build of the SDK.
6. The engine port copies `haylen-apple.json`, the privacy manifest of the engine, its Swift helpers, the headers and the CMake package that gives native libraries `haylen::native` into the SDK folder, and lists them in `haylen-sdk.json`.

| Slice | Architectures | Built with |
| --- | --- | --- |
| `macos` | arm64, x86_64 | The macOS SDK. |
| `ios` | arm64 | `CMAKE_SYSTEM_NAME=iOS` and the `iphoneos` SDK. |
| `ios-simulator` | arm64, x86_64 | `CMAKE_SYSTEM_NAME=iOS` and the `iphonesimulator` SDK. |
| `ios-maccatalyst` | arm64, x86_64 | `engine/cmake/haylen-catalyst.toolchain.cmake`, which compiles for the `macabi` flavor of iOS against the macOS SDK and its `iOSSupport` folder, because CMake has no system name for Mac Catalyst. The engine treats the build as iOS. |
| `tvos` | arm64 | `CMAKE_SYSTEM_NAME=tvOS` and the `appletvos` SDK. |
| `tvos-simulator` | arm64, x86_64 | `CMAKE_SYSTEM_NAME=tvOS` and the `appletvsimulator` SDK. |
| `xros` | arm64 | `CMAKE_SYSTEM_NAME=visionOS` and the `xros` SDK. |
| `xros-simulator` | arm64 | `CMAKE_SYSTEM_NAME=visionOS` and the `xrsimulator` SDK. The visionOS build of OpenSSL in Varn names no architecture and always comes out arm64, so the slice has no x86_64, the target `visionos-simulator` builds arm64 alone and Intel Macs run no visionOS simulator. |

The slices target the oldest systems that the code of the engine allows, which each slice records in `haylen-sdk.json` as the floor of the apps that link it:

- iOS and tvOS 16.3, and Mac Catalyst 16.4, which is macOS 13.3 on the Mac. Those releases have a C++ library whose `std::format` formats floating point numbers, which the engine uses for text and logs. The toolchain itself reaches back to iOS and tvOS 15 and macOS 12, and a build for iOS 15 fails only on `std::format` with floating point.
- visionOS 1.0, its first release, whose C++ library already formats floating point numbers with `std::format`.
- macOS 13.3, for the same `std::format` of its C++ library. SDL runs from macOS 10.13 on, and the Metal device from macOS 10.15 on, where Metal sorts GPUs into the families that it asks about, so neither sets the minimum.
- Mac Catalyst starts at 16.4 rather than 16.3, because Xcode maps iOS 16.3 to macOS 13.2.
- The engine parses floating point numbers with fast_float, because `std::from_chars` for floating point needs iOS, tvOS and macOS 26.

The engine port passes the versions as `CMAKE_OSX_DEPLOYMENT_TARGET`, and the Apple projects of apps take the floor of their slice as their deployment target when the app sets no newer one.

The SDL platform layer enters the main callbacks of SDL itself, so the runtime never defines `main` on Apple platforms. The app calls `haylen_main` instead, which `haylen/platform/apple/HaylenMain.h` declares and the `main.m` of the generated project calls:

```objc
#import "haylen/platform/apple/HaylenMain.h"

int main(int argc, char* argv[]) {
    return haylen_main(argc, argv);
}
```

When the system ends the app with `SIGTERM`, such as when the Simulator quits while the app runs, Xcode stops the app at `return haylen_main(argc, argv);` with "signal SIGTERM". That is the debugger, which stops on every signal before the app receives it, and not a crash. The frames of UIKit and AppKit run inside `haylen_main`, so the debugger shows that line. Continue in Xcode, and the app quits the way it quits when the system asks it to: SDL turns the signal into a request to quit, the app hears `appQuitRequested`, and the engine stops and saves what it keeps. A run without the debugger never stops there. To keep the debugger running on the signal, type `process handle SIGTERM --stop false --notify true` in the console of the debugger, or put that line in the `~/.lldbinit` file.

The bridge handlers of an app register before `main` runs, when their classes load, such as `AppBridgeLoader` of the starter app, which calls `[AppBridge registerHandlers]` in `+load`. Native plugins register theirs through their context, as the [plugin guide](../platform/plugins.md#the-apple-part) describes. Plain macOS executables of CMake projects, and the macOS desktop player, get the same `main` from `engine/src/platform/apple/AppleMain.cpp`.

### The Android libraries

The folder `engine/platform/android` is a Gradle project with five modules. The module `haylen` is the Android library (AAR) with the namespace `dev.haylen`. It holds:

- `HaylenActivity`, a `GameActivity` of the AndroidX games libraries and so an `AppCompatActivity`. It draws the app in a `SurfaceView` of an ordinary view hierarchy and installs the splash screen. It hides the system bars, apart from the status bar that the app shows, and reports the safe area, the on-screen keyboard and the orientation of the screen. It forwards low-memory warnings. It takes the back button through an `OnBackPressedCallback` while the app captures it, and hands its events to the plugins.
- `HaylenLinkActivity`, which hands the links and notifications of the app to the activity.
- `HaylenEditText`, the hidden field that edits the focused text field of the UI.
- `HaylenAudioFocus`, which holds the audio focus in the foreground and while the app plays in the background, `HaylenNetwork`, which follows the network while the app holds the permission `ACCESS_NETWORK_STATE`, `HaylenBattery`, `HaylenPowerSaving` and `HaylenTimeZone`, which follow the battery, the battery saver and the time zone, and `HaylenAccessibility`, which follows the accessibility settings.
- `HaylenMediaSession`, the now-playing controls with their media notification, and `HaylenPlaybackService`, the foreground service that keeps an app that plays in the background in the foreground while it plays.
- `HaylenAccessibilityTree`, which serves the elements of the UI to TalkBack and the other accessibility services.
- `HaylenDisplays`, which follows the displays of the device, `HaylenOutputs`, which shows the outputs of the app on the other displays, and `HaylenFold`, which reports the fold of foldable and dual-screen devices through Jetpack WindowManager.
- `HaylenPermissions` with `HaylenPermission`, which read and ask for the permissions of `system.permission` through one launcher of the Activity Result API.
- `HaylenBridge` for the platform bridge, and the host of the Android parts of [plugins](../platform/plugins.md#the-android-part): `HaylenPluginProvider`, which loads them when the process starts, `HaylenPlugin`, `HaylenPluginContext`, `HaylenRequirements`, `HaylenOverlay` with its `HaylenPlacement`, the screens of plugins with `HaylenScreens` and `HaylenScreen`, and the streams `HaylenVideoStream` and `HaylenAudioStream`.
- The splash themes, the R8 rules for the classes that C++ calls by name, the Kotlin HTTP transport of Varn and the Java class of its data views.

The manifest of `haylen` declares only the OpenGL ES 3 requirement, with no permission and no component. So the permissions, the back policy and the components that only some apps want never reach an app through it. The other modules each depend on `haylen` as an API:

- `haylen-plugins` holds only a manifest, which declares `HaylenPluginProvider` for the modules of plugins.
- `haylen-links` holds only a manifest, which declares the exported `HaylenLinkActivity` for plugins of links and notifications.
- `haylen-playback` holds only a manifest, which declares `HaylenPlaybackService` with the type `mediaPlayback` and the permissions `FOREGROUND_SERVICE` and `FOREGROUND_SERVICE_MEDIA_PLAYBACK`, for the apps whose `haylen.json` sets `audio.background`, as the [audio guide](../audio/audio.md#playing-in-the-background) describes. The Android backend adds it to the dependencies of the app module of those apps.
- `haylen-coroutines` holds `HaylenCoroutines`, with `kotlinx-coroutines-android`, as the [plugin guide](../platform/plugins.md#engine-libraries) describes.

The library packages the player, which the CMake project of the engine builds with `HAYLEN_BUILD_PLAYER` as the shared library `libhaylen.so`. It covers arm64-v8a, armeabi-v7a, for the 32-bit Android TV devices still in use, and x86_64, for emulators, from Android 8.1 (API 27) on. The NDK r30 links the arm64-v8a and x86_64 libraries with 16 KB page alignment, which Android 15 devices with 16 KB pages need, and `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES` makes it explicit. 32-bit ARM has no 16 KB pages and keeps 4 KB alignment.

The Android library needs API 27 because miniaudio plays through AAudio from Android 8.1 on, as the [build guide](build.md#dependencies) explains. The others need less: GameActivity needs API 21, AppCompat and the activity library need API 23, and the HTTP transport of Varn needs API 24. `HaylenActivity` reaches the system bars through `WindowCompat` and `WindowInsetsControllerCompat` of `androidx.core`, and covers display cutouts from Android 9 on, where cutouts exist. Back reaches `HaylenActivity` only through the `OnBackPressedCallback` that it enables while the app takes back or edits a text field. Android 13 and later call it through the back callbacks of the window, which the generated manifest turns on with `android:enableOnBackInvokedCallback`. So the system plays its predictive back animation when back leaves the app.

The command `haylen sdk build --target android` builds the Android SDK into `build/sdk/android-<config>/` with the engine port of Android, in these steps:

1. It builds the player and the SDK of the engine for every ABI, or for the ones that `--architecture` names, in `build/engine/android-<abi>-<config>`, one ABI after the other with the jobs of `--jobs`. So the compilers stay within those jobs, and the configures never write into the shared CPM sources at the same time.
2. It installs the CMake package of each ABI into `abis/<abi>/` of the SDK, which the native code of apps links against, and copies the player of each ABI into `jniLibs/<abi>/libhaylen.so`. The ABIs that the SDK folder holds from the same engine sources and configuration stay, so building one ABI later adds it to the SDK.
3. It runs Gradle with `-PhaylenNativeLibraries` for the players of every ABI of the SDK, `-PhaylenVarnSourceDir` for the folder where CPM placed Varn, `-PhaylenMavenDir` for the Maven repository and `-PhaylenBuildDirectory` for the build outputs, which go to `build/engine/android-libraries-<config>`. The tasks `copyVarnTransport` and `copyVarnDataView` copy `VarnHttp.kt` and `VarnDataView.java` of Varn into the module, because the engine links Varn into the player library and only the Kotlin HTTP transport and the data views of the Android library of Varn run in the app.
4. It runs the task `publishReleasePublicationToArtifactsRepository` of every module. That publishes `dev.haylen:haylen`, `dev.haylen:haylen-plugins`, `dev.haylen:haylen-links`, `dev.haylen:haylen-playback` and `dev.haylen:haylen-coroutines` at the engine version, with their POMs, to `maven/` of the SDK.
5. It writes `haylen-sdk.json` with one slice per ABI, which names the CMake package and the player of the ABI, and the Maven repository that every slice shares.

The POM of `haylen` lists the dependencies of the library, which Gradle brings into every app:

- The Kotlin standard library.
- `androidx.games:games-activity` 4.4.2, which the Gradle module metadata pins strictly. The Java classes of GameActivity must match its native side in `libhaylen.so`, which links the same version that `engine/cmake/haylen-dependencies.cmake` pins. A change of version updates the pin, the library and the template together.
- `androidx.appcompat:appcompat` 1.8.0, `androidx.activity:activity` 1.13.0 and `androidx.core:core` 1.19.1, as APIs, which `HaylenActivity` and its plugins build on.
- `androidx.core:core-splashscreen`, `androidx.customview:customview`, whose `ExploreByTouchHelper` serves the elements of the UI to accessibility services, and `androidx.window:window-java`, the Jetpack WindowManager that reports the fold of foldable devices.

The POMs of the other libraries list `haylen`. The POM of `haylen-coroutines` also lists `kotlinx-coroutines-android`, which apps and plugins with suspending handlers get as an API.

### Web and desktop

The web SDK holds the `haylen` web player, which carries the devices of WebGPU and WebGL2 and draws through WebGPU, or through WebGL2 where WebGPU does not start. It sits next to `haylen-audio-worklet.js`, the AudioWorklet processor that the runtime loads from the folder of its script, `haylen-content-store.js`, the worker of its content store, and `haylen-page.js` and `haylen-worker.js`, the page side and the worker of the Worker mode, which every site of the web backend carries too, so a page whose address holds `hosting=worker` runs the app in a dedicated worker. The Emscripten CMake package of the SDK links the runtime of a release and of a C++ app. The player embeds no app. In development the page hands it the package at runtime, so the same `haylen.wasm` runs every app.

The desktop SDK of this machine holds three things, which one build tree of the engine builds:

- The `haylen` player of this machine, which runs any package that its command line names, as the [build guide](build.md#the-desktop-player) describes, and which the `player` target uses.
- The content tool `haylen-content`, which builds the protected releases of apps, as the [content guide](../content/content.md#the-content-tool) describes.
- The CMake package of the engine, which the Windows and Linux projects of apps and the native libraries of the desktop link.

## Templates

The tooling keeps two kinds of templates:

- The app templates of `templates/`, which `haylen new` copies once into a new app: `templates/common` with `haylen.json`, the logo, the bridge handlers of every platform under `native/` and the `.gitignore`, `.editorconfig` and `.clang-format` of an app repository, then `templates/lua` with `source/main.lua` or `templates/cpp` with `src/App.cpp`, whose files replace a common file of the same path. The template of plugins, `templates/plugin`, is what `haylen plugin new` copies.
- The backend templates, which live with their backend under `tools/haylen/platforms/<backend>/template/` and are inputs of every preparation: the Info.plist files, launch screens, asset catalogs, `main.m` and bridging header of the Apple apps, the Gradle project, asset pack module and native CMake project of Android, the page files of the web, and the CMake projects with their resources of Windows and Linux. An app replaces one of their files through `files`, and never edits them.

A new app holds no platform project. The command `new` writes the name, the identifier, the category and the orientation into `haylen.json`, gives the window the name as its title, names the application class of Android after the identifier and, for `portrait`, turns the window and the design resolution upright. A C++ app also gets `"kind": "cpp"` and `"cpp": {"sources": ["src"]}`.

### The bridge example

Both kinds of app ship a working call of the [platform bridge](../platform/platform-bridge.md). The app calls the method `app.greet` with its kind, `Lua` or `C++`, as the name. The native code of the platform answers with a greeting that names its language. It sends the event `app.greeted` with the count of greetings so far, which the app shows next to the answer.

| Platform | Handler | Registered by |
| --- | --- | --- |
| macOS, iOS, iPadOS, Mac Catalyst, tvOS, visionOS | Swift, in `native/apple/AppBridge.swift`. | `AppBridgeLoader` of `native/apple/AppBridgeLoader.m`, in `+load`, through `platforms.apple.sources`. |
| Android | Kotlin, in `onCreate` of `App` in `native/android/App.kt`. | The application class that `platforms.android.applicationClass` names, through `platforms.android.sources`. |
| Web | JavaScript, in `Module.preRun` of `native/web/app.js`. | The page, which loads it after the loader through `platforms.web.scripts`. |
| Windows, Linux | No native handler in a Lua app, so the call fails with the code `noHandler` and the app shows its message. A C++ app answers in `registerDesktopHandlers` of `src/App.cpp`. | The engine handler of the C++ app. |

The Lua side is `Home:greet` of `source/main.lua`. It listens with `platform.on` and calls with `platform.call` inside `scene.spawn`, both owned by the scene. The C++ side is `HomeScene::enter` of `src/App.cpp`, which listens with `Bridge::on` and calls with `Bridge::call`. The desktop player holds no native code of the app, so there the call fails with `noHandler` too.

To replace the example with real code, rename the method and the event in the app and in each handler, change what they do and add more handlers next to them, with the APIs that the [platform bridge guide](../platform/platform-bridge.md) describes for each language. An app that needs no native code deletes the handlers, the call and the keys of `platforms` that name them.

## Splash screens

The object `splash` of `haylen.json` sets the launch screen of every platform:

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

The runtime ends the launch screen once the app drew its first complete frame and the `duration` passed since the runtime started. It ends it as the next frame starts, so the GPU had a whole frame to put the complete one on the screen. The platform then fades it out over `fadeOut` seconds. A frame is complete when it shows the app as it is meant to look: the first scene has loaded, or its loading view shows, every picture of the UI has arrived, and every GUI drew once before, so the code that places the world around the nodes of a GUI has their bounds. The error screen and the update screen of content are complete too. So the splash goes straight to the first real frame of the app, with no empty frame and no picture that pops in after it. An app changes the rest through the `files` of its sections:

- The `LaunchScreen.storyboard` of the iOS and tvOS apps, which a storyboard of the app replaces through the `files` of the `ios` or `tvos` section.
- The theme `Theme.Haylen.Splash` that the generated Android manifest names, which a resource file of the app replaces through the `files` of the `android` section.
- The page and `loader.css` of the web site, through the `files` of the `web` section.

The app icons live in the asset catalog `Assets.xcassets` of the Apple apps, which an asset catalog of the app replaces through the same `files`, and in the launcher resources of the Android project.

| Platform | Launch screen |
| --- | --- |
| iOS, iPadOS, Mac Catalyst | `LaunchScreen.storyboard`: the image `splash-logo` centered in the safe area over the color `splash-background`, both from the asset catalog. Auto Layout keeps the logo square, at most 200 points and at most half of the safe area in each direction, so it fits every device, orientation and iPad window size. The system takes the launch screen away as soon as the window of the app shows. So `AppleSplash` covers the view of the app with the same storyboard, which `UILaunchStoryboardName` of the `Info.plist` names, until the runtime ends the splash and fades it out. The cover lies inside that view, because UIKit puts the view of the root view controller above every other view of the window when the window first shows. |
| tvOS | `LaunchScreen.storyboard` with the same layout and a logo of at most 360 points, kept over the view of the app the same way. |
| visionOS | None. The system opens the window of the app without a launch screen, so the splash of `haylen.json` does not show there. |
| Android | The SplashScreen API of `androidx.core:core-splashscreen`, with the theme `Theme.Haylen.Splash` of the manifest showing `haylen_splash_icon` over `haylen_splash_background`, then `Theme.Haylen`, whose window background has the same color. The system splash screen ends with the first frame of the activity window, while the Android runtime draws only once the activity resumed and its surface exists. So `HaylenSplash` covers the surface of the app from that first frame with a view of the same background and icon. It stays until the runtime ends the splash through `nativeFramePresented`, and then fades out over the seconds of `nativeFadeOut`. No black frame shows in between. The icon insets the logo so it fits the circle that Android masks it with, in every orientation, on phones, tablets and TVs. |
| Web | The page shows the logo over the background with the progress bar and its percentage, until the runtime ends the splash through `Module.haylen.onSplashEnded`, which fades it out. |
| macOS, Windows, Linux | None. The window opens hidden and shows with the first complete frame, so it never shows empty. X11 keeps nothing that the app drew into a hidden window and paints a window with its background as it shows it, so the window has the `background` of the splash as its background, which shows for the moment before the first frame reaches it. A Wayland session without X11 takes the frames of a window only once the window shows, so the window shows at once there and appears with the first frame the app draws. |

Each backend writes the splash into its generated project:

- The Apple backend writes the logo into the image set `splash-logo` of `Splash.xcassets`, and the background into its color set `splash-background`, with the dark background as its dark appearance. When the app names no logo, the image set holds the SVG of the Haylen symbol of the template, with its vector data preserved.
- The Android backend writes the background into the values resources of the app module and the dark background into its night resources, and copies the logo as `haylen_splash_logo`. These replace the defaults of the library, the Haylen symbol as a vector drawable over `#FF101418`. Android splash logos are PNG, WebP or JPEG images.
- The web backend copies the logo, or the Haylen symbol, next to the files of the page as `splash.<ext>`, and writes both backgrounds into `config.json`.

## Development mode

Development mode is [hot reload](../core/lua.md#hot-reload): the running app reloads the files saved in its folder. The runtime turns it on only when its command line asks for it. The command line of the runtime takes these options:

| Option | Meaning |
| --- | --- |
| `--dev` | Runs in development, watching the package folder. |
| `--dev-server <address>` | Runs in development and connects to the development server at that WebSocket address, which pushes the saved files. The player then takes the saved files from the server alone and watches no folder, so a save never reloads twice. |
| `--native <folder>` | Adds a folder that `native.load` searches before the folders of the platform. It may repeat. |
| `--link <link>` | The link that opened the app. The process hands it to the instance of the app that runs already and ends, or plays the app with it, as [links on Windows and Linux](#links-on-windows-and-linux) describe. |
| `--graphics-api <name>` | The graphics API that the window tries first, such as `vulkan`, before the other APIs of the build. It replaces the `window.graphicsApis` of `haylen.json`, and the web page passes the API that its address names the same way. A name that is not a graphics API fails the app, whose error screen, log and page show the reason, and an API that the build lacks logs a warning. |
| The first argument that is not an option | The package to play, a folder or a zip. Without one, the runtime plays the package bundled with the app. A release build ignores it with a warning, since it plays only its own release. |

Other options are ignored, with the value that follows an option of one dash, because systems add their own, such as the ones that Xcode passes to the macOS apps it launches. Android starts apps without a command line. So a debuggable Android app takes the address of the development server from the extra `dev.haylen.developmentServer` of the intent that launched it, and an app that is not debuggable never reads it. A release build, which carries the bootstrap of its app, ignores `--dev` and `--dev-server` with a warning, since a protected release never runs in development. A development server outside the device is on the local network, which an app that targets Android 17 reaches only once the person grants it. So the development session asks with the prompt of the system before it connects, and logs why it does not connect when the person refuses. The address that `adb reverse` forwards belongs to the device itself and needs no permission.

Among the commands of the tooling, only these turn development on:

- `app run` in the Debug configuration, on any target, starts a development server and launches the app connected to it, or serves a web page that turns on development through `Module.haylen.development`.
- `app serve` serves the package of a Lua app with a development server to players on other devices of the local network, which download it and play it with [`haylen.launch`](../lua-api/haylen.md#haylenlaunchpath-options) and its option `developmentServer`.

So apps launched by hand, shipped apps, the sites that `app build` makes and the Release configuration never run in development mode.

### The development server

The development server of a run listens on the port of `--port`, 8000 by default, at `/haylen/development`, and the app connects with a random token of the run, without which the server refuses it. A web page reaches it on the same port as the site, through the token that `config.json` hands the page. The server works like this:

- It scans the package of the app and the folders of its plugins twice a second with the rules of the player. It leaves out hidden files, backup files and files under `source/` that are not Lua, and waits for the next scan with files that are still being written.
- It compiles the shaders whose sources changed.
- It sends every batch of saved files to every connected app as one JSON message, followed by the bytes of each file. An app reloads them in place, and the terminal prints what each app did, such as `The app reloaded "source/scenes/level.lua" in 4 ms.`
- An app that connects or reconnects later gets the files that changed since the package that its build shipped, so it always plays the files on disk.
- A file larger than 16 MiB stays out, with an error that names it. A shader that fails to compile prints its error in the terminal and in the apps, which keep the last good shader.
- It listens on the port of `--debugger-port`, 4711 by default, for one editor that attaches its debugger, and carries the messages of the Debug Adapter Protocol between the editor and the app that connected last, as [Debugger](debugger.md) explains.
- It writes its address and the token of the run into `development.json` of the build folder of the app, readable by its owner alone, through which `app capture` of another terminal reaches it, and removes it when the run ends.

| Target | How the app reaches the server |
| --- | --- |
| `android` | `adb reverse` forwards the port of the device, emulator or phone to this machine, so the app connects to `127.0.0.1`. The address travels as the extra `dev.haylen.developmentServer` of the launch intent, and the forward goes away when the run ends. |
| `player`, `macos`, `catalyst`, the simulators, `windows`, `linux` | The simulator and this machine share the loopback address of this machine. The address follows the arguments of `xcrun simctl launch` or of the executable. |
| `ios`, `tvos`, `visionos` | The device reaches this machine over the network. So the server listens on every address of this machine, or on the one that `--host` names, and the app connects to the address of this machine on the network, which `xcrun devicectl device process launch` passes. The device and this machine must share a network, and the first connection asks the person on the device for access to the local network. |

These launches start the player of macOS and the apps of Apple platforms with the environment variable `MTL_CAPTURE_ENABLED` set to 1, which turns on the frame capture of Metal for [GPU captures](../lua-api/debug.md#gpu-captures): a simulator takes it as `SIMCTL_CHILD_MTL_CAPTURE_ENABLED` and a device through `--environment-variables` of `xcrun devicectl`. Metal reads it as the process starts, so an app that Xcode launches takes the key `MetalCaptureEnabled` of its `Info.plist` instead.

An app that stops or restarts leaves the server, which says so and keeps serving the apps that connect next. The server stops when the run ends, such as when the app quits, the key `q` quits or Ctrl+C stops the streamed log.

## Web loader

The file `loader.js` runs when the page loads:

1. It creates `Module` with the canvas, so the page scripts of the app can add page handlers to `Module.preRun`.
2. It reads `config.json`, which the web backend writes and which `index.html` preloads next to the style and the loader. It holds the app name and identifier, whether `window.transparent` is set, the splash logo and backgrounds, the files of the protected release that download before the app starts, or `app.zip` in development, the size of each of them and of `haylen.wasm`, and the plugins with a web module.
3. It sets the title and the background, picks the light or the dark look of the splash, and downloads and decodes `splash.<ext>`, the logo of the app or the Haylen symbol, once for both the logo of the splash and the icon of the page. Only then does the content of the splash show, so its first paint already holds the logo, and only then do the runtime and the package start to download. For a transparent app the page itself has no background, so whatever holds the page, such as the page of an editor that embeds it in a frame, shows through the transparent pixels of the canvas. Only the splash keeps the splash background.
4. It checks for WebAssembly and for WebGPU or WebGL2, and a browser with neither sees a message instead of a blank page. The runtime draws through WebGPU, or through WebGL2 where WebGPU does not start. Add `?graphicsApi=webGpu` or `?graphicsApi=webGl2` to the address to try one of them first, which the loader passes on as `Module.haylen.graphicsApi`. Add `?hosting=worker` to run the app in the [Worker mode](build.md#hosting-the-runtime), a dedicated worker that keeps the page answering whatever the scripts of the app do, which the loader passes on as `Module.haylen.hosting`. Those are the names that `graphics.apiName()` gives the two APIs. Any other name fails the app, and the page shows why.
5. It downloads `haylen.wasm` with the manifests and the app domain of the protected release, or with `app.zip` in development, behind one progress bar with its percentage. Each download counts the bytes it streams against its `Content-Length`, or against the size in `config.json` when the length is missing or describes compressed bytes. Meanwhile it imports the web module of every plugin from `plugins/<id>/`.
6. It hands the WebAssembly bytes to the runtime through `Module.instantiateWasm`, which compiles them instead of downloading `haylen.wasm` again, or through `Module.wasmBinary` to the worker of the Worker mode. It hands the release as `Module.haylen.release`, or the package as `Module.haylen.packageData` in development, then loads `haylen.js`, or `haylen-page.js` in the Worker mode. The runtime opens the [content store](../content/content.md#the-web) of the app before it starts, and the content of the release streams from `app/` as the app needs it. Every file of the page downloads once.
7. Before the app starts, a `Module.preRun` callback holds the start with a run dependency. Meanwhile it calls `load(context)` of every plugin in load order, with the context of `Module.haylen.createPluginContext`, as the [plugin guide](../platform/plugins.md#web) describes. A plugin that fails to import or to load replaces the progress bar with its error, and the app does not start.
8. `Module.haylen.onSplashEnded` fades the splash out once the runtime ends it, and `Module.haylen.onStarted` tells the page that the app runs. `Module.haylen.onError` writes every error to the console. An error that happens before the app starts replaces the progress bar with the message and the Lua stack trace. Later errors show on the error screen of the runtime.

When `config.json` holds the `development` entry, which only a Debug run of `app run` writes, the loader sets `Module.haylen.development`, and the address of the development server with its token in `Module.haylen.developmentServer`. The [build guide](build.md#page-setup) lists every field that a page sets and the whole [runtime API](build.md#runtime-api) with its [editor contract](build.md#the-editor-contract), whose request `loadZip` replaces the running app later.

### Changing the splash

The splash is the element `#splash` of `index.html`: the logo, a thin progress bar, the loading text with the percentage under it and, when the app cannot start, the message of the failure. It stays centered in the page in every browser, since it covers the page and centers its content with a flexible box.

The logo is optional. A page whose `index.html` has no `#splash-logo` shows the bar, the loading text and the percentage alone, centered with no empty room above them, and the loader still gives the icon of the page the logo of `config.json`. For example, this splash has no logo:

```html
<div id="splash" class="starting">
    <div id="splash-progress" role="progressbar" aria-label="Loading" aria-valuemin="0" aria-valuemax="100" aria-valuenow="0">
        <div id="splash-bar"></div>
    </div>
    <p id="splash-caption" aria-hidden="true"><span>Loading</span><span id="splash-percent">0%</span></p>
    <p id="splash-status" hidden></p>
    <pre id="splash-details" hidden></pre>
</div>
```

Every value that an app usually changes is a variable at the top of `loader.css`, so an app gives its own `loader.css` through `platforms.web.files`. The first block holds the values over a dark splash background, and the block of `#splash.light` holds the colors over a light one. The loader adds the class `light` when the relative luminance of the splash background of `haylen.json` passes one half, and checks again when the system switches between light and dark colors. So the text and the bar always stand out from the background.

| Variable | Meaning |
| --- | --- |
| `--splash-logo-size` | The width and height of the logo, which keeps its proportions inside them. |
| `--splash-bar-width`, `--splash-bar-height` | The size of the progress bar, whose ends are round. |
| `--splash-gap` | The space between the logo, the bar and the failure message. The loading text and the percentage sit closer to the bar, at half of it. |
| `--splash-font` | The font of the loading text, the percentage and the failure message. |
| `--splash-text` | The color of the loading text and the percentage. |
| `--splash-track`, `--splash-bar` | The colors of the empty and the filled part of the bar. |
| `--splash-failed` | The color of the failure message. |
| `--splash-details-background`, `--splash-details-text` | The colors of the box with the details of a failure, such as the Lua stack trace. |

For example, an app with its brand color on the bar and a larger logo copies `loader.css` of the template and changes only the variables:

```css
:root {
    --splash-logo-size: min(50vmin, 280px);
    --splash-bar: #5b42f3;
}

#splash.light {
    --splash-bar: #5b42f3;
}
```

A deeper change, such as another layout, edits the rules below the variables and gives its own `index.html` through `files`, which keeps the mark `{{SCRIPTS}}` where the tooling loads the loader and the page scripts. The loader needs only the ids `splash`, `splash-progress`, `splash-bar`, `splash-percent`, `splash-status` and `splash-details`, and `splash-logo` when the splash shows a logo. It removes the class `starting`, which hides the content of `#splash` until the basic files arrived, and sets the classes `light`, `failed` and `hidden` on `#splash`.

## Platform support

| Platform | Status | How it runs |
| --- | --- | --- |
| macOS | Supported | macOS 13.3 and later, the first release whose C++ library formats floating point numbers with `std::format`, as [Haylen.xcframework](#haylenxcframework) explains. The `player` target in development, or the `macos` target of the Apple backend. |
| Windows, Linux | Supported | Windows 10 version 1903 and later, the first that applies the UTF-8 code page of the application manifest, through which the engine reads its command line. Linux with X11, or with Wayland, where the window keeps the place and the level that the compositor gives it, and the glibc of the machine that built the app, or later. The `player` target in development, or the `windows` and `linux` targets of the desktop backend. CI builds and tests on those hosts. The application manifest of Windows apps declares Windows 10 and 11. Linux apps load GIO at run time for the theme, and run without it, as [`haylen.system`](../lua-api/system.md#platforms) describes. |
| iOS, iPadOS | Supported | iOS and iPadOS 16.3 and later, on iPhone and iPad, with every orientation of `haylen.json` and every iPad window size. The fold of the foldable iPhone needs iOS 27.1 or later on the device, whose UIKit reports the hinge and the reserved regions. |
| Mac Catalyst | Supported | macOS 13.3 and later. The iPad app on the Mac, whose window opens at the size of `haylen.json` in the points of the Mac, where macOS places it. SDL reads the buttons and the wheel of the mouse through `GCMouse` and the pointer position through a hover gesture on the view of the app, as on an iPad with a mouse or a trackpad, and the runtime reads the keyboard through `GCKeyboard`, as on iOS and tvOS. Typed text comes from the native field of the [text input](../ui/text-input.md) of the runtime. |
| tvOS | Supported | tvOS 16.3 and later, on Apple TV and its simulator, with the Siri Remote and game controllers. |
| visionOS | Supported | visionOS 1.0 and later, on Apple Vision Pro and its simulator, which builds with the visionOS slices of `Haylen.xcframework` and draws through Metal in a window of the shared space. Scripts see the app as an iOS app, `haylen.platform` is `'ios'`, and the app takes the native libraries and the plugins that list `visionos`. The window has no orientation, status bar or launch screen, and the keyboard floats apart from it. The GPU tests and the whole test project passed on the Apple Vision Pro simulator of visionOS 27, and no physical Apple Vision Pro ran it. |
| watchOS | Not possible | The watchOS 27 SDK has no Metal, MetalKit, GameController or AudioToolbox, which the renderer, the input and the audio of the engine need. |
| Android | Supported | Phones, tablets and Android TV from one APK, on arm64-v8a, armeabi-v7a and x86_64, Android 8.1 (API 27) and later. |
| Web | Supported | Desktop and mobile browsers with WebGPU or WebGL2 and the exception handling and SIMD instructions of WebAssembly, which Chrome 95, Firefox 100 and Safari 16.4 all have, since Lua unwinds its errors and yields through exceptions and Box2D solves contacts with SIMD. Sound needs `AudioWorklet`, which browsers offer only to pages served over https or from localhost, and apps on other pages run without sound. |
| Game consoles | Through a port | A console needs a [port](../platform/ports.md) that a developer with access to its SDK writes in a private repository, since those SDKs come under licenses that keep their code out of this repository. The repository holds the mechanism of ports and a test port that runs on a desktop, and no port of a console. |

The engine and the apps of Apple platforms build with Xcode 27.1 or later, the first whose SDKs declare the hinge, the reserved regions and the scene accessory of external displays that the engine uses. Continuous integration selects the newest Xcode of its image by its exact version in `.github/actions/build-environment`, and a job stops with a sentence that names the version when the image lacks it.

Each platform draws through the graphics APIs of its build, in the order that the [RHI guide](../graphics/rhi.md#choosing-the-graphics-api) lists: Metal on Apple platforms, Direct3D 12, Direct3D 11, Vulkan and OpenGL on Windows, Vulkan and OpenGL on Linux, Vulkan and OpenGL ES on Android, and WebGPU and WebGL2 on the web. The same guide records where each device was [tested](../graphics/rhi.md#status-of-the-devices) and the limits of each API.
