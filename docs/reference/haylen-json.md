# 🧾 `haylen.json`

This page lists every key of `haylen.json`, the manifest at the root of every app package, with its type, its default and what it does. The engine reads the keys of the app before any Lua runs, and the tooling reads the keys of the generated projects and the plugins. Every key is optional, and an unknown key or a wrong value stops the app with one of the [errors](#errors-of-haylenjson) below. The [Lua guide](../core/lua.md#haylenjson) explains what the file is for, [`haylen.config`](../lua-api/haylen.md#haylenconfig) gives a script every key with its default, and the schema `tools/haylen/schemas/haylen.schema.json` describes every key for editors.

Colors are written as `"#RRGGBB"` or `"#AARRGGBB"`. A key marked "absent" has no value in `haylen.config` until `haylen.json` sets it.

## Identity

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `"Haylen App"` | The name of the app that people see. An empty name or `null` is the default name too, the same one that the tooling gives the projects of the app, as the [distribution guide](../tools/distribution.md#name) describes. |
| `identifier` | string | The identifier of the location of the package | A unique id in reverse domain form. It also names the folder where [`haylen.storage`](../lua-api/storage.md) and [`haylen.preferences`](../lua-api/preferences.md) keep their files. A package without one takes the identifier of where it comes from, its folder or zip file, the address of a launch or the project of a web editor, by the one rule that the tooling and the player follow, as the [distribution guide](../tools/distribution.md#identifier) describes. |
| `version` | string | `"1.0.0"` | The version of the app that people see, such as `"1.2.0"`. |
| `build` | integer | `1` | The build number of the app, a whole number from 1 to 2100000000 that grows with every upload to a store, such as 1, 2 and 3. Every platform takes its build number from it, as the [distribution guide](../tools/distribution.md) describes. Another value stops the start with `The "build" value <value> in "haylen.json" is not a build number. It is a whole number from 1 to 2100000000 that grows with every upload of the app.` |
| `description` | string | The value of `name` | One sentence about the app. The Windows and Linux templates give it to the executable, the desktop entry, the AppStream metainfo and the packages. |
| `company` | string | The value of `name` | The person or company that publishes the app, in the same places. |
| `copyright` | string | `"Copyright © <company>"` | The copyright notice of the app, which the Windows executable and the installer show. |
| `website` | string | `""` | The address of the website of the app. The AppStream metainfo, the `.deb` package and the Windows installer link to it. |

## Window

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `window.title` | string | The value of `name` | The title of the window. |
| `window.width`, `window.height` | integer | `1280`, `720` | The size of the window when it opens. |
| `window.fullscreen` | boolean | `false` | Starts in fullscreen. |
| `window.maxPixelDensity` | number | `0` | The most framebuffer pixels in each window point, or `0` for every pixel of the display, as [`window.setMaxPixelDensity`](../lua-api/window.md#windowsetmaxpixeldensitycap) describes. |
| `window.resizable` | boolean | `true` | Whether the player may resize a desktop window. [`window.setResizable`](../lua-api/window.md#desktop-windows) changes it later. The canvas of a web page follows the size of the page whatever it says, since the page decides that size. |
| `window.vsync` | boolean | `true` | Waits for the refresh of the display before it shows a frame. |
| `window.frameRate` | integer | `0` | The frames per second the app aims for, from 1 to 1000, or `0` for every refresh of the display. [`window.setFrameRate`](../lua-api/window.md#windowsetframerateframespersecond) changes it later. |
| `window.sampleCount` | integer | `1` | The sample count of multisampling. |
| `window.decorated` | boolean | `true` | Whether the desktop window has a title bar and a border. |
| `window.transparent` | boolean | `false` | Opens the window able to be transparent, and transparent, so the desktop shows through its transparent pixels. It also makes `clearColor` default to `"#00000000"`. Only a window that opened this way can turn transparent again after `window.setTransparent(false)`. |
| `window.alwaysOnTop` | boolean | `false` | Keeps the desktop window above normal windows. |
| `window.showInTaskbar` | boolean | `true` | Whether the window has a taskbar button. On macOS it also decides whether the app has a Dock icon and a menu bar. |
| `window.focusable` | boolean | `true` | Whether a click on the window activates the app and gives it the keyboard. |
| `window.mousePassthrough` | boolean | `false` | Lets every click pass through the window to what is behind it, until the app names regions that keep the mouse. |
| `window.keepScreenOn` | boolean | `false` | Keeps the screen on and its saver away while the app shows, as [`window.setKeepScreenOn`](../lua-api/window.md#windowsetkeepscreenonenabled) describes. |
| `window.lowPowerGpu` | boolean | `false` | Draws on the GPU that uses the least power, such as the integrated GPU of a laptop that has two, rather than on the most powerful one, which saves battery for an app that draws little. On Windows the player and the apps ask the drivers of NVIDIA and AMD for the discrete GPU unless this is `true`, and Direct3D, Vulkan and WebGPU pick their GPU by it, as [the RHI guide](../graphics/rhi.md#machines-with-two-gpus) explains. |
| `window.position` | string or object | Centered, absent | Where the desktop window opens: `"center"`, a point such as `{"x": 40, "y": 60}` in desktop points, or an anchored placement such as `{"anchor": "bottom", "area": "work", "display": "primary", "offset": [0, -8], "fill": "width"}`, where the display is `"primary"`, a number from 1 or the id of a display. The [desktop guide](../platform/desktop.md#placing-the-window) describes it. `haylen.config` holds it as `{x, y}` or `{anchor, area, display, offset, fill}`. |
| `window.statusBar` | string | `"hidden"` | How the status bar of phones and tablets shows when the app launches: `"hidden"`, or `"auto"`, `"light"` or `"dark"` content, as [`window.setStatusBar`](../lua-api/window.md#windowsetstatusbarvalue) describes. |
| `window.graphicsApis` | array of strings | `[]` | The graphics APIs that the window tries first, in order, such as `["vulkan", "openGl"]`. The names are `"metal"`, `"direct3D12"`, `"direct3D11"`, `"vulkan"`, `"openGl"`, `"openGlEs"`, `"webGpu"`, `"webGl2"` and `"port"`. After them the window tries the other APIs of the build in the order of the platform, and it leaves out the APIs that the build lacks, so one list serves every platform. The option `--graphics-api` of the player replaces it. Scripts read the API that draws in [`haylen.graphicsApi`](../lua-api/haylen.md#haylengraphicsapi). |
| `window.systemGestures` | string | `"system"` | How the system gestures at the edges of phones and tablets answer a swipe: `"system"`, `"deferred"` or `"autoHideIndicator"`, as [`window.setSystemGestures`](../lua-api/window.md#windowsetsystemgesturesvalue) describes. |
| `window.redraw` | string | `"always"` | When the app updates and draws: `"always"` in every frame, as a game does, or `"onChange"` only in the frames that something changed in, as an app of forms and lists does, which [`window.setRedraw`](../lua-api/window.md#windowsetredrawvalue) describes. |
| `window.externalDisplays` | boolean | `false` | Whether the app shows [outputs](../lua-api/display.md#displayopenid-options) on the external displays of an iPhone or an iPad of iOS 27 and later, whose scene UIKit connects only for an app that registers the scene accessory of external displays. Without it such a display mirrors the app. Every other platform connects external displays on its own, as the [displays guide](../platform/displays.md#content-on-another-display) explains. |
| `window.statusBarColor` | string | Absent | The color, such as `"#FF1E2A44"`, that the engine paints behind the status bar while it shows, as [`window.setStatusBarColor`](../lua-api/window.md#windowsetstatusbarcolorcolor) describes. |

The desktop options of `window` change later through [`haylen.window`](../lua-api/window.md#desktop-windows), and the [desktop guide](../platform/desktop.md) describes them.

## Design, orientation and timing

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `design.width`, `design.height` | number | `1920`, `1080` | The design resolution: the space of units that the app draws in and receives input in. |
| `design.scaling` | string | `"expand"` | How the design resolution fits the screen: `"fit"`, `"fill"`, `"stretch"`, `"expand"`, `"pixelPerfect"`, `"fitWidth"` or `"none"`, as [`haylen.viewport`](../lua-api/viewport.md) describes. |
| `orientation` | string | `"landscape"` | The orientation of a mobile app: `"landscape"`, `"portrait"` or `"any"`. The Android backend writes it into the manifest of the project that it generates. On iPhones and iPads the `Info.plist` lists the orientations of `orientation`, which the app launches in, as the [distribution guide](../tools/distribution.md) describes. Desktop windows and the web ignore it. |
| `fixedRate` | number | `60` | Fixed updates per second. [`haylen.fixedStep()`](../lua-api/haylen.md#haylenfixedstep) returns the length of one step in seconds. |
| `maxFrameTime` | number | `0.25` | The longest frame time in seconds that the clock accepts, so a stall never causes a burst of fixed steps. |
| `clearColor` | string | `"#FF000000"`, or `"#00000000"` for a transparent window | The background color. |

## Splash screen

The splash screen is the launch screen that iOS, tvOS, Mac Catalyst and Android show while the app starts, and the loading page of the web. The backend of each target builds it into the project that it generates, as the [distribution guide](../tools/distribution.md) describes.

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `splash.logo` | string | The Haylen symbol, `''` in `haylen.config` | The image of the launch screen, relative to `content/`, such as `"ui/splash.png"`. |
| `splash.background` | string | The value of `clearColor` | The background color of the launch screen. |
| `splash.darkBackground` | string | The value of `splash.background`, absent in `haylen.config` | The background color of the launch screen while the system uses dark colors. |
| `splash.duration` | number | `0` | The least seconds that the launch screen stays from the launch. It always lasts until the first complete frame of the app. |
| `splash.fadeOut` | number | `0.25` | The seconds that the launch screen takes to fade out into the app. |

## Lifecycle

These options start the values of [`haylen.lifecycle()`](../lua-api/haylen.md#haylenlifecycle), and the [lifecycle guide](../core/lifecycle.md#app-states) explains them.

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `lifecycle.pauseOnBackground` | boolean | `true` | Halts the app while it is in the background. |
| `lifecycle.pauseOnFocusLoss` | boolean | `false` | Halts the app while its window has no focus. |
| `lifecycle.muteOnFocusLoss` | boolean | `false` | Mutes the output while the app is not active. |

## Audio

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `audio.iosSession` | string | `"ambient"` | The category of the audio session on iOS and tvOS: `"ambient"`, `"soloAmbient"` or `"playback"`, as the [audio guide](../audio/audio.md#sessions-and-interruptions) describes. Only `"playback"` plays in the background, which the app declares to the system, as the [audio guide](../audio/audio.md#playing-in-the-background) describes. |
| `audio.mixWithOthers` | boolean | `false` | Lets the `"playback"` session play along with other apps. The other sessions refuse `true`. |
| `audio.latency` | string | `"balanced"` | How the output trades latency for power: `"low"`, `"balanced"` or `"powerSaving"`, as [`audio.outputLatency`](../lua-api/audio.md#audiooutputlatency) describes. |
| `audio.limiter` | boolean | `true` | Puts a [limiter](../lua-api/audio.md#limiter) on the `master` bus, so the mix never clips. |

## Input and UI

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `input.mouseAsTouch` | boolean | `false` | Makes the mouse act as a finger from the start, as [`input.setMouseAsTouch`](../lua-api/input.md#inputsetmouseastouchenabled) describes. |
| `input.touchAsMouse` | boolean | `false` | Makes a finger act as the mouse from the start, as [`input.setTouchAsMouse`](../lua-api/input.md#inputsettouchasmouseenabled) describes. |
| `input.scrollSpeed` | number | `1` | Multiplies every scroll of the wheels and the trackpads from the start, as [`input.setScrollSpeed`](../lua-api/input.md#inputsetscrollspeedspeed) describes. |
| `ui.scaleMode` | string | `"design"` | How large the interface draws: `"design"` or `"physical"`, as [`ui.setScaleMode`](../lua-api/ui.md#uisetscalemodemode) describes. |
| `ui.scale` | number | `1` | A factor from `0.25` to `4` that multiplies the size of the interface, as [`ui.setScale`](../lua-api/ui.md#uisetscalefactor) describes. |

## Debug

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `debug.stats` | string | `"off"` | The debug statistics that the app starts with: `"off"`, `"compact"` for a line of each compact item in a corner, or `"full"` for the debug overlay, as [`haylen.debug`](../lua-api/debug.md) describes. |
| `debug.statsItems` | list of strings | `["frameRate", "drawCalls", "geometry"]` | The [items](../lua-api/debug.md#compact-items) that the compact statistics show: `"frameRate"`, `"drawCalls"`, `"geometry"`, `"memory"`, `"counters"`, `"physics"`, `"particles"` and `"audio"`. |
| `debug.drawings` | list of strings | `[]` | The [debug drawings](../lua-api/debug.md#drawings) that the app starts with, such as `["physics", "bounds"]`. |
| `debug.objectEvents` | boolean | `false` | Publishes `objectCreated` and `objectDestroyed` for every counted object, as [`haylen.debug`](../lua-api/debug.md#object-counts-and-events) describes. |
| `debug.console` | boolean | `false` | Turns the [developer console](../lua-api/debug.md#console) on outside development runs, so it runs the lines of the web page and the [inspector](../lua-api/debug.md#inspector) opens. Development runs always have it. |
| `debug.safeArea` | string, number or array | Absent | A safe area to simulate instead of the one of the device: a device name such as `"iphoneDynamicIsland"`, or insets in window points, one number for every side, two for the top and bottom and the sides, or four for the top, right, bottom and left, as [`viewport.setSafeAreaSimulation`](../lua-api/viewport.md#viewportsetsafeareasimulationvalue) describes. |
| `debug.fold` | string or object | Absent | A fold to simulate instead of the one of the device: a preset such as `"book"` or `"tabletop"`, or an object with `axis`, `state` and `hinge`, as [`window.setFoldSimulation`](../lua-api/window.md#windowsetfoldsimulationvalue) describes. |
| `debug.showSafeArea` | boolean | `false` | Shows the debug view of the safe area from the start, as [`ui.setSafeAreaVisible`](../lua-api/ui.md#uisetsafeareavisiblevisible) describes. |

## Lua

The Lua layer of the engine reads these fields, which load before any Lua runs. In `haylen.config` the section `lua` holds every one of them with its default.

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `lua.autoloads` | list of strings | `[]` | The modules that load as [autoloads](../lua-api/haylen.md#haylenautoloadname-module) before `source/main.lua`, such as `"state.player-data"`. |
| `lua.reload` | string | `"module"` | How a changed Lua module applies while the app runs in development. `"module"` reloads it in place with the state of the app, and `"restart"` restarts the app, as [hot reload](../core/lua.md#hot-reload) describes. |
| `lua.executionBudget` | number | `5` | The seconds that the code of an app in development may run each time the platform enters the engine, such as for one frame, before the engine stops the app, from 0.1 to 600, as [the execution budget](../core/lua.md#the-execution-budget) describes. |
| `lua.memoryBudget` | number | `1024` | The megabytes that the Lua state of an app in development may hold before the engine stops the app, from 16 to 4096, as [the execution budget](../core/lua.md#the-execution-budget) describes. |

## Localization

These fields load the texts of the app before any Lua runs, as the [localization guide](../core/localization.md#configuring-the-languages) explains. Tags may be written with `-` or `_` in any case, and `haylen.config` holds them in their canonical form, such as `"pt-BR"`.

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `localization.folder` | string | Absent | The folder under `content/` whose `.json` files load as languages, each named after its language tag, such as `"locale"` for `content/locale/en.json` and `content/locale/pt-BR.json`, as [`localization.loadFolder`](../lua-api/localization.md#localizationloadfolderfolder) loads them. |
| `localization.fallback` | string | The first language of the folder | The language that gives the texts that the current language lacks, and the current language when no language of the app is close to the one wanted. Its file must be in the folder. |
| `localization.language` | string | Absent | The language that the app starts in, as [`localization.setLanguage`](../lua-api/localization.md#localizationsetlanguagetag) picks it. Without it the app follows the languages of the system. |

## Content, native libraries and plugins

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `content.updates` | object | Absent | The server of the protected content of a release, with `baseUrl`, `channel`, `"stable"` by default, and `enabled`, `true` by default. Without it the app has no content server. The [delivery settings](../content/content.md#delivery) describe it. |
| `content.delivery` | list of objects | `[]` | The rules that let assets arrive after the start of the app. Each rule has `match`, a pattern of paths relative to `content/`, or `group`, the name of a preload group, and either `mode`, which is `"required"`, `"prefetch"` or `"onDemand"`, or `pack`, the name of a pack that a store delivers on its own. The first rule that matches an asset decides. Without rules every asset is required. In `haylen.config` a rule that names a group becomes one rule with `match` for each entry of the group. See [`haylen.content`](../lua-api/content.md) and the [delivery settings](../content/content.md#delivery). |
| `content.groups` | string | Absent | The group manifest that defines the groups the rules name, relative to `content/`, such as `"groups.json"`. |
| `category` | string | Absent | The category that the stores and the app menus show the app in: `"game"`, `"entertainment"`, `"education"`, `"productivity"`, `"utility"`, `"graphics"`, `"music"`, `"video"`, `"social"` or `"developer"`. It is optional, and the tooling maps it to the category of each platform and leaves it out of every generated project of an app without one, as the [distribution guide](../tools/distribution.md#category) lists. The engine checks only its value. |
| `kind` | string | `"lua"` | The kind of the app: `"lua"`, whose code is Lua that the player of the engine runs, or `"cpp"`, whose C++ code of the `cpp` section compiles against the engine SDK. The engine checks only its value, and the tooling builds the app from it. |
| `cpp` | object | Absent | The C++ code of a C++ app, which the backend of every target compiles: `sources`, the folders and files of its sources relative to the app folder, `includeDirectories`, `definitions` and `lua`, `false` by default, which links the Lua layer for an app whose application runs Lua, as the [distribution guide](../tools/distribution.md#c-apps-and-lua) describes. The engine checks only that it is an object. |
| `platforms` | object | `{}` | The settings and files of the generated projects of the targets, by the name of a section that a backend reads, such as `apple`, `ios` or `android`, as the [distribution guide](../tools/distribution.md) lists. The engine checks only that each section is an object. |
| `native` | object | `{}` | The native libraries that the app ships, by name, as the [native code guide](../platform/native.md#packaging-libraries-with-an-app) describes. The engine keeps the section as `haylen.json` wrote it. |
| `physics3d` | object | `{}` | The settings that every 3D physics world of the app starts from, with the keys of the options of [`physics3d.newWorld`](../lua-api/physics3d.md#physics3dnewworldoptions) but `owner`, such as `{"gravity": [0, -1.62, 0], "velocitySteps": 12}`. The engine reads it before any Lua runs and refuses an unknown key or a value out of its range with a sentence that names the key, such as `physics3d.velocitySteps`. |
| `plugins` | object | `{}` | The [plugins](../platform/plugins.md) that the app uses, by id in `dash-case`, each with its entry: the values of its parameters under `parameters`, and the `path` or the `git` repository at `ref` that the tooling takes it from, such as `{"ads": {"parameters": {"testMode": true}}, "camera": {"path": "../plugins/camera"}}`. Every id needs `plugins/<id>/plugin.json` in the package, which the tooling places there wherever the plugin comes from, and `haylen.lock` pins a plugin of a git repository to a commit. The field [`platform.plugin(id).config`](../lua-api/platform.md#appplugin) adds the defaults of their parameters. |

## Platforms

The section `platforms` holds the settings and files of the generated project of each target, in sections that the backends read. The [distribution guide](../tools/distribution.md#what-an-app-customizes) explains how the values of the app, its plugins, the SDK and the backend merge.

| Section | Platforms | Keys |
| --- | --- | --- |
| `apple` | Every Apple platform. | `sources`: folders and files of Swift, Objective-C, Objective-C++, C and C++ that the app target compiles, such as the bridge handlers. `team`: the development team that signs the apps of devices. `buildSettings`: Xcode build settings of the app target, as [the Apple project](../tools/distribution.md#the-apple-project) describes. `privacy`: keys of the privacy manifest. |
| `ios` | iPhones and iPads. | `devices`: the devices that the app runs on, as [iOS devices](../tools/distribution.md#ios-devices) describe. `infoPlist`: keys of the `Info.plist`. `entitlements`: the entitlements that the app is signed with. |
| `catalyst`, `tvos`, `visionos`, `macos` | That platform. | `infoPlist`: keys of the `Info.plist`. `entitlements`: the entitlements that the app is signed with. |
| `android` | Android. | `sources`: the Kotlin and Java folders of the app module. `applicationClass`: the class of the application, where the bridge handlers register. `manifest`: `permissions` to add, `removedPermissions` that the merged manifest leaves out, `applicationAttributes` of the `<application>` element, `activityAttributes` of the activity and the `features` of the device. `targetSdkVersion`, `minify` and `gradleProperties`, as [the Android project](../tools/distribution.md#the-android-project) describes. |
| `web` | The web. | `scripts`: the page scripts of the app, which load after the loader. |
| `windows` | Windows. | `executableName`, `console` and `executionLevel`, as [Windows](../tools/distribution.md#windows) describes. |
| `linux` | Linux. | `executableName`, as [Linux](../tools/distribution.md#linux) describes. |

Every section also takes the keys that the core reads:

- `minimumSystemVersion`: the oldest system version of the platform that the app supports, in the section named after the platform, such as `platforms.ios` or `platforms.android`, where Android takes an API level such as `"29"`. A section that serves several platforms, such as `apple`, refuses it with a sentence that names the sections of its platforms.
- `architectures`: the architectures that a build without `--architecture` builds, in the section named after the platform, such as `["arm64-v8a"]` in `platforms.android`. Without it a build takes the default architectures of its target. An `--architecture` that names an architecture outside the list stops with a sentence that names both.
- `afterPrepare`: a Python script of the app that changes the generated project after every preparation, as [scripts after the preparation](../tools/distribution.md#scripts-after-the-preparation) describe.
- `files`: paths of the generated project mapped to files or folders of the app, which take the place of the template file or folder at that path. The Apple backend accepts only paths that its template holds, such as `Info.plist`, `LaunchScreen.storyboard` or `Assets.xcassets`, while the other backends also add new files, such as a drawable of Android.

## Plugins

The key `plugins` maps the id of every plugin that the app uses to its entry, an object with four optional keys:

| Key | Value |
| --- | --- |
| `path` | The folder of the plugin, absolute or relative to the app folder. Next to `git`, it is the folder of the plugin inside the repository, relative to its root, which never leads outside the repository. |
| `git` | The address of the git repository of the plugin, which holds `plugin.json` at its root, or in the folder that `path` names. A local repository is a path, absolute or relative to the app folder. |
| `ref` | The branch, tag or commit of `git`, `HEAD` of the repository when it is left out. |
| `parameters` | The values of the parameters of the plugin. |

## Errors of haylen.json

The engine checks `haylen.json` before any Lua runs. A package whose `haylen.json` fails to load shows `The app could not be loaded.` and the reason on the screen. In development the app still runs, so saving a fixed `haylen.json` restarts it.

| Message | Cause |
| --- | --- |
| `Unknown key "<key>" in "haylen.json".` | The file has a field that neither the engine nor the Lua layer reads. A field of a section names its section, such as `Unknown key "widht" in the "window" section of "haylen.json".` |
| `The value of "<key>" in "haylen.json" has the wrong type.` | A value is not of the type that the tables above give. The key names its section, such as `window.width`. |
| `The value of "<key>" in "haylen.json" must be a whole number from <minimum> to <maximum>.` | A whole number, such as `window.width` or `window.frameRate`, has a fraction, such as `800.5`, or lies outside the numbers it can hold, such as `1e20`. |
| `The value of "<key>" in "haylen.json" must be a number from <minimum> to <maximum>.` | A number with a fraction, such as `splash.duration`, lies outside the numbers it can hold. |
| `The value of "<key>" in "haylen.json" must be positive.` | One of `window.width`, `window.height`, `window.sampleCount`, `design.width`, `design.height`, `fixedRate` and `maxFrameTime` is `0` or less. |
| `The value of "<key>" in "haylen.json" must be a number of at least 0.` | The `splash.duration` or `splash.fadeOut` is negative or not finite. |
| `The "window.maxPixelDensity" value in "haylen.json" is out of range. It is 0 for every pixel of the display, or a positive number of framebuffer pixels in each point.` | The cap of the pixel density is negative or not finite. |
| `The "<key>" value "<value>" in "haylen.json" is not a color such as "#RRGGBB" or "#AARRGGBB".` | The `clearColor`, `window.statusBarColor`, `splash.background` or `splash.darkBackground` is not a color. |
| `The "window.statusBar" value "<value>" in "haylen.json" is unknown. It is "hidden", "auto", "light" or "dark".` | The status bar has another value. |
| `The "window.graphicsApis" value "<value>" in "haylen.json" is not a graphics API. It is "metal", "direct3D12", "direct3D11", "vulkan", "openGl", "openGlEs", "webGpu", "webGl2" or "port".` | The list names something else, or `"none"`. |
| `The "window.graphicsApis" of "haylen.json" names "<value>" twice.` | The list repeats an API. |
| `The "window.graphicsApis" of "haylen.json" is not an array of graphics API names, such as ["vulkan", "openGl"].` | The value is not an array. |
| `The "window.position" in "haylen.json" is invalid. <reason>` | The placement of the window is wrong, for the reason that follows. |
| `The "design.scaling" value "<value>" in "haylen.json" is unknown. It is "fit", "fill", "stretch", "expand", "pixelPerfect", "fitWidth" or "none".` | The scaling policy has another value. |
| `The orientation "<value>" in "haylen.json" is unknown. It is "landscape", "portrait" or "any".` | The orientation has another value. |
| `The "audio.iosSession" value "<value>" in "haylen.json" is unknown. It is "ambient", "soloAmbient" or "playback".` | The audio session has another value. |
| `The "audio.mixWithOthers" option in "haylen.json" needs the "iosSession" value "playback".` | The option `mixWithOthers` is `true` with another session. |
| `The "audio.latency" value "<value>" in "haylen.json" is unknown. It is "low", "balanced" or "powerSaving".` | The latency has another value. |
| `The "input.scrollSpeed" value in "haylen.json" must be a positive number.` | The scroll speed is not a positive finite number. |
| `The "ui.scaleMode" value "<value>" in "haylen.json" is unknown. It is "design" or "physical".` | The scale mode has another value. |
| `The "ui.scale" value in "haylen.json" must be from 0.25 to 4.` | The scale of the interface is out of its range. |
| `The "debug.stats" value "<value>" in "haylen.json" is unknown. It is "off", "compact" or "full".` | The debug statistics have another value. |
| `The "debug.statsItems" item "<item>" in "haylen.json" is unknown. It is "frameRate", "drawCalls", "geometry", "memory", "counters", "physics", "particles" or "audio".` | An item of the compact statistics is unknown. |
| `The "debug.drawings" list in "haylen.json" has an empty drawing name.` | A debug drawing has an empty name. |
| `The "debug.safeArea" in "haylen.json" is invalid. <reason>` | The simulated safe area is wrong, for the reason that follows. |
| `The "debug.fold" in "haylen.json" is invalid. <reason>` | The simulated fold is wrong, for the reason that follows. |
| `The "lua.reload" value "<value>" in "haylen.json" is unknown. It is "module" or "restart".` | The reload mode has another value. |
| `The "localization.<key>" value "<value>" in "haylen.json" is no language tag. Write a language such as "en", "pt-BR" or "pt_BR".` | The `localization.fallback` or `localization.language` is not a language tag. |
| `The "localization.fallback" of "haylen.json" needs a "localization.folder" that holds its language file.` | The fallback is set without the folder. |
| `The folder "<folder>" of "localization.folder" in "haylen.json" holds no language file. Add a file such as "<folder>/en.json".` | The folder has no `.json` file. The app shows it on the error screen once it starts. |
| `The "localization.fallback" of "haylen.json" names "<language>", whose file the folder "<folder>" lacks.` | The folder has no file of the fallback language. The app shows it on the error screen once it starts. |
| `The value of "lua.autoloads" in "haylen.json" must be a list of module names.` | The autoloads are not a list of strings. |
| `The "lua.autoloads" list in "haylen.json" has an empty module name.` | An autoload has an empty name. |
| `The "lua.executionBudget" value in "haylen.json" must be a number of seconds from 0.1 to 600.` | The execution budget is not a number of seconds in its range. |
| `The "lua.memoryBudget" value in "haylen.json" must be a number of megabytes from 16 to 4096.` | The memory budget is not a number of megabytes in its range. |
| `The "native" section of "haylen.json" must be an object of libraries.` | The section `native` is not an object. |
| `The "plugins" section of "haylen.json" must be an object of plugins by id.` | The section `plugins` is not an object. |
| `The plugin id "<id>" in "haylen.json" is not in "dash-case", such as "cloud-analytics".` | A plugin id has other characters. |
| `The plugin "<id>" in "haylen.json" must be an object with the optional keys "path", "git", "ref" and "parameters".` | The entry of a plugin is not an object. |
| `Unknown key "<key>" in the plugin "<id>" of "haylen.json".` | The entry of a plugin has a key other than `path`, `git`, `ref` and `parameters`. |
| `The "<key>" of the plugin "<id>" in "haylen.json" must be a text.` | A `path`, `git` or `ref` is empty or not a text. |
| `The "ref" of the plugin "<id>" in "haylen.json" picks a branch, tag or commit of a git repository, so it needs "git".` | An entry has a `ref` without `git`. |
| `The "parameters" of the plugin "<id>" in "haylen.json" must be an object of parameter values.` | The values of a plugin are not an object. |
| `The plugin "<id>" in "haylen.json" has no "plugins/<id>/plugin.json" in the package.` | The package lacks the manifest of a plugin that `haylen.json` lists. |
| `The plugin "<id>" in "haylen.json" comes from a "path" or a "git" repository, so the package has no "plugins/<id>/plugin.json". Run the app with "haylen app run", which places every plugin in the package wherever it comes from.` | A player runs an app folder directly, which lacks a plugin that `haylen.json` references elsewhere. |

The [delivery settings](../content/content.md#delivery) list the messages of the `content` section.
