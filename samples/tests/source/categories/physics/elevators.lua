-- Lifts on prismatic joints whose motors carry crates and a hero from `physics2d.newMover` between three floors of a building, called with the buttons beside their doors, from inside them or from the panel.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Hero = require('categories.physics.hero')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Elevators = haylen.class('Elevators', PhysicsTest)

Elevators.code = [[
local options = {ax = -300, ay = 360, axisY = 1, enableLimit = true, lower = -480}
local lift = world:createJoint('prismatic', rail, platform, options)
lift.enableMotor, lift.maxMotorForce = true, 2e6
lift.motorSpeed = m.clamp((floorTop - 350 - lift.translation) * 5, -220, 220)
local hero = physics2d.newMover(world, {radius = 18, height = 72, pushForce = 30000})
local riding = hero.groundBody == platform]]

Elevators.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}},
}

Elevators.floors = {350, 110, -130}
-- The roof of the building, above the top floor.
Elevators.roof = -310
Elevators.shafts = {-300, 300}
Elevators.shaftWidth = 160
Elevators.liftSpeed = 220
Elevators.walkSpeed = 320
Elevators.jumpSpeed = 640
Elevators.gravity = 1800
Elevators.wait = 2
Elevators.heroHeight = 72
Elevators.names = {'the left lift', 'the right lift'}

