-- A claw machine: the carriage slides on a prismatic joint along the rail, the claw drops on a second prismatic joint, and two fingers close with motorized revolute joints that hold a prize by friction, driven with keys, a gamepad or touch.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Claw = haylen.class('Claw', PhysicsTest)

Claw.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'grip', type = 'button', bindings = {'key:space', 'key:enter', 'button:south', 'virtual:grip'}},
}

local kRail = -380
local kHead = -300
local kCarriageSpeed, kDropSpeed = 320, 260
local kFingerSpeed = 3
local kGripTorque = 60000
local kChute = {-680, 360, 180, 60}
local kPrizeColors = {'#FFF06292', '#FF4FC3F7', '#FFAED581', '#FFFFD54F', '#FFBA68C8', '#FFFF8A65'}

function Claw:enter()
    self:frame{
        hint = 'Move the claw with the arrows, WASD, the left stick or the touch stick, where up and down raise and lower it, and close or open it with Space, Enter or the south button. Drop prizes in the chute on the left.',
        controls = {
            ui.label{text = 'Grip strength', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'grip', value = 1, min = 0.2, max = 2, step = 0.1, showValue = true, onChange = function(event) self:setGrip(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = Claw.actions,
        overlay = {
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
            ui.touchButton{action = 'grip', text = 'Grab', size = 150, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self.random = m.random(60)
    self.strength = 1
    self:build()
end

function Claw:build()
    self.world = physics2d.newWorld()
    self.prizes, self.won, self.closed = {}, 0, false

    local cabinet = self.world:createBody({type = 'static'})
    parts.box(cabinet, 1600, 40, {offsetY = 410})
    parts.box(cabinet, 20, 860, {offsetX = -790})
    parts.box(cabinet, 20, 860, {offsetX = 790})
    parts.box(cabinet, 20, 160, {offsetX = -570, offsetY = 330})
    parts.box(cabinet, 1560, 16, {offsetY = kRail - 30})
    cabinet:addBox(kChute[3], kChute[4], {offsetX = kChute[1], offsetY = kChute[2], sensor = true})
    self.cabinet = cabinet

    -- The parts of the claw share a negative group, so they never collide with each other.
    self.carriage = self.world:createBody({y = kRail})
    parts.box(self.carriage, 90, 26, {group = -1})
    self.slider = self.world:createJoint('prismatic', cabinet, self.carriage, {ax = 0, ay = kRail, axisX = 1, axisY = 0, enableLimit = true, lower = -680, upper = 680, enableMotor = true, maxMotorForce = 2e5})
    self.head = self.world:createBody({y = kHead})
    parts.box(self.head, 70, 30, {group = -1})
    self.cable = self.world:createJoint('prismatic', self.carriage, self.head, {ax = 0, ay = kHead, axisX = 0, axisY = 1, enableLimit = true, lower = 0, upper = 595, enableMotor = true, maxMotorForce = 2e4})
    self.fingers = {self:finger(-1), self:finger(1)}
    for _, body in ipairs({self.carriage, self.head, self.fingers[1].body, self.fingers[2].body}) do
        parts.paint(body, '#FFB0BEC5')
    end

    for index = 1, 8 do
        self:dropPrize(-480 + index * 140, 300)
    end

    self.world.onSensorBegin = function(sensor, visitor)
        if visitor.data.prize and not visitor.data.won then
            visitor.data.won, visitor.data.leaving = true, 0.8
            self.won = self.won + 1
        end
    end
end

-- A finger hangs from a bottom corner of the head, leaning out, with a hook at its tip that points inward. Closing turns the left finger counter-clockwise and the right one clockwise.
function Claw:finger(side)
    local x, y = side * 28, kHead + 15
    local body = self.world:createBody({x = x, y = y})
    parts.box(body, 12, 72, {offsetX = side * 9, offsetY = 35, rotation = -side * 0.25, group = -1, friction = 1})
    parts.box(body, 20, 10, {offsetX = side * 12, offsetY = 74, group = -1, friction = 1})
    local lower, upper = side < 0 and -0.45 or -0.35, side < 0 and 0.35 or 0.45
    local joint = self.world:createJoint('revolute', self.head, body, {ax = x, ay = y, enableLimit = true, lower = lower, upper = upper, enableMotor = true, motorSpeed = -side * kFingerSpeed, maxMotorTorque = kGripTorque * self.strength})
    return {body = body, joint = joint, side = side}
end

function Claw:dropPrize(x, y)
    local body = self.world:createBody({x = x, y = y, rotation = self.random:range(-0.3, 0.3)})
    local kind = self.random:integer(1, 3)
    if kind == 1 then
        parts.circle(body, 28, {density = 0.6, friction = 0.9})
    elseif kind == 2 then
        parts.box(body, 52, 52, {density = 0.6, friction = 0.9})
    else
        parts.capsule(body, -14, 0, 14, 0, 20, {density = 0.6, friction = 0.9})
    end
    parts.paint(body, kPrizeColors[self.random:integer(1, #kPrizeColors)])
    body.data.prize = true
    self.prizes[#self.prizes + 1] = body
end

function Claw:setGrip(strength)
    self.strength = strength
    for _, finger in ipairs(self.fingers) do
        finger.joint.maxMotorTorque = kGripTorque * strength
    end
end

function Claw:exit()
    Claw.super.exit(self)
    input.clearVirtual()
end

function Claw:update(dt)
    Claw.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if input.pressed('grip') then
        self.closed = not self.closed
    end
    local moveX, moveY = input.vector('move')
    self.slider.motorSpeed = moveX * kCarriageSpeed
    self.cable.motorSpeed = moveY * kDropSpeed
    for _, finger in ipairs(self.fingers) do
        finger.joint.motorSpeed = (self.closed and finger.side or -finger.side) * kFingerSpeed
    end
    self:status(string.format('Carriage at %.0f, claw down %.0f, claw %s, finger torque %.0f, prizes won %d, step %.2f ms', self.slider.translation, self.cable.translation, self.closed and 'closed' or 'open', math.abs(self.fingers[1].joint.motorTorque), self.won, self:stepTime()))
end

-- Prizes that reach the chute leave after a moment, and a new one drops into the pit.
function Claw:fixedUpdate(step)
    for index = #self.prizes, 1, -1 do
        local data = self.prizes[index].data
        if data.leaving then
            data.leaving = data.leaving - step
            if data.leaving <= 0 then
                table.remove(self.prizes, index):destroy()
                self:dropPrize(self.random:range(-400, 650), -150)
            end
        end
    end
    self:simulate(self.world, step)
end

function Claw:draw(area)
    graphics2d.drawRect({kChute[1] - kChute[3] / 2, kChute[2] - kChute[4] / 2, kChute[3], kChute[4]}, '#3366BB6A', {layer = -1})
    Claw.caption('Chute', kChute[1], kChute[2] - 50, {anchor = {0.5, 0.5}})
    parts.draw(self.cabinet)
    graphics2d.drawLine(self.carriage.x, self.carriage.y, self.head.x, self.head.y, 4, '#FF78909C')
    parts.drawAll(self.prizes, {layer = 1})
    parts.draw(self.carriage, {layer = 2})
    parts.draw(self.head, {layer = 2})
    for _, finger in ipairs(self.fingers) do
        parts.draw(finger.body, {layer = 2})
    end
end

return Claw
