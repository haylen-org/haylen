# 🎲 Procedural generation

This guide shows how to build game content from rules instead of by hand with the module `haylen.procedural2d`: forests, caves, dungeons, mazes, tile maps, territories and the borders of tiles. It explains how a seed makes every result repeat, which generator suits which content, how generated maps feed the other modules and how large generations stay off the frame. Read it before you make levels, worlds or loot layouts from code. The [`haylen.procedural2d` reference](../lua-api/procedural2d.md) lists every function and option.

## What procedural generation is

A generator takes a few options and a seed, and it returns content: a list of points, a map of cells or a set of shapes. The same options and the same seed always return the same content, on every device. So a world of any size fits in one number.

This gives a game three things:

- Variety. Each run, each day or each player can get a new level from a new seed.
- Small downloads. The app ships the rules and makes the content on the device.
- Repeatable bugs. A bug report that carries the seed shows the exact level again.

Generators never draw anything. They return data, and the app turns the data into sprites, tiles, physics bodies and paths with the other modules.

## Seeds and repeatable results

Every generator draws its random numbers from a `Random` of [`haylen.math`](../lua-api/math.md#random), a generator of numbers that always repeats the same sequence for the same seed. Pass the seed in the `seed` option, and the function makes its own generator from it.

```lua
local procedural = require('haylen.procedural2d')

local first = procedural.dungeon({seed = 42})
local again = procedural.dungeon({seed = 42})
print(#first.rooms == #again.rooms) -- true
```

A level often runs several generators in a row: the rooms, then the enemies, then the loot. Pass one `Random` to all of them in the `random` option instead of a seed. Each generator advances it, so the next one starts where the last one stopped, and the whole level still repeats from one seed.

```lua
local m = require('haylen.math')
local procedural = require('haylen.procedural2d')

local random = m.random(7)
local dungeon = procedural.dungeon({width = 80, height = 50, random = random})
local room = dungeon.rooms[2]
local enemies = procedural.scatter({region = {room.x, room.y, room.width, room.height}, density = 0.1, random = random})
print(#dungeon.rooms, #enemies)
```

For a daily challenge that is the same for every player, make the seed from the date.

```lua
local procedural = require('haylen.procedural2d')

local today = os.date('!*t')
local seed = today.year * 10000 + today.month * 100 + today.day
local maze = procedural.maze({width = 30, height = 20, seed = seed})
print('The maze of today has ' .. maze.passageCount .. ' passages.')
```

The asynchronous generators work on a copy of the `random` option, so the `Random` that the app passed does not advance. When several asynchronous steps must chain, give each one its own seed, such as `seed = base + 1` and `seed = base + 2`.

## Pick a generator

| You want | Use |
| --- | --- |
| Trees, rocks, loot and spawn points spread naturally over an area | [`procedural.scatter`](../lua-api/procedural2d.md#proceduralscatteroptions), at random, on a shaken grid or with Poisson spacing. |
| Points that never sit closer than a distance, without regions or types | [`m.poissonDisk`](../lua-api/math.md#mpoissondiskoptions) of `haylen.math`. |
| Organic caves and islands | [`procedural.cellularAutomaton`](../lua-api/procedural2d.md#proceduralcellularautomatonoptions). |
| Long winding tunnels and mines | [`procedural.drunkardWalk`](../lua-api/procedural2d.md#proceduraldrunkardwalkoptions). |
| Rooms joined by corridors | [`procedural.dungeon`](../lua-api/procedural2d.md#proceduraldungeonoptions). |
| A labyrinth with exactly one path between any two places | [`procedural.maze`](../lua-api/procedural2d.md#proceduralmazeoptions). |
| Maps and patterns whose tiles follow rules of what may touch what | [`procedural.waveFunctionCollapse`](../lua-api/procedural2d.md#proceduralwavefunctioncollapseoptions). |
| Territories, regions of a map and roads between towns | [`procedural.voronoi`](../lua-api/procedural2d.md#proceduralvoronoipoints-bounds) and [`procedural.delaunay`](../lua-api/procedural2d.md#proceduraldelaunaypoints). |
| Points spread evenly but still random | [`procedural.relax`](../lua-api/procedural2d.md#proceduralrelaxpoints-bounds-iterations). |
| The borders and corners of a painted or generated map | [Autotiling](#autotiling). |
| Smooth height maps, clouds and terrain | The noise of [`haylen.math`](../lua-api/math.md#noise2d), with `noise:sampleGrid` for a whole map in one call. |

The generators combine well. A cave can hold a scatter of crystals, a dungeon can hold a maze in one room, and Voronoi cells can each get a biome of their own.

## Maps are cell grids

The map generators return a `CellGrid` of [`haylen.spatial2d`](../lua-api/spatial2d.md#cellgrid), one integer per cell, with walls as `1` and floors as `0`. The same grid goes into the other modules without a conversion:

- The grid algorithms of `haylen.spatial2d` check and fix it. A [flood fill](../lua-api/spatial2d.md#spatial2dfloodfillgrid-x-y-options) finds what a start cell reaches, and [connected regions](../lua-api/spatial2d.md#spatial2dcomponentsgrid-options) find the pockets that nothing reaches.
- A tile layer of [`haylen.tiled`](../lua-api/tiled.md#mapfilllayer-grid-firstgid-column-row) takes it in one call of `map:fill`, after autotiling picks the tile of each cell.
- A [navigation grid](../lua-api/navigation2d.md#navgrid) takes its walls, so units find paths through the level.
- [Marching squares](../lua-api/math.md#marching-squares) of `haylen.math` trace its walls into outlines for physics bodies and for drawing.

## Recipe: a dungeon with rooms

This recipe lays out a dungeon, puts the start in the first room and the exit in the room that is farthest away by walking. It fills the other rooms with enemies and turns the walls into tiles.

```lua
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local procedural = require('haylen.procedural2d')
local tiled = require('haylen.tiled')

local width, height = 80, 50
local random = m.random(2026)
local dungeon = procedural.dungeon({width = width, height = height, minimumRoomSize = 5, random = random})

-- The walls of the dungeon become the blocked cells of a navigation grid.
local grid = navigation2d.newGrid(width, height)
for y = 0, height - 1 do
    for x = 0, width - 1 do
        grid:setWalkable(x, y, dungeon.grid:get(x, y) == 0)
    end
end

local function center(room)
    return {x = room.x + room.width // 2, y = room.y + room.height // 2}
end

-- A Dijkstra map from the start measures the walk to every room.
local start = center(dungeon.rooms[1])
local walk = grid:dijkstraMap({start})
local exit, farthest = nil, -1
for index, room in ipairs(dungeon.rooms) do
    local spot = center(room)
    local distance = walk:value(spot.x, spot.y)
    if distance > farthest and distance < math.huge then
        exit, farthest = index, distance
    end
end

-- Every room but the first and the exit gets enemies, spread so they never stand on each other.
local enemies = {}
for index, room in ipairs(dungeon.rooms) do
    if index ~= 1 and index ~= exit then
        local spots = procedural.scatter({region = {room.x, room.y, room.width, room.height}, method = 'poisson', spacing = 3, random = random})
        for _, spot in ipairs(spots) do
            enemies[#enemies + 1] = {x = math.floor(spot.x), y = math.floor(spot.y)}
        end
    end
end

-- Each wall takes the tile of the sides it shares with other walls, one of 16 tiles.
local map = tiled.newMapRenderer(tiled.newMap({width = width, height = height, tileWidth = 16, tileHeight = 16}))
local firstWall = map:addTileset({name = 'walls', image = 'tiles/walls.png', tileWidth = 16, tileHeight = 16})
map:addLayer({name = 'walls'})
map:fill('walls', procedural.autotile4(dungeon.grid, 1), firstWall)

print(#dungeon.rooms .. ' rooms, ' .. #enemies .. ' enemies, the exit in room ' .. exit .. '.')
```

The method `'bsp'` gives rooms spread evenly over the whole map. The method `'placement'` drops rooms at random free spots, so some parts of the map stay empty while others get crowded. A room is a plain table of cells, so the app can mark one as a shop, a treasure room or a boss room by its size or its distance from the start.

## Recipe: a cave

A cellular automaton makes caves that look carved by water. It can leave small pockets of floor that no path reaches. This recipe keeps only the largest open region and turns its walls into physics outlines.

```lua
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')

local cave = procedural.cellularAutomaton({width = 120, height = 80, fillChance = 0.46, seed = 9})

-- Count the cells of each open region, then fill every region but the largest with wall.
local labels, count = spatial2d.components(cave, {background = 1})
local sizes = {}
for label = 1, count do
    sizes[label] = 0
end
for y = 0, cave.height - 1 do
    for x = 0, cave.width - 1 do
        local label = labels:get(x, y)
        if label > 0 then
            sizes[label] = sizes[label] + 1
        end
    end
end
local largest = 1
for label = 2, count do
    if sizes[label] > sizes[largest] then
        largest = label
    end
end
for y = 0, cave.height - 1 do
    for x = 0, cave.width - 1 do
        if labels:get(x, y) ~= largest then
            cave:set(x, y, 1)
        end
    end
end

-- The walls become closed outlines of 16 pixel cells, and each outline becomes a chain of a static body.
local pixels = {}
for y = 0, cave.height - 1 do
    for x = 0, cave.width - 1 do
        pixels[#pixels + 1] = cave:get(x, y)
    end
end
local world = physics2d.newWorld()
local rock = world:createBody({type = 'static'})
for _, outline in ipairs(m.marchingSquares.traceBitmap(pixels, cave.width, cave.height, {spacing = 16})) do
    rock:addChain(outline, true)
end
```

The open floor is a hole in the outlines of the walls, so its outline winds the other way, and its chain holds the player inside. Pass an outline through [`m.polygon.simplify`](../lua-api/math.md#mpolygonsimplifypoints-tolerance-closed) first to drop the points of long straight walls.

Tune the cave with a few numbers. A higher `fillChance` gives narrower caves with more walls. More `steps` give smoother walls. A grid painted by hand or cut from noise smooths the same way with [`procedural.cellularAutomatonStep`](../lua-api/procedural2d.md#proceduralcellularautomatonstepgrid-options), one step at a time.

For mines and tunnels instead of open caves, use a drunkard walk. Its tunnels always connect, because every walker digs from the center.

```lua
local procedural = require('haylen.procedural2d')

local mine = procedural.drunkardWalk({width = 100, height = 60, coverage = 0.3, walkers = 3, seed = 4})
print(mine:get(50, 30)) -- 0
```

## Recipe: a forest

A scatter spreads objects over an area and gives each one a type. Poisson spacing keeps trees from growing into each other. A density map, made from noise, thins them into clearings. Exclusions keep them off roads and out of the village.

```lua
local procedural = require('haylen.procedural2d')

local village = {center = {1200, 900}, radius = 260}
local road = {polygon = {{0, 860}, {2400, 860}, {2400, 940}, {0, 940}}}

local forest = procedural.scatter({
    region = {0, 0, 2400, 1800},
    method = 'poisson',
    spacing = 40,
    maximumSpacing = 160,
    densityMap = {seed = 5, frequency = 0.003},
    exclude = {village, road},
    weights = {6, 3, 1},
    seed = 77,
})

local kinds = {'pine', 'oak', 'stump'}
for _, tree in ipairs(forest) do
    tree.kind = kinds[tree.type]
end
print(#forest .. ' trees.')
```

A biome map picks different weights in different places. Give `scatter` a `biome` noise and a list of `layers`, each with a range of the noise and its own weights, such as only pines in the cold north and only palms near the sea. The [scatter reference](../lua-api/procedural2d.md#proceduralscatteroptions) shows the layers.

## Recipe: a maze

A maze generator returns a `Maze`, a grid of cells with walls between them. Draw it as tiles with `maze:toGrid`, which puts each cell and each wall on a tile of its own.

```lua
local procedural = require('haylen.procedural2d')

local maze = procedural.maze({width = 20, height = 15, algorithm = 'backtracker', seed = 31})
local tiles = maze:toGrid()

-- Open the outer wall at the top left and at the bottom right for the entrance and the exit.
tiles:set(1, 0, 0)
tiles:set(tiles.width - 2, tiles.height - 1, 0)
print(tiles.width, tiles.height) -- 41 31
```

The algorithm changes how the maze feels. The `'backtracker'` makes long corridors that wind a lot, which feel hard. The `'prim'` and `'kruskal'` algorithms make many short dead ends, which feel busy. A perfect maze has no loops. To add some, open a few extra walls with `maze:open`.

## Recipe: tiles that follow rules

Wave Function Collapse fills a map with tiles so that every pair of neighbors is allowed. The rules come from a small sample map that an artist paints, or from a list of pairs. A sample teaches both the rules and how often each tile appears.

```lua
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')

-- A sample of grass (0), path (1) and water (2), where paths never touch water.
local rows = {
    {0, 0, 1, 0, 0, 0},
    {0, 0, 1, 1, 1, 0},
    {0, 0, 0, 0, 1, 0},
    {2, 2, 0, 0, 1, 0},
    {2, 2, 2, 0, 0, 0},
}
local sample = spatial2d.newCellGrid(6, 5)
for y, row in ipairs(rows) do
    for x, tile in ipairs(row) do
        sample:set(x - 1, y - 1, tile)
    end
end

local function generate(seed)
    return procedural.waveFunctionCollapse({sample = sample, width = 64, height = 40, attempts = 20, seed = seed})
end

local seed = 1
local land = generate(seed)
while not land and seed < 10 do
    seed = seed + 1
    land = generate(seed)
end
print(land and 'The land is ready.' or 'Every seed ran into a contradiction.')
```

The generator settles the cell with the fewest choices first. Sometimes a cell runs out of tiles, a contradiction, and the generator starts over, up to `attempts` times. It returns `nil` when every attempt failed, so the app tries another seed or loosens the rules. Rules with more allowed pairs fail less often. Use `fixed` to force tiles in some cells, such as a castle in the middle of the map, and `periodic` for a map that repeats without a seam.

To check a map painted by hand, read the rules with [`procedural.waveFunctionCollapseRules`](../lua-api/procedural2d.md#proceduralwavefunctioncollapserulesoptions) and call `rules:valid`. A level editor can then mark the cells that break the rules.

## Recipe: territories and roads

A Voronoi diagram splits an area into cells, one per point, where each cell holds the places closest to its point. It suits the kingdoms of a strategy map or the plates of a world. The Delaunay triangulation of the same points joins each point to its nearest neighbors, which suits roads between towns. Relaxation first spreads the points more evenly, so the cells have similar sizes.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local procedural = require('haylen.procedural2d')
local scene = require('haylen.scene')

local bounds = {0, 0, 1600, 900}
local rng = m.random(12)
local towns = {}
for index = 1, 24 do
    towns[index] = {rng:range(0, 1600), rng:range(0, 900)}
end
towns = procedural.relax(towns, bounds, 2)

local kingdoms = procedural.voronoi(towns, bounds)
local roads = procedural.delaunay(towns)
local camera = graphics2d.newCamera()
local hovered = 1

scene.push({
    update = function(self, dt)
        local x, y = camera:screenToWorld(input.mousePosition())
        -- Starting from the last answer makes the walk short while the cursor moves.
        hovered = roads:findNearest({x, y}, hovered) or hovered
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        for index, kingdom in ipairs(kingdoms) do
            graphics2d.drawPolygon(kingdom, index == hovered and '#FFE0C060' or m.fromHsv(index / #kingdoms, 0.4, 0.8))
        end
        for town, neighbors in ipairs(roads.neighbors) do
            for _, other in ipairs(neighbors) do
                if other > town then
                    local a, b = roads.points[town], roads.points[other]
                    graphics2d.drawLine(a.x, a.y, b.x, b.y, 2, '#FF5A4632')
                end
            end
        end
    end,
})
```

The point that `mesh:findNearest` returns is also the Voronoi cell under the cursor, so a click selects a kingdom without testing every polygon.

## Autotiling

A tile map looks right when each tile matches its neighbors: a wall tile shows an edge where it meets the floor and a corner where two edges meet. Autotiling picks those tiles from the map itself.

- A 4-neighbor mask records which of the four sides share the value of the cell. It has 16 values, so a tile set of 16 tiles covers every case. Use [`procedural.autotile4`](../lua-api/procedural2d.md#proceduralautotile4grid-value-edgesmatch) for walls, fences and pipes.
- An 8-neighbor mask also records the corners. Corners count only when both sides next to them match, which leaves 47 cases, the blob tile set. Use [`procedural.autotile8`](../lua-api/procedural2d.md#proceduralautotile8grid-value-edgesmatch) for terrain with inner and outer corners, such as cliffs and shores.
- A Wang set of Tiled gives each tile colors on its corners or edges. Use [`procedural.autotileWang`](../lua-api/procedural2d.md#proceduralautotilewangcolors-wangset-seed) when the tiles were set up as terrain in the Tiled editor.

Lay the tiles of a 16 tile set out in the order of the mask, and the tiles of a blob set in the order of [`procedural.blobIndex`](../lua-api/procedural2d.md#proceduralblobindexmask). The value of each cell is then the local id of its tile, ready for `map:fill`.

```lua
local procedural = require('haylen.procedural2d')
local tiled = require('haylen.tiled')

local cave = procedural.cellularAutomaton({width = 64, height = 64, seed = 3})
local map = tiled.newMapRenderer(tiled.newMap({width = 64, height = 64, tileWidth = 32, tileHeight = 32}))
local firstGid = map:addTileset({name = 'cliffs', image = 'tiles/cliffs-blob.png', tileWidth = 32, tileHeight = 32})
map:addLayer({name = 'cliffs'})
map:fill('cliffs', procedural.autotile8(cave, 1), firstGid)
```

A map that the player paints while the app runs can call [`map:paintTerrain`](../lua-api/tiled.md#mappaintterrainlayer-wangset-column-row-color) of `haylen.tiled` instead, which updates one cell and its neighbors.

## Off the frame

A frame lasts a few milliseconds, and a large generation can take much longer. Every map generator, the scatter, the Voronoi diagram and the relaxation have a version whose name ends in `Async`. It takes the same options, runs on a worker thread and returns a promise. The frame goes on while it works, so a loading screen keeps moving.

The `load` hook of a scene is the natural place for it. The scene enters once the hook finishes, and a [loading view](../lua-api/scene.md#loading-views) shows the progress meanwhile.

```lua
local procedural = require('haylen.procedural2d')
local scene = require('haylen.scene')

local world = {}

function world:load(context)
    context:progress(0, 'Carving the caves')
    self.caves = procedural.cellularAutomatonAsync({width = 1024, height = 1024, seed = context.params.seed}):await()
    context:progress(0.5, 'Laying out the dungeon')
    self.dungeon = procedural.dungeonAsync({width = 400, height = 300, seed = context.params.seed + 1}):await()
end

function world:enter()
    print('The caves are ' .. self.caves.width .. ' cells wide.')
end

scene.push(world, {params = {seed = 99}})
```

The asynchronous scatter takes noise tables for its maps and refuses Lua functions, because Lua never runs on a worker thread. Generation written in Lua, such as a custom placement pass over every cell, runs as a job of [`haylen.jobs`](../lua-api/jobs.md) instead. A job shares a time budget with the frame and pauses at `jobs.checkpoint()`.

## Costs

Every generator works in C++, so one call costs far less than the same work in Lua. What matters is how the cost grows with the size of the request:

| Generator | Grows with |
| --- | --- |
| Cellular automaton | The cells times the smoothing steps. |
| Drunkard walk | The steps of the walkers, which `coverage` and `maxSteps` bound. |
| Dungeon | The rooms and the attempts to place them. It stays cheap even for large maps. |
| Maze | The cells. |
| Scatter and Poisson disk sampling | The points times the `attempts` of each Poisson point. A density map that Lua computes adds a call per candidate. |
| Wave Function Collapse | The cells times the tiles, more for rules with few allowed pairs, and again for each restart after a contradiction. It is the most expensive generator. |
| Delaunay, Voronoi and relaxation | The points, in about n log n time. Relaxation repeats the Voronoi diagram once per iteration. |
| Autotiling | The cells. |

As a rule, run a generation on the frame when it is small and happens once, such as a room of a puzzle. Use the asynchronous versions for whole worlds, and for Wave Function Collapse on maps larger than a screen. The command `haylen bench --suite procedural` times the generators on the machine, as [the bench command](../tools/build.md#bench) describes.

## C++

The C++ types live in `haylen/2d/procedural/` in the namespace `haylen::procedural2d`: `Region`, `Scatter`, `CellularAutomaton`, `DrunkardWalk`, `Dungeon`, `Maze`, `WaveFunctionCollapse`, `Delaunay`, `Voronoi` and `Autotile`.
