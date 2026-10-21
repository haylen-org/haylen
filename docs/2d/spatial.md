# 📍 Spatial

This guide shows how to find things in space quickly with the module `haylen.spatial2d`. It finds the entities near a point, under the cursor, inside a box or along a ray. It also finds the cells a unit sees, the rooms a door joins and the hits of a fighting game. It explains what each spatial structure is for, how to choose between them, how the grid algorithms work with maps, and what each one costs. Read it when a game has more than a few dozen things that look for each other. The [`haylen.spatial2d` reference](../lua-api/spatial2d.md) lists every function and method.

## Why spatial structures

A game often asks what is near something. Which enemies does this explosion reach? Which coin does the player touch? Which unit is under the cursor? The simple answer tests every entity. That works for a few dozen entities, but the cost grows fast: 500 bullets against 500 enemies make 250 thousand tests in every frame.

A spatial structure files each entity by where it is. A question then looks only at the entities in that part of space, often a handful, whatever the size of the world. Each structure files entities in its own way, so each suits a different kind of game.

The structures store any Lua value, usually the table of the entity, and their queries return those values. So a query hands back the entities themselves, with no ids to look up.

```lua
local spatial2d = require('haylen.spatial2d')

local enemies = spatial2d.newHashGrid(64)
local slime = {name = 'slime', health = 3}
enemies:set(slime, {100, 100, 32, 32})

for _, enemy in ipairs(enemies:queryCircle(110, 110, 40)) do
    enemy.health = enemy.health - 1
end
print(slime.health) -- 2
```

## Choosing a structure

