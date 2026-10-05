-- An infinite map whose tile layers are stored as chunks, some at negative cells, drawn with culling as the camera pans over it.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')
local Pan = require('categories.tiled.pan')

local Infinite = haylen.class('Infinite', MapTest)

function Infinite:enter()
    self.map = tiled.newMapRenderer(assets.load('tiled/maps/infinite.tmj'))
    self.chunks = self.map:layer('ground').chunks
    self:frame{
        hint = 'Drag the map, or use WASD, the arrows, the directional pad or the left stick, and the mouse wheel zooms. The cell under the pointer can be negative, and R or the X button comes back to the origin.',
        controls = {
            ui.checkbox{id = 'chunks', text = 'Show the chunks', checked = true, onChange = function(event) self.showChunks = event.checked end},
            ui.row{
                ui.button{id = 'in', text = 'Zoom in', onClick = function() self.pan:zoom(1.25) end},
                ui.button{id = 'out', text = 'Zoom out', onClick = function() self.pan:zoom(0.8) end},
            },
            ui.button{id = 'origin', text = 'Back to the origin', onClick = function() self:home() end},
        },
        view = {1400, 820},
        play = true,
    }
    self.showChunks = true
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
    self:status(string.format('%s, chunks %d of 16 by 16 cells, cell %d, %d holds tile %d', self.map.infinite and 'Infinite map' or 'Finite map', #self.chunks, column, row, tiled.tileId(self.map:tile('ground', column, row))))
end

function Infinite:draw(area)
    local map = self.map
    map:draw(self.camera)
    if self.showChunks then
        local unit = graphics2d.canvasUnitSize()
        for _, chunk in ipairs(self.chunks) do
            local x, y = map:cellToWorld(chunk.x, chunk.y)
            graphics2d.drawRectOutline({x, y, chunk.width * map.tileWidth, chunk.height * map.tileHeight}, 2 * unit, '#AAFFD54F', {layer = 5})
            graphics2d.drawText(nil, string.format('%d, %d', chunk.x, chunk.y), x + 8, y + 6, {size = 18 * unit, outlineWidth = 2 * unit, layer = 5})
        end
    end
    graphics2d.drawCircle(0, 0, 6 * graphics2d.canvasUnitSize(), '#FFEF5350', {layer = 6})
end

return Infinite
