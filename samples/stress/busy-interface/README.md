# Busy Interface

Busy Interface is a stress sample of the UI: one dense screen of a made-up container port in operation, the way an operations dashboard looks. Live cards show metrics that change every frame, charts drawn with `haylen.graphics2d` sit next to the UI, recycled lists and grids hold tens of thousands of vessels and containers, a table follows the berths and a feed of events grows at the bottom of its list. The panel raises and lowers the load, so the cost of a heavy interface shows in the readout while it runs.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 haylen.py run samples/stress/busy-interface` |
| macOS app | `python3 haylen.py run samples/stress/busy-interface --platform macos` |
| iPhone and iPad simulator | `python3 haylen.py run samples/stress/busy-interface --platform ios-simulator` |
| Apple TV simulator | `python3 haylen.py run samples/stress/busy-interface --platform tvos-simulator` |
| Android device or emulator | `python3 haylen.py run samples/stress/busy-interface --platform android --device <serial>` |
| Browser | `python3 haylen.py run samples/stress/busy-interface --platform web --coep off` |
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

The readout shows the frame time, the frame rate and the draw calls of the engine, the changes per second, the items in the lists, the cells the collections keep alive and the memory of Lua.

## Numbers

The numbers come from the Release desktop player that `python3 haylen.py engine` builds, on an Apple M5 Pro with macOS, in a window of 1600 by 900 points, 3200 by 1800 pixels on its display, with `vsync` off. Each load settles for 3 seconds and then samples 3 seconds. The average frame and the one percent low, the average of the slowest 1 percent of the frames, come from `debug.frame().milliseconds`, the time the engine spends on a frame, and the frames per second are the frames counted in the 3 seconds, which never pass the 120 Hz of the display. Other heavy work ran on the machine at the same time, with a load average from 21 to 30 and nearly no idle processor time, so the frame thread did not always get the fastest cores, and the numbers are a floor of what the machine does.

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

The art of the sample is vector art made for it as SVG. The logo is an SVG picture of the GUI, the icons of the charts draw as vector images with `graphics2d.drawVector` together with the charts, and the chart panel rasterizes once when the dashboard loads with `image:rasterize(2)` of [`haylen.graphics`](../../../docs/lua-api/graphics.md#vectorimage) into a texture with the `linear` filter. The cards, the lists and the table draw with the flat colors of the theme. It follows one style guide. The `codex` command line and its `$imagegen` skill regenerate these images from this style guide in the same sizes and layouts, as the [project rules](../../../AGENTS.md#samples) describe, for one consistent look with the rest of the project.

| Part | Rule |
| --- | --- |
| Look | A dense dark console: flat surfaces with rounded corners, thin borders and color only where a value needs attention. |
| Outline | No ink outlines. Surfaces have a border of 2 units in `#2A3646` and corners of 12 units, and the panel art carries a faint top edge `#33425A`. |
| Light | Flat, without shadows inside the screen. A brighter accent marks the live parts: the line, the latest point and the icons. |
| Palette | Window `#0F141C`, panels `#141B25`, cards `#1B2430`, text `#E6ECF2` and `#8A9AAD`, accent blue `#3C9BE0`, success green `#35B37E`, warning amber `#E8A93A`, danger red `#E5534B`, and a chart palette of blue, green, amber, red, lilac `#A87FE0`, teal `#4FC9C4`, orange `#F07E4C` and steel `#9BB4CC`. |
| Logo | `content/dashboard/logo.svg`: a crane over a container on a blue tile of 64 units with corners of 14, drawn at 52 design units. |
| Icons | `content/dashboard/icon_moves.svg`, `icon_berths.svg` and `icon_share.svg`: line icons of 24 units in `currentColor`, tinted with the accent at 24 design units. |
| Panel | `content/dashboard/panel.svg`: a card of 64 units with corners of 12, rasterized into 128 pixels and drawn as a nine-slice with borders of 28 pixels at half scale. |

## Package layout

```text
busy-interface/
  app.json               Window, design resolution of 1920 by 1080 in landscape and identifier.
  source/
    main.lua             Sets the dense operations theme and opens the dashboard.
    scenes/dashboard.lua The dashboard: the GUI, the live values, the lists, the table, the feed and the charts.
    port.lua             The made-up port: metrics, vessels, containers, berths and events.
    charts.lua           The line, bar and ring charts drawn with graphics2d.
    hud.lua              The panel with the readout, refreshed four times per second, and the controls.
    tour.lua             The automatic run of the headless platform.
  content/
    dashboard/           The logo, the chart icons and the panel.
```
