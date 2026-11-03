-- A crane with a cab, a counterweight and a lattice boom that turns on a revolute motor and swings a heavy iron ball on a chain from `physics2d.newRope` into a wall of bricks, which breaks into a cloud of dust.
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local WreckingBall = haylen.class('WreckingBall', PhysicsTest)

WreckingBall.code = [[
local options = {ax = -560, ay = 10, enableLimit = true, upper = 0.61, enableMotor = true}
local boom = world:createJoint('revolute', mast, arm, options)
boom.maxMotorTorque, boom.motorSpeed = 5e7, (target - boom.angle) * 10
local rope = {from = tip, to = hook, segments = 12, endBody = ball, group = -1}
physics2d.newRope(world, rope)
ball:addCircle(48, {density = 12, group = -1})  -- The crane never hits its own ball.]]

WreckingBall.actions = {
    {name = 'swing', type = 'axis', positive = {'key:d', 'key:right', 'button:dpadRight', 'axis:leftX+', 'virtual:right'}, negative = {'key:a', 'key:left', 'button:dpadLeft', 'axis:leftX-', 'virtual:left'}},
}

WreckingBall.ground = 390
WreckingBall.pivot = {-560, 10}
WreckingBall.boomLength = 360
-- The boom swings between 55 and 20 degrees above the ground on the side of the wall, so the ball always hangs clear of the boom and the mast.
WreckingBall.restAngle = -0.96
WreckingBall.sweep = 0.61
-- By itself the boom eases down to the wall, holds while the ball swings into it, eases back up and holds again, in a cycle of this many seconds at the speed 1, and its motor follows that motion with this gain.
WreckingBall.cycle = 4.5
WreckingBall.gain = 10
WreckingBall.wallX = -28
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
    self.camera.position = {-120, 0}
    self.auto, self.speed, self.clock = true, 1, 0
    self.dust = Burst('environment/landing-dust', {count = 6, scale = 0.8, layer = 30})
    self:build()
end

function WreckingBall:build()
    if self.world then
        self.world:destroy()
    end
    self.world = physics2d.newWorld({owner = self})
    local ground = WreckingBall.ground
    local floor = self.world:createBody({type = 'static', x = 0, y = ground + 30})
    floor:addBox(8000, 60)
    local pivot = WreckingBall.pivot
    local mast = self.world:createBody({type = 'static', x = pivot[1], y = pivot[2]})
    mast:addBox(40, ground - pivot[2], {offsetY = (ground - pivot[2]) / 2, group = WreckingBall.crane})

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
            local x = WreckingBall.wallX + column * width + offset
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
    self:status(string.format('The boom points %d degrees up, the ball flies at %d per second, and %d of %d bricks are down.', PhysicsTest.degrees(-self.boom.rotation), math.floor(self.ball.velocity:length() + 0.5), self:bricksDown(), #self.bricks))
end

-- Input turns the boom, and without input the boom follows its cycle by itself, eased at both ends so the chain stays taut and the ball swings into the wall instead of whipping.
function WreckingBall:fixedUpdate(step)
    local swing = input.value('swing')
    if math.abs(swing) > 0.1 then
        self.motor.motorSpeed = swing * self.speed
    elseif self.auto then
        self.clock = self.clock + step * self.speed
        local target = WreckingBall.sweep * WreckingBall.profile(self.clock / WreckingBall.cycle % 1)
        local limit = self.speed * 1.5
        self.motor.motorSpeed = m.clamp((target - self.motor.angle) * WreckingBall.gain, -limit, limit)
    else
        self.motor.motorSpeed = 0
    end
    self:simulate(self.world, step)
end

-- Returns how far down the boom is, from 0 to 1, at a share of its cycle: easing down, holding at the wall, easing back up and holding at the top.
function WreckingBall.profile(share)
    if share < 0.3 then
        return m.smoothstep(0, 1, share / 0.3)
    elseif share < 0.55 then
        return 1
    elseif share < 0.85 then
        return 1 - m.smoothstep(0, 1, (share - 0.55) / 0.3)
    end
    return 0
end

-- The crane: a cab with a window and a counterweight on crawler tracks, a lattice mast and a lattice boom of two chords with braces between them.
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
    self:drawLattice(x, ground - 146, x, pivot[2], 24, 12, 6, 0)
    Look.block(x, ground - 146, 64, 14, 'iron', 1)

    local boom = self.boom
    local length = WreckingBall.boomLength
    self:drawLattice(boom.x, boom.y, boom.x + math.cos(boom.rotation) * length, boom.y + math.sin(boom.rotation) * length, 14, 8, 9, 3)
    Look.pin(pivot[1], pivot[2], 12 * unit, 4)
end

-- Draws a lattice from `ax`, `ay` to `bx`, `by`: two chords this far from its middle at each end, which taper toward the end, with braces that zigzag between them in `bays` bays.
function WreckingBall:drawLattice(ax, ay, bx, by, baseHalf, tipHalf, bays, layer)
    local unit = Look.pixel
    local dx, dy = bx - ax, by - ay
    local length = math.sqrt(dx * dx + dy * dy)
    local nx, ny = -dy / length, dx / length
    for _, side in ipairs({-1, 1}) do
        Look.line(ax + nx * baseHalf * side, ay + ny * baseHalf * side, bx + nx * tipHalf * side, by + ny * tipHalf * side, 6 * unit, '#FFFFC93C', layer)
    end
    for index = 0, bays - 1 do
        local from, to = index / bays, (index + 1) / bays
        local side = index % 2 == 0 and 1 or -1
        local halfFrom, halfTo = baseHalf + (tipHalf - baseHalf) * from, baseHalf + (tipHalf - baseHalf) * to
        Look.line(ax + dx * from + nx * halfFrom * side, ay + dy * from + ny * halfFrom * side, ax + dx * to - nx * halfTo * side, ay + dy * to - ny * halfTo * side, 4 * unit, '#FFE09A1E', layer)
    end
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
