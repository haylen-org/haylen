-- A waypoint graph: A* between two points, Dijkstra costs from the start to every point, heavy points that paths avoid, doors that close and one-way roads.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')

local Waypoints = haylen.class('Waypoints', AlgorithmTest)

Waypoints.code = [[
local graph = navigation2d.newGraph()
graph:addPoint(3, 180, 180, 4)  -- Point 3 at 180, 180, whose weight of 4 makes it heavy.
graph:connect(2, 3, false)  -- The road from 2 to 3 is one way.
graph:setEnabled(2, false)  -- A closed door blocks every path through it.
local route, cost = graph:findPath(1, 3)
local costs = graph:distances(1)  -- The cost of reaching every point from point 1.]]

local kColumns, kRows = 8, 5
local kSpacing = {188, 158}
local kHeavy = 4
local kPickDistance = 50

function Waypoints:enter()
    self:frame{
        hint = 'Tap or click a point with the selected brush. Closed doors show red and block every path, heavy points show brown and cost four times their distance, and arrows mark one-way roads. R or the X button builds new roads.',
        code = Waypoints.code,
        controls = {
            ui.radioGroup{id = 'brush', items = {{id = 'start', text = 'Move the start'}, {id = 'goal', text = 'Move the goal'}, {id = 'door', text = 'Open or close a door'}, {id = 'weight', text = 'Make a point heavy'}}, selected = 'goal', onChange = function(event) self.brush = event.value end},
            ui.checkbox{id = 'costs', text = 'Show Dijkstra costs', checked = true, onChange = function(event) self.showCosts = event.checked end},
            ui.button{id = 'reset', text = 'New roads', onClick = function() self:build() end},
        },
        focus = 'brush',
    }
    self.brush, self.showCosts = 'goal', true
    self.random = m.random(47)
    self.art = Art()
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
            local x = (column - (kColumns - 1) / 2) * kSpacing[1] + self.random:range(-40, 40)
            local y = (row - (kRows - 1) / 2) * kSpacing[2] + self.random:range(-34, 34)
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
    self:status(string.format('%d points, %d reachable from the start, %s', self.graph.size, reachable, self.route and string.format('a route of %d points that costs %.0f', #self.route, self.cost) or 'and the route is blocked'))
end

function Waypoints:draw(area)
    local graph = self.graph
    for _, road in ipairs(self.roads) do
        local a, b = graph:position(road.from), graph:position(road.to)
        graphics2d.drawStroke({a.x, a.y, b.x, b.y}, {width = 16, color = '#FF232A3B', cap = 'round'})
        graphics2d.drawStroke({a.x, a.y, b.x, b.y}, {width = 9, color = '#FF4C5A7C', cap = 'round', layer = 1})
        if road.oneWay then
            local middle, direction = (a + b) / 2, (b - a):normalized()
            local side = direction:perpendicular() * 13
            local tip = middle + direction * 18
            graphics2d.drawPolygon({tip, middle - direction * 9 + side, middle - direction * 9 - side}, '#FFE8EAF2', {layer = 3})
        end
    end
    if self.route then
        local points = {}
        for _, point in ipairs(self.route) do
            local position = graph:position(point)
            points[#points + 1] = position.x
            points[#points + 1] = position.y
        end
        graphics2d.drawStroke(points, {width = 9, color = '#FFFFC93C', join = 'round', cap = 'round', layer = 2})
    end

    -- Points are discs with a rim: closed doors red with a bar across, heavy points large and brown.
    for _, point in ipairs(graph:points()) do
        local position = graph:position(point)
        local closed, heavy = not graph:enabled(point), graph:weight(point) > 1
        local color = closed and '#FFF0473E' or (heavy and '#FFA0643A' or '#FFCDD3E6')
        local radius = heavy and 22 or 14
        graphics2d.drawShape({position.x - radius, position.y - radius, radius * 2, radius * 2}, {kind = 'ellipse', color = color, borderWidth = 3, borderColor = '#FF15172A', layer = 4})
        if closed then
            graphics2d.drawShape({position.x - 9, position.y - 3, 18, 6}, {radius = 3, color = '#FFFFFFFF', layer = 5})
        end
        local cost = self.distances[point]
        if self.showCosts and cost and point ~= self.start and point ~= self.goal then
            self.art:label(string.format('%.0f', cost), position.x, position.y - radius - 20, 20, '#FFE8EAF2', 6)
        end
    end
    local start, goal = graph:position(self.start), graph:position(self.goal)
    self.art:start(start.x, start.y + 6, 50, 10)
    self.art:goal(goal.x, goal.y + 6, 50, 10)
end

return Waypoints
