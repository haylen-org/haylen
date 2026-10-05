# Haylen Particles

Haylen Particles is a Lua sample of the particle emitters of `haylen.particles2d`, built from Lua tables and from `.particles` effect files. A menu lists one test per effect or feature, each test is a scene with a Back button, and Escape, the B button of a gamepad, the Menu button of an Apple TV remote or the Back button of an Android device returns to the menu.

| Test | What it shows |
| --- | --- |
| Fire | Flames, embers and smoke from three emitters that follow the cursor, with a strength slider. |
| Smoke | Prewarmed chimney smoke bent by the wind, in three kinds. |
| Explosion | One-shot bursts of a flash, a shock ring, fireballs, debris and smoke. |
| Rain | Streaks from a rectangle above the view and splashes along the ground. |
| Snow | A far and a near layer of flakes drifting in the wind. |
| Sparks | Welding sparks from a cone with gravity and damping, optionally only while held. |
| Trails | Trails in world space behind the cursor and a comet. |
| Magic | A vortex born on a ring with radial and tangential acceleration and twinkling stars. |
| Confetti | Colored confetti whose frames flip as it falls. |
| Fireworks | Rockets with trails that burst into stars and crackles. |
| Emitter shapes | Point, circle, ring, rectangle and cone spawn areas side by side. |
| Bursts and prewarm | Scheduled bursts, a one-shot cycle and a prewarmed plume next to a cold one. |
| Local and world space | Two circling emitters that differ only in `localSpace`. |
| Gravity and accelerations | Gravity, radial and tangential acceleration and damping on sliders. |
| Color, size and frames | Colors and sizes over the lifetime and frame animation. |
| Blend modes | Alpha, additive, multiply, screen and premultiplied blending over light and dark. |
| Effect files | Every `.particles` file of `content/effects`, loaded with `assets.load`. |
| Many particles | Four fountains of up to 25000 particles each with the live count. |

## Controls

Emitters that follow the cursor follow the mouse and the first finger, and WASD or the left stick move the cursor. A click, a tap, E or the X button fires what a test fires. Every setting of a test is a control in the panel on the right, which the mouse, touch, the arrow keys, the directional pad and a TV remote reach.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 haylen.py run samples/graphics/particles` |
| macOS app | `python3 haylen.py run samples/graphics/particles --platform macos` |
| iPhone and iPad simulator | `python3 haylen.py run samples/graphics/particles --platform ios-simulator` |
| Apple TV simulator | `python3 haylen.py run samples/graphics/particles --platform tvos-simulator` |
| Android device or emulator | `python3 haylen.py run samples/graphics/particles --platform android` |
| Browser | `python3 haylen.py run samples/graphics/particles --platform web` |

## Package layout

```text
particles/
  app.json               Window, design resolution of 1920 by 1080 and identifier.
  source/
    main.lua             Loads the action map and opens the menu.
    tests.lua            The tests in menu order.
    sample.lua           The base scene of every test with its header and Back button, and the cursor.
    art.lua              The particle images, their frames, the backdrop and labels.
    scenes/menu.lua      The menu.
    tests/               One scene per test.
  content/
    input/actions.json   The action map.
    images/              Particle images generated for the sample: soft dots, sparks, smoke frames, drops, flakes, stars, rings, confetti and sparkle frames.
    effects/             The .particles files of the effect files test.
```
