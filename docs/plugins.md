# Plugins

A plugin gives Lua apps a capability that each platform implements natively, such as ads, analytics, sign-in or purchases. It is a folder with a manifest, `plugin.json`, a Lua API under `source/` and a native part for each platform it supports: Swift or Objective-C sources with Swift packages for Apple platforms, a Gradle library module for Android, ES modules for the web and a CMake library for desktops. The Lua API is the same on every platform. An app lists the plugins it uses in its `app.json`, and make.py checks them, puts their Lua code into the package and assembles their native parts into the project of each platform, so an app still never compiles the engine.

These plugins are distributable folders, not the engine plugins of `haylen::plugins`, which are the C++ subsystems that make up the engine. The official plugins live in `plugins/` of the repository.

## Using plugins

### Commands

```sh
python3 make.py plugin list
python3 make.py plugin add admob --app ~/apps/my-game
python3 make.py plugin add ~/plugins/my-plugin --app ~/apps/my-game
python3 make.py plugin list --app ~/apps/my-game
python3 make.py plugin remove admob --app ~/apps/my-game
python3 make.py plugin new ~/plugins/my-plugin
```

| Command | Purpose |
| --- | --- |
| `plugin add <id\|folder> [--app]` | Copies an official plugin from `plugins/<id>` of the repository, or any plugin folder, into `plugins/<id>/` of the app, replacing an earlier copy. It lists the plugin in `app.json` with the default of every parameter that has one and an empty text for every required parameter, which the developer fills in, keeps the values the app already gives, and names the plugins it requires that the app does not list yet. |
| `plugin remove <id> [--app]` | Deletes `plugins/<id>/` of the app and its entry in `app.json`. |
| `plugin list [--app]` | Without `--app`, lists the official plugins with their version, platforms and description. With `--app`, lists the plugins of the app with their version, their platforms and their status: `ok`, the problems that keep a plugin from building for any of its platforms, a folder that `app.json` does not list, or a newer official version. |
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

A `file` parameter is a path relative to the app folder. Files such as `GoogleService-Info.plist` and `google-services.json` belong outside `source/` and `content/`, for example under `platform/`, so they never ship in the package, and make.py copies them into the projects that need them.

### Validation

