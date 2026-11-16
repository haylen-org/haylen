# 🚗 Car Dashboard

Car Dashboard is an app sample of the screen of an electric car on a drive through the city. A speedometer whose arc fills from cyan to orange holds the gear and the speed over the road ahead, where the lane assist lights the own lane and the cars of the other lanes drift ahead and behind. Around it, the navigation shows the next turn on a map that moves with the car, and cards show the battery with its range, the energy use, the pressure of the tyres, the driver assistants and the trip computer. The climate of both seats and the dock of the apps run along the bottom. Everything moves as the drive goes on: the speed rises on long streets and falls before every turn, the distance and the time to the turn count down, the signals blink, the battery drains and recovers energy when the car slows down, and the tyres warm up. All the data is made from a seed, so every run drives the same way.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/apps/car-dashboard` |
| macOS app | `haylen run samples/apps/car-dashboard --platform macos` |
| iPhone and iPad simulator | `haylen run samples/apps/car-dashboard --platform ios-simulator` |
| Apple TV simulator | `haylen run samples/apps/car-dashboard --platform tvos-simulator` |
| Android device or emulator | `haylen run samples/apps/car-dashboard --platform android --device <serial>` |
| Browser | `haylen run samples/apps/car-dashboard --platform web` |
| Headless player | `build/macos-debug/bin/haylen-headless samples/apps/car-dashboard` |

On the headless platform the app runs a tour by itself: it drives, warms the cabin, heats the seat of the passenger and goes through the apps of the dock, logs every step with the speed, the distance to the turn, the battery and the energy use, and quits.

## What it shows

| Part | What it shows |
| --- | --- |
| Status bar | The weather and the temperature outside, the temperature of the cabin, which follows the climate, the drive mode, the connections and the clock. |
| Speedometer | An arc of three quarters of a turn up to 240 km/h that fills with the speed through a conic gradient, the ticks and numbers that light as the speed passes them, the gear and the speed. |
| Road | The road ahead in perspective, the dashes of the lanes running toward the car with its speed, the lane assist in green, the cars of the other lanes, the tail lights that glow when the car brakes and the corner that blinks with the signal. |
| Assistant strip | The turn signals, the lane assist, the set speed of the adaptive cruise and the power of the motor, or the energy that braking recovers. |
| Navigation | The arrow of the next turn, its distance and street, the map of the city that moves with the car along the route, and the distance, the time and the arrival at the destination. |
| Trip computer | The distance, the average energy use and the time of the trip. |
| Battery | The charge in percent with its bar and the range at the average of the trip. |
| Energy use | The last half minute of energy use as a line over a soft green area, with the value of the moment. |
| Tyres | The car seen from above with the pressure of each tyre, which rises a little as the tyres warm up. |
| Assistants | The tiles of eco mode, traction control, the brakes and the lane assist. |
| Climate | For the driver and the passenger: cooler and warmer, the temperature, the automatic mode and the heating of the seat in three levels. |
| Dock | Home, navigation, music, phone, apps and settings, with the app that shows lit. |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Move between the controls of the climate and the dock | The arrows and Tab, or the pointer | The directional pad and the left stick | A tap | The directions |
| Press | Enter, Space or a click | The south button | A tap | Select or a click |

## How it works

The screen is one scene, `source/scenes/cockpit.lua`. Each frame it moves the car of `source/drive.lua`, asks `source/layout.lua` for the places of the parts, which change only when the screen or the bar of the controls does, and draws the cards of `source/panels.lua` and the cluster of `source/cluster.lua` with [`haylen.graphics2d`](../../../docs/lua-api/graphics2d.md). The controls of `source/controls.lua` are a GUI of [`haylen.ui`](../../../docs/lua-api/ui.md), so every device reaches them through the focus navigation of the engine.

- The drive follows a route of streets on a grid. The speed aims for the cruise of the street, waving with the traffic, and never more than lets the car slow down to the speed of the next turn, so it rises and falls like a real drive. The power comes from the drag of the air, the roll of the tyres and the change of speed of a car of 2,100 kg, and braking recovers part of it, which the battery, the energy use and the range follow.
- The energy use averages the road behind the car by distance, so a start or a stop weighs as much as the road it takes. The chart of its history scales itself to the values it shows, in steps of 5 kWh/100 km with room above and below the line, grows at once when a value needs more room and glides back when none does, so the line never runs along an edge.
- The arc of the speedometer is a sector of an ellipse drawn with `graphics2d.drawShape` over a texture of a conic gradient that `image:fillGradient` paints once, and the face of the dial cuts the sector into a ring. The battery bar, the backdrop and the halo behind the gauge are gradients too.
- The road is quads in perspective, and the lane assist and the area under the chart of the energy use are meshes whose vertex colors fade out.
- The map draws the blocks of the city around the car, the route as two strokes, a wide soft one and a bright one, and the car as a disc with an arrow, all inside a rounded mask.
- The arrow of the turn is a path that `path:stroke` draws, and the icons are SVG documents that `graphics2d.drawVector` and the buttons tint.
- Every table that the drawing uses is made once and filled again each frame, and the texts of the readouts are kept once they were made, so a frame that shows what the frame before it showed builds nothing.
- A landscape screen uses a design size of 1920 by 1080 and a portrait one 1080 by 1920, so the text keeps its size when the screen turns, and the bar of the controls shows in one row in landscape and in two rows in portrait. In portrait the cards keep the height their content needs and the map takes the rest, and a wide portrait screen, such as an iPad, sets the gauge beside the navigation.

## Style guide

The art follows the style of [Tiny Island](../../games/tiny-island/README.md#art), whose art is the standard of the repository, as the [project rules](../../../AGENTS.md#samples) describe, in the night palette of a car screen. Only the two pictures of the car are images, painted with the `codex` command line and its `$imagegen` skill, and everything else is drawn with shapes, paths, gradients, text and SVG icons.

| Part | Rule |
| --- | --- |
| Palette | Night `#060A14` behind everything with a backdrop that falls from deep blue `#0B1430`, panels `#0E1628` at 90 percent with edges `#1E2C4A`, text `#F1F5FB` and muted text `#8A9BB8`. The speed runs from cyan `#22D3EE` through blue `#3B82F6`, violet `#8B5CF6` and magenta `#EC4899` to orange `#F97316`. Energy and the lane assist are green `#22C55E`, the signals amber `#F5B83D`, the tail lights red `#FF3B4E` and the selected app violet `#7C4DFF`. |
| Type scale | The default font of the engine. Small text 26 units, body 30, labels 34, values 44 and the speed at 40 percent of the radius of the gauge, in design units of a 1920 by 1080 screen. |
| Shapes | Cards round to 32 with a soft shadow and an edge of 2, pills and the strip are round at their ends, tiles round to 22, and the buttons of the climate are circles. |
| Car | `content/images/car-rear.png`, 640 by 480 pixels, and `content/images/car-top.png`, 256 by 512 pixels: a sleek electric crossover in pearl white `#E9EEF5` and silver `#B9C3D3`, with dark tinted glass `#1B2440` and a slim red light bar `#FF3B4E`, in the bold dark navy outline `#1E1B33` and the soft cel shading of the repository, two tones and one highlight, light from the top left, on a transparent background, without a brand or a text. |
| Icons | `content/icons/`: line icons of 48 units with round strokes of 3.5 in `currentColor`. |

## Package layout

The sample folder is the package. Only `app.json`, `source/` and `content/` ship, and this README stays behind.

```text
car-dashboard/
  app.json               Window, design size of 1920 by 1080 with expand scaling, any orientation and the identifier.
  source/
    main.lua             Registers the theme and opens the dashboard.
    style.lua            The palette, the gradients, the theme and the drawing helpers.
    text.lua             The texts of the readouts, kept once they were made.
    drive.lua            The route, the speed, the energy, the tyres, the traffic and the signals.
    layout.lua           The places of the parts in landscape and in portrait.
    cluster.lua          The speedometer, the road and the strip of the assistants.
    panels.lua           The status bar, the navigation with its map, the trip computer, the battery, the energy use, the tyres and the assistants.
    controls.lua         The climate and the dock.
    tour.lua             The automatic run of the headless platform.
    scenes/cockpit.lua   The screen.
  content/
    images/              The car from behind and from above.
    icons/               The line icons.
  ui-tests/
    OrientationTests.swift  The UI test that turns the device to landscape and back and saves a picture of each, which "haylen test --ui samples/apps/car-dashboard" runs on a simulator.
```
