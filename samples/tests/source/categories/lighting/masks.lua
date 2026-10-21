-- Light masks and layer ranges: a light reaches a draw when the light mask of the draw shares a bit with the item mask of the light and the layer of the draw lies inside the range of the light. Crates, bushes and rocks each have a mask of their own, in three rows on the layers 1, 2 and 3.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local Masks = haylen.class('Masks', LightingTest)

Masks.code = [[
local lamp = lighting2d.newLight({radius = 1400, itemMask = 1 | 4, layerMin = 1, layerMax = 2})
graphics2d.draw(tiles, x, y, {source = crate, layer = 2, lightMask = 1})  -- Lit.
graphics2d.draw(tiles, x, y, {source = bush, layer = 2, lightMask = 2})  -- Not lit.
graphics2d.draw(tiles, x, y, {source = rock, layer = 3, lightMask = 4})  -- Above the range.
graphics2d.drawLight(lamp)
print(lamp:affects(2, 2))  -- Prints false.]]

-- The floor has the mask 8, which only the fill light reaches, so it stays the same whatever the lamp selects.
Masks.floorMask = 8
Masks.kinds = {
    {name = 'Crates', source = {530, 266, 128, 128}},
    {name = 'Bushes', source = {266, 266, 128, 128}},
    {name = 'Rocks', source = {398, 266, 128, 128}},
}
Masks.layers = {{id = '1', text = '1'}, {id = '2', text = '2'}, {id = '3', text = '3'}}
Masks.spacing = {x = 340, y = 210}
Masks.size = 170

function Masks:init(entry)
    Masks.super.init(self, entry)
    self.tiles = assets.texture(Room.tileset, {filter = 'linear'})
    self.room = Room()
    self.ambientLight = '#FF2A3046'
    self.bits = {true, false, true}
    self.fill = lighting2d.newLight({type = 'directional', intensity = 0.35, itemMask = Masks.floorMask})
    self.lamp = lighting2d.newLight({radius = 1400, color = '#FFFFF0D0', intensity = 0.9, itemMask = 1 | 4, layerMin = 1, layerMax = 2})
    self.pointer.origin = {0.5, 0.12}
end

function Masks:enter()
    local checks = {}
    for bit, kind in ipairs(Masks.kinds) do
        checks[bit] = ui.checkbox{id = 'mask' .. bit, text = string.format('%s, mask %d', kind.name, 1 << (bit - 1)), checked = self.bits[bit], onChange = function(event)
            self.bits[bit] = event.checked
            self:applyMask()
        end}
    end
    self:frame{
        hint = 'Pick what the lamp reaches by mask and by layer, and move it with the mouse, a finger, the arrows, WASD or a stick. What it does not reach stays in the dim ambient light.',
        code = Masks.code,
        cursor = true,
        controls = {
            ui.formField{label = 'The lamp reaches', ui.column{gap = 8, children = checks}},
            ui.formField{label = 'Lowest layer', ui.combo{id = 'layerMin', items = Masks.layers, selected = '1', onChange = function(event)
                self:setLayers(tonumber(event.value), math.max(tonumber(event.value), self.lamp.layerMax))
            end}},
            ui.formField{label = 'Highest layer', ui.combo{id = 'layerMax', items = Masks.layers, selected = '2', onChange = function(event)
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

-- Lists what the lamp reaches as words for the status line.
function Masks:reached()
    local names = {}
    for bit, kind in ipairs(Masks.kinds) do
        if self.bits[bit] then
            names[#names + 1] = kind.name:lower()
        end
    end
    if #names == 0 then
        return 'nothing'
    end
    if #names == 1 then
        return names[1]
    end
    return table.concat(names, ', ', 1, #names - 1) .. ' and ' .. names[#names]
end

function Masks:update(dt)
    Masks.super.update(self, dt)
    self.lamp.x, self.lamp.y = self.cursorX, self.cursorY
    local reached = self:reached()
    local layers = self.lamp.layerMin == self.lamp.layerMax and string.format('on layer %d', self.lamp.layerMin) or string.format('on layers %d to %d', self.lamp.layerMin, self.lamp.layerMax)
    self:status(reached == 'nothing' and 'The lamp reaches nothing.' or string.format('The lamp reaches %s %s.', reached, layers))
end

function Masks:draw(area)
    graphics2d.drawStatic(self.room.floor, 0, 0, {lightMask = Masks.floorMask})
    for column, kind in ipairs(Masks.kinds) do
        for row = 1, 3 do
            local x, y = (column - 2) * Masks.spacing.x + 100, (row - 2) * Masks.spacing.y + 60
            graphics2d.draw(self.tiles, x, y, {source = kind.source, width = Masks.size, height = Masks.size, layer = row, lightMask = 1 << (column - 1)})
        end
    end
    Room.drawLamp(self.lamp.x, self.lamp.y, self.lamp.color, 26)
    graphics2d.drawLight(self.fill)
    graphics2d.drawLight(self.lamp)
end

function Masks:renderUi()
    if not self.stage then
        return
    end
    Masks.super.renderUi(self)
    for column, kind in ipairs(Masks.kinds) do
        local x, y = self.camera:worldToScreen((column - 2) * Masks.spacing.x + 100, -Masks.spacing.y + 60 - Masks.size * 0.62)
        graphics2d.drawText(nil, string.format('%s, mask %d', kind.name, 1 << (column - 1)), x, y, {size = 26, anchor = {0.5, 1}, outlineWidth = 3})
    end
    for row = 1, 3 do
        local x, y = self.camera:worldToScreen(-Masks.spacing.x + 100 - Masks.size * 0.62, (row - 2) * Masks.spacing.y + 60)
        graphics2d.drawText(nil, 'Layer ' .. row, x, y, {size = 26, anchor = {1, 0.5}, outlineWidth = 3})
    end
end

return Masks
