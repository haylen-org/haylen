-- A Dijkstra map from the player, which red monsters walk downhill to chase it, and its flee map, which blue monsters walk downhill to run away along open routes.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local procedural2d = require('haylen.procedural2d')
local profiler = require('haylen.debug')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Board = require('board')
local picture = require('picture')
local sample = require('sample')

local Dijkstra = haylen.class('Dijkstra', sample.Test)

local kColumns, kRows, kCell = 48, 26, 30
local kMonsters = 10
local kStepTime = 0.15

function Dijkstra:enter()
    Dijkstra.super.enter(self, {
        hint = 'The player follows the pointer. Red monsters chase it on the Dijkstra map, blue ones flee it on the flee map, and the gold pulls the chasers when it is on.',
        controls = {
            ui.radioGroup{id = 'view', items = {{id = 'chase', text = 'Show the chase map'}, {id = 'flee', text = 'Show the flee map'}, {id = 'none', text = 'Show the map only'}}, selected = 'chase', onChange = function(event)
                self.view = event.value
                self:paint()
            end},
            ui.checkbox{id = 'gold', text = 'Gold pulls the chasers', onChange = function(event)
                self.useGold = event.checked
                self:plan()
            end},
            ui.button{id = 'reset', text = 'New caves', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'view',
    })
    self.view, self.useGold = 'chase', false
    self.random = m.random(59)
    self.board = Board.new(kColumns, kRows, kCell)
    self:build()
    timer.every(kStepTime, function() self:stepMonsters() end, {owner = self})
end

function Dijkstra:build()
    local cave = procedural2d.cellularAutomaton({width = kColumns, height = kRows, fillChance = 0.4, steps = 4, seed = self.random:integer(1, 9999)})
    self.cave = cave
    self.grid = navigation2d.newGrid(kColumns, kRows)
    local open = {}
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local wall = cave:get(column, row) == 1
            self.grid:setWalkable(column, row, not wall)
            if not wall then
                open[#open + 1] = {column, row}
            end
        end
    end
    self.open = open
    self.player = open[#open // 2 + 1]
    self.gold = open[self.random:integer(1, #open)]
    self.monsters = {}
    for index = 1, kMonsters * 2 do
        local cell = open[self.random:integer(1, #open)]
        self.monsters[index] = {x = cell[1], y = cell[2], chaser = index <= kMonsters}
    end
    self:plan()
end

-- Computes both maps for the player cell, with the gold as a second, stronger source of the chase map when it is on.
function Dijkstra:plan()
    local sources = {{x = self.player[1], y = self.player[2]}}
    if self.useGold then
        sources[2] = {x = self.gold[1], y = self.gold[2], value = -6}
    end
    profiler.beginScope('dijkstra maps')
    self.chase = self.grid:dijkstraMap(sources)
    self.flee = self.grid:dijkstraMap({{x = self.player[1], y = self.player[2]}})
    self.flee:flee()
    profiler.endScope()
    self:paint()
end

function Dijkstra:paint()
    local map = self.view == 'chase' and self.chase or self.flee
    local low, high = math.huge, -math.huge
    if self.view ~= 'none' then
        for _, cell in ipairs(self.open) do
            local value = map:value(cell[1], cell[2])
            if value < math.huge then
                low, high = math.min(low, value), math.max(high, value)
            end
        end
    end
    self.picture = picture.cells(kColumns, kRows, function(column, row)
        if self.cave:get(column, row) == 1 then
            return '#FF546E7A'
        end
        if self.view == 'none' then
            return '#FF232A36'
        end
        local value = map:value(column, row)
        return value < math.huge and picture.heat((value - low) / math.max(high - low, 1)) or '#FF000000'
    end)
end

function Dijkstra:stepMonsters()
    for _, monster in ipairs(self.monsters) do
        local x, y = (monster.chaser and self.chase or self.flee):next(monster.x, monster.y)
        if x then
            monster.x, monster.y = x, y
        end
    end
end

function Dijkstra:exit()
    Dijkstra.super.exit(self)
    self.grid, self.chase, self.flee, self.cave, self.picture = nil, nil, nil, nil, nil
end

function Dijkstra:update(dt)
    Dijkstra.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local column, row = self.board:cellAt(self.pointer.worldX, self.pointer.worldY)
    if column and self.grid:walkable(column, row) and (column ~= self.player[1] or row ~= self.player[2]) then
        self.player = {column, row}
        self:plan()
    end
    self:showStats(string.format('Maps %.3f ms\nPlayer %d, %d\nChase value here %.1f', self:timing('dijkstra maps'), self.player[1], self.player[2], self.chase:value(self.player[1], self.player[2])))
end

function Dijkstra:render()
    self:beginWorld()
    local board = self.board
    board:drawPicture(self.picture)
    for _, monster in ipairs(self.monsters) do
        local x, y = board:center(monster.x, monster.y)
        graphics2d.drawCircle(x, y, kCell * 0.38, monster.chaser and '#FFEF5350' or '#FF42A5F5', {layer = 2})
    end
    local px, py = board:center(self.player[1], self.player[2])
    graphics2d.drawCircle(px, py, kCell * 0.45, '#FFFFFFFF', {layer = 3})
    if self.useGold then
        local gx, gy = board:center(self.gold[1], self.gold[2])
        graphics2d.drawCircle(gx, gy, kCell * 0.4, '#FFFFD54F', {layer = 3})
    end
end

return Dijkstra
