# Plugins

A plugin gives Lua apps a capability that each platform implements natively, such as ads, analytics, sign-in or purchases. It is a folder with a manifest, `plugin.json`, a Lua API under `source/` and a native part for each platform it supports: Swift or Objective-C sources with Swift packages for Apple platforms, a Gradle library module for Android, ES modules for the web and a CMake library for desktops. The Lua API is the same on every platform. An app lists the plugins it uses in its `app.json`, and haylen.py checks them, puts their Lua code into the package and brings their native parts into the project of each platform through its generated folder `haylen/`, so an app still never compiles the engine.

These plugins are distributable folders, not the engine plugins of `haylen::plugins`, which are the C++ subsystems that make up the engine. Plugins that bring third-party SDKs, such as ads, analytics, crash reporting, sign-in or purchases, live in their own repositories under [haylen-org](https://github.com/haylen-org), and apps add them with `haylen.py plugin add` and the address of the repository. The engine repository holds no third-party SDK: [the demo plugin of the plugins sample](#demo-plugin-and-sample), built on platform APIs alone, exercises every capability a plugin uses.

## Using plugins

### Commands

```sh
python3 haylen.py plugin list
python3 haylen.py plugin add admob --app ~/apps/my-game
python3 haylen.py plugin add ~/plugins/my-plugin --app ~/apps/my-game
python3 haylen.py plugin list --app ~/apps/my-game
python3 haylen.py plugin remove admob --app ~/apps/my-game
python3 haylen.py plugin new ~/plugins/my-plugin
```

| Command | Purpose |
| --- | --- |
| `plugin add <folder\|repository> [--ref] [--app]` | Copies a plugin folder, or the root of a plugin repository at the branch, tag or commit that `--ref` names (the default branch otherwise), into `plugins/<id>/` of the app, replacing an earlier copy, without its git history and build outputs. The copy keeps the `.gitignore`, `.editorconfig` and `.clang-format` of the plugin, which never reach the package. It lists the plugin in `app.json` with the default of every parameter that has one and an empty text for every required parameter, which the developer fills in, keeps the values the app already gives, and names the plugins it requires that the app does not list yet. |
| `plugin remove <id> [--app]` | Deletes `plugins/<id>/` of the app and its entry in `app.json`. |
| `plugin list [--app]` | Lists the plugins of the app with their version, their platforms and their status: `ok`, the problems that keep a plugin from building for any of its platforms, or a folder that `app.json` does not list. |
| `plugin new <folder> [--id]` | Creates a plugin from `templates/plugin/` in a folder named after its id, with an example method and event on every platform. `--id` defaults to the folder name and must match it. |

`--app` takes an app folder or a sample path from `samples/`, and defaults to the current folder.

### app.json

```json
{
    "plugins": {
        "admob": {"iosAppId": "ca-app-pub-3940256099942544~1458002511", "androidAppId": "ca-app-pub-3940256099942544~3347511713", "testMode": true},
        "firebase": {"googleServicesPlist": "platform/firebase/GoogleService-Info.plist", "googleServicesJson": "platform/firebase/google-services.json"},
        "firebase-analytics": {}
    }
}
```

`plugins` maps the id of every plugin the app uses to the values of its parameters, and each id names the folder `plugins/<id>/` of the app. Values are plain JSON values. A value that differs per platform uses one parameter per platform, such as `iosAppId` and `androidAppId`, so nothing resolves platform objects at runtime. A parameter that the app leaves out takes its default. The README of every plugin lists its parameters.

A `file` parameter is a path relative to the app folder. Files such as `GoogleService-Info.plist` and `google-services.json` belong outside `source/` and `content/`, for example under `platform/`, so they never ship in the package, and haylen.py brings them into the projects that need them.

### Validation

Before `run` builds for a platform, haylen.py loads every plugin that `app.json` lists from `plugins/` of the app, checks its `plugin.json` against the [format](#pluginjson) and checks the values of the app against its parameters for that platform. The player of this machine checks them for this desktop. The tool `haylen.py` fails with every problem at once, each with its file and its key, for:

- an id whose folder has no `plugin.json`
- any problem of a `plugin.json`
- a plugin that `requires` another one that `app.json` does not list
- a value for a parameter that the plugin does not declare
- a value of the wrong type
- a required parameter without a value for the platform, which includes the empty text that `plugin add` writes
- a `file` parameter whose file does not exist

A parameter applies to the platforms it lists, or to every platform of the plugin when it lists none, so a missing required value and a missing file fail only the builds for those platforms.

### Load order

Plugins load in the order of `app.json`, except that every plugin loads after the plugins it requires, and two plugins that require each other fail the build. The same order loads the plugin classes on Apple platforms and the plugin modules on Android, which their runtimes read from the package, and the modules that the web loader imports.

### The package

For every plugin that `app.json` lists, the package carries `plugins/<id>/plugin.json` and `plugins/<id>/source/` next to `app.json`, `source/` and `content/`. `haylen.py package`, the package copies of the Apple and Android projects, the Android package index and the `app.zip` of the web all hold them. `require('<id>')` loads `plugins/<id>/source/init.lua`, and `require('<id>.<name>')` loads `plugins/<id>/source/<name>.lua`. Nothing else of a plugin folder ships in the package: its native parts reach the platform projects, and its README stays behind.

### Apple platforms

The tool `haylen.py` brings the Apple parts of the plugins into the target templates of `haylen/project.yml`, which the targets of `project.yml` name in `templates`, as the [distribution guide](distribution.md#the-apple-project) describes:

1. It copies the `sources` folder of every plugin to `haylen/plugins/<id>/sources/` and its resources to `haylen/plugins/<id>/resources/`.
2. It adds the sources, the Swift packages with the products they link, the system frameworks, the resources and the build scripts of each plugin to the templates `HaylenIOS`, `HaylenTVOS` and `HaylenMacOS` of the platforms the plugin lists. `HaylenIOS` builds iOS and Mac Catalyst, so a plugin that lists only one of `ios` and `catalyst` joins it with `destinationFilters`, which keep its sources, products and frameworks to that destination. A product or a framework that several plugins link joins a template once, and a framework of the engine or one that a target of the template links itself stays out, since XcodeGen refuses a dependency that a target lists twice.
3. It merges the `infoPlist` keys of the plugins into the `Info.plist` of each platform, where `ios/Info.plist` serves iOS and Mac Catalyst, the `entitlements` into the entitlements of iOS, Mac Catalyst, tvOS and macOS, and the `privacy` declarations into the privacy manifest of the app, all in `haylen/`. The values of the developer win and the keys of the plugins fill what is missing: objects merge key by key and arrays gain the items they lack, so two ad plugins share `SKAdNetworkItems`, while a key that two plugins, or a plugin and `app.json`, set to different values fails the build with both named.
4. `run` generates `App.xcodeproj` again with the pinned XcodeGen in `.tools/xcodegen`, which `haylen.py tools` and the first such build download, when `haylen/project.yml` changed and the project holds no edits made in Xcode since its last generation, and `python3 haylen.py xcodegen <app>` generates it whenever the developer asks.

The sources of a plugin compile into the targets like the files of `source/`, so Swift reaches the engine through `HaylenBridging.h`, and [the Apple part](#the-apple-part) describes the plugin class they hold. The resources land at the root of the app bundle. Build scripts run as post-build phases for every destination of their targets, so a script of a plugin that leaves out Mac Catalyst checks `IS_MACCATALYST` itself. The runtime reads the plugin classes and their order from the package, as [the plugin class](#the-plugin-class) describes. A target without its template, or with an `INFOPLIST_FILE` of its own, leaves out what the plugins bring, which [`check`](distribution.md#check) reports and the plugins answer with `unsupported` at run time.

### Android

1. The tool `haylen.py` copies the library module of every plugin into `haylen/plugins/<id>/` of the Android project.
2. It writes the plugin keys of `haylen/haylen.properties`. `plugins` lists the modules as `id=folder` entries, which `settings.gradle.kts` includes and `app/build.gradle.kts` depends on. `gradlePlugins` lists the Gradle plugins as `id=version` entries, and every `placeholder.<name>` key is a manifest placeholder.
3. The root `build.gradle.kts` puts the plugin marker of every Gradle plugin, `<id>:<id>.gradle.plugin:<version>`, on the build classpath, which serves the app module like a plugin declared with `apply false`, and `app/build.gradle.kts` applies each one by its id. A `plugins {}` block takes only literal ids, so a list that changes per app goes through the build classpath.
4. `app/build.gradle.kts` adds the placeholders to `manifestPlaceholders`, where the manifests of the plugin modules find them when the manifests merge.
5. The `files` of the plugins, such as `google-services.json` for `app/`, belong to the project. The tool `haylen.py` copies them into the copy of the template that it keeps for an app without `platform/android`, and in a project of the developer [`check`](distribution.md#check) names each one that the project lacks, with the copy to make.

The manifest of every module merges into the app with its permissions, its `dev.haylen.plugin.<id>` meta-data and its other entries, together with the manifests of the libraries the module depends on. Every plugin module depends on `dev.haylen:haylen-plugins`, whose manifest declares the provider that loads the plugin classes that the meta-data names when the app process starts, as [the Android part](#the-android-part) describes, so an app without plugins has no provider. A permission that the app removes from the merged manifest with `tools:node="remove"` stays out, which `check` reports and the plugin answers with `unsupported` at run time.

### Web

The tool `haylen.py` copies the `web/` folder of every plugin with a web part to `plugins/<id>/` of the site and lists `{id, version, module, config}` in `config.json`, where `module` is the path of its module in the site and `config` holds the values of its parameters with the defaults applied. The loader imports every module while the runtime downloads. Once the runtime exists and before the app starts, it calls the default export of each module, `load(context)`, in load order, with the context that `Module.haylen.createPluginContext(id, config)` of the runtime makes, and waits for the promise that `load` may return. A module that fails to import, or a `load` that fails, keeps the app from starting and shows the error on the loading page. A page served with `Cross-Origin-Opener-Policy: same-origin` cuts the popups of screens off from the app, which `check --coop same-origin` reports for the plugins whose modules register screens.

### Native libraries

The `native` section of a plugin joins the native libraries of the app, so haylen.py builds and places it like an entry of the `native` section of `app.json`, as [native libraries](distribution.md#native-libraries) describes, for the desktop player, Windows and Linux apps, the Apple projects and Android, on the platforms it lists. The library is named after the id of the plugin with its dashes turned into underscores, so `native.load('my_plugin')` loads the library of `my-plugin`.

## Plugin package

### Layout

```text
<id>/                 The id in dash-case, such as admob or firebase-analytics.
  plugin.json         The manifest.
  README.md           What the plugin does, its setup on each platform, its parameters and its complete Lua API with examples.
  source/             The Lua API: init.lua is require('<id>') and <name>.lua is require('<id>.<name>').
  apple/              Swift and Objective-C sources, compiled into the app targets of the platforms of the plugin.
  android/            Gradle Android library module with build.gradle.kts, src/main/AndroidManifest.xml and its Java or Kotlin sources.
  web/                ES modules, among them the module that plugin.json names.
  native/             CMake project of a C or C++ library.
```

A plugin needs only the folders of the platforms it supports. Paths in `plugin.json` are relative to the plugin folder and stay inside it, and haylen.py checks that each one exists.

### plugin.json

```json
{
    "id": "admob",
    "name": "AdMob",
    "version": "1.0.0",
    "description": "Banner, interstitial, rewarded and app open ads with consent.",
    "platforms": ["ios", "android", "web"],
    "requires": [],
    "parameters": {
        "iosAppId": {"type": "string", "platforms": ["ios"], "required": true, "description": "AdMob app id of the iOS app."},
        "androidAppId": {"type": "string", "platforms": ["android"], "required": true, "description": "AdMob app id of the Android app."},
        "testMode": {"type": "boolean", "default": false, "description": "Serve test ads."},
        "trackingDescription": {"type": "string", "platforms": ["ios"], "default": "Your data is used to show you more relevant ads.", "description": "Text of the App Tracking Transparency prompt."}
    },
    "apple": {
        "class": "HaylenAdMobPlugin",
        "sources": "apple",
        "packages": {
            "GoogleMobileAds": {"url": "https://github.com/googleads/swift-package-manager-google-mobile-ads.git", "exactVersion": "13.10.0", "products": ["GoogleMobileAds"]}
        },
        "frameworks": ["AppTrackingTransparency.framework"],
        "infoPlist": {"GADApplicationIdentifier": "${iosAppId}", "NSUserTrackingUsageDescription": "${trackingDescription}", "SKAdNetworkItems": [{"SKAdNetworkIdentifier": "cstr6suwn9.skadnetwork"}]}
    },
    "android": {
        "module": "android",
        "placeholders": {"haylenAdmobAppId": "${androidAppId}"}
    },
    "web": {"module": "web/admob.js"}
}
```

| Key | Required | Value |
| --- | --- | --- |
| `id` | Yes | The id in dash-case, the name of the plugin folder. |
| `name` | Yes | The name people read. |
| `version` | Yes | One to three numbers separated by dots, such as `1.2.0`. |
| `description` | Yes | What the plugin does, in one sentence. |
| `platforms` | Yes | The platforms the plugin supports, each once: `ios`, `catalyst`, `tvos`, `macos`, `android`, `web`, `windows` and `linux`. Its Apple part compiles only into the targets and destinations they allow. |
| `requires` | No | The ids of other plugins that the app must list as well, such as `firebase` for `firebase-analytics`. |
| `parameters` | No | The values an app gives the plugin in `app.json`, by name in camelCase. |
| `apple` | No | The Apple part, which needs one of `ios`, `catalyst`, `tvos` and `macos` in `platforms`. |
| `android` | No | The Android part, which needs `android` in `platforms`. |
| `web` | No | The web part, which needs `web` in `platforms`. |
| `native` | No | A native library, which follows the rules of the `native` section of `app.json`, with paths relative to the plugin folder and platforms of the plugin. |

A platform without its part runs the Lua API of the plugin alone, where calls to the native side fail with the code `noHandler`.

#### Parameters

| Key | Required | Value |
| --- | --- | --- |
| `type` | Yes | `string`, `number`, `integer`, `boolean`, `array`, `object` or `file`. A `file` is a path relative to the app folder. |
| `description` | Yes | What the value means, which haylen.py repeats when a required value is missing. |
| `platforms` | No | The platforms of the plugin where the parameter applies and, when it is required, where a value must exist. Every platform of the plugin by default. |
| `required` | No | Whether the app must give a value for the platforms of the parameter. `false` by default. |
| `default` | No | The value of an optional parameter that the app leaves out, of the type of the parameter. |

`${name}` in a string of the `apple` and `android` sections is replaced with the value of the parameter `name`, and each one must name a declared parameter. A string that is only a reference takes the value with its type, so `"${testMode}"` becomes a boolean in `Info.plist`, while a reference inside longer text inserts the value as text. A key or an item whose references name a parameter without a value is left out. Shell expansions such as `${BUILD_DIR%/Build/*}` are no references and stay as they are. Build settings in paths and scripts use the `$(NAME)` form, which also keeps XcodeGen from replacing them with environment variables when it generates the project.

#### apple

| Key | Value |
| --- | --- |
| `class` | The Objective-C runtime name of the plugin class, which Swift classes declare with `@objc(Name)`. The runtime creates it on every Apple platform of the plugin, as [the plugin class](#the-plugin-class) describes. |
| `sources` | The folder whose Swift, Objective-C, C and C++ files compile into the targets of the platforms of the plugin. |
| `packages` | Swift packages by name, each with its repository `url` (https or ssh), one `exactVersion` and the `products` that the targets link. Two plugins that name one package must agree on both. |
| `frameworks` | System frameworks and libraries, such as `StoreKit.framework` or `libz.tbd`. |
| `infoPlist` | Keys merged into the `Info.plist` of every platform of the plugin. |
| `entitlements` | Keys merged into the entitlements of every platform of the plugin. |
| `privacy` | Declarations merged into the privacy manifest of the app: `NSPrivacyAccessedAPITypes`, each with its `NSPrivacyAccessedAPIType` and `NSPrivacyAccessedAPITypeReasons`, `NSPrivacyCollectedDataTypes`, `NSPrivacyTracking` and `NSPrivacyTrackingDomains`, as the [privacy manifest](distribution.md#privacy-manifest) describes. |
| `resources` | Files copied to the root of the app bundle: a path inside the plugin, or a reference to a `file` parameter such as `"${googleServicesPlist}"`, which names a file of the app and is left out while the parameter has no value. |
| `buildScripts` | Post-build phases, each with a `name`, a `script` and optional `inputFiles` and `outputFiles`. |

#### android

| Key | Value |
| --- | --- |
| `module` | The folder of the Android library module, with `build.gradle.kts` and `src/main/AndroidManifest.xml`. The module applies `com.android.library`, which the Android template declares, and depends on the engine with `implementation("dev.haylen:haylen-plugins:$haylenEngineVersion")`, where `val haylenEngineVersion: String by extra` reads the engine version that the Android project gives every module, and on the [other engine libraries](#engine-libraries) it uses. |
| `gradlePlugins` | Gradle plugins that the app module applies, each as `{"id": ..., "version": ...}`, such as `{"id": "com.google.gms.google-services", "version": "4.5.0"}`. |
| `placeholders` | Manifest placeholders by name, whose text values the manifest of the module reads as `${name}`. |
| `files` | Copies into the Android project, each as `{"from": ..., "to": ...}`. `from` is a path inside the plugin or a reference to a `file` parameter, and `to` is a path inside the project, such as `app/google-services.json`. |

#### web

| Key | Value |
| --- | --- |
| `module` | The ES module in the `web` folder that the loader imports, such as `web/admob.js`. Its default export is `load(context)`, and it may import the other modules of the folder with relative paths. |

### Creating a plugin

```sh
python3 haylen.py plugin new ~/plugins/my-plugin
python3 haylen.py plugin add ~/plugins/my-plugin --app ~/apps/my-game
```

`plugin new` copies `templates/plugin/` with the id in the names of the files, classes and modules: `plugin.json`, a README, `source/init.lua` with the Lua API, `apple/<Name>Plugin.swift`, the Android module with its manifest meta-data and `<Name>Plugin.kt`, and `web/<id>.js`, together with the files that the repository of a plugin keeps at its root: `.gitignore`, which keeps out the build folders of the Android module and of the CMake project in `native/`, the caches of Gradle and Kotlin and the files of Finder and the editors, `.editorconfig`, which sets the style of text files, and `.clang-format`, the C, C++ and Objective-C style of the engine. Each native part answers the method `<id>.echo` and sends the event `<id>.echoed`, and the Lua API wraps both with the plugin handle of `platform.plugin(id)`.

## Lua API of plugins

### Modules

The Lua API of a plugin lives in `source/` of its folder, and the package carries it as `plugins/<id>/source/`. `require('<id>')` loads `plugins/<id>/source/init.lua`, and `require('<id>.<name>')` loads `plugins/<id>/source/<name>.lua` or, when that file is missing, `plugins/<id>/source/<name>/init.lua`, with dots of the name as folders like any module. Only the plugins that `app.json` lists have modules, so `require` looks for any other name in `source/` of the app. Plugin modules load as text like every chunk, and their chunk names are their package paths, so errors point at files such as `plugins/admob/source/init.lua`.

A module of the app whose first name part is the id of a plugin could never load, because `require` finds the module of the plugin first. The app stops on the error screen when it starts, before `source/main.lua` runs, with both files named: `The app module source/admob/ads.lua has the name admob.ads, which require resolves to plugins/admob/source/ads.lua of the plugin admob. Rename the module of the app.` An app in development restarts when a module or the `plugin.json` of one of its plugins changes, like it does for its own modules.

### The plugin handle

`platform.plugin(id)` of [haylen.platform](lua-api/platform.md#plugin-handles) returns the handle of a plugin, which its Lua modules use to reach its native part:

| Member | Meaning |
| --- | --- |
| `handle.id`, `handle.version` | The id and the `version` of `plugin.json`. |
| `handle.config` | The parameter values of `app.json` over the `default` of every parameter of `plugin.json`, the same values the native parts receive. |
| `handle.native` | Whether the native part of the plugin runs on this platform. |
| `handle:call(method, params, options)` | Calls `<id>.<method>` and returns a platform call, which a coroutine awaits. |
| `handle:send(method, params)` | Calls `<id>.<method>` when nothing needs its answer, without a call object. |
| `handle:on(event, listener)` | Listens to the event `<id>.<event>` and returns a connection. |
| `handle:videoStream(name)`, `handle:audioStream(name)` | The video or audio [stream](#streams) `name` that the native part opened, or `nil` until it opens it. |
| `handle:openScreen(name, params, options)` | Opens the [screen](#plugin-screens) `name` of the plugin, which covers the app until it ends, and returns a platform call that its result settles. |

`platform.plugins()` lists the plugins of the app with their `id`, `version` and `native`. Methods and events of plugins use camelCase names, and the native parts register and send them under the same names, which their contexts prefix with the id. A platform without the native part of a plugin answers its calls with the code `noHandler`, and a native part that cannot offer a method on its platform fails the call with the code `unsupported` and a message that says why, so the Lua API stays the same everywhere. `handle.native` tells the Lua API whether the native part exists at all.

```lua
-- plugins/game-center/source/init.lua
local platform = require('haylen.platform')

local handle = platform.plugin('game-center')
local gameCenter = {}

-- Signs the player in and returns a call whose await gives {playerId, displayName}.
function gameCenter.signIn()
    return handle:call('signIn', {showUi = handle.config.showSignInUi})
end

function gameCenter.submitScore(board, score)
    handle:send('submitScore', {board = board, score = score})
end

-- The native part sends playerChanged retained, so a listener added after launch still hears the player that signed in on its own.
function gameCenter.onPlayerChanged(listener)
    return handle:on('playerChanged', listener)
end

gameCenter.available = handle.native

return gameCenter
```

```lua
-- source/main.lua
local async = require('async')
local gameCenter = require('game-center')

gameCenter.onPlayerChanged(function(player) print('playing as ' .. player.displayName) end)
async.spawn(function()
    local player, err = gameCenter.signIn():await()
    print(player and player.playerId or err)
end)
```

## Engine services

The engine offers the native parts of plugins a few services on every platform. Native code reaches them through the plugin context of its platform from any thread, and the engine applies them on the frame thread at the start of the next frame. The headless host of the tests drives them the same way, as the [testing guide](testing.md#the-headless-host) describes.

### Reserved edges

A native view over the app, such as a banner ad, reserves the edge of the screen it sits on, under a key of its own, with insets in framebuffer pixels. The safe area of the app becomes the safe area of the device widened, edge by edge, to the largest reservation on that edge, so UI anchored to the safe area moves out of the way of the view on its own, `windowSafeAreaChanged` announces the change, and [`viewport.reservedInsets()`](lua-api/viewport.md#viewportreservedinsets) reports the reservations in design units. A view reserves again when it moves or changes size, and releasing its key gives the edge back. Reservations belong to the platform, so they outlive an app that restarts under them. The overlays of the plugin contexts reserve for the views they place with `reserve` set in their placement.

### Covering the app

Native UI that covers the app, such as a full screen ad, a consent form, a sign-in sheet or a purchase dialog, calls `coverApp` of its context when it shows and `uncoverApp` when it goes away. Covers are counted, so they nest. While any cover lasts the app is `inactive`, halted and muted, whatever its lifecycle options say, `haylen.appCovered()` returns `true`, and the app hears the usual `appInactive` and `appActive` events. The engine draws one frame once the cover begins, which stays on screen under the native UI, and no more until the cover ends, since a halted app would draw the same frame again. When the last cover ends the app comes back as it was, as the [lifecycle guide](lifecycle.md#covered-by-native-ui) describes. An `uncoverApp` without a `coverApp` is logged as an error and changes nothing. Native libraries cover the app with `coverApp` and `uncoverApp` of `HaylenNativeApi`, and a [screen](#plugin-screens) of a plugin covers the app on its own.

### Errors of the app

Every error that stops the app, the one its error screen shows, reaches the native parts of plugins as `{message, file, line, traceback, frames}`, where `frames` lists `{source, line, function, kind}` from the innermost call outward with the kind `lua`, `c` or `main`, the shape the web page receives in `onError`. A crash reporter records it as a non-fatal error with the stack of the Lua code. On the web, `context.onAppError(listener)` receives it, on Apple platforms the `appDidFailWithError:` method of the plugin class, and on Android the `onAppError(JSONObject)` method of the plugin class, on the main thread. A native library receives it as JSON text in the handler it gives `registerErrorHandler` of [`HaylenNativeApi`](lua-api/native.md#library-handlers), on the frame thread, which is how the native part of a plugin hears errors on the desktops.

### Retained events

Events that native code sends before the app listens, such as the deep link or the notification that opened the app or a purchase that finished while it was closed, are sent retained. A retained event waits, up to 32 per name, for the first listener of its name, which receives the waiting events in order, as the [platform bridge guide](platform_bridge.md#retained-events) describes. The web context sends one with `context.emit(event, payload, {retain: true})`, the context of an Apple plugin with `emitRetained:payload:`, the context of an Android plugin with `emitRetained(event, payload)`, and a native library with `emit(event, payload, 0, 0, HAYLEN_NATIVE_EMIT_RETAIN)` of `HaylenNativeApi`.

### Batched events

A plugin that reports something many times per frame, such as a sensor, the location or the progress of a download, sends its events batched. The batched events of a name that arrive in one frame reach the Lua listener once, as one list of their payloads in order, so the app runs its listener once per frame instead of once per event, as the [platform bridge guide](platform_bridge.md#batched-events) describes. The flag goes next to the retain flag: `context.emit(event, payload, {batched: true})` on the web, `emit:payload:retain:batched:` of the context of an Apple plugin, `try context.emit(event, payload, batched: true)` in Swift, `emit(event, payload, retain, batched)` of the context of an Android plugin, and `HAYLEN_NATIVE_EMIT_BATCHED` in the flags of `emit` of `HaylenNativeApi`.

### Bytes

Calls, answers and events carry bytes next to their JSON, such as a photo, a recording or a file, which never turn into text. The Lua API of a plugin marks a string that goes to the native part with [`platform.bytes(data)`](lua-api/platform.md#platformbytesdata), and every buffer that the native part sends arrives as a Lua string in its place. Each native part uses the byte type of its language, in its parameters and anywhere inside the values it answers and sends:

| Native part | Bytes it receives | Bytes it sends |
| --- | --- | --- |
| Apple, Objective-C and Swift | `NSData`, `Data` in Swift, in the parameters of `registerHandler:handler:`. | `NSData` and `Data` in the value of a reply and of `emit:payload:`. |
| Android, Java and Kotlin | `byte[]`, `ByteArray` in Kotlin, in the `JSONObject` of the parameters. | `byte[]` and `ByteBuffer` in the value of `reply.success` and of `emit`, where a direct `ByteBuffer` crosses JNI without a copy in Java. |
| Web | `Uint8Array` in the parameters of a handler. | `ArrayBuffer`, `Uint8Array` and every other `ArrayBuffer` view in the result of a handler and in `context.emit`. |
| Native library, C | An array of `HaylenNativeBuffer` next to the parameters of a handler. | `HaylenNativeBuffer` arrays of `resolve` and `emit`, which the JSON refers to as `{"$bytes": N}`. |

The helpers of the Swift template for `Codable` values carry JSON alone, so a Swift handler that takes or returns bytes registers with `registerHandler:handler:` and answers a dictionary. The [platform bridge guide](platform_bridge.md#byte-buffers) describes the format, and [`graphics.newTexture(bytes)`](lua-api/graphics.md#graphicsnewtexturebytes-options) and [`audio.newSound(bytes)`](lua-api/audio.md#audionewsoundbytes-options) turn the bytes of images and sounds into engine resources.

### Streams

A plugin that produces video or audio continuously, such as a camera, a video decoder, a microphone or a synthesized voice, feeds a stream instead of sending events. Streams belong to the process and have a name within their plugin, and the Lua API of the plugin reaches them with [`handle:videoStream(name)` and `handle:audioStream(name)`](lua-api/platform.md#video-streams), which return `nil` until the native part opens them.

- **Video.** Native code opens a `platform::VideoStream` with its pixel format, RGBA8 or BGRA8, and a size, and pushes frames from any thread with a pointer, a stride, a size and a timestamp. The stream copies each frame on the thread that pushes it, turning BGRA into RGBA and dropping the padding of the rows, and keeps only the newest one. At the start of every frame the engine uploads the newest frame, if it is new, into the dynamic texture of the stream through `graphics::Device`, so the texture changes once per frame at most, and a frame of another size resizes the texture in place.
- **Audio.** Native code opens a `platform::AudioStream` with its sample rate, channels, format, 32-bit float or 16-bit integer, and the frames its ring holds, and pushes interleaved samples from any thread, one thread at a time. The ring is lock-free with one writer and one reader, the voice that plays the stream, which resamples it to the mixer, plays silence where samples are missing and counts each short read as an underrun. Samples that do not fit while the ring is full are dropped, and push returns how many frames fit.

| Native part | Video | Audio |
| --- | --- | --- |
| Native library, C | `openVideoStream(plugin, name, format, width, height)` and `pushVideoFrame(stream, pixels, width, height, stride, timestamp)` of `HaylenNativeApi`. | `openAudioStream(plugin, name, sampleRate, channels, format, capacityFrames)` and `pushAudioFrames(stream, samples, frames)`. |
| Web | `context.videoStream(name)`, whose `push(source, timestamp)` takes an `ImageBitmap`, a `VideoFrame`, a `<video>`, an `<img>` or a `<canvas>`, draws it into a canvas of its size and copies its RGBA pixels into wasm memory. The timestamp defaults to the one of a `VideoFrame`, the current time of a video or the time of the push. | `context.audioStream(name, {sampleRate, channels, capacity})`, whose `push(samples)` copies an interleaved `Float32Array` into the ring and returns how many frames fit. The capacity defaults to one second of frames. |
| C++, inside the engine | `platform::PluginStreams::openVideo(plugin, name, format, width, height)` and `VideoStream::push(pixels, width, height, stride, timestamp)`. | `platform::PluginStreams::openAudio(plugin, name, sampleRate, channels, format, capacityFrames)` and `AudioStream::push(samples)` with a span of floats or 16-bit integers. |
| Apple, Objective-C and Swift | `openVideoStream:width:height:format:` of the context, `openVideoStream(_:width:height:format:)` in Swift, returns a `HaylenVideoStream`, whose `pushPixels:width:height:stride:timestamp:` takes a pointer and whose `pushPixelBuffer:timestamp:` takes a `CVPixelBuffer` of `kCVPixelFormatType_32BGRA` or `kCVPixelFormatType_32RGBA`, such as a frame of the camera, `push(_:timestamp:)` in Swift. | `openAudioStream:sampleRate:channels:format:capacity:`, `openAudioStream(_:sampleRate:channels:format:capacity:)` in Swift, returns a `HaylenAudioStream`, whose `pushSamples:frames:`, `push(_:frames:)` in Swift, writes interleaved floats or 16-bit integers and returns how many frames fit. |
| Android | `openVideoStream(name, width, height, format)` of the context returns a `HaylenVideoStream` with the format `HaylenVideoStream.Format.RGBA8` or `BGRA8`, whose `push(pixels, width, height, stride, timestamp)` takes a `ByteBuffer`, a direct one without a copy in Java, and whose `push(bitmap, timestamp)` takes a `Bitmap` of `Bitmap.Config.ARGB_8888`, which Android keeps as RGBA bytes, for a stream of the format `RGBA8`, locking its pixels instead of copying them in Java. | `openAudioStream(name, sampleRate, channels, format, capacity)` of the context returns a `HaylenAudioStream` with the format `HaylenAudioStream.Format.FLOAT32` or `INT16`, whose `push(samples, frames)` writes the first `frames` interleaved frames of a `float[]` or a `short[]` without a copy in Java and returns how many frames fit. |

Opening a stream again returns the same stream, whose handle stays valid for good, and opening it with another format fails. The web page is single-threaded, so its pushes and the mixing of its voices take turns on the thread of the page. On Android a stream opens once the first activity loaded the engine library, so a plugin opens its streams when the app asks for them rather than in `onLoad`, and the methods of the streams throw an `IllegalArgumentException` for arguments they cannot take and an `IllegalStateException` for samples of the other format or a bitmap for a stream of the format `BGRA8`.

### The native plugin list

`handle.native` and the `native` field of `platform.plugins()` come from the platform, which reports the plugins whose native part it loaded: on the web, the plugins whose module received a context, on Apple platforms the plugins whose class the runtime created, and on Android the plugins whose class the engine library created and loaded. A native library of the app declares itself the native part of a plugin with `registerPlugin(id)` of [`HaylenNativeApi`](lua-api/native.md#library-handlers), which is how the `native` part of a plugin counts on the desktops.

### Web modules

The web module of a plugin exports `default function load(context)`, which the loader calls before the runtime starts and whose promise it awaits. `Module.haylen.createPluginContext(id, config)` of the runtime makes the context, once per plugin.

| Member | Meaning |
| --- | --- |
| `context.id` | The id of the plugin. |
| `context.config` | The parameter values of `app.json` with the defaults of `plugin.json` applied. |
| `context.register(method, handler)` | Answers `<id>.<method>`, like `Module.haylen.register` described in the [platform bridge guide](platform_bridge.md#web). |
| `context.registerScreen(name, open)` | Opens the screen `<id>.<name>` with `open(params, screen)`, in a popup, with a redirect or with UI of the page, as [web screens](#web-screens) describes. |
| `context.restoredScreen` | The screen of the plugin that a redirect left before the page loaded again, with `id`, `name`, `token`, `resolve(value)` and `reject(error)`, or `null`, as [web screens](#web-screens) describes. |
| `context.emit(event, payload, options)` | Sends `<id>.<event>`, retained when `options.retain` is `true` and batched when `options.batched` is `true`, with `ArrayBuffer` and `Uint8Array` values as bytes. |
| `context.videoStream(name)` | Opens the [video stream](#streams) `name` of the plugin once the runtime is ready, and returns `{push(source, timestamp)}`, whose `push` returns `false` while it drops a frame, before the runtime is ready or while a video has no frame yet. |
| `context.audioStream(name, options)` | Opens the [audio stream](#streams) `name` of the plugin with `options.sampleRate`, `options.channels` and `options.capacity` in frames once the runtime is ready, and returns `{push(samples)}`. |
| `context.overlay.add(element, placement)` | Places an HTML element over the canvas and returns `{update(placement), setVisible(visible), remove()}`. |
| `context.coverApp()`, `context.uncoverApp()` | Cover the app while native UI shows, and end the cover. |
| `context.onAppError(listener)` | Calls `listener(error)` with the report of every error that stops the app. |
| `context.require(needs)` | Throws an error with the code `unsupported` and `data.missing` when the page lacks a secure context, an API or a feature of the permissions policy that `needs` names, as [web requirements](#web-requirements) describes. |

A module may register, emit, cover and place elements from `load` already: the runtime keeps the events until the first app starts and the covers and reservations until the WebAssembly runtime is ready.

The overlay is a layer over the canvas that lets the pointer through, so the app keeps its clicks and touches everywhere but on the elements of plugins, which receive their own. Keys belong to the element that has the focus: the app receives the keys of the canvas and the page, while an element of a plugin with the focus, such as the button of a modal dialog, receives its own keys with their default actions, like Escape closing the dialog. The focus moving between the canvas and elements of the page never makes the app inactive, since only the focus of the window of the page does, so an element that should leave the keyboard with the app keeps the focus from moving to it, as a button that calls `preventDefault` on `pointerdown` does. A placement has these fields:

| Field | Default | Meaning |
| --- | --- | --- |
| `anchor` | `'bottom'` | Where the element sits: `'top'`, `'bottom'`, `'left'`, `'right'`, `'topLeft'`, `'topRight'`, `'bottomLeft'`, `'bottomRight'` or `'center'`. |
| `margin` | `0` | The distance from the anchored edges, in page pixels. |
| `insideSafeArea` | `true` | Whether the element stays inside the safe area of the page, the CSS safe area insets that reach into the canvas. |
| `reserve` | `false` | Whether the element reserves the edge its anchor names, from the edge of the canvas to its far side, while it is visible. A centered element reserves nothing. |
| `width`, `height` | the size of the element | The size of the element in page pixels. |

The overlay places the element again whenever the canvas, the page or the element changes size, and reports its reservation in canvas pixels.

```js
// plugins/banner-kit/web/banner-kit.js
export default function load(context) {
    let banner = null;

    context.register("showBanner", (params) => {
        const element = document.createElement("div");
        element.textContent = params.text || "Banner";
        element.style.cssText = "background:#1d3557;color:#fff;font:16px system-ui;display:flex;align-items:center;justify-content:center;";
        element.addEventListener("click", () => context.emit("clicked", { at: Date.now() }));
        banner = context.overlay.add(element, { anchor: "bottom", height: 60, width: 320, reserve: true });
        return { shown: true };
    });

    context.register("hideBanner", () => {
        if (banner) {
            banner.remove();
            banner = null;
        }
        return null;
    });

    context.register("showInterstitial", async () => {
        context.coverApp();
        try {
            await new Promise((resolve) => setTimeout(resolve, 3000));
        } finally {
            context.uncoverApp();
        }
        return { completed: true };
    });

    context.onAppError((error) => console.warn("The app failed: " + error.message));
}
```

## Plugin screens

A screen is native UI of a plugin that takes over the app until it ends with one result, such as a paywall, a sign-in flow, a payment page, the activity of an SDK, a popup page on the web or a window on the desktops. The Lua API of the plugin opens it with [`handle:openScreen(name, params, options)`](lua-api/platform.md#screens), and the engine takes care of what every such flow needs, so the native part only shows its UI and reports how it ended.

### The model

- **Cover first.** The engine covers the app at the start of the frame after the request and only then hands the screen to the platform, so the app is `'inactive'`, halted and muted before the screen shows, as [covering the app](#covering-the-app) describes. While an opaque screen shows, which screens are by default, the engine draws nothing and the last frame stays on screen, while under a screen that lets the app show through it draws the halted app once and keeps that frame. The cover ends when the screen ends, however it ends, so native parts never cover the app for their screens themselves.
- **One at a time, in the foreground.** A screen opens at once while the app is `'active'`. One that the app asks for while it is `'inactive'` in the foreground, such as right in the answer of a dialog before its window has the focus again, waits until the app is `'active'`, within its `timeout`, and then opens, while in the background the call fails with the code `notActive`, also when the app goes there while the screen waits. While a screen shows, which the whole process shares, or waits, another one fails with the code `busy`.
- **The result.** The end of the screen settles its call, with the result or with a failure such as `cancelled` when the person closed the screen.
- **Restored ends.** A screen outlives the app that opened it: the app may restart under it, after a hot reload or `haylen.requestRestart()`, the process may end while the screen shows, as Android ends apps in the background, or a web page may leave for a redirect and load again. The end then reaches the next app as the retained event `<id>.screenRestored`, with `screen`, the name of the screen, `state`, the value that `options.state` gave, and `result`, or `error` with `message`, `code` and `data` for a failure. A restarted app starts covered by the screen that still shows, and `platform.screenShowing()` tells it why.
- **Cancel.** `call:cancel()` and a `timeout` fail the call at once and ask the platform to dismiss the screen. The cover lasts until the platform reports the screen gone, and that end reaches no app. A screen that waits ends at once and never reaches the platform.

```lua
-- plugins/paywall/source/init.lua
local platform = require('haylen.platform')

local handle = platform.plugin('paywall')
local paywall = {}

-- Shows the paywall of the offering, whose call answers with the purchase. The state comes back with a restored end.
function paywall.show(offering, state)
    return handle:openScreen('offer', {offering = offering}, {state = state})
end

-- An end that arrived after the app started again, with the state that the earlier app gave to show.
function paywall.onRestored(listener)
    return handle:on('screenRestored', listener)
end

return paywall
```

```lua
-- source/main.lua
local async = require('async')
local paywall = require('paywall')

paywall.onRestored(function(ending)
    if ending.result then
        print('bought ' .. ending.result.product .. ' on level ' .. ending.state.level)
    end
end)

async.spawn(function()
    local purchase, err = paywall.show('gold', {level = 12}):await()
    print(purchase and purchase.product or err.code)
end)
```

### The native contract

Every platform implements the same contract, which `Host::openScreen` and `Host::cancelScreen` of `engine/src/platform/Host.hpp` state for the engine.

1. The engine hands the platform a `platform::ScreenRequest`: the id of the screen, unique in the process, the id of the plugin, the name of the screen, the parameters as JSON with their byte buffers, the state and whether the screen is opaque.
2. The platform opens the screen that the native part of the plugin registered under that name. A name that nothing registered fails with the code `noHandler`, and a platform that cannot present the screen at that moment fails it with the code `notActive`.
3. The platform keeps the pending screen, its id, plugin, name and state, where it survives the end of the process: in the saved state of the activity on Android and in an entry of the session storage of the page on the web. Apple platforms keep it in the process, since the system brings back no controller of an app that it ended, and the desktops keep it nowhere, since their processes do not end under an app.
4. The native side ends the screen exactly once, from any thread, through `ScreenRelay::finish(id, ok, resultJson, buffers)`: with its result, or with a failure of `message`, `code` and `data`, such as `cancelled` when the person closed the screen or a code of its own.
5. When the app gives the screen up, `Host::cancelScreen(id)` asks the platform to dismiss it where it can, and the native side still ends it once it is gone.
6. When the process ended while the screen showed, the platform reads the pending screen back once the app starts again and hands its end to `ScreenRelay::restore(plugin, name, stateJson, ok, resultJson, buffers)`, which the engine delivers to the first app as `screenRestored`.

Native libraries come first, as with their handlers: a screen that a library registered opens there, on every platform that loads native libraries, and the platform gets every other screen.

| Platform | Screens |
| --- | --- |
| Web | Page screens that the web modules of plugins register, which open popups, redirect or show UI of the page, as [web screens](#web-screens) describes. |
| macOS player, Windows and Linux apps | Screens of native libraries, as [desktop screens](#desktop-screens) describes. Windows and Linux fail every other screen with the code `noHandler`. |
| iOS, iPadOS, Mac Catalyst, tvOS and the macOS app | Screens that plugin classes register, as [Apple screens](#apple-screens) describes, and screens of native libraries, where the app loads them. |
| Android | Screens that plugin classes register, as [Android screens](#android-screens) describes, and screens of native libraries. |

#### Web screens

`context.registerScreen(name, open)` of the web module of a plugin registers the screen `<id>.<name>`. The runtime calls `open(params, screen)` while it opens the screen, in the frame after the app asked for it, with the parameters, where bytes of the app arrive as `Uint8Array` values, and the screen.

| Member | Meaning |
| --- | --- |
| `screen.id`, `screen.name` | The id of the screen and its name. |
| `screen.token` | A random text that names the screen for the pages of its popup or redirect, which bring it back with their answer, such as the `state` of an OAuth request. |
| `screen.signal` | An `AbortSignal` that aborts when the app gives the screen up. |
| `screen.resolve(value)`, `screen.reject(error)` | End the screen with its result, whose `ArrayBuffer` and `Uint8Array` values cross as bytes, or with an error whose `code` and `data` the call keeps, like a failure of a handler. The first end counts. |
| `screen.popup(url, options)` | Opens a popup at once and returns a promise of its answer. `options.width` and `options.height` size it, 480 by 640 by default, and `options.origin` names another origin whose pages may answer besides the origin of the app. The promise fails with the code `popupBlocked` when the browser blocks the popup and with the code `cancelled` when the popup closes without an answer. |
| `screen.redirect(url)` | Keeps the pending screen in the session storage of the page and leaves the page for the address, or ends the screen with the code `storageUnavailable` when the session storage cannot keep it. |

`open` may be an async function, and an error it throws, or a promise it returns that fails, ends the screen with that error. A screen whose name no module registered fails with the code `noHandler`. When the app gives the screen up, the runtime aborts `screen.signal`, closes the popup of the screen and ends the screen as `cancelled`.

**Popups.** The page in the popup answers with a message `{haylenScreen: token, result}` or `{haylenScreen: token, error: {message, code, data}}`. It posts the message to `window.opener` with the origin of the app, and when it has no opener, such as after a page with `Cross-Origin-Opener-Policy: same-origin` cut the link, on the `BroadcastChannel` named `haylen-screens`, which only pages of the same origin reach. The runtime takes a message only when its token names the screen that shows and it comes from the popup of the screen with the origin of the app or `options.origin`, or from the channel, and then closes the popup. A sign-in or payment provider therefore returns to a callback page on the site of the app, which posts the answer.

```js
// plugins/checkout/web/checkout.js
export default function load(context) {
    context.registerScreen("pay", async (params, screen) => {
        // The popup opens at once, inside the activation of the tap, and the provider returns to callback.html of this site with the answer.
        const callback = new URL("callback.html", import.meta.url);
        callback.hash = screen.token;
        const answer = await screen.popup("https://pay.example.com/checkout?cart=" + params.cart + "&return=" + encodeURIComponent(callback.href), { width: 480, height: 700 });
        screen.resolve({ paid: answer.paid, receipt: answer.receipt });
    });
}
```

```html
<!-- plugins/checkout/web/callback.html -->
<script>
    const query = new URLSearchParams(location.search);
    const message = { haylenScreen: location.hash.slice(1), result: { paid: query.get("status") === "paid", receipt: query.get("receipt") } };
    if (window.opener) {
        window.opener.postMessage(message, location.origin);
    } else {
        new BroadcastChannel("haylen-screens").postMessage(message);
    }
    window.close();
</script>
```

**Redirects.** Providers that only redirect, and phones that open popups as tabs, suit `screen.redirect(url)`. The runtime keeps `{id, plugin, screen, token, state}` in the session storage of the page and leaves for the address, which stops the app. The provider returns to the page of the app with the answer in the address, the page loads again, and the runtime hands the kept screen to the context of its plugin as `context.restoredScreen`, with `id`, `name`, `token`, `resolve(value)` and `reject(error)`, or `null` without one. The module reads the answer from the address in `load`, ends the screen and cleans the address with `history.replaceState`, and the runtime delivers the end to the app that starts as `screenRestored`, with the state that the earlier app gave. A module that finds no answer in the address rejects the screen with the code `cancelled`, so the app learns that the flow ended. A page that the browser brings back from its cache after the redirect, with the app still in it, ends the pending screen as `cancelled` too.

```js
export default function load(context) {
    context.registerScreen("signIn", (params, screen) => {
        screen.redirect("https://accounts.example.com/authorize?client_id=" + context.config.clientId + "&state=" + screen.token + "&redirect_uri=" + encodeURIComponent(location.href));
    });

    // The provider came back to this page with the code and the state that names the screen.
    const returned = context.restoredScreen;
    if (returned) {
        const address = new URL(location.href);
        const code = address.searchParams.get("code");
        if (code && address.searchParams.get("state") === returned.token) {
            returned.resolve({ code });
        } else {
            returned.reject(Object.assign(new Error("The sign-in came back without a code."), { code: "cancelled" }));
        }
        address.search = "";
        history.replaceState(history.state, "", address.href);
    }
}
```

The rules of browsers apply to every page screen.

- **Activation.** Browsers open a popup only right after a click, a tap or a key press of the person, which `screen.popup` reports with the code `popupBlocked` otherwise. The runtime opens the screen in the frame after the app asked for it, so an app that opens the screen in reaction to a tap, such as from a button of `haylen.ui`, stays within the few seconds that browsers allow. An app that first awaits a network request may take too long, so it opens the screen first and lets the page of the popup load what it needs, or asks the person to tap again. Safari is stricter than other browsers with popups that do not open while the handler of the event runs.
- **Cross-origin opener policy.** `Cross-Origin-Opener-Policy: same-origin` on the page of the app puts every popup of another origin into a browsing context group of its own, which cuts the link between the popup and the app, so the popup cannot answer through `postMessage` and the app cannot tell when it closes. The web runtime is single-threaded and needs no cross-origin isolation, so pages leave the header out, as `haylen.py serve` does unless `--coop` asks for it, which the [distribution guide](distribution.md#serve) describes. A site that needs isolation for other reasons sends `same-origin-allow-popups`, or lets its callback pages answer on the `haylen-screens` channel.
- **Mobile browsers** open popups as tabs, which hides the tab of the app and sends the app to the background while the popup shows.

#### Desktop screens

A native library registers a screen with `registerScreen(plugin, name, open, cancel, user)` of [`HaylenNativeApi`](lua-api/native.md#library-handlers). The engine calls `open(user, screen, paramsJson, buffers, bufferCount)` on the frame thread once it covered the app, and the library opens a window of its own over the window of the app, which `getWindow` hands it, and ends the screen with `finishScreen(screen, ok, resultJson, buffers, bufferCount)` from any thread when the window closes. `cancel(user, screen)` asks it to close the window of a screen that the app gave up, and the library still calls `finishScreen` then. `coverApp` and `uncoverApp` cover the app for other native UI of a library, such as the overlay of an SDK.

| Desktop | Window of the app | Window of a screen |
| --- | --- | --- |
| macOS | `handle` is the `NSWindow*`, which Objective-C reads back with `(__bridge NSWindow*)handle`. The frame thread is the main thread of AppKit. | A sheet with `beginSheet:completionHandler:`, which keeps the events of the window of the app away while it shows, or a child window with `addChildWindow:ordered:`, which suits borderless and transparent windows. |
| Windows | `handle` is the `HWND`. The frame thread runs the message loop of the app, which dispatches the messages of every window that the thread creates. | A window that the window of the app owns, created with it as `hWndParent`, which stays above it and hides with it, after `EnableWindow(app, FALSE)`, which the library undoes before it destroys its window. |
| Linux | `handle` is the X11 `Window`, read back with `(Window)(uintptr_t)handle`, and `display` is the `Display*` of the engine, which belongs to the frame thread. | A window on a connection of its own from `XOpenDisplay`, whose events a thread of the library reads, marked with `XSetTransientForHint` for the window of the app and with `_NET_WM_WINDOW_TYPE_DIALOG` and `_NET_WM_STATE_MODAL`. |

The confirm screen of the [demo plugin](#screens) opens each of these windows.

#### Apple screens

`registerScreen:handler:` of the context of a plugin class, `registerScreen(_:handler:)` in Swift, registers the screen `<id>.<name>`. The runtime calls the handler on the main queue once the engine covered the app, with the parameters, where bytes arrive as `NSData` values, and a `HaylenScreen`, through which the handler shows its UI and which it ends. A name that no plugin registered fails with the code `noHandler`, and a screen that finds no window of the app to show over fails with the code `notActive`.

| Member | Meaning |
| --- | --- |
| `name`, `opaque` | The name of the screen and whether the app asked for an opaque one. |
| `presentViewController:`, `present(_:)` in Swift | iOS, iPadOS, Mac Catalyst and tvOS. Presents the controller from the topmost presented controller, once a running transition ended, inside a container that sees every way it goes away. A controller that keeps the automatic presentation style presents full screen for an opaque screen and as a sheet otherwise, and SwiftUI views present through a `UIHostingController`. |
| `presentWindow:`, `presentWindow(_:)` | The controller in a window of its own: on Mac Catalyst and on iPads that show several windows of an app a scene of the app, which fails the screen with the code `unsupported` on other devices, and on macOS an `NSViewController`, such as an `NSHostingController`, in a child window of the window of the app, which moves with it and keeps its level. Closing the window ends the screen, and every other end of the screen, its answer, a failure, a cancel of the app or its timeout, closes the window, which on Mac Catalyst and iPads destroys its scene with `requestSceneSessionDestruction`, also when the scene connects only after the screen ended. |
| `presentSheet:`, `presentSheet(_:)` | macOS. The `NSViewController` in a sheet of the window of the app. |
| `presenter`, `window` | The topmost presented controller on iOS, iPadOS, Mac Catalyst and tvOS and the window of the app on macOS, for SDKs that present their UI themselves. |
| `finishWithResult:`, `finish(_:)` | Ends the screen with its result, any value `NSJSONSerialization` accepts, with `NSData` values that cross as bytes. |
| `failWithMessage:code:data:`, `fail(_:code:data:)` | Ends the screen with a failure, whose code and data the call keeps. |
| `cancelHandler` | Runs when the app gives the screen up, after the runtime began to dismiss the UI it shows, so the UI of an SDK that presents itself closes too. A screen that shows nothing through the runtime and has no cancel handler ends as `cancelled` at once. |

The first end counts. The runtime then dismisses the UI it shows and hands the end to the engine once the UI is gone, so the cover lasts exactly as long as the UI. UI that goes away before the screen ended ends it with the code `cancelled`, which covers the swipe of the person, which the delegate of the presentation hears, the Menu button of a TV remote, the close button of a window, and a dismissal by the controller itself, by the `dismiss` action of SwiftUI or by an SDK, which the disappearance of the container shows. The methods `finish` and `fail` work from any thread. The pending screen lives in the process, and a plugin whose flow comes back through a link after the process ended sends that answer as an event of its own.

The app draws in one window, so on Mac Catalyst and iPad the windows of screens connect scenes with a delegate of their own, and any other scene of the app that the system connects, such as a second window that the person opens on an iPad, goes away at once instead of drawing the app twice. The `Info.plist` of the Apple template therefore declares support for several scenes.

```swift
// A paywall of an SDK in a controller that reports the purchase, which ends the screen and dismisses the controller.
context.registerScreen("offer") { (params: Offer, screen: HaylenScreen) in
    let controller = PaywallController(offering: params.offering) { purchase in
        try? screen.finish(encoding: purchase)
    }
    screen.present(controller)
}
```

#### Android screens

The context of a plugin class registers the screen `<id>.<name>` in `onLoad`, in one of two ways.

- **An Activity Result contract.** `registerScreen(name, contract, input, output)` takes an `ActivityResultContract`, a `HaylenScreen.Input` that turns the parameters of the app into the input of the contract, and a `HaylenScreen.Output` that turns the output of the contract into the result of the screen, any value that `reply.success` takes. Every `HaylenActivity` registers the launcher of the contract under the stable key `haylen.<id>.<name>` in `onCreate`, before it starts, so a result that Android delivers to a new activity, after the activity was recreated or the process ended while the screen showed, finds its launcher. A `null` output, which the contracts of AndroidX give when the person backs out, ends the screen with the code `cancelled`, and a thrown `HaylenBridge.Failure` ends it with its code and data.
- **An opener.** `registerScreen(name, opener)` takes a `HaylenScreen.Opener`, which the runtime calls on the main thread with the parameters and the `HaylenScreen`, for UI that the plugin shows itself, such as the launcher of an SDK that the plugin created in `onActivityCreated`.

The runtime opens the screen on the main thread once the engine covered the app. A name that no plugin registered fails with the code `noHandler`, and a screen that finds the activity away from the foreground with the code `notActive`. `HaylenScreen` has these members:

| Member | Meaning |
| --- | --- |
| `name()`, `isOpaque()` | The name of the screen and whether the app asked for an opaque one. |
| `finish(result)` | Ends the screen with its result, converted like the value of `reply.success`, with `byte[]` and `ByteBuffer` values as bytes. |
| `fail(message, code, data)`, `fail(throwable)` | End the screen with a failure, such as the code `cancelled` when the person closed the UI, whose code and data the call keeps. A `HaylenBridge.Failure` keeps its code and data, and any other error fails with the code `exception`. |
| `onCancel(listener)`, `isCancelled()` | The listener runs on the main thread when the app gives the screen up, so the plugin closes the UI it shows and ends the screen. A screen without listeners, and every contract screen, ends with the code `cancelled` at once, and the answer that its launcher may still receive goes nowhere. A contract screen also finishes the activity that its launcher started. |
| `isRestored()` | Whether the process ended while the screen showed. |

The first end counts, from any thread. The activity keeps the screen that shows, its id, plugin, name, state and opacity, in its saved state, under the key `haylen.screens` of its `SavedStateRegistry`. When the process ended while the screen showed, the activity that Android recreates reads it back, and the end of the screen reaches the next app as the retained event `<id>.screenRestored` with the state that the earlier app gave: the launcher of a contract screen receives the result that Android delivers again, and the plugin of an opener screen, whose answer arrives through its own launcher, ends the screen that `context.restoredScreen()` returns. An activity that only the process outlived, such as one that Android recreated, keeps showing the screen of the running process, whose end reaches the app that runs by then.

```kotlin
// plugins/confirm-kit/android/src/main/kotlin/com/example/confirmkit/ConfirmKitPlugin.kt
private var launcher: ActivityResultLauncher<String>? = null
private var pending: HaylenScreen? = null

override fun onLoad(context: HaylenPluginContext) {
    // The activity of the plugin answers true or false, and Back gives null, which ends the screen as cancelled.
    context.registerScreen("confirm", ConfirmContract(), HaylenScreen.Input { params -> params as JSONObject }, HaylenScreen.Output { confirmed -> JSONObject().put("confirmed", confirmed) })

    // A screen of an SDK, which reports its end through a callback of its own.
    context.registerScreen("offer") { params, screen ->
        pending = screen
        launcher?.launch((params as JSONObject).getString("offering"))
    }
}

// The launcher of the SDK belongs to the activity. A purchase that ends after the process ended reaches the screen that the process ended under.
override fun onActivityCreated(activity: HaylenActivity, savedInstanceState: Bundle?) {
    launcher = activity.activityResultRegistry.register("confirm-kit.offer", activity, OfferContract()) { purchase ->
        val screen = pending ?: context.restoredScreen()
        pending = null
        if (purchase == null) screen?.fail("The person closed the offer.", "cancelled", null) else screen?.finish(JSONObject().put("product", purchase))
    }
}
```

While another activity covers the app, the app keeps ticking in the background without drawing, as [the lifecycle guide](lifecycle.md#app-states) describes, so a cancel or a timeout that the app asks for reaches the screen while it shows, whether the screen is an activity or UI in the activity of the app, such as a dialog or a view of the overlay. `HaylenActivity` hands the request code of every activity that it starts for a result to the contract screen whose launcher starts it, and a cancel finishes that activity with `finishActivity`. The prompt of a contract that asks for permissions is no such activity, so it stays until the person answers it.

### C++

C++ apps and engine plugins open screens through `platform::Screens`, which `engine.getScreens()` returns.

| Member | Meaning |
| --- | --- |
| `open(plugin, screen, params, options, callback)` | Asks the plugin to open its screen with a `Bridge::Payload` of parameters and `Screens::Options` with `state`, `opaque` and `timeout`, and returns the id of the screen. A screen that the app asks for while it is inactive in the foreground waits until it is active. The callback receives the `Bridge::Result` of the screen at the start of a frame, `busy` and `notActive` included. |
| `cancel(id)` | Gives the screen up, which fails with the code `cancelled` at the next pump, and returns whether it was pending. |
| `isShowing()` | Whether the screen of a plugin shows, whichever app of the process opened it. |

```cpp
engine.getScreens().open("paywall", "offer", {.json = {{"offering", "gold"}}}, {.state = {{"level", 12}}}, [](haylen::platform::Bridge::Result result) {
    haylen::core::Log::info("The paywall ended with {}", result.ok ? result.value.json.dump() : result.error.message);
});
```

## Requirements

The Android and Xcode projects of an app belong to its developer, so the engine forces no permission, framework, component or declaration into them. Each plugin declares what it needs, the developer keeps it or takes it out, and the plugin checks the requirement at run time, before it calls the system API that needs it. When a requirement is missing, the plugin logs once what is missing and how to add it and fails the call with the code `unsupported`, whose `data.missing` lists every missing requirement, instead of crashing the app. The features of the engine follow the same rule, and the ones that answer nothing, such as `system.vibrate`, just do nothing after the log.

| Field of each entry of `data.missing` | Meaning |
| --- | --- |
| `kind` | What is missing, such as `permission` or `class`. |
| `name` | The name of what is missing, such as `android.permission.READ_CONTACTS`. |
| `file` | The file of the platform project that declares it, relative to the project, such as `app/src/main/AndroidManifest.xml` or `ios/Info.plist`, and empty on the web, where no file of a project adds a requirement. |
| `snippet` | The text that adds it to that file. On the web it is the text that allows a feature of the permissions policy, and empty for the other requirements. |

```lua
local async = require('async')
local contacts = require('contacts')

async.spawn(function()
    local _, err = contacts.pick():await()
    if err and err.code == 'unsupported' and err.data and err.data.missing then
        for _, missing in ipairs(err.data.missing) do
            if missing.file ~= '' then
                print(string.format('Add "%s" to "%s".', missing.snippet, missing.file))
            else
                print(string.format('The page lacks the %s "%s".', missing.kind, missing.name))
            end
        end
    end
end)
```

### Android requirements

`context.requirements()` of an Android plugin returns its `dev.haylen.HaylenRequirements`, which reads the merged manifest and the build of the app from any thread. The manifest stays the same while the process lives, so it reads the package once.

| Member | Meaning |
| --- | --- |
| `hasPermission(name)` | Whether the merged manifest declares the permission, from the `requestedPermissions` of the package. |
| `isGranted(name)` | Whether the app holds the permission now. A normal permission, such as `VIBRATE`, is granted exactly when it is declared, while a dangerous one, such as `CAMERA`, needs the person to grant it, which the plugin asks for with the [Activity Result API](#activity-results-and-permissions). |
| `hasClass(name)` | Whether the build has the class, which a dependency brings and R8 may have removed. |
| `hasMetaData(name)` | Whether the `<application>` of the merged manifest has the meta-data entry. |
| `hasActivity(className)`, `hasService(className)` | Whether the merged manifest declares the component. |
| `hasProvider(authority)` | Whether the merged manifest declares a provider with the authority. |
| `handlesScheme(scheme)` | Whether an activity of the app opens the links with the scheme. |
| `require(requirements...)` | Throws a `HaylenBridge.Failure` with the code `unsupported` and the data `{missing}` when the project lacks any of the requirements, after it logged each missing one once with the tag `haylen`. A handler that lets it through fails its call with it. |

`HaylenRequirements.Requirement` makes the requirements that `require` takes, each with the file and the snippet that add it:

| Factory | `kind` | `file` and `snippet` |
| --- | --- | --- |
| `permission(name)` | `permission` | `app/src/main/AndroidManifest.xml` with `<uses-permission android:name="<name>" />`. |
| `className(name, dependency)` | `class` | `app/build.gradle.kts` with `implementation("<dependency>")`. |
| `metaData(name, value)` | `metaData` | The manifest with `<meta-data android:name="<name>" android:value="<value>" />`. |
| `activity(className)`, `service(className)` | `activity`, `service` | The manifest with the component, not exported. |
| `provider(authority, className)` | `provider` | The manifest with the provider of the authority, not exported. |
| `urlScheme(scheme)` | `urlScheme` | The manifest with `HaylenLinkActivity` and an intent filter of the scheme. |

A permission requirement checks what the project declares, so a dangerous permission passes once the manifest declares it, and the plugin then asks the person for it.

```kotlin
context.register("pick") { _, reply ->
    context.requirements().require(HaylenRequirements.Requirement.permission(Manifest.permission.READ_CONTACTS))
    pending = reply
    picker?.launch(null)
}
```

The log line names the owner, what it needs, what happens without it and how to add it:

```text
The plugin "contacts" needs the permission "android.permission.READ_CONTACTS", which the app lacks, so the calls that need it fail with the code "unsupported". Add "<uses-permission android:name="android.permission.READ_CONTACTS" />" to "app/src/main/AndroidManifest.xml".
```

The engine checks its own features the same way. The manifest of the Android template declares their three permissions, which an app that does not use a feature deletes:

| Permission | Feature | Without it |
| --- | --- | --- |
| `INTERNET` | Network access: HTTP, sockets and `haylen.net`. | Connections fail. The errors of `haylen.net` end with a sentence that names the missing permission and how to add it, while the `http` and `socket` modules of Varn report what the system tells them. |
| `ACCESS_NETWORK_STATE` | The network state of [`haylen.networkState()`](lua-api/haylen.md#haylennetworkstate) and the events `networkOnline` and `networkOffline`. | The engine never follows the network, so the state stays `'unknown'` and the events never fire, which the log tells once at the info level. |
| `VIBRATE` | [`system.vibrate`](lua-api/system.md#systemvibrateseconds). | It does nothing, which the log tells once as a warning. |

### Apple requirements

`context.requirements` of an Apple plugin is its `HaylenRequirements`, declared in `haylen/platform/apple/HaylenRequirements.h`, which reads the `Info.plist`, the linked classes and, on macOS and Mac Catalyst, the entitlements of the app from any thread. A usage description matters most, since the system ends an app that asks for a permission without the usage description of that permission, so a plugin checks it before the call that asks.

| Member | Meaning |
| --- | --- |
| `hasInfoPlistKey:` | Whether the `Info.plist` has the key. |
| `hasUsageDescription:` | Whether the `Info.plist` gives the usage description a text, such as `NSCameraUsageDescription`. |
| `hasBackgroundMode:` | Whether `UIBackgroundModes` lists the mode. |
| `hasURLScheme:` | Whether a scheme of `CFBundleURLTypes` matches the scheme, ignoring case. |
| `hasClass:` | Whether the app links the class, which its framework or SDK brings. |
| `hasEntitlement:` | macOS and Mac Catalyst. Whether the app signs with the entitlement and its value is not `false`, through `SecTaskCopyValueForEntitlement` of the Security framework, which the runtime loads the first time a plugin asks, so apps need not link it. iOS and tvOS do not tell an app its entitlements, so a plugin there learns of a missing one from the error of the API that needs it, such as `application:didFailToRegisterForRemoteNotificationsWithError:` without `aps-environment`. |
| `missing:` | The requirements of a list that the project lacks, in their order, without a log. |
| `require:error:` | Returns `NO` with an error when the project lacks any of the requirements, after it logged each missing one once as a warning. The user info of the error holds the failure of the call, `message`, the code `unsupported` and `data` with `missing`, so an Objective-C handler replies with `reply(NO, error.userInfo)`. |

`HaylenRequirement` makes the requirements, each with the file and the snippet that add it:

| Factory | `kind` | `file` and `snippet` |
| --- | --- | --- |
| `infoPlistKey:value:`, `.infoPlistKey(_:value:)` in Swift | `infoPlistKey` | The `Info.plist` of the platform with `<key>` and `<string>` of the key and its text. |
| `usageDescription:`, `.usageDescription(_:)` | `usageDescription` | The `Info.plist` with the key and a text that tells the person why the app asks, which the developer replaces. |
| `backgroundMode:`, `.backgroundMode(_:)` | `backgroundMode` | The `Info.plist` with `UIBackgroundModes` and the mode. |
| `urlScheme:`, `.urlScheme(_:)` | `urlScheme` | The `Info.plist` with `CFBundleURLTypes` and the scheme. |
| `className:framework:`, `.className(_:framework:)` | `class` | `project.yml` with `- sdk: <framework>` for the dependencies of the target, such as `- sdk: UserNotifications.framework`. |
| `entitlement:`, `.entitlement(_:)` | `entitlement` | The entitlements file of the platform with the key and `<true/>`. It counts as missing only on macOS and Mac Catalyst. |

The `Info.plist` of the platform is `ios/Info.plist` on iOS and Mac Catalyst, `tvos/Info.plist` on tvOS and `macos/Info.plist` on macOS, and its entitlements file is `ios/App.entitlements`, `catalyst/App.entitlements`, `tvos/App.entitlements` or `macos/App.entitlements`, all relative to the Apple project. Swift plugins check with the `context.require` helper of the template, which throws the failure as a `HaylenFailure`:

```swift
context.register("pick") { (_: Empty) async throws -> Contact in
    try context.require(.usageDescription("NSContactsUsageDescription"))
    return try await picker.pick()
}
```

```objc
[context registerHandler:@"pick" handler:^(id params, HaylenReply reply) {
    NSError* error = nil;
    if (![context.requirements require:@[ [HaylenRequirement usageDescription:@"NSContactsUsageDescription"] ] error:&error]) {
        reply(NO, error.userInfo);
        return;
    }
    [picker pickWithReply:reply];
}];
```

```text
The plugin "contacts" needs the usage description "NSContactsUsageDescription", which the app lacks, so the calls that need it fail with the code "unsupported". Add "<key>NSContactsUsageDescription</key><string>Tell the person why the app asks for this.</string>" to "ios/Info.plist".
```

### Web requirements

`context.require(needs)` of the web module of a plugin checks what the page offers and throws an error with the code `unsupported` and `data.missing` when the page lacks any of it, after it logged each missing requirement once, as a warning on the console and through the `onLog` callback of the page, so a handler that lets it through fails its call with it. A key of `needs` other than the three of the table throws a `TypeError`.

| Key of `needs` | Value | Missing when | `kind` and `name` |
| --- | --- | --- | --- |
| `secureContext` | `true` | The page is not a secure context, as a page served over http from an address other than `localhost`, where browsers hold back the camera, the microphone, the Contact Picker and other powerful features. | `secureContext` and `https`. |
| `api` | A path or a list of paths | The browser lacks the API, a path of properties from the global object such as `navigator.contacts`. | `api` and the path. |
| `permissionsPolicy` | A feature or a list of features | The permissions policy of the page does not allow the feature, such as `camera`, which a page in a frame, as in a web editor, gets from the `allow` attribute of its frame, and a page on its own from the `Permissions-Policy` header it comes with. Browsers that do not tell the policy, such as Firefox and Safari, count every feature as allowed, and the error of the API tells instead. | `permissionsPolicy` and the feature, with the snippet `allow="camera"` for a page in a frame and `Permissions-Policy: camera=(self)` for a page on its own. |

```js
context.register("pick", async () => {
    context.require({ secureContext: true, api: "navigator.contacts" });
    const [contact] = await navigator.contacts.select(["name"]);
    return { name: contact ? contact.name[0] : null };
});
```

```text
The plugin "contacts" needs the API "navigator.contacts", which the page lacks, so the calls that need it fail with the code "unsupported". Run the app in a browser that offers it.
```

## The Apple part

The Apple part of a plugin is Swift or Objective-C in the `sources` folder of its `apple` section, which compiles into the targets of the Apple template for the platforms the plugin lists, next to the files of the app. The template's `source/HaylenBridging.h` imports `haylen/platform/apple/HaylenBridge.h`, `haylen/platform/apple/HaylenPlugin.h` and `haylen/platform/apple/HaylenNotificationPlugin.h`, so Swift sources reach the whole plugin API without imports of their own, and Objective-C sources import `HaylenPlugin.h`, or `HaylenNotificationPlugin.h` for a [notification plugin](#notifications). The headers of UserNotifications that `HaylenNotificationPlugin.h` brings link nothing, so an app links UserNotifications only when its code uses the framework.

### The plugin class

The `class` of the `apple` section names a class that conforms to the `HaylenPlugin` protocol. While the app launches, inside `application:willFinishLaunchingWithOptions:` on iOS, tvOS and Mac Catalyst and inside `applicationWillFinishLaunching:` on macOS, the runtime reads the plugins that `app.json` of the bundled package lists and the `plugin.json` of each, in [load order](#load-order), creates the class of every plugin that lists the platform once with `init` and calls `loadWithContext:`, `load(with:)` in Swift, with its context and the parameters of the plugin, so SDKs set up before launching ends. A Swift class names its Objective-C class with `@objc(Name)`, the name that `class` repeats. The runtime reports the ids it loaded, which `handle.native` and `platform.plugins()` show in Lua. The desktop player bundles no package, since it runs the package that its command line names, so it creates no plugin classes and runs the [native libraries](#native-libraries) of the plugins instead.

- A plugin that does not list the destination, such as a plugin for `ios` alone on Mac Catalyst, is skipped without a message, and runs without its native part there.
- A class that is missing on a platform its plugin lists is logged as an error that names the class, the plugin and what the project lacks, the Apple sources of the plugin, which a target compiles when it names its template of `haylen/project.yml` in `templates` of `project.yml`, and a class that does not conform to `HaylenPlugin` is logged as an error too. The plugin then runs without its native part.

```swift
// plugins/share-sheet/apple/ShareSheetPlugin.swift
import UIKit

@objc(ShareSheetPlugin)
final class ShareSheetPlugin: NSObject, HaylenPlugin {
    struct Share: Decodable {
        let text: String
    }

    struct Shared: Encodable {
        let completed: Bool
    }

    func load(with context: HaylenPluginContext) {
        // Answers share-sheet.share once the person closes the sheet, which covers the app while it shows.
        context.register("share") { (params: Share) async throws -> Shared in
            guard let presenter = context.viewController else {
                throw HaylenFailure("The app has no window yet.", code: "noWindow")
            }
            let sheet = UIActivityViewController(activityItems: [params.text], applicationActivities: nil)
            sheet.popoverPresentationController?.sourceView = presenter.view
            context.coverApp()
            defer { context.uncoverApp() }
            return await withCheckedContinuation { continuation in
                sheet.completionWithItemsHandler = { _, completed, _, _ in continuation.resume(returning: Shared(completed: completed)) }
                presenter.present(sheet, animated: true)
            }
        }
    }
}
```

The same plugin in Objective-C:

```objc
// plugins/share-sheet/apple/ShareSheetPlugin.m
#import <UIKit/UIKit.h>

#import "haylen/platform/apple/HaylenPlugin.h"

@interface ShareSheetPlugin : NSObject <HaylenPlugin>
@end

@implementation ShareSheetPlugin

- (void)loadWithContext:(HaylenPluginContext*)context {
    [context registerHandler:@"share" handler:^(id params, HaylenReply reply) {
        UIViewController* presenter = context.viewController;
        if (presenter == nil) {
            reply(NO, @{@"message" : @"The app has no window yet.", @"code" : @"noWindow"});
            return;
        }
        UIActivityViewController* sheet = [[UIActivityViewController alloc] initWithActivityItems:@[ params[@"text"] ] applicationActivities:nil];
        sheet.popoverPresentationController.sourceView = presenter.view;
        [context coverApp];
        sheet.completionWithItemsHandler = ^(UIActivityType type, BOOL completed, NSArray* items, NSError* error) {
            [context uncoverApp];
            reply(YES, @{@"completed" : @(completed)});
        };
        [presenter presentViewController:sheet animated:YES completion:nil];
    }];
}

@end
```

### The context

`HaylenPluginContext` is the part of the runtime that a plugin sees. Registering, emitting, covering, checking requirements and opening streams work from any thread, and handlers run on the main queue, while the window, the view controller and the overlay belong to the main thread, which Swift enforces with the main actor.

| Member | Meaning |
| --- | --- |
| `identifier` | The id of the plugin. |
| `config` | The parameters of the plugin in `app.json` over the `default` of every parameter of `plugin.json`, the same values as `handle.config` in Lua, as an `NSDictionary`. |
| `registerHandler:handler:`, `registerCancellableHandler:handler:` | Answer `<id>.<method>`, like the handlers of `HaylenBridge` in the [platform bridge guide](platform_bridge.md#apple-platforms). |
| `registerScreen:handler:` | Opens the screen `<id>.<name>`, as [Apple screens](#apple-screens) describes. |
| `openVideoStream:width:height:format:`, `openAudioStream:sampleRate:channels:format:capacity:` | Open the video or audio [stream](#streams) of a name, or return the one that is open, and return `nil` and log why for arguments the stream cannot take. |
| `emit:payload:`, `emitRetained:payload:`, `emit:payload:retain:batched:` | Send the event `<id>.<event>`, retained for the first listener of its name with `emitRetained:payload:`, and retained, batched or both with `emit:payload:retain:batched:`. A payload is any value `NSJSONSerialization` accepts, with `NSData` values that cross as bytes, or `nil`. |
| `overlay` | The overlay that places native views of the plugin over the app, as [overlays](#overlays) describes. |
| `requirements` | The `HaylenRequirements` of the plugin, which checks what the project of the app holds before the plugin calls a system API that needs it, as [Apple requirements](#apple-requirements) describes. |
| `coverApp`, `uncoverApp` | Cover the app while native UI of the plugin covers it, and end the cover, as [covering the app](#covering-the-app) describes. The covers of a plugin end when the window of the app goes away, and an `uncoverApp` without a cover of the plugin is logged as an error. |
| `viewController`, `windowScene` | The topmost view controller that the window of the app presents, the root view controller while nothing is presented, and the window scene of the app on iOS, tvOS and Mac Catalyst, for SDKs that present UI or need a scene. Both are `nil` until the scene connects. |
| `window` | The window of the app on macOS, `nil` until the app finished launching. |

Events that native code sends while no app runs, such as while the app launches or restarts, wait in the runtime and reach the next app in order once it starts. A link that opens the app, which the plugin sends retained, therefore reaches the first Lua listener of its name however late it comes.

### Events of the app

The runtime's application delegate hands every event below to the plugins that implement its method, in load order, on the main thread. On iOS, tvOS and Mac Catalyst it is `HaylenSceneDelegate`, which also delegates the scene, and on macOS `HaylenAppDelegate`. Both derive from the delegates of `sokol_app`, which the patch that the [distribution guide](distribution.md#notes-on-dependencies) describes lets the runtime name.

| Platforms | Methods |
| --- | --- |
| iOS, tvOS, Mac Catalyst | `application:willFinishLaunchingWithOptions:`, `application:didFinishLaunchingWithOptions:` |
| iOS, tvOS, Mac Catalyst | `scene:willConnectToSession:options:`, `scene:openURLContexts:`, `scene:continueUserActivity:`, `windowScene:performActionForShortcutItem:completionHandler:` (not tvOS) |
| iOS, tvOS, Mac Catalyst | `sceneDidBecomeActive:`, `sceneWillResignActive:`, `sceneWillEnterForeground:`, `sceneDidEnterBackground:` |
| iOS, tvOS, Mac Catalyst | `application:didRegisterForRemoteNotificationsWithDeviceToken:`, `application:didFailToRegisterForRemoteNotificationsWithError:`, `application:didReceiveRemoteNotification:fetchCompletionHandler:`, `application:handleEventsForBackgroundURLSession:completionHandler:` |
| macOS | `applicationWillFinishLaunching:`, `applicationDidFinishLaunching:`, `application:openURLs:`, `application:didRegisterForRemoteNotificationsWithDeviceToken:`, `application:didFailToRegisterForRemoteNotificationsWithError:`, `application:didReceiveRemoteNotification:` |
| Every Apple platform, for plugins that adopt `HaylenNotificationPlugin` | `userNotificationCenter:willPresentNotification:withCompletionHandler:`, `userNotificationCenter:didReceiveNotificationResponse:withCompletionHandler:` (not tvOS), as [notifications](#notifications) describes |
| Every Apple platform | `appDidFailWithError:`, `appDidFail(with:)` in Swift, with the report of every error that stops the app, `{message, file, line, traceback, frames}`, as [errors of the app](#errors-of-the-app) describes |

- **Links that open the app.** UIKit hands the links, the user activities and the shortcut item that launch an app only to the connection options of its scene. Right after `scene:willConnectToSession:options:`, the runtime hands them to `scene:openURLContexts:`, `scene:continueUserActivity:` and `windowScene:performActionForShortcutItem:completionHandler:`, so one method serves a link whether it opened the app or arrived while the app ran. The window of the app exists by then.
- **Completion handlers.** Each plugin that implements a method with a completion handler receives a handler of its own, which it calls once, and the runtime calls the handler of the system once after every plugin called its own, or at once when no plugin implements the method. A background fetch counts as new data when any plugin received new data, and otherwise as failed when any plugin failed. A shortcut action counts as handled when any plugin handled it, and a notification that arrives while the app is in front shows with the union of the options the plugins ask for, or not at all when none asks.
- **Remote notifications in the background.** UIKit warns about an application delegate that implements `application:didReceiveRemoteNotification:fetchCompletionHandler:` in an app without the `remote-notification` background mode, so the runtime takes that method, and hands it to the plugins, only when the `Info.plist` of the app lists `remote-notification` in `UIBackgroundModes`, which a plugin that receives remote notifications declares in the `infoPlist` of its `apple` section.
- **The window going away.** When the scene of the app disconnects, the runtime ends the covers that plugins left open and takes the overlay off the window, and the overlay comes back when a scene connects again.

### Notifications

The runtime refers to UserNotifications only through the Objective-C runtime, so the engine links no framework for notifications, and an app links UserNotifications only when its code uses it, as a plugin that posts or receives notifications does. Such a plugin declares `UserNotifications.framework` in the `frameworks` of its `apple` section and adopts `HaylenNotificationPlugin`, declared in `haylen/platform/apple/HaylenNotificationPlugin.h`, which adds the two methods of the delegate of the notification center to `HaylenPlugin`.

While the app launches, in `willFinishLaunching`, the runtime looks for `UNUserNotificationCenter` by its name. When the app links the framework and has plugins, the runtime makes its application delegate the delegate of the center, which is what the system needs to deliver the response to a notification that launched the app through `userNotificationCenter:didReceiveNotificationResponse:withCompletionHandler:`, and hands the events of the center to the plugins that implement their methods, as [events of the app](#events-of-the-app) describes. Plugins never replace the delegate. SDKs that swizzle the application delegate to find these events, such as Firebase with `FirebaseAppDelegateProxyEnabled`, can have that turned off in the `infoPlist` of their plugin, because the runtime already hands the events over.

```swift
// plugins/reminders/apple/RemindersPlugin.swift
import UserNotifications

@objc(RemindersPlugin)
final class RemindersPlugin: NSObject, HaylenNotificationPlugin {
    private var context: HaylenPluginContext!

    func load(with context: HaylenPluginContext) {
        self.context = context
    }

    // The reminders show while the app is in front too.
    func userNotificationCenter(_ center: UNUserNotificationCenter, willPresent notification: UNNotification, withCompletionHandler completionHandler: @escaping (UNNotificationPresentationOptions) -> Void) {
        completionHandler(notification.request.identifier.hasPrefix("reminders.") ? [.banner, .sound] : [])
    }

    // A tap that launched the app waits for the first listener of reminderOpened.
    func userNotificationCenter(_ center: UNUserNotificationCenter, didReceive response: UNNotificationResponse, withCompletionHandler completionHandler: @escaping () -> Void) {
        context.emitRetained("reminderOpened", payload: ["identifier": response.notification.request.identifier])
        completionHandler()
    }
}
```

### Overlays

`context.overlay` places native views of the plugin over the app, such as a banner. Its `addView:placement:`, `add(_:placement:)` in Swift, takes a view and a `HaylenPlacement` and returns a `HaylenOverlayItem`, `HaylenOverlay.Item` in Swift:

| Member | Meaning |
| --- | --- |
| `updatePlacement:`, `update(_:)` in Swift | Places the view again with another placement. |
| `visible`, `isVisible` in Swift | Shows or hides the view, which reserves its edge only while it shows. |
| `bounds` | The frame of the view over the app, in points from the top left corner, after the overlay laid it out. |
| `remove` | Takes the view off the overlay and gives its edge back. A removed item ignores later updates. |

A placement has the fields of the placements of the [web overlay](#web-modules), in points:

| Field | Default | Meaning |
| --- | --- | --- |
| `anchor` | `HaylenPlacementAnchorBottom`, `.bottom` in Swift | Where the view sits: top, bottom, left, right, the four corners or center. |
| `margin` | `0` | The distance from the edges the anchor names. |
| `insideSafeArea` | `YES` | Whether the view stays inside the safe area of the window, away from the home indicator, the camera housing and the overscan of TVs. |
| `reserve` | `NO` | Whether the view reserves the edge its anchor names, from the edge of the window to its far side, while it shows. A centered view reserves nothing. |
| `width`, `height` | `0` | The size of the view. Zero keeps the size its own constraints or its intrinsic content size give it, as for a button or a label. |

The overlay is a view over the view of the app. Touches and clicks reach the views of the overlay where they are, and the app everywhere else. It places its views with Auto Layout against its safe area layout guide, or against its edges when `insideSafeArea` is `NO`, so they follow rotation, iPad split view and resized Mac Catalyst and macOS windows, and whenever its layout changes it reports the edges that views reserve in framebuffer pixels, which the app sees as its safe area shrinking. A view added before the window exists shows once it does. On tvOS, and for the keyboard focus of iPad and Mac Catalyst, the views of the overlay never take the focus, so the remote and the keyboard keep driving the app, and a plugin whose UI the remote must drive presents a view controller over the app with a cover.

```swift
// Shows a banner at the bottom of the safe area, which the UI of the app anchored to the safe area moves above.
let placement = HaylenPlacement(anchor: .bottom)
placement.reserve = true
placement.width = 320
placement.height = 50
let banner = context.overlay.add(bannerView, placement: placement)

// Later, the banner moves to the top, and finally goes away.
banner.update(HaylenPlacement(anchor: .top))
banner.remove()
```

### Info.plist keys, entitlements and resources

The `infoPlist`, `entitlements` and `privacy` of the `apple` section merge into the files haylen.py writes in `haylen/` for every platform of the plugin, as [Apple platforms](#apple-platforms) describes, so a plugin declares there the URL schemes it answers, the background modes it needs, the usage descriptions of the permissions it asks for and the capabilities it uses:

```json
"apple": {
    "class": "PushPlugin",
    "sources": "apple",
    "frameworks": ["UserNotifications.framework"],
    "infoPlist": {
        "CFBundleURLTypes": [{"CFBundleURLName": "com.example.push", "CFBundleURLSchemes": ["${urlScheme}"]}],
        "UIBackgroundModes": ["remote-notification"]
    },
    "entitlements": {"aps-environment": "${apsEnvironment}"}
}
```

A URL scheme of `CFBundleURLTypes` opens the app, whose plugins receive the link in `scene:openURLContexts:`, or in `application:openURLs:` on macOS. Remote notifications need the `aps-environment` entitlement, which the app signs with through the entitlements file of its platform.

### Swift helpers

`source/HaylenBridgeAsync.swift` of the Apple template adds Swift helpers on top of the Objective-C API:

| Helper | Meaning |
| --- | --- |
| `context.register(method) { (params: Params) async throws -> Result in ... }` | Answers `<id>.<method>` with an async function on the main actor. `Params` decodes from the parameters of the call with `JSONDecoder` and `Result` encodes the answer with `JSONEncoder`. A thrown `HaylenFailure(message, code:, data:)` fails the call with its code and data, any other error fails it with the code `exception` and the type of the error in `data.type`, and the task of the call is cancelled when the app cancels the call or its timeout passes. |
| `try context.emit(event, payload, retain: false, batched: false)` | Sends `<id>.<event>` with an `Encodable` payload, retained when `retain` is `true` and batched when `batched` is `true`. It throws the error of the encoder when the payload does not encode. |
| `context.registerScreen(name) { (params: Params, screen: HaylenScreen) in ... }` | Opens the screen `<id>.<name>` with a handler on the main actor whose `Params` decode from the parameters of the app with `JSONDecoder`. A thrown `HaylenFailure` fails the screen with its code and data, and any other error fails it with the code `exception`. |
| `try screen.finish(encoding: result)` | Ends a screen with an `Encodable` result. It throws the error of the encoder when the result does not encode. |
| `try context.require(.usageDescription("NSCameraUsageDescription"), ...)` | Throws a `HaylenFailure` with the code `unsupported` and `data.missing` when the project lacks any of the [requirements](#apple-requirements), after it logged each missing one once, so a handler that lets it through fails its call with it. |
| `HaylenBridge.register(method) { ... }`, `try HaylenBridge.emit(event, payload, retain: false, batched: false)` | The same for handlers and events outside plugins, without the prefix. |

These helpers carry JSON alone, so bytes cross through the dictionaries of `registerHandler:handler:`, `emit:payload:` and `finishWithResult:`.

```swift
struct Purchase: Encodable {
    let product: String
    let token: String
}

// A purchase that finished while the app was closed reaches the first listener of purchaseUpdated.
try context.emit("purchaseUpdated", Purchase(product: "coins", token: transaction.jwsRepresentation), retain: true)
```

## The Android part

The Android part of a plugin is the Android library module that `module` of its `android` section names, which haylen.py includes in the Android project of the app as [Android](#android) describes. The module depends on `dev.haylen:haylen-plugins`, which brings the engine library with the AndroidX libraries it declares, AppCompat, the activity library and core, and the provider that loads plugins, and its manifest names the plugin class in a meta-data entry of its `<application>`, whose name is `dev.haylen.plugin.` followed by the id of the plugin:

```kotlin
// plugins/share-sheet/android/build.gradle.kts
// The Android project of the app gives every module the engine version it builds with.
val haylenEngineVersion: String by extra

dependencies {
    implementation("dev.haylen:haylen-plugins:$haylenEngineVersion")
}
```

```xml
<!-- plugins/share-sheet/android/src/main/AndroidManifest.xml -->
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <application>
        <meta-data android:name="dev.haylen.plugin.share-sheet" android:value="com.example.sharesheet.ShareSheetPlugin" />
    </application>
</manifest>
```

### Engine libraries

The engine library declares only what every app needs, so the parts that only some apps want are libraries of their own, which `haylen.py engine --platform android` publishes next to it at the engine version. Each one depends on `dev.haylen:haylen` as an API, and a plugin module depends on the ones it uses.

| Library | What it brings | Who depends on it |
| --- | --- | --- |
| `dev.haylen:haylen` | The player, `HaylenActivity`, the platform bridge, the plugin API and `HaylenRequirements`, with GameActivity, AppCompat, the activity library and core as APIs. Its manifest declares OpenGL ES 3 alone, with no permission and no component. | The app module. |
| `dev.haylen:haylen-plugins` | The provider that loads the plugins when the app process starts, `HaylenPluginProvider`. | Every plugin module. |
| `dev.haylen:haylen-links` | The activity that receives links and notification taps, `HaylenLinkActivity`, exported so plugins add their intent filters to it, as [links and notifications](#links-and-notifications) describes. | Plugins that receive links or post notifications. |
| `dev.haylen:haylen-coroutines` | Handlers written as suspending functions, `HaylenCoroutines.register` and `registerSuspend`, with `kotlinx-coroutines-android` as an API. | Kotlin plugins and apps with suspending handlers. |

An app whose plugins bring no provider and no link activity has no exported component besides its launcher activity. `HaylenActivity` logs an error with the tag `haylen` when the manifest names plugins in `dev.haylen.plugin.<id>` meta-data while the provider never ran, which happens when no plugin module depends on `dev.haylen:haylen-plugins`.

### The plugin class

A plugin class extends `dev.haylen.HaylenPlugin` and has a public constructor without parameters. `HaylenPluginProvider`, a content provider that the manifest of `dev.haylen:haylen-plugins` declares, starts with the app process, before `Application.onCreate`. It reads every `dev.haylen.plugin.<id>` entry of the merged manifest, creates each class once and calls `onLoad` with the context of the plugin, in the [load order](#load-order) of the `app.json` of the package, so SDKs set up before any app code runs. Its `initOrder` is 50, so the providers that SDKs start with at 100, such as the one of Firebase, have run by then. The engine receives the ids of the plugins that loaded, which `handle.native` and `platform.plugins()` show in Lua.

- A class that cannot be found or created, such as a class the module lacks or one without a public constructor without parameters, and a class that does not extend `HaylenPlugin`, are logged as errors with the tag `haylen` that name the class and the plugin, and the plugin runs without its native part.
- An `onLoad` that throws is logged the same way, the methods it registered go away, and the plugin runs without its native part.

```kotlin
// plugins/share-sheet/android/src/main/kotlin/com/example/sharesheet/ShareSheetPlugin.kt
package com.example.sharesheet

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import dev.haylen.HaylenActivity
import dev.haylen.HaylenBridge
import dev.haylen.HaylenPlugin
import dev.haylen.HaylenPluginContext
import org.json.JSONObject

class ShareSheetPlugin : HaylenPlugin() {
    private var chooser: ActivityResultLauncher<Intent>? = null
    private var pending: HaylenBridge.Reply? = null

    override fun onLoad(context: HaylenPluginContext) {
        // Answers share-sheet.share once the person leaves the chooser.
        context.register("share") { params, reply ->
            val launcher = chooser ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
            val send = Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, (params as JSONObject).getString("text"))
            pending = reply
            launcher.launch(Intent.createChooser(send, null))
        }
    }

    // Every activity takes the launcher before it starts, under a key of the plugin, so a result that Android delivers to a new activity finds it.
    override fun onActivityCreated(activity: HaylenActivity, savedInstanceState: Bundle?) {
        chooser = activity.activityResultRegistry.register("share-sheet.share", activity, ActivityResultContracts.StartActivityForResult()) { result ->
            pending?.success(JSONObject().put("completed", result.resultCode == Activity.RESULT_OK))
            pending = null
        }
    }
}
```

The same plugin in Java:

```java
// plugins/share-sheet/android/src/main/java/com/example/sharesheet/ShareSheetPlugin.java
package com.example.sharesheet;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import dev.haylen.HaylenActivity;
import dev.haylen.HaylenBridge;
import dev.haylen.HaylenPlugin;
import dev.haylen.HaylenPluginContext;
import java.util.Collections;
import org.json.JSONObject;

public final class ShareSheetPlugin extends HaylenPlugin {
    private ActivityResultLauncher<Intent> chooser;
    private HaylenBridge.Reply pending;

    @Override
    public void onLoad(HaylenPluginContext context) {
        context.register("share", (params, reply) -> {
            if (chooser == null) {
                throw new HaylenBridge.Failure("The app has no activity yet.", "noWindow", null);
            }
            Intent send = new Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, ((JSONObject) params).getString("text"));
            pending = reply;
            chooser.launch(Intent.createChooser(send, null));
        });
    }

    @Override
    public void onActivityCreated(HaylenActivity activity, Bundle savedInstanceState) {
        chooser = activity.getActivityResultRegistry().register("share-sheet.share", activity, new ActivityResultContracts.StartActivityForResult(), result -> {
            if (pending != null) {
                pending.success(Collections.singletonMap("completed", result.getResultCode() == Activity.RESULT_OK));
                pending = null;
            }
        });
    }
}
```

### The context

`HaylenPluginContext` is the part of the engine library that a plugin sees, one for each plugin. Registering, emitting and covering work from any thread, while the activity and the overlay belong to the main thread.

| Member | Meaning |
| --- | --- |
| `id()` | The id of the plugin. |
| `application()` | The `Application` of the app. |
| `activity()` | The running `HaylenActivity`, or `null` while there is none, as in `onLoad`. It is a `GameActivity` and so an `AppCompatActivity`, a `FragmentActivity` and a `ComponentActivity`, which SDKs take as it is. |
| `config()` | The parameters of the plugin in `app.json` over the `default` of every parameter of `plugin.json`, the same values as `handle.config` in Lua, as a `JSONObject`. |
| `register(method, handler)`, `register(method, handler, threading)` | Answer `<id>.<method>` like `HaylenBridge.register` in the [platform bridge guide](platform_bridge.md#android), on the main thread or, with `HaylenBridge.Threading.BACKGROUND`, on the shared background thread described in [threads](#threads). |
| `registerSuspend(method) { params -> result }` | Kotlin only, with `dev.haylen:haylen-coroutines`. Answers `<id>.<method>` with a suspending function, like `HaylenCoroutines.register`. |
| `registerScreen(name, contract, input, output)`, `registerScreen(name, opener)` | Open the screen `<id>.<name>` through an Activity Result contract or an opener, as [Android screens](#android-screens) describes. |
| `restoredScreen()` | The `HaylenScreen` of the plugin that showed when the process ended, which the plugin ends once its answer arrives, or `null`. |
| `openVideoStream(name, width, height, format)`, `openAudioStream(name, sampleRate, channels, format, capacity)` | Open the video or audio [stream](#streams) of a name, or return the one that is open, and throw an `IllegalArgumentException` for arguments the stream cannot take. |
| `emit(event, payload)`, `emitRetained(event, payload)`, `emit(event, payload, retain, batched)` | Send the event `<id>.<event>`, retained for the first listener of its name with `emitRetained`, and retained, batched or both with the last form. The payload is converted like the value of `reply.success`, with `byte[]` and `ByteBuffer` values as bytes. |
| `overlay()` | The overlay that places native views of the plugin over the app, as [Android overlays](#android-overlays) describes. |
| `requirements()` | The `HaylenRequirements` of the plugin, which checks what the project of the app holds before the plugin calls a system API that needs it, as [Android requirements](#android-requirements) describes. |
| `coverApp()`, `uncoverApp()` | Cover the app while native UI of the plugin covers it, and end the cover, as [covering the app](#covering-the-app) describes. The covers of a plugin end with the activity, an `uncoverApp` without a cover of the plugin is logged as an error, and a `coverApp` while no activity exists is logged as a warning and covers nothing. |
| `runOnMainThread(task)` | Runs the task at once on the main thread, and posts it there from any other thread, for callbacks of SDKs that arrive on threads of their own. |

Events that native code sends while no app runs, such as from `onLoad` before the first activity loads the native library, while the app restarts or between two activities, wait in the engine library, up to 32 per name with the oldest dropped first, and reach the next app in order once it starts. A link that opens the app, which the plugin sends retained, therefore reaches the first Lua listener of its name however late it comes.

A plugin with suspending handlers adds `implementation("dev.haylen:haylen-coroutines:$haylenEngineVersion")` to its module:

```kotlin
import dev.haylen.registerSuspend
import kotlinx.coroutines.delay

override fun onLoad(context: HaylenPluginContext) {
    context.registerSuspend("loadProfile") { params ->
        delay(100)
        mapOf("id" to (params as JSONObject).getString("id"), "name" to "Player")
    }
}
```

### Events of the activity

`HaylenActivity` hands its events to every plugin in load order, on the main thread. The activity calls `onActivityCreated` and `onActivityDestroyed` itself, while its AndroidX lifecycle hands over the start, the resume, the pause and the stop through an observer that the activity adds after `onActivityCreated`, so a plugin hears of a start or a resume after the activity handled it, and of a pause or a stop before.

| Method | When |
| --- | --- |
| `onActivityCreated(activity, savedInstanceState)` | A new activity loaded the native library and has not started yet, which is when plugins register their [Activity Result launchers](#activity-results-and-permissions). `activity.getIntent()` is the intent that launched it, such as a link or a notification that opened the app. |
| `onActivityStarted(activity)`, `onActivityResumed(activity)`, `onActivityPaused(activity)`, `onActivityStopped(activity)` | The lifecycle of the activity changes state. |
| `onActivityDestroyed(activity)` | The activity goes away after its app stopped. The views of the plugins over the app leave and their covers end right after. |
| `onNewIntent(intent)` | A [link or a notification](#links-and-notifications) reaches the running app, or a launch reaches the activity while it is on top, as the launcher icon does. The activity already holds the intent, so `getIntent()` returns it too. |
| `onConfigurationChanged(configuration)` | A change that the activity handles itself, which the manifest of the template lists: orientation, screen size and layout, density, keyboard, navigation and UI mode. Other changes, such as the language, create a new activity. |
| `onWindowFocusChanged(hasFocus)` | The window of the activity gains or loses the focus. The app draws only while it has it, and views over the app take the focus inside the window without taking it from the window. |
| `onTrimMemory(level)` | The system asks for memory. |
| `onAppError(error)` | An error stopped the app, with the report that [errors of the app](#errors-of-the-app) describes. |

### Activity results and permissions

The activity is a `ComponentActivity`, so plugins start activities for a result and ask for permissions with the Activity Result API, and no plugin shares request codes with another. A plugin registers each launcher in `onActivityCreated`, before the activity starts, as the API requires, with `activity.activityResultRegistry.register(key, activity, contract, callback)` and a key of its own, such as `<id>.<name>`, and the launcher ends with its activity. `ActivityResultContracts.RequestPermission` and `RequestMultiplePermissions` ask for permissions the same way. When the activity is gone by the time the other activity returns, after the activity was recreated or the process ended while the other app showed, Android keeps the result and delivers it to the launcher with the same key of the new activity once that one starts. No call of the app waits for it then, and no app of the new process listens yet, so a plugin that reports such a result to the app sends it retained, which waits for the first listener. The activity registers launchers of its own under keys that start with `haylen.`, for the [dialogs](lua-api/dialogs.md) of the engine and the [screens](#android-screens) of plugins, so plugins keep their keys out of that prefix.

```kotlin
override fun onActivityCreated(activity: HaylenActivity, savedInstanceState: Bundle?) {
    camera = activity.activityResultRegistry.register("scanner.camera", activity, ActivityResultContracts.RequestPermission()) { granted ->
        pending?.success(JSONObject().put("granted", granted))
        pending = null
    }
}
```

SDKs that take an `ActivityResultCaller`, a `ComponentActivity`, a `FragmentActivity` or a `LifecycleOwner`, such as paywalls, payment sheets, biometric prompts and sign-in flows, take the activity itself, and SDKs that add their views to the content of the activity, such as in-app messages, draw over the app. A plugin may add fragments with `activity.supportFragmentManager` and place a `ComposeView` with the overlay. A plugin whose UI answers back itself, such as a sheet over the app, adds an `OnBackPressedCallback` to `activity.onBackPressedDispatcher` with the activity as its lifecycle owner, which goes before the callback of the app while it is enabled.

### Links and notifications

The activity of the template is single top, so it stays the one activity that the native side of GameActivity needs, and when the launcher icon brings the app back, every screen that showed over the app, such as a purchase, a bank check or a sign-in page, shows again as the person left it. Links and notifications go to `dev.haylen.HaylenLinkActivity`, which shows nothing and runs in a task of its own. It hands them to the running `HaylenActivity`, whose plugins receive them in `onNewIntent`, and brings the task of the app to the front as the launcher icon does. When no activity runs, it keeps them for the `HaylenActivity` that the launch creates, whose plugins find them in `activity.getIntent()` in `onActivityCreated`, or that Android restores after it ended the process in the background, which keeps the intent it started with and hands the link to `onNewIntent` right after `onActivityCreated`. The activity is exported, since other apps start it, so only the manifest of `dev.haylen:haylen-links` declares it, and a plugin that receives links or posts notifications depends on that library. The plugin declares the intent filters of its links on that activity in its manifest, and the notifications it posts start that activity:

```xml
<activity android:name="dev.haylen.HaylenLinkActivity" android:exported="true">
    <intent-filter>
        <action android:name="android.intent.action.VIEW" />
        <category android:name="android.intent.category.DEFAULT" />
        <category android:name="android.intent.category.BROWSABLE" />
        <data android:scheme="${pushUrlScheme}" />
    </intent-filter>
</activity>
```

```kotlin
val open = Intent(context.application(), HaylenLinkActivity::class.java).setAction("com.example.push.OPEN").putExtra("message", messageId).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
val tap = PendingIntent.getActivity(context.application(), messageId.hashCode(), open, PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
```

### Android overlays

`context.overlay().add(view, placement)` places a native view over the app, such as a banner, and returns a `HaylenOverlay.Panel`:

| Member | Meaning |
| --- | --- |
| `update(placement)` | Places the view again with another placement. |
| `setVisible(visible)` | Shows or hides the view, which reserves its edge only while it shows. |
| `bounds()` | The frame of the view in the pixels of the activity window, or `null` while it does not show. |
| `remove()` | Takes the view off the app and gives its edge back, so the plugin may place the view again. A removed panel ignores later updates. |

A `HaylenPlacement` has the fields of the placements of the [web overlay](#web-modules), in dp:

| Field | Default | Meaning |
| --- | --- | --- |
| `anchor` | `HaylenPlacement.Anchor.BOTTOM` | Where the view sits: `TOP`, `BOTTOM`, `LEFT`, `RIGHT`, `TOP_LEFT`, `TOP_RIGHT`, `BOTTOM_LEFT`, `BOTTOM_RIGHT` or `CENTER`. The view is centered along every axis its anchor leaves free. |
| `marginDp` | `0` | The distance from the edges the anchor names. |
| `insideSafeArea` | `true` | Whether the view stays inside the safe area of the device, the one the app sees without reservations, clear of cutouts and of the system bars that stay on screen. |
| `reserve` | `false` | Whether the view reserves the edge its anchor names, from the edge of the window to its far side, while it shows. A centered view reserves nothing. |
| `widthDp`, `heightDp` | `HaylenPlacement.MEASURED` | The size of the view. `MEASURED` keeps the size of the layout parameters the view had when it was added, or its measured size when it had none. |

The overlay adds each view to the layout of the surface of the app, above the surface, so the view draws, takes the touches inside its frame and may take the focus and the keyboard, such as the fields of a form or a web view, while every other touch reaches the app and several fingers split between the views and the app. The overlay places its views again when the safe area, the orientation, the density or the size of the window changes, such as in multi-window mode, and the views leave with the activity, so a plugin places them again for a new activity, such as from `onActivityCreated` or when the app asks for them again. Keys go to the view that has the focus first, and the ones it leaves reach the app, while the directional pad never moves the focus to a view of the overlay and the sticks of controllers reach the app wherever the focus is, so a view that the remote of a TV must drive opens in a dialog or an activity of its own. The overlay works on the main thread, and `add` throws an `IllegalStateException` on other threads and while no activity exists.

```java
// Shows a banner at the bottom of the safe area, which the UI of the app anchored to the safe area moves above.
HaylenPlacement placement = new HaylenPlacement(HaylenPlacement.Anchor.BOTTOM);
placement.reserve = true;
placement.widthDp = 320;
placement.heightDp = 50;
HaylenOverlay.Panel banner = context.overlay().add(bannerView, placement);

// Later, the banner moves to the top, and finally goes away.
banner.update(new HaylenPlacement(HaylenPlacement.Anchor.TOP));
banner.remove();
```

### Threads

Handlers run on the main thread, where they may touch the activity and views. A handler registered with `HaylenBridge.Threading.BACKGROUND` runs on one background thread that every such handler shares, in the order the calls arrive, which suits work that does not touch the UI, such as disk, database or network access through blocking APIs. A handler that starts its own asynchronous work answers from any thread, and `onCancel` listeners always run on the main thread. The parameters of a call are parsed on the thread of its handler, so the frame thread of the app only hands the call over. The events of the activity, `onAppError` and the overlay use the main thread, and `context.runOnMainThread` brings a callback of an SDK there.

### R8

The consumer rules of the engine library keep every class that extends `HaylenPlugin` with its constructor, so the class names of the manifest survive minified release builds, and a plugin needs no rule for its own class. A plugin keeps what its own code or its SDKs reach by reflection in the `consumer-rules.pro` of its module, which its `build.gradle.kts` names with `consumerProguardFiles`, and never adds global options there, since they would change the build of the whole app.

## Demo plugin and sample

The plugins sample, [`samples/system/plugins`](../samples/system/plugins), carries its own plugin, [`native-demo`](../samples/system/plugins/plugins/native-demo), which exercises every capability of this guide with the APIs of each platform alone: UIKit, AppKit and SwiftUI on Apple platforms, the views, dialogs and intents of Android, the DOM on the web and the threads and the windows of the system in C on the desktops. It is the reference for writing a plugin: each of its parts is a small, complete example of the platform side of one capability, and its [README](../samples/system/plugins/plugins/native-demo/README.md) documents its Lua API the way every plugin documents its own. The [sample README](../samples/system/plugins/README.md) explains its tests and how to run them on each platform.

### The package

| File | What it shows |
| --- | --- |
| [`plugin.json`](../samples/system/plugins/plugins/native-demo/plugin.json) | Every platform, five parameters with defaults, the frameworks `UserNotifications.framework` and `AVFoundation.framework` of the Apple part, `${urlScheme}` in the `CFBundleURLTypes` of `infoPlist` and in the Android placeholder `nativeDemoUrlScheme`, `${cameraUsage}` in the `NSCameraUsageDescription` of the Apple platforms with a camera, and a `native` library for the desktops. |
| [`source/init.lua`](../samples/system/plugins/plugins/native-demo/source/init.lua) | The Lua API on the plugin handle, the load of the C library where no other native part loaded, and the `start` call that tells the native part of every new app. |
| [`apple/`](../samples/system/plugins/plugins/native-demo/apple) | `NativeDemoPlugin.swift`, the plugin class, a `HaylenNotificationPlugin`, which also draws its image with CoreGraphics, asks for permissions, schedules notifications and checks its requirements, `NativeDemoBanner.swift`, `NativeDemoScreen.swift`, `NativeDemoPicker.swift` and `NativeDemoConfirm.swift`, each for UIKit and AppKit, `NativeDemoConfirmView.swift`, the SwiftUI screen, and `NativeDemoVideo.swift` and `NativeDemoTone.swift`, which feed the streams from dispatch queues. |
| [`android/`](../samples/system/plugins/plugins/native-demo/android) | The library module with its manifest, its dependencies on `dev.haylen:haylen-plugins` and `dev.haylen:haylen-links`, and `NativeDemoPlugin.kt`, the plugin class, which also draws its image with a `Bitmap`, asks for permissions and schedules notifications, `NativeDemoBanner.kt`, `NativeDemoScreen.kt`, `NativeDemoConfirm.kt` and `NativeDemoConfirmActivity.kt`, the contract and the AndroidX activity of the confirm screen, `NativeDemoNotifier.kt`, which posts the notifications, and `NativeDemoVideo.kt` and `NativeDemoTone.kt`, which feed the streams from threads of their own. |
| [`web/`](../samples/system/plugins/plugins/native-demo/web) | `native-demo.js`, the web module, and `screen.html`, the confirm page of its popup and redirect screens. |
| [`native/`](../samples/system/plugins/plugins/native-demo/native) | `CMakeLists.txt` and `NativeDemo.c`, the library of the desktops, with a PNG encoder of its own and the threads that feed its streams, and `NativeDemoScreen.m` and `NativeDemoScreen.c`, the windows of its confirm screen. |

### The Lua API

`source/init.lua` wraps every method and event of the plugin in a function of its own, so apps never write method names, and reads `handle.native` to tell whether the native part runs. Apple platforms, Android and the web load their native parts before any Lua runs. The desktops run the C library, so on macOS, Windows and Linux, when no other native part loaded, the module calls `native.load('native_demo', {init = 'native_demo_haylen_init'})`, whose init function declares the library the native part of the plugin with `registerPlugin`. That happens once per process, since `handle.native` stays true after a restart, and the macOS app built from the Apple template runs the Swift part instead. The module then sends `start`, which every native part answers by ending what an earlier app of the process left running, the ticks and the banner, and by sending the error that stopped that app as the retained event `lastError`. A plugin that needs to know when a new app starts, since the engine restarts apps inside one process, uses such a call.

### Calls

| Capability | Apple, Swift | Android, Kotlin | Web | Desktops, C |
| --- | --- | --- | --- | --- |
| An answer on the main thread, `echo` | `context.registerHandler`, the Objective-C API, which takes any JSON value. | `context.register`, whose handlers run on the main thread. | `context.register`. | `registerHandler` of `HaylenNativeApi`, whose handlers run on the frame thread. |
| Work off the main thread, `compute` | `context.register` with `Decodable` parameters and an `Encodable` result, awaiting a global dispatch queue. | `context.register` with `HaylenBridge.Threading.BACKGROUND`. | An async handler that yields to the page between slices. | A thread of the library that calls `resolve`. |
| A typed failure, `fail` | A thrown `HaylenFailure` with a code and data. | `reply.failure(message, code, data)`, or a thrown `HaylenBridge.Failure`. | A thrown error with `code` and `data`. | `resolve` with `ok` 0 and an object with `message`, `code` and `data`. |
| Cancellation, `wait` | The task of the call is cancelled, which ends `Task.sleep`. | `reply.onCancel`. | The `abort` event of the `signal` of the call. | The cancel function of `registerHandler`. |
| An unsupported call | `HaylenFailure` with the code `unsupported`, for `pickFile`, the camera and `notify` on tvOS. | A thrown `HaylenBridge.Failure` with the code `unsupported`, for the camera of a device without one. | | `resolve` with the code `unsupported`, for the banner, the covering screen, the picker and the parameters. |
| Bytes both ways, `echoBytes` | `registerHandler`, whose parameters hold `Data` and whose answer returns it. | A `ByteArray` in the parameters, returned in a `JSONObject`. | A `Uint8Array` in the parameters, returned as it is. | The `HaylenNativeBuffer` of the handler, handed back to `resolve`. |
| An image as bytes, `generatedImage` | A `CGContext` and `CGImageDestination` of ImageIO, which write a PNG. | A `Bitmap`, a `Canvas` and `Bitmap.compress`, on the background thread. | A `<canvas>` and `toBlob`. | A PNG encoder of the library with stored deflate blocks. |

### Events

`burst` shows [batched events](#batched-events): `burst(count, ticks)` sends `count` events 30 times per second for `ticks` ticks, each marked batched, from a `Timer` on Apple platforms, the main `Handler` on Android, `setInterval` on the web and a thread of the library on the desktops, and then `burstDone`, so the Lua listener receives one list of the events of each frame. `tick` comes from a `Timer` on the main run loop on Apple platforms, a `Runnable` posted to the main `Handler` on Android, `setInterval` on the web and a thread of the library on the desktops, which each send with `context.emit`, `HaylenNativeApi.emit` or their equivalent from their thread. `loaded` goes out retained from `load(with:)`, `onLoad`, `load(context)` and the init function, before any app listens, and waits for the first listener of the process, which the sample connects only when its Events test opens.

### Streams

`startVideo` opens the video stream `pattern` and draws an animated pattern into it 30 times per second, and `startTone` opens the audio stream `tone` and synthesizes a sine wave into it a tenth of a second ahead of the clock. The C library draws BGRA frames of 320 by 180 pixels and 16-bit mono samples at 44100 Hz on threads of its own, which the engine turns into RGBA and resamples to the mixer. The Swift part draws the pattern with CoreGraphics into `CVPixelBuffer`s of a pool and pushes them with `push(_:timestamp:)`, and pushes mono floats at 44100 Hz, each from a dispatch queue of its own. The web module animates a `<canvas>` that it pushes into `context.videoStream('pattern')` and pushes `Float32Array` blocks into `context.audioStream('tone', {sampleRate = 44100, channels = 1})` from timers of the page. The Kotlin part draws the pattern with a `Canvas` into an RGBA `Bitmap` and pushes it with `push(bitmap, timestamp)`, and pushes mono floats at 44100 Hz with `push(samples, frames)`, each from a `HandlerThread` of its own. The Lua API returns the streams with `videoStream()` and `audioStream()`, whose texture the sample draws and whose voice it plays, with a level meter from `read`.

### Parameters

The native parts read their parameters from the context, `context.config` on Apple platforms and the web and `context.config()` on Android, with the defaults of `plugin.json` applied, and `nativeConfig` answers with them. Native libraries receive no parameters, so the Lua API passes what the C library needs in its calls, such as the interval of `ticks`, and the C part fails `config` with the code `unsupported`.

### Views over the app

The banner is a native view that the overlay of the context places at the top or the bottom of the safe area, 360 by 56 points, dp or page pixels, with `reserve` set or not: `context.overlay.add(view, placement:)` with a `HaylenPlacement` on Apple platforms, `context.overlay().add(view, placement)` on Android, which adds it over the surface of the app, and `context.overlay.add(element, placement)` on the web. `update`, the visibility and `remove` of the item it returns move, hide and remove the banner, and a native button inside it sends `bannerTapped`. The sample shows that its frame, laid out in the safe area, moves out of the way of a banner that reserves its edge, and that taps outside the banner reach the app.

### Covering the app

The native screen is a `UIViewController` presented full screen on iOS, iPadOS, Mac Catalyst and tvOS, a sheet of the window on macOS, a full screen `Dialog` on Android and a modal `<dialog>` element on the web. Each part calls `coverApp` right before it shows and `uncoverApp` once it closed, with `defer` in Swift and `finally` in JavaScript, and answers the call after the cover ended. The file picker of Apple platforms covers the app the same way, while the document picker of Android is an activity of its own, which pauses the app through its lifecycle.

### Screens

`confirm` is the [screen](#plugin-screens) of the plugin, which asks a question with Confirm and Decline and answers with `{confirmed, via, language}`. The web opens `web/screen.html` in a popup with `screen.popup`, whose page posts the answer to the app, and the desktops open a native window over the window of the app from `getWindow`: a sheet on macOS in `NativeDemoScreen.m`, an owned window on Windows and a transient X11 window on a connection of its own on Linux in `NativeDemoScreen.c`, whose Close button and close box end the screen as `cancelled`. Apple platforms show a UIKit controller in `NativeDemoConfirm.swift`, presented full screen, whose swipe and Menu button end the screen as `cancelled`, and an AppKit sheet on macOS, and add `swiftUI`, the same question in the SwiftUI view of `NativeDemoConfirmView.swift` through a hosting controller, presented over the app on iOS, iPadOS and tvOS and in a window of its own on Mac Catalyst and macOS, whose Close button dismisses it through SwiftUI. The web adds `redirect`, which leaves the page for the same confirm page and comes back with the answer and the token of the screen in the address, which the module reads through `context.restoredScreen` in `load`. Android registers the screen as the contract of `NativeDemoConfirm.kt`, which starts `NativeDemoConfirmActivity`, an `AppCompatActivity` of its own, whose Back button gives `null` and so ends the screen as `cancelled`, which a cancel of the app finishes, and whose end reaches the next app as `screenRestored` when the process ended while it showed. The Native screen test of the sample opens the screen, shows that the app was covered and drew nothing under it, tries a second screen, which fails with `busy`, cancels one from a timer while it shows, restarts the app under a screen with `haylen.requestRestart()`, so the next app receives `screenRestored` with the state, and opens the screen right in the answer of a message of [haylen.dialogs](lua-api/dialogs.md), which waits until the app is active.

### Native results

`pickFile` shows the platform side of a result: `ActivityResultContracts.OpenDocument` on Android, whose launcher the plugin class registers under the key `native-demo.pickFile` in `onActivityCreated` and which answers the call, or logs a pick that ends after the process ended while the picker showed, a `UIDocumentPickerViewController` that is its own delegate on iOS, iPadOS and Mac Catalyst, `NSOpenPanel` on macOS and an `<input type="file">` with its `change` and `cancel` events on the web. A cancelled picker answers `nil`.

### Permissions and notifications

The call `requestPermission` asks the person for `camera` or `notifications` with the prompt of the system and answers with `{kind, granted, status, language}`. On Apple platforms the camera prompt shows the `NSCameraUsageDescription` that the `infoPlist` of `plugin.json` gives, tvOS has no camera, and the notifications ask through `UNUserNotificationCenter`. The call `notify(seconds)` schedules a local notification of the plugin, which shows while the app is in front too, since the plugin class answers `userNotificationCenter(_:willPresent:withCompletionHandler:)` for its own notifications. Its tap reaches `userNotificationCenter(_:didReceive:withCompletionHandler:)` of the plugin class, which adopts `HaylenNotificationPlugin`, through the delegate of the notification center that the runtime sets because the plugin links UserNotifications, also when the tap launches the closed app, and the plugin sends it as `notificationOpened`, retained, so it waits for the listener of the Permissions test of the sample. On Android the plugin checks with `context.requirements()` that the manifest of the app declares `android.permission.CAMERA` or `android.permission.POST_NOTIFICATIONS`, which the manifest of its module does, and asks through a launcher of `ActivityResultContracts.RequestPermission` that it registers in `onActivityCreated`, answering with the status `authorized` or `denied`. Before Android 13 notifications need no permission, so the answer tells whether the person left them on. `notify` fails with the code `permissionDenied` while the notifications of the app are off, and otherwise sets an alarm, which starts the process when it was closed, whose receiver, `NativeDemoNotifier`, posts the notification through `NotificationManagerCompat` on a channel of its own. The tap starts `HaylenLinkActivity`, and the plugin sends `notificationOpened` from `onActivityCreated` or `onNewIntent`.

### Requirements

The call `requirementCheck` needs something that the plugin leaves out of the project of the app on purpose, so it shows the [requirements](#requirements) of the projects. On Android its handler calls `context.requirements().require` with the permission `android.permission.READ_CONTACTS`, which the manifest of the module never declares, so the call fails with the code `unsupported` and `data.missing` holds the permission with `app/src/main/AndroidManifest.xml` and its `<uses-permission>` snippet. On Apple platforms the Swift handler calls `try context.require(.usageDescription("NSContactsUsageDescription"))`, which the `infoPlist` of `plugin.json` never gives, so `data.missing` holds the usage description with the `Info.plist` of the platform and its snippet. On the web the module calls `context.require({secureContext: true, api: 'navigator.contacts'})`, and the Contact Picker API exists only in browsers of phones, so desktop browsers fail with the API in `data.missing`. The log tells once what is missing on every platform. An app whose project has the requirement, and a phone browser with the API, get `{met, language}` instead. The Requirements test of the sample shows the failure and every missing requirement with its file and snippet.

### Links and errors of the app

The scheme of `urlScheme` opens the app. On Apple platforms `scene(_:openURLContexts:)` and `application(_:open:)` receive the links, including the one that launched the app, and on Android, where the plugin declares the scheme on `HaylenLinkActivity` of `dev.haylen:haylen-links`, `onActivityCreated` receives the intent that launched the activity and `onNewIntent` the ones that reach it later. Each part sends `urlOpened` retained, so a link that launched the app waits for the first listener. The web stands in with the hash of the page address. The errors of the app reach `appDidFail(with:)`, `onAppError`, `context.onAppError` and the handler of `registerErrorHandler`, which keep the message until the next app sends `start`.

### Testing without the native part

The headless host of the engine tests loads no native parts, so `platform.plugins()` reports the plugin with `native` false and the sample shows `The native part is not available on this platform.` in every test that needs it, which is how a plugin with a Lua API of its own keeps an app running where its native part is missing.
