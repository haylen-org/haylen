-- Perfect mazes from the recursive backtracker, Prim and Kruskal, generated on a worker thread, drawn from `maze:toGrid` and solved with A* from the pin in one corner to the flag in the other, a line that walks the solution again and again.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local navigation2d = require('haylen.navigation2d')
local procedural2d = require('haylen.procedural2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')
local Board = require('categories.algorithms.board')
local code = require('harness.code')
local picture = require('categories.algorithms.picture')

local Mazes = haylen.class('Mazes', AlgorithmTest)

Mazes.code = [[
local maze = procedural2d.mazeAsync({width = 47, height = 25, algorithm = '%s', seed = 1}):await()
local tiles = maze:toGrid()  -- Cell x, y sits on tile 2x + 1, 2y + 1.
local deadEnd = maze:openings(3, 4) == procedural2d.north  -- It opens to the north alone.
local path = grid:findPath(1, 1, 93, 49, {diagonal = false})  -- A grid walkable where the tiles are 0.]]

local kWidth, kHeight = 47, 25
local kColumns, kRows, kCell = kWidth * 2 + 1, kHeight * 2 + 1, 15
-- The solution walks this many tiles a second and rests at the flag before it walks again.
local kWalkSpeed = 240
local kRest = 1.5

function Mazes:enter()
    self:frame{
        hint = 'The backtracker makes long winding corridors, while Prim and Kruskal make many short dead ends. Exactly one path joins the pin and the flag, and the gold line walks it. R or the X button takes a new seed.',
        code = string.format(Mazes.code, 'backtracker'),
        controls = {
            ui.radioGroup{id = 'algorithm', items = {{id = 'backtracker', text = 'Recursive backtracker'}, {id = 'prim', text = 'Prim'}, {id = 'kruskal', text = 'Kruskal'}}, selected = 'backtracker', onChange = function(event)
                self.algorithm = event.value
                self.codeSource = string.format(Mazes.code, event.value)
                self:set('code', {text = code.markup(self.codeSource)})
                self:generate()
            end},
            ui.checkbox{id = 'solve', text = 'Show the solution', checked = true, onChange = function(event) self.solve = event.checked end},
            ui.button{id = 'seed', text = 'New seed', onClick = function()
                self.seed = self.seed + 1
                self:generate()
            end},
        },
        focus = 'algorithm',
    }
    self.algorithm, self.solve, self.seed = 'backtracker', true, 1
    self.board = Board(kColumns, kRows, kCell)
    self.art = Art()
    self.trace = {}
    self:generate()
end

function Mazes:generate()
    self.request = (self.request or 0) + 1
    local request, started = self.request, haylen.elapsed()
    local options = {width = kWidth, height = kHeight, algorithm = self.algorithm, seed = self.seed}
    self:spawn(function()
        local maze = procedural2d.mazeAsync(options):await()
        if maze == nil or request ~= self.request then
            return
        end
        self.latency = (haylen.elapsed() - started) * 1000
        self:show(maze)
    end)
end

-- Draws the tiles of the maze and walks it with A* on a navigation grid of the same tiles.
function Mazes:show(maze)
    local tiles = maze:toGrid()
    local grid = navigation2d.newGrid(kColumns, kRows)
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            grid:setWalkable(column, row, tiles:get(column, row) == 0)
        end
    end
    self.path = grid:findPath(1, 1, kColumns - 2, kRows - 2, {diagonal = false})
    self.walked = 0
    self.picture = picture.cells(kColumns, kRows, function(column, row)
        return tiles:get(column, row) == 1 and '#FF263238' or '#FFD7CCC8'
    end)
    self.deadEnds = 0
    for y = 0, kHeight - 1 do
        for x = 0, kWidth - 1 do
            local openings = maze:openings(x, y)
            if openings == procedural2d.north or openings == procedural2d.east or openings == procedural2d.south or openings == procedural2d.west then
                self.deadEnds = self.deadEnds + 1
            end
        end
    end
    self.passages = maze.passageCount
end

function Mazes:update(dt)
    Mazes.super.update(self, dt)
    if input.pressed('reset') then
        self.seed = self.seed + 1
        self:generate()
    end
    self:walk(dt)
    self:status(string.format('The maze of %d by %d cells has %d passages and %d dead ends, and its solution of %d tiles was ready in %.0f ms.', kWidth, kHeight, self.passages or 0, self.deadEnds or 0, self.path and #self.path or 0, self.latency or 0))
end

-- Moves the head of the solution along the path, keeping the points it walked in one list that only grows until it starts again.
function Mazes:walk(dt)
    local path, trace = self.path, self.trace
    if path == nil then
        return
    end
    self.walked = self.walked + dt * kWalkSpeed
    if self.walked > #path + kRest * kWalkSpeed then
        self.walked = 0
    end
    local count = math.min(#path, math.floor(self.walked) + 1)
    if #trace > count * 2 then
        for index = #trace, 1, -1 do
            trace[index] = nil
        end
    end
    for index = #trace // 2 + 1, count do
        local cell = path[index]
        trace[index * 2 - 1], trace[index * 2] = self.board:center(cell.x or cell[1], cell.y or cell[2])
    end
end

function Mazes:draw(area)
    if self.picture == nil then
        return
    end
    local board = self.board
    graphics2d.drawShape({board.left - 14, board.top - 14, board.width + 28, board.height + 28}, {radius = 12, color = '#FF10131B', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    board:drawPicture(self.picture)
    local trace = self.trace
    if self.solve and #trace >= 4 then
        graphics2d.drawStroke(trace, {width = kCell * 0.42, color = '#FFFFC93C', join = 'round', cap = 'round', layer = 1})
        graphics2d.drawCircle(trace[#trace - 1], trace[#trace], kCell * 0.5, '#FFFFF2B0', {layer = 2})
    end
    local sx, sy = board:center(1, 1)
    local gx, gy = board:center(kColumns - 2, kRows - 2)
    self.art:start(sx, sy + kCell * 0.4, 40, 10)
    self.art:goal(gx, gy + kCell * 0.4, 40, 10)
end

return Mazes
