-- Caves generated on a worker thread, grown by a cellular automaton or dug by drunkard walkers, with every separate cave colored by `spatial2d.components`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local procedural2d = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')
local Board = require('categories.algorithms.board')
local picture = require('categories.algorithms.picture')

local Caves = haylen.class('Caves', AlgorithmTest)

Caves.code = [[
local cave = procedural2d.cellularAutomatonAsync({width = 128, height = 68, fillChance = 0.45, steps = 5, seed = 1}):await()
local tunnels = procedural2d.drunkardWalkAsync({width = 128, height = 68, coverage = 0.45, walkers = 5, seed = 1}):await()
local labels, caves = spatial2d.components(cave, {background = 1})  -- Walls are 1 and form no cave.
local cave = labels:get(column, row)  -- The number of the cave a cell belongs to.]]

local kColumns, kRows, kCell = 128, 68, 11
-- Soft tints of the earth of the separate caves, so each one reads apart without loud colors.
local kTints = {'#FFFFFFFF', '#FFE6C9A8', '#FFC9D8B0', '#FFB8C8D8', '#FFE0C0C8', '#FFD8D0A0', '#FFC0D0C0', '#FFD0B8A0'}
local kRegenerateDelay = 0.2

function Caves:enter()
    self:frame{
        hint = 'Every change grows a new map on a worker thread while this one stays on screen. The colors tell the caves that do not connect apart. R or the X button takes a new seed.',
        code = Caves.code,
        controls = {
            ui.formField{label = 'Generator', ui.radioGroup{id = 'method', items = {{id = 'automaton', text = 'Cellular automaton'}, {id = 'walk', text = 'Drunkard walk'}}, selected = 'automaton', onChange = function(event) self:change('method', event.value) end}},
            ui.formField{label = 'Wall chance, or coverage of the walk', ui.slider{id = 'fill', value = 0.45, min = 0.3, max = 0.6, step = 0.01, showValue = true, onChange = function(event) self:change('fill', event.value) end}},
            ui.formField{label = 'Smoothing steps, or walkers', ui.slider{id = 'steps', value = 5, min = 1, max = 10, step = 1, showValue = true, decimals = 0, onChange = function(event) self:change('steps', event.value) end}},
            ui.checkbox{id = 'regions', text = 'Color the separate caves', checked = true, onChange = function(event) self:change('regions', event.checked) end},
            ui.button{id = 'seed', text = 'New seed', onClick = function() self:change('seed', self.options.seed + 1) end},
        },
        focus = 'method',
    }
    self.options = {method = 'automaton', fill = 0.45, steps = 5, regions = true, seed = 1}
    self.board = Board(kColumns, kRows, kCell)
    self.art = Art()
    self.floor = graphics2d.newSpriteBatch(self.art.terrain)
    self.walls = graphics2d.newSpriteBatch(self.art.terrain)
    self.wait = 0
    self:generate()
end

function Caves:change(name, value)
    self.options[name] = value
    self.dirty = true
end

function Caves:generate()
    local options = self.options
    local started = haylen.elapsed()
    local promise
    if options.method == 'automaton' then
        promise = procedural2d.cellularAutomatonAsync({width = kColumns, height = kRows, fillChance = options.fill, steps = math.tointeger(options.steps), seed = options.seed})
    else
        promise = procedural2d.drunkardWalkAsync({width = kColumns, height = kRows, coverage = options.fill, walkers = math.tointeger(options.steps), seed = options.seed})
    end
    self.request = (self.request or 0) + 1
    local request = self.request
    self:spawn(function()
        local cave = promise:await()
        if cave and request == self.request then
            self.latency = (haylen.elapsed() - started) * 1000
            self:show(cave)
        end
    end)
end

-- The walls are painted stone and the floor painted earth, each cave tinted apart, with a shadow that the walls cast on the floor below them.
function Caves:show(cave)
    local labels, count = spatial2d.components(cave, {background = 1})
    self.count, self.cave = count, cave
    local open, board = 0, self.board
    self.floor:clear()
    self.walls:clear()
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            if cave:get(column, row) == 1 then
                self.art:addCell(self.walls, 'stone', board.left, board.top, kCell, column, row)
            else
                open = open + 1
                local tint = self.options.regions and kTints[labels:get(column, row) % #kTints + 1] or nil
                self.art:addCell(self.floor, 'dirt', board.left, board.top, kCell, column, row, tint)
            end
        end
    end
    self.shadow = picture.cells(kColumns, kRows, function(column, row)
        return cave:get(column, row) == 1 and '#80000000' or '#00000000'
    end)
    self.open = open
end

function Caves:update(dt)
    Caves.super.update(self, dt)
    if input.pressed('reset') then
        self:change('seed', self.options.seed + 1)
    end
    self.wait = math.max(0, self.wait - dt)
    if self.dirty and self.wait == 0 then
        self.dirty, self.wait = false, kRegenerateDelay
        self:generate()
    end
    self:status(string.format('%d caves with %d open cells of %d, grown in %.0f ms', self.count or 0, self.open or 0, kColumns * kRows, self.latency or 0))
end

function Caves:draw(area)
    local board = self.board
    graphics2d.drawShape({board.left - 6, board.top - 6, board.width + 12, board.height + 12}, {radius = 10, color = '#FF151A24', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    self.floor:draw({layer = 0})
    if self.shadow then
        graphics2d.draw(self.shadow, board.left + 3, board.top + 4, {pivotX = 0, pivotY = 0, width = board.width, height = board.height, layer = 1})
    end
    self.walls:draw({layer = 2})
end

return Caves
