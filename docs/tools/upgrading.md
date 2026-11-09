# ⬆️ Upgrading projects and plugins

This guide explains how to bring an app and its plugins up to the current templates of Haylen. It is written for people and for AI assistants alike. It says where to look, what to compare and what to change in every folder and file, so nobody has to work the process out again.

## Why projects fall behind

The templates of Haylen change over time. A new Xcode needs a new setting, Android Gradle Plugin moves a key, a CMake project gains a step, or `app.json` gains a field. An app copies the templates once, when `haylen new` creates it, and then the copies belong to the developer. The same is true of a plugin that `haylen plugin new` created. So an older app and an older plugin keep the shape of the templates of their time.

Two kinds of files exist in an app:

- Files that the developer owns: `app.json`, `source/`, `content/`, the root files and every `platform/<platform>/` project. Haylen never edits them, so an upgrade changes them by hand.
- Files that Haylen writes: the folder `haylen/` inside each platform project and everything under `build/`. These are written again by every build, so they are always current, and an upgrade never touches them.

An upgrade is the work of making the first kind match the current templates again, while keeping every choice that the developer made on purpose.

## Before you start

1. Update the engine, the repository of Haylen that you build with, to the version you want.
2. Commit or set aside every change of the app and of its plugins, so the upgrade is one diff that you can read and undo.
3. Build the engine artifacts of the new version once, which also proves that the toolchain of the machine is current.

```sh
cd ~/haylen
git pull
haylen engine
```

## Make a reference

The current templates are the reference. The simplest way to read them is a new app and a new plugin made with the same options as yours, in a scratch folder that you delete at the end.

```sh
haylen new /tmp/reference-app --kind lua --name "My Game" --identifier com.example.mygame --orientation landscape
haylen new /tmp/reference-cpp --kind cpp
haylen plugin new /tmp/reference-plugin --id my-plugin --with camera,microphone
```

Use `--kind cpp` for a C++ app, and give `plugin new` the same id and the same `--with` features that your plugin started with, so the reference holds the same blocks of code.

## Compare the whole app

Compare the app folder with the reference, leaving out what is written by builds and what is your own code and art:

```sh
diff -ru --exclude=build --exclude=haylen --exclude=content --exclude=source /tmp/reference-app ~/apps/my-game
```

Then compare each platform project with its template, one at a time. The command `platform diff` prints the files that only one side has and a unified diff of every text file that differs. It leaves out `haylen/`, build outputs and the state of Xcode for each person, and it changes nothing.

```sh
haylen platform diff ~/apps/my-game apple
haylen platform diff ~/apps/my-game android
haylen platform diff ~/apps/my-game web
haylen platform diff ~/apps/my-game windows
haylen platform diff ~/apps/my-game linux
```

A C++ app has only `android` and `web` projects, since its `CMakeLists.txt` builds the other platforms. An app without a `platform/<platform>/` folder needs nothing for that platform, because it builds a fresh copy of the template every time.

## How to read each difference

Every difference falls into one of three groups. Decide the group first, then act:

| Group | How you recognize it | What to do |
| --- | --- | --- |
| A change of the template | A setting, a key, a step, a file or a version that the reference has and your project lacks or has in an older form, and that you never chose. | Adopt it as the reference has it. |
| A choice of the developer | A value that names your company, your app, your signing, your store, your art or a feature you added, such as a usage description in your words, an extra framework or a custom build step. | Keep it. |
| Both | A file where the template changed and you also edited it, such as `project.yml` with your extra target. | Merge by hand: take the new lines of the template and keep your lines. |

The values that come from `app.json`, such as the name, the identifier, the version and the orientation, never need copying into a project, since builds write them into `haylen/`. A project that holds one of those values by hand should read it from `haylen/` again, the way the reference does.

## The root of the app

