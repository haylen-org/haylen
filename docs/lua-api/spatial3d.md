# 📖 `haylen.spatial3d`

The module `haylen.spatial3d` finds things in 3D space without testing every object. It has five spatial structures that store Lua values by their boxes or spheres and answer box, sphere, point, frustum, ray and nearest queries, a hierarchy of the triangles of a mesh for exact ray casts and closest points, and the walk of the cells of a grid of boxes. Distances and sizes are in meters. The [spatial guide](../3d/spatial.md) explains which structure suits which task.

```lua
local spatial3d = require('haylen.spatial3d')
```

## Structures

The five structures are [`HashGrid3D`](#hashgrid3d), [`Octree`](#octree), [`AabbTree3D`](#aabbtree3d), [`Bvh`](#bvh) and [`KdTree3D`](#kdtree3d). They work the same way:

- A structure stores any Lua value except `nil`, usually the table of an object. Queries return the values themselves.
- Values are compared by identity, the way table keys are. A structure keeps every stored value alive until it is removed or the structure is cleared.
- The methods `query`, `querySphere`, `queryPoint` and `queryFrustum` list the values in the order they were added, so results never depend on how the entries moved. The methods `raycast` and `kNearest` list them by distance, and a tie goes to the value added first.
- Boxes that touch count as overlapping, so a box of size zero works as a point.
- A query takes an optional table `results` as its last argument, which it fills from position 1, clears after the last value and returns. A query that runs every frame with the same table allocates nothing.

The structures `HashGrid3D`, `Octree`, `AabbTree3D` and `Bvh` store boxes, which are `Box` values of [`haylen.math`](math.md#box). A box that is not finite or whose largest corner lies below its smallest corner on an axis raises `Spatial bounds must be finite, and no coordinate of their largest corner may lie below the one of their smallest corner.` A `KdTree3D` stores points with an optional radius instead, and its queries treat each point as a sphere.

### spatial3d.newHashGrid(cellSize)

Returns a [`HashGrid3D`](#hashgrid3d), a spatial hash whose cubic cells are `cellSize` meters wide. A spatial hash files every entry under the cells that its box covers, and a cell about the size of a typical object, or a little larger, keeps queries fast. A size that is not positive and finite raises `A spatial hash needs a positive cell size.` A stored box covers at most 65536 cells, or `hash:set` raises `A spatial hash entry may cover at most 65536 cells, so the cell size should be closer to the size of the entries.`, and it lies within 1048575 cells of the origin, or `hash:set` raises `A spatial hash entry must lie within 1048575 cells of the origin.`

```lua
local spatial3d = require('haylen.spatial3d')

local hash = spatial3d.newHashGrid(2)
print(hash.cellSize) -- 2.0
```

### spatial3d.newOctree(region, options)

Returns an [`Octree`](#octree) over the `Box` `region`. An octree splits a crowded cell of its region into eight smaller cells, again and again. Each cell reaches past its own edges by `looseness` times half its size, so an entry goes down to the deepest cell that holds it whole and moves less often. Entries outside the region still work, and they stay in the root cell. The table `options` is optional, and an unknown key raises `Unknown option "<key>".`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `maxEntries` | integer | `8` | How many entries a cell holds before it splits in eight. |
| `maxDepth` | integer | `8` | How many times cells may split, from 0 to 16. |
| `looseness` | number | `1` | How far each cell reaches past its edges, as a share of half its size, from 0 to 4. |

A region without volume or a setting out of range raises `An octree needs a box with volume, room for an entry per node, a depth from 0 to 16 and a looseness from 0 to 4.`

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local level = spatial3d.newOctree(m.box({-200, -20, -200}, {200, 80, 200}), {maxEntries = 16})
print(level.region.max) -- Vec3(200.0, 80.0, 200.0)
```

### spatial3d.newAabbTree(margin)

Returns an [`AabbTree3D`](#aabbtree3d), a dynamic tree of boxes. It keeps a box around each entry that is `margin` meters larger on every side, `0.1` by default, and it moves an entry in the tree only when the entry leaves that box. A margin that is negative or not finite raises `An AABB tree needs a finite margin of zero or more.`

```lua
local spatial3d = require('haylen.spatial3d')

local bodies = spatial3d.newAabbTree(0.2)
print(bodies.margin) -- 0.2
```

### spatial3d.newBvh()

Returns an empty [`Bvh`](#bvh), a bounding volume hierarchy that a query needs built with `bvh:build` after entries come or go, and fitted with `bvh:refit` after they only move.

```lua
local spatial3d = require('haylen.spatial3d')

local props = spatial3d.newBvh()
print(props.built) -- true, since an empty tree needs no build
```

### spatial3d.newKdTree()

Returns an empty [`KdTree3D`](#kdtree3d), a tree of points that splits space along `x`, `y` and `z` in turn. It answers nearest queries fastest, and a query needs it rebuilt with `kdTree:build` after its points change.

```lua
local spatial3d = require('haylen.spatial3d')

local probes = spatial3d.newKdTree()
print(probes.size) -- 0
```

## Structure methods

These methods work on every structure, except where a section says otherwise.

### structure:set(value, box)

Stores `value` with the `Box` `box` in a `HashGrid3D`, an `Octree`, an `AabbTree3D` or a `Bvh`. When the value is already stored, it moves to the new box. A `nil` value raises `bad argument #1 to 'set' (a value to store is required)`, and a box that the structure refuses leaves the structure as it was. A `KdTree3D` takes a point instead, as [`kdTree:set`](#kdtreesetvalue-point-radius) describes.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local bodies = spatial3d.newAabbTree()
local crate = {name = 'crate'}
bodies:set(crate, m.boxFromCenter({0, 0.5, -4}, {1, 1, 1}))
print(bodies:has(crate)) -- true
```

### structure:setAll(values, buffer)

Stores or moves every value of the list `values` in one call, with the bounds that the [float buffer](collections.md#collectionsnewfloatbuffersize-value) `buffer` holds in turn: six numbers for each value of a box structure, its smallest corner and then its largest, and four numbers for each value of a `KdTree3D`, its center and then its radius. The structure checks every entry before it stores any, so a batch with a wrong entry raises its error and changes nothing. A buffer with fewer numbers than the values need raises `The buffer of a batch of <count> values needs <numbers> numbers, but it holds <size>.`, and a `nil` in the list raises `Item <index> of the values of a batch is nil, but every item needs a value to store.`

```lua
local collections = require('haylen.collections')
local spatial3d = require('haylen.spatial3d')

local crowd = spatial3d.newHashGrid(2)
local people, bounds = {}, collections.newFloatBuffer(6 * 100)
for index = 1, 100 do
    people[index] = {name = 'person' .. index}
    local x = index * 1.5
    bounds:set((index - 1) * 6 + 1, {x, 0, 0, x + 0.5, 1.8, 0.5})
end

crowd:setAll(people, bounds)
print(crowd.size) -- 100
```

### structure:remove(value)

Removes `value` and returns `true`, or returns `false` when the value is not stored.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local bodies = spatial3d.newAabbTree()
local crate = {}
bodies:set(crate, m.box({0, 0, 0}, {1, 1, 1}))
print(bodies:remove(crate), bodies:remove(crate)) -- true false
```

### structure:has(value)

Returns `true` when `value` is stored.

```lua
local spatial3d = require('haylen.spatial3d')

print(spatial3d.newHashGrid(1):has({})) -- false
```

### structure:bounds(value)

Returns the `Box` stored for `value` in a `HashGrid3D`, an `Octree`, an `AabbTree3D` or a `Bvh`, or `nil` when the value is not stored. A `KdTree3D` has [`kdTree:sphere`](#kdtreespherevalue) instead.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local bodies = spatial3d.newAabbTree()
local crate = {}
bodies:set(crate, m.box({0, 0, 0}, {1, 1, 1}))
print(bodies:bounds(crate).max) -- Vec3(1.0, 1.0, 1.0)
```

### structure:query(box, results)

Returns the list of values whose bounds touch the `Box` `box`.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local bodies = spatial3d.newAabbTree()
bodies:set({name = 'crate'}, m.box({0, 0, 0}, {1, 1, 1}))
print(#bodies:query(m.box({0.5, 0.5, 0.5}, {3, 3, 3}))) -- 1
```

### structure:querySphere(sphere, results)

Returns the list of values whose bounds the `Sphere` `sphere` touches, such as everything a blast reaches. A sphere whose radius is negative or not finite raises `A spatial sphere needs a finite radius of zero or more.`

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local bodies = spatial3d.newAabbTree()
bodies:set({name = 'barrel'}, m.boxFromCenter({3, 0.5, 0}, {1, 1, 1}))
print(#bodies:querySphere(m.sphere({0, 0, 0}, 3))) -- 1
```

### structure:queryPoint(point, results)

Returns the list of values whose bounds hold the point `point`.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local rooms = spatial3d.newBvh()
rooms:set({name = 'hall'}, m.box({0, 0, -10}, {10, 4, 0}))
rooms:build()
print(rooms:queryPoint({5, 1.7, -5})[1].name) -- hall
```

### structure:queryFrustum(frustum, results)

Returns the list of values that the `Frustum` `frustum` of a camera may see, which skips what the camera cannot see before it draws. Every structure returns exactly the values whose bounds the frustum does not classify as outside with [`frustum:classify`](math.md#frustumclassifyshape-tolerance). The trees and the k-d tree take whole branches that lie inside the frustum without testing them, while a `HashGrid3D` tests each value once, since a frustum reaches across far more cells than its values fill.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local camera = m.frustum(m.perspective({farPlane = 200}) * m.lookAt({0, 2, 10}, {0, 2, 0}))
local props = spatial3d.newAabbTree()
props:set({name = 'ahead'}, m.boxFromCenter({0, 2, -5}, {1, 1, 1}))
props:set({name = 'behind'}, m.boxFromCenter({0, 2, 30}, {1, 1, 1}))
local visible = {}
props:queryFrustum(camera, visible)
print(#visible, visible[1].name) -- 1 ahead
```

### structure:raycast(ray, limit)

Returns the list of hits of the `Ray3D` `ray` on the bounds of the stored values, from the origin of the ray on, at most `limit` of them, or every hit when `limit` is `nil` or `0`. Each hit is a table with the `value`, the `distance` from the origin of the ray, the `point` and the `normal` of the face it hits, like the hits of [`ray:cast`](math.md#raycastshape). Bounds are solid, so a ray that starts inside them hits at distance `0` with a zero normal.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local bodies = spatial3d.newAabbTree()
bodies:set({name = 'wall'}, m.box({-5, 0, -10}, {5, 3, -9}))
local hits = bodies:raycast(m.ray3d({0, 1.7, 0}, m.forward()), 1)
print(hits[1].value.name, hits[1].distance) -- wall 9.0
```

### structure:nearest(point, maxDistance, accept)

Returns the value whose bounds lie closest to `point` within `maxDistance`, which is `math.huge` when it is `nil`, or `nil` when none is close enough. With the function `accept`, it calls `accept(value)` on the candidates from the closest one on and returns the first that it accepts.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local items = spatial3d.newHashGrid(4)
items:set({name = 'coin'}, m.boxFromCenter({2, 0, 0}, {0.2, 0.2, 0.2}))
items:set({name = 'key', heavy = true}, m.boxFromCenter({1, 0, 0}, {0.2, 0.2, 0.2}))
local light = items:nearest({0, 0, 0}, 10, function(item) return not item.heavy end)
print(light.name) -- coin
```

### structure:kNearest(point, count, maxDistance, results)

Returns the list of up to `count` values whose bounds lie closest to `point` within `maxDistance`, `math.huge` by default, closest first.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local lights = spatial3d.newBvh()
for index = 1, 10 do
    lights:set({index = index}, m.boxFromCenter({index * 4, 3, 0}, {0.1, 0.1, 0.1}))
end

lights:build()
print(#lights:kNearest({0, 0, 0}, 4)) -- 4
```

### structure:clear()

Removes every value.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local bodies = spatial3d.newAabbTree()
bodies:set({}, m.box({0, 0, 0}, {1, 1, 1}))
bodies:clear()
print(bodies.size) -- 0
```

### structure.size

The number of stored values.

```lua
local spatial3d = require('haylen.spatial3d')

print(spatial3d.newOctree(require('haylen.math').box({0, 0, 0}, {10, 10, 10})).size) -- 0
```

## HashGrid3D

A `HashGrid3D` is a spatial hash that [`spatial3d.newHashGrid`](#spatial3dnewhashgridcellsize) returns. Moving an entry costs about the same as storing it, so it suits many entries of a similar size that move every frame, such as a crowd or bullets. It has every [structure method](#structure-methods), and the property `hash.cellSize`, the edge of its cells in meters.

```lua
local spatial3d = require('haylen.spatial3d')

print(spatial3d.newHashGrid(0.5).cellSize) -- 0.5
```

## Octree

An `Octree` is a loose octree over a region that [`spatial3d.newOctree`](#spatial3dnewoctreeregion-options) returns. It suits entries of varied sizes spread over a known region, such as the objects of a level. It has every [structure method](#structure-methods), the property `octree.region`, the `Box` it covers, and the property `octree.nodeCount`, the number of cells in use.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local level = spatial3d.newOctree(m.box({0, 0, 0}, {100, 100, 100}))
print(level.nodeCount) -- 1
```

## AabbTree3D

An `AabbTree3D` is a dynamic tree of boxes that [`spatial3d.newAabbTree`](#spatial3dnewaabbtreemargin) returns. It suits many moving entries of any size over space without limits, and many ray casts. It has every [structure method](#structure-methods), the property `tree.margin`, and the property `tree.height`, the number of levels below its root, which balancing keeps near the logarithm of its size.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local bodies = spatial3d.newAabbTree()
for index = 1, 64 do
    bodies:set({}, m.boxFromCenter({index, 0, 0}, {0.5, 0.5, 0.5}))
end

print(bodies.height <= 12) -- true
```

## Bvh

A `Bvh` is a bounding volume hierarchy that [`spatial3d.newBvh`](#spatial3dnewbvh) returns, built by the surface area heuristic. It answers queries fastest of the box structures, so it suits content that rarely changes, such as the props of a level. It has every [structure method](#structure-methods) and the methods below. A query on a tree that changed since its last build or refit raises `The BVH changed since its last build. Call "build", or "refit" when its entries only moved, before a query.`

### bvh:build()

Builds the tree from the stored values, in time proportional to n log n.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local props = spatial3d.newBvh()
props:set({}, m.box({0, 0, 0}, {1, 1, 1}))
props:build()
print(props.built) -- true
```

### bvh:refit()

Fits the boxes of the tree to values that moved, in time proportional to n, keeping the shape of the tree. Values that came or went since the last build raise `Entries came or went since the last build of the BVH, so it needs "build" instead of "refit".`

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local props = spatial3d.newBvh()
local door = {}
props:set(door, m.box({0, 0, 0}, {1, 2, 0.1}))
props:build()
props:set(door, m.box({0, 0, 1}, {1, 2, 1.1}))
props:refit()
print(#props:queryPoint({0.5, 1, 1.05})) -- 1
```

### bvh.built

`true` when queries may run, which is after a build or a refit that followed every change.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local props = spatial3d.newBvh()
props:set({}, m.box({0, 0, 0}, {1, 1, 1}))
print(props.built) -- false
```

### bvh.nodeCount and bvh.depth

The number of nodes of the tree, and the number of levels below its root, from its last build.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local props = spatial3d.newBvh()
for index = 1, 100 do
    props:set({}, m.boxFromCenter({index, 0, 0}, {0.5, 0.5, 0.5}))
end

props:build()
print(props.nodeCount > 1, props.depth > 0) -- true true
```

## KdTree3D

A `KdTree3D` is a tree of points that [`spatial3d.newKdTree`](#spatial3dnewkdtree) returns. It suits points that rarely change, when nearest queries come most often, such as light probes or spawn points. Its points change freely, but a query on a tree that changed raises `The k-d tree changed since it was last built, so it needs "build" before a query.` It has every [structure method](#structure-methods) and the methods below.

### kdTree:set(value, point, radius)

Stores `value` at the point `point` with the radius `radius`, `0` by default, or moves it when it is already stored. A point that is not finite or a radius that is negative or not finite raises `A k-d tree entry needs a finite point and a finite radius of zero or more.`

```lua
local spatial3d = require('haylen.spatial3d')

local probes = spatial3d.newKdTree()
probes:set({name = 'hall'}, {0, 2, 0}, 0.5)
probes:build()
print(probes:kNearest({1, 2, 0}, 1)[1].name) -- hall
```

### kdTree:sphere(value)

Returns the `Sphere` stored for `value`, or `nil` when the value is not stored.

```lua
local spatial3d = require('haylen.spatial3d')

local probes = spatial3d.newKdTree()
local probe = {}
probes:set(probe, {0, 2, 0}, 0.5)
print(probes:sphere(probe).radius) -- 0.5
```

### kdTree:build()

Rebuilds the tree from the stored points, in time proportional to n log n.

```lua
local spatial3d = require('haylen.spatial3d')

local probes = spatial3d.newKdTree()
probes:set({}, {0, 0, 0})
probes:build()
print(probes.built) -- true
```

### kdTree.built

`true` when the tree holds every change, so queries may run.

```lua
local spatial3d = require('haylen.spatial3d')

local probes = spatial3d.newKdTree()
probes:set({}, {0, 0, 0})
print(probes.built) -- false
```

## Meshes

### spatial3d.newMeshBvh(positions, indices)

Returns a [`MeshBvh`](#meshbvh) over the triangles of a mesh. The positions are a [float buffer](collections.md#collectionsnewfloatbuffersize-value) with three numbers for each vertex or a list of `Vec3` values, and the optional list `indices` names three vertices for each triangle, counted from 1. Without indices, the positions list every triangle in turn. A position that is not finite raises `Every position of a mesh must be a finite number.`, counts that do not make whole vertices or triangles raise `The positions of a mesh need three numbers for each vertex.`, `A mesh without indices needs three vertices for each triangle.` or `The indices of a mesh need three vertices for each triangle.`, and an index outside the vertices raises `Item <item> of the indices of a mesh names vertex <vertex>, but the vertex count of the mesh is <count>.` or `Item <item> of the indices of a mesh must name a vertex as a whole number from 1 on.`

```lua
local spatial3d = require('haylen.spatial3d')

local floor = spatial3d.newMeshBvh({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}, {0, 0, -10}}, {1, 2, 3, 1, 3, 4})
print(floor.triangleCount) -- 2
```

## MeshBvh

A `MeshBvh` is a bounding volume hierarchy over the triangles of a mesh that [`spatial3d.newMeshBvh`](#spatial3dnewmeshbvhpositions-indices) returns. It casts rays against the exact surface, and a ray never slips between two triangles that share an edge. Triangles count from 1 in the order of the mesh.

### mesh:raycast(ray)

Returns the nearest hit of the `Ray3D` `ray` on the surface, or `nil`. The hit is a table with the `triangle`, the `point`, the `normal` of the side the ray hits, which faces the ray, `front`, which is `true` when that side is the front of the triangle, where its corners run counter-clockwise, the `barycentric` weights of its three corners at the point as a `Vec3`, and the `distance` from the origin of the ray. Of hits at the same distance, the triangle that comes first wins.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local floor = spatial3d.newMeshBvh({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}, {0, 0, -10}}, {1, 2, 3, 1, 3, 4})
local hit = floor:raycast(m.ray3d({3, 5, -1}, -m.up()))
print(hit.triangle, hit.distance, hit.front) -- 1 5.0 true
```

### mesh:intersects(ray)

Returns `true` when the `Ray3D` `ray` hits the surface anywhere, which stops at the first hit, such as for a line of sight.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local wall = spatial3d.newMeshBvh({{-5, 0, -3}, {5, 0, -3}, {0, 5, -3}})
print(wall:intersects(m.ray3dBetween({0, 1, 0}, {0, 1, -10}))) -- true
```

### mesh:closestPoint(point, maxDistance)

Returns the point of the surface closest to `point` within `maxDistance`, `math.huge` by default, as a table with the `triangle`, the `point` and the `distance`, or `nil` when the surface lies farther away.

```lua
local spatial3d = require('haylen.spatial3d')

local floor = spatial3d.newMeshBvh({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}, {0, 0, -10}}, {1, 2, 3, 1, 3, 4})
local closest = floor:closestPoint({3, 4, -1})
print(closest.point, closest.distance) -- close to Vec3(3.0, 0.0, -1.0), and 4.0
```

### mesh:queryBox(box, results)

Returns the list of the triangles that touch the `Box` `box`, in ascending order, by the separating axes of the triangle and the box. The table `results` is reused as in the [structure queries](#structures).

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local floor = spatial3d.newMeshBvh({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}, {0, 0, -10}}, {1, 2, 3, 1, 3, 4})
print(#floor:queryBox(m.box({8, -1, -1}, {9, 1, 0}))) -- 1
```

### mesh:querySphere(sphere, results)

Returns the list of the triangles that the `Sphere` `sphere` touches, in ascending order, such as the triangles under a character to slide along.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local floor = spatial3d.newMeshBvh({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}, {0, 0, -10}}, {1, 2, 3, 1, 3, 4})
print(#floor:querySphere(m.sphere({5, 0, -5}, 0.1))) -- 2, the diagonal that both triangles share
```

### mesh:triangle(index)

Returns the `Triangle` with its three corners. A triangle past the last one raises `The triangle count of the mesh is <count>, so it has no triangle <index>.`

```lua
local spatial3d = require('haylen.spatial3d')

local floor = spatial3d.newMeshBvh({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}, {0, 0, -10}}, {1, 2, 3, 1, 3, 4})
print(floor:triangle(2).c) -- Vec3(0.0, 0.0, -10.0)
```

### mesh.vertexCount, mesh.triangleCount and mesh.bounds

The number of vertices and of triangles, and the `Box` that holds every triangle, or `nil` for a mesh without triangles. The properties `mesh.nodeCount` and `mesh.depth` tell the size and the depth of the hierarchy.

```lua
local spatial3d = require('haylen.spatial3d')

local floor = spatial3d.newMeshBvh({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}, {0, 0, -10}}, {1, 2, 3, 1, 3, 4})
print(floor.vertexCount, floor.triangleCount, floor.bounds.min) -- 4 2 Vec3(0.0, 0.0, -10.0)
```

## Grids

### spatial3d.traverseGrid(from, to, cellSize)

Returns the list of the cells of a grid of boxes that the segment from `from` to `to` crosses, in order, the walk of a voxel world. The first cell starts at the world origin, and `cellSize` is a number for cubic cells or a `Vec3`. Each cell is a table with its `x`, `y` and `z`, which count from the cell at the origin, the `distance` along the segment where it enters the cell and the `normal` of the face it enters through, which is zero for the first cell. A segment through an exact edge or corner of cells steps along `x` first, then `y`. A traversal lists at most 65536 cells, or it raises `A grid traversal lists at most 65536 cells.`, and cells without a positive and finite size raise `Grid cells need a positive and finite size.`

```lua
local spatial3d = require('haylen.spatial3d')

for _, cell in ipairs(spatial3d.traverseGrid({0.5, 0.5, 0.5}, {2.5, 0.5, 0.5}, 1)) do
    print(cell.x, cell.y, cell.z, cell.distance)
end
```

## Errors

| Message | Cause |
| --- | --- |
| `Spatial bounds must be finite, and no coordinate of their largest corner may lie below the one of their smallest corner.` | A box of a structure or a query is not finite or is turned inside out. |
| `A spatial query point must be finite.` | The point of a query is not finite. |
| `A spatial query radius must not be negative.` | The `maxDistance` of a nearest query is negative. |
| `A spatial sphere needs a finite radius of zero or more.` | The sphere of a query has a negative or endless radius. |
| `A spatial hash needs a positive cell size.` | The cell size of `spatial3d.newHashGrid` is not positive and finite. |
| `A spatial hash entry may cover at most 65536 cells, so the cell size should be closer to the size of the entries.` | A box of a `HashGrid3D` is far larger than its cells. |
| `A spatial hash entry must lie within 1048575 cells of the origin.` | A box of a `HashGrid3D` lies too far away. |
| `An octree needs a box with volume, room for an entry per node, a depth from 0 to 16 and a looseness from 0 to 4.` | The region or a setting of `spatial3d.newOctree` is out of range. |
| `An AABB tree needs a finite margin of zero or more.` | The margin of `spatial3d.newAabbTree` is negative or not finite. |
| `The BVH changed since its last build. Call "build", or "refit" when its entries only moved, before a query.` | A `Bvh` was queried after a change and before `bvh:build` or `bvh:refit`. |
| `Entries came or went since the last build of the BVH, so it needs "build" instead of "refit".` | A `Bvh` gained or lost values since its last build. |
| `The k-d tree changed since it was last built, so it needs "build" before a query.` | A `KdTree3D` was queried after a change and before `kdTree:build`. |
| `A k-d tree entry needs a finite point and a finite radius of zero or more.` | The point or the radius of `kdTree:set` is out of range. |
| `A batch of spatial entries needs as many bounds as ids.` | The bounds of a batch do not match its values. |
| `The buffer of a batch of <count> values needs <numbers> numbers, but it holds <size>.` | The float buffer of `structure:setAll` is too short. |
| `Item <index> of the values of a batch is nil, but every item needs a value to store.` | The list of `structure:setAll` has a hole. |
| `Grid cells need a positive and finite size.` | The cell size of `spatial3d.traverseGrid` is not positive and finite. |
| `A grid traversal lists at most 65536 cells.` | The segment of `spatial3d.traverseGrid` crosses too many cells. |
