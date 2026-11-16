# ⬆️ Upgrading apps and plugins

This guide brings an app and its plugins from the earlier layout of Haylen to the current one. It is written for people and for AI assistants alike. It says what an older app holds, where each of its pieces goes now and how to check the result, so nobody has to work the process out again.

## What changes

An app of the earlier layout holds files that the current layout no longer has. The tooling of the current layout refuses them instead of reading them, so an upgrade moves every piece by hand once.

| Earlier layout | Current layout |
| --- | --- |
| `haylen.json`, the manifest of the app. | `haylen.json`, with the same runtime keys and the tooling keys `kind`, `cpp`, `plugins` and `platforms`. |
| A project of each platform in `platform/<template>/`, which belonged to the developer, with a generated folder `haylen/` inside it. | No project in the app. Every build generates the project of its target in `build/apps/<app>-<hash>/`, and the app customizes it through the sections of `platforms` and files of its own repository. |
| A C++ app as a CMake project with `CMakeLists.txt` and `haylen_add_app`. | A C++ app as `"kind": "cpp"` with the `cpp` section, built by the same backends as a Lua app. |
| `plugin.json` with `platforms`, `requires` and the sections `apple`, `android`, `web`, `desktop` and `native`. | `plugin.json` with `interface`, `implementations`, `dependencies` and a list of typed `contributions`. |
| Git plugins at the branch or tag of `haylen.json`. | Git plugins pinned to a commit in `haylen.lock`, which a Release build requires. |
| The script `haylen.py` with Python. | The package `tools/haylen` of the repository, which the launcher `haylen` runs through uv. |
| Engine artifacts in `build/artifacts/`, rebuilt by the commands of apps. | Engine SDKs in `build/sdk/`, built only by `haylen sdk build`. |

