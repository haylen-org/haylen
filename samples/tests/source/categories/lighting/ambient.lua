-- Ambient light: the lit canvas starts from this color, so white shows the courtyard unchanged and dark blue turns it into night. The test tours the presets on its own until a preset is picked.
local haylen = require('haylen')
local m = require('haylen.math')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local Ambient = haylen.class('Ambient', LightingTest)

Ambient.code = [[
self.ambientLight = m.color('#FFFFFFFF')  -- White shows the scene unchanged.
local night = {ambientLight = '#FF28325A'}
tween.to(self, 0.8, night, {ease = 'sineInOut', owner = self})
graphics2d.beginWorld(camera, {ambientLight = self.ambientLight})]]

Ambient.presets = {
    {id = 'noon', text = 'Noon', color = '#FFFFFFFF', story = 'Noon: white shows every color as it was painted.'},
    {id = 'dusk', text = 'Dusk', color = '#FFD08A60', story = 'Dusk: a warm orange that dims and tints everything.'},
    {id = 'night', text = 'Night', color = '#FF28325A', story = 'Night: a dark blue where only lights would show color.'},
    {id = 'underwater', text = 'Underwater', color = '#FF3A86B8', story = 'Underwater: a deep teal that cools every color.'},
    {id = 'cave', text = 'Cave', color = '#FF0C0C12', story = 'Cave: almost black, so the scene nearly disappears.'},
}
Ambient.tourStep = 2.5

function Ambient:init(entry)
    Ambient.super.init(self, entry)
    self.room = Room()
    self.preset = 2
    self.ambientLight = m.color(Ambient.presets[self.preset].color)
end

function Ambient:enter()
    local items = {}
    for index, preset in ipairs(Ambient.presets) do
        items[index] = {id = preset.id, text = preset.text}
    end
    self:frame{
        hint = 'Pick an ambient color, which eases in over 0.8 seconds with a tween. Until you pick one, the test walks through them on its own.',
        code = Ambient.code,
        controls = {ui.formField{label = 'Ambient light', ui.radioGroup{id = 'preset', items = items, selected = Ambient.presets[self.preset].id, onChange = function(event)
            timer.cancel(self.tour)
            self:choose(event.value)
        end}}},
        focus = 'preset',
    }
    self.tour = timer.every(Ambient.tourStep, function()
        self.preset = self.preset % #Ambient.presets + 1
        local preset = Ambient.presets[self.preset]
        self:set('preset', {selected = preset.id})
        self:choose(preset.id)
    end, {owner = self})
end

function Ambient:choose(id)
    for index, preset in ipairs(Ambient.presets) do
        if preset.id == id then
            self.preset = index
            tween.to(self, 0.8, {ambientLight = preset.color}, {ease = 'sineInOut', owner = self, overwrite = true})
        end
    end
end

function Ambient:update(dt)
    Ambient.super.update(self, dt)
    self:status(Ambient.presets[self.preset].story)
end

function Ambient:draw(area)
    self.room:draw()
end

return Ambient
