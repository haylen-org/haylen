# Haylen Safe Area

Haylen Safe Area is a Lua sample of the safe area: the anchors of `haylen.ui` against the safe area and against the whole screen, an app that draws edge to edge under notches and home indicators while its controls stay safe, the debug overlay of the safe area and the device simulations of `haylen.viewport`. A menu lists one test per feature, each test is a scene with a Back button, and Escape, the east or Back button of a gamepad, the Menu button of an Apple TV remote or the Back button of an Android device returns to the menu.

Every test keeps its controls in a card in the middle of the safe area, with Back at the top left of the card, so the edges of the screen stay free for what the test shows. A desktop window and a browser have no notch, so there the sample starts with a simulated iPhone with a dynamic island. Phones, tablets and TVs keep their own safe area until the Device simulations test picks another one, and the sample turns with the device, since the insets of a phone change between portrait and landscape.

| Test | What it shows |
| --- | --- |
| Anchors | The 9 point presets and the 7 stretch presets of `anchor`, anchored with `anchorTo = 'safe'`, `'screen'` or both at once, with a margin from 0 to 96, over red bands that mark what lies outside the safe area. |
| Edge to edge | A world drawn over the whole visible screen, or boxed into the safe area to compare, a HUD whose column `ui.safeArea` keeps in the safe area inside a screen document, and a label anchored to the screen under the notch. |
| Debug overlay | `ui.setSafeAreaVisible` with the visible rectangle, the safe rectangle and the insets, and the `window_safe_area_changed` event as the device changes. |
| Device simulations | `viewport.setSafeAreaSimulation` with every simulated device and with custom insets in window points, switched while a HUD anchored to the safe area follows. |

## Controls

The mouse, touch, the keyboard, gamepads and TV remotes reach every control of the frame. Left and right change a focused stepper or segmented control, Enter or the south button opens a combo, and the hint line of each test lists what it adds.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run interface/safe-area` |
| macOS app | `python3 make.py run interface/safe-area --platform macos` |
| iPhone and iPad simulator | `python3 make.py run interface/safe-area --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run interface/safe-area --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run interface/safe-area --platform android` |
| Browser | `python3 make.py run interface/safe-area --platform web` |

To start any app with a simulated device and the overlay, add `"debug": {"safeArea": "iphoneNotch", "showSafeArea": true}` to its `app.json`.

## Package layout

```text
safe-area/
  app.json               Window, design resolution of 1920 by 1080, any orientation and identifier.
  source/
    main.lua             Adds the Back button of gamepads to ui_cancel, simulates an iPhone on desktops and opens the menu.
    tests.lua            The tests in menu order.
    sample.lua           The frame of every test, the way back to the menu, the simulated devices and the insets of the safe area.
    scenes/menu.lua      The menu.
    tests/               One scene per test.
```
