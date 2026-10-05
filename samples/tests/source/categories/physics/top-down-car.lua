-- A car seen from above from `physics2d.newTopDownVehicle` on a closed track, driven with the keyboard, a gamepad or touch: the tires hold the car on its line until a turn asks for more grip than they have, the handbrake lets the rear slide into a drift, and the camera follows the car. Cones and barrels slide where the car hits them.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local PhysicsTest = require('categories.physics.physics-test')

local TopDownCar = haylen.class('TopDownCar', PhysicsTest)

TopDownCar.actions = {
    {name = 'throttle', type = 'axis', positive = {'key:w', 'key:up', 'axis:rightTrigger+', 'virtual:gas'}, negative = {'key:s', 'key:down', 'axis:leftTrigger+', 'virtual:reverse'}},
    {name = 'steer', type = 'axis', positive = {'key:d', 'key:right', 'axis:leftX+', 'button:dpadRight', 'virtual:right'}, negative = {'key:a', 'key:left', 'axis:leftX-', 'button:dpadLeft', 'virtual:left'}},
    {name = 'handbrake', type = 'button', bindings = {'key:space', 'button:south', 'virtual:handbrake'}},
}

local kOuter = {x = 1500, y = 900}
local kInner = {x = 1000, y = 420}
local kLength, kWidth = 84, 42
local kMaxMarks = 600

