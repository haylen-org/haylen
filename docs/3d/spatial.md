# 🗂️ 3D spatial structures

This guide explains how `haylen.spatial3d` finds things in 3D space without testing every object: which structure suits which task, how the queries answer, how many objects change in one call, how a mesh answers exact ray casts and how a ray walks the cells of a voxel world. The [reference of `haylen.spatial3d`](../lua-api/spatial3d.md) lists every function with its options and errors, and the [3D math guide](../core/math.md) explains the boxes, spheres, rays and frustums that the queries take.

## Why spatial structures

A scene of thousands of objects asks the same questions every frame: what the camera sees, what a shot hits, what lies near the player, what a blast reaches. Testing every object costs time that grows with the number of objects, for every question. A spatial structure files the objects by where they are, so a question visits only the part of space it asks about and skips the rest in whole branches.

## Choosing a structure

| Structure | Best for | Cost of a change | Weak spot |
| --- | --- | --- | --- |
| [`HashGrid3D`](../lua-api/spatial3d.md#hashgrid3d) | Many objects of a similar size that move every frame, such as a crowd, particles or bullets. | Cheap, often only new bounds. | Objects far larger than its cells, and culling by the camera. |
| [`Octree`](../lua-api/spatial3d.md#octree) | Objects of varied sizes spread over a known region, such as the props of a level. | Cheap while an object stays in the reach of its cell. | Space far outside its region, which stays in one cell. |
| [`AabbTree3D`](../lua-api/spatial3d.md#aabbtree3d) | Many moving objects of any size over space without limits, and many ray casts. | Free inside its margin, about the logarithm of its size outside it. | Nothing in particular. It is a good default. |
| [`Bvh`](../lua-api/spatial3d.md#bvh) | Content that rarely changes, when queries come most often, such as the static meshes of a level. | A build of the whole tree, or a refit in linear time when objects only move. | Objects that come and go every frame. |
| [`KdTree3D`](../lua-api/spatial3d.md#kdtree3d) | Points that rarely change, when nearest queries come most often, such as light probes or spawn points. | A rebuild of the whole tree. | Objects that move every frame. |

Every structure answers the same queries, so a game can start with an `AabbTree3D` and move to another structure once it measures what it asks most. The queries return values in the order they were added, or by distance for rays and nearest queries, so results never depend on the history of the structure.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local level = spatial3d.newOctree(m.box({-100, -10, -100}, {100, 40, 100}))
local barrel = {name = 'barrel'}
level:set(barrel, m.boxFromCenter({3, 0.5, -2}, {0.6, 1, 0.6}))

local blast = level:querySphere(m.sphere({0, 0, 0}, 5))
print(#blast, blast[1].name) -- 1 barrel
```

## Culling by the camera

The frustum of a camera holds what it sees, and `structure:queryFrustum` lists what may be drawn. The trees classify each branch against the frustum: a branch outside is skipped whole, a branch wholly inside joins without testing anything below it, and only the branches that cross a plane are opened. The test never calls a box outside that reaches inside, so nothing the camera sees disappears. Every structure returns exactly the values whose bounds the frustum does not classify as outside, the same answer as culling every box with `frustum:cull`, and a spatial hash, which has no branches, tests each of its values once.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local props = spatial3d.newBvh()
for index = 1, 1000 do
    props:set({index = index}, m.boxFromCenter({(index % 40) * 5 - 100, 1, -(index // 40) * 5}, {1, 2, 1}))
end

props:build()
local visible = {}
local camera = m.frustum(m.perspective({aspect = 16 / 9, farPlane = 120}) * m.lookAt({0, 3, 10}, {0, 1, -40}))
props:queryFrustum(camera, visible)
print(#visible, 'props to draw')
```

## Queries that allocate nothing

A query that runs every frame takes a table as its last argument, fills it from position 1, clears what an earlier and longer answer left after the last value, and returns it. The structure keeps the lists of its own queries between calls, so a frame that asks what it asked the frame before allocates nothing, neither in C++ nor in Lua.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local crowd = spatial3d.newHashGrid(2)
local nearby = {}
local function update(player)
    crowd:querySphere(m.sphere(player.position, 4), nearby)
    for _, person in ipairs(nearby) do
        person.alert = true
    end
end
```

## Many objects in one call

Moving thousands of objects one call at a time spends most of the time crossing from Lua into C++. The method `structure:setAll` takes the list of values and a float buffer with the bounds of each in turn, six numbers for a box and four for the sphere of a k-d tree, and stores them all in one call. The structure checks every entry before it changes anything, so a batch with one wrong box raises its error and leaves the structure as it was. A physics step or an animation system can write the buffer from C++ and the Lua side only hands it on.

```lua
local collections = require('haylen.collections')
local spatial3d = require('haylen.spatial3d')

local boids = spatial3d.newHashGrid(1)
local flock, bounds = {}, collections.newFloatBuffer(6 * 500)
for index = 1, 500 do
    flock[index] = {index = index}
end

local function place(time)
    for index = 1, 500 do
        local x, y, z = math.cos(index + time) * 20, 5 + math.sin(index * 3) * 2, math.sin(index + time) * 20
        bounds:set((index - 1) * 6 + 1, {x - 0.1, y - 0.1, z - 0.1, x + 0.1, y + 0.1, z + 0.1})
    end

    boids:setAll(flock, bounds)
end

place(0)
print(boids.size) -- 500
```

## Building, refitting and rebuilding

A `Bvh` builds its tree once by the surface area heuristic: it splits each group of boxes where the two halves cost the least to search, so queries visit few nodes. When objects only move, such as the doors and platforms of a level, `bvh:refit` fits the boxes of the tree again in linear time and keeps its shape. When objects come or go, `bvh:build` builds it again. A `KdTree3D` always rebuilds with `kdTree:build`. Both refuse a query on a tree that changed since its last build, so a missing build shows where it happens instead of giving answers about old positions.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local platforms = spatial3d.newBvh()
local lift = {name = 'lift'}
platforms:set(lift, m.box({0, 0, 0}, {2, 0.2, 2}))
platforms:build()

local function raise(height)
    platforms:set(lift, m.box({0, height, 0}, {2, height + 0.2, 2}))
    platforms:refit()
end

raise(3)
print(#platforms:queryPoint({1, 3.1, 1})) -- 1
```

## Exact ray casts against meshes

A box around a character is enough to know that a shot passes near it, but not where it hits the model. A `MeshBvh` holds the triangles of a mesh in a hierarchy of their own, so a ray finds the nearest triangle it hits after visiting a few nodes, with the point, the normal, the side it hits and the weights of the corners there, which give the texture coordinate or the color of the point. A ray never slips between two triangles that share an edge, so a ray through a seam of a floor always lands. The closest point of the surface and the triangles near a box or a sphere come from the same hierarchy, such as for a character that slides along a wall.

The positions come as three floats for each vertex and the indices as three vertices for each triangle, the form of the meshes of glTF, so C++ code moves the positions that a glTF reader produces into a `MeshBvh` without a copy.

```lua
local m = require('haylen.math')
local spatial3d = require('haylen.spatial3d')

local floor = spatial3d.newMeshBvh({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}, {0, 0, -10}}, {1, 2, 3, 1, 3, 4})
local camera = m.perspective({aspect = 16 / 9}) * m.lookAt({5, 8, 5}, {5, 0, -5})
local hit = floor:raycast(m.ray3dFromView(camera, {0, 0}))
print(hit.point, hit.triangle) -- the point of the floor at the center of the view
```

## Walking a voxel grid

A world made of blocks keeps its blocks in cells of a grid, and a ray through it, such as the aim of a player who digs, crosses the cells one face at a time. The function `spatial3d.traverseGrid` lists those cells in order with the distance and the face where the ray enters each one, so the first solid cell is the block under the aim and its face tells where a new block goes.

```lua
local spatial3d = require('haylen.spatial3d')

local function isSolid(cell)
    return cell.y < 0
end

for _, cell in ipairs(spatial3d.traverseGrid({0.5, 3.5, 0.5}, {6.5, -2.5, 0.5}, 1)) do
    if isSolid(cell) then
        print('Dig', cell.x, cell.y, cell.z, 'place on', cell.normal)
        break
    end
end
```

## Costs

Every structure keeps the cost of a query close to the part of space it asks about:

| Structure | Change | Query |
| --- | --- | --- |
| `HashGrid3D` | The cells the entry covers. A move inside the same cells only updates the bounds. | The cells the query covers, and never much more than a visit to every entry. A frustum tests every entry. |
| `Octree` | The depth of the tree. A move inside the reach of its cell only updates the bounds. | About the logarithm of the size, plus the entries found. |
| `AabbTree3D` | Free inside the margin. Outside it, about the logarithm of the size. | About the logarithm of the size, plus the entries found. |
| `Bvh` | A build in n log n, a refit in n. | About the logarithm of the size, plus the entries found, with the fewest nodes of all. |
| `KdTree3D` | A rebuild in n log n. | About the logarithm of the size for nearest queries. |

The command `haylen engine bench --suite algorithms` times every structure against a search through every object, for the same scene and the same queries: 100000 boxes of one to four meters spread over a kilometer, and a mesh of 200000 triangles. These are the times of one query on one core of an x86-64 cloud machine in a Release build:

| Structure | Fill | Box query | Frustum query | First hit of a ray | 8 nearest |
| --- | --- | --- | --- | --- | --- |
| `HashGrid3D` | 121 ms | 11 µs | 2.9 ms | 3 µs | 85 µs |
| `Octree` | 29 ms | 9 µs | 1.8 ms | 24 µs | 32 µs |
| `AabbTree3D` | 76 ms | 10 µs | 1.1 ms | 36 µs | 20 µs |
| `Bvh` | 52 ms to build, 0.5 ms to refit | 1.2 µs | 0.5 ms | 3.4 µs | 2.6 µs |
| `KdTree3D` | 26 ms to build | 1.5 µs | 0.7 ms | 6.8 µs | 4.1 µs |
| Every box | None | 330 µs | 0.9 ms with `frustum:cull` | None | None |

The frustum sees about one box in six, so its time is mostly the list of the boxes it returns. A `MeshBvh` builds over the 200000 triangles in 59 ms, finds the first hit of a ray in 1.6 µs and the closest point of the surface in 2.4 µs, where testing every triangle takes 4.4 ms for each ray.

## C++

The C++ types live in `haylen/3d/spatial/` in the namespace `haylen::spatial3d`: `HashGrid`, `Octree`, `AabbTree`, `Bvh`, `KdTree`, `MeshBvh`, `GridRay` and `BvhNodes`, the nodes and walks that `Bvh` and `MeshBvh` share and that a plugin builds its own hierarchy with. Their queries fill vectors that the caller owns and walk on fixed stacks, so a query that repeats allocates nothing.
