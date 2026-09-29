-- The five map orientations of Tiled drawn by the same map:draw, with the cell under the pointer found by map:worldToCell and outlined from map:cellToWorld.
local haylen = require('haylen')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local sample = require('sample')

local Orientations = haylen.class('Orientations', sample.Test)

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
        self.maps[entry.id] = tiled.newMapRenderer(assets.load('maps/' .. entry.id .. '.tmj'))
    end
    Orientations.super.enter(self, {
        hint = 'Move the pointer over the map to see the cell under it. Q and E or the shoulder buttons switch the map.',
        controls = {
            ui.radioGroup{id = 'map', items = kMaps, selected = 'orthogonal', onChange = function(event) self:show(event.value) end},
        },
        stats = true,
        focus = 'map',
    })
    self:show('orthogonal')
end

function Orientations:show(id)
    self.current = id
    self.map = self.maps[id]
    local bounds = self.map.bounds
    self.viewSize = {bounds.width + kMargin * 2, bounds.height + kMargin * 2}
    self.camera:snapTo(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2)
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

function Orientations:exit()
    Orientations.super.exit(self)
    self.maps, self.map = nil, nil
end

function Orientations:update(dt)
    Orientations.super.update(self, dt)
    if input.pressed('next') or input.pressed('previous') then
        self:cycle(input.pressed('next') and 1 or -1)
    end
    local map = self.map
    self.column, self.row = map:worldToCell(self.pointer.worldX, self.pointer.worldY)
    local x, y = map:cellToWorld(self.column, self.row)
    self:showStats(string.format('orientation %s\nsize %d x %d cells\ntile %d x %d\ncell %d, %d\ncell corner %.0f, %.0f\nstagger %s, %s\nhex side %d\nskew %d, %d', map.orientation, map.width, map.height, map.tileWidth, map.tileHeight, self.column, self.row, x, y, map.staggerX and 'x' or 'y', map.staggerEven and 'even' or 'odd', map.hexSideLength, map.skewX, map.skewY))
end

-- Orthogonal, isometric and oblique cells share their corners with their neighbors, so four calls of cellToWorld outline them. Staggered and hexagonal cells sit in their bounding boxes.
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

function Orientations:render()
    self:beginWorld()
    self.map:draw(self.camera)
    graphics2d.drawRectOutline(self.map.bounds, 2 * graphics2d.canvasUnitSize(), '#66FFFFFF', {layer = 5})
    graphics2d.drawPolyline(self:cellOutline(), 3 * graphics2d.canvasUnitSize(), '#FFFFD54F', true, {layer = 6})
end

return Orientations
