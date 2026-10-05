-- Directional light: like the sun it covers the whole canvas from its rotation, so its shadows run across the entire view.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local Directional = haylen.class('Directional', LightingTest)

function Directional:init(entry)
    Directional.super.init(self, entry)
    self.room = Room()
    self.ambientLight = '#FF303A58'
    self.turning = false
    self.sun = lighting2d.newLight({type = 'directional', rotation = 0.6, color = '#FFFFE6C0', intensity = 1.1, shadows = true, shadowFilter = 'pcf13', shadowSmoothness = 1, shadowColor = '#B0000000'})
end

function Directional:enter()
    self:frame{
        hint = 'The sun travels from the middle of the room toward the cursor. Move it with the mouse, a finger, the arrows, WASD or a stick, or let the sun turn by itself.',
        cursor = true,
        controls = {
            ui.toggle{id = 'turn', text = 'Turn by itself', onChange = function(event)
                self.turning = event.checked
            end},
            ui.formField{label = 'Shadow strength', ui.slider{id = 'strength', min = 0, max = 1, value = self.sun.shadowColor.a, showValue = true, onChange = function(event)
                self.sun.shadowColor = m.color(0, 0, 0, event.value)
            end}},
        },
    }
end

function Directional:update(dt)
    Directional.super.update(self, dt)
    if self.turning then
        self.sun.rotation = m.wrapAngle(self.sun.rotation + dt * 0.4)
    elseif self.cursorX ~= 0 or self.cursorY ~= 0 then
        self.sun.rotation = math.atan(self.cursorY, self.cursorX)
    end
    self:status(string.format('Rotation %.2f radians, %.0f degrees', self.sun.rotation, math.deg(self.sun.rotation)))
end

function Directional:draw(area)
    self.room:draw()
    graphics2d.drawLight(self.sun)
    local tipX, tipY = math.cos(self.sun.rotation) * 220, math.sin(self.sun.rotation) * 220
    graphics2d.drawLine(0, 0, tipX, tipY, 4, '#C0FFE6C0', {layer = 3, unshaded = true})
    graphics2d.drawCircle(tipX, tipY, 10, '#FFFFE6C0', {layer = 3, unshaded = true})
end

return Directional
