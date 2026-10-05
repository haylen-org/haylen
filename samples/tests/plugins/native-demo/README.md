# Native Demo

A Haylen plugin that exercises every capability of native plugins with the APIs of each platform alone: calls on the main thread and on a background thread, typed failures, timeouts and cancellation, bytes both ways and an image drawn natively, video and audio streams, events, retained events and batched events, parameters with defaults, a native banner over the app that reserves its edge, a native screen that covers the app, screens of the plugin whose end reaches the next app after a restart, the file picker of the platform, permission prompts, local notifications, URLs that open the app and the errors that stop it. It is a plugin of the [test project](../../README.md), whose Plugins category, the tests from `PLG-001` to `PLG-015`, exercises every capability, and [the plugin guide](../../../../docs/plugins.md#demo-plugin-and-sample) walks through it as the reference for writing a plugin on each platform. Its Lua API is the same on every platform.

## Installation

```sh
python3 haylen.py plugin add samples/tests/plugins/native-demo --app my-app
```

`plugin add` copies the plugin into `plugins/native-demo/` of the app and lists it in the `plugins` section of its `app.json` with the defaults of its parameters:

```json
{
    "plugins": {
        "native-demo": {"greeting": "Hello from the native demo", "bannerColor": "#264653", "tickInterval": 1, "urlScheme": "haylendemo", "cameraUsage": "The Native Demo plugin asks for the camera to show how a plugin requests a permission."}
    }
}
```

## Platforms

| Platform | Native part | What it uses |
| --- | --- | --- |
| iOS, iPadOS, Mac Catalyst | `apple/`, Swift | UIKit views over the app, a presented view controller, `UIDocumentPickerViewController`, a UIKit and a SwiftUI screen, the latter in a window of its own on Mac Catalyst, `CVPixelBuffer`s drawn with CoreGraphics and floats from dispatch queues for the streams, `AVCaptureDevice` and `UNUserNotificationCenter` for the permissions and the notification, with `AVFoundation.framework` and `UserNotifications.framework` in the `frameworks` of `plugin.json` and the plugin class a `HaylenNotificationPlugin`, `CFBundleURLTypes`, `scene(_:openURLContexts:)`, `context.require` for the requirements and CoreGraphics with ImageIO for the image. |
| tvOS | `apple/`, Swift | The same, without a file picker, a camera and notifications that show, so `pickFile`, `requestPermission('camera')` and `notify` fail with the code `unsupported`. |
| macOS app | `apple/`, Swift | AppKit views over the app, a sheet, `NSOpenPanel`, an AppKit sheet and a SwiftUI window for the screens, the streams, the permissions and the notification as on iOS, `CFBundleURLTypes`, `application(_:open:)` and CoreGraphics with ImageIO for the image. |
| Android | `android/`, a Kotlin library module | Views of the overlay over the app, a full screen `Dialog`, an `AppCompatActivity` of its own for the confirm screen, `ActivityResultContracts.OpenDocument` and `RequestPermission` through the Activity Result API, an intent filter of `HaylenLinkActivity` of `dev.haylen:haylen-links` for the URL scheme and the taps on notifications, an alarm and `NotificationManagerCompat` for the notification, `HaylenRequirements` for the permissions, a `Bitmap` and a `Canvas` for the image and the video stream, and the streams of the context. |
| Web | `web/native-demo.js` and `web/screen.html` | DOM elements in the overlay, a modal `<dialog>`, a popup and a redirect to `screen.html` for the confirm screen, an `<input type="file">`, the hash of the page address, a `<canvas>` with `toBlob` for the image and the video stream, timers of the page for the tone and the bursts, and `context.require` for the requirements. |
| Desktop player, Windows, Linux | `native/`, a C library | Threads of the system and `HaylenNativeApi`, with a PNG encoder of its own, the video and audio streams of the engine and a window of its own for the confirm screen: a sheet with AppKit on macOS, an owned window with Win32 on Windows and a transient window with Xlib on Linux. The desktops place no views of native libraries over the app, so `showBanner`, `setBannerVisible`, `removeBanner`, `showScreen` and `pickFile` fail with the code `unsupported`, and so does `nativeConfig`, since native libraries receive no plugin parameters. The desktops open no URLs of the scheme either. |

The Lua API loads the C library with `native.load('native_demo', {init = 'native_demo_haylen_init'})` when it runs on macOS, Windows or Linux and no other native part loaded, which is the case of the desktop player and of Windows and Linux apps. The macOS app built from the Apple template runs the Swift part, which loads before any Lua runs.

## Parameters

| Parameter | Type | Default | Meaning |
| --- | --- | --- | --- |
| `greeting` | string | `"Hello from the native demo"` | Text of the native banner. |
| `bannerColor` | string | `"#264653"` | Background of the native banner and the native screen, as `#RRGGBB`. Another text fails `showBanner` and `showScreen` with the code `invalidColor`. |
| `tickInterval` | number | `1` | Seconds between two `tick` events. |
| `urlScheme` | string | `"haylendemo"` | URL scheme that opens the app, which the plugin declares in `CFBundleURLTypes` on Apple platforms and in an intent filter of `HaylenLinkActivity` on Android through the placeholder `nativeDemoUrlScheme`. |
| `cameraUsage` | string | `"The Native Demo plugin asks for the camera to show how a plugin requests a permission."` | The `NSCameraUsageDescription` of iOS, iPadOS, Mac Catalyst and macOS, which the camera prompt shows. |

## Lua API

```lua
local demo = require('native-demo')
```

Every function that talks to the native part returns a call of [haylen.platform](../../../../docs/lua-api/platform.md#calls), whose `await` inside a coroutine gives the answer, or `nil` and the error, a table with `message`, `code` and `data`. Every `on` function returns the connection of the listener, whose `disconnect()` stops it. Without the native part, as in the headless host of the engine tests, calls fail with the code `noHandler`.

### demo.available()

Returns whether the native part of the plugin runs on this platform, `platform.plugin('native-demo').native`.

```lua
local demo = require('native-demo')

if not demo.available() then
    print('The native part is not available on this platform.')
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

### demo.echoBytes(data)

Sends the bytes of the string `data` to the native part as a byte buffer, marked with `platform.bytes`, and the native part answers with `{data, size, thread, language}`, where `data` holds the same bytes as a Lua string. Swift reads and answers `Data`, Kotlin a `ByteArray`, JavaScript a `Uint8Array` and C the `HaylenNativeBuffer` of its handler. Without bytes it fails with the code `invalidParams`.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local data = string.char(0, 1, 2, 250, 255)
    local echoed = demo.echoBytes(data):await()
    print(echoed.language, echoed.size, echoed.data == data)
end)
```

### demo.generatedImage(width, height)

Draws the pattern of the plugin, a gradient from red to green with blue stripes, in an image of `width` by `height` pixels, from 1 to 2048, and answers with `{png, width, height, drawnWith, language}`, where `png` holds the bytes of a PNG file: drawn with CoreGraphics and written with ImageIO on Apple platforms, drawn in a `Bitmap` and compressed on the background thread on Android, drawn on a `<canvas>` and encoded with `toBlob` on the web, and drawn in memory and encoded by a PNG encoder of the library on the desktops. Another size fails with the code `invalidParams`.

```lua
local async = require('async')
local demo = require('native-demo')
local graphics = require('haylen.graphics')

async.spawn(function()
    local image = demo.generatedImage(384, 216):await()
    local texture = graphics.newTexture(image.png, {filter = 'linear'})
    print(image.drawnWith, #image.png, texture.width, texture.height)
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

Never answers by itself. `options` takes the `timeout` of [platform calls](../../../../docs/lua-api/platform.md#platformcallmethod-params-options). When the timeout passes or the app cancels the call, the native part hears it and sends `waitCancelled` with the `token`.

### demo.onWaitCancelled(listener)

Calls `listener({token, language})` when the native part heard that the app gave up a `wait`.

```lua
local async = require('async')
local demo = require('native-demo')

demo.onWaitCancelled(function(payload) print('The native part stopped waiting for', payload.token) end)
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

demo.onTick(function(tick) print('Tick', tick.count, 'on the thread', tick.thread) end)
demo.setTicking(true)
```

### demo.burst(count, ticks)

Sends `count` events of the name `burst` 30 times per second for `ticks` ticks, each marked batched, and then `burstDone` with `{events, ticks, language}`, and answers at once with `{count, ticks, language}`. Swift sends from a `Timer`, Kotlin from the main `Handler`, JavaScript from `setInterval` and C from a thread of the library.

### demo.onBurst(listener), demo.onBurstDone(listener)

`onBurst` calls `listener(list)` once per frame with the list of the `{tick, index, language}` of the events that arrived in that frame, in order, and `onBurstDone` calls `listener({events, ticks, language})` once the last event arrived.

```lua
local demo = require('native-demo')

local lists, events = 0, 0
demo.onBurst(function(list)
    lists = lists + 1
    events = events + #list
end)
demo.onBurstDone(function(done) print(events .. ' of ' .. done.events .. ' events arrived in ' .. lists .. ' lists.') end)
demo.burst(100, 30)
```

### demo.startVideo(), demo.stopVideo(), demo.videoStream()

`startVideo` opens the video stream `pattern` and draws the pattern into it 30 times per second with its stripes moving, and answers with `{width, height, fps, format, language}`: BGRA frames of 320 by 180 pixels from a thread of the library on the desktops, BGRA `CVPixelBuffer`s of 320 by 180 pixels drawn with CoreGraphics on a dispatch queue on Apple platforms, RGBA `Bitmap`s of 320 by 180 pixels drawn with a `Canvas` on a `HandlerThread` on Android, and a `<canvas>` of 320 by 180 pixels that `context.videoStream` copies on the web. `stopVideo` stops the frames. `videoStream()` returns the [video stream](../../../../docs/lua-api/platform.md#video-streams), or `nil` until the native part opened it.

```lua
local async = require('async')
local demo = require('native-demo')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        scene.spawn(self, function()
            if demo.startVideo():await() then
                self.video = demo.videoStream()
            end
        end)
    end,
    render = function(self)
        if self.video then
            graphics2d.beginScreen()
            graphics2d.draw(self.video.texture, 40, 40, {pivotX = 0, pivotY = 0})
        end
    end,
    exit = function(self)
        demo.stopVideo()
    end,
})
```

### demo.startTone(frequency), demo.stopTone(), demo.audioStream()

`startTone` opens the audio stream `tone` and synthesizes a sine wave of `frequency` hertz into it a tenth of a second ahead of the clock, and answers with `{frequency, sampleRate, channels, format, language}`: 16-bit mono samples at 44100 Hz from a thread of the library on the desktops, mono floats at 44100 Hz from a dispatch queue on Apple platforms and from a `HandlerThread` on Android, and `Float32Array` blocks at 44100 Hz from a timer of the page on the web. `stopTone` stops the samples. `audioStream()` returns the [audio stream](../../../../docs/lua-api/platform.md#audio-streams), or `nil` until the native part opened it.

```lua
local async = require('async')
local collections = require('haylen.collections')
local demo = require('native-demo')

