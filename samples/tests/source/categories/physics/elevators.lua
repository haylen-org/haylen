-- Lifts on prismatic joints whose motors carry crates and a character from `physics2d.newMover` between three floors, called with the buttons beside their doors, from inside them or from the panel.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Elevators = haylen.class('Elevators', PhysicsTest)

Elevators.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}},
}

local kFloors = {390, 110, -170}
local kShafts = {-300, 300}
local kShaftWidth = 160
local kLiftSpeed = 220
local kWalkSpeed = 320
local kJumpSpeed = 640
local kGravity = 1800
local kWait = 2

function Elevators:enter()
    self:frame{
        hint = 'Walk with A and D, the arrows or the left stick and jump with Space or the south button. Press up beside a lift door to call it, and up or down inside a lift to ride it. R or the X button starts over.',
        controls = {
            ui.button{id = 'leftUp', text = 'Left lift up', onClick = function() self:send(1, 1) end},
            ui.button{id = 'leftDown', text = 'Left lift down', onClick = function() self:send(1, -1) end},
            ui.button{id = 'rightUp', text = 'Right lift up', onClick = function() self:send(2, 1) end},
            ui.button{id = 'rightDown', text = 'Right lift down', onClick = function() self:send(2, -1) end},
            ui.toggle{id = 'auto', text = 'The right lift runs by itself', checked = true, onChange = function(event) self.auto = event.checked end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = Elevators.actions,
        overlay = {
            ui.touchStick{action = 'move', radius = 110, floating = true, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
            ui.touchButton{action = 'jump', text = 'Jump', size = 150, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self.auto = true
    self:build()
end

function Elevators:build()
    self.world = physics2d.newWorld()
    local building = self.world:createBody({type = 'static'})
    local half = kShaftWidth / 2
    for index, top in ipairs(kFloors) do
        local thickness = index == 1 and 40 or 20
        for _, span in ipairs({{-780, kShafts[1] - half}, {kShafts[1] + half, kShafts[2] - half}, {kShafts[2] + half, 780}}) do
            parts.box(building, span[2] - span[1], thickness, {offsetX = (span[1] + span[2]) / 2, offsetY = top + thickness / 2})
        end
    end
    for _, x in ipairs(kShafts) do
        parts.box(building, kShaftWidth, 10, {offsetX = x, offsetY = 425})
    end
    parts.box(building, 20, 860, {offsetX = -790})
    parts.box(building, 20, 860, {offsetX = 790})
    local rail = self.world:createBody({type = 'static', y = -420})
    self.statics = {building}

    self.lifts = {}
    for _, x in ipairs(kShafts) do
        local platform = self.world:createBody({x = x, y = kFloors[1] + 10})
        parts.box(platform, kShaftWidth - 10, 20, {friction = 1})
        parts.paint(platform, '#FFFFB74D')
        local joint = self.world:createJoint('prismatic', rail, platform, {ax = x, ay = kFloors[1] + 10, axisX = 0, axisY = 1, enableLimit = true, lower = kFloors[3] - kFloors[1], upper = 0, enableMotor = true, maxMotorForce = 2e6})
        self.lifts[#self.lifts + 1] = {body = platform, joint = joint, x = x, target = 1, wait = kWait}
    end

    self.crates = {}
    for _, spot in ipairs({{-300, kFloors[1]}, {300, kFloors[1]}, {-560, kFloors[1]}, {0, kFloors[2]}, {560, kFloors[3]}}) do
        local crate = self.world:createBody({x = spot[1], y = spot[2] - 26})
        parts.box(crate, 50, 50, {friction = 0.8})
        self.crates[#self.crates + 1] = crate
    end

    self.hero = physics2d.newMover(self.world, {x = -650, y = kFloors[1] - 37, radius = 18, height = 72, stepHeight = 12, snapDistance = 10, pushForce = 30000})
    self.fall, self.jump, self.lastMoveY = 0, false, 0
end

function Elevators:send(index, direction)
    local lift = self.lifts[index]
    lift.target = m.clamp(lift.target + direction, 1, #kFloors)
end

-- Returns the floor whose top is nearest to `y`, such as the top of a lift or the feet of the character.
function Elevators.nearestFloor(y)
    local best = 1
    for index, top in ipairs(kFloors) do
        if math.abs(top - y) < math.abs(kFloors[best] - y) then
            best = index
        end
    end
    return best
end

function Elevators:liftFloor(lift)
    return Elevators.nearestFloor(kFloors[1] + lift.joint.translation)
end

function Elevators:heroFloor()
    return Elevators.nearestFloor(self.hero.y + self.hero.height / 2)
end

-- Up or down inside a lift sends it a floor, and up beside a door calls the lift of that shaft to the floor of the character.
function Elevators:press(direction)
    for index, lift in ipairs(self.lifts) do
        if self.hero.groundBody == lift.body then
            self:send(index, direction)
            return
        end
    end
    for _, lift in ipairs(self.lifts) do
        if direction > 0 and math.abs(self.hero.x - lift.x) < kShaftWidth then
            lift.target = self:heroFloor()
        end
    end
end

function Elevators:exit()
    Elevators.super.exit(self)
    input.clearVirtual()
end

function Elevators:update(dt)
    Elevators.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local _, moveY = input.vector('move')
    if moveY < -0.6 and self.lastMoveY >= -0.6 then
        self:press(1)
    elseif moveY > 0.6 and self.lastMoveY <= 0.6 then
        self:press(-1)
    end
    self.lastMoveY = moveY
    if input.pressed('jump') then
        self.jump = true
    end
    local left, right = self.lifts[1], self.lifts[2]
    local riding = (self.hero.groundBody == left.body and ', riding the left lift') or (self.hero.groundBody == right.body and ', riding the right lift') or ''
    self:status(string.format('Left lift at floor %d going to %d, right lift at floor %d going to %d, character on floor %d%s, step %.2f ms', self:liftFloor(left), left.target, self:liftFloor(right), right.target, self:heroFloor(), riding, self:stepTime()))
end

-- Each motor runs toward the translation of its target floor and slows down as it gets there, and the character moves before the world steps.
function Elevators:fixedUpdate(step)
    for index, lift in ipairs(self.lifts) do
        local offset = kFloors[lift.target] - kFloors[1] - lift.joint.translation
        lift.joint.motorSpeed = m.clamp(offset * 5, -kLiftSpeed, kLiftSpeed)
        if index == 2 and self.auto and math.abs(offset) < 2 then
            lift.wait = lift.wait - step
            if lift.wait <= 0 then
                lift.wait = kWait
                lift.direction = (lift.target == #kFloors and -1) or (lift.target == 1 and 1) or lift.direction or 1
                lift.target = lift.target + lift.direction
            end
        end
    end

    local hero = self.hero
    local moveX = input.vector('move')
    local fall = self.fall + kGravity * step
    if self.jump and hero.grounded then
        fall = -kJumpSpeed
    end
    self.jump = false
    local velocity = hero:clip(moveX * kWalkSpeed, fall)
    hero:move(velocity.x * step, velocity.y * step)
    self.fall = velocity.y
    self:simulate(self.world, step)
end

function Elevators:draw(area)
    local half = kShaftWidth / 2
    for _, lift in ipairs(self.lifts) do
        graphics2d.drawRect({lift.x - half, -430, kShaftWidth, 860}, '#FF1F2430', {layer = -2})
        graphics2d.drawLine(lift.x, -430, lift.x, lift.body.y - 10, 3, '#FF90A4AE', {layer = -1})
        for floor, top in ipairs(kFloors) do
            local lit = lift.target == floor and self:liftFloor(lift) ~= floor
            local side = lift.x < 0 and -half - 30 or half + 30
            graphics2d.drawRect({lift.x + side - 10, top - 70, 20, 28}, '#FF37474F', {layer = -1})
            graphics2d.drawCircle(lift.x + side, top - 56, 6, lit and '#FFFFD54F' or '#FF607D8B', {layer = -1})
        end
    end
    for floor, top in ipairs(kFloors) do
        Elevators.caption(string.format('Floor %d', floor), -770, top - 40)
    end
    parts.drawAll(self.statics)
    for _, lift in ipairs(self.lifts) do
        parts.draw(lift.body, {layer = 1})
    end
    parts.drawAll(self.crates, {layer = 1})
    local hero = self.hero
    local reach = hero.height / 2 - hero.radius
    graphics2d.drawRect({hero.x - hero.radius, hero.y - reach, hero.radius * 2, reach * 2}, '#FF4DD0E1', {layer = 2})
    graphics2d.drawCircle(hero.x, hero.y - reach, hero.radius, '#FF4DD0E1', {layer = 2})
    graphics2d.drawCircle(hero.x, hero.y + reach, hero.radius, '#FF4DD0E1', {layer = 2})
    graphics2d.drawCircle(hero.x + 6, hero.y - reach, 4, '#FF263238', {layer = 3})
end

return Elevators
