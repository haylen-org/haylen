-- Pendulums: a cradle of five steel balls on distance joints that passes an impact through the row, a double pendulum that traces its chaotic path and a swing that grows with pushes timed to its motion.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Pendulums = haylen.class('Pendulums', PhysicsTest)

Pendulums.actions = {
    {name = 'push', type = 'button', bindings = {'key:p', 'button:north'}},
}

local kBalls = 5
local kBallRadius = 30
local kBallSpacing = 62
local kCradleX, kCradleTop, kStringLength = 60, -330, 380
local kLiftAngle = 0.7
local kPivot = {480, -110}
local kArmLength = 150
local kTrailLength = 180
local kSwingPivot = {-600, -360}
local kSwingLength = 380
local kPushSpeed = 140
local kRope = '#FFB0BEC5'

function Pendulums:enter()
    self:frame{
        hint = 'Lift balls of the cradle and let them go, or drag any ball, arm or the seat. P or the north button pushes the swing along its motion. R or the X button starts over.',
        controls = {
            ui.button{id = 'one', text = 'Lift one ball', onClick = function() self:lift(1) end},
            ui.button{id = 'two', text = 'Lift two balls', onClick = function() self:lift(2) end},
            ui.button{id = 'push', text = 'Push the swing', onClick = function() self:push() end},
            ui.button{id = 'kick', text = 'Kick the double pendulum', onClick = function() self.tip:applyImpulse(self.tip.mass * 900, 0) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        actions = Pendulums.actions,
        focus = 'one',
    }
    self:build()
end

function Pendulums:build()
    -- A low restitution threshold keeps the cradle bouncing at the small speeds of its last swings.
    self.world = physics2d.newWorld({restitutionThreshold = 20})
    self.grab = Grab(self.world)
    local bar = self.world:createBody({type = 'static', x = kCradleX, y = kCradleTop})
    parts.box(bar, kBalls * kBallSpacing + 40, 20)
    local frame = self.world:createBody({type = 'static', x = kSwingPivot[1], y = kSwingPivot[2]})
    parts.box(frame, 60, 20)
    self.statics = {bar, frame}

    self.balls = {}
    for index = 1, kBalls do
        local x = kCradleX + (index - (kBalls + 1) / 2) * kBallSpacing
        local ball = self.world:createBody({x = x, y = kCradleTop + kStringLength})
        parts.circle(ball, kBallRadius, {density = 8, friction = 0, restitution = 1})
        parts.paint(ball, '#FFCFD8DC')
        self.world:createJoint('distance', bar, ball, {ax = x, ay = kCradleTop, bx = x, by = kCradleTop + kStringLength})
        self.balls[index] = {body = ball, x = x}
    end

    local pivot = self.world:createBody({type = 'static', x = kPivot[1], y = kPivot[2]})
    parts.circle(pivot, 10)
    self.statics[#self.statics + 1] = pivot
    self.arm = self.world:createBody({x = kPivot[1], y = kPivot[2], rotation = -2.2})
    parts.box(self.arm, kArmLength, 14, {offsetX = kArmLength / 2})
    local elbowX, elbowY = kPivot[1] + math.cos(-2.2) * kArmLength, kPivot[2] + math.sin(-2.2) * kArmLength
    self.tip = self.world:createBody({x = elbowX, y = elbowY, rotation = -0.6})
    parts.box(self.tip, kArmLength, 14, {offsetX = kArmLength / 2})
    parts.circle(self.tip, 18, {offsetX = kArmLength, density = 2})
    self.world:createJoint('revolute', pivot, self.arm, {ax = kPivot[1], ay = kPivot[2]})
    self.world:createJoint('revolute', self.arm, self.tip, {ax = elbowX, ay = elbowY})
    self.trail = {}

    -- Two ropes from one pivot to the ends of the seat make a rigid triangle, so the seat stays level with its ropes.
    self.seat = self.world:createBody({x = kSwingPivot[1], y = kSwingPivot[2] + kSwingLength})
    parts.box(self.seat, 140, 16)
    for _, side in ipairs({-60, 60}) do
        self.world:createJoint('distance', frame, self.seat, {ax = kSwingPivot[1], ay = kSwingPivot[2], bx = kSwingPivot[1] + side, by = kSwingPivot[2] + kSwingLength})
    end
    self:lift(1)
end

-- Places the first balls of the row on their arcs at the lift angle and stills the whole row.
function Pendulums:lift(count)
    for index, ball in ipairs(self.balls) do
        local angle = index <= count and kLiftAngle or 0
        ball.body:setTransform(ball.x - math.sin(angle) * kStringLength, kCradleTop + math.cos(angle) * kStringLength, 0)
        ball.body.velocity = {0, 0}
        ball.body.angularVelocity = 0
    end
end

-- Pushes the seat along the way it moves, or to the right when it hangs still.
function Pendulums:push()
    local velocity = self.seat.velocity
    local direction = velocity.x < 0 and -1 or 1
    self.seat:applyImpulse(direction * self.seat.mass * kPushSpeed, 0)
end

function Pendulums:swingAngle()
    return math.deg(math.atan(self.seat.x - kSwingPivot[1], self.seat.y - kSwingPivot[2]))
end

function Pendulums:update(dt)
    Pendulums.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if input.pressed('push') then
        self:push()
    end
    self.grab:update(self.pointer, self.camera)
    local speeds = {}
    for index, ball in ipairs(self.balls) do
        speeds[index] = string.format('%.0f', ball.body.velocity:length())
    end
    local bob = self.trail[#self.trail] or {self.tip.x, self.tip.y}
    self:status(string.format('Cradle speeds %s, swing %.0f degrees, pendulum tip %.0f units per second, step %.2f ms', table.concat(speeds, ' '), self:swingAngle(), self.tip:velocityAt(bob[1], bob[2]):length(), self:stepTime()))
end

function Pendulums:fixedUpdate(step)
    self:simulate(self.world, step)
    local rotation = self.tip.rotation
    self.trail[#self.trail + 1] = {self.tip.x + math.cos(rotation) * kArmLength, self.tip.y + math.sin(rotation) * kArmLength}
    if #self.trail > kTrailLength then
        table.remove(self.trail, 1)
    end
end

function Pendulums:draw(area)
    parts.drawAll(self.statics)
    for _, ball in ipairs(self.balls) do
        graphics2d.drawLine(ball.x, kCradleTop, ball.body.x, ball.body.y, 3, kRope)
    end
    for _, side in ipairs({-60, 60}) do
        local cos, sin = math.cos(self.seat.rotation), math.sin(self.seat.rotation)
        graphics2d.drawLine(kSwingPivot[1], kSwingPivot[2], self.seat.x + side * cos, self.seat.y + side * sin, 3, kRope)
    end
    if #self.trail > 1 then
        graphics2d.drawPolyline(self.trail, 3, '#99FFD54F', false, {layer = 1})
    end
    for _, ball in ipairs(self.balls) do
        parts.draw(ball.body, {layer = 2})
    end
    parts.drawAll({self.arm, self.tip, self.seat}, {layer = 2})
    Pendulums.caption('Cradle', kCradleX, kCradleTop - 50, {anchor = {0.5, 0}})
    Pendulums.caption('Double pendulum', kPivot[1], -420, {anchor = {0.5, 0}})
    Pendulums.caption('Swing', kSwingPivot[1], kSwingPivot[2] - 50, {anchor = {0.5, 0}})
    self.grab:draw()
end

return Pendulums
