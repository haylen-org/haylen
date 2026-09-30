# Native Demo

A Haylen plugin that exercises every capability of native plugins with the APIs of each platform alone: calls on the main thread and on a background thread, typed failures, timeouts and cancellation, events and retained events, parameters with defaults, a native banner over the app that reserves its edge, a native screen that covers the app, the file picker of the platform, URLs that open the app and the errors that stop it. It is the plugin of the [plugins sample](../../README.md), and [the plugin guide](../../../../../docs/plugins.md#demo-plugin-and-sample) walks through it as the reference for writing a plugin on each platform. Its Lua API is the same on every platform.

## Installation

```sh
python3 make.py plugin add samples/system/plugins/plugins/native-demo --app my-game
```

`plugin add` copies the plugin into `plugins/native-demo/` of the app and lists it in the `plugins` section of its `app.json` with the defaults of its parameters:

```json
{
    "plugins": {
        "native-demo": {"greeting": "Hello from the native demo", "bannerColor": "#264653", "tickInterval": 1, "urlScheme": "haylendemo"}
    }
}
```

## Platforms

| Platform | Native part | What it uses |
| --- | --- | --- |
| iOS, iPadOS, Mac Catalyst | `apple/`, Swift | UIKit views over the app, a presented view controller, `UIDocumentPickerViewController`, `CFBundleURLTypes` and `scene(_:openURLContexts:)`. |
| tvOS | `apple/`, Swift | The same, without a file picker, so `pickFile` fails with the code `unsupported`. |
| macOS app | `apple/`, Swift | AppKit views over the app, a sheet, `NSOpenPanel`, `CFBundleURLTypes` and `application(_:open:)`. |
| Android | `android/`, a Kotlin library module | Views in panels of the overlay, a full screen `Dialog`, `ACTION_OPEN_DOCUMENT` with `startActivityForResult`, and an intent filter of the activity for the URL scheme. |
| Web | `web/native-demo.js` | DOM elements in the overlay, a modal `<dialog>`, an `<input type="file">` and the hash of the page address. |
| Desktop player, Windows, Linux | `native/`, a C library | Threads of the system and `HaylenNativeApi`. The desktops give native libraries no view API, so `showBanner`, `setBannerVisible`, `removeBanner`, `showScreen` and `pickFile` fail with the code `unsupported`, and so does `nativeConfig`, since native libraries receive no plugin parameters. The desktops open no URLs of the scheme either. |

The Lua API loads the C library with `native.load('native_demo', {init = 'native_demo_haylen_init'})` when it runs on macOS, Windows or Linux and no other native part loaded, which is the case of the desktop player and of Windows and Linux apps. The macOS app built from the Apple template runs the Swift part, which loads before any Lua runs.

## Parameters

| Parameter | Type | Default | Meaning |
| --- | --- | --- | --- |
| `greeting` | string | `"Hello from the native demo"` | Text of the native banner. |
| `bannerColor` | string | `"#264653"` | Background of the native banner and the native screen, as `#RRGGBB`. Another text fails `showBanner` and `showScreen` with the code `invalidColor`. |
| `tickInterval` | number | `1` | Seconds between two `tick` events. |
| `urlScheme` | string | `"haylendemo"` | URL scheme that opens the app, which the plugin declares in `CFBundleURLTypes` on Apple platforms and in an intent filter on Android through the placeholder `nativeDemoUrlScheme`. |

## Lua API

```lua
local demo = require('native-demo')
```

Every function that talks to the native part returns a call of [haylen.platform](../../../../../docs/lua-api/platform.md#calls), whose `await` inside a coroutine gives the answer, or `nil` and the error, a table with `message`, `code` and `data`. Every `on` function returns the connection of the listener, whose `disconnect()` stops it. Without the native part, as in the headless host of the engine tests, calls fail with the code `noHandler`.

### demo.available()

Returns whether the native part of the plugin runs on this platform, `platform.plugin('native-demo').native`.

```lua
local demo = require('native-demo')

if not demo.available() then
    print('the native part is not available on this platform')
end
```

### demo.config()

Returns the parameters of the plugin, the values of `app.json` over the defaults of `plugin.json`. `demo.id` and `demo.version` are the id and the version of the plugin.

```lua
local demo = require('native-demo')

print(demo.id, demo.version, demo.config().greeting, demo.config().tickInterval)
```

### demo.nativeConfig()

Answers with the parameters as the native part received them from its platform, the same values as `demo.config()`.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local config, err = demo.nativeConfig():await()
    print(config and config.urlScheme or err)
end)
```

### demo.echo(value)

Sends any JSON value to the native part, which answers on the main thread with `{echo, thread, language}`, where `thread` is `'main'`, or `'frame'` for the C library, whose handlers run on the frame thread.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local answer = demo.echo({text = 'hello', list = {1, 2, 3}}):await()
    print(answer.language, answer.thread, answer.echo.text)
end)
```

### demo.compute(limit)

Counts the primes below `limit` off the main thread and answers with `{primes, thread, detail, language}`: on a dispatch queue on Apple platforms, on the background thread of `HaylenBridge.Threading.BACKGROUND` on Android and on a thread of the library on the desktops, where `thread` is `'background'`. The web has one thread, so it counts in slices that yield to the page, and `thread` is `'main'`.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local answer = demo.compute(200000):await()
    print(answer.primes, answer.thread, answer.detail)
end)
```

### demo.fail()

Always fails with the message `The native demo failed on purpose.`, the code `demoFailure` and the data `{reason = 'requested', language}`.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local _, failure = demo.fail():await()
    print(failure.code, failure.data.reason, failure.message)
end)
```

