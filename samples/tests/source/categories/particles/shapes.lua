-- Emitter shapes: where new particles appear. Circles and rings use the x of `shapeSize` as their radius, rectangles use half their width and height, and cones fill the sector of the direction and spread.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Shapes = haylen.class('Shapes', ParticleTest)

Shapes.code = [[
-- Circles and rings take a radius.
particles2d.newEmitter({texture = dot, shape = 'circle', shapeSize = {110, 0}})
-- Rectangles take half their width and height.
particles2d.newEmitter({texture = dot, shape = 'rectangle', shapeSize = {110, 70}})
-- Cones fill the sector of the direction and the spread.
particles2d.newEmitter({texture = dot, shape = 'cone', shapeSize = {140, 0}, spread = 1.2})]]

Shapes.guide = {layer = 1}
-- The middle of the row of shapes, a little above the middle of the view to leave room for the captions.
Shapes.row = -60
Shapes.list = {
    {name = 'point', title = 'Point', size = {0, 0}},
    {name = 'circle', title = 'Circle', size = {110, 0}},
    {name = 'ring', title = 'Ring', size = {110, 0}},
    {name = 'rectangle', title = 'Rectangle', size = {110, 70}},
    {name = 'cone', title = 'Cone', size = {140, 0}},
}

function Shapes:init(entry)
    Shapes.super.init(self, entry)
    self.speed = 10
    self.emitters = {}
    for index, shape in ipairs(Shapes.list) do
        local emitter = particles2d.newEmitter(ParticleTest.look('soft-dot', {rate = 90, lifetime = {1.5, 2}, speed = {self.speed, self.speed * 1.5}, spread = shape.name == 'cone' and 1.2 or 6.2832, startSize = 18, endSize = 4, colors = {'#FF80E0FF', '#0040A0FF'}, shape = shape.name, shapeSize = shape.size, blend = 'additive', layer = 2, seed = 30 + index}))
        self.emitters[index] = emitter
    end
    self.scale = 1
end

-- Spreads the five shapes over the width of the canvas, which changes with the play area, and grows them with the room each one gets, so they fill the view in landscape and in portrait.
function Shapes:fit()
    local area = self.camera:visibleBounds()
    if area.width == self.width then
        return
    end
    self.width = area.width
    local spacing = math.min(520, (area.width - 120) / #Shapes.list)
    self.scale = spacing / 340
    for index, emitter in ipairs(self.emitters) do
        local shape = Shapes.list[index]
        local x = area.x + area.width / 2 + (index - 3) * spacing
        local y = shape.name == 'cone' and Shapes.row + shape.size[1] * self.scale / 2 or Shapes.row
        emitter.position = {x, y}
        emitter:configure({shapeSize = {shape.size[1] * self.scale, shape.size[2] * self.scale}, startSize = 18 * self.scale})
    end
end

function Shapes:enter()
    self:frame{
        hint = 'Slow particles show where each shape spawns them. Raise the speed to see them leave the shape.',
        code = Shapes.code,
        controls = {ui.formField{label = 'Speed', ui.slider{id = 'speed', min = 0, max = 200, value = self.speed, showValue = true, decimals = 0, onChange = function(event)
            self.speed = event.value
            for _, emitter in ipairs(self.emitters) do
                emitter:configure({speed = {event.value, event.value * 1.5}})
            end
        end}}},
        focus = 'speed',
    }
end

function Shapes:update(dt)
    Shapes.super.update(self, dt)
    if not self.stage then
        return
    end
    self:fit()
    for _, emitter in ipairs(self.emitters) do
        emitter:update(dt)
    end
    self:report('New particles leave their shapes at %.0f to %.0f units per second.', self.speed, self.speed * 1.5)
end

-- Outlines the spawn area of an emitter so the particles can be compared with it.
function Shapes.drawArea(emitter, shape, scale)
    local x, y = emitter.x, emitter.y
    local width, height = shape.size[1] * scale, shape.size[2] * scale
    local order, line, color = Shapes.guide, 5, '#80C8F0FF'
    if shape.name == 'circle' or shape.name == 'ring' then
        graphics2d.drawRing(x, y, width, line, color, order)
    elseif shape.name == 'rectangle' then
        graphics2d.drawShape({x - width, y - height, width * 2, height * 2}, {radius = 10, color = '#00000000', borderWidth = line, borderColor = color, layer = 1})
    elseif shape.name == 'cone' then
        graphics2d.drawArc(x, y, width, line, -1.5708 - 0.6, -1.5708 + 0.6, color, order)
        for _, angle in ipairs({-1.5708 - 0.6, -1.5708 + 0.6}) do
            graphics2d.drawLine(x, y, x + math.cos(angle) * width, y + math.sin(angle) * width, line, color, order)
        end
    else
        graphics2d.drawCircle(x, y, 8, color, order)
    end
end

function Shapes:draw(area)
    ParticleTest.backdrop({0.04, 0.05, 0.1}, {0.08, 0.1, 0.18})
    for index, emitter in ipairs(self.emitters) do
        local shape = Shapes.list[index]
        Shapes.drawArea(emitter, shape, self.scale)
        emitter:draw()
        ParticleTest.label(shape.title, emitter.x, Shapes.row + 110 * self.scale + 60, string.format("shape = '%s'", shape.name))
    end
end

return Shapes
