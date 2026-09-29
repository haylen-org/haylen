# haylen.navigation2d

`haylen.navigation2d` moves characters through the world. Navigation grids plan routes over square, isometric, staggered and hexagonal tile maps with A*, weighted A*, jump point search and hierarchical path finding, and build Dijkstra maps and flow fields that guide any number of units at once. Waypoint graphs find routes between hand-placed points, and navigation meshes find smooth routes through open levels for agents of any size. Steering agents move one character at a time with seek, flee, arrive, flocking, obstacle avoidance and wander, and crowds move thousands of agents together without collisions using optimal reciprocal collision avoidance. Long searches can run in the background and return promises.

```lua
local navigation2d = require('haylen.navigation2d')
```

## Cells

Grid cells are addressed by column and row, counting from 0 like Tiled cells, so `map:worldToCell` from [haylen.tiled](tiled.md) gives cells that the grid accepts. Hexagonal and staggered grids use the same offset coordinates as Tiled. Paths are lists of cell tables `{x = column, y = row}`. Functions that take cells in a list also accept cells written as `{column, row}`.

## Background work

Functions whose name ends in `Async` copy what they need, run on a worker thread and return a promise from Varn's `async` module. Call `:await()` on it inside a coroutine started with `async.spawn()`. It returns the result on success, and `nil` and the error message on failure. Invalid arguments still raise at once, when the function is called.

## Choosing a method

| Method | Best for |
| --- | --- |
| `grid:findPath` | One unit at a time on a tile map, with costs for terrain. |
| `grid:findPath` with `jumpPoint` | Long paths on large open grids where every cell costs the same. |
| `grid:hierarchical` | Many long searches on large grids that change a little at a time. |
| `grid:flowField` | Many units heading to the same goals, as in strategy and tower defense games. |
| `grid:dijkstraMap` | Monsters that chase or flee the player, mixing several goals of different appeal. |
| `newGraph` | Hand-placed waypoints, roads, doors that open and close. |
| `newNavMesh` | Open levels without a grid, with paths that hug corners and keep a radius clear. |
| `newAgent` | Smooth individual motion toward a target. |
| `newCrowd` | Hundreds or thousands of agents that must not overlap. |

## Functions

### navigation2d.newGrid(width, height, layout)

Creates a `NavGrid` of `width` columns and `height` rows. Every cell starts walkable with cost 1, and cells outside the grid count as blocked. A size that is not positive or does not fit 32-bit cell indices raises `A navigation grid needs a positive size that fits in 32-bit cell indices.` `layout` is optional and follows the map properties of Tiled:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `topology` | string | `'square'` | `'square'` for orthogonal, isometric and oblique maps, `'hexagonal'` for hexagonal maps and `'staggered'` for staggered isometric maps. |
| `staggerX` | boolean | `false` | The columns shift instead of the rows, like a stagger axis of `x` in Tiled. |
| `staggerEven` | boolean | `false` | The even rows or columns shift instead of the odd ones, like a stagger index of `even` in Tiled. |

Square and staggered cells step through their four sides and, with diagonal steps, through their four corners. Hexagonal cells step through their six sides, and every step costs the cost of the cell entered.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(40, 30)
local hexes = navigation2d.newGrid(20, 20, {topology = 'hexagonal', staggerX = true})
print(grid.width, grid.height, hexes.topology, hexes.staggerX)
```

### navigation2d.newGraph()

Creates an empty `NavGraph`.

### navigation2d.newNavMesh(boundary)

Creates a `NavMesh`. `boundary` is optional and sets the outline of the walkable area, a list of at least three points, which `mesh:setBoundary` can also set later.

### navigation2d.buildNavMeshAsync(boundary, obstacles)

Builds a `NavMesh` from the outline `boundary` and the optional list of obstacle polygons on a worker thread, and returns a promise that resolves with the built mesh. Use it for large levels so loading never stalls a frame. Invalid polygons raise the error at once, and a failed build rejects the promise.

```lua
local navigation2d = require('haylen.navigation2d')
local async = require('async')

