-- Sight on a grid and in the open: symmetric shadowcasting with fog of war, a Bresenham line to a guard and a Bresenham circle of the sight radius, and a visibility polygon among wall segments.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local spatial2d = require('haylen.spatial2d')
local ui = require('haylen.ui')

local Board = require('board')
local sample = require('sample')

local FieldOfView = haylen.class('FieldOfView', sample.Test)

local kColumns, kRows, kCell = 56, 30, 26
local kBounds = {-740, -400, 1480, 800}

function FieldOfView:enter()
    FieldOfView.super.enter(self, {
        hint = 'Move the pointer to look around. The grid remembers the cells it saw, and the guard sees you exactly when you see it.',
        controls = {
            ui.radioGroup{id = 'mode', items = {{id = 'grid', text = 'Shadowcasting on a grid'}, {id = 'polygon', text = 'Visibility polygon'}}, selected = 'grid', onChange = function(event) self.mode = event.value end},
            ui.label{text = 'Sight radius in cells', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'radius', value = 10, min = 2, max = 30, step = 1, showValue = true, decimals = 0, onChange = function(event) self.radius = event.value end},
            ui.button{id = 'forget', text = 'Forget what was seen', onClick = function() self.seen = {} end},
            ui.button{id = 'reset', text = 'New map', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'mode',
    })
    self.mode, self.radius = 'grid', 10
    self.random = m.random(79)
    self.board = Board.new(kColumns, kRows, kCell)
    self:build()
end

-- Rooms of walls with doorways and scattered pillars, and the same rooms as segments for the visibility polygon.
function FieldOfView:build()
    self.grid = spatial2d.newCellGrid(kColumns, kRows)
    self.segments = {}
    for column = 0, kColumns - 1, 14 do
        for row = 0, kRows - 1 do
            if row % 10 ~= 4 and row % 10 ~= 5 then
                self.grid:set(column, row, 1)
            end
        end
    end
    for row = 0, kRows - 1, 10 do
        for column = 0, kColumns - 1 do
            if column % 14 ~= 7 then
                self.grid:set(column, row, 1)
            end
        end
    end
    for _ = 1, 60 do
        self.grid:set(self.random:integer(1, kColumns - 2), self.random:integer(1, kRows - 2), 1)
    end
    for _ = 1, 22 do
        local x, y = self.random:range(-700, 600), self.random:range(-380, 300)
        local width, height = self.random:range(20, 160), self.random:range(20, 120)
        local corners = {{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}}
        for index = 1, 4 do
            self.segments[#self.segments + 1] = {corners[index], corners[index % 4 + 1]}
        end
    end
    self.guard = {kColumns // 2 + 3, kRows // 2 + 2}
    self.grid:set(self.guard[1], self.guard[2], 0)
    self.seen, self.visibleSet, self.line, self.ring = {}, {}, {}, {}
    self.viewer = {2, 2}
end

function FieldOfView:exit()
    FieldOfView.super.exit(self)
    self.grid, self.segments, self.seen, self.visible = nil, nil, nil, nil
end

function FieldOfView:update(dt)
    FieldOfView.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.mode == 'grid' then
        local column, row = self.board:cellAt(self.pointer.worldX, self.pointer.worldY)
        if column and self.grid:get(column, row) == 0 then
            self.viewer = {column, row}
        end
        profiler.beginScope('field of view')
        self.visible = spatial2d.fieldOfView(self.grid, self.viewer[1], self.viewer[2], self.radius)
        profiler.endScope()
        self.visibleSet = {}
        for _, cell in ipairs(self.visible) do
            local key = cell.y * kColumns + cell.x
            self.visibleSet[key] = true
            self.seen[key] = true
        end
        self.line = spatial2d.line(self.viewer[1], self.viewer[2], self.guard[1], self.guard[2])
        self.ring = spatial2d.circle(self.viewer[1], self.viewer[2], self.radius)
        local spotted = self.visibleSet[self.guard[2] * kColumns + self.guard[1]]
        self:showStats(string.format('visible cells %d\nshadowcasting %.3f ms\nthe guard %s', #self.visible, self:timing('field of view'), spotted and 'sees you' or 'does not see you'))
    else
        local x, y = self.pointer.worldX, self.pointer.worldY
        local bounds = m.rect(kBounds)
        if bounds:contains({x, y}) then
            profiler.beginScope('visibility polygon')
            self.polygon = spatial2d.visibilityPolygon({x, y}, self.segments, kBounds)
            profiler.endScope()
        end
        self:showStats(string.format('walls %d\npolygon corners %d\nvisibility %.3f ms', #self.segments, self.polygon and #self.polygon or 0, self:timing('visibility polygon')))
    end
end

function FieldOfView:renderGrid()
    local board = self.board
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local key = row * kColumns + column
            local wall = self.grid:get(column, row) == 1
            local color
            if self.visibleSet[key] then
                color = wall and '#FFB0BEC5' or '#FF5D6B3F'
            elseif self.seen[key] then
                color = wall and '#FF4A545E' or '#FF2A3138'
            end
            if color then
                graphics2d.drawRect(board:cellRect(column, row), color)
            end
        end
    end
    for _, cell in ipairs(self.ring) do
        if board:contains(cell.x, cell.y) then
            graphics2d.drawRectOutline(board:cellRect(cell.x, cell.y, 4), 2, '#66FFD54F', {layer = 1})
        end
    end
    for _, cell in ipairs(self.line) do
        graphics2d.drawRect(board:cellRect(cell.x, cell.y, 9), '#AAEF5350', {layer = 2})
    end
    local vx, vy = board:center(self.viewer[1], self.viewer[2])
    local gx, gy = board:center(self.guard[1], self.guard[2])
    graphics2d.drawCircle(vx, vy, kCell * 0.4, '#FFFFFFFF', {layer = 3})
    graphics2d.drawCircle(gx, gy, kCell * 0.4, '#FFEF5350', {layer = 3})
end

function FieldOfView:renderPolygon()
    graphics2d.drawRect(kBounds, '#FF141920')
    if self.polygon then
        graphics2d.drawPolygon(self.polygon, '#55FFE082', {layer = 1})
        graphics2d.drawCircle(self.pointer.worldX, self.pointer.worldY, 8, '#FFFFFFFF', {layer = 3})
    end
    for _, segment in ipairs(self.segments) do
        graphics2d.drawLine(segment[1][1], segment[1][2], segment[2][1], segment[2][2], 4, '#FFB0BEC5', {layer = 2})
    end
end

function FieldOfView:render()
    self:beginWorld()
    if self.mode == 'grid' then
        self:renderGrid()
    else
        self:renderPolygon()
    end
end

return FieldOfView
