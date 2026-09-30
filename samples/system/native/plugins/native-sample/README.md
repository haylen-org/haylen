# Native Sample

The local plugin of the [native sample](../../README.md) with the platform handlers of its platform handlers test: suspending Kotlin and a Java handler that throws on Android, async Swift on Apple platforms and JavaScript on the web. They answer with typed errors, fail their calls instead of crashing when they throw, and hear when the app cancels a call or its timeout passes. The sample lists it in its `app.json` with no parameters, and `make.py` builds it into the project of each platform as the [plugin guide](../../../../../docs/plugins.md) describes.

## Lua API

```lua
local nativeSample = require('native-sample')

local answer = nativeSample.greet('Lua'):await()
print(answer.greeting, answer.language)

local connection = nativeSample.onCancelled(function(stopped) print(stopped.language) end)
local call = nativeSample.slow()
call:cancel()
```

| Function | Meaning |
| --- | --- |
| `nativeSample.greet(name)` | Calls `native-sample.greet`, which answers `{greeting, language}` a moment later. |
| `nativeSample.refuse()` | Calls `native-sample.refuse`, which fails with the code `refused` and the data `{reason = 'requested'}`. |
| `nativeSample.explode()` | Calls `native-sample.explode`, whose handler throws, which fails the call with the code `exception`. |
| `nativeSample.slow(options)` | Calls `native-sample.slow` with the options of `platform.call`, such as `timeout`. It never answers by itself, and its handler sends `native-sample.cancelled` when the call ends early. |
| `nativeSample.onCancelled(listener)` | Calls `listener` with `{language}` whenever a slow handler stopped, and returns the connection. |

## Platforms

| Platform | Native part |
| --- | --- |
| iOS, Mac Catalyst, tvOS, macOS | `apple/NativeSamplePlugin.swift`, whose handlers are async functions registered with `register` of the Swift helpers of the template. |
| Android | The module in `android/`, whose Kotlin `NativeSamplePlugin` registers suspending handlers with `registerSuspend` of the `haylen-coroutines` library and the Java `ExplodeHandler`. |
| Web | `web/native-sample.js`, whose handlers return promises and follow the `AbortSignal` of their call. |
| Desktop player, Windows, Linux | None, so calls fail with the code `noHandler`. |

The plugin needs no setup on any platform.
