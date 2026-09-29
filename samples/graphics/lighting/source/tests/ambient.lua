-- Ambient light: the lit canvas starts from this color, so white shows the room unchanged and dark blue turns it into night.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')
local Stage = require('stage')

local Ambient = haylen.class('Ambient', sample.Test)

Ambient.hints = 'Pick an ambient color. The change eases over 0.8 seconds with a tween of the color.'

Ambient.presets = {
    {id = 'noon', text = 'Noon', color = '#FFFFFFFF'},
    {id = 'dusk', text = 'Dusk', color = '#FFD08A60'},
    {id = 'night', text = 'Night', color = '#FF28325A'},
    {id = 'underwater', text = 'Underwater', color = '#FF3A86B8'},
    {id = 'cave', text = 'Cave', color = '#FF0C0C12'},
}

function Ambient:init(entry)
    Ambient.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.stage = Stage()
    self.ambient = m.color(Ambient.presets[3].color)
end

function Ambient:controls()
    local items = {}
    for index, preset in ipairs(Ambient.presets) do
        items[index] = {id = preset.id, text = preset.text}
    end
    return {ui.radioGroup{id = 'preset', items = items, selected = 'night', onChange = function(event)
        self:choose(event.value)
    end}}
end

function Ambient:choose(id)
    for _, preset in ipairs(Ambient.presets) do
        if preset.id == id then
            tween.to(self, 0.8, {ambient = preset.color}, {ease = 'sine_in_out', owner = self, overwrite = true})
        end
    end
end

function Ambient:update(dt)
    Ambient.super.update(self, dt)
    self:setStatus('ambientLight = ' .. self.ambient:toHex())
end

function Ambient:render()
    graphics2d.beginWorld(self.camera, {ambientLight = self.ambient})
    self.stage:draw(true)
end

return Ambient
