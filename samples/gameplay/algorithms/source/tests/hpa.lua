-- Hierarchical path finding on a 256 by 256 map next to A* and jump point search, with walls painted in and only the touched clusters rebuilt.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local Board = require('board')
local picture = require('picture')
local sample = require('sample')

local Hpa = haylen.class('Hpa', sample.Test)

local kSize = 256
local kCell = 3.3
local kClusterSize = 16
local kSearches = {
    {name = 'HPA*', scope = 'hpa', color = '#FFFFD54F', width = 5},
    {name = 'A*', scope = 'a-star', color = '#FF4DD0E1', width = 2},
    {name = 'Jump point search', scope = 'jump-point', color = '#FFF06292', width = 2},
}

function Hpa:enter()
    Hpa.super.enter(self, {
        hint = 'Tap or click to move the goal, or paint walls with the wall brush. Releasing a stroke rebuilds only the clusters it touched.',
        controls = {
            ui.radioGroup{id = 'brush', items = {{id = 'goal', text = 'Move the goal'}, {id = 'start', text = 'Move the start'}, {id = 'wall', text = 'Paint walls'}}, selected = 'goal', onChange = function(event) self.brush = event.value end},
            ui.checkbox{id = 'clusters', text = 'Show the clusters', checked = true, onChange = function(event) self.showClusters = event.checked end},
            ui.button{id = 'reset', text = 'New map', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'brush',
    })
    self.brush, self.showClusters = 'goal', true
    self.random = m.random(61)
    self.board = Board.new(kSize, kSize, kCell, -250, 0)
    self:build()
end

-- Two doors in every stretch of a long wall.
local function door(along)
    local offset = along % 64
    return offset >= 12 and offset < 18 or offset >= 44 and offset < 50
end

-- Rooms of noise walled off in blocks with doors, so paths wind across the whole map.
function Hpa:build()
    self.grid = navigation2d.newGrid(kSize, kSize)
    self.walls = {}
    local noise = m.noise(self.random:integer(1, 9999))
    for row = 0, kSize - 1 do
        for column = 0, kSize - 1 do
            local wall = noise:fractal(column / 24, row / 24, 3) > 0.35
            if column % 64 == 32 and not door(row) or row % 64 == 32 and not door(column) then
                wall = true
            end
            self:setWall(column, row, wall)
        end
    end
    self.start, self.goal = self:open(4, 4), self:open(kSize - 5, kSize - 5)
    profiler.beginScope('hpa build')
    self.router = self.grid:hierarchical({clusterSize = kClusterSize})
    profiler.endScope()
    self:repaint()
    self:search()
end

function Hpa:setWall(column, row, wall)
    self.walls[row * kSize + column] = wall or nil
    self.grid:setWalkable(column, row, not wall)
end

-- Returns the nearest walkable cell along the diagonal from a corner.
function Hpa:open(column, row)
    while self.walls[row * kSize + column] do
        column, row = column + (column < kSize / 2 and 1 or -1), row + (row < kSize / 2 and 1 or -1)
    end
    return {column, row}
end

function Hpa:repaint()
    self.picture = picture.cells(kSize, kSize, function(column, row)
        return self.walls[row * kSize + column] and '#FF607D8B' or '#FF1F2530'
    end)
end

function Hpa:search()
    local sx, sy, gx, gy = self.start[1], self.start[2], self.goal[1], self.goal[2]
    local results = {}
    profiler.beginScope('hpa')
    results[1] = {self.router:findPath(sx, sy, gx, gy)}
    profiler.endScope()
    profiler.beginScope('a-star')
    results[2] = {self.grid:findPath(sx, sy, gx, gy)}
    profiler.endScope()
    profiler.beginScope('jump-point')
    results[3] = {self.grid:findPath(sx, sy, gx, gy, {jumpPoint = true})}
    profiler.endScope()
    for index, search in ipairs(kSearches) do
        search.path, search.cost = results[index][1], results[index][2]
    end
end

-- A stroke paints walls at once, and its release rebuilds the clusters around the cells it changed.
function Hpa:paint()
    local pointer = self.pointer
    self.board:stroke(pointer, function(column, row)
        if column == self.start[1] and row == self.start[2] or column == self.goal[1] and row == self.goal[2] then
            return
        end
        for dy = 0, 1 do
            for dx = 0, 1 do
                if self.board:contains(column + dx, row + dy) then
                    self:setWall(column + dx, row + dy, true)
                end
            end
        end
        local box = self.stroke or {column, row, column + 1, row + 1}
        self.stroke = {math.min(box[1], column), math.min(box[2], row), math.max(box[3], column + 1), math.max(box[4], row + 1)}
    end)
    if pointer.released and self.stroke then
        local box = self.stroke
        self.stroke = nil
        profiler.beginScope('hpa update')
        self.router:update(box[1], box[2], math.min(box[3], kSize - 1), math.min(box[4], kSize - 1))
        profiler.endScope()
        self:repaint()
        self:search()
    end
end

function Hpa:exit()
    Hpa.super.exit(self)
    self.grid, self.router, self.walls, self.picture = nil, nil, nil, nil
end

function Hpa:update(dt)
    Hpa.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.brush == 'wall' then
        self:paint()
    elseif self.pointer.pressed then
        local column, row = self.board:cellAt(self.pointer.worldX, self.pointer.worldY)
        if column and not self.walls[row * kSize + column] then
            self[self.brush] = {column, row}
            self:search()
        end
    end
    local lines = {string.format('entrances %d', self.router.nodeCount), string.format('build %.2f ms', self:timing('hpa build')), string.format('update %.2f ms', self:timing('hpa update'))}
    for _, search in ipairs(kSearches) do
        lines[#lines + 1] = string.format('%s %.2f ms', search.name, self:timing(search.scope))
        lines[#lines + 1] = search.path and string.format('  %d cells, cost %.0f', #search.path, search.cost) or '  no path'
    end
    self:showStats(table.concat(lines, '\n'))
end

function Hpa:render()
    self:beginWorld()
    local board = self.board
    board:drawPicture(self.picture)
    if self.showClusters then
        for index = 0, kSize, kClusterSize do
            local offset = index * kCell
            graphics2d.drawLine(board.left + offset, board.top, board.left + offset, board.top + board.height, 1, '#40FFFFFF', {layer = 1})
            graphics2d.drawLine(board.left, board.top + offset, board.left + board.width, board.top + offset, 1, '#40FFFFFF', {layer = 1})
        end
    end
    for index = #kSearches, 1, -1 do
        local search = kSearches[index]
        board:drawPath(search.path, search.width, search.color, {layer = 2 + index})
    end
    for _, cell in ipairs({self.start, self.goal}) do
        local x, y = board:center(cell[1], cell[2])
        graphics2d.drawCircle(x, y, 10, cell == self.start and '#FF66BB6A' or '#FFEF5350', {layer = 6})
    end
    local legendX = board.left + board.width + 40
    for index, search in ipairs(kSearches) do
        local y = board.top + (index - 1) * 40
        graphics2d.drawLine(legendX, y + 14, legendX + 40, y + 14, search.width + 2, search.color)
        graphics2d.drawText(nil, search.name, legendX + 56, y, {size = 26})
    end
end

return Hpa
