-- The five map orientations of Tiled drawn by the same `map:draw`, with the cell under the pointer found by `map:worldToCell` and outlined from `map:cellToWorld`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')

local Orientations = haylen.class('Orientations', MapTest)

Orientations.code = [[
local map = tiled.newMapRenderer(assets.load('tiled/maps/' .. name .. '.tmj', nil, {filter = 'linear'}))
print(map.orientation, map.width, map.height, map.tileWidth, map.tileHeight)
print(map.staggerX, map.staggerEven, map.hexSideLength, map.skewX)  -- What staggered, hexagonal and oblique maps add.
local column, row = map:worldToCell(x, y)  -- The nearest cell center on staggered and hexagonal maps.
local cornerX, cornerY = map:cellToWorld(column, row)
map:draw(camera)]]

local kMaps = {
    {id = 'orthogonal', text = 'Orthogonal'},
    {id = 'isometric', text = 'Isometric'},
    {id = 'staggered', text = 'Staggered isometric'},
    {id = 'hexagonal', text = 'Hexagonal'},
    {id = 'oblique', text = 'Oblique'},
}
local kMargin = 40

function Orientations:enter()
    self.maps = {}
    for _, entry in ipairs(kMaps) do
        self.maps[entry.id] = MapTest.loadMap('tiled/maps/' .. entry.id .. '.tmj')
    end
    self:frame{
        hint = 'Move the pointer over the map to see the cell under it. Q and E or the shoulder buttons switch the map.',
        code = Orientations.code,
        controls = {
            ui.radioGroup{id = 'map', items = kMaps, selected = 'orthogonal', onChange = function(event) self:show(event.value) end},
            ui.label{id = 'details', text = '', color = 'textMuted'},
        },
        focus = 'map',
    }
    self:show('orthogonal')
end

function Orientations:show(id)
    self.current = id
    self.map = self.maps[id]
    local bounds = self.map.pixelBounds
    self.view = {bounds.width + kMargin * 2, bounds.height + kMargin * 2}
    self.camera:snapTo(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2)
    self:details(self:describe())
end

-- The fields of the orientation that shows, without those that only other orientations use.
function Orientations:describe()
    local map = self.map
    local lines = {string.format('Orientation "%s"', map.orientation), string.format('%d by %d cells of %d by %d', map.width, map.height, map.tileWidth, map.tileHeight)}
    if map.orientation == 'staggered' or map.orientation == 'hexagonal' then
        lines[#lines + 1] = string.format('Staggered along %s, the %s rows shifted', map.staggerX and 'x' or 'y', map.staggerEven and 'even' or 'odd')
    end
    if map.orientation == 'hexagonal' then
        lines[#lines + 1] = string.format('Hexagon sides of %d', map.hexSideLength)
    end
    if map.orientation == 'oblique' then
        lines[#lines + 1] = string.format('Skewed by %d, %d', map.skewX, map.skewY)
    end
    return table.concat(lines, '\n')
end

function Orientations:cycle(step)
    for index, entry in ipairs(kMaps) do
        if entry.id == self.current then
            local following = kMaps[(index - 1 + step) % #kMaps + 1].id
            self:set('map', {selected = following})
            self:show(following)
            return
        end
    end
end

function Orientations:update(dt)
    Orientations.super.update(self, dt)
    if input.pressed('next') or input.pressed('previous') then
        self:cycle(input.pressed('next') and 1 or -1)
    end
    local map = self.map
    self.column, self.row = map:worldToCell(self.pointer.worldX, self.pointer.worldY)
    self.inside = self.column >= 0 and self.row >= 0 and self.column < map.width and self.row < map.height
    if not self.inside then
        self:status('Point at a cell of the map to find it.')
        return
    end
    local x, y = map:cellToWorld(self.column, self.row)
    self:status(string.format('The pointer is on the cell %d, %d, whose corner is at %.0f, %.0f.', self.column, self.row, x, y))
end

-- Orthogonal, isometric and oblique cells share their corners with their neighbors, so four calls of `cellToWorld` outline them. Staggered and hexagonal cells sit in their bounding boxes.
function Orientations:cellOutline()
    local map, column, row = self.map, self.column, self.row
    local width, height = map.tileWidth, map.tileHeight
    if map.orientation == 'staggered' or map.orientation == 'hexagonal' then
        local x, y = map:cellToWorld(column, row)
        if map.orientation == 'staggered' then
            return {{x + width / 2, y}, {x + width, y + height / 2}, {x + width / 2, y + height}, {x, y + height / 2}}
        end
        local rise = (height - map.hexSideLength) / 2
        return {{x + width / 2, y}, {x + width, y + rise}, {x + width, y + height - rise}, {x + width / 2, y + height}, {x, y + height - rise}, {x, y + rise}}
    end
    local points = {}
    for _, corner in ipairs({{0, 0}, {1, 0}, {1, 1}, {0, 1}}) do
        points[#points + 1] = {map:cellToWorld(column + corner[1], row + corner[2])}
    end
    return points
end

function Orientations:draw(area)
    self.map:draw(self.camera)
    if self.map.orientation == 'orthogonal' then
        MapTest.frameMap(self.map.pixelBounds)
    end
    if self.inside then
        local unit = graphics2d.canvasUnitSize()
        local points = self:cellOutline()
        graphics2d.drawPolygon(points, '#40FFC93C', {layer = 6})
        graphics2d.drawPolyline(points, 4 * unit, '#FFFFC93C', true, {layer = 7})
    end
end

return Orientations
