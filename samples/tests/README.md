# Haylen Tests

Haylen Tests is the test project of the engine: one app with a test of every engine feature, from sprites to native plugins, grouped in categories. Every test has a code, such as `PHY-004`, which the menu, the header of the test and every log line of the test show, so a failing test is reported by its code. Codes never change and are never reused, and a new test takes the next number of its category.

## Running

- Desktop: `python3 haylen.py run samples/tests` runs the project in the player with hot reload.
- A platform: `python3 haylen.py run samples/tests --platform ios-simulator`, and the same with `tvos-simulator`, `android`, `web`, `macos`, `windows` or `linux`.
- Automatic: the headless player runs every test that the headless platform supports, one after the other for 2 seconds each, and exits with an error status when any test raised an error, which the CI job `tests-project` does on every push:

```sh
python3 haylen.py build --target haylen-headless
build/macos-debug/bin/haylen-headless samples/tests --frames 60000
```

## Using the project

The menu lists the categories by section with their prefixes and the number of their tests. The search field finds tests by code or title, and Enter in it opens the first match. `Continue with` opens the test opened last, which the project remembers between launches in the preference `tests.last`, and `Run all` runs every test of the platform one after the other and lists the failures with their codes.

- Mouse, touch, keyboard, gamepads and TV remotes reach every screen. A test goes back to its list with Escape, the east gamepad button, the Menu button of a TV remote, the back button of Android or the Back button in the safe area, and the list goes back to the menu the same way.
- A test that reads the keyboard, gamepads or remotes as game input gives its play area the first focus, so the arrows, WASD, the d-pad, the sticks, Space and Enter move the game while the controls of the test stay out of the way. Tab walks the controls and the play area, and the View button of gamepads, the pause key and the Play/Pause button of TV remotes move the focus between the play area and the controls. A click or a tap on a control gives it the focus, and one on the play area gives the focus back to the game.
- A test that a platform cannot run opens a page with the exact reason instead of the test, and the list marks it.
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
| `tools/` | The generators of the art of the Interface and Text categories, of the sounds of the Audio category and of the maps of the Tiled category. |

A manifest returns the `prefix`, the `title` and the `description` of its category and its `tests`. Each test has a `code`, a `title`, a `description` and the `module` of its file, and a test that does not run everywhere lists the `platforms` it runs on and, in `unsupported`, the reason for each other platform, from `macos`, `windows`, `linux`, `ios`, `tvos`, `android`, `web` and `headless`. The catalog checks every manifest when the project starts.

## Settings of `app.json`

Some settings that `app.json` gives the whole project change at run time, so the tests that need other values set them on entry and restore them on exit:

- The orientation tests unlock the orientation with `window.lockOrientation('any')` and lock it to landscape again.
- The design resolution test changes the design size and the scaling with `viewport.setDesignSize` and `viewport.setScaling` and restores 1920 by 1080 with `expand`.
- The safe area tests simulate the safe areas of devices with `viewport.setSafeAreaSimulation` and restore the simulation they found.
- The window, lifecycle and preferences tests restore the fullscreen mode, the lifecycle options, the bus volumes, the language and the theme they change.

No test needs a setting that only `app.json` can give.

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
| `SPR-005` | Static batches | A tile map baked once to the GPU and drawn every frame without uploads, with parallax offsets. |  |
| `SPR-006` | Layers and y-sort | Heroes among trees sorted by the y they stand on, and a selected hero raised above every layer. |  |
| `SPR-007` | Pooled bullets | A bullet hell whose projectiles come from an object pool and draw in one batch. |  |
| `SPR-008` | Bouncing bunnies | From a few to many thousands of sprites with float buffers or one table each, with the frame rate. |  |
| `SPR-009` | Render targets | An offscreen canvas drawn every frame and used as a texture in several ways. |  |
| `SPR-010` | Primitives | Lines, polylines, rectangles, circles, rings, arcs, polygons and meshes. |  |
| `SPR-011` | Text | Sizes, colors, outlines, shadows, alignment, wrapping, anchors, rotation and measuring. |  |
| `SPR-012` | Vector images | SVG icons in many sizes, a swarm of hundreds that turn and an icon that grows, rasterized on worker threads into an atlas. |  |
| `SPR-013` | Recoloring by parts | One adventurer in many outfits from a base image and a mask of four parts, and a crowd of a sprite batch in one draw call. |  |

