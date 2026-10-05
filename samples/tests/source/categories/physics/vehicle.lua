-- A car from `physics2d.newVehicle` on a long hilly road, driven by its wheel motors with the keyboard, a gamepad or touch pedals.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Vehicle = haylen.class('Vehicle', PhysicsTest)

Vehicle.throttle = {name = 'throttle', type = 'axis', positive = {'key:d', 'key:right', 'axis:rightTrigger+', 'axis:leftX+', 'button:dpadRight', 'virtual:gas'}, negative = {'key:a', 'key:left', 'axis:leftTrigger+', 'axis:leftX-', 'button:dpadLeft', 'virtual:reverse'}}

local kTopSpeed = 30
-- The torque of each driven wheel stays below the weight of the chassis times half the wheelbase, so full throttle never stands the car on its rear wheels.
local kTorque = 35000
local kRoadLength = 9000
local kChassis = {width = 170, height = 36}
local kWheelRadius = 30

function Vehicle:enter()
    self:frame{
        hint = 'Drive with A and D, the arrows, the left stick or the triggers, or the pedals on a touch screen. R or the X button puts the car back on its wheels.',
        controls = {
            ui.radioGroup{id = 'drive', items = {{id = 'rear', text = 'Rear wheel drive'}, {id = 'front', text = 'Front wheel drive'}, {id = 'all', text = 'All wheel drive'}}, selected = 'all', onChange = function(event)
                self.drive = event.value
                self:place(self.car.chassis.x, self.car.chassis.y - 40)
            end},
            ui.button{id = 'reset', text = 'Put it back on its wheels', onClick = function() self:recover() end},
        },
        play = true,
        pointer = false,
        actions = {Vehicle.throttle},
        overlay = {
            ui.touchButton{action = 'reverse', text = 'Back', size = 150, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}},
            ui.touchButton{action = 'gas', text = 'Gas', size = 170, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self.drive = 'all'
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 4
    self:build()
end

function Vehicle:build()
    self.world = physics2d.newWorld()
    local noise = m.noise(4)
    -- An open chain collides only from its second point to its next-to-last one, so the first and last points sit past the walls that close the road.
    local road = {{-300, -600}, {-400, -600}, {-400, 300}}
    for x = -300, kRoadLength, 60 do
        local hills = noise:fractal(x / 900, 0.5, 3) * 260 * math.min(1, math.max(0, x) / 1500)
        road[#road + 1] = {x, 300 + hills}
    end
    road[#road + 1] = {kRoadLength, -600}
    road[#road + 1] = {kRoadLength - 100, -600}
    self.ground = self.world:createBody({type = 'static'})
    parts.chain(self.ground, road, false, {friction = 0.9})

    -- The road fills down to a flat bottom, which the chain outline alone would not show.
    local fill = {}
    for index = 3, #road - 2 do
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
        x = x, y = y, chassisWidth = kChassis.width, chassisHeight = kChassis.height, wheelRadius = kWheelRadius,
        rearWheel = {-65, 34}, frontWheel = {65, 34}, wheelDensity = 1.5, drive = self.drive, maxMotorTorque = kTorque, suspensionHertz = 4, suspensionTravel = 16,
    })
    parts.outline(self.car.chassis, {{-85, 18}, {-85, -18}, {-40, -18}, {-20, -48}, {40, -48}, {62, -18}, {85, -12}, {85, 18}})
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
    self.car.motorSpeed = input.value('throttle') * kTopSpeed
    local chassis = self.car.chassis
    self.camera:follow(chassis.x + chassis.velocity.x * 0.3, chassis.y - 60, dt)
    self:status(string.format('Speed %.0f units per second, motor %.1f radians per second, drive "%s", step %.2f ms', chassis.velocity:length(), self.car.motorSpeed, self.car.drive, self:stepTime()))
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