function TopDownCar:enter()
    self:frame{
        hint = 'Accelerate with W, the up arrow or the right trigger, reverse and brake with S, the down arrow or the left trigger, steer with A and D, the arrows or the left stick, and pull the handbrake with Space or the south button. Touch screens show pedals and arrows. R or the X button puts the car back on the start line.',
        controls = {
            ui.radioGroup{id = 'drive', items = {{id = 'rear', text = 'Rear wheel drive'}, {id = 'front', text = 'Front wheel drive'}, {id = 'all', text = 'All wheel drive'}}, selected = 'rear', onChange = function(event)
                self.drive = event.value
                self:place()
            end},
            ui.label{text = 'Grip of the tires', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'grip', value = 1400, min = 400, max = 3000, step = 100, showValue = true, decimals = 0, onChange = function(event)
                self.grip = event.value
                self:place()
            end},
            ui.button{id = 'reset', text = 'Back to the start line', onClick = function() self:place(true) end},
        },
        play = true,
        pointer = false,
        actions = TopDownCar.actions,
        overlay = {
            ui.touchButton{action = 'left', text = 'Left', size = 130, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}},
            ui.touchButton{action = 'right', text = 'Right', size = 130, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 190}},
            ui.touchButton{action = 'handbrake', text = 'Drift', size = 120, touchOnly = true, anchor = 'bottomRight', margin = {0, 710, 110, 0}},
            ui.touchButton{action = 'reverse', text = 'Back', size = 120, touchOnly = true, anchor = 'bottomRight', margin = {0, 860, 110, 0}},
            ui.touchButton{action = 'gas', text = 'Gas', size = 160, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self.drive = 'rear'
    self.grip = 1400
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 5
    self:build()
end

function TopDownCar:ellipse(radius, count, clockwise)
    local points = {}
    for index = 0, count - 1 do
        local angle = (clockwise and 1 or -1) * index / count * math.pi * 2
        points[#points + 1] = {math.cos(angle) * radius.x, math.sin(angle) * radius.y}
    end
    return points
end

-- The outer wall is a loop listed counterclockwise on screen, which holds the car inside it, and the inner wall a loop listed clockwise, which is solid from the outside.
function TopDownCar:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.outer = self:ellipse(kOuter, 64, false)
    self.inner = self:ellipse(kInner, 48, true)
    local walls = self.world:createBody({type = 'static'})
    walls:addChain(self.outer, true, {restitution = 0.3})
    walls:addChain(self.inner, true, {restitution = 0.3})

    self.props = {}
    for index = 0, 11 do
        local angle = index / 12 * math.pi * 2 + 0.3
        local radius = {x = (kOuter.x + kInner.x) / 2, y = (kOuter.y + kInner.y) / 2}
        local cone = index % 3 == 0
        local prop = self.world:createBody({x = math.cos(angle) * radius.x, y = math.sin(angle) * radius.y, linearDamping = cone and 3 or 1.5, angularDamping = 3})
        if cone then
            prop:addCircle(14, {density = 0.5})
        else
            prop:addBox(40, 40, {density = 1})
        end
        self.props[#self.props + 1] = {body = prop, cone = cone}
    end
    self.marks = {}
    self:place(true)
end

function TopDownCar:place(start)
    local x, y, rotation = 0, (kOuter.y + kInner.y) / 2, math.pi
    if self.car then
        local body = self.car.body
        if not start then
            x, y, rotation = body.x, body.y, body.rotation
        end
        self.car:destroy()
    end
    self.car = physics2d.newTopDownVehicle(self.world, {x = x, y = y, rotation = rotation, length = kLength, width = kWidth, frontAxle = 26, rearAxle = -26, drive = self.drive, grip = self.grip, topSpeed = 1100, acceleration = 800})
    if start then
        self.camera:snapTo(x, y)
    end
end

function TopDownCar:exit()
    TopDownCar.super.exit(self)
    input.clearVirtual()
end

function TopDownCar:update(dt)
    TopDownCar.super.update(self, dt)
    if input.pressed('reset') then
        self:place(true)
    end
    local car = self.car
    local throttle = input.value('throttle')
    -- Pressing against the way the car rolls brakes it before it drives the other way.
    if throttle < 0 and car.speed > 30 then
        car.throttle, car.brake = 0, -throttle
    else
        car.throttle, car.brake = throttle, 0
    end
    car.steering = input.value('steer')
    car.handbrake = input.down('handbrake')

    local body = car.body
    self.camera:follow(body.x + body.velocity.x * 0.25, body.y + body.velocity.y * 0.25, dt)
    self:status(string.format('Speed %d, slip %d, steering %.2f radians, %s, drive "%s", step %.2f ms', math.floor(car.speed + 0.5), math.floor(math.abs(car.slip) + 0.5), car.steeringAngle, car.drifting and 'drifting' or 'gripping', self.drive, self:stepTime()))
end

function TopDownCar:fixedUpdate(step)
    self:simulate(self.world, step)
    local car = self.car
    if car.drifting or car.handbrake and math.abs(car.speed) > 100 then
        local body = car.body
        local cos, sin = math.cos(body.rotation), math.sin(body.rotation)
        for _, side in ipairs({-1, 1}) do
            self.marks[#self.marks + 1] = {body.x - cos * 26 - sin * side * 16, body.y - sin * 26 + cos * side * 16}
        end
        while #self.marks > kMaxMarks do
            table.remove(self.marks, 1)
        end
    end
end

function TopDownCar:drawCar()
    local body, car = self.car.body, self.car
    local x, y, rotation = body.x, body.y, body.rotation
    local cos, sin = math.cos(rotation), math.sin(rotation)
    local function corner(dx, dy)
        return {x + cos * dx - sin * dy, y + sin * dx + cos * dy}
    end
    local order = {layer = 3}
    for _, axle in ipairs({{26, car.steeringAngle}, {-26, 0}}) do
        for _, side in ipairs({-1, 1}) do
            local cx, cy = table.unpack(corner(axle[1], side * kWidth / 2))
            local angle = rotation + axle[2]
            local tx, ty = math.cos(angle) * 11, math.sin(angle) * 11
            graphics2d.drawLine(cx - tx, cy - ty, cx + tx, cy + ty, 10, '#FF263238', {layer = 2})
        end
    end
    local hl, hw = kLength / 2, kWidth / 2
    graphics2d.drawPolygon({corner(-hl, -hw), corner(hl - 10, -hw), corner(hl, -hw + 8), corner(hl, hw - 8), corner(hl - 10, hw), corner(-hl, hw)}, '#FFE57373', order)
    graphics2d.drawPolygon({corner(4, -hw + 6), corner(20, -hw + 8), corner(20, hw - 8), corner(4, hw - 6)}, '#FF37474F', order)
end

function TopDownCar:draw(area)
    graphics2d.drawPolygon(self.outer, '#FF3A3F4B')
    graphics2d.drawPolyline(self.outer, 8, '#FFECEFF1', true)
    graphics2d.drawPolygon(self.inner, '#FF4E6E45', {layer = 1})
    graphics2d.drawPolyline(self.inner, 8, '#FFECEFF1', true, {layer = 1})
    for _, mark in ipairs(self.marks) do
        graphics2d.drawCircle(mark[1], mark[2], 4, '#55101010', {layer = 1})
    end
    for _, prop in ipairs(self.props) do
        local body = prop.body
        if prop.cone then
            graphics2d.drawCircle(body.x, body.y, 14, '#FFFF8A65', {layer = 2})
            graphics2d.drawCircle(body.x, body.y, 6, '#FFFFFFFF', {layer = 2})
        else
            local cos, sin = math.cos(body.rotation) * 20, math.sin(body.rotation) * 20
            graphics2d.drawPolygon({{body.x - cos + sin, body.y - sin - cos}, {body.x + cos + sin, body.y + sin - cos}, {body.x + cos - sin, body.y + sin + cos}, {body.x - cos - sin, body.y - sin + cos}}, '#FFFFD54F', {layer = 2})
        end
    end
    self:drawCar()
end

return TopDownCar
