-- A board of pegs where balls bounce down into slots, whose sensors count every ball that arrives and feed a histogram of the slots.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local PegBoard = haylen.class('PegBoard', PhysicsTest)

local kCenter = -200
local kSpacing = 52
local kRows = 12
local kSlots = 12
local kTop = -300
local kBallRadius = 9
local kRainInterval = 0.15
local kMaxBalls = 120
local kChart = {x = 210, bottom = 380, width = 40, gap = 6, height = 560}

function PegBoard:enter()
    self:frame{
        hint = 'Balls fall from the top center. Click or tap above the board to drop one where you point. R or the X button clears the counts.',
        controls = {
            ui.toggle{id = 'rain', text = 'Rain balls', checked = true, onChange = function(event) self.raining = event.checked end},
            ui.button{id = 'drop', text = 'Drop a ball', onClick = function() self:dropBall(kCenter) end},
            ui.button{id = 'burst', text = 'Drop 50 balls', onClick = function() self.queued = self.queued + 50 end},
            ui.button{id = 'reset', text = 'Clear the counts', onClick = function() self:build() end},
        },
        focus = 'rain',
    }
    self.random = m.random(53)
    self.raining = true
    self:build()
end

function PegBoard:build()
    self.world = physics2d.newWorld()
    local board = self.world:createBody({type = 'static'})
    for row = 0, kRows - 1 do
        local count = row % 2 == 0 and kSlots or kSlots - 1
        for index = 0, count - 1 do
            parts.circle(board, 6, {offsetX = kCenter + (index - (count - 1) / 2) * kSpacing, offsetY = kTop + row * kSpacing, restitution = 0.4})
        end
    end
    local half = kSlots / 2 * kSpacing
    for index = 0, kSlots do
        local x = kCenter + (index - kSlots / 2) * kSpacing
        parts.box(board, 6, 80, {offsetX = x, offsetY = 350})
    end
    parts.box(board, 16, 800, {offsetX = kCenter - half - 8, offsetY = 0})
    parts.box(board, 16, 800, {offsetX = kCenter + half + 8, offsetY = 0})
    parts.box(board, half * 2 + 32, 40, {offsetX = kCenter, offsetY = 410})
    parts.paint(board, '#FF78909C')
    self.board = board

    for slot = 1, kSlots do
        local sensor = self.world:createBody({type = 'static', x = kCenter + (slot - 0.5 - kSlots / 2) * kSpacing, y = 380})
        sensor:addBox(kSpacing - 8, 16, {sensor = true})
        sensor.data = {slot = slot}
    end
    self.counts = {}
    for slot = 1, kSlots do
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

function PegBoard:dropBall(x)
    local half = kSlots / 2 * kSpacing - kBallRadius - 2
    local ball = self.world:createBody({x = m.clamp(x + self.random:range(-4, 4), kCenter - half, kCenter + half), y = kTop - 60})
    parts.circle(ball, kBallRadius, {restitution = 0.4, friction = 0.1})
    ball.data.ball = true
    self.balls[#self.balls + 1] = ball
    self.dropped = self.dropped + 1
    if #self.balls > kMaxBalls then
        table.remove(self.balls, 1):destroy()
    end
end

function PegBoard:update(dt)
    PegBoard.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer = self.pointer
    if pointer.pressed and not ui.usingPointer() and pointer.worldY < kTop and math.abs(pointer.worldX - kCenter) < kSlots / 2 * kSpacing then
        self:dropBall(pointer.worldX)
    end
    local most = 0
    for _, count in ipairs(self.counts) do
        most = math.max(most, count)
    end
    self:status(string.format('Balls dropped %d, counted %d, in play %d, most in one slot %d, step %.2f ms', self.dropped, self.counted, #self.balls, most, self:stepTime()))
end

-- Balls that a slot counted settle in it for a moment and are then taken off the board.
function PegBoard:fixedUpdate(step)
    self.clock = self.clock + step
    if self.clock >= kRainInterval then
        self.clock = 0
        if self.queued > 0 then
            self.queued = self.queued - 1
            self:dropBall(kCenter)
        elseif self.raining then
            self:dropBall(kCenter)
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

function PegBoard:draw(area)
    graphics2d.drawRect({kCenter - kSlots / 2 * kSpacing, kTop - 90, kSlots * kSpacing, 480 - kTop}, '#FF1F2733', {layer = -1})
    parts.draw(self.board)
    parts.drawAll(self.balls, {layer = 1})

    local most = 1
    for _, count in ipairs(self.counts) do
        most = math.max(most, count)
    end
    for slot, count in ipairs(self.counts) do
        local x = kChart.x + (slot - 1) * (kChart.width + kChart.gap)
        local height = count / most * kChart.height
        graphics2d.drawRect({x, kChart.bottom - height, kChart.width, height}, m.fromHsv(0.55 - slot * 0.03, 0.6, 0.95), {layer = 1})
        PegBoard.caption(tostring(count), x + kChart.width / 2, kChart.bottom - height - 6, {anchor = {0.5, 1}, size = 16})
        PegBoard.caption(tostring(slot), x + kChart.width / 2, kChart.bottom + 8, {anchor = {0.5, 0}, size = 16})
    end
    graphics2d.drawLine(kChart.x - 6, kChart.bottom, kChart.x + kSlots * (kChart.width + kChart.gap), kChart.bottom, 2, '#88FFFFFF')
    PegBoard.caption('Balls per slot', kChart.x, kChart.bottom - kChart.height - 50, {size = 24, color = '#FFE8EAF2'})
end

return PegBoard
