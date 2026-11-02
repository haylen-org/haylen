-- A race car seen from above from `physics2d.newTopDownVehicle` on a closed track, driven with the keyboard, a gamepad or touch: the tires hold the car on its line until a turn asks for more grip than they have, the handbrake lets the rear slide into a drift that leaves marks on the asphalt, and the camera follows the car. Cones and barrels slide where the car hits them.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')

local TopDownCar = haylen.class('TopDownCar', PhysicsTest)

TopDownCar.code = [[
local world = physics2d.newWorld({gravity = {0, 0}})
local car = physics2d.newTopDownVehicle(world, {length = 96, width = 48, grip = 1400})
car.throttle = input.value('throttle')
car.steering = input.value('steer')
car.handbrake = input.down('handbrake')
walls:addChain(outer, true, {restitution = 0.3})  -- Counterclockwise, it holds the car in.]]

TopDownCar.actions = {
    {name = 'throttle', type = 'axis', positive = {'key:w', 'key:up', 'axis:rightTrigger+', 'virtual:gas'}, negative = {'key:s', 'key:down', 'axis:leftTrigger+', 'virtual:reverse'}},
    {name = 'steer', type = 'axis', positive = {'key:d', 'key:right', 'axis:leftX+', 'button:dpadRight', 'virtual:right'}, negative = {'key:a', 'key:left', 'axis:leftX-', 'button:dpadLeft', 'virtual:left'}},
    {name = 'handbrake', type = 'button', bindings = {'key:space', 'button:south', 'virtual:handbrake'}},
}

TopDownCar.outerRadius = {x = 1500, y = 900}
TopDownCar.innerRadius = {x = 1060, y = 480}
TopDownCar.length, TopDownCar.width = 96, 48
TopDownCar.axle = 30
TopDownCar.maxMarks = 600
-- The race car picture covers this many world units, a little more than the body so the tires show.
TopDownCar.picture = {128, 82}
-- The asphalt and the lawn repeat every this many world units, and the track and the infield are meshes of this many slices around the ellipse.
TopDownCar.tile = 384
TopDownCar.slices = 96
TopDownCar.texture = 512
TopDownCar.coneRadius, TopDownCar.barrelRadius = 20, 26
TopDownCar.shadow = '#40101018'

