-- World map: islands made from a seed in a job that shares the frame with the app. Layered noise with a mask around a few centers gives the height, more noise gives the moisture, and the two pick a biome for every cell, shaded by the slope. Rivers start in wet highlands and run downhill to the sea, towns with generated names settle on low land, and roads join them along the cheapest paths, around the water and over the hills.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local jobs = require('haylen.jobs')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local names = require('categories.procedural.names')

local WorldMap = haylen.class('WorldMap', AlgorithmTest)

WorldMap.code = [[
local height = noise:fractal(x / 120, y / 120, 5) * 0.55 + islandMask(x, y) - 0.45  -- Above the sea level is land.
local biome = pickBiome(height, moisture:fractal(x / 160, y / 160, 4))  -- Desert, grass, forest, rock or snow.
river = downhillFrom(wetHighland)  -- Each step goes to the lowest neighbor until it meets the sea.
local road = grid:findPath(townA.x, townA.y, townB.x, townB.y)  -- Water is blocked, hills cost more.]]

WorldMap.columns, WorldMap.rows, WorldMap.cell = 400, 216, 4
WorldMap.regenerateDelay = 0.25
WorldMap.water = {deep = m.color('#FF1B3A5E'), shallow = m.color('#FF3577A3')}
WorldMap.biomes = {
    beach = m.color('#FFE6D3A0'), desert = m.color('#FFDCC27C'), grass = m.color('#FF8EC25C'), forest = m.color('#FF4F9A48'), jungle = m.color('#FF2E7A40'),
    savanna = m.color('#FFB9AA6A'), woods = m.color('#FF3F7A40'), taiga = m.color('#FF3D6A50'), rock = m.color('#FF8D877C'), snow = m.color('#FFF2F4F6'),
}
WorldMap.neighbors = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}

function WorldMap:enter()
    self.options = {seed = 7, sea = 0, islands = 3, towns = 8, rivers = true, roads = true, names = true}
    self.wait = 0
    self:frame{
        code = WorldMap.code,
        hint = 'Every change makes a new world in a job that shares each frame with the app, while the old one stays on screen. R or the X button takes a new seed.',
        controls = {
            ui.formField{label = 'Sea level', ui.slider{id = 'sea', min = -0.25, max = 0.25, step = 0.01, value = 0, showValue = true, onChange = function(event) self:change('sea', event.value) end}},
            ui.formField{label = 'Islands', ui.slider{id = 'islands', min = 1, max = 5, step = 1, value = 3, showValue = true, decimals = 0, onChange = function(event) self:change('islands', math.tointeger(event.value)) end}},
            ui.formField{label = 'Towns', ui.slider{id = 'towns', min = 2, max = 14, step = 1, value = 8, showValue = true, decimals = 0, onChange = function(event) self:change('towns', math.tointeger(event.value)) end}},
            ui.checkbox{id = 'rivers', text = 'Rivers', checked = true, onChange = function(event) self.options.rivers = event.checked end},
            ui.checkbox{id = 'roads', text = 'Roads', checked = true, onChange = function(event) self.options.roads = event.checked end},
            ui.checkbox{id = 'names', text = 'Names of the towns', checked = true, onChange = function(event) self.options.names = event.checked end},
            ui.button{id = 'seed', text = 'New world', variant = 'primary', onClick = function() self:change('seed', self.options.seed + 1) end},
        },
    }
    self:generate()
end

function WorldMap:change(name, value)
    self.options[name] = value
    self.dirty = true
end