async.spawn(function()
    if demo.startTone(440):await() then
        local tone = demo.audioStream()
        tone:play({volume = 0.5})
        local samples = collections.newFloatBuffer(512)
        print(tone.sampleRate, tone:read(samples))
    end
end)
```

### demo.onLoaded(listener)

The native part sends `loaded` with `{language, platform}` retained when it loads, once per process, before any Lua runs on Apple platforms, Android and the web, and when the Lua API loads the C library on the desktops. The first listener of the process receives it however late it connects, and an app that restarted in the same process does not receive it again.

```lua
local demo = require('native-demo')

demo.onLoaded(function(payload) print('Loaded', payload.language, payload.platform) end)
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

demo.onBannerTapped(function(tap) print('Tapped', tap.count) end)
async.spawn(function()
    demo.showBanner('bottom', true):await()
    print('The banner reserves', viewport.reservedInsets().bottom)
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

events.on('appInactive', function() print('Covered', haylen.appCovered()) end)
async.spawn(function()
    local closed = demo.showScreen('Native screen'):await()
    print('The screen showed for', closed.seconds)
end)
```

### demo.openScreen(options)

Opens the confirm [screen](../../../../docs/plugins.md#plugin-screens) of the plugin, which asks a question with Confirm and Decline, and answers with `{confirmed, via, language}` once the person answers. The engine covers the app before the screen shows and draws nothing under it. It is a UIKit controller presented full screen on iOS, iPadOS, Mac Catalyst and tvOS, whose swipe and Menu button end it with the code `cancelled`, an AppKit sheet in the macOS app, a popup of `web/screen.html` on the web, which the browser blocks unless a click, a tap or a key press asked for it right before, and a window over the window of the app on the desktops: a sheet on macOS, an owned window on Windows and a transient window on Linux, whose Close button and close box end it with the code `cancelled`. On Android it is an `AppCompatActivity` of the plugin that the activity of the app starts through the Activity Result API, whose Back button ends it with the code `cancelled` and which a cancel of the app finishes, and when Android ended the process while it showed, its answer reaches the next app as `screenRestored`. `options` takes `state`, `opaque` and `timeout`, like `openScreen` of the plugin handle, and the state comes back with a restored end.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local answer, err = demo.openScreen({state = {level = 3}}):await()
    print(answer and ('Confirmed ' .. tostring(answer.confirmed) .. ' through ' .. answer.via) or err.code)
end)
```

### demo.openSwiftUIScreen(options)

Opens the same question in SwiftUI, which only Apple platforms have: a `UIHostingController` over the app on iOS, iPadOS and tvOS and in a window of its own on Mac Catalyst, and an `NSHostingController` in a window of its own in the macOS app. It answers like `openScreen` with `via` naming SwiftUI, and its Close button dismisses it through the dismiss action of SwiftUI, which fails the call with the code `cancelled`. The other platforms fail with the code `noHandler`.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local answer, err = demo.openSwiftUIScreen():await()
    print(answer and answer.via or err.code)
end)
```

### demo.openRedirectScreen(options)

Opens the confirm screen of the web by leaving the page for `web/screen.html`, which comes back with the answer in the address. The page loads again, so the answer reaches the new app as `screenRestored` with the state. The other platforms fail with the code `unsupported` or `noHandler`.

```lua
local demo = require('native-demo')

