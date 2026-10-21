# Haylen Tests

Haylen Tests is the test project of the engine: one app with a test of every engine feature, from sprites to native plugins, grouped in categories. Every test has a code, such as `PHY-004`, which the menu, the header of the test and every log line of the test show, so a failing test is reported by its code. Codes never change and are never reused, and a new test takes the next number of its category.

## Running

- Desktop: `haylen run samples/tests` runs the project in the player with hot reload.
- A platform: `haylen run samples/tests --platform ios-simulator`, and the same with `tvos-simulator`, `android`, `web`, `macos`, `windows` or `linux`.
- Automatic: the headless player runs every test that the headless platform supports, one after the other for 2 seconds each, lists the tests that raised an error, failed a check of their results or whose scene failed to load, and exits with the status 1 when any test failed, which the CI job `tests-project` does on every push:

```sh
haylen build --target haylen-headless
haylen build --target haylen_native_test
build/macos-debug/bin/haylen-headless samples/tests --frames 60000
```

## Using the project

The menu lists the categories by section with their prefixes and the number of their tests. The search field finds tests by code or title, and Enter in it opens the first match. `Continue with` opens the test opened last, which the project remembers between launches in the preference `tests.last`, `Run all` runs every test of the platform one after the other and lists the failures with their codes, and `Quit` closes the project where `haylen.canQuit()` says that quitting closes the app.

- Mouse, touch, keyboard, gamepads and TV remotes reach every screen. A test goes back to its list with Escape, the east gamepad button, the Menu button of a TV remote, the back button of Android or the Back button in the safe area, and the list goes back to the menu the same way.
- A test that reads the keyboard, gamepads or remotes as game input gives its play area the first focus, so the arrows, WASD, the d-pad, the sticks, Space and Enter move the game while the controls of the test stay out of the way. Tab walks the controls and the play area, and the View button of gamepads, the pause key and the Play/Pause button of TV remotes move the focus between the play area and the controls. A click or a tap on a control gives it the focus, and one on the play area gives the focus back to the game.
- A test that a platform cannot run opens a page with the exact reason instead of the test, the platforms where it runs and its code, and the list marks it.
- Every test shows, in a card under its play area or its content, a few lines of the Lua that does what it shows, with the real calls and options of the API, in the monospace font with the colors of the theme for keywords, strings, numbers, comments and the functions it calls. The card keeps the height of six lines in every test and scrolls longer code, and a line of code that wraps never breaks inside a string and continues indented under its start.
- The region of a test, its play area or its content, fills the room between the header and the code card, so nothing of a test runs under the card. Content taller than the region scrolls in it, while content that scrolls by itself, such as a collection of a hundred thousand rows, fills the region with `fixed = true`, the touch controls of a test anchor to the corners of its play area with `anchorTo = 'parent'` and show only where touch is the input, but in the tests of the touch controls themselves, and a GUI that a test mounts on its own stays inside the region with `Test:keepInRegion`.
- The panel of controls stretches its controls to its width and scrolls when they do not fit, the status line takes no room while it is empty and always shows the last status, and the footer shows the hint of the test with the keys that move around the frame.
- An error in a test shows the error screen of the engine. `Back to the app` on it, or Escape, the east button and the back button, returns to the list of the test without restarting the project, and the log keeps the error with the code of the test.

## Layout

| Path | Contents |
| --- | --- |
| `app.json` | The project in landscape at a design size of 1920 by 1080, the autoload of the Events tests, the native test libraries and the plugins the tests use. |
| `source/main.lua` | Loads the catalog, makes the app recoverable and opens the menu, or starts the automatic run on the headless platform. |
| `source/harness/` | What every test shares: the catalog of the manifests, the menu, the test list, the base class `Test` with its frame and play area, the unsupported page, the automatic run, rows of checks, a log of lines, a pointer for every device and the way between the screens. |
| `source/categories/<category>/` | One folder per category with `manifest.lua`, one file per test and the modules its tests share. |
| `content/<category>/` | The assets of each category, and `content/fonts/` the open-license fonts that several categories share, each next to its license. |
| `plugins/` | `native-demo`, the plugin that exercises every capability of native plugins, `native-sample` and `platform-sample` with the platform handlers of the Native and Platform tests, and `native-test`, which answers the test library in the browser and holds the CMake project of that library in `native/`. |
| `tools/` | The module that keys and cuts the paintings, the packers of the fighter with its training dummy and of the cutout puppet with its skeleton of the Animation category, the packer of the tank and its prefab of the Nodes category, the builder of the walker sheet of the Sprites category, the packer of the props and the painter of the pixel art of the Camera category, the packer of the fantasy theme of the Interface category, the packer of the particle atlases and the writer of the effect library, the generator of the bitmap fonts of the Text category, the generators of the sounds of the Audio category, of the maps and tilesets of the Tiled category and of the clip of the Video category. |

A manifest returns the `prefix`, the `title` and the `description` of its category and its `tests`. Each test has a `code`, a `title`, a `description` and the `module` of its file, and a test that does not run everywhere lists the `platforms` it runs on and, in `unsupported`, the reason for each other platform, from `macos`, `windows`, `linux`, `ios`, `tvos`, `android`, `web` and `headless`. The catalog checks every manifest when the project starts.

## Settings of `app.json`

Some settings that `app.json` gives the whole project change at run time, so the tests that need other values set them on entry and restore them on exit:

- The orientation tests unlock the orientation with `window.lockOrientation('any')` and lock it to landscape again, and the fold test restores the fold simulation it found.
- The design resolution test changes the design size and the scaling with `viewport.setDesignSize` and `viewport.setScaling` and restores 1920 by 1080 with `expand`.
- The safe area tests simulate the safe areas of devices with `viewport.setSafeAreaSimulation` and restore the simulation they found.
- The screen reader test turns on the tree of accessible elements with `accessibility.setTreeEnabled` and restores the value it found.
- The window, lifecycle and preferences tests restore the fullscreen mode, the lifecycle options, the bus volumes, the language and the theme they change.
- The UI scale and image theme tests restore the scale of the UI and the theme they change.

No test needs a setting that only `app.json` can give.

## Art of the fantasy theme

