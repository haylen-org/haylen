-- Sight on a grid and in the open: symmetric shadowcasting over a dungeon of painted tiles under a soft fog of war, a Bresenham line to a guard and a Bresenham circle of the sight radius, and a visibility polygon among wall segments.
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

local FieldOfView = haylen.class('FieldOfView', AlgorithmTest)

FieldOfView.code = [[
local map = spatial2d.newCellGrid(56, 30)
map:set(14, 6, 1)  -- Solid cells block sight.
local visible = spatial2d.fieldOfView(map, 2, 2, 10)
local sight, ring = spatial2d.line(2, 2, 31, 17), spatial2d.circle(2, 2, 10)
local polygon = spatial2d.visibilityPolygon({120, 40}, {{{300, 200}, {300, 400}}}, {-740, -400, 1480, 800})]]

local kColumns, kRows, kCell = 56, 30, 26
local kBounds = {-740, -400, 1480, 800}
-- The fog over cells out of sight: thin over the cells seen before and nearly black over the rest.
local kFog = {visible = '#00000000', seen = '#B00A0D16', hidden = '#F20A0D16'}

function FieldOfView:enter()
    self:frame{
        hint = 'Move the pointer to look around. The grid remembers the cells it saw, and the guard sees you exactly when you see it. The gold squares are the Bresenham circle of the sight radius and the red dots the Bresenham line to the guard. R or the X button builds a new map.',
        code = FieldOfView.code,
        controls = {
            ui.formField{label = 'Sight', ui.radioGroup{id = 'mode', items = {{id = 'grid', text = 'Shadowcasting on a grid'}, {id = 'polygon', text = 'Visibility polygon'}}, selected = 'grid', onChange = function(event) self.mode = event.value end}},
            ui.formField{label = 'Sight radius in cells', ui.slider{id = 'radius', value = 10, min = 2, max = 30, step = 1, showValue = true, decimals = 0, onChange = function(event) self.radius = event.value end}},
            ui.button{id = 'forget', text = 'Forget what was seen', onClick = function()
                self.seen = {}
                self.fogKey = nil
            end},
            ui.button{id = 'reset', text = 'New map', onClick = function() self:build() end},
        },
        focus = 'mode',
    }
    self.mode, self.radius = 'grid', 10
    self.random = m.random(79)
    self.board = Board(kColumns, kRows, kCell)
    self.art = Art()
    self.tiles = graphics2d.newSpriteBatch(self.art.terrain)
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
    self.seen, self.visibleSet, self.sightLine, self.ring = {}, {}, {}, {}
    self.viewer = {2, 2}
    self.fogKey = nil

    -- The dungeon is a floor of stone with walls of brick, painted once for each map.
    local board = self.board
    self.tiles:clear()
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local wall = self.grid:get(column, row) == 1
            self.art:addCell(self.tiles, wall and 'brick' or 'stone', board.left, board.top, kCell, column, row, wall and '#FFC8B8C0' or '#FF9AA0B4')
        end
    end
end

-- Covers the map with fog by what the viewer sees and saw, as a picture of one pixel per cell that blends between the cells, so the edge of sight is soft. It changes only when the cells in sight do.
function FieldOfView:updateFog()
    local key = self.viewer[1] .. ',' .. self.viewer[2] .. ',' .. self.radius
    if key == self.fogKey then
        return
    end
    self.fogKey = key
    self.fog = picture.cells(kColumns, kRows, function(column, row)
        local cell = row * kColumns + column
        return self.visibleSet[cell] and kFog.visible or self.seen[cell] and kFog.seen or kFog.hidden
    end, true)
end

