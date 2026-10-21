-- A crane with a cab, a counterweight and a lattice boom that turns on a revolute motor and swings a heavy iron ball on a chain from `physics2d.newRope` into a wall of bricks, which breaks into a cloud of dust.
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local WreckingBall = haylen.class('WreckingBall', PhysicsTest)

WreckingBall.code = [[
local boom = world:createJoint('revolute', mast, arm, {ax = -560, ay = 10, enableLimit = true, upper = 1.4, enableMotor = true, maxMotorTorque = 5e7})
boom.motorSpeed = swing * 1.0
physics2d.newRope(world, {from = tip, to = hook, segments = 12, thickness = 10, startBody = arm, endBody = ball, group = -1})
ball:addCircle(48, {density = 12, group = -1})  -- Shares the group of the boom and the chain, so they never collide.]]

WreckingBall.actions = {
    {name = 'swing', type = 'axis', positive = {'key:d', 'key:right', 'button:dpadRight', 'axis:leftX+', 'virtual:right'}, negative = {'key:a', 'key:left', 'button:dpadLeft', 'axis:leftX-', 'virtual:left'}},
}

WreckingBall.ground = 390
WreckingBall.pivot = {-560, 10}
WreckingBall.boomLength = 360
WreckingBall.restAngle = -2.2
WreckingBall.sweep = 1.4
WreckingBall.chainLength = 300
WreckingBall.ballRadius = 48
WreckingBall.brick = {64, 32}
WreckingBall.rows = 10
WreckingBall.crane = -1

