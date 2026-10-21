-- A wooden board of steel pegs where balls pour from a funnel and bounce down into slots, whose sensors count every ball that arrives and feed the histogram on the board beside it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local PegBoard = haylen.class('PegBoard', PhysicsTest)

PegBoard.code = [[
board:addCircle(8, {offsetX = x, offsetY = y, restitution = 0.4})  -- One static body holds every peg.
funnel:addSegment(-480, -340, -360, -270)  -- Pours every ball into the middle.
slot:addBox(40, 16, {sensor = true})
slot.data = {slot = 3}
world.onSensorBegin = function(sensor, visitor) counts[sensor.data.slot] = counts[sensor.data.slot] + 1 end]]

PegBoard.center = -330
PegBoard.spacing = 48
PegBoard.rows = 10
PegBoard.slots = 12
PegBoard.top = -200
PegBoard.pegRadius = 8
PegBoard.ballRadius = 10
PegBoard.floor = 340
PegBoard.ground = 410
PegBoard.rainInterval = 0.15
PegBoard.maxBalls = 120
PegBoard.warmUp = 6
-- The half width of the mouth of the funnel and of its outlet, and the heights of its rim and its outlet above the first row of pegs.
PegBoard.funnel = {mouth = 150, outlet = 30, rim = 140, bottom = 70}
PegBoard.chart = {left = 60, right = 620, top = -330, bottom = 330, gap = 8, height = 470}
PegBoard.bar = Look.tint('teal', '#FF2FB3C4', '#FF1E8494', '#FF9BE9F2')
PegBoard.ballMaterial = Look.tint('red', '#FFF0473E', '#FFB9302A', '#FFFF9A92')

function PegBoard:enter()
    self:frame{
        code = PegBoard.code,
        hint = 'Balls pour through the funnel. Click or tap over the board to drop one where you point. R or the X button clears the counts.',
        controls = {
            ui.toggle{id = 'rain', text = 'Pour balls', checked = true, onChange = function(event) self.raining = event.checked end},
            ui.button{id = 'drop', text = 'Drop a ball', onClick = function() self:pour() end},
            ui.button{id = 'burst', text = 'Pour 50 balls', onClick = function() self.queued = self.queued + 50 end},
            ui.button{id = 'reset', text = 'Clear the counts', onClick = function() self:build() end},
        },
        focus = 'rain',
        view = {1960, 870},
    }
    self.random = m.random(53)
    self.raining = true
    self:build()

    -- The board opens after a few seconds of pouring, so the histogram already shows the slots the balls favor.
    for _ = 1, PegBoard.warmUp * 60 do
        self:fixedUpdate(1 / 60)
    end
end

function PegBoard:build()
    self.world = physics2d.newWorld()
    local center, spacing, half = PegBoard.center, PegBoard.spacing, PegBoard.slots / 2 * PegBoard.spacing
    local board = self.world:createBody({type = 'static'})
    for row = 0, PegBoard.rows - 1 do
        local count = row % 2 == 0 and PegBoard.slots or PegBoard.slots - 1
        for index = 0, count - 1 do
            board:addCircle(PegBoard.pegRadius, {offsetX = center + (index - (count - 1) / 2) * spacing, offsetY = PegBoard.top + row * spacing, restitution = 0.4})
        end
    end
    Look.dress(board, 'steel')
    self.board = board

    -- The walls, the dividers of the slots and the funnel are steel too, but drawn on their own as plates and rails.
    local frame = self.world:createBody({type = 'static'})
    local wallTop = PegBoard.top - PegBoard.funnel.bottom
    for index = 0, PegBoard.slots do
        frame:addBox(6, 70, {offsetX = center + (index - PegBoard.slots / 2) * spacing, offsetY = PegBoard.floor - 35})
    end
    frame:addBox(16, PegBoard.floor - wallTop, {offsetX = center - half - 8, offsetY = (wallTop + PegBoard.floor) / 2})
    frame:addBox(16, PegBoard.floor - wallTop, {offsetX = center + half + 8, offsetY = (wallTop + PegBoard.floor) / 2})
    frame:addBox(half * 2 + 32, 40, {offsetX = center, offsetY = PegBoard.floor + 20})
    local funnel = PegBoard.funnel
    for _, side in ipairs({-1, 1}) do
        frame:addSegment(center + side * funnel.mouth, PegBoard.top - funnel.rim, center + side * funnel.outlet, PegBoard.top - funnel.bottom)
    end

    for slot = 1, PegBoard.slots do
        local sensor = self.world:createBody({type = 'static', x = center + (slot - 0.5 - PegBoard.slots / 2) * spacing, y = PegBoard.floor - 20})
        sensor:addBox(spacing - 8, 16, {sensor = true})
        sensor.data = {slot = slot}
    end
    self.counts = {}
    for slot = 1, PegBoard.slots do
        self.counts[slot] = 0
    end
    self.balls, self.dropped, self.counted, self.clock, self.queued = {}, 0, 0, 0, 0

    self.world.onSensorBegin = function(sensor, visitor, shapes)
        if visitor.data.ball then
            self.counts[sensor.data.slot] = self.counts[sensor.data.slot] + 1
            self.counted = self.counted + 1
            visitor.data.ball = false
        end
    end
end

