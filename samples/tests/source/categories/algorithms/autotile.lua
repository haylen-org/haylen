-- Tiles picked from their neighbors while the player paints: 4-bit masks that join the sides, 47-tile blob masks that also round the corners, and a corner Wang set looked up by `procedural2d.autotileWang`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local procedural2d = require('haylen.procedural2d')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')
local Board = require('categories.algorithms.board')

local Autotile = haylen.class('Autotile', AlgorithmTest)

Autotile.code = [[
local sides = procedural2d.autotile4(cells, 1)  -- The 4-bit mask of every cell that holds 1.
local blobs = procedural2d.autotile8(cells, 1)  -- One of the 47 blob tiles, or -1.
local set = {kind = 'corner', tiles = {{tileId = 5, wangId = {0, 1, 0, 2, 0, 1, 0, 2}}}}  -- Or a set of Tiled.
local tiles = procedural2d.autotileWang(corners, set, 3)  -- Corners sit on a grid one cell wider and taller.]]

local kColumns, kRows, kCell = 38, 21, 38
-- The shapes of the tiles draw in white into the masks of the painted beach and grass.
local kMask = '#FFFFFFFF'
local kInset = 8
-- The blob tiles round an open corner of the land by this radius.
local kRound = 16

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
    self:frame{
        hint = 'Paint land on the water with the pointer, starting on water to add land and on land to remove it. The Wang mode paints the corners of the cells. R or the X button raises a new island.',
        code = Autotile.code,
        controls = {
            ui.formField{label = 'Tiles', ui.radioGroup{id = 'mode', items = {{id = 'mask4', text = '4-bit masks'}, {id = 'blob', text = '47-tile blob masks'}, {id = 'wang', text = 'Corner Wang set'}}, selected = 'blob', onChange = function(event)
                self.mode = event.value
                self:retile()
            end}},
            ui.checkbox{id = 'numbers', text = 'Show the tile numbers', checked = true, onChange = function(event) self.numbers = event.checked end},
            ui.label{text = 'The 4-bit masks join the sides only, so corners stay square. The blob masks also read the diagonals, so open corners round off and inner corners keep a beach.', color = 'textMuted', font = 'caption'},
            ui.button{id = 'reset', text = 'New island', onClick = function() self:build() end},
        },
        focus = 'mode',
    }
    self.mode, self.numbers = 'blob', true
    self.random = m.random(97)
    self.set = cornerSet()
    self.board = Board(kColumns, kRows, kCell)
    self.art = Art()
    -- Each terrain covers the whole board once, and the masks show the parts of it the tiles pick.
    self.painted = {}
    for _, terrain in ipairs({'water', 'sand', 'grass'}) do
        local batch = graphics2d.newSpriteBatch(self.art.terrain)
        for row = 0, kRows - 1 do
            for column = 0, kColumns - 1 do
                self.art:addCell(batch, terrain, self.board.left, self.board.top, kCell, column, row)
            end
        end
        self.painted[terrain] = batch
    end
    self.maskers = {}
    for _, part in ipairs({'shore', 'land', 'notch'}) do
        self.maskers[part] = function() self:drawPart(part) end
    end
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
    self:status(string.format('The mode "%s" uses %d different tiles.', self.mode, count))
end

-- The shapes of a tile, drawn in white into the masks that the painted sand and grass show through: `shore` is the beach under the grass, `land` the grass and `notch` the curves of beach in the inner corners that the blob tiles keep.
function Autotile:drawShapes(part, x, y, column, row, tile)
    if self.mode == 'mask4' then
        self:drawSides(part, x, y, tile)
    elseif self.mode == 'blob' then
        self:drawBlob(part, x, y, column, row)
    elseif part == 'land' then
        self:drawWang(x, y, tile)
    end
end

-- A land cell of the side masks is all beach, with a middle of grass and an arm toward every side whose neighbor is land too.
function Autotile:drawSides(part, x, y, mask)
    if part == 'shore' then
        graphics2d.drawRect({x, y, kCell, kCell}, kMask)
        return
    end
    if part ~= 'land' then
        return
    end
    local inner = kCell - kInset * 2
    graphics2d.drawRect({x + kInset, y + kInset, inner, inner}, kMask)
    local arms = {{1, x + kInset, y, inner, kInset}, {2, x + kCell - kInset, y + kInset, kInset, inner}, {4, x + kInset, y + kCell - kInset, inner, kInset}, {8, x, y + kInset, kInset, inner}}
    for _, arm in ipairs(arms) do
        if mask & arm[1] ~= 0 then
            graphics2d.drawRect({arm[2], arm[3], arm[4], arm[5]}, kMask)
        end
    end
