# haylen.procedural2d

`haylen.procedural2d` generates 2D content: it scatters objects over areas, grows caves, dungeons and mazes, fills maps with Wave Function Collapse, builds Delaunay triangulations and Voronoi diagrams, and picks tiles by their neighbors at run time. Everything is deterministic: the same options and seed always give the same result. Large generations have asynchronous versions that run on the worker threads of the engine and return a promise.

```lua
local procedural = require('haylen.procedural2d')
```

## Seeds, maps and promises

Functions that take options read their generator from a `random` field holding a `Random` of [haylen.math](math.md), which advances as they draw, or seed a new one from a `seed` field, which defaults to `0`. Asynchronous versions copy the generator, so a `random` passed to them does not advance.

Maps are `CellGrid` objects of [haylen.spatial2d](spatial2d.md), so flood fills, connected regions and field of view work on them directly. Cells count from 0, and generators mark walls with `1` and floors with `0`.

Functions whose names end in `Async` take the same options, return a promise at once and resolve it on a later frame. Call `:await()` on the promise inside a coroutine started with `async.spawn()` of Varn's `async` module.

```lua
local procedural = require('haylen.procedural2d')
local async = require('async')

async.spawn(function()
    local cave = procedural.cavesAsync({width = 256, height = 256, seed = 7}):await()
    print(cave.width, cave:get(0, 0)) -- 256 1
end)
```

## Regions

### procedural.region(description)

Creates a `Region`, an area to place things in, from any of these descriptions, which every function that takes a region also accepts.

| Description | Area |
| --- | --- |
| A `Rect`, `{x, y, width, height}` or `{x = 0, y = 0, width = 10, height = 10}` | A rectangle. |
| `{center = {x, y}, radius = r}` | A circle. |
| `{center = {x, y}, radius = r, innerRadius = r2}` | A ring between both radii. |
| `{polygon = shape}` | A polygon given like the shapes of `m.polygon`, holes included. |
| A Tiled object table from [haylen.tiled](tiled.md) | A rectangle, ellipse or polygon object in map pixels, rotation included. Other object shapes raise `Only rectangle, ellipse and polygon objects have an area.` |

Random points are uniform over the area.

```lua
local procedural = require('haylen.procedural2d')
local m = require('haylen.math')

local meadow = procedural.region({polygon = {{0, 0}, {400, 0}, {300, 200}, {0, 250}}})
print(meadow.kind, meadow.area, meadow.bounds)
print(meadow:contains({50, 50}), meadow:randomPoint(m.random(3)))
```

### region.kind, region.area, region.bounds, region:contains(point), region:randomPoint(random)

`kind` is `'rect'`, `'circle'`, `'ring'` or `'polygon'`, `area` the size of the area and `bounds` its bounding `Rect`. `contains` tells whether a point lies inside, and `randomPoint` returns a uniform random point drawn from a `Random`.

```lua
local procedural = require('haylen.procedural2d')
local m = require('haylen.math')

local camp = procedural.region({center = {500, 500}, radius = 200, innerRadius = 120})
local rng = m.random(9)
for i = 1, 5 do
    print(camp:randomPoint(rng))
end
```

## Scattering

### procedural.scatter(options)

Places points over a region, keeps them out of exclusion zones and gives each one a type, and returns a list of `{x, y, type}` tables with types counted from 1. Unknown keys raise `Unknown option '<key>'.`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `region` | region | required | Area to fill. |
| `method` | string | `'random'` | `'random'` places the area times the density in points, `'grid'` places one point per cell of spacing size moved randomly by up to jitter times half the spacing, and `'poisson'` keeps points at least spacing apart. |
| `density` | number | `0.001` | Points per square unit for `'random'`. |
| `spacing` | number | `32` | Cell size for `'grid'` and smallest distance for `'poisson'`. |
| `maximumSpacing` | number | `0` | Largest distance for `'poisson'` where the density map is 0. |
| `jitter` | number | `1` | Share of half the spacing a grid point may move. |
| `attempts` | integer | `30` | Candidates tried around each Poisson point. |
| `densityMap` | function or noise | `nil` | Value from 0 to 1 at a point. `'random'` and `'grid'` keep each point with that probability, and `'poisson'` spaces points from `spacing` where it is 1 to `maximumSpacing` where it is 0. |
| `exclude` | table | `{}` | List of regions that receive no points. |
| `weights` | table | `{}` | Weight of each type, so the type of a point is drawn in proportion to them. |
| `biome` | function or noise | `nil` | Value at a point that picks the layer. |
| `layers` | table | `{}` | List of `{minimum, maximum, weights}`. A point takes the weights of the first layer whose range holds its biome value, and points outside every layer are dropped. |
| `seed`, `random` | | | The generator, as described above. |

