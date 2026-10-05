# Haylen Orientation

Haylen Orientation is a Lua sample of the screen orientation and the design resolution: reading the orientation and hearing it change, locking it, a screen that lays itself out again for portrait and landscape, and the scaling policies that map the design resolution onto screens of every shape. A menu lists one test per feature, each test is a scene with a Back button, and Escape, the east or Back button of a gamepad, the Menu button of an Apple TV remote or the Back button of an Android device returns to the menu. The app starts in any orientation, so phones and tablets turn it freely.

| Test | What it shows |
| --- | --- |
| Orientation and its event | The function `window.orientation()` with the window size, the density and the visible design area, the `windowOrientationChanged`, `windowResized` and `windowSafeAreaChanged` events, and a phone that turns with the screen. |
| Locking the orientation | The function `window.lockOrientation` with `portrait`, `landscape` and `any`, fullscreen for browsers, and what Android, iPhone and iPad, browsers, desktops and TVs do with a lock. |
| Adaptive layout | A character screen that stacks its parts when the visible area is taller than wide and places them side by side otherwise, rebuilt with `document:replace`. |
| Design resolution and scaling | The live design size, policy, visible rectangle, pixel rectangle and pixels per unit of this app, and a preview of `fit`, `fill`, `stretch`, `expand` and `pixelPerfect` on a phone in both orientations, a tablet, an ultrawide monitor, a Full HD monitor and a small window, computed like the engine does. |

Desktop windows, Mac Catalyst windows and TVs always count as landscape, so on a desktop the orientation never changes and the lock does nothing. The adaptive layout follows the shape of the visible area, so a desktop window resized taller than wide shows the portrait layout too. The scaling policy of an app is `design.scaling` in its `app.json`, which this sample leaves at `expand`, so the preview shows the other policies on simulated screens.

## Controls

The mouse, touch, the keyboard, gamepads and TV remotes reach every control. Left and right change a focused stepper or segmented control, and the hint line of each test lists what it adds.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 haylen.py run interface/orientation` |
| macOS app | `python3 haylen.py run interface/orientation --platform macos` |
| iPhone and iPad simulator | `python3 haylen.py run interface/orientation --platform ios-simulator` |
| Apple TV simulator | `python3 haylen.py run interface/orientation --platform tvos-simulator` |
| Android device or emulator | `python3 haylen.py run interface/orientation --platform android` |
| Browser | `python3 haylen.py run interface/orientation --platform web` |

## Package layout

```text
orientation/
  app.json               Window, design resolution of 1920 by 1080 with expand, any orientation and identifier.
  source/
    main.lua             Adds the Back button of gamepads to uiCancel and opens the menu.
    tests.lua            The tests in menu order.
    sample.lua           The frame of every test with its Back button and hints, and the way back to the menu.
    scenes/menu.lua      The menu.
    tests/               One scene per test.
```