The image theme of `GUI-033` in `content/interface/themes/fantasy/` is painted for the project with the `codex` command line and its `$imagegen` skill, as the project rules describe, in the style of [Tiny Island](../games/tiny-island/README.md#art), whose art is the standard of the repository. The tool `tools/interface/pack-fantasy-theme.py` packs the pieces into the atlas, writes the theme file and cuts the sheet of icons into icons of 128 pixels. Every prompt carries the key words of this guide.

| Part | Rule |
| --- | --- |
| Look | Polished casual fantasy interface in high definition, chunky cute rounded shapes and simple bold forms that read at a small size, seen straight from the front without perspective. |
| Outline | A bold clean outline of even thickness in very dark navy `#15172A` around every piece, and `#1E1B33` around the icons, with nothing outside it but transparency. |
| Shading | Soft cel shading with two tones per material and one soft glossy highlight, light from the top left, clean flat areas without noise or texture, and plain middles so every piece stretches as a nine-slice. |
| Palette | Slate indigo panels `#2F3352` with slate bevels `#3A4066`, dark navy insets `#1E2138` for fields, boxes and grooves, slate lavender `#5D6596` default buttons and tabs, royal blue `#2F86F6` primary pieces, red `#F0473E` destructive buttons, green `#4CD45A` checks, a warm yellow `#FFC93C` rim for the selection and the focus, and near white `#F2F3FF` pieces that the theme colors with `colorize`. |
| Sizes | Buttons 360 by 120 pixels, the panel 320 by 320, the title bar 480 by 112, the ribbon 720 by 140, fields 240 by 96, tabs 240 by 96, the selection plate 256 by 88, the tooltip 192 by 112, check boxes, radio marks, the knob and the close button 96 by 96, the groove 200 by 56, the fill 192 by 32, the badge 128 by 72, the scroll bar pieces 72 by 72 and icons 128 by 128. States of one piece, such as pressed, focused, checked or selected, are painted as edits of the first piece, so they keep its shape and size. |

The theme loads its pictures and surfaces with the `linear` filter, because the art is painted in high definition and the screen scales it.

## Art of the categories

The pictures of the categories are painted for the project with the `codex` command line and its `$imagegen` skill in the style of [Tiny Island](../games/tiny-island/README.md#art), whose art is the standard of the repository, as the project rules describe, a sheet of pieces at a time, then cut, scaled and placed locally. Every prompt carries the key words of this guide, and the states and frames of one piece are painted in one sheet or as edits of the first piece, so they keep its shape and size. The pictures load with the `linear` filter, because they are painted in high definition and the screen scales them.

| Part | Rule |
| --- | --- |
| Look | Polished casual fantasy in high definition, chunky cute shapes and simple bold forms that read at a small size. Characters are super deformed, with a head about 40 percent of their height, seen from the side in a slight three-quarter view and facing right. |
| Outline | A bold clean outline of even thickness in very dark navy `#1E1B33` around every character, prop and item, and `#15172A` around the interface pieces. Tiles, terrain and backdrops have none, so they join without seams. |
| Shading | Soft cel shading with two tones per material and one soft highlight, light from the top left, clean flat areas without noise or texture. |
| Palette | Bright saturated colors: fresh greens `#7BC74D`, golden sand `#F3D58D`, tropical water `#2FB3C4`, warm wood browns, royal blue `#2F86F6` and warm yellow `#FFC93C`, and near white `#F2F3FF` interface pieces that the theme colors with `colorize`. |
| Sprites | `content/sprites/`: the robot knight of 192 pixels, the bunny of 64 by 80, the rock of 96 by 64 and the tree of 128 by 192, each standing on its bottom edge and drawn at about half its size, the adventurer whose hat, shirt, trousers and boots the part mask marks in red, green, blue and yellow, a walker of 18 frames in cells of 256 with a run cycle of eight frames, built by `tools/sprites/pack-walker.py` from three painted sheets, the gem atlas with trimmed frames of 96 and the slime atlas with its speech bubble in cells of 96, a nine-slice whose straight edges stretch while its tail stays in the bottom-left corner, the crystal turret of the bullet test in two cells of 256, its stone base and its floating crystal, the four orbs of 64 that the algorithms, audio and components tests send moving, and a rope that tiles along a stroke. The glows of the bullets and the skies are gradients that the tests paint at run time, and the meadows behind the sprites are the painted strips of `content/camera/`. |
| Scenery | `content/camera/`: the props of the island of the camera tests, a shrine, a cottage, a well, a signpost, a stump, a log, mushrooms, a flowering bush, reeds, lily pads, a barrel and a campfire, painted in one sheet and packed by `tools/camera/pack-props.py`, which the tests set on the painted terrain of `content/tiled/` with its trees, rocks and bushes, and seamless strips of mountains, hills, clouds, the ground and grass about 1024 pixels wide, cut where the painted strip repeats, or for the hills and the ground joined into a loop by `tools/tile-strip.py`, which overlaps the two ends of a painting and switches from one to the other along the path where they differ least. The island and the knight of 16 pixels of the pixel snap tests stay pixel art of a small palette, drawn by `tools/camera/draw-pixel-art.py`, because those tests show pixel art. |
| Effects | `content/effects/`: the sun of 512 pixels and the moon of 256 of the sky of the effect tests, without an outline like the other backdrops, and the unlit campfire of 256 by 160 and the torch of 96 by 256, whose flames are particles. |
| Tiled | `content/tiled/`: tiles painted at four times the grid of the maps, 128 pixels for a cell of 32, with the shores of the water, the sand, the paths, the lava in its crust of scorched rock and the edge of the grass over the sand blended by the corner sets of `transitions.png`, which `tools/tiled/transitions.py` draws from the painted textures so every pair of tiles that share corners joins without a seam, and maps of real places laid out by `tools/tiled/nature.py`, whose lakes, roads and beaches lie on the corners of the cells and whose props stand on open ground, cut from seamless painted ground, water and lava textures whose animation frames ripple the same texture in place, with the props, the coin, the slime, the torch and the other objects painted in one sheet, the corner Wang set of the shore drawn from the field of its corners over the painted water and sand, isometric and hexagonal tiles cut from the same textures, sheared so neighbouring cells continue them, the hero of the y sorting test of 128 by 192 drawn at 32 by 48, the props of the island of the Camera tests that the scenery tileset names as an image collection, the roofs of the group layers test seen from straight above, painted for it and packed by `tools/tiled/pack-roofs.py`, the sea under that island, a frame of the painted water, and the backdrops of the parallax map, whose sky is a gradient of a clear day that `tools/tiled/images.py` draws, whose clouds are the strip of the Camera tests and whose ground is a strip seen from the side, with its grass in front of the foot of the near hills. Each tile sits in a gutter of its own edge pixels, the tilesets draw their tiles at the size of the grid with their collision shapes, and every Tiled test loads its map with the `linear` filter, since none of them shows pixel art. The tilesets and maps keep their layout and tile ids. |
| Nine-slice | `content/nine-slice/`: one atlas of near white pieces, the button, its pressed state, the panel, the knob, the ribbon, the groove and the fill, painted at twice the size they draw at, the near white panel on a trimmed atlas frame, and a wooden frame of 192 pixels around a striped middle, whose stripes show stretching against tiling, alone and cut into nine pieces. |
| Interface | `content/interface/`: five item icons of 128 pixels next to the eight of the fantasy theme, two avatars of 128, one landscape that tints into dusk and night, the wooden sign with its hover and pressed states and the touch controls. |
| Village | `content/lighting/scenery.png`: the cottage of 664 by 488 and the stone house of 475 by 618, whose windows are open so the lit glass that the lighting tests draw behind them shows through, the sun and the moon of about 290 pixels, the neon sign of 324 by 381 with its tubes painted again on their own so they glow apart from the sign, and the wall torch of 109 by 312, each on a transparent background, with the houses standing on their bottom edge. |
| Particle props | `content/particles/props.png`: the confetti cannon, the fountain, the spaceship, the grinder and the campfire of stones and logs that the particle tests emit from, of about 260 to 450 pixels, each on a transparent background. |
| Physics | `content/physics/`: three sheets painted on a transparent background and cut by `tools/physics/pack-art.py` into one picture per object, trimmed to what shows: a car seen from the side with empty wheel arches and its wheel, a race car seen from above, a lunar lander, a spaceship seen from above and a bronze cannon barrel, a wooden crate, a steel crate and an explosive barrel that boxes about as wide as tall wear, a wrapped candy, a plush bear and bunny, a goal flag and a coin, and four planets of 384 pixels, a grassy one, an icy moon, a desert and a gas giant. Every other body of the Physics tests draws in a material of `categories/physics/look.lua` with the same outline, two tones and highlight, the ragdolls wear outfits of capsules, the heroes are the walker of the Sprites tests, and the scenery repeats the sky, the mountains, the hills and the ground of `content/camera/`, or paints a starry sky for the tests in space. |
| Others | The fighter of four frames of 491 by 408 pixels with its training dummy and the cutout puppet of the Animation tests, whose limbs are capsules that overlap at round joints, built by `tools/animation/pack-fighter.py` and `tools/animation/pack-puppet.py`, the parts of the tank of the Nodes tests with the road wheels that turn inside its track, built by `tools/nodes/pack-tank.py`, the badge of the Events tests, the album cover of 512 pixels that the now playing test of the Audio tests hands the controls of the system, the portrait of the smith of 256 pixels, the sheet of the tavern, the cottage, the park and the field of 256 pixels each and the red team of the fighter, recolored from its sheet by `tools/algorithms/recolor-fighter.py`, of the Algorithms tests, the planet of the Shaders tests, the prompts and pictures of rich text, and the icons of a folder, a text file, a data file, a binary file, a missing file and the folder above in `content/files/icons/` and of the cloud and the device in `content/delivery/icons/`, drawn as SVG documents in the outline and palette of this guide. The maps of the Content delivery tests draw with the painted tiles and corner sets of `content/tiled/`. The pixel art hero, the noise, the pattern and the ramps of the Shaders tests and the bitmap fonts of the Text tests stay data that their tests read pixel by pixel. |

## Art of the particle library

The particle pieces of `content/particles/library/textures/` are painted for the project with the `codex` command line and its `$imagegen` skill, sheets of sixteen pieces at a time, then cut, trimmed, scaled and packed by the tool `tools/particles/pack-particle-textures.py` into the atlas `effects.png`, whose file `effects.json` names every piece, and the frames of every effect file move with them. The frame sequences, such as the explosion, the impact burst, the flame, the lightning bolt, the electric beam and the splash crown, are painted as one sheet each, cut into frames that keep their place in the cell so they stay registered, and packed into `sequences.png`, and the pixel art pieces of the pixel effects are packed into `pixel.png` the same way, and the glow line of the ribbons and the sheet of runes that `frameGrid` cuts stay images of their own. The tool `tools/particles/write-effect-library.py` builds every other effect of the library from these pieces as layers and writes the catalog. Every prompt carries the key words of this guide.

| Part | Rule |
| --- | --- |
| Look | Polished effect pieces of finished casual and action games in high definition, bold clean rounded shapes that read at a small size, seen straight from the front. |
| Light pieces | Glows, flares, sparkles, rays, rings, discs, swirls, spirals, slashes, streaks, magic circles, icons such as hearts, skulls, notes, bolts and arrows, orbs, beams, electric arcs and soft flames are pure white light with a smooth soft glow and no outline, painted on pure black, whose brightness becomes their alpha, so the colors of an effect tint them and additive blending adds their light. The soft dot also comes premultiplied for the premultiplied blending. |
| Realistic pieces | Flames, fireballs, smoke, smoke curls, embers, the splash crown and dust clouds are painted in grayscale with soft natural detail on pure black, white where they are hottest or densest, and become white pieces whose brightness is their alpha, so a flame takes the colors of its effect and smoke its gray. |
| Solid pieces | Clouds, cartoon puffs, rocks, dust puffs, leaves, petals, drops, bubbles, snowflakes, feathers, coins, gems and confetti are cel shaded with two tones and one soft highlight, light from the top left, on a transparent background and without dark outlines: clouds have a thin light blue rim, rocks are warm brown, the coin is gold and drops are light blue, and the others are white with light cool gray shading so a tint recolors them. |
| Pixel pieces | Crisp retro pixel art on a strict grid of art pixels, without anti-aliasing, gradients or blur, with one darker shade and one highlight, in the palette cream `#FFF8E0`, yellow `#FFD84A`, orange `#FF9A2E`, red `#E8452C`, dark red `#8A2342`, dark smoke `#4A3A48`, light gray `#C8C4CC`, gray `#8C8494`, light cyan `#9BE7FF`, gold `#FFE36E` and white, painted on flat magenta, which becomes transparent, and cut by reading every art pixel from the middle of its block and snapping it to the palette. The explosion is 32 art pixels, the bolt, flame, ring and smoke 16, and the coin, dots, leaf, star, bubble, drop, spark and streak 8, and pieces that the effects tint are white. |
| Sizes | Pieces of 512 pixels for the magic circles, rings and the disc, painted at 1024 and scaled down so a circle of several hundred units stays sharp, of 256 for rays, flares, swirls, smoke and fireballs, of 192 for flames, of 128 for icons, glows and puffs, and of 64 to 96 for small pieces, while the frames of a sequence are 256 pixels for the explosion, the impact and the splash crown, 192 by 256 for the flame, 192 by 512 for the bolt and 512 by 192 for the electric beam. The frames of one sheet, such as the flames, fireballs, smoke, leaves, petals and the twinkling sparkle, are painted in one sheet, so they keep their style, and the sparkle frames keep their sizes relative to each other. |

The effects load the atlas with the `linear` filter, because the art is painted in high definition and the screen scales it, and the pixel art with the `nearest` filter.

## Codes

### Graphics

#### Sprites (SPR)

Sprites, atlases, animation, batches, pools, render targets, shapes and text of the 2D renderer. The tests are in `source/categories/sprites/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `SPR-001` | Sprite basics | Pivot, rotation, scale, flips, tint and flash on one sprite. |  |
| `SPR-002` | Atlases and sheets | A hash atlas with trimmed frames, an array atlas with tags and a slice, and a grid sheet. |  |
| `SPR-003` | Animation | Clips, loop modes, frame and finish events and a queue of clips on an animator. |  |
| `SPR-004` | Sprite batches | Many sprites of one texture kept in C++ and drawn in one batch, added, changed and removed. |  |
| `SPR-005` | Static batches | A painted map baked once to the GPU and drawn every frame without uploads, under clouds that drift with parallax offsets. |  |
| `SPR-006` | Layers and y-sort | Heroes among trees sorted by the y they stand on, a selected hero raised above every layer, and a row that sort ids keep in place. |  |
| `SPR-007` | Pooled bullets | A bullet hell whose projectiles come from an object pool and draw in one batch. |  |
| `SPR-008` | Bouncing bunnies | From a few to many thousands of sprites with float buffers or one table each, with the frame rate. |  |
| `SPR-009` | Render targets | An offscreen canvas drawn every frame and used as a texture in several ways. |  |
| `SPR-010` | Primitives | Lines, polylines, rectangles, circles, rings, arcs, polygons, meshes, rounded shapes, ellipses, strokes along curves and fields of points. |  |
| `SPR-011` | Text | Sizes, colors, outlines, shadows, alignment, wrapping, anchors, rotation and measuring. |  |
| `SPR-012` | Vector images | SVG icons in many sizes, a swarm of hundreds that turn and an icon that grows, rasterized on worker threads into an atlas. |  |
| `SPR-013` | Recoloring by parts | One adventurer in many outfits from a base image and a mask of four parts, and a crowd of a sprite batch in one draw call. |  |
| `SPR-014` | Tile layers | An endless painted world in chunks of tile layers that stream in around the camera and bake only when their cells change. |  |
| `SPR-015` | Strokes and meshes | Lines drawn with the pointer and stroked with joins, caps, widths, colors, dashes and a tiled texture, and a kept mesh that waves in place. |  |
| `SPR-016` | Groups and masks | A round portrait and a shaped card drawn through masks, fog of war cut open around the pointer and sprites that fade as one group. |  |
| `SPR-017` | Images and runtime atlases | A badge composed from images off the frame thread, sprites drawn from separate textures or one atlas packed at run time, and mipmaps. |  |
| `SPR-018` | Screen capture | The whole screen captured into an image without the frame thread waiting, shown and saved as a PNG file. |  |
| `SPR-019` | Batches of several textures | A meadow of sprites from four separate images in one batch and one draw call, swayed by a material through the data of each sprite. |  |
| `SPR-020` | Blend modes | Every blend mode of a group over the same picture, from the blends of the GPU to overlay, darken, lighten, the dodges and burns, the lights, difference and exclusion, which mix with what lies below. |  |

#### Animation (ANI)

Frame data with boxes, points and events, animated batches, state graphs and skeletons. The tests are in `source/categories/animation/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `ANI-001` | Frame data | A punch from an atlas whose hitbox, hand and pivot follow the flips, turns and speed of the fighter, with frame events and a version on ticks. |  |
| `ANI-002` | Animated crowd | Up to 8,000 fighters of one animated batch punching at their own speeds, advanced and drawn with one call each, with waves of jabs. |  |
| `ANI-003` | State graph | A walker whose idle, run and attack clips a state graph picks from its speed, facing and an attack trigger, with the graph lit as it runs. |  |
| `ANI-004` | Skeletons | A cutout puppet that crossfades walk and idle, waves on a second track, flaps a cape mesh, reaches for the pointer and swaps skins. |  |

#### Nodes (NOD)

Retained trees of 2D nodes with parenting, culling, prefabs and saved descriptions. The tests are in `source/categories/nodes/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `NOD-001` | Node tree | Tanks of one prefab whose turrets aim at the pointer and wheels turn, culled as the camera pans, added, destroyed, hidden and saved. |  |

#### Camera (CAM)

The 2D camera following, framing, shaking, turning and splitting the view, with parallax, pixel snap, picking, debug drawing, zones, rails and pixel canvases. The tests are in `source/categories/camera/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `CAM-001` | Dead zone and drag margins | Following a walker that moves freely inside a dead zone or inside drag margins. |  |
| `CAM-002` | Smoothing and look-ahead | Position smoothing and a view that leads the walker by its velocity. |  |
| `CAM-003` | Limits | A view that stays inside limits, stopping at them or easing into them. |  |
| `CAM-004` | Zoom | Zoom around the pointer with the wheel, a pinch, keys or triggers, within limits. |  |
| `CAM-005` | Framing several targets | A camera that keeps three walkers in view, zooming out as they spread. |  |
| `CAM-006` | Shake | Trauma shake in every direction and directional recoil. |  |
| `CAM-007` | Rotation | A turning view with rotation smoothing, and "ignoreRotation" keeping it upright. |  |
| `CAM-008` | Split screen | Two walkers, each with a camera in its own half of the play area. |  |
| `CAM-009` | Minimap | A second camera in a corner viewport whose visibility mask draws a flat map of the island and leaves the painted ground and the props out. |  |
| `CAM-010` | Blending cameras | Smooth cuts between a walker camera and a landmark camera. |  |
| `CAM-011` | Parallax layers | Mountains, hills, clouds, the ground and grass scrolling at their own rates. |  |
| `CAM-012` | Pixel snap | Pixel art with and without snapping the view to whole pixels. |  |
| `CAM-013` | Screen to world | Picking the props of a clearing under the pointer in a zoomed and rotated view. |  |
| `CAM-014` | Debug drawing | The view, the limits and the drag box drawn from a zoomed-out camera. |  |
| `CAM-015` | Zones and rails | A camera rig that takes the limits, zoom and offset of the zone the walker is in, and follows along a rail. |  |
| `CAM-016` | Pixel canvas | A low-resolution canvas scaled up that scrolls in whole pixels or smoothly by the part of a pixel. |  |

#### Text and fonts (TXT)

TrueType, OpenType and bitmap fonts, families and fallbacks, complex scripts, wrapping, fitting text in a box, rich text, effects and measuring. The tests are in `source/categories/text/`. Each test draws on a page that fills its play area with a margin around it, at the zoom that fits the smallest size of the page, and spreads its content over the room the area adds, so every screen shows the same page at its own scale. Bitmap fonts draw at sizes where each pixel of the font covers whole pixels of the screen.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `TXT-001` | TrueType and OpenType sizes | A TrueType and an OpenType font from 12 to 96 units, and one word zoomed from 8 to 320, all from one distance field atlas. |  |
| `TXT-002` | Outline, shadow and glow | Outlines, shadows with blur and glows drawn from the distance field of the font. |  |
| `TXT-003` | Font families | A family with real bold, italic and mono faces next to families whose bold and italic are synthesized, and plain text underlined and struck through. |  |
| `TXT-004` | Fallback fonts | Japanese, Chinese and symbols drawn by fallback fonts that the family resolves character by character. |  |
| `TXT-005` | Bitmap fonts | A bitmap font in the text format, a colored bitmap font in the binary format and a grid font of LCD digits. |  |
| `TXT-006` | Alignment | Left, center, right and fill alignment, and the anchor point of a text block. |  |
| `TXT-007` | Complex scripts and right-to-left text | Arabic, Persian, Urdu, Hebrew, Hindi and Thai, right-to-left text with English and numbers inside and a typewriter. |  |
| `TXT-008` | Wrapping | Words that wrap at a width, lines that break between Chinese and Japanese characters, long words and line spacing. |  |
| `TXT-009` | Rich text tags | Every tag of the markup: styles, colors, sizes, outlines, links, hints, images, icons, paragraphs, lists, rules, tables and drop caps. |  |
| `TXT-010` | Effects and typewriter | The built-in effects with their attributes, and a dialogue revealed like a typewriter with pauses and speed changes. |  |
| `TXT-011` | Custom effects | Text effects and inline icons registered from Lua. |  |
| `TXT-012` | Measuring text | Text sizes, glyph quads, ascent, baseline and line height, glyph metrics, shaped advances with kerning, and rich text layouts. |  |
| `TXT-013` | Outlines and small text | Outlines that reach the letters beside them at every size, and small text that moves at its exact place or snapped to whole pixels. |  |
| `TXT-014` | Fitting text in a box | A name that shrinks to fit the width of its plate and a story that shrinks until its lines fit the height of its box, down to the smallest scale. |  |

#### Lighting (LIT)

Lit canvases with ambient, point, spot and directional lights, shadows, normal maps, emission, masks and many lights at once. The tests are in `source/categories/lighting/`, and the village of the street scenes, with its houses, lamp posts, sky and hills, is drawn by `source/categories/lighting/town.lua`, which the particle tests share.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `LIT-001` | Ambient light | The color a lit canvas starts from before any light, from noon to a dark cave. |  |
| `LIT-002` | Point lights | Colored point lights with their own radius and intensity, one on the cursor and more where you place them. |  |
| `LIT-003` | Spot lights | Cones that aim at the cursor and sweep the room, with their inner and outer angles. |  |
| `LIT-004` | Directional light | A sun that covers the whole canvas from one direction and casts long shadows. |  |
| `LIT-005` | Blend modes | Lights that add, subtract, mix or keep the maximum in the light map. |  |
| `LIT-006` | Intensity above 1 | Lights brighter than the unlit colors in a floating-point light map. |  |
| `LIT-007` | Shadows | Occluders, the "none", "pcf5" and "pcf13" filters, shadow color and smoothness. |  |
| `LIT-008` | Physics and Tiled occluders | Shadows cast by falling physics crates and by the walls of a Tiled object layer. |  |
| `LIT-009` | Normal maps and specular | Bricks and studs with normal maps generated in Lua, lit by the angle of the light with highlights. |  |
| `LIT-010` | Unshaded and emissive | A night street with signs that keep their colors in the dark through "unshaded", and windows and a neon sign that glow through "emission". |  |
| `LIT-011` | Light masks and layers | Lights that reach only the draws whose mask and layer they select. |  |
| `LIT-012` | Flicker | Wall torches and a campfire whose lights waver like flames, each with its own seed. |  |
| `LIT-013` | Day and night | A day cycle written in Lua over a village: the sky, the ambient color, the sun and the moon, the stars, street lamps and windows that light up at dusk. |  |
| `LIT-014` | Lit render targets | Lit canvases drawn into render targets and shown as screens. |  |
| `LIT-015` | Many lights | Hundreds of moving lights with the renderer counters. |  |

#### Nine-slice (NSL)

Frames cut into nine regions that stretch or tile, from borders or pieces, scaled, tinted, in UI themes and resized by hand. The tests are in `source/categories/nine-slice/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `NSL-001` | Stretch and tile | One framed image cut by borders, with its edges and center stretched or repeated. |  |
| `NSL-002` | Nine pieces | A frame made of nine separate regions of a sheet instead of borders. |  |
| `NSL-003` | Scale and tint | The same frame with its borders scaled and its colors tinted. |  |
| `NSL-004` | UI theme surfaces | Panels, buttons, sliders and progress bars drawn with nine-slice theme surfaces, colorized by each component. |  |
| `NSL-005` | Resizable panel | A panel dragged and resized by its edges and corners with the mouse, a finger or a gamepad. |  |
| `NSL-006` | Slices of trimmed atlas frames | A nine-slice of an atlas on a frame the packer trimmed, which must read the same pixels as the frame. |  |
| `NSL-007` | Edge cases | Frames that draw at fractional places and scales with linear filtering, tile off their grid, shrink below their borders and come from a sheet of twice the resolution. |  |

#### Particles (PRT)

Particle emitters from Lua tables, effect files and composite effects: weather, fire, bursts, delays, spawn and image shapes, direction modes, forces, turbulence, attractors, size, speed and spin curves, color stops and tints, rotation and frame modes, collisions with floors, walls and physics worlds, bounds, draw order, moving emitters, sub-emitters, trails and ribbons, lights and shading, pixel art, blending and tens of thousands of particles. The tests are in `source/categories/particles/`, and the composite effects they load in `content/particles/composites/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `PRT-001` | Fire | Flames, embers and smoke from three emitters that follow the cursor. |  |
| `PRT-002` | Smoke | Chimney smoke that starts fully grown with prewarm and bends in the wind. |  |
| `PRT-003` | Explosion | A flash, fireballs, debris, a shock ring and smoke in one burst. |  |
| `PRT-004` | Rain | Streaks across the whole view and splashes on the ground. |  |
| `PRT-005` | Snow | Two layers of flakes that drift and spin in the wind. |  |
| `PRT-006` | Sparks | Sparks from the disc of a grinder at the cursor, thrown from a cone with gravity and damping, stretched along their motion and bouncing on the floor of a physics world. |  |
| `PRT-007` | Trails | Trails in world space that follow the cursor and a comet. |  |
| `PRT-008` | Magic | A swirling orb with radial and tangential acceleration and twinkling stars. |  |
| `PRT-009` | Confetti | Colored confetti that flips through frames as it falls. |  |
| `PRT-010` | Fireworks | Rockets with trails that burst into colored stars. |  |
| `PRT-011` | Emitter shapes | Point, circle, ring, rectangle and cone spawn areas side by side. |  |
| `PRT-012` | Bursts and prewarm | Scheduled bursts, one-shot cycles and prewarm against a cold start. |  |
| `PRT-013` | Local and world space | Particles that stay behind a moving emitter against ones that move with it. |  |
| `PRT-014` | Gravity and accelerations | Gravity, radial and tangential acceleration and damping on sliders. |  |
| `PRT-015` | Color, size and frames | Color and size over the lifetime and frame animation. |  |
| `PRT-016` | Blend modes | The same particles with alpha, additive, multiply, screen and premultiplied blending. |  |
| `PRT-017` | Effect files | Effects loaded from ".particles" files in the content folder. |  |
| `PRT-018` | Many particles | Tens of thousands of particles with their live count. |  |
| `PRT-019` | Color stops and tints | Colors spread evenly, placed at "colorTimes" and held with "colorBlend" set to "steps", and confetti tinted from "tints" at random, in order or by the cycle time. |  |
| `PRT-020` | Size, speed and spin curves | Pops whose "sizeCurve" grows past the end size, sizes kept with "endSizeScale", dust that stops with a "speedCurve" and coins that spin down with a "spinCurve". |  |
| `PRT-021` | Rotation, stretch and frames | Random start rotations, "rotationStep", streaks aligned with their motion and stretched by their speed, leaves of many widths from "aspect", and the four "frameMode" values on a sheet cut by "frameGrid". |  |
| `PRT-022` | Turbulence and attractors | Smoke that curls with "turbulence" from the chimney of a house, and coins that jump out of a chest, fly to an attractor at the cursor and vanish at its kill radius. |  |
| `PRT-023` | Collision | Sparks that bounce, stick or die on a floor, balls inside the walls of an area and drops that hit the bodies of a physics world given with "setCollisionWorld". |  |
| `PRT-024` | Bounds and draw order | Rain that dies at the ground with "bounds", snow that wraps around a box, and smoke drawn "oldestFirst" next to "newestFirst". |  |
| `PRT-025` | Shapes and direction modes | The shapes "ellipse", "rectangleEdge", "arc", "polygon" and "polyline" turned and thickened, the four direction modes, and a polyline that follows the cursor through "setShapePoints". |  |
| `PRT-026` | Image shapes | A hero sprite read as an "imageShape" that sparkles on its visible pixels and breaks apart into pieces in its own colors with "colorFromImage". |  |
| `PRT-027` | Moving emitters | Exhaust that inherits the velocity of a spaceship that flies a figure eight until the cursor moves and then follows it, trails by "rate" and by "rateOverDistance", and the "scale" and "rotation" of an emitter on sliders. |  |
| `PRT-028` | Delays and burst cycles | A launch whose parts start in sequence with "delay", and crackles, sputters and pulses from bursts with cycles, intervals, probabilities and count ranges. |  |
| `PRT-029` | Sub-emitters | Rain that splashes where it dies, fireworks whose stars crackle when they die, debris that raises dust at every hit and comets that leave dust while they live, with "inheritColor" and "inheritVelocity". |  |
| `PRT-030` | Composite effects | An explosion of six parts with delays and a campfire of four parts from composite effect files, moved, scaled and turned as one, with the smoke switched off through "emitter". |  |
| `PRT-031` | Lights and shading | A night clearing lit by a flickering fire "light", embers and fireflies with "particleLights", flames with "emission" and "unshaded" runes, next to the same scene unlit. |  |
| `PRT-032` | Trails and ribbons | Neon sparks that each draw a "trail", and a blade ribbon of "newTrail" that follows the cursor with its width and lifetime on sliders. |  |
| `PRT-033` | Pixel art effects | Pixel leaves, explosions, coins, smoke and flames with "pixelSnap", "rotationStep" and "steps", next to the same effects without them. |  |
| `PRT-034` | Effect library | Every effect of "content/particles/library" by category from its "catalog.json", one-shot effects fired again on a press and by themselves, looping effects at the cursor, and the scale, rate, speed, lifetime, spread, gravity and damping of every emitter on sliders, with a night canvas for the lights, a coin counter that the coins of the effects fly into and the code of the effect that shows. |  |
| `PRT-035` | Effect library check | Every effect file of the library loaded on the worker threads, its emitters created and run, and the files of any effect that fails, grouped by their error, which fails the test. |  |

#### Screen and sprite effects (EFX)

Camera shake and flashes, distortion, dissolving sprites, outlines and glows, the post-processing chain of world canvases and the effects of images. The tests are in `source/categories/effects/`, and they draw the landscape of `source/categories/effects/scenery.lua` from the images of the camera and sprite tests and the painted sun, moon, campfire and torch of `content/effects/`, by day or by night.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `EFX-001` | Shake and flash | Hits on three knights that add camera trauma, whose shake decays with "traumaDecay" on a meter in the corner, flash the view with "camera:flash" and flash the struck knight with the "flash" of its draw, next to a directional recoil. |  |
| `EFX-002` | Heat haze and shock rings | Draws with a "distortion" in a canvas with post-processing: heat haze over a campfire, shock rings from the cursor, a rippling shield and a glass bubble that follows the cursor, with the bend distance on a slider. |  |
| `EFX-003` | Dissolve | Sprites that take turns to dissolve into noise with a colored edge and come back, with their names fading with them, and the duration, the edge width, the noise cell size and the edge color on controls. |  |
| `EFX-004` | Outline and glow | A selection outline on the sprite nearest the cursor and pulsing glows around the sprites picked with a press, by day and by night, with widths, sizes and colors on controls. |  |
| `EFX-005` | Post-processing chain | Bloom, vignette, saturation, contrast, brightness, color grading through lookup textures made in Lua, chromatic aberration, pixelation, blur, film grain and dithering over a scene with a campfire and a torch at the cursor, opening on the dream preset, with presets and the whole chain on a switch. |  |
| `EFX-006` | Image effects | Blur, sharpening, brightness, contrast, saturation, hue, pixelation, inversion and outlines applied to images on the task pool before they become textures, opening with a light blur and livelier colors beside the picture and the sprite as they are. |  |

#### Scenes and transitions (SCN)

The scene stack with every transition effect, custom effects, loading with progress and errors, every hook, transparent overlays and the game pause. The tests are in `source/categories/scenes/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `SCN-001` | Transition gallery | Every built-in effect, with pickers for the direction, the easing, the duration and the color. |  |
| `SCN-002` | Custom effect | A transition effect written in Lua that draws both scenes itself. |  |
| `SCN-003` | Loading and errors | A fade as the loading screen, a custom loading view with progress, preloading and a failing load routed by "onError". |  |
| `SCN-004` | Stack and hooks | Push, pop, replace and "popTo" with the stack and every scene hook, from "load" to "unload", shown on screen. |  |
| `SCN-005` | Transparent overlays | Overlay scenes that let the scenes below keep rendering while only the top one updates. |  |
| `SCN-006` | Pause and process modes | A paused world under a working pause menu, with "pausable", "whenPaused" and "always" timers and tweens. |  |
| `SCN-007` | Loading screen that fades out | A full-screen loading screen over a map that raises its terrain while it loads, fading out over the finished map, with the events of the change as they come. |  |

#### Shaders (SHD)

Custom fragment shaders compiled into ".shader" files: materials on sprites and text, uniforms from tweens and textures, post-processing chains, hot reload and a library of game shaders. The tests are in `source/categories/shaders/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `SHD-001` | Sprite materials | Dissolve, outline, hit flash, wave distortion, palette swap, hologram and pixelation shaders on sprites. |  |
| `SHD-002` | Uniforms with tweens | Shader values driven by tweens, yoyos, elastic curves and a timeline. |  |
| `SHD-003` | Textures as uniforms | A material that reads a gradient and a pattern texture, including a live render target. |  |
| `SHD-004` | Post-processing chain | The built-in vignette followed by custom blur, color grading and tube screen passes over a canvas. |  |
| `SHD-005` | Shaders on text | Rainbow, dissolve and flash materials on text drawn from its distance field. |  |
| `SHD-006` | Hot reload | How a desktop run of the player recompiles and reloads a shader while the app keeps running. |  |
| `SHD-007` | Shader load time | Every shader of the category loaded in the background and drawn at once, with the time of the files and of the first frame on the backend of the platform. |  |
| `SHD-008` | Shader library | Forty-five material shaders of games by category, from neon trails, lasers and shields to fire, frost, gold, glitches and shockwaves, with their numbers on sliders. |  |

#### Video (VID)

Video files decoded by the decoder of each platform into a texture that sprites and the UI draw, with their sound through the mixer. The tests are in `source/categories/video/`, and the clip in `content/video/`, which `tools/video/generate-content.py` draws.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `VID-001` | Video playback | A clip that plays, pauses, seeks, loops and changes its rate and volume, drawn as sprites and as an image of the UI, with its sound on the music bus. | linux, headless |

### Gameplay

#### Algorithms (ALG)

Path finding, spatial queries, procedural generation, geometry and game AI, each on a world you paint, drag or aim at. The tests are in `source/categories/algorithms/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `ALG-001` | A* on a grid | A* with its four heuristics, weighted A* and jump point search side by side on walls you paint. |  |
| `ALG-002` | Hex and isometric grids | Paths on hexagonal, isometric and staggered grids laid out like Tiled maps. |  |
| `ALG-003` | Waypoint graph | A* and Dijkstra over waypoints with weights, and doors that close. |  |
| `ALG-004` | Flow field | Hundreds of units heading to the pointer through one field computed on a worker thread. |  |
| `ALG-005` | Dijkstra and flee maps | Monsters that chase the player downhill on a Dijkstra map and others that flee on its flee map. |  |
| `ALG-006` | Hierarchical path finding | HPA* on a 256 by 256 map against A* and jump point search, with local updates. |  |
| `ALG-007` | Navigation mesh | A navigation mesh with funnel paths, an agent radius and obstacles you add. |  |
| `ALG-008` | Crowd | Hundreds of agents avoiding each other with ORCA, with separation, alignment and cohesion. |  |
| `ALG-009` | Spatial structures | A spatial hash, a quadtree, an AABB tree and a k-d tree answering queries under the pointer. |  |
| `ALG-010` | Field of view | Symmetric shadowcasting on a grid, Bresenham lines and circles, and a visibility polygon. |  |
| `ALG-011` | Flood fill and regions | The paint bucket and the connected regions of a map, with union-find joining islands. |  |
| `ALG-012` | Scatter | Objects spread by area and density inside polygons, with exclusions, biomes and weights. |  |
| `ALG-013` | Poisson disk | Evenly spread points whose spacing follows a density map. |  |
| `ALG-014` | Cellular caves | Caves grown by a cellular automaton, or dug by drunkard walkers, on a worker thread. |  |
| `ALG-015` | Wave Function Collapse | Tile maps that follow adjacency rules, with tiles you pin in place. |  |
| `ALG-016` | Dungeons | Rooms and corridors from binary space partitioning or from random placement. |  |
| `ALG-017` | Mazes | Perfect mazes from the recursive backtracker, Prim and Kruskal, solved with A*. |  |
| `ALG-018` | Voronoi and Delaunay | Voronoi cells, the Delaunay triangulation and Lloyd relaxation of points you add. |  |
| `ALG-019` | Autotiling | Tiles picked from their neighbors with 4-bit masks, 47-tile blob masks and a Wang set. |  |
| `ALG-020` | Polygon booleans | Union, difference, intersection, exclusion, offsets and convex decomposition of polygons with holes. |  |
| `ALG-021` | Marching squares | Outlines traced from a field you paint, simplified with Ramer-Douglas-Peucker. |  |
| `ALG-022` | Splines | Catmull-Rom, Bezier and B-spline curves walked at a steady speed. |  |
| `ALG-023` | Behavior trees | Guards that patrol, chase what they see and rest, driven by behavior trees. |  |
| `ALG-024` | Utility AI | Villagers that weigh hunger, energy and fun with response curves. |  |
| `ALG-025` | Influence maps | Two armies spread their threat over a map and read it to advance or retreat. |  |
| `ALG-026` | Ray casts without physics | Rays against segments, rectangles, circles, polygons, chains, grids and spatial trees, with bounces and fans. |  |
| `ALG-027` | Overlap boxes | Fighters of two teams on a belt of depth rows whose punch boxes reach the other team within a depth band, with rests after every hit, without physics. |  |
| `ALG-028` | Conversation | A smith who greets once and then in variants, sells swords while the gold lasts, tells a rumor once, hands commands to the test and saves the walk to resume it. |  |
| `ALG-029` | Colors of a picture | The palette of a picture by median cut, the swatches an interface takes from it, its placeholder text decoded into a blurred image, and text that reads best on each swatch with its contrast. |  |
| `ALG-030` | Smoothing a noisy signal | A wave measured with noise and spikes, and the lines of the low-pass, moving average, median, Hampel and Kalman filters through it, side by side. |  |

#### Audio (AUD)

Music, effects, buses, filters, positional sound, interruptions and many voices with sounds synthesized for these tests. The tests are in `source/categories/audio/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `AUD-001` | Music | Two streamed tracks with crossfades of any length, looping or played once, started partway, repeated from a loop start after an intro, paused, resumed and moved through with the cursor of their voice. |  |
| `AUD-002` | One-shot effects | Volume, pitch, pitch variation, pan, fades and the limit of voices of each sound. |  |
| `AUD-003` | Buses | The bus tree with volumes, mutes and process modes, and what the game pause stops. |  |
| `AUD-004` | Effects | Every filter, the delay and the reverb on a bus or on a voice, with tweened parameters. |  |
| `AUD-005` | Positional audio | Sounds in the world around a listener that follows the camera, with fade models, panning and Doppler. |  |
| `AUD-006` | Interruptions and lifecycle | Audio interruptions, route changes and app states as they happen, and what the background does to the sound. |  |
| `AUD-007` | Many voices | Bursts and rain of voices up to the limit of 128, with the statistics of every bus. |  |
| `AUD-008` | Voice limits | A hundred coins with and without the limit of their sound, a line whose priority outlasts bursts that fill every voice, and a tone that stops and resumes quickly without clicks. |  |
| `AUD-009` | Loudness and output | Thirty explosions with the limiter of the master bus on and off, its threshold and the gain it takes away, and the state and latency of the output with the unlock of a browser. |  |
| `AUD-010` | Now playing | A track in the now-playing controls of the system with its artwork, length and position, whose commands from the controls, media keys, headsets and remotes change what plays. |  |

#### Components (CMP)

Small parts of a game written in Lua, each in one file of `source/categories/components/` that needs only the engine and copies as it is into another project: `label-up.lua`, the numbers and words that pop out of what was hit, healed or collected, `health-bar.lua`, the bar over a character or across the screen, `ecs.lua`, a small entity component system of packed pools and systems, `composition.lua`, entities made of small behaviors that each do one thing, and `charts.lua`, eleven kinds of chart that survive any data. The tests are in `source/categories/components/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `CMP-001` | Label up | Damage, criticals, heals, gold and misses that pop out of a target, rise, stack and fade, from one pool that allocates nothing per frame. |  |
| `CMP-002` | Health bar | Bars that ease to the health with a trail of the health just lost, colors by the share left, ticks, a shield, a flash on hits and a bar that hides while it is full. |  |
| `CMP-003` | Entities, components and systems | Hundreds of balls whose behavior comes only from their components and the systems that run over them, with systems switched off and components taken away while it runs. |  |
| `CMP-004` | Composition | Four entities built from the same small behaviors, which follow, wander, circle, bob, shoot and take hits, with behaviors given to an entity and taken away while it runs. |  |
| `CMP-005` | Charts | Line, area, bar, stacked bar, pie, donut, scatter, radar, histogram, candlestick and heat map charts with data that is empty, negative, huge, missing, a single value or all equal. |  |

#### Events (EVT)

Signals, the event bus, owners that end their listeners, scene scopes, the engine lifecycle, the pause, autoloads and classes. The tests are in `source/categories/events/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `EVT-001` | Signals | Connect, once, priority, deferred, blocking and disconnecting during an emit. |  |
| `EVT-002` | Owners | Listeners that disconnect by themselves when the object or GUI that owns them ends. |  |
| `EVT-003` | Event bus | Channels, filters, priorities, consumed events and events queued for the end of the frame. |  |
| `EVT-004` | Scene scopes | Listeners, timers, tweens, tasks and GUIs of a scene that all end when it unloads. |  |
| `EVT-005` | Lifecycle log | Every engine event as it happens: app states, scene loads and transitions, window, assets, objects and sockets. |  |
| `EVT-006` | Pause | A pause menu that stops the game, and the "paused" and "unpaused" hooks and events. |  |
| `EVT-007` | Autoloads | A player data singleton that lives through every scene, and an autoload added at run time. |  |
| `EVT-008` | Classes | The class helper "haylen.class" with inheritance, "super" calls, metamethods, "is" checks and mixins. |  |
| `EVT-009` | Diagnostics | The live counts of "events.topics()" and "signal.list()", and the counts around the collection of an owner. |  |

#### Input (INP)

The keyboard, the mouse, touches, gestures, gamepads, the action map, remapping, touch controls and a hero that every device moves. The tests are in `source/categories/input/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `INP-001` | Keyboard | Keys held, pressed and released, modifiers, key events with repeats and the text typed with the layout of the platform. |  |
| `INP-002` | Mouse | Buttons, the wheel and the trackpad with their units and the scroll speed, the movement of a frame, cursor shapes, a hidden cursor, a captured mouse and a finger acting as the mouse. | tvos |
| `INP-003` | Touch | Up to ten fingers drawn with their id, phase, trail and time on the screen, and the mouse acting as a finger. | tvos |
| `INP-004` | Gestures | Taps, double taps, long presses and swipes, and pinches that zoom, turn and move a card, with their thresholds. | tvos |
| `INP-005` | Gamepads | Eight live controllers with sticks, triggers and buttons labeled as each controller labels them, their connections, rumbles of both motors and the dead zone. |  |
| `INP-006` | Action map | Button, axis and vector actions bound to keys, mouse, gamepads and touch controls, and the last device used. |  |
| `INP-007` | Remapping | Rebinding the controls with key captures, kept in the preferences for the next launch. |  |
| `INP-008` | Touch controls | A touch stick in its fixed, floating and following modes and touch buttons that drive virtual inputs of the action map. |  |
| `INP-009` | Character | A small platformer hero that every device moves at the same time, with prompts for the last device. |  |
| `INP-010` | Fixed steps | Presses counted in frames and in fixed steps at several time scales, where step edges count each press once, and a jump buffer that catches a press made just before landing. |  |
| `INP-011` | Players | A seat for the keyboard and touch and one for each of eight gamepads, each with an action map that reads only its own devices, joined with the start button of its gamepad. |  |
| `INP-012` | Commands | A command buffer that types runs, a jump attack and specials from keys, gamepads and touch buttons that a sliding thumb presses in turn, with the kept command, its age, the facing and a freeze like hit stop. |  |
| `INP-013` | Replay | A run recorded on fixed steps with the state of its random generator and replayed in place of the devices, with a hash of every step that shows the replay matches. |  |

#### Physics (PHY)

Bodies, materials, sensors, joints and the ready-made ropes, bridges, ragdolls, vehicles, movers, grabbers, force fields, terrain, fracture and fluids of the physics world, the common problems of physics next to their solutions, and the physics of common game cases. The tests are in `source/categories/physics/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `PHY-001` | Bodies and shapes | Boxes, circles, capsules, convex and concave polygons, segments and chains, dragged with a grabber. |  |
| `PHY-002` | Materials | Friction on a ramp, restitution of bouncing balls and density on a seesaw. |  |
| `PHY-003` | Sensors and contacts | A sensor zone that counts its visitors and the contact, hit and sensor events of the world. |  |
| `PHY-004` | Joints | Distance, revolute with a motor and limits, prismatic, weld, wheel, motor and filter joints. |  |
| `PHY-005` | Ropes | Chains of segments pinned in place or hung from bodies, which can be cut. |  |
| `PHY-006` | Bridge | A bridge of planks between two cliffs that bends under falling crates. |  |
| `PHY-007` | Ragdolls | Human figures with limited and stiff joints tumbling down the stairs and dragged over them by any limb. |  |
| `PHY-008` | Vehicle | A car on springy suspension driven with the keyboard, a gamepad or touch pedals over the hills, with the camera following it. |  |
| `PHY-009` | Explosions | Radial impulses with falloff and occlusion that scatter a tower of crates. |  |
| `PHY-010` | Destructible terrain | Ground carved and filled with bombs and tools, with its collision rebuilt chunk by chunk. |  |
| `PHY-011` | Fracture | Objects that break into Voronoi pieces where they are hit. |  |
| `PHY-012` | Liquids | A particle fluid poured into a tank where crates float or sink, stepped in C++ and drawn as metaballs. |  |
| `PHY-013` | One-way platforms | A character that jumps up through platforms, lands on them and drops through them. |  |
| `PHY-014` | Conveyors | Belts that carry crates with the tangent speed of their surface. |  |
| `PHY-015` | Ray and shape casts | Closest and all hits, filters, piercing, bounces, fans, shape casts, batches and picking. |  |
| `PHY-016` | Collision filtering | Categories, masks and groups that decide which bodies collide. |  |
| `PHY-017` | Stress test | Hundreds of bodies with the body count and the time of every step. |  |
| `PHY-018` | Debug drawings | The drawings of the debug module switched on and off: physics shapes and joints, sprite bounds with names, a drawer of the test and the overlay. |  |
| `PHY-019` | Tunneling | Fast balls and crates that pass through thin walls and floors on one side and stop with continuous collision and bullets on the other. |  |
| `PHY-020` | Tile seams | Boxes that catch on the joints between tiles made of one box each, next to the same tiles merged into one chain where they slide. |  |
| `PHY-021` | Slopes and steps | A dynamic capsule that slides down slopes and stumbles on stairs next to a mover that stands on slopes and steps up stairs, both on the same input. |  |
| `PHY-022` | Moving platforms | Platforms that jump to their place every step and lose their loads next to platforms moved by velocity that carry crates, balls and a mover. |  |
| `PHY-023` | Stretchy joints | A rope that stretches under a heavy weight next to one whose length limit holds, and a heavy chain that sags next to a stiffer one. |  |
| `PHY-024` | Sleeping | A pyramid that keeps the solver busy with sleeping off next to one that falls asleep, wakes where it is touched and costs almost nothing. |  |
| `PHY-025` | Smooth slow motion | One world drawn where its last step left it, moving in jumps in slow motion, and drawn interpolated between steps, gliding. |  |
| `PHY-026` | Bounces and hits | Bouncy balls from five heights that stop dead and report no hits under the default thresholds and keep bouncing with low ones. |  |
| `PHY-027` | Dragging | A ragdoll dragged up stairs by a mouse joint sized to one limb, which only scrapes it along, next to a grabber sized to the whole figure. |  |
| `PHY-028` | Top-down car | A car seen from above on a closed track that grips, drifts with the handbrake and pushes cones, driven with keys, a gamepad or touch. |  |
| `PHY-029` | Center of mass | Tall blocks with the center of mass where their shapes put it, which fall over, next to the same blocks with it moved low, which rock back upright. |  |
| `PHY-030` | Breaking joints | Shelves, a bridge and lamps whose joints break when their load passes their break force, with the force of each break shown where it happened. |  |
| `PHY-031` | Platformer | A character that runs, jumps with assists, climbs slopes and steps, passes one-way platforms, rides a lift and pushes crates on its way to the flag. |  |
| `PHY-032` | Pendulums | A cradle of five steel balls that passes an impact through the row, a double pendulum and a swing pushed in time. |  |
| `PHY-033` | Wrecking ball | A crane boom on a revolute motor that swings a heavy ball on a chain into a wall of bricks. |  |
| `PHY-034` | Grappling hook | A character that fires a hook, hangs on a rope from the ceiling, swings, reels in and out and lets go. |  |
| `PHY-035` | Slingshot | A slingshot pulled back and aimed along its predicted path at stacked targets. |  |
| `PHY-036` | Cannon | A cannon aimed by angle and power that fires bullets at a tower, with recoil on its carriage and a count of hits. |  |
| `PHY-037` | Springs | A trampoline, spring platforms on prismatic joints and jump pads that launch crates and balls. |  |
| `PHY-038` | Seesaw and catapult | A seesaw that tips toward the heavier end and a catapult that throws a stone when a weight falls on its arm. |  |
| `PHY-039` | Doors and levers | Flap doors that close with springs, a lever that opens a gate and a pressure plate that raises a drawbridge. |  |
| `PHY-040` | Elevators | Lifts on prismatic motors that carry crates and a character between floors, called with buttons. |  |
| `PHY-041` | Pinball | A pinball table with flippers on motors, bumpers that kick, a plunger on a spring, chain walls and a one-way lane. |  |
| `PHY-042` | Pool | A top-down pool table without gravity, a cue aimed with the pointer or a stick and pockets that take the balls. |  |
| `PHY-043` | Air hockey | A paddle that follows the pointer or a stick against a computer paddle, a fast puck, goal sensors and a score. |  |
| `PHY-044` | Bowling | A lane seen from above with ten pins and a heavy ball thrown with aim, and the count of the pins down. |  |
| `PHY-045` | Dominoes | A line of dominoes along a curve and up steps that fall from one push, with the time of the chain reaction. |  |
| `PHY-046` | Stacking | Blocks of many shapes dropped from a moving dropper into a tower, with its height and its collapses. |  |
| `PHY-047` | Peg board | A board of pegs where balls bounce into slots counted by sensors, with a histogram of the slots. |  |
| `PHY-048` | Magnets | Radial force fields that pull or push only the steel crates by their mask, switched on and off and dragged with the pointer. |  |
| `PHY-049` | Planets | Planets that pull with inverse square force fields in a world without gravity, moons on orbits and ships launched from the pointer that orbit or crash. |  |
| `PHY-050` | Wind | Fans as directional force fields that push leaves, balloons and boxes alike, gusts that change their strength and a vortex for light bodies. |  |
| `PHY-051` | Water | A pool as a buoyancy force field where boats and logs float, an anvil sinks, crates float by their density and the flow makes currents and waves. |  |
| `PHY-052` | Soft bodies | Jelly blobs of a ring of small bodies on springy joints around a center, which squash on impact and recover and are dragged with a grabber. |  |
| `PHY-053` | Cloth | A net of small bodies joined by distance joints and pinned at the top, which catches falling bodies and tears where a joint passes its break force. |  |
| `PHY-054` | Breakables | Crates, vases and glass panes that break into pieces when a hit reported by the world passes the impact each one stands, hit by thrown balls. |  |
| `PHY-055` | Explosive barrels | Barrels that blow up when clicked or hit hard and light the barrels their blast reaches in a chain reaction that scatters the crates. |  |
| `PHY-056` | Thrusters | A lander whose two thrusters push at points off its center, steered with keys, a gamepad or touch, with fuel, a landing speed and a sensor over the pad. |  |
| `PHY-057` | Space ship | A ship without gravity that turns, thrusts and fires, wraps around the edges of the stage and breaks drifting rocks into smaller ones. |  |
| `PHY-058` | Marble run | Marbles rolling down ramps of chains, through a funnel, over a wheel turned by a motor and up a lift back to the top in a loop. |  |
| `PHY-059` | Water wheel | Particle water released by a dam that turns a wheel on a revolute joint, with the speed of the wheel and a pump that brings the water back. |  |
| `PHY-060` | Claw machine | A carriage on a prismatic joint and two fingers on motorized revolute joints that pick up prizes by friction, driven with keys, a gamepad or touch. |  |
| `PHY-061` | Rope cutting | A candy hanging from ropes that a swipe cuts where a ray along the pointer crosses them, so it swings and falls into a basket with a sensor. |  |
| `PHY-062` | Traps | Spinning blades moved as kinematic bodies, a crusher on a prismatic joint and a rolling boulder that throw ragdolls and crates around. |  |
| `PHY-063` | Crate pushing | A top-down puzzle where a character driven by a limited force pushes heavy damped crates onto goal sensors with keys, a gamepad or a touch stick. |  |
| `PHY-064` | Collision report | Impacts that mark their points, normal and kept velocity with their speed and force, and crates at rest that show the force of their weight. |  |
| `PHY-065` | Playground | A room to try every value of a body and the world, with bodies dropped, grabbed, pushed and shot and a card that reads the chosen body. |  |
| `PHY-066` | Two worlds | Two planets with physics worlds of their own and bubbles of pull, where shots from a slingshot move to the world of the icy moon they reach. |  |

#### Procedural generation (GEN)

Worlds, people, critters, monsters and climbing creatures made from a seed and drawn with shapes, whose legs, arms, bodies, antennae and tentacles bend on the chains of `haylen.animation2d`. The tests are in `source/categories/procedural/`, with the cave of distance fields in `cave.lua`, the surface walker in `crawler.lua`, the stepping leg and its gait in `leg.lua` and the names made of syllables in `names.lua`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `GEN-001` | World map | Islands from layered noise with biomes, rivers that run downhill to the sea, towns with generated names and roads that avoid water and mountains. |  |
| `GEN-002` | Characters | A crowd of people built from a seed, with their build, skin, hair, face, clothes and accessories, whose arms bend on chains to wave at the pointer. |  |
| `GEN-003` | Critters | Bugs built from a seed with their bodies, colors, legs, antennae and wings, which scurry on legs that step in turns and flee the pointer. |  |
| `GEN-004` | Monsters | Monsters built from a seed with their bodies, eyes, mouths, horns, spikes, legs and tentacles, which walk or float and roar when touched. |  |
| `GEN-005` | Spider | A spider seen from the side, with eight high-kneed legs that grow from its cephalothorax and step in an alternating wave, that climbs the floor, walls, ceiling and rocks of a generated cave the shorter way around and leaps between rocks on a thread. |  |
| `GEN-006` | Centipede | A centipede whose body follows its head on a chain the shorter way around the rocks of a generated cave, up walls and across the ceiling, turning around over its own body, on dozens of legs that step in a wave from head to tail. |  |

#### Tiled (TLD)

Tiled maps of every orientation with their layers, objects, properties, collision, spawning, y sorting, ray casts, worlds and maps built in code. The tests are in `source/categories/tiled/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `TLD-001` | Orientations | Orthogonal, isometric, staggered, hexagonal and oblique maps, with the cell under the pointer. |  |
| `TLD-002` | Infinite maps | A map stored in chunks that reach negative cells, panned with the pointer, the keys or a stick. |  |
| `TLD-003` | Tile animations | Water, lava, torches, coins and slimes whose tiles play their frames on map time. |  |
| `TLD-004` | Image layers | A sky, clouds, hills and the ground seen from the side that repeat across the view and scroll at their own parallax, with bushes of a tile layer passing in front. |  |
| `TLD-005` | Group layers | Nested groups that pass their offset, tint, opacity and parallax on to their layers. |  |
| `TLD-006` | Objects and templates | Every object shape and object templates with their overrides, picked under the pointer. |  |
| `TLD-007` | Properties | Custom properties of every type on the map, its layers, its objects and its tiles. |  |
| `TLD-008` | Collision | Physics bodies built from tile collision shapes and collision objects, with sensors and filters. |  |
| `TLD-009` | Spawning | Entities created from objects by factories keyed by object class. |  |
| `TLD-010` | Y sorting | A character walking behind and in front of trees, fences and lamps sorted by their feet. |  |
| `TLD-011` | Ray casts | Rays against the cells of a tile layer and the shapes of an object layer, without physics. |  |
| `TLD-012` | Worlds | A world file that places listed maps and maps found by a file name pattern. |  |
| `TLD-013` | Digging the collision | Walls dug and built with tile changes while balls bounce, with the merged collision loops traced again around each change. |  |
| `TLD-014` | Maps built in code | A map made in code with ground filled from a grid, water painted with a corner Wang set and its Tiled JSON. |  |
| `TLD-015` | Automapping | A garden painted with plain tiles that the rules of a rule map give shores, random decoration and slimes, with a new seed on every press. |  |
| `TLD-016` | Layer blend modes | A map built in code whose water layer lies over the ground with each of the thirteen blend modes of Tiled, from normal, add, multiply and screen to overlay, darken, lighten, the dodges and burns, the lights, difference and exclusion. |  |

#### Tween (TWN)

Tweens of fields, colors, angles and text, every easing curve, timelines, playback controls, time scales, process modes and native tweens of sprites and UI nodes. The tests are in `source/categories/tween/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `TWN-001` | To, from, by and fromTo | The four ways a tween picks its start and end values. |  |
| `TWN-002` | Nested and several fields | Paths into nested tables and vectors, and many fields in one tween. |  |
| `TWN-003` | Colors in RGB and HSV | The same color change blended through RGB and through hue, saturation and value. |  |
| `TWN-004` | Shortest-path angles | Angles that turn the short way around the circle, next to plain numbers. |  |
| `TWN-005` | Counters and typewriter | Whole-number score counters and text revealed one character at a time. |  |
| `TWN-006` | Easing gallery | Every easing family with its parameters, steps, cubic Bezier curves and curves by points. |  |
| `TWN-007` | Custom easing | Easing curves written as Lua functions. |  |
| `TWN-008` | Timelines | Append, join, insert, labels, pauses and callbacks placed in time. |  |
| `TWN-009` | Nested timelines | Timelines inside a timeline that repeats and yoyos as a whole. |  |
| `TWN-010` | Repeat modes | Restart, yoyo and incremental loops with a delay between them. |  |
| `TWN-011` | Playback controls | Play, pause, resume, restart, reverse, seek, complete and kill from buttons. |  |
| `TWN-012` | Time scale | The speed of one tween and of a whole group of tweens by tag. |  |
| `TWN-013` | Ready-made tweens | Move, scale, rotate, fade, tint, jump, path, Bezier, blink, shake and punch on sprites. |  |
| `TWN-014` | Stagger | One tween on many targets, started from the start, the end or the center. |  |
| `TWN-015` | Overwrite mode | A new tween on the same fields takes them over, or fights the old one. |  |
| `TWN-016` | Scene and target lifetime | Tweens that end with the scene that owns them and with a target that is collected. |  |
| `TWN-017` | Process modes | Which tweens run while the game is paused, and tweens on real time. |  |
| `TWN-018` | UI node tweens | Native tweens of the offset, scale, opacity and tint of UI nodes. |  |
| `TWN-019` | Stress test | Thousands of native tweens on sprites with their count and the frame time. |  |

### Interface

#### Interface (GUI)

Every component of "haylen.ui" with its themes, focus navigation, tweens, text entry and touch controls. The tests are in `source/categories/interface/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `GUI-001` | Containers | Rows, columns, grids, stacks, scrolls, cards, panels, dividers, tabs, form fields and splitters. |  |
| `GUI-002` | Text | Labels in every font role and color, alignment, wrapping, page headers, empty states and alerts. |  |
| `GUI-003` | Buttons | Every button variant, icons above labels for tab bars, content at the start for search bars, image buttons, chips and menu buttons. |  |
| `GUI-004` | Choices | Check boxes, toggles and radio groups. |  |
| `GUI-005` | Text fields | Text fields with every keyboard and return key, secret fields, text areas and filter fields. |  |
| `GUI-006` | Pickers | Combos, color fields, number fields and sliders. |  |
| `GUI-007` | Indicators | Badges, status dots, busy rings, progress bars, circular progress and cooldowns, icons, images and avatars. |  |
| `GUI-008` | Collections | Lists with draggable rows that draw only the rows in view, trees and tables. |  |
| `GUI-009` | Settings | A settings screen built from a settings form, rows and actions. |  |
| `GUI-010` | Overlays | Dialogs, toasts, tooltips, popovers and context menus. |  |
| `GUI-011` | Game controls | Steppers, segmented controls, range sliders and key capture fields that rebind actions of the action map. |  |
| `GUI-012` | Windows and pages | A draggable window, accordions, a carousel and a scroll that snaps to its cards. |  |
| `GUI-013` | Slot grid | An inventory, a hotbar in another GUI and a chest list that trade items by drag and drop. |  |
| `GUI-014` | Rich text | Markup with styles, links, hints, images, icons, lists, tables, effects and a typewriter reveal. |  |
| `GUI-015` | Themes | The dark and light themes and a textured theme drawn with nine-slice surfaces. |  |
| `GUI-016` | Focus navigation | Directional and explicit neighbours, focus scopes, wrapping, a row that draws its own focus and going back with keys, gamepads and TV remotes. |  |
| `GUI-017` | UI tweens | Node transforms that move, scale, fade and tint controls with native tweens. |  |
| `GUI-018` | Text input | The hidden native field behind text fields, the on-screen keyboard and the plain keyboard on every platform. |  |
| `GUI-019` | Touch controls | Two touch sticks at once, one that follows the finger and one fixed, and touch buttons that drive actions next to keys and gamepads. |  |
| `GUI-020` | Layouts | Justify, alignItems, growing within bounds, margins, padding, rows that wrap, grids that fit their columns, stacks and aspect ratios. |  |
| `GUI-021` | Recycled lists | Recycled cells of mixed types in a list with sticky sections, a grid and a shelf, with inserts, moves, drags, cell menus and scrolling. |  |
| `GUI-022` | Long lists | A hundred thousand entries loaded in pages with placeholders, and a number field that scrolls to any entry. |  |
| `GUI-023` | Toast stacks | Toasts at every position as lists or decks, a queue past the limit, variants, indicators, actions, an upload and the events of a notice. |  |
| `GUI-024` | Theme overrides | Styles that replace the colors, metrics, fonts and surfaces of a subtree, styles that nest, subtrees in other themes and cursors. |  |
| `GUI-025` | Node events | The events every kind reports: mount, unmount, show, hide, hover, press, drag, release and scroll, in a log. |  |
| `GUI-026` | UI scale | The design and physical scale modes and the scale factor, with the size of a control in points on the screen. |  |
| `GUI-027` | Custom GUI in Lua | A small immediate-mode GUI written in Lua with buttons, a check box, a slider, a native text field, a clipped list and the focus. |  |
| `GUI-028` | Scroll bars | Every component that scrolls, each with its scroll bar in a lane of its own at least four points from its content, at every scale. |  |
| `GUI-029` | SVG pictures | SVG documents as icons, buttons, list rows and images, sharp at every size and scale, with tinted icons, rounded corners and surfaces. |  |
| `GUI-030` | Shared focus | A page and a player bar in GUIs of their own that share the focus, a sheet over a scroll and a form that keeps its maximum width. |  |
| `GUI-031` | Smooth curves | Every curved component and shape in the design and physical scales, beside their magnified edges, which fade over one pixel. |  |
| `GUI-032` | Images made at run time | A minimap rendered into a render target every frame, a portrait composed on the CPU and a vector image built in code, in components. |  |
| `GUI-033` | Image theme | A casual fantasy theme drawn from one atlas of nine-slice pieces and a set of icons, beside the default theme at several UI scales. |  |

#### Accessibility (ACC)

The accessibility settings of the system and what follows them, the elements that screen readers read with their actions and announcements, and the deficiencies of color vision. The tests are in `source/categories/accessibility/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `ACC-001` | Accessibility settings | The text scale, reduced motion, high contrast, reduced transparency and screen reader of the system, live, with the UI that follows. |  |
| `ACC-002` | Screen reader elements | The elements a screen reader reads of a form, with their roles, labels, values and states, the actions it performs and announcements. |  |
| `ACC-003` | Color vision | Colors that tell things apart, as protanopia, deuteranopia and tritanopia see them, and corrected for each. |  |

#### Orientation (ORI)

The orientation of the screen with its events and its lock, a layout that adapts to the shape of the screen, the scaling of the design resolution, and folds and dual screens. The tests are in `source/categories/orientation/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `ORI-001` | Orientation and its event | The orientation of the screen, the size of the window and the events that announce every turn. |  |
| `ORI-002` | Locking the orientation | Locking the screen in portrait, in landscape or letting it turn, and where each platform allows it. |  |
| `ORI-003` | Adaptive layout | A screen that stacks its parts in portrait and places them side by side in landscape. |  |
| `ORI-004` | Design resolution and scaling | How "fit", "fill", "stretch", "expand", "pixelPerfect", "fitWidth" and "none" map the design resolution onto screens of every shape. |  |
| `ORI-005` | Folds and dual screens | A map and its instruments laid out around the fold of a book, a tabletop, a fold opened flat and the hinge of a dual-screen device, simulated or reported by the device. |  |

#### Safe area (SAF)

The anchors of the interface against the safe area and the whole screen, drawing edge to edge, the debug overlay, the simulated devices and the status bar. The tests are in `source/categories/safe-area/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `SAF-001` | Anchors | The 16 anchor presets with margins, against the safe area and against the whole screen. |  |
| `SAF-002` | Edge to edge | The app draws under the notch, the rounded corners and the home indicator while the controls stay in the safe area. |  |
| `SAF-003` | Debug overlay | The debug view that shades what lies outside the safe area and prints its insets. |  |
| `SAF-004` | Device simulations | An iPhone with a notch or a dynamic island, an iPad, an Android phone with a gesture bar, a TV and custom insets, switched while the app runs. |  |
| `SAF-005` | Status bar | The status bar of phones and tablets shown with automatic, light or dark content, a color painted behind it and the safe area below it. |  |

### System

#### Content delivery (DLV)

Assets that the app needs to start, assets that download in the background or when the app asks for them, their progress, and how a release ships and updates its content. The tests are in `source/categories/delivery/`, and the rules of `content.delivery` in `app.json` name the folders of `content/delivery/` that each test delivers later.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `DLV-001` | Required content | An asset that is on the device before the app starts, read at once, and how the app runs from a protected release. |  |
| `DLV-002` | On-demand content | An asset that comes only when the app asks for it: a synchronous load refuses it, and an asynchronous load brings it first. |  |
| `DLV-003` | Prefetch and progress | Assets brought onto the device without loading them, with the progress of the bytes that arrive. |  |
| `DLV-004` | Updates and offline play | The status of the content, the update events, a check that fails and the content that plays when nothing can download. |  |

#### Development (DEV)

Hot reload while the project runs in development, with "haylen run samples/tests" or with "--platform web": each test names a file to edit and shows what a saved change does while the test runs. Without development the tests say that hot reload is off. The error screen of every kind of code that runs for the app, and the return to the app from it, run in development and in apps that ship. The tests are in `source/categories/development/`, and the files to edit in `source/categories/development/edit/` and `content/development/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `DEV-001` | Module reload | A greeting from a module that reloads in place while a counter keeps counting. |  |
| `DEV-002` | Classes | Walkers of a class that take its new methods in place, with their positions kept. |  |
| `DEV-003` | Callbacks | Timers with a module function, a closure made at run time and a call through a table, after a reload. |  |
| `DEV-004` | Hooks and kept values | The reloaded hook, a kept signal whose listener never doubles and the reloads the event bus reports. |  |
| `DEV-005` | Resume after a fix | An error in an update that saving the fix resumes from, with the state of the test kept. |  |
| `DEV-006` | Assets | JSON and the texts of a language that reload in place, with the asset events of each save. |  |
| `DEV-007` | Errors and recovery | Errors of scene hooks, tasks, timers, tweens, signals, events, native events, platform answers, asset loads, sockets and buttons on the error screen with their stack, and the return to the app. |  |
| `DEV-008` | Developer console | Commands of the test with typed arguments, lines run from code with their results and errors, completions and the console window. |  |
| `DEV-009` | Profiler traces | A trace of 60 frames with the scopes of the frame, the jobs of the workers and the time of the GPU, written to user storage and counted by kind. |  |
| `DEV-010` | Crashes and breadcrumbs | Breadcrumbs with their event, the report of the native crash that ended the last run and a button that crashes the app. |  |
| `DEV-011` | Inspector | A watched table and camera listed and edited through the inspector, an edit that a setter refuses and the inspector window. |  |

#### Files and storage (FIL)

The files of the package, the user folder through "haylen.storage" and Varn "fs", save slots, zip archives and a browser of the user folder. The tests are in `source/categories/files/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `FIL-001` | Package files | Text, JSON and binary files read from the content folder of the package. |  |
| `FIL-002` | User folder with storage | Write, read, append, list, "exists", sizes and remove, synchronously with "haylen.storage". |  |
| `FIL-003` | User folder with fs | Folders, files, streams, copies and removals, asynchronously with Varn "fs" on the I/O pool. |  |
| `FIL-004` | Save slots | Saving, loading and deleting named slots with the summaries a load menu shows. |  |
| `FIL-005` | Zip archives | Creating, listing and extracting zip archives with Varn "zip", including one shipped in the package. |  |
| `FIL-006` | File browser | The folders and files of the user folder, with sizes, dates and a preview. |  |
| `FIL-007` | Save versions | A slot of version 1 read by version 3 through migrations, the revision and backup of every write, a damaged slot that its backup replaces and a slot moved to another device. |  |
| `FIL-008` | Resume state | A screen and coins that a launch after the system ended the app in the background finds again, with how long ago the app left. |  |

#### Localization (LOC)

Translated text in six languages with arguments, plurals, nested keys, a fallback language, the best match for the device, fonts per language and right-to-left layouts. The tests are in `source/categories/localization/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `LOC-001` | Switching languages | A title screen whose every text changes the moment the language does. |  |
| `LOC-002` | Arguments | Names, numbers and booleans in placeholders, and escaped braces. |  |
| `LOC-003` | Plurals | The "zero", "one" and "other" forms picked by count, with the rules of each language. |  |
| `LOC-004` | Nested keys | Groups of keys in the language files, read with dotted keys. |  |
| `LOC-005` | Fallback language | Keys a language lacks, taken from the fallback language, and keys no language has. |  |
| `LOC-006` | Best match | The device language and other tags matched to the languages of the app. |  |
| `LOC-007` | Fonts per language | One font family whose fallbacks draw Japanese in every component. |  |
| `LOC-008` | Layout follows the text | Buttons and paragraphs that measure again when the language changes, a pseudo translation that shows where texts would overflow, and a title that shrinks to fit. |  |
| `LOC-009` | Right-to-left interface | Arabic mirrors the whole interface, and nodes with a direction of their own keep it. |  |
| `LOC-010` | Numbers, dates and lists | Decimals, percentages, amounts, compact numbers, dates in every style, lists and the plural and ordinal categories of ten languages. |  |

#### Native libraries (NAT)

The test library of the engine called through "haylen.native" and Varn "ffi", its callbacks and bridge handlers, a static library on iOS and tvOS and the handlers of each platform. The tests are in `source/categories/native/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `NAT-001` | Calls | Numbers, text, structs by value and by pointer, and buffers of the test library through Varn "ffi". |  |
| `NAT-002` | Callbacks | Callbacks that run during the call, at the next frame and from a thread of the library. |  |
| `NAT-003` | Library handlers | Handlers and events that the library registers through "HaylenNativeApi", with typed errors, timeouts and cancellation. |  |
| `NAT-004` | Static library | The library linked statically into an iOS or tvOS app and found through its symbol table. | macos, windows, linux, android, web, headless |
| `NAT-005` | Platform handlers | Kotlin, Java, Swift and JavaScript handlers that answer, refuse, throw and stop when the app cancels. | windows, linux, headless |

#### Network (NET)

HTTP requests with Varn "http" and WebSockets with "haylen.net" against public echo services, with downloads, errors, reconnection, connection events and a chat, and WebSockets, HTTP and sockets against servers inside the app at their limits. The tests are in `source/categories/network/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `NET-001` | HTTP requests | GET and POST over HTTPS with JSON, showing the status, the headers and the body. |  |
| `NET-002` | Download progress | A response streamed in chunks with a progress bar, then saved to the user folder. |  |
| `NET-003` | Errors | Unknown hosts, timeouts, error statuses, bad certificates and bodies that are not JSON. |  |
| `NET-004` | WebSocket echo | Text and binary messages, a round-trip ping and closing over a secure WebSocket. |  |
| `NET-005` | Reconnection | Automatic reconnection with a growing wait between attempts. |  |
| `NET-006` | Connection events | The events of a socket and the connection and network events of the event bus. |  |
| `NET-007` | Chat | A small chat over the echo socket that queues messages while it reconnects. |  |
| `NET-008` | Local WebSocket | A large message, bytes that wait to be sent, a dropped connection that comes back, a connect timeout, a refused port and a socket that closes with its owner, against a server inside the app. | web |
| `NET-009` | Local HTTP | A large upload, a response over its limit, many requests at once, a deadline of the app and a request that ends with its owner, against a server inside the app. | web, android |
| `NET-010` | Local sockets | A large TCP stream with sends that wait for the peer, a silent peer, a refused port and a burst of UDP datagrams, on 127.0.0.1. | web |
| `NET-011` | MessagePack | A match state encoded and decoded on the frame thread, a large message decoded off the frame thread and broken messages that are refused. |  |

#### Platform (PLT)

The bridge to the native code of the plugin "platform-sample", what the window and the device report, "haylen.system", the native dialogs and every event of the platform. The tests are in `source/categories/platform/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `PLT-001` | Native events | Checks that the native code of the plugin "platform-sample" sends the ticks of its ticker from the language of the platform and that an event sent from Lua arrives, failing where the project builds the native code and it is missing, next to the events of the platform itself. |  |
| `PLT-002` | Custom handler | Checks the answer of the method "platform-sample.echo", which Java gives on Android, Objective-C on Apple platforms and JavaScript on the web, and its failure without a text, failing where the project builds the native code and it is missing. |  |
| `PLT-003` | Window and device | Platform, backend, safe area, orientation, pointer, fullscreen, the on-screen keyboard and the network. |  |
| `PLT-004` | System | What "haylen.system" tells about the device, its theme and battery with their changes, an address to open and a vibration. |  |
| `PLT-005` | Dialogs | A native message with three buttons, the pickers of files to open, of the destination of a save and of a folder, and a message that the app gives up. |  |
| `PLT-006` | Event checklist | Every event that comes from the platform, marked live as it fires, and the events this platform never sends with the reason. |  |
| `PLT-007` | Launching apps | A small app package that the test writes into its storage and launches in place of the project with a new engine, whose Back button, quit or back button at its root starts the project again. |  |

#### Plugins (PLG)

Every capability of native plugins through the plugin "native-demo", written with the APIs of each platform alone, and its simulations of the SDKs of cameras, maps, stores, ads and sign-in. The tests are in `source/categories/plugins/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `PLG-001` | Calls | The call "echo" on the main thread, "compute" on a background thread, a typed failure, and a call that only a timeout or a cancel ends. | headless |
| `PLG-002` | Events | The "tick" events of a native timer, and "loaded", which the native part sent retained when it loaded. | headless |
| `PLG-003` | Binary payloads | Bytes that cross to the native part and back as buffers, and an image that the native part draws and returns as PNG bytes. | headless |
| `PLG-004` | Streams | A video stream that the native part draws 30 times per second, and an audio stream of a tone that the app plays. | headless |
| `PLG-005` | Batched events | 100 events per frame from the native part, which arrive as one list per frame. | headless |
| `PLG-006` | Configuration | The parameters of the plugin from "app.json" and the defaults of "plugin.json", in Lua and in the native part. |  |
| `PLG-007` | Native banner | A native view over the app at the top or bottom that may reserve its edge, with a native button, while other taps reach the app. | windows, linux, headless |
| `PLG-008` | Covering native UI | A native screen over the whole app, which halts the app until it closes. | windows, linux, headless |
| `PLG-009` | Native screen | A screen of the plugin whose answer reaches the call, and after a restart the next app as "screenRestored". | headless |
| `PLG-010` | Native result | The file picker of the platform, which answers with the name of the picked file. | tvos, windows, linux, headless |
| `PLG-011` | Permissions | The camera and notification prompts of the system, and a local notification whose tap reaches the app, even from a closed app. | tvos, windows, linux, headless |
| `PLG-012` | Requirements | A call whose native part needs what the project of the app lacks, which fails with the code "unsupported" and lists what is missing and how to add it. | windows, linux, headless |
| `PLG-013` | Opened URLs | Links with the scheme of the plugin that open the app, before or after it started. | windows, linux, headless |
| `PLG-014` | App errors | An error of the app that the native part keeps and sends back to the next app. | headless |
| `PLG-015` | Plugin info | The plugins of the app and whether their native part runs on this platform. |  |
| `PLG-016` | Camera and microphone | The camera of the device in a video stream with photos as JPEG bytes, and the microphone in an audio stream with its level, as a camera SDK delivers them. | tvos, windows, linux, headless |
| `PLG-017` | Location and map | Where the device is, after the permission of the person, and a native map over the app at that place, as a map SDK shows it. | windows, linux, headless |
| `PLG-018` | Sharing and other apps | The share sheet of the platform, and the settings, a map app and the mail app opened through their links. | headless |
| `PLG-019` | Fake store | The products of a fake store, purchases that the person confirms in a native sheet with a receipt, a kept product and restored purchases. | headless |
| `PLG-020` | Fake ads | A full-screen ad and a rewarded ad that cover the app while they show, and the reward of an ad watched to the end. | headless |
| `PLG-021` | Fake sign-in | An account that the person picks in a native sheet, the current user that the device keeps and signing out. | headless |
| `PLG-022` | Bridge benchmark | The round trip of one call, the calls per second, and the work of a frame with N calls and with N batched events, logged for the platform it runs on. | headless |

#### Preferences (PRF)

Choices of the player kept between sessions with "haylen.preferences": dotted keys, the settings of the engine, a settings screen in three languages and the way back to the defaults. The tests are in `source/categories/preferences/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `PRF-001` | Keys and values | Dotted keys set, read, removed, saved and loaded, next to the file they are saved in. |  |
| `PRF-002` | Engine settings | Bus volumes, mutes, fullscreen and the action map, captured into preferences and applied back. |  |
| `PRF-003` | Settings screen | A complete settings screen whose choices persist across restarts, in three languages. |  |
| `PRF-004` | Reset to defaults | Every stored value next to its default, and the way back to the defaults. |  |

#### Varn (VRN)

The modules of Varn, the Lua runtime of the engine, each lesson with its code and its live result. The tests are in `source/categories/varn/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `VRN-001` | Tasks and promises | Tasks that await promises, sleeps that never block a frame, rejections, failures caught, long loops that hand the frame back and callbacks turned into promises. |  |
| `VRN-002` | Combinators | All, race, any, every outcome and a few at a time, on a timeline of the jobs they wait on. |  |
| `VRN-003` | Deadlines and cancellation | Deadlines that give up waiting, tasks cancelled for good and tasks that end with their owner. |  |
| `VRN-004` | HTTP requests | GET with a query, POST with JSON, other methods, error statuses and timeouts against a server inside the app, and a server that the test opens on a free port and closes. | web, android |
| `VRN-005` | Streaming responses | Server-sent events that reach the app one by one, a body the server writes in parts and a download read in pieces. | web, android |
| `VRN-006` | TCP and UDP sockets | An echo server on a free port and its client with text and binary data, the end of a stream, and datagrams one by one and in batches. | web |
| `VRN-007` | WebSocket | A WebSocket server of Varn inside the app and a client of "haylen.net" that talk in text, bytes, broadcasts and pings. | web |
| `VRN-008` | JSON | Tables to JSON text and back, with the way Lua values map to JSON values, nulls that stay, marked arrays and objects, and errors that say where. |  |
| `VRN-009` | XML | A level written in XML, decoded into nodes and drawn, and nodes encoded back to text. |  |
| `VRN-010` | Files | A private folder in the storage of the app, with writes, appends, streams, copies, listings and removal. |  |
| `VRN-011` | Zip archives | Files packed, listed with their sizes and extracted, and the archives refused before anything is written because an entry would leave its folder. |  |
| `VRN-012` | Crypto | Digests and HMAC against published vectors, random bytes, codecs, UUIDs and encryption. |  |
| `VRN-013` | Dates and times | A live clock, ISO-8601 with offsets, calendar arithmetic, differences and boundaries. |  |
| `VRN-014` | Log and platform | Leveled lines with structured fields in the log of the engine, and what Varn knows about the device. |  |
| `VRN-015` | Processes | Commands with their output, errors and exit codes, a deadline that kills one, and the environment. | ios, tvos, web |
| `VRN-016` | C with ffi | C declarations, calls into the C library, structs with methods, buffers, pointers and a Lua function that C calls back. | web |
