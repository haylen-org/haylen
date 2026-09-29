# Haylen Tiled

A Lua sample with one scene per feature of [haylen.tiled](../../../docs/lua-api/tiled.md), on small maps in the JSON format of Tiled 1.12. The menu lists the tests, each test opens as its own scene with a Back button, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu.

| Test | What it shows |
| --- | --- |
| Orientations | Orthogonal, isometric, staggered, hexagonal and oblique maps, with the cell under the pointer from `map:worldToCell` outlined from `map:cellToWorld`. |
| Infinite maps | A map stored in chunks that reach negative cells, with its chunks outlined, panned and zoomed. |
| Tile animations | Water, lava and torches animating on tile layers, coins and slimes animating as tile objects, and the frames of the tile under the pointer. |
| Image layers | A sky, clouds that repeat on both axes and two ranges of hills, each with its own parallax. |
| Group layers | Nested groups that pass their offset, tint, opacity and parallax on to their layers, shown and hidden at run time. |
| Objects and templates | Rectangles, ellipses, capsules, points, polygons, polylines, text and tile objects, and templates with overrides. |
| Properties | String, int, float, bool, color, file, object, class and list properties, nested classes and lists and custom type names, on the map, its layers, its objects and its tiles. |
| Collision | Physics bodies from tile collision shapes and collision objects, water tiles and a pit as sensors, a layer without collision and fences in their own category. |
| Spawning | Entities made by factories keyed by object class, placed with the offsets of their group. |
| Y sorting | A hero walking behind and in front of trees, lamps and fences sorted by the y they stand on. |
| Ray casts | Rays against the cells of a tile layer, with a filter by tile class, and against the shapes of an object layer. |
| Worlds | A `.world` file that places a listed map and the maps a file name pattern finds. |

## Content

Everything under `content/` comes from `tools/generate_content.py`, which draws the pixel art and writes the tilesets, templates, maps and the world with the Python standard library, so the same run always writes the same files. The maps, tilesets (`.tsj`), templates (`.tj`) and the world open and edit in Tiled 1.12.

```sh
python3 samples/gameplay/tiled/tools/generate_content.py
```

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run samples/gameplay/tiled` |
| macOS app | `python3 make.py run samples/gameplay/tiled --platform macos` |
| iPhone and iPad simulator | `python3 make.py run samples/gameplay/tiled --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run samples/gameplay/tiled --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run samples/gameplay/tiled --platform android --device <serial>` |
| Browser | `python3 make.py run samples/gameplay/tiled --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
| Point, pick and drag | The pointer and the left mouse button | Right stick moves a cursor, right trigger presses | Finger | Options of the panel |
| Pan the camera | Drag, WASD or the arrows, mouse wheel to zoom | Left stick | Drag and the zoom buttons | Swipes |
| Walk in the y sorting test | WASD or the arrows, or click where to go | Left stick | Touch stick or tap where to go | Swipes |
| Start over | R | West button | Button of the panel | Button of the panel |
| Next or previous map | E and Q | Shoulder buttons | Radio buttons | Radio buttons |
