# 📖 `haylen.navigation3d`

The module `haylen.navigation3d` bakes navigation meshes of 3D worlds and walks them. It collects the geometry of a world, bakes it into a mesh of polygons in tiles, answers paths, nearest points, rays, walls and random points, carves the mesh around obstacles, crosses gaps through links and moves crowds of agents that keep clear of each other. Every distance is in meters, every speed in meters per second and every angle in radians. The [navigation guide](../3d/navigation.md) explains how the parts fit together and what each one costs.

```lua
local navigation3d = require('haylen.navigation3d')
```

## Functions

### navigation3d.newGeometry()

Returns an empty [`SourceGeometry`](#sourcegeometry), the world that a mesh bakes from.

```lua
local m = require('haylen.math')
local navigation3d = require('haylen.navigation3d')

local level = navigation3d.newGeometry()
level:addMesh({m.vec3(-20, 0, -20), m.vec3(-20, 0, 20), m.vec3(20, 0, 20), m.vec3(20, 0, -20)}, {1, 2, 3, 1, 3, 4})
level:addBox(m.obb(m.vec3(0, 1, 0), nil, m.vec3(2, 1, 2)))
print(level.triangleCount) -- 14
```

### navigation3d.build(geometry, settings)

Bakes the [`SourceGeometry`](#sourcegeometry) `geometry` on the calling thread and returns a [`NavMesh3D`](#navmesh3d). The table `settings` is optional, with the keys that [settings](#settings) lists. A geometry without triangles raises `The geometry of a navigation mesh holds no triangles, so there is nothing to bake.`, and a tile that holds more than it fits raises `The tile <x>, <z> of a navigation mesh holds more <regions, polygons, floors or vertices> than one tile fits. Bake it with a smaller "tileSize" or a larger "cellSize".`. A large world bakes with `buildAsync` instead.

```lua
local navigation3d = require('haylen.navigation3d')

local level = navigation3d.newGeometry()
level:addHeightfield({0, 0, 0, 0.5, 1, 0.5, 0, 0, 0}, 3, 3, {origin = {-10, 0, -10}, spacing = 10})
local mesh = navigation3d.build(level, {agentRadius = 0.4, agentHeight = 1.8})
print(mesh.tileCount > 0) -- true
```

### navigation3d.buildAsync(geometry, settings)

Bakes a copy of the geometry with every tile in a job of its own on the task pool, and returns a promise that resolves with the [`NavMesh3D`](#navmesh3d) or rejects with the error of the bake. The geometry may change while the bake runs. Settings out of their range raise at once.

```lua
local navigation3d = require('haylen.navigation3d')
local scene = require('haylen.scene')

local level = {}

function level:enter()
    local geometry = navigation3d.newGeometry()
    geometry:addHeightfield({0, 0, 0, 0}, 2, 2, {origin = {-50, 0, -50}, spacing = 100})
    scene.spawn(self, function()
        self.mesh = navigation3d.buildAsync(geometry, {tileSize = 24}):await()
        print(self.mesh.tileCount)
    end)
end

scene.push(level)
```

### navigation3d.decode(bytes, name)

Reads the bytes of a `.navmesh` file, a Lua string that [`mesh:encode`](#meshencode) wrote, such as one read from the storage of the app, and returns a [`NavMesh3D`](#navmesh3d). The optional `name` names the file in errors. Bytes that are damaged raise `The navigation mesh "<name>" is damaged or was not made by this version of Haylen.`. A `.navmesh` file of the package loads with `assets.load` of [`haylen.assets`](assets.md), as the asset type `navmesh`.

```lua
local assets = require('haylen.assets')

local mesh = assets.load('levels/harbor.navmesh')
print(mesh.polygonCount)
```

### navigation3d.newCrowd(mesh, options)

Returns a [`Crowd3D`](#crowd3d) of agents that walk the mesh. The table `options` is optional:

| Option | Default | Meaning |
| --- | --- | --- |
| `maxAgents` | `256` | How many agents the crowd holds at once, from 1 to 65535. The crowd allocates every agent when it is made. |
| `maxAgentRadius` | `0.6` | The largest radius of its agents, which sizes the grid that finds neighbors and how far an agent looks for the mesh. |
| `pathBudget` | `100` | How many search steps the paths of all agents take in each step at most, so a crowd that plans long paths never stalls a frame. |

```lua
local navigation3d = require('haylen.navigation3d')

local level = navigation3d.newGeometry()
level:addHeightfield({0, 0, 0, 0}, 2, 2, {origin = {-20, 0, -20}, spacing = 40})
local crowd = navigation3d.newCrowd(navigation3d.build(level), {maxAgents = 500})
print(crowd.count) -- 0
```

## Settings

The table of [`navigation3d.build`](#navigation3dbuildgeometry-settings) and [`navigation3d.buildAsync`](#navigation3dbuildasyncgeometry-settings) takes these keys, and an unknown key raises `Unknown option "<key>".`. A value out of its range raises a sentence that names the setting, such as `The setting "agentMaxSlope" of a navigation mesh must be an angle in radians from 0 up to, but not including, half of pi.`.

| Key | Default | Meaning |
| --- | --- | --- |
| `cellSize` | `0.3` | The side of a voxel on the ground, from 0.01 to 100. Smaller voxels follow the geometry closer and bake slower. |
| `cellHeight` | `0.2` | The height of a voxel, from 0.01 to 100. |
| `agentHeight` | `2` | The height the agent needs above the ground, at least three voxels. |
| `agentRadius` | `0.6` | The radius of the agent. The mesh keeps that far from walls and ledges. |
| `agentMaxClimb` | `0.9` | The highest step the agent climbs. |
| `agentMaxSlope` | `0.785398` | The steepest slope the agent walks, in radians from 0 up to half of pi. |
| `regionMinArea` | `6` | Islands of walkable ground smaller than this many square meters are removed. |
| `regionMergeArea` | `36` | Regions smaller than this many square meters merge into larger neighbors. |
| `edgeMaxLength` | `12` | The longest edge of a polygon along a wall, `0` for no limit. |
| `edgeMaxError` | `0.4` | How far the outline of a polygon may stray from the voxels. |
| `polygonMaxVertices` | `6` | The most corners of a polygon, from 3 to 6. |
| `detailSampleDistance` | `1.8` | The spacing of the samples of the detail mesh, which follows the height of the ground inside a polygon. A spacing below 0.9 voxels follows the polygons only. |
| `detailSampleMaxError` | `0.2` | How far the detail mesh may stray from the voxels in height. |
| `partition` | `'watershed'` | How the ground of a tile splits into regions: `'watershed'`, the most even polygons, `'monotone'`, the fastest bake, or `'layers'`. A mesh with obstacles always splits into layers. |
| `tileSize` | `32` | The side of a tile on the ground. A tile with obstacles holds at most 255 voxels on a side with its border. |
| `filterLowObstacles` | `true` | Walks over obstacles lower than the climb, such as curbs. |
| `filterLedges` | `true` | Removes the ground at ledges higher than the climb. |
| `filterLowCeilings` | `true` | Removes the ground under ceilings lower than the agent. |
| `obstacles` | `false` | Keeps the tiles as layers, so [obstacles](../3d/navigation.md#obstacles) carve the mesh while the app runs. |
| `maxObstacles` | `256` | How many obstacles the mesh holds at once, from 1 to 65536. |
| `bounds` | the geometry | A `Box` of [`haylen.math`](math.md#box), the part of the world to bake. |

## SourceGeometry

A `SourceGeometry` collects the world that a mesh bakes from. Triangles whose corners run counter-clockwise seen from above face up and can be walked on, the front faces of the engine. Each piece names its area in the option `area`: `'ground'` by default, `'none'` for ground that is never walked, or any other name, which takes the next free area as it first appears. A geometry holds up to 63 areas besides `'none'`, and a 64th raises `A navigation mesh holds at most 63 areas besides "none", so the area "<name>" does not fit.`. Where two surfaces meet within a step, the area that appeared later wins. A number that is not finite or lies more than a trillion meters from the origin raises `Every <part> of the geometry of a navigation mesh must be a finite number within a trillion meters of the origin.`.

### geometry:addMesh(positions, indices, options)

Adds the triangles of a mesh. The positions are a [float buffer](collections.md#float-buffers) with three numbers for each vertex or a list of `Vec3` values. The optional list `indices` names three vertices for each triangle, counted from 1, and without it the positions list every triangle in turn. The options are `transform`, a `Mat4` that places the mesh, and `area`. A transform that mirrors turns the triangles over, so they keep facing out. Wrong counts raise the errors of [`spatial3d.newMeshBvh`](spatial3d.md#spatial3dnewmeshbvhpositions-indices).

```lua
local m = require('haylen.math')
local navigation3d = require('haylen.navigation3d')

local level = navigation3d.newGeometry()
local ramp = {m.vec3(0, 0, 0), m.vec3(0, 0, 4), m.vec3(6, 2, 4), m.vec3(6, 2, 0)}
level:addMesh(ramp, {1, 2, 3, 1, 3, 4}, {transform = m.compose({10, 0, 0}, m.quat()), area = 'ramp'})
print(level.areas[2]) -- ramp
```

### geometry:addBox(box, options), geometry:addSphere(sphere, options) and geometry:addCapsule(capsule, options)

Add the surface of an `Obb`, a `Sphere` or a `Capsule` of [`haylen.math`](math.md), the shapes that bodies of physics use, cut into triangles. The option is `area`.

```lua
local m = require('haylen.math')
local navigation3d = require('haylen.navigation3d')

local level = navigation3d.newGeometry()
level:addBox(m.obb(m.vec3(0, 0.5, 0), m.fromAxisAngle(m.up(), 0.3), m.vec3(1, 0.5, 2)))
level:addSphere(m.sphere(m.vec3(5, 1, 0), 1))
level:addCapsule(m.capsule(m.vec3(-5, 0.5, 0), m.vec3(-5, 2, 0), 0.5))
```

### geometry:addCylinder(start, end, radius, options)

Adds a cylinder between the centers of its caps, such as a pillar or a barrel. The option is `area`. A radius that is not a finite number above zero raises `The radius of a cylinder of the geometry of a navigation mesh must be a finite number above zero.`.

```lua
local navigation3d = require('haylen.navigation3d')

local level = navigation3d.newGeometry()
level:addCylinder({0, 0, 0}, {0, 4, 0}, 0.6)
```

### geometry:addHeightfield(heights, columns, rows, options)

Adds terrain as a grid of heights, `columns` along x and `rows` along z, row by row from the smallest z. The heights are a float buffer or a list of numbers. The options are `origin`, the point of the sample of the smallest x and z, whose height adds to every sample, `spacing`, the meters between samples, `1` by default, and `area`. A grid needs at least two columns and two rows and holds at most 67108864 samples, and a count of heights that does not match raises `A heightfield of <columns> columns and <rows> rows needs <count> heights, but it holds <count>.`.

```lua
local collections = require('haylen.collections')
local navigation3d = require('haylen.navigation3d')

local heights = collections.newFloatBuffer(64 * 64)
for index = 1, #heights do
    heights[index] = math.sin(index * 0.05)
end
local level = navigation3d.newGeometry()
level:addHeightfield(heights, 64, 64, {origin = {-32, 0, -32}})
```

### geometry:addVolume(outline, bottom, top, options)

Marks its area on the walkable ground inside a convex outline, a list of 3 to 256 points whose heights are not used, from `bottom` to `top`, such as water, a road or, with the area `'none'`, a hole. The option is `area`.

```lua
local navigation3d = require('haylen.navigation3d')

local level = navigation3d.newGeometry()
level:addHeightfield({0, 0, 0, 0}, 2, 2, {origin = {-10, 0, -10}, spacing = 20})
level:addVolume({{-10, 0, 2}, {-10, 0, 5}, {10, 0, 5}, {10, 0, 2}}, -1, 1, {area = 'water'})
```

### geometry:addLink(start, end, options)

Adds a [link](#links) that the bake places in the tile of its start.

```lua
local navigation3d = require('haylen.navigation3d')

local level = navigation3d.newGeometry()
level:addLink({4, 3, 0}, {6, 0, 0}, {kind = 'jump', bidirectional = false})
```

### geometry.areas, geometry.bounds and geometry.triangleCount

The names of the areas from area 1 on, where `'ground'` comes first, the `Box` around every triangle, or `nil` without triangles, and the count of triangles.

## Links

A link crosses a gap that the mesh does not cover, such as a jump down a ledge, a ladder or a teleporter. Each end joins the mesh within its radius. The options of a link are:

| Option | Default | Meaning |
| --- | --- | --- |
| `radius` | `0.5` | How far from each end the link joins the mesh. |
| `bidirectional` | `true` | Whether agents cross it both ways or from its start to its end only. |
| `kind` | `'jump'` | How a crowd crosses it: `'jump'`, `'ladder'` or `'teleport'`. |
| `area` | `'ground'` | The area whose cost a filter gives the crossing. |

## NavMesh3D

A `NavMesh3D` is a baked navigation mesh. Its queries run on the frame thread, and every query takes an optional table of these options, whose unknown keys raise `Unknown option "<key>".`:

| Option | Default | Meaning |
| --- | --- | --- |
| `filter` | every area at cost 1 | A [`QueryFilter`](#queryfilter) made by this mesh. |
| `searchExtents` | `{2, 4, 2}` | Half the size of the box around a point in which the point snaps to the mesh. A point farther than that from every polygon is off the mesh. |
| `maxNodes` | `4096` | How many polygons a search visits at most, from 1 to 65534. |
| `maxPolygons` | `1024` | How many polygons a path keeps at most. |
| `maxPoints` | `256` | How many points a path keeps at most. |

A search that runs out of nodes, polygons or points answers with the partial path it found.

### mesh:findPath(start, goal, options)

Returns the straight path from the start to the goal along the corridor of polygons, as `{points, links, partial, length}`, or `nil` when the start or the goal is off the mesh. The list `points` holds `Vec3` values from the start, where the path turns at corners, to the end. The item of `links` at a point is the id of the link that starts there, or `0`. A path with `partial` set ends at the point closest to the goal that the start reaches, because the goal is cut off, such as behind a closed door, or the search ran out. The search also casts rays across the polygons it reaches and takes the straight way where one is clear and costs less, so paths across wide polygons stay straight.

```lua
local assets = require('haylen.assets')

local mesh = assets.load('levels/harbor.navmesh')
local path = mesh:findPath({-40, 0, 12}, {35, 4, -8})
if path and not path.partial then
    print(#path.points .. ' points over ' .. path.length .. ' meters')
end
```

### mesh:findNearest(point, options)

Returns the point of the mesh closest to `point` within the search extents as `{position, polygon, area}`, with the name of its area, or `nil`. A polygon is a whole number that stays valid until its tile rebuilds.

```lua
local assets = require('haylen.assets')

local mesh = assets.load('levels/harbor.navmesh')
local spot = mesh:findNearest({3, 10, 4}, {searchExtents = {1, 20, 1}})
print(spot and spot.area)
```

### mesh:raycast(start, end, options)

Casts a ray along the surface from the start, which snaps to the mesh, toward the end seen from above, and returns `{hit, fraction, position, normal}`, or `nil` when the start is off the mesh. A ray that reaches its end has `hit` set to `false` and `fraction` 1. A ray that meets a wall gives the fraction of the way it went, the point on the ground and the `normal` of the wall. Use it to tell whether an agent sees its goal across the mesh.

```lua
local assets = require('haylen.assets')

local mesh = assets.load('levels/harbor.navmesh')
local ray = mesh:raycast({0, 0, 0}, {20, 0, 0})
print(ray and ray.hit)
```

### mesh:moveAlongSurface(start, end, options)

Moves from the start toward the end along the surface, sliding along the walls it meets, and returns where it stopped as `{position, polygon, area}`, or `nil` when the start is off the mesh. It suits a character that the player steers and that must stay on the mesh.

```lua
local assets = require('haylen.assets')

local mesh = assets.load('levels/harbor.navmesh')
local moved = mesh:moveAlongSurface({0, 0, 0}, {0.5, 0, 0.2})
print(moved and moved.position)
```

### mesh:findDistanceToWall(point, radius, options)

Returns the closest wall within `radius` of the point as `{distance, position, normal}`, where `normal` points from the wall toward the point, or `nil` when no wall is that close.

```lua
local assets = require('haylen.assets')

local mesh = assets.load('levels/harbor.navmesh')
local wall = mesh:findDistanceToWall({2, 0, 2}, 3)
print(wall and wall.distance)
```

### mesh:findRandomPoint(options) and mesh:findRandomPointAround(center, radius, options)

Return a random point of the mesh as `{position, polygon, area}`, where every square meter of ground, seen from above, is as likely as any other, or `nil` for a mesh without polygons that the filter passes. The second one picks within `radius` of the center, among the polygons that the center reaches, so the point never lies across a wall. The options also take the generator: a `Random` of [`haylen.math`](math.md#random) in `random`, which advances, or a `seed`, `0` by default.

```lua
local assets = require('haylen.assets')
local m = require('haylen.math')

local mesh = assets.load('levels/harbor.navmesh')
local random = m.random(7)
for index = 1, 10 do
    local spawn = mesh:findRandomPoint({random = random})
    print(spawn.position)
end
local nearby = mesh:findRandomPointAround({0, 0, 0}, 5, {seed = 3})
print(nearby and nearby.position)
```

### mesh:newFilter(options)

Returns a [`QueryFilter`](#queryfilter) for this mesh. The option `costs` maps the names of areas to costs of at least 1, and the option `excluded` lists the areas a query never crosses. A name that the mesh does not hold raises `The navigation mesh has no area "<name>". Name it in the geometry that the mesh bakes from.`, and a cost below 1 raises `The cost of an area of a query filter must be a finite number of at least 1.`.

```lua
local assets = require('haylen.assets')

local mesh = assets.load('levels/harbor.navmesh')
local wader = mesh:newFilter({costs = {water = 4}, excluded = {'door'}})
local path = mesh:findPath({-40, 0, 12}, {35, 0, -8}, {filter = wader})
print(path and path.length)
```

### mesh:area(name)

Returns the number of an area, from 1 to 63, or 0 for `'none'`.

### mesh:addObstacle(obstacle), mesh:setObstacle(id, obstacle) and mesh:removeObstacle(id)

Add, move and remove an obstacle that carves the mesh while it stands, on a mesh baked with `obstacles`. An obstacle is a table: `{shape = 'cylinder', position, radius, height}` stands on its position, and `{shape = 'box', position, halfExtents, yaw}` stands around it, turned by `yaw` radians around the up axis. The mesh keeps every agent its radius away from an obstacle. `addObstacle` returns the id of the obstacle, and `removeObstacle` returns whether the mesh held it. The tiles of an obstacle rebuild on the task pool, and take their place at the start of a later frame. A mesh without obstacles raises `This navigation mesh was baked without obstacles. Bake it with the setting "obstacles" to add them.`, and one with all of its obstacles raises `The navigation mesh holds at most <count> obstacles, the "maxObstacles" it was baked with.`.

```lua
local assets = require('haylen.assets')

local mesh = assets.load('levels/yard.navmesh')
local truck = mesh:addObstacle({shape = 'box', position = {0, 1, 0}, halfExtents = {1.5, 1, 4}, yaw = 0.4})
mesh:setObstacle(truck, {shape = 'box', position = {6, 1, 0}, halfExtents = {1.5, 1, 4}, yaw = 0.4})
mesh:removeObstacle(truck)
```

### mesh:obstacle(id)

Returns the table of an obstacle, or `nil` for an id that the mesh does not hold.

### mesh:addLink(start, end, options), mesh:removeLink(id) and mesh:link(id)

Add a [link](#links) and return its id, remove one, and return the table of one as `{start, end, radius, bidirectional, kind, area}`, or `nil`. The links of the geometry of the bake keep the ids from 1 on, in their order. A change rebuilds the tile of the start of the link.

```lua
local assets = require('haylen.assets')

local mesh = assets.load('levels/harbor.navmesh')
local ladder = mesh:addLink({10, 0, 4}, {10, 6, 5}, {kind = 'ladder'})
print(mesh:link(ladder).kind) -- ladder
```

### mesh:rebuildAsync(geometry, box)

Bakes the tiles that touch the `Box` again from a copy of the geometry, such as after a wall fell, and returns a promise that resolves with `true` once every tile is in place. The links of the geometry stay out, since links belong to the mesh. A box outside the bounds the mesh was baked for raises `The area of a rebuild must touch the bounds that the navigation mesh was baked for.`.

```lua
local assets = require('haylen.assets')
local m = require('haylen.math')
local navigation3d = require('haylen.navigation3d')
local scene = require('haylen.scene')

local level = {}

function level:enter()
    self.mesh = assets.load('levels/harbor.navmesh')
    self.geometry = navigation3d.newGeometry()
    self.geometry:addHeightfield({0, 0, 0, 0}, 2, 2, {origin = {-50, 0, -50}, spacing = 100})
end

function level:wallFell()
    scene.spawn(self, function()
        self.mesh:rebuildAsync(self.geometry, m.box({-2, -1, -10}, {2, 4, 10})):await()
    end)
end

scene.push(level)
```

### mesh:updatedAsync()

Returns a promise that resolves with `true` once no tile waits for its rebuild, or rejects with the error of a rebuild that failed.

### mesh:encode()

Returns the bytes of the mesh as a Lua string in the `.navmesh` format, which [`navigation3d.decode`](#navigation3ddecodebytes-name) reads back. Obstacles stay out, since they belong to the running app.

### mesh:triangles()

Returns a float buffer with nine numbers for each triangle of the surface, its three corners, such as to draw the mesh.

### mesh.tileCount, mesh.polygonCount, mesh.obstacleCount and mesh.linkCount

The counts of the tiles in place, of the polygons of the ground, of the obstacles and of the links.

### mesh.areas, mesh.bounds and mesh.settings

The names of the areas from area 1 on, the `Box` the mesh was baked over, and the table of its [settings](#settings).

### mesh.upToDate and mesh.pendingTiles

Whether no tile waits for its rebuild, and how many columns of tiles wait.

## QueryFilter

A `QueryFilter` holds the cost of each area of one mesh and the areas its queries never cross, which [`mesh:newFilter`](#meshnewfilteroptions) makes. A path through an area costs its length times the cost of the area.

### filter:cost(area) and filter:excluded(area)

Return the cost of an area by its name and whether the filter skips it. A name that the mesh does not hold raises `The navigation mesh of the filter has no area "<name>".`.

```lua
local assets = require('haylen.assets')

local mesh = assets.load('levels/harbor.navmesh')
local filter = mesh:newFilter({costs = {water = 4}})
print(filter:cost('water'), filter:excluded('water')) -- 4.0 false
```

## Crowd3D

A `Crowd3D` moves many agents together on a mesh. Each agent follows a corridor of polygons toward its target, steers around the corners ahead, keeps clear of walls and of the other agents on the plane of the mesh, and crosses the links on its way by their kind. The work of each agent spreads over the job system, and a step gives the same result whatever the number of threads. Agents are named by whole numbers that are never reused while the crowd lives.

### crowd:addAgent(options)

Adds an agent and returns its id. The agent starts at the point of the mesh closest to `position`, or in the state `'invalid'` when the mesh is not near. A full crowd raises `The crowd is full with its <count> agents, the "maxAgents" it was made with.`, and an unknown key raises `Unknown option "<key>".`.

| Option | Default | Meaning |
| --- | --- | --- |
| `position` | `{0, 0, 0}` | Where the agent starts. |
| `radius` | `0.6` | Its radius, at most the `maxAgentRadius` of the crowd. |
| `height` | `2` | Its height. |
| `maxSpeed` | `3.5` | Its top speed. |
| `maxAcceleration` | `8` | How fast its velocity changes, in meters per second squared. |
| `collisionRange` | 12 times the radius | How far it looks for walls and agents. |
| `pathOptimizationRange` | 30 times the radius | How far ahead it shortens its corridor when it sees past a corner. |
| `separation` | `false` | Whether it also pushes away from close neighbors, with the weight `separationWeight`, `2` by default. |
| `avoidance` | `'high'` | How carefully it picks a velocity that avoids others: `'none'`, `'low'`, `'medium'` or `'high'`. |
| `anticipateTurns` | `true` | Whether it rounds the next corner instead of walking to it. |
| `optimizeVisibility` | `true` | Whether it shortens its corridor to corners it sees. |
| `optimizeTopology` | `true` | Whether it looks for a shorter corridor now and then. |
| `filter` | every area at cost 1 | A [`QueryFilter`](#queryfilter) of the mesh. |
| `linkTraversal` | `'automatic'` | How it crosses a link: `'automatic'` by the kind of the link, a teleport at once, a jump on an arc of `jumpHeight` at its speed and a ladder at `climbSpeed`, or `'manual'`, which waits at the start of the link until [`crowd:completeLink`](#crowdcompletelinkid). |
| `jumpHeight` | `1` | The height of the arc of a jump above the line between its ends. |
| `climbSpeed` | `1.5` | The speed on a ladder. |

```lua
local assets = require('haylen.assets')
local navigation3d = require('haylen.navigation3d')

local mesh = assets.load('levels/harbor.navmesh')
local crowd = navigation3d.newCrowd(mesh, {maxAgents = 64, maxAgentRadius = 0.5})
local guard = crowd:addAgent({position = {0, 0, 0}, radius = 0.5, maxSpeed = 2.5, linkTraversal = 'manual'})
crowd:setTarget(guard, {20, 0, 10})
```

### crowd:removeAgent(id) and crowd:hasAgent(id)

Remove an agent, returning whether the crowd held it, and tell whether it holds one. Every other method raises `The crowd has no agent <id>.` for an id that it does not hold.

### crowd:setTarget(id, point), crowd:setVelocity(id, velocity) and crowd:clearTarget(id)

Send an agent to a point of the mesh near `point`, returning `false` when no polygon is near it, make it walk at a velocity, such as from a flow of its own, or stop it.

### crowd:setTargets(buffer, first)

Sends every agent at once, with three numbers for each agent in the order of [`crowd.agents`](#crowdagents-and-crowdcount), from the value `first` of the float buffer on, `1` by default.

### crowd:setPosition(id, point)

Moves an agent to the point of the mesh near `point` at once, such as after a cut scene, and plans its target again from there. It returns `false` when no polygon is near the point.

### crowd:completeLink(id)

Lets an agent that waits at the start of a link with `'manual'` traversal go on from its end, such as after the animation of a climb, and returns whether it waited.

### crowd:step(dt)

Moves every agent by `dt` seconds over the job system of the engine. Call it once a frame, in the update of the scene.

```lua
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local navigation3d = require('haylen.navigation3d')
local scene = require('haylen.scene')

local mesh = assets.load('levels/harbor.navmesh')
local crowd = navigation3d.newCrowd(mesh, {maxAgents = 1000})
for index = 1, 1000 do
    local id = crowd:addAgent({position = {index % 40 - 20, 0, index // 40 - 12}})
    crowd:setTarget(id, {30, 0, 0})
end
local positions = collections.newFloatBuffer(1000 * 3)

scene.push({
    update = function(self, dt)
        crowd:step(dt)
        crowd:readPositions(positions)
    end,
})
```

### crowd:position(id), crowd:velocity(id) and crowd:target(id)

Return the position and the velocity of an agent as `Vec3` values, and its target, or `nil` when it has none or walks at a velocity.

### crowd:state(id) and crowd:partial(id)

Return the state of an agent, `'walking'`, `'onLink'` or `'invalid'` when the mesh is not under it, and whether its target is cut off from it, so it walks to the closest point it reaches.

### crowd:readPositions(buffer, first) and crowd:readVelocities(buffer, first)

Write three numbers for each agent in the order of [`crowd.agents`](#crowdagents-and-crowdcount) into the float buffer, from the value `first` on, `1` by default, so the positions of every agent reach Lua in one call. A buffer too short raises `The positions of <count> agents need <numbers> numbers, but there is room for <size>.`, and a `first` below 1 raises an argument error that says `the values of a buffer are numbered from one`.

### crowd:linkEntries()

Lists the agents that reached a link in the last step as `{agent, link, kind, start, end}`, such as to play the animation of a climb.

```lua
local assets = require('haylen.assets')
local navigation3d = require('haylen.navigation3d')

local mesh = assets.load('levels/harbor.navmesh')
local crowd = navigation3d.newCrowd(mesh)
crowd:step(1 / 60)
for _, entry in ipairs(crowd:linkEntries()) do
    print(entry.agent .. ' takes the ' .. entry.kind .. ' ' .. entry.link)
end
```

### crowd.agents and crowd.count

The ids of the agents in the order that the bulk reads and writes use, which changes only when an agent is added or removed, and how many agents the crowd holds.

## Errors

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | An option table holds a key that its function does not take. |
| `The setting "<setting>" of a navigation mesh must be <range>.` | A setting of a bake is out of its range. |
| `The geometry of a navigation mesh holds no triangles, so there is nothing to bake.` | A bake got an empty geometry. |
| `The tile <x>, <z> of a navigation mesh holds more <part> than one tile fits. Bake it with a smaller "tileSize" or a larger "cellSize".` | A tile holds more regions, polygons, floors or vertices than one tile of the mesh keeps. |
| `The tile <x>, <z> of a navigation mesh spans more height than 8191 voxels of its "cellHeight" hold. Bake it with a larger "cellHeight".` | The geometry of a tile is taller than its voxels count. |
| `The tile <x>, <z> of a navigation mesh did not assemble, since its voxels split into more regions or corners than one tile holds or the system ran out of memory. Bake the mesh again with a smaller "tileSize".` | A tile of a mesh with obstacles or of a `.navmesh` file holds more than one tile of Detour keeps. |
| `A navigation mesh holds at most 65536 tiles on the ground, so bake a smaller world or a larger "tileSize".` | The bounds of a bake need too many tiles. |
| `A navigation mesh holds at most 63 areas besides "none", so the area "<name>" does not fit.` | A geometry names a 64th area. |
| `Every <part> of the geometry of a navigation mesh must be a finite number within a trillion meters of the origin.` | A point of the geometry is not finite or lies too far away. |
| `The radius of a link of a navigation mesh must be a finite number above zero.` | A link of a geometry or of a mesh has a radius of zero or less. |
| `The navigation mesh has no area "<name>". Name it in the geometry that the mesh bakes from.` | A filter or a link names an area that the mesh does not hold. |
| `The navigation mesh "<name>" is damaged or was not made by this version of Haylen.` | The bytes of a `.navmesh` file break a rule of the format. |
| `This navigation mesh was baked without obstacles. Bake it with the setting "obstacles" to add them.` | An obstacle went to a mesh baked without `obstacles`. |
| `The navigation mesh holds at most <count> obstacles, the "maxObstacles" it was baked with.` | A mesh holds all of its obstacles. |
| `The navigation mesh has no obstacle <id>.` | `mesh:setObstacle` got an id that the mesh does not hold. |
| `The area of a rebuild must touch the bounds that the navigation mesh was baked for.` | The box of `mesh:rebuildAsync` lies outside the mesh. |
| `The tile <x>, <z> of a navigation mesh has more floors than the mesh keeps room for. Bake the mesh again with the geometry that adds them.` | A rebuild stacked more floors in a tile than the bake left room for. |
| `A query of a navigation mesh visits from 1 to 65534 polygons, not <count>.` | The `maxNodes` of a query is out of range. |
| `The cost of an area of a query filter must be a finite number of at least 1.` | A cost of `mesh:newFilter` is below 1 or not finite. |
| `The crowd is full with its <count> agents, the "maxAgents" it was made with.` | `crowd:addAgent` found no free slot. |
| `The crowd has no agent <id>.` | A method of a crowd got an id that it does not hold. |
| `The positions of <count> agents need <numbers> numbers, but there is room for <size>.` | The buffer of `crowd:readPositions` is too short, and the same sentence names the velocities of `crowd:readVelocities`. |
| `The targets of <count> agents need <numbers> numbers, but there are <size>.` | The buffer of `crowd:setTargets` is too short. |
