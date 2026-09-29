-- One station per joint type: springy and limited distance joints, a revolute motor and a revolute with limits, a prismatic lift, soft welds, a wheel on suspension, a motor joint and a filter joint.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('grab')
local parts = require('parts')
local sample = require('sample')

local Joints = haylen.class('Joints', sample.Test)

local kLabel = {size = 24, color = '#FFFFD54F', anchor = {0.5, 0}, layer = 10}
local kLiftTop, kLiftBottom = -290, -80

function Joints:enter()
    Joints.super.enter(self, {
        hint = 'Drag any body to feel its joint: the mouse joint that drags it is a joint too. Reverse turns every motor around, R or X rebuilds the stations.',
        controls = {
            ui.button{id = 'reverse', text = 'Reverse the motors', onClick = function() self:reverse() end},
            ui.checkbox{id = 'outlines', text = 'Show joints', onChange = function(event) self.debug = event.checked end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'reverse',
    })
    self:build()
end

function Joints:build()
    self.world = physics2d.newWorld()
    self.grab = Grab.new(self.world)
    self.bodies, self.statics, self.labels, self.links = {}, {}, {}, {}
    self.wheels = {}
    self.direction, self.cartDirection = 1, 1
    self.time = 0

    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40)
    self.statics[1] = ground
    self:distance(-600, -380)
    self:revoluteMotor(-200, -380)
    self:revoluteLimits(200, -380)
    self:prismatic(600, -380)
    self:weld(-600, 40)
    self:wheel(-200, 40)
    self:motor(200, 40)
    self:filter(600, 40)
end

function Joints:static(x, y, width, height)
    local body = self.world:createBody({type = 'static', x = x, y = y})
    parts.box(body, width or 30, height or 30)
    self.statics[#self.statics + 1] = body
    return body
end

function Joints:dynamic(x, y, options)
    options = options or {}
    options.x, options.y = x, y
    local body = self.world:createBody(options)
    self.bodies[#self.bodies + 1] = body
    return body
end

function Joints:label(text, x, y)
    self.labels[#self.labels + 1] = {text = text, x = x, y = y}
end

-- Draws a joint as a line from a fixed point or a body to a body, so every station shows what holds it.
function Joints:link(from, to)
    self.links[#self.links + 1] = {from = from, to = to}
end

function Joints:distance(x, y)
    self:label('Distance', x, y)
    local anchor = self:static(x, y + 60, 200, 16)
    local spring = self:dynamic(x - 60, y + 220)
    parts.circle(spring, 24)
    self.world:createJoint('distance', anchor, spring, {ax = x - 60, ay = y + 68, bx = x - 60, by = y + 220, enableSpring = true, hertz = 1.5, dampingRatio = 0.1})
    self:link({x - 60, y + 68}, spring)
    local rope = self:dynamic(x + 80, y + 150)
    parts.box(rope, 40, 40)
    self.world:createJoint('distance', anchor, rope, {ax = x + 60, ay = y + 68, bx = x + 80, by = y + 150, enableLimit = true, lower = 40, upper = 200, enableSpring = true})
    self:link({x + 60, y + 68}, rope)
end

function Joints:revoluteMotor(x, y)
    self:label('Revolute with a motor', x, y)
    local hub = self:static(x, y + 170, 20, 20)
    local blade = self:dynamic(x, y + 170)
    parts.box(blade, 260, 20)
    parts.box(blade, 20, 260)
    self.windmill = self.world:createJoint('revolute', hub, blade, {ax = x, ay = y + 170, enableMotor = true, motorSpeed = 1.5, maxMotorTorque = 1e8})
end

function Joints:revoluteLimits(x, y)
    self:label('Revolute with limits', x, y)
    local hinge = self:static(x - 100, y + 100, 24, 24)
    local door = self:dynamic(x + 10, y + 100, {angularVelocity = 3})
    parts.box(door, 220, 24)
    self.world:createJoint('revolute', hinge, door, {ax = x - 100, ay = y + 100, enableLimit = true, lower = -0.6, upper = 1.2})
end

function Joints:prismatic(x, y)
    self:label('Prismatic with a motor', x, y)
    local rail = self:static(x, y + 170, 12, 280)
    local lift = self:dynamic(x, y + 250)
    parts.box(lift, 180, 20)
    self.lift = lift
    self.piston = self.world:createJoint('prismatic', rail, lift, {ax = x, ay = y + 250, axisX = 0, axisY = 1, enableLimit = true, lower = -170, upper = 60, enableMotor = true, motorSpeed = -120, maxMotorForce = 1e6})
    local crate = self:dynamic(x + 40, y + 200)
    parts.box(crate, 40, 40)
end

-- A beam of boxes held by welds, stiff at the wall and soft further out, so it sags and springs back.
function Joints:weld(x, y)
    self:label('Weld', x, y)
    local wall = self:static(x - 170, y + 150, 30, 160)
    local previous = wall
    for index = 1, 5 do
        local link = self:dynamic(x - 170 + index * 60, y + 150)
        parts.box(link, 58, 26)
        self.world:createJoint('weld', previous, link, {ax = x - 170 + index * 60 - 30, ay = y + 150, hertz = index == 1 and 0 or 6, dampingRatio = 0.3})
        previous = link
    end
end

function Joints:wheel(x, y)
    self:label('Wheel on a spring', x, y)
    self:static(x, y + 330, 360, 20)
    local cart = self:dynamic(x, y + 240)
    parts.box(cart, 160, 30)
    self.cart, self.cartHome = cart, x
    for _, side in ipairs({-60, 60}) do
        local wheel = self:dynamic(x + side, y + 290)
        parts.circle(wheel, 26, {friction = 0.9})
        local joint = self.world:createJoint('wheel', cart, wheel, {ax = x + side, ay = y + 290, axisX = 0, axisY = 1, enableSpring = true, hertz = 3, dampingRatio = 0.4, enableLimit = true, lower = -20, upper = 20, enableMotor = true, motorSpeed = 3, maxMotorTorque = 2e6})
        self.wheels[#self.wheels + 1] = joint
    end
    local bump = self.world:createBody({type = 'static', x = x, y = y + 318})
    parts.polygon(bump, {{-40, 0}, {0, -14}, {40, 0}})
    self.statics[#self.statics + 1] = bump
end

-- A kinematic body circles around, and the motor joint pulls the box toward the offset it had, lagging behind with a limited force.
function Joints:motor(x, y)
    self:label('Motor', x, y)
    self.leader = self.world:createBody({type = 'kinematic', x = x, y = y + 170})
    parts.circle(self.leader, 16)
    parts.paint(self.leader, '#FF90A4AE')
    self.statics[#self.statics + 1] = self.leader
    self.leaderCenter = {x = x, y = y + 170}
    local follower = self:dynamic(x + 90, y + 170, {gravityScale = 0})
    parts.box(follower, 50, 50)
    self.world:createJoint('motor', self.leader, follower, {maxMotorForce = 3000, maxMotorTorque = 30000})
    self:link(self.leader, follower)
end

-- The filtered pair falls through each other, while the plain pair stacks.
function Joints:filter(x, y)
    self:label('Filter', x, y)
    self:static(x, y + 330, 320, 20)
    local lower = self:dynamic(x - 70, y + 280)
    parts.box(lower, 60, 60)
    local upper = self:dynamic(x - 50, y + 120)
    parts.box(upper, 60, 60)
    self.world:createJoint('filter', lower, upper)
    local plainLower = self:dynamic(x + 70, y + 280)
    parts.box(plainLower, 60, 60)
    local plainUpper = self:dynamic(x + 70, y + 120)
    parts.box(plainUpper, 60, 60)
    self:label('filtered      plain', x, y + 350)
end

function Joints:reverse()
    self.direction = -self.direction
    self.windmill.motorSpeed = 1.5 * self.direction
    self:driveCart(self.direction)
end

function Joints:driveCart(direction)
    self.cartDirection = direction
    for _, wheel in ipairs(self.wheels) do
        wheel.motorSpeed = 3 * direction
    end
end

function Joints:exit()
    Joints.super.exit(self)
    self.world, self.grab, self.bodies, self.statics = nil, nil, nil, nil
end

function Joints:update(dt)
    Joints.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self:showStats(string.format('bodies %d\nstep %.2f ms', self.world.bodyCount, sample.milliseconds('physics step')))
end

-- The lift turns around at the ends of its rail, and the leader of the motor joint keeps circling.
function Joints:fixedUpdate(step)
    self.time = self.time + step
    if self.lift.y <= kLiftTop then
        self.piston.motorSpeed = 120
    elseif self.lift.y >= kLiftBottom then
        self.piston.motorSpeed = -120
    end
    local offset = self.cart.x - self.cartHome
    if offset > 100 and self.cartDirection > 0 or offset < -100 and self.cartDirection < 0 then
        self:driveCart(-self.cartDirection)
    end
    local center = self.leaderCenter
    local x = center.x + math.cos(self.time * 1.5) * 90
    local y = center.y + math.sin(self.time * 1.5) * 90
    self.leader.velocity = {(x - self.leader.x) / step, (y - self.leader.y) / step}
    sample.step(self.world, step)
end

function Joints:render()
    self:beginWorld()
    parts.drawAll(self.statics)
    parts.drawAll(self.bodies)
    for _, label in ipairs(self.labels) do
        graphics2d.drawText(nil, label.text, label.x, label.y, kLabel)
    end
    for _, link in ipairs(self.links) do
        local from = link.from
        local x, y = from.x or from[1], from.y or from[2]
        graphics2d.drawLine(x, y, link.to.x, link.to.y, 3, '#AAB0BEC5', {layer = -1})
    end
    self.grab:draw()
    if self.debug then
        self.world:debugDraw({layer = 20})
    end
end

return Joints
