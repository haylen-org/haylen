-- A flow field computed once on a worker thread for the cell under the pointer, and hundreds of units that only read the step of their cell.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')
local Board = require('categories.algorithms.board')
local picture = require('categories.algorithms.picture')

local FlowField = haylen.class('FlowField', AlgorithmTest)

FlowField.code = [[
async.spawn(function()
    field = grid:flowFieldAsync({{32, 17}}):await()  -- Computed on a worker thread.
end)
local direction = field:direction(unit.x // 24, unit.y // 24)  -- Each unit reads only its cell.
unit.x, unit.y = unit.x + direction.x * 150 * dt, unit.y + direction.y * 150 * dt]]

local kColumns, kRows, kCell = 64, 34, 24
-- The arrow of the effects atlas points up, so it turns a quarter turn more than the angle of its step.
FlowField.arrowTurn = math.pi / 2
local kUnits = 800
local kSpeed = 150

function FlowField:enter()
    self:frame{
        hint = 'Move the pointer, or tap, to call every unit to that cell. Each new goal computes a field on a worker thread while the units keep walking the old one. R or the X button builds new walls.',
        code = FlowField.code,
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
    self.art = Art()
    self.orbs = assets.texture('sprites/images/bullets.png', {filter = 'linear', mipmaps = true})
    self.effects = assets.load('particles/library/textures/effects.json', 'atlas', {filter = 'linear'})
    self.walls = graphics2d.newSpriteBatch(self.art.terrain)
    self.arrows = graphics2d.newSpriteBatch(self.effects.texture)
    self:build()
end

function FlowField:build()
    self.grid = navigation2d.newGrid(kColumns, kRows)
    local noise = m.noise(self.random:integer(1, 999))
    self.blocked = {}
    self.walls:clear()
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local wall = noise:fractal(column / 12, row / 12, 3) > 0.25
            self.blocked[row * kColumns + column] = wall
            self.grid:setWalkable(column, row, not wall)
            if wall then
                self.art:addCell(self.walls, 'stone', self.board.left, self.board.top, kCell, column, row)
            end
        end
    end
    -- The walls cast a short shadow on the floor, down and to the right.
    self.shadow = picture.cells(kColumns, kRows, function(column, row)
        return self.blocked[row * kColumns + column] and '#90000000' or '#00000000'
    end)
    self.arrows:clear()
    self.field, self.goal, self.wanted = nil, nil, {kColumns // 2, kRows // 2}
    self:scatter()
end

function FlowField:scatter()
    self.units, self.sprites = {}, {}
    while #self.units < kUnits do
        local column, row = self.random:integer(0, kColumns - 1), self.random:integer(0, kRows - 1)
        if not self.blocked[row * kColumns + column] then
            local x, y = self.board:center(column, row)
            self.units[#self.units + 1] = {x = x + self.random:range(-8, 8), y = y + self.random:range(-8, 8)}
            self.sprites[#self.sprites + 1] = {width = 15, height = 15, source = {64, 0, 64, 64}, color = m.fromHsv(self.random:nextFloat() * 0.12 + 0.52, 0.35, 1):toHex()}
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
        self:pointArrows()
    end)
end

function FlowField:update(dt)
    FlowField.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local column, row = self.board:cellAt(self.pointer.worldX, self.pointer.worldY)
    if column and not self.blocked[row * kColumns + column] then
        self.wanted = {column, row}
    end
    self:request()
    self:move(dt)
    self:status(string.format('%d units on %d cells, the last field took %s, toward the cell %s', #self.units, kColumns * kRows, self.latency and string.format('%.0f ms', self.latency) or 'a moment', self.goal and string.format('%d, %d', self.goal[1], self.goal[2]) or 'none yet'))
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
            if nextColumn and not self.blocked[nextRow * kColumns + nextColumn] then
                unit.x, unit.y = nx, ny
            else
                local cx, cy = board:center(column, row)
                unit.x, unit.y = unit.x + (cx - unit.x) * 0.2, unit.y + (cy - unit.y) * 0.2
            end
        end
    end
end

-- The arrows of a field are one sprite per open cell, turned to the step of the cell, in a batch made once for every new field.
function FlowField:pointArrows()
    local field, board, source = self.field, self.board, self.effects:source('arrow')
    self.arrows:clear()
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local direction = field:direction(column, row)
            if not direction:isZero() then
                local x, y = board:center(column, row)
                self.arrows:add({x = x, y = y, width = kCell * 0.85, height = kCell * 0.85, rotation = math.atan(direction.y, direction.x) + FlowField.arrowTurn, source = source, color = '#80B4C8F0'})
            end
        end
    end
end

function FlowField:draw(area)
    local board = self.board
    graphics2d.drawRect({board.left, board.top, board.width, board.height}, '#FF2A3448', {layer = -2})
    board:drawPicture(self.shadow, {layer = -1})
    graphics2d.draw(self.shadow, board.left + 5, board.top + 6, {pivotX = 0, pivotY = 0, width = board.width, height = board.height, layer = -1})
    self.walls:draw({layer = 0})
    local field = self.field
    if field and self.showDistance then
        for row = 0, kRows - 1 do
            for column = 0, kColumns - 1 do
                local distance = field:distance(column, row)
                if distance < math.huge then
                    graphics2d.drawRect(board:cellRect(column, row, 1), picture.heat(distance / 80), {layer = 1})
                end
            end
        end
    end
    if field and self.showArrows then
        self.arrows:draw({layer = 2})
    end
    for index, unit in ipairs(self.units) do
        local sprite = self.sprites[index]
        sprite.x, sprite.y = unit.x, unit.y
    end
    graphics2d.drawBatch(self.orbs, self.sprites, {layer = 3})
    if self.goal then
        local x, y = board:center(self.goal[1], self.goal[2])
        self.art:goal(x, y + 10, 52, 10)
    end
end

return FlowField
