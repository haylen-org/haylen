-- Ambient light: the lit canvas starts from this color, so white shows the room unchanged and dark blue turns it into night.
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local Ambient = haylen.class('Ambient', LightingTest)

Ambient.presets = {
    {id = 'noon', text = 'Noon', color = '#FFFFFFFF'},
    {id = 'dusk', text = 'Dusk', color = '#FFD08A60'},
    {id = 'night', text = 'Night', color = '#FF28325A'},
    {id = 'underwater', text = 'Underwater', color = '#FF3A86B8'},
    {id = 'cave', text = 'Cave', color = '#FF0C0C12'},
}

function Ambient:init(entry)
    Ambient.super.init(self, entry)
    self.room = Room()
    self.ambientLight = m.color(Ambient.presets[3].color)
end

function Ambient:enter()
    local items = {}
    for index, preset in ipairs(Ambient.presets) do
        items[index] = {id = preset.id, text = preset.text}
    end
    self:frame{
        hint = 'Pick an ambient color. The change eases over 0.8 seconds with a tween of the color.',
        controls = {ui.formField{label = 'Ambient light', ui.radioGroup{id = 'preset', items = items, selected = 'night', onChange = function(event)
            self:choose(event.value)
        end}}},
        focus = 'preset',
    }
end

function Ambient:choose(id)
    for _, preset in ipairs(Ambient.presets) do
        if preset.id == id then
            tween.to(self, 0.8, {ambientLight = preset.color}, {ease = 'sineInOut', owner = self, overwrite = true})
        end
    end
end

function Ambient:update(dt)
    Ambient.super.update(self, dt)
    self:status(string.format('Option "ambientLight" is "%s"', self.ambientLight:toHex()))
end

function Ambient:draw(area)
    self.room:draw()
end

return Ambient
