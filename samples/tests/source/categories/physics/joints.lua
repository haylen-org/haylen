-- One compartment of a cabinet per joint type: springy and limited distance joints, a revolute motor and a revolute with limits, a prismatic lift, soft welds, a wheel on suspension, a motor joint and a filter joint, each drawn with what holds it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Joints = haylen.class('Joints', PhysicsTest)

Joints.code = [[
world:createJoint('distance', beam, ball, {ax = ax, ay = ay, enableSpring = true, hertz = 1.5})
world:createJoint('revolute', hub, blades, {ax = x, ay = y, enableMotor = true, motorSpeed = 1.5})
world:createJoint('revolute', post, door, {ax = x, ay = y, enableLimit = true, upper = 1.2})
world:createJoint('prismatic', rail, lift, {axisY = 1, enableMotor = true, motorSpeed = -120})
world:createJoint('weld', wall, plank, {ax = x, ay = y, hertz = 24, dampingRatio = 0.7})
world:createJoint('wheel', cart, wheel, {axisY = 1, enableSpring = true, hertz = 3})
world:createJoint('motor', leader, follower, {maxMotorForce = 3000})
world:createJoint('filter', lower, upper)  -- The pair never collides.]]

Joints.ground = 330
Joints.beam = -360
Joints.shelf = -10
Joints.columns = {-600, -200, 200, 600}
Joints.liftTop, Joints.liftBottom = -270, -70
-- The welds stiffen toward the wall, where they carry the weight of every plank beyond them.
Joints.weldHertz = {0, 40, 32, 26, 20}