demo.openRedirectScreen({state = {level = 3}})
```

### demo.onScreenRestored(listener)

Calls `listener({screen, state, result})`, or `{screen, state, error}` for a failure, for a screen whose app restarted, or whose page loaded again, before it ended. The event is retained, so the first listener receives it however late it connects.

```lua
local demo = require('native-demo')

demo.onScreenRestored(function(ending)
    print(ending.screen, ending.state.level, ending.result and ending.result.confirmed)
end)
```

### demo.pickFile()

Opens the file picker of the platform and answers with `{name}` of the picked file, or `nil` when the person cancels: `ActivityResultContracts.OpenDocument` on Android, whose launcher the plugin registers under a stable key when the activity is created, `UIDocumentPickerViewController` on iOS, iPadOS and Mac Catalyst, `NSOpenPanel` on macOS and an `<input type="file">` on the web. It fails with the code `unsupported` on tvOS and the desktops. The browser opens its chooser only right after a click, a tap or a key press, and fails with the code `noUserGesture` otherwise.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local picked, err = demo.pickFile():await()
    print(err or (picked and picked.name or 'Cancelled'))
end)
```

### demo.requestPermission(kind)

Asks the person for the permission `kind`, `'camera'` or `'notifications'`, with the prompt of the system, and answers with `{kind, granted, status, language}` once the person answered, or at once when the person answered before. On Apple platforms the camera prompt shows the text of the `cameraUsage` parameter, `status` is the authorization status of `AVCaptureDevice` or `UNUserNotificationCenter`, such as `'authorized'` or `'denied'`, and tvOS fails the camera with the code `unsupported`. On Android the plugin first checks that the manifest of the app declares `android.permission.CAMERA` or `android.permission.POST_NOTIFICATIONS`, which the manifest of its module does, and fails with the code `unsupported` and `data.missing` otherwise, `status` is `'authorized'` or `'denied'`, notifications ask for nothing before Android 13, where the answer tells whether the person left them on, and a device without a camera fails the camera with the code `unsupported`. Another kind fails with the code `invalidPermission`, and a second request while one shows with the code `busy`.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local answer, err = demo.requestPermission('notifications'):await()
    print(answer and answer.status or err.code)
