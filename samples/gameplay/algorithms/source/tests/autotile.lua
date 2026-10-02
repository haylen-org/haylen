-- Tiles picked from their neighbors while the player paints: 4-bit masks that join the sides, 47-tile blob masks that also round the corners, and a corner Wang set looked up by `procedural2d.autotileWang`.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local procedural2d = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local Board = require('board')
local sample = require('sample')

local Autotile = haylen.class('Autotile', sample.Test)

local kColumns, kRows, kCell = 38, 21, 38
local kWater, kShore, kLand = '#FF1E6091', '#FFE9D8A6', '#FF7CB342'
local kInset = 8

-- A corner Wang set of sixteen tiles, one per mix of water (1) and grass (2) on the four corners, in the `wangId` order of Tiled.
local function cornerSet()
    local tiles = {}
    for id = 0, 15 do
        local topLeft, topRight, bottomRight, bottomLeft = (id & 1) ~= 0, (id & 2) ~= 0, (id & 4) ~= 0, (id & 8) ~= 0
        local function color(grass) return grass and 2 or 1 end
        tiles[#tiles + 1] = {tileId = id, wangId = {0, color(topRight), 0, color(bottomRight), 0, color(bottomLeft), 0, color(topLeft)}}
    end
    return {kind = 'corner', tiles = tiles}
end

function Autotile:enter()
    Autotile.super.enter(self, {
        hint = 'Paint land on the water with the pointer, starting on water to add land and on land to remove it. The Wang mode paints the corners of the cells.',
        controls = {
            ui.radioGroup{id = 'mode', items = {{id = 'mask4', text = '4-bit masks'}, {id = 'blob', text = '47-tile blob masks'}, {id = 'wang', text = 'Corner Wang set'}}, selected = 'blob', onChange = function(event)
                self.mode = event.value
                self:retile()
            end},
            ui.checkbox{id = 'numbers', text = 'Show the tile numbers', onChange = function(event) self.numbers = event.checked end},
            ui.button{id = 'reset', text = 'New island', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'mode',
    })
    self.mode = 'blob'
    self.random = m.random(97)
    self.set = cornerSet()
    self.board = Board.new(kColumns, kRows, kCell)
    self:build()
end

function Autotile:build()
    self.cells = spatial2d.newCellGrid(kColumns, kRows)
    self.corners = spatial2d.newCellGrid(kColumns + 1, kRows + 1, 1)
    local noise = m.noise(self.random:integer(1, 9999))
    for row = 0, kRows do
        for column = 0, kColumns do
            local land = noise:fractal(column / 9, row / 9, 3) > 0.1
            self.corners:set(column, row, land and 2 or 1)
            if column < kColumns and row < kRows then
                self.cells:set(column, row, land and 1 or 0)
            end
        end
    end
    self:retile()
end

function Autotile:retile()
    if self.mode == 'mask4' then
        self.tiles = procedural2d.autotile4(self.cells, 1)
    elseif self.mode == 'blob' then
        self.tiles = procedural2d.autotile8(self.cells, 1)
    else
        self.tiles = procedural2d.autotileWang(self.corners, self.set, 3)
    end
end

-- A stroke adds or removes land by what it starts on, painting cells or, in the Wang mode, the corner nearest to the pointer.
function Autotile:paint()
    local pointer = self.pointer
    if not pointer.down then
        return
    end
    local board = self.board
    if self.mode == 'wang' then
        local column = math.floor((pointer.worldX - board.left) / kCell + 0.5)
        local row = math.floor((pointer.worldY - board.top) / kCell + 0.5)
        if self.corners:contains(column, row) then
            if pointer.pressed then
                self.brush = self.corners:get(column, row) == 1 and 2 or 1
            end
            self.corners:set(column, row, self.brush)
            self:retile()
        end
        return
    end
    board:stroke(pointer, function(column, row, first)
        if first then
            self.brush = self.cells:get(column, row) == 0 and 1 or 0
        end
        self.cells:set(column, row, self.brush)
    end)
    self:retile()
end

function Autotile:exit()
    Autotile.super.exit(self)
    self.cells, self.corners, self.tiles, self.set = nil, nil, nil, nil
end

function Autotile:update(dt)
    Autotile.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self:paint()
    local used = {}
    local count = 0
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local tile = self.tiles:get(column, row)
            if tile >= 0 and not used[tile] then
                used[tile], count = true, count + 1
            end
        end
    end
    self:showStats(string.format('Mode %s\nDifferent tiles in use %d', self.mode, count))
end

-- A land cell of the side masks draws its middle and an arm toward every side whose neighbor is land too.
function Autotile:drawSides(x, y, mask)
    local inner = kCell - kInset * 2
    graphics2d.drawRect({x + kInset, y + kInset, inner, inner}, kLand)
    local arms = {{1, x + kInset, y, inner, kInset}, {2, x + kCell - kInset, y + kInset, kInset, inner}, {4, x + kInset, y + kCell - kInset, inner, kInset}, {8, x, y + kInset, kInset, inner}}
    for _, arm in ipairs(arms) do
        if mask & arm[1] ~= 0 then
            graphics2d.drawRect({arm[2], arm[3], arm[4], arm[5]}, kLand)
        end
    end
end

-- A blob cell also fills a corner when both of its sides and the diagonal between them are land.
function Autotile:drawBlob(x, y, column, row)
    local mask = procedural2d.mask8(self.cells, column, row)
    local sides = (mask & 1 ~= 0 and 1 or 0) | (mask & 4 ~= 0 and 2 or 0) | (mask & 16 ~= 0 and 4 or 0) | (mask & 64 ~= 0 and 8 or 0)
    self:drawSides(x, y, sides)
    local corners = {{2, x + kCell - kInset, y}, {8, x + kCell - kInset, y + kCell - kInset}, {32, x, y + kCell - kInset}, {128, x, y}}
    for _, corner in ipairs(corners) do
        if mask & corner[1] ~= 0 then
            graphics2d.drawRect({corner[2], corner[3], kInset, kInset}, kLand)
        end
    end
end

-- A Wang tile draws its four corners in the colors its `wangId` gives them.
function Autotile:drawWang(x, y, tile)
    local half = kCell / 2
    local quarters = {{tile & 1, x, y}, {tile & 2, x + half, y}, {tile & 4, x + half, y + half}, {tile & 8, x, y + half}}
    for _, quarter in ipairs(quarters) do
        if quarter[1] ~= 0 then
            graphics2d.drawRect({quarter[2], quarter[3], half, half}, kLand)
        end
    end
end

function Autotile:render()
    self:beginWorld()
    local board = self.board
    graphics2d.drawRect(board:bounds(), kWater)
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local tile = self.tiles:get(column, row)
            local x, y = board.left + column * kCell, board.top + row * kCell
            if tile >= 0 then
                -- Masked cells start as shore, so the sides and corners the mask leaves out show as a beach.
                if self.mode ~= 'wang' then
                    graphics2d.drawRect({x, y, kCell, kCell}, kShore)
                end
                if self.mode == 'mask4' then
                    self:drawSides(x, y, tile)
                elseif self.mode == 'blob' then
                    self:drawBlob(x, y, column, row)
                else
                    self:drawWang(x, y, tile)
                end
                if self.numbers then
                    graphics2d.drawText(nil, tostring(tile), x + kCell / 2, y + kCell / 2, {size = 16, anchor = {0.5, 0.5}, color = '#FF1B1E2B', layer = 1})
                end
            end
        end
    end
    board:drawLines('#18FFFFFF', {layer = 2})
end

return Autotile
