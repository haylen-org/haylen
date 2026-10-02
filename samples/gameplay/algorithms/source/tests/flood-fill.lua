-- The paint bucket of `spatial2d.floodFill`, the connected regions of `spatial2d.components` and a union-find that joins islands with bridges.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local Board = require('board')
local picture = require('picture')
local sample = require('sample')

local FloodFill = haylen.class('FloodFill', sample.Test)

local kColumns, kRows, kCell = 64, 34, 23
local kColors = {[0] = '#FF1E3A5F', '#FF7CB342', '#FFFFB74D', '#FFE57373', '#FFBA68C8', '#FF4DD0E1'}

function FloodFill:enter()
    FloodFill.super.enter(self, {
        hint = 'Tap or click with the selected tool. The bucket fills every connected cell of the same color, the regions view numbers each island, and joining islands unites their sets.',
        controls = {
            ui.radioGroup{id = 'tool', items = {{id = 'bucket', text = 'Paint bucket'}, {id = 'regions', text = 'Show the regions'}, {id = 'join', text = 'Join two islands'}}, selected = 'bucket', onChange = function(event) self:setTool(event.value) end},
            ui.radioGroup{id = 'color', horizontal = true, items = {{id = '2', text = 'Sand'}, {id = '3', text = 'Clay'}, {id = '4', text = 'Rose'}}, selected = '2', onChange = function(event) self.color = tonumber(event.value) end},
            ui.checkbox{id = 'diagonal', text = 'Corners connect', onChange = function(event)
                self.diagonal = event.checked
                self:analyze()
            end},
            ui.button{id = 'reset', text = 'New islands', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'tool',
    })
    self.tool, self.color, self.diagonal = 'bucket', 2, false
    self.random = m.random(83)
    self.board = Board.new(kColumns, kRows, kCell)
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

function FloodFill:repaint()
    self.picture = picture.cells(kColumns, kRows, function(column, row)
        if self.tool == 'bucket' then
            return kColors[self.grid:get(column, row)]
        end
        local label = self.labels:get(column, row)
        if label == 0 then
            return kColors[0]
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
            profiler.beginScope('flood fill')
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

function FloodFill:exit()
    FloodFill.super.exit(self)
    self.grid, self.labels, self.sets, self.picture = nil, nil, nil, nil
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
    self:showStats(string.format('Islands %d\nSets after bridges %d\nLast fill %d cells\nFill %.3f ms\nRegions %.3f ms', self.count, self.sets.setCount, self.filled or 0, self:timing('flood fill'), self:timing('regions')))
end

function FloodFill:render()
    self:beginWorld()
    local board = self.board
    board:drawPicture(self.picture)
    for _, bridge in ipairs(self.bridges) do
        local x1, y1 = board:center(bridge[1], bridge[2])
        local x2, y2 = board:center(bridge[3], bridge[4])
        graphics2d.drawLine(x1, y1, x2, y2, 6, '#FFFFFFFF', {layer = 1})
    end
    if self.picked then
        local x, y = board:center(self.picked.column, self.picked.row)
        graphics2d.drawRing(x, y, 14, 4, '#FFFFFFFF', {layer = 2})
    end
end

return FloodFill
