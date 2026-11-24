# 📖 `haylen.system`

The module `haylen.system` tells what the device and its operating system are, and follows the theme, the battery, the power saving mode, the thermal state, the time zone and the languages of the system. It also offers services that every system has. It reads and writes the clipboard, whose calls the system may refuse, as the [failures](#failures) describe. Use it to know the languages the person prefers, to match the colors of the app to the system, and to do less work on a low battery, while the system saves power or while the device runs hot. Optional capabilities of the device, such as the camera, the location, opening links, vibrating, native dialogs or the controls of the system that show what the app plays, live in plugins with the permissions that they need, and the methods of the app and its plugins live in [`haylen.platform`](platform.md).

```lua
local system = require('haylen.system')
```

The platform reads what the device is once, when the first app of the process starts. It reads the theme, the battery, the power saving mode, the thermal state, the time zone and the languages whenever the system reports a change. The engine publishes each change once, at the start of the next frame, as the [events](#events) `systemThemeChanged`, `batteryChanged`, `powerSavingChanged`, `thermalStateChanged`, `timeZoneChanged` and `systemLanguagesChanged`.

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

### system.thermalState()

Returns how hot the device runs as the system judges it, so the app draws less before the system slows the device down to cool it, and its frames stay smooth. The event `thermalStateChanged` reports every change. The [platforms](#platforms) section tells where each platform reads it.

| State | Meaning |
| --- | --- |
| `'nominal'` | The device runs cool, and the app needs to do nothing. |
| `'fair'` | The device runs a little warm. An app can stop work that can wait. |
| `'serious'` | The device runs hot and the system slows it down. An app lowers its quality, such as fewer shadows, effects and particles, or a lower frame rate. |
| `'critical'` | The device runs so hot that the system slows it down a lot to cool it. An app does as little as it can until the state comes down. |
| `'unknown'` | The platform does not tell. |

```lua
local events = require('haylen.events')
local system = require('haylen.system')
local window = require('haylen.window')

local function follow(state)
    window.setFrameRate((state == 'serious' or state == 'critical') and 30 or 0)
end

follow(system.thermalState())
events.on('thermalStateChanged', function(change)
    follow(change.state)
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

Puts `text` on the clipboard of the system. It returns a Varn promise that resolves on a later frame, with nothing once the text reached the clipboard. A `text` that is not a string raises a bad argument error, and bytes of `text` that are not UTF-8 reach the clipboard as U+FFFD.

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

## Events

The engine publishes these changes on [`haylen.events`](events.md#engine-events), once per change, at the start of the frame after the system reported it. An app that starts reads the current values, and it hears only the later changes.

| Event | When | Data |
| --- | --- | --- |
| `systemThemeChanged` | The system switched between light and dark colors. | `{theme}`, which is `'light'` or `'dark'`. |
| `batteryChanged` | The level, the charging or the state of the battery changed. | The table that `system.battery()` returns. |
| `powerSavingChanged` | The power saving mode of the system turned on or off. | `{enabled}`, the boolean that `system.powerSaving()` returns. |
| `thermalStateChanged` | The device runs hotter or cooler. | `{state}`, the state that `system.thermalState()` returns. |
| `timeZoneChanged` | The time zone of the device changed. | `{timeZone}`, the IANA name that `system.timeZone()` returns. |
| `systemLanguagesChanged` | The languages that the person prefers changed, such as when the person picked another language in the settings of the device. | `{languages}`, the list that `system.languages()` returns. |

```lua
local events = require('haylen.events')

events.on('batteryChanged', function(battery)
    print(string.format('The battery is %s at %s.', battery.state, battery.level and math.floor(battery.level * 100) .. '%' or 'an unknown level'))
end)
```

## Failures

A promise of this module that a platform or the person refuses rejects with the message, and with a table that `await` returns as its third value. Its field `error` holds the message, and `code` is one of these.

| Code | When |
| --- | --- |
| `'denied'` | The person or the platform refused the call, such as a clipboard that the person did not allow or a browser that wants an input of the person first. The message names the capability and how to allow it again. |
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

This table tells where each platform reads the power saving mode, the thermal state and the time zone.

| Platform | Power saving | Thermal state | Time zone |
| --- | --- | --- | --- |
| macOS, iOS, iPadOS and Mac Catalyst | The low power mode of `NSProcessInfo`, with every change. | The thermal state of `NSProcessInfo`, with every change. | The time zone of the system, with every change. |
| tvOS | `false`, since a TV has no low power mode. | The thermal state of `NSProcessInfo`, with every change. | The time zone of the system, with every change. |
| Android | The battery saver of the power manager, with every change while the activity lives. | The thermal status of the power manager from Android 10 on, with every change while the activity lives. Light and moderate throttling count as `'fair'`, severe throttling as `'serious'`, and the statuses from critical to shutdown as `'critical'`. Older versions report none, so it stays `'unknown'`. | The default time zone, and the broadcast of every change while the activity lives. |
| Web | `nil`, since browsers tell pages nothing about it. | `'unknown'`, since browsers tell pages nothing about it. | The time zone of `Intl`, which the page reads again each time it becomes visible or its window takes the focus, since browsers announce no change. |
| Windows | The battery saver of the power status, which Windows 11 calls the energy saver, with every change. | `'unknown'`, since Windows tells apps nothing about it. | The time zone of the system turned into its IANA name by the ICU of Windows, with every time change that the window hears. |
| Linux | The power saver profile of the power profile monitor of GIO, from GLib 2.70 on, with every change, and `nil` without it. | `'unknown'`, since Linux offers apps no thermal state of the system. The thermal zones of the kernel give temperatures, but not how the system judges them. | The zone of `TZ`, which stays for the process, or else the zone that `/etc/localtime` links to, with every change of the link that GIO sees. |

## Android permissions

The engine library of Android declares no permission, so the manifest of the app declares the ones that its features need. The Android backend declares these three in the manifest of every app that it generates, and an app that does not use one names it in `platforms.android.manifest.removedPermissions` of `haylen.json`, which leaves it out of the merged manifest. The permissions that plugins use, such as `android.permission.CAMERA` or `android.permission.ACCESS_LOCAL_NETWORK`, come with the manifest of their module, and the plugins ask the person for them. A feature whose permission is missing never crashes the app. It logs once, with the tag `haylen`, what is missing, and goes on without it.

| Permission | Feature | Without it |
| --- | --- | --- |
| `android.permission.VIBRATE` | The rumble of gamepads, [`input.rumble`](input.md#inputrumbleindex-options). | The gamepad never rumbles, which the log tells once as a warning. |
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
| `getPowerSaving()` | A `std::optional<bool>`, empty where the platform does not tell. |
| `getThermalState()` | The `haylen::platform::ThermalState` of `haylen/platform/ThermalState.hpp`, `Unknown` where the platform does not tell. |
| `getTimeZone()` | The IANA name of the time zone, empty where the platform does not tell. |
| `getLanguages()` | The languages that the person prefers, most preferred first, in their canonical form. |
| `getClipboard()` | The text on the clipboard for a paste of the person into a field of the UI. |
| `readClipboard(callback)` | Reads the text on the clipboard for the app and calls back on the frame thread, at a later frame, with a `haylen::platform::SystemAnswer` of `haylen/platform/SystemAnswer.hpp` and the text. The state of the answer is `Granted`, or `Denied` or `Unsupported` with the sentence in `message`. |
| `setClipboard(text, callback)` | Puts the text on the clipboard and calls back at a later frame with a `SystemAnswer`. |

Each callback may be empty when the caller does not want the answer, which then reaches nobody.

The function `graphics::Device::getAdapterName()` names the GPU on its own.

```cpp
const haylen::platform::SystemInfo& info = engine.getSystem().getInfo();
haylen::core::Log::info("Running on {} with {} cores", haylen::platform::SystemInfo::osName(info.os), info.cpuCores);
engine.getSystem().setClipboard(info.deviceModel, [](haylen::platform::SystemAnswer answer) {
    haylen::core::Log::info("The copy {}", answer.state == haylen::platform::SystemAnswer::State::Granted ? "landed" : answer.message);
});
```
