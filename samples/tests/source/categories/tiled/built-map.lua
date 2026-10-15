-- A map built in code with `tiled.newMap`: a ground layer filled from a grid of generated tiles and a water layer painted with a corner Wang set, whose shores follow the corners each stroke touches, described again as Tiled JSON with `map:toJson`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local json = require('json')
local m = require('haylen.math')
local spatial2d = require('haylen.spatial2d')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')

local BuiltMap = haylen.class('BuiltMap', MapTest)

local kColumns, kRows, kTile = 40, 24, 32
local kGround, kWater = 'ground', 'water'

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
    self.map:addTileset(BuiltMap.painted('shore', {
        {name = 'water', type = 'corner', colors = {'water'}, tiles = BuiltMap.shoreTiles()},
    }))
    self.map:addLayer({name = kGround})
    self.map:addLayer({name = kWater})
    self.grid = spatial2d.newCellGrid(kColumns, kRows)
    self.strokes = 0
    self:generate()
end

-- Fills the ground from noise in one call: grass, grass with flowers and dirt paths, as local ids of the terrain tileset.
function BuiltMap:generate()
    self.seed = self.seed + 1
    local noise = m.noise(self.seed)
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local value = noise:fractal(column * 0.12, row * 0.12, 3, 2, 0.5)
            self.grid:set(column, row, value > 0.3 and 2 or ((column * 7 + row * 3) % 9 == 0 and 1 or 0))
        end
    end
    self.map:fill(kGround, self.grid, self.terrain)
    self.description = nil
end

function BuiltMap:describe()
    local text = json.encode(self.map:toJson('tiled/maps/built.tmj'))
    self.description = string.format('Tiled JSON of %d bytes with %d layers', #text, #self.map:layers())
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
    self:status(string.format('Cells %d x %d, painted cells %d, draw calls %d. %s', kColumns, kRows, self.strokes, graphics2d.stats().drawCalls, self.description or 'Describe the map to measure its Tiled JSON.'))
end

function BuiltMap:draw(area)
    self.map:draw(self.camera)
end

return BuiltMap