end)
```

### demo.notify(seconds), demo.onNotificationOpened(listener)

`notify` schedules a local notification of the plugin after `seconds` and answers with `{identifier, seconds, language}`. The notification shows while the app is in front too, and when the person taps it the native part sends `notificationOpened` retained with `{identifier, title, action, language}`, so a tap that launched the closed app reaches the first listener however late it connects. On Apple platforms it goes through `UNUserNotificationCenter`, which the plugin links with `UserNotifications.framework` in the `frameworks` of its `plugin.json`, and whose delegate the runtime then owns and hands to the plugin class, a `HaylenNotificationPlugin`, and tvOS fails `notify` with the code `unsupported`, since TVs show no notifications. On Android an alarm posts the notification through `NotificationManagerCompat` on the channel `native-demo`, also when the app was closed, its tap starts `HaylenLinkActivity`, which hands it to the app, and `notify` fails with the code `permissionDenied` while the notifications of the app are off.

```lua
local async = require('async')
local demo = require('native-demo')

demo.onNotificationOpened(function(tap)
    print('Opened from ' .. tap.identifier)
end)

async.spawn(function()
    demo.requestPermission('notifications'):await()
    demo.notify(5):await()
end)
```

### demo.requirementCheck()

Calls a method whose native part needs something that the plugin leaves out of the project of the app on purpose, to show how a plugin checks its [requirements](../../../../docs/plugins.md#requirements). On Android it needs the permission `android.permission.READ_CONTACTS`, which the manifest of its module never declares, on Apple platforms the usage description `NSContactsUsageDescription`, which the `infoPlist` of its `plugin.json` never gives, and on the web the Contact Picker API `navigator.contacts` in a secure context, which only browsers of phones offer. The call fails with the code `unsupported` and `data.missing` lists what is missing as `{kind, name, file, snippet}`, while the log tells once what is missing and how to add it. A web requirement has no file of a project, so its `file` and `snippet` are empty. An app whose project has the requirement, and a phone browser with the API, get `{met, language}` instead, and the desktops, whose C library checks no requirements, answer with the code `noHandler`.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local answer, err = demo.requirementCheck():await()
    if answer then
        print('Met in', answer.language)
    elseif err.data and err.data.missing then
        for _, missing in ipairs(err.data.missing) do
            print(missing.kind, missing.name, missing.file, missing.snippet)
        end
    end
end)
```