#### Camera (CAM)

The 2D camera following, framing, shaking, turning and splitting the view, with parallax, pixel snap, picking and debug drawing. The tests are in `source/categories/camera/`.

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
| `CAM-009` | Minimap | A second camera in a corner viewport that leaves details out with visibility bits. |  |
| `CAM-010` | Blending cameras | Smooth cuts between a walker camera and a landmark camera. |  |
| `CAM-011` | Parallax layers | Mountains, hills, clouds and grass scrolling at their own rates. |  |
| `CAM-012` | Pixel snap | Pixel art with and without snapping the view to whole pixels. |  |
| `CAM-013` | Screen to world | Picking objects under the pointer in a zoomed and rotated view. |  |
| `CAM-014` | Debug drawing | The view, the limits and the drag box drawn from a zoomed-out camera. |  |

#### Text and fonts (TXT)

TrueType, OpenType and bitmap fonts, families and fallbacks, complex scripts, wrapping, rich text, effects and measuring. The tests are in `source/categories/text/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `TXT-001` | TrueType and OpenType sizes | A TrueType and an OpenType font from 12 to 96 units, and one word zoomed from 8 to 320, all from one distance field atlas. |  |
| `TXT-002` | Outline, shadow and glow | Outlines, shadows with blur and glows drawn from the distance field of the font. |  |
| `TXT-003` | Font families | A family with real bold, italic and mono faces next to families whose bold and italic are synthesized. |  |
| `TXT-004` | Fallback fonts | Japanese, Chinese and symbols drawn by fallback fonts that the family resolves character by character. |  |
| `TXT-005` | Bitmap fonts | A bitmap font in the text format, a colored bitmap font in the binary format and a grid font of LCD digits. |  |
| `TXT-006` | Alignment | Left, center, right and fill alignment, and the anchor point of a text block. |  |
| `TXT-007` | Complex scripts and right-to-left text | Arabic, Persian, Urdu, Hebrew, Hindi and Thai, right-to-left paragraphs with English and numbers inside, wrapping in every script and a right-to-left typewriter. |  |
| `TXT-008` | Wrapping | Words that wrap at a width, lines that break between Chinese and Japanese characters, long words and line spacing. |  |
| `TXT-009` | Rich text tags | Every tag of the markup: styles, colors, sizes, outlines, links, hints, images, icons, paragraphs, lists, rules, tables and drop caps. |  |
| `TXT-010` | Effects and typewriter | The built-in effects with their attributes, and a dialogue revealed like a typewriter with pauses and speed changes. |  |
| `TXT-011` | Custom effects | Text effects and inline icons registered from Lua. |  |
| `TXT-012` | Measuring text | Text sizes, glyph quads, ascent, baseline and line height, glyph metrics, shaped advances with kerning, and rich text layouts. |  |
| `TXT-013` | Outlines and small text | Outlines that reach the letters beside them at every size, and small text that moves at its exact place or snapped to whole pixels. |  |

#### Lighting (LIT)

Lit canvases with ambient, point, spot and directional lights, shadows, normal maps, emission, masks and many lights at once. The tests are in `source/categories/lighting/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `LIT-001` | Ambient light | The color a lit canvas starts from before any light, from noon to a dark cave. |  |
| `LIT-002` | Point lights | Colored point lights with their own radius and intensity, one on the cursor and more where you place them. |  |
| `LIT-003` | Spot lights | Cones that aim at the cursor and sweep the room, with their inner and outer angles. |  |
| `LIT-004` | Directional light | A sun that covers the whole canvas from one direction and casts long shadows. |  |
| `LIT-005` | Blend modes | Lights that add, subtract or mix into the light map. |  |
| `LIT-006` | Intensity above 1 | Lights brighter than the unlit colors in a floating-point light map. |  |
| `LIT-007` | Shadows | Occluders, the "none", "pcf5" and "pcf13" filters, shadow color and smoothness. |  |
| `LIT-008` | Physics and Tiled occluders | Shadows cast by falling physics crates and by the walls of a Tiled object layer. |  |
| `LIT-009` | Normal maps and specular | Bricks and studs with normal maps generated in Lua, lit by the angle of the light with highlights. |  |
| `LIT-010` | Unshaded and emissive | Signs that keep their colors in the dark and windows and neon that glow. |  |
| `LIT-011` | Light masks and layers | Lights that reach only the draws whose mask and layer they select. |  |
| `LIT-012` | Flicker | Torches that waver like flames, each with its own seed. |  |
| `LIT-013` | Day and night | A day cycle written in Lua: ambient color, sun, street lamps and lit windows. |  |
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

