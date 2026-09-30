# haylen.system

`haylen.system` tells what the device and its operating system are, follows the theme of the system and the battery, and offers the services every system has for apps: opening a url with the app that handles it and vibrating the device. Use it to pick the language of a first launch, to match the colors of the app to the system, to lower the work of the app on a low battery, and for links to web pages, stores and mail. Native message boxes and file pickers live in [haylen.dialogs](dialogs.md), and the methods of the app and its plugins in [haylen.platform](platform.md).

```lua
local system = require('haylen.system')
```

The platform reads what the device is once, when the first app of the process starts, and the theme and the battery whenever the system reports a change. The engine publishes each change once, at the start of the next frame, as the `systemThemeChanged` and `batteryChanged` [events](#events).

## Functions

### system.info()

Returns a new table that describes the device. A value that the platform does not report is `nil`, never a guess, so code checks each value it relies on.

| Field | Type | Meaning |
| --- | --- | --- |
| `os` | string | The operating system: `'macOs'`, `'windows'`, `'linux'`, `'ios'`, `'ipadOs'`, `'tvOs'`, `'android'` or `'web'`. Mac Catalyst apps run on `'macOs'`. |
| `osVersion` | string or `nil` | The version of the operating system, such as `'15.5'` on macOS, `'18.5'` on iOS, `'15'` on Android, `'10.0.26100'` on Windows or `'Ubuntu 24.04.1 LTS (Linux 6.8.0-45-generic)'` on Linux. On the web it is the system that runs the browser with its version, such as `'macOS 15.5.0'`, where the browser tells it. |
| `deviceModel` | string or `nil` | The model identifier, such as `'Mac15,11'`, `'iPhone16,1'` or `'Pixel 9'`. |
| `manufacturer` | string or `nil` | The maker of the device, such as `'Apple'` or `'Google'`. |
| `deviceKind` | string | `'desktop'`, `'phone'`, `'tablet'`, `'tv'` or `'browser'`. |
| `cpuName` | string or `nil` | The name of the processor, such as `'Apple M3 Pro'`. |
| `cpuCores` | integer or `nil` | The number of logical processor cores. |
| `memoryBytes` | integer or `nil` | The total memory of the device in bytes. |
| `gpuName` | string or `nil` | The GPU the graphics device of the engine runs on, as its backend names it: the Metal device, the adapter of Direct3D 11, the `GL_RENDERER` string of OpenGL, the renderer that `WEBGL_debug_renderer_info` unmasks on WebGL 2 where the browser offers it, and the adapter info of WebGPU, whose description the vendor and the architecture replace where the browser leaves it empty. |
| `locale` | string or `nil` | The language of the user as a BCP 47 tag, such as `'pt-BR'`. |
| `languages` | table | Every language the user prefers, most preferred first, as BCP 47 tags. It is empty where the platform does not report them. |
| `timeZone` | string or `nil` | The IANA name of the time zone, such as `'America/Sao_Paulo'`. |

```lua
local localization = require('haylen.localization')
local system = require('haylen.system')

local info = system.info()
print(string.format('%s %s on a %s', info.os, info.osVersion or '', info.deviceKind))
if info.locale then
    localization.setLanguage(localization.findBestMatch(info.locale) or 'en')
end
```

### system.theme()

Returns `'light'` or `'dark'`, the colors the system shows as the app last heard at the start of a frame. The `systemThemeChanged` event reports every change.

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

Returns a new table with the battery as the app last heard of it at the start of a frame. The `batteryChanged` event reports every change.

| Field | Type | Meaning |
| --- | --- | --- |
| `level` | number or `nil` | The charge from 0 to 1, `nil` where the platform does not tell it. |
| `charging` | boolean | Whether the battery charges. |
| `state` | string | `'charging'`, `'discharging'`, `'full'`, `'none'` for a device without a battery, or `'unknown'` where the platform does not tell it. |

```lua
local system = require('haylen.system')

local battery = system.battery()
if battery.state == 'discharging' and battery.level and battery.level < 0.2 then
    print('the battery is low, so the app draws fewer particles')
end
```

### system.openUrl(url)

Opens `url` with the app the system picks for it, such as the browser for a web page, the store for a store link or the mail app for a `mailto:` link, and returns a Varn promise that resolves with `true` when an app took the url and `false` otherwise, on a later frame. A url that is not a string raises a bad argument error, and an empty url raises `Opening a url needs a url.`.

```lua
local async = require('async')
local system = require('haylen.system')

async.spawn(function()
    if not system.openUrl('https://example.com/tiny-island'):await() then
        print('no app opens web pages on this device')
    end
end)
```

### system.vibrate(seconds)

Vibrates the device for `seconds` where it can vibrate and does nothing elsewhere. Android phones and the browsers of phones with the Vibration API vibrate for the time given, iPhones play a medium impact on their haptic engine whatever the time, and the other devices do nothing. On Android the app needs the permission `android.permission.VIBRATE`, which the manifest of the Android template declares, and without it `vibrate` logs once what is missing and how to add it and does nothing, as [Android permissions](#android-permissions) describes. A number that is not positive raises `A vibration lasts a positive number of seconds.`.

```lua
local system = require('haylen.system')

local function onHit()
    system.vibrate(0.08)
end

onHit()
```

## Events

The engine publishes the changes on [haylen.events](events.md#engine-events), once per change at the start of the frame after the system reported it. An app that starts reads the current values and hears only later changes.

| Event | When | Data |
| --- | --- | --- |
| `systemThemeChanged` | The system switched between light and dark colors. | `{theme}`, `'light'` or `'dark'`. |
| `batteryChanged` | The level, the charging or the state of the battery changed. | The table `system.battery()` returns. |

```lua
local events = require('haylen.events')

events.on('batteryChanged', function(battery)
    print(string.format('battery %s at %s', battery.state, battery.level and math.floor(battery.level * 100) .. '%' or 'an unknown level'))
end)
```

## Platforms

| Platform | Theme | Battery | Device |
| --- | --- | --- | --- |
| macOS | The appearance of the app, with every change. | The power sources of the Mac, `'none'` on desktops without a battery. | The model, the processor, the memory, the version and the languages of the Mac. |
| iOS and iPadOS | The trait collection of the app, with every change. | The battery of the device, whose level the system updates about once a minute. | The model identifier, the version and the languages. |
| Mac Catalyst | The trait collection of the app, with every change. | The power sources of the Mac where the system offers them. | The same values as iOS, describing the Mac. |
| tvOS | The trait collection of the app, with every change. | `'unknown'`, since TVs have no battery the app sees. | The model identifier and the version. |
| Android | The night mode of the configuration, with every change. | The sticky battery broadcast, with every change. | The model and its maker, the memory, the cores, the version and the languages. |
| Web | The `prefers-color-scheme` media query, with every change. | The Battery Status API where the browser has it, which only Chromium browsers do, and `'unknown'` elsewhere. | What `navigator` exposes, which browsers limit on purpose, so most values stay `nil`. |
| Windows | The app theme of the personalization settings, with every change. | The power status of the system, with every change, `'none'` on desktops without a battery. | The processor from the registry, the memory, the version and the model of the computer. |
| Linux | The color scheme of the desktop portal, with every change. Without the portal the theme stays `'light'` and never changes. | The power supplies of the system, read every 30 seconds. | The processor, the memory, the distribution and the model of the computer. |

Every platform opens urls: macOS through the workspace, iOS, iPadOS, Mac Catalyst and tvOS through the application, Android with a view intent, Windows through the shell, Linux with `xdg-open`, and the web in a new tab, which a popup blocker may refuse.

## C++

`engine.getSystem()` returns the `haylen::platform::System` of the running app, declared in `haylen/platform/System.hpp`. `getInfo()` returns the `haylen::platform::SystemInfo`, whose `Os` and `DeviceKind` enums name the operating system and the kind of device and whose empty strings and zeros are the values the platform does not report, `getTheme()` returns the `haylen::platform::Theme`, `Light` or `Dark`, and `getBattery()` the `haylen::platform::Battery` with its optional `level`, `charging` and `State`. `openUrl(url, callback)` calls back on the frame thread with whether an app took the url, and `vibrate(seconds)` vibrates. Both throw `std::invalid_argument` for the values Lua rejects, and `graphics::Device::getAdapterName()` names the GPU on its own.

```cpp
const haylen::platform::SystemInfo& info = engine.getSystem().getInfo();
haylen::core::Log::info("Running on {} with {} cores", haylen::platform::SystemInfo::osName(info.os), info.cpuCores);
engine.getSystem().openUrl("https://example.com", [](bool opened) {
    haylen::core::Log::info("The page {}", opened ? "opened" : "did not open");
});
```
