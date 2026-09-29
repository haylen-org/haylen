# Haylen Nine-Patch

A Lua sample with one scene per feature of the nine-slice frames of [haylen.graphics2d](../../../docs/lua-api/graphics2d.md#nineslice) and the theme surfaces of [haylen.ui](../../../docs/lua-api/ui.md#theme-surfaces). The menu lists the tests, each test opens as its own scene with a Back button and the code it runs, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu.

| Test | What it shows |
| --- | --- |
| Stretch and tile | A framed image cut by borders, with the cut lines on the source, drawn at a size that grows and shrinks with its edges and center stretched and tiled. |
| Nine pieces | A frame made of nine separate regions of a sheet, numbered on the source, stretched and tiled. |
| Scale and tint | The same frame with its borders scaled from half to twice their size and tinted, and a large frame with the scale and the tint of the panel. |
| UI theme surfaces | A theme whose panels, banner, buttons, tracks, fills and knobs are nine-slice images that colorize takes the color of each component into, with the fill image drawn in every tone for comparison. |
| Resizable panel | A nine-slice window moved by its middle and resized by its edges and corners, with its text wrapping inside the borders. |

The images under `content/` were drawn for this sample from code: the framed panel, the sheet of nine pieces and the gray surfaces of the UI theme.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run samples/graphics/nine-patch` |
| macOS app | `python3 make.py run samples/graphics/nine-patch --platform macos` |
| iPhone and iPad simulator | `python3 make.py run samples/graphics/nine-patch --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run samples/graphics/nine-patch --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run samples/graphics/nine-patch --platform android --device <serial>` |
| Browser | `python3 make.py run samples/graphics/nine-patch --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Use the panel of a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Move and resize the panel | Drag with the left mouse button, or WASD to resize | Right stick resizes | Drag with a finger | Not on a remote, whose panel sets the fill and the border scale |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
