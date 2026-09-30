# Haylen Algorithms

A Lua sample with one scene per algorithm of [`haylen.navigation2d`](../../../docs/lua-api/navigation2d.md), [`haylen.spatial2d`](../../../docs/lua-api/spatial2d.md), [`haylen.procedural2d`](../../../docs/lua-api/procedural2d.md), [`haylen.math`](../../../docs/lua-api/math.md) and [`haylen.ai`](../../../docs/lua-api/ai.md). The menu lists the tests, each test opens as its own scene with a Back button, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu. Heavy work shows its time from the engine profiler, and generators with an asynchronous version run on the worker threads so the frame never waits for them.

| Test | What it shows |
| --- | --- |
| A* on a grid | A* with the octile, Manhattan, Euclidean and Chebyshev heuristics, weighted A* and jump point search side by side on walls and mud you paint. |
| Hex and isometric grids | Paths on hexagonal grids that shift rows or columns, isometric grids and staggered grids. |
| Waypoint graph | A* between waypoints, Dijkstra costs from the start, heavy points, doors that close and one-way roads. |
| Flow field | Eight hundred units following one field computed on a worker thread toward the pointer. |
| Dijkstra and flee maps | Monsters that chase the player on a Dijkstra map, a gold source that pulls harder, and monsters that flee on its flee map. |
| Hierarchical path finding | HPA* on a 256 by 256 map next to A* and jump point search, with the clusters a stroke of walls rebuilds. |
| Navigation mesh | A mesh built on a worker thread, funnel paths with an agent radius, and obstacles added and removed. |
| Crowd | Hundreds of ORCA agents swapping sides, crossing a circle or following the pointer, with separation, alignment and cohesion. |
| Spatial structures | A spatial hash, a quadtree, a dynamic AABB tree and a k-d tree answering rectangle, circle, point, ray and nearest queries under the pointer. |
| Field of view | Symmetric shadowcasting with fog of war, Bresenham lines and circles, and a visibility polygon among walls. |
| Flood fill and regions | The paint bucket, connected regions and a union-find that joins islands. |
| Scatter | Objects spread inside a polygon at random by area and density, on a jittered grid or with Poisson spacing, kept out of lakes and a village, and typed by weights and biomes. |
| Poisson disk | Points whose spacing follows a noise density on a worker thread or a focus you pick. |
| Cellular caves | Caves from a cellular automaton or drunkard walkers, colored by connected region. |
| Wave Function Collapse | A terrain of six tiles following adjacency rules, with tiles you pin in place. |
| Dungeons | Rooms and corridors from binary space partitioning or from random placement joined by a spanning tree. |
| Mazes | Perfect mazes from the recursive backtracker, Prim and Kruskal, solved with A*. |
| Voronoi and Delaunay | Voronoi cells, the Delaunay triangulation and Lloyd relaxation of points you add. |
| Autotiling | 4-bit side masks, 47-tile blob masks and a corner Wang set picked while you paint. |
| Polygon booleans | Union, difference, intersection and exclusion of a ring you drag over a star, offsets with every corner style and convex decomposition. |
| Marching squares | Outlines traced from a field or a bitmap you raise and lower, simplified with Ramer-Douglas-Peucker. |
| Splines | Catmull-Rom, Bezier and B-spline curves through points you drag, sampled by distance and walked at a steady speed. |
| Behavior trees | Guards that chase what they see past walls, rest and patrol, with a cooldown on their shouts. |
| Utility AI | Villagers that weigh hunger, tiredness and boredom with response curves. |
| Influence maps | Two armies that spread their strength and advance or retreat on the balance. |
| Ray casts without physics | Rays against segments, rectangles, circles, polygons, chains, grid cells and an AABB tree, with piercing, bounces and fans. |

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run samples/gameplay/algorithms` |
| macOS app | `python3 make.py run samples/gameplay/algorithms --platform macos` |
| iPhone and iPad simulator | `python3 make.py run samples/gameplay/algorithms --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run samples/gameplay/algorithms --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run samples/gameplay/algorithms --platform android --device <serial>` |
| Browser | `python3 make.py run samples/gameplay/algorithms --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
| Paint, pick and aim | Left mouse button and the pointer | Right stick moves a cursor, right trigger presses | Finger | Options of the panel |
| Change the options | Click, or the arrows and Enter once an option has the focus | Directional pad and south button | Tap | Swipe and select |
| Start over or a new seed | R | West button | Button of the panel | Button of the panel |
| Next or previous mode | E and Q | Shoulder buttons | Radio buttons | Radio buttons |

The options panel of each test takes the focus when the test opens, so gamepads and TV remotes reach every option with the directional pad.
