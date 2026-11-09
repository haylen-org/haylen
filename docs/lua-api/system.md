# 📖 `haylen.system`

The module `haylen.system` tells what the device and its operating system are, and follows the theme, the battery, the power saving mode, the time zone and the languages of the system. It also offers services that every system has. It opens a url with the app that handles it, reads and writes the clipboard, vibrates the device, and checks and asks for the [permissions](#permissions) that a system grants while the app runs, those of the engine and those of the plugins of the app, with one vocabulary of states on every platform. It shows what the app plays in the now-playing controls of the system, whose media keys and buttons reach the app. Use it to know the languages the person prefers, to match the colors of the app to the system, to do less work on a low battery or while the system saves power, and for links to web pages, stores and mail. Native message boxes and file pickers live in [`haylen.dialogs`](dialogs.md), and the methods of the app and its plugins live in [`haylen.platform`](platform.md).

```lua
local system = require('haylen.system')
```

The platform reads what the device is once, when the first app of the process starts. It reads the theme, the battery, the power saving mode, the time zone and the languages whenever the system reports a change. The engine publishes each change once, at the start of the next frame, as the [events](#events) `systemThemeChanged`, `batteryChanged`, `powerSavingChanged`, `timeZoneChanged` and `systemLanguagesChanged`.

## Functions

### system.info()

Returns a new table that describes the device. A value that the platform does not report is `nil`, never a guess, so code checks each value it relies on. The [platforms](#platforms) section tells what each platform reports.

| Field | Type | Meaning |
| --- | --- | --- |
| `os` | string | The operating system: `'macOs'`, `'windows'`, `'linux'`, `'ios'`, `'ipadOs'`, `'tvOs'`, `'android'`, `'web'`, or `'port'` on a [port](../platform/ports.md) to another platform, whose name [`haylen.platform`](haylen.md#haylenplatform) gives. Mac Catalyst apps run on `'macOs'`. |
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

```lua
local system = require('haylen.system')

local info = system.info()
print(string.format('%s %s on a %s.', info.os, info.osVersion or '', info.deviceKind))
```

### system.languages()

Returns a new list of the languages that the person prefers on the device, most preferred first, as the app last heard at the start of a frame. Each one is a language tag in its canonical form, such as `'pt-BR'` or `'zh-Hans-CN'`, as [`localization.normalizeTag`](localization.md#localizationnormalizetagtag) writes it. The engine leaves out what the platform reports twice and, with a warning in the log, what is no language tag. The list is empty where the platform tells none. The event `systemLanguagesChanged` reports every change. The current language of [`haylen.localization`](localization.md) follows this list until the app picks one, and the [localization guide](../core/localization.md#where-the-languages-come-from) tells where each platform reads it.

```lua
local system = require('haylen.system')

local languages = system.languages()
print('The device prefers ' .. (languages[1] or 'no language it tells') .. '.')
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

### system.powerSaving()

Returns `true` while the power saving mode of the system is on, such as the low power mode of Apple platforms, the battery saver of Android and Windows or the power saver profile of Linux, `false` while it is off, and `nil` where the platform does not tell, as the [platforms](#platforms) section lists. The event `powerSavingChanged` reports every change. An app that saves power too draws fewer effects, lowers its frame rate with [`window.setFrameRate`](window.md#windowsetframerateframespersecond) or pauses work that can wait.

```lua
local events = require('haylen.events')
local system = require('haylen.system')
local window = require('haylen.window')

local function follow(enabled)
    window.setFrameRate(enabled and 30 or 0)
end

follow(system.powerSaving())
events.on('powerSavingChanged', function(change)
    follow(change.enabled)
end)
```

### system.timeZone()

Returns the IANA name of the time zone of the device, such as `'America/Sao_Paulo'`, or `nil` where the platform does not tell it. The event `timeZoneChanged` reports every change, such as a traveler who lands in another zone, and the local times that [`localization.formatDate`](localization.md) writes follow the new zone from that event on.

```lua
local events = require('haylen.events')
local system = require('haylen.system')

print('The clock of the app runs in ' .. (system.timeZone() or 'an unknown zone') .. '.')
events.on('timeZoneChanged', function(change)
    print('The device moved to ' .. change.timeZone .. '.')
end)
```

### system.openUrl(url)

Opens `url` with the app that the system picks for it: the browser for a web page, the store for a store link or the mail app for a `mailto:` link. It returns a Varn promise that resolves on a later frame, with nothing once an app took the url. A url that is not a string raises a bad argument error, and an empty url raises `Opening a url needs a url.`.

The promise rejects with a [failure](#failures) of the code `'unsupported'` when no app on the device opens the url, such as an unknown scheme, and of the code `'denied'` when the browser blocked the new tab, which it does without an input of the person shortly before, such as a click or a key. Every platform opens urls: macOS through the workspace, iOS, iPadOS, Mac Catalyst and tvOS through the application, Android with a view intent, Windows through the shell, Linux with `xdg-open` and the web in a new tab.

```lua
local async = require('async')
local system = require('haylen.system')

async.spawn(function()
    local _, message = system.openUrl('https://example.com/tiny-island'):await()
    if message then
        print(message)
    end
end)
```

### system.clipboard()

Reads the text on the clipboard of the system. It returns a Varn promise that resolves on a later frame with the text, or with an empty string when the clipboard holds no text. Some systems ask the person before an app reads text that another app wrote, and the promise waits for the answer. A refusal rejects with a [failure](#failures) of the code `'denied'` and the sentence that says why, since the clipboard has no permission of its own to check beforehand. The engine keeps up to 64 KiB of clipboard text on macOS, Windows, Linux and the Apple TV.

- iOS, iPadOS and visionOS ask the person whether the app may paste, or show their notice that the app pasted, as the settings of the app say.
- macOS 15.4 and later follow the setting of the pasteboard for the app, which may ask the person.
- Android gives the clipboard only to the app that has the focus.
- An Apple TV has no clipboard, so the text stays inside the app.
- Browsers read the clipboard only in a secure context, served over https or from localhost. Chromium asks the person the first time, and Safari and Firefox show a paste control at each read. Outside a secure context the promise rejects with the code `'unsupported'`.

A paste of the person into a field of the UI reads the clipboard on its own, without this function.

```lua
local async = require('async')
local system = require('haylen.system')

async.spawn(function()
    local code, message = system.clipboard():await()
    if not code then
        print(message)
    elseif #code > 0 then
        print('Pasted the invite code ' .. code .. '.')
    end
end)
```

### system.setClipboard(text)

Puts `text` on the clipboard of the system. It returns a Varn promise that resolves on a later frame, with nothing once the text reached the clipboard. A `text` that is not a string raises a bad argument error.

Desktops, phones, tablets and TVs take the text at once, and an Apple TV keeps it inside the app. A browser writes the clipboard only for a page served over https or from localhost, which has the focus and an input of the person shortly before, such as a click or a key. So a copy that a button or a key of the app asks for reaches the clipboard, while a copy without an input of the person rejects with a [failure](#failures) of the code `'denied'` and the reason of the browser. A page without the clipboard API, served over plain http from another machine, rejects with the code `'unsupported'`.

```lua
local async = require('async')
local system = require('haylen.system')

local seed = 184467
async.spawn(function()
    local _, message = system.setClipboard('Island seed: ' .. seed):await()
    if message then
        print('The seed shows on the screen instead, because ' .. message)
    end
end)
```

### system.vibrate(seconds)

Vibrates the device for `seconds`. It returns a Varn promise that resolves on a later frame, with nothing once the device vibrated. A number that is not positive raises `A vibration lasts a positive number of seconds.`.

- Android phones, and the browsers of phones with the Vibration API, vibrate for the time given.
- iPhones play a medium impact on their haptic engine, whatever the time.
- The other devices have no motor that apps reach, so the promise rejects with a [failure](#failures) of the code `'unsupported'` and the reason, such as `A Mac has no vibration motor.`.
- A browser vibrates only after an input of the person, such as a tap, and rejects with the code `'denied'` before.

On Android the app needs the permission `android.permission.VIBRATE`, which the manifest of the Android template declares. Without it the promise rejects with the code `'unsupported'` and `data.missing`, as [Android permissions](#android-permissions) describes. A vibration often plays on its own, such as on every hit, so the engine logs each reason of a refusal once and a rejection that nothing awaits never stops the app.

```lua
local system = require('haylen.system')

local function onHit()
    system.vibrate(0.08)
end

onHit()
```

### system.permission(name)

Reads the state of the [permission](#permissions) `name`, as far as the platform tells it, without asking the person. It returns a Varn promise that resolves on a later frame with the state, since some platforms, such as browsers, tell it only asynchronously. A name that is neither a permission of the engine nor one that a plugin of the app declares raises an error, such as `The permission "bluetooth" is neither one of the engine, which are "screens" and "localNetwork", nor one that a plugin of the app declares. A plugin declares the permissions it owns in the "permissions" of its "plugin.json".`.

| State | Meaning |
| --- | --- |
| `'granted'` | The app holds the permission, or the platform needs none for it. |
| `'prompt'` | The person was not asked yet, so [`system.requestPermission`](#systemrequestpermissionname) shows the prompt of the system. |
| `'denied'` | The person refused it, or the system no longer asks. A request then answers `'denied'` at once, and the person allows it in the settings of the system. |
| `'unknown'` | The platform does not tell the app until it tries, as Apple platforms do for the local network, and asks the person at the first use. |

A platform that has no such capability rejects with a [failure](#failures) of the code `'unsupported'` and the reason, such as `The browser cannot list every screen.`.

```lua
local async = require('async')
local system = require('haylen.system')

async.spawn(function()
    local state, reason = system.permission('screens'):await()
    if state == 'prompt' then
        print('The app explains why it wants every screen before it asks.')
    elseif not state then
        print(reason)
    end
end)
```

### system.requestPermission(name)

Asks the person for the [permission](#permissions) `name` with the prompt of the system, where the platform has one, and returns a Varn promise that resolves on a later frame with the state that the person left: `'granted'`, `'denied'`, `'unknown'`, or `'prompt'` when the system did not ask, such as Safari, which asks only during a tap or a click. A permission that the app holds already answers `'granted'` at once, without a prompt, and so does a platform that never asks.

The engine, or the plugin that owns the permission, first checks what the project of the app must declare for it: the usage description of the `Info.plist` on Apple platforms and the permission of the manifest on Android. Without it the request shows no prompt, logs once with the tag `haylen` what is missing and how to add it, and rejects with a [failure](#failures) of the code `'unsupported'`, whose `data.missing` lists what is missing as `{kind, name, file, snippet}`, as the [plugin guide](../platform/plugins.md#requirements) describes. A platform without the capability rejects with the same code and its reason. The app asked, so a rejection that nothing awaits never stops it.

```lua
local async = require('async')
local system = require('haylen.system')

async.spawn(function()
    local state, message, failure = system.requestPermission('localNetwork'):await()
    if failure then
        print(failure.code, message)
        return
    end
    if state == 'denied' then
        print('The app cannot find players on this Wi-Fi.')
    end
end)
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
| `powerSavingChanged` | The power saving mode of the system turned on or off. | `{enabled}`, the boolean that `system.powerSaving()` returns. |
| `timeZoneChanged` | The time zone of the device changed. | `{timeZone}`, the IANA name that `system.timeZone()` returns. |
| `systemLanguagesChanged` | The languages that the person prefers changed, such as when the person picked another language in the settings of the device. | `{languages}`, the list that `system.languages()` returns. |
| `mediaCommand` | A control of the system, a media key or a headset button asked the app to change what plays. It comes only once the app showed what plays with [`system.setNowPlaying`](#systemsetnowplayinginfo). | `{command}`, one of the names of `commands`, with `position` in seconds for `'seek'`. |

```lua
local events = require('haylen.events')

events.on('batteryChanged', function(battery)
    print(string.format('The battery is %s at %s.', battery.state, battery.level and math.floor(battery.level * 100) .. '%' or 'an unknown level'))
end)
```

## Permissions

A capability that at least one platform gates behind a permission, a prompt or a policy has a permission, so an app checks it with [`system.permission`](#systempermissionname) and asks for it with [`system.requestPermission`](#systemrequestpermissionname) the same way everywhere. A platform that never asks answers `'granted'`. The engine owns only the permissions that it uses itself.

| Name | What it gives the app |
| --- | --- |
| `'screens'` | Every screen of the device, with where it sits, for windows and outputs on other screens. |
| `'localNetwork'` | The devices of the local network, such as a development server on the computer of the developer, a game on the same Wi-Fi or a device of the home. |

Every other permission belongs to a plugin of the app, such as `'camera'`, `'location'`, `'motion'` or `'notifications'`. A plugin declares the permissions it owns in the `permissions` of its `plugin.json`, and one plugin of an app owns each name, as the [plugin guide](../platform/plugins.md#permissions) describes. For such a name the engine calls the native part of the plugin through the bridge: `<id>.permission` reads the state and `<id>.requestPermission` asks, both with the parameters `{name}`, and the answer settles the promise like the answer of a permission of the engine. A plugin without a native part on this platform rejects with the code `'unsupported'` and the sentence `The plugin "<id>" that owns the permission "<name>" offers no part on this platform.`, and a native part without the handler rejects with `The plugin "<id>" that owns the permission "<name>" offers no part on this platform that answers it.`. Any other failure of the native part rejects with the code `'unsupported'` and the sentence `The plugin "<id>" failed to answer the permission "<name>".` followed by its message. The reference of each plugin tells what each platform answers for its permissions.

```lua
local async = require('async')
local system = require('haylen.system')

-- The plugin "camera" of the app declares the permission "camera" in its "plugin.json".
async.spawn(function()
    local state, message, failure = system.requestPermission('camera'):await()
    if failure then
        print(failure.code, message)
    elseif state ~= 'granted' then
        print('The app shows the scanner once the person allows the camera.')
    end
end)
```

These tables tell what each platform answers for the permissions of the engine and when it asks the person.

| Name | iOS, iPadOS, Mac Catalyst, visionOS and macOS | tvOS |
| --- | --- | --- |
| `'screens'` | `'granted'`. | `'granted'`. |
| `'localNetwork'` | `'unknown'`, since the system asks the person the first time the app reaches the local network, with `NSLocalNetworkUsageDescription`, and tells the app nothing of it. | `'unknown'`. |

| Name | Android | Web | Windows | Linux |
| --- | --- | --- | --- | --- |
| `'screens'` | `'granted'`. | The Window Management API, with the prompt of `getScreenDetails`. A browser without it answers `'unsupported'`. | `'granted'`. | `'granted'`. |
| `'localNetwork'` | `ACCESS_LOCAL_NETWORK` for an app that targets Android 17, on Android 17 or later. Older systems and older targets answer `'granted'`. | The state of `local-network-access` in Chromium, where a request answers `'unknown'` while the browser would ask at the first access, and `'granted'` elsewhere. | `'granted'`. | `'granted'`. |

On Android a permission is `'denied'` once the engine asked for it before and Android no longer explains why the app wants it, which it does after a person refused twice. Browsers need a secure context, served over https or from localhost, for every screen, and a page in a frame needs the feature in the `allow` attribute of its frame. Without them the call rejects with the code `'unsupported'` and `data.missing`.

## Failures

A promise of this module that a platform or the person refuses rejects with the message, and with a table that `await` returns as its third value. Its field `error` holds the message, and `code` is one of these.

| Code | When |
| --- | --- |
| `'denied'` | The person or the platform refused the call, such as a clipboard that the person did not allow or a browser that wants an input of the person first. The message names the capability and how to ask for it again. |
| `'unsupported'` | The platform has no such capability, with the reason, or the project of the app lacks a requirement, which `data.missing` lists as `{kind, name, file, snippet}`. |

```lua
local async = require('async')
local system = require('haylen.system')

async.spawn(function()
    local text, message, failure = system.clipboard():await()
    if failure and failure.code == 'denied' then
        print(message)
    end
end)
```

## Platforms

This table tells where each platform reads the theme, the battery and what the device is. The [localization guide](../core/localization.md#where-the-languages-come-from) tells where each platform reads the languages.

| Platform | Theme | Battery | Device |
| --- | --- | --- | --- |
| macOS | The appearance of the app, with every change. | The power sources of the Mac, `'none'` on desktops without a battery. | The model, the processor, the memory and the version of the Mac. |
| iOS and iPadOS | The trait collection of the app, with every change. | The battery of the device, whose level the system updates about once a minute. | The model identifier, the version, the cores, the memory and the time zone. The simulator reports the model it simulates, and the processor and the memory of the Mac. |
| Mac Catalyst | The trait collection of the app, with every change. | The power sources of the Mac, `'none'` on desktops without a battery. | `'macOs'`, with the model, the processor, the memory and the version of the Mac. |
| tvOS | The trait collection of the app, with every change. | `'none'`, since TVs run on mains power. | The model identifier, the version, the cores and the memory. |
| Android | The night mode of the configuration, with every change. The activity handles the change itself, because the manifest of the template lists `uiMode` in its `configChanges`. | The sticky battery broadcast, heard while the activity lives, with every change. It is `'none'` on devices without a battery, such as TVs. A plugged battery that Android keeps from charging counts as discharging. | The model and its maker, the processor where Android 12 and later name it, the cores, the memory, the version and the time zone. A TV is a device in the television mode or with the leanback feature. A tablet is a device whose smallest screen side has 600 dp. A value that the device reports as unknown stays `nil`. |
| Web | The media query `prefers-color-scheme`, with every change. | The Battery Status API with every change, where the browser has it, which only Chromium browsers do. It is `'unknown'` elsewhere. A browser that cannot read a battery, as on a desktop without one, reports a full one. | What the browser tells, which it limits on purpose. That is the system it runs on with its version and the model of a phone through `navigator.userAgentData` where the browser has it, the cores, the memory that `navigator.deviceMemory` rounds and caps, and the time zone. The processor and the maker stay `nil`. |
| Windows | The app theme of the personalization settings, with every change. | The power status of the system, with every change that Windows announces, `'none'` on desktops without a battery. | The version of the kernel, the model and the maker of the computer and the processor from the registry, the cores, the memory and the time zone. The ICU of Windows 10 1903 and later gives the IANA name of the time zone. |
| Linux | The color scheme of the settings portal of the desktop, with every change, through GIO, which the engine loads at run time. Without GIO or the portal the theme stays `'light'` and never changes. | The first battery among the power supplies of the system, read when the app starts and every 30 seconds, `'none'` without one. | The distribution of `os-release` with the release of the kernel, the model and the maker of the computer, the processor, which ARM processors often leave out, the cores, the memory, and the time zone of `TZ` or `/etc/localtime`. |

This table tells where each platform reads the power saving mode and the time zone.

| Platform | Power saving | Time zone |
| --- | --- | --- |
| macOS, iOS, iPadOS and Mac Catalyst | The low power mode of `NSProcessInfo`, with every change. | The time zone of the system, with every change. |
| tvOS | `false`, since a TV has no low power mode. | The time zone of the system, with every change. |
| Android | The battery saver of the power manager, with every change while the activity lives. | The default time zone, and the broadcast of every change while the activity lives. |
| Web | `nil`, since browsers tell pages nothing about it. | The time zone of `Intl`, which the page reads again each time it becomes visible or its window takes the focus, since browsers announce no change. |
| Windows | The battery saver of the power status, which Windows 11 calls the energy saver, with every change. | The time zone of the system turned into its IANA name by the ICU of Windows, with every time change that the window hears. |
| Linux | The power saver profile of the power profile monitor of GIO, from GLib 2.70 on, with every change, and `nil` without it. | The zone of `TZ`, which stays for the process, or else the zone that `/etc/localtime` links to, with every change of the link that GIO sees. |

This table tells where each platform shows what plays, and which keys and buttons reach the app.

| Platform | Controls | Media keys and buttons |
| --- | --- | --- |
| iOS and iPadOS | The lock screen, the control center and the Dynamic Island, through the info center and the remote commands of MediaPlayer, which the engine loads at run time. The lock screen shows an app whose audio session plays. So an app that plays in the background sets `audio.iosSession` of `app.json` to `playback` and `audio.background` to `true`, as the [audio guide](../audio/audio.md#playing-in-the-background) describes. | Headsets, CarPlay and the other accessories. |
| tvOS | The now-playing screen of the Apple TV. | The play and pause button of the Siri Remote and of other remotes. |
| macOS and Mac Catalyst | The now-playing item of the control center and of the menu bar, with the playback state that the media keys follow. | The media keys of the keyboard and the buttons of headsets. |
| Android | The media notification of the shade and the lock screen, of a media session. Android 13 and later draw the controls from the commands. The notifications of a media session need no permission. The notification channel takes its name from the string `haylen_media_channel` of the resources of the app, `Playback` by default. In an app that [plays in the background](../audio/audio.md#playing-in-the-background), the notification of what plays keeps the app in the foreground while it shows as playing. | Headsets, Bluetooth devices and the media buttons of TV remotes. The button that toggles reaches the app as `'togglePlayPause'`. |
| Web | The media hub of the browser and the lock screen of phones, through the Media Session API. The browser shows them only while the page plays sound. | The media keys of the keyboard and of headsets. A browser that lacks an action leaves it out. |
| Windows | The media flyout of the volume and of the lock screen, through the transport controls of the window of the app. | The media keys of the keyboard and of headsets. |
| Linux | None. Linux desktops show the players that serve the MPRIS interface on the session bus, which the engine does not serve. So `setNowPlaying` changes nothing there. | None. |

The browser and Windows have no toggle button. So an app that offers `'togglePlayPause'` without `'play'` and `'pause'` receives `'togglePlayPause'` from their play and pause buttons.

## Android permissions

The engine library of Android declares no permission, so the manifest of the app declares the ones that its features need. The manifest of the Android template declares these four as visible defaults, and an app deletes the ones it does not use. The [permissions](#permissions) that plugins use, such as `android.permission.CAMERA`, come with the manifest of their module. A feature whose permission is missing never crashes the app. It logs once, with the tag `haylen`, what is missing and the line that adds it to `app/src/main/AndroidManifest.xml`, and goes on without it.

| Permission | Feature | Without it |
| --- | --- | --- |
| `android.permission.VIBRATE` | `system.vibrate`. | It rejects with the code `'unsupported'` and `data.missing`, which the log tells once as a warning. |
| `android.permission.ACCESS_NETWORK_STATE` | The network state of [`haylen.networkState()`](haylen.md#haylennetworkstate) and the events `networkOnline` and `networkOffline` of [`haylen.events`](events.md#engine-events), which the platform reports as `networkChanged`. | The engine never follows the network. The state stays `'unknown'` and the events never fire, which the log tells once at the info level. |
| `android.permission.INTERNET` | Network access: the `http` and `socket` modules of Varn and [`haylen.net`](net.md). | Connections fail. The `error` events of `haylen.net` sockets end with a sentence that names the missing permission and how to add it, while Varn reports what the system tells it. |
| `android.permission.ACCESS_LOCAL_NETWORK` | Reaching the devices of the local network from Android 17, for an app that targets it. The person grants it at run time, so the app asks with [`system.requestPermission('localNetwork')`](#systemrequestpermissionname), and the development session of a debug build asks by itself before it reaches a development server outside the device. | The request fails with the code `'unsupported'`. Android drops the packets of a connection to the local network, so `haylen.net` sockets and connects of the `socket` module fail at once with a sentence that names the permission, the requests of the `http` module that fail end with a sentence that says that access to the local network is not granted, and the development session says why it does not connect. |

The [plugin guide](../platform/plugins.md#requirements) describes how plugins check their own requirements the same way.

## C++

The function `engine.getSystem()` returns the `haylen::platform::System` of the running app, declared in `haylen/platform/System.hpp`.

| Member | Meaning |
| --- | --- |
| `getInfo()` | The `haylen::platform::SystemInfo`. Its enums `Os` and `DeviceKind` name the operating system and the kind of device. Its empty strings and zeros are the values that the platform does not report. |
| `getTheme()` | The `haylen::platform::Theme`, `Light` or `Dark`. |
| `getBattery()` | The `haylen::platform::Battery`, with its optional `level`, `charging` and `State`. |
| `getPowerSaving()` | A `std::optional<bool>`, empty where the platform does not tell. |
| `getTimeZone()` | The IANA name of the time zone, empty where the platform does not tell. |
| `getLanguages()` | The languages that the person prefers, most preferred first, in their canonical form. |
| `openUrl(url, callback)` | Opens the url and calls back on the frame thread, at a later frame, with a `haylen::platform::Permission::Answer`, whose state is `Granted`, or `Denied` or `Unsupported` with the sentence in `message`. |
| `getClipboard()` | The text on the clipboard for a paste of the person into a field of the UI. |
| `readClipboard(callback)` | Reads the text on the clipboard for the app and calls back at a later frame with an `Answer` and the text. |
| `setClipboard(text, callback)` | Puts the text on the clipboard and calls back at a later frame with an `Answer`. |
| `vibrate(seconds, callback)` | Vibrates the device and calls back at a later frame with an `Answer`. |
| `checkPermission(kind, callback)` | Reads the state of a `Permission::Kind` of the engine, `Screens` or `LocalNetwork`, and calls back at a later frame with an `Answer`, whose state is `Unsupported` with the reason where the platform has no such capability. |
| `requestPermission(kind, callback)` | Asks the person for a permission of the engine and calls back at a later frame with an `Answer`, whose `missing` lists what the project of the app lacks. |
| `checkPermission(name, callback)`, `requestPermission(name, callback)` | The same by the name of `system.permission`, for a permission of the engine or one that a plugin of the app declares, which the bridge asks the native part of the plugin for. They throw `std::invalid_argument` for a name that nobody owns, and `std::runtime_error` when the plugins of the app cannot be read. |

The functions `openUrl` and `vibrate` throw `std::invalid_argument` for the values that Lua rejects. The function `graphics::Device::getAdapterName()` names the GPU on its own.

```cpp
const haylen::platform::SystemInfo& info = engine.getSystem().getInfo();
haylen::core::Log::info("Running on {} with {} cores", haylen::platform::SystemInfo::osName(info.os), info.cpuCores);
engine.getSystem().openUrl("https://example.com", [](haylen::platform::Permission::Answer answer) {
    haylen::core::Log::info("The page {}", answer.state == haylen::platform::Permission::State::Granted ? "opened" : answer.message);
});
```
