-- Point lights: each one shines around its position up to its radius, with its own color and intensity.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local PointLights = haylen.class('PointLights', LightingTest)

PointLights.colors = {'#FFFFB070', '#FF70C0FF', '#FFFF6AA0', '#FF90FF8A', '#FFFFE890', '#FFB890FF'}

function PointLights:init(entry)
    PointLights.super.init(self, entry)
    self.room = Room()
    self.random = m.random(7)
    self.held = lighting2d.newLight({radius = 320, color = PointLights.colors[1], intensity = 1.2})
    self.lights = {
        lighting2d.newLight({x = -560, y = -260, radius = 260, color = PointLights.colors[2]}),
        lighting2d.newLight({x = 520, y = 240, radius = 380, color = PointLights.colors[3], intensity = 0.8}),
    }
end

function PointLights:enter()
    self:frame{
        hint = 'Move the light with the mouse, a finger, the arrows, WASD or a stick. A click, a tap, E, Enter or the south button places a copy.',
        cursor = true,
        controls = {
            ui.formField{label = 'Radius', ui.slider{id = 'radius', min = 60, max = 700, value = self.held.radius, showValue = true, decimals = 0, onChange = function(event)
                self.held.radius = event.value
            end}},
            ui.formField{label = 'Intensity', ui.slider{id = 'intensity', min = 0, max = 2, value = self.held.intensity, showValue = true, onChange = function(event)
                self.held.intensity = event.value
            end}},
            ui.button{id = 'clear', text = 'Clear placed lights', onClick = function()
                self.lights = {}
            end},
        },
    }
end

function PointLights:update(dt)
    PointLights.super.update(self, dt)
    local held = self.held
    held.x, held.y = self.cursorX, self.cursorY
    if self:pressed() then
        self.lights[#self.lights + 1] = lighting2d.newLight({x = held.x, y = held.y, radius = held.radius, color = held.color, intensity = held.intensity})
        held.color = PointLights.colors[self.random:integer(1, #PointLights.colors)]
    end
    self:status(string.format('Lights %d   Radius %.0f   Intensity %.2f', #self.lights + 1, held.radius, held.intensity))
end

function PointLights:draw(area)
    self.room:draw()
    for _, light in ipairs(self.lights) do
        graphics2d.drawLight(light)
    end
    graphics2d.drawLight(self.held)
end

return PointLights
