-- The paint bucket of `spatial2d.floodFill`, the connected regions of `spatial2d.components` and a union-find that joins islands with bridges, on islands smoothed from noise whose coasts, traced by marching squares, draw a beach and a line of foam.
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
-- Islands smaller than this many cells sink, so no speck of land is left alone in the sea.
local kSmallest = 8

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
    local noise = m.noise(self.random:integer(1, 9999))
    local land = {}
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            land[row * kColumns + column] = noise:fractal(column / 10, row / 10, 3) > 0.15
        end
    end
    -- Two passes of the majority of the nine cells around each cell round the coasts off, and small islands sink.
    for _ = 1, 2 do
        land = FloodFill.smoothed(land)
    end
    self.grid = spatial2d.newCellGrid(kColumns, kRows)
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            self.grid:set(column, row, land[row * kColumns + column] and 1 or 0)
        end
    end
    self:sinkSmallIslands()
    self:traceCoasts()
    self.bridges = {}
    self:analyze()
end

-- Returns the land of a map where each cell takes what most of the nine cells around it are, with the edges of the map counted as water.
function FloodFill.smoothed(land)
    local result = {}
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local count = 0
            for dy = -1, 1 do
                for dx = -1, 1 do
                    local x, y = column + dx, row + dy
                    if x >= 0 and y >= 0 and x < kColumns and y < kRows and land[y * kColumns + x] then
                        count = count + 1
                    end
                end
            end
            result[row * kColumns + column] = count >= 5
        end
    end
    return result
end

function FloodFill:sinkSmallIslands()
    local labels, count = spatial2d.components(self.grid, {background = 0})
    local sizes = {}
    for label = 1, count do
        sizes[label] = 0
    end
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local label = labels:get(column, row)
            if label > 0 then
                sizes[label] = sizes[label] + 1
            end
        end
    end
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local label = labels:get(column, row)
            if label > 0 and sizes[label] < kSmallest then
                self.grid:set(column, row, 0)
            end
        end
    end
end

-- The coasts pass between the centers of land and water cells, and the foam runs a little out at sea.
function FloodFill:traceCoasts()
    local values = {}
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            values[#values + 1] = self.grid:get(column, row) ~= 0 and 1 or 0
        end
    end
    local origin = {self.board.left + kCell / 2, self.board.top + kCell / 2}
    local coasts = m.marchingSquares.trace(values, kColumns, kRows, {threshold = 0.5, spacing = kCell, origin = origin})
    self.coasts, self.foam = {}, {}
    for index, coast in ipairs(coasts) do
        self.coasts[index] = FloodFill.flat(coast)
    end
    for index, line in ipairs(m.polygon.offset(coasts, kCell * 0.5)) do
        self.foam[index] = FloodFill.flat(line)
    end
end

function FloodFill.flat(points)
    local flat = {}
    for _, point in ipairs(points) do
        flat[#flat + 1] = point.x
        flat[#flat + 1] = point.y
    end
    return flat
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
    self:status(string.format('%d islands make %d sets after the bridges, the last fill painted %d cells in %s, and the regions took %s.', self.count, self.sets.setCount, self.filled or 0, AlgorithmTest.milliseconds(self:timing('floodFill')), AlgorithmTest.milliseconds(self:timing('regions'))))
end

function FloodFill:draw(area)
    local board = self.board
    graphics2d.drawShape({board.left - 6, board.top - 6, board.width + 12, board.height + 12}, {radius = 10, color = '#FF151A24', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    graphics2d.pushClip(board:bounds())
    if self.tool == 'bucket' then
        self.terrain:draw({layer = 0})
        for _, line in ipairs(self.foam) do
            graphics2d.drawStroke(line, {width = 5, color = '#C8E8FBFF', closed = true, join = 'round', layer = 1})
        end
        for _, coast in ipairs(self.coasts) do
            graphics2d.drawStroke(coast, {width = kCell * 0.8, color = '#FFE9CF8C', closed = true, join = 'round', layer = 1})
        end
    else
        board:drawPicture(self.picture)
        for _, coast in ipairs(self.coasts) do
            graphics2d.drawStroke(coast, {width = 3, color = '#FF15172A', closed = true, join = 'round', layer = 1})
        end
    end
    graphics2d.popClip()
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