### demo.onUrlOpened(listener)

Calls `listener({url})` for every URL with the `urlScheme` that opens the app, whether it launched the app or reached it while it ran. The native part sends `urlOpened` retained, so the URL that launched the app waits for the first listener. The web stands in with the address of the page when it loads with a hash and whenever the hash changes.

```lua
local demo = require('native-demo')

demo.onUrlOpened(function(opened) print('Opened', opened.url) end)
```

### demo.onLastError(listener)

The native part receives every error that stops the app, through `appDidFail(with:)` on Apple platforms, `onAppError` on Android, `context.onAppError` on the web and `registerErrorHandler` of `HaylenNativeApi` on the desktops, and keeps its message. When the next app of the process loads the Lua API, which sends `start`, the native part sends `lastError` retained with `{message, file, line, language}`, so an app restarted from its error screen learns why the one before it stopped.

```lua
local demo = require('native-demo')

demo.onLastError(function(failure) print('The last app stopped with', failure.message, failure.file, failure.line) end)
```

## Native API

| Method or event | Params or payload | Answer |
| --- | --- | --- |
| `native-demo.start` | `{}`, sent by the Lua API when it loads | `null`. Stops the ticks, the streams and the bursts and removes the banner that an earlier app of the process left, and sends `lastError`. |
| `native-demo.echo` | `{value}` | `{echo, thread, language}` |
| `native-demo.echoBytes` | `{data}` with the bytes of `data` | `{data, size, thread, language}` with the same bytes |
| `native-demo.generatedImage` | `{width, height}` | `{png, width, height, drawnWith, language}` with the bytes of `png` |
| `native-demo.startVideo`, `native-demo.stopVideo` | `{}` | `{width, height, fps, format, thread, language}`, and `null` |
| `native-demo.startTone`, `native-demo.stopTone` | `{frequency}`, and `{}` | `{frequency, sampleRate, channels, format, language}`, and `null` |
| `native-demo.burst` | `{count, ticks}` | `{count, ticks, language}` |
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
| `native-demo.requirementCheck` | `{}` | `{met, language}`, or fails with `unsupported` and `{missing}` while the project lacks what it needs. |
| `native-demo.loaded` (event, retained) | `{language, platform}` | |
| `native-demo.tick` (event) | `{count, thread, language}` | |
| `native-demo.burst` (event, batched) | `{tick, index, language}` | |
| `native-demo.burstDone` (event) | `{events, ticks, language}` | |
| `native-demo.waitCancelled` (event) | `{token, language}` | |
| `native-demo.bannerTapped` (event) | `{count, language}` | |
| `native-demo.urlOpened` (event, retained) | `{url}` | |
| `native-demo.lastError` (event, retained) | `{message, file, line, language}` | |

| Stream | Kind | Format |
| --- | --- | --- |
| `pattern` | Video | BGRA frames of 320 by 180 pixels, 30 per second, from C, RGBA bitmaps of the same size from Kotlin, and RGBA frames of the canvas on the web. |
| `tone` | Audio | 16-bit mono samples at 44100 Hz from C, and float mono samples at 44100 Hz from Kotlin and on the web. |