A function map is called with the point as a `Vec2`. A noise map is a table `{seed = 0, frequency = 0.01, octaves = 4, gain = 0.5}` of fractal noise, which a density map rescales from -1 to 1 into 0 to 1.

```lua
local procedural = require('haylen.procedural2d')

local forest = procedural.scatter({
    region = {0, 0, 2048, 2048},
    method = 'poisson',
    spacing = 24,
    maximumSpacing = 96,
    densityMap = {seed = 3, frequency = 0.004},
    exclude = {{center = {1024, 1024}, radius = 200}},
    biome = {seed = 11, frequency = 0.002},
    layers = {
        {minimum = -1, maximum = -0.2, weights = {1, 0, 0}},
        {minimum = -0.2, maximum = 1, weights = {2, 3, 1}},
    },
    seed = 42,
})
print(#forest, forest[1].x, forest[1].y, forest[1].type)
```

### procedural.scatterAsync(options)

Scatters on a worker thread and resolves with the list of points. Maps must be noise tables, and a function raises `Asynchronous scattering takes a noise table for densityMap instead of a function.`

```lua
local procedural = require('haylen.procedural2d')
local async = require('async')

async.spawn(function()
    local rocks = procedural.scatterAsync({region = {0, 0, 4096, 4096}, density = 0.0005, seed = 1}):await()
    print(#rocks)
end)
```

## Caves and dungeons

### procedural.caves(options), procedural.cavesAsync(options)

Grows a cave map with a cellular automaton: random walls smoothed by counting the walls around each cell, and returns it as a `CellGrid`. A floor cell turns into a wall with at least `birthLimit` walls among its eight neighbors, and a wall stays a wall with at least `survivalLimit`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `width`, `height` | integer | `64` | Size of the map. |
| `fillChance` | number | `0.45` | Chance of a wall before smoothing. |
| `steps` | integer | `5` | Smoothing steps. |
| `birthLimit`, `survivalLimit` | integer | `5`, `4` | Neighbor counts of the rule. |
| `solidBorder` | boolean | `true` | Keeps the outermost cells as walls and counts cells beyond the map as walls. |

```lua
local procedural = require('haylen.procedural2d')
local spatial = require('haylen.spatial2d')

local cave = procedural.caves({width = 80, height = 50, fillChance = 0.47, seed = 3})
local labels, regions = spatial.components(cave, {background = 1})
print(cave:get(40, 25), regions, labels.width)
```

### procedural.drunkardWalk(options), procedural.drunkardWalkAsync(options)

Carves winding caves with random walkers that start at the center and dig floors until `coverage` of the cells inside the border is open, and returns the `CellGrid`. Walking stops early after `maxSteps` steps. Sides below 3 or no walkers raise `A drunkard walk needs a map of at least 3 by 3 cells and at least one walker.`

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `width`, `height` | integer | `64` | Size of the map. |
| `coverage` | number | `0.4` | Share of the inner cells to open. |
| `walkers` | integer | `1` | Walkers taking turns. |
| `maxSteps` | integer | `100000` | Most steps in total. |

```lua
local procedural = require('haylen.procedural2d')

local tunnels = procedural.drunkardWalk({width = 60, height = 40, coverage = 0.35, walkers = 4, seed = 8})
print(tunnels:get(30, 20)) -- 0
```

### procedural.dungeon(options), procedural.dungeonAsync(options)

Lays out rooms joined by corridors, and every room can reach every other one. `'bsp'` splits the map in two again and again and puts one room in each part, and `'placement'` drops rooms at random free spots and joins them with a minimum spanning tree. Returns `{grid, rooms, connections}`, where rooms are `{x, y, width, height}` tables and connections pairs of room positions from 1. Room sizes that are not positive and ordered, or a map or leaf too small for the smallest room with its padding, raise an error.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `method` | string | `'bsp'` | `'bsp'` or `'placement'`. |
| `width`, `height` | integer | `64`, `48` | Size of the map. |
| `minimumRoomSize`, `maximumRoomSize` | integer | `4`, `10` | Room sides. |
| `minimumLeafSize` | integer | `10` | Parts smaller than twice this stop splitting. |
| `maximumRooms`, `roomAttempts` | integer | `12`, `200` | Limits of the placement method. |
| `padding` | integer | `1` | Wall cells kept around each room. |

```lua
local procedural = require('haylen.procedural2d')

local dungeon = procedural.dungeon({method = 'bsp', width = 96, height = 64, seed = 12})
local start = dungeon.rooms[1]
print(#dungeon.rooms, #dungeon.connections, start.x, start.y)
print(dungeon.grid:get(start.x, start.y)) -- 0
```

## Mazes

### procedural.maze(options), procedural.mazeAsync(options)