The keys of the platform projects that the developer changed by hand, such as a usage description in an `Info.plist`, a permission of an Android manifest or a page script, become keys of `platforms` in `haylen.json`. The files that the developer replaced, such as icons or a launch screen, stay in the app repository and the `files` of a section name them. The [distribution guide](distribution.md#what-an-app-customizes) lists every section and key.

## Before you start

1. Update the repository of Haylen that you build with, and install uv, the one requirement of the tooling.
2. Commit or set aside every change of the app and of its plugins, so the upgrade is one diff that you can read and undo.
3. Build the engine SDKs of the targets that the app ships to, which also proves that the toolchain of the machine is current.

```sh
cd ~/haylen
git pull
haylen sdk build --target player
haylen sdk build --target ios --target ios-simulator
haylen sdk build --target android
haylen doctor
```

The command `doctor` shows, for every target, the backend, the engine port, the SDK and the tools that this machine has and lacks.

## Make a reference

A new app and a new plugin show the current layout. Make them with the same options as yours, in a scratch folder that you delete at the end.

```sh
haylen new /tmp/reference-app --kind lua --name "My Game" --identifier com.example.mygame --orientation landscape
haylen new /tmp/reference-cpp --kind cpp
haylen plugin new /tmp/reference-plugin --id my-plugin
```

The reference app holds `haylen.json`, `source/` or `src/`, `content/logo.png`, the bridge handlers of every platform under `native/` and the files `.gitignore`, `.editorconfig` and `.clang-format`. Its `haylen.json` already names the handlers in `platforms`.

## From haylen.json to haylen.json

1. Rename `haylen.json` to `haylen.json`. The runtime keys keep their names and values, such as `window`, `design`, `splash`, `content`, `localization`, `autoload` and `debug`, and the `plugins` entries keep their `path`, `git`, `ref` and `parameters`.
2. Add `"kind": "cpp"` and the `cpp` section to a C++ app, as [a C++ app](#a-c-app) describes. A Lua app may leave `kind` out, since `lua` is the default.
3. List the platforms of every entry of `native` in full. A library that lists `ios` ships only to iPhones and iPads now, so a library that also served Mac Catalyst and visionOS adds `catalyst` and `visionos` to its `platforms`.
4. Move the paths of `native` that pointed into `platform/<template>/`, such as `platform/windows/steam_api64.dll`, to a folder of the app, such as `native/windows/`.
5. Add the `platforms` section with what the old projects held, as the next section describes.
6. Copy the current `.gitignore` of the reference, which keeps `build/`, `dist/` and `platform/android/keystore/` out of the repository.

Run the app on the desktop to check the runtime keys: every unknown key and every invalid value stops the start with a sentence that names it and says what is expected. The [reference of `haylen.json`](../lua-api/haylen.md#haylenconfig) lists every runtime key.

```sh
haylen app run ~/apps/my-game
```

### The build number

The `build` of `haylen.json` is the build number of every platform: `CFBundleVersion` on Apple platforms, `versionCode` on Android, the fourth number of the Windows version resource and the app build of protected releases, as [the version and build number](distribution.md#version-and-build-number) describe. The `version` is only the version that people see. A `haylen.json` without `build` builds as build 1, so an app that already reached a store sets it before its next upload:

1. Find the last `versionCode` that Google Play received in the Play Console, under the app bundles of the app, and the last build that App Store Connect received for the current version.
2. Set `build` above the highest of them, because Google Play refuses an upload whose `versionCode` is not higher than the last one.
3. If the app publishes content with `haylen content publish`, keep `build` at or above the build of the last publication, since a channel serves the build that published it and every later build.
4. Drop any build number that the scripts of continuous integration set on their own, so the `build` stays the only source.

```json
{
    "version": "1.2.3",
    "build": 48
}
```

## From platform projects to haylen.json

Go through each folder `platform/<template>/` of the app, compare it with the backend template of its platform under `tools/haylen/platforms/<backend>/template/` of the repository, and sort every difference:

| Group | How you recognize it | What to do |
| --- | --- | --- |
| A part of the old template | A file, a key or a step that the backend template holds or the backend writes itself, such as the include of `haylen/project.yml`, the reading of `haylen.properties` or `haylen_app`. | Drop it. The generated project has it. |
| A value of `haylen.json` | The name, the identifier, the version, the build, the orientation or the status bar. | Drop it. The backends take them from `haylen.json`. |
| A choice of the developer | A value that names your company, your app, your signing, your art or a feature you added, such as a usage description in your words or an extra permission. | Move it into a key of `platforms`, or keep the file in the app repository and name it in `files`. |

When every choice has a new home, delete the folder `platform/<template>/`. Only `platform/android/keystore/` stays, since the Android backend reads the signing keys of the app there.

### Apple

The folder `platform/apple/` held `project.yml`, the generated `App.xcodeproj`, `App.xcconfig`, the sources in `source/` and one folder per platform with its `Info.plist`, its entitlements and its asset catalog.

| Earlier file | Current place |
| --- | --- |
| `source/AppBridge.swift` and the other sources of the app | A folder of the app, such as `native/apple/`, which `platforms.apple.sources` lists. Xcode compiles it where it lies. |
| `source/main.m` | Generated. Handlers that `main` registered before `haylen_main` now register when their class loads, such as in `+load` of `AppBridgeLoader` in `native/apple/AppBridgeLoader.m` of the reference, which calls `[AppBridge registerHandlers]`. |
| `source/HaylenBridging.h` | Generated, so delete it. |
| The keys that you added to `ios/Info.plist` | `platforms.ios.infoPlist`, and `platforms.catalyst.infoPlist` too, since the iOS target of the old project also built the Mac Catalyst app. |
| The keys of `tvos/Info.plist`, `visionos/Info.plist` and `macos/Info.plist` | `platforms.tvos.infoPlist`, `platforms.visionos.infoPlist` and `platforms.macos.infoPlist`. |
| `ios/App.entitlements`, `ios/Catalyst.entitlements`, `tvos/App.entitlements`, `visionos/App.entitlements`, `macos/App.entitlements` | `platforms.ios.entitlements`, `platforms.catalyst.entitlements` and the `entitlements` of the other sections. |
| `PrivacyInfo.xcprivacy` at the root of the project | `platforms.apple.privacy`. |
| `DEVELOPMENT_TEAM` of `App.xcconfig` | `platforms.apple.team`, or the variable `HAYLEN_APPLE_TEAM` of the machine. |
| Other settings after the include of `App.xcconfig`, or in `settings` of `project.yml` | `platforms.apple.buildSettings`, which win over the settings of the backend, except the ones that the backend owns, such as the architectures, the signing, the deployment targets and the identity of the app, which come from their own keys. |
| The minimum versions in `options` of `project.yml` | `minimumSystemVersion` of the section of each platform, such as `platforms.ios.minimumSystemVersion`. |
| Your app icons in `Assets.xcassets`, or your `LaunchScreen.storyboard` | The same files in the app repository, which `files` of the section of each platform names, such as `{"Assets.xcassets": "art/ios/Assets.xcassets"}`. |
| Extra frameworks, Swift packages, resources, build phases or targets of `project.yml` | A plugin of the app in `plugins/<id>/`, whose contributions `framework`, `swiftPackage`, `appleResource`, `buildScript` and `appExtension` bring them, as the [plugin guide](../platform/plugins.md) describes. |

The `Info.plist`, the entitlements and the privacy manifest of the app no longer win over the plugins. They merge with the contributions of the plugins, and two owners that give one key different values stop the build with both named. So a usage description that a plugin takes as a parameter, such as `cameraUsage`, moves into the `parameters` of its entry in `haylen.json` instead of `infoPlist`.

```json
{
    "platforms": {
        "apple": {"sources": ["native/apple"], "team": "ABCDE12345"},
        "ios": {"infoPlist": {"ITSAppUsesNonExemptEncryption": false}, "files": {"Assets.xcassets": "art/ios/Assets.xcassets"}},
        "catalyst": {"infoPlist": {"ITSAppUsesNonExemptEncryption": false}}
    }
}
```

### Android

The folder `platform/android/` held a Gradle project that read `haylen/haylen.properties`, with the application class `App` and its bridge handlers in `app/src/main/kotlin/`.

| Earlier file | Current place |
| --- | --- |
| `App.kt` and the other Kotlin and Java sources of the app | A folder of the app, such as `native/android/`, which `platforms.android.sources` lists, with the class in `platforms.android.applicationClass`, such as `com.example.mygame.App`. |
| Permissions that you added to `AndroidManifest.xml` | `platforms.android.manifest.permissions`. |
| Permissions that you removed with `tools:node="remove"` | `platforms.android.manifest.removedPermissions`. |
| Attributes that you added to `<application>` | `platforms.android.manifest.applicationAttributes`, without the `android:` prefix. |
| `minSdk` of `app/build.gradle.kts` | `platforms.android.minimumSystemVersion`, an API level such as `"29"`. |
| Your launcher icon and TV banner in `app/src/main/res/` | The same files in the app repository, which `platforms.android.files` names by their paths in the generated project, such as `{"app/src/main/res/drawable/banner.xml": "art/android/banner.xml"}`. |
| Extra dependencies, components, intent filters or Gradle plugins | A plugin of the app in `plugins/<id>/`, whose contributions `gradleModule`, `mavenRepository`, `gradlePlugin` and `manifestPlaceholder` bring them. |
| `keystore/` with the upload key | It stays where it is, in `platform/android/keystore/`, which `haylen app sign` writes and the backend reads. |

### Web

The folder `platform/web/` held `index.html`, `loader.js`, `loader.css` and the page code `app.js`.

| Earlier file | Current place |
| --- | --- |
| `app.js` and other page code | A file of the app, such as `native/web/app.js`, which `platforms.web.scripts` lists. The page loads it after the loader. |
| `loader.css` with your values | A copy of the current `loader.css` of the web template with your values, which `platforms.web.files` names, such as `{"loader.css": "native/web/loader.css"}`. |
| `index.html` with your changes | A copy of the current `index.html` of the web template with your changes, named in `files`, which keeps the mark `{{SCRIPTS}}` where the tooling loads the loader and the page scripts. |
| `loader.js` | Generated from the template, which talks to the runtime of the engine. Move code of the app that you put in it into a page script. |

### Windows and Linux

The folders `platform/windows/` and `platform/linux/` held CMake projects that included `haylen/haylen.cmake` and called `haylen_app` on their executable.

| Earlier file | Current place |
| --- | --- |
| `resources/app.ico`, the icons of `resources/icons/` | The same files in the app repository, which `platforms.windows.files` or `platforms.linux.files` names, such as `{"resources/app.ico": "art/windows/app.ico"}`. |
| `resources/app.rc`, `resources/app.manifest` with your changes | A copy of the current file of the template with your changes, named in `files`. |
| `resources/app.desktop`, `resources/app.metainfo.xml` with your categories, licenses or content rating | Your files, named in `platforms.linux.files`. The backend completes them with the values of `haylen.json`, and what your file gives wins. |
| Extra steps of `CMakeLists.txt` | A copy of the current `CMakeLists.txt` of the template with your steps, named in `files`, which keeps the include of `app.cmake` and the target `main`. |
| The executable of `haylen_app` | Generated in `app.cmake` with `haylen_add_app`. |

## A C++ app

A C++ app of the earlier layout was a CMake project whose `CMakeLists.txt` added the engine and built the target `main` with `haylen_add_app`, next to `src/App.cpp` and `src/AppBridge.mm`. The current backends compile the C++ sources of the app themselves, against the prebuilt SDK, with the same project as a Lua app:

1. Add `"kind": "cpp"` to `haylen.json`, and the `cpp` section with the folders and files of the sources in `sources`, the include folders in `includeDirectories` and the preprocessor definitions in `definitions`.
2. Move `src/AppBridge.mm` to `native/apple/` and list that folder in `platforms.apple.sources`. Its `+load` keeps registering the handlers.
3. Delete `CMakeLists.txt` and the CPM cache in `.cache/`, since no app build compiles the engine.
4. Move the Android and web projects of `platform/` into `platforms` as the sections above describe.

```json
{
    "kind": "cpp",
    "cpp": {"sources": ["src"], "includeDirectories": ["src"], "definitions": ["MY_GAME_TRACE=0"]},
    "platforms": {
        "apple": {"sources": ["native/apple"]}
    }
}
```

The sources define `haylen::core::Application::create`, as before. Compile errors point to engine APIs that changed, and the public headers under `engine/include/haylen/` hold their current form. A library that the old `CMakeLists.txt` linked becomes a native library of the `native` section or of a plugin.

## Plugins

A plugin lives in `plugins/<id>/` of the app, in a folder anywhere on disk or in a git repository, at its root or in a folder of it, which `haylen.json` references. Its `plugin.json` changes shape, while its Lua API in `source/` and its native parts stay where they are.

### From the earlier plugin.json

| Earlier key | Current key |
| --- | --- |
| None | `"interface": 1`, the version of the plugin interface that the manifest follows. |
| `platforms`, a list | `implementations`, an object that maps every supported platform to `native`, when the plugin needs its native part there, or `lua`, when its Lua API works alone. A platform that is missing does not support the plugin. |
| `requires`, a list of ids | `dependencies`, an object that maps every id to the oldest version of the same major version that the plugin works with. |
| `permissions`, `parameters` | The same. |
| `apple.class` | A contribution `appleClass` with `name`. |
| `apple.sources` | A contribution `appleSources` with `path`. |
| `apple.packages`, by name | A contribution `swiftPackage` per package, with `name`, `url`, `exactVersion` and `products`. |
| `apple.frameworks` | A contribution `framework` per framework, with `name`. |
| `apple.binaryFrameworks` | A contribution `binaryFramework` per framework, with `path` and `embed`. |
| `apple.infoPlist`, `apple.entitlements`, `apple.privacy` | A contribution `infoPlist`, `entitlements` or `privacy`, with the keys in `values`. |
| `apple.resources` | A contribution `appleResource` per file, with `path`. |
| `apple.buildScripts` | A contribution `buildScript` per script, with `name`, `script`, `inputFiles` and `outputFiles`. |
| `apple.extensions` | A contribution `appExtension` per extension, whose `type` is now `kind` and whose `platforms` are the platforms of the contribution. |
| `apple.assetPacks` | A contribution `assetPacks` with `prefetch`. |
| `android.module` | A contribution `gradleModule` with `path`. |
| `android.repositories` | A contribution `mavenRepository` per address, with `url`. |
| `android.gradlePlugins` | A contribution `gradlePlugin` per plugin, with `id` and `version`. |
| `android.placeholders`, by name | A contribution `manifestPlaceholder` per placeholder, with `name` and `value`. |
| `android.files` | A contribution `projectFile` per file, with `from` and `to`. |
| `android.assetPacks` | A contribution `assetPacks` with `fastFollow`. |
| `web.module` | A contribution `webModule` with `path`. |
| `desktop.urlSchemes` | A contribution `urlScheme` per scheme, with `scheme`. |
| `native` | A contribution `nativeLibrary` with `files` or `cmake`, `link` and `symbols`, whose `platforms` are the platforms of the contribution. |

Every contribution names the platforms it serves in `platforms`, so one value that only some platforms need, such as a usage description that tvOS lacks, is a contribution of its own. The placeholders `${parameter}` keep working in every field. The [plugin guide](../platform/plugins.md) lists every contribution type with its fields and how it merges.

This is the sign-in plugin of a sample before and after:

```json
{
    "id": "sign-in",
    "name": "Sign-In",
    "version": "1.0.0",
    "description": "Signs the player in with an account of the platform.",
    "platforms": ["android", "web"],
    "parameters": {
        "clientId": {"type": "string", "default": "", "description": "The web client id of the identity service."}
    },
    "android": {"module": "android"},
    "web": {"module": "web/sign-in.js"}
}
```

```json
{
    "interface": 1,
    "id": "sign-in",
    "name": "Sign-In",
    "version": "1.0.0",
    "description": "Signs the player in with an account of the platform.",
    "implementations": {"android": "native", "web": "native"},
    "parameters": {
        "clientId": {"type": "string", "default": "", "description": "The web client id of the identity service."}
    },
    "contributions": [
        {"type": "gradleModule", "platforms": ["android"], "path": "android"},
        {"type": "webModule", "platforms": ["web"], "path": "web/sign-in.js"}
    ]
}
```

And the Apple and desktop parts of a plugin that served every platform:

```json
{
    "apple": {
        "class": "NativeDemoPlugin",
        "sources": "apple",
        "frameworks": ["AVFoundation.framework"],
        "infoPlist": {"NSCameraUsageDescription": "${cameraUsage}"}
    },
    "desktop": {"urlSchemes": ["${urlScheme}"]},
    "native": {"cmake": "native", "platforms": ["macos", "windows", "linux"]}
}
```

```json
{
    "contributions": [
        {"type": "appleClass", "platforms": ["ios", "catalyst", "tvos", "visionos", "macos"], "name": "NativeDemoPlugin"},
        {"type": "appleSources", "platforms": ["ios", "catalyst", "tvos", "visionos", "macos"], "path": "apple"},
        {"type": "framework", "platforms": ["ios", "catalyst", "tvos", "visionos", "macos"], "name": "AVFoundation.framework"},
        {"type": "infoPlist", "platforms": ["ios", "catalyst", "visionos", "macos"], "values": {"NSCameraUsageDescription": "${cameraUsage}"}},
        {"type": "urlScheme", "platforms": ["windows", "linux"], "scheme": "${urlScheme}"},
        {"type": "nativeLibrary", "platforms": ["macos", "windows", "linux"], "cmake": "native"}
    ]
}
```

Compare the native parts with the reference plugin too, since its templates follow the current engine:

```sh
diff -ru /tmp/reference-plugin ~/apps/my-game/plugins/my-plugin
```

| Part | What to compare |
| --- | --- |
| `apple/` | The plugin class, its context and how it reports requirements and permissions. Keep your Swift or Objective-C code. |
| `android/` | `build.gradle.kts`, with its versions and its dependencies on `haylen-plugins`, `haylen-links` or `haylen-coroutines`, and the plugin class. Take the versions of the reference. |
| `web/` | The ES module and how it registers with its context. |
| `native/` | The C++20 library of the desktop, built with `find_package(haylen CONFIG REQUIRED)` and `haylen::native`, and its init function behind `extern "C"`. |
| `.gitignore`, `.editorconfig`, `.clang-format` | Take the reference. |

### The plugins of haylen.json

Every entry of the `plugins` section of `haylen.json` is an object whose parameter values sit under `parameters`, next to the optional `path`, `git` and `ref` that say where the plugin comes from, as [haylen.json](../platform/plugins.md#haylenjson) describes. An entry that holds its values directly fails with `The key "plugins.<id>.<name>" is not a key of a plugin entry`, so move the values into `parameters`:

```json
{
    "plugins": {
        "ads": {"parameters": {"iosAppId": "ios-app-1234", "testMode": true}}
    }
}
```

A plugin that comes from a git repository needs its commit in `haylen.lock` before a Release build takes it. The command `plugin update` pins the commit that its branch or tag names now, and the file `haylen.lock` belongs in the repository of the app:

```sh
haylen plugin update ads --app ~/apps/my-game
```

A plugin that the app keeps as a copy in `plugins/<id>/`, but that lives in a folder or a git repository of its own, can become a reference instead, so the app follows the plugin without copying it again. Run `haylen plugin add` with the folder or the repository and `--reference`, which writes the entry and pins a repository in `haylen.lock`:

```sh
haylen plugin add ~/plugins/my-plugin --reference --app ~/apps/my-game
haylen plugin add https://example.com/plugins.git --ref 1.0.0 --path share --reference --app ~/apps/my-game
```

A repository that holds several plugins, one in each folder, takes `--path` with the folder of the plugin, which the entry keeps as `path` next to `git` and `ref`. Then list the plugins of the app, which reports each problem that keeps one from building and where each plugin comes from:

```sh
haylen plugin list --app ~/apps/my-game
```

### The permissions of plugins

A plugin that asks for a permission of a capability it serves, such as `camera`, `microphone`, `location`, `preciseLocation`, `backgroundLocation`, `notifications` or `photos`, owns that permission. It declares the names in `permissions` of its `plugin.json`, registers each one with the context of every native part, and keeps calling `requirePermission` before it uses the capability, as the [permissions](../platform/plugins.md#permissions) section describes. A name that the plugin does not register fails `requirePermission`.

```json
{
    "permissions": ["camera"]
}
```

```swift
context.register(HaylenPermission(name: "camera", capability: "the camera", requirements: [.usageDescription("NSCameraUsageDescription")], state: { answer in
    let status = AVCaptureDevice.authorizationStatus(for: .video)
    answer(status == .authorized ? .granted : status == .notDetermined ? .prompt : .denied, nil)
}, request: { answer in
    AVCaptureDevice.requestAccess(for: .video) { granted in answer(granted ? .granted : .denied, nil) }
}))
```

```kotlin
context.registerPermission(HaylenPermission("camera", "the camera", Manifest.permission.CAMERA))
```

An Android part that fails a call on a device without the capability, such as a check of `hasSystemFeature` before `requirePermission`, moves that check into `unsupportedReason` of its `HaylenPermission`. The permission then answers `unsupported` with the reason, both to `system.permission` and to `requirePermission`:

```kotlin
private class CameraPermission : HaylenPermission("camera", "the camera", Manifest.permission.CAMERA) {
    override fun unsupportedReason(context: Context): String? {
        return if (context.packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY)) null else "This device has no camera."
    }
}
```

```js
context.registerPermission({ name: "camera", capability: "the camera", needs: { secureContext: true }, query: "camera", request: () => askForCamera() });
```

The app keeps calling `system.permission` and `system.requestPermission` with the same names once a plugin of the app declares them. A name that no plugin of the app declares raises an error that says so. The clipboard has no permission, so a read that the person refuses fails with the code `denied`. The library of photos has no permission in the engine, and the photo picker of the system, which a dialogs plugin offers, needs none.

### The motion sensors of the device

The accelerometer, the gyroscope and the attitude of phones and tablets belong to the `motion` plugin, which owns the permission `motion`. The engine keeps the motion sensors of gamepads. Add the plugin to the app with `haylen plugin add`, from its folder or its repository, and give it the text of the prompt of iOS in its parameter `motionUsage`:

```sh
haylen plugin add ~/plugins/motion --reference --app ~/apps/my-game
```

```json
{
    "plugins": {
        "motion": {"path": "../plugins/motion", "parameters": {"motionUsage": "Tilt the phone to roll the ball."}}
    }
}
```

Then replace each call of `haylen.input` with the call of the plugin, which takes the same sensors, axes and units:

| Before | After |
| --- | --- |
| `input.startMotion({sensors, rate, owner})` | `motion.start({sensors, rate, owner})`, whose `await` gives `{sensors}` once they run, or `nil` and the error with the code `unsupported`, `denied`, `failed` or `cancelled`. |
| `request:stop()` of a `MotionRequest` | `motion.stop()`. A new `motion.start` replaces what runs. |
| `input.motionSensors()` | `motion.sensors()` |
| `input.acceleration()`, `input.rotationRate()`, `input.attitude()` | `motion.acceleration()`, `motion.rotationRate()`, `motion.attitude()` |
| `input.tilt()`, `input.calibrateTilt()`, `input.resetTilt()` | `motion.tilt()`, `motion.calibrateTilt()`, `motion.resetTilt()` |
| `input.startMotion({device = false, gamepads = true, sensors, owner})` | `input.startGamepadMotion({sensors, owner})`, which returns a `GamepadMotionRequest` that runs at once. |
| The option `motion` of `input.newActionMap` and of `map.devices` | Leave it out. The tilt bindings read the gamepads of the map. |

The bindings `tilt:` and `stick:tilt` of the action map read the tilt of gamepads. A game that also rolls with the tilt of the phone binds a virtual stick next to them and sets it from the plugin every frame:

```lua
local input = require('haylen.input')
local motion = require('motion')
local scene = require('haylen.scene')

input.loadActions({actions = {
    {name = 'roll', type = 'vector', bindings = {'stick:left', 'stick:tilt', 'virtualStick:tilt'}},
}})

scene.push({
    enter = function(self)
        motion.start({sensors = {'accelerometer'}, owner = self})
    end,
    update = function(self, dt)
        input.setVirtualStick('tilt', motion.tilt())
        local x, y = input.vector('roll')
        self.x = (self.x or 0) + x * 400 * dt
        self.y = (self.y or 0) + y * 400 * dt
    end,
})
```

The app keeps checking and asking for the permission with `system.permission('motion')` and `system.requestPermission('motion')`, which the plugin answers now. The text of the prompt lives in the parameter `motionUsage` alone, since an `NSMotionUsageDescription` in `platforms.ios.infoPlist` with other words would stop the build as a conflict with the plugin.

### Opening links and vibrating

The engine opens no urls and vibrates nothing. The plugin `links` opens a url, and the plugin `haptics` vibrates the device and plays the feedback of its haptic engine. List them in `haylen.json` with the folder or the repository where they live:

```json
{
    "plugins": {
        "links": {"path": "../plugins/links"},
        "haptics": {"path": "../plugins/haptics"}
    }
}
```

Then replace `system.openUrl(url)` with `links.open(url)` and `system.vibrate(seconds)` with `haptics.vibrate(seconds)`. Both return a platform call, whose `await` gives `true`, or `nil` and an error whose `code` is `unsupported` or `denied` and whose `message` says why. So a check of the third value that `await` returned, the failure of `haylen.system`, becomes a check of the second one. A refusal that nothing awaits is logged once and never stops the app, as before.

```lua
local async = require('async')
local haptics = require('haptics')
local links = require('links')

async.spawn(function()
    local opened, err = links.open('https://example.com'):await()
    print(opened and 'Opened.' or err.message)
end)
haptics.vibrate(0.08)
```

On Android the permission `android.permission.VIBRATE` comes with the manifest of the `haptics` module. The generated manifest keeps it for the rumble of gamepads.

### Native dialogs

The engine has no `haylen.dialogs`. The native message boxes and pickers live in the plugin `dialogs`, whose Lua API keeps the functions, options, answers, errors and calls of the module: `message`, `openFiles`, `saveFile` and `openFolder`, with `openPhotos`, the photo picker of the system, which needs no permission. Add the plugin to the app, by a copy or by a reference to its folder:

```sh
haylen plugin add ~/plugins/dialogs --app ~/apps/my-game
```

Then require the plugin by its id instead of the module of the engine:

```lua
local dialogs = require('dialogs')

local button = dialogs.message({title = 'Unsaved map', text = 'Save the changes before leaving?', kind = 'warning', buttons = {'Save', 'Discard', 'Cancel'}}):await()
```

These things change with the move:

- C++ code calls the methods of the plugin through the bridge, such as `engine.getPlatform().call("dialogs.message", ...)`, with the parameters and answers that the README of the plugin lists, instead of `engine.getDialogs()`.
- Picked files that have no path of their own, on phones, tablets and the web, are copies in a folder of the plugin, which the README of the plugin names, and no longer under `tmp/dialogs` of the storage on every platform.
- On Linux the pickers and messages load GTK 3 when they show. The Debian package of the Linux project recommends GIO and the sound servers alone, so an app that shows dialogs gives the Linux project a copy of the `CMakeLists.txt` of the template through `platforms.linux.files`, with `libgtk-3-0t64 | libgtk-3-0` added to `CPACK_DEBIAN_PACKAGE_RECOMMENDS`.

## The commands

Every command of the earlier `haylen.py` has its place in the current command line, which the [command reference](commands.md) describes:

| Earlier command | Current command |
| --- | --- |
| `haylen run <app>` | `haylen app run <app>`, which plays a Lua app in the player of this machine. |
| `haylen run <app> --platform <platform>` | `haylen app run <app> --target <target>`. |
| `haylen run <app> --platform <platform> --build-only` | `haylen app build <app> --target <target>`. |
| `haylen prepare <app> --platform <platform>` | `haylen app prepare <app> --target <target>`. |
| `haylen check <app> --platform <platform>` | `haylen app verify <app> --target <target>`. |
| `haylen platform add`, `platform diff`, `platform xcodegen` | None, since every build generates its project. The command `haylen project inspect` shows what the project receives. |
| `haylen package <app>` | `haylen app zip <app>`. |
| `haylen shaders <app>` | `haylen app shaders <app>`. |
| `haylen serve <app>` | `haylen app serve <app>`, while `haylen serve <folder>` serves a folder. |
| `haylen capture <app>` | `haylen app capture <app>`. |
| `haylen distribution installer <app> --platform <platform>` | `haylen app package <app> --target <target> --format installer`. |
| `haylen distribution steam <app> --platform <platform> --app-id <id> --depot <name>=<id>` | `haylen app package <app> --target <target> --format steam --option appId=<id> --option depots=<name>=<id>`. |
| `haylen distribution android-key <app>` | `haylen app sign <app> --target android`, with `--kind debug` for the debug key and `--option` for the alias, the password, the name and `force`. |
| `haylen content <action> --platform <platform>` | `haylen content <action> --target <target>`. |
| `haylen engine` | `haylen sdk build --target <target>`. |
| `haylen sdk --check-consumers` | `haylen engine consumers`. |
| `haylen configure`, `build`, `bench`, `fuzz`, `coverage`, `format`, `clean`, `tools` | `haylen engine configure`, `engine build`, `engine bench`, `engine fuzz`, `engine coverage`, `engine format`, `engine clean` and `engine tools`. |
| `haylen new`, `devices`, `plugin`, `test` | The same names. |

Scripts of continuous integration change in the same way, and they build the SDK of each target once before the apps.

## Verify

An upgrade is done when every target of the app resolves, builds and passes its verification:

```sh
haylen project inspect ~/apps/my-game --target ios-simulator
haylen app prepare ~/apps/my-game --target ios-simulator
haylen app build ~/apps/my-game --target android
haylen app verify ~/apps/my-game --target android
haylen app build ~/apps/my-game --target web --config Release
haylen app verify ~/apps/my-game --target web --config Release
```

The command `project inspect` shows every value that the generated project receives, with its origin, so a key that went to the wrong section or a conflict between the app and a plugin shows before anything builds. The command `app verify` examines the last build for what the engine and every plugin need, and for what a release never ships. Run the app on every target it ships to, in Debug and in Release, and try each feature that uses a plugin. When everything passes, delete the folders `platform/<template>/` but `platform/android/keystore/`, delete the reference folders and commit the upgrade as one change.

```sh
rm -rf /tmp/reference-app /tmp/reference-cpp /tmp/reference-plugin
```

## For AI assistants

An assistant that upgrades an app follows the steps of this guide in order and never guesses a value. Its rules:

1. Read the current layout from a reference made with `haylen new` and `haylen plugin new`, and from the backend templates under `tools/haylen/platforms/<backend>/template/`. Never rely on memory of an earlier layout.
2. Sort every difference of a platform project into a part of the old template, a value of `haylen.json` or a choice of the developer, as [from platform projects to haylen.json](#from-platform-projects-to-haylenjson) describes. Drop the first two, and move each choice into a key of `platforms`, a file that `files` names or a plugin of the app.
3. Never edit what the tooling generates under `build/`, and never keep a platform project in the app, apart from the signing keys in `platform/android/keystore/`.
4. Convert every `plugin.json` key by key with the table of [the earlier plugin.json](#from-the-earlier-pluginjson), and keep every parameter with its default.
5. Treat every error of `project inspect`, of a build, of `app verify` or of the start of the app as the list of what remains, and fix each one where its sentence points, which names the manifest or the plugin it comes from.
6. Report every choice of the developer that it moved and where it went, so a person can review them.
