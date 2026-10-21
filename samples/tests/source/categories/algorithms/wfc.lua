-- Wave Function Collapse on a worker thread: a terrain of six tiles where each tile only touches its neighbors in the chain from deep water to mountains, with tiles the player pins in place.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local assets = require('haylen.assets')
local procedural2d = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')
local Board = require('categories.algorithms.board')

local Wfc = haylen.class('Wfc', AlgorithmTest)

Wfc.code = [[
local allow = {{3, 4, 'right'}, {4, 3, 'right'}, {3, 4, 'down'}, {4, 3, 'down'}}  -- Grass and forest touch on every side.
local fixed = spatial2d.newCellGrid(64, 34, -1)  -- The value -1 leaves a cell free.
fixed:set(20, 10, 5)  -- Pins a mountain.
local options = {tiles = 6, allow = allow, weights = {3, 3, 2, 4, 3, 2}, width = 64, height = 34, fixed = fixed, seed = 2}
local map = procedural2d.waveFunctionCollapseAsync(options):await()]]

local kColumns, kRows, kCell = 64, 34, 23
-- Each tile draws from a painted texture of the Tiled tests, darkened for deep water and forests, with bushes on forests and rocks on mountains.
local kTiles = {
    {name = 'Deep water', terrain = 'water', tint = '#FF6E8FCC'},
    {name = 'Water', terrain = 'water'},
    {name = 'Sand', terrain = 'sand'},
    {name = 'Grass', terrain = 'grass'},
    {name = 'Forest', terrain = 'grass', tint = '#FFA8C890', prop = 'bush'},
    {name = 'Mountain', terrain = 'stone', prop = 'rock'},
}
local kWeights = {3, 3, 2, 4, 3, 2}

-- Each tile touches itself and the tiles right before and after it in the chain, on every side. A rule lets its second tile sit on one side of its first, so every pair takes both orders.
local function rules()
    local allow = {}
    for tile = 0, #kTiles - 1 do
        for _, other in ipairs({tile, tile + 1}) do
            if other < #kTiles then
                allow[#allow + 1] = {tile, other, 'right'}
                allow[#allow + 1] = {tile, other, 'down'}
                allow[#allow + 1] = {other, tile, 'right'}
                allow[#allow + 1] = {other, tile, 'down'}
            end
        end
    end
    return allow
end

function Wfc:enter()
    local items, shares = {}, {}
    for index, tile in ipairs(kTiles) do
        items[index] = {id = tostring(index - 1), text = tile.name}
        shares[index] = ui.label{id = 'share' .. index, text = string.format('%s: weight %g', tile.name, kWeights[index])}
    end
    self:frame{
        hint = 'Tap or click cells to pin the selected tile there, then the map collapses again around every pin. Mountains never touch water. R or the X button takes a new seed.',
        code = Wfc.code,
        controls = {
            ui.label{text = 'Tile to pin', font = 'caption', color = 'textMuted'},
            ui.combo{id = 'tile', items = items, selected = '5', onChange = function(event) self.pin = tonumber(event.value) end},
            ui.checkbox{id = 'periodic', text = 'Seamless edges', onChange = function(event)
                self.periodic = event.checked
                self:generate()
            end},
            ui.button{id = 'seed', text = 'New seed', onClick = function()
                self.seed = self.seed + 1
                self:generate()
            end},
            ui.button{id = 'clear', text = 'Remove the pins', onClick = function()
                self.fixed:fill(-1)
                self.pins = 0
                self:generate()
            end},
            ui.formField{label = 'Weight and share of the map', ui.column{gap = 4, children = shares}},
        },
        focus = 'tile',
    }
    self.pin, self.periodic, self.seed, self.pins = 5, false, 2, 0
    self.art = Art()
    self.batch = graphics2d.newSpriteBatch(self.art.terrain)
    self.allow = rules()
    self.fixed = spatial2d.newCellGrid(kColumns, kRows, -1)
    self.board = Board(kColumns, kRows, kCell)
    self:generate()
end

function Wfc:generate()
    self.request = (self.request or 0) + 1
    local request, started = self.request, haylen.elapsed()
    local options = {tiles = #kTiles, allow = self.allow, weights = kWeights, width = kColumns, height = kRows, periodic = self.periodic, fixed = self.fixed, seed = self.seed, attempts = 20}
    self:spawn(function()
        local map = procedural2d.waveFunctionCollapseAsync(options):await()
        if request ~= self.request then
            return
        end
        self.latency = (haylen.elapsed() - started) * 1000
        self.failed = map == nil
        if map then
            self:paint(map)
        end
    end)
end

function Wfc:update(dt)
    Wfc.super.update(self, dt)
    if input.pressed('reset') then
        self.seed = self.seed + 1
        self:generate()
    end
    if self.pointer.pressed then
        local column, row = self.board:cellAt(self.pointer.worldX, self.pointer.worldY)
        if column then
            self.pins = self.pins + (self.fixed:get(column, row) < 0 and 1 or 0)
            self.fixed:set(column, row, self.pin)
            self:generate()
        end
    end
    self:status(string.format('%d tiles, %d rules and %d pins, %s in %.0f ms', #kTiles, #self.allow, self.pins, self.failed and 'every attempt contradicted' or 'collapsed', self.latency or 0))
end

-- Fills the batch with the tiles of a map, once per map, and counts the share of every tile.
function Wfc:paint(map)
    local batch, board, art = self.batch, self.board, self.art
    local counts = {0, 0, 0, 0, 0, 0}
    local props = {}
    batch:clear()
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local index = map:get(column, row) + 1
            local tile = kTiles[index]
            counts[index] = counts[index] + 1
            art:addCell(batch, tile.terrain, board.left, board.top, kCell, column, row, tile.tint)
            if tile.prop and (column + row * 3) % 2 == 0 then
                props[#props + 1] = {tile.prop, board.left + (column + 0.5) * kCell, board.top + (row + 0.9) * kCell}
            end
        end
    end
    -- The props stand in rows from the top down, so the lower ones cover the ones behind them.
    for _, prop in ipairs(props) do
        local source = art.sources[prop[1]]
        batch:add({x = prop[2], y = prop[3], width = kCell * 1.5, height = kCell * 1.5, pivotX = 0.5, pivotY = 1, source = source})
    end
    for index, tile in ipairs(kTiles) do
        self:set('share' .. index, {text = string.format('%s: weight %g, %d%%', tile.name, kWeights[index], math.floor(counts[index] * 100 / (kColumns * kRows) + 0.5))})
    end
end

function Wfc:draw(area)
    local board = self.board
    graphics2d.drawShape({board.left - 6, board.top - 6, board.width + 12, board.height + 12}, {radius = 10, color = '#FF151A24', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    self.batch:draw({layer = 0})
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            if self.fixed:get(column, row) >= 0 then
                local x, y = board:center(column, row)
                graphics2d.drawShape({x - kCell * 0.42, y - kCell * 0.42, kCell * 0.84, kCell * 0.84}, {kind = 'ellipse', color = '#00000000', borderWidth = 3, borderColor = '#FFFFFFFF', layer = 2})
            end
        end
    end
end

return Wfc
