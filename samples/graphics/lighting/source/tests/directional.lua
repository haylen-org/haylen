-- Directional light: like the sun it covers the whole canvas from its rotation, so its shadows run across the entire view.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local ui = require('haylen.ui')

local sample = require('sample')
local Stage = require('stage')

local Directional = haylen.class('Directional', sample.Test)

Directional.hints = 'The sun travels from the middle of the screen toward the cursor. Move it with the mouse, a finger, WASD or the left stick, or let the sun turn by itself.'

function Directional:init(entry)
    Directional.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.stage = Stage()
    self.cursor = sample.Cursor()
    self.turning = false
    self.sun = lighting2d.newLight({type = 'directional', rotation = 0.6, color = '#FFFFE6C0', intensity = 1.1, shadows = true, shadowFilter = 'pcf13', shadowSmoothness = 1, shadowColor = '#B0000000'})
end

function Directional:controls()
    return {
        ui.toggle{text = 'Turn by itself', onChange = function(event)
            self.turning = event.checked
        end},
        ui.formField{label = 'Shadow strength', ui.slider{min = 0, max = 1, value = self.sun.shadowColor.a, showValue = true, onChange = function(event)
            self.sun.shadowColor = m.color(0, 0, 0, event.value)
        end}},
    }
end

function Directional:update(dt)
    self.cursor:update(dt)
    if self.turning then
        self.sun.rotation = m.wrapAngle(self.sun.rotation + dt * 0.4)
    else
        local x, y = self.cursor:world(self.camera)
        if x ~= 0 or y ~= 0 then
            self.sun.rotation = math.atan(y, x)
        end
    end
    self:setStatus(string.format('rotation %.2f radians, %.0f degrees', self.sun.rotation, math.deg(self.sun.rotation)))
end

function Directional:render()
    graphics2d.beginWorld(self.camera, {ambientLight = '#FF303A58'})
    self.stage:draw(true)
    graphics2d.drawLight(self.sun)
end

function Directional:renderUi()
    graphics2d.beginScreen()
    local centerX, centerY = self.camera:worldToScreen(0, 0)
    local tipX, tipY = centerX + math.cos(self.sun.rotation) * 220, centerY + math.sin(self.sun.rotation) * 220
    graphics2d.drawLine(centerX, centerY, tipX, tipY, 4, '#C0FFE6C0')
    graphics2d.drawCircle(tipX, tipY, 10, '#FFFFE6C0')
    self.cursor:draw()
end

return Directional
