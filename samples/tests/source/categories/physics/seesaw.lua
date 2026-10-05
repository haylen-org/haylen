-- A seesaw on a revolute joint that tips toward the heavier end, and a catapult whose arm throws a stone when a heavy weight falls on its short end.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Seesaw = haylen.class('Seesaw', PhysicsTest)

local kCenter = -380
local kWeights = {
    light = {size = 50, density = 1, color = '#FFB0BEC5'},
    medium = {size = 60, density = 3, color = '#FF78909C'},
    heavy = {size = 70, density = 6, color = '#FF546E7A'},
    catapult = {size = 80, density = 4, color = '#FF455A64'},
}
local kPivot = {320, 300}
local kArmAngle = 0.2
local kStoneSpot = {358, -31}

function Seesaw:enter()
    self:frame{
        hint = 'Drop weights on either end of the seesaw, or drag them where you like. Drop the catapult weight on the short end of its arm to throw the stone. R or the X button starts over.',
        controls = {
            ui.radioGroup{id = 'mass', items = {{id = 'light', text = 'Light weight'}, {id = 'medium', text = 'Medium weight'}, {id = 'heavy', text = 'Heavy weight'}}, selected = 'medium', onChange = function(event) self.mass = event.value end},
            ui.button{id = 'left', text = 'Drop on the left end', onClick = function() self:drop(kCenter - 250, self.mass) end},
            ui.button{id = 'right', text = 'Drop on the right end', onClick = function() self:drop(kCenter + 250, self.mass) end},
            ui.button{id = 'throw', text = 'Drop the catapult weight', onClick = function() self:drop(kPivot[1] - 98, 'catapult', -150) end},
            ui.button{id = 'catapult', text = 'Set the catapult again', onClick = function() self:buildCatapult() end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'left',
    }
    self.mass = 'medium'
    self.random = m.random(41)
    self:build()
end

function Seesaw:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    local room = self.world:createBody({type = 'static'})
    parts.box(room, 1600, 40, {offsetY = 410})
    parts.box(room, 20, 860, {offsetX = -790})
    parts.box(room, 20, 860, {offsetX = 790})
    local fulcrum = self.world:createBody({type = 'static'})
    parts.polygon(fulcrum, {{kCenter - 50, 390}, {kCenter + 50, 390}, {kCenter, 320}})
    self.statics = {room, fulcrum}

    self.plank = self.world:createBody({x = kCenter, y = 311})
    parts.box(self.plank, 620, 18, {friction = 0.8})
    parts.paint(self.plank, '#FFA1887F')
    self.hinge = self.world:createJoint('revolute', fulcrum, self.plank, {ax = kCenter, ay = 320, enableLimit = true, lower = -0.3, upper = 0.3})
    self.weights = {}
    self.post, self.arm, self.stone = nil, nil, nil
    self:buildCatapult()
end

-- The arm rests with its long end on the ground and turns up to its limit, where the stone leaves its cup.
function Seesaw:buildCatapult()
    for _, body in ipairs({self.post, self.arm, self.stone}) do
        body:destroy()
    end
    self.post = self.world:createBody({type = 'static', x = kPivot[1], y = kPivot[2]})
    parts.box(self.post, 30, 90, {offsetY = 45})
    self.arm = self.world:createBody({x = kPivot[1], y = kPivot[2], rotation = kArmAngle})
    parts.box(self.arm, 520, 18, {offsetX = 140, friction = 0.9})
    parts.box(self.arm, 10, 30, {offsetX = 316, offsetY = -24})
    parts.box(self.arm, 10, 30, {offsetX = 400, offsetY = -24})
    parts.paint(self.arm, '#FF8D6E63')
    self.catapult = self.world:createJoint('revolute', self.post, self.arm, {ax = kPivot[1], ay = kPivot[2], enableLimit = true, lower = -0.7, upper = 0})

    local cos, sin = math.cos(kArmAngle), math.sin(kArmAngle)
    local x, y = kPivot[1] + kStoneSpot[1] * cos - kStoneSpot[2] * sin, kPivot[2] + kStoneSpot[1] * sin + kStoneSpot[2] * cos
    self.stone = self.world:createBody({x = x, y = y})
    parts.circle(self.stone, 22, {density = 2})
    parts.paint(self.stone, '#FF90A4AE')
    self.throw = {x = x, y = y, height = 0, distance = 0}
end

function Seesaw:drop(x, kind, y)
    local spec = kWeights[kind]
    local weight = self.world:createBody({x = x + self.random:range(-20, 20), y = y or -300})
    parts.box(weight, spec.size, spec.size, {density = spec.density, friction = 0.8})
    parts.paint(weight, spec.color)
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
    self:status(string.format('Seesaw %.0f degrees, left %.1f kg, right %.1f kg, catapult arm %.0f degrees, stone thrown %.0f units high and %.0f units far, step %.2f ms', math.deg(self.hinge.angle), left, right, math.deg(-self.catapult.angle), throw.height, throw.distance, self:stepTime()))
end

function Seesaw:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Seesaw:draw(area)
    parts.drawAll(self.statics)
    parts.draw(self.post)
    parts.draw(self.plank, {layer = 1})
    parts.draw(self.arm, {layer = 1})
    parts.draw(self.stone, {layer = 2})
    parts.drawAll(self.weights, {layer = 2})
    graphics2d.drawCircle(kCenter, 320, 8, '#FF37474F', {layer = 3})
    graphics2d.drawCircle(kPivot[1], kPivot[2], 10, '#FF37474F', {layer = 3})
    Seesaw.caption('Seesaw', kCenter, -400, {anchor = {0.5, 0}})
    Seesaw.caption('Catapult', 450, -400, {anchor = {0.5, 0}})
    self.grab:draw()
end

return Seesaw
