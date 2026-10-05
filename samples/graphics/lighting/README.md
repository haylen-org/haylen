# Haylen Lighting

Haylen Lighting is a Lua sample of the 2D lighting of `haylen.lighting2d` and the lit canvases of `haylen.graphics2d`. A menu lists one test per feature, each test is a scene with a Back button, and Escape, the B button of a gamepad, the Menu button of an Apple TV remote or the Back button of an Android device returns to the menu.

| Test | What it shows |
| --- | --- |
| Ambient light | The `ambientLight` of a lit canvas, eased between presets with a tween of the color. |
| Point lights | Point lights with their own color, radius and intensity, one on the cursor and more where you place them. |
| Spot lights | A spot light aimed at the cursor with its inner and outer angles, and two sweeping searchlights. |
| Directional light | A sun that covers the whole canvas from its rotation and casts shadows across the view. |
| Blend modes | Lights that add, subtract and mix into the light map. |
| Intensity above 1 | Lights of intensity 1, 2 and 4 in the floating-point light map that `graphics2d.hdrLighting()` reports. |
| Shadows | Closed and open occluders, the none, PCF5 and PCF13 filters, shadow color, smoothness and culling. |
| Physics and Tiled occluders | Occluders from falling physics crates with `lighting2d.occludersFromBody` and from the walls of `content/maps/walls.tmj` with `lighting2d.occludersFromMap`. |
| Normal maps and specular | Bricks and metal studs whose textures and normal maps are generated in Lua from height fields, with light height, specular and shininess. |
| Unshaded and emissive | Road signs that keep their colors with `unshaded` and windows and neon that glow with `emission`. |
| Light masks and layers | The `itemMask` of a light against the `lightMask` of each draw, and `layerMin` and `layerMax` against its layer. |
| Flicker | Torches driven by `lighting2d.flicker`, with speed, amount and a seed per torch. |
| Day and night | A day cycle written in Lua that blends ambient and sky colors between keyframes, turns the sun and switches lamps and windows on at dusk. |
| Lit render targets | Two lit canvases drawn into render targets with `graphics2d.beginTarget` and shown as screens. |
| Many lights | Up to 2048 moving lights with the counters of `graphics2d.stats()`. |

## Controls

The light or the cursor of a test follows the mouse and the first finger, and WASD or the left stick move it. A click, a tap, E or the X button places what the test places. Every setting of a test is a control in the panel on the right, which the mouse, touch, the arrow keys, the directional pad and a TV remote reach.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 haylen.py run samples/graphics/lighting` |
| macOS app | `python3 haylen.py run samples/graphics/lighting --platform macos` |
| iPhone and iPad simulator | `python3 haylen.py run samples/graphics/lighting --platform ios-simulator` |
| Apple TV simulator | `python3 haylen.py run samples/graphics/lighting --platform tvos-simulator` |
| Android device or emulator | `python3 haylen.py run samples/graphics/lighting --platform android` |
| Browser | `python3 haylen.py run samples/graphics/lighting --platform web` |

## Package layout

```text
lighting/
  app.json               Window, design resolution of 1920 by 1080 and identifier.
  source/
    main.lua             Loads the action map and opens the menu.
    tests.lua            The tests in menu order.
    sample.lua           The base scene of every test with its header and Back button, and the cursor.
    stage.lua            The room of crates and pillars with their occluders.
    scenes/menu.lua      The menu.
    tests/               One scene per test.
  content/
    input/actions.json   The action map.
    maps/walls.tmj       The Tiled object layer of the occluders test.
```
