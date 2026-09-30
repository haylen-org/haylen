# Haylen Platform

A Lua sample with one scene per part of [haylen.platform](../../../docs/lua-api/platform.md), the [platform bridge guide](../../../docs/platform_bridge.md), what [haylen.window](../../../docs/lua-api/window.md) and [haylen.viewport](../../../docs/lua-api/viewport.md) report and what [haylen.system](../../../docs/lua-api/system.md) tells about the device, and the native dialogs of [haylen.dialogs](../../../docs/lua-api/dialogs.md). The menu lists the tests, each test opens as its own scene with a Back button, its live values and the calls it makes, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu.

| Test | What it shows |
| --- | --- |
| Native events | `platform-sample.tick` events from a ticker that native code runs on request and `platform-sample.activity` events that native code sends by itself, received through the handle of the plugin, an event sent from Lua with `platform.emit`, and the events the platform reports to the event hook of the scene. |
| Custom handler | `platform-sample.echo`, a method of the local plugin of this app answered by Java on Android, Objective-C on Apple platforms and JavaScript on the web, with its answer and its failure without a text. The desktop player reports the method as missing and explains why, and a Lua stand-in answers it there on request. |
| Window and device | The platform, the backend, the engine and app versions, the window and design sizes, the visible and safe areas with a drawing, the orientation, the pointer, fullscreen, the on-screen keyboard and the network, with the window, keyboard, network and app events as they happen and a simulated safe area. |
| System | Everything `system.info()` reports about the device, the theme and the battery with the `systemThemeChanged` and `batteryChanged` events as they happen, `system.openUrl` with a url that opens and one that no app takes, with the answer and the frames it took, and `system.vibrate` on a button. On an Android emulator, `adb shell cmd uimode night yes` and `no` switch the theme, and `adb shell dumpsys battery set level 37`, `set status 2` for charging or `3` for discharging and `reset` change the battery. |
| Dialogs | [haylen.dialogs](../../../docs/lua-api/dialogs.md): a warning message with three buttons, the picker of files to open, whose files the test reads at their paths, the picker of the destination of a save, which writes a small JSON report, the picker of a folder, and a message that the app gives up after two seconds, which the platform closes, each with its answer and the frames it took. Platforms without a picker answer with the code `unsupported`. |

## The native code of the app

The app keeps its native code in the local plugin [`plugins/platform-sample`](plugins/platform-sample), which its `app.json` lists, so the sample keeps no platform project and `make.py run` builds a copy of the template of each platform that it keeps in the build folder of the app, as the [distribution guide](../../../docs/distribution.md#platform-projects) describes.

```text
plugins/platform-sample/
  plugin.json                  The platforms of the plugin, the class of its Apple part, its Android module and its web module.
  source/init.lua              The Lua API that the tests load with require('platform-sample').
  apple/PlatformSamplePlugin.m The Objective-C plugin class, which answers platform-sample.echo and platform-sample.ticker and sends platform-sample.activity, for iOS, tvOS, Mac Catalyst and macOS.
  android/                     The Android module, whose Java PlatformSamplePlugin answers the same methods and sends platform-sample.activity when the activity resumes or pauses.
  web/platform-sample.js       The web module, which answers the same methods and sends platform-sample.activity when the tab shows or hides.
```

| Method or event | Contract |
| --- | --- |
| `platform-sample.echo` | Takes `{text}` and answers `{echo, characters, language, system}`, or fails with `The method "platform-sample.echo" needs a text.` when the text is missing or empty. |
| `platform-sample.ticker` | Takes `{count, interval}` in milliseconds, answers `{started, count, interval}` and then sends `count` `platform-sample.tick` events of `{count, total, source}`. |
| `platform-sample.activity` | Sent by native code with `{state, source}` when the activity resumes or pauses, the Apple app becomes active or resigns, or the tab shows or hides. |

The desktop player, `python3 make.py run system/platform` without a platform, runs the Lua of the app and nothing else, so `platform-sample.echo` and `platform-sample.ticker` fail with `No native handler is registered for "<method>".`, which the sample shows. A handler for the desktop is C++ code, `engine.getPlatform().registerHandler`, in a C++ app built with `haylen_add_app` such as [the embedding sample](../../cpp/embedding), or a native library of the plugin, and the Lua player loads neither for this plugin. `platform.registerHandler` answers a method with Lua instead, which the custom handler test offers there. The macOS app of `--platform macos` builds the Apple part of the plugin, so the Objective-C class answers there.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run system/platform` |
| macOS app | `python3 make.py run system/platform --platform macos` |
| iPhone and iPad simulator | `python3 make.py run system/platform --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run system/platform --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run system/platform --platform android --device <serial>` |
| Browser | `python3 make.py run system/platform --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Use the panel of a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
