-- The terrain of the world, made from its seed alone: a height and a moisture field of fractal noise pick the terrain of every tile, the slope of the height shades it, and the trees, bushes and rocks that a Poisson scatter places grow where their terrain allows. The same seed always makes the same world, so a chunk built again matches the one that unloaded.
local jobs = require('haylen.jobs')
local m = require('haylen.math')

local Terrain = {}
Terrain.__index = Terrain

Terrain.tileSize = 32
Terrain.chunkTiles = 64
Terrain.chunkSize = Terrain.tileSize * Terrain.chunkTiles
Terrain.heightScale = 1 / 300
Terrain.moistureScale = 1 / 420
Terrain.tileFields = {'x', 'y', 'sourceX', 'rotation', 'red', 'green', 'blue'}
Terrain.decorationFields = {'x', 'y', 'width', 'height', 'sourceX'}

-- Terrain ids, which are also the columns of the atlas, where every terrain has two variants of a tile of 64 pixels in a cell of 72.
Terrain.deepWater, Terrain.water, Terrain.sand, Terrain.grass, Terrain.meadow, Terrain.forest, Terrain.rock, Terrain.snow = 0, 1, 2, 3, 4, 5, 6, 7
Terrain.tileCell = 72
Terrain.tileInset = 4
Terrain.tilePixels = 64

-- Decorations in the order of the atlas, cells of 80 by 112 pixels every 96 pixels, with the size they draw at in world units.
Terrain.pine, Terrain.tree, Terrain.palm, Terrain.bush, Terrain.boulder, Terrain.flowers, Terrain.snowyPine, Terrain.reeds = 0, 1, 2, 3, 4, 5, 6, 7
Terrain.decorationCell = 96
Terrain.decorationInset = 8
Terrain.decorationTop = 88
Terrain.decorationPixels = {80, 112}
Terrain.decorationSize = {46, 64}
-- The decoration that each of the three scatter types becomes on each terrain, or false where nothing grows.
Terrain.growth = {
    [Terrain.deepWater] = {false, false, false},
    [Terrain.water] = {false, false, false},
    [Terrain.sand] = {Terrain.palm, false, Terrain.reeds},
    [Terrain.grass] = {Terrain.tree, Terrain.flowers, Terrain.bush},
    [Terrain.meadow] = {Terrain.bush, Terrain.tree, Terrain.boulder},
    [Terrain.forest] = {Terrain.pine, Terrain.tree, Terrain.pine},
    [Terrain.rock] = {Terrain.boulder, Terrain.boulder, false},
    [Terrain.snow] = {Terrain.snowyPine, false, false},
}

function Terrain.new(seed)
    local self = setmetatable({}, Terrain)
    self.seed = seed
    self.height = m.noise(seed)
    self.moisture = m.noise(seed + 1)
    return self
end

-- The options of the Poisson scatter of one chunk, whose density follows a noise field of the whole world, so neighbor chunks agree at their edges.
function Terrain:scatterOptions(chunkX, chunkY)
    local size = Terrain.chunkSize
    return {
        region = {chunkX * size, chunkY * size, size, size},
        method = 'poisson',
        spacing = 34,
        maximumSpacing = 150,
        densityMap = {seed = self.seed + 2, frequency = 0.0012, octaves = 3},
        weights = {2, 1, 1},
        seed = (self.seed * 7919 + chunkX * 92821 + chunkY * 68917) % 2147483647,
    }
end

local function classify(height, moisture)
    if height < -0.3 then
        return Terrain.deepWater
    elseif height < -0.07 then
        return Terrain.water
    elseif height < -0.01 then
        return Terrain.sand
    elseif height < 0.42 then
        if moisture > 0.12 then
            return Terrain.forest
        elseif moisture < -0.3 then
            return Terrain.meadow
        end
        return Terrain.grass
    elseif height < 0.56 then
        return Terrain.rock
    end
    return Terrain.snow
