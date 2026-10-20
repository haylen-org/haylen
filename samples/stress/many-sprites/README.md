# Many Sprites

Many Sprites is a stress sample of sprite drawing: a pond where a school of tens to hundreds of thousands of fish swims around a bait. Every fish is a sprite with its own position, heading and animation frame, all of them move every frame, and the whole school draws in one call. The panel adds and removes fish ten thousand at a time and switches how many fish move each frame and how their sprites reach the renderer, so the cost of each part shows in the readout while the school swims.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/stress/many-sprites` |
| macOS app | `haylen run samples/stress/many-sprites --platform macos` |
| iPhone and iPad simulator | `haylen run samples/stress/many-sprites --platform ios-simulator` |
| Apple TV simulator | `haylen run samples/stress/many-sprites --platform tvos-simulator` |
| Android device or emulator | `haylen run samples/stress/many-sprites --platform android --device <serial>` |
| Browser | `haylen run samples/stress/many-sprites --platform web` |
| Headless player | `build/macos-debug/bin/haylen-headless samples/stress/many-sprites --frames 900` |

On the headless platform the app runs a short tour by itself: it adds fish, goes through every simulation and hand-over mode, empties the pond, fills it again, logs the frame numbers of every step and quits.

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Add or remove 10,000 fish | Plus and minus, or the panel buttons | The right and left triggers, or the shoulders while the pond has the focus | The panel buttons | The panel buttons |
| Change the simulation | N, or the panel | A click of the right stick, or the panel | The panel | The panel |
| Change the hand-over | M, or the panel | The north button while the pond has the focus, or the panel | The panel | The panel |
| Lead the school | Hold the left button on the pond, or the arrows and WASD while the pond has the focus | The right stick, and the left stick while the pond has the focus | Hold a finger on the pond | The directions while the pond has the focus |
| Move the focus between the pond and the panel | Tab | The View button | A tap on the pond or the panel | Play/Pause |

The panel starts with the focus, so a gamepad or a remote works it at once. Escape and the east button have nothing to close on this screen and do nothing, and on a TV the Menu or Back button leaves the app from it, as the platforms ask. A few seconds after the last input the bait drifts back to its loop through the pond.

## How it works

The simulation lives in plain Lua arrays: the sprite fields of every fish in one array, four numbers each, and its place on the ellipse it circles, its tail beat and its species in others. Each frame a fish moves a share of the way toward its point on the ellipse around the bait, turns toward it with `math.atan`, and picks the frame of its tail from a phase that runs faster the farther it is from its point. The two halves of the school circle on crossed ellipses that turn opposite ways.

The simulation modes decide how many fish move each frame:

| Mode | What runs |
| --- | --- |
| Every frame | Every fish moves every frame. |
| Staggered | The odd fish move in one frame and the even fish in the next, each by the time of two frames, which halves the Lua work and still looks smooth. |
| Frozen | Nothing moves and nothing is handed over, so the frame holds only what drawing costs. |

The hand-over modes decide how the sprites reach the renderer, the fast paths of the [performance section of the Lua guide](../../../docs/core/lua.md#performance):

| Mode | What runs |
| --- | --- |
| Buffer | One `buffer:set(1, sprites)` copies the array into a float buffer of [`haylen.collections`](../../../docs/lua-api/collections.md#float-buffers), and one `graphics2d.drawBatch(texture, buffer, layout)` draws the school with the fields `x`, `y`, `rotation` and `sourceX`. |
| Batch | The same copy moves a sprite batch with `batch:writeFields`, which keeps the size and the source of every sprite in C++, and `batch:draw` draws it. |
| Tables | Every fish is a Lua table that `graphics2d.drawBatch(texture, tables)` reads key by key, the slow way that one table per sprite costs. |

The readout shows the frame time, the frame rate and the draw calls of the engine, the fish, the sprites drawn, and the time of the Lua simulation and of the hand-over, which the profiler scopes `swarm` and `handOver` of [`haylen.debug`](../../../docs/lua-api/debug.md) measure.

## Numbers

The numbers come from the Release desktop player that `haylen engine` builds, on an Apple M5 Pro with macOS, in a window of 1600 by 900 points, 3200 by 1800 pixels on its display, with `vsync` off. Each load settles for 2 seconds and then samples 3 seconds. The average frame and the one percent low, the average of the slowest 1 percent of the frames, come from `debug.frame().milliseconds`, the time the engine spends on a frame, and the frames per second are the frames counted in the 3 seconds, which never pass the 120 Hz of the display. Other heavy work ran on the machine at the same time, with a load average from 21 to 30 and nearly no idle processor time, so the frame thread did not always get the fastest cores, and the numbers are a floor of what the machine does.

| Fish | Simulation | Hand-over | Average frame | One percent low | Frames per second | Lua simulation | Hand-over time |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 10,000 | Every frame | Buffer | 2.15 ms | 2.58 ms | 120 | 1.48 ms | 0.25 ms |
| 20,000 | Every frame | Buffer | 4.17 ms | 6.99 ms | 120 | 3.46 ms | 0.56 ms |
| 50,000 | Every frame | Buffer | 10.70 ms | 12.28 ms | 90 | 8.82 ms | 1.44 ms |
| 70,000 | Every frame | Buffer | 14.54 ms | 16.15 ms | 66 | 11.20 ms | 1.99 ms |
| 80,000 | Every frame | Buffer | 17.89 ms | 21.92 ms | 55 | 14.22 ms | 2.26 ms |
| 100,000 | Every frame | Buffer | 22.94 ms | 31.32 ms | 43 | 19.66 ms | 2.89 ms |
| 150,000 | Every frame | Buffer | 32.63 ms | 36.76 ms | 30 | 27.49 ms | 4.28 ms |
| 200,000 | Every frame | Buffer | 45.72 ms | 50.56 ms | 22 | 40.44 ms | 5.78 ms |
| 100,000 | Staggered | Buffer | 14.90 ms | 18.09 ms | 64 | 9.62 ms | 3.34 ms |
| 140,000 | Staggered | Buffer | 20.67 ms | 23.06 ms | 48 | 14.75 ms | 4.56 ms |
| 160,000 | Staggered | Buffer | 23.72 ms | 26.71 ms | 41 | 16.39 ms | 5.18 ms |
| 200,000 | Staggered | Buffer | 28.97 ms | 32.13 ms | 34 | 18.83 ms | 5.08 ms |
| 100,000 | Every frame | Batch | 23.95 ms | 26.78 ms | 41 | 16.03 ms | 3.03 ms |
| 100,000 | Every frame | Tables | 43.25 ms | 45.67 ms | 23 | 14.08 ms | 3.63 ms |
| 100,000 | Frozen | Buffer | 5.84 ms | 11.43 ms | 118 | 0.00 ms | 0.00 ms |
| 200,000 | Frozen | Buffer | 11.33 ms | 16.61 ms | 84 | 0.00 ms | 0.00 ms |
| 300,000 | Frozen | Buffer | 15.41 ms | 18.06 ms | 63 | 0.00 ms | 0.00 ms |
| 500,000 | Frozen | Buffer | 22.14 ms | 25.73 ms | 44 | 0.00 ms | 0.00 ms |

With every fish moving every frame, the pond holds 60 frames per second up to about 75,000 fish: 70,000 fish run at 66 frames per second and 80,000 at 55. The Lua simulation is most of the frame, about 0.15 to 0.2 milliseconds for every thousand fish, while one `buffer:set` hands a thousand fish to the renderer in 0.03 milliseconds and the whole school draws in one call. The staggered simulation halves the Lua work and holds 64 frames per second with 100,000 fish. Drawing alone, with the school frozen, holds 60 frames per second up to 300,000 fish.

At 100,000 fish the buffer and the batch cost about the same, 22.9 and 24.0 milliseconds, while the tables spend about 25 milliseconds more inside `drawBatch`, which reads a Lua table for every sprite. When the machine was quieter, the same school simulated 100,000 fish in 12 milliseconds and held 70 frames per second, so the Lua work depends on the cores the frame thread gets.

## Art

Every image of the sample is painted for it with the `codex` command line and its `$imagegen` skill in the style of [Tiny Island](../../games/tiny-island/README.md#art), whose art is the standard of the repository, as the [project rules](../../../AGENTS.md#samples) describe, then cut and scaled into place. The pond loads the images with the `linear` filter, because the art is painted in high definition and the screen scales it, and the atlas of the fish with mipmaps, because a fish draws at a fifth of its size. Every prompt carries the key words of this guide.

| Part | Rule |
| --- | --- |
| Look | Polished casual fantasy in high definition, chunky cute shapes and simple bold forms that read at a small size, seen from straight above. |
| Outline | A bold clean outline of even thickness in very dark navy `#1E1B33` around every fish, pad and blossom. The floor and the light have none. |
| Shading | Soft cel shading with two tones per material and one soft highlight, light from the top left, clean flat areas without noise or texture. |
| Palette | Water `#1A5961` with sand patches `#20666C` and hollows `#155058`, pebbles from `#245E63` to `#4D8C83` with highlights `#8CC3B5`, weeds `#2F8A63`. Ember fish `#F26B3A` with cream spots `#FFF1D6` and peach fins `#FFB38A`, silver fish `#8FA6BC` with a slate stripe `#4F6A85`, jade fish `#26B39A` with golden fins and stripe `#F2C14E`. Pads `#4FA34A` with veins `#3B8337` and gloss `#9AD982`, blossoms `#F7B8CF` and `#FFE3EE` around a golden heart `#F2C14E`. The light is white. |
| Fish | The top row of `content/sprites/pond.png`: 18 cells of 128 by 80 pixels, three species of six frames each, painted in one sheet, facing right, whose rear body and tail swing through one swim cycle, each species cut with one box so its head keeps its place. Every fish draws at 26 by 16 design units. |
| Lily pads | Below the fish in the same atlas, the pad of 256 pixels at `0, 84` and the blossom of 128 at `260, 84`, drawn at 130 to 240 design units, a blossom at 45 percent of its pad. |
| Pond | `content/pond/pond-floor.png` and `content/pond/caustics.png`: seamless tiles of 512 pixels. The light is white, and its brightness is its alpha. Parallax layers repeat the floor at 256 units and the light at 320 and 460 units, drifting and drawn additive at about 10 and 14 percent. |

## Package layout

```text
many-sprites/
  app.json               Window, design resolution of 1920 by 1080 in landscape and identifier.
  source/
    main.lua             Sets the pond theme of the panel and opens the pond.
    scenes/pond.lua      The pond: textures, the bait, input, the panel and the drawing.
    swarm.lua            The school: its arrays, the simulation modes and the hand-over modes.
    hud.lua              The panel with the readout, refreshed four times per second, the controls and a Quit button where quitting closes the app.
    tour.lua             The automatic run of the headless platform.
  content/
    sprites/pond.png     The atlas of the fish, the lily pad and the blossom.
    pond/                The floor and the light.
```
