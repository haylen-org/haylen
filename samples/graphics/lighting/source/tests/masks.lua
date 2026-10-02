-- Light masks and layer ranges: a light reaches a draw when the light mask of the draw shares a bit with the item mask of the light and the layer of the draw lies inside the range of the light.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local sample = require('sample')

local Masks = haylen.class('Masks', sample.Test)

Masks.hints = 'Columns have the light masks 1, 2 and 4, and rows sit on the layers 1, 2 and 3. Move the light with the mouse, a finger, WASD or the left stick.'

-- The floor has the mask 8, which only the fill light reaches, so it stays the same whatever the test light selects.
Masks.floorMask = 8

function Masks:init(entry)
    Masks.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.cursor = sample.Cursor()
    self.bits = {true, true, true}
    self.fill = lighting2d.newLight({type = 'directional', intensity = 0.25, itemMask = Masks.floorMask})
    self.lamp = lighting2d.newLight({radius = 1400, color = '#FFFFF0D0', intensity = 1.4, itemMask = 7, layerMin = 1, layerMax = 3})
end

function Masks:controls()
    local checks = {}
    for bit = 1, 3 do
        checks[bit] = ui.checkbox{text = 'Mask ' .. (1 << (bit - 1)), checked = true, onChange = function(event)
            self.bits[bit] = event.checked
            self:applyMask()
        end}
    end
    local layers = {{id = '1', text = '1'}, {id = '2', text = '2'}, {id = '3', text = '3'}}
    return {
        ui.formField{label = 'Item mask of the light', ui.column{gap = 8, children = checks}},
        ui.formField{label = 'Lowest layer', ui.combo{id = 'layerMin', items = layers, selected = '1', onChange = function(event)
            self:setLayers(tonumber(event.value), math.max(tonumber(event.value), self.lamp.layerMax))
        end}},
        ui.formField{label = 'Highest layer', ui.combo{id = 'layerMax', items = layers, selected = '3', onChange = function(event)
            self:setLayers(math.min(tonumber(event.value), self.lamp.layerMin), tonumber(event.value))
        end}},
    }
end

-- Keeps the range valid, moving the other end along when one end passes it.
function Masks:setLayers(low, high)
    self.lamp.layerMin, self.lamp.layerMax = low, high
    self.header:set('layerMin', {selected = tostring(low)})
    self.header:set('layerMax', {selected = tostring(high)})
end

function Masks:applyMask()
    local mask = 0
    for bit = 1, 3 do
        if self.bits[bit] then
            mask = mask | (1 << (bit - 1))
        end
    end
    self.lamp.itemMask = mask
end

function Masks:update(dt)
    self.cursor:update(dt)
    self.lamp.x, self.lamp.y = self.cursor:world(self.camera)
    self:setStatus(string.format('Light with "itemMask" %d, "layerMin" %d, "layerMax" %d', self.lamp.itemMask, self.lamp.layerMin, self.lamp.layerMax))
end

function Masks:render()
    graphics2d.beginWorld(self.camera, {ambientLight = '#FF101218'})
    graphics2d.drawRect({-1600, -900, 3200, 1800}, '#FF7A7A82', {lightMask = Masks.floorMask})
    for column = 1, 3 do
        for row = 1, 3 do
            local x, y = (column - 2) * 300 - 80, (row - 2) * 230 + 60
            graphics2d.drawRect({x, y, 160, 160}, '#FFE8E0D0', {layer = row, lightMask = 1 << (column - 1)})
        end
    end
    graphics2d.drawLight(self.fill)
    graphics2d.drawLight(self.lamp)
end

function Masks:renderUi()
    graphics2d.beginScreen()
    for column = 1, 3 do
        local x, y = self.camera:worldToScreen((column - 2) * 300, -215)
        graphics2d.drawText(nil, 'Mask ' .. (1 << (column - 1)), x, y, {size = 32, anchor = {0.5, 0.5}, outlineWidth = 3})
    end
    for row = 1, 3 do
        local x, y = self.camera:worldToScreen(-560, (row - 2) * 230 + 140)
        graphics2d.drawText(nil, 'Layer ' .. row, x, y, {size = 32, anchor = {1, 0.5}, outlineWidth = 3})
    end
    self.cursor:draw()
end

return Masks
