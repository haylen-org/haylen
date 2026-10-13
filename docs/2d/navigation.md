# Navigation

The module `haylen.navigation2d` moves characters through the world: it plans routes over tile grids, waypoint graphs and navigation meshes, guides many units at once with Dijkstra maps and flow fields, and moves agents smoothly with steering and crowds that never overlap. This guide explains how to pick a method, how navigation fits with maps and physics, and how to keep it off the frame. The [`haylen.navigation2d` reference](../lua-api/navigation2d.md) lists the complete API.

## Picking a method

| The game has | Use |
| --- | --- |
| A tile map and one unit at a time that plans a route, with terrain costs. | `grid:findPath` of a `NavGrid`, A\* by default and weighted A\* to trade length for speed. |
| Large open grids where every cell costs the same. | `grid:findPath` with jump point search. |
| Many long searches on a large grid that changes a little at a time. | `grid:hierarchicalPathfinder`, which plans over clusters and updates only the clusters that changed. |
| Many units that head to the same goals, as in strategy and tower defense games. | `grid:flowField`, which every unit reads in constant time. |
| Monsters that chase or flee the player and weigh several goals. | `grid:dijkstraMap`, whose `flee` turns a chase into an escape. |
| Hand-placed waypoints, roads and doors that open and close. | A `NavGraph`. |
| Open levels without a grid, with paths that hug corners and keep a radius clear. | A `NavMesh`. |
| Smooth motion of one character toward a target. | A `SteeringAgent`, with seek, flee, arrive, flocking, obstacle avoidance and wander. |
| Hundreds or thousands of agents that must not overlap. | A `Crowd`, which solves optimal reciprocal collision avoidance for every agent each step. |

A route from a planner and the motion of an agent combine: a unit plans its cells or points once and steers along them every frame, and a crowd keeps the units of a route apart.

## Grids and maps

Grid cells count from 0 by column and row, and hexagonal and staggered grids use the offset coordinates of Tiled, so the cells of a map of [`haylen.tiled`](../lua-api/tiled.md) and the cells of a grid are the same numbers. A grid marks cells walkable and gives them costs, so swamps cost more than roads, and paths come back as lists of cells. A navigation mesh takes its walkable area and obstacles as polygons, such as the object outlines of a Tiled map or the outlines of the shapes of [`haylen.physics2d`](../lua-api/physics2d.md), and finds paths for agents of any radius.

## Off the frame

Functions whose names end in `Async` copy what they need, run on the worker threads of the engine and return a promise that a coroutine awaits, so a long search, a flow field over a large map or the build of a navigation mesh never holds up a frame. A grid keeps one copy of its cells for background work and shares it between calls until a cell changes, as [background work](../lua-api/navigation2d.md#background-work) describes. Invalid arguments still raise at once, in the call.

## C++

The C++ types live in `haylen/2d/navigation/` in the namespace `haylen::navigation2d`: `Grid` with `GridSearch`, `DijkstraMap`, `FlowField`, `HierarchicalPathfinder`, `Graph` with `GraphSearch`, `NavMesh`, `SteeringAgent`, `Wanderer` and `Crowd`.
