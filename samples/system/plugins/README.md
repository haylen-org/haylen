# Haylen Plugins

A Lua sample that tests every capability of native plugins with its own plugin, [native-demo](plugins/native-demo), which is written with the APIs of each platform alone: Swift with UIKit and AppKit on Apple platforms, Kotlin with the views, dialogs and intents of Android, JavaScript with the DOM on the web and C with the threads and the windows of the system on the desktops. The [plugin guide](../../../docs/plugins.md) explains how plugins work, and its [walk-through of the demo plugin](../../../docs/plugins.md#demo-plugin-and-sample) is the reference for writing one. The menu lists the tests, each test opens as its own scene with a Back button, the rows of what it checks with their results and a panel of controls, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu. Every row that settles also prints a `[plugins]` line to the log.

`app.json` lists the plugin with its parameters and leaves `tickInterval` out on purpose, so its default shows:

```json
"plugins": {
    "native-demo": {"greeting": "Hello from app.json", "bannerColor": "#1D3557", "urlScheme": "haylendemo"}
}
```

`source/main.lua` loads the Lua API of the plugin, `require('native-demo')`, before it opens the menu, which tells the native part that a new app started. Where the native part does not run, as in the headless host of the engine tests, every test shows `The native part is not available on this platform.` instead of its checks.

## Tests

| Test | What it checks | How to run it |
| --- | --- | --- |
| Calls | `echo` answers on the main thread, `compute` counts the primes below 200000 on a background thread and says which thread ran it, `fail` answers with the code `demoFailure` and its data, and `wait`, which never answers by itself, ends with its timeout of 0.5 seconds and with `call:cancel()`, each time with `waitCancelled` from the native part, which heard it. | Opens with the calls running. Run the calls again repeats them. |
| Events | `loaded`, which the native part sent retained when it loaded, reaches this test, whose listener connects long after the menu opened, and `tick` events of a native timer arrive every `tickInterval` seconds and stop when the timer stops. `loaded` comes once per process, so after a restart from the error screen the test says so. | Start the native ticks, wait a few seconds and stop them. |
| Binary payloads | `echoBytes` sends every byte value from 0 to 255 and zeros, marked with `platform.bytes`, and the native part answers with the same bytes, and `generatedImage` answers with the bytes of a PNG file that the native part drew, which `graphics.newTexture` turns into the texture the test draws. | Opens with the checks running. Run the checks again repeats them. |
| Streams | The video stream `pattern`, which the native part draws 30 times per second, reaches the texture that the test draws, at least 60 frames of the size the native part gave, and the audio stream `tone`, a sine wave of 440 Hz, plays as a voice for two seconds with a level above 0.1 in the newest samples, which the test draws as a waveform and a meter with the count of underruns. Apple platforms and Android show that their streams come in a later version. | Start the video and the tone, watch and listen, and stop them. |
| Batched events | `burst` sends 100 events 30 times per second for 30 ticks, marked batched, and all 3000 arrive in order in about one list per frame, far fewer lists than events. | Opens with the bursts running. Send the bursts repeats them. |
| Configuration | The parameters in Lua, each with where its value comes from, `tickInterval` with the default of `plugin.json`, and the parameters that the native part received from its platform, which match. | Opens with the results. |
| Native banner | A native bar with the greeting and a Tap button over the app, at the bottom or the top of the safe area, reserving its edge or not. While it reserves, the safe area shrinks and this whole frame, laid out in the safe area, moves out of its way. The Tap button sends `bannerTapped`, and taps and clicks on the stage reach the app, which counts them and marks the last one. | Show the banner, tap its button, tap the stage around it, move it to the top, stop reserving its edge, hide it, show it again and remove it. |
| Covering native UI | A native screen over the whole app, which covers the app while it shows: `haylen.appCovered()` is true, which the render hook of the test sees while the engine keeps drawing the halted app, no update runs while covered, `appInactive` arrives when the screen shows and `appActive` once Close closes it, and the call answers with the seconds the screen showed. | Show the native screen, wait a moment and press Close. |
| Native screen | The confirm screen of the plugin, a popup page on the web and a native window over the window of the app on the desktops, answers the call with what the person picked. The engine covers the app before the screen shows, so `appInactive` finds `haylen.appCovered()` and `platform.screenShowing()` true, and no update runs and no frame is drawn under the opaque screen. A second screen fails with `busy`, and a cancel after a second closes the screen and ends the cover. A screen that shows while the app restarts with `haylen.requestRestart()`, and the redirect screen of the web, whose page loads again, reach the next app as `screenRestored` with the state the app gave. Apple platforms and Android show that their screens come in a later version. | Open the screen and answer it, open and cancel after a second, open and restart the app, answer the screen and open the test again, and on the web open by redirect and answer the page. |
| Native result | The file picker of the platform answers with the name of the picked file, and with `nil` when the person cancels. | Pick a file, then pick again and cancel. |
| Opened URLs | Every URL with the scheme `haylendemo` that opened the app, before it started or while it ran, received as the retained event `urlOpened`, so the URL that launched the app and the ones that arrived while the test was closed show when it opens. | Open `haylendemo://hello` as the table below shows, with the app running or closed, and open the test. |
| App errors | A button raises a Lua error on purpose and the error screen shows it. The native part receives the error in its app error hook and keeps the message, and when the app restarts from the error screen it sends the message back as the retained event `lastError`, which the test shows. | Raise a Lua error, restart the app from the error screen with R, Restart app or the south or select button, and open the test again. |
| Plugin info | `platform.plugins()` lists `native-demo` with its version and `native`, true where the native part runs and false in the headless host. | Opens with the results. |

## What each platform runs

| Where | Native part | Not available there |
| --- | --- | --- |
| iPhone and iPad, Mac Catalyst | Swift in `plugins/native-demo/apple` | The streams and the screens of plugins, which the Apple runtime brings in a later version of the engine. |
| Apple TV | Swift | `pickFile`, since tvOS has no file picker, the streams and the screens of plugins. The views over the app never take the focus, so the remote cannot press the Tap button of the banner. |
| macOS app | Swift with AppKit | The streams and the screens of plugins. |
| Android | Kotlin in `plugins/native-demo/android` | The streams and the screens of plugins, which the Android library brings in a later version of the engine. Gamepads and TV remotes cannot press the Tap button of the banner, whose panel never takes the focus. |
| Browser | JavaScript in `plugins/native-demo/web` | The browser opens a file chooser and a popup only right after a click, a tap or a key press, so a gamepad cannot open them. |
| Desktop player, Windows and Linux apps | C in `plugins/native-demo/native` | The banner, the covering native screen, the file picker, the native view of the parameters and opened URLs, which fail with the code `unsupported` or never arrive, since the desktops place no views of native libraries over the app. The confirm screen is a window of the library. |

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run system/plugins` |
| macOS app | `python3 make.py run system/plugins --platform macos` |
| Mac Catalyst app | `python3 make.py run system/plugins --platform catalyst` |
| iPhone and iPad simulator | `python3 make.py run system/plugins --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run system/plugins --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run system/plugins --platform android --device <serial>` |
| Windows and Linux | `python3 make.py run system/plugins --platform windows` or `--platform linux` on that host |
| Browser | `python3 make.py run system/plugins --platform web` |

## Opening URLs

| Where | Command |
| --- | --- |
| iPhone, iPad and Apple TV simulators | `xcrun simctl openurl booted haylendemo://hello` |
| macOS and Mac Catalyst apps | `open -a <path of the .app> haylendemo://hello` |
| Android | `adb shell am start -a android.intent.action.VIEW -d haylendemo://hello` |
| Browser | Add a hash to the address, such as `http://localhost:8000/#hello`, before loading or while the app runs. |

A URL that launches a closed app waits in the native part until the app starts, and then in the bridge until the Opened URLs test listens.

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Use the panel of a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Close the native screen | Enter or click on Close, or Escape on the web | Directional pad and south button | Tap Close | Select on Close, or Menu |
| Answer the confirm screen | Click Confirm, Decline or Close, Return for Confirm and Escape for Close on macOS | | Tap Confirm or Decline | |
| Restart from the error screen | R, or click Restart app | South button on Restart app, which has the focus | Tap Restart app | Select on Restart app, which has the focus |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
