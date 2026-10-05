-- Blend modes: `add` brightens the light map, `subtract` darkens it and `mix` replaces it by the strength of the light.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local BlendModes = haylen.class('BlendModes', LightingTest)

BlendModes.modes = {{id = 'add', text = 'Add'}, {id = 'subtract', text = 'Subtract'}, {id = 'mix', text = 'Mix'}}

function BlendModes:init(entry)
    BlendModes.super.init(self, entry)
    self.room = Room()
    self.ambientLight = '#FF6A6A78'
    self.fixed = {
        {label = 'Add', light = lighting2d.newLight({x = -560, y = 0, radius = 360, color = '#FFFFB060', blend = 'add'})},
        {label = 'Subtract', light = lighting2d.newLight({x = 0, y = 0, radius = 360, color = '#FFFFFFFF', intensity = 0.8, blend = 'subtract'})},
        {label = 'Mix', light = lighting2d.newLight({x = 560, y = 0, radius = 360, color = '#FF3070FF', blend = 'mix'})},
    }
    self.held = lighting2d.newLight({radius = 260, color = '#FF60FF90', blend = 'add'})
end

function BlendModes:enter()
    self:frame{
        hint = 'Three fixed lights show each mode. The light on the cursor takes the mode you pick, so move it over the others to see how they combine.',
        cursor = true,
        controls = {ui.formField{label = 'Blend of the cursor light', ui.radioGroup{id = 'blend', items = BlendModes.modes, selected = 'add', onChange = function(event)
            self.held.blend = event.value
        end}}},
    }
end

function BlendModes:update(dt)
    BlendModes.super.update(self, dt)
    self.held.x, self.held.y = self.cursorX, self.cursorY
    self:status(string.format('Cursor light with "blend" set to "%s"', self.held.blend))
end

function BlendModes:draw(area)
    self.room:draw()
    for _, entry in ipairs(self.fixed) do
        graphics2d.drawLight(entry.light)
    end
    graphics2d.drawLight(self.held)
end

function BlendModes:renderUi()
    if not self.stage then
        return
    end
    BlendModes.super.renderUi(self)
    for _, entry in ipairs(self.fixed) do
        local x, y = self.camera:worldToScreen(entry.light.x, entry.light.y)
        graphics2d.drawText(nil, entry.label, x, y, {size = 44, anchor = {0.5, 0.5}, outlineWidth = 3})
    end
end

return BlendModes