Particle emitters from Lua tables and effect files: weather, fire, bursts, shapes, forces, blending and tens of thousands of particles. The tests are in `source/categories/particles/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `PRT-001` | Fire | Flames, embers and smoke from three emitters that follow the cursor. |  |
| `PRT-002` | Smoke | Chimney smoke that starts fully grown with prewarm and bends in the wind. |  |
| `PRT-003` | Explosion | A flash, fireballs, debris, a shock ring and smoke in one burst. |  |
| `PRT-004` | Rain | Streaks across the whole view and splashes on the ground. |  |
| `PRT-005` | Snow | Two layers of flakes that drift and spin in the wind. |  |
| `PRT-006` | Sparks | Welding sparks from a cone with gravity and damping. |  |
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

#### Shaders (SHD)

Custom fragment shaders compiled into ".shader" files: materials on sprites and text, uniforms from tweens and textures, post-processing chains and hot reload. The tests are in `source/categories/shaders/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `SHD-001` | Sprite materials | Dissolve, outline, hit flash, wave distortion, palette swap, hologram and pixelation shaders on sprites. |  |
| `SHD-002` | Uniforms with tweens | Shader values driven by tweens, yoyos, elastic curves and a timeline. |  |
| `SHD-003` | Textures as uniforms | A material that reads a gradient and a pattern texture, including a live render target. |  |
| `SHD-004` | Post-processing chain | The built-in vignette followed by custom blur, color grading and tube screen passes over a canvas. |  |
| `SHD-005` | Shaders on text | Rainbow, dissolve and flash materials on text drawn from its distance field. |  |
| `SHD-006` | Hot reload | How a desktop run of the player recompiles and reloads a shader while the app keeps running. |  |
| `SHD-007` | Shader load time | Every shader of the category loaded in the background and drawn at once, with the time of the files and of the first frame on the backend of the platform. |  |

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

#### Audio (AUD)

