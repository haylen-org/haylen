# Platform Sample

The local plugin of the [platform sample](../../README.md), which holds the native code of the app: Objective-C on Apple platforms, Java on Android and JavaScript on the web answer its methods and send its events through the [platform bridge](../../../../../docs/platform_bridge.md). The sample lists it in its `app.json` with no parameters, and `make.py` builds it into the project of each platform as the [plugin guide](../../../../../docs/plugins.md) describes.

## Lua API

```lua
local platformSample = require('platform-sample')

local answer, err = platformSample.echo('Hello'):await()
print(answer and answer.language or err.message)

local connection = platformSample.onTick(function(tick) print(tick.count, tick.total, tick.source) end)
platformSample.ticker(5, 500)
```

| Function | Meaning |
| --- | --- |
| `platformSample.echo(text)` | Calls `platform-sample.echo`, which answers `{echo, characters, language, system}`, or fails with `The method "platform-sample.echo" needs a text.` when the text is empty. |
| `platformSample.ticker(count, interval)` | Calls `platform-sample.ticker`, which answers `{started, count, interval}` and then sends `count` `platform-sample.tick` events, `interval` milliseconds apart. |
| `platformSample.onTick(listener)` | Calls `listener` with `{count, total, source}` for every `platform-sample.tick` event and returns the connection. |
| `platformSample.onActivity(listener)` | Calls `listener` with `{state, source}` for every `platform-sample.activity` event, which the native code sends when the Android activity resumes or pauses, the Apple app becomes active or resigns, or the tab shows or hides, and returns the connection. |

## Platforms

| Platform | Native part |
| --- | --- |
| iOS, Mac Catalyst, tvOS, macOS | `apple/PlatformSamplePlugin.m`, whose class registers the methods in `loadWithContext:` and follows the notifications of the app. |
| Android | The module in `android/`, whose `PlatformSamplePlugin` registers the methods in `onLoad` and sends the activity events from `onActivityResumed` and `onActivityPaused`. |
| Web | `web/platform-sample.js`, which registers the methods in `load` and follows the visibility of the tab. |
| Desktop player, Windows, Linux | None, so calls fail with the code `noHandler`. |

The plugin needs no setup on any platform.