function Elevators:enter()
    self:frame{
        code = Elevators.code,
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
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
            ui.touchButton{action = 'jump', text = 'Jump', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.camera.position = {0, 30}
    self.auto = true
    self.heroArt = Hero(Elevators.heroHeight)
    self.glow = Scenery.glow()
    self:build()
end

function Elevators:build()
    if self.world then
        self.world:destroy()
    end
    self.world = physics2d.newWorld({owner = self})
    local building = self.world:createBody({type = 'static'})
    local half = Elevators.shaftWidth / 2
    local shafts, floors = Elevators.shafts, Elevators.floors
    self.slabs = {}
    for index, top in ipairs(floors) do
        local thickness = index == 1 and 40 or 24
        for _, span in ipairs({{-780, shafts[1] - half}, {shafts[1] + half, shafts[2] - half}, {shafts[2] + half, 780}}) do
            building:addBox(span[2] - span[1], thickness, {offsetX = (span[1] + span[2]) / 2, offsetY = top + thickness / 2})
            self.slabs[#self.slabs + 1] = {(span[1] + span[2]) / 2, top + thickness / 2, span[2] - span[1], thickness}
        end
    end
    for _, x in ipairs(shafts) do
        building:addBox(Elevators.shaftWidth, 10, {offsetX = x, offsetY = floors[1] + 30})
    end
    local wall = floors[1] - Elevators.roof
    building:addBox(24, wall, {offsetX = -792, offsetY = Elevators.roof + wall / 2})
    building:addBox(24, wall, {offsetX = 792, offsetY = Elevators.roof + wall / 2})
    building:addBox(1608, 24, {offsetY = Elevators.roof + 12})
    local rail = self.world:createBody({type = 'static', y = Elevators.roof})

    self.lifts = {}
    for _, x in ipairs(shafts) do
        local platform = self.world:createBody({x = x, y = floors[1] + 10})
        platform:addBox(Elevators.shaftWidth - 10, 20, {friction = 1})
        Look.dress(platform, 'steel')
        local joint = self.world:createJoint('prismatic', rail, platform, {ax = x, ay = floors[1] + 10, axisX = 0, axisY = 1, enableLimit = true, lower = floors[3] - floors[1], upper = 0, enableMotor = true, maxMotorForce = 2e6})
        self.lifts[#self.lifts + 1] = {body = platform, joint = joint, x = x, target = 1, wait = Elevators.wait}
    end

    self.crates = {}
    for _, spot in ipairs({{-300, floors[1]}, {300, floors[1]}, {-560, floors[1]}, {0, floors[2]}, {560, floors[3]}}) do
        local crate = self.world:createBody({x = spot[1], y = spot[2] - 26})
        crate:addBox(50, 50, {friction = 0.8})
        Look.dress(crate, 'woodCrate')
        self.crates[#self.crates + 1] = crate
    end

    self.hero = physics2d.newMover(self.world, {x = -650, y = floors[1] - 37, radius = 18, height = Elevators.heroHeight, stepHeight = 12, snapDistance = 10, pushForce = 30000})
    self.fall, self.jump, self.lastMoveY = 0, false, 0
end

function Elevators:send(index, direction)
    local lift = self.lifts[index]
    lift.target = m.clamp(lift.target + direction, 1, #Elevators.floors)
end

-- Returns the floor whose top is nearest to `y`, such as the top of a lift or the feet of the hero.
function Elevators.nearestFloor(y)
    local floors = Elevators.floors
    local best = 1
    for index, top in ipairs(floors) do
        if math.abs(top - y) < math.abs(floors[best] - y) then
            best = index
        end
    end
    return best
end

function Elevators:liftFloor(lift)
    return Elevators.nearestFloor(Elevators.floors[1] + lift.joint.translation)
end

function Elevators:heroFloor()
    return Elevators.nearestFloor(self.hero.y + self.hero.height / 2)
end

-- Up or down inside a lift sends it a floor, and up beside a door calls the lift of that shaft to the floor of the hero.
function Elevators:press(direction)
    for index, lift in ipairs(self.lifts) do
        if self.hero.groundBody == lift.body then
            self:send(index, direction)
            return
        end
    end
    for _, lift in ipairs(self.lifts) do
        if direction > 0 and math.abs(self.hero.x - lift.x) < Elevators.shaftWidth then
            lift.target = self:heroFloor()
        end
    end
end

function Elevators:exit()
    Elevators.super.exit(self)
    input.clearVirtual()
end

function Elevators:describe(index)
    local lift = self.lifts[index]
    local floor = self:liftFloor(lift)
    if floor == lift.target then
        return string.format('%s waits on floor %d', Elevators.names[index], floor)
    end
    return string.format('%s rides to floor %d', Elevators.names[index], lift.target)
end

function Elevators:update(dt)
    Elevators.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local moveX, moveY = input.vector('move')
    if moveY < -0.6 and self.lastMoveY >= -0.6 then
        self:press(1)
    elseif moveY > 0.6 and self.lastMoveY <= 0.6 then
        self:press(-1)
    end
    self.lastMoveY = moveY
    if input.pressed('jump') then
        self.jump = true
    end
    self.heroArt:update(dt, moveX * Elevators.walkSpeed, self.hero.grounded)
    local riding = ''
    for index, lift in ipairs(self.lifts) do
        if self.hero.groundBody == lift.body then
            riding = string.format(' and rides %s', Elevators.names[index])
        end
    end
    local left = self:describe(1)
    self:status(string.format('%s%s, %s, and the hero stands on floor %d%s.', left:sub(1, 1):upper(), left:sub(2), self:describe(2), self:heroFloor(), riding))
end

-- Each motor runs toward the translation of its target floor and slows down as it gets there, and the hero moves before the world steps.
function Elevators:fixedUpdate(step)
    local floors = Elevators.floors
    for index, lift in ipairs(self.lifts) do
        local offset = floors[lift.target] - floors[1] - lift.joint.translation
        lift.joint.motorSpeed = m.clamp(offset * 5, -Elevators.liftSpeed, Elevators.liftSpeed)
        if index == 2 and self.auto and math.abs(offset) < 2 then
            lift.wait = lift.wait - step
            if lift.wait <= 0 then
                lift.wait = Elevators.wait
                lift.direction = (lift.target == #floors and -1) or (lift.target == 1 and 1) or lift.direction or 1
                lift.target = lift.target + lift.direction
            end
        end
    end

    local hero = self.hero
    local moveX = input.vector('move')
    local fall = self.fall + Elevators.gravity * step
    if self.jump and hero.grounded then
        fall = -Elevators.jumpSpeed
    end
    self.jump = false
    local velocity = hero:clip(moveX * Elevators.walkSpeed, fall)
    hero:move(velocity.x * step, velocity.y * step)
    self.fall = velocity.y
    self:simulate(self.world, step)
end

-- The building: a papered back wall under a roof with the shafts dark behind it, concrete floors, brick walls, a cage around each lift on its cables and a call lamp beside every door, green where the lift waits and amber, blinking, where it is called.
function Elevators:drawBuilding()
    local unit = Look.pixel
    local half = Elevators.shaftWidth / 2
    local floors, roof = Elevators.floors, Elevators.roof
    graphics2d.drawRect({-780, roof, 1560, floors[1] - roof}, '#FFF3E3C4', {layer = -20})
    for x = -780, 780, 120 do
        graphics2d.drawLine(x, roof, x, floors[1], 3 * unit, '#FFE6D2AE', {layer = -19})
    end
    for _, slab in ipairs(self.slabs) do
        Look.block(slab[1], slab[2], slab[3], slab[4], 'slate', -8)
    end
    for _, x in ipairs({-792, 792}) do
        Look.block(x, (roof + floors[1]) / 2, 24, floors[1] - roof, 'brick', -7)
    end
    Look.block(0, roof + 12, 1640, 24, 'slate', -6)
    Look.block(0, roof - 10, 1680, 20, 'brick', -6)
    for _, lift in ipairs(self.lifts) do
        graphics2d.drawRect({lift.x - half, roof + 24, Elevators.shaftWidth, floors[1] - roof - 24}, '#FF3A3F5E', {layer = -18})
        local top = lift.body.y - 10
        for _, side in ipairs({-half + 14, half - 14}) do
            Look.line(lift.x + side, roof + 24, lift.x + side, top - 110, 3 * unit, '#FF9AA5BE', -6)
        end
        Look.block(lift.x, top - 110, Elevators.shaftWidth - 6, 14, 'iron', 1)
        for _, side in ipairs({-half + 4, half - 4}) do
            Look.block(lift.x + side, top - 55, 8, 110, 'iron', 1)
        end
        for floor, level in ipairs(floors) do
            self:drawLamp(lift, floor, level)
        end
    end
end

-- A call lamp is a round lamp on a steel plate beside the door of a floor: green and glowing where the lift waits with its doors open, amber and blinking where it is called, and dark glass otherwise.
function Elevators:drawLamp(lift, floor, level)
    local unit = Look.pixel
    local half = Elevators.shaftWidth / 2
    local x = lift.x + (lift.x < 0 and -half - 34 or half + 34)
    local y = level - 64
    local here = self:liftFloor(lift) == floor and lift.target == floor
    local called = lift.target == floor and not here
    local lit = here or called and math.sin(haylen.elapsed() * 8) > -0.3
    local color = here and '#FF6FDC8C' or called and '#FFFFC93C' or '#FF3A3F5E'
    Look.block(x, y, 32, 46, 'steel', -5)
    if lit then
        graphics2d.draw(self.glow, x, y, {width = 70, height = 70, color = color, blend = 'additive', layer = -4})
    end
    graphics2d.drawShape({x - 10, y - 10, 20, 20}, {kind = 'ellipse', color = color, borderWidth = 3 * unit, borderColor = Look.ink, layer = -4})
    if lit then
        graphics2d.drawCircle(x - 3, y - 3, 3.5, '#CCFFFFFF', {layer = -4})
    end
end

function Elevators:draw(area)
    Scenery.sky(self.camera, Elevators.floors[1])
    Scenery.ground(Elevators.floors[1])
    self:drawBuilding()
    for floor, top in ipairs(Elevators.floors) do
        self:label(string.format('Floor %d', floor), -700, top - 40, {size = 20})
    end
    for _, lift in ipairs(self.lifts) do
        Look.draw(lift.body, {layer = 1})
    end
    Look.drawAll(self.crates, {layer = 1})
    self.heroArt:draw(self.hero.x, self.hero.y + Elevators.heroHeight / 2, 2)
end

return Elevators
