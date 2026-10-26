# 🏋️ Busy Interface

Busy Interface is a stress sample of the UI: one dense screen of a made-up container port in operation, the way an operations dashboard looks. Live cards show metrics that change every frame, charts drawn with `haylen.graphics2d` sit next to the UI, recycled lists and grids hold tens of thousands of vessels and containers, a table follows the berths and a feed of events grows at the bottom of its list. The panel raises and lowers the load, so the cost of a heavy interface shows in the readout while it runs.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/stress/busy-interface` |
| macOS app | `haylen run samples/stress/busy-interface --platform macos` |
| iPhone and iPad simulator | `haylen run samples/stress/busy-interface --platform ios-simulator` |
| Apple TV simulator | `haylen run samples/stress/busy-interface --platform tvos-simulator` |
| Android device or emulator | `haylen run samples/stress/busy-interface --platform android --device <serial>` |
| Browser | `haylen run samples/stress/busy-interface --platform web` |
| Headless player | `build/macos-debug/bin/haylen-headless samples/stress/busy-interface --frames 900` |

On the headless platform the app runs a short tour by itself: it shows more cards, fills the lists with a hundred thousand items each, scrolls, goes through every rate of change, hides the charts, logs the frame numbers of every step and quits.

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Show 6, 12, 24 or 48 live cards | Plus and minus, or the panel | The right and left triggers, or the panel | The panel | The panel |
| Put 1,000, 10,000 or 100,000 items in each list | The panel | The panel | The panel | The panel |
| Change values every frame, ten times a second or never | R, or the panel | A click of the right stick, or the panel | The panel | The panel |
| Show or hide the charts | The panel | The panel | The panel | The panel |
| Move between the controls, the lists and the table | The arrows, Tab, Page Up, Page Down, Home and End | The directional pad, the left stick and the shoulders | A tap | The directions |
| Scroll the lists | The wheel or the scrollbars | The focus | A drag or a fling | The focus |

The panel starts with the focus. Escape and the east button have nothing to close on this screen and do nothing, and on a TV the Menu or Back button leaves the app from it, as the platforms ask.

## How it works

The screen is one GUI of [`haylen.ui`](../../../docs/lua-api/ui.md) in a dense theme of its own: a header, a grid of live cards in a scroll, a row of three charts, and a row of four sections, the vessels, the containers, the berths and the events.

- Every card shows a metric of the port with its value, a bar and a badge. The metrics wander every frame, and the app changes a value, a bar or a badge with `gui:set` only when what it shows changes, which the readout counts as changes per second.
- The vessels are a `collection` under sticky terminal headers, and the containers a grid `collection` of cells of a fixed shape, each with up to 100,000 items, which jobs of [`haylen.jobs`](../../../docs/lua-api/jobs.md) build so the frame never waits for them. Only the items in view move: the app advances the vessels and a few containers in view and binds them again with `reload`.
- The berths are a `table` whose rows the app replaces four times a second, and the events a `collection` that sticks to its end while a new line arrives every 0.3 seconds and only the last 300 lines stay.
- The charts are `haylen.graphics2d` drawings in the spaces the GUI leaves open for them, under their titles: a line chart of the moves of the cranes over the last 180 changes with its target, bars of the progress of the berths and a ring of the share of every crane. Their panels are a nine-slice of the panel art in the colors of the cards.

The readout shows the graphics API that draws the app, the frame time, the frame rate, the time the GPU took for a frame where the API measures it and the draw calls of the engine, the changes per second, the items in the lists, the cells the collections keep alive and the memory of Lua.

## Numbers

The numbers come from the Release desktop player that `haylen engine` builds, on an Apple M5 Pro with macOS, in a window of 1600 by 900 points, 3200 by 1800 pixels on its display, with `vsync` off. Each load settles for 3 seconds and then samples 3 seconds. The average frame and the one percent low, the average of the slowest 1 percent of the frames, come from `debug.frame().milliseconds`, the time the engine spends on a frame, and the frames per second are the frames counted in the 3 seconds, which never pass the 120 Hz of the display. Other heavy work ran on the machine at the same time, with a load average from 21 to 30 and nearly no idle processor time, so the frame thread did not always get the fastest cores, and the numbers are a floor of what the machine does.

| Cards | Items in each list | Values change | Charts | Lists scrolling | Average frame | One percent low | Frames per second | Draw calls | Lua memory |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 12 | 10,000 | Never | On | No | 0.80 ms | 2.25 ms | 120 | 229 | 10.7 MB |
| 12 | 10,000 | Every frame | On | No | 2.96 ms | 5.40 ms | 120 | 234 | 10.5 MB |
| 24 | 10,000 | Every frame | On | No | 4.40 ms | 5.46 ms | 120 | 284 | 10.0 MB |
| 48 | 10,000 | Every frame | On | No | 4.44 ms | 4.78 ms | 120 | 382 | 10.6 MB |
| 48 | 10,000 | Ten a second | On | No | 3.91 ms | 10.31 ms | 119 | 382 | 9.9 MB |
| 48 | 10,000 | Every frame | Off | No | 2.06 ms | 2.77 ms | 120 | 348 | 10.6 MB |
| 48 | 100,000 | Every frame | On | No | 14.50 ms | 16.71 ms | 67 | 384 | 106.9 MB |
| 48 | 100,000 | Every frame | On | Yes | 14.75 ms | 19.11 ms | 66 | 384 | 123.4 MB |
| 12 | 100,000 | Every frame | On | Yes | 15.19 ms | 16.97 ms | 61 | 236 | 136.0 MB |

With 10,000 items in each list, the screen holds the 120 frames per second of the display at every number of cards: 48 cards that change every frame cost 4.4 milliseconds with 382 draw calls, of which the charts take about 2.4 milliseconds, and paused values leave a frame of 0.8 milliseconds.

With 100,000 items in each list, the frame grows to about 14.5 milliseconds and the screen still holds 60 frames per second, from 61 to 67, scrolling or not. Most of the extra time is the `reload` of the rows in view, whose cost grows with the length of the list, and the items raise the memory of Lua from about 10 to more than 100 MB.

## Art

The sample uses one image. The logo is painted for it with the `codex` command line and its `$imagegen` skill in the style of [Tiny Island](../../games/tiny-island/README.md#art), whose art is the standard of the repository, as the [project rules](../../../AGENTS.md#samples) describe, and the theme sets `imageFilter` to `linear`, because the logo is painted in high definition and the screen scales it. The icons of the charts are line icons in SVG that the dashboard tints with its accent, and the panels of the charts draw as rounded shapes of `graphics2d.drawShape`, so the cards, the charts, the lists and the table all draw with the flat colors of the theme.

| Part | Rule |
| --- | --- |
| Look | A dense dark console: flat surfaces with rounded corners, thin borders and color only where a value needs attention. |
| Logo | `content/dashboard/logo.png` of 128 pixels, drawn at 52 design units: a chunky cute harbor crane in white and warm yellow `#FFC93C` lifting an orange container over a short dock, on a royal blue `#2F86F6` rounded tile, with a bold clean outline in very dark navy `#1E1B33` and soft cel shading lit from the top left, like the app icons of Tiny Island. |
| Icons | `content/dashboard/icon-moves.svg`, `icon-berths.svg` and `icon-share.svg`: line icons of 24 units in `currentColor`, tinted with the accent at 24 design units. |
| Panels | Cards of the chart color `#1B2430` with a border of 2 units in `#2A3646` and corners of 12 units. |
| Palette | Window `#0F141C`, panels `#141B25`, cards `#1B2430`, text `#E6ECF2` and `#8A9AAD`, accent blue `#3C9BE0`, success green `#35B37E`, warning amber `#E8A93A`, danger red `#E5534B`, and a chart palette of blue, green, amber, red, lilac `#A87FE0`, teal `#4FC9C4`, orange `#F07E4C` and steel `#9BB4CC`. |

## Package layout

```text
busy-interface/
  app.json               Window, design resolution of 1920 by 1080 in landscape and identifier.
  source/
    main.lua             Sets the dense operations theme and opens the dashboard.
    scenes/dashboard.lua The dashboard: the GUI, the live values, the lists, the table, the feed and the charts.
    port.lua             The made-up port: metrics, vessels, containers, berths and events.
    charts.lua           The line, bar and ring charts drawn with graphics2d.
    hud.lua              The panel with the readout, refreshed four times per second, the controls and a Quit button where quitting closes the app.
    tour.lua             The automatic run of the headless platform.
  content/
    dashboard/           The logo and the chart icons.
```
