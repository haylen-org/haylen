-- A lander with two thrusters that push with `body:applyForce` at points off its center, so one thruster turns it and both lift it, with fuel, a landing speed limit and a sensor over the landing pad.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Thrusters = haylen.class('Thrusters', PhysicsTest)

Thrusters.actions = {
    {name = 'leftThruster', type = 'button', bindings = {'key:a', 'key:left', 'axis:leftTrigger+', 'button:dpadLeft', 'virtual:leftThruster'}},
    {name = 'rightThruster', type = 'button', bindings = {'key:d', 'key:right', 'axis:rightTrigger+', 'button:dpadRight', 'virtual:rightThruster'}},
    {name = 'bothThrusters', type = 'button', bindings = {'key:w', 'key:up', 'key:space', 'button:south', 'virtual:bothThrusters'}},
}

local kStart = {-560, -300}
local kThrusters = {{-18, 14}, {18, 14}}
local kFuel = 100
local kBurn = 9
local kCrashSpeed = 150
local kLandSpeed = 12
local kLandTime = 1
local kPad = {430, 242, 200}
local kGround = {{-800, -430}, {-800, 200}, {-650, 260}, {-500, 180}, {-380, 300}, {-200, 330}, {-50, 220}, {100, 290}, {250, 250}, {330, 250}, {530, 250}, {600, 150}, {700, 220}, {800, 120}, {800, -430}}

