-- An infinite map whose tile layers are stored as chunks, some at negative cells, drawn with culling as the camera pans over it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')
local Pan = require('categories.tiled.pan')

local Infinite = haylen.class('Infinite', MapTest)

Infinite.code = [[
local map = tiled.newMapRenderer(assets.load('tiled/maps/infinite.tmj', nil, {filter = 'linear'}))
for _, chunk in ipairs(map:layer('ground').chunks) do print(chunk.x, chunk.y, chunk.width, chunk.height) end
local column, row = map:worldToCell(x, y)  -- Negative left of and above the origin.
local gid = tiled.tileId(map:tile('ground', column, row))
map:draw(camera)  -- Draws only the chunks the camera sees.]]

function Infinite:enter()
    self.map = MapTest.loadMap('tiled/maps/infinite.tmj')
    self.chunks = self.map:layer('ground').chunks
    self.empty = nil
    self:frame{
        hint = 'Drag the map, or use WASD, the arrows, the directional pad or the left stick, and the mouse wheel zooms. The cell under the pointer can be negative, and R or the X button comes back to the origin.',
        code = Infinite.code,
        controls = {
            ui.checkbox{id = 'chunks', text = 'Show the chunks', checked = true, onChange = function(event) self.showChunks = event.checked end},
            ui.row{wrap = true,
                ui.button{id = 'in', text = 'Zoom in', onClick = function() self.pan:zoom(1.25) end},
                ui.button{id = 'out', text = 'Zoom out', onClick = function() self.pan:zoom(0.8) end},
            },
            ui.button{id = 'origin', text = 'Back to the origin', onClick = function() self:home() end},
            ui.sectionTitle{text = 'Layers stored in chunks'},
            ui.label{id = 'details', text = '', color = 'textMuted'},
        },
        view = {1400, 820},
        play = true,
    }
    self.showChunks = true
    local lines = {}
    for _, layer in ipairs(self.map:layers()) do
        lines[#lines + 1] = string.format('Layer "%s" in %d chunks', layer.name, #layer.chunks)
    end
    self:details(table.concat(lines, '\n'))
    self.pan = Pan(self)
    self:home()
end

function Infinite:home()
    self.zoomScale = 1
    self.camera:snapTo(0, 0)
end

function Infinite:update(dt)
    Infinite.super.update(self, dt)
    if input.pressed('reset') then
        self:home()
    end
    self.pan:update(dt)
    local column, row = self.map:worldToCell(self.pointer.worldX, self.pointer.worldY)
    local gid = self.map:tile('ground', column, row)
    self:status(string.format('The map holds %d chunks of 16 by 16 cells. The cell %d, %d under the pointer %s.', #self.chunks, column, row, gid == 0 and 'is in no chunk' or string.format('holds the tile %d', tiled.tileId(gid))))
end

-- The places of the chunks the map holds, and of the empty places between them in the box they span, which draw as hatched slots.
function Infinite:slots()
    local held, left, top, right, bottom = {}, math.huge, math.huge, -math.huge, -math.huge
    for _, chunk in ipairs(self.chunks) do
        held[chunk.x .. ',' .. chunk.y] = true
        left, top = math.min(left, chunk.x), math.min(top, chunk.y)
        right, bottom = math.max(right, chunk.x + chunk.width), math.max(bottom, chunk.y + chunk.height)
    end
    local size = self.chunks[1].width
    local empty = {}
    for y = top, bottom - size, size do
        for x = left, right - size, size do
            if not held[x .. ',' .. y] then
                empty[#empty + 1] = {x = x, y = y, size = size}
            end
        end
    end
    return empty
end

function Infinite:draw(area)
    local map = self.map
    local unit = graphics2d.canvasUnitSize()
    map:draw(self.camera)
    if self.showChunks then
        self.empty = self.empty or self:slots()
        for _, slot in ipairs(self.empty) do
            local x, y = map:cellToWorld(slot.x, slot.y)
            local side = slot.size * map.tileWidth
            graphics2d.drawShape({x + 6 * unit, y + 6 * unit, side - 12 * unit, side - 12 * unit}, {radius = 18 * unit, color = '#FF1C2130', borderWidth = 3 * unit, borderColor = '#FF3A4066', layer = 4})
            for stripe = 1, 7 do
                local offset = side * stripe / 8
                graphics2d.drawLine(x + offset, y + 10 * unit, x + 10 * unit, y + offset, 3 * unit, '#FF262C3C', {layer = 4})
                graphics2d.drawLine(x + side - 10 * unit, y + offset, x + offset, y + side - 10 * unit, 3 * unit, '#FF262C3C', {layer = 4})
            end
            MapTest.pill('No chunk here', x + side / 2, y + side / 2, {anchor = {0.5, 0.5}, size = 22, layer = 5})
        end
        for _, chunk in ipairs(self.chunks) do
            local x, y = map:cellToWorld(chunk.x, chunk.y)
            graphics2d.drawRectOutline({x, y, chunk.width * map.tileWidth, chunk.height * map.tileHeight}, 3 * unit, '#B3FFC93C', {layer = 5})
            MapTest.pill(string.format('Chunk %d, %d', chunk.x, chunk.y), x + 10 * unit, y + 10 * unit, {anchor = {0, 0}, size = 18})
        end
    end
    graphics2d.drawShape({-9 * unit, -9 * unit, 18 * unit, 18 * unit}, {kind = 'ellipse', color = '#FFF0473E', borderWidth = 3 * unit, borderColor = MapTest.ink, layer = 11})
    MapTest.pill('Cell 0, 0', 0, -14 * unit, {size = 18, layer = 11})
end

return Infinite
