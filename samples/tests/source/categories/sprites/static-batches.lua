-- Static batches: a painted map of meadows, lakes, beaches, dirt roads and woods baked once to the GPU and seen through a camera that pans, under clouds that take an offset each frame to drift and move faster than the ground, compared with drawing the same ground as sprite batches every frame.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')
local Terrain = require('categories.sprites.terrain')

local StaticBatches = haylen.class('StaticBatches', SpriteTest)

StaticBatches.tile = 64
StaticBatches.columns = 60
StaticBatches.rows = 34
-- Clouds float closer to the camera than the ground, so they move by this share more than it, and drift with the wind.
StaticBatches.cloudParallax = 0.45
StaticBatches.wind = 40

StaticBatches.code = [[
local ground = graphics2d.newSpriteBatch(tiles)  -- One add per tile of the map.
local baked = ground:bake()  -- Copies the sprites to the GPU once.
graphics2d.drawStatic(baked)  -- Draws every frame without uploading.
local x = wind * time - camera.x * 0.45
graphics2d.drawStatic(clouds, x, 0, {layer = 3})  -- An offset moves the whole batch without baking again.]]

-- Returns the kind of ground at a corner of the map from noise: lakes ringed by beaches, dirt roads along the ridges of a second noise, and grass.
function StaticBatches.kindAt(lakes, roads, column, row)
    local height = lakes:fractal(column * 0.07, row * 0.07, 3, 2, 0.5)
    if height < -0.3 then
        return Terrain.water
    end
    if height < -0.22 then
        return Terrain.sand
    end
    if math.abs(roads:fractal(column * 0.05, row * 0.05, 2, 2, 0.5)) < 0.045 then
        return Terrain.path
    end
    return Terrain.grass
end

function StaticBatches:enter()
    local size, columns, rows = StaticBatches.tile, StaticBatches.columns, StaticBatches.rows
    local lakes, roads = m.noise(3), m.noise(11)
    local function kindAt(column, row)
        return StaticBatches.kindAt(lakes, roads, column, row)
    end
    self.grassBatch, self.edgesBatch = Terrain.batches(columns, rows, size, 0, 0, kindAt)
    self.grass, self.edges = self.grassBatch:bake(), self.edgesBatch:bake()
    self.trees = self:plant(kindAt)
    self.clouds, self.shadows = self:sky()
    self.useBatch = false
    self.time = 0
    self:frame{
        code = StaticBatches.code,
        hint = 'The ground is baked once and uploads nothing while the camera pans. Draw it as sprite batches instead to see the bytes it would upload every frame. The clouds move faster than the ground, and their shadows stay on it.',
        controls = {ui.toggle{id = 'batch', text = 'Draw as sprite batches', onChange = function(event) self.useBatch = event.checked end}},
        focus = 'batch',
    }
end

-- Bakes the woods: trees and pines on grass cells away from the water and the roads, added from the top down so the lower trees cover the ones behind them.
function StaticBatches:plant(kindAt)
    local size = StaticBatches.tile
    local area = {size, size, (StaticBatches.columns - 2) * size, (StaticBatches.rows - 2) * size}
    local points = m.poissonDisk({area = area, minimumDistance = 150, seed = 6, accept = function(point)
        local column, row = math.floor(point.x / size), math.floor(point.y / size)
        for dy = 0, 1 do
            for dx = 0, 1 do
                if kindAt(column + dx, row + dy) ~= Terrain.grass then
                    return false
                end
            end
        end
        return true
    end})
    table.sort(points, function(a, b) return a.y < b.y end)
    local trees = graphics2d.newSpriteBatch(assets.texture('tiled/tiles/props.png', Terrain.textureOptions))
    for index, point in ipairs(points) do
        local source = index % 3 == 0 and {134, 2, 128, 256} or {2, 2, 128, 256}
        trees:add({x = point.x, y = point.y, width = 96, height = 192, pivotY = 0.94, source = source, flipHorizontal = index % 2 == 0})
    end
    return trees:bake()
end

-- Bakes the clouds and their shadows, which stay on the ground. The clouds of one map width repeat on both sides, so the drift wraps around without a jump, and reach past the bottom of the map for the parallax.
function StaticBatches:sky()
    local texture = assets.texture('tiled/images/clouds.png', {filter = 'linear'})
    local clouds, shadows = graphics2d.newSpriteBatch(texture), graphics2d.newSpriteBatch(texture)
    local mapWidth, mapHeight = StaticBatches.columns * StaticBatches.tile, StaticBatches.rows * StaticBatches.tile
    local points = m.poissonDisk({area = {0, -300, mapWidth, mapHeight * 1.6}, minimumDistance = 700, seed = 4})
    for copy = -1, 1 do
        for index, point in ipairs(points) do
            local scale = 1.6 + (index % 4) * 0.35
            local x = point.x + copy * mapWidth
            local width, height = texture.width * scale, texture.height * scale
            clouds:add({x = x, y = point.y, width = width, height = height, color = '#E8FFFFFF'})
            shadows:add({x = x + 90, y = point.y + 160, width = width, height = height, color = '#30102018'})
        end
    end
    return clouds:bake(), shadows:bake()
end

function StaticBatches:update(dt)
    StaticBatches.super.update(self, dt)
    self.time = self.time + dt
    if self.area then
        local rangeX = StaticBatches.columns * StaticBatches.tile - self.area.width
        local rangeY = StaticBatches.rows * StaticBatches.tile - self.area.height
        self.camera.position = {(math.sin(self.time * 0.12) * 0.5 + 0.5) * rangeX, (math.sin(self.time * 0.09) * 0.5 + 0.5) * rangeY}
    end
    local tiles = self.grass:size() + self.edges:size()
    local kilobytes = math.floor(graphics2d.stats().uploadedBytes / 1024 + 0.5)
    if self.useBatch then
        self:status(string.format('The frame uploads %d kilobytes, most of them for the %d tiles of the ground drawn as sprite batches.', kilobytes, tiles))
    else
        self:status(string.format('The frame uploads %d kilobytes, none of them for the %d tiles of the ground baked once.', kilobytes, tiles))
    end
end

function StaticBatches:draw(area)
    if self.useBatch then
        self.grassBatch:draw()
        self.edgesBatch:draw()
    else
        graphics2d.drawStatic(self.grass)
        graphics2d.drawStatic(self.edges)
    end
    graphics2d.drawStatic(self.trees, 0, 0, {layer = 1})
    local drift = (StaticBatches.wind * self.time) % (StaticBatches.columns * StaticBatches.tile)
    graphics2d.drawStatic(self.shadows, drift, 0, {layer = 2})
    graphics2d.drawStatic(self.clouds, drift - self.camera.x * StaticBatches.cloudParallax, -self.camera.y * StaticBatches.cloudParallax, {layer = 3})
end

return StaticBatches
