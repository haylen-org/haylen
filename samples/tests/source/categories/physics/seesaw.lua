-- A seesaw on a revolute joint that tips toward the heavier end, and a catapult whose arm throws a stone when a heavy weight falls on its short end.
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

local Seesaw = haylen.class('Seesaw', PhysicsTest)

Seesaw.code = [[
local hinge = world:createJoint('revolute', fulcrum, plank, {ax = -380, ay = 320, enableLimit = true, lower = -0.3, upper = 0.3})
local catapult = world:createJoint('revolute', post, arm, {ax = 320, ay = 300, enableLimit = true, lower = -0.7, upper = 0})  -- The stone leaves at the limit.
for _, contact in ipairs(plank:contacts()) do load = load + contact.other.mass end  -- The masses on the plank.]]

Seesaw.ground = 390
Seesaw.center = -380
Seesaw.weights = {
    light = {size = 50, density = 1, material = 'wood'},
    medium = {size = 60, density = 3, material = 'stone'},
    heavy = {size = 70, density = 6, material = 'iron'},
    catapult = {size = 80, density = 4, material = 'iron'},
}
Seesaw.pivot = {320, 300}
Seesaw.armAngle = 0.2
Seesaw.stoneSpot = {358, -31}

function Seesaw:enter()
    self:frame{
        code = Seesaw.code,
        hint = 'Drop weights on either end of the seesaw, or drag them where you like. Drop the catapult weight on the short end of its arm to throw the stone. R or the X button starts over.',
        controls = {
            ui.label{text = 'Weight', font = 'heading'},
            ui.radioGroup{id = 'mass', items = {{id = 'light', text = 'Light wood'}, {id = 'medium', text = 'Medium stone'}, {id = 'heavy', text = 'Heavy iron'}}, selected = 'medium', onChange = function(event) self.mass = event.value end},
            ui.button{id = 'left', text = 'Drop on the left end', onClick = function() self:drop(Seesaw.center - 250, self.mass) end},
            ui.button{id = 'right', text = 'Drop on the right end', onClick = function() self:drop(Seesaw.center + 250, self.mass) end},
            ui.button{id = 'throw', text = 'Drop the catapult weight', onClick = function() self:drop(Seesaw.pivot[1] - 98, 'catapult', -150) end},
            ui.button{id = 'catapult', text = 'Set the catapult again', onClick = function() self:buildCatapult() end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'left',
        view = {1600, 660},
    }
    self.camera.position = {0, 70}
    self.mass = 'medium'
    self.random = m.random(41)
    self:build()
    timer.after(0.6, function() self:drop(Seesaw.center + 250, 'heavy') end, {owner = self})
    timer.after(1.2, function() self:drop(Seesaw.pivot[1] - 98, 'catapult', -150) end, {owner = self})
end

function Seesaw:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    local ground = self.world:createBody({type = 'static', x = 0, y = Seesaw.ground + 30})
    ground:addBox(8000, 60)
    local fulcrum = self.world:createBody({type = 'static'})
    fulcrum:addPolygon({{Seesaw.center - 50, Seesaw.ground}, {Seesaw.center + 50, Seesaw.ground}, {Seesaw.center, 320}})
    Look.dress(fulcrum, 'stone')
    self.statics = {fulcrum}

    self.plank = self.world:createBody({x = Seesaw.center, y = 311})
    self.plank:addBox(620, 18, {friction = 0.8})
    Look.dress(self.plank, 'wood')
    self.hinge = self.world:createJoint('revolute', fulcrum, self.plank, {ax = Seesaw.center, ay = 320, enableLimit = true, lower = -0.3, upper = 0.3})
    self.weights = {}
    self.post, self.arm, self.stone = nil, nil, nil
    self:buildCatapult()
end

-- The arm rests with its long end on the ground and turns up to its limit, where the stone leaves its cup.
function Seesaw:buildCatapult()
    for _, body in ipairs({self.post, self.arm, self.stone}) do
        body:destroy()
    end
    local pivot = Seesaw.pivot
    self.post = self.world:createBody({type = 'static', x = pivot[1], y = pivot[2]})
    self.post:addBox(30, 90, {offsetY = 45})
    Look.dress(self.post, 'darkWood')
    self.arm = self.world:createBody({x = pivot[1], y = pivot[2], rotation = Seesaw.armAngle})
    self.arm:addBox(520, 18, {offsetX = 140, friction = 0.9})
    self.arm:addBox(10, 30, {offsetX = 316, offsetY = -24})
    self.arm:addBox(10, 30, {offsetX = 400, offsetY = -24})
    Look.dress(self.arm, 'wood')
    self.catapult = self.world:createJoint('revolute', self.post, self.arm, {ax = pivot[1], ay = pivot[2], enableLimit = true, lower = -0.7, upper = 0})

    local cos, sin = math.cos(Seesaw.armAngle), math.sin(Seesaw.armAngle)
    local spot = Seesaw.stoneSpot
    local x, y = pivot[1] + spot[1] * cos - spot[2] * sin, pivot[2] + spot[1] * sin + spot[2] * cos
    self.stone = self.world:createBody({x = x, y = y})
    self.stone:addCircle(22, {density = 2})
    Look.dress(self.stone, 'stone')
    self.throw = {x = x, y = y, height = 0, distance = 0}
end

function Seesaw:drop(x, kind, y)
    local spec = Seesaw.weights[kind]
    local weight = self.world:createBody({x = x + self.random:range(-20, 20), y = y or -300})
    weight:addBox(spec.size, spec.size, {density = spec.density, friction = 0.8})
    Look.dress(weight, spec.material)
    self.weights[#self.weights + 1] = weight
end

-- Adds up the masses that rest on each side of the seesaw from the contacts of the plank.
function Seesaw:loads()
    local left, right = 0, 0
    for _, contact in ipairs(self.plank:contacts()) do
        local other = contact.other
        if other.type == 'dynamic' then
            if other.x < self.plank.x then
                left = left + other.mass
            else
                right = right + other.mass
            end
        end
    end
    return left, right
end

function Seesaw:update(dt)
    Seesaw.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local throw = self.throw
    throw.height = math.max(throw.height, throw.y - self.stone.y)
    throw.distance = math.max(throw.distance, math.abs(self.stone.x - throw.x))
    local left, right = self:loads()
    local tilt = PhysicsTest.degrees(self.hinge.angle)
    local lean = tilt == 0 and 'stands level' or string.format('leans %d degrees to the %s', math.abs(tilt), tilt > 0 and 'right' or 'left')
    local stone = throw.height > 10 and string.format(', and the stone flew %d units high and %d far', math.floor(throw.height + 0.5), math.floor(throw.distance + 0.5)) or ', and the stone waits in its cup'
    self:status(string.format('The seesaw %s with %.1f kilograms on the left and %.1f on the right%s.', lean, left, right, stone))
end

function Seesaw:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Seesaw:draw(area)
    Scenery.sky(self.camera, Seesaw.ground)
    Scenery.ground(Seesaw.ground)
    Look.drawAll(self.statics)
    Look.draw(self.post)
    Look.draw(self.plank, {layer = 1})
    Look.draw(self.arm, {layer = 1})
    Look.draw(self.stone, {layer = 2})
    Look.drawAll(self.weights, {layer = 2})
    local unit = Look.pixel
    Look.pin(Seesaw.center, 320, 8 * unit, 3)
    Look.pin(Seesaw.pivot[1], Seesaw.pivot[2], 10 * unit, 3)
    self:label('A seesaw', Seesaw.center, 120, {size = 22})
    self:label('A catapult', Seesaw.pivot[1] + 130, 120, {size = 22})
    self.grab:draw()
end

return Seesaw
