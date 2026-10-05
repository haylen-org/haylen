-- The five map orientations of Tiled drawn by the same `map:draw`, with the cell under the pointer found by `map:worldToCell` and outlined from `map:cellToWorld`.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')

local Orientations = haylen.class('Orientations', MapTest)

local kMaps = {
    {id = 'orthogonal', text = 'Orthogonal'},
    {id = 'isometric', text = 'Isometric'},
    {id = 'staggered', text = 'Staggered isometric'},
    {id = 'hexagonal', text = 'Hexagonal'},
    {id = 'oblique', text = 'Oblique'},
}
local kMargin = 48

function Orientations:enter()
    self.maps = {}
    for _, entry in ipairs(kMaps) do
        self.maps[entry.id] = tiled.newMapRenderer(assets.load('tiled/maps/' .. entry.id .. '.tmj'))
    end
    self:frame{
        hint = 'Move the pointer over the map to see the cell under it. Q and E or the shoulder buttons switch the map.',
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
    local map = self.map
    self:details(string.format('Orientation "%s"\nSize %d by %d cells\nTile %d by %d\nStagger axis "%s", index "%s"\nHex side %d\nSkew %d, %d', map.orientation, map.width, map.height, map.tileWidth, map.tileHeight, map.staggerX and 'x' or 'y', map.staggerEven and 'even' or 'odd', map.hexSideLength, map.skewX, map.skewY))
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
    local x, y = map:cellToWorld(self.column, self.row)
    self:status(string.format('Cell %d, %d, its corner at %.0f, %.0f', self.column, self.row, x, y))
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
    graphics2d.drawRectOutline(self.map.pixelBounds, 2 * graphics2d.canvasUnitSize(), '#66FFFFFF', {layer = 5})
    graphics2d.drawPolyline(self:cellOutline(), 3 * graphics2d.canvasUnitSize(), '#FFFFD54F', true, {layer = 6})
end

return Orientations
