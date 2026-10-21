# 📖 `haylen.navigation2d`

The module `haylen.navigation2d` moves characters through the world. Navigation grids plan routes over square, isometric, staggered and hexagonal tile maps with A\*, weighted A\*, jump point search and hierarchical path finding. They also build Dijkstra maps and flow fields, which guide any number of units at once. Waypoint graphs find routes between points that the app places, and navigation meshes find smooth routes through open levels for agents of any size. Steering agents move one character smoothly, and crowds move thousands of agents together without overlaps. Long searches run in the background and return promises. The [navigation guide](../2d/navigation.md) explains how to pick a method, how the parts combine and what each one costs.

```lua
local navigation2d = require('haylen.navigation2d')
```

## Cells

Grid cells are addressed by column and row, counting from 0 like the cells of Tiled. So the `column, row` that [`map:worldToCell`](tiled.md#mapworldtocellx-y) of `haylen.tiled` returns is a cell that a grid accepts. Hexagonal and staggered grids use the same offset coordinates as Tiled. Paths come back as lists of cell tables `{x = column, y = row}`. Functions that take cells in a list also accept a cell written as `{column, row}`.

## Background work

Functions whose names end in `Async` copy what they need, run on a worker thread and return a promise of the `async` module of Varn. Call `:await()` on it inside a coroutine that `async.spawn` starts. It returns the result on success, and `nil` and the error message on failure. Wrong arguments still raise at once, in the call. Every coroutine that waits for the same promise gets its own copy of the result.

A grid keeps one copy of its cells for background work, and it shares that copy between calls until one of its cells changes. So many background searches on a grid that does not change copy it once.

## Grids

### navigation2d.newGrid(width, height, layout)

Returns a [`NavGrid`](#navgrid) of `width` columns and `height` rows. Every cell starts walkable with a cost of 1, and cells outside the grid count as blocked. A size that is not positive, or that does not fit 32-bit cell indices, raises `A navigation grid needs a positive size that fits in 32-bit cell indices.` The table `layout` is optional and follows the map properties of Tiled.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `topology` | string | `'square'` | The value `'square'` stands for orthogonal, isometric and oblique maps, `'hexagonal'` for hexagonal maps and `'staggered'` for staggered isometric maps. |
| `staggerX` | boolean | `false` | The columns shift instead of the rows, like a stagger axis of `x` in Tiled. |
| `staggerEven` | boolean | `false` | The even rows or columns shift instead of the odd ones, like a stagger index of `even` in Tiled. |

Square and staggered cells step through their four sides, and through their four corners when diagonal steps are on. Hexagonal cells step through their six sides. Every step costs the cost of the cell it enters.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(40, 30)
local hexes = navigation2d.newGrid(20, 20, {topology = 'hexagonal', staggerX = true})
print(grid.width, grid.height, hexes.topology, hexes.staggerX) -- 40 30 hexagonal true
```

## NavGrid

A `NavGrid` is a grid of walkable cells with costs, which [`navigation2d.newGrid`](#navigation2dnewgridwidth-height-layout) returns. The methods `grid:setWalkable`, `grid:setCost` and `grid:cost` raise an error such as `Cell 9,9 is outside the navigation grid.` for a cell outside the grid.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | The number of columns. |
| `height` | integer | read | The number of rows. |
| `topology` | string | read | `'square'`, `'hexagonal'` or `'staggered'`. |
| `staggerX` | boolean | read | Whether the columns shift instead of the rows. |
| `staggerEven` | boolean | read | Whether the even rows or columns shift. |
| `uniformCost` | boolean | read | The value is `true` when every cell costs 1, which jump point search needs. |
| `expandedCount` | integer | read | The cells that the last `grid:findPath` expanded. It shows how much work a heuristic, a weight or jump point search saves. |

### grid:contains(x, y)

Returns `true` when the cell lies inside the grid.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(10, 10)
print(grid:contains(9, 9), grid:contains(10, 0)) -- true false
```

### grid:setWalkable(x, y, walkable)

Marks the cell as walkable when `walkable` is `true`, or as blocked when it is `false`.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(10, 10)
for row = 0, 7 do
    grid:setWalkable(5, row, false)
end
```

### grid:walkable(x, y)

Returns `true` when the cell lies inside the grid and is walkable.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(10, 10)
grid:setWalkable(5, 3, false)
print(grid:walkable(5, 3), grid:walkable(5, 8), grid:walkable(-1, 0)) -- false true false
```

### grid:setCost(x, y, cost)

Sets the cost of entering the cell. A cost of 1 is plain ground, and larger costs make paths avoid the cell, such as a swamp. A cost below 1 or not finite raises `A navigation cost must be finite and at least 1.`

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(20, 12)
for column = 4, 15 do
    grid:setCost(column, 6, 5)
end
print(grid.uniformCost) -- false
```

### grid:cost(x, y)

Returns the cost of entering the cell.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(20, 12)
grid:setCost(8, 6, 5)
print(grid:cost(8, 6), grid:cost(8, 5)) -- 5.0 1.0
```

### grid:estimate(fromX, fromY, toX, toY, heuristic)

Returns how long the walk between two cells is by the guess that searches use, a heuristic, which ignores walls and costs. The argument `heuristic` is `'manhattan'`, `'octile'`, `'euclidean'` or `'chebyshev'`. Hexagonal grids always count hex steps, whatever the heuristic.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(20, 20)
print(grid:estimate(0, 0, 3, 4, 'manhattan'), grid:estimate(0, 0, 3, 4, 'octile'), grid:estimate(0, 0, 3, 4, 'euclidean'))
```

### grid:findPath(startX, startY, goalX, goalY, options)

Finds the cheapest path from the start cell to the goal cell. It returns two values: the path, a list of cells with both ends, and its cost. It returns `nil` when either end is blocked or the goal cannot be reached. Diagonal steps never cut the corner of a blocked cell. Equal paths always resolve the same way, so the same grid gives the same path on every device. The grid keeps its search buffers, so repeated searches allocate nothing once the buffers have grown. The table `options` is optional.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `diagonal` | boolean | `true` | Allows diagonal steps, which cost the square root of 2 times the cost of the cell. Hexagonal grids ignore it. |
| `smooth` | boolean | `false` | Returns the path after [`grid:smoothPath`](#gridsmoothpathpath), with only the cells where it turns. The cost stays the cost of the path before smoothing. |
| `heuristic` | string | `'octile'` or `'manhattan'` | How the search guesses the distance left: `'manhattan'`, `'octile'`, `'euclidean'` or `'chebyshev'`. The default is `'octile'` with diagonal steps and `'manhattan'` without them, which are exact on open ground. With diagonal steps, `'manhattan'` guesses too long, so it can return a path that is not the cheapest. Hexagonal grids always count hex steps and refuse this option. |
| `weight` | number | `1` | Weighted A\*. A weight above 1 explores fewer cells and returns a path that costs at most `weight` times the cheapest one. |
| `jumpPoint` | boolean | `false` | Jump point search, which skips over runs of open ground. It returns paths as cheap as A\* while it expands far fewer cells, most of all on large open maps. It needs a square or staggered grid where every cell costs 1. |

These errors come from wrong options:

- An unknown key raises `Unknown option "<key>".`
- A weight below 1 or not finite raises `A search weight must be finite and at least 1.`
- A heuristic on a hexagonal grid raises `Hexagonal grids count hex steps and take no heuristic.`
- Jump point search raises `Jump point search needs a square or staggered grid.` on a hexagonal grid, and `Jump point search needs a grid where every cell costs 1.` on a grid with costs.

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
print('Cost', cost)

local corners = grid:findPath(0, 0, 8, 0, {smooth = true})
local manhattan = grid:findPath(0, 0, 3, 3, {diagonal = false})
local jumped = grid:findPath(0, 0, 8, 0, {jumpPoint = true})
local rough = grid:findPath(0, 0, 8, 0, {weight = 2, heuristic = 'euclidean'})
print(#corners, #manhattan, #jumped, #rough, grid:findPath(0, 0, 20, 20))

grid:findPath(0, 0, 8, 0)
local plain = grid.expandedCount
grid:findPath(0, 0, 8, 0, {jumpPoint = true})
print(plain, grid.expandedCount)
```

### grid:findPathAsync(startX, startY, goalX, goalY, options)

Searches like [`grid:findPath`](#gridfindpathstartx-starty-goalx-goaly-options) on a copy of the grid on a worker thread. It returns a promise that resolves with the path, or with `nil` when there is none. Changes to the grid after the call do not affect the search. Wrong options, such as jump point search on a grid with costs, raise at once.

```lua
local async = require('async')
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(512, 512)
async.spawn(function()
    local path = grid:findPathAsync(0, 0, 511, 511, {jumpPoint = true}):await()
    print(#path)
end)
```

### grid:lineOfSight(fromX, fromY, toX, toY)

Returns `true` when the straight line between the centers of two cells crosses only walkable cells. A line that passes exactly through a corner needs both cells beside the corner to be walkable. On hexagonal grids, it checks the cells along the line between the centers of both hexagons.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(8, 8)
grid:setWalkable(3, 1, false)
print(grid:lineOfSight(0, 1, 7, 1), grid:lineOfSight(0, 0, 7, 0)) -- false true
```

### grid:smoothPath(path)

Returns a shorter copy of `path`. It keeps the start, the goal and every cell where a straight walk would leave walkable ground. Smoothing looks at walkability only, so the result may cross costly cells.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(10, 10)
local path = grid:findPath(0, 0, 9, 3, {diagonal = false})
local waypoints = grid:smoothPath(path)
print(#path, #waypoints)
print(#grid:smoothPath({{0, 4}, {1, 4}, {2, 4}})) -- 2
```

### grid:raycast(from, to, cellSize)

Casts a ray in world units from the point `from` to the point `to` over a square grid whose cells are `cellSize` units wide, with the first cell at the origin. The cell size is a number, or a `{width, height}` pair. It returns the hit on the first blocked cell, or `nil`. The hit is a table like the hits of [`haylen.math`](math.md#ray-casts), with the fields `column` and `row` of the cell. Cells outside the grid block the ray like any blocked cell. Other topologies raise `Grid ray casts need a square grid.`

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(20, 12)
grid:setWalkable(10, 2, false)
local hit = grid:raycast({8, 80}, {600, 80}, 32)
print(hit.column, hit.row, hit.x, hit.normalX) -- 10 2 320.0 -1.0
```

### grid:dijkstraMap(sources, options)

Returns a [`DijkstraMap`](#dijkstramap). For every cell, it stores the cost of the cheapest walk to the nearest source, plus the value of that source. The argument `sources` is a list of cells, and each may have a `value` field, `0` by default. Lower values attract more, so a treasure can pull harder than a lever. The table `options` is optional and takes `diagonal`, `true` by default. Blocked and unreachable cells get `math.huge`. The map keeps its grid alive, and it reads the walkability of the grid as it is when it steps. A source value that is not finite raises `A Dijkstra map source needs a finite value.`

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(30, 20)
local player = {x = 5, y = 5}
local gold = {x = 25, y = 15, value = -4}
local chase = grid:dijkstraMap({player, gold})
print(chase:value(5, 5), chase:value(25, 15), chase:value(6, 5)) -- 0.0 -4.0 1.0
```

### grid:dijkstraMapAsync(sources, options)

Computes a Dijkstra map like [`grid:dijkstraMap`](#griddijkstramapsources-options) on a copy of the grid on a worker thread, and returns a promise that resolves with the map. A source value that is not finite raises `A Dijkstra map source needs a finite value.` at once.

```lua
local async = require('async')
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(256, 256)
async.spawn(function()
    local chase = grid:dijkstraMapAsync({{128, 128}}):await()
    print(chase:value(0, 0))
end)
```

### grid:flowField(goals, options)

Returns a [`FlowField`](#flowfield). For every cell, it stores the step toward the nearest goal, so any number of units move by reading the step of their cell. The argument `goals` is a list of cells. The table `options` is optional and takes `diagonal`, `true` by default. The field is a snapshot of the grid when it was computed.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(40, 30)
local field = grid:flowField({{39, 15}})
print(field:next(0, 15)) -- 1 15
print(field:direction(0, 0), field:distance(0, 15))
```

### grid:flowFieldAsync(goals, options)

Computes a flow field like [`grid:flowField`](#gridflowfieldgoals-options) on a copy of the grid on a worker thread, and returns a promise that resolves with the field.

```lua
local async = require('async')
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(256, 256)
async.spawn(function()
    local field = grid:flowFieldAsync({{128, 128}}):await()
    print(field:distance(0, 0))
end)
```

### grid:hierarchicalPathfinder(options)

Returns a [`HierarchicalPathfinder`](#hierarchicalpathfinder) over a square grid, the HPA\* of Botea, Müller and Schaeffer. It splits the grid into square clusters joined by entrances. A search first walks a small graph of the entrances, and then only the cells along the chosen route. That makes long searches on large grids much faster than A\*. Paths are near the cheapest, usually within a few percent. The path finder keeps its grid alive and reads it on every search. Other topologies raise `Hierarchical path finding needs a square grid.` The table `options` is optional.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `clusterSize` | integer | `16` | The width and height of a cluster in cells, at least 2. Larger clusters make fewer entrances and slower searches inside a cluster. A size above the grid makes one cluster that covers it. A smaller size raises `Hierarchical path finding needs clusters of at least 2 cells.` |
| `diagonal` | boolean | `true` | Allows diagonal steps. |

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(256, 256)
for row = 0, 200 do
    grid:setWalkable(128, row, false)
end
local router = grid:hierarchicalPathfinder({clusterSize = 32})
local path, cost = router:findPath(0, 0, 255, 0)
print(#path, cost, router.nodeCount)
```

### grid:hierarchicalPathfinderAsync(options)

Builds a `HierarchicalPathfinder` like [`grid:hierarchicalPathfinder`](#gridhierarchicalpathfinderoptions) from a copy of the grid on a worker thread, so building one over a large map never stalls a frame. It returns a promise that resolves with the path finder. The path finder reads the grid itself once it arrives, so cells that changed after the call need [`router:update`](#routerupdatex1-y1-x2-y2). Wrong options and grids that are not square raise at once.

```lua
local async = require('async')
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(1024, 1024)
async.spawn(function()
    local router = grid:hierarchicalPathfinderAsync({clusterSize = 32}):await()
    print(#router:findPath(0, 0, 1023, 1023), router.nodeCount)
end)
```

## DijkstraMap

A `DijkstraMap` is a map of distances, which [`grid:dijkstraMap`](#griddijkstramapsources-options) returns. A unit walks downhill on it, toward lower values, to reach the nearest source. The method `map:value` raises `The cell is outside the Dijkstra map.` for a cell outside the map, and `map:next` returns `nil` there.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | The number of columns. |
| `height` | integer | read | The number of rows. |

### map:value(x, y)

Returns the value of the cell, or `math.huge` for a blocked or unreachable cell.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(10, 10)
local map = grid:dijkstraMap({{0, 0}})
print(map:value(3, 0), map.width, map.height) -- 3.0 10 10
```

### map:next(x, y)

Returns the column and row of the neighbor that leads downhill along a cheapest path. It reads the walkability of the grid as it is now. It returns `nil` at a lowest point, at a blocked cell, at an unreachable one and outside the map.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(10, 10)
local map = grid:dijkstraMap({{0, 0}})
local x, y = map:next(3, 0)
print(x, y, map:next(0, 0)) -- 2 0 nil
```

### map:values()

Returns every value row by row, from the top left cell, which suits drawing the map. Blocked and unreachable cells hold `math.huge`.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(4, 3)
local values = grid:dijkstraMap({{0, 0}}):values()
print(#values, values[1], values[2], values[5]) -- 12 0.0 1.0 1.0
```

### map:flee(coefficient)

Turns the map into a flee map. It multiplies every value by `coefficient`, `-1.2` by default, and walks the map again. The cells farthest from the sources become the lowest, while dead ends stay higher. So a unit that walks downhill runs away along open escape routes instead of into corners. A coefficient that is not negative and finite raises `A flee map needs a finite negative coefficient.`

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

A `FlowField` stores the step toward the nearest goal for every cell, which [`grid:flowField`](#gridflowfieldgoals-options) returns. A cell outside the field raises `The cell is outside the flow field.`

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | The number of columns. |
| `height` | integer | read | The number of rows. |

### field:next(x, y)

Returns the column and row of the cell to step into. It returns `nil` at a goal, at a blocked cell and at an unreachable one.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(10, 10)
local field = grid:flowField({{9, 0}})
local x, y = field:next(5, 0)
print(x, y, field:next(9, 0)) -- 6 0 nil
```

### field:direction(x, y)

Returns the unit `Vec2` from the cell toward its next cell, in grid coordinates, which is the world direction on square grids. It returns a zero vector where there is no next cell.

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

### field:distance(x, y)

Returns the cost of the cheapest walk from the cell to a goal, or `math.huge` when no goal can be reached.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(10, 10)
local field = grid:flowField({{9, 0}}, {diagonal = false})
print(field:distance(0, 0), field.width) -- 9.0 10
```

## HierarchicalPathfinder

A `HierarchicalPathfinder` plans long paths over clusters of a square grid, which [`grid:hierarchicalPathfinder`](#gridhierarchicalpathfinderoptions) returns. It reads its grid on every search, so after cells change, `router:update` must rebuild the clusters around them. Until then, a search whose route crosses the changed cells finds no path.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `clusterSize` | integer | read | The width and height of a cluster in cells. |
| `diagonal` | boolean | read | Whether paths take diagonal steps. |
| `nodeCount` | integer | read | The number of entrance cells in the graph of entrances. |

### router:findPath(startX, startY, goalX, goalY)

Returns the cells from the start to the goal, both included, and the cost of the path. It returns `nil` when either end is blocked, the goal cannot be reached or the route crosses cells that changed since the last `router:update`.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(128, 128)
local router = grid:hierarchicalPathfinder({diagonal = false})
local path, cost = router:findPath(0, 64, 127, 64)
print(#path, cost)
print(router.clusterSize, router.diagonal) -- 16 false
```

### router:update(x1, y1, x2, y2)

Rebuilds the clusters that hold the cells from `x1`, `y1` to `x2`, `y2` after their walkability or costs changed. With two arguments, `router:update(x, y)` rebuilds the cluster of one cell. Only the changed clusters and their neighbors are rebuilt.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(128, 128)
local router = grid:hierarchicalPathfinder({diagonal = false})
for row = 0, 127 do
    grid:setWalkable(64, row, row == 100)
end
router:update(64, 0, 64, 127)
print(#router:findPath(0, 64, 127, 64))
```

### router:rebuild()

Rebuilds every cluster, such as after a large part of the grid changed.

```lua
local navigation2d = require('haylen.navigation2d')

local grid = navigation2d.newGrid(64, 64)
local router = grid:hierarchicalPathfinder()
for column = 0, 63 do
    grid:setCost(column, 32, 4)
end
router:rebuild()
print(router.nodeCount)
```

## Waypoint graphs

### navigation2d.newGraph()

Returns an empty [`NavGraph`](#navgraph).

```lua
local navigation2d = require('haylen.navigation2d')

local roads = navigation2d.newGraph()
print(roads.size) -- 0
```

## NavGraph

A `NavGraph` finds paths through waypoints, which [`navigation2d.newGraph`](#navigation2dnewgraph) returns. The app picks the integer id of every point. Each point has a position, a weight and an enabled flag, and connections join points in one or both directions. Stepping into a point costs the distance to it times its weight. Paths use A\* and never pass through a disabled point. An unknown id raises an error such as `Point 7 is not in the graph.`

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of points. |

### graph:addPoint(id, x, y, weight)

Adds a point at `x`, `y`. When the point exists, it moves and takes the new weight, and it keeps its connections. The argument `weight` scales the cost of stepping into the point, `1` by default. A weight below 1 or not finite raises `A graph point weight must be finite and at least 1.`

```lua
local navigation2d = require('haylen.navigation2d')

local rooms = navigation2d.newGraph()
rooms:addPoint(1, 0, 0)
rooms:addPoint(2, 50, 0, 3)
print(rooms.size) -- 2
```

### graph:removePoint(id)

Removes the point and its connections and returns `true`, or returns `false` when it was not in the graph.

```lua
local navigation2d = require('haylen.navigation2d')

local rooms = navigation2d.newGraph()
rooms:addPoint(2, 50, 0)
print(rooms:removePoint(2), rooms:removePoint(2)) -- true false
```

### graph:hasPoint(id)

Returns `true` when the point is in the graph.

```lua
local navigation2d = require('haylen.navigation2d')

local rooms = navigation2d.newGraph()
rooms:addPoint(1, 0, 0)
print(rooms:hasPoint(1), rooms:hasPoint(2)) -- true false
```

### graph:position(id)

Returns the position of the point as a `Vec2`.

```lua
local navigation2d = require('haylen.navigation2d')

local stops = navigation2d.newGraph()
stops:addPoint(1, 40, 30)
print(stops:position(1)) -- Vec2(40.0, 30.0)
```

### graph:setPosition(id, x, y)

Moves the point.

```lua
local navigation2d = require('haylen.navigation2d')

local stops = navigation2d.newGraph()
stops:addPoint(1, 0, 0)
stops:setPosition(1, 40, 30)
print(stops:position(1).x) -- 40.0
```

### graph:weight(id)

Returns the weight of the point.

```lua
local navigation2d = require('haylen.navigation2d')

local stops = navigation2d.newGraph()
stops:addPoint(1, 0, 0, 2.5)
print(stops:weight(1)) -- 2.5
```

### graph:setWeight(id, weight)

Changes the weight of the point, with the same rule as `graph:addPoint`.

```lua
local navigation2d = require('haylen.navigation2d')

local stops = navigation2d.newGraph()
stops:addPoint(1, 0, 0)
stops:setWeight(1, 4)
print(stops:weight(1)) -- 4.0
```

### graph:enabled(id)

Returns `true` when paths may pass through the point.

```lua
local navigation2d = require('haylen.navigation2d')

local stops = navigation2d.newGraph()
stops:addPoint(1, 0, 0)
print(stops:enabled(1)) -- true
```

### graph:setEnabled(id, enabled)

Enables or disables the point. A disabled point keeps its connections, so a door can close and open again.

```lua
local navigation2d = require('haylen.navigation2d')

local stops = navigation2d.newGraph()
stops:addPoint(1, 0, 0)
stops:setEnabled(1, false)
print(stops:enabled(1)) -- false
```

### graph:connect(from, to, bidirectional)

Connects two points, both ways unless `bidirectional` is `false`. Connecting a point to itself raises `A graph point cannot connect to itself.`

```lua
local navigation2d = require('haylen.navigation2d')

local rails = navigation2d.newGraph()
rails:addPoint(1, 0, 0)
rails:addPoint(2, 100, 0)
rails:addPoint(3, 200, 0)
rails:connect(1, 2)
rails:connect(2, 3, false)
print(rails:connected(2, 1), rails:connected(3, 2)) -- true false
```

### graph:disconnect(from, to, bidirectional)

Removes the connection, both ways unless `bidirectional` is `false`.

```lua
local navigation2d = require('haylen.navigation2d')

local rails = navigation2d.newGraph()
rails:addPoint(1, 0, 0)
rails:addPoint(2, 100, 0)
rails:connect(1, 2)
rails:disconnect(2, 1, false)
print(rails:connected(1, 2), rails:connected(2, 1)) -- true false
```

### graph:connected(from, to)

Returns `true` when a path may step from the first point into the second.

```lua
local navigation2d = require('haylen.navigation2d')

local rails = navigation2d.newGraph()
rails:addPoint(1, 0, 0)
rails:addPoint(2, 100, 0)
rails:connect(1, 2, false)
print(rails:connected(1, 2), rails:connected(2, 1)) -- true false
```

### graph:neighbors(id)

Returns the list of the points that one step reaches from the point, in the order they were connected.

```lua
local navigation2d = require('haylen.navigation2d')

local rails = navigation2d.newGraph()
for id = 1, 3 do
    rails:addPoint(id, id * 100, 0)
end
rails:connect(1, 3)
rails:connect(1, 2)
print(table.concat(rails:neighbors(1), ' ')) -- 3 2
```

### graph:points()

Returns the list of every point id in increasing order.

```lua
local navigation2d = require('haylen.navigation2d')

local rails = navigation2d.newGraph()
for id = 3, 1, -1 do
    rails:addPoint(id, id * 100, 0)
end
print(table.concat(rails:points(), ' ')) -- 1 2 3
```

### graph:closestPoint(x, y, includeDisabled)

Returns the id of the enabled point closest to `x`, `y`, or of any point when `includeDisabled` is `true`. A tie goes to the smaller id. It returns `nil` for an empty graph. It checks every point, so its cost grows with the size of the graph.

```lua
local navigation2d = require('haylen.navigation2d')

local roads = navigation2d.newGraph()
roads:addPoint(1, 0, 0)
roads:addPoint(2, 100, 0)
roads:setEnabled(2, false)
print(roads:closestPoint(90, 10), roads:closestPoint(90, 10, true)) -- 1 2
```

### graph:findPath(from, to)

Returns the list of point ids from `from` to `to`, both included, and the cost of the path. It returns `nil` when either end is disabled or `to` cannot be reached.

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
print(table.concat(route, ' > '), cost) -- 1 > 2 > 3 200.0

roads:setEnabled(2, false)
route, cost = roads:findPath(1, 3)
print(table.concat(route, ' > '), cost) -- 1 > 4 > 3 400.0
```

### graph:distances(source)

Returns a table from point id to the cost of the cheapest path from `source`, computed with Dijkstra's algorithm. It holds only the points that can be reached.

```lua
local navigation2d = require('haylen.navigation2d')

local roads = navigation2d.newGraph()
roads:addPoint(1, 0, 0)
roads:addPoint(2, 100, 0)
roads:addPoint(3, 500, 500)
roads:connect(1, 2)
local costs = roads:distances(1)
print(costs[2], costs[3]) -- 100.0 nil
```

### graph:clear()

Removes every point.

```lua
local navigation2d = require('haylen.navigation2d')

local roads = navigation2d.newGraph()
roads:addPoint(1, 0, 0)
roads:clear()
print(roads.size, roads:closestPoint(0, 0)) -- 0 nil
```

## Navigation meshes

### navigation2d.newNavMesh(boundary)

Returns a [`NavMesh`](#navmesh). The argument `boundary` is optional. It sets the outline of the walkable area, a list of at least three points, which `mesh:setBoundary` can also set later.

```lua
local navigation2d = require('haylen.navigation2d')

local mesh = navigation2d.newNavMesh({{0, 0}, {640, 0}, {640, 480}, {0, 480}})
print(mesh.dirty) -- true
print(mesh.triangleCount, mesh.dirty)
```

### navigation2d.buildNavMeshAsync(boundary, obstacles)

Builds a `NavMesh` from the outline `boundary` and the optional list of obstacle polygons on a worker thread. It returns a promise that resolves with the built mesh. Use it for large levels, so loading never stalls a frame. A wrong polygon raises at once, and a build that fails rejects the promise.

```lua
local async = require('async')
local navigation2d = require('haylen.navigation2d')

async.spawn(function()
    local boundary = {{0, 0}, {4096, 0}, {4096, 4096}, {0, 4096}}
    local rocks = {{{1000, 1000}, {1200, 1000}, {1200, 1200}, {1000, 1200}}}
    local mesh = navigation2d.buildNavMeshAsync(boundary, rocks):await()
    print(mesh.triangleCount, mesh.dirty)
end)
```

## NavMesh

A `NavMesh` covers the walkable area of a level with triangles, which [`navigation2d.newNavMesh`](#navigation2dnewnavmeshboundary) and [`navigation2d.buildNavMeshAsync`](#navigation2dbuildnavmeshasyncboundary-obstacles) return. It is built with a constrained Delaunay triangulation, from a boundary polygon and obstacle polygons, such as the collision objects of a Tiled map that [`map:objectOutlines`](tiled.md#mapobjectoutlineslayer) returns. Obstacles may overlap each other and cross the boundary. The [navigation guide](../2d/navigation.md#navigation-meshes) explains how paths keep an agent clear of walls.

Changing a polygon marks the mesh as dirty, and the mesh rebuilds on the next query or on `mesh:build`. A polygon is a list of points. A polygon with fewer than three points raises `A navigation mesh polygon needs at least three points.`, and a point that is not finite raises `A navigation mesh polygon needs finite points.`

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `boundary` | list of Vec2 | read | The outline of the walkable area. |
| `obstacleCount` | integer | read | The number of obstacles. |
| `triangleCount` | integer | read | The number of triangles. Reading it rebuilds a dirty mesh first. |
| `dirty` | boolean | read | The value is `true` when the polygons changed since the last build. A new mesh is dirty. |

### mesh:setBoundary(polygon)

Sets the outline of the walkable area.

```lua
local navigation2d = require('haylen.navigation2d')

local mesh = navigation2d.newNavMesh()
mesh:setBoundary({{0, 0}, {640, 0}, {640, 480}, {0, 480}})
print(#mesh.boundary) -- 4
```

### mesh:addObstacle(polygon)

Adds an obstacle and returns its id. Ids count from 1.

```lua
local navigation2d = require('haylen.navigation2d')

local mesh = navigation2d.newNavMesh({{0, 0}, {640, 0}, {640, 480}, {0, 480}})
local pillar = mesh:addObstacle({{300, 200}, {340, 200}, {340, 240}, {300, 240}})
print(pillar, mesh.obstacleCount) -- 1 1
```

### mesh:setObstacle(id, polygon)

Changes the shape of an obstacle, such as a door that moves. An unknown id raises an error such as `Obstacle 3 is not in the navigation mesh.`

```lua
local navigation2d = require('haylen.navigation2d')

local mesh = navigation2d.newNavMesh({{0, 0}, {640, 0}, {640, 480}, {0, 480}})
local door = mesh:addObstacle({{300, 0}, {340, 0}, {340, 200}, {300, 200}})
mesh:setObstacle(door, {{300, 0}, {340, 0}, {340, 400}, {300, 400}})
```

### mesh:removeObstacle(id)

Removes an obstacle and returns `true`, or returns `false` when it was not in the mesh.

```lua
local navigation2d = require('haylen.navigation2d')

local mesh = navigation2d.newNavMesh({{0, 0}, {640, 0}, {640, 480}, {0, 480}})
local crate = mesh:addObstacle({{100, 100}, {140, 100}, {140, 140}, {100, 140}})
print(mesh:removeObstacle(crate), mesh:removeObstacle(crate)) -- true false
```

### mesh:clearObstacles()

Removes every obstacle.

```lua
local navigation2d = require('haylen.navigation2d')

local mesh = navigation2d.newNavMesh({{0, 0}, {640, 0}, {640, 480}, {0, 480}})
mesh:addObstacle({{100, 100}, {140, 100}, {140, 140}, {100, 140}})
mesh:clearObstacles()
print(mesh.obstacleCount) -- 0
```

### mesh:build()

Rebuilds the triangles now, instead of on the next query, such as during a loading screen. A mesh without a boundary raises `A navigation mesh needs a boundary before it builds.`, here and on any query that rebuilds it.

```lua
local navigation2d = require('haylen.navigation2d')

local mesh = navigation2d.newNavMesh({{0, 0}, {640, 0}, {640, 480}, {0, 480}})
mesh:addObstacle({{300, 0}, {340, 0}, {340, 400}, {300, 400}})
mesh:build()
print(mesh.dirty, mesh.triangleCount)
```

### mesh:findPath(x1, y1, x2, y2, agentRadius)

Returns the path from the start point to the goal point and its length. The path is a list of `Vec2`: the start, the points to turn at and the goal. The argument `agentRadius` is the radius of the agent that walks it, `0` by default. A radius that is negative or not finite raises `An agent radius must be finite and not negative.`

Paths run through the triangles with A\*, and the funnel algorithm pulls them straight, so they turn only around the corners of obstacles. With an agent radius, the path keeps the agent clear of walls:

- It skips a passage narrower than the agent. A way through the triangles has room exactly when every edge it crosses is at least as long as the agent is wide, within a thousandth of that width. The triangulation adds points on the walls where corners come close to them, which makes this hold, and it keeps walls that run almost parallel from filling with points.
- Every leg of the path stays inside the mesh. The path turns at points just off each corner: one point for a turn up to a right angle, and two for a wider turn. So the legs on both sides of a corner stay at least the radius away from it.
- A start closer than the radius to a wall first steps straight to its room. The room is the closest point within twice the radius that is the radius away from every wall. A goal that close to a wall is reached last from its room. So an agent pushed against a wall, or sent to a point on one, still finds its way.

It returns `nil` when either point lies outside the mesh, when no corridor is wide enough for the agent, or when the start or the goal has no room or cannot step straight to it.

```lua
local navigation2d = require('haylen.navigation2d')

local mesh = navigation2d.newNavMesh({{0, 0}, {300, 0}, {300, 200}, {0, 200}})
mesh:addObstacle({{140, -20}, {160, -20}, {160, 130}, {140, 130}})

-- The start touches the left wall, so the path first steps to 10, 50, where an agent of radius 10 has room.
local path, length = mesh:findPath(3, 50, 290, 50, 10)
print(#path, path[2].x, path[2].y, length)

-- A corridor narrower than the agent leaves it no room at all.
local corridor = navigation2d.newNavMesh({{0, 0}, {200, 0}, {200, 12}, {0, 12}})
print(corridor:findPath(20, 6, 180, 6, 10)) -- nil
```

### mesh:findTriangle(x, y)

Returns the index of the triangle under the point, counted from 1 like the list of `mesh:triangles`, or `nil` outside the mesh.

```lua
local navigation2d = require('haylen.navigation2d')

local mesh = navigation2d.newNavMesh({{0, 0}, {400, 0}, {400, 300}, {0, 300}})
print(mesh:findTriangle(50, 150), mesh:findTriangle(-5, -5))
```

### mesh:contains(x, y)

Returns `true` when the point lies on the walkable area.

```lua
local navigation2d = require('haylen.navigation2d')

local mesh = navigation2d.newNavMesh({{0, 0}, {400, 0}, {400, 300}, {0, 300}})
mesh:addObstacle({{150, 50}, {250, 50}, {250, 250}, {150, 250}})
print(mesh:contains(50, 150), mesh:contains(200, 150)) -- true false
```

### mesh:closestPoint(x, y)

Returns the closest point of the walkable area as two numbers `x, y`: the point itself when it lies inside the mesh, or the closest point of the mesh when it lies outside. It returns `nil` for an empty mesh. Use it to move a click on a wall onto walkable ground.

```lua
local navigation2d = require('haylen.navigation2d')

local mesh = navigation2d.newNavMesh({{0, 0}, {400, 0}, {400, 300}, {0, 300}})
print(mesh:closestPoint(500, 100)) -- 400.0 100.0
```

### mesh:triangles()

Returns the list of triangles, each a list of three `Vec2` corners, for drawing the mesh.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local navigation2d = require('haylen.navigation2d')
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

## Steering

### navigation2d.newAgent(options)

Returns a [`SteeringAgent`](#steeringagent). The table `options` is optional.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | The starting position. |
| `maxSpeed` | number | `120` | The speed limit in units per second. |
| `maxForce` | number | `600` | The largest change of velocity per second. Lower values turn and brake more slowly. |
| `wanderDistance` | number | `60` | How far ahead of the agent the circle of `agent:wander` lies. |
| `wanderRadius` | number | `30` | The radius of that circle. |
| `wanderJitter` | number | `4` | How fast the wander target drifts around the circle, in radians per second. |
| `seed` | integer | `1` | The seed of the wander randomness. Agents with the same seed and state wander the same way. |

```lua
local navigation2d = require('haylen.navigation2d')

local bat = navigation2d.newAgent({x = 100, y = 100, maxSpeed = 90, maxForce = 300, wanderJitter = 6, seed = 42})
print(bat.position, bat.maxSpeed) -- Vec2(100.0, 100.0) 90.0
```

## SteeringAgent

A `SteeringAgent` is a moving body for steering behaviors, which [`navigation2d.newAgent`](#navigation2dnewagentoptions) returns. Each behavior returns the change of velocity it wants, as a `Vec2`. The app adds up the behaviors it uses, and `agent:apply` turns the velocity toward the sum within `maxForce` and moves the agent. Targets and neighbors are a `Vec2` or a table `{x, y}` or `{x = 0, y = 0}`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `position` | Vec2 | read and write | The current position. Reading returns a copy. |
| `velocity` | Vec2 | read and write | The current velocity in units per second. Reading returns a copy. |
| `maxSpeed` | number | read and write | The speed limit. |
| `maxForce` | number | read and write | The largest change of velocity per second. |
| `wanderDistance` | number | read and write | How far ahead of the agent the circle of `agent:wander` lies. |
| `wanderRadius` | number | read and write | The radius of that circle. |
| `wanderJitter` | number | read and write | How fast the wander target drifts around the circle, in radians per second. |

```lua
local navigation2d = require('haylen.navigation2d')

local moth = navigation2d.newAgent({wanderJitter = 3})
moth.wanderRadius = 60
moth.wanderJitter = 8
moth.position = {200, 120}
print(moth.wanderDistance, moth.wanderRadius, moth.position.x) -- 60.0 60.0 200.0
```

### agent:seek(target)

Returns the change that turns the velocity toward `target` at full speed.

```lua
local navigation2d = require('haylen.navigation2d')

local hound = navigation2d.newAgent()
hound:apply(hound:seek({300, 0}), 1 / 60)
print(hound.velocity.x > 0) -- true
```

### agent:flee(threat)

Returns the change that turns the velocity away from `threat` at full speed.

```lua
local navigation2d = require('haylen.navigation2d')

local rabbit = navigation2d.newAgent({x = 10, y = 0})
rabbit:apply(rabbit:flee({0, 0}), 1 / 60)
print(rabbit.velocity.x > 0) -- true
```

### agent:arrive(target, slowingRadius)

Returns the change that moves toward `target` and slows down evenly inside `slowingRadius`, so the agent stops on the target.

```lua
local navigation2d = require('haylen.navigation2d')

local car = navigation2d.newAgent()
for _ = 1, 600 do
    car:apply(car:arrive({200, 0}, 80), 1 / 60)
end
print(car.position) -- close to Vec2(200.0, 0.0)
```

### agent:separation(neighbors, radius)

Returns a push away from every position of the list `neighbors` that is closer than `radius`, stronger the closer it is. A neighbor on the exact same spot gives no direction and is ignored.

```lua
local navigation2d = require('haylen.navigation2d')

local sheep = navigation2d.newAgent({x = 0, y = 0})
print(sheep:separation({{10, 0}, {0, 10}}, 30))
```

### agent:alignment(velocities)

Returns the change that turns the velocity toward the average of the list of neighbor `velocities` at full speed, which keeps a flock heading the same way. An empty list returns a zero vector.

```lua
local navigation2d = require('haylen.navigation2d')

local bird = navigation2d.newAgent()
print(bird:alignment({{0, 50}, {10, 40}}), bird:alignment({}))
```

### agent:cohesion(positions)

Returns the change that seeks the center of the list of neighbor `positions`, which keeps a flock together. An empty list returns a zero vector.

```lua
local navigation2d = require('haylen.navigation2d')

local fish = navigation2d.newAgent()
print(fish:cohesion({{100, 0}, {100, 100}}))
```

### agent:avoid(circles, lookAhead)

Returns a sideways swerve around the closest circle that the path ahead of the agent enters within `lookAhead` units, stronger the closer it is. This is the obstacle avoidance of Reynolds. Circles are tables such as `{center = {100, 50}, radius = 20}` or `{{100, 50}, 20}`, and their radius should include the radius of the agent. It returns a zero vector when the way ahead is clear or the agent stands still.

```lua
local navigation2d = require('haylen.navigation2d')

local boat = navigation2d.newAgent()
boat.velocity = {100, 0}
print(boat:avoid({{center = {60, 5}, radius = 20}}, 120))
```

### agent:wander(dt)

Returns the change that follows a target drifting around a circle ahead of the agent, which gives smooth and unpredictable motion. Each agent keeps its own wander state, which moves on by `dt` seconds.

```lua
local navigation2d = require('haylen.navigation2d')
local scene = require('haylen.scene')

local firefly = navigation2d.newAgent({maxSpeed = 40})

scene.push({
    update = function(self, dt)
        firefly:apply(firefly:wander(dt), dt)
    end,
})
```

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
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawCircle(0, -40, 50, '#FF605040')
        graphics2d.drawCircle(leader.position.x, leader.position.y, 12, '#FFFFD040')
        for _, follower in ipairs(followers) do
            graphics2d.drawCircle(follower.position.x, follower.position.y, 8, '#FF60A0FF')
        end
    end,
})
```

## Crowds

### navigation2d.newCrowd(flocking)

Returns a [`Crowd`](#crowd). The table `flocking` is optional. It sets the weights of the flocking forces, which the crowd adds to the velocity that every agent wants, from the neighbors that the agent avoids.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `separation` | number | `0` | Pushes agents apart before they get close. |
| `alignment` | number | `0` | Turns agents toward the heading of their neighbors. |
| `cohesion` | number | `0` | Pulls agents toward the center of their neighbors. |

```lua
local navigation2d = require('haylen.navigation2d')

local sheep = navigation2d.newCrowd({separation = 0.5, alignment = 0.3, cohesion = 0.2})
print(sheep.agentCount, sheep.separation) -- 0 0.5
```

## Crowd

A `Crowd` moves many agents at once with optimal reciprocal collision avoidance, ORCA, which [`navigation2d.newCrowd`](#navigation2dnewcrowdflocking) returns. Every step, each agent takes the velocity closest to the one it wants among the velocities that keep it clear of its neighbors for a time horizon, and clear of obstacles for a shorter one. Each agent of a pair takes half of the effort to avoid the other. Steps are deterministic, allocate nothing once the crowd has grown, and spread over the job system of the engine.

Agents are known by the integer ids that `crowd:addAgent` returns, which count from 0. A removed agent frees its id for a later agent. An unknown id raises an error such as `Agent 7 is not in the crowd.`, and a position, target or velocity that is not finite raises `Crowd positions, targets and velocities must be finite.`

Agents that all want the same spot, or that meet head on in a symmetric formation, can settle in a standoff where every agent keeps its neighbors clear. That is the correct ORCA answer. Break it by giving the targets a small spread, or by adding a little `separation`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `agentCount` | integer | read | The number of agents. |
| `separation` | number | read and write | The weight of the push away from close neighbors. |
| `alignment` | number | read and write | The weight of the turn toward the heading of neighbors. |
| `cohesion` | number | read and write | The weight of the pull toward the center of neighbors. |

### crowd:addAgent(options)

Adds an agent and returns its id. The table `options` is optional.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | `0` | The starting position. |
| `vx`, `vy` | number | `0` | The starting velocity. |
| `radius` | number | `16` | The radius that the agent keeps clear. |
| `maxSpeed` | number | `120` | The speed limit in units per second. |
| `neighborDistance` | number | `160` | How far the agent looks for neighbors to avoid. |
| `maxNeighbors` | integer | `10` | How many of the closest neighbors the agent avoids. |
| `timeHorizon` | number | `2` | How many seconds ahead the agent avoids other agents. Larger values react earlier and move more carefully. |
| `obstacleTimeHorizon` | number | `1` | How many seconds ahead the agent avoids obstacles. |

A negative size, a value that is not finite or a time horizon that is not positive raises `A crowd agent needs a finite position, velocity, radius, speed and neighbor distance and positive time horizons.`

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local scout = crowd:addAgent({x = 10, y = 20, radius = 8, maxSpeed = 90})
print(scout, crowd.agentCount) -- 0 1
```

### crowd:removeAgent(id)

Removes the agent and returns `true`, or returns `false` when it was not in the crowd.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local scout = crowd:addAgent()
print(crowd:removeAgent(scout), crowd:removeAgent(scout), crowd.agentCount) -- true false 0
```

### crowd:hasAgent(id)

Returns `true` when the agent is in the crowd.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local scout = crowd:addAgent()
print(crowd:hasAgent(scout), crowd:hasAgent(99)) -- true false
```

### crowd:addObstacle(polygon)

Adds a solid polygon, in either winding, that agents steer around. A list of two points makes a wall that blocks both sides. Fewer points raise `A crowd obstacle needs at least two points.`, and a point that is not finite raises `A crowd obstacle needs finite points.`

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local runner = crowd:addAgent({x = 0, y = 0, radius = 8})
crowd:addObstacle({{50, -400}, {50, 400}})
crowd:setTarget(runner, 100, 0)
for _ = 1, 30 do
    crowd:step(1 / 30)
end
print(crowd:position(runner).x < 50) -- true
```

### crowd:clearObstacles()

Removes every obstacle.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
crowd:addObstacle({{50, -400}, {50, 400}})
crowd:clearObstacles()
```

### crowd:position(id)

Returns the position of the agent as a `Vec2`.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local guard = crowd:addAgent({x = 30, y = 40})
print(crowd:position(guard)) -- Vec2(30.0, 40.0)
```

### crowd:setPosition(id, x, y)

Moves the agent at once, such as after a teleport.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local guard = crowd:addAgent()
crowd:setPosition(guard, 200, 100)
print(crowd:position(guard).x) -- 200.0
```

### crowd:velocity(id)

Returns the velocity that the agent took in the last step, as a `Vec2`.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local guard = crowd:addAgent({vx = 30})
print(crowd:velocity(guard).x) -- 30.0
```

### crowd:radius(id)

Returns the radius of the agent.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local guard = crowd:addAgent({radius = 12})
print(crowd:radius(guard)) -- 12.0
```

### crowd:maxSpeed(id)

Returns the speed limit of the agent.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local guard = crowd:addAgent({maxSpeed = 60})
print(crowd:maxSpeed(guard)) -- 60.0
```

### crowd:setPreferredVelocity(id, vx, vy)

Sets the velocity that the agent wants, such as the direction of a flow field times its speed. It also drops the target of the agent.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local drone = crowd:addAgent({maxSpeed = 80})
crowd:setTarget(drone, 300, 0)
crowd:setPreferredVelocity(drone, 0, 40)
print(crowd:target(drone), crowd:preferredVelocity(drone).y) -- nil 40.0
```

### crowd:preferredVelocity(id)

Returns the velocity that the agent wants, as a `Vec2`. It is zero while the agent has a target.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local drone = crowd:addAgent()
crowd:setTarget(drone, 300, 0)
print(crowd:preferredVelocity(drone).x) -- 0.0
```

### crowd:setTarget(id, x, y)

Makes the agent head for the point at full speed every step, and slow down to stop on it.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local drone = crowd:addAgent()
crowd:setTarget(drone, 300, 0)
print(crowd:target(drone).x) -- 300.0
```

### crowd:clearTarget(id)

Drops the target of the agent, which then wants to stand still.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local drone = crowd:addAgent()
crowd:setTarget(drone, 0, 0)
crowd:clearTarget(drone)
print(crowd:target(drone)) -- nil
```

### crowd:target(id)

Returns the target of the agent as a `Vec2`, or `nil` when it has none.

```lua
local navigation2d = require('haylen.navigation2d')

local crowd = navigation2d.newCrowd()
local drone = crowd:addAgent()
print(crowd:target(drone)) -- nil
```

### crowd:step(dt)

Picks the new velocity of every agent from the state before the step, then moves them all by `dt` seconds. A time that is not positive and finite raises `A crowd step needs a positive and finite time.`

```lua
local graphics2d = require('haylen.graphics2d')
local navigation2d = require('haylen.navigation2d')
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

## Errors

These messages come from the functions and methods of the module. Messages that start with `bad argument` come from Lua, which adds the position and the name of the function.

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | An options table has a key that it does not take. |
| `The option "<option>" of "<function>" is invalid: <reason>.` | An option has the wrong type or an unknown name, such as `The option "heuristic" of "findPath" is invalid: unknown value 'taxi'.`. |
| `bad argument #5 to 'estimate' (unknown value '<name>')` | The heuristic of `grid:estimate` is not one of the four names. |
| `A navigation grid needs a positive size that fits in 32-bit cell indices.` | The size of `navigation2d.newGrid` is out of range. |
| `Cell <x>,<y> is outside the navigation grid.` | The method `grid:setWalkable`, `grid:setCost` or `grid:cost` got a cell outside the grid. |
| `A navigation cost must be finite and at least 1.` | The cost of `grid:setCost` is out of range. |
| `A search weight must be finite and at least 1.` | The `weight` of a path search is below 1 or not finite. |
| `Hexagonal grids count hex steps and take no heuristic.` | A search on a hexagonal grid got a `heuristic`. |
| `Jump point search needs a square or staggered grid.` | A search with `jumpPoint` ran on a hexagonal grid. |
| `Jump point search needs a grid where every cell costs 1.` | A search with `jumpPoint` ran on a grid with costs. |
| `Grid ray casts need a square grid.` | The method `grid:raycast` ran on another topology. |
| `A Dijkstra map source needs a finite value.` | A source of a Dijkstra map has a `value` that is not finite. |
| `The cell is outside the Dijkstra map.` | The method `map:value` got a cell outside the map. |
| `A flee map needs a finite negative coefficient.` | The coefficient of `map:flee` is not negative and finite. |
| `The cell is outside the flow field.` | A method of a `FlowField` got a cell outside the field. |
| `Hierarchical path finding needs a square grid.` | A hierarchical path finder was made for another topology. |
| `Hierarchical path finding needs clusters of at least 2 cells.` | The `clusterSize` is below 2. |
| `A graph point weight must be finite and at least 1.` | The weight of a graph point is out of range. |
| `Point <id> is not in the graph.` | A method of a `NavGraph` got an unknown id. |
| `A graph point cannot connect to itself.` | The method `graph:connect` got the same point twice. |
| `A navigation mesh polygon needs at least three points.` | A boundary or obstacle has fewer than three points. |
| `A navigation mesh polygon needs finite points.` | A point of a boundary or obstacle is not finite. |
| `Obstacle <id> is not in the navigation mesh.` | The method `mesh:setObstacle` got an unknown id. |
| `A navigation mesh needs a boundary before it builds.` | A mesh without a boundary was built or queried. |
| `An agent radius must be finite and not negative.` | The agent radius of `mesh:findPath` is out of range. |
| `The navigation mesh triangulation lost a constraint.` | The build could not keep a wall in the triangles, which only extreme or broken polygons cause. |
| `The navigation mesh triangulation did not converge.` | The build could not settle the triangles around a wall, which only extreme or broken polygons cause. |
| `A crowd agent needs a finite position, velocity, radius, speed and neighbor distance and positive time horizons.` | An option of `crowd:addAgent` is out of range. |
| `Agent <id> is not in the crowd.` | A method of a `Crowd` got an unknown id. |
| `Crowd positions, targets and velocities must be finite.` | A position, target or velocity given to a `Crowd` is not finite. |
| `A crowd obstacle needs at least two points.` | The polygon of `crowd:addObstacle` has fewer than two points. |
| `A crowd obstacle needs finite points.` | A point of `crowd:addObstacle` is not finite. |
| `A crowd step needs a positive and finite time.` | The time of `crowd:step` is not positive and finite. |
