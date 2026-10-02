-- Caves generated on a worker thread, grown by a cellular automaton or dug by drunkard walkers, with every separate cave colored by `spatial2d.components`.
local haylen = require('haylen')
local async = require('async')
local input = require('haylen.input')
local m = require('haylen.math')
local procedural2d = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local Board = require('board')
local picture = require('picture')
local sample = require('sample')

local Caves = haylen.class('Caves', sample.Test)

local kColumns, kRows, kCell = 128, 68, 11
local kRegenerateDelay = 0.2

function Caves:enter()
    Caves.super.enter(self, {
        hint = 'Every change grows a new map on a worker thread while this one stays on screen. The colors tell the caves that do not connect apart.',
        controls = {
            ui.radioGroup{id = 'method', items = {{id = 'automaton', text = 'Cellular automaton'}, {id = 'walk', text = 'Drunkard walk'}}, selected = 'automaton', onChange = function(event) self:change('method', event.value) end},
            ui.label{text = 'Wall chance, or coverage of the walk', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'fill', value = 0.45, min = 0.3, max = 0.6, step = 0.01, showValue = true, onChange = function(event) self:change('fill', event.value) end},
            ui.label{text = 'Smoothing steps, or walkers', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'steps', value = 5, min = 1, max = 10, step = 1, showValue = true, decimals = 0, onChange = function(event) self:change('steps', event.value) end},
            ui.checkbox{id = 'regions', text = 'Color the separate caves', checked = true, onChange = function(event) self:change('regions', event.checked) end},
            ui.button{id = 'seed', text = 'New seed', onClick = function() self:change('seed', self.options.seed + 1) end},
        },
        stats = true,
        focus = 'method',
    })
    self.options = {method = 'automaton', fill = 0.45, steps = 5, regions = true, seed = 1}
    self.board = Board.new(kColumns, kRows, kCell)
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
    async.spawn(function()
        local cave = promise:await()
        if cave and self.options and request == self.request then
            self.latency = (haylen.elapsed() - started) * 1000
            self:show(cave)
        end
    end)
end

function Caves:show(cave)
    local labels, count = spatial2d.components(cave, {background = 1})
    self.count, self.cave = count, cave
    local open = 0
    self.picture = picture.cells(kColumns, kRows, function(column, row)
        if cave:get(column, row) == 1 then
            return '#FF37474F'
        end
        open = open + 1
        if not self.options.regions then
            return '#FFBCAAA4'
        end
        return m.fromHsv((labels:get(column, row) * 0.618) % 1, 0.45, 0.9):toHex()
    end)
    self.open = open
end

function Caves:exit()
    Caves.super.exit(self)
    self.cave, self.picture, self.options = nil, nil, nil
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
    self:showStats(string.format('Caves %d\nOpen cells %d of %d\nReady in %.0f ms', self.count or 0, self.open or 0, kColumns * kRows, self.latency or 0))
end

function Caves:render()
    self:beginWorld()
    if self.picture then
        self.board:drawPicture(self.picture)
    end
end

return Caves
