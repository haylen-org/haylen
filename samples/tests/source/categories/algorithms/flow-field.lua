-- A flow field computed once on a worker thread for the cell under the pointer, and hundreds of units that only read the step of their cell.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Board = require('categories.algorithms.board')
local picture = require('categories.algorithms.picture')

local FlowField = haylen.class('FlowField', AlgorithmTest)

local kColumns, kRows, kCell = 64, 34, 24
local kUnits = 800
local kSpeed = 150

function FlowField:enter()
    self:frame{
        hint = 'Move the pointer, or tap, to call every unit to that cell. Each new goal computes a field on a worker thread while the units keep walking the old one. R or the X button builds new walls.',
        controls = {
            ui.checkbox{id = 'arrows', text = 'Show the field', checked = true, onChange = function(event) self.showArrows = event.checked end},
            ui.checkbox{id = 'distance', text = 'Show the distances', onChange = function(event) self.showDistance = event.checked end},
            ui.button{id = 'scatter', text = 'Scatter the units', onClick = function() self:scatter() end},
            ui.button{id = 'walls', text = 'New walls', onClick = function() self:build() end},
        },
        focus = 'arrows',
    }
    self.showArrows = true
    self.random = m.random(53)
    self.board = Board(kColumns, kRows, kCell)
    self.white = graphics.whiteTexture()
    self:build()
end

function FlowField:build()
    self.grid = navigation2d.newGrid(kColumns, kRows)
    local noise = m.noise(self.random:integer(1, 999))
    self.walls = {}
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local wall = noise:fractal(column / 12, row / 12, 3) > 0.25
            self.walls[row * kColumns + column] = wall
            self.grid:setWalkable(column, row, not wall)
        end
    end
    self.picture = picture.cells(kColumns, kRows, function(column, row)
        return self.walls[row * kColumns + column] and '#FF546E7A' or '#FF232A36'
    end)
    self.field, self.goal, self.wanted = nil, nil, {kColumns // 2, kRows // 2}
    self:scatter()
end

function FlowField:scatter()
    self.units, self.sprites = {}, {}
    while #self.units < kUnits do
        local column, row = self.random:integer(0, kColumns - 1), self.random:integer(0, kRows - 1)
        if not self.walls[row * kColumns + column] then
            local x, y = self.board:center(column, row)
            self.units[#self.units + 1] = {x = x + self.random:range(-8, 8), y = y + self.random:range(-8, 8)}
            self.sprites[#self.sprites + 1] = {width = 8, height = 8, color = m.fromHsv(self.random:nextFloat() * 0.15 + 0.5, 0.6, 1):toHex()}
        end
    end
end

-- Starts a field for the wanted goal unless one is running, and swaps it in once the worker finishes.
function FlowField:request()
    if self.pending or self.wanted == nil or (self.goal and self.goal[1] == self.wanted[1] and self.goal[2] == self.wanted[2]) then
        return
    end
    local grid, goal, started = self.grid, self.wanted, haylen.elapsed()
    self.pending = true
    self:spawn(function()
        local field = grid:flowFieldAsync({goal}):await()
        self.pending = false
        if field == nil or grid ~= self.grid then
            return
        end
        self.field, self.goal = field, goal
        self.latency = (haylen.elapsed() - started) * 1000
    end)
end

function FlowField:update(dt)
    FlowField.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local column, row = self.board:cellAt(self.pointer.worldX, self.pointer.worldY)
    if column and not self.walls[row * kColumns + column] then
        self.wanted = {column, row}
    end
    self:request()
    self:move(dt)
    self:status(string.format('Units %d, cells %d, last field %s, goal %s', #self.units, kColumns * kRows, self.latency and string.format('in %.0f ms', self.latency) or 'pending', self.goal and string.format('%d, %d', self.goal[1], self.goal[2]) or 'none'))
end

-- Every unit reads the direction of its cell and slides a little away from walls it touches.
function FlowField:move(dt)
    local field = self.field
    if field == nil then
        return
    end
    local board = self.board
    for _, unit in ipairs(self.units) do
        local column, row = board:cellAt(unit.x, unit.y)
        if column then
            local direction = field:direction(column, row)
            local nx, ny = unit.x + direction.x * kSpeed * dt, unit.y + direction.y * kSpeed * dt
            local nextColumn, nextRow = board:cellAt(nx, ny)
            if nextColumn and not self.walls[nextRow * kColumns + nextColumn] then
                unit.x, unit.y = nx, ny
            else
                local cx, cy = board:center(column, row)
                unit.x, unit.y = unit.x + (cx - unit.x) * 0.2, unit.y + (cy - unit.y) * 0.2
            end
        end
    end
end

function FlowField:draw(area)
    local board = self.board
    board:drawPicture(self.picture)
    local field = self.field
    if field and (self.showArrows or self.showDistance) then
        for row = 0, kRows - 1 do
            for column = 0, kColumns - 1 do
                local x, y = board:center(column, row)
                if self.showDistance then
                    local distance = field:distance(column, row)
                    if distance < math.huge then
                        graphics2d.drawRect(board:cellRect(column, row, 1), picture.heat(distance / 80), {layer = 1})
                    end
                end
                if self.showArrows then
                    local direction = field:direction(column, row)
                    if not direction:isZero() then
                        graphics2d.drawLine(x - direction.x * 6, y - direction.y * 6, x + direction.x * 8, y + direction.y * 8, 2, '#66FFFFFF', {layer = 2})
                    end
                end
            end
        end
    end
    for index, unit in ipairs(self.units) do
        local sprite = self.sprites[index]
        sprite.x, sprite.y = unit.x, unit.y
    end
    graphics2d.drawBatch(self.white, self.sprites, {layer = 3})
    if self.goal then
        local x, y = board:center(self.goal[1], self.goal[2])
        graphics2d.drawRing(x, y, 14, 4, '#FFFFD54F', {layer = 4})
    end
end

return FlowField
