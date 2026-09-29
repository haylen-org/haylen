-- Spot lights: point lights that shine only inside a cone around their rotation, full inside the inner angle and fading out at the outer one.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local sample = require('sample')
local Stage = require('stage')

local SpotLights = haylen.class('SpotLights', sample.Test)

SpotLights.hints = 'The lamp in the middle aims at the cursor: move it with the mouse, a finger, WASD or the left stick. Two searchlights sweep on their own.'

function SpotLights:init(entry)
    SpotLights.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.stage = Stage()
    self.cursor = sample.Cursor()
    self.cone = math.rad(60)
    self.softness = 0.5
    self.lamp = lighting2d.newLight({type = 'spot', x = -60, y = 40, radius = 900, color = '#FFFFF0C8', intensity = 1.4, shadows = true, shadowFilter = 'pcf5'})
    self.searchlights = {
        lighting2d.newLight({type = 'spot', x = -900, y = -500, radius = 1500, innerAngle = 0.12, outerAngle = 0.22, color = '#FF80C8FF'}),
        lighting2d.newLight({type = 'spot', x = 900, y = -500, radius = 1500, innerAngle = 0.12, outerAngle = 0.22, color = '#FFFF90C0'}),
    }
    self.time = 0
end

function SpotLights:controls()
    return {
        ui.formField{label = 'Cone angle in degrees', ui.slider{min = 10, max = 180, value = math.deg(self.cone), showValue = true, decimals = 0, onChange = function(event)
            self.cone = math.rad(event.value)
        end}},
        ui.formField{label = 'Inner angle as a share of the cone', ui.slider{min = 0, max = 1, value = self.softness, showValue = true, onChange = function(event)
            self.softness = event.value
        end}},
    }
end

function SpotLights:update(dt)
    SpotLights.super.update(self, dt)
    self.cursor:update(dt)
    self.time = self.time + dt

    local x, y = self.cursor:world(self.camera)
    self.lamp.rotation = math.atan(y - self.lamp.y, x - self.lamp.x)
    self.lamp.outerAngle = self.cone
    self.lamp.innerAngle = self.cone * self.softness
    self.searchlights[1].rotation = math.pi * 0.25 + math.sin(self.time * 0.7) * 0.35
    self.searchlights[2].rotation = math.pi * 0.75 + math.sin(self.time * 0.9 + 1) * 0.35
    self:setStatus(string.format('rotation %.2f, innerAngle %.2f, outerAngle %.2f', self.lamp.rotation, self.lamp.innerAngle, self.lamp.outerAngle))
end

function SpotLights:render()
    graphics2d.beginWorld(self.camera, {ambientLight = '#FF10141C'})
    self.stage:draw(true)
    graphics2d.drawCircle(self.lamp.x, self.lamp.y, 18, '#FFFFF0C8', {layer = 2, unshaded = true})
    for _, light in ipairs(self.searchlights) do
        graphics2d.drawLight(light)
    end
    graphics2d.drawLight(self.lamp)
end

function SpotLights:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return SpotLights
