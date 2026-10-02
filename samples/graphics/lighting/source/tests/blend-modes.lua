-- Blend modes: `add` brightens the light map, `subtract` darkens it and `mix` replaces it by the strength of the light.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local sample = require('sample')
local Stage = require('stage')

local BlendModes = haylen.class('BlendModes', sample.Test)

BlendModes.hints = 'Three fixed lights show each mode. The light on the cursor takes the mode you pick, so drag it over the others to see how they combine.'

function BlendModes:init(entry)
    BlendModes.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.stage = Stage()
    self.cursor = sample.Cursor()
    self.fixed = {
        {label = 'Add', light = lighting2d.newLight({x = -560, y = 0, radius = 360, color = '#FFFFB060', blend = 'add'})},
        {label = 'Subtract', light = lighting2d.newLight({x = 0, y = 0, radius = 360, color = '#FFFFFFFF', intensity = 0.8, blend = 'subtract'})},
        {label = 'Mix', light = lighting2d.newLight({x = 560, y = 0, radius = 360, color = '#FF3070FF', blend = 'mix'})},
    }
    self.held = lighting2d.newLight({radius = 260, color = '#FF60FF90', blend = 'add'})
end

function BlendModes:controls()
    local items = {{id = 'add', text = 'Add'}, {id = 'subtract', text = 'Subtract'}, {id = 'mix', text = 'Mix'}}
    return {ui.formField{label = 'Blend of the cursor light', ui.radioGroup{items = items, selected = 'add', onChange = function(event)
        self.held.blend = event.value
    end}}}
end

function BlendModes:update(dt)
    self.cursor:update(dt)
    self.held.x, self.held.y = self.cursor:world(self.camera)
    self:setStatus('Cursor light blend = ' .. self.held.blend)
end

function BlendModes:render()
    graphics2d.beginWorld(self.camera, {ambientLight = '#FF6A6A78'})
    self.stage:draw(true)
    for _, entry in ipairs(self.fixed) do
        graphics2d.drawLight(entry.light)
    end
    graphics2d.drawLight(self.held)
end

function BlendModes:renderUi()
    graphics2d.beginScreen()
    for _, entry in ipairs(self.fixed) do
        local x, y = self.camera:worldToScreen(entry.light.x, entry.light.y)
        graphics2d.drawText(nil, entry.label, x, y, {size = 44, anchor = {0.5, 0.5}, outlineWidth = 3})
    end
    self.cursor:draw()
end

return BlendModes
