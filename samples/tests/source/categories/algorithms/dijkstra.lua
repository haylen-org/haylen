-- A Dijkstra map from the player, which red monsters walk downhill to chase it, and its flee map, which blue monsters walk downhill to run away along open routes.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local procedural2d = require('haylen.procedural2d')
local profiler = require('haylen.debug')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')
local Board = require('categories.algorithms.board')
local picture = require('categories.algorithms.picture')

local Dijkstra = haylen.class('Dijkstra', AlgorithmTest)

Dijkstra.code = [[
local chase = grid:dijkstraMap({{x = 10, y = 5}, {x = 30, y = 12, value = -6}})  -- The player and the gold.
local flee = grid:dijkstraMap({{x = 10, y = 5}})
flee:flee()
local x, y = chase:next(monster.x, monster.y)  -- One step downhill.]]

local kColumns, kRows, kCell = 48, 26, 28
local kMonsters = 10
local kStepTime = 0.15

function Dijkstra:enter()
    self:frame{
        hint = 'The adventurer follows the pointer. Slimes chase it downhill on the Dijkstra map, bunnies flee it on the flee map, and the coin pulls the slimes when it is on. R or the X button digs new caves.',
        code = Dijkstra.code,
        controls = {
            ui.formField{label = 'Colors of the floor', ui.radioGroup{id = 'view', items = {{id = 'chase', text = 'Distances of the chase'}, {id = 'flee', text = 'Distances of the flight'}, {id = 'none', text = 'No distances'}}, selected = 'chase', onChange = function(event)
                self.shownMap = event.value
                self:paint()
            end}},
            ui.checkbox{id = 'gold', text = 'The coin pulls the slimes', onChange = function(event)
                self.useGold = event.checked
                self:plan()
            end},
            ui.button{id = 'reset', text = 'New caves', onClick = function() self:build() end},
        },
        focus = 'view',
    }
    self.shownMap, self.useGold = 'chase', false
    self.random = m.random(59)
    self.board = Board(kColumns, kRows, kCell, 0, -30)
    self.art = Art()
    self.slime = assets.load('sprites/atlases/slime.json', 'atlas', {filter = 'linear'})
    self.bunny = assets.texture('sprites/images/bunny.png', {filter = 'linear'})
    local ramp = graphics.newImage(256, 4)
    local stops = {}
    for step = 0, 4 do
        stops[#stops + 1] = {step / 4, picture.heat(step / 4)}
    end
    ramp:fillGradient({stops = stops})
    self.ramp = graphics.newTexture(ramp, {filter = 'linear'})
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
    profiler.beginScope('dijkstraMaps')
    self.chase = self.grid:dijkstraMap(sources)
    self.flee = self.grid:dijkstraMap({{x = self.player[1], y = self.player[2]}})
    self.flee:flee()
    profiler.endScope()
    self:paint()
end

function Dijkstra:paint()
    local map = self.shownMap == 'chase' and self.chase or self.flee
    local low, high = math.huge, -math.huge
    if self.shownMap ~= 'none' then
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
        if self.shownMap == 'none' then
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
    self:status(string.format('Both maps in %.3f ms, the adventurer at %d, %d, where the chase map holds %.1f', self:timing('dijkstraMaps'), self.player[1], self.player[2], self.chase:value(self.player[1], self.player[2])))
end

function Dijkstra:draw(area)
    local board = self.board
    graphics2d.drawShape({board.left - 6, board.top - 6, board.width + 12, board.height + 12}, {radius = 10, color = '#FF151A24', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    board:drawPicture(self.picture)
    local slime = self.slime:source('slime 0')
    for _, monster in ipairs(self.monsters) do
        local x, y = board:center(monster.x, monster.y)
        Art.shadow(x, y + kCell * 0.4, kCell * 0.9, 1)
        if monster.chaser then
            graphics2d.draw(self.slime.texture, x, y + kCell * 0.45, {source = slime, width = kCell * 1.8, height = kCell * 1.8, pivotY = 0.95, layer = 2})
        else
            graphics2d.draw(self.bunny, x, y + kCell * 0.45, {width = kCell * 1.4, height = kCell * 1.75, pivotY = 1, layer = 2})
        end
    end
    if self.useGold then
        local gx, gy = board:center(self.gold[1], self.gold[2])
        self.art:prop('coin', gx, gy + kCell * 0.6, kCell * 1.4, 3)
    end
    local px, py = board:center(self.player[1], self.player[2])
    Art.shadow(px, py + kCell * 0.4, kCell, 3)
    self.art:character(px, py + kCell * 0.45, kCell * 2.8, 'blue', 4)

    -- The legend under the map reads the colors of the floor from near to far.
    if self.shownMap ~= 'none' then
        local y = board.top + board.height + 22
        graphics2d.drawText(nil, self.shownMap == 'chase' and 'Near the adventurer' or 'Safest from the adventurer', board.left, y, {size = 22, color = '#FFAAB4CC', anchor = {0, 0.5}})
        graphics2d.drawShape({board.left + 260, y - 8, 360, 16}, {radius = 8, texture = self.ramp})
        graphics2d.drawText(nil, self.shownMap == 'chase' and 'Far from it' or 'Closest to it', board.left + 640, y, {size = 22, color = '#FFAAB4CC', anchor = {0, 0.5}})
    end
end

return Dijkstra
