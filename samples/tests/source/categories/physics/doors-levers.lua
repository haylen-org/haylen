-- Mechanisms made of joints in a castle yard: flap doors on limited hinges that rolling balls push open and springs close, a lever that opens an iron gate on a prismatic motor when pulled to its end, and a pressure plate sensor that raises a drawbridge over a moat.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local DoorsLevers = haylen.class('DoorsLevers', PhysicsTest)

DoorsLevers.code = [[
world:createJoint('revolute', beam, door, {ax = x, ay = -170, enableLimit = true, lower = -1.3, upper = 1.3, enableSpring = true, hertz = 1.5})
local gate = world:createJoint('prismatic', beam, slab, {ax = 40, ay = -115, axisY = 1, enableLimit = true, lower = -120, upper = 0, enableMotor = true})
local lever = world:createJoint('revolute', ground, handle, {ax = -560, ay = 300, enableLimit = true, upper = 1.2, enableMotor = true, maxMotorTorque = 8e4})
gate.motorSpeed = lever.angle >= 1.15 and -300 or 300
drawbridge.motorSpeed = #plate:overlaps() > 0 and -1.5 or 1.5  -- The plate holds the bridge up.]]

DoorsLevers.worldCategory, DoorsLevers.beamCategory = 1, 2
DoorsLevers.ground = 300
DoorsLevers.doors = {-300, -170}
DoorsLevers.gateX = 40
DoorsLevers.lever = {-560, 300}
DoorsLevers.leverSweep = 1.2
DoorsLevers.leverFriction = 8e4
DoorsLevers.plate = {330, 283}
DoorsLevers.hinge = {430, 300}
DoorsLevers.spawnInterval = 1.6
DoorsLevers.maxBalls = 14
DoorsLevers.moat = {430, 630, 340}
DoorsLevers.ramp = {{-790, -290}, {-420, -70}, {-420, -50}, {-790, -270}}

function DoorsLevers:enter()
    self:frame{
        code = DoorsLevers.code,
        hint = 'Balls roll down through the flap doors. Drag the lever to its right end to open the gate and back to close it, and put a crate on the plate to raise the drawbridge. R or the X button starts over.',
        controls = {
            ui.toggle{id = 'rain', text = 'Roll balls', checked = true, onChange = function(event) self.raining = event.checked end},
            ui.button{id = 'ball', text = 'Roll a ball', onClick = function() self:rollBall() end},
            ui.button{id = 'lever', text = 'Pull the lever', onClick = function() self:pullLever() end},
            ui.button{id = 'crate', text = 'Drop a crate on the plate', onClick = function() self:dropCrate() end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'rain',
    }
    self.camera.position = {0, 30}
    self.raining = true
    self.random = m.random(43)
    self.ramp = Look.newShape(DoorsLevers.ramp, 'wood')
    self.chain = {0, 0, 0, 0}
    self:build()
end

function DoorsLevers:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.balls, self.crates, self.clock, self.open, self.pulling = {}, {}, 0, false, nil
    self.across = 0

    local ground = DoorsLevers.ground
    self.ground = self.world:createBody({type = 'static'})
    self.ground:addBox(1230, 400, {offsetX = -185, offsetY = ground + 200})
    self.ground:addBox(170, 400, {offsetX = 715, offsetY = ground + 200})
    self.ground:addBox(24, 760, {offsetX = 790, offsetY = ground - 380})
    self.ground:addPolygon(DoorsLevers.ramp)
    self.ground:addBox(680, 20, {offsetX = -80, offsetY = -50})
    local beam = self.world:createBody({type = 'static'})
    beam:addBox(680, 24, {offsetX = -80, offsetY = -182, category = DoorsLevers.beamCategory})

    -- Each door hangs from the beam, swings either way up to its limits and its spring pulls it shut.
    self.doors = {}
    for _, x in ipairs(DoorsLevers.doors) do
        local door = self.world:createBody({x = x, y = -170})
        door:addBox(16, 100, {offsetY = 50, density = 0.5})
        Look.dress(door, 'wood')
        self.doors[#self.doors + 1] = {body = door, joint = self.world:createJoint('revolute', beam, door, {ax = x, ay = -170, enableLimit = true, lower = -1.3, upper = 1.3, enableSpring = true, hertz = 1.5, dampingRatio = 0.6})}
    end

    -- The gate leaves out the beam from its mask, so it slides up into it.
    self.gate = self.world:createBody({x = DoorsLevers.gateX, y = -115})
    self.gate:addBox(28, 108, {mask = DoorsLevers.worldCategory})
    Look.dress(self.gate, 'iron')
    self.slider = self.world:createJoint('prismatic', beam, self.gate, {ax = DoorsLevers.gateX, ay = -115, axisX = 0, axisY = 1, enableLimit = true, lower = -120, upper = 0, enableMotor = true, motorSpeed = 300, maxMotorForce = 2e4})

    -- A motor at speed 0 with a small torque holds the lever wherever it is left, like friction in its hinge.
    local lever = DoorsLevers.lever
    self.lever = self.world:createBody({x = lever[1], y = lever[2], rotation = -DoorsLevers.leverSweep / 2})
    self.lever:addBox(16, 150, {offsetY = -75})
    self.lever:addCircle(18, {offsetY = -150})
    self.leverJoint = self.world:createJoint('revolute', self.ground, self.lever, {ax = lever[1], ay = lever[2], enableLimit = true, lower = 0, upper = DoorsLevers.leverSweep, enableMotor = true, maxMotorTorque = DoorsLevers.leverFriction})

    local plate = self.world:createBody({type = 'static', x = DoorsLevers.plate[1], y = DoorsLevers.plate[2]})
    self.plateShape = plate:addBox(110, 26, {sensor = true})
    local hinge = DoorsLevers.hinge
    self.bridge = self.world:createBody({x = hinge[1], y = hinge[2]})
    self.bridge:addBox(200, 18, {offsetX = 100, offsetY = 9, friction = 0.8})
    Look.dress(self.bridge, 'wood')
    self.drawbridge = self.world:createJoint('revolute', self.ground, self.bridge, {ax = hinge[1], ay = hinge[2], enableLimit = true, lower = 0, upper = 1.25, enableMotor = true, motorSpeed = 1.5, maxMotorTorque = 2e7})
    self:rollBall()
end

function DoorsLevers:rollBall()
    local ball = self.world:createBody({x = -750 + self.random:range(-10, 10), y = -330})
    ball:addCircle(22, {friction = 0.4, density = 1.5})
    Look.dress(ball, 'stone')
    self.balls[#self.balls + 1] = {body = ball}
    if #self.balls > DoorsLevers.maxBalls then
        table.remove(self.balls, 1).body:destroy()
    end
end

function DoorsLevers:dropCrate()
    local crate = self.world:createBody({x = DoorsLevers.plate[1], y = 100})
    crate:addBox(60, 60, {density = 2, friction = 0.9})
    Look.dress(crate, 'woodCrate')
    self.crates[#self.crates + 1] = crate
end

-- Drives the lever with its motor toward the end it is farther from.
function DoorsLevers:pullLever()
    self.pulling = self.leverJoint.angle < DoorsLevers.leverSweep / 2 and 1 or -1
end

function DoorsLevers:update(dt)
    DoorsLevers.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local pressed = #self.plateShape:overlaps()
    local gate = self.open and 'the lever is pulled and the gate open' or 'the lever is up and the gate shut'
    local bridge = pressed > 0 and 'the plate holds the drawbridge up' or 'the drawbridge lies down'
    self:status(string.format('Now %s, and %s. %d balls crossed the moat.', gate, bridge, self.across))
end

-- The lever switches the gate only at the ends of its travel, and the plate holds the bridge up while anything presses it.
function DoorsLevers:fixedUpdate(step)
    self.clock = self.clock + step
    if self.raining and self.clock >= DoorsLevers.spawnInterval then
        self.clock = 0
        self:rollBall()
    end
    local lever = self.leverJoint
    if lever.angle >= DoorsLevers.leverSweep - 0.05 then
        self.open = true
        self.pulling = self.pulling ~= 1 and self.pulling or nil
    elseif lever.angle <= 0.05 then
        self.open = false
        self.pulling = self.pulling ~= -1 and self.pulling or nil
    end
    lever.motorSpeed = self.pulling and self.pulling * 3 or 0
    lever.maxMotorTorque = self.pulling and 1e6 or DoorsLevers.leverFriction
    self.slider.motorSpeed = self.open and -300 or 300
    self.drawbridge.motorSpeed = #self.plateShape:overlaps() > 0 and -1.5 or 1.5
    self:simulate(self.world, step)
    for index = #self.balls, 1, -1 do
        local ball = self.balls[index]
        if not ball.across and ball.body.x > 650 then
            ball.across = true
            self.across = self.across + 1
        end
        if ball.body.y > 500 then
            table.remove(self.balls, index).body:destroy()
        end
    end
end

-- The castle: a grassy yard on both banks of the moat between stone walls, a tower, the frame of the shelf and the beam on posts, the gate in its frame, the lever on its base, the plate set into the ground and the chains of the drawbridge on a post.
function DoorsLevers:drawCastle()
    local ground = DoorsLevers.ground
    local unit = Look.pixel
    local moat = DoorsLevers.moat
    Scenery.ground(ground, nil, moat[1])
    Scenery.ground(ground, moat[2], nil)
    for _, x in ipairs({moat[1] + 8, moat[2] - 8}) do
        Look.block(x, ground + 200, 16, 400, 'stone', -7)
    end
    Look.block(812, ground - 380, 70, 760, 'brick', -7)
    Look.drawShape(self.ramp, -7)
    Look.block(-80, -50, 680, 20, 'wood', -7)
    Look.block(-80, -182, 680, 24, 'darkWood', -7)
    for _, x in ipairs({-430, 270}) do
        Look.block(x, (-182 + ground) / 2, 18, ground + 182, 'darkWood', -8)
    end
    Look.block(-780, (-280 + ground) / 2, 16, ground + 280, 'darkWood', -8)
    for _, side in ipairs({-1, 1}) do
        Look.block(DoorsLevers.gateX + side * 22, -112, 10, 120, 'iron', -6)
    end
    local lever = DoorsLevers.lever
    Look.block(lever[1], lever[2] - 8, 70, 18, 'iron', 2)
    local pressed = #self.plateShape:overlaps() > 0
    local plate = DoorsLevers.plate
    Look.block(plate[1], ground - (pressed and 3 or 6), 110, pressed and 6 or 12, pressed and 'green' or 'yellow', 1)
    Look.block(390, (ground + 130) / 2, 22, ground - 130, 'darkWood', 0)
    local bridge = self.bridge
    local cos, sin = math.cos(bridge.rotation), math.sin(bridge.rotation)
    self.chain[1], self.chain[2] = 390, 140
    self.chain[3], self.chain[4] = bridge.x + cos * 196, bridge.y + sin * 196
    Look.line(self.chain[1], self.chain[2], self.chain[3], self.chain[4], 5 * unit, '#FF9AA5BE', 0)
    Look.pin(390, 140, 7 * unit, 1)
end

function DoorsLevers:draw(area)
    Scenery.sky(self.camera, DoorsLevers.ground, {hills = false})
    local moat = DoorsLevers.moat
    Scenery.water(moat[1], moat[2], moat[3], {layer = -9})
    self:drawCastle()
    for _, door in ipairs(self.doors) do
        Look.draw(door.body, {layer = 1})
    end
    Look.draw(self.gate, {layer = 1})
    Look.draw(self.bridge, {layer = 1})
    local lever = self.lever
    local cos, sin = math.cos(lever.rotation), math.sin(lever.rotation)
    Look.block(lever.x + sin * 75, lever.y - cos * 75, 16, 150, 'darkWood', 1, lever.rotation)
    Look.disc(lever.x + sin * 150, lever.y - cos * 150, 18, 'red', 1)
    Look.drawAll(self.crates, {layer = 2})
    for _, ball in ipairs(self.balls) do
        Look.draw(ball.body, {layer = 2})
    end
    Look.pin(DoorsLevers.lever[1], DoorsLevers.lever[2], 9 * Look.pixel, 3)
    self.grab:draw()
end

return DoorsLevers