-- Drops a ball at `x` and `y`, or into the mouth of the funnel.
function PegBoard:pour(x, y)
    local ball = self.world:createBody({x = x or PegBoard.center + self.random:range(-90, 90), y = y or PegBoard.top - PegBoard.funnel.rim - 20})
    ball:addCircle(PegBoard.ballRadius, {restitution = 0.4, friction = 0.1})
    Look.dress(ball, PegBoard.ballMaterial)
    ball.data.ball = true
    self.balls[#self.balls + 1] = ball
    self.dropped = self.dropped + 1
    if #self.balls > PegBoard.maxBalls then
        table.remove(self.balls, 1):destroy()
    end
end

function PegBoard:update(dt)
    PegBoard.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer = self.pointer
    local half = PegBoard.slots / 2 * PegBoard.spacing - PegBoard.ballRadius - 2
    local x, y = pointer.worldX, pointer.worldY
    if pointer.pressed and not ui.usingPointer() and y < PegBoard.top + 150 and math.abs(x - PegBoard.center) < half then
        self:pour(x, math.min(y, PegBoard.top - 30))
    end
    local most, slot = 0, 1
    for index, count in ipairs(self.counts) do
        if count > most then
            most, slot = count, index
        end
    end
    local busiest = most > 0 and string.format(', and slot %d holds the most with %d', slot, most) or ''
    self:status(string.format('%d balls poured and %d counted in the slots%s.', self.dropped, self.counted, busiest))
end

-- Balls that a slot counted settle in it for a moment and are then taken off the board.
function PegBoard:fixedUpdate(step)
    self.clock = self.clock + step
    if self.clock >= PegBoard.rainInterval then
        self.clock = 0
        if self.queued > 0 then
            self.queued = self.queued - 1
            self:pour()
        elseif self.raining then
            self:pour()
        end
    end
    self:simulate(self.world, step)
    for index = #self.balls, 1, -1 do
        local ball = self.balls[index]
        if not ball.data.ball and ball.velocity:length() < 20 then
            table.remove(self.balls, index):destroy()
        end
    end
end

-- The board is a dark panel in a wooden frame on two legs, with steel walls, slot dividers and the funnel on top.
function PegBoard:drawBoard()
    local unit = Look.pixel
    local center, half, funnel = PegBoard.center, PegBoard.slots / 2 * PegBoard.spacing, PegBoard.funnel
    local top = PegBoard.top - funnel.rim - 60
    for _, side in ipairs({-1, 1}) do
        Look.block(center + side * (half - 40), (PegBoard.floor + PegBoard.ground) / 2 + 20, 26, PegBoard.ground - PegBoard.floor + 20, 'darkWood', -3)
    end
    graphics2d.drawShape({center - half - 40, top, half * 2 + 80, PegBoard.floor + 50 - top}, {radius = 22, color = '#FFA86B3C', borderWidth = 5 * unit, borderColor = Look.ink, layer = -2})
    graphics2d.drawShape({center - half - 22, top + 18, half * 2 + 44, PegBoard.floor + 14 - top}, {radius = 12, color = '#FF26294A', layer = -2})
    for index = 0, PegBoard.slots do
        Look.block(center + (index - PegBoard.slots / 2) * PegBoard.spacing, PegBoard.floor - 35, 6, 70, 'steel', 0)
    end
    local wallTop = PegBoard.top - funnel.bottom
    Look.block(center - half - 8, (wallTop + PegBoard.floor) / 2, 16, PegBoard.floor - wallTop, 'steel', 0)
    Look.block(center + half + 8, (wallTop + PegBoard.floor) / 2, 16, PegBoard.floor - wallTop, 'steel', 0)
    Look.block(center, PegBoard.floor + 10, half * 2 + 32, 20, 'steel', 0)
    for _, side in ipairs({-1, 1}) do
        Look.line(center + side * funnel.mouth, PegBoard.top - funnel.rim, center + side * funnel.outlet, PegBoard.top - funnel.bottom, 10, '#FFA9BCD3', 0)
    end
end

-- The histogram stands on a board of its own beside the pegs, with one bar for each slot, never shorter than a stub, its count on top and its number below.
function PegBoard:drawChart()
    local unit = Look.pixel
    local chart = PegBoard.chart
    graphics2d.drawShape({chart.left - 20, chart.top - 20, chart.right - chart.left + 40, chart.bottom - chart.top + 90}, {radius = 22, color = '#FFA86B3C', borderWidth = 5 * unit, borderColor = Look.ink, layer = -2})
    graphics2d.drawShape({chart.left - 2, chart.top - 2, chart.right - chart.left + 4, chart.bottom - chart.top + 54}, {radius = 12, color = '#FF26294A', layer = -2})
    self:tag('Balls per slot', (chart.left + chart.right) / 2, chart.top + 34, {size = 26})
    local most = 1
    for _, count in ipairs(self.counts) do
        most = math.max(most, count)
    end
    local width = (chart.right - chart.left - chart.gap * (PegBoard.slots + 1)) / PegBoard.slots
    for slot, count in ipairs(self.counts) do
        local x = chart.left + chart.gap + (slot - 1) * (width + chart.gap)
        local height = math.max(8, count / most * chart.height)
        Look.block(x + width / 2, chart.bottom - height / 2, width, height, PegBoard.bar, 0)
        self:tag(tostring(count), x + width / 2, chart.bottom - height - 16, {size = 18})
        self:tag(tostring(slot), x + width / 2, chart.bottom + 24, {size = 18, color = '#FFA9BCD3'})
    end
end

function PegBoard:draw(area)
    Scenery.sky(self.camera, PegBoard.ground)
    Scenery.ground(PegBoard.ground)
    self:drawBoard()
    Look.draw(self.board, {layer = 0})
    Look.drawAll(self.balls, {layer = 1})
    self:drawChart()
end

return PegBoard
