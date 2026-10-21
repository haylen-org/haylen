-- Hierarchical path finding on a 256 by 256 map next to A* and jump point search, with walls painted in and only the touched clusters rebuilt.
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

local Hpa = haylen.class('Hpa', AlgorithmTest)

Hpa.code = [[
local router = grid:hierarchicalPathfinder({clusterSize = 16})
local path, cost = router:findPath(4, 4, 251, 251)
grid:setWalkable(120, 40, false)
router:update(120, 40)  -- Rebuilds only the cluster of the changed cell.]]

local kSize = 256
local kCell = 3.05
local kClusterSize = 16
local kMapX = -362
-- The paths nest: each search draws thinner over the one before, so a stretch they share shows all three colors and a stretch where they part shows each alone.
local kSearches = {
    {name = 'HPA*', scope = 'hpa', color = '#FFFFC93C', width = 15},
    {name = 'A*', scope = 'aStar', color = '#FF2FB3C4', width = 8},
    {name = 'Jump point search', scope = 'jumpPoint', color = '#FFF06292', width = 3},
}
local kLegend = {x = 80, y = -400, width = 690, height = 800}

function Hpa:enter()
    self:frame{
        hint = 'Tap or click to move the goal, or paint walls with the wall brush. Releasing a stroke rebuilds only the clusters it touched. R or the X button makes a new map.',
        code = Hpa.code,
        controls = {
            ui.radioGroup{id = 'brush', items = {{id = 'goal', text = 'Move the goal'}, {id = 'start', text = 'Move the start'}, {id = 'wall', text = 'Paint walls'}}, selected = 'goal', onChange = function(event) self.brush = event.value end},
            ui.checkbox{id = 'clusters', text = 'Show the clusters', checked = true, onChange = function(event) self.showClusters = event.checked end},
            ui.button{id = 'reset', text = 'New map', onClick = function() self:build() end},
        },
        focus = 'brush',
    }
    self.brush, self.showClusters = 'goal', true
    self.random = m.random(61)
    self.art = Art()
    self.board = Board(kSize, kSize, kCell, kMapX, 0)
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
            local wall = noise:fractal(column / 30, row / 30, 3) > 0.32
            if column % 64 == 32 and not door(row) or row % 64 == 32 and not door(column) then
                wall = true
            end
            self:setWall(column, row, wall)
        end
    end
    self.start, self.goal = self:open(4, 4), self:open(kSize - 5, kSize - 5)
    profiler.beginScope('hpaBuild')
    self.router = self.grid:hierarchicalPathfinder({clusterSize = kClusterSize})
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
        return self.walls[row * kSize + column] and '#FF7C8AAB' or '#FF262D3D'
    end)
end

function Hpa:search()
    local sx, sy, gx, gy = self.start[1], self.start[2], self.goal[1], self.goal[2]
    local results = {}
    profiler.beginScope('hpa')
    results[1] = {self.router:findPath(sx, sy, gx, gy)}
    profiler.endScope()
    profiler.beginScope('aStar')
    results[2] = {self.grid:findPath(sx, sy, gx, gy)}
    profiler.endScope()
    profiler.beginScope('jumpPoint')
    results[3] = {self.grid:findPath(sx, sy, gx, gy, {jumpPoint = true})}
    profiler.endScope()
    self.results = {}
    for index, result in ipairs(results) do
        self.results[index] = {path = result[1], cost = result[2]}
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
        profiler.beginScope('hpaUpdate')
        self.router:update(box[1], box[2], math.min(box[3], kSize - 1), math.min(box[4], kSize - 1))
        profiler.endScope()
        self:repaint()
        self:search()
    end
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
    for index, search in ipairs(kSearches) do
        self.results[index].milliseconds = self:timing(search.scope)
    end
    self:status(string.format('Entrances between clusters %d, built in %.2f ms, last stroke rebuilt in %.2f ms', self.router.nodeCount, self:timing('hpaBuild'), self:timing('hpaUpdate')))
end

function Hpa:draw(area)
    local board = self.board
    graphics2d.drawShape({board.left - 8, board.top - 8, board.width + 16, board.height + 16}, {radius = 12, color = '#FF151A24', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    board:drawPicture(self.picture)
    if self.showClusters then
        for index = kClusterSize, kSize - 1, kClusterSize do
            local offset = index * kCell
            graphics2d.drawLine(board.left + offset, board.top, board.left + offset, board.top + board.height, 1.5, '#38FFFFFF', {layer = 1})
            graphics2d.drawLine(board.left, board.top + offset, board.left + board.width, board.top + offset, 1.5, '#38FFFFFF', {layer = 1})
        end
    end
    for index, search in ipairs(kSearches) do
        board:drawPath(self.results[index].path, search.width, search.color, {layer = 2 + index})
    end
    local sx, sy = board:center(self.start[1], self.start[2])
    local gx, gy = board:center(self.goal[1], self.goal[2])
    self.art:start(sx, sy + 6, 46, 10)
    self.art:goal(gx, gy + 6, 46, 10)
    self:drawLegend()
end

-- The legend sits on a card beside the map: a sample of each line, its name and what its search found.
function Hpa:drawLegend()
    local card = kLegend
    graphics2d.drawShape({card.x, card.y, card.width, card.height}, {radius = 20, color = '#FF1E2433', borderWidth = 2, borderColor = '#FF343D55'})
    local left = card.x + 36
    graphics2d.drawText(nil, 'Three searches, one map', left, card.y + 34, {size = 34})
    graphics2d.drawText(nil, 'Each line draws thinner over the one before, so where the paths agree all three colors nest, and where they part each shows alone.', left, card.y + 84, {size = 23, color = '#FFAAB4CC', maxWidth = card.width - 72, layer = 1})
    for index, search in ipairs(kSearches) do
        local result = self.results[index]
        local y = card.y + 250 + (index - 1) * 150
        graphics2d.drawLine(left, y + 22, left + 64, y + 22, search.width, search.color, {layer = 1})
        graphics2d.drawText(nil, search.name, left + 92, y, {size = 30, layer = 1})
        local summary = result.path and string.format('%d cells, cost %.0f, %.2f ms', #result.path, result.cost, result.milliseconds or 0) or 'No path between the flags.'
        graphics2d.drawText(nil, summary, left + 92, y + 44, {size = 24, color = '#FFAAB4CC', layer = 1})
    end
    graphics2d.drawText(nil, string.format('The map has %d clusters of %d by %d cells.', (kSize // kClusterSize) * (kSize // kClusterSize), kClusterSize, kClusterSize), left, card.y + card.height - 64, {size = 22, color = '#FF7A8099', maxWidth = card.width - 72, layer = 1})
end

return Hpa
