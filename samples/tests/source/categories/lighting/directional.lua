-- Directional light: like the sun it covers the whole canvas from its rotation, so its shadows run across the entire view. The sun at the edge of the courtyard shows where the light comes from.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')
local Town = require('categories.lighting.town')

local Directional = haylen.class('Directional', LightingTest)

Directional.code = [[
local sun = lighting2d.newLight({type = 'directional', color = '#FFFFE6C0', shadows = true})
sun.shadowFilter, sun.shadowColor = 'pcf13', '#B0000000'
sun.rotation = math.atan(y, x)  -- The direction the light travels.
graphics2d.drawOccluder(crate)
graphics2d.drawLight(sun)  -- Its shadows run across the whole view.]]

function Directional:init(entry)
    Directional.super.init(self, entry)
    self.room = Room()
    self.town = Town()
    self.ambientLight = '#FF303A58'
    self.turning = false
    self.sun = lighting2d.newLight({type = 'directional', rotation = 0.6, color = '#FFFFE6C0', intensity = 1.1, shadows = true, shadowFilter = 'pcf13', shadowSmoothness = 1, shadowColor = '#B0000000'})
end

function Directional:enter()
    self:frame{
        hint = 'The light travels from the sun toward the cursor, so point where the shadows should fall: with the mouse, a finger, the arrows, WASD or a stick. Or let the sun turn by itself.',
        code = Directional.code,
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
    self:status(string.format('The light travels at %.0f degrees, and every shadow points the same way.', m.wrapAngle(self.sun.rotation) / math.pi * 180 % 360))
end

-- Places the sun against the edge of the canvas that the light comes from, opposite to the way it travels.
function Directional:drawSun()
    local area = graphics2d.canvasBounds()
    local dx, dy = -math.cos(self.sun.rotation), -math.sin(self.sun.rotation)
    local margin = 110
    local reachX = (area.width / 2 - margin) / math.max(math.abs(dx), 1e-4)
    local reachY = (area.height / 2 - margin) / math.max(math.abs(dy), 1e-4)
    local reach = math.min(reachX, reachY)
    local x, y = area.x + area.width / 2 + dx * reach, area.y + area.height / 2 + dy * reach
    graphics2d.draw(graphics2d.lightTexture(), x, y, {width = 420, height = 420, color = '#A0FFE08A', blend = 'additive', layer = 6, unshaded = true})
    self.town:piece('sun', x, y, 0.5, {layer = 7, rotation = haylen.elapsed() * 0.2, unshaded = true})
end

function Directional:draw(area)
    self.room:draw()
    graphics2d.drawLight(self.sun)
    self:drawSun()
end

return Directional
