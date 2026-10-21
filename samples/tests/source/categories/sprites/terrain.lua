-- The painted ground of the package, laid out by the corners of its cells: grass in every cell, and sand, dirt paths and water whose edges follow the kind of ground at the four corners of each cell. It draws the tiles of `content/tiled/tiles/`, painted at 128 pixels in a margin of 2 and a spacing of 4.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Terrain = haylen.class('Terrain')

-- The kinds of ground a corner holds, from the bottom up, and the row of the corner set of each kind over grass in the transitions image.
Terrain.grass = 0
Terrain.sand = 1
Terrain.path = 2
Terrain.water = 3
Terrain.rows = {[Terrain.sand] = 4, [Terrain.path] = 5, [Terrain.water] = 0}

Terrain.textureOptions = {filter = 'linear', mipmaps = true}
-- The share of grass cells that show the grass with flowers.
Terrain.flowers = 0.14

-- Returns the area of the painted tile at a column and row of a tile image.
function Terrain.source(column, row)
    return {2 + column * 132, 2 + row * 132, 128, 128}
end

-- Returns the image of the grass and the image of the corner sets.
function Terrain.textures()
    return assets.texture('tiled/tiles/terrain.png', Terrain.textureOptions), assets.texture('tiled/tiles/transitions.png', Terrain.textureOptions)
end

-- Returns the corner set tile of a kind of ground in a cell whose corners hold the given kinds, from 0 for none of them to 15 for all four.
function Terrain.mask(kind, topLeft, topRight, bottomRight, bottomLeft)
    return (topLeft == kind and 1 or 0) + (topRight == kind and 2 or 0) + (bottomRight == kind and 4 or 0) + (bottomLeft == kind and 8 or 0)
end

-- Returns the grass tile of a cell, mostly plain with some flowers scattered without a pattern, the same for a cell every time.
function Terrain.grassAt(column, row)
    local hash = column * 374761393 + row * 668265263
    hash = (hash ~ (hash >> 13)) * 1274126177
    hash = (hash ~ (hash >> 16)) & 0xFFFF
    return hash < Terrain.flowers * 0x10000 and 1 or 0
end

-- Calls `add(texture, source)` for every tile of a cell from the bottom up: its grass on the first texture, then the corner tiles of sand, paths and water on the second.
function Terrain.tiles(column, row, kindAt, add)
    add(1, Terrain.source(Terrain.grassAt(column, row), 0))
    local topLeft, topRight = kindAt(column, row), kindAt(column + 1, row)
    local bottomRight, bottomLeft = kindAt(column + 1, row + 1), kindAt(column, row + 1)
    for kind = Terrain.sand, Terrain.water do
        local mask = Terrain.mask(kind, topLeft, topRight, bottomRight, bottomLeft)
        if mask > 0 then
            add(2, Terrain.source(mask, Terrain.rows[kind]))
        end
    end
end

-- Lays the ground of a grid of cells of `size` units whose top-left corner is at `x`, `y` into two sprite batches, the grass and the edges over it. The function `kindAt(column, row)` returns the kind of ground at a corner, from 0, 0 to `columns`, `rows`.
function Terrain.batches(columns, rows, size, x, y, kindAt)
    local grass, edges = Terrain.textures()
    local batches = {graphics2d.newSpriteBatch(grass), graphics2d.newSpriteBatch(edges)}
    batches[1]:reserve(columns * rows)
    for row = 0, rows - 1 do
        for column = 0, columns - 1 do
            local left, top = x + column * size, y + row * size
            Terrain.tiles(column, row, kindAt, function(texture, source)
                batches[texture]:add({x = left, y = top, width = size, height = size, pivotX = 0, pivotY = 0, source = source})
            end)
        end
    end
    return batches[1], batches[2]
end

-- Bakes the ground of `Terrain.batches` into two static batches, the grass and the edges over it.
function Terrain.bake(columns, rows, size, x, y, kindAt)
    local grass, edges = Terrain.batches(columns, rows, size, x, y, kindAt)
    return grass:bake(), edges:bake()
end

return Terrain
