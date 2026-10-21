-- Water from `physics2d.newFluid` held in a stone reservoir behind a sluice gate, pouring down a wooden chute onto a paddle wheel on a revolute joint that it turns, and pumped from the basin up a pipe back into the reservoir. The gate opens by itself when the test opens.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local WaterWheel = haylen.class('WaterWheel', PhysicsTest)

WaterWheel.code = [[
local water = physics2d.newFluid(world, {radius = 4, smoothingRadius = 16, maxParticles = 2600, viscosity = 0.2})
water:fill({-705, -250, 370, 125})  -- The reservoir behind the dam.
world:createJoint('revolute', axle, wheel, {ax = 110, ay = 120, enableMotor = true, motorSpeed = 0, maxMotorTorque = 3000})  -- Friction in the bearing.
gate:moveTo(-320, gate.y - 400 * step, 0)  -- Slides the dam open.
water:positions(buffer)  water:remove(index)  water:spawn(x, y)  -- The pump.]]

WaterWheel.particleRadius = 4
WaterWheel.maxParticles = 2600
WaterWheel.ground = 390
WaterWheel.wheelSpot = {110, 120}
WaterWheel.gatePath = {x = -320, closed = -200, open = -310, speed = 400}
WaterWheel.pumpLine = 300
WaterWheel.pumpRate = 5
WaterWheel.pumpEvery = 2
WaterWheel.firstOpening = 1
-- The pipe of the pump climbs outside the reservoir, crosses over its wall and pours from a spout over the water.
WaterWheel.pipe = {-772, 340, -772, -374, -690, -374, -690, -350}
WaterWheel.spout = {-690, -344}
WaterWheel.chute = {x = -140, y = -90, width = 372, rotation = 0.11}

function WaterWheel:enter()
    self:frame{
        code = WaterWheel.code,
        hint = 'Open the dam to let the water pour down the chute onto the wheel, whose bearing has some friction. The pump carries the water from the basin back into the reservoir while the dam is open. Drag the wheel to turn it by hand. R or the X button refills.',
        controls = {
            ui.button{id = 'dam', text = 'Open the dam', onClick = function() self:setDam(not self.damOpen) end},
            ui.checkbox{id = 'pump', text = 'Pump the water back', checked = true, onChange = function(event) self.pumping = event.checked end},
            ui.button{id = 'reset', text = 'Refill', onClick = function() self:build() end},
        },
        focus = 'dam',
        view = {1600, 800},
    }
    self.random = m.random(59)
    self.pumping = true
    self.positions = collections.newFloatBuffer(WaterWheel.maxParticles * 2)
    self.pipeStroke = {width = 1, color = Look.ink, join = 'round', cap = 'butt', layer = 0}
    self:build()
    timer.after(WaterWheel.firstOpening, function() self:setDam(true) end, {owner = self})
end

function WaterWheel:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.steps, self.pumped, self.pumpRate, self.pumpTime = 0, 0, 0, 0

    -- Thick walls push back the particles that the pressure of the water squeezes into them.
    local walls = self.world:createBody({type = 'static'})
    walls:addBox(4000, 40, {offsetY = WaterWheel.ground + 20})
    walls:addBox(40, 280, {offsetX = -780, offsetY = 270})
    walls:addBox(40, 280, {offsetX = 780, offsetY = 270})
    walls:addBox(420, 30, {offsetX = -515, offsetY = -105})
    walls:addBox(30, 245, {offsetX = -740, offsetY = -227})
    local chute = WaterWheel.chute
    local trough = self.world:createBody({type = 'static', x = chute.x, y = chute.y, rotation = chute.rotation})
    trough:addBox(chute.width, 20)

    local path = WaterWheel.gatePath
    self.gate = self.world:createBody({type = 'kinematic', x = path.x, y = path.closed})
    self.gate:addBox(24, 160)
    Look.dress(self.gate, 'darkWood')
    self:setDam(false)

    local spot = WaterWheel.wheelSpot
    local axle = self.world:createBody({type = 'static', x = spot[1], y = spot[2]})
    self.wheel = self.world:createBody({x = spot[1], y = spot[2]})
    self.wheel:addCircle(30, {density = 0.5})
    for paddle = 0, 7 do
        local angle = paddle * math.pi / 4
        self.wheel:addBox(100, 12, {offsetX = math.cos(angle) * 80, offsetY = math.sin(angle) * 80, rotation = angle, density = 0.5})
    end
    self.world:createJoint('revolute', axle, self.wheel, {ax = spot[1], ay = spot[2], enableMotor = true, motorSpeed = 0, maxMotorTorque = 3000})

    self.water = physics2d.newFluid(self.world, {radius = WaterWheel.particleRadius, smoothingRadius = 16, maxParticles = WaterWheel.maxParticles, viscosity = 0.2})
    self.water:fill({-705, -250, 370, 125})
    self.water:fill({-740, 330, 1480, 55})
