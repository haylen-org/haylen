# haylen.spatial2d

`haylen.spatial2d` finds things in space quickly. It offers four structures that store Lua values by their bounds or positions and answer area, circle, point, ray and nearest-neighbor queries without testing every entity: a spatial hash, a quadtree, a dynamic AABB tree and a k-d tree. It also offers the classic grid algorithms of 2D games on a grid of integer cells: ray casts and walks across cells, Bresenham lines and circles, a symmetric field of view, visibility polygons, flood fills, connected regions and union-find. Screen picking turns the cursor or a touch into world points, rays and the values under them.

```lua
local spatial2d = require('haylen.spatial2d')
```

## Choosing a structure

| Structure | Best for |
| --- | --- |
| `SpatialHash` | Many entries of similar size that move every frame, such as enemies and bullets. |
| `QuadTree` | Entries of very different sizes spread over a known area, such as the objects of a level. |
| `AabbTree` | Many moving entries of any size over an unbounded area. Small moves inside the enlarged box of an entry cost nothing. |
| `KdTree` | Points that rarely change, when nearest-neighbor queries dominate, such as resources or spawn points. |

## Functions

### spatial2d.newHash(cellSize)

Creates a `SpatialHash` whose grid cells are `cellSize` units wide and high. A cell size about the size of a typical entity, or a little larger, keeps queries fast. A size that is not positive and finite raises `A spatial hash needs a positive cell size.`

A stored rectangle may cover at most 65536 cells, or `set` raises `A spatial hash entry may cover at most 65536 cells, so the cell size should be closer to the size of the entries.`, and it must lie within 536870912 cells of the origin, or `set` raises `A spatial hash entry must lie within 536870912 cells of the origin.` A query never costs much more than a visit to every stored value, so an area far larger than the stored values, or a point or a ray far away from them, stays cheap.

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHash(64)
print(hash.cellSize)
```

### spatial2d.newQuadTree(area, options)

Creates a `QuadTree` over the rectangle `area`. Entries outside the area still work and stay in the root quadrant. `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `maxEntries` | integer | `8` | How many entries a quadrant holds before it splits in four. |
| `maxDepth` | integer | `8` | How many times quadrants may split, from 0 to 16. |

An empty area, a `maxEntries` of 0 or a `maxDepth` outside 0 to 16 raises `A quadtree needs an area of positive size, room for an entry per quadrant and a depth from 0 to 16.`

```lua
local spatial2d = require('haylen.spatial2d')

local tree = spatial2d.newQuadTree({0, 0, 4096, 4096}, {maxEntries = 4, maxDepth = 6})
print(tree.area.width, tree.nodeCount)
```

### spatial2d.newAabbTree(margin)

Creates an `AabbTree` that enlarges the box it keeps for each entry by `margin` units on every side, 4 by default. Larger margins make small moves free, and smaller ones make queries tighter.

```lua
local spatial2d = require('haylen.spatial2d')

local tree = spatial2d.newAabbTree(8)
print(tree.margin, tree.height)
```

### spatial2d.newKdTree()

Creates an empty `KdTree`.

```lua
local spatial2d = require('haylen.spatial2d')

local trees = spatial2d.newKdTree()
trees:set({kind = 'oak'}, 120, 80)
trees:build()
```

### spatial2d.newCellGrid(width, height, value)

Creates a `CellGrid` of `width` columns and `height` rows whose cells all hold the integer `value`, 0 by default. Grid algorithms treat 0 as open ground and any other value as solid, unless they compare values.

```lua
local spatial2d = require('haylen.spatial2d')

local dungeon = spatial2d.newCellGrid(40, 30)
for column = 0, 39 do
    dungeon:set(column, 0, 1)
end
```

### spatial2d.newUnionFind(count)

Creates a `UnionFind` of `count` elements, 0 by default, numbered from 1, each in a set of its own.

```lua
local spatial2d = require('haylen.spatial2d')

local sets = spatial2d.newUnionFind(10)
sets:unite(1, 2)
print(sets:connected(2, 1), sets.setCount)
```