### demo.wait(token, options)

Never answers by itself. `options` takes the `timeout` of [platform calls](../../../../../docs/lua-api/platform.md#platformcallmethod-params-options). When the timeout passes or the app cancels the call, the native part hears it and sends `waitCancelled` with the `token`.

### demo.onWaitCancelled(listener)

Calls `listener({token, language})` when the native part heard that the app gave up a `wait`.

```lua
local async = require('async')
local demo = require('native-demo')

demo.onWaitCancelled(function(payload) print('the native part stopped waiting for', payload.token) end)
async.spawn(function()
    local _, failure = demo.wait(1, {timeout = 0.5}):await()
    print(failure.code)
    local call = demo.wait(2)
    call:cancel()
end)
```

### demo.setTicking(enabled)

Starts or stops the native timer that sends `tick` every `tickInterval` seconds, and answers with `{enabled, interval}`. Swift runs a `Timer` on the main run loop, Kotlin posts to the main `Handler`, JavaScript uses `setInterval` and C a thread of the library.

### demo.onTick(listener)

Calls `listener({count, thread, language})` for every tick.

```lua
local demo = require('native-demo')

demo.onTick(function(tick) print('tick', tick.count, 'on the', tick.thread, 'thread') end)
demo.setTicking(true)
```

### demo.onLoaded(listener)

The native part sends `loaded` with `{language, platform}` retained when it loads, once per process, before any Lua runs on Apple platforms, Android and the web, and when the Lua API loads the C library on the desktops. The first listener of the process receives it however late it connects, and an app that restarted in the same process does not receive it again.

```lua
local demo = require('native-demo')

demo.onLoaded(function(payload) print('loaded', payload.language, payload.platform) end)
```

### demo.showBanner(anchor, reserve)

Shows the native banner, a bar with the `greeting` and a Tap button on the `bannerColor`, 360 by 56 points, dp or page pixels, or places it again. `anchor` is `'top'` or `'bottom'` of the safe area, and with `reserve` true the banner reserves its edge, so the safe area of the app shrinks and UI anchored to it moves out of its way. Answers with `{anchor, reserve, visible}`. Another anchor fails with the code `invalidAnchor`.

### demo.setBannerVisible(visible)

Shows or hides the banner, which reserves its edge only while it shows, and answers with `{anchor, reserve, visible}`. Without a banner it fails with the code `noBanner`.

### demo.removeBanner()

Takes the banner off the app and gives its edge back.

### demo.onBannerTapped(listener)

Calls `listener({count, language})` whenever the Tap button of the banner is tapped or clicked. Touches and clicks outside the banner reach the app. The views over the app never take the focus, so the remote of a TV and gamepads cannot press the button.

```lua
local async = require('async')
local demo = require('native-demo')
local viewport = require('haylen.viewport')

demo.onBannerTapped(function(tap) print('tapped', tap.count) end)
async.spawn(function()
    demo.showBanner('bottom', true):await()
    print('the banner reserves', viewport.reservedInsets().bottom)
    demo.showBanner('top', false):await()
    demo.setBannerVisible(false):await()
    demo.removeBanner():await()
end)
```

### demo.showScreen(title)

Shows a native screen with `title`, a line of text and a Close button over the whole app, and answers with `{seconds}` once Close, the Menu button of a TV remote, the Back button of Android or Escape on the web closed it. It is a presented view controller on iOS, iPadOS, Mac Catalyst and tvOS, a sheet on macOS, a full screen dialog on Android and a modal `<dialog>` on the web, and the plugin covers the app while it shows, so the app is inactive, halted and muted and `haylen.appCovered()` is true.

```lua
local async = require('async')
local demo = require('native-demo')
local events = require('haylen.events')
local haylen = require('haylen')

events.on('appInactive', function() print('covered', haylen.appCovered()) end)
async.spawn(function()
    local closed = demo.showScreen('Native screen'):await()
    print('the screen showed for', closed.seconds)
end)
```

### demo.pickFile()

Opens the file picker of the platform and answers with `{name}` of the picked file, or `nil` when the person cancels: `ACTION_OPEN_DOCUMENT` through `startActivityForResult` and `onActivityResult` on Android, `UIDocumentPickerViewController` on iOS, iPadOS and Mac Catalyst, `NSOpenPanel` on macOS and an `<input type="file">` on the web. It fails with the code `unsupported` on tvOS and the desktops. The browser opens its chooser only right after a click, a tap or a key press, and fails with the code `noUserGesture` otherwise.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local picked, err = demo.pickFile():await()
    print(err or (picked and picked.name or 'cancelled'))
end)
```

### demo.onUrlOpened(listener)

Calls `listener({url})` for every URL with the `urlScheme` that opens the app, whether it launched the app or reached it while it ran. The native part sends `urlOpened` retained, so the URL that launched the app waits for the first listener. The web stands in with the address of the page when it loads with a hash and whenever the hash changes.

```lua
local demo = require('native-demo')

