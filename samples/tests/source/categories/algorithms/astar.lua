-- A*, weighted A* and jump point search on the same grid, side by side, with walls, mud and both ends painted by the player.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Board = require('categories.algorithms.board')
local picture = require('categories.algorithms.picture')

local AStar = haylen.class('AStar', AlgorithmTest)

local kColumns, kRows, kCell = 30, 36, 14
local kMudCost = 5
local kColors = {floor = '#FF2B3240', wall = '#FF90A4AE', mud = '#FF6D4C41'}
local kPanels = {
    {name = 'A*', scope = 'a-star', x = -540},
    {name = 'Weighted A*', scope = 'weighted', x = 0},
    {name = 'Jump point search', scope = 'jump-point', x = 540},
}

function AStar:enter()
    self:frame{
        hint = 'Paint on any of the three grids with the selected brush, and R or the X button paints new random walls. Every search runs again at once and shows its path, cost and time.',
        controls = {
            ui.radioGroup{id = 'brush', items = {{id = 'wall', text = 'Paint walls'}, {id = 'mud', text = 'Paint mud that costs 5'}, {id = 'floor', text = 'Erase'}, {id = 'start', text = 'Move the start'}, {id = 'goal', text = 'Move the goal'}}, selected = 'wall', onChange = function(event) self.brush = event.value end},
            ui.combo{id = 'heuristic', items = {{id = 'octile', text = 'Octile'}, {id = 'manhattan', text = 'Manhattan'}, {id = 'euclidean', text = 'Euclidean'}, {id = 'chebyshev', text = 'Chebyshev'}}, selected = 'octile', onChange = function(event) self:setOption('heuristic', event.value) end},
            ui.label{text = 'Weight of weighted A*', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'weight', value = 2, min = 1, max = 5, step = 0.25, showValue = true, onChange = function(event) self:setOption('weight', event.value) end},
            ui.checkbox{id = 'diagonal', text = 'Diagonal steps', checked = true, onChange = function(event) self:setOption('diagonal', event.checked) end},
            ui.row{ui.button{id = 'random', text = 'Random walls', onClick = function() self:randomize() end}, ui.button{id = 'clear', text = 'Clear', onClick = function() self:clear() end}},
        },
        focus = 'brush',
    }
    self.brush = 'wall'
    self.options = {heuristic = 'octile', weight = 2, diagonal = true}
    self.random = m.random(41)
    self.grid = navigation2d.newGrid(kColumns, kRows)
    self.cells = {}
    self.start, self.goal = {2, 2}, {kColumns - 3, kRows - 3}
    self.panels = {}
    for index, panel in ipairs(kPanels) do
        self.panels[index] = {name = panel.name, scope = panel.scope, x = panel.x, board = Board(kColumns, kRows, kCell, panel.x, 20)}
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
    profiler.beginScope('a-star')
    results[1] = {self.grid:findPath(sx, sy, gx, gy, {heuristic = options.heuristic, diagonal = options.diagonal})}
    profiler.endScope()
    profiler.beginScope('weighted')
    results[2] = {self.grid:findPath(sx, sy, gx, gy, {heuristic = options.heuristic, diagonal = options.diagonal, weight = options.weight})}
    profiler.endScope()
    if self.grid.uniformCost then
        profiler.beginScope('jump-point')
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
        board:drawPicture(self.picture)
        board:drawPath(panel.path, 5, '#FFFFD54F', {layer = 2})
        for _, cell in ipairs(panel.path or {}) do
            local x, y = board:center(cell.x, cell.y)
            graphics2d.drawCircle(x, y, 3, '#FFFFF59D', {layer = 3})
        end
        local sx, sy = board:center(self.start[1], self.start[2])
        local gx, gy = board:center(self.goal[1], self.goal[2])
        graphics2d.drawCircle(sx, sy, kCell * 0.6, '#FF66BB6A', {layer = 4})
        graphics2d.drawCircle(gx, gy, kCell * 0.6, '#FFEF5350', {layer = 4})
        graphics2d.drawText(nil, panel.name, panel.x, board.top - 16, {size = 30, anchor = {0.5, 1}})
        local summary = panel.note or (panel.path and string.format('%d cells, cost %.1f\nTime %.3f ms', #panel.path, panel.cost, panel.milliseconds) or 'No path')
        graphics2d.drawText(nil, summary, panel.x, board.top + board.height + 12, {size = 22, anchor = {0.5, 0}, align = 'center', maxWidth = board.width})
    end
end

return AStar
