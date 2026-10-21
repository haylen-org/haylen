-- Pendulums: a cradle of five steel balls on distance joints that passes an impact through the row, a double pendulum that traces its chaotic path and a swing that grows with pushes timed to its motion, each on a frame that stands on the ground.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Pendulums = haylen.class('Pendulums', PhysicsTest)

Pendulums.code = [[
local world = physics2d.newWorld({restitutionThreshold = 20})
ball:addCircle(30, {density = 8, friction = 0, restitution = 1})
world:createJoint('distance', bar, ball, {ax = x, ay = -300, bx = x, by = 60})  -- The string of a cradle ball.
world:createJoint('revolute', arm, tip, {ax = elbowX, ay = elbowY})  -- The elbow of the double pendulum.
seat:applyImpulse(seat.mass * 140, 0)  -- A push along the swing.]]

Pendulums.actions = {
    {name = 'push', type = 'button', bindings = {'key:p', 'button:north'}},
}

Pendulums.ground = 360
Pendulums.balls = 5
Pendulums.ballRadius = 30
Pendulums.ballSpacing = 62
Pendulums.cradleX, Pendulums.cradleTop, Pendulums.stringLength = 40, -290, 360
Pendulums.liftAngle = 0.7
Pendulums.pivot = {500, -40}
Pendulums.armLength = 150
Pendulums.trailLength = 150
Pendulums.swingPivot = {-580, -290}
Pendulums.swingLength = 380
Pendulums.pushSpeed = 140
Pendulums.string = '#FFF2F3FF'
Pendulums.captionY = -335

