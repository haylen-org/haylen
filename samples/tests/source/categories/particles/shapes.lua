-- Emitter shapes: where new particles appear. Circles and rings use the x of `shapeSize` as their radius, rectangles use half their width and height, and cones fill the sector of the direction and spread.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Shapes = haylen.class('Shapes', ParticleTest)

Shapes.list = {
    {name = 'point', size = {0, 0}},
    {name = 'circle', size = {110, 0}},
    {name = 'ring', size = {110, 0}},
    {name = 'rectangle', size = {110, 70}},
    {name = 'cone', size = {140, 0}},
}

function Shapes:init(entry)
    Shapes.super.init(self, entry)
    self.speed = 10
    self.emitters = {}
    for index, shape in ipairs(Shapes.list) do
        local emitter = particles2d.newEmitter({texture = ParticleTest.texture('soft'), rate = 90, lifetime = {1.5, 2}, speed = {self.speed, self.speed * 1.5}, spread = shape.name == 'cone' and 1.2 or 6.2832, startSize = 18, endSize = 4, colors = {'#FF80E0FF', '#0040A0FF'}, shape = shape.name, shapeSize = shape.size, blend = 'additive', layer = 2, seed = 30 + index})
        emitter.position = {(index - 3) * 330, 40}
        self.emitters[index] = emitter
    end
end

function Shapes:enter()
    self:frame{
        hint = 'Slow particles show where each shape spawns them. Raise the speed to see them leave the shape.',
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
    for _, emitter in ipairs(self.emitters) do
        emitter:update(dt)
    end
    self:status(string.format('Speed %.0f to %.0f', self.speed, self.speed * 1.5))
end

-- Outlines the spawn area of an emitter so the particles can be compared with it.
function Shapes.drawArea(emitter, shape)
    local x, y = emitter.x, emitter.y
    local order = {layer = 1}
    if shape.name == 'circle' or shape.name == 'ring' then
        graphics2d.drawRing(x, y, shape.size[1], 2, '#60FFFFFF', order)
    elseif shape.name == 'rectangle' then
        graphics2d.drawRectOutline({x - shape.size[1], y - shape.size[2], shape.size[1] * 2, shape.size[2] * 2}, 2, '#60FFFFFF', order)
    elseif shape.name == 'cone' then
        graphics2d.drawArc(x, y, shape.size[1], 2, -1.5708 - 0.6, -1.5708 + 0.6, '#60FFFFFF', order)
        for _, angle in ipairs({-1.5708 - 0.6, -1.5708 + 0.6}) do
            graphics2d.drawLine(x, y, x + math.cos(angle) * shape.size[1], y + math.sin(angle) * shape.size[1], 2, '#60FFFFFF', order)
        end
    else
        graphics2d.drawCircle(x, y, 4, '#60FFFFFF', order)
    end
end

function Shapes:draw(area)
    ParticleTest.backdrop({0.04, 0.05, 0.1}, {0.08, 0.1, 0.18})
    for index, emitter in ipairs(self.emitters) do
        Shapes.drawArea(emitter, Shapes.list[index])
        emitter:draw()
        ParticleTest.label('Shape "' .. Shapes.list[index].name .. '"', emitter.x, emitter.y + 170)
    end
end

return Shapes
