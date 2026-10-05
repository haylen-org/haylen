# Haylen Sprites

A Lua sample with one scene per feature of 2D drawing in [`haylen.graphics2d`](../../../docs/lua-api/graphics2d.md), [`haylen.animation2d`](../../../docs/lua-api/animation2d.md) and [`haylen.collections`](../../../docs/lua-api/collections.md). The menu lists the tests, each test opens as its own scene with a Back button, the code it runs and its live numbers, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu. Every test preloads the images of the sample in its `load` hook while the fade covers the screen.

| Test | What it shows |
| --- | --- |
| Sprite basics | Pivot, rotation, scale, flips, tint and a hit flash on one hero, next to a row of sprites that show one property each. |
| Atlases and sheets | A TexturePacker atlas in the hash layout with trimmed frames, an Aseprite atlas in the array layout with tagged animations and a nine-slice from a slice, and a grid sheet. |
| Animation | Idle, run and attack clips with frame and finish events, a queue of clips, speed, and the loop, once and ping-pong modes side by side. |
| Sprite batches | A field of gems in one sprite batch that the test adds to, changes and removes from, compared with one draw per gem. |
| Static batches | A tile map and trees baked once and drawn without uploads, with a parallax offset, compared with the same map as a sprite batch. |
| Layers and y-sort | Heroes among trees and rocks sorted by the y they stand on, and a selected hero raised above every layer with its shadow. |
| Pooled bullets | Spiral, ring and aimed patterns of projectiles from an object pool, drawn in one additive batch. |
| Bunnymark | Bouncing bunnies from ten to many thousands, moved with a float buffer or with one table each, with the frame rate. |
| Render targets | An offscreen canvas drawn every frame and used as a texture, turned, tinted, flipped and on a waving mesh. |
| Primitives | Lines, polylines, rectangles, circles, rings, arcs, polygons and meshes with vertex colors and a texture. |
| Text | Sizes, color, outline, shadow, alignment and wrapping, anchors, rotation, measuring and a line of rich text. |

The images under `content/` were drawn for this sample from code: the hero, the walker sheet, the gem and slime atlases, the bunny, the bullets, the tiles and the props.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 haylen.py run samples/graphics/sprites` |
| macOS app | `python3 haylen.py run samples/graphics/sprites --platform macos` |
| iPhone and iPad simulator | `python3 haylen.py run samples/graphics/sprites --platform ios-simulator` |
| Apple TV simulator | `python3 haylen.py run samples/graphics/sprites --platform tvos-simulator` |
| Android device or emulator | `python3 haylen.py run samples/graphics/sprites --platform android --device <serial>` |
| Browser | `python3 haylen.py run samples/graphics/sprites --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Use the panel of a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Act on the stage: attack, select, add or remove a gem, aim, pour bunnies | Left mouse button | Buttons of the panel | Finger | Buttons of the panel |
| Add or remove bunnies | + and - | Right and left shoulder buttons | Buttons of the panel | Buttons of the panel |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
