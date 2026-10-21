-- A claw machine in a glass cabinet: the carriage slides on a prismatic joint along the rail, the claw drops on a second prismatic joint, and two fingers close with motorized revolute joints that hold a plush prize by friction, driven with keys, a gamepad or touch. The machine plays by itself until the player moves the claw.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')

local Claw = haylen.class('Claw', PhysicsTest)

Claw.code = [[
local slider = world:createJoint('prismatic', cabinet, carriage, {ax = 0, ay = -380, axisX = 1, axisY = 0, enableLimit = true, lower = -680, upper = 680, enableMotor = true, maxMotorForce = 2e5})
local cable = world:createJoint('prismatic', carriage, head, {ax = 0, ay = -300, axisX = 0, axisY = 1, enableLimit = true, lower = 0, upper = 595, enableMotor = true, maxMotorForce = 2e4})
local finger = world:createJoint('revolute', head, hook, {ax = 28, ay = -285, enableLimit = true, lower = -0.35, upper = 0.45, enableMotor = true, maxMotorTorque = 60000})
slider.motorSpeed, cable.motorSpeed = moveX * 320, moveY * 260
finger.motorSpeed = closed and 3 or -3  -- The fingers hold the prize by friction.]]

Claw.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'grip', type = 'button', bindings = {'key:space', 'key:enter', 'button:south', 'virtual:grip'}},
}

Claw.rail = -380
Claw.headY = -300
Claw.floor = 390
Claw.carriageSpeed, Claw.dropSpeed = 320, 260
Claw.fingerSpeed = 3
Claw.gripTorque = 60000
Claw.chute = {-680, 360, 180, 60}
Claw.chuteWall = -570
-- The prizes: plush toys painted as pictures over a box, and balls with a face.
Claw.toys = {
    {picture = 'physics/bear.png', width = 60, height = 68, drawWidth = 70, drawHeight = 81},
    {picture = 'physics/bunny.png', width = 52, height = 66, drawWidth = 58, drawHeight = 73},
    {ball = 28},
}
-- Each finger is drawn as a steel stroke through these points, from its hinge to the tip of its hook, mirrored on the left.
Claw.fingerPath = {0, 0, 18, 70, 4, 77}
-- The steps of the machine when it plays by itself: it seeks a prize, drops, grabs, lifts, carries it over the chute and lets go.
Claw.demoSteps = {seek = 'looks for a prize', drop = 'drops the claw', grab = 'grabs', lift = 'lifts the prize', carry = 'carries it to the chute', release = 'lets go'}