async.spawn(function()
    local boundary = {{0, 0}, {4096, 0}, {4096, 4096}, {0, 4096}}
    local rocks = {{{1000, 1000}, {1200, 1000}, {1200, 1200}, {1000, 1200}}}
    local mesh = navigation2d.buildNavMeshAsync(boundary, rocks):await()
    print(mesh.triangleCount, mesh.dirty)
end)
```

### navigation2d.newAgent(options)

Creates a `SteeringAgent`. `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Starting position. |
| `maxSpeed` | number | `120` | Speed limit in units per second. |
| `maxForce` | number | `600` | Largest velocity change per second. Lower values turn and brake more slowly. |
| `wanderDistance` | number | `60` | Distance ahead of the agent of the circle that `wander` aims at. |
| `wanderRadius` | number | `30` | Radius of that circle. |
| `wanderJitter` | number | `4` | How fast the wander target drifts around the circle, in radians per second. |
| `seed` | integer | `1` | Seed of the wander randomness. Agents with the same seed and state wander the same way. |

```lua
local navigation2d = require('haylen.navigation2d')

local bat = navigation2d.newAgent({x = 100, y = 100, maxSpeed = 90, maxForce = 300, wanderJitter = 6, seed = 42})
```

### navigation2d.newCrowd(flocking)

Creates a `Crowd`. `flocking` is optional and sets the weights of the flocking forces added to the velocity every agent prefers, computed from the neighbors it avoids:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `separation` | number | `0` | Pushes agents apart before they get close. |
| `alignment` | number | `0` | Turns agents toward the heading of their neighbors. |
| `cohesion` | number | `0` | Pulls agents toward the center of their neighbors. |

```lua
local navigation2d = require('haylen.navigation2d')

local sheep = navigation2d.newCrowd({separation = 0.5, alignment = 0.3, cohesion = 0.2})
print(sheep.agentCount)
```

## NavGrid

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | Number of columns. |
| `height` | integer | read | Number of rows. |
| `topology` | string | read | `'square'`, `'hexagonal'` or `'staggered'`. |
| `staggerX` | boolean | read | Whether the columns shift instead of the rows. |
| `staggerEven` | boolean | read | Whether the even rows or columns shift. |
| `uniformCost` | boolean | read | True when every cell costs 1, which jump point search needs. |

`setWalkable`, `setCost` and `cost` raise an error such as `Cell 9,9 is outside the navigation grid.` for cells outside the grid.

### grid:contains(x, y)

Returns true when the cell is inside the grid.

### grid:setWalkable(x, y, walkable)

Marks the cell as walkable or blocked.

### grid:walkable(x, y)

Returns true when the cell is inside the grid and walkable.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(10, 10)
for row = 0, 7 do
    grid:setWalkable(5, row, false)
end
print(grid:contains(9, 9), grid:contains(10, 0), grid:walkable(5, 3), grid:walkable(5, 8))
```

### grid:setCost(x, y, cost)

Sets the cost of entering the cell, where 1 is plain ground and larger values make paths avoid the cell. Costs below 1 or not finite raise `A navigation cost must be finite and at least 1.`

### grid:cost(x, y)

Returns the cost of entering the cell.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(20, 12)
for column = 4, 15 do
    grid:setCost(column, 6, 5)
end
print(grid:cost(8, 6), grid:cost(8, 5), grid.uniformCost)
```

### grid:findPath(startX, startY, goalX, goalY, options)

Finds the cheapest path from the start cell to the goal cell and returns it as a list of cells that includes both ends, followed by its cost, or `nil` when either end is blocked or the goal cannot be reached. Diagonal steps never cut the corner of a blocked cell. Equal paths always resolve the same way, so the same grid gives the same path on every device. The grid keeps its search buffers, so repeated searches allocate nothing once they have grown. `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `diagonal` | boolean | `true` | Allows diagonal steps, which cost the square root of 2 times the cell cost. Hexagonal grids ignore it. |
| `smooth` | boolean | `false` | Returns the path after `grid:smooth`, keeping only the cells where it turns. The cost stays the cost of the unsmoothed path. |
| `heuristic` | string | `'octile'` or `'manhattan'` | How the search estimates the distance left: `'manhattan'`, `'octile'`, `'euclidean'` or `'chebyshev'`. The default is `'octile'` with diagonal steps and `'manhattan'` without them, which are exact on open ground. Hexagonal grids always count hex steps and reject this option with `Hexagonal grids count hex steps and take no heuristic.` |
| `weight` | number | `1` | Weighted A*. Values above 1 explore fewer cells and return paths that cost at most `weight` times the cheapest one. Values below 1 raise `A search weight must be finite and at least 1.` |
| `jumpPoint` | boolean | `false` | Jump point search, which skips over runs of open ground and returns paths as cheap as A* while expanding far fewer cells, most of all on large open maps. It needs a square or staggered grid where every cell costs 1, and raises `Jump point search needs a square or staggered grid.` or `Jump point search needs a grid where every cell costs 1.` otherwise. |

Unknown keys raise `Unknown option 'name'.`

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(9, 5)
for row = 0, 3 do
    grid:setWalkable(4, row, false)
end

local path, cost = grid:findPath(0, 0, 8, 0)
for _, cell in ipairs(path) do
    print(cell.x, cell.y)
end
print('cost', cost)

local corners = grid:findPath(0, 0, 8, 0, {smooth = true})
local manhattan = grid:findPath(0, 0, 3, 3, {diagonal = false})
local jumped = grid:findPath(0, 0, 8, 0, {jumpPoint = true})
local rough = grid:findPath(0, 0, 8, 0, {weight = 2, heuristic = 'euclidean'})
print(#corners, #manhattan, #jumped, #rough, grid:findPath(0, 0, 20, 20))
```

