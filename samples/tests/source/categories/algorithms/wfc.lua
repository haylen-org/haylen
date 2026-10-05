-- Wave Function Collapse on a worker thread: a terrain of six tiles where each tile only touches its neighbors in the chain from deep water to mountains, with tiles the player pins in place.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local procedural2d = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Board = require('categories.algorithms.board')
local picture = require('categories.algorithms.picture')

local Wfc = haylen.class('Wfc', AlgorithmTest)

local kColumns, kRows, kCell = 64, 34, 23
local kTiles = {
    {name = 'Deep water', color = '#FF0D3B66'},
    {name = 'Water', color = '#FF1E6091'},
    {name = 'Sand', color = '#FFE9D8A6'},
    {name = 'Grass', color = '#FF7CB342'},
    {name = 'Forest', color = '#FF2E7D32'},
    {name = 'Mountain', color = '#FF8D8D8D'},
}
local kWeights = {2, 3, 1.5, 4, 3, 1.5}

-- Each tile touches itself and the tiles right before and after it in the chain, on every side.
local function rules()
    local allow = {}
    for tile = 0, #kTiles - 1 do
        for _, other in ipairs({tile, tile + 1}) do
            if other < #kTiles then
                allow[#allow + 1] = {tile, other, 'right'}
                allow[#allow + 1] = {tile, other, 'down'}
            end
        end
    end
    return allow
end

function Wfc:enter()
    local items = {}
    for index, tile in ipairs(kTiles) do
        items[index] = {id = tostring(index - 1), text = tile.name}
    end
    self:frame{
        hint = 'Tap or click cells to pin the selected tile there, then the map collapses again around every pin. Mountains never touch water. R or the X button takes a new seed.',
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
        },
        focus = 'tile',
    }
    self.pin, self.periodic, self.seed, self.pins = 5, false, 1, 0
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
            self.picture = picture.cells(kColumns, kRows, function(column, row)
                return kTiles[map:get(column, row) + 1].color
            end)
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
    self:status(string.format('Tiles %d, rules %d, pins %d, %s, ready in %.0f ms', #kTiles, #self.allow, self.pins, self.failed and 'every attempt contradicted' or 'collapsed', self.latency or 0))
end

function Wfc:draw(area)
    local board = self.board
    if self.picture then
        board:drawPicture(self.picture)
    end
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local tile = self.fixed:get(column, row)
            if tile >= 0 then
                local x, y = board:center(column, row)
                graphics2d.drawRing(x, y, kCell * 0.35, 3, '#FFFFFFFF', {layer = 1})
            end
        end
    end
end

return Wfc