Music, effects, buses, filters, positional sound, interruptions and many voices with sounds synthesized for these tests. The tests are in `source/categories/audio/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `AUD-001` | Music | Two streamed tracks with crossfades of any length, looping or played once, paused and resumed. |  |
| `AUD-002` | One-shot effects | Volume, pitch, pitch variation, pan, fades and a limit of voices per sound. |  |
| `AUD-003` | Buses | The bus tree with volumes, mutes and process modes, and what the game pause stops. |  |
| `AUD-004` | Effects | Every filter, the delay and the reverb on a bus or on a voice, with tweened parameters. |  |
| `AUD-005` | Positional audio | Sounds in the world around a listener that follows the camera, with fade models, panning and Doppler. |  |
| `AUD-006` | Interruptions and lifecycle | Audio interruptions, route changes and app states as they happen, and what the background does to the sound. |  |
| `AUD-007` | Many voices | Bursts and rain of voices up to the limit of 128, with the statistics of every bus. |  |

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
| `INP-002` | Mouse | Buttons, the wheel, the movement of a frame, cursor shapes, a hidden cursor, a captured mouse and a finger acting as the mouse. | tvos |
| `INP-003` | Touch | Up to ten fingers drawn with their id, phase, trail and time on the screen, and the mouse acting as a finger. | tvos |
| `INP-004` | Gestures | Taps, double taps, long presses, swipes and pinches with their thresholds. | tvos |
| `INP-005` | Gamepads | Four live controllers with sticks, triggers and buttons, their connections and the dead zone. |  |
| `INP-006` | Action map | Button, axis and vector actions bound to keys, mouse, gamepads and touch controls, and the last device used. |  |
| `INP-007` | Remapping | Rebinding the controls with key captures, kept in the preferences for the next launch. |  |
| `INP-008` | Touch controls | A touch stick in its fixed, floating and following modes and touch buttons that drive virtual inputs of the action map. |  |
| `INP-009` | Character | A small platformer hero that every device moves at the same time, with prompts for the last device. |  |

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
| `PHY-019` | Tunneling | Fast balls and crates that pass through thin walls, moving plates and thin floors on one side and stop with continuous collision and bullets on the other. |  |
| `PHY-020` | Tile seams | Boxes that catch on the joints between tiles made of one box each, next to the same tiles merged into one chain where they slide. |  |
| `PHY-021` | Slopes and steps | A dynamic capsule that slides down slopes and stumbles on stairs next to a mover that stands on slopes and steps up stairs, both on the same input. |  |
| `PHY-022` | Moving platforms | Platforms that jump to their place every step and lose their loads next to platforms moved by velocity that carry crates, balls and a mover. |  |
| `PHY-023` | Stretchy joints | A rope that stretches under a heavy weight next to one whose length limit holds, and a heavy ball on a chain of light links next to one with denser links, more sub-steps and stiffer joints. |  |
| `PHY-024` | Sleeping | A pyramid that keeps the solver busy with sleeping off next to one that falls asleep, wakes where it is touched and costs almost nothing. |  |
| `PHY-025` | Smooth slow motion | One world drawn where its last step left it, moving in jumps in slow motion, and drawn interpolated between steps, gliding. |  |
| `PHY-026` | Bounces and hits | Bouncy balls from five heights that stop dead and report no hits under the default thresholds and keep bouncing with low ones. |  |
| `PHY-027` | Dragging | A ragdoll pulled up the stairs by a mouse joint sized to the limb it holds, which only scrapes it along, next to a grabber sized to the whole figure, which lifts it. |  |
| `PHY-028` | Top-down car | A car seen from above on a closed track, driven with the keyboard, a gamepad or touch, that grips, slides into drifts with the handbrake and pushes cones while the camera follows it. |  |
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
| `PHY-050` | Wind | Fans as directional force fields that push leaves, balloons and boxes with the same force, gusts that change their strength and a vortex that swirls light bodies. |  |
| `PHY-051` | Water | A pool as a buoyancy force field where boats and logs float, an anvil sinks, crates float by their density and the flow makes currents and waves. |  |
| `PHY-052` | Soft bodies | Jelly blobs of a ring of small bodies on springy joints around a center, which squash on impact and recover and are dragged with a grabber. |  |
| `PHY-053` | Cloth | A net of small bodies joined by distance joints and pinned at the top, which catches falling bodies and tears where a joint passes its break force. |  |
| `PHY-054` | Breakables | Crates, vases and glass panes that break into pieces when a hit reported by the world passes the impact each one stands, hit by thrown balls. |  |
| `PHY-055` | Explosive barrels | Barrels that blow up when clicked or hit hard and light the barrels their blast reaches in a chain reaction that scatters the crates. |  |
| `PHY-056` | Thrusters | A lander whose two thrusters push at points off its center, steered with keys, a gamepad or touch, with fuel, a landing speed and a sensor over the pad. |  |
| `PHY-057` | Space ship | A ship without gravity that turns, thrusts and fires, wraps around the edges of the stage and breaks drifting rocks into smaller ones. |  |
| `PHY-058` | Marble run | Marbles rolling down ramps of chains, through a funnel, over a wheel turned by a motor and up a lift back to the top in a loop. |  |
| `PHY-059` | Water wheel | Particle water released by a dam that pours on a wheel on a revolute joint and turns it, with the speed of the wheel and a pump that brings the water back. |  |
| `PHY-060` | Claw machine | A carriage on a prismatic joint and two fingers on motorized revolute joints that pick up prizes by friction, driven with keys, a gamepad or touch. |  |
| `PHY-061` | Rope cutting | A candy hanging from ropes that a swipe cuts where a ray along the pointer crosses them, so it swings and falls into a basket with a sensor. |  |
| `PHY-062` | Traps | Spinning blades moved as kinematic bodies, a crusher on a prismatic joint and a rolling boulder that throw ragdolls and crates around. |  |
| `PHY-063` | Crate pushing | A top-down puzzle where a character driven by a limited force pushes heavy damped crates onto goal sensors with keys, a gamepad or a touch stick. |  |

#### Tiled (TLD)

Tiled maps of every orientation with their layers, objects, properties, collision, spawning, y sorting, ray casts and worlds. The tests are in `source/categories/tiled/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `TLD-001` | Orientations | Orthogonal, isometric, staggered, hexagonal and oblique maps, with the cell under the pointer. |  |
| `TLD-002` | Infinite maps | A map stored in chunks that reach negative cells, panned with the pointer, the keys or a stick. |  |
| `TLD-003` | Tile animations | Water, lava, torches, coins and slimes whose tiles play their frames on map time. |  |
| `TLD-004` | Image layers | A sky, clouds and hills that repeat across the view and scroll at their own parallax. |  |
| `TLD-005` | Group layers | Nested groups that pass their offset, tint, opacity and parallax on to their layers. |  |
| `TLD-006` | Objects and templates | Every object shape and object templates with their overrides, picked under the pointer. |  |
| `TLD-007` | Properties | Custom properties of every type on the map, its layers, its objects and its tiles. |  |
| `TLD-008` | Collision | Physics bodies built from tile collision shapes and collision objects, with sensors and filters. |  |
| `TLD-009` | Spawning | Entities created from objects by factories keyed by object class. |  |
| `TLD-010` | Y sorting | A character walking behind and in front of trees, fences and lamps sorted by their feet. |  |
| `TLD-011` | Ray casts | Rays against the cells of a tile layer and the shapes of an object layer, without physics. |  |
| `TLD-012` | Worlds | A world file that places listed maps and maps found by a file name pattern. |  |
| `TLD-013` | Digging the collision | Walls dug and built with tile changes while balls bounce, with the merged collision loops traced again around each change. |  |

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
| `GUI-003` | Buttons | Every button variant, image buttons, chips and menu buttons. |  |
| `GUI-004` | Choices | Check boxes, toggles and radio groups. |  |
| `GUI-005` | Text fields | Text fields with every keyboard and return key, secret fields, text areas and filter fields. |  |
| `GUI-006` | Pickers | Combos, color fields, number fields and sliders. |  |
| `GUI-007` | Indicators | Badges, status dots, busy rings, progress bars, circular progress and cooldowns, icons, images and avatars. |  |
| `GUI-008` | Collections | Lists with draggable rows, trees and tables. |  |
| `GUI-009` | Settings | A settings screen built from a settings form, rows and actions. |  |
| `GUI-010` | Overlays | Dialogs, toasts, tooltips, popovers and context menus. |  |
| `GUI-011` | Game controls | Steppers, segmented controls, range sliders and key capture fields that rebind actions of the action map. |  |
| `GUI-012` | Windows and pages | A draggable window, accordions, a carousel and a scroll that snaps to its cards. |  |
| `GUI-013` | Slot grid | An inventory, a hotbar in another GUI and a chest list that trade items by drag and drop. |  |
| `GUI-014` | Rich text | Markup with styles, links, hints, images, icons, lists, tables, effects and a typewriter reveal. |  |
| `GUI-015` | Themes | The dark and light themes and a textured theme drawn with nine-slice surfaces. |  |
| `GUI-016` | Focus navigation | Directional and explicit neighbours, focus scopes, wrapping and going back with keys, gamepads and TV remotes. |  |
| `GUI-017` | UI tweens | Node transforms that move, scale, fade and tint controls with native tweens. |  |
| `GUI-018` | Text input | The hidden native field behind text fields, the on-screen keyboard and the plain keyboard on every platform. |  |
| `GUI-019` | Touch controls | Two touch sticks at once, one that follows the finger drawn with images and one fixed drawn with circles, and touch buttons that drive actions of the action map next to keys and gamepads. |  |
| `GUI-020` | Layouts | Justify, alignItems, growing within size bounds, margins, padding, rows that wrap, grids that fit their columns, stacks and the aspect ratio at any width. |  |
| `GUI-023` | Toast stacks | Toasts at every position of the safe area that stack without covering each other, a queue past the limit of a stack and a dialog whose backdrop fades with it. |  |
| `GUI-024` | Theme overrides | Styles that replace colors, metrics, fonts and surfaces of one subtree, styles that nest, subtrees in other themes in every state and the cursor of each node. |  |
| `GUI-025` | Node events | The events every kind reports: mount, unmount, show, hide, hover, press, drag, release and scroll, in a log. |  |
| `GUI-026` | UI scale | The design and physical scale modes and the scale factor, with the size of a control in points on the screen. |  |

