-- Shadows: lights with shadows are blocked by occluders, closed outlines and open walls, with a filter, a color and a smoothness of their own.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local Shadows = haylen.class('Shadows', LightingTest)

Shadows.code = [[
local lamp = lighting2d.newLight({radius = 900, shadows = true, shadowFilter = 'pcf5'})
lamp.shadowSmoothness, lamp.shadowColor = 1, '#FF102050'
local wall = lighting2d.newOccluder({points = {380, -380, 380, -140, 500, -20}, closed = false})
graphics2d.drawOccluder(wall)  -- An open outline casts a shadow from either side.
crate.cull = 'counterClockwise'  -- Its lit sides cast no shadow.
graphics2d.drawLight(lamp)]]

Shadows.colors = {black = '#FF000000', blue = '#FF102050', soft = '#80000000'}

function Shadows:init(entry)
    Shadows.super.init(self, entry)
    self.room = Room()
    self.ambientLight = '#FF181C26'
    self.held = lighting2d.newLight({radius = 900, color = '#FFFFE0B0', intensity = 1.3, shadows = true, shadowFilter = 'pcf5', shadowSmoothness = 1})
    self.corner = lighting2d.newLight({x = 820, y = 420, radius = 700, color = '#FFFF5050', intensity = 0.7, shadows = true, shadowFilter = 'pcf13'})
    self.wall = lighting2d.newOccluder({points = {380, -380, 380, -140, 500, -20}, closed = false})
    self.door = lighting2d.newOccluder({points = {0, 0, 180, 0}, closed = false, x = -160, y = 80})
    self.bricks = Room.bricks()
end

function Shadows:enter()
    self:frame{
        hint = 'Move the light with the mouse, a finger, the arrows, WASD or a stick. The brick wall is an open outline and the wooden door turns on its hinge.',
        code = Shadows.code,
        cursor = true,
        controls = {
            ui.formField{label = 'Filter', ui.radioGroup{id = 'filter', horizontal = true, selected = 'pcf5', items = {{id = 'none', text = 'None'}, {id = 'pcf5', text = 'PCF5'}, {id = 'pcf13', text = 'PCF13'}}, onChange = function(event)
                self.held.shadowFilter = event.value
            end}},
            ui.formField{label = 'Smoothness', ui.slider{id = 'smoothness', min = 0, max = 6, value = self.held.shadowSmoothness, showValue = true, onChange = function(event)
                self.held.shadowSmoothness = event.value
            end}},
            ui.formField{label = 'Shadow color', ui.radioGroup{id = 'color', selected = 'black', items = {{id = 'black', text = 'Black'}, {id = 'blue', text = 'Blue'}, {id = 'soft', text = 'Half transparent'}}, onChange = function(event)
                self.held.shadowColor = Shadows.colors[event.value]
            end}},
            ui.toggle{id = 'cull', text = 'Cull the lit sides', checked = true, onChange = function(event)
                for _, occluder in ipairs(self.room.occluders) do
                    occluder.cull = event.checked and 'counterClockwise' or 'disabled'
                end
            end},
        },
    }
end

function Shadows:update(dt)
    Shadows.super.update(self, dt)
    self.held.x, self.held.y = self.cursorX, self.cursorY
    self.door.rotation = self.door.rotation + dt * 0.8
    self:status(string.format('The lamp filters its shadows with "%s" at a smoothness of %.1f, and %d lights cast shadows.', self.held.shadowFilter, self.held.shadowSmoothness, graphics2d.stats().shadows))
end

-- Returns the points of an occluder in the world as a flat list for a stroke.
function Shadows.flat(occluder)
    local flat = {}
    for _, point in ipairs(occluder:worldPoints()) do
        flat[#flat + 1] = point.x
        flat[#flat + 1] = point.y
    end
    return flat
end

function Shadows:draw(area)
    self.room:draw()
    Room.drawWall(self.bricks, Shadows.flat(self.wall), 56, 1)
    local door = Shadows.flat(self.door)
    graphics2d.drawStroke(door, {width = 40, color = '#FF1E1B33', cap = 'round', layer = 1})
    graphics2d.drawStroke(door, {width = 30, color = '#FFA8683A', cap = 'round', layer = 2})
    graphics2d.drawStroke(door, {width = 6, color = '#FF7A4A28', cap = 'round', layer = 3})
    graphics2d.drawCircle(door[1], door[2], 18, '#FF1E1B33', {layer = 3})
    graphics2d.drawCircle(door[1], door[2], 12, '#FF8890B0', {layer = 4})
    graphics2d.drawOccluder(self.wall)
    graphics2d.drawOccluder(self.door)
    Room.drawLamp(self.corner.x, self.corner.y, self.corner.color, 26)
    graphics2d.drawLight(self.corner)
    Room.drawLamp(self.held.x, self.held.y, self.held.color, 26)
    graphics2d.drawLight(self.held)
end

return Shadows
