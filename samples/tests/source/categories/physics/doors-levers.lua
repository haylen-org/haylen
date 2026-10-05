-- Mechanisms made of joints: flap doors on limited hinges that rolling balls push open and springs close, a lever that opens a gate on a prismatic motor when pulled to its end, and a pressure plate sensor that raises a drawbridge.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local DoorsLevers = haylen.class('DoorsLevers', PhysicsTest)

local kWorld, kBeam = 1, 2
local kDoors = {-300, -170}
local kGateX = 40
local kLever = {-560, 300}
local kLeverSweep = 1.2
local kLeverFriction = 8e4
local kPlate = {330, 283}
local kHinge = {430, 300}
local kSpawnInterval = 1.6
local kMaxBalls = 14

function DoorsLevers:enter()
    self:frame{
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
    self.raining = true
    self.random = m.random(43)
    self:build()
end

function DoorsLevers:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.balls, self.crates, self.clock, self.open, self.pulling = {}, {}, 0, false, nil

    self.ground = self.world:createBody({type = 'static'})
    parts.box(self.ground, 1230, 130, {offsetX = -185, offsetY = 365})
    parts.box(self.ground, 170, 130, {offsetX = 715, offsetY = 365})
    parts.box(self.ground, 20, 860, {offsetX = 790})
    parts.polygon(self.ground, {{-790, -290}, {-420, -70}, {-420, -50}, {-790, -270}})
    parts.box(self.ground, 680, 20, {offsetX = -80, offsetY = -50})
    local beam = self.world:createBody({type = 'static'})
    parts.box(beam, 680, 24, {offsetX = -80, offsetY = -182, category = kBeam})
    self.statics = {self.ground, beam}

    -- Each door hangs from the beam, swings either way up to its limits and its spring pulls it shut.
    self.doors = {}
    for _, x in ipairs(kDoors) do
        local door = self.world:createBody({x = x, y = -170})
        parts.box(door, 14, 100, {offsetY = 50, density = 0.5})
        parts.paint(door, '#FF8D6E63')
        self.doors[#self.doors + 1] = {body = door, joint = self.world:createJoint('revolute', beam, door, {ax = x, ay = -170, enableLimit = true, lower = -1.3, upper = 1.3, enableSpring = true, hertz = 1.5, dampingRatio = 0.6})}
    end

    -- The gate leaves out the beam from its mask, so it slides up into it.
    self.gate = self.world:createBody({x = kGateX, y = -115})
    parts.box(self.gate, 26, 108, {mask = kWorld})
    parts.paint(self.gate, '#FF78909C')
    self.slider = self.world:createJoint('prismatic', beam, self.gate, {ax = kGateX, ay = -115, axisX = 0, axisY = 1, enableLimit = true, lower = -120, upper = 0, enableMotor = true, motorSpeed = 300, maxMotorForce = 2e4})

    -- A motor at speed 0 with a small torque holds the lever wherever it is left, like friction in its hinge.
    self.lever = self.world:createBody({x = kLever[1], y = kLever[2], rotation = -kLeverSweep / 2})
    parts.box(self.lever, 16, 150, {offsetY = -75})
    parts.circle(self.lever, 16, {offsetY = -150})
    parts.paint(self.lever, '#FFE57373')
    self.leverJoint = self.world:createJoint('revolute', self.ground, self.lever, {ax = kLever[1], ay = kLever[2], enableLimit = true, lower = 0, upper = kLeverSweep, enableMotor = true, maxMotorTorque = kLeverFriction})

    local plate = self.world:createBody({type = 'static', x = kPlate[1], y = kPlate[2]})
    self.plateShape = plate:addBox(110, 26, {sensor = true})
    self.bridge = self.world:createBody({x = kHinge[1], y = kHinge[2]})
    parts.box(self.bridge, 200, 18, {offsetX = 100, offsetY = 9, friction = 0.8})
    parts.paint(self.bridge, '#FFBCAAA4')
    self.drawbridge = self.world:createJoint('revolute', self.ground, self.bridge, {ax = kHinge[1], ay = kHinge[2], enableLimit = true, lower = 0, upper = 1.25, enableMotor = true, motorSpeed = 1.5, maxMotorTorque = 2e7})
end

function DoorsLevers:rollBall()
    local ball = self.world:createBody({x = -750 + self.random:range(-10, 10), y = -330})
    parts.circle(ball, 22, {friction = 0.4, density = 1.5})
    self.balls[#self.balls + 1] = ball
    if #self.balls > kMaxBalls then
        table.remove(self.balls, 1):destroy()
    end
end

function DoorsLevers:dropCrate()
    local crate = self.world:createBody({x = kPlate[1], y = 100})
    parts.box(crate, 60, 60, {density = 2, friction = 0.9})
    parts.paint(crate, '#FFFFB74D')
    self.crates[#self.crates + 1] = crate
end

-- Drives the lever with its motor toward the end it is farther from.
function DoorsLevers:pullLever()
    self.pulling = self.leverJoint.angle < kLeverSweep / 2 and 1 or -1
end

function DoorsLevers:update(dt)
    DoorsLevers.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local across = 0
    for _, ball in ipairs(self.balls) do
        across = across + (ball.x > 640 and 1 or 0)
    end
    local pressed = #self.plateShape:overlaps()
    self:status(string.format('Doors %.0f and %.0f degrees, lever %.0f degrees, gate %s, plate pressed by %d, bridge %.0f degrees, balls across %d, step %.2f ms', math.deg(self.doors[1].joint.angle), math.deg(self.doors[2].joint.angle), math.deg(self.leverJoint.angle), self.open and 'open' or 'shut', pressed, math.deg(self.drawbridge.angle), across, self:stepTime()))
end

-- The lever switches the gate only at the ends of its travel, and the plate holds the bridge up while anything presses it.
function DoorsLevers:fixedUpdate(step)
    self.clock = self.clock + step
    if self.raining and self.clock >= kSpawnInterval then
        self.clock = 0
        self:rollBall()
    end
    local lever = self.leverJoint
    if lever.angle >= kLeverSweep - 0.05 then
        self.open = true
        self.pulling = self.pulling ~= 1 and self.pulling or nil
    elseif lever.angle <= 0.05 then
        self.open = false
        self.pulling = self.pulling ~= -1 and self.pulling or nil
    end
    lever.motorSpeed = self.pulling and self.pulling * 3 or 0
    lever.maxMotorTorque = self.pulling and 1e6 or kLeverFriction
    self.slider.motorSpeed = self.open and -300 or 300
    self.drawbridge.motorSpeed = #self.plateShape:overlaps() > 0 and -1.5 or 1.5
    self:simulate(self.world, step)
    for index = #self.balls, 1, -1 do
        if self.balls[index].y > 500 then
            table.remove(self.balls, index):destroy()
        end
    end
end

function DoorsLevers:draw(area)
    local pressed = #self.plateShape:overlaps() > 0
    parts.drawAll(self.statics)
    graphics2d.drawRect({kPlate[1] - 55, kPlate[2] + (pressed and 11 or 5), 110, 10}, pressed and '#FF6FDCA0' or '#FFFFD54F', {layer = 1})
    graphics2d.drawPolyline({{kLever[1], kLever[2] + 20}, {kGateX, kLever[2] + 20}, {kGateX, -60}}, 3, self.open and '#996FDCA0' or '#55FFFFFF', false, {layer = -1})
    graphics2d.drawPolyline({{kPlate[1], kPlate[2] + 15}, {kHinge[1], kHinge[2] + 40}}, 3, pressed and '#996FDCA0' or '#55FFFFFF', false, {layer = -1})
    for _, door in ipairs(self.doors) do
        parts.draw(door.body, {layer = 1})
    end
    parts.drawAll({self.gate, self.lever, self.bridge}, {layer = 1})
    parts.drawAll(self.crates, {layer = 2})
    parts.drawAll(self.balls, {layer = 2})
    graphics2d.drawCircle(kLever[1], kLever[2], 10, '#FF37474F', {layer = 3})
    self.grab:draw()
end

return DoorsLevers