### grid:findPathAsync(startX, startY, goalX, goalY, options)

Searches like `grid:findPath` on a copy of the grid on a worker thread, and returns a promise that resolves with the path, or with `nil` when there is none. Changes made to the grid after the call do not affect the search. Invalid options raise the error at once.

```lua
local navigation2d = require('haylen.navigation2d')
local async = require('async')

local grid = navigation2d.newGrid(512, 512)
async.spawn(function()
    local path = grid:findPathAsync(0, 0, 511, 511, {jumpPoint = true}):await()
    print(#path)
end)
```

### grid:lineOfSight(fromX, fromY, toX, toY)

Returns true when the straight segment between the centers of both cells crosses only walkable cells. A segment that passes exactly through a corner needs both cells beside the corner to be walkable. Hexagonal grids check the cells along the line between both hexagon centers.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(8, 8)
grid:setWalkable(3, 1, false)
print(grid:lineOfSight(0, 1, 7, 1), grid:lineOfSight(0, 0, 7, 0))
```

### grid:smooth(path)

Returns a shorter copy of `path` that keeps the start, the goal and every cell where a straight walk would leave walkable ground. Smoothing looks at walkability only, so the result may cross costly cells.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(10, 10)
local path = grid:findPath(0, 0, 9, 3, {diagonal = false})
local waypoints = grid:smooth(path)
print(#path, #waypoints)
print(#grid:smooth({{0, 4}, {1, 4}, {2, 4}}))
```

### grid:raycast(from, to, cellSize)

