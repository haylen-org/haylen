-- Water from `physics2d.newFluid` held behind a dam that a button opens, pouring down a chute onto a paddle wheel on a revolute joint that it turns, and pumped from the basin back into the reservoir.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local WaterWheel = haylen.class('WaterWheel', PhysicsTest)

local kRadius = 4
local kMaxParticles = 2600
local kWheel = {110, 120}
local kGate = {x = -320, closed = -200, open = -380, speed = 400}
local kPumpLine = 300
local kPumpRate = 10
local kPumpEvery = 2

function WaterWheel:enter()
    self:frame{
        hint = 'Open the dam to let the water pour down the chute onto the wheel, whose bearing has some friction. The pump carries the water from the basin back into the reservoir while the dam is open. Drag the wheel to turn it by hand. R or the X button refills.',
        controls = {
            ui.button{id = 'dam', text = 'Open the dam', onClick = function() self:setDam(not self.damOpen) end},
            ui.checkbox{id = 'pump', text = 'Pump the water back', checked = true, onChange = function(event) self.pumping = event.checked end},
            ui.button{id = 'reset', text = 'Refill', onClick = function() self:build() end},
        },
        focus = 'dam',
    }
    self.random = m.random(59)
    self.pumping = true
    self.positions = collections.newFloatBuffer(kMaxParticles * 2)
    self:build()
end

function WaterWheel:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.steps, self.pumped, self.pumpRate, self.pumpTime = 0, 0, 0, 0

    -- Thick walls push back the particles that the pressure of the water squeezes into them.
    local walls = self.world:createBody({type = 'static'})
    parts.box(walls, 1600, 40, {offsetY = 410})
    parts.box(walls, 40, 280, {offsetX = -780, offsetY = 270})
    parts.box(walls, 40, 280, {offsetX = 780, offsetY = 270})
    parts.box(walls, 420, 30, {offsetX = -515, offsetY = -105})
    parts.box(walls, 30, 320, {offsetX = -740, offsetY = -265})
    local chute = self.world:createBody({type = 'static', x = -140, y = -90, rotation = 0.11})
    parts.box(chute, 372, 20)
    self.statics = {walls, chute}

    self.gate = self.world:createBody({type = 'kinematic', x = kGate.x, y = kGate.closed})
    parts.box(self.gate, 24, 160)
    parts.paint(self.gate, '#FF8D6E63')
    self:setDam(false)

    local axle = self.world:createBody({type = 'static', x = kWheel[1], y = kWheel[2]})
    self.wheel = self.world:createBody({x = kWheel[1], y = kWheel[2]})
    parts.circle(self.wheel, 30, {density = 0.5})
    for paddle = 0, 7 do
        local angle = paddle * math.pi / 4
        parts.box(self.wheel, 100, 12, {offsetX = math.cos(angle) * 80, offsetY = math.sin(angle) * 80, rotation = angle, density = 0.5})
    end
    parts.paint(self.wheel, '#FFFFB74D')
    self.world:createJoint('revolute', axle, self.wheel, {ax = kWheel[1], ay = kWheel[2], enableMotor = true, motorSpeed = 0, maxMotorTorque = 3000})

    self.water = physics2d.newFluid(self.world, {radius = kRadius, smoothingRadius = 16, maxParticles = kMaxParticles, viscosity = 0.2})
    self.water:fill({-705, -250, 370, 125})
    self.water:fill({-740, 330, 1480, 55})
end

function WaterWheel:setDam(open)
    self.damOpen = open
    self:set('dam', {text = open and 'Close the dam' or 'Open the dam'})
end

-- The pump takes particles from under its line in the basin and spawns them again over the reservoir, a few each time, walking the particles from the end because a removal moves the last one into the gap.
function WaterWheel:pump()
    self.water:positions(self.positions)
    local moved = 0
    for index = self.water.size, 1, -1 do
        if moved == kPumpRate then
            break
        end
        if self.positions[index * 2] > kPumpLine then
            self.water:remove(index)
            self.water:spawn(self.random:range(-690, -360), self.random:range(-420, -330))
            moved = moved + 1
        end
    end
    self.pumped = self.pumped + moved
end

function WaterWheel:update(dt)
    WaterWheel.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self.pumpTime = self.pumpTime + dt
    if self.pumpTime >= 1 then
        self.pumpRate, self.pumped, self.pumpTime = self.pumped / self.pumpTime, 0, 0
    end
    local rpm = math.abs(self.wheel.angularVelocity) * 60 / (math.pi * 2)
    self:status(string.format('Wheel %.1f turns a minute, dam %s, particles %d, pumped %.0f a second, fluid %.2f ms, step %.2f ms', rpm, self.damOpen and 'open' or 'closed', self.water.size, self.pumpRate, self.water.stepMilliseconds, self:stepTime()))
end

function WaterWheel:fixedUpdate(step)
    local target = self.damOpen and kGate.open or kGate.closed
    self.gate:moveTo(kGate.x, self.gate.y + m.clamp(target - self.gate.y, -kGate.speed * step, kGate.speed * step), 0)
    self.steps = self.steps + 1
    if self.damOpen and self.pumping and self.steps % kPumpEvery == 0 then
        self:pump()
    end
    self:simulate(self.world, step)
end

function WaterWheel:draw(area)
    graphics2d.drawRect({-790, -360, 20, 690}, '#FF455A64', {layer = -1})
    graphics2d.drawRect({-790, -360, 120, 20}, '#FF455A64', {layer = -1})
    parts.drawAll(self.statics)
    parts.draw(self.gate, {layer = 1})
    parts.draw(self.wheel, {layer = 1})
    graphics2d.drawCircle(kWheel[1], kWheel[2], 8, '#FF37474F', {layer = 2})
    self.water:draw({radius = kRadius * 2.6, color = '#D04FA3F7', outlineColor = '#FFB3E5FC', outlineWidth = 0.08, layer = 3})
    self.grab:draw()
end

return WaterWheel
