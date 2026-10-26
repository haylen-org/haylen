# 🏋️ Streamed Map

Streamed Map is a stress sample of a large world: a map of 16,384 by 16,384 tiles, more than 268 million, made from a seed and streamed in chunks of 64 by 64 tiles around a camera that flies over it by itself or that the player drives. Chunks build in the background as the camera nears them, bake into static batches that draw with one call each, and unload once the camera leaves them far behind. Zooming out to see more than a million tiles at once shows the streaming at work, with the chunks that build lit in amber at the edge of the view.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/stress/streamed-map` |
| macOS app | `haylen run samples/stress/streamed-map --platform macos` |
| iPhone and iPad simulator | `haylen run samples/stress/streamed-map --platform ios-simulator` |
| Apple TV simulator | `haylen run samples/stress/streamed-map --platform tvos-simulator` |
| Android device or emulator | `haylen run samples/stress/streamed-map --platform android --device <serial>` |
| Browser | `haylen run samples/stress/streamed-map --platform web` |
| Headless player | `build/macos-debug/bin/haylen-headless samples/stress/streamed-map --frames 900` |

On the headless platform the app runs a short tour by itself: it flies at several zooms and build budgets, starts a new world, logs the frame numbers of every step and quits.

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Fly the camera | The arrows or WASD while the map has the focus, or a drag on the map | The left stick while the map has the focus | A drag on the map | The directions while the map has the focus |
| Zoom | The wheel over the map, Q and E, plus and minus, or the panel | The right and left triggers, or the panel | A pinch on the map, or the panel | The panel |
| Turn the autopilot on and off | The panel | The panel | The panel | The panel |
| Change the build time per frame | The panel | The panel | The panel | The panel |
| Move the focus between the map and the panel | Tab | The View button | A tap on the map or the panel | Play/Pause |

The map starts with the focus, so the arrows, a stick or a remote fly the camera at once, and any input takes the camera from the autopilot until the panel turns it on again. Escape and the east button have nothing to close on this screen and do nothing, and on a TV the Menu or Back button leaves the app from it, as the platforms ask.

## How it works

The terrain comes from the seed alone. A height field of fractal noise of [`haylen.math`](../../../docs/lua-api/math.md) picks deep water, water, sand, grass, meadow, forest, rock or snow for every tile with a moisture field, the slope toward the top left shades the land and the depth darkens the water, and every tile takes one of two variants and a quarter turn from a hash of its place. Trees, palms, reeds, bushes, flowers, boulders and snowy pines grow where a Poisson scatter of [`haylen.procedural2d`](../../../docs/lua-api/procedural2d.md) places them over a density field of the whole world, each by the terrain under it, so neighbor chunks agree at their edges and a chunk built again matches the one that unloaded.

Every frame the app wants the chunks that the camera sees and a ring of one chunk around them, and unloads the chunks more than three chunks away. Up to eight missing chunks build at once, nearest first, each in a task that the scene owns:

1. The function `procedural.scatterAsync` places the decorations of the chunk on a worker thread.
2. A job of [`haylen.jobs`](../../../docs/lua-api/jobs.md) samples the noise of the 4,096 tiles and their borders, sorts the decorations from the top down and makes the sprite fields of both, pausing at every row once the build time of the frame is spent, which the panel sets from 2 to 16 milliseconds.
3. On the frame thread, one `buffer:set` and one `batch:writeFields` fill a sprite batch that `batch:bake()` turns into a static batch for the tiles and another for the decorations, so a loaded chunk uploads nothing again and draws with two calls of `graphics2d.drawStatic`.

The readout shows the graphics API that draws the app, the frame time, the frame rate, the time the GPU took for a frame where the API measures it and the draw calls of the engine, the loaded chunks, the chunks building and waiting, the tiles and decorations drawn in the frame, the sprites baked into the loaded chunks, the memory of Lua and the zoom.

## Numbers

The numbers come from the Release desktop player that `haylen engine` builds, on an Apple M5 Pro with macOS, in a window of 1600 by 900 points, 3200 by 1800 pixels on its display, with `vsync` off. Each load settles for 3 seconds and then samples 3 seconds. The average frame and the one percent low, the average of the slowest 1 percent of the frames, come from `debug.frame().milliseconds`, the time the engine spends on a frame, and the frames per second are the frames counted in the 3 seconds, which never pass the 120 Hz of the display. Other heavy work ran on the machine at the same time, with a load average from 21 to 30 and nearly no idle processor time, so the frame thread did not always get the fastest cores, and the numbers are a floor of what the machine does.

| Zoom | Build time per frame | Outlines | Tiles drawn | Trees and rocks drawn | Loaded chunks | Baked sprites | Average frame | One percent low | Frames per second |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1.00 | 4 ms | Off | 16,384 | 305 | 28 | 120,683 | 3.57 ms | 4.66 ms | 120 |
| 0.50 | 4 ms | Off | 16,384 | 644 | 30 | 129,520 | 3.57 ms | 4.22 ms | 120 |
| 0.25 | 4 ms | Off | 49,152 | 2,147 | 52 | 221,030 | 3.66 ms | 5.91 ms | 120 |
| 0.12 | 4 ms | Off | 172,032 | 8,595 | 100 | 428,602 | 3.72 ms | 5.41 ms | 120 |
| 0.06 | 4 ms | Off | 491,520 | 22,799 | 216 | 925,515 | 4.27 ms | 6.37 ms | 120 |
| 0.04 | 4 ms | Off | 917,504 | 40,588 | 357 | 1,527,650 | 4.61 ms | 7.39 ms | 120 |
| 0.03 | 4 ms | Off | 1,622,016 | 75,376 | 552 | 2,364,874 | 8.27 ms | 8.76 ms | 120 |
| 0.03 | 4 ms | On | 1,712,128 | 74,956 | 569 | 2,439,677 | 8.28 ms | 8.62 ms | 120 |
| 0.03 | 16 ms | Off | 1,712,128 | 77,178 | 570 | 2,437,963 | 5.85 ms | 18.09 ms | 101 |
| 0.12 | 2 ms | Off | 172,032 | 5,991 | 97 | 414,247 | 0.90 ms | 5.92 ms | 120 |

The camera flies on its autopilot while the zoom steps down from 1 to 0.03, where the view holds about 550 chunks with 1.6 million tiles and 75,000 trees and rocks, out of 2.4 million baked sprites. Every zoom with a build time of 4 milliseconds per frame holds the 120 frames per second of the display: the average frame grows from 3.6 to 8.3 milliseconds as the view draws two static batches for every chunk in it, and the one percent low stays under 9 milliseconds while chunks keep building at the edges.

The build time trades how soon chunks appear for the slowest frames: 16 milliseconds per frame lets the one percent low reach 18 milliseconds and the frame rate drop to 101 frames per second.

## Art

The terrain and its decorations are painted for the sample with the `codex` command line and its `$imagegen` skill in the style of [Tiny Island](../../games/tiny-island/README.md#art), whose art is the standard of the repository, as the [project rules](../../../AGENTS.md#samples) describe, then cut and scaled into one atlas, `content/terrain/terrain.png` of 1152 by 208 pixels, which the map loads with the `linear` filter, because the art is painted in high definition and the screen scales it. Every prompt carries the key words of this guide.

| Part | Rule |
| --- | --- |
| Look | Polished casual fantasy map art in high definition, a flat painted map seen from above with small marks inside the tiles, and chunky cute decorations seen from a slight three-quarter top-down view, standing on their base. |
| Outline | Tiles have none, so they join seamlessly. Decorations have a bold clean outline of even thickness in very dark navy `#1E1B33`. |
| Shading | Soft cel shading with two tones per material and one soft highlight, light from the top left, clean flat areas without noise or texture. On the map, the slope of the height brightens the tiles that face the light and darkens the others, and deep water is darker. |
| Palette | Deep water `#1E4C86`, water `#2C77B5`, sand `#E6CF95`, grass `#69B04F`, meadow `#A8BC58`, forest `#3E8743`, rock `#8A8781` and snow `#EEF2F6`, each with a lighter and a darker shade for its marks. Trees `#2F7A45` to `#5BAE5E` on trunks `#7A5232`, palms `#4BA15A` on `#9C7046`, boulders `#8F8B84`, flowers in gold, coral, cream and lilac. |
| Tiles | Eight terrains of two variants in the top row, cells of 72 pixels holding a tile of 64 whose plain base color fills the outer band of the tile and the gutter of 4 pixels around it, so tiles join and turn in quarter turns without seams and linear filtering never mixes neighbors. A tile draws at 32 world units. |
| Decorations | Pine, tree, palm, bush, boulder, flowers, snowy pine and reeds, painted in one sheet at one scale, in cells of 80 by 112 pixels every 96 from the top of 88, standing on a line 5 pixels above the bottom of their cell, drawn at 46 by 64 world units times a scale from 0.8 to 1.2, with their pivot at their base. |

## Package layout

```text
streamed-map/
  app.json               Window, design resolution of 1920 by 1080 in landscape and identifier.
  source/
    main.lua             Sets the map theme of the panel and opens the flight.
    scenes/flight.lua    The flight: the camera, the autopilot, input, the panel and the drawing.
    chunks.lua           The streaming: wanted chunks, builds, baking, unloading and the outlines.
    terrain.lua          The terrain from the seed: tiles, shading and decorations.
    hud.lua              The panel with the readout, refreshed four times per second, the controls and a Quit button where quitting closes the app.
    tour.lua             The automatic run of the headless platform.
  content/
    terrain/terrain.png  The atlas of the tiles and the decorations.
```
