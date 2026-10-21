-- Sight on a grid and in the open: symmetric shadowcasting with fog of war, a Bresenham line to a guard and a Bresenham circle of the sight radius, and a visibility polygon among wall segments.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')
local Board = require('categories.algorithms.board')

local FieldOfView = haylen.class('FieldOfView', AlgorithmTest)

FieldOfView.code = [[
local map = spatial2d.newCellGrid(56, 30)
map:set(14, 6, 1)  -- Solid cells block sight.
local visible = spatial2d.fieldOfView(map, 2, 2, 10)
local sight, ring = spatial2d.line(2, 2, 31, 17), spatial2d.circle(2, 2, 10)
local polygon = spatial2d.visibilityPolygon({120, 40}, {{{300, 200}, {300, 400}}}, {-740, -400, 1480, 800})]]

local kColumns, kRows, kCell = 56, 30, 26
local kBounds = {-740, -400, 1480, 800}

function FieldOfView:enter()
    self:frame{
        hint = 'Move the pointer to look around. The grid remembers the cells it saw, and the guard sees you exactly when you see it. The yellow cells are the Bresenham circle of the sight radius and the red ones the Bresenham line to the guard. R or the X button builds a new map.',
        code = FieldOfView.code,
        controls = {
            ui.formField{label = 'Sight', ui.radioGroup{id = 'mode', items = {{id = 'grid', text = 'Shadowcasting on a grid'}, {id = 'polygon', text = 'Visibility polygon'}}, selected = 'grid', onChange = function(event) self.mode = event.value end}},
            ui.formField{label = 'Sight radius in cells', ui.slider{id = 'radius', value = 10, min = 2, max = 30, step = 1, showValue = true, decimals = 0, onChange = function(event) self.radius = event.value end}},
            ui.button{id = 'forget', text = 'Forget what was seen', onClick = function() self.seen = {} end},
            ui.button{id = 'reset', text = 'New map', onClick = function() self:build() end},
        },
        focus = 'mode',
    }
    self.mode, self.radius = 'grid', 10
    self.random = m.random(79)
    self.board = Board(kColumns, kRows, kCell)
    self.art = Art()
    self:build()
end

