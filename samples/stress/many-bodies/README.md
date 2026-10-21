# 🏋️ Many Bodies

Many Bodies is a stress sample of physics: a toy bin that three chutes pour thousands of toys into, blocks, balls, gems and pills, every one a Box2D body of its own shape. A blast throws the toys apart, the bin shakes like a table in an earthquake, and the whole pile draws from one sprite batch that two calls move each frame. The panel pours and removes toys a thousand at a time and shows the bodies, the awake bodies, the contacts and the time of every physics step.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/stress/many-bodies` |
| macOS app | `haylen run samples/stress/many-bodies --platform macos` |
| iPhone and iPad simulator | `haylen run samples/stress/many-bodies --platform ios-simulator` |
| Apple TV simulator | `haylen run samples/stress/many-bodies --platform tvos-simulator` |
| Android device or emulator | `haylen run samples/stress/many-bodies --platform android --device <serial>` |
| Browser | `haylen run samples/stress/many-bodies --platform web` |
| Headless player | `build/macos-debug/bin/haylen-headless samples/stress/many-bodies --frames 900` |

On the headless platform the app runs a short tour by itself: it pours, sets off blasts, shakes the bin, removes toys, empties the bin, pours again, logs the frame numbers of every step and quits.

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pour or remove 1,000 toys | Plus and minus, or the panel buttons | The right and left triggers, or the panel | The panel buttons | The panel buttons |
| Blast | B, a click on the bin, or the panel | The west button, or the panel | A tap on the bin, or the panel | The panel, or Play/Pause |
| Shake the bin | K, or the panel | A click of the right stick, or the panel | The panel | The panel |
| Zoom | The wheel over the bin, Q and E | The right stick up and down | A pinch on the bin | None, the view fits the bin |
| Fit the whole bin again | The panel | The panel | The panel | The panel |

The panel starts with the focus, so a gamepad or a remote works it at once. Escape and the east button have nothing to close on this screen and do nothing, and on a TV the Menu or Back button leaves the app from it, as the platforms ask. The Blast button and the blast key set off a blast at a random toy of the pile.

## How it works

The bin is one kinematic body made of the boxes of its floor, walls and lid, which leaves a gap under each chute. A shake moves that body with `body:moveTo` every fixed step along a fading sine, so the bin carries the pile and the solver shakes it for real, and the camera shakes with it. A blast is `physics2d.explode` of [`haylen.physics2d`](../../../docs/lua-api/physics2d.md), which pushes every toy within its radius at the point of the toy closest to the center, so toys hit off center spin. A toy that a blast throws out through a gap of the lid leaves the bin for good, which a sweep over the transforms finds twice a second.

The chutes drop a row of ten toys every second fixed step, each a block, a ball, a hexagonal gem or a capsule pill, whose shape matches its art exactly. Toys raise no contact, hit or sensor events, since nothing reads them, and a little spin damping with a sleep speed of a tenth of a meter per second lets a settled pile fall asleep, after which it costs nothing until a blast, a shake or a new toy wakes it. The world interpolates, steps at the fixed rate on the threads of the job system once enough of its bodies are awake, and the [physics guide](../../../docs/2d/physics.md) explains each of these choices.

The option `maxFrameTime` of `app.json` is 0.05 seconds, so a frame runs at most three fixed steps: past the limit of the machine the simulation slows down instead of every frame falling further behind.

Every toy is a sprite of one batch whose size and source the app sets once when the toy appears. Each frame, `world:readTransforms(bodies, buffer, 1, true)` copies the interpolated position and rotation of every body into a float buffer and `batch:writeFields(buffer, {'x', 'y', 'rotation'})` moves the sprites, so no Lua runs for each body, and the pile is one draw call.

## Numbers

The numbers come from the Release desktop player that `haylen engine` builds, on an Apple M5 Pro with macOS, in a window of 1600 by 900 points, 3200 by 1800 pixels on its display, with `vsync` off. Each load settles for 8 seconds, 14 for the pile at rest, and then samples 3 seconds. The average frame and the one percent low, the average of the slowest 1 percent of the frames, come from `debug.frame().milliseconds`, the time the engine spends on a frame, and the frames per second are the frames counted in the 3 seconds, which never pass the 120 Hz of the display. Other heavy work ran on the machine at the same time, with a load average from 21 to 30 and nearly no idle processor time, so the frame thread did not always get the fastest cores, and the numbers are a floor of what the machine does.

| Toys | State | Average frame | One percent low | Frames per second | Last physics step | Awake bodies | Contacts |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 3,000 | At rest | 3.34 ms | 3.73 ms | 120 | 0.00 ms | 0 | 9,882 |
| 3,000 | Shaking with a blast every half second | 3.40 ms | 3.83 ms | 120 | 2.02 ms | 3,001 | 9,351 |
| 6,000 | Shaking with a blast every half second | 5.43 ms | 12.59 ms | 119 | 6.12 ms | 6,001 | 20,848 |
| 8,000 | Shaking with a blast every half second | 4.27 ms | 9.96 ms | 120 | 5.95 ms | 8,001 | 28,647 |
| 10,000 | Shaking with a blast every half second | 6.55 ms | 13.64 ms | 116 | 11.74 ms | 10,001 | 36,842 |
| 12,000 | Shaking with a blast every half second | 7.35 ms | 15.93 ms | 106 | 8.78 ms | 12,001 | 45,375 |
| 14,000 | Shaking with a blast every half second | 9.39 ms | 17.98 ms | 89 | 11.68 ms | 14,001 | 54,335 |
| 15,000 | Shaking with a blast every half second | 9.93 ms | 21.35 ms | 87 | 9.59 ms | 15,001 | 58,619 |
| 16,000 | Shaking with a blast every half second | 9.51 ms | 15.87 ms | 89 | 11.05 ms | 16,000 | 63,157 |
| 17,000 | Shaking with a blast every half second | 10.16 ms | 19.32 ms | 85 | 11.79 ms | 17,001 | 68,035 |
| 18,000 | Shaking with a blast every half second | 15.44 ms | 34.61 ms | 61 | 13.00 ms | 18,001 | 72,339 |

The chutes pour the toys of each load while it settles, and every load but the pile at rest then shakes the bin and sets off a blast every half second, which keeps every toy awake. The bin holds 60 frames per second with every toy awake up to about 18,000 toys, and 100 frames per second up to 12,000. A settled pile of 3,000 toys sleeps, and its physics costs nothing until something wakes it.

The world steps 60 times per second, so at 120 frames per second every other frame pays a step, and the average frame can be shorter than one step. The last physics step is the time of the last step of each load, from `world:stats().stepMilliseconds`, which runs on the threads of the job system once enough bodies are awake.

## Art

Every image of the sample is painted for it with the `codex` command line and its `$imagegen` skill in the style of [Tiny Island](../../games/tiny-island/README.md#art), whose art is the standard of the repository, as the [project rules](../../../AGENTS.md#samples) describe, then cut and scaled into place. The playroom loads the images with the `linear` filter, because the art is painted in high definition and the screen scales it, and the atlas of the toys with mipmaps, because the bin is seen whole at a third of its size. Every prompt carries the key words of this guide.

| Part | Rule |
| --- | --- |
| Look | Polished casual fantasy in high definition, chunky glossy toy shapes and simple bold forms that read at a small size, seen straight from the side. |
| Outline | A bold clean outline of even thickness in very dark navy `#1E1B33` around every toy, the plank and the chute. |
| Shading | Soft cel shading with two tones per material and one soft glossy highlight, light from the top left, clean flat areas without noise or texture. |
| Palette | Four toy colors, each with its light and dark shade: red `#E8553F`, blue `#3D7FD9`, yellow `#F2B630` and green `#4DB36B`, with cream `#FFF6E8` faces and bands. The bin is wood `#C9884A` with grain `#9B5F2C` and nails `#7C4A22`, the chutes red, and the room a plum wallpaper of stripes `#3B2E5A` and `#43356A` with stars `#6A58A0`. |
| Toys | `content/room/playroom.png`: four rows of four colors, each toy of 128 pixels, or 128 by 64 for the pills, in cells 144 pixels apart, cut to its outline so the art fills its body. Blocks draw at 32 by 32 world units, balls at a diameter of 32, gems, which stand on a corner, 34 tall and pills at 44 by 22. |
| Bin | The plank of 192 pixels at `8, 584` of the same atlas, with borders of 40, which a nine-slice tiles over the floor, the walls and the lid, 90 world units thick. |
| Chutes | The funnel of 400 by 329 pixels at `208, 584` of the same atlas, whose neck is 0.35 of its width, drawn 971 world units wide so the neck covers each gap of the lid. |
| Room | `content/room/wallpaper.png`: a seamless tile of 512 pixels, repeated at 256 world units by a parallax layer at half the speed of the camera. |

## Package layout

```text
many-bodies/
  app.json               Window, design resolution of 1920 by 1080 in landscape and identifier.
  source/
    main.lua             Sets the playroom theme of the panel and opens the playroom.
    scenes/playroom.lua  The playroom: textures, the camera, input, blasts, the panel and the drawing.
    toy-bin.lua          The bin: the world, the chutes, the toys, the blast, the shake and the sprite batch.
    hud.lua              The panel with the readout, refreshed four times per second, the controls and a Quit button where quitting closes the app.
    tour.lua             The automatic run of the headless platform.
  content/
    room/                The atlas of the toys, the plank and the chute, and the wallpaper.
```