function Claw:enter()
    self:frame{
        code = Claw.code,
        hint = 'Move the claw with the arrows, WASD, the left stick or the touch stick, where up and down raise and lower it, and close or open it with Space, Enter or the south button. Drop prizes in the chute on the left.',
        controls = {
            ui.label{text = 'Grip strength', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'grip', value = 1, min = 0.2, max = 2, step = 0.1, showValue = true, onChange = function(event) self:setGrip(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = Claw.actions,
        view = {1860, 940},
        overlay = {
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
            ui.touchButton{action = 'grip', text = 'Grab', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.camera.position = {0, -15}
    self.random = m.random(60)
    self.strength = 1
    self.pictures = {}
    for index, toy in ipairs(Claw.toys) do
        if toy.picture then
            self.pictures[index] = assets.texture(toy.picture, {filter = 'linear'})
        end
    end
    self.fingerPoints = {0, 0, 0, 0, 0, 0}
    self.fingerStroke = {width = 1, color = Look.ink, join = 'round', cap = 'round', layer = 3}
    self.prizeOptions = {width = 0, height = 0, rotation = 0, layer = 1}
    self:build()
end

function Claw:build()
    self.world = physics2d.newWorld()
    self.prizes, self.won, self.closed = {}, 0, false
    self.demo, self.step, self.wait, self.target = true, 'seek', 0, nil

    local cabinet = self.world:createBody({type = 'static'})
    cabinet:addBox(1600, 40, {offsetY = Claw.floor + 20})
    cabinet:addBox(20, 860, {offsetX = -790})
    cabinet:addBox(20, 860, {offsetX = 790})
    cabinet:addBox(20, 160, {offsetX = Claw.chuteWall, offsetY = 330})
    cabinet:addBox(1560, 16, {offsetY = Claw.rail - 30})
    local chute = Claw.chute
    cabinet:addBox(chute[3], chute[4], {offsetX = chute[1], offsetY = chute[2], sensor = true})
    self.cabinet = cabinet

    -- The parts of the claw share a negative group, so they never collide with each other.
    self.carriage = self.world:createBody({y = Claw.rail})
    self.carriage:addBox(90, 26, {group = -1})
    self.slider = self.world:createJoint('prismatic', cabinet, self.carriage, {ax = 0, ay = Claw.rail, axisX = 1, axisY = 0, enableLimit = true, lower = -680, upper = 680, enableMotor = true, maxMotorForce = 2e5})
    self.head = self.world:createBody({y = Claw.headY})
    self.head:addBox(70, 30, {group = -1})
    self.cable = self.world:createJoint('prismatic', self.carriage, self.head, {ax = 0, ay = Claw.headY, axisX = 0, axisY = 1, enableLimit = true, lower = 0, upper = 595, enableMotor = true, maxMotorForce = 2e4})
    self.fingers = {self:finger(-1), self:finger(1)}

    for index = 1, 8 do
        self:dropPrize(-480 + index * 140, 300)
    end

    self.world.onSensorBegin = function(sensor, visitor)
        if visitor.data and visitor.data.prize and not visitor.data.won then
            visitor.data.won, visitor.data.leaving = true, 0.8
            self.won = self.won + 1
        end
    end
end

-- A finger hangs from a bottom corner of the head, leaning out, with a hook at its tip that points inward. Closing turns the left finger counter-clockwise and the right one clockwise.
function Claw:finger(side)
    local x, y = side * 28, Claw.headY + 15
    local body = self.world:createBody({x = x, y = y})
    body:addBox(12, 72, {offsetX = side * 9, offsetY = 35, rotation = -side * 0.25, group = -1, friction = 1})
    body:addBox(20, 10, {offsetX = side * 12, offsetY = 74, group = -1, friction = 1})
    local lower, upper = side < 0 and -0.45 or -0.35, side < 0 and 0.35 or 0.45
    local joint = self.world:createJoint('revolute', self.head, body, {ax = x, ay = y, enableLimit = true, lower = lower, upper = upper, enableMotor = true, motorSpeed = -side * Claw.fingerSpeed, maxMotorTorque = Claw.gripTorque * self.strength})
    return {body = body, joint = joint, side = side}
end

function Claw:dropPrize(x, y)
    local body = self.world:createBody({x = x, y = y, rotation = self.random:range(-0.3, 0.3)})
    local kind = self.random:integer(1, #Claw.toys)
    local toy = Claw.toys[kind]
    if toy.ball then
        body:addCircle(toy.ball, {density = 0.6, friction = 0.9})
        Look.dress(body, Look.toys[self.random:integer(1, #Look.toys)])
    else
        body:addBox(toy.width, toy.height, {density = 0.6, friction = 0.9})
    end
    body.data = body.data or {}
    body.data.prize, body.data.kind = true, kind
    self.prizes[#self.prizes + 1] = body
end

function Claw:setGrip(strength)
    self.strength = strength
    for _, finger in ipairs(self.fingers) do
        finger.joint.maxMotorTorque = Claw.gripTorque * strength
    end
end

function Claw:exit()
    Claw.super.exit(self)
    input.clearVirtual()
end

-- When the machine plays by itself, it picks the prize nearest the middle, lowers the open claw onto it, closes, lifts, carries it over the chute and opens.
function Claw:play(dt)
    local carriage = self.carriage
    self.wait = math.max(0, self.wait - dt)
    if not (self.target and self.target.valid and not self.target.data.won) then
        self.target, self.step = nil, 'seek'
        local best = math.huge
        for _, prize in ipairs(self.prizes) do
            if prize.valid and not prize.data.won and prize.x > Claw.chuteWall + 80 and math.abs(prize.x) < best then
                self.target, best = prize, math.abs(prize.x)
            end
        end
    end
    local target = self.target
    local moveX, moveY = 0, 0
    if self.step == 'seek' and target then
        self.closed = false
        moveX = m.clamp((target.x - carriage.x) / 60, -1, 1)
        if math.abs(target.x - carriage.x) < 6 then
            self.step = 'drop'
        end
    elseif self.step == 'drop' then
        moveY = 1
        if self.head.y + 70 > target.y or self.cable.translation > 590 then
            self.step, self.closed, self.wait = 'grab', true, 0.7
        end
    elseif self.step == 'grab' and self.wait == 0 then
        self.step = 'lift'
    elseif self.step == 'lift' then
        moveY = -1
        if self.cable.translation < 4 then
            self.step = 'carry'
        end
    elseif self.step == 'carry' then
        local chute = Claw.chute[1]
        moveX = m.clamp((chute - carriage.x) / 60, -1, 1)
        if math.abs(chute - carriage.x) < 6 then
            self.step, self.closed, self.wait = 'release', false, 1
        end
    elseif self.step == 'release' and self.wait == 0 then
        self.target, self.step = nil, 'seek'
    end
    return moveX, moveY
end

function Claw:update(dt)
    Claw.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local moveX, moveY = input.vector('move')
    if input.pressed('grip') then
        self.closed, self.demo = not self.closed, false
    end
    if moveX ~= 0 or moveY ~= 0 then
        self.demo = false
    end
    if self.demo then
        moveX, moveY = self:play(dt)
    end
    self.slider.motorSpeed = moveX * Claw.carriageSpeed
    self.cable.motorSpeed = moveY * Claw.dropSpeed
    for _, finger in ipairs(self.fingers) do
        finger.joint.motorSpeed = (self.closed and finger.side or -finger.side) * Claw.fingerSpeed
    end
    local won = self.won == 1 and '1 prize' or string.format('%d prizes', self.won)
    if self.demo then
        self:status(string.format('The machine plays by itself until you move the claw: it %s, and it has won %s.', Claw.demoSteps[self.step], won))
    else
        self:status(string.format('The claw is %d units from the middle and %d units down, it is %s, and you have won %s.', PhysicsTest.round(self.slider.translation), PhysicsTest.round(self.cable.translation), self.closed and 'closed' or 'open', won))
    end
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

-- The cabinet is a red frame with a row of bulbs on its top, a dark back, the prize chute behind its wall and a glass front with reflections over everything.
function Claw:drawCabinet()
    local unit = Look.pixel
    local bounds = graphics2d.canvasBounds()
    graphics2d.drawRect(bounds, '#FF221D3B', {layer = -30})
    graphics2d.drawShape({-830, -470, 1660, 910}, {radius = 40, color = '#FFC4342C', borderWidth = 5 * unit, borderColor = Look.ink, layer = -20})
    graphics2d.drawShape({-800, -420, 1600, 830}, {radius = 18, color = '#FF2C2752', layer = -20})
    local time = haylen.elapsed()
    for index = 0, 26 do
        local x = -780 + index * 60
        local on = (index + math.floor(time * 4)) % 3 == 0
        Look.disc(x, -446, 8, on and 'yellow' or 'orange', -19)
    end
    Look.block(0, Claw.floor + 20, 1580, 40, 'darkWood', -5)
    local chute = Claw.chute
    graphics2d.drawShape({chute[1] - chute[3] / 2, chute[2] - chute[4] / 2 - 10, chute[3], chute[4] + 30}, {radius = 10, color = '#FF15172A', borderWidth = 3 * unit, borderColor = '#FFFFC93C', layer = -4})
    Look.block(Claw.chuteWall, 330, 20, 160, 'steel', 4)
    Look.block(0, Claw.rail - 30, 1560, 16, 'iron', 2)
    self:label('Prize chute', chute[1], 210, {size = 22, plate = '#F2B9302A'})
end

function Claw:drawGlass()
    for _, stripe in ipairs({{-520, 120}, {-430, 50}, {260, 90}}) do
        local x, width = stripe[1], stripe[2]
        graphics2d.drawPolygon({{x, -420}, {x + width, -420}, {x + width - 300, 410}, {x - 300, 410}}, '#14FFFFFF', {layer = 10})
    end
end

-- A prize is its plush picture or a ball with a happy face, which turns with its body.
function Claw:drawPrize(prize)
    local toy = Claw.toys[prize.data.kind]
    if toy.ball then
        Look.draw(prize, {layer = 1})
        local cos, sin = math.cos(prize.rotation), math.sin(prize.rotation)
        for _, side in ipairs({-1, 1}) do
            local ex, ey = side * 9, -5
            graphics2d.drawCircle(prize.x + ex * cos - ey * sin, prize.y + ex * sin + ey * cos, 3.5, Look.ink, {layer = 1})
        end
        graphics2d.drawArc(prize.x + 4 * sin, prize.y - 4 * cos, 10, 3, prize.rotation + 0.5, prize.rotation + math.pi - 0.5, Look.ink, {layer = 1})
        return
    end
    local options = self.prizeOptions
    options.width, options.height, options.rotation = toy.drawWidth, toy.drawHeight, prize.rotation
    graphics2d.draw(self.pictures[prize.data.kind], prize.x, prize.y, options)
end

-- The claw is a trolley on the rail, a steel cable, a rounded head and two steel fingers that end in hooks.
function Claw:drawClaw()
    local unit = Look.pixel
    local carriage, head = self.carriage, self.head
    Look.block(carriage.x, carriage.y, 90, 26, 'yellow', 3)
    Look.disc(carriage.x - 28, carriage.y - 14, 8, 'steel', 3)
    Look.disc(carriage.x + 28, carriage.y - 14, 8, 'steel', 3)
    Look.line(carriage.x, carriage.y + 13, head.x, head.y - 15, 4, '#FF474F66', 2)
    local stroke, points = self.fingerStroke, self.fingerPoints
    for _, finger in ipairs(self.fingers) do
        local body, side = finger.body, finger.side
        local cos, sin = math.cos(body.rotation), math.sin(body.rotation)
        for index = 1, #Claw.fingerPath, 2 do
            local x, y = Claw.fingerPath[index] * side, Claw.fingerPath[index + 1]
            points[index], points[index + 1] = body.x + x * cos - y * sin, body.y + x * sin + y * cos
        end
        stroke.width, stroke.color = 12 + Look.outlinePixels * unit * 2, Look.ink
        graphics2d.drawStroke(points, stroke)
        stroke.width, stroke.color = 12, '#FFA9BCD3'
        graphics2d.drawStroke(points, stroke)
    end
    Look.block(head.x, head.y, 74, 32, 'steel', 3)
    graphics2d.drawShape({head.x - 26, head.y - 40, 52, 40}, {kind = 'ellipse', startAngle = math.pi, sweep = math.pi, color = '#FFF0473E', borderWidth = Look.outlinePixels * unit, borderColor = Look.ink, layer = 3})
end

function Claw:draw(area)
    self:drawCabinet()
    for _, prize in ipairs(self.prizes) do
        if prize.valid then
            self:drawPrize(prize)
        end
    end
    self:drawClaw()
    self:drawGlass()
end

return Claw
