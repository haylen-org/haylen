# 🧾 `plugin.json`

This page lists every key of `plugin.json`, the manifest of a native plugin, with the keys of its parameters, its placeholders and every type of contribution that the backends read. The [plugins guide](../platform/plugins.md#pluginjson) explains what a plugin is and shows a complete manifest, and the schema `tools/haylen/schemas/plugin.schema.json` checks every manifest.

## Keys

| Key | Required | Value |
| --- | --- | --- |
| `interface` | Yes | The version of the plugin interface of Haylen that the manifest follows, `1`, apart from the version of the plugin. |
| `id` | Yes | The id in dash-case, which is the key of the plugin in the `plugins` of `haylen.json` and the name of its folder in the package. |
| `name` | Yes | The name that people read. |
| `version` | Yes | The release version of the plugin, one to three numbers separated by dots, such as `1.2.0`. |
| `description` | Yes | What the plugin does, in one sentence. |
| `implementations` | Yes | How the plugin works on each platform it supports, by the names `ios`, `catalyst`, `tvos`, `visionos`, `macos`, `android`, `web`, `windows` and `linux`: `native` when its native part runs there, and `lua` when its Lua API works alone. A platform that is missing does not support the plugin, and the build of the app for it fails. |
| `dependencies` | No | The plugins that the app must list as well, by id, with the oldest version of the same major version that this plugin works with, such as `{"cloud": "1.2"}` for `cloud-analytics`. |
| `permissions` | No | The names of the [permissions](../platform/plugins.md#permissions) that the plugin owns, in camelCase, such as `camera`, and no two plugins of an app declare the same name. |
| `parameters` | No | The values that an app gives the plugin in `haylen.json`, by name in camelCase. |
| `contributions` | No | What the plugin brings to the projects of its platforms, as a list of [contributions](#contributions). |

## Parameters

Each parameter is an object with these keys:

| Key | Required | Value |
| --- | --- | --- |
| `type` | Yes | `string`, `number`, `integer`, `boolean`, `array`, `object` or `file`. A `file` is a path relative to the app folder. |
| `description` | Yes | What the value means. The tooling repeats it when a required value is missing. |
| `platforms` | No | The platforms of the plugin where the parameter applies and, when it is required, where a value must exist. It is every platform of the plugin by default. |
| `required` | No | Whether the app must give a value for the platforms of the parameter. It is `false` by default. |
| `default` | No | The value of an optional parameter that the app leaves out, of the type of the parameter. |

A value that differs per platform uses one parameter per platform, such as `iosAppId` and `androidAppId`. So nothing resolves platform values while the app runs.

## Placeholders

The placeholder `${name}` in a string of a contribution becomes the value of the parameter `name`. Each placeholder must name a declared parameter.

- A string that is only a placeholder takes the value with its type. So `"${testMode}"` becomes a boolean in `Info.plist`.
- A placeholder inside longer text inserts the value as text.
- A key or an item whose placeholders name a parameter without a value is left out.
- A contribution takes only the parameters that apply to its platforms. So a parameter of `macos` alone, such as one that turns on the entitlements of the hardened runtime, leaves its keys out of the other platforms.
- Shell expansions such as `${BUILD_DIR%/Build/*}` are no placeholders, and they stay as they are.
- Build settings in paths and scripts use the form `$(NAME)`.

## Contributions

Every contribution is an object with its `type`, the `platforms` it applies to and the fields of its type. The backend of each platform declares the types it reads, with the JSON Schema of their fields, the identity that tells two contributions of one type apart and the way contributions of several owners merge. The tooling checks every contribution against its type when it resolves the plugins, so a field that the type refuses fails before a project exists, with the plugin and the key named. A type that no installed backend declares for a platform of the contribution fails the same way.

Each type merges in one of four modes. The owners of a contribution are the plugins and, for the keys that the sections of `haylen.json` turn into contributions, the app. No owner wins by the order in which the files were read: every conflict stops the resolution with both owners and both files named.

| Mode | Rule |
| --- | --- |
| `accumulate` | Every contribution stays, such as every `appleSources` folder and every `gradleModule`. |
| `unique` | One contribution stays for each identity, such as one `swiftPackage` for each `url`. A second contribution with the same identity must be equal in every field, or the resolution fails with the fields that differ, except the lists that the type joins, such as the `products` of a `swiftPackage`, which gather the items of every owner. |
| `dictionary` | The `values` objects merge key by key. Objects inside them merge the same way, arrays gain the items they lack, and two different values of one key fail. So two ad plugins share `SKAdNetworkItems` of `Info.plist`. |
| `exclusive` | One owner of an app alone may give the type, such as the delivery of the packs of the content. |

The app gives contributions of its own through the sections of `platforms` in `haylen.json`: `infoPlist` and `entitlements` of the sections `ios`, `catalyst`, `tvos`, `visionos` and `macos`, and `privacy` of the section `apple`. They merge with those of the plugins under the owner `app`, by the rules of their type, so the app never replaces a key that a plugin needs. The command `haylen project inspect` shows every merged contribution with its owners. The [distribution guide](../tools/distribution.md) describes the sections of `platforms`.

### Native library contributions

Every backend that loads native libraries reads these types, and the core of the tooling reads the oldest system version itself.

| Type | Fields | Merge | Value |
| --- | --- | --- | --- |
| `nativeLibrary` | `files` or `cmake`, `link`, `symbols` | `accumulate` | A native library, with the rules of the entries of the `native` section of `haylen.json` that the [native code guide](../platform/native.md#packaging-libraries-with-an-app) describes, and its paths relative to the plugin folder. It takes the id of the plugin as its name, and joins the native libraries of the app, as [native libraries](../platform/plugins.md#native-libraries) describes. |
| `minimumSystemVersion` | `version` | `accumulate` | The oldest version of the system of its platforms that the plugin runs on, which raises the minimum of the app only where the app allows it. |

The init function of the library takes the id with underscores for its dashes, since a C function is named with an identifier. So `native.load('my-plugin', {init = 'my_plugin_haylen_init'})` loads the library of `my-plugin`.

```json
{"type": "nativeLibrary", "platforms": ["macos", "windows", "linux"], "cmake": "native"}
```

### Apple contributions

The Apple platforms are `ios`, `catalyst`, `tvos`, `visionos` and `macos`, and every Apple contribution names those it applies to.

| Type | Fields | Merge | Value |
| --- | --- | --- | --- |
| `appleClass` | `name` | `accumulate` | The Objective-C runtime name of the plugin class, which Swift classes declare with `@objc(Name)`. The runtime reads it from the package and creates the class on the platforms of the contribution where the plugin has a native part, as [the Apple plugin class](../platform/plugins.md#the-apple-plugin-class) describes. |
| `appleSources` | `path` | `accumulate` | The folder whose Swift, Objective-C, C and C++ files compile into the app. |
| `swiftPackage` | `name`, `url`, `exactVersion`, `products` | `unique` by `url` | A Swift package with its repository `url` (https or ssh), one `exactVersion` and the `products` that the app links. Two plugins that name one package by its url must agree on its `name` and `exactVersion`, and the app links the `products` of both, so several plugins of one SDK share its package. Another version of one package stops the resolution. |
| `framework` | `name` | `unique` by `name` | A system framework or library, such as `StoreKit.framework` or `libz.tbd`. |
| `binaryFramework` | `path`, `embed` | `unique` by `path` | A framework that the plugin ships in its folder, such as the `.xcframework` of an SDK that its vendor hands out as a file, with `embed` `true` for a dynamic framework, which the app embeds and signs, and `false` for a static one, which the app only links. Two plugins that ship a framework of the same file name stop the build. |
| `infoPlist` | `values` | `dictionary` | Keys merged into the `Info.plist` of the app. A text may name the build settings of the identity of the app, `$(HAYLEN_BUNDLE_IDENTIFIER)`, `$(HAYLEN_PRODUCT_NAME)`, `$(HAYLEN_DISPLAY_NAME)`, `$(MARKETING_VERSION)` and `$(CURRENT_PROJECT_VERSION)`, such as `"group.$(HAYLEN_BUNDLE_IDENTIFIER)"`. |
| `entitlements` | `values` | `dictionary` | Keys merged into the entitlements of the app, whose texts may name the same build settings. |
| `privacy` | `values` | `dictionary` | Declarations merged into the privacy manifest of the app: `NSPrivacyAccessedAPITypes`, each with its `NSPrivacyAccessedAPIType` and `NSPrivacyAccessedAPITypeReasons`, `NSPrivacyCollectedDataTypes`, `NSPrivacyTracking` and `NSPrivacyTrackingDomains`, as the [privacy manifest](../tools/distribution.md) describes. |
| `appleResource` | `path` | `accumulate` | A file copied to the root of the app bundle: a path inside the plugin, or a placeholder of a `file` parameter such as `"${servicesPlist}"`, which names a file of the app and is left out while the parameter has no value. |
| `buildScript` | `name`, `script`, `inputFiles`, `outputFiles`, `basedOnDependencyAnalysis` | `accumulate` | A post-build phase. A script that writes no file sets `basedOnDependencyAnalysis` to `false`, so it runs on every build without a warning of Xcode. A script reaches the checkout of a Swift package of the plugins at `"${BUILD_DIR%/Build/*}/SourcePackages/checkouts/<package>"`, where `<package>` is the last part of the URL of the package. |
| `appExtension` | `name`, `kind`, `point`, `sources`, `minimumVersion`, `frameworks`, `infoPlist`, `entitlements` | `unique` by `name` | An app extension that the app embeds, such as the downloader extension of Background Assets, on iOS, tvOS, visionOS and macOS: its name of letters and digits that starts with a capital letter, `extensionKit` or `appExtension` as its kind, the extension point that the system runs it for, the folder of its code, and optionally the oldest system version it runs on, its system frameworks, its `Info.plist` keys and its entitlements. Its bundle identifier is the one of the app followed by its name in lowercase. Two plugins that add an extension of the same name stop the build. |
| `assetPacks` | `prefetch` | `exclusive` | Declares that the plugin delivers the named [packs](../content/content.md#delivery) of the content from the App Store with Background Assets, such as `{"prefetch": "${prefetchPacks}"}`. The Apple release build then packs every named pack into an asset pack of Background Assets with `ba-package`, as [distribution](../tools/distribution.md) describes. The packs that `prefetch` lists download with the install and every update, and the others when the app asks for them. Only one plugin of an app may declare it. |

### Android contributions

| Type | Fields | Merge | Value |
| --- | --- | --- | --- |
| `gradleModule` | `path` | `accumulate` | The folder of the Android library module, with `build.gradle.kts` and `src/main/AndroidManifest.xml`. The module applies `com.android.library`. It depends on the engine with `implementation("dev.haylen:haylen-plugins:$haylenEngineVersion")`, and on the [other engine libraries](../platform/plugins.md#engine-libraries) it uses. The line `val haylenEngineVersion = extra["haylenEngineVersion"] as String` reads the engine version that the Android project gives every module. |
| `mavenRepository` | `url` | `unique` by `url` | The https address of a Maven repository that the module and its Gradle plugins come from, besides Google and Maven Central, such as `https://maven.example.com/releases`, so a plugin brings an SDK that its vendor publishes in a repository of its own. |
| `gradlePlugin` | `id`, `version` | `unique` by `id` | A Gradle plugin that the app module applies, such as `{"id": "com.example.services", "version": "1.0.0"}`. |
| `manifestPlaceholder` | `name`, `value` | `unique` by `name` | A manifest placeholder, whose text value the manifest of the module reads as `${name}`. |
| `projectFile` | `from`, `to` | `unique` by `to` | A copy into the Android project. The value `from` is a path inside the plugin or a placeholder of a `file` parameter, and `to` is a path inside the project, such as `app/services.json`. |
| `assetPacks` | `fastFollow` | `exclusive` | Declares that the plugin delivers the named [packs](../content/content.md#delivery) of the content from Google Play, such as `{"fastFollow": "${fastFollowPacks}"}`. The Android release build then writes an asset pack of Google Play for every named pack, as [distribution](../tools/distribution.md) describes. Play downloads the packs that `fastFollow` lists, by their names in `content.delivery`, right after the install, and the others when the app asks for them. Only one plugin of an app may declare it. The plugin adds a source of packs with `content.addPackSource`, which answers each pack with the folder where Play placed it. |

### Web contributions

| Type | Fields | Merge | Value |
| --- | --- | --- | --- |
| `webModule` | `path` | `accumulate` | The ES module in the `web` folder that the loader imports, such as `web/ads.js`. Its default export is `load(context)`, and it may import the other modules of the folder with relative paths. |

### Desktop contributions

Windows and Linux open an app from a link of a URL scheme that the app registers with the system. The contributions of the type `urlScheme` name those schemes, and the projects of Windows and Linux register them, as the [distribution guide](../tools/distribution.md) describes. Each link reaches the app as the event [`linkOpened`](../lua-api/events.md#app-and-plugins), so the Lua API of a plugin hears links on the desktops without a native part. A page is the link that opened it, so the web module of a plugin reads the links of its scheme from the address of the page, such as from its hash, and hands each one to the app with `context.openLink(url)`, which the app hears as `linkOpened` too.

| Type | Fields | Merge | Value |
| --- | --- | --- | --- |
| `urlScheme` | `scheme` | `unique` by `scheme` | The scheme, such as `camera-scanner`, or a string parameter that holds one, such as `${scheme}`. |

```json
{"type": "urlScheme", "platforms": ["windows", "linux"], "scheme": "${scheme}"}
```
