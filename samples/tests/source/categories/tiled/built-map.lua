-- A map built in code with `tiled.newMap`: a ground layer filled from a grid of generated tiles and a water layer painted with a corner Wang set, whose shores follow the corners each stroke touches, described again as Tiled JSON with `map:toJson`.
local haylen = require('haylen')
local input = require('haylen.input')
local json = require('json')
local m = require('haylen.math')
local spatial2d = require('haylen.spatial2d')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')

local BuiltMap = haylen.class('BuiltMap', MapTest)

BuiltMap.pathRow = 5

BuiltMap.code = [[
local map = tiled.newMapRenderer(tiled.newMap({width = 40, height = 20, tileWidth = 32, tileHeight = 32}))
local first = map:addTileset(terrain)  -- {image = 'tiled/tiles/terrain.png', tileWidth = 128, renderGridSize = true}
map:addLayer({name = 'ground'})
map:fill('ground', grid, first)  -- A CellGrid of local tile ids.
map:paintTerrain('paths', 'path', column, row, 1)  -- Paints a corner Wang set, and 0 erases.
local text = json.encode(map:toJson('tiled/maps/built.tmj'))]]

local kColumns, kRows, kTile = 40, 20, 32
local kGround, kPaths, kWater = 'ground', 'paths', 'water'

-- The shore tiles number their corners under water as bits, 1 top right, 2 bottom right, 4 bottom left and 8 top left, and a Wang id lists the edges and corners clockwise from the top edge.
function BuiltMap.shoreTiles()
    local tiles = {}
    for tile = 1, 15 do
        local function corner(bit)
            return (tile & bit) ~= 0 and 1 or 0
        end
        tiles[#tiles + 1] = {tile = tile, wangId = {0, corner(1), 0, corner(2), 0, corner(4), 0, corner(8)}}
    end
    return tiles
end

-- The paths of the transitions, sixteen tiles a row with the paths in the row "pathRow", number their corners under the path as bits, 1 top left, 2 top right, 4 bottom right and 8 bottom left.
function BuiltMap.pathTiles()
    local tiles = {}
    for mask = 1, 15 do
        local function corner(bit)
            return (mask & bit) ~= 0 and 1 or 0
        end
        tiles[#tiles + 1] = {tile = BuiltMap.pathRow * 16 + mask, wangId = {0, corner(2), 0, corner(4), 0, corner(8), 0, corner(1)}}
    end
    return tiles
end

-- The tilesets of the painted tiles, which hold four pixels for every pixel of the grid in gutters of their edge pixels and draw at the size of the grid.
function BuiltMap.painted(name, wangSets)
    return {name = name, image = 'tiled/tiles/' .. name .. '.png', filter = 'linear', tileWidth = kTile * 4, tileHeight = kTile * 4, margin = 2, spacing = 4, renderGridSize = true, wangSets = wangSets}
end

function BuiltMap:enter()
    self.seed = 5
    self.erasing = false
    self:build()
    self:frame{
        hint = 'Hold the pointer on the map to paint water, whose shores follow the painted corners. The toggle erases instead, and R or the X button generates new ground.',
        code = BuiltMap.code,
        controls = {
            ui.toggle{id = 'erase', text = 'Erase water', onChange = function(event) self.erasing = event.checked end},
            ui.button{id = 'generate', text = 'Generate new ground', onClick = function() self:generate() end},
            ui.button{id = 'describe', text = 'Describe as Tiled JSON', onClick = function() self:describe() end},
        },
        view = {kColumns * kTile + 64, kRows * kTile + 64},
        focus = 'generate',
    }
    self.camera:snapTo(kColumns * kTile / 2, kRows * kTile / 2)
end

function BuiltMap:build()
    self.map = tiled.newMapRenderer(tiled.newMap({width = kColumns, height = kRows, tileWidth = kTile, tileHeight = kTile}))
    self.terrain = self.map:addTileset(BuiltMap.painted('terrain'))
    self.map:addTileset(BuiltMap.painted('transitions', {
        {name = 'path', type = 'corner', colors = {'path'}, tiles = BuiltMap.pathTiles()},
    }))
    self.map:addTileset(BuiltMap.painted('shore', {
        {name = 'water', type = 'corner', colors = {'water'}, tiles = BuiltMap.shoreTiles()},
    }))
    self.map:addLayer({name = kGround})
    self.map:addLayer({name = kPaths})
    self.map:addLayer({name = kWater})
    self.grid = spatial2d.newCellGrid(kColumns, kRows)
    self.empty = spatial2d.newCellGrid(kColumns, kRows, -1)
    self.strokes = 0
    self:generate()
    -- A lake to begin with, painted cell by cell like a stroke of the pointer.
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            if ((column - 29) / 6.5) ^ 2 + ((row - 9) / 4.5) ^ 2 < 1 then
                self.map:paintTerrain(kWater, 'water', column, row, 1)
                self.strokes = self.strokes + 1
            end
        end
    end
end

-- Fills the ground in one call, grass and grass with flowers as local ids of the terrain tileset, and paints the dirt paths where the noise rises with the corner Wang set of the paths.
function BuiltMap:generate()
    self.seed = self.seed + 1
    local noise = m.noise(self.seed)
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            self.grid:set(column, row, (column * 7 + row * 3) % 9 == 0 and 1 or 0)
        end
    end
    self.map:fill(kGround, self.grid, self.terrain)
    self.map:fill(kPaths, self.empty, self.terrain)
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            if noise:fractal(column * 0.12, row * 0.12, 3, 2, 0.5) > 0.32 then
                self.map:paintTerrain(kPaths, 'path', column, row, 1)
            end
        end
    end
    self.description = nil
end

function BuiltMap:describe()
    local text = json.encode(self.map:toJson('tiled/maps/built.tmj'))
    self.description = string.format('Its Tiled JSON holds %d bytes and %d layers.', #text, #self.map:layers())
end

function BuiltMap:update(dt)
    BuiltMap.super.update(self, dt)
    if input.pressed('reset') then
        self:generate()
    end
    if self.pointer.down then
        local column, row = self.map:worldToCell(self.pointer.worldX, self.pointer.worldY)
        if column >= 0 and row >= 0 and column < kColumns and row < kRows and (column ~= self.lastColumn or row ~= self.lastRow) then
            self.map:paintTerrain(kWater, 'water', column, row, self.erasing and 0 or 1)
            self.lastColumn, self.lastRow = column, row
            self.strokes = self.strokes + 1
        end
    else
        self.lastColumn, self.lastRow = nil, nil
    end
    self:status(string.format('The map of %d by %d cells has %d strokes of water. %s', kColumns, kRows, self.strokes, self.description or 'Describe the map to measure its Tiled JSON.'))
end

function BuiltMap:draw(area)
    self.map:draw(self.camera)
    MapTest.frameMap(self.map.pixelBounds)
end

return BuiltMap
