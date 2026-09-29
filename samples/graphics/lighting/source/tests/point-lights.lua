-- Point lights: each one shines around its position up to its radius, with its own color and intensity.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local ui = require('haylen.ui')

local sample = require('sample')
local Stage = require('stage')

local PointLights = haylen.class('PointLights', sample.Test)

PointLights.hints = 'Move the light with the mouse, a finger, WASD or the left stick. Click, tap, E or the X button places a copy.'

PointLights.colors = {'#FFFFB070', '#FF70C0FF', '#FFFF6AA0', '#FF90FF8A', '#FFFFE890', '#FFB890FF'}

function PointLights:init(entry)
    PointLights.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.stage = Stage()
    self.cursor = sample.Cursor()
    self.random = m.random(7)
    self.held = lighting2d.newLight({radius = 320, color = PointLights.colors[1], intensity = 1.2})
    self.placed = {
        lighting2d.newLight({x = -560, y = -260, radius = 260, color = PointLights.colors[2]}),
        lighting2d.newLight({x = 520, y = 240, radius = 380, color = PointLights.colors[3], intensity = 0.8}),
    }
end

function PointLights:controls()
    return {
        ui.formField{label = 'Radius', ui.slider{min = 60, max = 700, value = self.held.radius, showValue = true, decimals = 0, onChange = function(event)
            self.held.radius = event.value
        end}},
        ui.formField{label = 'Intensity', ui.slider{min = 0, max = 2, value = self.held.intensity, showValue = true, onChange = function(event)
            self.held.intensity = event.value
        end}},
        ui.button{text = 'Clear placed lights', onClick = function()
            self.placed = {}
        end},
    }
end

function PointLights:update(dt)
    self.cursor:update(dt)
    self.held.x, self.held.y = self.cursor:world(self.camera)
    if sample.pressed() then
        self.placed[#self.placed + 1] = lighting2d.newLight({x = self.held.x, y = self.held.y, radius = self.held.radius, color = self.held.color, intensity = self.held.intensity})
        self.held.color = PointLights.colors[self.random:integer(1, #PointLights.colors)]
    end
    self:setStatus(string.format('%d lights, radius %.0f, intensity %.2f', #self.placed + 1, self.held.radius, self.held.intensity))
end

function PointLights:render()
    graphics2d.beginWorld(self.camera, {ambientLight = '#FF141820'})
    self.stage:draw(true)
    for _, light in ipairs(self.placed) do
        graphics2d.drawLight(light)
    end
    graphics2d.drawLight(self.held)
end

function PointLights:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return PointLights
