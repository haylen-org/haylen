# Haylen Preferences

A Lua sample with one scene per way an app keeps the choices of the player between sessions with [haylen.preferences](../../../docs/lua-api/preferences.md). The menu lists the tests, each test opens as its own scene with a Back button, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu.

| Test | What it shows |
| --- | --- |
| Keys and values | `preferences.set`, `get`, `has`, `remove`, `save` and `load` with dotted keys such as `profile.stats.coins`, values of every kind including lists and groups, the errors of a key with an empty part and of a key that passes through a value, and the preferences in memory next to `preferences.json` on disk with a badge that follows `preferences.dirty()`. |
| Engine settings | The volume and mute of every audio bus, fullscreen and a rebindable jump key changed live over a looping tune, stored with `preferences.capture()`, scrambled without saving and brought back with `preferences.apply()` or `preferences.load()`. |
| Settings screen | A complete settings screen with sliders, toggles, a segmented control, a language combo, a stepper, a text field and a key capture. Every change applies at once, Save writes the file and leaving the screen saves what is still unsaved, so the values come back after a restart. The labels are translations in English, Portuguese and Spanish, so picking a language relabels the screen live. |
| Reset to defaults | Every stored setting next to its default with the changed ones marked, a reset of the audio group alone and a full reset behind a confirmation dialog. |

`source/settings.lua` holds the defaults. At startup it loads the default action map, then applies the stored engine settings with `preferences.apply()` and the stored language, and the first launch stores the defaults. The tune and the click are generated tones made for this sample.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run samples/system/preferences` |
| macOS app | `python3 make.py run samples/system/preferences --platform macos` |
| iPhone and iPad simulator | `python3 make.py run samples/system/preferences --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run samples/system/preferences --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run samples/system/preferences --platform android --device <serial>` |
| Browser | `python3 make.py run samples/system/preferences --platform web` |

To see the settings persist, change them, close the app and run it again. The preferences live in `preferences.json` in the user folder of `dev.haylen.samples.preferences`, which the browser keeps in IndexedDB. The fullscreen setting shows on desktop and in the browser only, since phones and TVs are always fullscreen.

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test or a control | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Change a slider, a segmented control or a stepper | Left and right, or drag | Left and right on the directional pad | Drag or tap | Swipe left and right |
| Rebind jump | Focus the key field, press Enter, then the new key | South button, then the new button | Tap, then a key of an attached keyboard | Select, then a button |
| Jump, to try the binding | Space, or the new key | South button | None | None |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