function Joints:enter()
    self:frame{
        code = Joints.code,
        hint = 'Drag any body to feel its joint: the mouse joint that drags it is a joint too. Reverse turns every motor around, R or the X button rebuilds the cabinet.',
        controls = {
            ui.button{id = 'reverse', text = 'Reverse the motors', onClick = function() self:reverse() end},
            ui.checkbox{id = 'outlines', text = 'Show the engine drawing of the joints', onChange = function(event) self.debug = event.checked end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'reverse',
    }
    self:build()
end

function Joints:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies, self.statics, self.labels = {}, {}, {}
    self.springs, self.wheels = {}, {}
    self.direction, self.cartDirection = 1, 1
    self.time = 0

    local ground = self.world:createBody({type = 'static', x = 0, y = Joints.ground + 30})
    ground:addBox(8000, 60)
    self.beam = self:static(0, Joints.beam, 1640, 24, 'darkWood')
    self:static(0, Joints.shelf, 1640, 24, 'darkWood')
    for _, x in ipairs({-800, -400, 0, 400, 800}) do
        self:static(x, (Joints.beam + Joints.shelf) / 2, 20, Joints.shelf - Joints.beam - 24, 'darkWood')
        self:static(x, (Joints.shelf + Joints.ground) / 2, 20, Joints.ground - Joints.shelf - 24, 'darkWood')
    end

    local top, bottom = (Joints.beam + Joints.shelf) / 2, (Joints.shelf + Joints.ground) / 2
    self:distance(Joints.columns[1], top)
    self:revoluteMotor(Joints.columns[2], top)
    self:revoluteLimits(Joints.columns[3], top)
    self:prismatic(Joints.columns[4], top)
    self:weld(Joints.columns[1], bottom)
    self:wheel(Joints.columns[2], bottom)
    self:motor(Joints.columns[3], bottom)
    self:filter(Joints.columns[4], bottom)
end

function Joints:static(x, y, width, height, material)
    local body = self.world:createBody({type = 'static', x = x, y = y})
    body:addBox(width, height)
    Look.dress(body, material)
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

function Joints:title(text, x, y)
    self.labels[#self.labels + 1] = {text = text, x = x, y = y}
end

function Joints:distance(x, y)
    self:title('Distance', x, Joints.beam + 34)
    local ball = self:dynamic(x - 120, y + 30)
    ball:addCircle(26)
    Look.dress(ball, 'red')
    self.world:createJoint('distance', self.beam, ball, {ax = x - 120, ay = Joints.beam + 12, bx = x - 120, by = y + 30, enableSpring = true, hertz = 1.5, dampingRatio = 0.1})
    ball:applyImpulse(0, ball.mass * 320)
    self.springs[#self.springs + 1] = {x = x - 120, y = Joints.beam + 12, body = ball, kind = 'spring'}
    local crate = self:dynamic(x + 120, y - 10)
    crate:addBox(50, 50)
    Look.dress(crate, 'woodCrate')
    self.world:createJoint('distance', self.beam, crate, {ax = x + 100, ay = Joints.beam + 12, bx = x + 120, by = y - 35, enableLimit = true, lower = 40, upper = 170, enableSpring = true})
    self.springs[#self.springs + 1] = {x = x + 100, y = Joints.beam + 12, body = crate, offset = {0, -25}, kind = 'rope'}
end

function Joints:revoluteMotor(x, y)
    self:title('Revolute with a motor', x, Joints.beam + 34)
    local hub = self.world:createBody({type = 'static', x = x, y = y + 20})
    local blades = self:dynamic(x, y + 20)
    blades:addBox(250, 22)
    blades:addBox(22, 250)
    Look.dress(blades, 'wood')
    self.windmill = self.world:createJoint('revolute', hub, blades, {ax = x, ay = y + 20, enableMotor = true, motorSpeed = 1.5, maxMotorTorque = 1e8})
    self.hub = {x = x, y = y + 20}
end

function Joints:revoluteLimits(x, y)
    self:title('Revolute with limits', x, Joints.beam + 34)
    local hinge = self:static(x - 110, y, 26, 26, 'steel')
    local door = self:dynamic(x, y, {angularVelocity = 3})
    door:addBox(220, 24)
    Look.dress(door, 'wood')
    self.world:createJoint('revolute', hinge, door, {ax = x - 110, ay = y, enableLimit = true, lower = -0.6, upper = 1.2})
    self.hinge = {x = x - 110, y = y, lower = -0.6, upper = 1.2}
end

function Joints:prismatic(x, y)
    self:title('Prismatic with a motor', x, Joints.beam + 34)
    local rail = self:static(x - 70, y + 25, 16, Joints.shelf - Joints.beam - 110, 'steel')
    local lift = self:dynamic(x - 10, Joints.liftBottom)
    lift:addBox(140, 18)
    Look.dress(lift, 'steel')
    self.lift = lift
    self.piston = self.world:createJoint('prismatic', rail, lift, {ax = x - 10, ay = Joints.liftBottom, axisX = 0, axisY = 1, enableLimit = true, lower = Joints.liftTop - Joints.liftBottom - 10, upper = 10, enableMotor = true, motorSpeed = -120, maxMotorForce = 1e6})
    local crate = self:dynamic(x + 10, Joints.liftBottom - 40)
    crate:addBox(46, 46)
    Look.dress(crate, 'woodCrate')
end

-- A beam of planks held by welds, rigid at the wall and soft further out, so it sags a little and springs back.
function Joints:weld(x, y)
    self:title('Weld', x, Joints.shelf + 46)
    local wall = self:static(x - 170, y + 20, 30, 170, 'stone')
    local previous = wall
    local face = x - 155
    self.welds = {}
    for index = 1, 5 do
        local link = self:dynamic(face + (index - 0.5) * 56, y)
        link:addBox(56, 24)
        Look.dress(link, 'wood')
        self.world:createJoint('weld', previous, link, {ax = face + (index - 1) * 56, ay = y, hertz = Joints.weldHertz[index], dampingRatio = 0.7})
        self.welds[index] = {a = previous, b = link}
        previous = link
    end
end

function Joints:wheel(x, y)
    self:title('Wheel on a spring', x, Joints.shelf + 46)
    local cart = self:dynamic(x, Joints.ground - 104)
    cart:addBox(170, 32)
    Look.dress(cart, 'wood')
    self.cart, self.cartHome = cart, x
    for _, side in ipairs({-58, 58}) do
        local wheel = self:dynamic(x + side, Joints.ground - 27)
        wheel:addCircle(26, {friction = 0.9})
        Look.dress(wheel, 'wheel')
        local joint = self.world:createJoint('wheel', cart, wheel, {ax = x + side, ay = Joints.ground - 27, axisX = 0, axisY = 1, enableSpring = true, hertz = 3, dampingRatio = 0.4, enableLimit = true, lower = -30, upper = 30, enableMotor = true, motorSpeed = 3, maxMotorTorque = 2e6})
        self.wheels[#self.wheels + 1] = {joint = joint, body = wheel, side = side}
    end
    local bump = self.world:createBody({type = 'static', x = x, y = Joints.ground})
    bump:addPolygon({{-46, 0}, {0, -16}, {46, 0}})
    Look.dress(bump, 'stone')
    self.statics[#self.statics + 1] = bump
end

-- A kinematic body circles around, and the motor joint pulls the block toward the offset it had, lagging behind with a limited force.
function Joints:motor(x, y)
    self:title('Motor', x, Joints.shelf + 46)
    self.leader = self.world:createBody({type = 'kinematic', x = x + 80, y = y + 20})
    self.leader:addCircle(16)
    Look.dress(self.leader, 'gold')
    self.statics[#self.statics + 1] = self.leader
    self.leaderCenter = {x = x, y = y + 20}
    self.leaderPath = {}
    for index = 0, 47 do
        local angle = index / 48 * math.pi * 2
        self.leaderPath[#self.leaderPath + 1] = x + math.cos(angle) * 80
        self.leaderPath[#self.leaderPath + 1] = y + 20 + math.sin(angle) * 80
    end
    self.follower = self:dynamic(x + 150, y + 20, {gravityScale = 0})
    self.follower:addBox(54, 54)
    Look.dress(self.follower, 'purple')
    self.world:createJoint('motor', self.leader, self.follower, {maxMotorForce = 3000, maxMotorTorque = 30000})
end

-- The filtered pair falls through each other, while the plain pair stacks.
function Joints:filter(x, y)
    self:title('Filter', x, Joints.shelf + 46)
    local lower = self:dynamic(x - 85, Joints.ground - 30)
    lower:addBox(60, 60)
    Look.dress(lower, 'teal')
    self.ghost = self:dynamic(x - 55, Joints.ground - 190)
    self.ghost:addBox(60, 60)
    Look.dress(self.ghost, Look.tint('teal', '#882FB3C4', '#881E8494', '#AA9BE9F2'))
    self.world:createJoint('filter', lower, self.ghost)
    local plainLower = self:dynamic(x + 85, Joints.ground - 30)
    plainLower:addBox(60, 60)
    Look.dress(plainLower, 'orange')
    local plainUpper = self:dynamic(x + 85, Joints.ground - 190)
    plainUpper:addBox(60, 60)
    Look.dress(plainUpper, 'orange')
    self.filterCaptions = {{text = 'Passes through', x = x - 70}, {text = 'Stacks', x = x + 85}}
end

function Joints:reverse()
    self.direction = -self.direction
    self.windmill.motorSpeed = 1.5 * self.direction
    self:driveCart(self.direction)
end

function Joints:driveCart(direction)
    self.cartDirection = direction
    for _, wheel in ipairs(self.wheels) do
        wheel.joint.motorSpeed = 3 * direction
    end
end

function Joints:update(dt)
    Joints.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self:status(string.format('Eight joints at work: the windmill turns at %d degrees a second and the lift is %d units up its rail.', PhysicsTest.degrees(self.windmill.motorSpeed), math.floor(Joints.liftBottom - self.lift.y + 0.5)))
end

-- The lift turns around at the ends of its rail, the cart turns around at the ends of its compartment and the leader of the motor joint keeps circling.
function Joints:fixedUpdate(step)
    self.time = self.time + step
    if self.lift.y <= Joints.liftTop then
        self.piston.motorSpeed = 120
    elseif self.lift.y >= Joints.liftBottom then
        self.piston.motorSpeed = -120
    end
    local offset = self.cart.x - self.cartHome
    if offset > 90 and self.cartDirection > 0 or offset < -90 and self.cartDirection < 0 then
        self:driveCart(-self.cartDirection)
    end
    local center = self.leaderCenter
    local x = center.x + math.cos(self.time * 1.5) * 80
    local y = center.y + math.sin(self.time * 1.5) * 80
    self.leader.velocity = {(x - self.leader.x) / step, (y - self.leader.y) / step}
    self:simulate(self.world, step)
end

function Joints:drawJoints()
    local unit = Look.pixel
    for _, spring in ipairs(self.springs) do
        local body = spring.body
        local tx, ty = body.x, body.y
        if spring.offset then
            local cos, sin = math.cos(body.rotation), math.sin(body.rotation)
            tx, ty = body.x + spring.offset[1] * cos - spring.offset[2] * sin, body.y + spring.offset[1] * sin + spring.offset[2] * cos
        end
        if spring.kind == 'spring' then
            Look.spring(spring.x, spring.y, tx, ty, 22, 7, '#FFA9BCD3', 0)
        else
            Look.line(spring.x, spring.y, tx, ty, 4 * unit, '#FFE8D3A8', 0)
        end
        Look.pin(spring.x, spring.y, 7 * unit, 2)
    end
    Look.pin(self.hub.x, self.hub.y, 12 * unit, 2)

    local hinge = self.hinge
    graphics2d.drawArc(hinge.x, hinge.y, 150, 4 * unit, hinge.lower, hinge.upper, '#99FFFFFF', {layer = -1})
    Look.pin(hinge.x, hinge.y, 8 * unit, 2)

    for _, wheel in ipairs(self.wheels) do
        local cart = self.cart
        local cos, sin = math.cos(cart.rotation), math.sin(cart.rotation)
        local ax, ay = cart.x + wheel.side * cos, cart.y + wheel.side * sin
        Look.spring(ax, ay, wheel.body.x, wheel.body.y, 16, 4, '#FFFFC93C', -1)
    end

    graphics2d.drawStroke(self.leaderPath, {width = 3 * unit, color = '#992F3352', closed = true, dashes = {10 * unit, 8 * unit}, layer = -1})
    Look.dashed(self.leader.x, self.leader.y, self.follower.x, self.follower.y, '#CC2F3352', 2)

    for _, weld in ipairs(self.welds) do
        local b = weld.b
        local cos, sin = math.cos(b.rotation), math.sin(b.rotation)
        Look.pin(b.x - 28 * cos, b.y - 28 * sin, 5 * unit, 2, '#FF7088A6')
    end
end

function Joints:draw(area)
    Scenery.sky(self.camera, Joints.ground)
    Scenery.ground(Joints.ground)
    Look.drawAll(self.statics)
    Look.drawAll(self.bodies, {layer = 1})
    self:drawJoints()
    for _, label in ipairs(self.labels) do
        self:label(label.text, label.x, label.y, {size = 20})
    end
    for _, caption in ipairs(self.filterCaptions) do
        self:tag(caption.text, caption.x, Joints.ground - 150, {size = 17})
    end
    self.grab:draw()
    if self.debug then
        self.world:debugDraw({layer = 20})
    end
end

return Joints
