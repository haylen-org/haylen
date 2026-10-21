-- The paint bucket of `spatial2d.floodFill`, the connected regions of `spatial2d.components` and a union-find that joins islands with bridges.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')
local Board = require('categories.algorithms.board')
local picture = require('categories.algorithms.picture')

local FloodFill = haylen.class('FloodFill', AlgorithmTest)

FloodFill.code = [[
local filled = spatial2d.floodFill(map, 10, 6, {diagonal = false, value = 2})  -- The paint bucket.
local labels, count = spatial2d.components(map, {background = 0})
local sets = spatial2d.newUnionFind(count)
sets:unite(labels:get(3, 4), labels:get(40, 20))  -- A bridge joins two islands.]]

local kColumns, kRows, kCell = 64, 34, 23
-- The bucket paints the painted terrain of the Tiled tests: water around grass islands, and sand, flowers or dirt.
local kTerrains = {[0] = 'water', 'grass', 'sand', 'flowers', 'dirt'}
local kWater = '#FF2A8FB0'

function FloodFill:enter()
    self:frame{
        hint = 'Tap or click with the selected tool. The bucket fills every connected cell of the same color, the regions view numbers each island, and joining islands unites their sets. R or the X button makes new islands.',
        code = FloodFill.code,
        controls = {
            ui.formField{label = 'Tool', ui.radioGroup{id = 'tool', items = {{id = 'bucket', text = 'Paint bucket'}, {id = 'regions', text = 'Show the regions'}, {id = 'join', text = 'Join two islands'}}, selected = 'bucket', onChange = function(event) self:setTool(event.value) end}},
            ui.formField{label = 'What the bucket paints', ui.segmentedControl{id = 'color', items = {{id = '2', text = 'Sand'}, {id = '3', text = 'Flowers'}, {id = '4', text = 'Dirt'}}, selected = '2', onChange = function(event) self.color = tonumber(event.value) end}},
            ui.checkbox{id = 'diagonal', text = 'Corners connect', onChange = function(event)
                self.diagonal = event.checked
                self:analyze()
            end},
            ui.button{id = 'reset', text = 'New islands', onClick = function() self:build() end},
        },
        focus = 'tool',
    }
    self.tool, self.color, self.diagonal = 'bucket', 2, false
    self.random = m.random(83)
    self.board = Board(kColumns, kRows, kCell)
    self.art = Art()
    self.terrain = graphics2d.newSpriteBatch(self.art.terrain)
    self:build()
end

function FloodFill:build()
    self.grid = spatial2d.newCellGrid(kColumns, kRows)
    local noise = m.noise(self.random:integer(1, 9999))
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            self.grid:set(column, row, noise:fractal(column / 10, row / 10, 3) > 0.15 and 1 or 0)
        end
    end
    self.bridges = {}
    self:analyze()
end

-- Numbers the islands and starts a union-find with one set per island.
function FloodFill:analyze()
    profiler.beginScope('regions')
    self.labels, self.count = spatial2d.components(self.grid, {background = 0, diagonal = self.diagonal})
    profiler.endScope()
    self.sets = spatial2d.newUnionFind(self.count)
    self.bridges, self.picked = {}, nil
    self:repaint()
end

-- The bucket shows the terrain in painted tiles, and the regions show every set of islands in a color of its own over the water.
function FloodFill:repaint()
    self.terrain:clear()
    local board = self.board
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            self.art:addCell(self.terrain, kTerrains[self.grid:get(column, row)], board.left, board.top, kCell, column, row)
        end
    end
    self.picture = picture.cells(kColumns, kRows, function(column, row)
        local label = self.labels:get(column, row)
        if label == 0 then
            return kWater
        end
        local root = self.sets:find(label)
        return m.fromHsv((root * 0.618) % 1, 0.55, 0.85):toHex()
    end)
end

-- Painted cells change the regions, so leaving the bucket numbers them again.
function FloodFill:setTool(tool)
    self.tool, self.picked = tool, nil
    if tool ~= 'bucket' and self.painted then
        self.painted = false
        self:analyze()
    else
        self:repaint()
    end
end

function FloodFill:apply(column, row)
    if self.tool == 'bucket' then
        if self.grid:get(column, row) ~= 0 then
            profiler.beginScope('floodFill')
            self.filled = #spatial2d.floodFill(self.grid, column, row, {diagonal = self.diagonal, value = self.color})
            profiler.endScope()
            self.painted = true
            self:repaint()
        end
        return
    end
    local label = self.labels:get(column, row)
    if self.tool ~= 'join' or label == 0 then
        return
    end
    if self.picked == nil then
        self.picked = {label = label, column = column, row = row}
        return
    end
    if self.sets:unite(self.picked.label, label) then
        self.bridges[#self.bridges + 1] = {self.picked.column, self.picked.row, column, row}
    end
    self.picked = nil
    self:repaint()
end

function FloodFill:update(dt)
    FloodFill.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.pointer.pressed then
        local column, row = self.board:cellAt(self.pointer.worldX, self.pointer.worldY)
        if column then
            self:apply(column, row)
        end
    end
    self:status(string.format('%d islands in %d sets after the bridges, the last fill painted %d cells in %.3f ms, regions in %.3f ms', self.count, self.sets.setCount, self.filled or 0, self:timing('floodFill'), self:timing('regions')))
end

function FloodFill:draw(area)
    local board = self.board
    graphics2d.drawShape({board.left - 6, board.top - 6, board.width + 12, board.height + 12}, {radius = 10, color = '#FF151A24', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    if self.tool == 'bucket' then
        self.terrain:draw({layer = 0})
    else
        board:drawPicture(self.picture)
    end
    -- A bridge is a plank road between the two islands it joins.
    for _, bridge in ipairs(self.bridges) do
        local x1, y1 = board:center(bridge[1], bridge[2])
        local x2, y2 = board:center(bridge[3], bridge[4])
        graphics2d.drawStroke({x1, y1, x2, y2}, {width = 14, color = '#FF5A3A20', cap = 'round', layer = 1})
        graphics2d.drawStroke({x1, y1, x2, y2}, {width = 8, color = '#FFC8955A', cap = 'round', dashes = {10, 4}, layer = 2})
    end
    if self.picked then
        local x, y = board:center(self.picked.column, self.picked.row)
        graphics2d.drawShape({x - 16, y - 16, 32, 32}, {kind = 'ellipse', color = '#00000000', borderWidth = 4, borderColor = '#FFFFFFFF', layer = 3})
    end
end

return FloodFill
