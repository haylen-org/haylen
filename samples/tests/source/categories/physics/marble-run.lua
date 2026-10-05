-- Marbles rolling down ramps made of chains, through a funnel, over a paddle wheel turned by a revolute motor and back up in a kinematic lift that `body:moveTo` drives, in an endless loop.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local MarbleRun = haylen.class('MarbleRun', PhysicsTest)

-- Chains collide on their upper side when listed from left to right, and the short first segments are lips that turn marbles back.
local kTrack = {
    {{-520, -190}, {600, -295}},
    {{-680, -240}, {-660, -160}, {-100, -90}},
    {{-160, -100}, {-22, -10}},
    {{22, -10}, {160, -100}},
    {{-150, 160}, {-130, 235}, {600, 330}},
}
local kWheel = {60, 120}
local kLift = {x = 680, bottom = 370, top = -330, speed = 300, carry = 0.12, dump = -0.4, turn = 1.5}
local kLoadTime, kDumpTime = 2.5, 1.6
local kMarbleRadius = 14
local kMaxMarbles = 24
local kStates = {loading = 'loading', rising = 'rising', dumping = 'tipping out', lowering = 'going down'}

function MarbleRun:enter()
    self:frame{
        hint = 'Marbles roll down the ramps, drop through the funnel, ride over the paddle wheel and wait at the gate for the lift that carries them back to the top. Drag marbles anywhere. R or the X button starts over.',
        controls = {
            ui.button{id = 'marble', text = 'Add a marble', onClick = function() self:addMarble(self.random:range(-400, 400), -380) end},
            ui.label{text = 'Wheel motor speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'wheel', value = 2, min = 0, max = 5, step = 0.25, showValue = true, onChange = function(event) self:setWheelSpeed(event.value) end},
            ui.toggle{id = 'lift', text = 'Run the lift', checked = true, onChange = function(event) self.running = event.checked end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'marble',
    }
    self.random = m.random(58)
    self.running, self.wheelSpeed = true, 2
    self:build()
end

function MarbleRun:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.marbles, self.trips = {}, 0

    local track = self.world:createBody({type = 'static'})
    parts.box(track, 1600, 40, {offsetY = 410})
    for _, points in ipairs(kTrack) do
        parts.chain(track, points, false, {friction = 0.4})
    end
    self.track = track

    local hub = self.world:createBody({type = 'static', x = kWheel[1], y = kWheel[2]})
    self.wheel = self.world:createBody({x = kWheel[1], y = kWheel[2]})
    parts.circle(self.wheel, 24)
    for paddle = 0, 5 do
        local angle = paddle * math.pi / 3
        parts.box(self.wheel, 70, 10, {offsetX = math.cos(angle) * 57, offsetY = math.sin(angle) * 57, rotation = angle})
    end
    parts.paint(self.wheel, '#FF4DB6AC')
    self.motor = self.world:createJoint('revolute', hub, self.wheel, {ax = kWheel[1], ay = kWheel[2], enableMotor = true, motorSpeed = self.wheelSpeed, maxMotorTorque = 1e7})

    self.gate = self.world:createBody({type = 'static', x = 604, y = 296})
    self.gate:addBox(8, 56)
    self.lift = self.world:createBody({type = 'kinematic', x = kLift.x, y = kLift.bottom, rotation = kLift.carry})
    parts.box(self.lift, 150, 14)
    parts.box(self.lift, 12, 70, {offsetX = 69, offsetY = -42})
    parts.paint(self.lift, '#FFFFB74D')
    self:setLift('loading')

    for index = 1, 12 do
        self:addMarble(-450 + index * 75, -360)
    end
end

function MarbleRun:addMarble(x, y)
    local marble = self.world:createBody({x = x, y = y})
    parts.circle(marble, kMarbleRadius, {density = 2, friction = 0.4, restitution = 0.15, rollingResistance = 0.02})
    self.marbles[#self.marbles + 1] = marble
    if #self.marbles > kMaxMarbles then
        table.remove(self.marbles, 1):destroy()
    end
end

function MarbleRun:setWheelSpeed(speed)
    self.wheelSpeed = speed
    self.motor.motorSpeed = speed
end

-- The gate holds the marbles back whenever the lift is away from the bottom.
function MarbleRun:setLift(state)
    self.liftState, self.liftTime = state, 0
    self.gate.enabled = state ~= 'loading'
    if state == 'rising' then
        self.trips = self.trips + 1
    end
end

-- The lift waits at the bottom, rises tilted back against its wall, tips forward at the top and goes down again, each step a `moveTo` toward where it should be next.
function MarbleRun:moveLift(step)
    local lift, state = self.lift, self.liftState
    self.liftTime = self.liftTime + step
    local targetY, targetRotation = kLift.bottom, kLift.carry
    if state == 'loading' and self.running and self.liftTime > kLoadTime then
        self:setLift('rising')
    elseif state == 'rising' then
        targetY = kLift.top
        if lift.y <= kLift.top + 0.5 then
            self:setLift('dumping')
        end
    elseif state == 'dumping' then
        targetY, targetRotation = kLift.top, kLift.dump
        if self.liftTime > kDumpTime then
            self:setLift('lowering')
        end
    elseif state == 'lowering' and lift.y >= kLift.bottom - 0.5 then
        self:setLift('loading')
    end
    local y = lift.y + m.clamp(targetY - lift.y, -kLift.speed * step, kLift.speed * step)
    local rotation = lift.rotation + m.clamp(targetRotation - lift.rotation, -kLift.turn * step, kLift.turn * step)
    lift:moveTo(kLift.x, y, rotation)
end

function MarbleRun:update(dt)
    MarbleRun.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self:status(string.format('Marbles %d, lift %s, trips %d, wheel %.0f turns a minute, step %.2f ms', #self.marbles, kStates[self.liftState], self.trips, self.wheel.angularVelocity * 60 / (math.pi * 2), self:stepTime()))
end

function MarbleRun:fixedUpdate(step)
    self:moveLift(step)
    self:simulate(self.world, step)
end

function MarbleRun:draw(area)
    graphics2d.drawLine(kLift.x, -430, self.lift.x, self.lift.y - 80, 3, '#FF90A4AE', {layer = -1})
    graphics2d.drawRect({kLift.x - 84, -430, 4, 820}, '#FF37474F', {layer = -1})
    graphics2d.drawRect({kLift.x + 80, -430, 4, 820}, '#FF37474F', {layer = -1})
    parts.draw(self.track)
    graphics2d.drawRect({600, 268, 8, 56}, self.gate.enabled and '#FFE57373' or '#33E57373', {layer = 1})
    parts.draw(self.wheel, {layer = 1})
    parts.draw(self.lift, {layer = 1})
    parts.drawAll(self.marbles, {layer = 2})
    self.grab:draw()
end

return MarbleRun