-- Generates in a job and shows the world when it is ready, unless a newer request replaced it.
function WorldMap:generate()
    self.request = (self.request or 0) + 1
    local request = self.request
    local options = {seed = self.options.seed, sea = self.options.sea, islands = self.options.islands, towns = self.options.towns}
    local started = haylen.elapsed()
    local promise = jobs.spawn(function() return WorldMap.build(options) end, {owner = self})
    self:spawn(function()
        local world = promise:await()
        if world and request == self.request then
            world.milliseconds = (haylen.elapsed() - started) * 1000
            world.texture = graphics.newTexture(WorldMap.columns, WorldMap.rows, {pixels = world.pixels, filter = 'linear'})
            world.pixels = nil
            self.world = world
            self:log('World %d: %d%% land, %d rivers, %d towns, ready in %.0f ms.', options.seed, world.land, #world.rivers, #world.towns, world.milliseconds)
        end
    end)
end

-- Builds the whole world for the options, checking in with the job budget after every row.
function WorldMap.build(options)
    local columns, rows = WorldMap.columns, WorldMap.rows
    local random = m.random(options.seed)
    local relief, wetness = m.noise(options.seed), m.noise(options.seed + 1000)
    local centers = {}
    for _ = 1, options.islands do
        centers[#centers + 1] = {random:range(0.2, 0.8) * columns, random:range(0.25, 0.75) * rows, random:range(0.22, 0.38) * columns}
    end

    local heights, moistures = {}, {}
    for row = 0, rows - 1 do
        for column = 0, columns - 1 do
            local mask = 0
            for _, center in ipairs(centers) do
                local distance = math.sqrt((column - center[1]) ^ 2 + (row - center[2]) ^ 2) / center[3]
                mask = math.max(mask, 1 - distance)
            end
            local index = row * columns + column + 1
            heights[index] = relief:fractal(column / 120, row / 120, 5) * 0.55 + mask * 0.9 - 0.45 - options.sea
            moistures[index] = wetness:fractal(column / 160, row / 160, 4) * 0.5 + 0.5
        end
        jobs.checkpoint()
    end

    local world = {heights = heights}
    world.pixels, world.land = WorldMap.paint(heights, moistures)
    jobs.checkpoint()
    world.rivers = WorldMap.rivers(heights, moistures, random)
    world.towns = WorldMap.settle(heights, world.rivers, options.towns, random)
    world.roads = WorldMap.roads(heights, world.rivers, world.towns)
    return world
end

-- Returns the color of a cell of land from its height over the sea and its moisture.
function WorldMap.biome(height, moisture)
    local biomes = WorldMap.biomes
    if height < 0.03 then
        return biomes.beach
    elseif height < 0.3 then
        return moisture < 0.3 and biomes.desert or moisture < 0.55 and biomes.grass or moisture < 0.75 and biomes.forest or biomes.jungle
    elseif height < 0.5 then
        return moisture < 0.35 and biomes.savanna or moisture < 0.65 and biomes.woods or biomes.taiga
    end
    return height < 0.68 and biomes.rock or biomes.snow
end

-- Paints one pixel per cell, lit from the top left by the slope, and returns the pixels with the share of land.
function WorldMap.paint(heights, moistures)
    local columns, rows = WorldMap.columns, WorldMap.rows
    local bytes, land, char = {}, 0, string.char
    for row = 0, rows - 1 do
        for column = 0, columns - 1 do
            local index = row * columns + column + 1
            local height = heights[index]
            local color
            if height < 0 then
                color = WorldMap.water.deep:lerp(WorldMap.water.shallow, m.clamp(1 + height * 4, 0, 1))
            else
                land = land + 1
                local below = heights[math.min(row + 1, rows - 1) * columns + math.min(column + 1, columns - 1) + 1]
                local light = m.clamp(1 + (height - math.max(below, 0)) * 12, 0.7, 1.3)
                local base = WorldMap.biome(height, moistures[index])
                color = m.color(m.clamp(base.r * light, 0, 1), m.clamp(base.g * light, 0, 1), m.clamp(base.b * light, 0, 1), 1)
            end
            bytes[#bytes + 1] = char(math.floor(color.r * 255 + 0.5), math.floor(color.g * 255 + 0.5), math.floor(color.b * 255 + 0.5), 255)
        end
    end
    return table.concat(bytes), math.floor(land / (columns * rows) * 100 + 0.5)
end

-- Starts rivers in wet highlands and walks each to its lowest neighbor until it reaches the sea, a lake where no neighbor is lower, or another river.
function WorldMap.rivers(heights, moistures, random)
    local columns, rows = WorldMap.columns, WorldMap.rows
    local rivers, wet = {}, {}
    for _ = 1, 800 do
        if #rivers >= 12 then
            break
        end
        local column, row = random:integer(1, columns - 2), random:integer(1, rows - 2)
        local index = row * columns + column + 1
        if heights[index] > 0.3 and moistures[index] > 0.45 and not wet[index] then
            local river = {}
            while true do
                river[#river + 1] = {x = column, y = row}
                wet[row * columns + column + 1] = true
                local lowest, nextColumn, nextRow = heights[row * columns + column + 1], nil, nil
                for _, step in ipairs(WorldMap.neighbors) do
                    local c, r = column + step[1], row + step[2]
                    if c >= 0 and r >= 0 and c < columns and r < rows and heights[r * columns + c + 1] < lowest then
                        lowest, nextColumn, nextRow = heights[r * columns + c + 1], c, r
                    end
                end
                if not nextColumn or lowest < 0 or wet[nextRow * columns + nextColumn + 1] then
                    if nextColumn then
                        river[#river + 1] = {x = nextColumn, y = nextRow}
                    end
                    break
                end
                column, row = nextColumn, nextRow
            end
            if #river >= 12 then
                rivers[#rivers + 1] = river
            end
        end
    end
    rivers.wet = wet
    return rivers
end

-- Settles towns on low dry land, near water when it can, and far enough from each other.
function WorldMap.settle(heights, rivers, count, random)
    local columns, rows = WorldMap.columns, WorldMap.rows
    local towns = {}
    for _ = 1, 3000 do
        if #towns >= count then
            break
        end
        local column, row = random:integer(6, columns - 7), random:integer(6, rows - 7)
        local height = heights[row * columns + column + 1]
        local spaced = true
        for _, town in ipairs(towns) do
            spaced = spaced and (town.column - column) ^ 2 + (town.row - row) ^ 2 > 44 * 44
        end
        if spaced and height > 0.04 and height < 0.3 and not rivers.wet[row * columns + column + 1] then
            towns[#towns + 1] = {column = column, row = row, name = names.town(random)}
        end
    end
    return towns
end

-- Joins the towns into networks, each new town to the nearest town already joined that a road can reach, along the cheapest path where the sea is blocked, hills cost more and rivers cost a bridge. A town that reaches none, such as one on another island, starts a network of its own.
function WorldMap.roads(heights, rivers, towns)
    local columns, rows = WorldMap.columns, WorldMap.rows
    local grid = navigation2d.newGrid(columns, rows)
    for row = 0, rows - 1 do
        for column = 0, columns - 1 do
            local index = row * columns + column + 1
            local height = heights[index]
            if height < 0 then
                grid:setWalkable(column, row, false)
            else
                grid:setCost(column, row, 1 + math.max(0, height - 0.1) * 14 + (rivers.wet[index] and 6 or 0))
            end
        end
    end
    local roads, joined = {}, {}
    for _, town in ipairs(towns) do
        table.sort(joined, function(a, b) return (a.column - town.column) ^ 2 + (a.row - town.row) ^ 2 < (b.column - town.column) ^ 2 + (b.row - town.row) ^ 2 end)
        for _, other in ipairs(joined) do
            local path = grid:findPath(town.column, town.row, other.column, other.row)
            if path then
                roads[#roads + 1] = path
                break
            end
        end
        joined[#joined + 1] = town
        jobs.checkpoint()
    end
    return roads
end

-- Returns the flat list of world points through the centers of a list of cells, each with its `x` and `y`.
function WorldMap.points(cells)
    local points = {}
    local left, top, cell = -WorldMap.columns * WorldMap.cell / 2, -WorldMap.rows * WorldMap.cell / 2, WorldMap.cell
    for _, entry in ipairs(cells) do
        points[#points + 1] = left + (entry.x + 0.5) * cell
        points[#points + 1] = top + (entry.y + 0.5) * cell
    end
    return points
end

function WorldMap:update(dt)
    WorldMap.super.update(self, dt)
    if input.pressed('reset') then
        self:change('seed', self.options.seed + 1)
    end
    self.wait = math.max(0, self.wait - dt)
    if self.dirty and self.wait == 0 then
        self.dirty, self.wait = false, WorldMap.regenerateDelay
        self:generate()
    end
    local world = self.world
    if world then
        self:status(string.format('World %d, land %d%%, rivers %d, towns %d, roads %d, ready in %.0f ms', self.options.seed, world.land, #world.rivers, #world.towns, #world.roads, world.milliseconds))
    end
end

function WorldMap:draw(area)
    local world = self.world
    if not world then
        return
    end
    local width, height = WorldMap.columns * WorldMap.cell, WorldMap.rows * WorldMap.cell
    graphics2d.draw(world.texture, -width / 2, -height / 2, {pivotX = 0, pivotY = 0, width = width, height = height})
    if not world.riverPoints then
        world.riverPoints, world.roadPoints = {}, {}
        for _, river in ipairs(world.rivers) do
            local points = WorldMap.points(river)
            local widths = {}
            for index = 1, #points // 2 do
                widths[index] = 1.5 + index / (#points // 2) * 3.5
            end
            world.riverPoints[#world.riverPoints + 1] = {points = points, widths = widths}
        end
        for _, road in ipairs(world.roads) do
            world.roadPoints[#world.roadPoints + 1] = WorldMap.points(road)
        end
    end
    if self.options.rivers then
        for _, river in ipairs(world.riverPoints) do
            graphics2d.drawStroke(river.points, {widths = river.widths, color = '#FF3A7CB8', join = 'round', cap = 'round', layer = 1})
        end
    end
    if self.options.roads then
        for _, points in ipairs(world.roadPoints) do
            graphics2d.drawStroke(points, {width = 3, color = '#FF6A4A2C', join = 'round', cap = 'round', dashes = {10, 6}, layer = 2})
        end
    end
    local left, top, cell = -width / 2, -height / 2, WorldMap.cell
    for _, town in ipairs(world.towns) do
        local x, y = left + (town.column + 0.5) * cell, top + (town.row + 0.5) * cell
        graphics2d.drawShape({x - 9, y - 9, 18, 18}, {radius = 4, color = '#FFF4E3C1', borderWidth = 3, borderColor = '#FF4A2E1C', layer = 3})
        if self.options.names then
            graphics2d.drawText(nil, town.name, x, y - 14, {size = 22, color = '#FFFFFFFF', outlineWidth = 3, outlineColor = '#E6201812', anchor = {0.5, 1}, layer = 4})
        end
    end
end

return WorldMap