function Pendulums:enter()
    self:frame{
        code = Pendulums.code,
        hint = 'Lift balls of the cradle and let them go, or drag any ball, arm or the seat. P or the north button pushes the swing along its motion. R or the X button starts over.',
        controls = {
            ui.button{id = 'one', text = 'Lift one ball', onClick = function() self:lift(1) end},
            ui.button{id = 'two', text = 'Lift two balls', onClick = function() self:lift(2) end},
            ui.button{id = 'push', text = 'Push the swing', onClick = function() self:push() end},
            ui.button{id = 'kick', text = 'Kick the double pendulum', onClick = function() self.tip:applyImpulse(self.tip.mass * 900, 0) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        actions = Pendulums.actions,
        focus = 'one',
    }
    self.trail = {}
    for index = 1, Pendulums.trailLength do
        self.trail[index] = {0, 0}
    end
    self.trailPoints, self.trailColors, self.trailShown = {}, {}, {}
    for index = 1, Pendulums.trailLength do
        self.trailColors[index] = {1, 0.79, 0.24, (index / Pendulums.trailLength) ^ 2}
    end
    self:build()
end

function Pendulums:build()
    -- A low restitution threshold keeps the cradle bouncing at the small speeds of its last swings.
    self.world = physics2d.newWorld({restitutionThreshold = 20})
    self.grab = Grab(self.world)
    local bar = self.world:createBody({type = 'static', x = Pendulums.cradleX, y = Pendulums.cradleTop})
    bar:addBox(Pendulums.balls * Pendulums.ballSpacing + 60, 22)
    local frame = self.world:createBody({type = 'static', x = Pendulums.swingPivot[1], y = Pendulums.swingPivot[2]})

    self.balls = {}
    for index = 1, Pendulums.balls do
        local x = Pendulums.cradleX + (index - (Pendulums.balls + 1) / 2) * Pendulums.ballSpacing
        local ball = self.world:createBody({x = x, y = Pendulums.cradleTop + Pendulums.stringLength})
        ball:addCircle(Pendulums.ballRadius, {density = 8, friction = 0, restitution = 1})
        Look.dress(ball, 'steel')
        self.world:createJoint('distance', bar, ball, {ax = x, ay = Pendulums.cradleTop, bx = x, by = Pendulums.cradleTop + Pendulums.stringLength})
        self.balls[index] = {body = ball, x = x}
    end

    local pivotX, pivotY = Pendulums.pivot[1], Pendulums.pivot[2]
    local pivot = self.world:createBody({type = 'static', x = pivotX, y = pivotY})
    local arm = Pendulums.armLength
    self.arm = self.world:createBody({x = pivotX, y = pivotY, rotation = -2.2})
    self.arm:addBox(arm, 16, {offsetX = arm / 2})
    Look.dress(self.arm, 'wood')
    local elbowX, elbowY = pivotX + math.cos(-2.2) * arm, pivotY + math.sin(-2.2) * arm
    self.tip = self.world:createBody({x = elbowX, y = elbowY, rotation = -0.6})
    self.tip:addBox(arm, 16, {offsetX = arm / 2})
    self.tip:addCircle(20, {offsetX = arm, density = 2})
    Look.dress(self.tip, 'wood')
    self.world:createJoint('revolute', pivot, self.arm, {ax = pivotX, ay = pivotY})
    self.world:createJoint('revolute', self.arm, self.tip, {ax = elbowX, ay = elbowY})
    self.trailCount, self.trailNext = 0, 1

    -- Two ropes from one pivot to the ends of the seat make a rigid triangle, so the seat stays level with its ropes.
    self.seat = self.world:createBody({x = Pendulums.swingPivot[1], y = Pendulums.swingPivot[2] + Pendulums.swingLength})
    self.seat:addBox(150, 18)
    Look.dress(self.seat, 'red')
    for _, side in ipairs({-60, 60}) do
        self.world:createJoint('distance', frame, self.seat, {ax = Pendulums.swingPivot[1], ay = Pendulums.swingPivot[2], bx = Pendulums.swingPivot[1] + side, by = Pendulums.swingPivot[2] + Pendulums.swingLength})
    end
    self:lift(1)
    self:push()
end

-- Places the first balls of the row on their arcs at the lift angle and stills the whole row.
function Pendulums:lift(count)
    for index, ball in ipairs(self.balls) do
        local angle = index <= count and Pendulums.liftAngle or 0
        ball.body:setTransform(ball.x - math.sin(angle) * Pendulums.stringLength, Pendulums.cradleTop + math.cos(angle) * Pendulums.stringLength, 0)
        ball.body.velocity = {0, 0}
        ball.body.angularVelocity = 0
    end
end

-- Pushes the seat along the way it moves, or to the right when it hangs still.
function Pendulums:push()
    local velocity = self.seat.velocity
    local direction = velocity.x < 0 and -1 or 1
    self.seat:applyImpulse(direction * self.seat.mass * Pendulums.pushSpeed, 0)
end

function Pendulums:swingAngle()
    return math.deg(math.atan(self.seat.x - Pendulums.swingPivot[1], self.seat.y - Pendulums.swingPivot[2]))
end

function Pendulums:update(dt)
    Pendulums.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if input.pressed('push') then
        self:push()
    end
    self.grab:update(self.pointer, self.camera)
    local fastest = 0
    for _, ball in ipairs(self.balls) do
        fastest = math.max(fastest, ball.body.velocity:length())
    end
    self:status(string.format('The fastest ball of the cradle moves at %d per second, and the swing reaches %d degrees.', math.floor(fastest + 0.5), math.abs(math.floor(self:swingAngle() + 0.5))))
end

-- The tip of the double pendulum adds its place to a ring of the last places every step, which the trail draws from the oldest.
function Pendulums:fixedUpdate(step)
    self:simulate(self.world, step)
    local rotation = self.tip.rotation
    local slot = self.trail[self.trailNext]
    slot[1], slot[2] = self.tip.x + math.cos(rotation) * Pendulums.armLength, self.tip.y + math.sin(rotation) * Pendulums.armLength
    self.trailNext = self.trailNext % Pendulums.trailLength + 1
    self.trailCount = math.min(self.trailCount + 1, Pendulums.trailLength)
end

function Pendulums:drawTrail()
    local count = self.trailCount
    if count < 2 then
        return
    end
    local points, colors = self.trailPoints, self.trailColors
    for index = 1, count do
        local slot = self.trail[(self.trailNext - count + index - 2) % Pendulums.trailLength + 1]
        points[index * 2 - 1], points[index * 2] = slot[1], slot[2]
    end
    for extra = #points, count * 2 + 1, -1 do
        points[extra] = nil
    end
    local offset, shown = Pendulums.trailLength - count, self.trailShown
    for index = 1, Pendulums.trailLength do
        shown[index] = index <= count and colors[index + offset] or nil
    end
    graphics2d.drawStroke(points, {width = 6 * Look.pixel, colors = shown, join = 'round', cap = 'round', layer = 1})
end

-- The frames: a wooden cradle on two posts, a post that holds the double pendulum and an A-frame for the swing.
function Pendulums:drawFrames()
    local ground = Pendulums.ground
    local cradleWidth = Pendulums.balls * Pendulums.ballSpacing + 60
    local left, right = Pendulums.cradleX - cradleWidth / 2, Pendulums.cradleX + cradleWidth / 2
    Look.block(Pendulums.cradleX, Pendulums.cradleTop, cradleWidth + 30, 22, 'darkWood', 0)
    for _, x in ipairs({left, right}) do
        Look.block(x, (Pendulums.cradleTop + ground) / 2, 20, ground - Pendulums.cradleTop, 'darkWood', -1)
    end
    Look.block(Pendulums.cradleX, ground - 10, cradleWidth + 80, 20, 'darkWood', -1)

    local pivotX, pivotY = Pendulums.pivot[1], Pendulums.pivot[2]
    Look.block(pivotX, (pivotY + ground) / 2, 22, ground - pivotY, 'stone', -1)

    local sx, sy = Pendulums.swingPivot[1], Pendulums.swingPivot[2]
    for _, side in ipairs({-1, 1}) do
        local footX = sx + side * 150
        local length = math.sqrt((footX - sx) ^ 2 + (ground - sy) ^ 2)
        Look.block((sx + footX) / 2, (sy + ground) / 2, 20, length, 'darkWood', -1, math.atan(-(footX - sx), ground - sy))
    end
    Look.block(sx, sy, 60, 20, 'darkWood', 0)
end

-- Every ball of the cradle hangs on two strings that meet it, so it swings in the plane of the frame.
function Pendulums:drawStrings()
    local unit = Look.pixel
    for _, ball in ipairs(self.balls) do
        for _, side in ipairs({-18, 18}) do
            Look.line(ball.x + side, Pendulums.cradleTop + 10, ball.body.x, ball.body.y, 2.5 * unit, Pendulums.string, 0)
        end
    end
    local cos, sin = math.cos(self.seat.rotation), math.sin(self.seat.rotation)
    for _, side in ipairs({-60, 60}) do
        Look.line(Pendulums.swingPivot[1], Pendulums.swingPivot[2], self.seat.x + side * cos, self.seat.y + side * sin, 4 * unit, '#FFE8C78E', 0)
    end
end

function Pendulums:draw(area)
    Scenery.sky(self.camera, Pendulums.ground)
    Scenery.ground(Pendulums.ground)
    self:drawFrames()
    self:drawStrings()
    self:drawTrail()
    for _, ball in ipairs(self.balls) do
        Look.draw(ball.body, {layer = 2})
    end
    Look.drawAll({self.arm, self.tip, self.seat}, {layer = 2})
    local unit = Look.pixel
    Look.pin(Pendulums.pivot[1], Pendulums.pivot[2], 9 * unit, 3)
    Look.pin(Pendulums.swingPivot[1], Pendulums.swingPivot[2], 8 * unit, 3)
    local elbow = self.arm
    Look.pin(elbow.x + math.cos(elbow.rotation) * Pendulums.armLength, elbow.y + math.sin(elbow.rotation) * Pendulums.armLength, 7 * unit, 3)
    self:label('A swing', Pendulums.swingPivot[1], Pendulums.captionY, {size = 20})
    self:label('A cradle', Pendulums.cradleX, Pendulums.captionY, {size = 20})
    self:label('A double pendulum', Pendulums.pivot[1], Pendulums.captionY, {size = 20})
    self.grab:draw()
end

return Pendulums
