# 🧱 Native Base

A local plugin of the [test project](../../README.md) that the plugin [`native-demo`](../native-demo/README.md) depends on, for the test `PLG-024`, Plugin dependencies. Its native part offers the helper `describe`, which the native part of `native-demo` calls directly on Apple platforms, Android and the web, so the test shows how the native code of one plugin reaches the native code of a plugin it depends on. The project lists it in its `haylen.json` with no parameters, since `native-demo` names it in the `dependencies` of its `plugin.json`, and the backend of each target builds it into the generated project as the [plugin guide](../../../../docs/platform/plugins.md) describes.

## Lua API

```lua
local async = require('async')
local base = require('native-base')

async.spawn(function()
    local answer = base.describe('Hello'):await()
    print(answer.text, answer.language)
end)
```

| Function | Meaning |
| --- | --- |
| `base.available()` | Whether the native part of the plugin runs on this platform. |
| `base.describe(text)` | Calls `native-base.describe`, which answers `{text, language}`, where `text` is the text followed by the words of the native part, such as `Hello, as the native part of native-base wrote it on Android.`. |

## The helper of the native part

Each native part keeps the helper in a type of its own, which the native part of a plugin that depends on `native-base` calls the way it calls its own code:

| Platform | Helper | How a plugin that depends on it reaches it |
| --- | --- | --- |
| iOS, Mac Catalyst, tvOS, visionOS, macOS | `NativeBase.describe(_:)` of `apple/NativeBase.swift` | The Apple sources of every plugin compile into the one app target, so Swift calls `NativeBase.describe("Hello")`. |
| Android | `NativeBase.describe(text)` of the Kotlin package `com.haylen.plugins.nativebase` | The Android backend adds `api(project(":native-base"))` to the build script of the module of each plugin that depends on it, so Kotlin imports `com.haylen.plugins.nativebase.NativeBase`. |
| Web | The named export `describe` of `web/native-base.js` | The page maps `haylen-plugin:native-base` to the module of the plugin, so a module imports `import { describe } from "haylen-plugin:native-base";` and shares its one instance. |
| Desktop player, Windows, Linux | None | The plugin works with its Lua API alone there, so its calls fail with the code `noHandler`. |

The plugin needs no setup on any platform.
