-- Dungeons generated on a worker thread: rooms from binary space partitioning or from random placement joined by a minimum spanning tree, with the corridors carved between them.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local procedural2d = require('haylen.procedural2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Board = require('categories.algorithms.board')
local picture = require('categories.algorithms.picture')

local Dungeon = haylen.class('Dungeon', AlgorithmTest)

local kColumns, kRows, kCell = 96, 52, 15

function Dungeon:enter()
    self:frame{
        hint = 'Pick a method and room sizes. The lines join the rooms the corridors connect, so every room reaches every other one. R or the X button takes a new seed.',
        controls = {
            ui.radioGroup{id = 'method', items = {{id = 'bsp', text = 'Binary space partitioning'}, {id = 'placement', text = 'Random placement'}}, selected = 'bsp', onChange = function(event) self:change('method', event.value) end},
            ui.label{text = 'Largest room side', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'size', value = 12, min = 6, max = 18, step = 1, showValue = true, decimals = 0, onChange = function(event) self:change('size', event.value) end},
            ui.checkbox{id = 'links', text = 'Show the connections', checked = true, onChange = function(event) self.showLinks = event.checked end},
            ui.button{id = 'seed', text = 'New seed', onClick = function() self:change('seed', self.options.seed + 1) end},
        },
        focus = 'method',
    }
    self.options = {method = 'bsp', size = 12, seed = 1}
    self.showLinks = true
    self.board = Board(kColumns, kRows, kCell)
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
            self.picture = picture.cells(kColumns, kRows, function(column, row)
                return dungeon.grid:get(column, row) == 1 and '#FF263238' or '#FFA1887F'
            end)
        end
    end)
end

function Dungeon:update(dt)
    Dungeon.super.update(self, dt)
    if input.pressed('reset') then
        self:change('seed', self.options.seed + 1)
    end
    local dungeon = self.dungeon
    self:status(self.failure or string.format('Rooms %d, connections %d, ready in %.0f ms', dungeon and #dungeon.rooms or 0, dungeon and #dungeon.connections or 0, self.latency or 0))
end

function Dungeon:roomCenter(room)
    return self.board:center(room.x + room.width / 2 - 0.5, room.y + room.height / 2 - 0.5)
end

function Dungeon:draw(area)
    local board, dungeon = self.board, self.dungeon
    if dungeon == nil then
        return
    end
    board:drawPicture(self.picture)
    for index, room in ipairs(dungeon.rooms) do
        local x, y = board.left + room.x * kCell, board.top + room.y * kCell
        graphics2d.drawRectOutline({x, y, room.width * kCell, room.height * kCell}, 3, '#FFFFD54F', {layer = 1})
        local cx, cy = self:roomCenter(room)
        graphics2d.drawText(nil, tostring(index), cx, cy, {size = 26, anchor = {0.5, 0.5}, color = '#FF3E2723', layer = 3})
    end
    if self.showLinks then
        for _, link in ipairs(dungeon.connections) do
            local x1, y1 = self:roomCenter(dungeon.rooms[link[1]])
            local x2, y2 = self:roomCenter(dungeon.rooms[link[2]])
            graphics2d.drawLine(x1, y1, x2, y2, 3, '#CC4FC3F7', {layer = 2})
        end
    end
end

return Dungeon
