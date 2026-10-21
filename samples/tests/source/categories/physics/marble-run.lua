-- Glass marbles rolling down wooden rails made of chains on a toy board, through a funnel, over a paddle wheel turned by a revolute motor and back up in a kinematic lift that `body:moveTo` drives, in an endless loop.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local MarbleRun = haylen.class('MarbleRun', PhysicsTest)

MarbleRun.code = [[
track:addChain({{-680, -240}, {-660, -160}, {-100, -90}}, false, {friction = 0.4})  -- Solid on top, listed from left to right.
world:createJoint('revolute', hub, wheel, {ax = 60, ay = 120, enableMotor = true, motorSpeed = 2, maxMotorTorque = 1e7})
lift:moveTo(680, y, rotation)  -- A kinematic lift that carries the marbles up.
marble:addCircle(14, {density = 2, friction = 0.4, restitution = 0.15, rollingResistance = 0.02})
gate.enabled = false  -- Lets the waiting marbles roll onto the lift.]]

-- Chains collide on their upper side when listed from left to right, and the short first segments are lips that turn marbles back.
MarbleRun.track = {
    {{-520, -190}, {600, -295}},
    {{-680, -240}, {-660, -160}, {-100, -90}},
    {{-160, -100}, {-22, -10}},
    {{22, -10}, {160, -100}},
    {{-150, 160}, {-130, 235}, {600, 330}},
}
MarbleRun.rail = 14
MarbleRun.ground = 390
MarbleRun.board = {left = -790, right = 596, top = -380, bottom = 375}
MarbleRun.wheelSpot = {60, 120}
MarbleRun.liftPath = {x = 680, bottom = 370, top = -330, speed = 300, carry = 0.12, dump = -0.4, turn = 1.5}
-- The lift is a floor with a wall at its far end, drawn as one outline, and its cable runs down the outer face of the wall to the corner of the floor.
MarbleRun.cabin = {{-75, -7}, {63, -7}, {63, -77}, {75, -77}, {75, 7}, {-75, 7}}
MarbleRun.hitch = {75, 7}
MarbleRun.pulley = {755, -362}
MarbleRun.loadTime, MarbleRun.dumpTime = 2.5, 1.6
MarbleRun.marbleRadius = 14
MarbleRun.maxMarbles = 24
MarbleRun.states = {loading = 'loads at the bottom', rising = 'rises', dumping = 'tips out at the top', lowering = 'goes down'}

