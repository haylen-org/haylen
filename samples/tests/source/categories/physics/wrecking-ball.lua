-- A crane whose boom turns on a revolute motor and swings a heavy ball on a chain from `physics2d.newRope` into a wall of bricks.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local WreckingBall = haylen.class('WreckingBall', PhysicsTest)

WreckingBall.actions = {
    {name = 'swing', type = 'axis', positive = {'key:d', 'key:right', 'button:dpadRight', 'axis:leftX+', 'virtual:right'}, negative = {'key:a', 'key:left', 'button:dpadLeft', 'axis:leftX-', 'virtual:left'}},
}

local kPivot = {-560, -40}
local kBoomLength = 360
local kRestAngle = -2.2
local kSweep = 1.8
local kChainLength = 320
local kBallRadius = 45
local kBrick = {70, 34}
local kRows = 7
local kCrane = -1

function WreckingBall:enter()
    self:frame{
        hint = 'Turn the boom with A and D, the arrows, the left stick or the touch buttons, or let it swing by itself. R or the X button builds the wall again.',
        controls = {
            ui.toggle{id = 'auto', text = 'Swing by itself', checked = true, onChange = function(event) self.auto = event.checked end},
            ui.label{text = 'Motor speed in radians per second', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1.2, min = 0.4, max = 2.5, step = 0.1, showValue = true, decimals = 1, onChange = function(event) self.speed = event.value end},
            ui.button{id = 'reset', text = 'Build the wall again', onClick = function() self:build() end},
        },
        view = {1840, 1000},
        play = true,
        pointer = false,
        actions = WreckingBall.actions,
        overlay = {
            ui.touchButton{action = 'left', text = 'Left', size = 140, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}},
            ui.touchButton{action = 'right', text = 'Right', size = 140, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self.auto, self.speed, self.direction = true, 1.2, 1
    self:build()
end

function WreckingBall:build()
    self.world = physics2d.newWorld()
    local ground = self.world:createBody({type = 'static', x = 0, y = 460})
    parts.box(ground, 1840, 140)
    local mast = self.world:createBody({type = 'static', x = kPivot[1], y = kPivot[2]})
    parts.box(mast, 40, 430, {offsetY = 215})
    parts.box(mast, 140, 60, {offsetX = 30, offsetY = 400})
    parts.paint(mast, '#FFFFB74D')
    self.statics = {ground, mast}

    -- The boom, its chain and the ball share a negative group, so they never collide with each other.
    self.boom = self.world:createBody({x = kPivot[1], y = kPivot[2], rotation = kRestAngle})
    parts.box(self.boom, kBoomLength, 24, {offsetX = kBoomLength / 2, group = kCrane})
    parts.paint(self.boom, '#FFFFCA28')
    self.motor = self.world:createJoint('revolute', mast, self.boom, {ax = kPivot[1], ay = kPivot[2], enableLimit = true, lower = 0, upper = kSweep, enableMotor = true, maxMotorTorque = 5e7})

    local tipX, tipY = kPivot[1] + math.cos(kRestAngle) * kBoomLength, kPivot[2] + math.sin(kRestAngle) * kBoomLength
    self.ball = self.world:createBody({x = tipX, y = tipY + kChainLength + kBallRadius})
    parts.circle(self.ball, kBallRadius, {density = 12, group = kCrane})
    parts.paint(self.ball, '#FF546E7A')
    self.chain = physics2d.newRope(self.world, {from = {tipX, tipY}, to = {tipX, tipY + kChainLength}, segments = 12, thickness = 10, density = 2, startBody = self.boom, endBody = self.ball, group = kCrane})

    self.bricks = {}
    for row = 0, kRows - 1 do
        local y = 390 - kBrick[2] / 2 - row * kBrick[2]
        local layout = row % 2 == 0 and {{-125, 70}, {-55, 70}, {15, 70}} or {{-142.5, 35}, {-90, 70}, {-20, 70}, {32.5, 35}}
        for _, brick in ipairs(layout) do
            local body = self.world:createBody({x = brick[1], y = y})
            parts.box(body, brick[2] - 2, kBrick[2] - 2, {friction = 0.7})
            parts.paint(body, row % 2 == 0 and '#FFE57373' or '#FFEF9A9A')
            self.bricks[#self.bricks + 1] = {body = body, x = brick[1], y = y}
        end
    end
end

function WreckingBall:bricksDown()
    local count = 0
    for _, brick in ipairs(self.bricks) do
        local body = brick.body
        if math.abs(body.x - brick.x) > 20 or math.abs(body.y - brick.y) > 20 or math.abs(body.rotation) > 0.3 then
            count = count + 1
        end
    end
    return count
end

function WreckingBall:exit()
    WreckingBall.super.exit(self)
    input.clearVirtual()
end

function WreckingBall:update(dt)
    WreckingBall.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self:status(string.format('Boom %.0f degrees, motor %.1f radians per second, ball %.0f units per second, bricks down %d of %d, step %.2f ms', math.deg(self.motor.angle), self.motor.motorSpeed, self.ball.velocity:length(), self:bricksDown(), #self.bricks, self:stepTime()))
end

-- Input turns the boom, and without input the boom swings by itself from one limit to the other.
function WreckingBall:fixedUpdate(step)
    local swing = input.value('swing')
    local angle = self.motor.angle
    if math.abs(swing) > 0.1 then
        self.motor.motorSpeed = swing * self.speed
    elseif self.auto then
        if angle >= kSweep - 0.02 then
            self.direction = -1
        elseif angle <= 0.02 then
            self.direction = 1
        end
        self.motor.motorSpeed = self.direction * self.speed
    else
        self.motor.motorSpeed = 0
    end
    self:simulate(self.world, step)
end

function WreckingBall:draw(area)
    parts.drawAll(self.statics)
    for _, brick in ipairs(self.bricks) do
        parts.draw(brick.body)
    end
    for _, segment in ipairs(self.chain:segments()) do
        local cos, sin = math.cos(segment.rotation), math.sin(segment.rotation)
        local hx, hy = cos * segment.length / 2, sin * segment.length / 2
        graphics2d.drawLine(segment.x - hx, segment.y - hy, segment.x + hx, segment.y + hy, 8, '#FF90A4AE', {layer = 1})
        graphics2d.drawRing(segment.x + hx, segment.y + hy, 5, 3, '#FF607D8B', {layer = 1})
    end
    parts.draw(self.boom, {layer = 2})
    parts.draw(self.ball, {layer = 2})
    graphics2d.drawCircle(kPivot[1], kPivot[2], 12, '#FF37474F', {layer = 3})
end

return WreckingBall
