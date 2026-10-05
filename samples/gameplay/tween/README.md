# Haylen Tween

A Lua sample with one scene per feature of [`haylen.tween`](../../../docs/lua-api/tween.md). The menu lists the tests, each test opens as its own scene with a Back button, the code it runs under its stage and the live values of its tweens, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu.

| Test | What it shows |
| --- | --- |
| To, from, by and fromTo | Four boxes that take their start and end values in the four ways. |
| Nested and several fields | Paths into nested tables and vector and color components, and five fields in one tween. |
| Colors in RGB and HSV | The same color change blended through RGB and through hue, with trails that paint the way each takes. |
| Shortest-path angles | A needle listed in the `angles` option that turns the short way, next to a plain number that unwinds the long way. |
| Counters and typewriter | A score that counts in whole numbers next to a plain number, and dialog lines revealed one character at a time. |
| Easing gallery | Every Penner family in its in, out and in-out forms, back and elastic with parameters, steps, cubic Bézier curves and curves by points. |
| Custom easing | Curves written as Lua functions, plotted and driving balls. |
| Timelines | A cutscene built with `append`, `join`, a pause, callbacks, a label and inserts, with a log of its steps and seeking to the label. |
| Nested timelines | Three child timelines inside a parent that yoyos forever, with the progress of each one. |
| Repeat modes | Restart, yoyo and incremental loops with a delay between them and a loop counter. |
| Playback controls | Play, pause, resume, restart, reverse, complete, kill and a seek slider on one tween, with its state and its callbacks counted. |
| Time scale | The speed of one tween, of the tweens of a tag and of the whole app, and a tween on real time. |
| Ready-made tweens | Move, scale, rotate, fade, tint, jump, path, Bézier, blink, shake and punch on sprites, animated natively. |
| Stagger | A hop and a bar growth that travel along a row from the start, the end or the center. |
| Overwrite mode | A ball sent to new targets, where overwrite kills the old tween and without it the tweens fight. |
| Scene and target lifetime | A child scene that owns a tween, and table and sprite targets whose tweens stop once they are collected. |
| Process modes | Tweens in every process mode and on real time, with the game pause and the time scale. |
| UI node tweens | A reward card whose nodes slide, pulse, tint, fade and shake through their transforms. |
| Stress test | Up to 20,000 sprites with one native tween each, the tween count and the frame time. |

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 haylen.py run samples/gameplay/tween` |
| macOS app | `python3 haylen.py run samples/gameplay/tween --platform macos` |
| iPhone and iPad simulator | `python3 haylen.py run samples/gameplay/tween --platform ios-simulator` |
| Apple TV simulator | `python3 haylen.py run samples/gameplay/tween --platform tvos-simulator` |
| Android device or emulator | `python3 haylen.py run samples/gameplay/tween --platform android --device <serial>` |
| Browser | `python3 haylen.py run samples/gameplay/tween --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Use the panel of a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Replay the test | R | West button | Replay button | Replay button |
| Pick a target or a curve | Click the stage | Stepper of the panel | Tap the stage | Stepper of the panel |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |

The panel of each test takes the focus when the test opens, so gamepads and TV remotes reach every control with the directional pad.