-- Rooms of walls with doorways and scattered pillars, and the same rooms as segments for the visibility polygon.
function FieldOfView:build()
    self.grid = spatial2d.newCellGrid(kColumns, kRows)
    self.segments = {}
    for column = 0, kColumns - 1, 14 do
        for row = 0, kRows - 1 do
            if row % 10 ~= 4 and row % 10 ~= 5 then
                self.grid:set(column, row, 1)
            end
        end
    end
    for row = 0, kRows - 1, 10 do
        for column = 0, kColumns - 1 do
            if column % 14 ~= 7 then
                self.grid:set(column, row, 1)
            end
        end
    end
    for _ = 1, 60 do
        self.grid:set(self.random:integer(1, kColumns - 2), self.random:integer(1, kRows - 2), 1)
    end
    for _ = 1, 22 do
        local x, y = self.random:range(-700, 600), self.random:range(-380, 300)
        local width, height = self.random:range(20, 160), self.random:range(20, 120)
        local corners = {{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}}
        for index = 1, 4 do
            self.segments[#self.segments + 1] = {corners[index], corners[index % 4 + 1]}
        end
    end
    self.guard = {kColumns // 2 + 3, kRows // 2 + 2}
    self.grid:set(self.guard[1], self.guard[2], 0)
    self.seen, self.visibleSet, self.sightLine, self.ring = {}, {}, {}, {}
    self.viewer = {2, 2}
end

function FieldOfView:update(dt)
    FieldOfView.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.mode == 'grid' then
        local column, row = self.board:cellAt(self:aim(-120, 40))
        if column and self.grid:get(column, row) == 0 then
            self.viewer = {column, row}
        end
        profiler.beginScope('fieldOfView')
        self.visible = spatial2d.fieldOfView(self.grid, self.viewer[1], self.viewer[2], self.radius)
        profiler.endScope()
        self.visibleSet = {}
        for _, cell in ipairs(self.visible) do
            local key = cell.y * kColumns + cell.x
            self.visibleSet[key] = true
            self.seen[key] = true
        end
        self.sightLine = spatial2d.line(self.viewer[1], self.viewer[2], self.guard[1], self.guard[2])
        self.ring = spatial2d.circle(self.viewer[1], self.viewer[2], self.radius)
        local spotted = self.visibleSet[self.guard[2] * kColumns + self.guard[1]]
        self:status(string.format('%d cells in sight, shadowcast in %.3f ms, and the guard %s', #self.visible, self:timing('fieldOfView'), spotted and 'sees you' or 'does not see you'))
    else
        local x, y = self:aim(-200, -60)
        self.eye = {x, y}
        local bounds = m.rect(kBounds)
        if bounds:contains({x, y}) then
            profiler.beginScope('visibilityPolygon')
            self.polygon = spatial2d.visibilityPolygon({x, y}, self.segments, kBounds)
            profiler.endScope()
        end
        self:status(string.format('%d walls, a polygon of %d corners in %.3f ms', #self.segments, self.polygon and #self.polygon or 0, self:timing('visibilityPolygon')))
    end
end

function FieldOfView:renderGrid()
    local board = self.board
    graphics2d.drawShape({board.left - 6, board.top - 6, board.width + 12, board.height + 12}, {radius = 10, color = '#FF0E1118', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local key = row * kColumns + column
            local wall = self.grid:get(column, row) == 1
            local color
            -- Cells in sight are lit, cells seen before keep a dim memory, and the rest of the map shows only faintly.
            if self.visibleSet[key] then
                color = wall and '#FFB4BED2' or '#FF56704C'
            elseif self.seen[key] then
                color = wall and '#FF465066' or '#FF262E3D'
            else
                color = wall and '#FF232A39' or '#FF141923'
            end
            graphics2d.drawRect(board:cellRect(column, row), color)
        end
    end
    for _, cell in ipairs(self.ring) do
        if board:contains(cell.x, cell.y) then
            graphics2d.drawShape(board:cellRect(cell.x, cell.y, 3), {radius = 4, color = '#70FFC93C', layer = 1})
        end
    end
    for _, cell in ipairs(self.sightLine) do
        graphics2d.drawShape(board:cellRect(cell.x, cell.y, 6), {radius = 4, color = '#B0F0473E', layer = 2})
    end
    local vx, vy = board:center(self.viewer[1], self.viewer[2])
    local gx, gy = board:center(self.guard[1], self.guard[2])
    Art.shadow(vx, vy + kCell * 0.4, kCell, 3)
    self.art:character(vx, vy + kCell * 0.45, kCell * 2, 'blue', 4)
    Art.shadow(gx, gy + kCell * 0.4, kCell, 3)
    self.art:character(gx, gy + kCell * 0.45, kCell * 2, 'red', 4, true)
end

function FieldOfView:renderPolygon()
    graphics2d.drawShape(kBounds, {radius = 10, color = '#FF141920', borderWidth = 3, borderColor = '#FF3A4560'})
    if self.polygon then
        graphics2d.drawPolygon(self.polygon, '#55FFE082', {layer = 1})
    end
    for _, segment in ipairs(self.segments) do
        graphics2d.drawStroke({segment[1][1], segment[1][2], segment[2][1], segment[2][2]}, {width = 6, color = '#FFB4BED2', cap = 'round', layer = 2})
    end
    if self.eye then
        Art.shadow(self.eye[1], self.eye[2] + 12, 30, 3)
        self.art:character(self.eye[1], self.eye[2] + 14, 56, 'blue', 4)
    end
end

function FieldOfView:draw(area)
    if self.mode == 'grid' then
        self:renderGrid()
    else
        self:renderPolygon()
    end
end

return FieldOfView
