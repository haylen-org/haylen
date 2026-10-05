-- Light masks and layer ranges: a light reaches a draw when the light mask of the draw shares a bit with the item mask of the light and the layer of the draw lies inside the range of the light.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')

local Masks = haylen.class('Masks', LightingTest)

-- The floor has the mask 8, which only the fill light reaches, so it stays the same whatever the test light selects.
Masks.floorMask = 8
Masks.layers = {{id = '1', text = '1'}, {id = '2', text = '2'}, {id = '3', text = '3'}}

function Masks:init(entry)
    Masks.super.init(self, entry)
    self.ambientLight = '#FF101218'
    self.bits = {true, true, true}
    self.fill = lighting2d.newLight({type = 'directional', intensity = 0.25, itemMask = Masks.floorMask})
    self.lamp = lighting2d.newLight({radius = 1400, color = '#FFFFF0D0', intensity = 1.4, itemMask = 7, layerMin = 1, layerMax = 3})
end

function Masks:enter()
    local checks = {}
    for bit = 1, 3 do
        checks[bit] = ui.checkbox{id = 'mask' .. bit, text = 'Mask ' .. (1 << (bit - 1)), checked = true, onChange = function(event)
            self.bits[bit] = event.checked
            self:applyMask()
        end}
    end
    self:frame{
        hint = 'Columns have the light masks 1, 2 and 4, and rows sit on the layers 1, 2 and 3. Move the light with the mouse, a finger, the arrows, WASD or a stick.',
        cursor = true,
        controls = {
            ui.formField{label = 'Item mask of the light', ui.column{gap = 8, children = checks}},
            ui.formField{label = 'Lowest layer', ui.combo{id = 'layerMin', items = Masks.layers, selected = '1', onChange = function(event)
                self:setLayers(tonumber(event.value), math.max(tonumber(event.value), self.lamp.layerMax))
            end}},
            ui.formField{label = 'Highest layer', ui.combo{id = 'layerMax', items = Masks.layers, selected = '3', onChange = function(event)
                self:setLayers(math.min(tonumber(event.value), self.lamp.layerMin), tonumber(event.value))
            end}},
        },
    }
end

-- Keeps the range valid, moving the other end along when one end passes it.
function Masks:setLayers(low, high)
    self.lamp.layerMin, self.lamp.layerMax = low, high
    self:set('layerMin', {selected = tostring(low)})
    self:set('layerMax', {selected = tostring(high)})
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
    Masks.super.update(self, dt)
    self.lamp.x, self.lamp.y = self.cursorX, self.cursorY
    self:status(string.format('Light with "itemMask" %d, "layerMin" %d and "layerMax" %d', self.lamp.itemMask, self.lamp.layerMin, self.lamp.layerMax))
end

function Masks:draw(area)
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
    if not self.stage then
        return
    end
    Masks.super.renderUi(self)
    for column = 1, 3 do
        local x, y = self.camera:worldToScreen((column - 2) * 300, -215)
        graphics2d.drawText(nil, 'Mask ' .. (1 << (column - 1)), x, y, {size = 32, anchor = {0.5, 0.5}, outlineWidth = 3})
    end
    for row = 1, 3 do
        local x, y = self.camera:worldToScreen(-560, (row - 2) * 230 + 140)
        graphics2d.drawText(nil, 'Layer ' .. row, x, y, {size = 32, anchor = {1, 0.5}, outlineWidth = 3})
    end
end

return Masks
