-- A grid of square cells placed in the world: it converts between cells and world positions, draws a picture of its cells and its lines, and turns pointer drags into strokes of cells.
local graphics2d = require('haylen.graphics2d')
local spatial2d = require('haylen.spatial2d')

local Board = {}
Board.__index = Board

-- The board is centered on `x`, `y`, the world origin by default.
function Board.new(columns, rows, cellSize, x, y)
    local width, height = columns * cellSize, rows * cellSize
    return setmetatable({columns = columns, rows = rows, cellSize = cellSize, left = (x or 0) - width / 2, top = (y or 0) - height / 2, width = width, height = height}, Board)
end

function Board:contains(column, row)
    return column >= 0 and row >= 0 and column < self.columns and row < self.rows
end

-- Returns the cell under a world position, or nil outside the board.
function Board:cellAt(x, y)
    local column, row = (x - self.left) // self.cellSize, (y - self.top) // self.cellSize
    if self:contains(column, row) then
        return math.tointeger(column), math.tointeger(row)
    end
end

function Board:center(column, row)
    return self.left + (column + 0.5) * self.cellSize, self.top + (row + 0.5) * self.cellSize
end

function Board:cellRect(column, row, inset)
    inset = inset or 0
    return {self.left + column * self.cellSize + inset, self.top + row * self.cellSize + inset, self.cellSize - inset * 2, self.cellSize - inset * 2}
end

function Board:bounds()
    return {self.left, self.top, self.width, self.height}
end

-- Draws a texture with one pixel per cell over the whole board.
function Board:drawPicture(texture, order)
    graphics2d.draw(texture, self.left, self.top, {pivotX = 0, pivotY = 0, width = self.width, height = self.height, layer = order and order.layer, depth = order and order.depth})
end

function Board:drawLines(color, order)
    for column = 0, self.columns do
        local x = self.left + column * self.cellSize
        graphics2d.drawLine(x, self.top, x, self.top + self.height, 1, color, order)
    end
    for row = 0, self.rows do
        local y = self.top + row * self.cellSize
        graphics2d.drawLine(self.left, y, self.left + self.width, y, 1, color, order)
    end
end

-- Draws a path given as cells, through the centers of its cells.
function Board:drawPath(path, thickness, color, order)
    if path == nil or #path < 2 then
        return
    end
    local points = {}
    for index, cell in ipairs(path) do
        local x, y = self:center(cell.x or cell[1], cell.y or cell[2])
        points[index] = {x, y}
    end
    graphics2d.drawPolyline(points, thickness, color, false, order)
end

-- Calls `visit(column, row, first)` for every cell a pointer drag crosses, with no gaps however fast it moves. `first` is true for the cell the press started on.
function Board:stroke(pointer, visit)
    if not pointer.down then
        self.last = nil
        return
    end
    local column, row = self:cellAt(pointer.worldX, pointer.worldY)
    if column == nil then
        return
    end
    if self.last == nil or pointer.pressed then
        visit(column, row, true)
    else
        for _, cell in ipairs(spatial2d.line(self.last[1], self.last[2], column, row)) do
            visit(cell.x, cell.y, false)
        end
    end
    self.last = {column, row}
end

return Board
