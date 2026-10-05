# Native Demo

A Haylen plugin that exercises every capability of native plugins with the APIs of each platform alone: calls on the main thread and on a background thread, typed failures, timeouts and cancellation, bytes both ways and an image drawn natively, video and audio streams, events, retained events and batched events, parameters with defaults, a native banner over the app that reserves its edge, a native screen that covers the app, screens of the plugin whose end reaches the next app after a restart, the file picker of the platform, permission prompts, local notifications, URLs that open the app and the errors that stop it. It also simulates the SDKs that real plugins bring, with the same APIs alone: a camera with photos and a microphone, the location and a map, the share sheet and other apps, and a fake store, fake ads and a fake sign-in, whose sheets are screens of the plugin. It is a plugin of the [test project](../../README.md), whose Plugins category, the tests from `PLG-001` to `PLG-021`, exercises every capability, and [the plugin guide](../../../../docs/plugins.md#demo-plugin) walks through it as the reference for writing a plugin on each platform. Its Lua API is the same on every platform.

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
| iOS, iPadOS, Mac Catalyst | `apple/`, Swift | `NativeDemoDevice.swift` with AVFoundation for the camera and the microphone, Core Location, a MapKit view over the app, `UIActivityViewController` and the links of other apps, `NativeDemoStore.swift` with the fake store, ads and sign-in as screens of the plugin in alerts of `NativeDemoSheet.swift`, UIKit views over the app, a presented view controller, `UIDocumentPickerViewController`, a UIKit and a SwiftUI screen, the latter in a window of its own on Mac Catalyst, `CVPixelBuffer`s drawn with CoreGraphics and floats from dispatch queues for the streams, `AVCaptureDevice` and `UNUserNotificationCenter` for the permissions and the notification, with `AVFoundation.framework` and `UserNotifications.framework` in the `frameworks` of `plugin.json` and the plugin class a `HaylenNotificationPlugin`, `CFBundleURLTypes`, `scene(_:openURLContexts:)`, `context.require` for the requirements and CoreGraphics with ImageIO for the image. |
| tvOS | `apple/`, Swift | The same, without a file picker, a camera, a microphone for apps, a share sheet and notifications that show, so `pickFile`, `requestPermission('camera')`, `startCamera`, `takePhoto`, `startMicrophone`, `share` and `notify` fail with the code `unsupported`. |
| macOS app | `apple/`, Swift | The simulations of iOS with `NSSharingServicePicker`, `NSWorkspace` and `NSAlert` sheets, with the entitlements of the hardened runtime that the camera, the microphone and the location need, AppKit views over the app, a sheet, `NSOpenPanel`, an AppKit sheet and a SwiftUI window for the screens, the streams, the permissions and the notification as on iOS, `CFBundleURLTypes`, `application(_:open:)` and CoreGraphics with ImageIO for the image. |
| Android | `android/`, a Kotlin library module | `NativeDemoDevice.kt` with Camera2, `AudioRecord` and `LocationManager`, a drawn map in `NativeDemoMap.kt`, the share sheet and other apps through intents, `NativeDemoStore.kt` with the fake store, ads and sign-in as screens of the plugin in dialogs, the permissions `RECORD_AUDIO`, `ACCESS_FINE_LOCATION` and `ACCESS_COARSE_LOCATION` next to the others, views of the overlay over the app, a full screen `Dialog`, an `AppCompatActivity` of its own for the confirm screen, `ActivityResultContracts.OpenDocument` and `RequestPermission` through the Activity Result API, an intent filter of `HaylenLinkActivity` of `dev.haylen:haylen-links` for the URL scheme and the taps on notifications, an alarm and `NotificationManagerCompat` for the notification, `HaylenRequirements` for the permissions, a `Bitmap` and a `Canvas` for the image and the video stream, and the streams of the context. |
| Web | `web/native-demo.js`, `web/simulations.js` and `web/screen.html` | `getUserMedia` with an audio worklet for the camera and the microphone, the Geolocation API, a drawn map in the overlay, the Web Share API, links of other apps, and `<dialog>` elements for the fake store, ads and sign-in as screens of the plugin, all through `context.require` for a secure context and the API, DOM elements in the overlay, a modal `<dialog>`, a popup and a redirect to `screen.html` for the confirm screen, an `<input type="file">`, the hash of the page address, a `<canvas>` with `toBlob` for the image and the video stream, timers of the page for the tone and the bursts, and `context.require` for the requirements. |
| Desktop player, Windows, Linux | `native/`, a C library | Threads of the system and `HaylenNativeApi`, with a PNG encoder of its own, the video and audio streams of the engine and a window of its own for the confirm screen: a sheet with AppKit on macOS, an owned window with Win32 on Windows and a transient window with Xlib on Linux. The same windows show the fake store, ads and sign-in, and the shell of the system opens other apps. The desktops place no views of native libraries over the app, so `showBanner`, `setBannerVisible`, `removeBanner`, `showScreen`, `showMap` and `pickFile` fail with the code `unsupported`, and so do the camera, the microphone, the location and `share`, whose desktop APIs differ on each system, and `nativeConfig`, since native libraries receive no plugin parameters. The desktops open no URLs of the scheme either. |

The Lua API loads the C library with `native.load('native_demo', {init = 'native_demo_haylen_init'})` when it runs on macOS, Windows or Linux and no other native part loaded, which is the case of the desktop player and of Windows and Linux apps. The macOS app built from the Apple template runs the Swift part, which loads before any Lua runs.

## Parameters

| Parameter | Type | Default | Meaning |
| --- | --- | --- | --- |
| `greeting` | string | `"Hello from the native demo"` | Text of the native banner. |
| `bannerColor` | string | `"#264653"` | Background of the native banner and the native screen, as `#RRGGBB`. Another text fails `showBanner` and `showScreen` with the code `invalidColor`. |
| `tickInterval` | number | `1` | Seconds between two `tick` events. |
| `urlScheme` | string | `"haylendemo"` | URL scheme that opens the app, which the plugin declares in `CFBundleURLTypes` on Apple platforms and in an intent filter of `HaylenLinkActivity` on Android through the placeholder `nativeDemoUrlScheme`. |
| `cameraUsage` | string | `"The Native Demo plugin asks for the camera to show how a plugin requests a permission and to simulate the camera of an SDK."` | The `NSCameraUsageDescription` of iOS, iPadOS, Mac Catalyst and macOS, which the camera prompt shows. |
| `microphoneUsage` | string | `"The Native Demo plugin records the microphone to simulate the audio capture of an SDK."` | The `NSMicrophoneUsageDescription` of iOS, iPadOS, Mac Catalyst and macOS. |
| `locationUsage` | string | `"The Native Demo plugin reads the location to simulate the location and the map of an SDK."` | The `NSLocationWhenInUseUsageDescription` and `NSLocationUsageDescription` of the Apple platforms. |
| `macDevices` | boolean | `true` | Whether the macOS app signs with the entitlements `com.apple.security.device.camera`, `com.apple.security.device.audio-input` and `com.apple.security.personal-information.location`, which the hardened runtime needs for the camera, the microphone and the location. The parameter has a value on macOS alone, so the other platforms leave the entitlements out. |

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

Opens the same question in SwiftUI, which only Apple platforms have: a `UIHostingController` over the app on iOS, iPadOS and tvOS and in a window of its own on Mac Catalyst, and an `NSHostingController` in a window of its own in the macOS app. It answers like `openScreen` with `via` naming SwiftUI, and its Close button ends it with the code `cancelled`, which closes it on every platform, the window of Mac Catalyst included. The other platforms fail with the code `noHandler`.

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

## Simulations of SDKs

The functions below stand in for the SDKs that real plugins bring. Each answers with what the platform gives and fails with the code `unsupported` and the reason where the platform cannot run it. A purchase, an ad and a sign-in take over the app until they end, so they are [screens](../../../../docs/plugins.md#plugin-screens) of the plugin, which the engine covers the app for, and their `options` take the `state`, `opaque` and `timeout` of `handle:openScreen`. The test project shows them in the tests `PLG-016` to `PLG-021`.

### demo.startCamera(facing), demo.stopCamera(), demo.cameraStream(), demo.takePhoto()

Starts the camera, `'back'` by default or `'front'`, after the permission of the person, and answers with `{width, height, facing, language}` once frames flow. The native part pushes its frames into the video stream `camera`, which `demo.cameraStream()` returns, and `takePhoto` answers with `{jpeg, width, height, language}`, the newest frame as the bytes of a JPEG file. A device without a camera, Apple TV and the desktops of the C library fail with `unsupported`, a refused permission with `permissionDenied`, and `takePhoto` without a camera with `cameraOff`.

```lua
local async = require('async')
local graphics = require('haylen.graphics')
local demo = require('native-demo')

async.spawn(function()
    local started, err = demo.startCamera('back'):await()
    if err then
        print('No camera', err.code, err.message)
        return
    end
    local photo = demo.takePhoto():await()
    local texture = graphics.newTexture(photo.jpeg)
    print(started.width, started.height, texture.width, demo.cameraStream().texture.width)
end)
```

### demo.startMicrophone(), demo.stopMicrophone(), demo.microphoneStream(), demo.onMicrophoneLevel(listener)

Starts the microphone after the permission of the person and answers with `{sampleRate, channels, language}`. The native part pushes mono floats into the audio stream `microphone`, which `demo.microphoneStream()` returns and the app plays or reads, and sends `microphoneLevel` with `{level, language}`, the level of the newest samples from 0 to 1, about ten times per second. On iOS the plugin switches the audio session to record while the microphone runs and restores it afterwards.

```lua
local async = require('async')
local demo = require('native-demo')

demo.onMicrophoneLevel(function(update) print('Level', update.level) end)
async.spawn(function()
    local started = demo.startMicrophone():await()
    print(started and started.sampleRate)
end)
```

### demo.location(), demo.showMap(anchor, latitude, longitude), demo.removeMap()

The call `location` asks for the permission of the person while the app is in use and answers with `{latitude, longitude, accuracy, language}`, the accuracy in meters, or fails with `permissionDenied` or `locationUnknown`. The call `showMap` places a native map of 360 by 220 points at the `'top'` or `'bottom'` of the app, centered on the place, and answers with `{anchor, latitude, longitude, drawnWith, language}`: MapKit on Apple platforms, and a view that draws the map on Android and the web, as the view of a map SDK would.

```lua
local async = require('async')
local demo = require('native-demo')

async.spawn(function()
    local place = demo.location():await() or {latitude = -22.9519, longitude = -43.2105}
    demo.showMap('bottom', place.latitude, place.longitude):await()
end)
```

### demo.share(text, url), demo.openApp(kind)

The call `share` opens the share sheet of the platform with the text and the url, which may be `nil`, and answers with `{shared, language}`, where `shared` tells whether the person shared, or is `nil` on Android, whose share sheet tells nothing. The call `openApp` opens the settings of the app for `'settings'`, a map app at a place for `'maps'` and a new message of the mail app for `'mail'`, and answers with `{opened, language}`. A web page cannot open the settings, and the C library opens no map app on Windows and Linux, which fail with `unsupported`.

```lua
local demo = require('native-demo')

demo.share('A text', 'https://example.com')
demo.openApp('mail')
```

### demo.products(), demo.purchase(productId, options), demo.restorePurchases(), demo.onPurchaseUpdated(listener)

The fake store lists three products, `coins.small` and `coins.large`, which the person buys again and again, and `ads.remove`, which the device keeps, each `{id, title, description, price, currency, kind}`. The screen `purchase` shows a sheet with the product and answers with `{productId, transactionId, receipt, language}` once the person buys it, or fails with `cancelled`, and `restorePurchases` answers with the kept purchases, each `{productId, transactionId}`. Every purchase and every restored one sends `purchaseUpdated` with `{productId, transactionId, state, language}`, where `state` is `'purchased'` or `'restored'`. Nothing is charged. The device keeps the purchases in `UserDefaults`, `SharedPreferences` and the local storage of the page, and the C library for the life of the process.

```lua
local async = require('async')
local demo = require('native-demo')

demo.onPurchaseUpdated(function(update) print(update.productId, update.state) end)
async.spawn(function()
    local purchase, err = demo.purchase('ads.remove'):await()
    print(purchase and purchase.transactionId or err.code)
end)
```

### demo.showInterstitial(options), demo.showRewarded(options), demo.onAdRewarded(listener)

The screen `interstitial` shows a fake full-screen ad until the person continues to the app and answers with `{closed, language}`, and the screen `rewarded` asks whether the person watches a fake ad to the end and answers with `{rewarded, amount, currency, language}`, sending `adRewarded` with `{amount, currency, language}` when it grants the reward. The engine covers the app while each shows.

```lua
local demo = require('native-demo')

demo.onAdRewarded(function(reward) print('Earned', reward.amount, reward.currency) end)
demo.showRewarded()
```

### demo.signIn(options), demo.currentUser(), demo.signOut(), demo.onUserChanged(listener)

The screen `signIn` lets the person pick a fake account and answers with `{userId, name, email, token, language}`, or fails with `cancelled`. The device keeps the account, which `currentUser` answers with, or `nil`, until `signOut` forgets it. Every sign-in sends `userChanged` with the account, and every sign-out with `nil`. No password and no network take part.

```lua
local async = require('async')
local demo = require('native-demo')

demo.onUserChanged(function(account) print(account and account.name or 'Signed out') end)
async.spawn(function()
    local account = demo.signIn():await()
    print(account and account.email)
end)
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
| `native-demo.startCamera`, `native-demo.stopCamera` | `{facing}`, and `{}` | `{width, height, facing, language}`, and `null` |
| `native-demo.takePhoto` | `{}` | `{jpeg, width, height, language}` with the bytes of `jpeg` |
| `native-demo.startMicrophone`, `native-demo.stopMicrophone` | `{}` | `{sampleRate, channels, language}`, and `null` |
| `native-demo.location` | `{}` | `{latitude, longitude, accuracy, language}` |
| `native-demo.showMap`, `native-demo.removeMap` | `{anchor, latitude, longitude}`, and `{}` | `{anchor, latitude, longitude, drawnWith, language}`, and `null` |
| `native-demo.share` | `{text, url}` | `{shared, language}` |
| `native-demo.openApp` | `{kind}` | `{opened, language}` |
| `native-demo.products` | `{}` | A list of `{id, title, description, price, currency, kind}` |
| `native-demo.restorePurchases` | `{}` | A list of `{productId, transactionId}` |
| `native-demo.currentUser`, `native-demo.signOut` | `{}` | The account or `null`, and `null` |
| `native-demo.purchase` (screen) | `{productId}` | `{productId, transactionId, receipt, language}` |
| `native-demo.interstitial` (screen) | `{placement}` | `{closed, language}` |
| `native-demo.rewarded` (screen) | `{placement}` | `{rewarded, amount, currency, language}` |
| `native-demo.signIn` (screen) | `{scopes}` | `{userId, name, email, token, language}` |
| `native-demo.microphoneLevel` (event) | `{level, language}` | |
| `native-demo.purchaseUpdated` (event) | `{productId, transactionId, state, language}` | |
| `native-demo.adRewarded` (event) | `{amount, currency, language}` | |
| `native-demo.userChanged` (event) | The account, or `null` | |

| Stream | Kind | Format |
| --- | --- | --- |
| `pattern` | Video | BGRA frames of 320 by 180 pixels, 30 per second, from C, RGBA bitmaps of the same size from Kotlin, and RGBA frames of the canvas on the web. |
| `tone` | Audio | 16-bit mono samples at 44100 Hz from C, and float mono samples at 44100 Hz from Kotlin and on the web. |
| `camera` | Video | BGRA pixel buffers of 640 by 480 pixels from Swift, RGBA frames of 640 by 480 pixels from Kotlin, and RGBA frames of the video of the camera on the web. |
| `microphone` | Audio | Float mono samples at the rate of the input of the device from Swift and on the web, and at 44100 Hz from Kotlin. |
