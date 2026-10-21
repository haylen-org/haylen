# 📐 2D

The 2D block holds the modules that build 2D games and apps: retained node trees, frame and skeletal animation, particles, lighting, physics, Tiled maps, navigation, spatial queries and procedural generation. Their Lua modules end with `2d`, so 3D modules can grow next to them.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| Animation | Frame animations, sprite atlases and frame data, animators, fixed steps, animated batches, state graphs, skeletons, procedural chains, atlases packed at run time and performance. | [`animation.md`](animation.md) |
| Lighting | Lit canvases, light types and blend modes, masks and layers, how surfaces take light, shadows and occluders from bodies and maps, light queries for gameplay, flicker and performance. | [`lighting.md`](lighting.md) |
| Navigation | Planning and moving: paths on grids, waypoint graphs and navigation meshes, flow fields and Dijkstra maps for many units, steering, crowds that follow a path, work off the frame, which method to choose and what each costs. | [`navigation.md`](navigation.md) |
| Nodes | Retained trees of objects made of parts: what nodes pass to their children, updating and drawing, handles, prefabs, nodes with physics, tweens, animation and particles, and performance. | [`nodes.md`](nodes.md) |
| Particles | Emitters and effect files, how particles are born, move, collide and look, composite effects, sub-emitters, trails, lights, the effect library and performance. | [`particles.md`](particles.md) |
| Physics | Worlds, bodies, shapes and joints, units, stepping on several threads, sleeping, collisions and queries, the helpers, the common problems of physics games with their solutions, and performance. | [`physics.md`](physics.md) |
| Procedural | Seeds that repeat, which generator suits which content, maps as cell grids, recipes for dungeons, caves, forests, mazes, tiles that follow rules and territories, autotiling, work off the frame and what each generator costs. | [`procedural.md`](procedural.md) |
| Spatial | How spatial structures work and which to choose, queries, picking units under the cursor, grid algorithms with fog of war, lights and level checks, screen picking, overlap worlds for hits and what each costs. | [`spatial.md`](spatial.md) |
| Tiled | Maps and worlds made with the Tiled editor: authoring, supported features, loading, drawing, reading a map, collision, spawning objects, ray casts, maps built in code, automapping and streamed worlds. | [`tiled.md`](tiled.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.animation2d` | Frame animations from grids and atlases with frame data, animators, animated sprite batches, state graphs, skeletal and cutout animation, chains for procedural motion and atlases packed at run time. | [`lua-api/animation2d.md`](../lua-api/animation2d.md) |
| `haylen.lighting2d` | Point, spot and directional lights with blend modes, masks and shadows, occluders from outlines, physics bodies and Tiled maps, light queries and flame flicker. | [`lua-api/lighting2d.md`](../lua-api/lighting2d.md) |
| `haylen.navigation2d` | A\*, weighted A\*, jump point search and hierarchical path finding on square, isometric, staggered and hexagonal grids, Dijkstra maps, flow fields, waypoint graphs, navigation meshes with agent radius, steering agents and ORCA crowds, with asynchronous versions. | [`lua-api/navigation2d.md`](../lua-api/navigation2d.md) |
| `haylen.nodes2d` | Retained trees of 2D nodes with transforms, colors, visibility and layers that subtrees inherit, culled drawing in one call, prefabs, saving to descriptions, nodes that follow bodies and native properties for tweens. | [`lua-api/nodes2d.md`](../lua-api/nodes2d.md) |
| `haylen.particles2d` | Particle emitters with their options, composite systems, trails, image shapes and `.particles` effect files. | [`lua-api/particles2d.md`](../lua-api/particles2d.md) |
| `haylen.physics2d` | Box2D worlds stepped on several threads with interpolation between fixed steps, bodies, shapes with their materials, geometry and outlines for drawing, joints that change while they run and break under load, contacts, sensors, ray casts with filters, piercing, bounces and fans, shape casts, batched ray casts on the job system, screen picking with a radius, path prediction, ray debug drawing and queries, with ropes, bridges, ragdolls, side and top-down vehicles, character movers, grabbers, force fields, one-way platforms, conveyors, explosions, destructible terrain, fractures and fluids. | [`lua-api/physics2d.md`](../lua-api/physics2d.md) |
| `haylen.procedural2d` | Scattering over regions, caves, dungeons, mazes, Wave Function Collapse, Delaunay and Voronoi, and autotiling, all deterministic by seed and with asynchronous versions. | [`lua-api/procedural2d.md`](../lua-api/procedural2d.md) |
| `haylen.spatial2d` | Spatial hashes, quadtrees, dynamic AABB trees and k-d trees with area, circle, point, ray and nearest queries, screen picking, grid algorithms: ray casts, Bresenham lines and circles, field of view, visibility polygons, flood fills, connected regions and union-find, and overlap worlds that find the boxes that reach each other. | [`lua-api/spatial2d.md`](../lua-api/spatial2d.md) |
| `haylen.tiled` | Tiled maps and worlds: drawing with y sorting, maps built at run time with terrain painting and saved as Tiled JSON, automapping, collision, objects and spawning, ray casts against tiles and objects, and object outlines for navigation meshes. | [`lua-api/tiled.md`](../lua-api/tiled.md) |
