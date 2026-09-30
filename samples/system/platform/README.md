# Haylen Platform

A Lua sample with one scene per part of [haylen.platform](../../../docs/lua-api/platform.md), the [platform bridge guide](../../../docs/platform_bridge.md), what [haylen.window](../../../docs/lua-api/window.md) and [haylen.viewport](../../../docs/lua-api/viewport.md) report and what [haylen.system](../../../docs/lua-api/system.md) tells about the device. The menu lists the tests, each test opens as its own scene with a Back button, its live values and the calls it makes, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu.

| Test | What it shows |
| --- | --- |
| Native events | `sample.tick` events from a ticker that native code runs on request and `sample.activity` events that native code sends by itself, received with `platform.on`, an event sent from Lua with `platform.emit`, and the events the platform reports to the event hook of the scene. |
| Custom handler | `sample.echo`, a method of this app answered by Java on Android, Objective-C on Apple platforms and JavaScript on the web, with its answer and its failure without a text. The desktop player reports the method as missing and explains why, and a Lua stand-in answers it there on request. |
| Window and device | The platform, the backend, the engine and app versions, the window and design sizes, the visible and safe areas with a drawing, the orientation, the pointer, fullscreen, the on-screen keyboard and the network, with the window, keyboard, network and app events as they happen and a simulated safe area. |
| System | Everything `system.info()` reports about the device, the theme and the battery with the `systemThemeChanged` and `batteryChanged` events as they happen, `system.openUrl` with a url that opens and one that no app takes, with the answer and the frames it took, and `system.vibrate` on a button. |

## The native code of the app

The app keeps its native code in `platform/`, which `make.py run` lays over the template of each platform, as the [distribution guide](../../../docs/distribution.md#platform-overrides) describes.

```text
platform/
  android/app/app.gradle                                          Names PlatformSampleApplication through the haylenApplication manifest placeholder.
  android/app/src/main/java/dev/haylen/samples/platform/          PlatformSampleApplication registers SamplePlugin in onCreate, which answers sample.echo and sample.ticker and sends sample.activity.
  apple/source/main.mm                                            SamplePlugin registers the same methods and events before haylen_main, for iOS, tvOS, Mac Catalyst and macOS.
  web/app.js                                                      Registers the same methods in Module.preRun and sends sample.activity when the tab shows or hides.
```

| Method or event | Contract |
| --- | --- |
| `sample.echo` | Takes `{text}` and answers `{echo, characters, language, system}`, or fails with `sample.echo needs a text.` when the text is missing or empty. |
| `sample.ticker` | Takes `{count, interval}` in milliseconds, answers `{started, count, interval}` and then sends `count` `sample.tick` events of `{count, total, source}`. |
| `sample.activity` | Sent by native code with `{state, source}` when the activity resumes or pauses, the Apple app becomes active or resigns, or the tab shows or hides. |

The desktop player, `python3 make.py run system/platform` without a platform, runs the Lua of the app and nothing else, so `sample.echo` and `sample.ticker` fail with `No native handler is registered for <method>.`, which the sample shows. A handler for the desktop is C++ code, `engine.getPlatform().registerHandler`, in a C++ app built with `haylen_add_app` such as [the embedding sample](../../cpp/embedding), and the Lua player cannot load one. `platform.registerHandler` answers a method with Lua instead, which the custom handler test offers there. The macOS app of `--platform macos` builds the Apple template with `main.mm`, so the handlers of the app answer there.

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
