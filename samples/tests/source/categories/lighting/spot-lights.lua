-- Spot lights: point lights that shine only inside a cone around their rotation, full inside the inner angle and fading out at the outer one. A lantern in the middle aims at the cursor and two searchlights on stands sweep the courtyard.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local SpotLights = haylen.class('SpotLights', LightingTest)

SpotLights.code = [[
local lamp = lighting2d.newLight({type = 'spot', x = -60, y = 40, radius = 900, intensity = 1.4, shadows = true})
lamp.rotation = math.atan(y - lamp.y, x - lamp.x)  -- Aims at a point.
lamp.outerAngle, lamp.innerAngle = math.rad(60), math.rad(30)  -- Full inside the inner angle, fading out at the outer one.
graphics2d.drawLight(lamp)]]

function SpotLights:init(entry)
    SpotLights.super.init(self, entry)
    self.room = Room()
    self.ambientLight = '#FF10141C'
    self.cone = math.rad(60)
    self.softness = 0.5
    self.lamp = lighting2d.newLight({type = 'spot', x = -60, y = 40, radius = 900, color = '#FFFFF0C8', intensity = 1.4, shadows = true, shadowFilter = 'pcf5'})
    self.searchlights = {
        lighting2d.newLight({type = 'spot', x = -880, y = -430, radius = 1500, innerAngle = 0.12, outerAngle = 0.22, color = '#FF80C8FF'}),
        lighting2d.newLight({type = 'spot', x = 880, y = -430, radius = 1500, innerAngle = 0.12, outerAngle = 0.22, color = '#FFFF90C0'}),
    }
    self.time = 0
end

function SpotLights:enter()
    self:frame{
        hint = 'The lamp in the middle aims at the cursor: move it with the mouse, a finger, the arrows, WASD or a stick. Two searchlights sweep on their own.',
        code = SpotLights.code,
        cursor = true,
        controls = {
            ui.formField{label = 'Cone angle in degrees', ui.slider{id = 'cone', min = 10, max = 180, value = math.deg(self.cone), showValue = true, decimals = 0, onChange = function(event)
                self.cone = math.rad(event.value)
            end}},
            ui.formField{label = 'Inner angle as a share of the cone', ui.slider{id = 'softness', min = 0, max = 1, value = self.softness, showValue = true, onChange = function(event)
                self.softness = event.value
            end}},
        },
    }
end

function SpotLights:update(dt)
    SpotLights.super.update(self, dt)
    self.time = self.time + dt
    local lamp = self.lamp
    lamp.rotation = math.atan(self.cursorY - lamp.y, self.cursorX - lamp.x)
    lamp.outerAngle = self.cone
    lamp.innerAngle = self.cone * self.softness
    self.searchlights[1].rotation = math.pi * 0.25 + math.sin(self.time * 0.7) * 0.35
    self.searchlights[2].rotation = math.pi * 0.75 + math.sin(self.time * 0.9 + 1) * 0.35
    self:status(string.format('The lantern shines fully within %.0f degrees of its aim and fades out by %.0f degrees.', math.deg(lamp.innerAngle), math.deg(lamp.outerAngle)))
end

-- Draws a searchlight on its stand: a round base and a barrel that turns with the light, with its glass glowing in the color of the light.
function SpotLights.drawHousing(light, length)
    local dx, dy = math.cos(light.rotation), math.sin(light.rotation)
    local order = {layer = 5, unshaded = true}
    graphics2d.drawCircle(light.x, light.y, 34, '#FF1E1B33', order)
    graphics2d.drawCircle(light.x, light.y, 28, '#FF5A6080', order)
    graphics2d.drawLine(light.x, light.y, light.x + dx * length, light.y + dy * length, 34, '#FF1E1B33', {layer = 6, unshaded = true})
    graphics2d.drawLine(light.x, light.y, light.x + dx * length, light.y + dy * length, 24, '#FF8890B0', {layer = 6, unshaded = true})
    graphics2d.drawCircle(light.x + dx * length, light.y + dy * length, 15, light.color, {layer = 7, emission = 1.5})
end

function SpotLights:draw(area)
    self.room:draw()
    for _, light in ipairs(self.searchlights) do
        SpotLights.drawHousing(light, 48)
        graphics2d.drawLight(light)
    end
    SpotLights.drawHousing(self.lamp, 30)
    graphics2d.drawLight(self.lamp)
end

return SpotLights
