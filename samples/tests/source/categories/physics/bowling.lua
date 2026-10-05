-- A bowling lane seen from above: a heavy ball that linear damping slows as it rolls, aimed and thrown with the pointer, the keys or a stick, ten light pins, gutter sensors and the count of the pins down.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Bowling = haylen.class('Bowling', PhysicsTest)

Bowling.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'throw', type = 'button', bindings = {'key:space', 'button:south', 'virtual:throw'}},
}

local kLaneHalf = 110
local kGutter = 60
local kFoulLine = -620
local kBallRadius = 26
local kPinRadius = 13
local kHeadPin = 380
local kMaxSpeed = 1500
local kMaxAim = 0.25
local kChargeTime = 1.2
local kSettleTime = 2

function Bowling:enter()
    self:frame{
        hint = 'Drag back from the ball and let go to throw, or move it across the line with W and S, aim with A and D or the left stick and hold Space or the south button to charge the throw. R or the X button sets the pins again.',
        controls = {
            ui.button{id = 'reset', text = 'Set the pins again', onClick = function() self:rack() end},
        },
        play = true,
        actions = Bowling.actions,
        overlay = {
            ui.touchStick{action = 'move', radius = 110, floating = true, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
            ui.touchButton{action = 'throw', text = 'Throw', size = 150, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self.world = physics2d.newWorld({gravity = {0, 0}})
    local alley = self.world:createBody({type = 'static'})
    local outer = kLaneHalf + kGutter
    parts.box(alley, 1500, 30, {offsetY = -outer - 15, restitution = 0.3})
    parts.box(alley, 1500, 30, {offsetY = outer + 15, restitution = 0.3})
    parts.box(alley, 30, outer * 2 + 60, {offsetX = 735, restitution = 0.1})
    parts.paint(alley, '#FF4E342E')
    self.alley = alley
    for _, side in ipairs({-1, 1}) do
        local gutter = self.world:createBody({type = 'static', x = 0, y = side * (kLaneHalf + kGutter / 2)})
        gutter:addBox(1440, kGutter, {sensor = true})
    end
    self.world.onSensorBegin = function(sensor, visitor, shapes)
        if visitor == self.ball then
            self.inGutter = true
        end
    end
    self.throws, self.strikes, self.best = 0, 0, 0
    self:rack()
end

function Bowling:rack()
    for _, body in ipairs(self.pins or {}) do
        body:destroy()
    end
    if self.ball then
        self.ball:destroy()
    end
    self.pins, self.spots = {}, {}
    for row = 0, 3 do
        for index = 0, row do
            local x, y = kHeadPin + row * 34, (index - row / 2) * 40
            local pin = self.world:createBody({x = x, y = y, linearDamping = 2.2, angularDamping = 2})
            parts.circle(pin, kPinRadius, {density = 0.4, restitution = 0.5, friction = 0.3})
            parts.paint(pin, '#FFF5F5F5')
            self.pins[#self.pins + 1] = pin
            self.spots[#self.spots + 1] = {x, y}
        end
    end
    self.ball = self.world:createBody({x = kFoulLine - 30, y = 0, linearDamping = 0.12, angularDamping = 0.5})
    parts.circle(self.ball, kBallRadius, {density = 6, restitution = 0.1, friction = 0.2})
    parts.paint(self.ball, '#FF3949AB')
    self.state, self.aim, self.power, self.charging, self.inGutter, self.settle = 'aiming', 0, 0, false, false, 0
end

function Bowling:pinsDown()
    local count = 0
    for index, pin in ipairs(self.pins) do
        local spot = self.spots[index]
        if math.abs(pin.x - spot[1]) > 14 or math.abs(pin.y - spot[2]) > 14 then
            count = count + 1
        end
    end
    return count
end

function Bowling:throw()
    if self.state == 'aiming' and self.power > 0.05 then
        local speed = self.power * kMaxSpeed * self.ball.mass
        self.ball:applyImpulse(math.cos(self.aim) * speed, math.sin(self.aim) * speed)
        self.state, self.throws = 'rolling', self.throws + 1
    end
    self.power, self.charging, self.dragging = 0, false, false
end

function Bowling:exit()
    Bowling.super.exit(self)
    input.clearVirtual()
end

function Bowling:update(dt)
    Bowling.super.update(self, dt)
    if input.pressed('reset') then
        self:rack()
    end
    local pointer, ball = self.pointer, self.ball
    if self.state == 'aiming' then
        if pointer.pressed and not ui.usingPointer() then
            self.dragging = true
        end
        if self.dragging then
            local dx, dy = ball.x - pointer.worldX, ball.y - pointer.worldY
            self.aim = m.clamp(math.atan(dy, math.max(dx, 1)), -kMaxAim, kMaxAim)
            self.power = math.min(1, math.sqrt(dx * dx + dy * dy) / 300)
            if pointer.released then
                self:throw()
            end
        end
        local moveX, moveY = input.vector('move')
        if moveY ~= 0 then
            ball:setTransform(kFoulLine - 30, m.clamp(ball.y + moveY * 200 * dt, -kLaneHalf + kBallRadius + 2, kLaneHalf - kBallRadius - 2), 0)
        end
        self.aim = m.clamp(self.aim + moveX * 0.3 * dt, -kMaxAim, kMaxAim)
        if input.down('throw') then
            self.charging = true
            self.power = math.min(1, self.power + dt / kChargeTime)
        elseif self.charging then
            self:throw()
        end
    end
    local down = self:pinsDown()
    self:status(string.format('Pins down %d of 10, throws %d, strikes %d, best %d, ball %.0f units per second%s, aim %.1f degrees, power %.0f percent', down, self.throws, self.strikes, self.best, ball.velocity:length(), self.inGutter and ' in the gutter' or '', math.deg(self.aim), self.power * 100))
end

-- A ball in a gutter runs straight down it, and once the ball stops or reaches the pit the pins settle, are counted and are set again.
function Bowling:fixedUpdate(step)
    local ball = self.ball
    if self.inGutter then
        ball.velocity = {ball.velocity.x, 0}
    end
    if self.state == 'rolling' and (ball.x > kHeadPin + 200 or ball.velocity:length() < 10) then
        self.state, self.settle = 'settling', kSettleTime
    elseif self.state == 'settling' then
        self.settle = self.settle - step
        if self.settle <= 0 then
            local down = self:pinsDown()
            self.best = math.max(self.best, down)
            self.strikes = self.strikes + (down == 10 and 1 or 0)
            self:rack()
        end
    end
    self:simulate(self.world, step)
end

function Bowling:draw(area)
    local outer = kLaneHalf + kGutter
    graphics2d.drawRect({-750, -kLaneHalf, 1470, kLaneHalf * 2}, '#FFD7B98E', {layer = -3})
    graphics2d.drawRect({-750, -outer, 1470, kGutter}, '#FF37474F', {layer = -3})
    graphics2d.drawRect({-750, kLaneHalf, 1470, kGutter}, '#FF37474F', {layer = -3})
    graphics2d.drawLine(kFoulLine, -kLaneHalf, kFoulLine, kLaneHalf, 4, '#FFC62828', {layer = -2})
    for index = -2, 2 do
        local x, y = -260 + math.abs(index) * 30, index * 40
        graphics2d.drawPolygon({{x - 14, y - 8}, {x + 14, y}, {x - 14, y + 8}}, '#996D4C41', {layer = -2})
    end
    parts.draw(self.alley)
    for _, pin in ipairs(self.pins) do
        parts.draw(pin, {layer = 1})
        graphics2d.drawRing(pin.x, pin.y, kPinRadius - 4, 3, '#FFE53935', {layer = 2})
    end
    parts.draw(self.ball, {layer = 2})
    if self.state == 'aiming' then
        local ball, dx, dy = self.ball, math.cos(self.aim), math.sin(self.aim)
        graphics2d.drawLine(ball.x + dx * 40, ball.y + dy * 40, ball.x + dx * (60 + self.power * 300), ball.y + dy * (60 + self.power * 300), 6, '#CCFFD54F', {layer = 3})
    end
    graphics2d.drawText(nil, string.format('Pins down %d', self:pinsDown()), 0, -outer - 80, {size = 40, color = '#FFE8EAF2', anchor = {0.5, 0.5}})
end

return Bowling
