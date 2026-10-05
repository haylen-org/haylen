# Haylen Native

A Lua sample that calls the test library of the engine, `engine/tests/native/NativeTest.c`, on every platform through [`haylen.native`](../../../docs/lua-api/native.md) and Varn's `ffi`, and the platform handlers of the app through [`haylen.platform`](../../../docs/lua-api/platform.md). The [native code guide](../../../docs/native.md) explains each part. The menu lists the tests, each test opens as its own scene with a Back button and a list of checks that pass, fail or do not apply to the platform, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu. Every check also prints a `[native]` line to the log.

| Test | What it checks |
| --- | --- |
| Calls | The function `native.load` by name, integers, doubles, text, a struct by value, a struct by pointer, a buffer with its checksum, and `native.findSymbol` with a call through the address it returns. |
| Callbacks | A Varn `ffi.cast` callback and a `native.callback` with `thread = 'frame'` during the call, a callback at the next frame, and a callback from a thread of the library with a byte range bounded by a length parameter. |
| Library handlers | The init function of the library with `HaylenNativeApi`: an event and an answer from threads of the library, a typed error, a timeout and a cancel that the library hears about. |
| Static library | On iOS and tvOS, `native_test_static` linked into the app, `native.load` returning `ffi.C`, the symbol table and calls to the linked copy. |
| Platform handlers | Handlers of the local plugin `native-sample`: suspending Kotlin and a throwing Java handler on Android, async Swift on Apple platforms and JavaScript on the web, with a typed error, an exception that fails the call and cancels and timeouts that reach the handler. |

On the web, where the browser loads no native libraries, the local plugin `native-test` answers the same functions and handlers through the bridge, and the checks run against its answers.

## The native code of the app

```text
app.json                     The native section, with native_test, built from native/ for every platform, and native_test_static, linked into iOS and tvOS apps with its symbols, and the plugins section, which lists the two local plugins.
native/CMakeLists.txt        Builds native_test and native_test_static from the source of the engine tests.
plugins/native-sample/       The platform handlers: the Kotlin plugin class and the Java handler that throws of its Android module, the Swift plugin class of apple/ and the web module.
plugins/native-test/         The web module that answers the functions and handlers of the test library in the browser, where they are native-test.<name> instead of native_test.<name>.
```

The sample keeps no platform project, so `haylen.py run` builds a copy of the template of each platform that it keeps in the build folder of the app, as the [distribution guide](../../../docs/distribution.md#platform-projects) describes. The desktop player, `python3 haylen.py run system/native` without a platform, builds the library for this machine and passes its folder to the player with `--native`. It runs no native part of the plugins, so the platform handlers test reports that it does not apply there.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 haylen.py run system/native` |
| macOS app | `python3 haylen.py run system/native --platform macos` |
| iPhone and iPad simulator | `python3 haylen.py run system/native --platform ios-simulator` |
| Apple TV simulator | `python3 haylen.py run system/native --platform tvos-simulator` |
| Android device or emulator | `python3 haylen.py run system/native --platform android --device <serial>` |
| Windows and Linux | `python3 haylen.py run system/native --platform windows` or `--platform linux` on that host |
| Browser | `python3 haylen.py run system/native --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Run the checks again | Enter on the button, or click | South button | Tap | Select |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
