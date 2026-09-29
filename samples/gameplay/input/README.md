# Haylen Input

A Lua sample with one scene per feature of [haylen.input](../../../docs/lua-api/input.md), the [input guide](../../../docs/input.md) and the touch controls and key captures of [haylen.ui](../../../docs/lua-api/ui.md). The menu lists the tests, each test opens as its own scene with a Back button, its live values and the calls it makes, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu. The back press reaches the root of the document as a cancel, so it never leaves a test while a key capture listens or a popup is open.

| Test | What it shows |
| --- | --- |
| Keyboard | A keyboard drawn from the key names that lights the keys held and flashes the keys pressed and released, the modifiers, the text typed through `input.text()` and a log of the key and character events with their repeats. The on-screen keyboard opens from the panel on phones and TVs. |
| Mouse | The three buttons held, pressed and released, the wheel totals, the movement of each frame, the position in design units and framebuffer pixels, a zone for each cursor shape, a hidden cursor and a captured mouse that moves a crosshair by its movement alone. |
| Touch | Up to ten fingers with their id, phase and time down, where each landed and a trail that fades once it lifts. |
| Gestures | Taps, double taps, long presses, swipes and pinches marked where they happen, a card that pinches to zoom, and sliders for the thresholds of the recognizer. |
| Gamepads | A live card for each of the four gamepads with sticks, triggers, shoulders, d-pad and face buttons, the connect and disconnect events and the shared dead zone. Holding the east button for a second goes back, so a tap on it shows on the card. |
| Action map | The button, axis and vector actions of the sample with their bindings and live state from keys, the mouse, gamepads and touch controls at once, the press threshold, the gamepad the bindings read and prompts for the last device. |
| Remapping | A key capture for the keyboard or mouse binding and one for the gamepad binding of every gameplay action, kept in the preferences with `preferences.capture()`, which `main.lua` applies on the next launch. |
| Touch controls | A touch stick and two touch buttons that write virtual inputs, with their floating mode, radius, dead zone, size and touch-only option changed live. |
| Character | A small platformer hero with coyote time, a jump buffer, a variable jump and a dash that reads only actions, so every device drives it at the same time. |

The sample has no assets: everything is drawn with primitives. Its action map lives in `source/controls.lua`.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run gameplay/input` |
| macOS app | `python3 make.py run gameplay/input --platform macos` |
| iPhone and iPad simulator | `python3 make.py run gameplay/input --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run gameplay/input --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run gameplay/input --platform android --device <serial>` |
| Browser | `python3 make.py run gameplay/input --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Use the panel of a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Move, jump and dash | WASD or arrows, Space or left click, Left Shift or right click | Left stick or d-pad, south, west | Touch stick and buttons | Directional pad |
| Throttle | E and Q | Right and left triggers | | |
| Back to the menu | Escape or the Back button | East button, held for a second in the gamepad test | Back button | Menu |

The keyboard, gamepad, action map, touch control and character tests keep every key and gamepad button for themselves, so on devices with a pointer their panels answer the mouse and touch only, and a TV keeps focus navigation on them. The remapping test saves the whole action map in the preferences, and its Reset button brings the defaults back.