### spatial2d.raycastGrid(grid, from, to, cellSize)

Walks the cells of a `CellGrid` that the ray from `from` to `to` crosses, where each cell is `cellSize` units wide, given as a number or a `{width, height}` pair, and the first cell sits at the origin. Returns the hit on the first solid cell, a hit table like those of [haylen.math](math.md#ray-casts) with the extra fields `column` and `row`, or `nil`. The ray only travels over the grid, and a ray that starts in a solid cell hits it at distance 0. A cell size that is not positive and finite raises `Grid cells need a positive and finite size.`

```lua
local spatial2d = require('haylen.spatial2d')

local walls = spatial2d.newCellGrid(20, 15)
walls:set(8, 3, 1)
local hit = spatial2d.raycastGrid(walls, {40, 56}, {600, 56}, 16)
print(hit.column, hit.row, hit.x, hit.normalX)
```

### spatial2d.traverseGrid(from, to, cellSize)

Returns every cell that the segment from `from` to `to` crosses on an endless grid of cells `cellSize` units wide, given as a number or a `{width, height}` pair, with the first cell at the origin. The cells come in order from the start as `{x = column, y = row, distance}` tables, where `distance` is how far along the segment it enters the cell, 0 for the first one. Unlike `spatial2d.line`, it lists every cell the segment touches, which suits lasers, bullets and tile highlights. A segment through the exact corner of four cells steps along x first. A cell size that is not positive and finite raises `Grid cells need a positive and finite size.`, a segment beyond the 32-bit range of cells raises `A grid ray must stay within the 32-bit range of cells.`, and a segment across more than 65536 cells raises `A grid traversal lists at most 65536 cells.`

```lua
local spatial2d = require('haylen.spatial2d')

for _, cell in ipairs(spatial2d.traverseGrid({5, 5}, {35, 17}, 10)) do
    print(cell.x, cell.y, cell.distance)
end
```

### spatial2d.line(x1, y1, x2, y2)

Returns the cells of the Bresenham line from one cell to another, both included, as a list of `{x = column, y = row}` tables in order.

```lua
local spatial2d = require('haylen.spatial2d')

for _, cell in ipairs(spatial2d.line(0, 0, 5, 2)) do
    print(cell.x, cell.y)
end
```

### spatial2d.circle(x, y, radius)

Returns the outline of a circle of cells with the midpoint algorithm, each cell once, sorted by row and then by column. A radius of 0 gives the center alone.

```lua
local spatial2d = require('haylen.spatial2d')

local ring = spatial2d.circle(10, 10, 4)
print(#ring)
```

### spatial2d.fieldOfView(grid, x, y, radius)

Returns the cells of the grid visible from the cell `x`, `y` within `radius` cells, measured between cell centers, with the symmetric shadowcasting of Albert Ford. Solid cells block sight and are visible themselves, and cells outside the grid count as solid. Sight is symmetric: when an open cell sees another, the other sees it back, which keeps stealth fair in roguelikes.

```lua
local spatial2d = require('haylen.spatial2d')

local map = spatial2d.newCellGrid(30, 20)
map:set(12, 10, 1)
local seen = {}
for _, cell in ipairs(spatial2d.fieldOfView(map, 10, 10, 8)) do
    seen[cell.y * 30 + cell.x] = true
end
print(seen[10 * 30 + 11], seen[10 * 30 + 14])
```

### spatial2d.visibilityPolygon(origin, walls, bounds)

Returns the outline of the area visible from `origin` among the wall segments, clipped to the rectangle `bounds`, as a list of `Vec2` in order of increasing angle. It is the shape of a 2D light or of what a guard sees. Walls and bounds that are not finite raise `A visibility polygon needs finite walls and bounds.`, and an origin outside the bounds, or one that is not a number, raises `A visibility polygon needs an origin inside its bounds.`

```lua
local spatial2d = require('haylen.spatial2d')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local walls = {{{300, 200}, {300, 400}}, {{500, 100}, {600, 100}}}
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        local light = spatial2d.visibilityPolygon({400, 300}, walls, {0, 0, 800, 600})
        graphics2d.drawPolygon(light, '#40FFE080')
    end,
})
```

### spatial2d.floodFill(grid, x, y, options)

Returns the cells connected to the cell `x`, `y` that hold its value, starting with it, in breadth-first order, like the paint bucket of an image editor. A start outside the grid returns an empty list. `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `diagonal` | boolean | `false` | Also connects cells that only share a corner. |
| `value` | integer | none | Paints the filled cells with this value. |

```lua
local spatial2d = require('haylen.spatial2d')

local canvas = spatial2d.newCellGrid(16, 16)
for row = 0, 15 do
    canvas:set(8, row, 1)
end
local left = spatial2d.floodFill(canvas, 0, 0, {value = 2})
print(#left, canvas:get(3, 3), canvas:get(12, 3))
```

### spatial2d.components(grid, options)

Splits the grid into regions of connected cells that share a value, such as the islands of a map or the rooms of a dungeon. Returns a new `CellGrid` with the region number of every cell, numbered from 1 in the order their first cell appears row by row, and the number of regions. `options` is optional:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `diagonal` | boolean | `false` | Also joins cells that only share a corner. |
| `background` | integer | none | Cells with this value form no region and get 0. |

```lua
local spatial2d = require('haylen.spatial2d')

local sea = spatial2d.newCellGrid(12, 8)
sea:set(1, 1, 1)
sea:set(2, 1, 1)
sea:set(8, 5, 1)
local islands, count = spatial2d.components(sea, {background = 0})
print(count, islands:get(2, 1), islands:get(8, 5))
```

### spatial2d.screenRay(camera, x, y)

Returns the ray from the world point at the center of the view of `camera` to the world point under the screen point `x`, `y`, as `x1, y1, x2, y2`, ready for any ray cast. Screen points are in design coordinates, like the pointer positions of [haylen.input](input.md), and the camera draws a world canvas. To turn a screen point into a world point, use `camera:screenToWorld(x, y)` from [haylen.graphics2d](graphics2d.md).

```lua
local spatial2d = require('haylen.spatial2d')
local graphics2d = require('haylen.graphics2d')
local physics2d = require('haylen.physics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local world = physics2d.newWorld({gravity = {0, 0}})
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        local x1, y1, x2, y2 = spatial2d.screenRay(camera, input.mousePosition())
        local hit = world:raycast(x1, y1, x2, y2)
        self.aim = hit and {hit.x, hit.y} or {x2, y2}
    end,
})
```

## Structures

The four structures store any non-nil Lua value, usually the entity table itself, and compare values by identity, the way table keys do. A structure keeps a reference to every stored value until it is removed or the structure is cleared. Queries return new lists of values. `query`, `queryCircle`, `queryPoint` and `pick` list them in the order the values were added, so results never depend on how entries moved, while `raycast` and `kNearest` list them by distance, with ties going to the value added first. A value removed and stored again counts as added again. Bounds that touch count as overlapping, so zero-sized rectangles work as points.

`SpatialHash`, `QuadTree` and `AabbTree` store rectangles, which accept a `Rect` or a table `{x, y, width, height}` or `{x = 0, y = 0, width = 0, height = 0}`. A rectangle with a negative size or a non-finite component raises `Spatial bounds must be finite and have a non-negative size.` The `KdTree` stores points with an optional radius and treats them as circles in every query.

### structure:set(value, rect)

Stores `value` with the bounds `rect`, or moves it when it is already stored. A `nil` value raises `a value to store is required`. `KdTree` takes `set(value, x, y, radius)` instead, with a radius of 0 by default.

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHash(64)
local goblin = {name = 'goblin', x = 100, y = 80}
hash:set(goblin, {goblin.x - 16, goblin.y - 16, 32, 32})

goblin.x = goblin.x + 5
hash:set(goblin, {goblin.x - 16, goblin.y - 16, 32, 32})
```

### structure:remove(value)

Removes `value` and returns true, or returns false when it was not stored.

### structure:has(value)

Returns true when `value` is stored.

### structure:bounds(value)

Returns the `Rect` stored for `value`, or `nil` when it is not stored. `KdTree` has `position(value)` instead, which returns its point as a `Vec2`.

```lua
local spatial2d = require('haylen.spatial2d')

local tree = spatial2d.newAabbTree()
local chest = {name = 'chest'}
tree:set(chest, {200, 120, 24, 20})
print(tree:has(chest), tree:bounds(chest).width)
print(tree:remove(chest), tree:remove(chest), tree:bounds(chest))
```

### structure:query(rect)

Returns the list of values whose bounds overlap or touch `rect`.

```lua
local spatial2d = require('haylen.spatial2d')

local tree = spatial2d.newQuadTree({0, 0, 1000, 1000})
local units = {{name = 'archer', x = 40, y = 40}, {name = 'lancer', x = 300, y = 90}, {name = 'monk', x = 900, y = 600}}
for _, unit in ipairs(units) do
    tree:set(unit, {unit.x - 20, unit.y - 20, 40, 40})
end

for _, unit in ipairs(tree:query({0, 0, 400, 200})) do
    print(unit.name .. ' is inside the selection box.')
end
```

### structure:queryCircle(x, y, radius)

Returns the list of values whose bounds come within `radius` of the point `x`, `y`. A negative radius raises `A spatial query radius must not be negative.`

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHash(64)
local barrel = {name = 'barrel'}
hash:set(barrel, {120, 100, 32, 32})

for _, target in ipairs(hash:queryCircle(100, 100, 50)) do
    print(target.name .. ' takes explosion damage.')
end
```

### structure:queryPoint(x, y)

Returns the list of values whose bounds contain or touch the point `x`, `y`.

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHash(64)
hash:set({name = 'button'}, {100, 100, 200, 60})
print(hash:queryPoint(150, 120)[1].name)
```

### structure:raycast(x1, y1, x2, y2, limit)

Casts a ray from `x1`, `y1` to `x2`, `y2` against the stored bounds and returns the hits from the start of the ray on, each a hit table like those of [haylen.math](math.md#ray-casts) with the stored value in `value`. The optional `limit` keeps the first hits only, so a limit of 1 finds the closest value and larger limits pierce through several.

```lua
local spatial2d = require('haylen.spatial2d')

local tree = spatial2d.newAabbTree()
for index = 1, 5 do
    tree:set({name = 'crate ' .. index}, {index * 100, -20, 40, 40})
end

for _, hit in ipairs(tree:raycast(0, 0, 1000, 0, 3)) do
    print(hit.value.name, hit.x, hit.distance)
end
```

### structure:nearest(x, y, maxDistance, accept)

Returns the value whose bounds lie closest to the point `x`, `y`, within `maxDistance`, or `nil` when none is close enough. When `accept` is given, it is called with the candidate values from the closest one on, until it returns a true value, and the ones for which it returns a false value are skipped. It may remove values from the structure or clear it, such as dead entities it cleans up, and a removed value is not offered anymore. Ties go to the value added first. Errors raised by `accept` propagate out of the call. A point that is not finite raises `A spatial query point must be finite.`, here and in `kNearest`.

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHash(64)
local tree = {kind = 'tree', wood = 0}
local goldMine = {kind = 'gold', gold = 50}
hash:set(tree, {40, 0, 32, 32})
hash:set(goldMine, {200, 0, 64, 64})

local closest = hash:nearest(0, 0, 500)
local mine = hash:nearest(0, 0, 500, function(value) return value.kind == 'gold' end)
print(closest.kind, mine.kind)
```

### structure:kNearest(x, y, count, maxDistance)

Returns up to `count` values whose bounds lie closest to the point `x`, `y`, closest first, within the optional `maxDistance`.

```lua
local spatial2d = require('haylen.spatial2d')

local wells = spatial2d.newKdTree()
for index = 1, 20 do
    wells:set({id = index}, index * 50, (index % 4) * 70)
end
wells:build()

for _, well in ipairs(wells:kNearest(300, 100, 3)) do
    print('well ' .. well.id)
end
```

### structure:pick(camera, x, y)

Returns the values whose bounds contain the world point under the screen point `x`, `y`, seen through `camera`. Screen points are in design coordinates, like pointer positions.

```lua
local spatial2d = require('haylen.spatial2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local units = spatial2d.newHash(64)
units:set({name = 'knight'}, {-20, -20, 40, 40})
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        if input.mousePressed() then
            for _, unit in ipairs(units:pick(camera, input.mousePosition())) do
                print('selected ' .. unit.name)
            end
        end
    end,
})
```

### structure:clear()

Removes every value.

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHash(64)
hash:set('marker', {0, 0, 0, 0})
hash:clear()
print(hash.size)
```

### Structure properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | Number of stored values, on every structure. |
| `cellSize` | number | read | The cell size of a `SpatialHash`. |
| `area` | Rect | read | The area of a `QuadTree`. |
| `nodeCount` | integer | read | The quadrants a `QuadTree` uses now, including the root. |
| `margin` | number | read | The margin of an `AabbTree`. |
| `height` | integer | read | The levels below the root of an `AabbTree`, which balancing keeps near the logarithm of its size. |
| `built` | boolean | read | False when a `KdTree` changed since its last `build`. |

### kdTree:build()

Rebuilds a `KdTree` from its current points. Queries on a tree that changed since its last build raise `The k-d tree changed since it was last built, so it needs build before a query.`

```lua
local spatial2d = require('haylen.spatial2d')

local stars = spatial2d.newKdTree()
stars:set('sun', 0, 0, 10)
stars:set('comet', 300, 40)
print(stars.built)
stars:build()
print(stars.built, stars:kNearest(290, 40, 1)[1], stars:position('sun').x)
```

## CellGrid

A `CellGrid` holds one integer per cell. Cells are addressed by column and row, counting from 0 like Tiled cells. `set` and `get` raise an error such as `Cell 9,9 is outside the cell grid.` for cells outside the grid.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | Number of columns. |
| `height` | integer | read | Number of rows. |

### grid:get(x, y)

Returns the value of a cell.

### grid:set(x, y, value)

Sets the value of a cell.

### grid:fill(value)

Sets every cell to `value`.

### grid:contains(x, y)

Returns true when the cell is inside the grid.

```lua
local spatial2d = require('haylen.spatial2d')

local grid = spatial2d.newCellGrid(10, 8)
grid:fill(1)
grid:set(4, 4, 0)
print(grid.width, grid.height, grid:get(4, 4), grid:get(0, 0), grid:contains(10, 0))
```

## UnionFind

A `UnionFind` keeps disjoint sets of the elements 1 to `size`, with union by size and path halving, so every call runs in nearly constant time. Elements below 1 raise `union-find elements count from 1`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | Number of elements. |
| `setCount` | integer | read | Number of disjoint sets. |

### sets:add()

Adds an element in a set of its own and returns it.

### sets:find(element)

Returns the representative element of the set that holds `element`.

### sets:unite(first, second)

Merges the sets of both elements and returns false when they already shared one.

### sets:connected(first, second)

Returns true when both elements share a set.

### sets:setSize(element)

Returns the number of elements in the set of `element`.

```lua
local spatial2d = require('haylen.spatial2d')

local rooms = spatial2d.newUnionFind(6)
rooms:unite(1, 2)
rooms:unite(2, 3)
local extra = rooms:add()
rooms:unite(extra, 6)
print(rooms:connected(1, 3), rooms:setSize(3), rooms:find(6) == rooms:find(extra), rooms.setCount)
```

### sets:reset(count)

Starts over with `count` elements, each in a set of its own, reusing the memory of the union-find instead of creating a new one.

```lua
local spatial2d = require('haylen.spatial2d')

local islands = spatial2d.newUnionFind(100)
islands:unite(1, 2)
islands:reset(50)
print(islands.size, islands.setCount, islands:connected(1, 2)) -- 50 50 false
```
