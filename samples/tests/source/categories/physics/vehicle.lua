-- A car from `physics2d.newVehicle` on a long grassy road over the hills, driven with the keyboard, a gamepad or touch pedals: the motors size their torque from the acceleration it asks for, so it climbs, brakes and coasts without lifting its nose, and the camera follows it inside the limits of the road.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Vehicle = haylen.class('Vehicle', PhysicsTest)

Vehicle.code = [[
local car = physics2d.newVehicle(world, {x = 0, y = 200, chassisWidth = 240, chassisHeight = 50, wheelRadius = 30, rearWheel = {-70, 34}, frontWheel = {70, 34}, drive = 'all'})
car.throttle = input.value('throttle')  -- From -1 to 1.
car.brake = input.down('brake') and 1 or 0
road:addChain(points, false, {friction = 0.9})
camera.limits = {-400, -1600, 9400, 2500}  -- The view never leaves the road.
camera:follow(car.chassis.x, car.chassis.y - 150, dt)]]

Vehicle.actions = {
    {name = 'throttle', type = 'axis', positive = {'key:d', 'key:right', 'axis:rightTrigger+', 'axis:leftX+', 'button:dpadRight', 'virtual:gas'}, negative = {'key:a', 'key:left', 'axis:leftTrigger+', 'axis:leftX-', 'button:dpadLeft', 'virtual:reverse'}},
    {name = 'brake', type = 'button', bindings = {'key:space', 'key:s', 'key:down', 'button:south', 'virtual:brake'}},
}

Vehicle.roadLength = 9000
Vehicle.start = -400
Vehicle.wheelRadius = 30
-- The car picture is drawn at this many world units per pixel, which puts its wheel arches over the wheels, and this far above and beside the middle of the chassis.
Vehicle.carScale = 0.7
Vehicle.carOffset = {2, -24}
Vehicle.metersPerUnit = 1 / 64

function Vehicle:enter()
    self:frame{
        code = Vehicle.code,
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
            ui.touchButton{action = 'reverse', text = 'Back', size = 140, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}},
            ui.touchButton{action = 'brake', text = 'Brake', size = 140, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 184}},
            ui.touchButton{action = 'gas', text = 'Gas', size = 170, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.drive = 'all'
    self.antiRoll = 0.5
    self.carTexture = assets.texture('physics/car.png', {filter = 'linear'})
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 4
    self.camera.limits = {Vehicle.start, -1600, Vehicle.roadLength - Vehicle.start, 2500}
    self:build()
end

function Vehicle:build()
    self.world = physics2d.newWorld()
    local noise = m.noise(4)
    -- Every segment of an open chain collides, so the walls at both ends close the road.
    local road = {{Vehicle.start, -600}, {Vehicle.start, 300}}
    self.surface = {}
    for x = Vehicle.start + 60, Vehicle.roadLength, 60 do
        local hills = noise:fractal(x / 900, 0.5, 3) * 260 * math.min(1, math.max(0, x) / 1500)
        road[#road + 1] = {x, 300 + hills}
    end
    road[#road + 1] = {Vehicle.roadLength, -600}
    for index = 2, #road - 1 do
        self.surface[#self.surface + 1] = road[index]
    end
    self.ground = self.world:createBody({type = 'static'})
    self.ground:addChain(road, false, {friction = 0.9})
    self.terrain = Scenery.newTerrain(self.surface, 2000)
    self.signs = {}
    for x = 1000, Vehicle.roadLength - 500, 1000 do
        self.signs[#self.signs + 1] = {x = x, y = self:surfaceAt(x), text = string.format('%d m', math.floor(x * Vehicle.metersPerUnit + 0.5))}
    end

    self:place(0, 200)
    self.camera:snapTo(0, 50)
end

function Vehicle:surfaceAt(x)
    local surface = self.surface
    for index = 2, #surface do
        local a, b = surface[index - 1], surface[index]
        if x <= b[1] then
            return a[2] + (b[2] - a[2]) * (x - a[1]) / (b[1] - a[1])
        end
    end
    return surface[#surface][2]
end

function Vehicle:place(x, y)
    if self.car then
        self.car:destroy()
    end
    self.car = physics2d.newVehicle(self.world, {
        x = x, y = y, chassisWidth = 240, chassisHeight = 50, wheelRadius = Vehicle.wheelRadius, rearWheel = {-70, 34}, frontWheel = {70, 34},
        wheelDensity = 1.5, drive = self.drive, acceleration = 700, topSpeed = 1400, suspensionHertz = 4, suspensionTravel = 16, antiRoll = self.antiRoll,
    })
    for _, wheel in ipairs({self.car.rearWheel, self.car.frontWheel}) do
        Look.dress(wheel, 'wheel')
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
    self.camera:follow(chassis.x + chassis.velocity.x * 0.3, chassis.y - 150, dt)
    local speed = math.floor(math.abs(self.car.speed) * Vehicle.metersPerUnit * 3.6 + 0.5)
    local where = self.car.grounded and 'on the road' or 'in the air'
    local driving = self.car.brake > 0 and 'braking' or self.car.throttle > 0.05 and 'speeding up' or self.car.throttle < -0.05 and 'reversing' or 'coasting'
    self:status(string.format('The car is %s at %d km/h %s, %d m down the road.', driving, speed, where, math.max(0, math.floor(chassis.x * Vehicle.metersPerUnit + 0.5))))
end

function Vehicle:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Vehicle:drawCar()
    local chassis = self.car.chassis
    local cos, sin = math.cos(chassis.rotation), math.sin(chassis.rotation)
    local dx, dy = Vehicle.carOffset[1], Vehicle.carOffset[2]
    Look.draw(self.car.rearWheel, {layer = 1})
    Look.draw(self.car.frontWheel, {layer = 1})
    graphics2d.draw(self.carTexture, chassis.x + dx * cos - dy * sin, chassis.y + dx * sin + dy * cos, {width = 384 * Vehicle.carScale, height = 201 * Vehicle.carScale, rotation = chassis.rotation, layer = 2})
end

function Vehicle:draw(area)
    Scenery.sky(self.camera, 300, {parallax = 1})
    Scenery.drawTerrain(self.terrain)
    Look.block(Vehicle.start - 60, -150, 120, 1100, 'stone', -8)
    for _, sign in ipairs(self.signs) do
        Look.block(sign.x, sign.y - 50, 14, 100, 'darkWood', -9)
        self:label(sign.text, sign.x, sign.y - 100, {size = 20, layer = -8})
    end
    self:drawCar()
end

return Vehicle