function MarbleRun:enter()
    self:frame{
        code = MarbleRun.code,
        hint = 'Marbles roll down the ramps, drop through the funnel, ride over the paddle wheel and wait at the gate for the lift that carries them back to the top. Drag marbles anywhere. R or the X button starts over.',
        controls = {
            ui.button{id = 'marble', text = 'Add a marble', onClick = function() self:addMarble(self.random:range(-400, 400), -360) end},
            ui.label{text = 'Wheel motor speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'wheel', value = 2, min = 0, max = 5, step = 0.25, showValue = true, onChange = function(event) self:setWheelSpeed(event.value) end},
            ui.toggle{id = 'lift', text = 'Run the lift', checked = true, onChange = function(event) self.running = event.checked end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'marble',
    }
    self.random = m.random(58)
    self.running, self.wheelSpeed = true, 2
    self.rails = {}
    for index, points in ipairs(MarbleRun.track) do
        local flat = {}
        for _, point in ipairs(points) do
            flat[#flat + 1] = point[1]
            flat[#flat + 1] = point[2] + MarbleRun.rail / 2
        end
        self.rails[index] = flat
    end
    self.railStroke = {width = 1, color = Look.ink, join = 'round', cap = 'round', layer = 0}
    self.cable = {0, 0, 0, 0}
    self:build()
end

function MarbleRun:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.marbles, self.trips = {}, 0

    local track = self.world:createBody({type = 'static'})
    track:addBox(4000, 40, {offsetY = MarbleRun.ground + 20})
    for _, points in ipairs(MarbleRun.track) do
        track:addChain(points, false, {friction = 0.4})
    end

    local spot = MarbleRun.wheelSpot
    local hub = self.world:createBody({type = 'static', x = spot[1], y = spot[2]})
    self.wheel = self.world:createBody({x = spot[1], y = spot[2]})
    self.wheel:addCircle(24)
    for paddle = 0, 5 do
        local angle = paddle * math.pi / 3
        self.wheel:addBox(70, 10, {offsetX = math.cos(angle) * 57, offsetY = math.sin(angle) * 57, rotation = angle})
    end
    self.motor = self.world:createJoint('revolute', hub, self.wheel, {ax = spot[1], ay = spot[2], enableMotor = true, motorSpeed = self.wheelSpeed, maxMotorTorque = 1e7})

    self.gate = self.world:createBody({type = 'static', x = 604, y = 296})
    self.gate:addBox(8, 56)
    local path = MarbleRun.liftPath
    self.lift = self.world:createBody({type = 'kinematic', x = path.x, y = path.bottom, rotation = path.carry})
    self.lift:addBox(150, 14)
    self.lift:addBox(12, 70, {offsetX = 69, offsetY = -42})
    Look.dress(self.lift, 'yellow', {outline = MarbleRun.cabin})
    self:setLift('loading')

    for index = 1, 12 do
        self:addMarble(-450 + index * 75, -350)
    end
end

function MarbleRun:addMarble(x, y)
    local marble = self.world:createBody({x = x, y = y})
    marble:addCircle(MarbleRun.marbleRadius, {density = 2, friction = 0.4, restitution = 0.15, rollingResistance = 0.02})
    Look.dress(marble, Look.toys[#self.marbles % #Look.toys + 1], {spin = true})
    self.marbles[#self.marbles + 1] = marble
    if #self.marbles > MarbleRun.maxMarbles then
        table.remove(self.marbles, 1):destroy()
    end
end

function MarbleRun:setWheelSpeed(speed)
    self.wheelSpeed = speed
    self.motor.motorSpeed = speed
end

-- The gate holds the marbles back whenever the lift is away from the bottom.
function MarbleRun:setLift(state)
    self.liftState, self.liftTime = state, 0
    self.gate.enabled = state ~= 'loading'
    if state == 'rising' then
        self.trips = self.trips + 1
    end
end

-- The lift waits at the bottom, rises tilted back against its wall, tips forward at the top and goes down again, each step a `moveTo` toward where it should be next.
function MarbleRun:moveLift(step)
    local lift, state, path = self.lift, self.liftState, MarbleRun.liftPath
    self.liftTime = self.liftTime + step
    local targetY, targetRotation = path.bottom, path.carry
    if state == 'loading' and self.running and self.liftTime > MarbleRun.loadTime then
        self:setLift('rising')
    elseif state == 'rising' then
        targetY = path.top
        if lift.y <= path.top + 0.5 then
            self:setLift('dumping')
        end
    elseif state == 'dumping' then
        targetY, targetRotation = path.top, path.dump
        if self.liftTime > MarbleRun.dumpTime then
            self:setLift('lowering')
        end
    elseif state == 'lowering' and lift.y >= path.bottom - 0.5 then
        self:setLift('loading')
    end
    local y = lift.y + m.clamp(targetY - lift.y, -path.speed * step, path.speed * step)
    local rotation = lift.rotation + m.clamp(targetRotation - lift.rotation, -path.turn * step, path.turn * step)
    lift:moveTo(path.x, y, rotation)
end

function MarbleRun:update(dt)
    MarbleRun.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local turns = math.floor(math.abs(self.wheel.angularVelocity) * 60 / (math.pi * 2) + 0.5)
    self:status(string.format('%d marbles roll, the lift %s after %d %s, and the wheel turns %d times a minute.', #self.marbles, MarbleRun.states[self.liftState], self.trips, self.trips == 1 and 'trip' or 'trips', turns))
end

function MarbleRun:fixedUpdate(step)
    self:moveLift(step)
    self:simulate(self.world, step)
end

-- The board is a plywood panel in a frame, and every rail is a wooden strip along its chain, screwed to the panel at its ends.
function MarbleRun:drawBoard()
    local unit = Look.pixel
    local board = MarbleRun.board
    graphics2d.drawShape({board.left, board.top, board.right - board.left, board.bottom - board.top}, {radius = 18, color = '#FFF2D4A0', borderWidth = 10, borderColor = '#FFA86B3C', layer = -6})
    graphics2d.drawShape({board.left, board.top, board.right - board.left, board.bottom - board.top}, {radius = 18, color = '#00000000', borderWidth = 4 * unit, borderColor = Look.ink, layer = -6})
    for y = board.top + 60, board.bottom - 30, 90 do
        graphics2d.drawLine(board.left + 14, y, board.right - 14, y + 6, 2 * unit, '#33A86B3C', {layer = -6})
    end
    local stroke = self.railStroke
    for index, rail in ipairs(self.rails) do
        stroke.width, stroke.color = MarbleRun.rail + Look.outlinePixels * unit * 2, Look.ink
        graphics2d.drawStroke(rail, stroke)
        stroke.width, stroke.color = MarbleRun.rail, '#FFA86B3C'
        graphics2d.drawStroke(rail, stroke)
        local points = MarbleRun.track[index]
        for _, point in ipairs({points[1], points[#points]}) do
            Look.pin(point[1], point[2] + MarbleRun.rail / 2, 4, 0)
        end
    end
end

-- The paddle wheel is a wooden plate behind six paddles on a steel hub, and the gate a red bar that lifts while the lift loads.
function MarbleRun:drawWheel()
    local wheel = self.wheel
    Look.disc(wheel.x, wheel.y, 48, 'darkWood', 0, wheel.rotation)
    for paddle = 0, 5 do
        local angle = wheel.rotation + paddle * math.pi / 3
        Look.block(wheel.x + math.cos(angle) * 57, wheel.y + math.sin(angle) * 57, 72, 14, 'wood', 1, angle)
    end
    Look.disc(wheel.x, wheel.y, 24, 'steel', 1)
    Look.pin(wheel.x, wheel.y, 8, 1)
end

-- The lift runs in a steel shaft with braces, on a cable from a pulley at the top.
function MarbleRun:drawLift()
    local path, lift = MarbleRun.liftPath, self.lift
    local left, right = path.x - 84, path.x + 82
    Look.line(right, -420, right, MarbleRun.ground, 8, '#FF66708A', -2)
    Look.line(left, -420, left, -300, 8, '#FF66708A', -2)
    Look.line(left, -230, left, 250, 8, '#FF66708A', -2)
    for y = -200, 200, 120 do
        Look.line(left, y, right, y + 120, 4, '#FF474F66', -3)
    end
    local cos, sin = math.cos(lift.rotation), math.sin(lift.rotation)
    local hitch = MarbleRun.hitch
    local cable = self.cable
    cable[1], cable[2] = MarbleRun.pulley[1], MarbleRun.pulley[2]
    cable[3], cable[4] = lift.x + hitch[1] * cos - hitch[2] * sin, lift.y + hitch[1] * sin + hitch[2] * cos
    Look.line(cable[1], cable[2], cable[3], cable[4], 3, '#FF2F3352', 2)
    Look.disc(MarbleRun.pulley[1] - 14, MarbleRun.pulley[2], 16, 'steel', 1, haylen.elapsed())
    Look.pin(cable[3], cable[4], 5, 2)
    Look.draw(lift, {layer = 1})
    local gate = self.gate
    Look.block(gate.x, gate.y - (gate.enabled and 0 or 50), 10, 56, 'red', 1)
end

function MarbleRun:draw(area)
    Scenery.sky(self.camera, MarbleRun.ground)
    Scenery.ground(MarbleRun.ground)
    self:drawBoard()
    self:drawWheel()
    self:drawLift()
    Look.drawAll(self.marbles, {layer = 2})
    self.grab:draw()
end

return MarbleRun
