-- Paths on the grid layouts of Tiled: hexagonal maps that shift rows or columns, isometric maps and staggered isometric maps, each with the topology navigation2d reads.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local ui = require('haylen.ui')

local sample = require('sample')

local HexIso = haylen.class('HexIso', sample.Test)

local kRadius = 26
local kSqrt3 = math.sqrt(3)
local kTile = 72
local kPickDistance = 40

-- Each layout gives the grid size, its topology and where the center and the corners of a cell are, with odd rows or columns shifted like Tiled's default stagger index.
local kLayouts = {
    hexRows = {columns = 18, rows = 14, layout = {topology = 'hexagonal'}, center = function(column, row)
        return column * kSqrt3 * kRadius + (row % 2) * kSqrt3 * kRadius / 2, row * 1.5 * kRadius
    end, corner = function(index) return math.cos(math.rad(60 * index - 90)) * kRadius, math.sin(math.rad(60 * index - 90)) * kRadius end},
    hexColumns = {columns = 22, rows = 12, layout = {topology = 'hexagonal', staggerX = true}, center = function(column, row)
        return column * 1.5 * kRadius, row * kSqrt3 * kRadius + (column % 2) * kSqrt3 * kRadius / 2
    end, corner = function(index) return math.cos(math.rad(60 * index)) * kRadius, math.sin(math.rad(60 * index)) * kRadius end},
    isometric = {columns = 14, rows = 14, layout = {topology = 'square'}, center = function(column, row)
        return (column - row) * kTile / 2, (column + row) * kTile / 4
    end, corner = function(index) return ({0, kTile / 2, 0, -kTile / 2})[index + 1], ({-kTile / 4, 0, kTile / 4, 0})[index + 1] end, corners = 4},
    staggered = {columns = 12, rows = 26, layout = {topology = 'staggered'}, center = function(column, row)
        return column * kTile + (row % 2) * kTile / 2, row * kTile / 4
    end, corner = function(index) return ({0, kTile / 2, 0, -kTile / 2})[index + 1], ({-kTile / 4, 0, kTile / 4, 0})[index + 1] end, corners = 4},
}

function HexIso:enter()
    HexIso.super.enter(self, {
        hint = 'Tap or click cells with the selected brush. The path steps through six sides on hexagons and through sides and corners on diamonds.',
        controls = {
            ui.radioGroup{id = 'layout', items = {{id = 'hexRows', text = 'Hex, shifted rows'}, {id = 'hexColumns', text = 'Hex, shifted columns'}, {id = 'isometric', text = 'Isometric'}, {id = 'staggered', text = 'Staggered isometric'}}, selected = 'hexRows', onChange = function(event) self:build(event.value) end},
            ui.radioGroup{id = 'brush', items = {{id = 'wall', text = 'Toggle walls'}, {id = 'start', text = 'Move the start'}, {id = 'goal', text = 'Move the goal'}}, selected = 'wall', onChange = function(event) self.brush = event.value end},
            ui.button{id = 'random', text = 'Random walls', onClick = function() self:build(self.name) end},
        },
        stats = true,
        focus = 'layout',
    })
    self.brush = 'wall'
    self.random = m.random(43)
    self:build('hexRows')
end

function HexIso:build(name)
    self.name, self.shape = name, kLayouts[name]
    local shape = self.shape
    self.grid = navigation2d.newGrid(shape.columns, shape.rows, shape.layout)
    self.walls = {}
    for row = 0, shape.rows - 1 do
        for column = 0, shape.columns - 1 do
            if self.random:chance(0.22) then
                self:setWall(column, row, true)
            end
        end
    end
    self.start, self.goal = {0, 0}, {shape.columns - 1, shape.rows - 1}
    self:setWall(0, 0, false)
    self:setWall(shape.columns - 1, shape.rows - 1, false)

    -- The grid is centered on the stage from the extent of its cell centers.
    local left, top, right, bottom = math.huge, math.huge, -math.huge, -math.huge
    for row = 0, shape.rows - 1 do
        for column = 0, shape.columns - 1 do
            local x, y = shape.center(column, row)
            left, top, right, bottom = math.min(left, x), math.min(top, y), math.max(right, x), math.max(bottom, y)
        end
    end
    self.offset = {-(left + right) / 2, -(top + bottom) / 2}
    self:search()
end

function HexIso:setWall(column, row, wall)
    self.walls[row * self.shape.columns + column] = wall or nil
    self.grid:setWalkable(column, row, not wall)
end

function HexIso:center(column, row)
    local x, y = self.shape.center(column, row)
    return x + self.offset[1], y + self.offset[2]
end

function HexIso:cellAt(x, y)
    local best, bestColumn, bestRow = kPickDistance * kPickDistance, nil, nil
    for row = 0, self.shape.rows - 1 do
        for column = 0, self.shape.columns - 1 do
            local cx, cy = self:center(column, row)
            local distance = (cx - x) ^ 2 + (cy - y) ^ 2
            if distance < best then
                best, bestColumn, bestRow = distance, column, row
            end
        end
    end
    return bestColumn, bestRow
end

function HexIso:search()
    self.path, self.cost = self.grid:findPath(self.start[1], self.start[2], self.goal[1], self.goal[2])
end

function HexIso:exit()
    HexIso.super.exit(self)
    self.grid, self.walls = nil, nil
end

function HexIso:update(dt)
    HexIso.super.update(self, dt)
    if input.pressed('reset') then
        self:build(self.name)
    end
    if self.pointer.pressed then
        local column, row = self:cellAt(self.pointer.worldX, self.pointer.worldY)
        if column then
            if self.brush == 'wall' then
                self:setWall(column, row, not self.walls[row * self.shape.columns + column])
            else
                self:setWall(column, row, false)
                self[self.brush] = {column, row}
            end
            self:search()
        end
    end
    self:showStats(string.format('topology %s\nstagger %s\npath %s', self.grid.topology, self.grid.staggerX and 'columns' or 'rows', self.path and string.format('%d cells, cost %.1f', #self.path, self.cost) or 'none'))
end

function HexIso:drawCell(column, row, color, order)
    local x, y = self:center(column, row)
    local points = {}
    for index = 0, (self.shape.corners or 6) - 1 do
        local dx, dy = self.shape.corner(index)
        points[#points + 1] = {x + dx * 0.94, y + dy * 0.94}
    end
    graphics2d.drawPolygon(points, color, order)
end

function HexIso:render()
    self:beginWorld()
    local onPath = {}
    for _, cell in ipairs(self.path or {}) do
        onPath[cell.y * self.shape.columns + cell.x] = true
    end
    for row = 0, self.shape.rows - 1 do
        for column = 0, self.shape.columns - 1 do
            local index = row * self.shape.columns + column
            local color = self.walls[index] and '#FF90A4AE' or (onPath[index] and '#FF8D7B2E' or '#FF2B3240')
            self:drawCell(column, row, color)
        end
    end
    self:drawCell(self.start[1], self.start[2], '#FF66BB6A', {layer = 1})
    self:drawCell(self.goal[1], self.goal[2], '#FFEF5350', {layer = 1})
    if self.path then
        local points = {}
        for index, cell in ipairs(self.path) do
            points[index] = {self:center(cell.x, cell.y)}
        end
        graphics2d.drawPolyline(points, 4, '#FFFFD54F', false, {layer = 2})
    end
end

return HexIso