function TopDownCar:enter()
    self:frame{
        code = TopDownCar.code,
        hint = 'Drive with W and S or the triggers, steer with A and D or the left stick, and drift with Space or the south button. Touch screens show pedals. R or the X button puts the car back on the start line.',
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
        view = {980, 470},
        overlay = {
            ui.touchButton{action = 'left', text = 'Left', size = 130, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}},
            ui.touchButton{action = 'right', text = 'Right', size = 130, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 174}},
            ui.touchButton{action = 'handbrake', text = 'Drift', size = 120, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 194, 24, 0}},
            ui.touchButton{action = 'reverse', text = 'Back', size = 120, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 344, 24, 0}},
            ui.touchButton{action = 'gas', text = 'Gas', size = 160, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.drive = 'rear'
    self.grip = 1400
    self.racer = assets.texture('physics/race-car.png', {filter = 'linear'})
    self.asphalt = assets.texture('physics/asphalt.png', {filter = 'linear', wrap = 'repeat'})
    self.lawn = assets.texture('physics/lawn.png', {filter = 'linear', wrap = 'repeat'})
    self.lawnSource = {0, 0, 0, 0}
    self.lawnOptions = {width = 0, height = 0, pivotX = 0, pivotY = 0, source = self.lawnSource, layer = -20}
    self:buildGround()
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 5
    self:build()
end

function TopDownCar.ellipse(radius, count, clockwise)
    local points = {}
    for index = 0, count - 1 do
        local angle = (clockwise and 1 or -1) * index / count * math.pi * 2
        points[#points + 1] = {math.cos(angle) * radius.x, math.sin(angle) * radius.y}
    end
    return points
end

function TopDownCar.flatten(points)
    local flat = {}
    for _, point in ipairs(points) do
        flat[#flat + 1] = point[1]
        flat[#flat + 1] = point[2]
    end
    return flat
end

-- The asphalt is a ring of slices between the edges of the track, and the infield is a fan inside it, both with their texture laid out in world units so it repeats along them.
function TopDownCar:buildGround()
    local outer, inner = TopDownCar.outerRadius, TopDownCar.innerRadius
    local tile, count = TopDownCar.tile, TopDownCar.slices
    local ring, field, ringIndices, fieldIndices = {}, {{x = 0, y = 0, u = 0, v = 0}}, {}, {}
    for index = 0, count - 1 do
        local angle = index / count * math.pi * 2
        local cos, sin = math.cos(angle), math.sin(angle)
        local ox, oy, ix, iy = cos * outer.x, sin * outer.y, cos * inner.x, sin * inner.y
        ring[#ring + 1] = {x = ox, y = oy, u = ox / tile, v = oy / tile}
        ring[#ring + 1] = {x = ix, y = iy, u = ix / tile, v = iy / tile}
        field[#field + 1] = {x = ix, y = iy, u = ix / tile, v = iy / tile}
        local following = (index + 1) % count
        for _, corner in ipairs({index * 2 + 1, index * 2 + 2, following * 2 + 1, index * 2 + 2, following * 2 + 2, following * 2 + 1}) do
            ringIndices[#ringIndices + 1] = corner
        end
        for _, corner in ipairs({1, index + 2, following + 2}) do
            fieldIndices[#fieldIndices + 1] = corner
        end
    end
    self.track = graphics2d.newMesh(self.asphalt, ring, ringIndices)
    self.infield = graphics2d.newMesh(self.lawn, field, fieldIndices)
end

-- The outer wall is a loop listed counterclockwise on screen, which holds the car inside it, and the inner wall a loop listed clockwise, which is solid from the outside.
function TopDownCar:build()
    local outer, inner = TopDownCar.outerRadius, TopDownCar.innerRadius
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.outer = TopDownCar.ellipse(outer, 64, false)
    self.inner = TopDownCar.ellipse(inner, 48, true)
    self.outerEdge = TopDownCar.flatten(self.outer)
    self.innerEdge = TopDownCar.flatten(self.inner)
    self.middle = TopDownCar.flatten(TopDownCar.ellipse({x = (outer.x + inner.x) / 2, y = (outer.y + inner.y) / 2}, 64, false))
    local walls = self.world:createBody({type = 'static'})
    walls:addChain(self.outer, true, {restitution = 0.3})
    walls:addChain(self.inner, true, {restitution = 0.3})

    self.props = {}
    for index = 0, 11 do
        local angle = index / 12 * math.pi * 2 + 0.3
        local radius = {x = (outer.x + inner.x) / 2, y = (outer.y + inner.y) / 2}
        local cone = index % 3 == 0
        local prop = self.world:createBody({x = math.cos(angle) * radius.x, y = math.sin(angle) * radius.y, linearDamping = cone and 3 or 1.5, angularDamping = 3})
        if cone then
            prop:addCircle(TopDownCar.coneRadius, {density = 0.5})
        else
            prop:addCircle(TopDownCar.barrelRadius, {density = 1})
        end
        self.props[#self.props + 1] = {body = prop, cone = cone}
    end
    self.marks = {}
    self:place(true)
end

function TopDownCar:place(start)
    local x, y, rotation = 0, (TopDownCar.outerRadius.y + TopDownCar.innerRadius.y) / 2, math.pi
    if self.car then
        local body = self.car.body
        if not start then
            x, y, rotation = body.x, body.y, body.rotation
        end
        self.car:destroy()
    end
    self.car = physics2d.newTopDownVehicle(self.world, {x = x, y = y, rotation = rotation, length = TopDownCar.length, width = TopDownCar.width, frontAxle = TopDownCar.axle, rearAxle = -TopDownCar.axle, drive = self.drive, grip = self.grip, topSpeed = 1100, acceleration = 800})
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
    local speed = math.floor(math.abs(car.speed) / 64 * 3.6 + 0.5)
    self:status(string.format('The car %s at %d km/h with the wheels turned %d degrees.', car.drifting and 'drifts' or 'grips the road', speed, math.abs(PhysicsTest.degrees(car.steeringAngle))))
end

function TopDownCar:fixedUpdate(step)
    self:simulate(self.world, step)
    local car = self.car
    if car.drifting or car.handbrake and math.abs(car.speed) > 100 then
        local body = car.body
        local cos, sin = math.cos(body.rotation), math.sin(body.rotation)
        for _, side in ipairs({-1, 1}) do
            self.marks[#self.marks + 1] = {body.x - cos * TopDownCar.axle - sin * side * 20, body.y - sin * TopDownCar.axle + cos * side * 20}
        end
        while #self.marks > TopDownCar.maxMarks do
            table.remove(self.marks, 1)
        end
    end
end

-- The track is asphalt between kerbs of red and white on a lawn, with a dashed middle line and a chequered start line across the bottom straight.
function TopDownCar:drawTrack()
    local unit = Look.pixel
    local bounds = graphics2d.canvasBounds()
    local scale = TopDownCar.texture / TopDownCar.tile
    local source, lawn = self.lawnSource, self.lawnOptions
    source[1], source[2] = bounds.x * scale % TopDownCar.texture, bounds.y * scale % TopDownCar.texture
    source[3], source[4] = bounds.width * scale, bounds.height * scale
    lawn.width, lawn.height = bounds.width, bounds.height
    graphics2d.draw(self.lawn, bounds.x, bounds.y, lawn)
    self.track:draw(0, 0, {layer = -19})
    self.infield:draw(0, 0, {layer = -18})
    for _, edge in ipairs({self.outerEdge, self.innerEdge}) do
        graphics2d.drawStroke(edge, {width = 30, color = '#FFF2F3FF', closed = true, join = 'round', layer = -17})
        graphics2d.drawStroke(edge, {width = 30, color = '#FFF0473E', closed = true, dashes = {44, 44}, layer = -17})
        graphics2d.drawStroke(edge, {width = 4 * unit, color = Look.ink, closed = true, join = 'round', layer = -17})
    end
    graphics2d.drawStroke(self.middle, {width = 8, color = '#B3F2F3FF', closed = true, dashes = {60, 50}, layer = -16})
    local top, bottom = TopDownCar.innerRadius.y + 15, TopDownCar.outerRadius.y - 15
    local square = 24
    for row = 0, math.floor((bottom - top) / square) - 1 do
        for column = 0, 1 do
            local color = (row + column) % 2 == 0 and '#FFF2F3FF' or '#FF26283A'
            graphics2d.drawRect({-square + column * square, top + row * square, square, square}, color, {layer = -16})
        end
    end
    for _, mark in ipairs(self.marks) do
        graphics2d.drawCircle(mark[1], mark[2], 6, '#33101018', {layer = -15})
    end
end

-- Seen from above, a cone is an orange cone with a white reflective band on its square base, and a barrel is a wooden lid with planks, a bung and an iron hoop, both with a shadow on the ground.
function TopDownCar:drawProps()
    local unit = Look.pixel
    local ow = Look.outlinePixels * unit
    for _, prop in ipairs(self.props) do
        local body = prop.body
        local x, y = body.x, body.y
        if prop.cone then
            local r = TopDownCar.coneRadius
            graphics2d.drawShape({x - r * 1.1 + 6, y - r * 1.1 + 8, r * 2.2, r * 2.2}, {radius = r * 0.5, rotation = body.rotation, color = TopDownCar.shadow, layer = 1})
            graphics2d.drawShape({x - r * 1.1, y - r * 1.1, r * 2.2, r * 2.2}, {radius = r * 0.45, rotation = body.rotation, color = '#FFD0581E', borderWidth = ow, borderColor = Look.ink, layer = 2})
            graphics2d.drawShape({x - r * 0.85, y - r * 0.85, r * 1.7, r * 1.7}, {kind = 'ellipse', color = '#FFFF7A2E', borderWidth = ow, borderColor = Look.ink, layer = 2})
            graphics2d.drawRing(x, y, r * 0.52, r * 0.24, '#FFF7F7FF', {layer = 2})
            graphics2d.drawCircle(x, y, r * 0.24, '#FFFFA35C', {layer = 2})
            graphics2d.drawCircle(x - r * 0.3, y - r * 0.38, r * 0.14, '#B3FFFFFF', {layer = 2})
        else
            local r = TopDownCar.barrelRadius
            graphics2d.drawCircle(x + 6, y + 8, r, TopDownCar.shadow, {layer = 1})
            Look.disc(x, y, r, 'darkWood', 2, body.rotation)
            local cos, sin = math.cos(body.rotation), math.sin(body.rotation)
            for _, offset in ipairs({-0.34, 0.34}) do
                local px, py = -sin * offset * r, cos * offset * r
                graphics2d.drawLine(x + px - cos * r * 0.7, y + py - sin * r * 0.7, x + px + cos * r * 0.7, y + py + sin * r * 0.7, 2.5 * unit, '#FF7A4724', {layer = 2})
            end
            graphics2d.drawRing(x, y, r * 0.86, r * 0.16, '#FF5A6070', {layer = 2})
            graphics2d.drawCircle(x + cos * r * 0.45, y + sin * r * 0.45, r * 0.13, '#FF4A2A16', {layer = 2})
        end
    end
end

function TopDownCar:drawCar()
    local body = self.car.body
    local size = TopDownCar.picture
    graphics2d.draw(self.racer, body.x, body.y, {width = size[1], height = size[2], rotation = body.rotation, layer = 3})
end

function TopDownCar:draw(area)
    self:drawTrack()
    self:drawProps()
    self:drawCar()
end

return TopDownCar