function Thrusters:enter()
    self:frame{
        hint = 'A, the left arrow or the left trigger fires the left thruster, which turns the lander right, D, the right arrow or the right trigger fires the right one, and W, Space or the south button fires both. Land slowly and upright on the pad.',
        controls = {
            ui.label{text = 'Thruster acceleration', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'thrust', value = 300, min = 200, max = 500, step = 10, showValue = true, decimals = 0, onChange = function(event) self.thrust = event.value end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = Thrusters.actions,
        overlay = {
            ui.touchButton{action = 'leftThruster', text = 'Left', size = 140, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}},
            ui.touchButton{action = 'bothThrusters', text = 'Both', size = 140, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 200}},
            ui.touchButton{action = 'rightThruster', text = 'Right', size = 140, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self.thrust = 300
    self.random = m.random(56)
    self:build()
end

function Thrusters:build()
    self.world = physics2d.newWorld({gravity = {0, 300}})
    self.fuel, self.state, self.onPad, self.still, self.touchdown = kFuel, 'Flying', 0, 0, 0
    self.throttles = {0, 0}

    self.ground = self.world:createBody({type = 'static'})
    parts.chain(self.ground, kGround, false, {friction = 0.8})
    local fill = {table.unpack(kGround, 2, #kGround - 1)}
    fill[#fill + 1] = {800, 430}
    fill[#fill + 1] = {-800, 430}
    parts.outline(self.ground, fill)
    parts.paint(self.ground, '#FF5D5A6B')
    self.pad = self.world:createBody({type = 'static', x = kPad[1], y = kPad[2]})
    parts.box(self.pad, kPad[3], 16, {friction = 0.9})
    self.pad:addBox(kPad[3], 60, {offsetY = -38, sensor = true})

    self.lander = self.world:createBody({x = kStart[1], y = kStart[2], angularDamping = 4})
    parts.polygon(self.lander, {{-36, -30}, {36, -30}, {44, 10}, {-44, 10}})
    parts.capsule(self.lander, -30, 10, -50, 40, 5)
    parts.capsule(self.lander, 30, 10, 50, 40, 5)
    parts.paint(self.lander, '#FFECEFF1')

    -- Every shape of the lander enters and leaves the sensor on its own, so a count tells whether any part is over the pad.
    self.world.onSensorBegin = function(sensor, visitor)
        if visitor == self.lander then
            self.onPad = self.onPad + 1
        end
    end
    self.world.onSensorEnd = function(sensor, visitor)
        if visitor == self.lander then
            self.onPad = self.onPad - 1
        end
    end
    self.world.onHit = function(a, b, contact)
        if (a == self.lander or b == self.lander) and self.state == 'Flying' then
            self.touchdown = math.max(self.touchdown, contact.speed)
            if contact.speed > kCrashSpeed then
                self.state = 'Crashed'
                parts.paint(self.lander, '#FFE57373')
            end
        end
    end
end

function Thrusters:exit()
    Thrusters.super.exit(self)
    input.clearVirtual()
end

function Thrusters:update(dt)
    Thrusters.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local both = input.value('bothThrusters')
    local active = self.state ~= 'Crashed' and self.fuel > 0
    self.throttles[1] = active and math.max(input.value('leftThruster'), both) or 0
    self.throttles[2] = active and math.max(input.value('rightThruster'), both) or 0
    local velocity = self.lander.velocity
    local speed = math.sqrt(velocity.x ^ 2 + velocity.y ^ 2)
    self:status(string.format('%s, fuel %.0f%%, speed %.0f, tilt %.0f degrees, hardest touch %.0f, safe up to %d, %s the pad, step %.2f ms', self.state, self.fuel / kFuel * 100, speed, math.deg(self.lander.rotation), self.touchdown, kCrashSpeed, self.onPad > 0 and 'over' or 'off', self:stepTime()))
end

-- Each thruster pushes along the up direction of the lander at its own point, so a single one also turns it.
function Thrusters:fixedUpdate(step)
    local lander = self.lander
    local cos, sin = math.cos(lander.rotation), math.sin(lander.rotation)
    for index, point in ipairs(kThrusters) do
        local throttle = self.throttles[index]
        if throttle > 0 then
            local force = lander.mass * self.thrust * throttle
            lander:applyForce(sin * force, -cos * force, lander.x + point[1] * cos - point[2] * sin, lander.y + point[1] * sin + point[2] * cos)
            self.fuel = math.max(0, self.fuel - throttle * kBurn * step)
        end
    end
    self:simulate(self.world, step)

    local velocity = lander.velocity
    local resting = math.sqrt(velocity.x ^ 2 + velocity.y ^ 2) < kLandSpeed and math.abs(lander.rotation) < 0.25
    self.still = resting and self.onPad > 0 and self.still + step or 0
    if self.state == 'Flying' and self.still > kLandTime then
        self.state = 'Landed'
    elseif self.state == 'Landed' and self.onPad == 0 then
        self.state = 'Flying'
    end
end

function Thrusters:draw(area)
    parts.draw(self.ground)
    parts.draw(self.pad, {layer = 1})
    for _, side in ipairs({-1, 1}) do
        graphics2d.drawCircle(kPad[1] + side * (kPad[3] / 2 - 10), kPad[2] - 14, 7, self.state == 'Landed' and '#FF66BB6A' or '#FFFFD54F', {layer = 2})
    end
    graphics2d.drawRectOutline({kPad[1] - kPad[3] / 2, kPad[2] - 68, kPad[3], 60}, 2, self.onPad > 0 and '#8866BB6A' or '#44FFFFFF', {layer = 2})

    local lander = self.lander
    parts.draw(lander, {layer = 3})
    local cos, sin = math.cos(lander.rotation), math.sin(lander.rotation)
    for index, point in ipairs(kThrusters) do
        local throttle = self.throttles[index]
        if throttle > 0 then
            local length = (30 + self.random:range(0, 14)) * throttle
            local x, y = lander.x + point[1] * cos - point[2] * sin, lander.y + point[1] * sin + point[2] * cos
            graphics2d.drawPolygon({{x - cos * 10, y - sin * 10}, {x + cos * 10, y + sin * 10}, {x - sin * length, y + cos * length}}, '#FFFFB74D', {layer = 2, blend = 'additive'})
        end
    end
    graphics2d.drawRect({-780, -410, 300, 18}, '#FF2C3147', {layer = 4})
    graphics2d.drawRect({-780, -410, 300 * self.fuel / kFuel, 18}, self.fuel > 20 and '#FF6FDCA0' or '#FFFF8A84', {layer = 5})
    Thrusters.caption('Fuel', -780, -386, {layer = 5})
end

return Thrusters
