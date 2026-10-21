-- Marching squares tracing the outlines of a height field you raise and lower, or of the bitmap of its cells above the threshold, simplified with Ramer-Douglas-Peucker.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Board = require('categories.algorithms.board')
local picture = require('categories.algorithms.picture')

local MarchingSquares = haylen.class('MarchingSquares', AlgorithmTest)

MarchingSquares.code = [[
local outlines = m.marchingSquares.trace(heights, 80, 44, {threshold = 0.5, spacing = 18})
local solid = m.marchingSquares.traceBitmap(pixels, 80, 44, {spacing = 18})  -- Pixels that are not zero are solid.
local simpler = m.polygon.simplify(outlines[1], 4)  -- Ramer-Douglas-Peucker.]]

local kColumns, kRows, kSpacing = 80, 44, 18
local kBrush = 3.5

function MarchingSquares:enter()
    self:frame{
        hint = 'Hold on the field to raise it, or to lower it with the other brush. The outlines follow the threshold between the samples. R or the X button makes a new field.',
        code = MarchingSquares.code,
        controls = {
            ui.formField{label = 'Brush', ui.segmentedControl{id = 'brush', items = {{id = 'raise', text = 'Raise'}, {id = 'lower', text = 'Lower'}}, selected = 'raise', onChange = function(event) self.brush = event.value end}},
            ui.formField{label = 'What to trace', ui.radioGroup{id = 'source', items = {{id = 'field', text = 'Trace the field'}, {id = 'bitmap', text = 'Trace the bitmap'}}, selected = 'field', onChange = function(event) self:change('source', event.value) end}},
            ui.formField{label = 'Threshold', ui.slider{id = 'threshold', value = 0.5, min = 0.2, max = 0.8, step = 0.02, showValue = true, onChange = function(event) self:change('threshold', event.value) end}},
            ui.formField{label = 'Simplify tolerance', ui.slider{id = 'tolerance', value = 0, min = 0, max = 12, step = 0.5, showValue = true, decimals = 1, onChange = function(event) self:change('tolerance', event.value) end}},
            ui.button{id = 'reset', text = 'New field', onClick = function() self:build() end},
        },
        focus = 'brush',
    }
    self.brush = 'raise'
    self.options = {source = 'field', threshold = 0.5, tolerance = 0}
    self.random = m.random(101)
    self.board = Board(kColumns, kRows, kSpacing)
    self:build()
end

function MarchingSquares:build()
    local noise = m.noise(self.random:integer(1, 9999))
    self.values = {}
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            self.values[row * kColumns + column + 1] = m.saturate(0.5 + noise:fractal(column / 16, row / 16, 4) * 0.8)
        end
    end
    self:trace()
end

function MarchingSquares:change(name, value)
    self.options[name] = value
    self:trace()
end

-- Samples sit on the corners of the board cells, so the outlines line up with the picture of the field.
function MarchingSquares:trace()
    local options = self.options
    local origin = {self.board.left + kSpacing / 2, self.board.top + kSpacing / 2}
    profiler.beginScope('marchingSquares')
    local outlines
    if options.source == 'field' then
        outlines = m.marchingSquares.trace(self.values, kColumns, kRows, {threshold = options.threshold, spacing = kSpacing, origin = origin})
    else
        local pixels = {}
        for index, value in ipairs(self.values) do
            pixels[index] = value >= options.threshold and 1 or 0
        end
        outlines = m.marchingSquares.traceBitmap(pixels, kColumns, kRows, {spacing = kSpacing, origin = {self.board.left, self.board.top}})
    end
    if options.tolerance > 0 then
        for index, outline in ipairs(outlines) do
            outlines[index] = m.polygon.simplify(outline, options.tolerance)
        end
    end
    profiler.endScope()
    self.outlines = outlines
    self.points = 0
    for _, outline in ipairs(outlines) do
        self.points = self.points + #outline
    end
    self.picture = picture.cells(kColumns, kRows, function(column, row)
        local value = self.values[row * kColumns + column + 1]
        return m.color(0.1 + value * 0.25, 0.15 + value * 0.35, 0.2 + value * 0.25):toHex()
    end, true)
end

-- The brush raises or lowers the samples around the pointer, most at its center.
function MarchingSquares:paint(dt)
    local column, row = self.board:cellAt(self.pointer.worldX, self.pointer.worldY)
    if column == nil then
        return
    end
    local sign = self.brush == 'raise' and 1 or -1
    for y = math.max(0, row - 4), math.min(kRows - 1, row + 4) do
        for x = math.max(0, column - 4), math.min(kColumns - 1, column + 4) do
            local distance = math.sqrt((x - column) ^ 2 + (y - row) ^ 2)
            if distance < kBrush then
                local index = y * kColumns + x + 1
                self.values[index] = m.saturate(self.values[index] + sign * (1 - distance / kBrush) * 2.5 * dt)
            end
        end
    end
    self:trace()
end

function MarchingSquares:update(dt)
    MarchingSquares.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.pointer.down then
        self:paint(dt)
    end
    self:status(string.format('%d samples traced into %d outlines of %d points in %.3f ms', kColumns * kRows, #self.outlines, self.points, self:timing('marchingSquares')))
end

function MarchingSquares:draw(area)
    self.board:drawPicture(self.picture)
    for _, outline in ipairs(self.outlines) do
        local points = {}
        for _, point in ipairs(outline) do
            points[#points + 1] = point.x or point[1]
            points[#points + 1] = point.y or point[2]
        end
        if #points >= 4 then
            graphics2d.drawStroke(points, {width = 4, color = m.polygonSignedArea(outline) >= 0 and '#FFFFC93C' or '#FF2FB3C4', closed = true, join = 'round', layer = 1})
        end
    end
end

return MarchingSquares
