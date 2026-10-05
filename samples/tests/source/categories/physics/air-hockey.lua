-- Air hockey seen from above: kinematic paddles that `body:moveTo` drives, one following the pointer, the keys or a stick and one played by the computer, a puck with `bullet = true` and goal sensors that keep the score.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local AirHockey = haylen.class('AirHockey', PhysicsTest)

AirHockey.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
}

local kHalfWidth, kHalfHeight = 700, 380
local kGoal = 110
local kPaddleRadius, kPuckRadius = 40, 26
local kPaddleSpeed = 1800
local kComputerSpeed = 900
local kKeySpeed = 900
local kWinningScore = 7
local kServeDelay = 1

function AirHockey:enter()
    self:frame{
        hint = 'Move your paddle on the left half with the mouse, a finger, the keys or the left stick, and hit the puck into the goal on the right. The first to 7 wins. R or the X button starts a new match.',
        controls = {
            ui.button{id = 'reset', text = 'New match', onClick = function() self:build() end},
        },
        play = true,
        actions = AirHockey.actions,
        overlay = {
            ui.touchStick{action = 'move', radius = 110, floating = true, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
        },
    }
    self:build()
end

function AirHockey:build()
    self.world = physics2d.newWorld({gravity = {0, 0}, maxSpeed = 2400})
    -- Each side has a long wall, the two walls beside its goal mouth and the three walls of its goal box.
    local rink = self.world:createBody({type = 'static'})
    local side = (kHalfHeight - kGoal) / 2
    for _, sign in ipairs({-1, 1}) do
        parts.box(rink, kHalfWidth * 2 + 60, 30, {offsetY = sign * (kHalfHeight + 15), restitution = 0.9, friction = 0})
        for _, piece in ipairs({{15, -kGoal - side, 30, side * 2}, {15, kGoal + side, 30, side * 2}, {75, 0, 30, kGoal * 2 + 60}, {45, -kGoal - 15, 90, 30}, {45, kGoal + 15, 90, 30}}) do
            parts.box(rink, piece[3], piece[4], {offsetX = sign * (kHalfWidth + piece[1]), offsetY = piece[2], restitution = 0.9, friction = 0})
        end
    end
    parts.paint(rink, '#FF5C6BC0')
    self.rink = rink
    for _, goal in ipairs({{-1, 'computer'}, {1, 'player'}}) do
        local net = self.world:createBody({type = 'static', x = goal[1] * (kHalfWidth + 40), y = 0})
        net:addBox(40, kGoal * 2, {sensor = true})
        net.data = {scorer = goal[2]}
    end

    self.puck = self.world:createBody({linearDamping = 0.15, bullet = true})
    parts.circle(self.puck, kPuckRadius, {restitution = 0.9, friction = 0})
    parts.paint(self.puck, '#FF263238')
    self.player = self:paddle(-500, '#FFE57373')
    self.computer = self:paddle(500, '#FF64B5F6')
    self.target = {x = -500, y = 0}
    self.scores = {player = 0, computer = 0}
    self.serve, self.message = 0, nil

    self.world.onSensorBegin = function(sensor, visitor, shapes)
        if visitor == self.puck and self.serve <= 0 then
            local scorer = sensor.data.scorer
            self.scores[scorer] = self.scores[scorer] + 1
            visitor.velocity = {0, 0}
            self.serve, self.serveSide = kServeDelay, scorer == 'player' and 1 or -1
            if self.scores[scorer] >= kWinningScore then
                self.message = scorer == 'player' and 'You win the match' or 'The computer wins the match'
                self.serve = kServeDelay * 3
            end
        end
    end
end

function AirHockey:paddle(x, color)
    local body = self.world:createBody({type = 'kinematic', x = x, y = 0})
    parts.circle(body, kPaddleRadius, {restitution = 0.6})
    parts.paint(body, color)
    return body
end

-- Moves a paddle toward a point of its own half, no faster than its top speed.
function AirHockey:drive(body, x, y, minimumX, maximumX, speed, step)
    x = m.clamp(x, minimumX, maximumX)
    y = m.clamp(y, -kHalfHeight + kPaddleRadius, kHalfHeight - kPaddleRadius)
    local dx, dy = x - body.x, y - body.y
    local length = math.sqrt(dx * dx + dy * dy)
    local reach = speed * step
    if length > reach then
        dx, dy = dx / length * reach, dy / length * reach
    end
    body:moveTo(body.x + dx, body.y + dy)
end

function AirHockey:exit()
    AirHockey.super.exit(self)
    input.clearVirtual()
end

function AirHockey:update(dt)
    AirHockey.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer = self.pointer
    if self.lastPointer and (pointer.x ~= self.lastPointer[1] or pointer.y ~= self.lastPointer[2]) and not ui.usingPointer() then
        self.target = {x = pointer.worldX, y = pointer.worldY}
    end
    self.lastPointer = {pointer.x, pointer.y}
    local moveX, moveY = input.vector('move')
    self.target.x = m.clamp(self.target.x + moveX * kKeySpeed * dt, -kHalfWidth, 0)
    self.target.y = m.clamp(self.target.y + moveY * kKeySpeed * dt, -kHalfHeight, kHalfHeight)
    self:status(string.format('You %d, computer %d, puck %.0f units per second, your paddle %.0f units per second, step %.2f ms', self.scores.player, self.scores.computer, self.puck.velocity:length(), self.player.velocity:length(), self:stepTime()))
end

-- The computer defends its goal while the puck is away and goes for the puck once it crosses the middle.
function AirHockey:fixedUpdate(step)
    local puck = self.puck
    self:drive(self.player, self.target.x, self.target.y, -kHalfWidth + kPaddleRadius, -kPaddleRadius, kPaddleSpeed, step)
    if puck.x > 0 then
        self:drive(self.computer, puck.x + 30, puck.y, kPaddleRadius, kHalfWidth - kPaddleRadius, kComputerSpeed, step)
    else
        self:drive(self.computer, kHalfWidth - 140, puck.y * 0.6, kPaddleRadius, kHalfWidth - kPaddleRadius, kComputerSpeed, step)
    end
    if self.serve > 0 then
        self.serve = self.serve - step
        if self.serve <= 0 then
            if self.message then
                self.scores, self.message = {player = 0, computer = 0}, nil
            end
            puck:setTransform(self.serveSide * 200, 0, 0)
            puck.velocity = {0, 0}
        end
    end
    self:simulate(self.world, step)
end

function AirHockey:draw(area)
    graphics2d.drawRect({-kHalfWidth, -kHalfHeight, kHalfWidth * 2, kHalfHeight * 2}, '#FFE3F2FD', {layer = -2})
    graphics2d.drawLine(0, -kHalfHeight, 0, kHalfHeight, 4, '#66E57373', {layer = -1})
    graphics2d.drawRing(0, 0, 110, 4, '#66E57373', {layer = -1})
    for _, x in ipairs({-kHalfWidth, kHalfWidth}) do
        graphics2d.drawArc(x, 0, 160, 4, x < 0 and -math.pi / 2 or math.pi / 2, x < 0 and math.pi / 2 or math.pi * 1.5, '#6664B5F6', {layer = -1})
        graphics2d.drawRect({x + (x < 0 and -60 or 0), -kGoal, 60, kGoal * 2}, '#FF37474F', {layer = -1})
    end
    parts.draw(self.rink)
    for _, paddle in ipairs({self.player, self.computer}) do
        parts.draw(paddle, {layer = 1})
        graphics2d.drawCircle(paddle.x, paddle.y, 16, '#55FFFFFF', {layer = 2})
    end
    parts.draw(self.puck, {layer = 1})
    graphics2d.drawText(nil, string.format('%d   %d', self.scores.player, self.scores.computer), 0, -kHalfHeight + 50, {size = 48, color = '#9937474F', anchor = {0.5, 0.5}, layer = -1})
    if self.message then
        graphics2d.drawText(nil, self.message, 0, -40, {size = 56, color = '#FF37474F', anchor = {0.5, 0.5}, layer = 3})
    end
end

return AirHockey