| Structure | Best for | Moving an entry | Not suited for |
| --- | --- | --- | --- |
| [`HashGrid`](../lua-api/spatial2d.md#hashgrid) | Many entries of a similar size that move every frame, such as enemies and bullets. | Cheap, often only new bounds. | Entries far larger than its cells. |
| [`QuadTree`](../lua-api/spatial2d.md#quadtree) | Entries of very different sizes over an area known ahead, such as the objects of a level. | Moderate. | Worlds without limits. |
| [`AabbTree`](../lua-api/spatial2d.md#aabbtree) | Many moving entries of any size over a world without limits, and many ray casts. | Free inside its margin, moderate outside it. | Nothing in particular. It is a good default. |
| [`KdTree`](../lua-api/spatial2d.md#kdtree) | Points that rarely change, when the nearest queries come most often, such as resources or spawn points. | A rebuild of the whole tree. | Entries that move every frame. |

A game with physics already has these queries in its world of [`haylen.physics2d`](../lua-api/physics2d.md), such as `world:queryPoint` and `world:raycast`. Use a structure of `haylen.spatial2d` for the entities without bodies, such as pickups, sounds, the targets of an ability or thousands of simple enemies that physics would make too costly.

### Spatial hash

A [spatial hash](../lua-api/spatial2d.md#spatial2dnewhashgridcellsize) cuts the world into square cells of one size and files each entry under the cells its rectangle covers. A query visits only the cells it touches. Moving an entry within its cells costs almost nothing, so a hash suits crowds that move every frame.

Pick a cell size about the size of a typical entry, or a little larger. Cells that are too small file each entry many times, and cells that are too large put many entries in each cell. An entry far larger than the cells, such as a boss among bullets, is better in a structure of its own.

```lua
local scene = require('haylen.scene')
local spatial2d = require('haylen.spatial2d')

local bullets = {}
local enemies = {}
local grid = spatial2d.newHashGrid(48)

for index = 1, 200 do
    local enemy = {x = (index % 20) * 60, y = (index // 20) * 60, health = 2}
    enemies[index] = enemy
    grid:set(enemy, {enemy.x - 16, enemy.y - 16, 32, 32})
end

scene.push({
    update = function(self, dt)
        for _, bullet in ipairs(bullets) do
            bullet.x = bullet.x + bullet.vx * dt
            for _, enemy in ipairs(grid:queryPoint(bullet.x, bullet.y)) do
                enemy.health = enemy.health - 1
                if enemy.health <= 0 then
                    grid:remove(enemy)
                end
            end
        end
    end,
})
```

### Quadtree

A [quadtree](../lua-api/spatial2d.md#spatial2dnewquadtreearea-options) starts with one square, its area. When a square holds more than `maxEntries` entries, it splits into four smaller squares, down to `maxDepth` levels. Crowded places get small squares and empty places stay one big square, so it handles entries of very different sizes well. A large entry stays in a large square and a small one goes deep. It needs the area of the level ahead. Entries outside the area still work, but they all stay in the first square, which slows the queries that reach them.

```lua
local spatial2d = require('haylen.spatial2d')

local level = spatial2d.newQuadTree({0, 0, 8192, 8192})
level:set({kind = 'castle'}, {4000, 4000, 900, 700})
level:set({kind = 'flower'}, {120, 300, 8, 8})
print(#level:query({100, 280, 50, 50})) -- 1
```

### AABB tree

An [AABB tree](../lua-api/spatial2d.md#spatial2dnewaabbtreemargin) keeps the entries in a balanced tree of boxes. AABB stands for axis-aligned bounding box, a rectangle that is not rotated. Each box of the tree holds two smaller boxes, and the leaves hold the entries. The tree keeps a box a little larger than each entry, by its `margin`, and moves the entry only when it leaves that box. So small moves cost nothing, and the world needs no limits. Ray casts walk down only the boxes that the ray crosses, which makes the tree good for many rays.

```lua
local spatial2d = require('haylen.spatial2d')

local asteroids = spatial2d.newAabbTree(12)
for index = 1, 300 do
    asteroids:set({id = index}, {index * 37 % 5000, index * 91 % 5000, 40, 40})
end
local hits = asteroids:raycast(0, 2500, 5000, 2500, 1)
print(hits[1] and hits[1].value.id or 'The laser hits nothing.')
```

### k-d tree

A [k-d tree](../lua-api/spatial2d.md#spatial2dnewkdtree) stores points, each with an optional radius. It splits space in two along x, then each half along y, and so on, which makes nearest queries very fast. The tree is built in one pass, so call `kdTree:build` after changes and before the next query. Rebuilding costs a pass over every point, so the tree suits points that change rarely, such as trees to chop, wells or the spawn points of a level.

```lua
local spatial2d = require('haylen.spatial2d')

local wells = spatial2d.newKdTree()
for index = 1, 50 do
    wells:set({id = index, water = 10}, index * 97 % 3000, index * 53 % 3000)
end
wells:build()

local worker = {x = 1500, y = 800}
local well = wells:nearest(worker.x, worker.y, 1000)
print(well and well.id)
```

## Keeping a structure in step

A structure knows only what the app tells it. Store each entity when it appears, call `set` again when it moves, and remove it when it dies. Calling `set` on a value that is already stored moves it, so the same call serves both cases.

```lua
local scene = require('haylen.scene')
local spatial2d = require('haylen.spatial2d')

local fish = {}
local grid = spatial2d.newHashGrid(32)

local function spawn(x, y)
    local one = {x = x, y = y, vx = 30, vy = 10}
    fish[#fish + 1] = one
    grid:set(one, {x - 8, y - 8, 16, 16})
end

for index = 1, 100 do
    spawn(index * 20, index * 7 % 400)
end

scene.push({
    update = function(self, dt)
        for _, one in ipairs(fish) do
            one.x, one.y = one.x + one.vx * dt, one.y + one.vy * dt
            grid:set(one, {one.x - 8, one.y - 8, 16, 16})
        end
    end,
})
```

A structure keeps every stored value alive. An entity that leaves the game but stays in a structure keeps its memory and still shows up in queries. So remove an entity from every structure when it dies, or call `structure:clear` when the level ends.

## Asking questions

Every structure answers the same questions:

| Question | Method |
| --- | --- |
| What lies in this box, such as a selection box or the view of the camera? | [`structure:query`](../lua-api/spatial2d.md#structurequeryrect) |
| What lies within this distance, such as the reach of an explosion? | [`structure:queryCircle`](../lua-api/spatial2d.md#structurequerycirclex-y-radius) |
| What lies at this point, such as under a bullet? | [`structure:queryPoint`](../lua-api/spatial2d.md#structurequerypointx-y) |
| What does this ray cross, and in what order? | [`structure:raycast`](../lua-api/spatial2d.md#structureraycastx1-y1-x2-y2-limit) |
| What is closest, or what is the closest one that fits? | [`structure:nearest`](../lua-api/spatial2d.md#structurenearestx-y-maxdistance-accept) |
| What are the closest few? | [`structure:kNearest`](../lua-api/spatial2d.md#structureknearestx-y-count-maxdistance) |
| What is under the cursor or the finger? | [`structure:pick`](../lua-api/spatial2d.md#structurepickcamera-x-y) |

The answers come in a fixed order. Area, circle and point queries list values in the order they were added, and ray and nearest queries list them by distance. So the same game state gives the same answer on every device and in every replay, which keeps lockstep multiplayer and replays in step.

The function `accept` of `structure:nearest` finds the closest entity that passes a test, without listing every entity. It may also clean up the entities it skips.

```lua
local spatial2d = require('haylen.spatial2d')

local targets = spatial2d.newAabbTree()
targets:set({name = 'Bat', alive = false}, {10, 0, 16, 16})
targets:set({name = 'Wolf', alive = true}, {80, 0, 32, 32})

local tower = {x = 0, y = 0, range = 200}
local target = targets:nearest(tower.x, tower.y, tower.range, function(enemy)
    if not enemy.alive then
        targets:remove(enemy)
        return false
    end
    return true
end)
print(target.name, targets.size) -- Wolf 1
```

## Recipe: picking units under the cursor

A strategy game selects one unit with a click and many units with a box that the player drags. The camera turns screen points into world points, and a structure answers which units lie there. No unit needs a physics body.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local scene = require('haylen.scene')
local spatial2d = require('haylen.spatial2d')

local camera = graphics2d.newCamera()
local army = {}
local units = spatial2d.newHashGrid(64)
for index = 1, 40 do
    local unit = {x = (index % 8) * 80 - 300, y = (index // 8) * 80 - 200, selected = false}
    army[index] = unit
    units:set(unit, {unit.x - 16, unit.y - 16, 32, 32})
end

local function selectOnly(chosen)
    for _, unit in ipairs(army) do
        unit.selected = false
    end
    for _, unit in ipairs(chosen) do
        unit.selected = true
    end
end

scene.push({
    update = function(self, dt)
        if input.mousePressed() then
            self.dragStart = m.vec2(camera:screenToWorld(input.mousePosition()))
        end
        if input.mouseReleased() and self.dragStart then
            local dragEnd = m.vec2(camera:screenToWorld(input.mousePosition()))
            if self.dragStart:distance(dragEnd) < 4 then
                -- A click without a drag selects what lies under the cursor.
                selectOnly(units:pick(camera, input.mousePosition()))
            else
                selectOnly(units:query(m.fromMinMax(self.dragStart:min(dragEnd), self.dragStart:max(dragEnd))))
            end
            self.dragStart = nil
        end
    end,
})
```

The method `structure:pick` takes screen points in design coordinates, like the pointer positions of `haylen.input`, and converts them through the camera. For a touch screen, give it the position of the touch. For a game with physics bodies, [`spatial2d.screenRay`](../lua-api/spatial2d.md#spatial2dscreenraycamera-x-y) turns the cursor into a ray for `world:raycast` of `haylen.physics2d`.

## Grid algorithms

Many 2D games live on a grid of tiles. A [`CellGrid`](../lua-api/spatial2d.md#cellgrid) holds one integer per cell, and the grid algorithms work on it. The generators of [`haylen.procedural2d`](procedural.md) return the same type, with walls as `1` and floors as `0`. Unless an algorithm compares values, `0` is open ground and any other value is solid.

| You want | Use |
| --- | --- |
| The first wall that a shot or a sight line hits, in world units | [`spatial2d.raycastGrid`](../lua-api/spatial2d.md#spatial2draycastgridgrid-from-to-cellsize) |
| Every cell that a laser crosses, such as tiles to highlight or burn | [`spatial2d.traverseGrid`](../lua-api/spatial2d.md#spatial2dtraversegridfrom-to-cellsize) |
| A line or a circle of cells for a tool or an area effect | [`spatial2d.line`](../lua-api/spatial2d.md#spatial2dlinex1-y1-x2-y2) and [`spatial2d.circle`](../lua-api/spatial2d.md#spatial2dcirclex-y-radius) |
| The cells a unit sees, for fog of war and stealth | [`spatial2d.fieldOfView`](../lua-api/spatial2d.md#spatial2dfieldofviewgrid-x-y-radius) |
| The exact shape that a light or a guard sees among wall segments | [`spatial2d.visibilityPolygon`](../lua-api/spatial2d.md#spatial2dvisibilitypolygonorigin-walls-bounds) |
| Everything that a cell reaches, such as the paint bucket or a check that the exit is reachable | [`spatial2d.floodFill`](../lua-api/spatial2d.md#spatial2dfloodfillgrid-x-y-options) |
| Every separate region, such as islands, rooms or pockets of a cave | [`spatial2d.components`](../lua-api/spatial2d.md#spatial2dcomponentsgrid-options) |
| Groups that join over time, such as rooms that doors connect | [`spatial2d.newUnionFind`](../lua-api/spatial2d.md#spatial2dnewunionfindcount) |

The difference between a Bresenham line and a grid traversal matters for gameplay. A Bresenham line takes one cell per step along its longer axis, so it looks like a thin line of pixels, but it can slip between two walls that touch at a corner. A traversal lists every cell that the segment touches, so a shot never passes between those walls.

### Recipe: fog of war

Fog of war hides the parts of the map that the player cannot see. A cell is in one of three states: never seen, seen before, or in view now. This recipe keeps the first two in a `CellGrid` and recomputes the view when the hero moves to another cell.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local spatial2d = require('haylen.spatial2d')

local tile = 32
local walls = spatial2d.newCellGrid(40, 30)
for row = 5, 20 do
    walls:set(18, row, 1)
end

local explored = spatial2d.newCellGrid(40, 30)
local visible = {}
local hero = {x = 10, y = 12, sight = 8}
local camera = graphics2d.newCamera()

local function look()
    visible = {}
    for _, cell in ipairs(spatial2d.fieldOfView(walls, hero.x, hero.y, hero.sight)) do
        visible[cell.y * walls.width + cell.x] = true
        explored:set(cell.x, cell.y, 1)
    end
end

look()

scene.push({
    update = function(self, dt)
        local x, y = hero.x, hero.y
        -- The movement code changes the cell of the hero, and a new cell needs a new view.
        if x ~= self.lastX or y ~= self.lastY then
            self.lastX, self.lastY = x, y
            look()
        end
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        for row = 0, walls.height - 1 do
            for column = 0, walls.width - 1 do
                local rect = {column * tile, row * tile, tile, tile}
                if explored:get(column, row) == 0 then
                    graphics2d.drawRect(rect, '#FF000000')
                elseif not visible[row * walls.width + column] then
                    graphics2d.drawRect(rect, '#99000000')
                end
            end
        end
    end,
})
```

The field of view is symmetric. When the hero sees a guard, the guard sees the hero too, so the same call decides what the player sees and who sees the player. It treats cells outside the grid as walls, and walls themselves count as seen, so the edges of rooms show up. For a large map, draw the fog with a [`TileLayer`](../lua-api/graphics2d.md#tilelayer) of `haylen.graphics2d`, which bakes the cells, instead of a rectangle per cell.

### Recipe: a light that walls block

A visibility polygon is the exact shape that a point sees among wall segments. Draw it as the area of a torch, or test whether a guard sees the player with [`m.polygonContains`](../lua-api/math.md#mpolygoncontainspolygon-point).

```lua
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local scene = require('haylen.scene')
local spatial2d = require('haylen.spatial2d')

local walls = {{{300, 200}, {300, 400}}, {{500, 100}, {600, 100}}, {{450, 350}, {550, 420}}}
local camera = graphics2d.newCamera()
local guard = {x = 400, y = 300}
local player = {x = 620, y = 380}

scene.push({
    update = function(self, dt)
        self.view = spatial2d.visibilityPolygon({guard.x, guard.y}, walls, {0, 0, 800, 600})
        self.spotted = m.polygonContains(self.view, {player.x, player.y})
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawPolygon(self.view, self.spotted and '#60FF4040' or '#40FFE080')
    end,
})
```

The cost of a visibility polygon grows with the square of the walls, as [costs](#costs) explains. Pass only the walls near the light: keep the walls in an `AabbTree` and query the area that the light reaches.

### Recipe: checking a generated level

A generated level must be playable: the exit must be reachable from the start. A flood fill from the start lists every cell the player can walk to.

```lua
local procedural = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')

local cave = procedural.cellularAutomaton({width = 60, height = 40, seed = 5})
local start, exit = {x = 10, y = 20}, {x = 50, y = 20}
cave:set(start.x, start.y, 0)
cave:set(exit.x, exit.y, 0)

local reached = {}
for _, cell in ipairs(spatial2d.floodFill(cave, start.x, start.y)) do
    reached[cell.y * cave.width + cell.x] = true
end
print(reached[exit.y * cave.width + exit.x] and 'The exit is reachable.' or 'Try another seed.')
```

A union-find answers the same question when the level changes over time, such as doors that open one by one. Each room is an element, and each open door unites two rooms. Asking whether two rooms connect then takes nearly constant time, however many doors opened.

```lua
local spatial2d = require('haylen.spatial2d')

local rooms = spatial2d.newUnionFind(5)
local doors = {{1, 2}, {2, 3}, {4, 5}}
for _, door in ipairs(doors) do
    rooms:unite(door[1], door[2])
end
print(rooms:connected(1, 3), rooms:connected(1, 5), rooms.setCount) -- true false 2
```

## Screen picking

The cursor lives on the screen, and the world lives behind a camera that moves, zooms and rotates. Three tools bridge the two:

- The method [`camera:screenToWorld`](../lua-api/graphics2d.md#camerascreentoworldx-y) of `haylen.graphics2d` turns a screen point into a world point.
- The method [`structure:pick`](../lua-api/spatial2d.md#structurepickcamera-x-y) lists the values of a structure under a screen point.
- The function [`spatial2d.screenRay`](../lua-api/spatial2d.md#spatial2dscreenraycamera-x-y) returns the ray from the center of the view to the world point under the cursor. It is ready for any ray cast, such as the aim of a turret in the middle of the screen.

Screen points are in design coordinates, like the positions that `input.mousePosition()` and the touches of [`haylen.input`](../lua-api/input.md) report, so the same code serves mouse and touch.

## Overlap worlds

Fighting games, brawlers and many action games resolve hits by their own rules. A punch lands when its hit box touches the body box of the other fighter, once per swing, and only on the frames of the attack that the animation marks. Physics bodies fit this badly, because the boxes change every frame of the animation and the game wants a list of touches, not forces. An overlap world fits it exactly.

The pieces of an overlap world:

- An entity is one fighter or object. The world gives it an id.
- A set is the group of boxes of one frame of an animation, such as the body box and the hit box of a punch. The app defines each set once with `world:defineSet`.
- Each fixed step, the app places each entity with the set of its current animation frame, its position, its depth and its facing. A fighter that faces left mirrors its boxes. Many entities go in one call with `world:placeAll` and a float buffer.
- Then `world:contacts` lists the pairs of boxes that reach each other.

A box reaches another when its `mask` shares a bit with the `layers` of the other box. The app chooses the bits, such as one bit per team, so the world holds no rule of combat. The `depth` of a box sets how far apart two fighters may stand on the depth axis and still touch, which keeps the fighters of a belt brawler on far rows apart. A rest stops the boxes of one entity from reaching another for a number of steps, so one swing hits once.

```lua
local collections = require('haylen.collections')
local scene = require('haylen.scene')
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld({kinds = {'body', 'hit'}})
local idle = world:defineSet({{kind = 'body', x = -18, y = -78, width = 36, height = 78, layers = 1}})
local punch = world:defineSet({
    {kind = 'body', x = -18, y = -78, width = 36, height = 78, layers = 1},
    {kind = 'hit', x = 12, y = -62, width = 34, height = 14, mask = 1},
})

local hero = {id = world:add(), x = 200, facing = 1, set = punch}
local rival = {id = world:add(), x = 236, facing = -1, set = idle, health = 100}
local contacts = collections.newFloatBuffer(8 * 32)

scene.push({
    fixedUpdate = function(self, step)
        world:place(hero.id, hero.x, 0, 0, hero.facing, hero.set)
        world:place(rival.id, rival.x, 0, 0, rival.facing, rival.set)
        world:step()

        local count = math.min(world:contacts(contacts), 32)
        for index = 0, count - 1 do
            local base = index * 8
            if contacts[base + 1] == hero.id and contacts[base + 3] == rival.id then
                rival.health = rival.health - 10
                -- The punch lands once, then rests for the 20 steps of the swing.
                world:rest(hero.id, rival.id, 20)
            end
        end
    end,
})
```

The contacts come in a fixed order: by the order the entities were added, then by box. The same inputs give the same hits in every replay and on every machine. A float buffer receives the contacts without a table per contact, so a step allocates nothing. For a world of many entities, give `cellSize` to `spatial2d.newOverlapWorld`, and the world finds the candidate pairs through a spatial hash instead of testing every pair. The [overlap world reference](../lua-api/spatial2d.md#overlapworld) lists the boxes, the contacts and their order.

## Costs

The structures work in C++, and a query costs one call across the Lua boundary however many entries it visits. What matters is how the cost grows:

| Structure | Store or move | Query |
| --- | --- | --- |
| `HashGrid` | The cells the entry covers. A move inside the same cells only updates the bounds. | The cells the query covers, and never much more than a visit to every entry. |
| `QuadTree` | The depth of the tree. | The squares the query reaches, about the logarithm of the size for small queries. |
| `AabbTree` | Free inside the margin. Outside it, about the logarithm of the size. | About the logarithm of the size, plus the entries found. |
| `KdTree` | A full rebuild with `kdTree:build`, about n log n. | About the logarithm of the size for nearest queries. |

Every query returns a new Lua list, which allocates. A hot loop that runs many queries per frame, such as one per bullet, does best with point and circle queries that find few entries.

The grid algorithms cost what they touch:

| Algorithm | Grows with |
| --- | --- |
| `spatial2d.fieldOfView` | The cells within the radius, so the square of the radius. |
| `spatial2d.floodFill` and `spatial2d.components` | The cells they visit, up to the whole grid. |
| `spatial2d.raycastGrid`, `spatial2d.traverseGrid` and `spatial2d.line` | The cells along the line. |
| `spatial2d.visibilityPolygon` | The square of the walls, since it casts a few rays at each wall end against every wall. |
| `UnionFind` | Nearly constant per call. |

The command `haylen bench --suite algorithms` times the AABB and k-d tree queries on the machine, as [the bench command](../tools/build.md#bench) describes.

## C++

The C++ types live in `haylen/2d/spatial/` in the namespace `haylen::spatial2d`: `HashGrid`, `QuadTree`, `AabbTree`, `KdTree`, `CellGrid`, `UnionFind`, `OverlapWorld`, `ScreenPicker` and the algorithms `GridRay`, `Bresenham`, `FieldOfView`, `VisibilityPolygon`, `FloodFill` and `ConnectedComponents`.
