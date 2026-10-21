# 🎲 Procedural generation

The module `haylen.procedural2d` generates 2D content: it scatters objects over regions, grows caves and dungeons, carves mazes, fills maps with Wave Function Collapse, builds Delaunay triangulations and Voronoi diagrams, and picks tile variants by their neighbors. This guide explains how generation stays reproducible, how generated maps reach the other modules and how generation stays off the frame. The [`haylen.procedural2d` reference](../lua-api/procedural2d.md) lists the complete API.

## Reproducible by seed

Every generator is deterministic: the same options and seed always give the same result, so a world can be shared as a seed, a daily challenge is the same for every player and a bug report replays exactly. A generator takes a `seed`, or a `random` generator of [`haylen.math`](../lua-api/math.md) that advances as it draws, so a sequence of generators that share one generator stays reproducible as a whole.

## What each generator suits

| Content | Generator |
| --- | --- |
| Trees, rocks, loot and spawn points spread naturally over an area. | Scattering over a region, at random, on a jittered grid or with Poisson disk spacing, with density maps and excluded regions. |
| Organic caves and islands. | Cellular automata and drunkard walks. |
| Rooms connected by corridors. | Dungeons. |
| Labyrinths with a single path between any two cells. | Mazes. |
| Maps and patterns that follow adjacency rules from a sample or a tile set. | Wave Function Collapse. |
| Territories, roads between points and meshes over scattered points. | Delaunay triangulation and Voronoi diagrams. |
| Borders and corners of painted or generated maps. | Autotiling by masks of 4 or 8 neighbors and by the Wang sets of Tiled. |

## Maps and the other modules

Generated maps are `CellGrid` objects of [`haylen.spatial2d`](../lua-api/spatial2d.md), where walls are `1` and floors are `0`, so flood fills, connected regions and field of view check and use them directly, such as removing caves that no path reaches. A grid becomes the walkable cells of a grid of [`haylen.navigation2d`](../lua-api/navigation2d.md), and autotiling turns it into the tiles of a map of [`haylen.tiled`](../lua-api/tiled.md) built at run time.

## Off the frame

Generators whose names end in `Async` take the same options, copy them, run on the worker threads of the engine and return a promise, so a large world generates behind a loading screen or while the player explores. An asynchronous generator copies its `random` generator, which therefore does not advance, as [seeds, maps and promises](../lua-api/procedural2d.md#seeds-maps-and-promises) describes. Generation written in Lua runs as a job of [`haylen.jobs`](../lua-api/jobs.md), which shares a budget of each frame.

## C++

The C++ types live in `haylen/2d/procedural/` in the namespace `haylen::procedural2d`: `Region`, `Scatter`, `CellularAutomaton`, `DrunkardWalk`, `Dungeon`, `Maze`, `WaveFunctionCollapse`, `Delaunay`, `Voronoi` and `Autotile`.
