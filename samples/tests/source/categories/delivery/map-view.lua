-- Draws the small maps of the delivery tests with the painted tiles of the Tiled tests, one character per corner of the tiles: water, sand, grass and rock. Grass covers the ground, sand and water blend into it through their corner sets, the water plays its frames and rocks stand on their corners, all inside a rounded frame centered in the area.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local MapView = haylen.class('MapView')

-- The painted tiles hold 128 pixels in gutters of 2 and 4, the transitions keep one corner set per row with the corner mask as the column, and the water plays the four rows from its first one.
MapView.tile = 128
MapView.margin, MapView.spacing = 2, 4
MapView.grass, MapView.rock = 0, 19
MapView.sandRow, MapView.waterRow = 4, 0
MapView.waterFrames, MapView.waterSeconds = 4, 0.18
MapView.terrains = {['~'] = 'water', ['.'] = 'sand', ['"'] = 'grass', ['^'] = 'rock'}

function MapView:init()
    self.terrain = assets.texture('tiled/tiles/terrain.png', {filter = 'linear'})
    self.transitions = assets.texture('tiled/tiles/transitions.png', {filter = 'linear'})
    self.frame = 0
end

-- The source of a tile of an atlas of painted tiles with `columns` columns.
function MapView.source(index, columns)
    local step = MapView.tile + MapView.spacing
    return {MapView.margin + index % columns * step, MapView.margin + index // columns * step, MapView.tile, MapView.tile}
end

-- Whether the corner at `column`, `row` holds a terrain of `kinds`.
function MapView.holds(map, column, row, kinds)
    local line = map.rows[row + 1]
    return kinds[MapView.terrains[line:sub(column + 1, column + 1)]] and 1 or 0
end

-- Builds the layers of a map for a size of the tiles: the grass, the sand under the beaches and the water, and the rocks.
function MapView:build(map, size)
    local columns, rows = #map.rows[1] - 1, #map.rows - 1
    local options = {cellWidth = size, cellHeight = size}
    self.ground = graphics2d.newTileLayer(self.terrain, options)
    self.sand = graphics2d.newTileLayer(self.transitions, options)
    self.water = graphics2d.newTileLayer(self.transitions, options)
    self.ground:defineFrame(1, {source = MapView.source(MapView.grass, 8), width = size, height = size})
    for mask = 1, 15 do
        self.sand:defineFrame(mask, {source = MapView.source(MapView.sandRow * 16 + mask, 16), width = size, height = size})
    end
    self:defineWater(size)

    local sandy, wet = {sand = true, water = true, rock = true}, {water = true}
    for row = 0, rows - 1 do
        for column = 0, columns - 1 do
            self.ground:setCell(column, row, 1)
            for _, layer in ipairs({{self.sand, sandy}, {self.water, wet}}) do
                local kinds = layer[2]
                local mask = MapView.holds(map, column, row, kinds) | MapView.holds(map, column + 1, row, kinds) << 1 | MapView.holds(map, column + 1, row + 1, kinds) << 2 | MapView.holds(map, column, row + 1, kinds) << 3
                layer[1]:setCell(column, row, mask)
            end
        end
    end
    self.rocks = {}
    for row, line in ipairs(map.rows) do
        for column = 1, #line do
            if line:sub(column, column) == '^' then
                self.rocks[#self.rocks + 1] = {(column - 1) * size, (row - 1) * size}
            end
        end
    end
    self.map, self.size, self.columns, self.rows = map, size, columns, rows
end

function MapView:defineWater(size)
    for mask = 1, 15 do
        self.water:defineFrame(mask, {source = MapView.source((MapView.waterRow + self.frame) * 16 + mask, 16), width = size, height = size})
    end
end

-- Draws a map centered in the area with tiles as large as fit, or nothing for a map that is not loaded.
function MapView:draw(map, area)
    if not map then
        return
    end
    local size = math.floor(math.min(area.width * 0.86 / (#map.rows[1] - 1), area.height * 0.8 / (#map.rows - 1)))
    if map ~= self.map or size ~= self.size then
        self:build(map, size)
    end
    local frame = math.floor(haylen.elapsed() / MapView.waterSeconds) % MapView.waterFrames
    if frame ~= self.frame then
        self.frame = frame
        self:defineWater(size)
    end

    local width, height = self.columns * size, self.rows * size
    local left, top = math.floor((area.width - width) / 2), math.floor((area.height - height) / 2)
    local frameRect = {left, top, width, height}
    graphics2d.drawShape({left - 6, top + 4, width + 12, height + 12}, {radius = 24, color = '#66000000', softness = 18})
    graphics2d.pushMask(function()
        graphics2d.drawShape(frameRect, {radius = 20})
    end)
    self.ground:draw({layer = 0}, left, top)
    self.sand:draw({layer = 1}, left, top)
    self.water:draw({layer = 2}, left, top)
    for _, rock in ipairs(self.rocks) do
        graphics2d.draw(self.terrain, left + rock[1], top + rock[2] + size * 0.15, {source = MapView.source(MapView.rock, 8), width = size * 1.1, height = size * 1.1, layer = 3})
    end
    graphics2d.popMask()
    graphics2d.drawShape(frameRect, {radius = 20, color = '#00000000', borderWidth = 4, borderColor = '#FF1E1B33', layer = 4})
end

return MapView
