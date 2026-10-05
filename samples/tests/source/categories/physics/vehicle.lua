-- A car from `physics2d.newVehicle` on a long hilly road, driven with the keyboard, a gamepad or touch pedals: the motors size their torque from the acceleration it asks for, so it climbs, brakes and coasts without lifting its nose, and the camera follows it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Vehicle = haylen.class('Vehicle', PhysicsTest)

Vehicle.actions = {
    {name = 'throttle', type = 'axis', positive = {'key:d', 'key:right', 'axis:rightTrigger+', 'axis:leftX+', 'button:dpadRight', 'virtual:gas'}, negative = {'key:a', 'key:left', 'axis:leftTrigger+', 'axis:leftX-', 'button:dpadLeft', 'virtual:reverse'}},
    {name = 'brake', type = 'button', bindings = {'key:space', 'key:s', 'key:down', 'button:south', 'virtual:brake'}},
}

local kRoadLength = 9000
local kWheelRadius = 30
local kOutline = {{-85, 18}, {-85, -18}, {-40, -18}, {-20, -48}, {40, -48}, {62, -18}, {85, -12}, {85, 18}}

function Vehicle:enter()
    self:frame{
        hint = 'Drive with D and A, the arrows, the left stick or the triggers, brake with Space, S or the south button, or use the pedals on a touch screen. R or the X button puts the car back on its wheels.',
        controls = {
            ui.radioGroup{id = 'drive', items = {{id = 'rear', text = 'Rear wheel drive'}, {id = 'front', text = 'Front wheel drive'}, {id = 'all', text = 'All wheel drive'}}, selected = 'all', onChange = function(event)
                self.drive = event.value
                self:place(self.car.chassis.x, self.car.chassis.y - 40)
            end},
            ui.label{text = 'Anti-roll between the axles', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'antiRoll', value = 0.5, min = 0, max = 1, showValue = true, decimals = 2, onChange = function(event) self.antiRoll = event.value end},
            ui.button{id = 'reset', text = 'Put it back on its wheels', onClick = function() self:recover() end},
        },
        play = true,
        pointer = false,
        actions = Vehicle.actions,
        overlay = {
            ui.touchButton{action = 'reverse', text = 'Back', size = 140, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}},
            ui.touchButton{action = 'brake', text = 'Brake', size = 140, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 200}},
            ui.touchButton{action = 'gas', text = 'Gas', size = 170, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self.drive = 'all'
    self.antiRoll = 0.5
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 4
    self:build()
end

function Vehicle:build()
    self.world = physics2d.newWorld()
    local noise = m.noise(4)
    -- Every segment of an open chain collides, so the walls at both ends close the road.
    local road = {{-400, -600}, {-400, 300}}
    for x = -300, kRoadLength, 60 do
        local hills = noise:fractal(x / 900, 0.5, 3) * 260 * math.min(1, math.max(0, x) / 1500)
        road[#road + 1] = {x, 300 + hills}
    end
    road[#road + 1] = {kRoadLength, -600}
    self.ground = self.world:createBody({type = 'static'})
    parts.chain(self.ground, road, false, {friction = 0.9})

    -- The road fills down to a flat bottom, which the chain outline alone would not show.
    local fill = {}
    for index = 2, #road - 1 do
        fill[#fill + 1] = road[index]
    end
    fill[#fill + 1] = {kRoadLength, 900}
    fill[#fill + 1] = {-400, 900}
    parts.outline(self.ground, fill)
    parts.paint(self.ground, '#FF4E5D45')

    self:place(0, 200)
    self.camera:snapTo(0, 100)
end

function Vehicle:place(x, y)
    if self.car then
        self.car:destroy()
    end
    self.car = physics2d.newVehicle(self.world, {
        x = x, y = y, chassisWidth = 170, chassisHeight = 36, wheelRadius = kWheelRadius, rearWheel = {-65, 34}, frontWheel = {65, 34},
        wheelDensity = 1.5, drive = self.drive, acceleration = 700, topSpeed = 1400, suspensionHertz = 4, suspensionTravel = 16, antiRoll = self.antiRoll,
    })
    parts.outline(self.car.chassis, kOutline)
    parts.paint(self.car.chassis, '#FFE57373')
    for _, wheel in ipairs({self.car.rearWheel, self.car.frontWheel}) do
        parts.round(wheel, kWheelRadius)
        parts.round(wheel, kWheelRadius * 0.45)
        parts.paint(wheel, '#FF37474F')
    end
end

function Vehicle:recover()
    local chassis = self.car.chassis
    self:place(chassis.x, chassis.y - 80)
end

function Vehicle:exit()
    Vehicle.super.exit(self)
    input.clearVirtual()
end

function Vehicle:update(dt)
    Vehicle.super.update(self, dt)
    if input.pressed('reset') then
        self:recover()
    end
    self.car.throttle = input.value('throttle')
    self.car.brake = input.down('brake') and 1 or 0
    local chassis = self.car.chassis
    self.camera:follow(chassis.x + chassis.velocity.x * 0.3, chassis.y - 60, dt)
    self:status(string.format('Speed %d units per second, throttle %.2f, brake %.0f, %s, drive "%s", step %.2f ms', math.floor(self.car.speed + 0.5), self.car.throttle, self.car.brake, self.car.grounded and 'on the road' or 'in the air', self.car.drive, self:stepTime()))
end

function Vehicle:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Vehicle:draw(area)
    parts.draw(self.ground)
    parts.draw(self.car.chassis, {layer = 1})
    parts.draw(self.car.rearWheel, {layer = 2})
    parts.draw(self.car.frontWheel, {layer = 2})
    for x = 0, kRoadLength, 1000 do
        graphics2d.drawText(nil, string.format('%d meters', x // 64), x, -300, {size = 28, color = '#66FFFFFF', anchor = {0.5, 0.5}})
    end
end

return Vehicle