Generates a perfect maze, where exactly one path joins any two cells, and returns a `Maze`. `'backtracker'` makes long winding corridors, while `'prim'` and `'kruskal'` make many short dead ends. Options are `{width = 16, height = 16, algorithm = 'backtracker'}` with the generator fields.

```lua
local procedural = require('haylen.procedural2d')

local maze = procedural.maze({width = 20, height = 12, algorithm = 'prim', seed = 5})
print(maze.width, maze.height, maze.passageCount) -- 20 12 239
```

### maze:openings(x, y), maze:open(x, y, side), maze:toGrid()

`openings` returns the sides a cell opens to as a mask of `procedural.north` (1), `procedural.east` (2), `procedural.south` (4) and `procedural.west` (8). `open` opens the wall on one side of a cell and the matching wall of its neighbor, and walls that lead out of the maze raise `Only walls between two cells of the maze can open.` `toGrid` draws the maze as a `CellGrid` of `2 * width + 1` by `2 * height + 1` tiles, with cell `(x, y)` on tile `(2x + 1, 2y + 1)`.

```lua
local procedural = require('haylen.procedural2d')

local maze = procedural.maze({width = 8, height = 8, seed = 1})
local exits = maze:openings(0, 0)
print(exits & procedural.east ~= 0, exits & procedural.south ~= 0)
local tiles = maze:toGrid()
print(tiles.width, tiles:get(1, 1)) -- 17 0
```

## Wave Function Collapse

### procedural.waveFunctionCollapse(options), procedural.waveFunctionCollapseAsync(options)

Fills a map with tiles so that every pair of neighbors follows adjacency rules, the simple tiled model of Wave Function Collapse, and returns a `CellGrid` of tile numbers, or `nil` when every attempt ran into a contradiction. It settles the cell with the fewest choices left, picks one of its tiles by weight and spreads what that rules out, restarting when a cell runs out of tiles.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `sample` | CellGrid | `nil` | Learns the rules and tile frequencies from the neighbors of a sample whose cells hold tile numbers from 0. |
| `periodicSample` | boolean | `false` | Also pairs the sample cells across its opposite edges. |
| `tiles` | integer | | Number of tiles when there is no sample. |
| `allow` | table | `{}` | List of `{first, second, side}` rules: tile `second` may sit on `side` of tile `first`, where side is `'right'`, `'down'`, `'left'` or `'up'`, and the opposite also holds. |
| `weights` | table | | Weight of each tile from tile 0 on. A weight of 0 keeps a tile out. |
| `width`, `height` | integer | `32` | Size of the output. |
| `periodic` | boolean | `false` | Also matches the tiles across opposite edges of the output, so it tiles seamlessly. |
| `attempts` | integer | `10` | Restarts after contradictions. |
| `fixed` | CellGrid | `nil` | Tile each cell must take, or -1 to leave it free, of the size of the output. Another size raises `The fixed cells must match the size of the output.` |

```lua
local procedural = require('haylen.procedural2d')
local spatial = require('haylen.spatial2d')

-- Water (0), sand (1) and grass (2): water never touches grass.
local rules = {}
for _, pair in ipairs({{0, 0}, {1, 1}, {2, 2}, {0, 1}, {1, 0}, {1, 2}, {2, 1}}) do
    rules[#rules + 1] = {pair[1], pair[2], 'right'}
    rules[#rules + 1] = {pair[1], pair[2], 'down'}
end
local fixed = spatial.newCellGrid(48, 32, -1)
fixed:set(24, 16, 2)
local island = procedural.waveFunctionCollapse({tiles = 3, allow = rules, weights = {3, 1, 2}, width = 48, height = 32, fixed = fixed, seed = 9})
print(island and island:get(24, 16)) -- 2
```

## Triangulation and Voronoi

### procedural.delaunay(points)

Returns the Delaunay triangulation of a list of points as `{triangles, halfedges, hull, neighbors}`, built with the sweep hull algorithm of Delaunator. `triangles` lists three point positions from 1 per triangle, wound with a positive `m.polygonArea`. `halfedges` gives, for each edge of `triangles`, the position of the same edge in the neighboring triangle, or 0 on the convex hull. `hull` lists the points on the convex hull in order, and `neighbors` the points joined to each point. No point lies inside the circumcircle of any triangle. Duplicate points stay out, and points on one line give no triangles.

```lua
local procedural = require('haylen.procedural2d')

local stars = {{0, 0}, {100, 10}, {40, 80}, {120, 90}, {60, 30}}
local mesh = procedural.delaunay(stars)
for i = 1, #mesh.triangles, 3 do
    print(mesh.triangles[i], mesh.triangles[i + 1], mesh.triangles[i + 2])
end
print(#mesh.hull, #mesh.neighbors[5])
```

