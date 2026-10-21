-- Dungeons generated on a worker thread: rooms from binary space partitioning or from random placement joined by a minimum spanning tree, with the corridors carved between them.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local procedural2d = require('haylen.procedural2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')
local Board = require('categories.algorithms.board')

local Dungeon = haylen.class('Dungeon', AlgorithmTest)

Dungeon.code = [[
local options = {method = 'bsp', width = 96, height = 52, minimumRoomSize = 4, maximumRoomSize = 12, seed = 1}
local dungeon = procedural2d.dungeonAsync(options):await()
local wall = dungeon.grid:get(10, 20) == 1  -- Floors are 0 and walls are 1.
local room = dungeon.rooms[1]  -- A table of x, y, width and height.
local link = dungeon.connections[1]  -- The positions of two rooms that a corridor joins.]]

local kColumns, kRows, kCell = 96, 52, 15

function Dungeon:enter()
    self:frame{
        hint = 'Pick a method and room sizes. The lines join the rooms the corridors connect, so every room reaches every other one. R or the X button takes a new seed.',
        code = Dungeon.code,
        controls = {
            ui.formField{label = 'Rooms', ui.radioGroup{id = 'method', items = {{id = 'bsp', text = 'Binary space partitioning'}, {id = 'placement', text = 'Random placement'}}, selected = 'bsp', onChange = function(event) self:change('method', event.value) end}},
            ui.formField{label = 'Largest room side', ui.slider{id = 'size', value = 12, min = 6, max = 18, step = 1, showValue = true, decimals = 0, onChange = function(event) self:change('size', event.value) end}},
            ui.checkbox{id = 'links', text = 'Show the connections', checked = true, onChange = function(event) self.showLinks = event.checked end},
            ui.button{id = 'seed', text = 'New seed', onClick = function() self:change('seed', self.options.seed + 1) end},
        },
        focus = 'method',
    }
    self.options = {method = 'bsp', size = 12, seed = 1}
    self.showLinks = true
    self.board = Board(kColumns, kRows, kCell)
    self.art = Art()
    self.tiles = graphics2d.newSpriteBatch(self.art.terrain)
    self:generate()
end

function Dungeon:change(name, value)
    self.options[name] = value
    self:generate()
end

function Dungeon:generate()
    local options = self.options
    self.request = (self.request or 0) + 1
    local request, started = self.request, haylen.elapsed()
    local description = {method = options.method, width = kColumns, height = kRows, minimumRoomSize = 4, maximumRoomSize = math.tointeger(options.size), minimumLeafSize = math.tointeger(options.size) + 2, maximumRooms = 18, seed = options.seed}
    self:spawn(function()
        local dungeon, failure = procedural2d.dungeonAsync(description):await()
        if request ~= self.request then
            return
        end
        self.failure = failure
        if dungeon then
            self.latency = (haylen.elapsed() - started) * 1000
            self.dungeon = dungeon
            self:paint(dungeon)
        end
    end)
end

function Dungeon:update(dt)
    Dungeon.super.update(self, dt)
    if input.pressed('reset') then
        self:change('seed', self.options.seed + 1)
    end
    local dungeon = self.dungeon
    self:status(self.failure or string.format('%d rooms joined by %d corridors, made in %.0f ms', dungeon and #dungeon.rooms or 0, dungeon and #dungeon.connections or 0, self.latency or 0))
end

-- Rooms have floors of stone slabs and corridors of packed earth, and only the walls that touch a floor show their bricks, so the dungeon reads like a map.
function Dungeon:paint(dungeon)
    local grid, board, tiles = dungeon.grid, self.board, self.tiles
    local inRoom = {}
    for _, room in ipairs(dungeon.rooms) do
        for row = room.y, room.y + room.height - 1 do
            for column = room.x, room.x + room.width - 1 do
                inRoom[row * kColumns + column] = true
            end
        end
    end
    tiles:clear()
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            if grid:get(column, row) == 0 then
                self.art:addCell(tiles, inRoom[row * kColumns + column] and 'stone' or 'dirt', board.left, board.top, kCell, column, row)
            elseif self:touchesFloor(grid, column, row) then
                self.art:addCell(tiles, 'brick', board.left, board.top, kCell, column, row, '#FFB8A8B0')
            end
        end
    end
end

function Dungeon:touchesFloor(grid, column, row)
    for dy = -1, 1 do
        for dx = -1, 1 do
            local x, y = column + dx, row + dy
            if x >= 0 and y >= 0 and x < kColumns and y < kRows and grid:get(x, y) == 0 then
                return true
            end
        end
    end
    return false
end

function Dungeon:roomCenter(room)
    return self.board:center(room.x + room.width / 2 - 0.5, room.y + room.height / 2 - 0.5)
end

function Dungeon:draw(area)
    local board, dungeon = self.board, self.dungeon
    if dungeon == nil then
        return
    end
    graphics2d.drawShape({board.left - 6, board.top - 6, board.width + 12, board.height + 12}, {radius = 10, color = '#FF10131B', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    self.tiles:draw({layer = 0})
    if self.showLinks then
        for _, link in ipairs(dungeon.connections) do
            local x1, y1 = self:roomCenter(dungeon.rooms[link[1]])
            local x2, y2 = self:roomCenter(dungeon.rooms[link[2]])
            graphics2d.drawStroke({x1, y1, x2, y2}, {width = 4, color = '#CC2FB3C4', cap = 'round', dashes = {12, 8}, layer = 2})
        end
    end
    -- The numbers sit on pills above the links, so a line never crosses a number.
    for index, room in ipairs(dungeon.rooms) do
        local cx, cy = self:roomCenter(room)
        self.art:label(tostring(index), cx, cy, 24, '#FFFFFFFF', 10)
    end
end

return Dungeon