#### Orientation (ORI)

The orientation of the screen with its events and its lock, a layout that adapts to the shape of the screen, and the scaling of the design resolution. The tests are in `source/categories/orientation/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `ORI-001` | Orientation and its event | The orientation of the screen, the size of the window and the events that announce every turn. |  |
| `ORI-002` | Locking the orientation | Locking the screen in portrait, in landscape or letting it turn, and where each platform allows it. |  |
| `ORI-003` | Adaptive layout | A screen that stacks its parts in portrait and places them side by side in landscape. |  |
| `ORI-004` | Design resolution and scaling | How "fit", "fill", "stretch", "expand" and "pixelPerfect" map the design resolution onto screens of every shape. |  |

#### Safe area (SAF)

The anchors of the interface against the safe area and the whole screen, drawing edge to edge, the debug overlay and the simulated devices. The tests are in `source/categories/safe-area/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `SAF-001` | Anchors | The 16 anchor presets with margins, against the safe area and against the whole screen. |  |
| `SAF-002` | Edge to edge | The app draws under the notch, the rounded corners and the home indicator while the controls stay in the safe area. |  |
| `SAF-003` | Debug overlay | The debug view that shades what lies outside the safe area and prints its insets. |  |
| `SAF-004` | Device simulations | An iPhone with a notch or a dynamic island, an iPad, an Android phone with a gesture bar, a TV and custom insets, switched while the app runs. |  |

