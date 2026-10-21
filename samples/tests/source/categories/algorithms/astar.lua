-- A*, weighted A* and jump point search on the same grid, side by side, with walls, mud and both ends painted by the player.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')
local Board = require('categories.algorithms.board')
local picture = require('categories.algorithms.picture')

local AStar = haylen.class('AStar', AlgorithmTest)

AStar.code = [[
local grid = navigation2d.newGrid(30, 36)
grid:setWalkable(4, 7, false)
grid:setCost(5, 7, 5)  -- Mud that costs 5.
local path, cost = grid:findPath(2, 2, 27, 33, {heuristic = 'octile', diagonal = true})
local quick = grid:findPath(2, 2, 27, 33, {weight = 2})  -- Weighted A*.
local jumped = grid:findPath(2, 2, 27, 33, {jumpPoint = true})  -- Needs grid.uniformCost.]]

local kColumns, kRows, kCell = 30, 36, 16
local kMudCost = 5
local kColors = {floor = '#FF2B3240', wall = '#FF90A4AE', mud = '#FF6D4C41'}
local kPanels = {
    {name = 'A*', scope = 'aStar', x = -540},
    {name = 'Weighted A*', scope = 'weighted', x = 0},
    {name = 'Jump point search', scope = 'jumpPoint', x = 540},
}

function AStar:enter()
    self:frame{
        hint = 'Paint on any of the three grids with the selected brush, and R or the X button paints new random walls. Every search runs again at once and shows its path, cost and time.',
        code = AStar.code,
        controls = {
            ui.radioGroup{id = 'brush', items = {{id = 'wall', text = 'Paint walls'}, {id = 'mud', text = 'Paint mud that costs 5'}, {id = 'floor', text = 'Erase'}, {id = 'start', text = 'Move the start'}, {id = 'goal', text = 'Move the goal'}}, selected = 'wall', onChange = function(event) self.brush = event.value end},
            ui.formField{label = 'Heuristic', ui.combo{id = 'heuristic', items = {{id = 'octile', text = 'Octile'}, {id = 'manhattan', text = 'Manhattan'}, {id = 'euclidean', text = 'Euclidean'}, {id = 'chebyshev', text = 'Chebyshev'}}, selected = 'octile', onChange = function(event) self:setOption('heuristic', event.value) end}},
            ui.formField{label = 'Weight of weighted A*', ui.slider{id = 'weight', value = 2, min = 1, max = 5, step = 0.25, showValue = true, onChange = function(event) self:setOption('weight', event.value) end}},
            ui.checkbox{id = 'diagonal', text = 'Diagonal steps', checked = true, onChange = function(event) self:setOption('diagonal', event.checked) end},
            ui.row{gap = 12, ui.button{id = 'random', text = 'Random walls', grow = 1, onClick = function() self:randomize() end}, ui.button{id = 'clear', text = 'Clear', grow = 1, onClick = function() self:clear() end}},
        },
        focus = 'brush',
    }
    self.brush = 'wall'
    self.options = {heuristic = 'octile', weight = 2, diagonal = true}
    self.random = m.random(41)
    self.art = Art()
    self.grid = navigation2d.newGrid(kColumns, kRows)
    self.cells = {}
    self.start, self.goal = {2, 2}, {kColumns - 3, kRows - 3}
    self.panels = {}
    for index, panel in ipairs(kPanels) do
        self.panels[index] = {name = panel.name, scope = panel.scope, x = panel.x, board = Board(kColumns, kRows, kCell, panel.x, 0)}
    end
    self:randomize()
end

function AStar:setOption(name, value)
    self.options[name] = value
    self.dirty = true
end

function AStar:paint(column, row, kind)
    if (column == self.start[1] and row == self.start[2]) or (column == self.goal[1] and row == self.goal[2]) then
        return
    end
    self.cells[row * kColumns + column] = kind
    self.grid:setWalkable(column, row, kind ~= 'wall')
    self.grid:setCost(column, row, kind == 'mud' and kMudCost or 1)
    self.dirty = true
end

function AStar:randomize()
    self:clear()
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            if self.random:chance(0.28) then
                self:paint(column, row, 'wall')
            end
        end
    end
end

function AStar:clear()
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            self:paint(column, row, 'floor')
        end
    end
end

-- Runs the three searches in their own profiler scopes, so each panel shows its own time.
function AStar:search()
    local sx, sy, gx, gy = self.start[1], self.start[2], self.goal[1], self.goal[2]
    local options = self.options
    local results = {}
    profiler.beginScope('aStar')
    results[1] = {self.grid:findPath(sx, sy, gx, gy, {heuristic = options.heuristic, diagonal = options.diagonal})}
    profiler.endScope()
    profiler.beginScope('weighted')
    results[2] = {self.grid:findPath(sx, sy, gx, gy, {heuristic = options.heuristic, diagonal = options.diagonal, weight = options.weight})}
    profiler.endScope()
    if self.grid.uniformCost then
        profiler.beginScope('jumpPoint')
        results[3] = {self.grid:findPath(sx, sy, gx, gy, {diagonal = options.diagonal, jumpPoint = true})}
        profiler.endScope()
    else
        results[3] = {nil, nil, 'Jump point search needs cells that all cost 1, so erase the mud.'}
    end
    for index, panel in ipairs(self.panels) do
        panel.path, panel.cost, panel.note = results[index][1], results[index][2], results[index][3]
    end
    self.picture = picture.cells(kColumns, kRows, function(column, row)
        return kColors[self.cells[row * kColumns + column] or 'floor']
    end)
end

function AStar:update(dt)
    AStar.super.update(self, dt)
    if input.pressed('reset') then
        self:randomize()
    end
    for _, panel in ipairs(self.panels) do
        panel.board:stroke(self.pointer, function(column, row)
            if self.brush == 'start' or self.brush == 'goal' then
                self:paint(column, row, 'floor')
                self[self.brush] = {column, row}
                self.dirty = true
            else
                self:paint(column, row, self.brush)
            end
        end)
    end
    if self.dirty then
        self.dirty = false
        self:search()
    end
    local times = {}
    for _, panel in ipairs(self.panels) do
        panel.milliseconds = self:timing(panel.scope)
        times[#times + 1] = string.format('%s %.3f ms', panel.name, panel.path and panel.milliseconds or 0)
    end
    self:status(table.concat(times, ', '))
end

function AStar:draw(area)
    for _, panel in ipairs(self.panels) do
        local board = panel.board
        graphics2d.drawShape({board.left - 5, board.top - 5, board.width + 10, board.height + 10}, {radius = 8, color = '#FF151A24', borderWidth = 2, borderColor = '#FF3A4560', layer = -1})
        board:drawPicture(self.picture)
        board:drawPath(panel.path, 6, '#FFFFC93C', {layer = 2})
        local sx, sy = board:center(self.start[1], self.start[2])
        local gx, gy = board:center(self.goal[1], self.goal[2])
        self.art:start(sx, sy + kCell * 0.4, 34, 10)
        self.art:goal(gx, gy + kCell * 0.4, 34, 10)
        graphics2d.drawText(nil, panel.name, panel.x, board.top - 18, {size = 32, anchor = {0.5, 1}})
        local summary = panel.note or (panel.path and string.format('%d cells, cost %.1f, %.3f ms', #panel.path, panel.cost, panel.milliseconds) or 'No path between the flags')
        graphics2d.drawText(nil, summary, panel.x, board.top + board.height + 16, {size = 24, color = '#FFAAB4CC', anchor = {0.5, 0}, align = 'center', maxWidth = board.width + 40})
    end
end

return AStar
