-- Shadows: lights with shadows are blocked by occluders, closed outlines and open walls, with a filter, a color and a smoothness of their own.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local sample = require('sample')
local Stage = require('stage')

local Shadows = haylen.class('Shadows', sample.Test)

Shadows.hints = 'Move the light with the mouse, a finger, WASD or the left stick. The wall on the right is an open outline and the door turns.'

Shadows.colors = {black = '#FF000000', blue = '#FF102050', soft = '#80000000'}

function Shadows:init(entry)
    Shadows.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.stage = Stage()
    self.cursor = sample.Cursor()
    self.held = lighting2d.newLight({radius = 900, color = '#FFFFE0B0', intensity = 1.3, shadows = true, shadowFilter = 'pcf5', shadowSmoothness = 1})
    self.corner = lighting2d.newLight({x = 820, y = 420, radius = 700, color = '#FFFF5050', intensity = 0.7, shadows = true, shadowFilter = 'pcf13'})
    self.wall = lighting2d.newOccluder({points = {760, -420, 760, -120, 880, 0}, closed = false})
    self.door = lighting2d.newOccluder({points = {0, 0, 180, 0}, closed = false, x = -160, y = 80})
end

function Shadows:controls()
    return {
        ui.formField{label = 'Filter', ui.radioGroup{horizontal = true, selected = 'pcf5', items = {{id = 'none', text = 'None'}, {id = 'pcf5', text = 'PCF5'}, {id = 'pcf13', text = 'PCF13'}}, onChange = function(event)
            self.held.shadowFilter = event.value
        end}},
        ui.formField{label = 'Smoothness', ui.slider{min = 0, max = 6, value = self.held.shadowSmoothness, showValue = true, onChange = function(event)
            self.held.shadowSmoothness = event.value
        end}},
        ui.formField{label = 'Shadow color', ui.radioGroup{selected = 'black', items = {{id = 'black', text = 'Black'}, {id = 'blue', text = 'Blue'}, {id = 'soft', text = 'Half transparent'}}, onChange = function(event)
            self.held.shadowColor = Shadows.colors[event.value]
        end}},
        ui.toggle{align = 'stretch', text = 'Cull the lit sides', checked = true, onChange = function(event)
            for _, occluder in ipairs(self.stage.occluders) do
                occluder.cull = event.checked and 'counterClockwise' or 'disabled'
            end
        end},
    }
end

function Shadows:update(dt)
    Shadows.super.update(self, dt)
    self.cursor:update(dt)
    self.held.x, self.held.y = self.cursor:world(self.camera)
    self.door.rotation = self.door.rotation + dt * 0.8
    self:setStatus(string.format('shadowFilter %s, shadowSmoothness %.1f, shadow maps %d', self.held.shadowFilter, self.held.shadowSmoothness, graphics2d.stats().shadows))
end

function Shadows:render()
    graphics2d.beginWorld(self.camera, {ambientLight = '#FF181C26'})
    self.stage:draw(true)
    for _, occluder in ipairs({self.wall, self.door}) do
        local points = occluder:worldPoints()
        for index = 1, #points - 1 do
            graphics2d.drawLine(points[index].x, points[index].y, points[index + 1].x, points[index + 1].y, 12, '#FF6A6A76', {layer = 1})
        end
        graphics2d.drawOccluder(occluder)
    end
    graphics2d.drawLight(self.corner)
    graphics2d.drawLight(self.held)
end

function Shadows:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return Shadows