end

-- A small integer hash of a tile, which picks its variant and its quarter turn.
local function hash(x, y)
    local value = (x * 73856093) ~ (y * 19349663)
    return (value ~ (value >> 13)) & 0xFFFF
end

-- Builds the sprite fields of the tiles and decorations of one chunk. It runs as a job of "haylen.jobs", which pauses at every row once the budget of the frame is spent.
function Terrain:build(chunkX, chunkY, points)
    local tiles, size = Terrain.chunkTiles, Terrain.tileSize
    local firstX, firstY = chunkX * tiles, chunkY * tiles
    local heightNoise, moistureNoise = self.height, self.moisture
    local heightScale, moistureScale = Terrain.heightScale, Terrain.moistureScale
    local width = tiles + 1

    -- Heights with one more row and column above and to the left, so every tile compares with its neighbor toward the light.
    local heights = {}
    for row = 0, tiles do
        local y = (firstY + row - 1) * heightScale
        for column = 0, tiles do
            heights[row * width + column + 1] = heightNoise:fractal((firstX + column - 1) * heightScale, y, 6)
        end
        jobs.checkpoint()
    end

    local fields, types = {}, {}
    local cell, inset, quarter = Terrain.tileCell, Terrain.tileInset, math.pi / 2
    local slot = 0
    for row = 1, tiles do
        local tileY = firstY + row - 1
        for column = 1, tiles do
            local tileX = firstX + column - 1
            local height = heights[row * width + column + 1]
            local kind = classify(height, moistureNoise:fractal(tileX * moistureScale, tileY * moistureScale, 3))
            local shade
            if kind <= Terrain.water then
                shade = m.clamp(1 + height * 0.9, 0.55, 1)
            else
                shade = m.clamp(0.93 + (height - heights[(row - 1) * width + column]) * 7, 0.8, 1)
            end
            local mix = hash(tileX, tileY)
            types[(row - 1) * tiles + column] = kind
            fields[slot + 1], fields[slot + 2] = (tileX + 0.5) * size, (tileY + 0.5) * size
            fields[slot + 3] = (kind * 2 + mix % 2) * cell + inset
            fields[slot + 4] = (mix >> 1) % 4 * quarter
            fields[slot + 5], fields[slot + 6], fields[slot + 7] = shade, shade, shade
            slot = slot + 7
        end
        jobs.checkpoint()
    end
    return {tiles = fields, decorations = self:decorate(chunkX, chunkY, points, types)}
end

-- Turns the scattered points into decorations by the terrain under each one, ordered from the top down so the lower ones cover the higher ones.
function Terrain:decorate(chunkX, chunkY, points, types)
    local tiles, size = Terrain.chunkTiles, Terrain.tileSize
    local originX, originY = chunkX * Terrain.chunkSize, chunkY * Terrain.chunkSize
    local placed = {}
    for _, point in ipairs(points) do
        local column = math.min(tiles - 1, (point.x - originX) // size)
        local row = math.min(tiles - 1, (point.y - originY) // size)
        local decoration = Terrain.growth[types[row * tiles + column + 1]][point.type]
        if decoration then
            placed[#placed + 1] = {x = point.x, y = point.y, decoration = decoration, scale = 0.8 + hash(point.x // 1, point.y // 1) % 40 / 100}
        end
    end
    jobs.checkpoint()
    table.sort(placed, function(a, b) return a.y < b.y end)

    local fields = {}
    local width, height = Terrain.decorationSize[1], Terrain.decorationSize[2]
    for index, item in ipairs(placed) do
        local slot = index * 5 - 5
        fields[slot + 1], fields[slot + 2] = item.x, item.y
        fields[slot + 3], fields[slot + 4] = width * item.scale, height * item.scale
        fields[slot + 5] = item.decoration * Terrain.decorationCell + Terrain.decorationInset
    end
    return fields
end

return Terrain
