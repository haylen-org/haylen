-- Blend modes: `add` brightens the light map, `subtract` darkens it, `mix` replaces it by the strength of the light and `max` keeps the brighter of the map and the light.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local BlendModes = haylen.class('BlendModes', LightingTest)

BlendModes.code = [[
graphics2d.beginWorld(camera, {ambientLight = '#FF6A6A78'})
graphics2d.drawLight({x = -660, y = 0, radius = 300, color = '#FFFFB060', blend = 'add'})  -- Brightens.
graphics2d.drawLight({x = -220, y = 0, radius = 300, intensity = 0.8, blend = 'subtract'})  -- Darkens.
graphics2d.drawLight({x = 220, y = 0, radius = 300, color = '#FF3070FF', blend = 'mix'})  -- Replaces by its strength.
graphics2d.drawLight({x = 660, y = 0, radius = 300, color = '#FFFFD070', blend = 'max'})  -- Keeps the brighter.]]

BlendModes.modes = {{id = 'add', text = 'Add'}, {id = 'subtract', text = 'Subtract'}, {id = 'mix', text = 'Mix'}, {id = 'max', text = 'Max'}}

function BlendModes:init(entry)
    BlendModes.super.init(self, entry)
    self.room = Room()
    self.ambientLight = '#FF6A6A78'
    self.fixed = {
        {label = 'Add', story = 'Brightens', light = lighting2d.newLight({x = -660, y = -60, radius = 300, color = '#FFFFB060', blend = 'add'})},
        {label = 'Subtract', story = 'Darkens', light = lighting2d.newLight({x = -220, y = -60, radius = 300, color = '#FFFFFFFF', intensity = 0.8, blend = 'subtract'})},
        {label = 'Mix', story = 'Replaces by strength', light = lighting2d.newLight({x = 220, y = -60, radius = 300, color = '#FF3070FF', blend = 'mix'})},
        {label = 'Max', story = 'Only where brighter', light = lighting2d.newLight({x = 660, y = -60, radius = 300, color = '#FFFFD070', blend = 'max'})},
    }
    self.held = lighting2d.newLight({radius = 260, color = '#FF60FF90', blend = 'add'})
    self.pointer.origin = {0.5, 0.86}
end

function BlendModes:enter()
    self:frame{
        hint = 'Four fixed lights show each mode. The light on the cursor takes the mode you pick, so move it over the others to see how they combine.',
        code = BlendModes.code,
        cursor = true,
        controls = {ui.formField{label = 'Blend of the cursor light', ui.radioGroup{id = 'blend', items = BlendModes.modes, selected = 'add', onChange = function(event)
            self.held.blend = event.value
        end}}},
    }
end

function BlendModes:update(dt)
    BlendModes.super.update(self, dt)
    self.held.x, self.held.y = self.cursorX, self.cursorY
    self:status(string.format('The green light on the cursor blends with "%s".', self.held.blend))
end

function BlendModes:draw(area)
    self.room:draw()
    for _, entry in ipairs(self.fixed) do
        Room.drawLamp(entry.light.x, entry.light.y, entry.light.color, 26)
        graphics2d.drawLight(entry.light)
    end
    Room.drawLamp(self.held.x, self.held.y, self.held.color, 26)
    graphics2d.drawLight(self.held)
end

function BlendModes:renderUi()
    if not self.stage then
        return
    end
    BlendModes.super.renderUi(self)
    for _, entry in ipairs(self.fixed) do
        local x, y = self.camera:worldToScreen(entry.light.x, entry.light.y + 70)
        graphics2d.drawText(nil, entry.label, x, y, {size = 36, anchor = {0.5, 0}, outlineWidth = 3})
        graphics2d.drawText(nil, entry.story, x, y + 44, {size = 24, anchor = {0.5, 0}, outlineWidth = 3, color = '#FFE8EAF2'})
    end
end

return BlendModes