function WreckingBall:enter()
    self:frame{
        code = WreckingBall.code,
        hint = 'Turn the boom with A and D, the arrows, the left stick or the touch buttons, or let it swing by itself. R or the X button builds the wall again.',
        controls = {
            ui.toggle{id = 'auto', text = 'Swing by itself', checked = true, onChange = function(event) self.auto = event.checked end},
            ui.label{text = 'Speed of the boom', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 1, min = 0.4, max = 2, step = 0.1, showValue = true, decimals = 1, onChange = function(event) self.speed = event.value end},
            ui.button{id = 'reset', text = 'Build the wall again', onClick = function() self:build() end},
        },
        view = {1760, 840},
        play = true,
        pointer = false,
        actions = WreckingBall.actions,
        overlay = {
            ui.touchButton{action = 'left', text = 'Left', size = 140, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}},
            ui.touchButton{action = 'right', text = 'Right', size = 140, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.camera.position = {0, 0}
    self.auto, self.speed, self.direction = true, 1, 1
    self.dust = Burst('environment/landing-dust', {count = 6, scale = 0.8, layer = 30})
    self:build()
end

function WreckingBall:build()
    self.world = physics2d.newWorld()
    local ground = WreckingBall.ground
    local floor = self.world:createBody({type = 'static', x = 0, y = ground + 30})
    floor:addBox(8000, 60)
    local pivot = WreckingBall.pivot
    local mast = self.world:createBody({type = 'static', x = pivot[1], y = pivot[2]})
    mast:addBox(40, ground - pivot[2], {offsetY = (ground - pivot[2]) / 2})

    -- The boom, its chain and the ball share a negative group, so they never collide with each other.
    self.boom = self.world:createBody({x = pivot[1], y = pivot[2], rotation = WreckingBall.restAngle})
    self.boom:addBox(WreckingBall.boomLength, 26, {offsetX = WreckingBall.boomLength / 2, group = WreckingBall.crane})
    self.motor = self.world:createJoint('revolute', mast, self.boom, {ax = pivot[1], ay = pivot[2], enableLimit = true, lower = 0, upper = WreckingBall.sweep, enableMotor = true, maxMotorTorque = 5e7})

    local tipX = pivot[1] + math.cos(WreckingBall.restAngle) * WreckingBall.boomLength
    local tipY = pivot[2] + math.sin(WreckingBall.restAngle) * WreckingBall.boomLength
    self.ball = self.world:createBody({x = tipX, y = tipY + WreckingBall.chainLength + WreckingBall.ballRadius})
    self.ball:addCircle(WreckingBall.ballRadius, {density = 12, group = WreckingBall.crane})
    Look.dress(self.ball, 'iron')
    self.chain = physics2d.newRope(self.world, {from = {tipX, tipY}, to = {tipX, tipY + WreckingBall.chainLength}, segments = 12, thickness = 10, density = 2, startBody = self.boom, endBody = self.ball, group = WreckingBall.crane})

    self.bricks = {}
    local width, height = WreckingBall.brick[1], WreckingBall.brick[2]
    for row = 0, WreckingBall.rows - 1 do
        local y = ground - height / 2 - row * height
        local offset = row % 2 == 0 and 0 or width / 2
        for column = 0, 4 - (row % 2) do
            local x = 0 + column * width + offset
            local body = self.world:createBody({x = x, y = y})
            body:addBox(width - 2, height - 2, {friction = 0.7})
            Look.dress(body, 'brick', {radius = 2})
            self.bricks[#self.bricks + 1] = {body = body, x = x, y = y}
        end
    end
    self.world.onHit = function(a, b, contact)
        if (a == self.ball or b == self.ball) and contact.speed > 250 then
            self.dust:fire(contact.x, contact.y, math.min(1.4, contact.speed / 600))
        end
    end
end

function WreckingBall:bricksDown()
    local count = 0
    for _, brick in ipairs(self.bricks) do
        local body = brick.body
        if math.abs(body.x - brick.x) > 20 or math.abs(body.y - brick.y) > 20 or math.abs(body.rotation) > 0.3 then
            count = count + 1
        end
    end
    return count
end

function WreckingBall:exit()
    WreckingBall.super.exit(self)
    input.clearVirtual()
end

function WreckingBall:update(dt)
    WreckingBall.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.dust:update(dt)
    self:status(string.format('The boom is raised %d degrees, the ball flies at %d per second, and %d of %d bricks are down.', PhysicsTest.degrees(self.motor.angle), math.floor(self.ball.velocity:length() + 0.5), self:bricksDown(), #self.bricks))
end

-- Input turns the boom, and without input the boom swings by itself from one limit to the other.
function WreckingBall:fixedUpdate(step)
    local swing = input.value('swing')
    local angle = self.motor.angle
    if math.abs(swing) > 0.1 then
        self.motor.motorSpeed = swing * self.speed
    elseif self.auto then
        if angle >= WreckingBall.sweep - 0.02 then
            self.direction = -1
        elseif angle <= 0.02 then
            self.direction = 1
        end
        self.motor.motorSpeed = self.direction * self.speed
    else
        self.motor.motorSpeed = 0
    end
    self:simulate(self.world, step)
end

-- The crane: a cab with a window and a counterweight on crawler tracks, a steel mast, and a lattice boom of two chords with braces between them.
function WreckingBall:drawCrane()
    local pivot, ground = WreckingBall.pivot, WreckingBall.ground
    local unit = Look.pixel
    local x = pivot[1]
    Look.block(x - 20, ground - 24, 260, 44, 'tire', 1)
    for wheel = 0, 4 do
        Look.disc(x - 130 + wheel * 55, ground - 24, 14, 'steel', 2)
    end
    Look.block(x - 10, ground - 96, 180, 100, 'yellow', 1)
    Look.block(x + 40, ground - 104, 70, 50, Look.tint('glass', '#CC9BE7FF', '#CC6FB8D8', '#FFFFFFFF'), 2)
    Look.block(x - 105, ground - 90, 70, 80, 'iron', 2)
    Look.block(x, (pivot[2] + ground - 140) / 2, 36, ground - 140 - pivot[2], 'steel', 0)

    local boom = self.boom
    local cos, sin = math.cos(boom.rotation), math.sin(boom.rotation)
    local nx, ny = -sin * 12, cos * 12
    local length = WreckingBall.boomLength
    for _, side in ipairs({-1, 1}) do
        Look.line(boom.x + nx * side, boom.y + ny * side, boom.x + cos * length + nx * side, boom.y + sin * length + ny * side, 6 * unit, '#FFFFC93C', 3)
    end
    for index = 0, 8 do
        local a, b = index / 9 * length, (index + 1) / 9 * length
        local side = index % 2 == 0 and 1 or -1
        Look.line(boom.x + cos * a + nx * side, boom.y + sin * a + ny * side, boom.x + cos * b - nx * side, boom.y + sin * b - ny * side, 4 * unit, '#FFE09A1E', 3)
    end
    Look.pin(pivot[1], pivot[2], 12 * unit, 4)
end

function WreckingBall:drawChain()
    local unit = Look.pixel
    for index, segment in ipairs(self.chain:segments()) do
        local cos, sin = math.cos(segment.rotation), math.sin(segment.rotation)
        local hx, hy = cos * segment.length / 2, sin * segment.length / 2
        Look.line(segment.x - hx, segment.y - hy, segment.x + hx, segment.y + hy, (index % 2 == 0 and 10 or 5) * unit, '#FF8A96AE', 2)
    end
end

function WreckingBall:draw(area)
    Scenery.sky(self.camera, WreckingBall.ground)
    Scenery.ground(WreckingBall.ground)
    for _, brick in ipairs(self.bricks) do
        Look.draw(brick.body, {layer = 1})
    end
    self:drawCrane()
    self:drawChain()
    Look.draw(self.ball, {layer = 3})
    self.dust:draw()
end

return WreckingBall
