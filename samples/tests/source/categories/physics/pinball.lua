-- A pinball table: flippers on revolute joints with motors and limits, bumpers that kick the ball when it touches them, a plunger on a spring, walls from a chain and a one-way gate at the top of the launch lane.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Pinball = haylen.class('Pinball', PhysicsTest)

Pinball.actions = {
    {name = 'leftFlipper', type = 'button', bindings = {'key:a', 'key:left', 'key:leftShift', 'button:leftShoulder', 'axis:leftTrigger+', 'virtual:leftFlipper'}},
    {name = 'rightFlipper', type = 'button', bindings = {'key:d', 'key:right', 'key:rightShift', 'button:rightShoulder', 'axis:rightTrigger+', 'virtual:rightFlipper'}},
    {name = 'plunger', type = 'button', bindings = {'key:space', 'key:s', 'key:down', 'button:south', 'virtual:plunger'}},
}

local kBallRadius = 14
local kLane = 315
local kPlungerY = 350
local kFlippers = {{x = -125, side = 1, action = 'leftFlipper'}, {x = 125, side = -1, action = 'rightFlipper'}}
local kFlipperY = 330
local kFlipperLength = 105
local kBumpers = {{-100, -170}, {90, -190}, {0, -60}}
local kKick = 520
local kLitTime = 0.15
local kPullTime = 0.7