end

-- A blob cell is a beach under the grass, rounded at every corner whose two sides are open, and a land cell whose diagonal is water keeps a curve of beach in that inner corner.
function Autotile:drawBlob(part, x, y, column, row)
    local mask = procedural2d.mask8(self.cells, column, row)
    local north, east, south, west = mask & 1 ~= 0, mask & 4 ~= 0, mask & 16 ~= 0, mask & 64 ~= 0
    local radii = {not north and not west and kRound or 0, not north and not east and kRound or 0, not south and not east and kRound or 0, not south and not west and kRound or 0}
    if part == 'shore' then
        graphics2d.drawShape({x, y, kCell, kCell}, {radius = radii, color = kMask})
    elseif part == 'land' then
        local left, top = west and 0 or kInset, north and 0 or kInset
        local right, bottom = east and 0 or kInset, south and 0 or kInset
        local inner = {}
        for index, radius in ipairs(radii) do
            inner[index] = radius > 0 and radius - kInset or 0
        end
        graphics2d.drawShape({x + left, y + top, kCell - left - right, kCell - top - bottom}, {radius = inner, color = kMask})
    else
        local notches = {{north and east and mask & 2 == 0, x + kCell, y}, {south and east and mask & 8 == 0, x + kCell, y + kCell}, {south and west and mask & 32 == 0, x, y + kCell}, {north and west and mask & 128 == 0, x, y}}
        for _, notch in ipairs(notches) do
            if notch[1] then
                graphics2d.drawShape({notch[2] - kInset, notch[3] - kInset, kInset * 2, kInset * 2}, {kind = 'ellipse', color = kMask})
            end
        end
    end
end

-- A Wang tile draws its four corners as grass where its `wangId` gives them grass.
function Autotile:drawWang(x, y, tile)
    local half = kCell / 2
    local quarters = {{tile & 1, x, y}, {tile & 2, x + half, y}, {tile & 4, x + half, y + half}, {tile & 8, x, y + half}}
    for _, quarter in ipairs(quarters) do
        if quarter[1] ~= 0 then
            graphics2d.drawRect({quarter[2], quarter[3], half, half}, kMask)
        end
    end
end

-- Draws the shapes of one part for every tile that has one, inside the function of a mask.
function Autotile:drawPart(part)
    local board = self.board
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local tile = self.tiles:get(column, row)
            if tile >= 0 then
                self:drawShapes(part, board.left + column * kCell, board.top + row * kCell, column, row, tile)
            end
        end
    end
end

-- The sea, the beach and the grass are the painted tiles of the Tiled tests, and the tiles the masks pick show the beach and the grass through their shapes, with the number of every tile on it.
function Autotile:draw(area)
    local board = self.board
    graphics2d.drawShape({board.left - 6, board.top - 6, board.width + 12, board.height + 12}, {radius = 10, color = '#FF151A24', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    self.painted.water:draw({layer = 0})
    for index, part in ipairs({'shore', 'land', 'notch'}) do
        graphics2d.pushMask(self.maskers[part], {layer = index})
        self.painted[part == 'land' and 'grass' or 'sand']:draw()
        graphics2d.popMask()
    end
    if not self.numbers then
        return
    end
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local tile = self.tiles:get(column, row)
            if tile >= 0 then
                local x, y = board.left + column * kCell, board.top + row * kCell
                graphics2d.drawShape({x + 0.5, y + 0.5, kCell - 1, kCell - 1}, {radius = 4, color = '#00000000', borderWidth = 1.5, borderColor = '#30203018', layer = 4})
                graphics2d.drawText(nil, tostring(tile), x + kCell / 2, y + kCell / 2, {size = 17, anchor = {0.5, 0.5}, color = '#FF1E3A1A', layer = 5})
            end
        end
    end
end

return Autotile
