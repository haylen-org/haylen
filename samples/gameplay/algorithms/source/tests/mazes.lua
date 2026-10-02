-- Perfect mazes from the recursive backtracker, Prim and Kruskal, generated on a worker thread, drawn from `maze:toGrid` and solved with A* from corner to corner.
local haylen = require('haylen')
local async = require('async')
local input = require('haylen.input')
local navigation2d = require('haylen.navigation2d')
local procedural2d = require('haylen.procedural2d')
local ui = require('haylen.ui')

local Board = require('board')
local picture = require('picture')
local sample = require('sample')

local Mazes = haylen.class('Mazes', sample.Test)

local kWidth, kHeight = 47, 25
local kColumns, kRows, kCell = kWidth * 2 + 1, kHeight * 2 + 1, 15

function Mazes:enter()
    Mazes.super.enter(self, {
        hint = 'The backtracker makes long winding corridors, while Prim and Kruskal make many short dead ends. Exactly one path joins the two corners.',
        controls = {
            ui.radioGroup{id = 'algorithm', items = {{id = 'backtracker', text = 'Recursive backtracker'}, {id = 'prim', text = 'Prim'}, {id = 'kruskal', text = 'Kruskal'}}, selected = 'backtracker', onChange = function(event)
                self.algorithm = event.value
                self:generate()
            end},
            ui.checkbox{id = 'solve', text = 'Show the solution', checked = true, onChange = function(event) self.solve = event.checked end},
            ui.button{id = 'seed', text = 'New seed', onClick = function()
                self.seed = self.seed + 1
                self:generate()
            end},
        },
        stats = true,
        focus = 'algorithm',
    })
    self.algorithm, self.solve, self.seed = 'backtracker', true, 1
    self.board = Board.new(kColumns, kRows, kCell)
    self:generate()
end

function Mazes:generate()
    self.request = (self.request or 0) + 1
    local request, started = self.request, haylen.elapsed()
    local options = {width = kWidth, height = kHeight, algorithm = self.algorithm, seed = self.seed}
    async.spawn(function()
        local maze = procedural2d.mazeAsync(options):await()
        if maze == nil or self.board == nil or request ~= self.request then
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

function Mazes:exit()
    Mazes.super.exit(self)
    self.board, self.picture, self.path = nil, nil, nil
end

function Mazes:update(dt)
    Mazes.super.update(self, dt)
    if input.pressed('reset') then
        self.seed = self.seed + 1
        self:generate()
    end
    self:showStats(string.format('Cells %d x %d\nPassages %d\nDead ends %d\nSolution %d tiles\nReady in %.0f ms', kWidth, kHeight, self.passages or 0, self.deadEnds or 0, self.path and #self.path or 0, self.latency or 0))
end

function Mazes:render()
    self:beginWorld()
    if self.picture == nil then
        return
    end
    self.board:drawPicture(self.picture)
    if self.solve then
        self.board:drawPath(self.path, kCell * 0.5, '#FFE53935', {layer = 1})
    end
end

return Mazes
