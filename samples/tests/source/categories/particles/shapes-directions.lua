-- Shapes and direction modes: the spawn areas `ellipse`, `rectangleEdge`, `arc`, `polygon` and `polyline`, turned by `shapeAngle` and spread across `shapeThickness`, send their particles along `direction` with `'fixed'`, away from the center with `'outward'`, toward it with `'inward'` or around it with `'tangent'`. A polyline whose points follow the cursor changes with `emitter:setShapePoints` whenever the cursor moves far enough.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local ShapesDirections = haylen.class('ShapesDirections', ParticleTest)

ShapesDirections.modes = {{id = 'fixed', text = 'Fixed'}, {id = 'outward', text = 'Outward'}, {id = 'inward', text = 'Inward'}, {id = 'tangent', text = 'Tangent'}}
ShapesDirections.shapes = {
    {name = 'ellipse', size = {150, 70}, color = '#FF80E0FF'},
    {name = 'rectangleEdge', size = {130, 80}, color = '#FFFFD060'},
    {name = 'arc', size = {120, 0}, arc = {-2.8, -0.35}, color = '#FFFF80A0'},
    {name = 'polygon', points = {{-120, -40}, {10, -40}, {10, -110}, {130, 0}, {10, 110}, {10, 40}, {-120, 40}}, color = '#FF80FFA0'},
    {name = 'polyline', points = {{-140, 60}, {-70, -60}, {0, 40}, {70, -60}, {140, 60}}, color = '#FFC0A0FF'},
}
ShapesDirections.row = -250
ShapesDirections.pathPoints = 16
ShapesDirections.pathStep = 36
ShapesDirections.lines = {layer = 1}

