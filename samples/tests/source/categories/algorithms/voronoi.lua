-- Voronoi cells and the Delaunay triangulation of points the player adds, and Lloyd relaxation on a worker thread that evens their spacing out step by step.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local procedural2d = require('haylen.procedural2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')

local Voronoi = haylen.class('Voronoi', AlgorithmTest)

local kBounds = {-740, -400, 1480, 800}
local kGlide = 0.35

function Voronoi:enter()
    self:frame{
        hint = 'Tap or click to add a point. Relax moves every point to the center of its cell, which evens the cells out after a few steps. R or the X button relaxes once.',
        controls = {
            ui.button{id = 'relax', text = 'Relax once', onClick = function() self:relax() end},
            ui.checkbox{id = 'cells', text = 'Voronoi cells', checked = true, onChange = function(event) self.showCells = event.checked end},
            ui.checkbox{id = 'triangles', text = 'Delaunay triangles', checked = true, onChange = function(event) self.showTriangles = event.checked end},
            ui.button{id = 'scatter', text = 'New points', onClick = function() self:scatter() end},
        },
        focus = 'relax',
    }
    self.showCells, self.showTriangles = true, true
    self.random = m.random(89)
    self:scatter()
end

function Voronoi:scatter()
    self.points = {}
    for index = 1, 70 do
        self.points[index] = {self.random:range(kBounds[1], kBounds[1] + kBounds[3]), self.random:range(kBounds[2], kBounds[2] + kBounds[4])}
    end
    self.moves, self.relaxations = nil, 0
    self:rebuild()
end

-- The triangulation is quick enough to run at once, while the cells come from a worker thread.
function Voronoi:rebuild()
    profiler.beginScope('delaunay')
    self.mesh = procedural2d.delaunay(self.points)
    profiler.endScope()
    self.request = (self.request or 0) + 1
    local request, points = self.request, self.points
    self:spawn(function()
        local cells = procedural2d.voronoiAsync(points, kBounds):await()
        if cells and request == self.request then
            self.cells = cells
        end
    end)
end

function Voronoi:relax()
    if self.moves then
        return
    end
    local from = self.points
    self:spawn(function()
        local to = procedural2d.relaxAsync(from, kBounds, 1):await()
        if to and self.points == from then
            self.moves = {from = from, to = to, time = 0}
        end
    end)
end

-- Points glide to their relaxed places, and the diagram follows them every frame.
function Voronoi:glide(dt)
    local moves = self.moves
    moves.time = math.min(kGlide, moves.time + dt)
    local t = m.ease('quadInOut', moves.time / kGlide)
    local points = {}
    for index, point in ipairs(moves.from) do
        local target = moves.to[index]
        points[index] = {m.lerp(point[1], target.x, t), m.lerp(point[2], target.y, t)}
    end
    self.points = points
    if moves.time >= kGlide then
        self.moves = nil
        self.relaxations = self.relaxations + 1
    end
    self:rebuild()
end

function Voronoi:update(dt)
    Voronoi.super.update(self, dt)
    if input.pressed('reset') then
        self:relax()
    end
    if self.moves then
        self:glide(dt)
    elseif self.pointer.pressed then
        self.points[#self.points + 1] = {self.pointer.worldX, self.pointer.worldY}
        self:rebuild()
    end
    self:status(string.format('Points %d, triangles %d, relaxations %d, Delaunay %.3f ms', #self.points, #self.mesh.triangles // 3, self.relaxations, self:timing('delaunay')))
end

function Voronoi:draw(area)
    if self.showCells and self.cells then
        for index, cell in ipairs(self.cells) do
            if #cell >= 3 then
                graphics2d.drawPolygon(cell, m.fromHsv((index * 0.618) % 1, 0.35, 0.45))
                graphics2d.drawPolyline(cell, 2, '#FF1A2029', true, {layer = 1})
            end
        end
    end
    if self.showTriangles then
        local triangles = self.mesh.triangles
        for index = 1, #triangles, 3 do
            local a, b, c = self.points[triangles[index]], self.points[triangles[index + 1]], self.points[triangles[index + 2]]
            graphics2d.drawPolyline({a, b, c}, 1.5, '#AAFFFFFF', true, {layer = 2})
        end
    end
    for _, point in ipairs(self.points) do
        graphics2d.drawCircle(point[1], point[2], 5, '#FFFFD54F', {layer = 3})
    end
end

return Voronoi