Casts a ray in world units from the point `from` to the point `to` over a square grid whose cells are `cellSize` units wide, given as a number or a `{width, height}` pair, with the first cell at the origin. Returns the hit on the first blocked cell, a hit table like those of [haylen.math](math.md#ray-casts) with the extra fields `column` and `row`, or `nil`. Cells outside the grid block the ray like any blocked cell. Other topologies raise `Grid ray casts need a square grid.`

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(20, 12)
grid:setWalkable(10, 2, false)
local hit = grid:raycast({8, 40}, {600, 40}, 32)
print(hit.column, hit.row, hit.x, hit.normalX)
```

### grid:dijkstraMap(sources, options)

Computes a `DijkstraMap`, the distance map of roguelikes, which stores for every cell the cost of the cheapest walk to the nearest source plus the value of that source. `sources` is a list of cells, each with an optional `value` field that defaults to 0, where lower values attract more, so a treasure can pull harder than a lever. `options` is optional and takes `diagonal`, true by default. Blocked and unreachable cells get `math.huge`. The map keeps its grid alive and reads its current walkability when stepping.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(30, 20)
local player = {x = 5, y = 5}
local gold = {x = 25, y = 15, value = -4}
local chase = grid:dijkstraMap({player, gold})
print(chase:value(5, 5), chase:value(25, 15), chase:value(6, 5))
```

### grid:dijkstraMapAsync(sources, options)

Computes a Dijkstra map like `grid:dijkstraMap` on a copy of the grid on a worker thread, and returns a promise that resolves with the map.

### grid:flowField(goals, options)

Computes a `FlowField` that stores for every cell the step toward the nearest goal, so any number of units can move by reading the step of their cell. `goals` is a list of cells, and `options` is optional and takes `diagonal`, true by default. The field is a snapshot of the grid when it was computed.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(40, 30)
local field = grid:flowField({{39, 15}})
print(field:next(0, 15))
print(field:direction(0, 0), field:distance(0, 15))
```

### grid:flowFieldAsync(goals, options)

Computes a flow field like `grid:flowField` on a copy of the grid on a worker thread, and returns a promise that resolves with the field.

```lua
local navigation2d = require('haylen.navigation2d')
local async = require('async')

local grid = navigation2d.newGrid(256, 256)
async.spawn(function()
    local field = grid:flowFieldAsync({{128, 128}}):await()
    local chase = grid:dijkstraMapAsync({{128, 128}}):await()
    print(field:distance(0, 0), chase:value(0, 0))
end)
```

### grid:hierarchical(options)

Creates a `HierarchicalPath` over a square grid, the HPA* of Botea, Müller and Schaeffer. The grid splits into square clusters joined by entrances, a small graph of entrances is searched first and then only the cells along the chosen route, which makes long searches on large grids much faster than A*. Paths are near optimal, usually within a few percent of the cheapest one. It keeps its grid alive and reads it on every search, so after cells change, `path:update` must rebuild the clusters around them. Other topologies raise `Hierarchical path finding needs a square grid.` `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `clusterSize` | integer | `16` | Width and height of a cluster in cells, at least 2. Larger clusters make fewer entrances and slower local searches. |
| `diagonal` | boolean | `true` | Allows diagonal steps. |

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(256, 256)
for row = 0, 200 do
    grid:setWalkable(128, row, false)
end
local router = grid:hierarchical({clusterSize = 32})
local path, cost = router:findPath(0, 0, 255, 0)
print(#path, cost, router.nodeCount)
```

### grid:hierarchicalAsync(options)

Builds a `HierarchicalPath` like `grid:hierarchical` from a copy of the grid on a worker thread, so building one over a large map never stalls a frame, and returns a promise that resolves with it. The path finder reads the grid itself once it arrives, so cells changed after the call and before the promise resolved need `path:update`. Invalid options raise the error at once, and a grid that is not square rejects the promise with `Hierarchical path finding needs a square grid.`

```lua
local navigation2d = require('haylen.navigation2d')
local async = require('async')

local grid = navigation2d.newGrid(1024, 1024)
async.spawn(function()
    local router = grid:hierarchicalAsync({clusterSize = 32}):await()
    print(#router:findPath(0, 0, 1023, 1023), router.nodeCount)
end)
```

## DijkstraMap

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | Number of columns. |
| `height` | integer | read | Number of rows. |

Cells outside the map raise `The cell is outside the Dijkstra map.`

### map:value(x, y)

Returns the value of the cell, `math.huge` for blocked and unreachable cells.

### map:next(x, y)

Returns the column and row of the neighbor that leads downhill along a cheapest path, or `nil` at a lowest point, at blocked cells and at unreachable ones.

### map:flee(coefficient)

Turns the map into a flee map. Every value is multiplied by `coefficient`, -1.2 by default, and the walk runs again, so the cells farthest from the sources become the lowest while dead ends stay higher. Walking downhill then runs away along open escape routes instead of into corners. A coefficient that is not negative raises `A flee map needs a finite negative coefficient.`

```lua
local navigation2d = require('haylen.navigation2d')
local scene = require('haylen.scene')

local grid = navigation2d.newGrid(30, 20)
local monster = {x = 12, y = 10}
local player = {x = 10, y = 10}

scene.push({
    update = function(self, dt)
        local fear = grid:dijkstraMap({player})
        fear:flee()
        local x, y = fear:next(monster.x, monster.y)
        if x then
            monster.x, monster.y = x, y
        end
    end,
})
```

## FlowField

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | Number of columns. |
| `height` | integer | read | Number of rows. |

Cells outside the field raise `The cell is outside the flow field.`

### field:next(x, y)

Returns the column and row of the cell to step into, or `nil` at a goal, at blocked cells and at unreachable ones.

### field:direction(x, y)

Returns the unit `Vec2` from the cell toward its next cell in grid coordinates, which is the world direction on square grids, or a zero vector where there is no next cell.

### field:distance(x, y)

Returns the cost of the cheapest walk from the cell to a goal, `math.huge` when no goal can be reached.

```lua
local navigation2d = require('haylen.navigation2d')
local scene = require('haylen.scene')

local tileSize = 32
local grid = navigation2d.newGrid(40, 30)
local field = grid:flowField({{20, 15}})
local units = {}
for index = 1, 500 do
    units[index] = {x = (index % 40) * tileSize + 16, y = (index % 30) * tileSize + 16}
end

scene.push({
    update = function(self, dt)
        for _, unit in ipairs(units) do
            local direction = field:direction(unit.x // tileSize, unit.y // tileSize)
            unit.x = unit.x + direction.x * 80 * dt
            unit.y = unit.y + direction.y * 80 * dt
        end
    end,
})
```

## HierarchicalPath

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `clusterSize` | integer | read | Width and height of a cluster in cells. |
| `nodeCount` | integer | read | Number of entrance cells in the graph of entrances. |

### path:findPath(startX, startY, goalX, goalY)

Returns the cells from the start to the goal, both included, followed by the cost of the path, or `nil` when either end is blocked or the goal cannot be reached.

### path:update(x1, y1, x2, y2)

Rebuilds the clusters that hold the cells from `x1`, `y1` to `x2`, `y2` after their walkability or costs changed, or the cluster of one cell with `path:update(x, y)`. Only the changed clusters and their neighbors are rebuilt.

### path:rebuild()

Rebuilds every cluster.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(128, 128)
local router = grid:hierarchical()
print(router:findPath(0, 64, 127, 64))

for row = 0, 127 do
    grid:setWalkable(64, row, row == 100)
end
router:update(64, 0, 64, 127)
local path = router:findPath(0, 64, 127, 64)
print(#path)
```

## NavGraph

A `NavGraph` finds paths through waypoints. The game picks the integer id of every point. Each point has a position, a weight that scales the cost of stepping into it and an enabled flag, and connections join points in one or both directions. Stepping into a point costs the distance to it times its weight. Paths use A* and never pass through disabled points. Unknown ids raise an error such as `Point 7 is not in the graph.`

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | Number of points. |

### graph:addPoint(id, x, y, weight)

Adds a point, or moves an existing one and changes its weight while keeping its connections. `weight` defaults to 1, and weights below 1 raise `A graph point weight must be finite and at least 1.`

### graph:removePoint(id)

Removes the point and its connections and returns true, or returns false when it was not in the graph.

### graph:hasPoint(id)

Returns true when the point is in the graph.

### graph:position(id)

Returns the position of the point as a `Vec2`.

### graph:setPosition(id, x, y)

Moves the point.

### graph:weight(id)

Returns the weight of the point.

### graph:setWeight(id, weight)

Changes the weight of the point.

### graph:enabled(id)

Returns true when paths may pass through the point.

### graph:setEnabled(id, enabled)

Enables or disables the point. Disabled points keep their connections, so a door can close and open again.

### graph:connect(from, to, bidirectional)

Connects two points, both ways unless `bidirectional` is false. Connecting a point to itself raises `A graph point cannot connect to itself.`

### graph:disconnect(from, to, bidirectional)

Removes the connection, both ways unless `bidirectional` is false.

### graph:connected(from, to)

Returns true when a path may step from the first point into the second.

### graph:neighbors(id)

Returns the list of points reachable in one step from the point, in the order they were connected.

### graph:points()

Returns the list of every point id in ascending order.

### graph:closest(x, y, includeDisabled)

Returns the id of the enabled point closest to `x`, `y`, or of any point when `includeDisabled` is true, with ties going to the smaller id, or `nil` for an empty graph.

### graph:findPath(from, to)

Returns the list of point ids from `from` to `to`, both included, followed by the cost of the path, or `nil` when either end is disabled or `to` cannot be reached.

### graph:distances(source)

Returns a table from point id to the cost of the cheapest path from `source`, computed with Dijkstra, holding only the points that can be reached.

### graph:clear()

Removes every point.

```lua
local navigation2d = require('haylen.navigation2d')

local roads = navigation2d.newGraph()
roads:addPoint(1, 0, 0)
roads:addPoint(2, 100, 0)
roads:addPoint(3, 100, 100)
roads:addPoint(4, 0, 100, 3)
roads:connect(1, 2)
roads:connect(2, 3)
roads:connect(1, 4)
roads:connect(4, 3)

local route, cost = roads:findPath(1, 3)
print(table.concat(route, ' > '), cost)

roads:setEnabled(2, false)
route, cost = roads:findPath(1, 3)
print(table.concat(route, ' > '), cost)

print(roads:closest(90, 10), roads:closest(90, 10, true), roads:distances(1)[4], roads.size)
```

## NavMesh

A `NavMesh` covers the walkable area of a level with triangles, built with a constrained Delaunay triangulation from a boundary polygon and obstacle polygons, such as the collision objects of a Tiled map from `tileMap:objectOutlines`. Obstacles may overlap each other and cross the boundary. Paths run through the triangles with A* and are pulled straight with the funnel algorithm, so they turn only at obstacle corners, and they keep an agent radius clear of every corner and skip passages narrower than the agent. Changing a polygon marks the mesh as dirty, and it rebuilds on the next query or on `mesh:build`. Polygons are lists of points, and fewer than three points raise `A navigation mesh polygon needs at least three points.`

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `boundary` | list of Vec2 | read | The outline of the walkable area. |
| `obstacleCount` | integer | read | Number of obstacles. |
| `triangleCount` | integer | read | Number of triangles, which rebuilds a dirty mesh first. |
| `dirty` | boolean | read | True when the polygons changed since the last build. |

### mesh:setBoundary(polygon)

Sets the outline of the walkable area.

### mesh:addObstacle(polygon)

Adds an obstacle and returns its id.

### mesh:setObstacle(id, polygon)

Changes the shape of an obstacle, such as a door that moves. Unknown ids raise an error such as `Obstacle 3 is not in the navigation mesh.`

### mesh:removeObstacle(id)

Removes an obstacle and returns true, or returns false when it was not in the mesh.

### mesh:clearObstacles()

Removes every obstacle.

### mesh:build()

Rebuilds the triangles now, instead of on the next query. A mesh without a boundary raises `A navigation mesh needs a boundary before it builds.`

### mesh:findPath(x1, y1, x2, y2, agentRadius)

Returns the path from the start point to the goal point as a list of `Vec2` with the start, the corners to turn at and the goal, followed by its length, or `nil` when either point lies outside the mesh or no corridor is wide enough for the agent. `agentRadius` defaults to 0.

### mesh:findTriangle(x, y)

Returns the index of the triangle under the point, counting from 1 like `mesh:triangles`, or `nil` outside the mesh.

### mesh:contains(x, y)

Returns true when the point lies on the walkable area.

### mesh:closestPoint(x, y)

Returns the point itself inside the mesh and the closest point of the mesh outside it, as two numbers, or `nil` for an empty mesh. Use it to move a click on a wall onto walkable ground.

### mesh:triangles()

Returns the list of triangles, each a list of three `Vec2` corners, for drawing the mesh.

```lua
local navigation2d = require('haylen.navigation2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local mesh = navigation2d.newNavMesh({{0, 0}, {800, 0}, {800, 600}, {0, 600}})
mesh:addObstacle({{300, 100}, {500, 100}, {500, 300}, {300, 300}})
local door = mesh:addObstacle({{600, 350}, {620, 350}, {620, 600}, {600, 600}})
local hero = {x = 50, y = 50}

scene.push({
    update = function(self, dt)
        if input.mousePressed() then
            local x, y = mesh:closestPoint(camera:screenToWorld(input.mousePosition()))
            self.route = mesh:findPath(hero.x, hero.y, x, y, 12)
        end
        if input.keyPressed('o') then
            mesh:removeObstacle(door)
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        for _, triangle in ipairs(mesh:triangles()) do
            graphics2d.drawPolyline(triangle, 1, '#40FFFFFF', true)
        end
        if self.route then
            graphics2d.drawPolyline(self.route, 3, '#FFFFD040')
        end
    end,
})
```

## SteeringAgent

A `SteeringAgent` is a moving body for steering behaviors. Each behavior returns the velocity change it wants as a `Vec2`, the app adds the behaviors it uses, and `agent:apply` turns the velocity toward the sum within `maxForce` and moves the agent. Targets and neighbors accept a `Vec2` or a table `{x, y}` or `{x = 0, y = 0}`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `position` | Vec2 | read and write | Current position. Reading returns a copy. |
| `velocity` | Vec2 | read and write | Current velocity in units per second. Reading returns a copy. |
| `maxSpeed` | number | read and write | Speed limit. |
| `maxForce` | number | read and write | Largest velocity change per second. |
| `wanderDistance` | number | read and write | Distance ahead of the agent of the circle that `wander` aims at. |
| `wanderRadius` | number | read and write | Radius of that circle. |
| `wanderJitter` | number | read and write | How fast the wander target drifts around the circle, in radians per second. |

```lua
local navigation2d = require('haylen.navigation2d')

local moth = navigation2d.newAgent({wanderJitter = 3})
print(moth.wanderDistance, moth.wanderRadius, moth.wanderJitter)
moth.wanderRadius = 60
moth.wanderJitter = 8
```

### agent:seek(target)

Returns the change that turns the velocity toward `target` at full speed.

### agent:flee(threat)

Returns the change that turns the velocity away from `threat` at full speed.

### agent:arrive(target, slowingRadius)

Returns the change that moves toward `target` and slows down linearly inside `slowingRadius`, stopping on the target.

### agent:separation(neighbors, radius)

Returns a push away from every position in the list `neighbors` that is closer than `radius`, stronger the closer it is. A neighbor on the exact same spot gives no direction and is ignored.

### agent:alignment(velocities)

Returns the change that turns the velocity toward the average of the list of neighbor `velocities` at full speed, which keeps a flock heading the same way. An empty list returns a zero vector.

### agent:cohesion(positions)

Returns the change that seeks the center of the list of neighbor `positions`, which keeps a flock together. An empty list returns a zero vector.

### agent:avoid(circles, lookAhead)

Returns a sideways swerve around the closest circle that the path ahead of the agent enters within `lookAhead` units, stronger the closer it is, the obstacle avoidance of Reynolds. Circles are tables such as `{center = {100, 50}, radius = 20}` or `{{100, 50}, 20}` and should include the radius of the agent. It returns a zero vector when the way ahead is clear or the agent stands still.

### agent:wander(dt)

Returns the change that follows a target drifting around a circle ahead of the agent, which gives smooth and unpredictable motion. Each agent keeps its own wander state, advanced by `dt` seconds.

### agent:apply(force, dt)

Changes the velocity by `force`, limited to `maxForce * dt`, caps the speed at `maxSpeed` and moves the position by the velocity over `dt` seconds.

```lua
local graphics2d = require('haylen.graphics2d')
local navigation2d = require('haylen.navigation2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local leader = navigation2d.newAgent({x = -300, y = 0, maxSpeed = 140})
local followers = {}
for index = 1, 5 do
    followers[index] = navigation2d.newAgent({x = -300 - index * 20, y = index * 10, maxSpeed = 120, seed = index})
end
local wanderer = navigation2d.newAgent({x = 200, y = 200, maxSpeed = 60})
local goal = {300, -100}
local rocks = {{center = {0, -40}, radius = 50}}

scene.push({
    update = function(self, dt)
        leader:apply(leader:arrive(goal, 120) + leader:avoid(rocks, 120), dt)

        local positions, velocities = {}, {}
        for index, follower in ipairs(followers) do
            positions[index] = follower.position
            velocities[index] = follower.velocity
        end
        for _, follower in ipairs(followers) do
            local force = follower:seek(leader.position) + follower:separation(positions, 40) * 1.5 + follower:alignment(velocities) * 0.3 + follower:cohesion(positions) * 0.2
            if (follower.position - leader.position):length() < 60 then
                force = follower:flee(leader.position)
            end
            follower:apply(force + follower:avoid(rocks, 80), dt)
        end

        wanderer:apply(wanderer:wander(dt), dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawCircle(0, -40, 50, '#FF605040')
        graphics2d.drawCircle(leader.position.x, leader.position.y, 12, '#FFFFD040')
        for _, follower in ipairs(followers) do
            graphics2d.drawCircle(follower.position.x, follower.position.y, 8, '#FF60A0FF')
        end
        graphics2d.drawCircle(wanderer.position.x, wanderer.position.y, 8, '#FFA060FF')
    end,
})
```

Moving along a grid path combines paths and steering:

```lua
local navigation2d = require('haylen.navigation2d')
local scene = require('haylen.scene')

local tileSize = 64
local grid = navigation2d.newGrid(20, 12)
local walker = navigation2d.newAgent({x = 32, y = 32, maxSpeed = 160})
local route = grid:findPath(0, 0, 15, 9, {smooth = true})
local waypoint = 2

scene.push({
    update = function(self, dt)
        local cell = route[waypoint]
        if not cell then
            return
        end
        local target = {cell.x * tileSize + tileSize / 2, cell.y * tileSize + tileSize / 2}
        local isLast = waypoint == #route
        walker:apply(isLast and walker:arrive(target, 64) or walker:seek(target), dt)
        if (walker.position - target):length() < 8 and not isLast then
            waypoint = waypoint + 1
        end
    end,
})
```

## Crowd

A `Crowd` moves many agents at once with optimal reciprocal collision avoidance, the ORCA of the RVO2 library. Every step, each agent takes the velocity closest to the one it prefers among those that keep it clear of its neighbors for a time horizon, sharing the effort with them, and clear of static obstacles for a shorter one. Steps are deterministic, allocate nothing once the crowd has grown and spread over the engine job system. Agents are identified by the integer ids `crowd:addAgent` returns, which a removed agent frees for later agents. Unknown ids raise an error such as `Agent 7 is not in the crowd.`

Agents that all want the same spot, or that meet head on in a symmetric formation, can settle in a standoff where every agent keeps its neighbors clear, which is the correct ORCA answer. Games break it by giving targets a small spread or by adding a little `separation`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `agentCount` | integer | read | Number of agents. |
| `separation` | number | read and write | Weight of the push away from close neighbors. |
| `alignment` | number | read and write | Weight of the turn toward the heading of neighbors. |
| `cohesion` | number | read and write | Weight of the pull toward the center of neighbors. |

### crowd:addAgent(options)

Adds an agent and returns its id. `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | Starting position. |
| `vx`, `vy` | number | `0` | Starting velocity. |
| `radius` | number | `16` | Radius the agent keeps clear. |
| `maxSpeed` | number | `120` | Speed limit in units per second. |
| `neighborDistance` | number | `160` | How far the agent looks for neighbors to avoid. |
| `maxNeighbors` | integer | `10` | How many of the closest neighbors the agent avoids. |
| `timeHorizon` | number | `2` | How many seconds ahead the agent avoids other agents. Larger values react earlier and move more cautiously. |
| `obstacleTimeHorizon` | number | `1` | How many seconds ahead the agent avoids obstacles. |

Negative sizes, non-finite values and time horizons that are not positive raise `A crowd agent needs a finite position, radius, speed and neighbor distance and positive time horizons.`

### crowd:removeAgent(id)

Removes the agent and returns true, or returns false when it was not in the crowd.

### crowd:hasAgent(id)

Returns true when the agent is in the crowd.

### crowd:addObstacle(polygon)

Adds a solid polygon, in either winding, that agents steer around, or a wall that blocks both sides when it has two points. Fewer points raise `A crowd obstacle needs at least two points.`

### crowd:clearObstacles()

Removes every obstacle.

### crowd:position(id)

Returns the position of the agent as a `Vec2`.

### crowd:setPosition(id, x, y)

Moves the agent, such as after a teleport.

### crowd:velocity(id)

Returns the velocity the agent took in the last step as a `Vec2`.

### crowd:radius(id)

Returns the radius of the agent.

### crowd:setPreferredVelocity(id, vx, vy)

Sets the velocity the agent wants, such as the direction of a flow field times its speed, which also drops its target.

### crowd:setTarget(id, x, y)

Makes the agent head for the point at full speed every step and slow down to stop on it.

### crowd:clearTarget(id)

Drops the target of the agent, which then wants to stand still.

### crowd:target(id)

Returns the target of the agent as a `Vec2`, or `nil` when it has none.

### crowd:step(dt)

Picks the new velocity of every agent from the state before the step, then moves them all by `dt` seconds. A time that is not positive raises `A crowd step needs a positive and finite time.`

```lua
local navigation2d = require('haylen.navigation2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local crowd = navigation2d.newCrowd({separation = 0.2})
crowd:addObstacle({{-60, -60}, {60, -60}, {60, 60}, {-60, 60}})
crowd:addObstacle({{-400, 250}, {400, 250}})

local soldiers = {}
for index = 0, 99 do
    local angle = index / 100 * math.pi * 2
    local id = crowd:addAgent({x = math.cos(angle) * 300, y = math.sin(angle) * 200, radius = 8, maxSpeed = 90})
    crowd:setTarget(id, -math.cos(angle) * 300, -math.sin(angle) * 200)
    soldiers[#soldiers + 1] = id
end

scene.push({
    update = function(self, dt)
        crowd:step(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawRect({-60, -60, 120, 120}, '#FF404040')
        for _, id in ipairs(soldiers) do
            local position = crowd:position(id)
            graphics2d.drawCircle(position.x, position.y, crowd:radius(id), '#FF60C0FF')
        end
    end,
})
```
