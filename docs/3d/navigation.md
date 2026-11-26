# 🧭 3D navigation

This guide shows how characters find their way through 3D worlds with the module `haylen.navigation3d`. It explains what a navigation mesh is, how a world bakes into one, how areas and filters change the way paths choose, how obstacles and links change the mesh while the app runs, how crowds move many agents at once and what each part costs. Read it before you add enemies that chase the player through a level, units that obey clicks or a town full of walkers. The [reference of `haylen.navigation3d`](../lua-api/navigation3d.md) lists every function with its options and errors, and the [coordinates of 3D](coordinates.md) give the meters and axes it measures in.

## The navigation mesh

A navigation mesh covers the ground that an agent can walk with convex polygons. A path finds the polygons between two points and then pulls itself tight like a string, so it turns only at the corners of walls. The mesh knows the size of the agent it was baked for: it keeps the radius of the agent away from walls, ledges and obstacles, leaves out the ground under ceilings lower than the agent and joins steps no higher than the agent climbs. So an agent that walks a path never scrapes a wall or walks into a ceiling.

A mesh serves one size of agent. A game with people and trucks bakes one mesh for each, with the `agentRadius` and `agentHeight` of each.

The mesh is cut into tiles, squares of the ground of `tileSize` meters. Each tile bakes on its own, so tiles bake in parallel on the job system, one tile rebuilds without the others, and an obstacle that moves changes only the tiles around it.

## Baking a world