function FieldOfView:update(dt)
    FieldOfView.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.mode == 'grid' then
        local column, row = self.board:cellAt(self:aim(-120, 40))
        if column and self.grid:get(column, row) == 0 then
            self.viewer = {column, row}
        end
        profiler.beginScope('fieldOfView')
        self.visible = spatial2d.fieldOfView(self.grid, self.viewer[1], self.viewer[2], self.radius)
        profiler.endScope()
        self.visibleSet = {}
        for _, cell in ipairs(self.visible) do
            local key = cell.y * kColumns + cell.x
            self.visibleSet[key] = true
            self.seen[key] = true
        end
        self.sightLine = spatial2d.line(self.viewer[1], self.viewer[2], self.guard[1], self.guard[2])
        self.ring = spatial2d.circle(self.viewer[1], self.viewer[2], self.radius)
        self:updateFog()
        local spotted = self.visibleSet[self.guard[2] * kColumns + self.guard[1]]
        self:status(string.format('%d cells are in sight, shadowcast in %s, and the guard %s.', #self.visible, AlgorithmTest.milliseconds(self:timing('fieldOfView')), spotted and 'sees you' or 'does not see you'))
    else
        local x, y = self:aim(-200, -60)
        self.eye = {x, y}
        local bounds = m.rect(kBounds)
        if bounds:contains({x, y}) then
            profiler.beginScope('visibilityPolygon')
            self.polygon = spatial2d.visibilityPolygon({x, y}, self.segments, kBounds)
            profiler.endScope()
        end
        self:status(string.format('The %d walls leave a polygon of %d corners, found in %s.', #self.segments, self.polygon and #self.polygon or 0, AlgorithmTest.milliseconds(self:timing('visibilityPolygon'))))
    end
end

-- The painted dungeon lies under the fog, the circle of the sight radius shows as gold squares and the line to the guard as red dots, and the guard stands in the dark until the viewer sees it.
function FieldOfView:renderGrid()
    local board = self.board
    graphics2d.drawShape({board.left - 6, board.top - 6, board.width + 12, board.height + 12}, {radius = 10, color = '#FF0E1118', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    self.tiles:draw({layer = 0})
    if self.fog then
        board:drawPicture(self.fog, {layer = 5})
    end
    for _, cell in ipairs(self.ring) do
        if board:contains(cell.x, cell.y) then
            graphics2d.drawShape(board:cellRect(cell.x, cell.y, 4), {radius = 5, color = '#30FFC93C', borderWidth = 3, borderColor = '#FFFFC93C', layer = 6})
        end
    end
    for _, cell in ipairs(self.sightLine) do
        local x, y = board:center(cell.x, cell.y)
        graphics2d.drawCircle(x, y, 5, '#FFF0473E', {layer = 7})
    end
    local vx, vy = board:center(self.viewer[1], self.viewer[2])
    local gx, gy = board:center(self.guard[1], self.guard[2])
    local guardLayer = self.visibleSet[self.guard[2] * kColumns + self.guard[1]] and 8 or 4
    Art.shadow(gx, gy + kCell * 0.4, kCell * 1.2, guardLayer - 1)
    self.art:character(gx, gy + kCell * 0.45, kCell * 2.6, 'red', guardLayer, true)
    Art.shadow(vx, vy + kCell * 0.4, kCell * 1.2, 8)
    self.art:character(vx, vy + kCell * 0.45, kCell * 2.6, 'blue', 9)
end

function FieldOfView:renderPolygon()
    graphics2d.drawShape(kBounds, {radius = 10, color = '#FF141920', borderWidth = 3, borderColor = '#FF3A4560'})
    if self.polygon then
        graphics2d.drawPolygon(self.polygon, '#55FFE082', {layer = 1})
    end
    for _, segment in ipairs(self.segments) do
        graphics2d.drawStroke({segment[1][1], segment[1][2], segment[2][1], segment[2][2]}, {width = 6, color = '#FFB4BED2', cap = 'round', layer = 2})
    end
    if self.eye then
        Art.shadow(self.eye[1], self.eye[2] + 12, 30, 3)
        self.art:character(self.eye[1], self.eye[2] + 14, 56, 'blue', 4)
    end
end

function FieldOfView:draw(area)
    if self.mode == 'grid' then
        self:renderGrid()
    else
        self:renderPolygon()
    end
end

return FieldOfView