### System

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
| `LOC-008` | Layout follows the text | Buttons and paragraphs that measure again when the language changes. |  |
| `LOC-009` | Right-to-left interface | Arabic mirrors the whole interface, and nodes with a direction of their own keep it. |  |

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

HTTP requests with Varn "http" and WebSockets with "haylen.net" against public echo services, with downloads, errors, reconnection, connection events and a chat. The tests are in `source/categories/network/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `NET-001` | HTTP requests | GET and POST over HTTPS with JSON, showing the status, the headers and the body. |  |
| `NET-002` | Download progress | A response streamed in chunks with a progress bar, then saved to the user folder. |  |
| `NET-003` | Errors | Unknown hosts, timeouts, error statuses, bad certificates and bodies that are not JSON. |  |
| `NET-004` | WebSocket echo | Text and binary messages, a round-trip ping and closing over a secure WebSocket. |  |
| `NET-005` | Reconnection | Automatic reconnection with a growing wait between attempts. |  |
| `NET-006` | Connection events | The events of a socket and the connection and network events of the event bus. |  |
| `NET-007` | Chat | A small chat over the echo socket that queues messages while it reconnects. |  |

#### Platform (PLT)

The bridge to the native code of the plugin "platform-sample", what the window and the device report, "haylen.system" and the native dialogs. The tests are in `source/categories/platform/`.

| Code | Test | What it shows | Unsupported on |
| --- | --- | --- | --- |
| `PLT-001` | Native events | Events that the native code of the plugin "platform-sample" sends through the bridge, and the events of the platform itself. |  |
| `PLT-002` | Custom handler | The method "platform-sample.echo", which Java answers on Android, Objective-C on Apple platforms and JavaScript on the web. |  |
| `PLT-003` | Window and device | Platform, backend, safe area, orientation, pointer, fullscreen, the on-screen keyboard and the network. |  |
| `PLT-004` | System | What "haylen.system" tells about the device, its theme and battery with their changes, an address to open and a vibration. |  |
| `PLT-005` | Dialogs | A native message with three buttons, the pickers of files to open, of the destination of a save and of a folder, and a message that the app gives up. |  |

#### Plugins (PLG)

Every capability of native plugins through the plugin "native-demo", written with the APIs of each platform alone. The tests are in `source/categories/plugins/`.

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
| `PLG-011` | Permissions | The camera and notification prompts of the system, and a local notification whose tap reaches the app, even from a closed app. | tvos, windows, linux, web, headless |
| `PLG-012` | Requirements | A call whose native part needs what the project of the app lacks, which fails with the code "unsupported" and lists what is missing and how to add it. | windows, linux, headless |
| `PLG-013` | Opened URLs | Links with the scheme of the plugin that open the app, before or after it started. | windows, linux, headless |
| `PLG-014` | App errors | An error of the app that the native part keeps and sends back to the next app. | headless |
| `PLG-015` | Plugin info | The plugins of the app and whether their native part runs on this platform. |  |

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
| `VRN-001` | Tasks and promises | Tasks that await promises, sleeps that never block a frame, rejections and callbacks turned into promises. |  |
| `VRN-002` | Combinators | All, race, any, every outcome and a few at a time, on a timeline of the jobs they wait on. |  |
| `VRN-003` | Deadlines and cancellation | Deadlines that give up waiting, tasks cancelled for good and tasks that end with their owner. |  |
| `VRN-004` | HTTP requests | GET with a query, POST with JSON, other methods, error statuses and timeouts against a server inside the app. | web, android |
| `VRN-005` | Streaming responses | Server-sent events that reach the app one by one, and a download read in pieces. | web, android |
| `VRN-006` | TCP and UDP sockets | An echo server and its client with text and binary data, the end of a stream, and datagrams. | web |
| `VRN-007` | WebSocket | A WebSocket server of Varn inside the app and a client of "haylen.net" that talk in text, bytes, broadcasts and pings. | web |
| `VRN-008` | JSON | Tables to JSON text and back, with the way Lua values map to JSON values. |  |
| `VRN-009` | XML | A level written in XML, decoded into nodes and drawn, and nodes encoded back to text. |  |
| `VRN-010` | Files | A private folder in the storage of the app, with writes, appends, streams, copies, listings and removal. |  |
| `VRN-011` | Zip archives | Files packed, listed and extracted, and the archives refused because an entry would leave its folder. |  |
| `VRN-012` | Crypto | Digests and HMAC against published vectors, random bytes, codecs, UUIDs and encryption. |  |
| `VRN-013` | Dates and times | A live clock, ISO-8601 with offsets, calendar arithmetic, differences and boundaries. |  |
| `VRN-014` | Log and platform | Leveled lines with structured fields in the log of the engine, and what Varn knows about the device. |  |
| `VRN-015` | Processes | Commands with their output, errors and exit codes, a deadline that kills one, and the environment. | ios, tvos, web |
| `VRN-016` | C with ffi | C declarations, calls into the C library, structs with methods, buffers, pointers and a Lua function that C calls back. | web |
