# 🧊 3D

The 3D block holds what builds 3D games and apps, starting with the conventions that every 3D module follows, the math of 3D space, the reader of glTF models and the spatial structures that find things in it. Its Lua modules end with `3d`, next to the 2D modules, while the math of both dimensions lives in `haylen.math`.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| Coordinates | The conventions of 3D: right-handed axes with `y` up, meters, radians, matrices by columns, unit quaternions, counter-clockwise front faces, reversed depth and the range of each graphics API, and light in linear space. | [`coordinates.md`](coordinates.md) |
| glTF | The glTF 2.0 files that Haylen reads, as GLB or as `.gltf` with files beside it or `data:` URIs inside the package, the three steps of a read, the document with its objects, the rules of the specification that the reader checks, its limits, its extensions, reading from C++ and what a read costs. | [`gltf.md`](gltf.md) |
| Math | Vectors in 3D and how they carry points and directions, matrices that place, view and project, rotations and the forms that build them, transforms in a hierarchy with what they cannot hold, dual quaternions for skinning, the shapes with their ray casts and culling by the view, curves with their frames, paths of curves and NURBS surfaces and volumes, noise and random points in 3D, planes, the errors of what has no direction or no inverse, and comparing with tolerance. | [`math.md`](math.md) |
| Physics | 3D worlds in meters: their settings in `haylen.json`, steps and their passes, sleeping, threads on the job system, the memory of a step, the limits a world reports, the state hash that checks determinism, and backends that plugins register in place of the built-in Jolt Physics. | [`physics.md`](physics.md) |
| Spatial | Which spatial structure suits which task, culling by the camera, queries that allocate nothing, batches of many objects, building and refitting, exact ray casts against meshes, walking a voxel grid and what each costs. | [`spatial.md`](spatial.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.gltf` | Reading glTF 2.0 files into a read-only document whose objects form one graph of tables, accessors read as float buffers and lists of integers, and images decoded on the frame thread or the task pool. | [`lua-api/gltf.md`](../lua-api/gltf.md) |
| `haylen.physics3d` | 3D physics worlds in meters with their settings, steps, statistics, state hashes, owners and backends, their bodies and the collision shapes of the bodies. | [`lua-api/physics3d.md`](../lua-api/physics3d.md) |
| `haylen.spatial3d` | Spatial hashes, loose octrees, dynamic AABB trees, bounding volume hierarchies and k-d trees with box, sphere, point, frustum, ray and nearest queries and batches of many objects, the bounding volume hierarchy of a mesh for exact ray casts and closest points, and the walk of the cells of a voxel grid. | [`lua-api/spatial3d.md`](../lua-api/spatial3d.md) |
