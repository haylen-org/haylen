# Haylen Scenes

Haylen Scenes is a Lua sample of the scene stack of `haylen.scene`: every transition effect, a custom effect, loading with progress and errors, the stack operations with every hook, transparent overlays and the game pause with process modes. A menu lists one test per feature, each test is a scene with a Back button, and Escape, the B button of a gamepad, the Menu button of an Apple TV remote or the Back button of an Android device returns to the menu.

| Test | What it shows |
| --- | --- |
| Transition gallery | The 24 built-in effects, each pushing a card with the chosen direction, easing, duration and color, and popping it back with the opposite direction. |
| Custom effect | Blinds and a curtain written in Lua as effect tables that draw both scenes. |
| Loading and errors | A fade that holds its covered frame as the loading screen, a custom loading view with the progress and message of the load, `scene.preload` with `scene.loadProgress` drawn by the test, and a load that fails and reaches `onError`, which routes to a card with the message. |
| Stack and hooks | Push, pop, replace, `popTo` and `popToRoot` with instant, slide or fade transitions, the stack listed on screen and every hook logged, from `load` to `unload`, with the `params` of each change. |
| Transparent overlays | HUD and dialog overlays that keep the world rendering below while only the top scene updates. |
| Pause and process modes | A paused world whose pausable timer and tween stop, a pause menu in `whenPaused`, and an `always` timer that keeps counting. |

## Controls

Every operation of a test is a button in the panel on the right, which the mouse, touch, the arrow keys, the directional pad and a TV remote reach. Escape or the B button pops the top card or overlay, P or the Y button opens the pause menu, and Back returns to the menu from any depth of the stack.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 haylen.py run samples/graphics/scenes` |
| macOS app | `python3 haylen.py run samples/graphics/scenes --platform macos` |
| iPhone and iPad simulator | `python3 haylen.py run samples/graphics/scenes --platform ios-simulator` |
| Apple TV simulator | `python3 haylen.py run samples/graphics/scenes --platform tvos-simulator` |
| Android device or emulator | `python3 haylen.py run samples/graphics/scenes --platform android` |
| Browser | `python3 haylen.py run samples/graphics/scenes --platform web` |

## Package layout

```text
scenes/
  app.json               Window, design resolution of 1920 by 1080 and identifier.
  source/
    main.lua             Loads the action map and opens the menu.
    tests.lua            The tests in menu order.
    sample.lua           The base scene of every test with its header and Back button, and the way back from any depth.
    card.lua             The full-screen scene the tests push, which loads in steps and reports its hooks.
    scenes/menu.lua      The menu.
    tests/               One scene per test.
  content/
    input/actions.json   The action map.
```
