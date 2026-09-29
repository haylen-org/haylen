-- A waypoint graph: A* between two points, Dijkstra costs from the start to every point, heavy points that paths avoid, doors that close and one-way roads.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local ui = require('haylen.ui')

local sample = require('sample')

local Waypoints = haylen.class('Waypoints', sample.Test)

local kColumns, kRows = 8, 5
local kSpacing = 180
local kHeavy = 4
local kPickDistance = 50

function Waypoints:enter()
    Waypoints.super.enter(self, {
        hint = 'Tap or click a point with the selected brush. Closed doors block every path, and heavy points cost four times their distance.',
        controls = {
            ui.radioGroup{id = 'brush', items = {{id = 'start', text = 'Move the start'}, {id = 'goal', text = 'Move the goal'}, {id = 'door', text = 'Open or close a door'}, {id = 'weight', text = 'Make a point heavy'}}, selected = 'goal', onChange = function(event) self.brush = event.value end},
            ui.checkbox{id = 'costs', text = 'Show Dijkstra costs', checked = true, onChange = function(event) self.showCosts = event.checked end},
            ui.button{id = 'reset', text = 'New roads', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'brush',
    })
    self.brush, self.showCosts = 'goal', true
    self.random = m.random(47)
    self:build()
end

function Waypoints:build()
    self.graph = navigation2d.newGraph()
    self.roads = {}
    local function id(column, row)
        return row * kColumns + column + 1
    end
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local x = (column - (kColumns - 1) / 2) * kSpacing + self.random:range(-40, 40)
            local y = (row - (kRows - 1) / 2) * kSpacing + self.random:range(-40, 40)
            self.graph:addPoint(id(column, row), x, y)
        end
    end
    local function road(from, to, oneWay)
        self.graph:connect(from, to, not oneWay)
        self.roads[#self.roads + 1] = {from = from, to = to, oneWay = oneWay}
    end
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            if column < kColumns - 1 and self.random:chance(0.85) then
                road(id(column, row), id(column + 1, row), self.random:chance(0.12))
            end
            if row < kRows - 1 and self.random:chance(0.75) then
                road(id(column, row), id(column, row + 1), self.random:chance(0.12))
            end
            if column < kColumns - 1 and row < kRows - 1 and self.random:chance(0.25) then
                road(id(column, row), id(column + 1, row + 1))
            end
        end
    end
    self.start, self.goal = 1, kColumns * kRows
    self:search()
end

function Waypoints:search()
    self.route, self.cost = self.graph:findPath(self.start, self.goal)
    self.distances = self.graph:distances(self.start)
end

function Waypoints:apply(point)
    if self.brush == 'door' then
        self.graph:setEnabled(point, not self.graph:enabled(point))
    elseif self.brush == 'weight' then
        self.graph:setWeight(point, self.graph:weight(point) > 1 and 1 or kHeavy)
    else
        self[self.brush] = point
    end
    self:search()
end

function Waypoints:exit()
    Waypoints.super.exit(self)
    self.graph, self.roads = nil, nil
end

function Waypoints:update(dt)
    Waypoints.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer = self.pointer
    if pointer.pressed then
        local point = self.graph:closestPoint(pointer.worldX, pointer.worldY, true)
        if point and self.graph:position(point):distance({pointer.worldX, pointer.worldY}) < kPickDistance then
            self:apply(point)
        end
    end
    local reachable = 0
    for _ in pairs(self.distances) do
        reachable = reachable + 1
    end
    self:showStats(string.format('points %d\nreachable %d\nroute %s', self.graph.size, reachable, self.route and string.format('%d points, cost %.0f', #self.route, self.cost) or 'blocked'))
end

function Waypoints:render()
    self:beginWorld()
    local graph = self.graph
    for _, road in ipairs(self.roads) do
        local a, b = graph:position(road.from), graph:position(road.to)
        graphics2d.drawLine(a.x, a.y, b.x, b.y, 6, '#FF455A64')
        if road.oneWay then
            local middle, direction = (a + b) / 2, (b - a):normalized()
            local side = direction:perpendicular() * 12
            local tip = middle + direction * 16
            graphics2d.drawPolygon({tip, middle - direction * 8 + side, middle - direction * 8 - side}, '#FFB0BEC5', {layer = 1})
        end
    end
    if self.route then
        local points = {}
        for index, point in ipairs(self.route) do
            points[index] = graph:position(point)
        end
        graphics2d.drawPolyline(points, 10, '#CCFFD54F', false, {layer = 2})
    end
    for _, point in ipairs(graph:points()) do
        local position = graph:position(point)
        local color = not graph:enabled(point) and '#FFEF5350' or (graph:weight(point) > 1 and '#FF8D6E63' or '#FFB0BEC5')
        local radius = graph:weight(point) > 1 and 22 or 15
        if point == self.start or point == self.goal then
            color, radius = point == self.start and '#FF66BB6A' or '#FFEF5350', 22
        end
        graphics2d.drawCircle(position.x, position.y, radius, color, {layer = 3})
        if not graph:enabled(point) then
            graphics2d.drawLine(position.x - 12, position.y - 12, position.x + 12, position.y + 12, 4, '#FF1A2029', {layer = 4})
        end
        local cost = self.distances[point]
        if self.showCosts and cost then
            graphics2d.drawText(nil, string.format('%.0f', cost), position.x, position.y - 26, {size = 20, anchor = {0.5, 1}, color = '#FFE0E0E0', layer = 5})
        end
    end
end

return Waypoints