demo.onUrlOpened(function(opened) print('opened', opened.url) end)
```

### demo.onLastError(listener)

The native part receives every error that stops the app, through `appDidFail(with:)` on Apple platforms, `onAppError` on Android, `context.onAppError` on the web and `registerErrorHandler` of `HaylenNativeApi` on the desktops, and keeps its message. When the next app of the process loads the Lua API, which sends `start`, the native part sends `lastError` retained with `{message, file, line, language}`, so an app restarted from its error screen learns why the one before it stopped.

```lua
local demo = require('native-demo')

demo.onLastError(function(failure) print('the last app stopped with', failure.message, failure.file, failure.line) end)
```

## Native API

| Method or event | Params or payload | Answer |
| --- | --- | --- |
| `native-demo.start` | `{}`, sent by the Lua API when it loads | `null`. Stops the ticks and removes the banner that an earlier app of the process left, and sends `lastError`. |
| `native-demo.echo` | `{value}` | `{echo, thread, language}` |
| `native-demo.compute` | `{limit}` | `{primes, thread, detail, language}` |
| `native-demo.fail` | `{}` | Fails with `demoFailure` and `{reason, language}`. |
| `native-demo.wait` | `{token}` | Never answers. |
| `native-demo.ticks` | `{enabled, interval}` | `{enabled, interval}` |
| `native-demo.config` | `{}` | The parameters of the plugin. |
| `native-demo.showBanner` | `{anchor, reserve}` | `{anchor, reserve, visible}` |
| `native-demo.setBannerVisible` | `{visible}` | `{anchor, reserve, visible}` |
| `native-demo.removeBanner` | `{}` | `null` |
| `native-demo.showScreen` | `{title}` | `{seconds}` |
| `native-demo.pickFile` | `{}` | `{name}` or `null` |
| `native-demo.loaded` (event, retained) | `{language, platform}` | |
| `native-demo.tick` (event) | `{count, thread, language}` | |
| `native-demo.waitCancelled` (event) | `{token, language}` | |
| `native-demo.bannerTapped` (event) | `{count, language}` | |
| `native-demo.urlOpened` (event, retained) | `{url}` | |
| `native-demo.lastError` (event, retained) | `{message, file, line, language}` | |
