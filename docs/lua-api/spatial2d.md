# 📖 `haylen.spatial2d`

The module `haylen.spatial2d` finds things in space without testing every entity. It has four spatial structures that store Lua values by their bounds or positions and answer area, circle, point, ray and nearest queries. It has the grid algorithms of 2D games on a `CellGrid`: ray casts, Bresenham lines and circles, field of view, visibility polygons, flood fills, connected regions and union-find. It turns the cursor into world rays for screen picking, and its overlap worlds find the boxes that reach each other for games that resolve their own hits. The [spatial guide](../2d/spatial.md) explains which structure to choose, how the grid algorithms fit with maps, and when an overlap world replaces physics.

```lua
local spatial2d = require('haylen.spatial2d')
```

## Structures

The four structures are [`HashGrid`](#hashgrid), [`QuadTree`](#quadtree), [`AabbTree`](#aabbtree) and [`KdTree`](#kdtree). They work the same way:

- A structure stores any Lua value except `nil`, usually the table of an entity. Queries return the values themselves.
- Values are compared by identity, the way table keys are. A structure keeps every stored value alive until it is removed or the structure is cleared.
- Every query returns a new list. The methods `query`, `queryCircle`, `queryPoint` and `pick` list the values in the order they were added, so results never depend on how the entries moved. The methods `raycast` and `kNearest` list them by distance, and a tie goes to the value added first. A value that is removed and stored again counts as added again.
- Bounds that touch count as overlapping, so a rectangle of size zero works as a point.

The structures `HashGrid`, `QuadTree` and `AabbTree` store rectangles. A rectangle is a `Rect`, a table `{x, y, width, height}` or a table `{x = 0, y = 0, width = 0, height = 0}`. A rectangle with a negative size or a component that is not finite raises `Spatial bounds must be finite and have a non-negative size.` A `KdTree` stores points with an optional radius instead, and its queries treat each point as a circle.

### spatial2d.newHashGrid(cellSize)

Returns a [`HashGrid`](#hashgrid), a spatial hash whose cells are `cellSize` units wide and high. A spatial hash files every entry under the cells that its rectangle covers. A cell size about the size of a typical entity, or a little larger, keeps queries fast. A size that is not positive and finite raises `A spatial hash needs a positive cell size.`

A stored rectangle covers at most 65536 cells, or `hash:set` raises `A spatial hash entry may cover at most 65536 cells, so the cell size should be closer to the size of the entries.`. It lies within 536870912 cells of the origin, or `hash:set` raises `A spatial hash entry must lie within 536870912 cells of the origin.`. A query never costs much more than a visit to every stored value, so a huge area, or a point or a ray far from every entry, stays cheap.

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHashGrid(64)
print(hash.cellSize) -- 64.0
```

### spatial2d.newQuadTree(area, options)

Returns a [`QuadTree`](#quadtree) over the rectangle `area`. A quadtree splits a crowded square of its area into four smaller squares, again and again. Entries outside the area still work, and they stay in the root square. The table `options` is optional.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `maxEntries` | integer | `8` | How many entries a square holds before it splits in four. |
| `maxDepth` | integer | `8` | How many times squares may split, from 0 to 16. |

An empty area, a `maxEntries` of 0 or a `maxDepth` outside 0 to 16 raises `A quadtree needs an area of positive size, room for an entry per quadrant and a depth from 0 to 16.`

```lua
local spatial2d = require('haylen.spatial2d')

local tree = spatial2d.newQuadTree({0, 0, 4096, 4096}, {maxEntries = 4, maxDepth = 6})
print(tree.area.width, tree.nodeCount) -- 4096.0 1
```

### spatial2d.newAabbTree(margin)

Returns an [`AabbTree`](#aabbtree), a dynamic tree of bounding boxes. It keeps a box around each entry that is `margin` units larger on every side, `4` by default, and it moves an entry in the tree only when the entry leaves that box. Larger margins make more small moves free, and smaller ones make queries tighter. A margin that is negative or not finite raises `An AABB tree needs a finite margin of zero or more.`

```lua
local spatial2d = require('haylen.spatial2d')

local tree = spatial2d.newAabbTree(8)
print(tree.margin, tree.height) -- 8.0 0
```

### spatial2d.newKdTree()

Returns an empty [`KdTree`](#kdtree), a tree of points that splits space along x and y in turn. It answers nearest queries fastest, and it is rebuilt with `kdTree:build` after its points change.

```lua
local spatial2d = require('haylen.spatial2d')

local trees = spatial2d.newKdTree()
trees:set({kind = 'oak'}, 120, 80)
trees:build()
print(trees.size, trees.built) -- 1 true
```

## Structure methods

These methods exist on every structure, except where a method names the structures it belongs to. The examples call the structure `structure`, or the name of the kind they use.

### structure:set(value, rect)

Stores `value` with the bounds `rect` in a `HashGrid`, a `QuadTree` or an `AabbTree`. When the value is already stored, it moves to the new bounds. A `nil` value raises `bad argument #1 to 'set' (a value to store is required)`. A `KdTree` takes a point instead, as [`kdTree:set`](#kdtreesetvalue-x-y-radius) describes.

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHashGrid(64)
local goblin = {name = 'goblin', x = 100, y = 80}
hash:set(goblin, {goblin.x - 16, goblin.y - 16, 32, 32})

goblin.x = goblin.x + 5
hash:set(goblin, {goblin.x - 16, goblin.y - 16, 32, 32})
```

### structure:remove(value)

Removes `value` and returns `true`, or returns `false` when it was not stored.

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHashGrid(64)
local coin = {name = 'coin'}
hash:set(coin, {10, 10, 8, 8})
print(hash:remove(coin), hash:remove(coin)) -- true false
```

### structure:has(value)

Returns `true` when `value` is stored.

```lua
local spatial2d = require('haylen.spatial2d')

local tree = spatial2d.newAabbTree()
local chest = {name = 'chest'}
tree:set(chest, {200, 120, 24, 20})
print(tree:has(chest), tree:has({name = 'chest'})) -- true false
```

### structure:bounds(value)

Returns the `Rect` stored for `value` in a `HashGrid`, a `QuadTree` or an `AabbTree`, or `nil` when the value is not stored. A `KdTree` has [`kdTree:point`](#kdtreepointvalue) instead.

```lua
local spatial2d = require('haylen.spatial2d')

local tree = spatial2d.newAabbTree()
local chest = {name = 'chest'}
tree:set(chest, {200, 120, 24, 20})
print(tree:bounds(chest).width) -- 24.0
tree:remove(chest)
print(tree:bounds(chest)) -- nil
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

local hash = spatial2d.newHashGrid(64)
hash:set({name = 'barrel'}, {120, 100, 32, 32})

for _, target in ipairs(hash:queryCircle(100, 100, 50)) do
    print('The ' .. target.name .. ' takes explosion damage.')
end
```

### structure:queryPoint(x, y)

Returns the list of values whose bounds contain or touch the point `x`, `y`.

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHashGrid(64)
hash:set({name = 'button'}, {100, 100, 200, 60})
print(hash:queryPoint(150, 120)[1].name) -- button
```

### structure:raycast(x1, y1, x2, y2, limit)

Casts a ray from `x1`, `y1` to `x2`, `y2` against the stored bounds. It returns the hits in order from the start of the ray. Each hit is a table like the hits of [`haylen.math`](math.md#ray-casts), with the stored value in its `value` field. The optional `limit` keeps only the first hits: a limit of 1 finds the closest value, and a larger limit pierces through several.

```lua
local spatial2d = require('haylen.spatial2d')

local tree = spatial2d.newAabbTree()
for index = 1, 5 do
    tree:set({name = 'Crate ' .. index}, {index * 100, -20, 40, 40})
end

for _, hit in ipairs(tree:raycast(0, 0, 1000, 0, 3)) do
    print(hit.value.name, hit.x, hit.distance)
end
```

### structure:nearest(x, y, maxDistance, accept)

Returns the value whose bounds lie closest to the point `x`, `y`, within `maxDistance`, or `nil` when none is that close. A tie goes to the value added first.

The function `accept` is optional. When it is given, the structure calls it with the candidates from the closest one on, until it returns a true value, and it skips the candidates for which it returns a false value. It may remove values from the structure or clear it, such as dead entities it cleans up, and a removed value is not offered again. An error raised in `accept` leaves the call.

A point that is not finite raises `A spatial query point must be finite.`, here and in `structure:kNearest`.

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHashGrid(64)
local tree = {kind = 'tree', wood = 0}
local goldMine = {kind = 'gold', gold = 50}
hash:set(tree, {40, 0, 32, 32})
hash:set(goldMine, {200, 0, 64, 64})

local closest = hash:nearest(0, 0, 500)
local mine = hash:nearest(0, 0, 500, function(value) return value.kind == 'gold' end)
print(closest.kind, mine.kind) -- tree gold
```

### structure:kNearest(x, y, count, maxDistance)

Returns up to `count` values whose bounds lie closest to the point `x`, `y`, closest first. The optional `maxDistance` leaves out values farther away.

```lua
local spatial2d = require('haylen.spatial2d')

local wells = spatial2d.newKdTree()
for index = 1, 20 do
    wells:set({id = index}, index * 50, (index % 4) * 70)
end
wells:build()

for _, well in ipairs(wells:kNearest(300, 100, 3)) do
    print('Well ' .. well.id)
end
```

### structure:pick(camera, x, y)

Returns the values whose bounds contain the world point under the screen point `x`, `y`, as `camera` of [`haylen.graphics2d`](graphics2d.md#camera) sees it. Screen points are in design coordinates, like the pointer positions of [`haylen.input`](input.md#inputmouseposition).

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')
local spatial2d = require('haylen.spatial2d')

local units = spatial2d.newHashGrid(64)
units:set({name = 'knight'}, {-20, -20, 40, 40})
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        if input.mousePressed() then
            for _, unit in ipairs(units:pick(camera, input.mousePosition())) do
                print('Selected the ' .. unit.name .. '.')
            end
        end
    end,
})
```

### structure:clear()

Removes every value.

```lua
local spatial2d = require('haylen.spatial2d')

local hash = spatial2d.newHashGrid(64)
hash:set('marker', {0, 0, 0, 0})
hash:clear()
print(hash.size) -- 0
```

## HashGrid

A `HashGrid` is a spatial hash that [`spatial2d.newHashGrid`](#spatial2dnewhashgridcellsize) returns. Moving an entry costs about the same as storing it, so it suits many entries of a similar size that move every frame. It has the [structure methods](#structure-methods) `hash:set`, `hash:remove`, `hash:has`, `hash:bounds`, `hash:query`, `hash:queryCircle`, `hash:queryPoint`, `hash:raycast`, `hash:nearest`, `hash:kNearest`, `hash:pick` and `hash:clear`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of stored values. |
| `cellSize` | number | read | The width and height of a cell. |

```lua
local spatial2d = require('haylen.spatial2d')

local bullets = spatial2d.newHashGrid(32)
bullets:set({damage = 2}, {10, 10, 4, 4})
print(bullets.size, bullets.cellSize) -- 1 32.0
```

## QuadTree

A `QuadTree` is a quadtree that [`spatial2d.newQuadTree`](#spatial2dnewquadtreearea-options) returns. It suits entries of very different sizes spread over a known area. It has the [structure methods](#structure-methods) `tree:set`, `tree:remove`, `tree:has`, `tree:bounds`, `tree:query`, `tree:queryCircle`, `tree:queryPoint`, `tree:raycast`, `tree:nearest`, `tree:kNearest`, `tree:pick` and `tree:clear`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of stored values. |
| `area` | Rect | read | The area that the tree covers. |
| `nodeCount` | integer | read | The squares that the tree uses now, the root included. |

```lua
local spatial2d = require('haylen.spatial2d')

local level = spatial2d.newQuadTree({0, 0, 2048, 2048}, {maxEntries = 2})
for index = 1, 10 do
    level:set({id = index}, {index * 150, index * 150, 20, 20})
end
print(level.size, level.nodeCount > 1) -- 10 true
```

## AabbTree

An `AabbTree` is a dynamic tree of bounding boxes that [`spatial2d.newAabbTree`](#spatial2dnewaabbtreemargin) returns. It suits many moving entries of any size over an area without limits. It has the [structure methods](#structure-methods) `tree:set`, `tree:remove`, `tree:has`, `tree:bounds`, `tree:query`, `tree:queryCircle`, `tree:queryPoint`, `tree:raycast`, `tree:nearest`, `tree:kNearest`, `tree:pick` and `tree:clear`.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of stored values. |
| `margin` | number | read | How much larger than its entry the box of each entry is, on every side. |
| `height` | integer | read | The levels below the root. Balancing keeps it near the logarithm of the size. |

```lua
local spatial2d = require('haylen.spatial2d')

local ships = spatial2d.newAabbTree(16)
for index = 1, 100 do
    ships:set({id = index}, {index * 40, 0, 30, 30})
end
print(ships.size, ships.height, ships.margin)
```

## KdTree

A `KdTree` is a tree of points that [`spatial2d.newKdTree`](#spatial2dnewkdtree) returns. It suits points that rarely change, when nearest queries come most often. Its points change freely, but a query needs a [`kdTree:build`](#kdtreebuild) after a change. It has the [structure methods](#structure-methods) `kdTree:remove`, `kdTree:has`, `kdTree:query`, `kdTree:queryCircle`, `kdTree:queryPoint`, `kdTree:raycast`, `kdTree:nearest`, `kdTree:kNearest`, `kdTree:pick` and `kdTree:clear`, and the methods below.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of stored values. |
| `built` | boolean | read | The value is `false` when the tree changed since its last `kdTree:build`. |

### kdTree:set(value, x, y, radius)

Stores `value` at the point `x`, `y` with a `radius`, `0` by default, or moves it when it is already stored. A `nil` value raises `bad argument #1 to 'set' (a value to store is required)`, and a point or radius that is not finite, or a negative radius, raises `A k-d tree entry needs a finite point and a finite radius of zero or more.`

```lua
local spatial2d = require('haylen.spatial2d')

local stars = spatial2d.newKdTree()
stars:set('sun', 0, 0, 10)
stars:set('comet', 300, 40)
print(stars.size, stars.built) -- 2 false
```

### kdTree:point(value)

Returns the point stored for `value` as a `Vec2`, or `nil` when the value is not stored.

```lua
local spatial2d = require('haylen.spatial2d')

local stars = spatial2d.newKdTree()
stars:set('sun', 12, 34, 10)
print(stars:point('sun'), stars:point('moon')) -- Vec2(12.0, 34.0) nil
```

### kdTree:build()

Rebuilds the tree from its current points. A query on a tree that changed since its last build raises `The k-d tree changed since it was last built, so it needs "build" before a query.`

```lua
local spatial2d = require('haylen.spatial2d')

local stars = spatial2d.newKdTree()
stars:set('sun', 0, 0, 10)
stars:set('comet', 300, 40)
stars:build()
print(stars.built, stars:kNearest(290, 40, 1)[1]) -- true comet
```

## Grid algorithms

The grid algorithms work on a [`CellGrid`](#cellgrid), a grid of integer cells such as a tile map or the map of a generator of [`haylen.procedural2d`](procedural2d.md). Cells count from 0 by column and row. Unless an algorithm compares values, `0` is open ground and any other value is solid.

### spatial2d.newCellGrid(width, height, value)

Returns a [`CellGrid`](#cellgrid) of `width` columns and `height` rows whose cells all hold the integer `value`, `0` by default. A size that is not positive, or a grid too large for 32-bit cell indices, raises `A cell grid needs a positive size that fits in 32-bit cell indices.`

```lua
local spatial2d = require('haylen.spatial2d')

local dungeon = spatial2d.newCellGrid(40, 30)
for column = 0, 39 do
    dungeon:set(column, 0, 1)
end
```

### spatial2d.raycastGrid(grid, from, to, cellSize)

Casts a ray from the point `from` to the point `to` over the cells of a `CellGrid`, where each cell is `cellSize` units wide and the first cell sits at the origin. The cell size is a number, or a `{width, height}` pair for cells that are not square. It returns the hit on the first solid cell, or `nil`. The hit is a table like the hits of [`haylen.math`](math.md#ray-casts), with the fields `column` and `row` of the cell. The ray only travels over the grid, and a ray that starts in a solid cell hits it at distance 0. A cell size that is not positive and finite raises `Grid cells need a positive and finite size.`

```lua
local spatial2d = require('haylen.spatial2d')

local walls = spatial2d.newCellGrid(20, 15)
walls:set(8, 3, 1)
local hit = spatial2d.raycastGrid(walls, {40, 56}, {600, 56}, 16)
print(hit.column, hit.row, hit.x, hit.normalX) -- 8 3 128.0 -1.0
```

### spatial2d.traverseGrid(from, to, cellSize)

Returns every cell that the segment from `from` to `to` crosses, on a grid without limits whose cells are `cellSize` units wide, a number or a `{width, height}` pair, with the first cell at the origin. The cells come in order from the start, as `{x = column, y = row, distance}` tables. The field `distance` is how far along the segment it enters the cell, `0` for the first one. Unlike [`spatial2d.line`](#spatial2dlinex1-y1-x2-y2), it lists every cell the segment touches, which suits lasers, bullets and the highlight of tiles. A segment through the exact corner of four cells steps along x first.

A cell size that is not positive and finite raises `Grid cells need a positive and finite size.`. A segment beyond the 32-bit range of cells raises `A grid ray must stay within the 32-bit range of cells.`, a segment whose length is not finite raises `A ray across an unbounded grid needs a finite length.`, and a segment across more than 65536 cells raises `A grid traversal lists at most 65536 cells.`

```lua
local spatial2d = require('haylen.spatial2d')

for _, cell in ipairs(spatial2d.traverseGrid({5, 5}, {35, 17}, 10)) do
    print(cell.x, cell.y, cell.distance)
end
```

### spatial2d.line(x1, y1, x2, y2)

Returns the cells of the Bresenham line from one cell to another, both included, as a list of `{x = column, y = row}` tables in order. A Bresenham line takes one cell per step along its longer axis, so it has no gaps and no doubled cells.

```lua
local spatial2d = require('haylen.spatial2d')

for _, cell in ipairs(spatial2d.line(0, 0, 5, 2)) do
    print(cell.x, cell.y)
end
```

### spatial2d.circle(x, y, radius)

Returns the outline of a circle of cells around the cell `x`, `y`, made with the midpoint circle algorithm. Each cell comes once, sorted by row and then by column. A radius of 0 gives the center alone, and a negative radius raises `A circle needs a radius of zero or more cells.`

```lua
local spatial2d = require('haylen.spatial2d')

local ring = spatial2d.circle(10, 10, 4)
print(#ring)
```

### spatial2d.fieldOfView(grid, x, y, radius)

Returns the cells of the grid that the cell `x`, `y` sees within `radius` cells, measured between cell centers, as a list of `{x, y}` tables. It uses the symmetric shadowcasting of Albert Ford: solid cells block sight and are visible themselves, and cells outside the grid count as solid. Sight is symmetric, so when an open cell sees another, the other sees it back, which keeps stealth fair.

```lua
local spatial2d = require('haylen.spatial2d')

local map = spatial2d.newCellGrid(30, 20)
map:set(12, 10, 1)
local seen = {}
for _, cell in ipairs(spatial2d.fieldOfView(map, 10, 10, 8)) do
    seen[cell.y * 30 + cell.x] = true
end
print(seen[10 * 30 + 11], seen[10 * 30 + 14]) -- true nil
```

### spatial2d.visibilityPolygon(origin, walls, bounds)

Returns the outline of the area that the point `origin` sees among the wall segments, clipped to the rectangle `bounds`. The outline is a list of `Vec2` in order of increasing angle. It is the shape of a 2D light or of what a guard sees. Walls are segments such as `{{x1, y1}, {x2, y2}}`. Walls or bounds that are not finite raise `A visibility polygon needs finite walls and bounds.`, and an origin outside the bounds, or one that is not a number, raises `A visibility polygon needs an origin inside its bounds.`

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local spatial2d = require('haylen.spatial2d')

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

Returns the cells connected to the cell `x`, `y` that hold its value, starting with it, in breadth-first order, like the paint bucket of an image editor. A start outside the grid returns an empty list. The table `options` is optional.

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
print(#left, canvas:get(3, 3), canvas:get(12, 3)) -- 128 2 0
```

### spatial2d.components(grid, options)

Splits the grid into regions of connected cells that share a value, such as the islands of a map or the rooms of a dungeon. It returns two values: a new `CellGrid` with the region number of every cell, and the number of regions. Regions count from 1, in the order their first cell appears row by row. The table `options` is optional.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `diagonal` | boolean | `false` | Also joins cells that only share a corner. |
| `background` | integer | none | Cells with this value form no region and get `0`. |

```lua
local spatial2d = require('haylen.spatial2d')

local sea = spatial2d.newCellGrid(12, 8)
sea:set(1, 1, 1)
sea:set(2, 1, 1)
sea:set(8, 5, 1)
local islands, count = spatial2d.components(sea, {background = 0})
print(count, islands:get(2, 1), islands:get(8, 5)) -- 2 1 2
```

### spatial2d.newUnionFind(count)

Returns a [`UnionFind`](#unionfind) of `count` elements, `0` by default, numbered from 1, each in a set of its own.

```lua
local spatial2d = require('haylen.spatial2d')

local sets = spatial2d.newUnionFind(10)
sets:unite(1, 2)
print(sets:connected(2, 1), sets.setCount) -- true 9
```

## CellGrid

A `CellGrid` holds one integer per cell, which [`spatial2d.newCellGrid`](#spatial2dnewcellgridwidth-height-value) and the generators of [`haylen.procedural2d`](procedural2d.md) return. Cells are addressed by column and row, counting from 0 like the cells of Tiled. The methods `grid:get` and `grid:set` raise an error such as `Cell 9,9 is outside the cell grid.` for a cell outside the grid.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `width` | integer | read | The number of columns. |
| `height` | integer | read | The number of rows. |

### grid:get(x, y)

Returns the value of a cell.

```lua
local spatial2d = require('haylen.spatial2d')

local grid = spatial2d.newCellGrid(10, 8, 1)
print(grid:get(4, 4)) -- 1
```

### grid:set(x, y, value)

Sets the value of a cell to the integer `value`.

```lua
local spatial2d = require('haylen.spatial2d')

local grid = spatial2d.newCellGrid(10, 8)
grid:set(4, 4, 3)
print(grid:get(4, 4)) -- 3
```

### grid:fill(value)

Sets every cell to `value`.

```lua
local spatial2d = require('haylen.spatial2d')

local grid = spatial2d.newCellGrid(10, 8)
grid:fill(1)
print(grid:get(0, 0), grid:get(9, 7)) -- 1 1
```

### grid:contains(x, y)

Returns `true` when the cell lies inside the grid.

```lua
local spatial2d = require('haylen.spatial2d')

local grid = spatial2d.newCellGrid(10, 8)
print(grid.width, grid.height, grid:contains(9, 7), grid:contains(10, 0)) -- 10 8 true false
```

## UnionFind

A `UnionFind` keeps the elements 1 to `size` in sets that never share an element, which [`spatial2d.newUnionFind`](#spatial2dnewunionfindcount) returns. It merges sets by size and halves paths as it finds them, so every call takes nearly constant time. Use it to join regions, such as rooms that a door connects. An element below 1 raises `bad argument #1 to 'find' (union-find elements count from 1)`, with the position and the name of the method. An element above `size` raises `Element <element> is outside the union-find of <size> elements.`, which names the element as you passed it, such as `Element 11 is outside the union-find of 10 elements.` for `sets:find(11)` on 10 elements.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `size` | integer | read | The number of elements. |
| `setCount` | integer | read | The number of sets. |

### sets:add()

Adds an element in a set of its own and returns it.

```lua
local spatial2d = require('haylen.spatial2d')

local sets = spatial2d.newUnionFind(3)
print(sets:add(), sets.size, sets.setCount) -- 4 4 4
```

### sets:find(element)

Returns the representative of the set that holds `element`, one element that stands for the whole set. Two elements share a set when they have the same representative.

```lua
local spatial2d = require('haylen.spatial2d')

local sets = spatial2d.newUnionFind(6)
sets:unite(5, 6)
print(sets:find(5) == sets:find(6), sets:find(1) == sets:find(6)) -- true false
```

### sets:unite(first, second)

Merges the sets of both elements. It returns `true`, or `false` when they already shared a set.

```lua
local spatial2d = require('haylen.spatial2d')

local sets = spatial2d.newUnionFind(4)
print(sets:unite(1, 2), sets:unite(2, 1)) -- true false
```

### sets:connected(first, second)

Returns `true` when both elements share a set.

```lua
local spatial2d = require('haylen.spatial2d')

local rooms = spatial2d.newUnionFind(6)
rooms:unite(1, 2)
rooms:unite(2, 3)
print(rooms:connected(1, 3), rooms:connected(1, 4)) -- true false
```

### sets:setSize(element)

Returns the number of elements in the set of `element`.

```lua
local spatial2d = require('haylen.spatial2d')

local rooms = spatial2d.newUnionFind(6)
rooms:unite(1, 2)
rooms:unite(2, 3)
print(rooms:setSize(3), rooms:setSize(6)) -- 3 1
```

### sets:reset(count)

Starts over with `count` elements, each in a set of its own. It reuses the memory of the union-find instead of creating a new one.

```lua
local spatial2d = require('haylen.spatial2d')

local islands = spatial2d.newUnionFind(100)
islands:unite(1, 2)
islands:reset(50)
print(islands.size, islands.setCount, islands:connected(1, 2)) -- 50 50 false
```

## Screen picking

### spatial2d.screenRay(camera, x, y)

Returns the ray from the world point at the center of the view of `camera` to the world point under the screen point `x`, `y`, as four numbers `x1, y1, x2, y2`, ready for any ray cast. Screen points are in design coordinates, like the pointer positions of [`haylen.input`](input.md#inputmouseposition), and `camera` is the camera of [`haylen.graphics2d`](graphics2d.md#camera) that draws the world. To turn a screen point into a world point alone, use [`camera:screenToWorld`](graphics2d.md#camerascreentoworldx-y).

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local scene = require('haylen.scene')
local spatial2d = require('haylen.spatial2d')

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

## Overlap worlds

An overlap world keeps entities that each show one set of boxes, such as the body, hit and grab boxes of one frame of a fighter. Every fixed step, it lists the pairs of boxes that reach each other, for games that resolve their own hits without physics bodies. The [spatial guide](../2d/spatial.md#overlap-worlds) explains when to use one.

A box reaches another box when three things hold:

- Its `mask` shares a bit with the `layers` of the other box. Teams and kinds of attacks are bits that the app chooses, so the world holds no rule of combat.
- Their rectangles overlap.
- Their entities stand within the sum of the `depth` reaches of the two boxes. A world of depth rows, such as the belt of a brawler, keeps fighters on far rows apart, and a world with every depth at 0 is plain 2D.

A box rectangle is relative to the origin of an entity that faces right. A mirrored entity places the box `x, y, width, height` at `-x - width, y`. Contacts come in the order the entities were added, then by box, whatever moved and whatever found the pairs, so results repeat in replays. Entities, sets and boxes count from 1. An id that is not an integer of 32 bits raises `bad argument #1 to 'place' (an id is an integer of 32 bits)`, with the position and the name of the method.

### spatial2d.newOverlapWorld(options)

Returns an [`OverlapWorld`](#overlapworld). The table `options` is optional.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `kinds` | list of strings | `{}` | The names of the kinds of boxes, at most 256, which `world:debugDraw` colors. |
| `touching` | boolean | `true` | Whether boxes whose edges only touch overlap. |
| `cellSize` | number | `0` | Above 0, the world finds candidate pairs through a spatial hash of that cell size, for worlds of many entities. At 0, it tests every pair. |

More than 256 kinds raise `An overlap world has at most 256 kinds of boxes.`, and a negative cell size raises `The cell size of an overlap world must be 0 or positive.`

```lua
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld({kinds = {'body', 'hit', 'grab'}, cellSize = 128})
```

## OverlapWorld

An `OverlapWorld` is the world that [`spatial2d.newOverlapWorld`](#spatial2dnewoverlapworldoptions) returns.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `entityCount` | integer | read | The number of entities. |

### world:defineSet(boxes)

Defines a set of boxes once, such as the boxes of one frame of an animation, and returns its id. Each box is a table `{kind, x, y, width, height, layers, mask, depth}`. The `kind` is a name of `kinds`, the first kind by default. The `layers` are `1` by default, the `mask` is `0` and the `depth` is `0`, so a box reaches nothing until it has a mask.

A box without a positive size raises `A box needs a positive width and height.`, an unknown kind raises `The box kind "<kind>" is not one of the kinds of the overlap world.` and bits that are not an integer of 32 bits raise `The "<field>" of a box is an integer of 32 bits.`. Another key raises `Unknown option "<key>".`

```lua
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld({kinds = {'body', 'hit'}})
local punching = world:defineSet({
    {kind = 'body', x = -18, y = -78, width = 36, height = 78, layers = 1, depth = 12},
    {kind = 'hit', x = 12, y = -62, width = 34, height = 14, mask = 1},
})
print(punching) -- 1
```

### world:add()

Creates an entity without boxes and returns its id.

```lua
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld()
local hero, rival = world:add(), world:add()
print(hero, rival, world.entityCount) -- 1 2 2
```

### world:remove(entity)

Deletes an entity and frees its id for a later `world:add`. The entity that takes the id comes last in the order of contacts. An unknown entity raises `The overlap world has no entity <id>.`

```lua
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld()
local hero = world:add()
world:remove(hero)
print(world.entityCount) -- 0
```

### world:place(entity, x, y, depth, facing, set)

Shows the boxes of `set` for an entity at the position `x`, `y` and the depth `depth`. A negative `facing` mirrors the boxes. The set `0` shows no boxes. An unknown entity raises `The overlap world has no entity <id>.`, and an unknown set raises `The overlap world has no box set <id>.`

```lua
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld()
local standing = world:defineSet({{x = -18, y = -78, width = 36, height = 78}})
local hero = world:add()
world:place(hero, 200, 0, 420, -1, standing)
```

### world:placeAll(buffer, first)

Places many entities in one call from a [float buffer](collections.md#collectionsnewfloatbuffersize-value). Each entity takes six numbers, `entity, x, y, depth, facing, set`, read from the position `first`, `1` by default, to the end of the buffer. A `first` below 1 raises `bad argument #2 to 'placeAll' (the first value of a buffer is at position 1)`.

```lua
local collections = require('haylen.collections')
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld()
local set = world:defineSet({{width = 10, height = 10, mask = 1}})
local first, second = world:add(), world:add()
local placements = collections.newFloatBuffer(12)
placements:set(1, {first, 0, 0, 0, 1, set, second, 5, 0, 0, -1, set})
world:placeAll(placements)
```

### world:contacts(buffer)

Finds every contact and returns how many there are. With a float buffer, it writes eight numbers per contact, `source, sourceBox, target, targetBox` and the `x, y, width, height` of the overlap, as many contacts as fit, so a step allocates nothing. Without a buffer, it returns a list of `{source, sourceBox, target, targetBox, overlap}` tables instead, where `overlap` is a `Rect`, which suits small worlds. A pair that rests yields no contact.

```lua
local collections = require('haylen.collections')
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld({kinds = {'body', 'hit'}})
local standing = world:defineSet({{kind = 'body', x = -18, y = -78, width = 36, height = 78}})
local punching = world:defineSet({{kind = 'hit', x = 12, y = -62, width = 34, height = 14, mask = 1}})
local hero, rival = world:add(), world:add()
world:place(hero, 200, 0, 0, 1, punching)
world:place(rival, 236, 0, 0, -1, standing)
local contacts = collections.newFloatBuffer(8 * 64)
local count = world:contacts(contacts)
for index = 0, math.min(count, 64) - 1 do
    local base = index * 8
    print(contacts[base + 1], contacts[base + 3], contacts[base + 7])
end
```

### world:rest(source, target, ticks)

Keeps the boxes of the entity `source` from reaching the entity `target` for `ticks` steps, as a hit that lands once in a window does, or a pause between hits. A value of `0` ends a rest.

```lua
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld()
local hero, rival = world:add(), world:add()
world:rest(hero, rival, 7)
```

### world:resting(source, target)

Returns `true` while a rest keeps `source` from reaching `target`.

```lua
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld()
local hero, rival = world:add(), world:add()
world:rest(hero, rival, 1)
print(world:resting(hero, rival), world:resting(rival, hero)) -- true false
```

### world:step()

Counts every rest down by one step. Call it once per fixed step.

```lua
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld()
local hero, rival = world:add(), world:add()
world:rest(hero, rival, 1)
world:step()
print(world:resting(hero, rival)) -- false
```

### world:clear()

Removes every entity and every rest. The sets stay defined.

```lua
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld()
local set = world:defineSet({{width = 10, height = 10}})
world:add()
world:clear()
print(world.entityCount, set) -- 0 1
```

### world:debugDraw(order)

Outlines every placed box in the active canvas, with a color for each kind. The optional `order` takes the [draw order](graphics2d.md#draw-order) keys of `haylen.graphics2d`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local spatial2d = require('haylen.spatial2d')

local world = spatial2d.newOverlapWorld({kinds = {'body', 'hit'}})
local camera = graphics2d.newCamera()

scene.push({
    render = function(self)
        graphics2d.beginWorld(camera)
        world:debugDraw({layer = 100})
    end,
})
```

## Errors

These messages come from the functions and methods of the module. Messages that start with `bad argument` come from Lua, which adds the position and the name of the function.

| Message | Cause |
| --- | --- |
| `A spatial hash needs a positive cell size.` | The cell size of `spatial2d.newHashGrid` is not positive and finite. |
| `A spatial hash entry may cover at most 65536 cells, so the cell size should be closer to the size of the entries.` | A rectangle of a `HashGrid` is far larger than its cells. |
| `A spatial hash entry must lie within 536870912 cells of the origin.` | A rectangle of a `HashGrid` lies too far away. |
| `A quadtree needs an area of positive size, room for an entry per quadrant and a depth from 0 to 16.` | The area or the options of `spatial2d.newQuadTree` are out of range. |
| `An AABB tree needs a finite margin of zero or more.` | The margin of `spatial2d.newAabbTree` is negative or not finite. |
| `A k-d tree entry needs a finite point and a finite radius of zero or more.` | The point or the radius of `kdTree:set` is out of range. |
| `The k-d tree changed since it was last built, so it needs "build" before a query.` | A `KdTree` was queried after a change and before `kdTree:build`. |
| `bad argument #1 to 'set' (a value to store is required)` | The value of `structure:set` is `nil`. |
| `Spatial bounds must be finite and have a non-negative size.` | A rectangle of a structure has a negative size or a component that is not finite. |
| `A spatial query radius must not be negative.` | The radius of `structure:queryCircle` is negative. |
| `A spatial query point must be finite.` | The point of `structure:nearest` or `structure:kNearest` is not finite. |
| `A cell grid needs a positive size that fits in 32-bit cell indices.` | The size of `spatial2d.newCellGrid` is out of range. |
| `Cell <x>,<y> is outside the cell grid.` | The method `grid:get` or `grid:set` got a cell outside the grid. |
| `Grid cells need a positive and finite size.` | The cell size of `spatial2d.raycastGrid` or `spatial2d.traverseGrid` is out of range. |
| `A grid ray must stay within the 32-bit range of cells.` | The segment of `spatial2d.traverseGrid` reaches too far. |
| `A ray across an unbounded grid needs a finite length.` | The segment of `spatial2d.traverseGrid` is not finite. |
| `A grid traversal lists at most 65536 cells.` | The segment of `spatial2d.traverseGrid` crosses too many cells. |
| `A circle needs a radius of zero or more cells.` | The radius of `spatial2d.circle` is negative. |
| `A visibility polygon needs finite walls and bounds.` | A wall or the bounds of `spatial2d.visibilityPolygon` are not finite. |
| `A visibility polygon needs an origin inside its bounds.` | The origin of `spatial2d.visibilityPolygon` lies outside its bounds. |
| `Unknown option "<key>".` | An options table or a box has a key that it does not take. |
| `The option "<option>" of "<function>" is invalid: <reason>.` | An option has the wrong type, such as `The option "maxDepth" of "newQuadTree" is invalid: number has no integer representation.`. |
| `bad argument #1 to 'find' (union-find elements count from 1)` | A method of a `UnionFind` got an element below 1. |
| `Element <element> is outside the union-find of <size> elements.` | A method of a `UnionFind` got an element above its size. |
| `An overlap world has at most 256 kinds of boxes.` | The `kinds` of `spatial2d.newOverlapWorld` name too many kinds. |
| `The cell size of an overlap world must be 0 or positive.` | The `cellSize` of `spatial2d.newOverlapWorld` is negative or not finite. |
| `A box needs a positive width and height.` | A box of `world:defineSet` has no area. |
| `The box kind "<kind>" is not one of the kinds of the overlap world.` | A box of `world:defineSet` names an unknown kind. |
| `The "<field>" of a box is an integer of 32 bits.` | The `layers` or `mask` of a box is out of range. |
| `The overlap world has no entity <id>.` | A method of an `OverlapWorld` got an unknown entity. |
| `The overlap world has no box set <id>.` | The method `world:place` got an unknown set. |
| `bad argument #1 to 'place' (an id is an integer of 32 bits)` | An id of an `OverlapWorld` method is negative or too large. |
| `bad argument #2 to 'placeAll' (the first value of a buffer is at position 1)` | The `first` of `world:placeAll` is below 1. |