| File | What to compare | What usually changes |
| --- | --- | --- |
| `app.json` | Run the app. Every unknown key and every invalid value stops the start with a sentence that names it and says what is expected. Compare with the reference and with [every field of `app.json`](../lua-api/haylen.md#haylenconfig). | A key that was renamed or moved into another section, a value whose names changed, and new fields whose defaults you may want to set. |
| `.gitignore` | Diff it with the reference. | New build outputs and caches of tools that the app must never commit. |
| `.editorconfig`, `.clang-format` | Diff them with the reference. | The style of the code of the engine, which your native code follows. |
| `source/` | Run the app on the desktop with `haylen run`, then on each platform. A removed Lua function or option raises an error that names it. Look the name up in the [Lua API reference](../lua-api/README.md), which documents the current form. | Calls that became asynchronous and return a promise, options that moved into an options table, and names that changed. |
| `content/` | Usually nothing. Shaders of the app compile again by themselves with `haylen shaders`. | A shader that includes the shader library of the engine may need the current names of the library. |

## The Apple project

The project lives in `platform/apple/`. Its source of truth is `project.yml`, and `App.xcodeproj` is generated from it.

| File | What to compare | Notes |
| --- | --- | --- |
| `project.yml` | `options` (the minimum versions), `settings`, the targets and their `templates`, `sources`, `excludes` and the `include` of `haylen/project.yml`. | Merge: keep your own targets, frameworks and settings, and take the new keys of the reference. Never remove the include or the `templates` of a target unless you link the engine yourself. |
| `App.xcconfig` | The `#include "haylen/Haylen.xcconfig"` line and the settings around it. | Your settings after the include win, so keep them. |
| `ios/Info.plist`, `tvos/Info.plist`, `macos/Info.plist`, `visionos/Info.plist` | Every key of the reference. | Keep your usage descriptions and your keys. Take the keys that the reference gained. Keys of plugins and of `app.json` are filled into the completed copy in `haylen/` by the build, so they need no copy here. |
| Entitlements | The entitlements files of the reference, if any. | Keep yours. Plugins add theirs at build time. |
| `ios/LaunchScreen.storyboard` | Diff it. | It shows the splash of `app.json`, so keep your art in `app.json`, not in the storyboard. |
| `source/main.m`, `source/AppBridge.swift`, `source/HaylenBridging.h` | Diff them. | Keep your bridge handlers. Take changes of the registration code and of the bridging header. |
| Asset catalogs | Diff the `Contents.json` files. | Keep your icons. |

When `project.yml` is done, generate the Xcode project again and build:

```sh
haylen platform xcodegen ~/apps/my-game
haylen run ~/apps/my-game --platform ios-simulator
```

If `run` says that `App.xcodeproj` changed since its last generation, move the changes that were made in Xcode into `project.yml`, then generate it again. The section on [generating App.xcodeproj](distribution.md#generating-appxcodeproj) explains that check.

## The Android project

The project lives in `platform/android/`. It is a Gradle project that reads `haylen/haylen.properties`.

| File | What to compare | Notes |
| --- | --- | --- |
| `settings.gradle.kts` | The plugin management, the repositories, the inclusion of plugin modules and asset packs, and the extra property `haylenEngineVersion`. | Take the reference. Keep only repositories and modules that you added yourself. |
| `build.gradle.kts` (root) | The versions of the Android Gradle Plugin and Kotlin, and the build script repositories. | Take the versions of the reference, since the engine builds with them. |
| `app/build.gradle.kts` | `compileSdk`, `targetSdk`, `minSdk`, `ndkVersion`, `namespace`, the dependencies on the `haylen` libraries, among them the ones of `engineLibraries`, such as `haylen-playback` for the audio that plays in the background, signing, and the way it reads `haylen.properties`. | Keep your signing, flavors and extra dependencies. Take every value that the reference reads from `haylen.properties`. |
| `gradle.properties` | Every property. | Keep your memory settings if you raised them. |
| `gradle/wrapper/gradle-wrapper.properties` | The Gradle version and its checksum. | Take the reference. |
| `app/src/main/AndroidManifest.xml` | Permissions, the activity, its intent filters, `android:configChanges` and the application class. | Keep your permissions and filters. Take what the reference gained, such as a permission that a newer system requires. A permission you removed on purpose with `tools:node="remove"` stays removed. |
| `app/src/main/kotlin/` | The `App` class and its bridge handlers. | Keep your handlers. Take changes to how they register. |
| `app/src/main/res/` | Icons, themes and values. | Keep your icons. Take changes to the themes of the template, which the status bar and the splash depend on. |

Then build and install on the emulator:

```sh
haylen run ~/apps/my-game --platform android
```

## The web project

The project lives in `platform/web/`.

| File | What to compare | Notes |
| --- | --- | --- |
| `index.html` | The structure of the page, the elements that `loader.js` uses and the scripts it loads. | Keep your texts and your extra tags. |
| `loader.js` | The whole file. | Take the reference, since it talks to the runtime of the engine. Keep only edits that you made on purpose, and move them to `app.js` if they are app code. |
| `loader.css` | The variables at the top and the rules below them. | Keep your values of the variables. Take the rules of the reference. |
| `app.js` | The page code and its bridge handlers. | Keep your handlers. |

```sh
haylen run ~/apps/my-game --platform web
```

## The Windows and Linux projects

The projects live in `platform/windows/` and `platform/linux/`. Each is a CMake project that includes `haylen/haylen.cmake` and calls `haylen_app` on its executable.

| File | What to compare | Notes |
| --- | --- | --- |
| `CMakeLists.txt` | `cmake_minimum_required`, the include of `haylen/haylen.cmake`, `haylen_app(<target>)`, the install rules and the packaging. | Merge: keep your extra sources and libraries. |
| `signing.cmake` (Windows) | The signing step. | Keep the name of your certificate variable. |
| `resources/` | The icon, the version resource and the manifest on Windows. The desktop entry, the metainfo and the icons on Linux. | Keep your icons and texts. Take new fields of the manifest, the desktop entry and the metainfo. |

```sh
haylen run ~/apps/my-game --platform windows
haylen run ~/apps/my-game --platform linux
```

## A C++ app

A C++ app is a CMake project, so compare these files with the C++ reference as well:

| File | What to compare | Notes |
| --- | --- | --- |
| `CMakeLists.txt` | The version range of CMake, how it adds the engine (`HAYLEN_ENGINE_SOURCE` or `find_package`), the `haylen_add_app` call and its options. | Keep your sources and libraries. |
| `src/App.cpp` | The app class and the desktop bridge handlers. | Your code stays. Compile errors point to engine APIs that changed, and the public headers under `engine/include/haylen/` hold their current form. |
| `src/AppBridge.mm` | The Objective-C bridge handler. | Keep your handlers. |

## Plugins

A plugin lives in `plugins/<id>/` of the app, in a folder anywhere on disk or in a repository of its own, which `app.json` references. Compare it with the reference plugin made with the same id and features:

```sh
diff -ru /tmp/reference-plugin ~/apps/my-game/plugins/my-plugin
```

| Part | What to compare | Notes |
| --- | --- | --- |
| `plugin.json` | Every section against [the plugin.json format](../platform/plugins.md#pluginjson). Builds validate it, and an unknown key or an invalid value stops the build with a sentence that names it. | Keys of the `apple`, `android`, `web` and `native` sections change most often. Keep your parameters and their defaults. |
| `source/` | The Lua API of the plugin, against the [Lua API of a plugin](../platform/plugins.md#the-lua-api-of-a-plugin). | Calls that a platform may gate answer with states and promises, as the [permissions](../platform/plugins.md#permissions) section describes. |
| `apple/` | The plugin class, its context and how it reports requirements and permissions. | Keep your Swift or Objective-C code. Take changes to the protocol and to the helper calls. |
| `android/` | `build.gradle.kts` (its versions, `compileSdk`, `minSdk` and the dependencies on `haylen-plugins`, `haylen-links` or `haylen-coroutines`) and the plugin class. | Take the versions of the reference. Keep your dependencies and code. |
| `web/` | The ES module and how it registers with its context. | Keep your code. |
| `native/` | `CMakeLists.txt` and the init function of the desktop library. | Keep your code. |
| `.gitignore`, `.editorconfig`, `.clang-format` | Diff them. | Take the reference. |

### The plugins of app.json

Every entry of the `plugins` section of `app.json` is an object whose parameter values sit under `parameters`, next to the optional `path`, `git` and `ref` that say where the plugin comes from, as [app.json](../platform/plugins.md#appjson) describes. An entry that holds its values directly fails with `The key "plugins.<id>.<name>" is not a key of a plugin entry`, so move the values into `parameters`:

```json
{
    "plugins": {
        "ads": {"parameters": {"iosAppId": "ios-app-1234", "testMode": true}}
    }
}
```

A plugin that the app keeps as a copy in `plugins/<id>/`, but that lives in a folder or a repository of its own, can become a reference instead, so the app follows the plugin without copying it again. Run `haylen plugin add` with the folder or the repository and `--reference`, which writes the entry with the values that the app gives and deletes the copy:

```sh
haylen plugin add ~/plugins/my-plugin --reference --app ~/apps/my-game
haylen plugin add https://github.com/haylen-org/share.git --ref 1.0.0 --reference --app ~/apps/my-game
```

An app with a web project of its own takes `loader.js` of the reference, which hands each plugin module the `parameters` of its entry in `config.json`.

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

Then list the plugins of the app, which reports each problem that keeps one from building and where each plugin comes from:

```sh
haylen plugin list --app ~/apps/my-game
```

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

The app keeps checking and asking for the permission with `system.permission('motion')` and `system.requestPermission('motion')`, which the plugin answers now. An `Info.plist` of the Apple project that holds its own `NSMotionUsageDescription` keeps it, since the value of the developer wins over the `motionUsage` of the plugin.

### Opening links and vibrating

The engine opens no urls and vibrates nothing. The plugin `links` opens a url, and the plugin `haptics` vibrates the device and plays the feedback of its haptic engine. List them in `app.json` with the folder or the repository where they live:

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

On Android the permission `android.permission.VIBRATE` comes with the manifest of the `haptics` module. The manifest of the template keeps it for the rumble of gamepads.

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
- On Linux the pickers and messages load GTK 3 when they show. The Debian package of the Linux template recommends GIO and the sound servers alone, so an app that shows dialogs adds `libgtk-3-0t64 | libgtk-3-0` to `CPACK_DEBIAN_PACKAGE_RECOMMENDS` of `platform/linux/CMakeLists.txt`.

## Verify

An upgrade is done when every platform of the app builds, runs and passes its checks:

```sh
haylen check ~/apps/my-game --platform ios-simulator
haylen check ~/apps/my-game --platform android
haylen check ~/apps/my-game --platform web
haylen check ~/apps/my-game --platform ios-simulator --config Release
```

The command `check` compares the last build with what the engine and every plugin need, and prints each missing piece with the snippet to add and the file to add it to, as [check](distribution.md#check) describes. Run the app on every platform it ships to, in Debug and in Release, and try each feature that uses a plugin. When everything passes, delete the reference folders and commit the upgrade as one change.

```sh
rm -rf /tmp/reference-app /tmp/reference-cpp /tmp/reference-plugin
```

## For AI assistants

An assistant that upgrades a project follows the steps of this guide in order and never guesses a value. Its rules:

1. Read the current templates from a reference made with `haylen new` and `haylen plugin new`, and compare with `diff` and `haylen platform diff`. Never rely on memory of an older template.
2. Sort every difference into a change of the template, a choice of the developer or both, as [how to read each difference](#how-to-read-each-difference) describes. Adopt changes of the template, keep choices of the developer, and merge files that hold both.
3. Never edit `haylen/` or `build/`, since builds write them again.
4. Never copy values of `app.json` into a project, and never remove the lines that include `haylen/`.
5. Generate `App.xcodeproj` from `project.yml` with `haylen platform xcodegen`, and never edit `project.pbxproj` by hand.
6. Treat every error of a build, of `check` or of the start of the app as the list of what remains, and fix each one where its sentence points.
7. Report every choice of the developer that it kept and every merge it made, so a person can review them.