### procedural.voronoi(points, bounds), procedural.voronoiAsync(points, bounds)

Returns the Voronoi cell of every point inside the `Rect` `bounds`, in the order of the points. Cell `i` holds every position closer to point `i` than to any other point, as a convex outline of `Vec2` with a positive area. Duplicate points get empty cells.

```lua
local procedural = require('haylen.procedural2d')
local m = require('haylen.math')

local rng = m.random(6)
local sites = {}
for i = 1, 30 do
    sites[i] = {rng:range(0, 800), rng:range(0, 600)}
end
local provinces = procedural.voronoi(sites, {0, 0, 800, 600})
print(#provinces, m.polygonArea(provinces[1]))
```

### procedural.relax(points, bounds, iterations), procedural.relaxAsync(points, bounds, iterations)

Moves every point to the centroid of its Voronoi cell, `iterations` times, which defaults to `1`, and returns the new points. This Lloyd relaxation evens out the spacing while keeping the points random.

```lua
local procedural = require('haylen.procedural2d')
local m = require('haylen.math')

local rng = m.random(2)
local towns = {}
for i = 1, 20 do
    towns[i] = {rng:range(0, 1000), rng:range(0, 1000)}
end
towns = procedural.relax(towns, {0, 0, 1000, 1000}, 3)
print(towns[1])
```

## Autotiling

Autotiling picks tile variants at run time from the neighbors of each cell, so painted or generated maps get borders and corners without placing them by hand.

### procedural.mask4(grid, x, y, edgesMatch), procedural.mask8(grid, x, y, edgesMatch)

`mask4` returns the sides whose neighbor holds the same value as the cell: north 1, east 2, south 4 and west 8. `mask8` returns the neighbors that hold the same value clockwise from north: north 1, north-east 2, east 4, south-east 8, south 16, south-west 32, west 64 and north-west 128, where a corner only counts when both sides next to it match, which leaves the 47 masks of a blob tile set. Cells beyond the grid match unless `edgesMatch` is `false`. A cell outside the grid raises `the cell is outside the grid`.

```lua
local procedural = require('haylen.procedural2d')
local spatial = require('haylen.spatial2d')

local water = spatial.newCellGrid(3, 3, 1)
water:set(0, 0, 0)
print(procedural.mask4(water, 1, 1), procedural.mask8(water, 1, 1)) -- 15 127
```

### procedural.blobIndex(mask)

Numbers the 47 blob masks from 0 in increasing mask order, and returns -1 for masks that `mask8` never returns. Lay a blob tile set out in that order to index it directly.

```lua
local procedural = require('haylen.procedural2d')

print(procedural.blobIndex(0), procedural.blobIndex(255), procedural.blobIndex(2)) -- 0 46 -1
```

### procedural.autotile4(grid, value, edgesMatch), procedural.autotile8(grid, value, edgesMatch)

Return a `CellGrid` with the 4-bit mask or the blob index of every cell that holds `value`, and -1 for the other cells.

```lua
local procedural = require('haylen.procedural2d')

local cave = procedural.caves({width = 32, height = 32, seed = 4})
local walls = procedural.autotile8(cave, 1)
print(walls:get(0, 0))
```

### procedural.wang(colors, wangSet, seed)

Picks the tiles of a Tiled Wang set and returns a `CellGrid` of tileset tile ids, or -1 where no tile matches. `wangSet` is one of the `wangSets` of a tileset of [haylen.tiled](tiled.md), or a table `{kind, tiles = {{tileId, wangId}}}` alike, and colors are the Wang color numbers of Tiled, from 1, with 0 for no color. Corner and mixed sets read colors at the corners of the cells from a grid one cell wider and taller than the result, where cell `(x, y)` has its corners at `(x, y)`, `(x + 1, y)`, `(x + 1, y + 1)` and `(x, y + 1)`, and mixed sets color an edge when both of its corners agree and accept any color otherwise. Edge sets read one color per cell and color each side whose neighbor shares that color, which suits paths and fences. When several tiles match, a hash of the cell and the seed picks one. Other kinds raise `Wang sets must be of the corner, edge or mixed kind.`

```lua
local procedural = require('haylen.procedural2d')
local spatial = require('haylen.spatial2d')
local tiled = require('haylen.tiled')
local assets = require('haylen.assets')

local map = tiled.newMap(assets.load('maps/island.tmj'))
local terrain = map:tilesets()[1].wangSets[1]
local corners = spatial.newCellGrid(33, 33, 1)
for i = 10, 20 do
    corners:set(i, i, 2)
end
local tiles = procedural.wang(corners, terrain, 7)
print(tiles.width, tiles:get(10, 10))
```
