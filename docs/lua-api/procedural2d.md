# 📖 `haylen.procedural2d`

The module `haylen.procedural2d` builds 2D content from rules and a seed. It scatters objects over areas, grows caves, lays out dungeons, carves mazes, fills maps with Wave Function Collapse, builds Delaunay triangulations and Voronoi diagrams, and picks tiles by their neighbors. The same options and the same seed always give the same result. The [procedural generation guide](../2d/procedural.md) explains which generator suits which content, how to build whole levels and how to keep generation off the frame.

```lua
local procedural = require('haylen.procedural2d')
```

## Seeds, maps and promises

Every generator draws its random numbers from a generator. Give it a `Random` of [`haylen.math`](math.md#random) in the `random` field, and it advances as the generator draws. Or give a `seed` field, an integer, and the function makes a new generator from it. The seed is `0` when the options have neither.

The maps that generators return are `CellGrid` values of [`haylen.spatial2d`](spatial2d.md#cellgrid). A `CellGrid` holds one integer per cell, and cells count from 0. Generators write `1` for a wall and `0` for a floor.

Functions whose names end in `Async` take the same options and return a promise at once. The work runs on the worker threads of the engine, and the promise resolves on a later frame. Wait for it with `:await()` inside a coroutine that `async.spawn` of Varn starts. An unknown option or a value of the wrong type raises at once, in the call. A failure of the generator itself, such as a map that is too small, rejects the promise with the same message that the other version raises. An asynchronous generator works on a copy of the `random` field, so the generator that the app passed does not advance.

```lua
local async = require('async')
local procedural = require('haylen.procedural2d')

async.spawn(function()
    local cave = procedural.cellularAutomatonAsync({width = 256, height = 256, seed = 7}):await()
    print(cave.width, cave:get(0, 0)) -- 256 1
end)
```

## Regions

A region is an area to place things in. Every function that takes a region accepts any of these descriptions, or a `Region` value.

| Description | Area |
| --- | --- |
| A `Rect`, `{x, y, width, height}` or `{x = 0, y = 0, width = 10, height = 10}` | A rectangle. |
| `{center = {x, y}, radius = r}` | A circle. |
| `{center = {x, y}, radius = r, innerRadius = r2}` | A ring between the two radii. |
| `{polygon = shape}` | A polygon, given like the shapes of [`m.polygon`](math.md#polygon-operations). Its outlines are united first, and holes stay holes. |
| A table of a Tiled object from [`haylen.tiled`](tiled.md), such as one that `map:objects` returns | The area of a rectangle, ellipse or polygon object in map pixels, with its rotation. |

A circle or ring table accepts only `center`, `radius` and `innerRadius`, and another key raises `Unknown option "<key>".`. An inner radius below 0 or above the radius raises `A ring needs an inner radius between zero and its outer radius.`. A polygon without area raises `A polygon region needs an outline with an area.`. A Tiled object of another shape, such as a point or a polyline, raises `Only rectangle, ellipse and polygon objects have an area.`, and an unknown shape name raises `Unknown Tiled object shape "<shape>".`.

### procedural.region(description)

Returns a `Region` made from a description. Make one when the same area serves many calls, so the area is read once. The polygon of a region is split into triangles when the region is made.

```lua
local m = require('haylen.math')
local procedural = require('haylen.procedural2d')

local meadow = procedural.region({polygon = {{0, 0}, {400, 0}, {300, 200}, {0, 250}}})
print(meadow.kind, meadow.area, meadow.bounds)
print(meadow:contains({50, 50}), meadow:randomPoint(m.random(3)))
```

## Region

A `Region` is an area that [`procedural.region`](#proceduralregiondescription) returns.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `kind` | string | read | The shape: `'rect'`, `'circle'`, `'ring'` or `'polygon'`. A ring with an inner radius of `0` is a `'circle'`. |
| `area` | number | read | The size of the area in square units. |
| `bounds` | Rect | read | The smallest rectangle around the area. |

### region:contains(point)

Returns `true` when `point` lies inside the area. The point is a `Vec2` or a table such as `{x, y}`.

```lua
local procedural = require('haylen.procedural2d')

local camp = procedural.region({center = {500, 500}, radius = 200, innerRadius = 120})
print(camp:contains({500, 500}), camp:contains({650, 500})) -- false true
```

### region:randomPoint(random)

Returns a random point inside the area as a `Vec2`. Every part of the area has the same chance. The argument `random` is a `Random` of [`haylen.math`](math.md#random), which advances.

```lua
local m = require('haylen.math')
local procedural = require('haylen.procedural2d')

local camp = procedural.region({center = {500, 500}, radius = 200, innerRadius = 120})
local rng = m.random(9)
for i = 1, 5 do
    print(camp:randomPoint(rng))
end
```

## Scattering

### procedural.scatter(options)

Places points over a region and gives each one a type. It keeps the points out of the regions of `exclude`. It returns a list of `{x, y, type}` tables, with types counted from 1.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `region` | region | required | The area to fill. |
| `method` | string | `'random'` | How to place the points. The method `'random'` places the area times `density` points at random. The method `'grid'` places one point per cell of `spacing` size, moved at random by up to `jitter` times half the spacing. The method `'poisson'` keeps every point at least `spacing` away from the others. |
| `density` | number | `0.001` | Points per square unit for `'random'`. |
| `spacing` | number | `32` | The cell size for `'grid'`, and the smallest distance between points for `'poisson'`. |
| `maximumSpacing` | number | required with `'poisson'` and a density map | The largest distance for `'poisson'`, used where the density map is 0. It is at least `spacing`. |
| `jitter` | number | `1` | The share of half the spacing that a grid point may move. |
| `attempts` | integer | `30` | The candidates tried around each Poisson point, from `1` to `1000`. Another count raises `Poisson sampling needs between 1 and 1000 attempts.` |
| `densityMap` | function or noise table | none | A value from 0 to 1 at each point. With `'random'` and `'grid'`, each point stays with that chance. With `'poisson'`, the distance between points goes from `spacing` where the map is 1 to `maximumSpacing` where it is 0. |
| `exclude` | list of regions | `{}` | Regions that get no points. |
| `weights` | list of numbers | `{}` | The weight of each type. The type of a point is drawn in proportion to the weights. Without weights, every point has type 1. |
| `biome` | function or noise table | none | A value at each point that picks the layer. |
| `layers` | list | `{}` | A list of `{minimum, maximum, weights}` tables. A point takes the weights of the first layer whose range holds its biome value. A point outside every layer is dropped. Layers need a `biome`. |
| `seed`, `random` | | | The generator, as [seeds, maps and promises](#seeds-maps-and-promises) describes. |

A map is a function or a noise table. A function receives the point as a `Vec2` and returns a number. A noise table is `{seed = 0, frequency = 0.01, octaves = 4, gain = 0.5}`, with those defaults, and it samples the fractal noise of [`noise:fractal`](math.md#noisefractalx-y-octaves-lacunarity-gain) at the point times `frequency`. Noise goes from -1 to 1, and a density map rescales it to 0 to 1, while a biome keeps it as it is.

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

These errors come from wrong options:

- A map of another type raises `The "densityMap" option takes a function or a noise table.`, or the same message that names `biome`.
- A density below 0 or not finite, or a spacing that is not positive and finite, raises `Scattering needs a finite density of at least zero and a finite positive spacing.`
- A `'poisson'` scatter with a density map and without a finite `maximumSpacing` of at least `spacing` raises `Poisson scattering with a density map needs a finite "maximumSpacing" of at least the spacing.`
- Layers without a `biome` raise `Scatter layers need a biome function.`
- A region that would take more than 16777216 points raises `Scattering over this region would need more than 16777216 points, so the density must be lower or the spacing larger.`. A Poisson scatter whose sampling grid would need more cells than that raises `The minimum distance is too small for the area, whose sampling grid would need more than 16777216 cells.`

### procedural.scatterAsync(options)

Scatters like [`procedural.scatter`](#proceduralscatteroptions) on a worker thread and resolves with the list of points. Its maps must be noise tables, because Lua never runs on a worker thread. A function raises `Asynchronous scattering takes a noise table for "densityMap" instead of a function.`, or the same message that names `biome`.

```lua
local async = require('async')
local procedural = require('haylen.procedural2d')

async.spawn(function()
    local rocks = procedural.scatterAsync({region = {0, 0, 4096, 4096}, density = 0.0005, seed = 1}):await()
    print(#rocks)
end)
```

## Caves

### procedural.cellularAutomaton(options)

Grows a cave map and returns it as a `CellGrid`. It fills the map with random walls, then smooths it a few times. Each smoothing step counts the walls among the eight neighbors of every cell. A floor turns into a wall when it has at least `birthLimit` wall neighbors. A wall stays a wall when it has at least `survivalLimit`. The table `options` is optional.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `width`, `height` | integer | `64` | The size of the map in cells. |
| `fillChance` | number | `0.45` | The chance of a wall before smoothing. |
| `steps` | integer | `5` | How many times to smooth. |
| `birthLimit` | integer | `5` | The wall neighbors that turn a floor into a wall. |
| `survivalLimit` | integer | `4` | The wall neighbors that keep a wall. |
| `solidBorder` | boolean | `true` | Keeps the outermost cells as walls and counts cells beyond the map as walls. |
| `seed`, `random` | | | The generator, as [seeds, maps and promises](#seeds-maps-and-promises) describes. |

```lua
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')

local cave = procedural.cellularAutomaton({width = 80, height = 50, fillChance = 0.47, seed = 3})
local labels, regions = spatial2d.components(cave, {background = 1})
print(cave:get(40, 25), regions, labels.width)
```

### procedural.cellularAutomatonAsync(options)

Grows a cave like [`procedural.cellularAutomaton`](#proceduralcellularautomatonoptions) on a worker thread and resolves with the `CellGrid`.

```lua
local async = require('async')
local procedural = require('haylen.procedural2d')

async.spawn(function()
    local cave = procedural.cellularAutomatonAsync({width = 512, height = 512, steps = 6, seed = 21}):await()
    print(cave.width, cave.height)
end)
```

### procedural.cellularAutomatonStep(grid, options)

Runs one smoothing step of the cave rule over any `CellGrid` of walls (`1`) and floors (`0`), such as a painted map or a noise map cut at a threshold. It returns a new smoothed grid and leaves `grid` as it was. The table `options` is optional and takes `birthLimit`, `survivalLimit` and `solidBorder`, with the defaults and meaning of [`procedural.cellularAutomaton`](#proceduralcellularautomatonoptions).

```lua
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')

local painted = spatial2d.newCellGrid(5, 5)
painted:set(2, 2, 1)
local smooth = procedural.cellularAutomatonStep(painted, {solidBorder = false})
print(smooth:get(2, 2), procedural.cellularAutomatonStep(painted):get(0, 0)) -- 0 1
```

### procedural.drunkardWalk(options)

Carves winding tunnels and returns the `CellGrid`. Random walkers start at the center of a map of walls. They take turns to step in a random direction and dig a floor where they stand. They stop once `coverage` of the cells inside the border is floor, or after `maxSteps` steps in total. The outermost cells stay walls. The table `options` is optional.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `width`, `height` | integer | `64` | The size of the map in cells. |
| `coverage` | number | `0.4` | The share of the inner cells to open. |
| `walkers` | integer | `1` | How many walkers take turns. |
| `maxSteps` | integer | `100000` | The most steps of all walkers together. |
| `seed`, `random` | | | The generator, as [seeds, maps and promises](#seeds-maps-and-promises) describes. |

A side below 3 or no walkers raise `A drunkard walk needs a map of at least 3 by 3 cells and at least one walker.`

```lua
local procedural = require('haylen.procedural2d')

local tunnels = procedural.drunkardWalk({width = 60, height = 40, coverage = 0.35, walkers = 4, seed = 8})
print(tunnels:get(30, 20)) -- 0
```

### procedural.drunkardWalkAsync(options)

Carves tunnels like [`procedural.drunkardWalk`](#proceduraldrunkardwalkoptions) on a worker thread and resolves with the `CellGrid`.

```lua
local async = require('async')
local procedural = require('haylen.procedural2d')

async.spawn(function()
    local tunnels = procedural.drunkardWalkAsync({width = 400, height = 300, walkers = 8, seed = 2}):await()
    print(tunnels:get(200, 150)) -- 0
end)
```

## Dungeons

### procedural.dungeon(options)

Lays out rooms and joins them with corridors, so every room can reach every other one. It returns a table with three fields:

- The field `grid` is the `CellGrid` of the map, with walls `1` and floors `0`.
- The field `rooms` is a list of `{x, y, width, height}` tables in cells.
- The field `connections` is a list of `{first, second}` pairs. Each pair holds the positions in `rooms` of the two rooms that a corridor joins, counted from 1.

The method `'bsp'` splits the map in two again and again, puts one room in each part and joins the closest rooms of the two halves of every split. The method `'placement'` drops rooms at random free spots and joins them through the shortest set of corridors between their centers. Each corridor is an L shape between two room centers. The table `options` is optional.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `method` | string | `'bsp'` | `'bsp'` or `'placement'`. |
| `width`, `height` | integer | `64`, `48` | The size of the map in cells. |
| `minimumRoomSize`, `maximumRoomSize` | integer | `4`, `10` | The shortest and longest side of a room. |
| `minimumLeafSize` | integer | `10` | With `'bsp'`, a part smaller than twice this size stops splitting. |
| `maximumRooms` | integer | `12` | With `'placement'`, the most rooms. |
| `roomAttempts` | integer | `200` | With `'placement'`, how many spots to try. |
| `padding` | integer | `1` | The wall cells kept around each room. |
| `seed`, `random` | | | The generator, as [seeds, maps and promises](#seeds-maps-and-promises) describes. |

Room sizes below 1, a maximum below the minimum or a padding below 0 raise `Dungeon rooms need a positive minimum size, a maximum size of at least the minimum and a padding of at least zero.`. A map or a leaf too small for the smallest room with its padding raises `The dungeon and its leaves must fit the smallest room with its padding.`

```lua
local procedural = require('haylen.procedural2d')

local dungeon = procedural.dungeon({method = 'bsp', width = 96, height = 64, seed = 12})
local start = dungeon.rooms[1]
print(#dungeon.rooms, #dungeon.connections, start.x, start.y)
print(dungeon.grid:get(start.x, start.y)) -- 0
```

### procedural.dungeonAsync(options)

Lays out a dungeon like [`procedural.dungeon`](#proceduraldungeonoptions) on a worker thread and resolves with the same table.

```lua
local async = require('async')
local procedural = require('haylen.procedural2d')

async.spawn(function()
    local dungeon = procedural.dungeonAsync({method = 'placement', width = 200, height = 120, maximumRooms = 40, seed = 5}):await()
    print(#dungeon.rooms)
end)
```

## Mazes

### procedural.maze(options)

Carves a perfect maze, where exactly one path joins any two cells, and returns a [`Maze`](#maze). The table `options` is required.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `width`, `height` | integer | `16` | The size of the maze in cells. |
| `algorithm` | string | `'backtracker'` | The algorithm `'backtracker'` makes long winding corridors. The algorithms `'prim'` and `'kruskal'` make many short dead ends. |
| `seed`, `random` | | | The generator, as [seeds, maps and promises](#seeds-maps-and-promises) describes. |

A side below 1, or a maze whose `maze:toGrid` would not fit in 32-bit cell indices, raises `A maze needs at least one cell on each side and a tile grid that fits in 32-bit cell indices.`

```lua
local procedural = require('haylen.procedural2d')

local maze = procedural.maze({width = 20, height = 12, algorithm = 'prim', seed = 5})
print(maze.width, maze.height, maze.passageCount) -- 20 12 239
```

### procedural.mazeAsync(options)

Carves a maze like [`procedural.maze`](#proceduralmazeoptions) on a worker thread and resolves with the `Maze`.

```lua
local async = require('async')
local procedural = require('haylen.procedural2d')

async.spawn(function()
    local maze = procedural.mazeAsync({width = 500, height = 500, algorithm = 'kruskal', seed = 3}):await()
    print(maze.passageCount) -- 249999
end)
```

### procedural.newMaze(width, height)

Returns a `Maze` with every wall closed. Carve it by hand with `maze:open`, or with an algorithm of your own. The sizes follow the rules of [`procedural.maze`](#proceduralmazeoptions).

```lua
local procedural = require('haylen.procedural2d')

local corridor = procedural.newMaze(4, 1)
for x = 0, 2 do
    corridor:open(x, 0, procedural.east)
end
print(corridor.passageCount, corridor:openings(1, 0)) -- 3 10
```

### procedural.north, procedural.east, procedural.south, procedural.west

The sides of a maze cell as bits of a mask: `procedural.north` is `1`, `procedural.east` is `2`, `procedural.south` is `4` and `procedural.west` is `8`. Test a side with the `&` operator of Lua.

```lua
local procedural = require('haylen.procedural2d')

local maze = procedural.maze({width = 8, height = 8, seed = 1})
local exits = maze:openings(0, 0)
print(exits & procedural.east ~= 0, exits & procedural.south ~= 0, exits & procedural.north ~= 0)
```

## Maze

A `Maze` is a grid of cells with walls between them, which [`procedural.maze`](#proceduralmazeoptions) and [`procedural.newMaze`](#proceduralnewmazewidth-height) return. Cells count from 0. A cell outside the maze raises `The cell is outside the maze.`

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | The number of columns. |
| `height` | integer | read | The number of rows. |
| `passageCount` | integer | read | The number of open walls. A perfect maze has one less than its number of cells. |

### maze:openings(x, y)

Returns the sides of a cell that are open, as a mask of [`procedural.north`, `procedural.east`, `procedural.south` and `procedural.west`](#proceduralnorth-proceduraleast-proceduralsouth-proceduralwest).

```lua
local procedural = require('haylen.procedural2d')

local maze = procedural.maze({width = 8, height = 8, seed = 1})
local exits = maze:openings(3, 3)
for _, side in ipairs({'north', 'east', 'south', 'west'}) do
    if exits & procedural[side] ~= 0 then
        print('The cell opens to the ' .. side .. '.')
    end
end
```

### maze:open(x, y, side)

Opens the wall on one side of a cell, and the matching wall of the neighbor on that side. The argument `side` is one of the four side values. A wall on the edge of the maze, or a `side` that is not one side, raises `Only walls between two cells of the maze can open.`

```lua
local procedural = require('haylen.procedural2d')

local room = procedural.newMaze(2, 2)
room:open(0, 0, procedural.south)
print(room:openings(0, 1) == procedural.north) -- true
```

### maze:toGrid()

Draws the maze as a `CellGrid` of tiles, with walls `1` and floors `0`. The grid is `2 * width + 1` tiles wide and `2 * height + 1` tiles high. Cell `(x, y)` sits on tile `(2x + 1, 2y + 1)`, and an open wall becomes a floor tile between two cells. The grid is ready for path finding and for the tiles of a map.

```lua
local procedural = require('haylen.procedural2d')

local maze = procedural.maze({width = 8, height = 8, seed = 1})
local tiles = maze:toGrid()
print(tiles.width, tiles:get(1, 1), tiles:get(0, 0)) -- 17 0 1
```

## Wave Function Collapse

Wave Function Collapse fills a map with tiles so that every pair of neighbors follows adjacency rules. A rule says that one tile may sit on one side of another. This is the simple tiled model of the algorithm.

### procedural.waveFunctionCollapse(options)

Fills a map and returns a `CellGrid` of tile numbers, counted from 0. It returns `nil` when every attempt ran into a contradiction, a cell with no tile left. It settles the cell with the fewest choices left, picks one of its tiles by weight, and spreads what that choice rules out to the other cells. It starts over when a cell runs out of tiles. The table `options` is required.

The rules come from a sample or from a list:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `sample` | CellGrid | none | A small map of tile numbers from 0. The rules and the weights of the tiles come from its pairs of neighbors and how often each tile appears. |
| `periodicSample` | boolean | `false` | Also pairs the cells of the sample across its opposite edges. |
| `tiles` | integer | none | The number of tiles when there is no sample. |
| `allow` | list | `{}` | Without a sample, a list of `{first, second, side}` rules. Each rule lets tile `second` sit on `side` of tile `first`, where `side` is `'right'`, `'down'`, `'left'` or `'up'`. The opposite also holds, so `{0, 1, 'right'}` also lets tile 0 sit left of tile 1. |
| `weights` | list of numbers | `1` per tile, or the counts of the sample | The weight of each tile, from tile 0 on. A weight of 0 keeps a tile out. |

The output takes these keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `width`, `height` | integer | `32` | The size of the output in cells. |
| `periodic` | boolean | `false` | Also matches the tiles across opposite edges of the output, so it repeats without a seam. |
| `attempts` | integer | `10` | How many times to start over after a contradiction. |
| `fixed` | CellGrid | none | A grid of the size of the output with the tile that each cell must take, or `-1` to leave the cell free. |
| `seed`, `random` | | | The generator, as [seeds, maps and promises](#seeds-maps-and-promises) describes. |

```lua
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')

-- Water (0), sand (1) and grass (2), where water never touches grass.
local rules = {}
for _, pair in ipairs({{0, 0}, {1, 1}, {2, 2}, {0, 1}, {1, 0}, {1, 2}, {2, 1}}) do
    rules[#rules + 1] = {pair[1], pair[2], 'right'}
    rules[#rules + 1] = {pair[1], pair[2], 'down'}
end
local fixed = spatial2d.newCellGrid(48, 32, -1)
fixed:set(24, 16, 2)
local island = procedural.waveFunctionCollapse({tiles = 3, allow = rules, weights = {3, 1, 2}, width = 48, height = 32, fixed = fixed, seed = 9})
print(island and island:get(24, 16)) -- 2
```

These errors come from wrong rules or output keys:

- No sample and no `tiles`, or `tiles = 0`, raise `Wave function collapse needs at least one tile.`
- A rule or a weight for a tile beyond the tile count raises `The rules have no tile <tile>.`
- A negative weight raises `Tile weights cannot be negative.`
- A sample with a negative cell raises `Every cell of a sample must hold a tile number of at least 0.`
- A side of the output below 1 raises `Wave function collapse needs at least one cell on each side.`
- A `fixed` grid of another size raises `The fixed cells must match the size of the output.`

### procedural.waveFunctionCollapseAsync(options)

Fills a map like [`procedural.waveFunctionCollapse`](#proceduralwavefunctioncollapseoptions) on a worker thread. It resolves with the `CellGrid`, or with `nil` after a contradiction in every attempt.

```lua
local async = require('async')
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')

local sample = spatial2d.newCellGrid(6, 6)
for x = 0, 5 do
    sample:set(x, 5, 1)
end

async.spawn(function()
    local tiles = procedural.waveFunctionCollapseAsync({sample = sample, width = 128, height = 64, seed = 4}):await()
    print(tiles and tiles.width or 'Every attempt failed.')
end)
```

### procedural.waveFunctionCollapseRules(options)

Reads the rule keys of [`procedural.waveFunctionCollapse`](#proceduralwavefunctioncollapseoptions), which are `sample`, `periodicSample`, `tiles`, `allow` and `weights`, and returns a [`WaveFunctionCollapseRules`](#wavefunctioncollapserules). Use it to see what a sample taught, or to check a map painted by hand against the rules. Any other key raises `Unknown option "<key>".`

```lua
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')

local sample = spatial2d.newCellGrid(4, 1)
for x = 0, 3 do
    sample:set(x, 0, x % 2)
end
local rules = procedural.waveFunctionCollapseRules({sample = sample, periodicSample = true})
print(rules.tileCount, rules:weight(1)) -- 2 2.0
```

## WaveFunctionCollapseRules

A `WaveFunctionCollapseRules` holds the tiles, their weights and which tile may sit beside which. The function [`procedural.waveFunctionCollapseRules`](#proceduralwavefunctioncollapserulesoptions) returns one.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `tileCount` | integer | read | The number of tiles. |

### rules:allowed(first, second, side)

Returns `true` when tile `second` may sit on `side` of tile `first`, where `side` is `'right'`, `'down'`, `'left'` or `'up'`. A tile the rules do not have returns `false`.

```lua
local procedural = require('haylen.procedural2d')

local rules = procedural.waveFunctionCollapseRules({tiles = 2, allow = {{0, 1, 'right'}}})
print(rules:allowed(0, 1, 'right'), rules:allowed(1, 0, 'left'), rules:allowed(0, 0, 'right')) -- true true false
```

### rules:weight(tile)

Returns the weight of a tile, or `0` for a tile the rules do not have.

```lua
local procedural = require('haylen.procedural2d')

local rules = procedural.waveFunctionCollapseRules({tiles = 3, weights = {5, 1, 0}})
print(rules:weight(0), rules:weight(2), rules:weight(7)) -- 5.0 0.0 0.0
```

### rules:valid(grid, periodic)

Returns `true` when every pair of neighbors of the `CellGrid` follows the rules. With `periodic` set to `true`, it also checks the pairs across opposite edges. A cell that holds no tile of the rules makes it `false`.

```lua
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')

local sample = spatial2d.newCellGrid(4, 1)
for x = 0, 3 do
    sample:set(x, 0, x % 2)
end
local rules = procedural.waveFunctionCollapseRules({sample = sample, periodicSample = true})

local painted = spatial2d.newCellGrid(6, 2)
for x = 0, 5 do
    painted:set(x, 0, x % 2)
    painted:set(x, 1, x % 2)
end
print(rules:valid(painted), rules:valid(sample, true)) -- true true
```

## Triangulation and Voronoi

### procedural.delaunay(points)

Returns the Delaunay triangulation of a list of points as a [`Delaunay`](#delaunay). In a Delaunay triangulation, no point lies inside the circle through the corners of any triangle, so the triangles are as wide as they can be. It is built with a sweep hull in O(n log n) time. Duplicate points stay out of the triangles, and points that all lie on one line give no triangles. A point that is not finite raises `A Delaunay triangulation needs finite points.`

```lua
local procedural = require('haylen.procedural2d')

local stars = {{0, 0}, {100, 10}, {40, 80}, {120, 90}, {60, 30}}
local mesh = procedural.delaunay(stars)
print(mesh.triangleCount, #mesh.hull)
```

### procedural.voronoi(points, bounds)

Returns the Voronoi cell of every point inside the rectangle `bounds`, in the order of the points. Cell `i` holds every position that is closer to point `i` than to any other point. Each cell is a convex outline, a list of `Vec2` with a positive area. A duplicate point gets an empty cell. To find the cell that holds a position, use [`mesh:findNearest`](#meshfindnearestposition-start) on the triangulation of the same points. A point that is not finite raises `A Delaunay triangulation needs finite points.`, and bounds that are not finite raise `A Voronoi diagram needs a finite area.`

```lua
local m = require('haylen.math')
local procedural = require('haylen.procedural2d')

local rng = m.random(6)
local sites = {}
for i = 1, 30 do
    sites[i] = {rng:range(0, 800), rng:range(0, 600)}
end
local provinces = procedural.voronoi(sites, {0, 0, 800, 600})
print(#provinces, m.polygonSignedArea(provinces[1]))
```

### procedural.voronoiAsync(points, bounds)

Builds the cells like [`procedural.voronoi`](#proceduralvoronoipoints-bounds) on a worker thread and resolves with the list of cells. It reports wrong points and bounds by rejecting its promise.

```lua
local async = require('async')
local m = require('haylen.math')
local procedural = require('haylen.procedural2d')

local rng = m.random(1)
local sites = {}
for i = 1, 5000 do
    sites[i] = {rng:range(0, 4096), rng:range(0, 4096)}
end

async.spawn(function()
    local cells = procedural.voronoiAsync(sites, {0, 0, 4096, 4096}):await()
    print(#cells)
end)
```

### procedural.relax(points, bounds, iterations)

Moves every point to the center of its Voronoi cell inside `bounds`, `iterations` times, `1` by default, and returns the new points. This is Lloyd relaxation. It evens out the spacing of random points while they still look random.

```lua
local m = require('haylen.math')
local procedural = require('haylen.procedural2d')

local rng = m.random(2)
local towns = {}
for i = 1, 20 do
    towns[i] = {rng:range(0, 1000), rng:range(0, 1000)}
end
towns = procedural.relax(towns, {0, 0, 1000, 1000}, 3)
print(towns[1])
```

### procedural.relaxAsync(points, bounds, iterations)

Relaxes the points like [`procedural.relax`](#proceduralrelaxpoints-bounds-iterations) on a worker thread and resolves with the new points.

```lua
local async = require('async')
local m = require('haylen.math')
local procedural = require('haylen.procedural2d')

local rng = m.random(4)
local seeds = {}
for i = 1, 2000 do
    seeds[i] = {rng:range(0, 2048), rng:range(0, 2048)}
end

async.spawn(function()
    local even = procedural.relaxAsync(seeds, {0, 0, 2048, 2048}, 4):await()
    print(#even)
end)
```

## Delaunay

A `Delaunay` is a triangulation that [`procedural.delaunay`](#proceduraldelaunaypoints) returns. Its lists count points, triangles and edges from 1. Each list is built the first time it is read, and later reads return the same table, so a loop may read them freely. Do not change these tables.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `points` | list of Vec2 | read | The points of the triangulation. |
| `triangles` | list of integers | read | Three point positions per triangle, wound so that [`m.polygonSignedArea`](math.md#mpolygonsignedareapolygon) is positive. Triangle `t` uses the entries `3t - 2`, `3t - 1` and `3t`. |
| `halfedges` | list of integers | read | For each entry of `triangles`, the position of the same edge in the neighboring triangle, or `0` on the convex hull. Edge `e` runs from point `triangles[e]` to the next point of its triangle. |
| `hull` | list of integers | read | The points on the convex hull in order. When all points lie on one line, every distinct point along the line. |
| `neighbors` | list of lists | read | For each point, the points that an edge joins to it, in increasing order. |
| `triangleCount` | integer | read | The number of triangles. |

```lua
local procedural = require('haylen.procedural2d')

local mesh = procedural.delaunay({{0, 0}, {100, 10}, {40, 80}, {120, 90}, {60, 30}})
local triangles = mesh.triangles
for i = 1, #triangles, 3 do
    print(triangles[i], triangles[i + 1], triangles[i + 2])
end
print(#mesh.points, #mesh.halfedges, #mesh.neighbors[5])
```

### mesh:circumcenter(triangle)

Returns the center of the circle through the three corners of a triangle, counted from 1, as a `Vec2`. That is where the Voronoi cells of its three corners meet. A triangle outside `1` to `triangleCount` raises `bad argument #1 to 'circumcenter' (the triangle is outside the triangulation)`.

```lua
local procedural = require('haylen.procedural2d')

local mesh = procedural.delaunay({{100, 100}, {400, 120}, {250, 380}, {600, 300}})
for triangle = 1, mesh.triangleCount do
    print(mesh:circumcenter(triangle))
end
```

### mesh:findNearest(position, start)

Returns the position of the point nearest to `position`, counted from 1. That point is also the Voronoi cell that holds `position`. It returns `nil` for a triangulation without points. It walks along the edges, in about the square root of the point count in steps. It starts from the point `start` when one is given, so a start near the answer, such as the last answer for a moving position, makes it faster. A duplicate point given as `start` returns itself. A start outside the points raises `bad argument #2 to 'findNearest' (the start is outside the points)`.

```lua
local procedural = require('haylen.procedural2d')

local towns = {{100, 100}, {400, 120}, {250, 380}, {600, 300}}
local mesh = procedural.delaunay(towns)
local region = mesh:findNearest({380, 150})
print(region, mesh:findNearest({590, 310}, region)) -- 2 4
```

## Autotiling

Autotiling picks the tile of each cell from its neighbors, so painted or generated maps get borders and corners without placing them by hand.

### procedural.mask4(grid, x, y, edgesMatch)

Returns the sides of a cell whose neighbor holds the same value, as a mask: north `1`, east `2`, south `4` and west `8`. Cells beyond the grid match, unless `edgesMatch` is `false`. A cell outside the grid raises `bad argument #2 to 'mask4' (the cell is outside the grid)`.

```lua
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')

local water = spatial2d.newCellGrid(3, 3, 1)
water:set(1, 0, 0)
print(procedural.mask4(water, 1, 1), procedural.mask4(water, 0, 0, false)) -- 14 4
```

### procedural.mask8(grid, x, y, edgesMatch)

Returns the neighbors of a cell that hold the same value, clockwise from north: north `1`, north-east `2`, east `4`, south-east `8`, south `16`, south-west `32`, west `64` and north-west `128`. A corner counts only when both sides next to it match too. That leaves the 47 masks of a blob tile set. Cells beyond the grid match, unless `edgesMatch` is `false`. A cell outside the grid raises `bad argument #2 to 'mask8' (the cell is outside the grid)`.

```lua
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')

local water = spatial2d.newCellGrid(3, 3, 1)
water:set(0, 0, 0)
print(procedural.mask4(water, 1, 1), procedural.mask8(water, 1, 1)) -- 15 127
```

### procedural.blobIndex(mask)

Numbers the 47 masks of [`procedural.mask8`](#proceduralmask8grid-x-y-edgesmatch) from 0, in increasing order of the mask. It returns `-1` for a mask that `mask8` never returns. Lay a blob tile set out in this order to find a tile by its index.

```lua
local procedural = require('haylen.procedural2d')

print(procedural.blobIndex(0), procedural.blobIndex(255), procedural.blobIndex(2)) -- 0 46 -1
```

### procedural.autotile4(grid, value, edgesMatch)

Returns a new `CellGrid` of the size of `grid`. Each cell that holds `value` gets its [`procedural.mask4`](#proceduralmask4grid-x-y-edgesmatch), and every other cell gets `-1`.

```lua
local procedural = require('haylen.procedural2d')

local maze = procedural.maze({width = 10, height = 10, seed = 6}):toGrid()
local walls = procedural.autotile4(maze, 1)
print(walls:get(0, 0), walls:get(1, 1)) -- 15 -1
```

### procedural.autotile8(grid, value, edgesMatch)

Returns a new `CellGrid` of the size of `grid`. Each cell that holds `value` gets the [`procedural.blobIndex`](#proceduralblobindexmask) of its mask, and every other cell gets `-1`.

```lua
local procedural = require('haylen.procedural2d')

local cave = procedural.cellularAutomaton({width = 32, height = 32, seed = 4})
local walls = procedural.autotile8(cave, 1)
print(walls:get(0, 0))
```

### procedural.autotileWang(colors, wangSet, seed)

Picks the tiles of a Wang set of Tiled and returns a `CellGrid` of the tile ids of the tileset, or `-1` where no tile matches. A Wang set gives each tile a color on its corners, its edges or both, and the tiles of a map match where their colors meet.

- The argument `colors` is a `CellGrid` of the Wang color numbers of Tiled, from 1, with 0 for no color.
- The argument `wangSet` is one of the `wangSets` of a tileset of [`haylen.tiled`](tiled.md#maptilesets), or a table like it: `{kind = 'corner', tiles = {{tileId = 0, wangId = {0, 1, 0, 1, 0, 1, 0, 1}}}}`. A Wang id lists 8 colors, clockwise from the top edge.
- The integer `seed`, `0` by default, picks among several tiles that match. A hash of the cell and the seed makes the choice, so a cell keeps its tile.

A `'corner'` or `'mixed'` set reads the colors at the corners of the cells from `colors`, which is one cell wider and taller than the result. Cell `(x, y)` has its corners at `(x, y)`, `(x + 1, y)`, `(x + 1, y + 1)` and `(x, y + 1)`. A mixed set colors an edge when both of its corners agree, and accepts any color on it otherwise. An `'edge'` set reads one color per cell and colors each side whose neighbor shares that color, which suits paths and fences. Another kind raises `Wang sets must be of the "corner", "edge" or "mixed" kind.`

```lua
local assets = require('haylen.assets')
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')
local tiled = require('haylen.tiled')

local map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
local terrain = map:tilesets()[1].wangSets[1]
local corners = spatial2d.newCellGrid(33, 33, 1)
for i = 10, 20 do
    corners:set(i, i, 2)
end
local tiles = procedural.autotileWang(corners, terrain, 7)
print(tiles.width, tiles:get(10, 10))
```

## Errors

These messages come from the functions and methods of the module. Messages that start with `bad argument` come from Lua, which adds the position and the name of the function.

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | An options table, a circle region or a noise table has a key that it does not take. |
| `The option "<option>" of "<function>" is invalid: <reason>.` | An option has the wrong type or an unknown name, such as `The option "method" of "dungeon" is invalid: unknown value 'cave'.`. |
| `A ring needs an inner radius between zero and its outer radius.` | The `innerRadius` of a region is below 0 or above its `radius`. |
| `A polygon region needs an outline with an area.` | The `polygon` of a region covers no area. |
| `Only rectangle, ellipse and polygon objects have an area.` | A Tiled object of another shape was given as a region. |
| `Unknown Tiled object shape "<shape>".` | The `shape` of an object table is not a shape of Tiled. |
| `The "densityMap" option takes a function or a noise table.` | A map of a scatter is another type. The same message names `biome`. |
| `Asynchronous scattering takes a noise table for "densityMap" instead of a function.` | A map of `procedural.scatterAsync` is a function. The same message names `biome`. |
| `Scattering needs a finite density of at least zero and a finite positive spacing.` | The `density` or the `spacing` of a scatter is out of range. |
| `Poisson scattering with a density map needs a finite "maximumSpacing" of at least the spacing.` | A Poisson scatter with a density map has no valid `maximumSpacing`. |
| `Poisson sampling needs between 1 and 1000 attempts.` | The `attempts` of a scatter are out of range. |
| `Scatter layers need a biome function.` | A scatter has `layers` but no `biome`. |
| `Scattering over this region would need more than 16777216 points, so the density must be lower or the spacing larger.` | A scatter would place too many points. |
| `The minimum distance is too small for the area, whose sampling grid would need more than 16777216 cells.` | A Poisson scatter would need too many cells. |
| `A drunkard walk needs a map of at least 3 by 3 cells and at least one walker.` | A side of a drunkard walk is below 3, or `walkers` is below 1. |
| `Dungeon rooms need a positive minimum size, a maximum size of at least the minimum and a padding of at least zero.` | The room sizes or the padding of a dungeon are out of range. |
| `The dungeon and its leaves must fit the smallest room with its padding.` | The map or `minimumLeafSize` is too small for one room. |
| `A maze needs at least one cell on each side and a tile grid that fits in 32-bit cell indices.` | A side of a maze is below 1, or the maze is too large. |
| `The cell is outside the maze.` | A method of a maze got a cell outside it. |
| `Only walls between two cells of the maze can open.` | The method `maze:open` got a wall on the edge or a value that is not one side. |
| `Wave function collapse needs at least one tile.` | The rules have no sample and no tiles. |
| `The rules have no tile <tile>.` | A rule or a weight names a tile beyond the tile count. |
| `Tile weights cannot be negative.` | A weight of the rules is negative. |
| `Every cell of a sample must hold a tile number of at least 0.` | A sample holds a negative cell. |
| `Wave function collapse needs at least one cell on each side.` | A side of the output is below 1. |
| `The fixed cells must match the size of the output.` | The `fixed` grid has another size. |
| `A Delaunay triangulation needs finite points.` | A point of a triangulation, a Voronoi diagram or a relaxation is not finite. |
| `A Voronoi diagram needs a finite area.` | The bounds of a Voronoi diagram or a relaxation are not finite. |
| `bad argument #1 to 'circumcenter' (the triangle is outside the triangulation)` | The triangle of `mesh:circumcenter` is out of range. |
| `bad argument #2 to 'findNearest' (the start is outside the points)` | The start of `mesh:findNearest` is out of range. |
| `bad argument #2 to 'mask4' (the cell is outside the grid)` | The cell of `procedural.mask4` or `procedural.mask8` is outside the grid. |
| `Wang sets must be of the "corner", "edge" or "mixed" kind.` | The `kind` of a Wang set is another value. |
