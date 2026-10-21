# 📖 `haylen.system`

The module `haylen.system` tells what the device and its operating system are, and follows the theme and the battery of the system. It also offers services that every system has. It opens a url with the app that handles it and vibrates the device. It shows what the app plays in the now-playing controls of the system, whose media keys and buttons reach the app. Use it to pick the language of a first launch, to match the colors of the app to the system, to do less work on a low battery, and for links to web pages, stores and mail. Native message boxes and file pickers live in [`haylen.dialogs`](dialogs.md), and the methods of the app and its plugins live in [`haylen.platform`](platform.md).

```lua
local system = require('haylen.system')
```

The platform reads what the device is once, when the first app of the process starts. It reads the theme and the battery whenever the system reports a change. The engine publishes each change once, at the start of the next frame, as the [events](#events) `systemThemeChanged` and `batteryChanged`.

## Functions

### system.info()

Returns a new table that describes the device. A value that the platform does not report is `nil`, never a guess, so code checks each value it relies on. The [platforms](#platforms) section tells what each platform reports.

| Field | Type | Meaning |
| --- | --- | --- |
| `os` | string | The operating system: `'macOs'`, `'windows'`, `'linux'`, `'ios'`, `'ipadOs'`, `'tvOs'`, `'android'` or `'web'`. Mac Catalyst apps run on `'macOs'`. |
| `osVersion` | string or `nil` | The version of the operating system, such as `'15.5'` on macOS, `'18.5'` on iOS, `'15'` on Android, `'10.0.26100'` on Windows or `'Ubuntu 24.04.1 LTS (Linux 6.8.0-45-generic)'` on Linux. On the web it is the system that runs the browser with its version, such as `'macOS 15.5.0'`, where the browser tells it. |
| `deviceModel` | string or `nil` | The model identifier, such as `'Mac15,11'` or `'iPhone16,1'`. |
| `manufacturer` | string or `nil` | The maker of the device, such as `'Apple'` or `'Google'`. |
| `deviceKind` | string | `'desktop'`, `'phone'`, `'tablet'`, `'tv'` or `'browser'`. |
| `cpuName` | string or `nil` | The name of the processor, such as `'Apple M3 Pro'`. |
| `cpuCores` | integer or `nil` | The number of logical processor cores. |
| `architecture` | string | The processor architecture that the app runs on: `'arm64'`, `'arm'`, `'x64'`, `'x86'`, `'wasm32'` or `'wasm64'`. It is the architecture of the build of the app. So a 32-bit Android library on a 64-bit phone reports `'arm'`, and a Mac app that Rosetta translates reports `'x64'`. |
| `bits` | integer | The width of the pointers of the app, `32` or `64`. |
| `memoryBytes` | integer or `nil` | The total memory of the device in bytes. |
| `gpuName` | string or `nil` | The GPU that the graphics device of the engine runs on, as its graphics API names it. That is the Metal device, the adapter of Direct3D 11, the `GL_RENDERER` string of OpenGL, or on WebGL 2 the renderer that `WEBGL_debug_renderer_info` unmasks where the browser offers it. On WebGPU it is the adapter info, whose description the vendor and the architecture replace where the browser leaves it empty. |
| `locale` | string or `nil` | The language of the user as a BCP 47 tag, such as `'pt-BR'`. |
| `languages` | table | Every language that the user prefers, most preferred first, as BCP 47 tags. It is empty where the platform does not report them. |
| `timeZone` | string or `nil` | The IANA name of the time zone, such as `'America/Sao_Paulo'`. |

```lua
local localization = require('haylen.localization')
local system = require('haylen.system')

local info = system.info()
print(string.format('%s %s on a %s.', info.os, info.osVersion or '', info.deviceKind))
if info.locale then
    localization.setLanguage(localization.findBestMatch(info.locale) or 'en')
end
```

### system.theme()

Returns `'light'` or `'dark'`, the colors that the system shows, as the app last heard at the start of a frame. The event `systemThemeChanged` reports every change.

```lua
local events = require('haylen.events')
local system = require('haylen.system')
local ui = require('haylen.ui')

ui.setTheme(system.theme())
events.on('systemThemeChanged', function(change)
    ui.setTheme(change.theme)
end)
```

### system.battery()

Returns a new table with the battery, as the app last heard of it at the start of a frame. The event `batteryChanged` reports every change.

| Field | Type | Meaning |
| --- | --- | --- |
| `level` | number or `nil` | The charge from 0 to 1, or `nil` where the platform does not tell it. |
| `charging` | boolean | Whether the battery charges. |
| `state` | string | `'charging'`, `'discharging'`, `'full'`, `'none'` for a device without a battery, or `'unknown'` where the platform does not tell it. |

```lua
local system = require('haylen.system')

local battery = system.battery()
if battery.state == 'discharging' and battery.level and battery.level < 0.2 then
    print('The battery is low, so the app draws fewer particles.')
end
```

### system.openUrl(url)

Opens `url` with the app that the system picks for it: the browser for a web page, the store for a store link or the mail app for a `mailto:` link. It returns a Varn promise that resolves on a later frame, with `true` when an app took the url and `false` otherwise. A url that is not a string raises a bad argument error, and an empty url raises `Opening a url needs a url.`.

Every platform opens urls: macOS through the workspace, iOS, iPadOS, Mac Catalyst and tvOS through the application, Android with a view intent, Windows through the shell and Linux with `xdg-open`. The web opens a new tab, which a popup blocker may refuse.

```lua
local async = require('async')
local system = require('haylen.system')

async.spawn(function()
    if not system.openUrl('https://example.com/tiny-island'):await() then
        print('No app opens web pages on this device.')
    end
end)
```

### system.vibrate(seconds)

Vibrates the device for `seconds` where it can vibrate, and does nothing elsewhere. A number that is not positive raises `A vibration lasts a positive number of seconds.`.

- Android phones, and the browsers of phones with the Vibration API, vibrate for the time given.
- iPhones play a medium impact on their haptic engine, whatever the time.
- The other devices do nothing.

On Android the app needs the permission `android.permission.VIBRATE`, which the manifest of the Android template declares. Without it `vibrate` logs once what is missing and how to add it, and does nothing, as [Android permissions](#android-permissions) describes.

```lua
local system = require('haylen.system')

local function onHit()
    system.vibrate(0.08)
end

onHit()
```

### system.setNowPlaying(info)

Shows what the app plays in the now-playing controls of the system, such as the lock screen and the control center of iPhones, the media notification of Android, the media controls of Windows and the media hub of browsers. From then on the media keys of keyboards, the buttons of headsets and the play and pause buttons of TV remotes reach the app as [`mediaCommand` events](#events).

A second call replaces what showed. So an app calls it again when the track, the position or the playing state changes. While it plays, the controls move the position on by themselves. The engine takes the app out of the controls when the app stops. The table `info` takes these keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title`, `artist`, `album` | string | `''` | The texts that the controls show. |
| `duration` | number | `0` | The length in seconds. The value `0` is a length that is not known, such as a live stream, which shows no position. |
| `position` | number | `0` | Where the playback is, in seconds. |
| `playing` | boolean | `false` | Whether it plays, which the play and pause control shows. |
| `artwork` | string | none | The bytes of a PNG or JPEG image, such as what `assets.bytes` returns. |
| `commands` | list of strings | every command | The controls to offer: `'play'`, `'pause'`, `'togglePlayPause'`, `'stop'`, `'next'`, `'previous'` and `'seek'`. |

These are the errors of the function:

- An unknown key raises `Unknown option "<key>".`.
- A negative duration or position raises `The duration and the position of what plays cannot be negative.`.
- An artwork that is not a string raises `The option "artwork" must be the bytes of a PNG or JPEG image.`.
- A `commands` value that is not a table raises `The option "commands" must be a list of command names.`.
- An unknown command raises `The option "commands" names "<name>", which is not "play", "pause", "togglePlayPause", "stop", "next", "previous" or "seek".`.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local events = require('haylen.events')
local system = require('haylen.system')

local track = {title = 'Harbor at dawn', artist = 'The island band', album = 'Tides', duration = 184}
local voice = audio.playMusic(assets.load('music/harbor.ogg', 'sound', {stream = true}))
local function show(playing)
    system.setNowPlaying({title = track.title, artist = track.artist, album = track.album, duration = track.duration, position = audio.cursor(voice), playing = playing, artwork = assets.bytes('music/tides.png'), commands = {'play', 'pause', 'togglePlayPause', 'seek'}})
end
show(true)

events.on('mediaCommand', function(event)
    if event.command == 'pause' or (event.command == 'togglePlayPause' and not audio.paused(voice)) then
        audio.pause(voice)
        show(false)
    elseif event.command == 'play' or event.command == 'togglePlayPause' then
        audio.resume(voice)
        show(true)
    elseif event.command == 'seek' then
        audio.setCursor(voice, event.position)
        show(not audio.paused(voice))
    end
end)
```

### system.clearNowPlaying()

Takes the app out of the now-playing controls of the system, which then stop sending `mediaCommand`.

```lua
local system = require('haylen.system')

system.setNowPlaying({title = 'Menu theme', playing = true})
system.clearNowPlaying()
```

## Events

The engine publishes these changes on [`haylen.events`](events.md#engine-events), once per change, at the start of the frame after the system reported it. An app that starts reads the current values, and it hears only the later changes.

| Event | When | Data |
| --- | --- | --- |
| `systemThemeChanged` | The system switched between light and dark colors. | `{theme}`, which is `'light'` or `'dark'`. |
| `batteryChanged` | The level, the charging or the state of the battery changed. | The table that `system.battery()` returns. |
| `mediaCommand` | A control of the system, a media key or a headset button asked the app to change what plays. It comes only once the app showed what plays with [`system.setNowPlaying`](#systemsetnowplayinginfo). | `{command}`, one of the names of `commands`, with `position` in seconds for `'seek'`. |

```lua
local events = require('haylen.events')

events.on('batteryChanged', function(battery)
    print(string.format('The battery is %s at %s.', battery.state, battery.level and math.floor(battery.level * 100) .. '%' or 'an unknown level'))
end)
```

## Platforms

This table tells where each platform reads the theme, the battery and what the device is.

| Platform | Theme | Battery | Device |
| --- | --- | --- | --- |
| macOS | The appearance of the app, with every change. | The power sources of the Mac, `'none'` on desktops without a battery. | The model, the processor, the memory, the version and the languages of the Mac. |
| iOS and iPadOS | The trait collection of the app, with every change. | The battery of the device, whose level the system updates about once a minute. | The model identifier, the version, the cores, the memory, the languages and the time zone. The simulator reports the model it simulates, and the processor and the memory of the Mac. |
| Mac Catalyst | The trait collection of the app, with every change. | The power sources of the Mac, `'none'` on desktops without a battery. | `'macOs'`, with the model, the processor, the memory, the version and the languages of the Mac. |
| tvOS | The trait collection of the app, with every change. | `'none'`, since TVs run on mains power. | The model identifier, the version, the cores, the memory and the languages. |
| Android | The night mode of the configuration, with every change. The activity handles the change itself, because the manifest of the template lists `uiMode` in its `configChanges`. | The sticky battery broadcast, heard while the activity lives, with every change. It is `'none'` on devices without a battery, such as TVs. A plugged battery that Android keeps from charging counts as discharging. | The model and its maker, the processor where Android 12 and later name it, the cores, the memory, the version, the languages and the time zone. A TV is a device in the television mode or with the leanback feature. A tablet is a device whose smallest screen side has 600 dp. A value that the device reports as unknown stays `nil`. |
| Web | The media query `prefers-color-scheme`, with every change. | The Battery Status API with every change, where the browser has it, which only Chromium browsers do. It is `'unknown'` elsewhere. A browser that cannot read a battery, as on a desktop without one, reports a full one. | What the browser tells, which it limits on purpose. That is the system it runs on with its version and the model of a phone through `navigator.userAgentData` where the browser has it, the cores, the memory that `navigator.deviceMemory` rounds and caps, the languages and the time zone. The processor and the maker stay `nil`. |
| Windows | The app theme of the personalization settings, with every change. | The power status of the system, with every change that Windows announces, `'none'` on desktops without a battery. | The version of the kernel, the model and the maker of the computer and the processor from the registry, the cores, the memory, the languages of the user and the time zone. The ICU of Windows 10 1903 and later gives the IANA name of the time zone. |
| Linux | The color scheme of the settings portal of the desktop, with every change, through GIO, which the engine loads at run time. Without GIO or the portal the theme stays `'light'` and never changes. | The first battery among the power supplies of the system, read when the app starts and every 30 seconds, `'none'` without one. | The distribution of `os-release` with the release of the kernel, the model and the maker of the computer, the processor, which ARM processors often leave out, the cores, the memory, the languages of `LANGUAGE` and `LANG`, and the time zone of `TZ` or `/etc/localtime`. |

This table tells where each platform shows what plays, and which keys and buttons reach the app.

| Platform | Controls | Media keys and buttons |
| --- | --- | --- |
| iOS and iPadOS | The lock screen, the control center and the Dynamic Island, through the info center and the remote commands of MediaPlayer, which the engine loads at run time. The lock screen shows an app whose audio session plays. So an app that plays in the background sets `audio.iosSession` of `app.json` to `playback`. | Headsets, CarPlay and the other accessories. |
| tvOS | The now-playing screen of the Apple TV. | The play and pause button of the Siri Remote and of other remotes. |
| macOS and Mac Catalyst | The now-playing item of the control center and of the menu bar, with the playback state that the media keys follow. | The media keys of the keyboard and the buttons of headsets. |
| Android | The media notification of the shade and the lock screen, of a media session. Android 13 and later draw the controls from the commands. The notifications of a media session need no permission. The notification channel takes its name from the string `haylen_media_channel` of the resources of the app, `Playback` by default. | Headsets, Bluetooth devices and the media buttons of TV remotes. The button that toggles reaches the app as `'togglePlayPause'`. |
| Web | The media hub of the browser and the lock screen of phones, through the Media Session API. The browser shows them only while the page plays sound. | The media keys of the keyboard and of headsets. A browser that lacks an action leaves it out. |
| Windows | The media flyout of the volume and of the lock screen, through the transport controls of the window of the app. | The media keys of the keyboard and of headsets. |
| Linux | None. Linux desktops show the players that serve the MPRIS interface on the session bus, which the engine does not serve. So `setNowPlaying` changes nothing there. | None. |

The browser and Windows have no toggle button. So an app that offers `'togglePlayPause'` without `'play'` and `'pause'` receives `'togglePlayPause'` from their play and pause buttons.

## Android permissions

The engine library of Android declares no permission, so the manifest of the app declares the ones that its features need. The manifest of the Android template declares all three as visible defaults, and an app deletes the ones it does not use. A feature whose permission is missing never crashes the app. It logs once, with the tag `haylen`, what is missing and the line that adds it to `app/src/main/AndroidManifest.xml`, and goes on without it.

| Permission | Feature | Without it |
| --- | --- | --- |
| `android.permission.VIBRATE` | `system.vibrate`. | It does nothing, which the log tells once as a warning. |
| `android.permission.ACCESS_NETWORK_STATE` | The network state of [`haylen.networkState()`](haylen.md#haylennetworkstate) and the events `networkOnline` and `networkOffline` of [`haylen.events`](events.md#engine-events), which the platform reports as `networkChanged`. | The engine never follows the network. The state stays `'unknown'` and the events never fire, which the log tells once at the info level. |
| `android.permission.INTERNET` | Network access: the `http` and `socket` modules of Varn and [`haylen.net`](net.md). | Connections fail. The `error` events of `haylen.net` sockets end with a sentence that names the missing permission and how to add it, while Varn reports what the system tells it. |

The [plugin guide](../platform/plugins.md#requirements) describes how plugins check their own requirements the same way.

## C++

The function `engine.getSystem()` returns the `haylen::platform::System` of the running app, declared in `haylen/platform/System.hpp`.

| Member | Meaning |
| --- | --- |
| `getInfo()` | The `haylen::platform::SystemInfo`. Its enums `Os` and `DeviceKind` name the operating system and the kind of device. Its empty strings and zeros are the values that the platform does not report. |
| `getTheme()` | The `haylen::platform::Theme`, `Light` or `Dark`. |
| `getBattery()` | The `haylen::platform::Battery`, with its optional `level`, `charging` and `State`. |
| `openUrl(url, callback)` | Opens the url and calls back on the frame thread with whether an app took it. |
| `vibrate(seconds)` | Vibrates the device. |

The functions `openUrl` and `vibrate` throw `std::invalid_argument` for the values that Lua rejects. The function `graphics::Device::getAdapterName()` names the GPU on its own.

```cpp
const haylen::platform::SystemInfo& info = engine.getSystem().getInfo();
haylen::core::Log::info("Running on {} with {} cores", haylen::platform::SystemInfo::osName(info.os), info.cpuCores);
engine.getSystem().openUrl("https://example.com", [](bool opened) {
    haylen::core::Log::info("The page {}", opened ? "opened" : "did not open");
});
```
