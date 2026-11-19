# 🏡 Smart Home

Smart Home is an app sample of the control panel of a home, the kind that hangs on a wall or runs on a tablet in the kitchen. It shows the house under the sky of the hour, the scenes that set the whole home at once, the thermostat, the lights of every room with their brightness and color, the security with the camera at the front door, the doors and the alarm, the energy of the day with the sun on the roof and the battery, and the blinds, the speaker and the washer. The day runs a minute every second, so the sun rises and sets, the panels on the roof follow it, the battery fills and empties, the room warms toward its target, the washer counts down and now and then someone rings the doorbell or walks up the path, which arrives as a notice and on the camera. Every value is made up, so every run starts the same morning.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen app run samples/apps/smart-home` |
| macOS app | `haylen app run samples/apps/smart-home --target macos` |
| iPhone and iPad simulator | `haylen app run samples/apps/smart-home --target ios-simulator` |
| Apple TV simulator | `haylen app run samples/apps/smart-home --target tvos-simulator` |
| Android device or emulator | `haylen app run samples/apps/smart-home --target android --device <serial>` |
| Browser | `haylen app run samples/apps/smart-home --target web` |
| Headless player | `build/engine/macos-debug/bin/haylen-headless samples/apps/smart-home` |

On the headless platform the app runs a tour by itself: it goes through the scenes, dims a room, warms the climate and unlocks a door, logs every step with the time, the lights, the climate, the sun, the home and the battery, and quits.

## What it shows

| Card | What it shows |
| --- | --- |
| Top bar | A greeting for the time of the day, how many lights are on and the clock. |
| House | The house under a sky that turns from day to dusk and night, with the sun or the moon crossing it and the windows glowing at night while the rooms are lit, and whether the doors are locked and the alarm armed. |
| Scenes | Morning, away, movie and night, which set the lights and their color, the blinds, the alarm, the locks of the doors, the target and the mode of the climate and the music, with the scene that is on lit. The panel opens with the morning scene on. |
| Climate | The dial of the thermostat, filled to the target in the color of the mode, with the mark of the room temperature, cooler and warmer, the mode and the humidity. |
| Rooms | The living room, the kitchen, the bedroom and the office, each with its lights on or off, their brightness and four colors of light. |
| Security | The camera at the front door, which sees in infrared at night and frames a visitor when it sees motion, the alarm and the locks of the doors. |
| Energy today | The sun on the roof over the hours as an amber area, what the home used so far as a coral line and the hour of now, with the solar power, the home, the battery and the grid. |
| Devices | The blinds, the speaker with its song, play and pause and its volume, and the washer with the time left. |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Move between the controls | The arrows and Tab, or the pointer | The directional pad and the left stick | A tap | The directions |
| Press, switch or pick | Enter, Space or a click | The south button | A tap | Select or a click |
| Change a slider or a segment | The left and right arrows, or a drag | Left and right | A drag | Left and right |
| Scroll the cards | The wheel or a trackpad | The focus moves the cards into view | A drag | The focus moves the cards into view |

## How it works

The screen is one scene, `source/scenes/wall.lua`. Each frame it advances the home of `source/home.lua`, lets the panel of `source/panel.lua` change the texts whose values changed and draws the live pictures of `source/views.lua`.

- The panel is a GUI of [`haylen.ui`](../../../docs/lua-api/ui.md) in the physical scale mode: a top bar over a scroll with a `grid` whose `minColumnWidth` fits as many columns as the screen holds, one on a phone in portrait and up to four on a wide wall tablet, so it fits every screen in both orientations. Every control is a component of the engine, so the mouse, a finger, the keyboard, a gamepad and a TV remote all reach it.
- The house, the dial, the camera and the chart of the energy are drawn every frame with [`haylen.graphics2d`](../../../docs/lua-api/graphics2d.md) into render targets that [`ui.setImage`](../../../docs/lua-api/ui.md#uisetimagename-source) names, and pictures of the GUI show them. So they lay out, scroll and fade with their cards. Each target takes the pixels its picture covers on the screen and is made again only when that size changes, so it stays sharp.
- The sky behind the house crossfades three gradients, the glow of the sun and of the windows is a radial gradient on an ellipse, and the area under the curve of the sun is a mesh with a vertical gradient. The camera is drawn from shapes: clouds that drift, a tree that sways, the porch light at dusk, a visitor in the box of the detection and the dark corners of the lens.
- A scene changes the home and the home tells the panel, which sets the controls again. The notices of the doorbell, of the camera and of the washer drop in as banners of `ui.showMessage`.
- The tables that the drawings use are made once and filled again each frame, the texts are kept once they were made, and the panel sets a text only when its value changes, so a frame that shows what the frame before it showed builds nothing.

## Style guide

The panel has a light look of its own, warm like a wall of a home, apart from the night palette of the car dashboard. Its art follows the style of [Tiny Island](../../games/tiny-island/README.md#art), whose art is the standard of the repository, as the [project rules](../../../AGENTS.md#samples) describe. The picture of the house is the one image, painted with the `codex` command line and its `$imagegen` skill, and everything else is drawn with shapes, gradients, text and SVG icons.

| Part | Rule |
| --- | --- |
| Palette | Linen `#F4F1EC` falling to mist `#E9EEF3` behind white cards `#FFFFFF` with hairlines `#E6E8EE`, ink `#1D2433` and muted text `#6B7385`. Coral `#FF7A59` is the accent, heat and the line of the energy used, teal `#14B8A6` the focus and the climate that holds, sky `#5AA9F7` cooling, amber `#FFB547` the light and the sun, green `#22A06B` what is safe and red `#E5484D` the recording dot. |
| Type scale | The default font of the engine. Captions 19 units, body 22, buttons 21, headings 27 and titles 40, in the physical scale mode, where a unit is three quarters of a point. |
| Shapes | Cards round to 28 with a padding of 24, controls round to 18, the swatches of the light colors and the dial are circles, and the pictures inside cards round to 18 or 20. |
| Spacing | Steps of 8, 12, 16, 24 and 32, with 24 between the cards. |
| House | `content/images/house.png`, 768 by 640 pixels: a modern two storey family house in a clean isometric view with solar panels, a garage, a porch light and a garden, in warm white `#F4EFE6`, sand wood `#D9A86C` and a charcoal roof `#3A3F55`, in the bold dark navy outline `#1E1B33` and the soft cel shading of the repository, two tones and one highlight, light from the top left, on a transparent background. |
| Icons | `content/icons/`: line icons of 48 units with round strokes of 3 in `currentColor`. |

## Package layout

The sample folder is the package. Only `haylen.json`, `source/` and `content/` ship, and this README stays behind.

```text
smart-home/
  haylen.json               Window with the status bar in dark content, physical UI scale, any orientation and the identifier.
  source/
    main.lua             Registers the theme and opens the panel.
    style.lua            The palette, the gradients and the theme.
    text.lua             The texts of the panel, kept once they were made.
    home.lua             The day, the rooms, the climate, the doors, the energy, the devices, the scenes and the events.
    panel.lua            The GUI: the top bar and the cards.
    views.lua            The live pictures of the house, the dial, the camera and the energy.
    tour.lua             The automatic run of the headless platform.
    scenes/wall.lua      The screen.
  content/
    images/house.png     The house.
    icons/               The line icons.
  ui-tests/
    OrientationTests.swift  The UI test that turns the device to landscape and back and saves a picture of each, which "haylen test --ui samples/apps/smart-home" runs on a simulator.
```
