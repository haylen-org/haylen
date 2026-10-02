-- Catmull-Rom, Bezier and B-spline curves through control points you drag, sampled at even distances and walked at a steady speed by arc length.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local sample = require('sample')

local Splines = haylen.class('Splines', sample.Test)

local kPoints = {{-620, 200}, {-420, -260}, {-160, 240}, {60, -300}, {280, 200}, {480, -220}, {640, 180}}
local kPickDistance = 40
local kSpeed = 380

function Splines:enter()
    Splines.super.enter(self, {
        hint = 'Drag the control points. Bezier curves pass through every third point and bend toward the others, and a closed Bezier uses the first six points.',
        controls = {
            ui.radioGroup{id = 'kind', items = {{id = 'catmullRom', text = 'Catmull-Rom'}, {id = 'bezier', text = 'Bezier'}, {id = 'bSpline', text = 'B-spline'}}, selected = 'catmullRom', onChange = function(event) self:change('kind', event.value) end},
            ui.checkbox{id = 'closed', text = 'Closed curve', onChange = function(event) self:change('closed', event.checked) end},
            ui.label{text = 'Sample spacing', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'spacing', value = 60, min = 20, max = 200, step = 5, showValue = true, decimals = 0, onChange = function(event) self:change('spacing', event.value) end},
        },
        stats = true,
        focus = 'kind',
    })
    self.options = {kind = 'catmullRom', closed = false, spacing = 60}
    self.points = {}
    for index, point in ipairs(kPoints) do
        self.points[index] = {point[1], point[2]}
    end
    self.traveled = 0
    self:rebuild()
end

function Splines:change(name, value)
    self.options[name] = value
    self:rebuild()
end

function Splines:rebuild()
    local options = self.options
    local points = self.points
    if options.kind == 'bezier' and options.closed then
        points = {table.unpack(self.points, 1, 6)}
    end
    self.spline = m.spline(points, {kind = options.kind, closed = options.closed})
    self.curve = self.spline:sample(160)
    self.samples = self.spline:sampleByDistance(options.spacing)
end

function Splines:exit()
    Splines.super.exit(self)
    self.spline, self.curve, self.samples, self.points = nil, nil, nil, nil
end

function Splines:update(dt)
    Splines.super.update(self, dt)
    local pointer = self.pointer
    if pointer.pressed then
        self.dragged = nil
        for index, point in ipairs(self.points) do
            if math.abs(point[1] - pointer.worldX) < kPickDistance and math.abs(point[2] - pointer.worldY) < kPickDistance then
                self.dragged = index
            end
        end
    elseif not pointer.down then
        self.dragged = nil
    end
    if self.dragged then
        self.points[self.dragged] = {pointer.worldX, pointer.worldY}
        self:rebuild()
    end
    self.traveled = (self.traveled + kSpeed * dt) % self.spline.length
    self:showStats(string.format('Kind %s\nSegments %d\nLength %.0f\nSamples %d', self.spline.kind, self.spline.segmentCount, self.spline.length, #self.samples))
end

function Splines:render()
    self:beginWorld()
    graphics2d.drawPolyline(self.points, 1.5, '#55FFFFFF', self.options.closed)
    graphics2d.drawPolyline(self.curve, 4, '#FF4FC3F7', self.options.closed, {layer = 1})
    for _, point in ipairs(self.samples) do
        graphics2d.drawCircle(point.x, point.y, 5, '#FFFFFFFF', {layer = 2})
    end
    for index, point in ipairs(self.points) do
        local color = index == self.dragged and '#FFFFD54F' or '#FFEF5350'
        graphics2d.drawCircle(point[1], point[2], 12, color, {layer = 3})
    end
    local position = self.spline:pointAtDistance(self.traveled)
    local direction = self.spline:tangentAtDistance(self.traveled)
    local side = direction:perpendicular()
    local tip = position + direction * 30
    graphics2d.drawPolygon({tip, position - direction * 14 + side * 16, position - direction * 14 - side * 16}, '#FFFFD54F', {layer = 4})
end

return Splines
