# 🧊 3D

The 3D block holds what builds 3D games and apps, starting with the conventions that every 3D module follows, the math of 3D space and the spatial structures that find things in it. Its Lua modules end with `3d`, next to the 2D modules, while the math of both dimensions lives in `haylen.math`.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| Coordinates | The conventions of 3D: right-handed axes with `y` up, meters, radians, matrices by columns, unit quaternions, counter-clockwise front faces, reversed depth and the range of each graphics API, and light in linear space. | [`coordinates.md`](coordinates.md) |
| Math | Vectors in 3D and how they carry points and directions, matrices that place, view and project, rotations and the forms that build them, transforms in a hierarchy with what they cannot hold, dual quaternions for skinning, the shapes with their ray casts and culling by the view, curves with their frames, paths of curves and NURBS surfaces and volumes, noise and random points in 3D, planes, the errors of what has no direction or no inverse, and comparing with tolerance. | [`math.md`](math.md) |
| Spatial | Which spatial structure suits which task, culling by the camera, queries that allocate nothing, batches of many objects, building and refitting, exact ray casts against meshes, walking a voxel grid and what each costs. | [`spatial.md`](spatial.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.spatial3d` | Spatial hashes, loose octrees, dynamic AABB trees, bounding volume hierarchies and k-d trees with box, sphere, point, frustum, ray and nearest queries and batches of many objects, the bounding volume hierarchy of a mesh for exact ray casts and closest points, and the walk of the cells of a voxel grid. | [`lua-api/spatial3d.md`](../lua-api/spatial3d.md) |