A [`SourceGeometry`](../lua-api/navigation3d.md#sourcegeometry) collects what the mesh bakes from: the triangles of meshes, the shapes that physics uses, such as boxes, spheres, capsules and cylinders, terrain as a grid of heights, volumes that mark areas and links across gaps. The bake turns the triangles into voxels of `cellSize` by `cellHeight` meters, keeps the voxels an agent can stand on, joins them into regions and outlines the regions with polygons. Smaller voxels follow the geometry more closely and bake slower, and a voxel about half the radius of the agent is a good start.

Triangles whose corners run counter-clockwise seen from above face up, as every front face of the engine does, so the floors and ramps of a level face up when the level faces out.

```lua
local m = require('haylen.math')
local navigation3d = require('haylen.navigation3d')

local level = navigation3d.newGeometry()
level:addHeightfield({0, 0, 0, 0}, 2, 2, {origin = {-30, 0, -30}, spacing = 60})
for index = 1, 12 do
    local x, z = (index % 4) * 12 - 18, (index // 4) * 14 - 16
    level:addBox(m.obb(m.vec3(x, 1, z), nil, m.vec3(2, 1, 1)))
end

local mesh = navigation3d.build(level, {agentRadius = 0.4, agentHeight = 1.8, agentMaxClimb = 0.5, tileSize = 24})
local path = mesh:findPath({-28, 0, -28}, {28, 0, 28})
print(#path.points .. ' points, ' .. string.format('%.1f', path.length) .. ' meters')
```

The bake of a level of many tiles takes long, so [`navigation3d.buildAsync`](../lua-api/navigation3d.md#navigation3dbuildasyncgeometry-settings) bakes every tile in a job of its own on the task pool and hands the mesh to the frame thread whole. A query never waits for a bake and never sees half of one.

A mesh that a level always uses bakes once. Its bytes, from [`mesh:encode`](../lua-api/navigation3d.md#meshencode), go into a `.navmesh` file of the content of the app, which `assets.load` reads on a worker thread. The reader treats every byte as hostile: it checks every count, index, area and voxel of the file, that each tile sits on its own column, that every polygon is convex and meets its neighbors edge to edge, that every polygon marks the edges of its outline and that no point lies so far away that its distances overflow, before the mesh takes any of it. The polygons that the voxels of an obstacle layer make pass the same checks of shape and neighbors before they reach the mesh. A file that breaks a rule fails with the sentence that it is damaged, so no file makes a query run forever or read past the mesh.

```lua
local assets = require('haylen.assets')

local mesh = assets.load('levels/harbor.navmesh')
print(mesh.tileCount .. ' tiles, ' .. mesh.polygonCount .. ' polygons')
```

### Partitions

The ground of a tile splits into regions before it becomes polygons, in one of three ways. `watershed`, the default, makes the most even polygons and suits most levels. `monotone` bakes fastest, with long thin polygons, which suits meshes that rebuild often. `layers` sits between them, and a mesh with obstacles always splits its tiles into layers.

## Areas and filters

Each piece of the geometry names its area, such as `road`, `water` or `door`. The area `ground` is the default, and the area `none` marks ground that is never walked, such as a hole that a volume cuts. A [`QueryFilter`](../lua-api/navigation3d.md#queryfilter) gives each area a cost and skips the areas it excludes. A path through an area costs its length times the cost of the area, so a cost of 4 on water makes an agent walk four meters around before it wades one. A cost is at least 1, since a cheaper area would let the search guess the rest of a path too long and miss the cheapest one.

```lua
local navigation3d = require('haylen.navigation3d')

local field = navigation3d.newGeometry()
field:addHeightfield({0, 0, 0, 0}, 2, 2, {origin = {-15, 0, -15}, spacing = 30, area = 'grass'})
field:addVolume({{-15, 0, 6}, {-15, 0, 9}, {15, 0, 9}, {15, 0, 6}}, -1, 1, {area = 'road'})
local mesh = navigation3d.build(field, {tileSize = 12})

local walker = mesh:newFilter({costs = {grass = 10}})
local cart = mesh:newFilter({excluded = {'grass'}})
print(mesh:findPath({-12, 0, 0}, {12, 0, 0}, {filter = walker}).length > 24) -- true
print(mesh:findNearest({0, 0, 0}, {filter = cart, searchExtents = {1, 1, 1}})) -- nil
```

A door that closes is an excluded area: name the volume of the doorway `door`, and the filter of a locked door excludes it. A crowd gives each agent its own filter, so a guard with the key walks through while the others go around.

## Paths

A path answers one of three ways, so an app never mistakes a wrong path for a right one:

- A full path reaches the goal, and its `partial` is `false`.
- A partial path ends at the point closest to the goal that the start reaches, because the goal is cut off, such as behind a closed door or on a ledge too high to climb. Its `partial` is `true`, so the app can show that the way is blocked.
- No path, `nil`, when the start or the goal is not within the search extents of the mesh, such as a click on the sky.

The search steps from polygon to polygon and weighs each step by the middles of their edges, which would bend a path across wide polygons toward their corners. So it also casts rays across the polygons it reaches, and takes the straight way where a ray is clear and costs less than the way through the polygon before, as the patch of the build guide on [shortcuts](../tools/build.md#detour-any-angle-costspatch) explains. Paths across open ground stay straight, and a cheap road still wins over costly grass. Steps are weighed at the edges of polygons, so a path is the cheapest way through the polygons and close to the cheapest line, not always exactly that line.

Every other query works on the same polygons:

| Query | Answers |
| --- | --- |
| [`mesh:findNearest`](../lua-api/navigation3d.md#meshfindnearestpoint-options) | The point of the mesh closest to a point, within the search extents, with its area. Use it to snap a click to the ground. |
| [`mesh:raycast`](../lua-api/navigation3d.md#meshraycaststart-end-options) | Where a straight walk along the ground stops at a wall. Use it to tell whether an agent sees its goal. |
| [`mesh:moveAlongSurface`](../lua-api/navigation3d.md#meshmovealongsurfacestart-end-options) | Where a short move slides to along the walls. Use it for a character that the player steers. |
| [`mesh:findDistanceToWall`](../lua-api/navigation3d.md#meshfinddistancetowallpoint-radius-options) | The closest wall and its normal, such as to keep a camera off walls. |
| [`mesh:findRandomPoint`](../lua-api/navigation3d.md#meshfindrandompointoptions-and-meshfindrandompointaroundcenter-radius-options) | A point where every square meter is as likely as any other, across the mesh or within a radius that the center reaches, such as for spawns and wandering. |

The search extents are an exact box: a point snaps only to the mesh inside it, even though the polygons are gathered by bounds a voxel coarser. And a random point around a center always lies within the radius, since each polygon counts with the part of it that the circle covers.

## Obstacles

A mesh baked with `obstacles` keeps its tiles as layers of voxels, and an obstacle carves the layers it stands on. A cylinder suits a barrel or a person who stands still, and a box turned by its yaw around the up axis suits a crate, a car or a closed gate. The mesh keeps every agent its radius away from an obstacle and its height below it, so an agent never walks into a crate.

A change of an obstacle marks the tiles around it, and the plugin of 3D navigation starts their rebuild on the task pool at the start of the next frame. The tiles take their place on the frame thread when they finish, and the agents whose corridors crossed them plan again. A tile that changes while its rebuild runs rebuilds again after it, so an obstacle that moves every frame costs one rebuild at a time for each of its tiles.

```lua
local navigation3d = require('haylen.navigation3d')
local scene = require('haylen.scene')

local yard = navigation3d.newGeometry()
yard:addHeightfield({0, 0, 0, 0}, 2, 2, {origin = {-40, 0, -40}, spacing = 80})
local mesh = navigation3d.build(yard, {obstacles = true, tileSize = 16})
local truck = mesh:addObstacle({shape = 'box', position = {0, 1, 0}, halfExtents = {1.5, 1, 4}})

scene.push({
    time = 0,
    update = function(self, dt)
        self.time = self.time + dt
        mesh:setObstacle(truck, {shape = 'box', position = {math.sin(self.time) * 10, 1, 0}, halfExtents = {1.5, 1, 4}, yaw = self.time})
    end,
})
```

A change of the world that is not an obstacle, such as a wall that falls or a bridge that the player builds, bakes its tiles again from new geometry with [`mesh:rebuildAsync`](../lua-api/navigation3d.md#meshrebuildasyncgeometry-box), which rebuilds every tile whose border reaches the box, so the edges of neighboring tiles keep meeting.

## Links

A link joins two places that the mesh does not connect: a jump down a ledge, a ladder, a teleporter or a gap to leap. Paths use links like any polygon, at the cost of their area, and a link that is not bidirectional goes one way only, such as a drop that nobody climbs back up. The point of a path where a link starts carries the id of the link, so the app knows where an agent jumps.

```lua
local navigation3d = require('haylen.navigation3d')

local cliff = navigation3d.newGeometry()
cliff:addHeightfield({0, 0, 0, 0}, 2, 2, {origin = {-20, 0, -20}, spacing = 40})
cliff:addBox(require('haylen.math').obb({5, 1.5, 0}, nil, {5, 1.5, 20}))
cliff:addLink({0.5, 3, 0}, {-1.5, 0, 0}, {kind = 'jump', bidirectional = false})
local mesh = navigation3d.build(cliff, {agentRadius = 0.4})
local path = mesh:findPath({6, 3, 0}, {-10, 0, 0})
print(path and path.partial) -- false
```

## Crowds

A [`Crowd3D`](../lua-api/navigation3d.md#crowd3d) moves many agents on one mesh and keeps them clear of each other. Each agent follows a corridor of polygons toward its target, which grows as it walks, so a long path never stalls a frame: a new target gets a quick search, and the rest of its path waits in a queue that spends at most `pathBudget` search steps of every step on all agents. Each agent steers toward the corners ahead, rounding them, slows down at the end of its path, and picks the velocity closest to the one it wants among those that hit no neighbor and no wall soon, sampled on the plane of the mesh. Agents that still overlap push each other apart, half of the way each, and every move slides along the mesh.

The work of each agent spreads over the job system, with a query of its own for each thread, and each agent reads only what the agents around it held before the phase of the step that it runs. So a step gives the same positions on one thread and on many, and a replay moves every agent the same way. Every agent and buffer is made with the crowd, so a step allocates nothing on its own as agents move.

Agents cross links by their kind: a teleport at once, a jump on an arc at the speed of the agent and a ladder at its climb speed. An agent with `linkTraversal = 'manual'` waits at the start of a link until the app calls `crowd:completeLink`, such as after the animation of a climb, and every step lists the agents that reached a link in `crowd:linkEntries()`.

The positions of every agent reach Lua in one call, [`crowd:readPositions`](../lua-api/navigation3d.md#crowdreadpositionsbuffer-first-and-crowdreadvelocitiesbuffer-first), into a float buffer in the order of `crowd.agents`, and `crowd:setTargets` sends every agent at once. So a crowd of thousands costs Lua two calls a frame.

```lua
local collections = require('haylen.collections')
local navigation3d = require('haylen.navigation3d')
local scene = require('haylen.scene')

local square = navigation3d.newGeometry()
square:addHeightfield({0, 0, 0, 0}, 2, 2, {origin = {-60, 0, -60}, spacing = 120})
local mesh = navigation3d.build(square, {agentRadius = 0.4, tileSize = 30})
local crowd = navigation3d.newCrowd(mesh, {maxAgents = 1000, maxAgentRadius = 0.4})
for index = 0, 999 do
    crowd:addAgent({position = {-50 + index % 32 * 1.3, 0, -20 + index // 32 * 1.3}, radius = 0.4})
end

local targets = collections.newFloatBuffer(1000 * 3)
for index = 0, 999 do
    targets:set(index * 3 + 1, {4 + index % 32 * 2, 0, -31 + index // 32 * 2})
end
crowd:setTargets(targets)

local positions = collections.newFloatBuffer(1000 * 3)
scene.push({
    update = function(self, dt)
        crowd:step(dt)
        crowd:readPositions(positions)
    end,
})
```

Agents that all want the same spot crowd around it, and an agent that arrived stands still, so the others go around it. Spread the targets of a group, such as with a formation, so the gaps between the agents that arrived stay wider than an agent.

## Costs

The command `haylen engine bench --suite algorithms` times the parts of 3D navigation on a yard of 200 by 200 meters with 300 boxes of crates and walls, baked with an agent of 0.4 meters and the other settings at their defaults. These are the times of a Release build on an x86-64 cloud machine with 16 threads:

| Work | Time | Grows with | Advice |
| --- | --- | --- | --- |
| The bake of the yard on one thread | 116 ms | The voxels of every tile, so the area of the world over the square of `cellSize`. | Bake levels ahead into `.navmesh` files, and bake in the background with `buildAsync`, which bakes the tiles side by side. |
| A path across the yard | 0.07 ms | The polygons it visits, bounded by `maxNodes`. | Plan once when the goal changes, and walk the path every frame. |
| A crate that moves, with its tiles rebuilt | 0.65 ms | The voxels of the tiles around it, once for each tile at a time. | Keep tiles small for meshes with many obstacles. |
| A step of 2000 agents on one thread | 9.3 ms | The agents times their neighbors. | Lower `collisionRange` for dense crowds, and `avoidance = 'low'` for far agents. |
| A step of 2000 agents on the job system | 3.9 ms | The same work spread over the threads, besides the path requests and the links, which run on the frame thread. | Step the crowd once a frame, with the job system of the engine, as `crowd:step` does. |

## C++

The C++ types live in `haylen/3d/navigation/` in the namespace `haylen::navigation3d`: `SourceGeometry`, `BuildSettings`, `NavMesh` with its queries, obstacles, links and rebuilds, `QueryFilter`, `Obstacle`, `Link` and `Crowd`. The plugin `plugins::Navigation3DPlugin` registers the asset type `navmesh` and starts the rebuilds of the tiles of the meshes that it follows at the start of every frame. A C++ app that keeps a mesh of its own calls `mesh.update(&engine.getJobs())` once a frame instead.

```cpp
#include "haylen/3d/navigation/Crowd.hpp"
#include "haylen/3d/navigation/NavMesh.hpp"

void walk(haylen::core::JobSystem& jobs) {
    haylen::navigation3d::SourceGeometry level;
    level.addHeightfield({.origin = {-20.0F, 0.0F, -20.0F}, .columns = 2, .rows = 2, .spacing = 40.0F, .heights = {0.0F, 0.0F, 0.0F, 0.0F}});
    const haylen::navigation3d::NavMesh mesh = haylen::navigation3d::NavMesh::build(level, {});

    haylen::navigation3d::Crowd crowd(mesh, {.maxAgents = 100});
    const haylen::navigation3d::Crowd::AgentId agent = crowd.addAgent({.position = {-10.0F, 0.0F, 0.0F}});
    crowd.setTarget(agent, {10.0F, 0.0F, 0.0F});
    crowd.step(1.0F / 60.0F, &jobs);
}
```
