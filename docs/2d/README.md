# 📐 2D

The 2D block holds the modules that build 2D games and apps: retained node trees, frame and skeletal animation, particles, lighting, physics, Tiled maps, navigation, spatial queries and procedural generation. Their Lua modules end with `2d`, so 3D modules can grow next to them.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| Animation | Frame animations, atlases with frame data, animators, animated batches, state graphs, skeletons and atlases packed at run time. | [`animation.md`](animation.md) |
| Lighting | Lit canvases, lights, occluders and shadows, light queries for gameplay and performance. | [`lighting.md`](lighting.md) |
| Navigation | Path finding on grids, graphs and navigation meshes, flow fields, Dijkstra maps, steering and crowds, and which to choose. | [`navigation.md`](navigation.md) |
| Nodes | Retained trees of composed objects, prefabs and how nodes work with sprites, physics, animation and tweens. | [`nodes.md`](nodes.md) |
| Particles | How emitters simulate and draw particles, composite effects, trails, the effect library and performance. | [`particles.md`](particles.md) |
| Physics | Worlds, bodies and shapes, units, stepping on several threads, sleeping, the common problems of physics games and their solutions, and performance. | [`physics.md`](physics.md) |
| Procedural | Deterministic generation of scatterings, caves, dungeons, mazes, tile maps, triangulations and autotiles, on the frame or in the background. | [`procedural.md`](procedural.md) |
| Spatial | Spatial structures, grid algorithms, screen picking and overlap worlds, and which to choose. | [`spatial.md`](spatial.md) |
| Tiled | Maps and worlds made with the Tiled editor: authoring, supported features, loading, drawing, reading a map, collision and spawning objects. | [`tiled.md`](tiled.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.animation2d` | Frame animations from grids and atlases with frame data, animators, animated sprite batches, state graphs, skeletal and cutout animation, and atlases packed at run time. | [`lua-api/animation2d.md`](../lua-api/animation2d.md) |
| `haylen.lighting2d` | Point, spot and directional lights with blend modes, masks and shadows, occluders from outlines, physics bodies and Tiled maps, light queries and flame flicker. | [`lua-api/lighting2d.md`](../lua-api/lighting2d.md) |
| `haylen.navigation2d` | A\*, weighted A\*, jump point search and hierarchical path finding on square, isometric, staggered and hexagonal grids, Dijkstra maps, flow fields, waypoint graphs, navigation meshes with agent radius, steering agents and ORCA crowds, with asynchronous versions. | [`lua-api/navigation2d.md`](../lua-api/navigation2d.md) |
| `haylen.nodes2d` | Retained trees of 2D nodes with transforms, colors, visibility and layers that subtrees inherit, culled drawing in one call, prefabs, saving to descriptions, nodes that follow bodies and native properties for tweens. | [`lua-api/nodes2d.md`](../lua-api/nodes2d.md) |
| `haylen.particles2d` | Particle emitters, composite systems, trails and `.particles` effect files. | [`lua-api/particles2d.md`](../lua-api/particles2d.md) |
| `haylen.physics2d` | Box2D worlds stepped on several threads with interpolation between fixed steps, bodies, shapes with their materials, geometry and outlines for drawing, joints that change while they run and break under load, contacts, sensors, ray casts with filters, piercing, bounces and fans, shape casts, batched ray casts on the job system, screen picking with a radius, path prediction, ray debug drawing and queries, with ropes, bridges, ragdolls, side and top-down vehicles, character movers, grabbers, force fields, one-way platforms, conveyors, explosions, destructible terrain, fractures and fluids. | [`lua-api/physics2d.md`](../lua-api/physics2d.md) |
| `haylen.procedural2d` | Scattering over regions, caves, dungeons, mazes, Wave Function Collapse, Delaunay and Voronoi, and autotiling, all deterministic by seed and with asynchronous versions. | [`lua-api/procedural2d.md`](../lua-api/procedural2d.md) |
| `haylen.spatial2d` | Spatial hashes, quadtrees, dynamic AABB trees and k-d trees with area, circle, point, ray and nearest queries, screen picking, and grid algorithms: ray casts, Bresenham lines and circles, field of view, visibility polygons, flood fills, connected regions and union-find. | [`lua-api/spatial2d.md`](../lua-api/spatial2d.md) |
| `haylen.tiled` | Tiled maps and worlds: drawing with y sorting, maps built at run time with terrain painting and saved as Tiled JSON, collision, objects and spawning, ray casts against tiles and objects, and object outlines for navigation meshes. | [`lua-api/tiled.md`](../lua-api/tiled.md) |
