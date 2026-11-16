-- Effects of your own on glass: two `.shader` files of the test project shade glass draws through their materials and read what lies behind them with `haylen_backdrop`. The mosaic turns a panel into privacy glass of square cells, and the lens at the cursor magnifies the landscape around its center, with the cell, the lines and the zoom on the controls.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Scenery = require('categories.effects.scenery')

local OwnEffects = haylen.class('OwnEffects', ParticleTest)

OwnEffects.code = [[
-- The file content/shaders/glass-mosaic.glsl, which "haylen app shaders" compiles, reads behind the glass.
--   vec2 middle = (floor(haylen_output_point / cell) + 0.5) * cell;
--   vec4 behind = haylen_backdrop(middle - haylen_output_point);
local mosaic = graphics2d.newMaterial(assets.shader('shaders/glass-mosaic.shader'), {cell = 32, lines = 0.1})
graphics2d.drawGlass({-820, -380, 760, 560}, {radius = 40, blur = 0, refraction = 16, material = mosaic})
lens:set('center', {x, y})
graphics2d.drawGlass({x - 180, y - 180, 360, 360}, {kind = 'ellipse', blur = 0, material = lens})]]

OwnEffects.lensRadius = 180

function OwnEffects:init(entry)
    OwnEffects.super.init(self, entry)
    self.scenery = Scenery()
    self.knight = assets.texture('sprites/images/hero.png', {filter = 'linear'})
    self.cell, self.lines, self.zoom = 32, 0.1, 1.8
    self.mosaic = graphics2d.newMaterial(assets.shader('shaders/glass-mosaic.shader'), {cell = self.cell, lines = self.lines})
    self.lens = graphics2d.newMaterial(assets.shader('shaders/glass-lens.shader'), {radius = OwnEffects.lensRadius, zoom = self.zoom})
    self.mosaicOptions = {radius = 40, blur = 0, refraction = self.cell / 2, material = self.mosaic, layer = 10}
    self.lensOptions = {kind = 'ellipse', blur = 0, material = self.lens, borderWidth = 3, borderColor = '#801E1B33', layer = 11}
end

function OwnEffects:enter()
    self:frame{
        hint = 'The lens follows the cursor and magnifies whatever lies behind it, the mosaic panel included.',
        code = OwnEffects.code,
        cursor = true,
        controls = {
            ui.formField{label = 'Mosaic cell', ui.slider{id = 'cell', min = 8, max = 96, value = self.cell, showValue = true, decimals = 0, onChange = function(event)
                self.cell = event.value
                self.mosaic:set('cell', event.value)
                self.mosaicOptions.refraction = event.value / 2
            end}},
            ui.formField{label = 'Mosaic lines', ui.slider{id = 'lines', min = 0, max = 0.4, value = self.lines, showValue = true, decimals = 2, onChange = function(event)
                self.lines = event.value
                self.mosaic:set('lines', event.value)
            end}},
            ui.formField{label = 'Lens zoom', ui.slider{id = 'zoom', min = 1, max = 4, value = self.zoom, showValue = true, decimals = 2, onChange = function(event)
                self.zoom = event.value
                self.lens:set('zoom', event.value)
            end}},
        },
    }
end

function OwnEffects:update(dt)
    OwnEffects.super.update(self, dt)
    self.scenery:update(dt)
    self.lens:set('center', {self.cursorX, self.cursorY})
    self:report('The mosaic shows cells of %.0f units, and the lens magnifies %.2f times in its middle.', self.cell, self.zoom)
end

function OwnEffects:draw(area)
    self.scenery:draw()
    graphics2d.draw(self.knight, -440, Scenery.ground, {scaleX = 1.3, scaleY = 1.3, pivotY = 1, layer = 1})
    for index = 0, 5 do
        graphics2d.drawCircle(-750 + index * 300, -200 + math.sin(self.scenery.time * 1.3 + index) * 160, 60, index % 2 == 0 and '#FFF0473E' or '#FF2F86F6', {layer = 2})
    end
    graphics2d.drawGlass({-820, -380, 760, 560}, self.mosaicOptions)
    local radius = OwnEffects.lensRadius
    graphics2d.drawGlass({self.cursorX - radius, self.cursorY - radius, radius * 2, radius * 2}, self.lensOptions)
end

return OwnEffects