function Pinball:enter()
    self:frame{
        hint = 'Hold Space, S or the south button to pull the plunger and let go to launch. The left flipper takes A, the left arrow, Shift, the left shoulder or the left trigger, and the right flipper the same keys on the right. R or the X button starts a new game.',
        controls = {
            ui.button{id = 'launch', text = 'Launch the ball', onClick = function() self.autoPull = kPullTime end},
            ui.button{id = 'reset', text = 'New game', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = Pinball.actions,
        overlay = {
            ui.touchButton{action = 'leftFlipper', text = 'Left', size = 150, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}},
            ui.touchButton{action = 'rightFlipper', text = 'Right', size = 150, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
            ui.touchButton{action = 'plunger', text = 'Plunger', size = 120, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 290, 0}},
        },
    }
    self.best = 0
    self:build()
end

function Pinball:build()
    self.world = physics2d.newWorld({gravity = {0, 700}})
    self.score, self.balls, self.autoPull = 0, 1, 0

    -- The outer wall is one chain that runs counter-clockwise on screen, so it holds the ball on its inner side.
    self.cabinet = self.world:createBody({type = 'static'})
    local wall = {{340, 430}, {340, -280}}
    for step = 1, 15 do
        local angle = math.pi * step / 16
        wall[#wall + 1] = {25 + 315 * math.cos(angle), -280 - 140 * math.sin(angle)}
    end
    for _, point in ipairs({{-290, -280}, {-290, 220}, {-125, 318}}) do
        wall[#wall + 1] = point
    end
    parts.chain(self.cabinet, wall, false, {friction = 0.1})
    parts.segment(self.cabinet, 290, -150, 290, 430)
    parts.segment(self.cabinet, 125, 318, 290, 220)
    parts.segment(self.cabinet, 290, 430, 340, 430)
    parts.paint(self.cabinet, '#FF5C6BC0')
    local gate = self.world:createBody({type = 'static', x = kLane, y = -168, rotation = -0.61})
    parts.box(gate, 62, 8, {oneWay = {0, -1}})
    parts.paint(gate, '#FFFFD54F')
    self.statics = {self.cabinet, gate}

    -- The kickers above the flippers bounce the ball back faster than it came, with a restitution above 1.
    for _, points in ipairs({{{-250, 150}, {-250, 230}, {-180, 268}}, {{180, 268}, {250, 230}, {250, 150}}}) do
        local kicker = self.world:createBody({type = 'static'})
        parts.polygon(kicker, points, {restitution = 1.1})
        parts.paint(kicker, '#FF4DD0E1')
        kicker.data.kicker = true
        self.statics[#self.statics + 1] = kicker
    end

    self.bumpers = {}
    for _, spot in ipairs(kBumpers) do
        local bumper = self.world:createBody({type = 'static', x = spot[1], y = spot[2]})
        parts.circle(bumper, 28, {restitution = 0.5})
        parts.paint(bumper, '#FFE57373')
        bumper.data.bumper = true
        self.bumpers[#self.bumpers + 1] = {body = bumper, lit = 0}
    end

    -- Each flipper rests against one limit, and its motor swings it to the other while its button is held.
    self.flippers = {}
    for _, spec in ipairs(kFlippers) do
        local flipper = self.world:createBody({x = spec.x, y = kFlipperY, rotation = 0.5 * spec.side})
        local length = kFlipperLength * spec.side
        parts.polygon(flipper, {{0, -12}, {length, -6}, {length, 6}, {0, 12}}, {density = 2, friction = 0.4})
        parts.circle(flipper, 12, {density = 2})
        parts.paint(flipper, '#FFFFB74D')
        local lower, upper = spec.side > 0 and -0.95 or 0, spec.side > 0 and 0 or 0.95
        local joint = self.world:createJoint('revolute', self.cabinet, flipper, {ax = spec.x, ay = kFlipperY, enableLimit = true, lower = lower, upper = upper, enableMotor = true, maxMotorTorque = 5e6})
        self.flippers[#self.flippers + 1] = {body = flipper, joint = joint, side = spec.side, action = spec.action}
    end

    self.plunger = self.world:createBody({x = kLane, y = kPlungerY})
    parts.box(self.plunger, 44, 24, {density = 4})
    parts.paint(self.plunger, '#FF90A4AE')
    self.spring = self.world:createJoint('prismatic', self.cabinet, self.plunger, {ax = kLane, ay = kPlungerY, axisX = 0, axisY = 1, enableLimit = true, lower = -10, upper = 70, enableSpring = true, hertz = 3.6, dampingRatio = 0.05, maxMotorForce = 1e5, motorSpeed = 160})
    self:serve()

    self.world.onContactBegin = function(a, b, contact)
        local ball, other = a, b
        if b.data.ball then
            ball, other = b, a
        end
        if not ball.data.ball then
            return
        end
        if other.data.bumper then
            local dx, dy = ball.x - other.x, ball.y - other.y
            local length = math.max(1, math.sqrt(dx * dx + dy * dy))
            ball:applyImpulse(dx / length * ball.mass * kKick, dy / length * ball.mass * kKick)
            self.score = self.score + 100
            for _, bumper in ipairs(self.bumpers) do
                if bumper.body == other then
                    bumper.lit = kLitTime
                end
            end
        elseif other.data.kicker then
            self.score = self.score + 10
        end
    end
end

function Pinball:serve()
    self.ball = self.world:createBody({x = kLane, y = kPlungerY - 12 - kBallRadius, bullet = true})
    parts.circle(self.ball, kBallRadius, {density = 2, restitution = 0.3, friction = 0.2})
    parts.paint(self.ball, '#FFECEFF1')
    self.ball.data.ball = true
end

function Pinball:exit()
    Pinball.super.exit(self)
    input.clearVirtual()
end

function Pinball:update(dt)
    Pinball.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    for _, bumper in ipairs(self.bumpers) do
        bumper.lit = math.max(0, bumper.lit - dt)
    end
    self.best = math.max(self.best, self.score)
    self:status(string.format('Score %d, best %d, ball %d, ball speed %.0f, plunger pulled %.0f units, step %.2f ms', self.score, self.best, self.balls, self.ball.velocity:length(), math.max(0, self.spring.translation), self:stepTime()))
end

function Pinball:fixedUpdate(step)
    for _, flipper in ipairs(self.flippers) do
        flipper.joint.motorSpeed = (input.down(flipper.action) and -20 or 12) * flipper.side
    end
    self.autoPull = math.max(0, self.autoPull - step)
    self.spring.enableMotor = input.down('plunger') or self.autoPull > 0
    self:simulate(self.world, step)
    if self.ball.y > 460 then
        self.ball:destroy()
        self.balls = self.balls + 1
        self:serve()
    end
end

function Pinball:draw(area)
    graphics2d.drawPolygon({{-300, -440}, {350, -440}, {350, 440}, {-300, 440}}, '#FF141A2E', {layer = -1})
    parts.drawAll(self.statics)
    for _, bumper in ipairs(self.bumpers) do
        local body = bumper.body
        parts.draw(body)
        graphics2d.drawRing(body.x, body.y, 36, 6, bumper.lit > 0 and '#FFFFF59D' or '#66FFFFFF', {layer = 1})
    end
    for _, flipper in ipairs(self.flippers) do
        parts.draw(flipper.body, {layer = 1})
    end
    parts.draw(self.plunger, {layer = 1})
    parts.draw(self.ball, {layer = 2})
    graphics2d.drawText(nil, string.format('Score %d', self.score), -560, -200, {size = 56, color = '#FFFFD54F', anchor = {0.5, 0.5}})
    graphics2d.drawText(nil, string.format('Best %d', self.best), -560, -120, {size = 32, color = '#FFE8EAF2', anchor = {0.5, 0.5}})
    graphics2d.drawText(nil, string.format('Ball %d', self.balls), -560, -70, {size = 32, color = '#FFE8EAF2', anchor = {0.5, 0.5}})
    Pinball.caption('One-way gate', 360, -168, {anchor = {0, 0.5}})
end

return Pinball
