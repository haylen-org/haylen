# Haylen Shaders

Haylen Shaders is a Lua sample of custom shaders: fragment shaders in the annotated GLSL of the shader guide, compiled into `.shader` files and drawn through materials of `haylen.graphics2d`. A menu lists one test per feature, each test is a scene with a Back button, and Escape, the B button of a gamepad, the Menu button of an Apple TV remote or the Back button of an Android device returns to the menu.

| Test | What it shows |
| --- | --- |
| Sprite materials | Dissolve, outline, hit flash, wave distortion, palette swap, hologram and pixelation materials on sprites, with their uniforms set every frame. |
| Uniforms with tweens | Uniforms fed by tweens of plain fields: a yoyo, a color in HSV, an elastic curve, stepped easing and a timeline. |
| Textures as uniforms | A material that maps the brightness of a draw through a gradient texture and lays a pattern texture over it, including a render target redrawn every frame. |
| Post-processing chain | The built-in vignette followed by a two-pass blur, color grading and a CRT pass in `postProcess.materials`. |
| Shaders on text | Rainbow, dissolve and flash materials on text, which keeps its glyph shape and outline. |
| Hot reload | A material of `content/shaders/live.glsl` and the steps to edit it while a desktop dev run reloads it in place. |

## Shaders

Every source lives in `content/shaders/` next to the `.shader` file compiled from it, which ships in the package:

```sh
python3 make.py shaders samples/graphics/shaders
```

`make.py run` compiles the sources that changed on its own, and a desktop dev run reloads a `.shader` file as soon as it is compiled again, as the [shader guide](../../../docs/shaders.md) explains. Browsers, phones and TVs run the compiled files.

| Source | Effect |
| --- | --- |
| `dissolve.glsl` | Burns the draw away through a noise texture with a glowing edge. |
| `outline.glsl` | Outlines the opaque pixels of a sprite. |
| `flash.glsl` | Mixes the draw toward a flat color. |
| `wave.glsl` | Bends the image with a traveling sine wave. |
| `palette.glsl` | Replaces each pixel with the target color of its nearest source color. |
| `hologram.glsl` | A tinted, flickering hologram with scanlines and a color split. |
| `pixelate.glsl` | Samples the image in blocks. |
| `texture_map.glsl` | Maps the brightness through a gradient texture and lays a pattern texture over it. |
| `blur.glsl` | One direction of a Gaussian blur. |
| `grading.glsl` | Tints shadows and highlights apart with its own contrast. |
| `crt.glsl` | Curved glass, scanlines, a color split and dark corners. |
| `rainbow.glsl` | A flowing rainbow with a sweeping shine for text. |
| `live.glsl` | The stripes of the hot reload test. |

## Controls

A click, a tap, E or the X button flashes the hero in the tests that flash it. Every setting of a test is a control in the panel on the right, which the mouse, touch, the arrow keys, the directional pad and a TV remote reach.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run samples/graphics/shaders` |
| macOS app | `python3 make.py run samples/graphics/shaders --platform macos` |
| iPhone and iPad simulator | `python3 make.py run samples/graphics/shaders --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run samples/graphics/shaders --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run samples/graphics/shaders --platform android` |
| Browser | `python3 make.py run samples/graphics/shaders --platform web` |

## Package layout

```text
shaders/
  app.json               Window, design resolution of 1920 by 1080 and identifier.
  source/
    main.lua             Loads the action map and opens the menu.
    tests.lua            The tests in menu order.
    sample.lua           The base scene of every test with its header and Back button.
    art.lua              The images, the materials and the grid of captioned cells.
    scenes/menu.lua      The menu.
    tests/               One scene per test.
  content/
    input/actions.json   The action map.
    images/              The hero, the planet, noise, a pattern and gradient ramps, all generated for the sample.
    shaders/             The shader sources and their compiled .shader files.
```