function ShapesDirections:init(entry)
    ShapesDirections.super.init(self, entry)
    self.mode = 'outward'
    self.angle = 0
    self.thickness = 0
    local dot = ParticleTest.library('soft_dot')
    self.emitters = {}
    self.labels = {}
    for index, shape in ipairs(ShapesDirections.shapes) do
        self.labels[index] = 'Shape "' .. shape.name .. '"'
        local emitter = particles2d.newEmitter({texture = dot, rate = 60, lifetime = {1.2, 1.6}, speed = {30, 60}, spread = 0.3, directionMode = self.mode, startSize = 16, endSize = 4, colors = {shape.color, shape.color, '#00FFFFFF'}, shape = shape.name, shapeSize = shape.size, shapeArc = shape.arc, shapePoints = shape.points, blend = 'additive', layer = 2, seed = 250 + index})
        emitter.x, emitter.y = -760 + (index - 1) * 380, ShapesDirections.row
        self.emitters[index] = emitter
    end
    self.outlines = {}
    self:traceOutlines()

    -- The path keeps its point tables and recycles the oldest one for every new point, so following the cursor allocates nothing.
    self.path = {}
    for index = 1, ShapesDirections.pathPoints do
        self.path[index] = {-600 + (index - 1) * 80, 300}
    end
    self.follower = particles2d.newEmitter({texture = dot, rate = 160, lifetime = {0.8, 1.2}, speed = {20, 70}, spread = 0.8, gravity = {0, -60}, startSize = 18, endSize = 2, colors = {'#FFFFFFFF', '#FF60E0FF', '#0040A0FF'}, shape = 'polyline', shapePoints = self.path, blend = 'additive', maxParticles = 512, layer = 3, seed = 256})
    self.emitters[#self.emitters + 1] = self.follower
end

-- Computes the outline of every spawn area turned by the shape angle, once at the start and again whenever the angle changes.
function ShapesDirections:traceOutlines()
    for index, shape in ipairs(ShapesDirections.shapes) do
        local x, y = self.emitters[index].x, self.emitters[index].y
        local points = {}
        if shape.name == 'ellipse' then
            for step = 0, 47 do
                local angle = step / 48 * m.tau
                points[#points + 1] = {math.cos(angle) * shape.size[1], math.sin(angle) * shape.size[2]}
            end
        elseif shape.name == 'rectangleEdge' then
            local w, h = shape.size[1], shape.size[2]
            points = {{-w, -h}, {w, -h}, {w, h}, {-w, h}}
        elseif shape.name == 'arc' then
            for step = 0, 24 do
                local angle = shape.arc[1] + (shape.arc[2] - shape.arc[1]) * step / 24
                points[#points + 1] = {math.cos(angle) * shape.size[1], math.sin(angle) * shape.size[1]}
            end
        else
            for _, point in ipairs(shape.points) do
                points[#points + 1] = {point[1], point[2]}
            end
        end
        local cos, sin = math.cos(self.angle), math.sin(self.angle)
        for _, point in ipairs(points) do
            point[1], point[2] = x + point[1] * cos - point[2] * sin, y + point[1] * sin + point[2] * cos
        end
        self.outlines[index] = {points = points, closed = shape.name ~= 'arc' and shape.name ~= 'polyline'}
    end
end

function ShapesDirections:reshape()
    for index = 1, #ShapesDirections.shapes do
        self.emitters[index]:configure({shapeAngle = self.angle, shapeThickness = self.thickness})
    end
    self.follower:configure({shapeThickness = self.thickness})
    self:traceOutlines()
end

function ShapesDirections:enter()
    self:frame{
        hint = 'The top row spawns on five shapes and sends the particles the way of the direction mode. The bottom line follows the cursor: move it with the mouse, a finger, the arrows, WASD or a stick.',
        cursor = true,
        controls = {
            ui.formField{label = 'Direction mode', ui.radioGroup{id = 'mode', items = ShapesDirections.modes, selected = self.mode, onChange = function(event)
                self.mode = event.value
                for index = 1, #ShapesDirections.shapes do
                    self.emitters[index]:configure({directionMode = event.value})
                end
            end}},
            ui.formField{label = 'Shape angle', ui.slider{id = 'angle', min = -3.1416, max = 3.1416, value = self.angle, showValue = true, onChange = function(event)
                self.angle = event.value
                self:reshape()
            end}},
            ui.formField{label = 'Shape thickness', ui.slider{id = 'thickness', min = 0, max = 80, value = self.thickness, showValue = true, decimals = 0, onChange = function(event)
                self.thickness = event.value
                self:reshape()
            end}},
        },
    }
end

-- Adds the cursor as the head of the path once it moved a step away from the last head.
function ShapesDirections:follow()
    local path = self.path
    local head = path[#path]
    if (self.cursorX - head[1]) ^ 2 + (self.cursorY - head[2]) ^ 2 < ShapesDirections.pathStep ^ 2 then
        return
    end
    local oldest = path[1]
    table.move(path, 2, #path, 1)
    oldest[1], oldest[2] = self.cursorX, self.cursorY
    path[#path] = oldest
    self.follower:setShapePoints(path)
end

function ShapesDirections:update(dt)
    ShapesDirections.super.update(self, dt)
    self:follow()
    local count = ParticleTest.updateAll(self.emitters, dt)
    self:report('Direction mode "%s"   Shape angle %.2f   Thickness %.0f   Particles %d', self.mode, self.angle, self.thickness, count)
end

function ShapesDirections:draw(area)
    ParticleTest.backdrop({0.04, 0.05, 0.1}, {0.08, 0.1, 0.18})
    for index, outline in ipairs(self.outlines) do
        graphics2d.drawPolyline(outline.points, 2, '#60FFFFFF', outline.closed, ShapesDirections.lines)
        ParticleTest.label(self.labels[index], self.emitters[index].x, -60)
    end
    graphics2d.drawPolyline(self.path, 2, '#40FFFFFF', false, ShapesDirections.lines)
    ParticleTest.label('A polyline that follows the cursor', 0, 440)
    ParticleTest.drawAll(self.emitters)
end

return ShapesDirections