Before `run` builds for a platform, make.py loads every plugin that `app.json` lists from `plugins/` of the app, checks its `plugin.json` against the [format](#pluginjson) and checks the values of the app against its parameters for that platform. The player of this machine checks them for this desktop. make.py fails with every problem at once, each with its file and its key, for:

- an id whose folder has no `plugin.json`
- any problem of a `plugin.json`
- a plugin that `requires` another one that `app.json` does not list
- a value for a parameter that the plugin does not declare
- a value of the wrong type
- a required parameter without a value for the platform, which includes the empty text that `plugin add` writes
- a `file` parameter whose file does not exist

A parameter applies to the platforms it lists, or to every platform of the plugin when it lists none, so a missing required value and a missing file fail only the builds for those platforms.

### Load order

Plugins load in the order of `app.json`, except that every plugin loads after the plugins it requires, and two plugins that require each other fail the build. The same order lists the plugin classes on Apple platforms, the plugin modules on Android and the modules that the web loader imports.

### The package

For every plugin that `app.json` lists, the package carries `plugins/<id>/plugin.json` and `plugins/<id>/source/` next to `app.json`, `source/` and `content/`. `make.py package`, the package copies of the Apple and Android projects, the Android package index and the `app.zip` of the web all hold them. `require('<id>')` loads `plugins/<id>/source/init.lua`, and `require('<id>.<name>')` loads `plugins/<id>/source/<name>.lua`. Nothing else of a plugin folder ships in the package: its native parts reach the platform projects, and its README stays behind.

### Apple platforms

When the plugins of an app add sources, Swift packages, system frameworks, resources or build scripts, make.py assembles the Apple project in three steps:

1. It copies the `sources` folder of every plugin to `plugins/<id>/sources/` of the project and its resources to `plugins/<id>/resources/`.
2. It writes `plugins.json`, the XcodeGen include of `project.yml`, which adds the sources, the Swift packages with the products they link, the system frameworks, the resources and the build scripts of each plugin to the `iOS`, `tvOS` and `macOS` targets of the platforms the plugin lists. The `iOS` target builds iOS and Mac Catalyst, so a plugin that lists only one of `ios` and `catalyst` joins it with `destinationFilters`, which keep its sources, products and frameworks to that destination. A product or a framework that several plugins link joins a target once.
3. It generates `App.xcodeproj` again with the pinned XcodeGen in `.tools/xcodegen`, which `make.py tools` and the first such build download. An app whose plugins add nothing to the project keeps the committed project.

The sources of a plugin compile into the targets like the files of `source/`, so Swift reaches the engine through `HaylenBridging.h`. The resources land at the root of the app bundle. Build scripts run as post-build phases for every destination of their targets, so a script of a plugin that leaves out Mac Catalyst checks `IS_MACCATALYST` itself.

Every Apple build also merges the `infoPlist` keys of the plugins into the `Info.plist` of each platform that make.py writes, with `HaylenPlugins`, the array of the plugin classes of that platform in load order. `ios/Info.plist` serves iOS and Mac Catalyst, and the runtime skips a class that a destination leaves out, which is how Mac Catalyst leaves out plugins that list only `ios`. The `entitlements` of the plugins go to `ios/App.entitlements`, `catalyst/App.entitlements`, `tvos/App.entitlements` and `macos/App.entitlements`, and `App.xcconfig` signs each target and SDK platform with its file through `CODE_SIGN_ENTITLEMENTS`. Objects merge key by key and arrays gain the items they lack, so two ad plugins share `SKAdNetworkItems`, while a key that two plugins, or a plugin and `app.json`, set to different values fails the build with both named.

### Android

1. make.py copies the library module of every plugin into `plugins/<id>/` of the Android project.
2. It writes the plugin keys of `gradle.properties`. `haylen.plugins` lists the modules as `id=folder` entries, which `settings.gradle.kts` includes and `app/build.gradle.kts` depends on. `haylen.gradlePlugins` lists the Gradle plugins as `id=version` entries, and every `haylen.placeholder.<name>` key is a manifest placeholder.
3. The root `build.gradle.kts` puts the plugin marker of every Gradle plugin, `<id>:<id>.gradle.plugin:<version>`, on the build classpath, which serves the app module like a plugin declared with `apply false`, and `app/build.gradle.kts` applies each one by its id. A `plugins {}` block takes only literal ids, so a list that changes per app goes through the build classpath.
4. `app/build.gradle.kts` adds the placeholders to `manifestPlaceholders`, where the manifests of the plugin modules find them when the manifests merge.
5. make.py copies the `files` of the plugins into the project, such as `google-services.json` into `app/`.

The manifest of every module merges into the app with its permissions, its `dev.haylen.plugin.<id>` meta-data and its other entries.

### Web

make.py copies the `web/` folder of every plugin with a web part to `plugins/<id>/` of the site and lists `{id, version, module, config}` in `config.json`, where `module` is the path of its module in the site and `config` holds the values of its parameters with the defaults applied. The loader imports every module while the runtime downloads. Once the runtime exists and before the app starts, it calls the default export of each module, `load(context)`, in load order, with the context that `Module.haylen.createPluginContext(id, config)` of the runtime makes, and waits for the promise that `load` may return. A module that fails to import, or a `load` that fails, keeps the app from starting and shows the error on the loading page.

### Native libraries

The `native` section of a plugin joins the native libraries of the app, so make.py builds and places it like an entry of the `native` section of `app.json`, as [native libraries](distribution.md#native-libraries) describes, for the desktop player, Windows and Linux apps, the Apple projects and Android, on the platforms it lists. The library is named after the id of the plugin with its dashes turned into underscores, so `native.load('my_plugin')` loads the library of `my-plugin`.

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

A plugin needs only the folders of the platforms it supports. Paths in `plugin.json` are relative to the plugin folder and stay inside it, and make.py checks that each one exists.

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
| `description` | Yes | What the value means, which make.py repeats when a required value is missing. |
| `platforms` | No | The platforms of the plugin where the parameter applies and, when it is required, where a value must exist. Every platform of the plugin by default. |
| `required` | No | Whether the app must give a value for the platforms of the parameter. `false` by default. |
| `default` | No | The value of an optional parameter that the app leaves out, of the type of the parameter. |

`${name}` in a string of the `apple` and `android` sections is replaced with the value of the parameter `name`, and each one must name a declared parameter. A string that is only a reference takes the value with its type, so `"${testMode}"` becomes a boolean in `Info.plist`, while a reference inside longer text inserts the value as text. A key or an item whose references name a parameter without a value is left out. Shell expansions such as `${BUILD_DIR%/Build/*}` are no references and stay as they are. Build settings in paths and scripts use the `$(NAME)` form, which also keeps XcodeGen from replacing them with environment variables when it generates the project.

#### apple

| Key | Value |
| --- | --- |
| `class` | The Objective-C runtime name of the plugin class, which Swift classes declare with `@objc(Name)`. It joins `HaylenPlugins` of every platform of the plugin. |
| `sources` | The folder whose Swift, Objective-C, C and C++ files compile into the targets of the platforms of the plugin. |
| `packages` | Swift packages by name, each with its repository `url` (https or ssh), one `exactVersion` and the `products` that the targets link. Two plugins that name one package must agree on both. |
| `frameworks` | System frameworks and libraries, such as `StoreKit.framework` or `libz.tbd`. |
| `infoPlist` | Keys merged into the `Info.plist` of every platform of the plugin. |
| `entitlements` | Keys merged into the entitlements of every platform of the plugin. |
| `resources` | Files copied to the root of the app bundle: a path inside the plugin, or a reference to a `file` parameter such as `"${googleServicesPlist}"`, which names a file of the app and is left out while the parameter has no value. |
| `buildScripts` | Post-build phases, each with a `name`, a `script` and optional `inputFiles` and `outputFiles`. |

#### android

| Key | Value |
| --- | --- |
| `module` | The folder of the Android library module, with `build.gradle.kts` and `src/main/AndroidManifest.xml`. The module applies `com.android.library`, which the Android template declares, and depends on the engine with `compileOnly("dev.haylen:haylen:${providers.gradleProperty("haylen.engineVersion").get()}")`. |
| `gradlePlugins` | Gradle plugins that the app module applies, each as `{"id": ..., "version": ...}`, such as `{"id": "com.google.gms.google-services", "version": "4.5.0"}`. |
| `placeholders` | Manifest placeholders by name, whose text values the manifest of the module reads as `${name}`. |
| `files` | Copies into the Android project, each as `{"from": ..., "to": ...}`. `from` is a path inside the plugin or a reference to a `file` parameter, and `to` is a path inside the project, such as `app/google-services.json`. |

#### web

| Key | Value |
| --- | --- |
| `module` | The ES module in the `web` folder that the loader imports, such as `web/admob.js`. Its default export is `load(context)`, and it may import the other modules of the folder with relative paths. |

### Creating a plugin

```sh
python3 make.py plugin new ~/plugins/my-plugin
python3 make.py plugin add ~/plugins/my-plugin --app ~/apps/my-game
```

`plugin new` copies `templates/plugin/` with the id in the names of the files, classes and modules: `plugin.json`, a README, `source/init.lua` with the Lua API, `apple/<Name>Plugin.swift`, the Android module with its manifest meta-data and `<Name>Plugin.kt`, and `web/<id>.js`. Each native part answers the method `<id>.echo` and sends the event `<id>.echoed`, and the Lua API wraps both with the plugin handle of `platform.plugin(id)`.