end

function WaterWheel:setDam(open)
    self.damOpen = open
    self:set('dam', {text = open and 'Close the dam' or 'Open the dam'})
end

-- The pump takes particles from under its line in the basin and spawns them again under the spout, a few each time, walking the particles from the end because a removal moves the last one into the gap.
function WaterWheel:pump()
    self.water:positions(self.positions)
    local spout = WaterWheel.spout
    local moved = 0
    for index = self.water.size, 1, -1 do
        if moved == WaterWheel.pumpRate then
            break
        end
        if self.positions[index * 2] > WaterWheel.pumpLine then
            self.water:remove(index)
            self.water:spawn(spout[1] + self.random:range(-14, 14), spout[2] + self.random:range(0, 30), 0, 120)
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
    local turns = math.abs(self.wheel.angularVelocity) * 60 / (math.pi * 2)
    local dam = self.damOpen and 'open' or 'closed'
    self:status(string.format('The wheel turns %.1f times a minute with the dam %s, and the pump lifts %d drops a second of the %d in play.', turns, dam, math.floor(self.pumpRate + 0.5), self.water.size))
end

function WaterWheel:fixedUpdate(step)
    local path = WaterWheel.gatePath
    local target = self.damOpen and path.open or path.closed
    self.gate:moveTo(path.x, self.gate.y + m.clamp(target - self.gate.y, -path.speed * step, path.speed * step), 0)
    self.steps = self.steps + 1
    if self.damOpen and self.pumping and self.steps % WaterWheel.pumpEvery == 0 then
        self:pump()
    end
    self:simulate(self.world, step)
end

-- The reservoir and the basin are stone, the chute a wooden trough, the gate slides in two steel guides, and the pump is an iron box with a steel pipe.
function WaterWheel:drawWorks()
    local unit = Look.pixel
    Look.block(-515, -105, 420, 30, 'stone', 4)
    Look.block(-740, -227, 30, 245, 'stone', 4)
    for _, x in ipairs({-780, 780}) do
        Look.block(x, 270, 40, 280, 'stone', 4)
    end
    local chute = WaterWheel.chute
    Look.block(chute.x, chute.y, chute.width, 20, 'wood', 0, chute.rotation)
    local path = WaterWheel.gatePath
    for _, side in ipairs({-1, 1}) do
        Look.line(path.x + side * 18, path.open - 80, path.x + side * 18, -126, 5, '#FFA9BCD3', 0)
    end
    Look.draw(self.gate, {layer = 1})
    local stroke = self.pipeStroke
    stroke.width, stroke.color, stroke.layer = 16 + Look.outlinePixels * unit * 2, Look.ink, 4
    graphics2d.drawStroke(WaterWheel.pipe, stroke)
    stroke.width, stroke.color = 16, '#FF9AA5BE'
    graphics2d.drawStroke(WaterWheel.pipe, stroke)
    Look.block(-735, 340, 70, 46, 'iron', 5)
    Look.disc(-735, 340, 12, 'steel', 5, haylen.elapsed() * 6)
end

-- The water wheel is two wooden rims joined by spokes behind eight paddles on a steel hub.
function WaterWheel:drawWheel()
    local wheel = self.wheel
    local x, y, rotation = wheel.x, wheel.y, wheel.rotation
    graphics2d.drawRing(x, y, 116, 14 + Look.outlinePixels * Look.pixel * 2, Look.ink, {layer = 0})
    graphics2d.drawRing(x, y, 116, 14, '#FFA86B3C', {layer = 0})
    for spoke = 0, 7 do
        local angle = rotation + (spoke + 0.5) * math.pi / 4
        Look.line(x, y, x + math.cos(angle) * 110, y + math.sin(angle) * 110, 6, '#FF7A4724', 0)
    end
    for paddle = 0, 7 do
        local angle = rotation + paddle * math.pi / 4
        Look.block(x + math.cos(angle) * 80, y + math.sin(angle) * 80, 100, 14, 'wood', 1, angle)
    end
    Look.disc(x, y, 30, 'steel', 1, rotation)
    Look.pin(x, y, 9, 1)
end

function WaterWheel:draw(area)
    Scenery.sky(self.camera, WaterWheel.ground)
    Scenery.ground(WaterWheel.ground)
    self:drawWorks()
    self:drawWheel()
    self.water:draw({radius = WaterWheel.particleRadius * 2.6, color = '#E038C2D2', outlineColor = '#FFE9FBFF', outlineWidth = 0.12, layer = 3})
    self.grab:draw()
end

return WaterWheel
