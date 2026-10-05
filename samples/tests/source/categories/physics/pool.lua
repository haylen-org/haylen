-- A pool table seen from above in a world without gravity: balls that slow down by linear damping and fall asleep early, a cue aimed with the pointer or a stick with a guide from `world:castCircle`, and pocket sensors that take the balls and count them.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Pool = haylen.class('Pool', PhysicsTest)

Pool.actions = {
    {name = 'turn', type = 'axis', positive = {'key:d', 'key:right', 'button:dpadRight'}, negative = {'key:a', 'key:left', 'button:dpadLeft'}},
    {name = 'aim', type = 'vector', bindings = {'stick:left', 'virtualStick:aim'}},
    {name = 'shoot', type = 'button', bindings = {'key:space', 'button:south', 'virtual:shoot'}},
}

local kCushion, kBall, kCue, kPocket = 1, 2, 4, 8
local kWidth, kHeight = 1320, 660
local kRadius = 18
local kMaxSpeed = 2200
local kChargeTime = 1.2
local kReady = 40
local kColors = {'#FFFFD54F', '#FF42A5F5', '#FFEF5350', '#FFAB47BC', '#FFFF7043', '#FF66BB6A', '#FF8D6E63', '#FF212121'}
local kHead = {-330, 0}

function Pool:enter()
    self:frame{
        hint = 'Drag back from the cue ball and let go to shoot, or turn the cue with A and D, the arrows or the left stick and hold Space or the south button to charge the shot. R or the X button racks the balls again.',
        controls = {
            ui.button{id = 'rack', text = 'Rack the balls', onClick = function() self:build() end},
        },
        play = true,
        actions = Pool.actions,
        overlay = {
            ui.touchStick{action = 'aim', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
            ui.touchButton{action = 'shoot', text = 'Shoot', size = 150, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self:build()
end

function Pool:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    local halfWidth, halfHeight = kWidth / 2, kHeight / 2
    local rails = self.world:createBody({type = 'static'})
    for _, rail in ipairs({{-halfWidth / 2, -halfHeight - 15, halfWidth - 80, 30}, {halfWidth / 2, -halfHeight - 15, halfWidth - 80, 30}, {-halfWidth / 2, halfHeight + 15, halfWidth - 80, 30}, {halfWidth / 2, halfHeight + 15, halfWidth - 80, 30}, {-halfWidth - 15, 0, 30, kHeight - 80}, {halfWidth + 15, 0, 30, kHeight - 80}}) do
        parts.box(rails, rail[3], rail[4], {offsetX = rail[1], offsetY = rail[2], restitution = 0.8, friction = 0.2, category = kCushion})
    end
    rails:addChain({{-halfWidth - 60, -halfHeight - 60}, {-halfWidth - 60, halfHeight + 60}, {halfWidth + 60, halfHeight + 60}, {halfWidth + 60, -halfHeight - 60}}, true, {category = kCushion})
    parts.paint(rails, '#FF2E7D32')
    self.rails = rails

    self.pockets = {}
    for _, spot in ipairs({{-halfWidth - 12, -halfHeight - 12}, {0, -halfHeight - 22}, {halfWidth + 12, -halfHeight - 12}, {-halfWidth - 12, halfHeight + 12}, {0, halfHeight + 22}, {halfWidth + 12, halfHeight + 12}}) do
        local pocket = self.world:createBody({type = 'static', x = spot[1], y = spot[2]})
        pocket:addCircle(30, {sensor = true, category = kPocket})
        self.pockets[#self.pockets + 1] = pocket
    end

    self.balls = {}
    local number = 0
    for row = 0, 4 do
        for column = 0, row do
            number = number + 1
            local ball = self:addBall(300 + row * 31.5, (column - row / 2) * 37, kColors[(number - 1) % 8 + 1], kBall)
            ball.data.stripe = number > 8
        end
    end
    self.cue = self:addBall(kHead[1], kHead[2], '#FFF5F5F5', kCue)
    self.aim, self.power, self.pocketed, self.shots, self.charging = 0, 0, 0, 0, false

    self.world.onSensorBegin = function(sensor, visitor, shapes)
        if not (visitor and visitor.type == 'dynamic') then
            return
        end
        if visitor == self.cue then
            visitor:setTransform(kHead[1], kHead[2], 0)
            visitor.velocity = {0, 0}
        else
            self.pocketed = self.pocketed + 1
            visitor:destroy()
        end
    end
end

function Pool:addBall(x, y, color, category)
    local ball = self.world:createBody({x = x, y = y, linearDamping = 0.9, angularDamping = 1.5, sleepThreshold = 12})
    parts.circle(ball, kRadius, {restitution = 0.95, friction = 0.05, category = category})
    parts.paint(ball, color)
    self.balls[#self.balls + 1] = ball
    return ball
end

function Pool:ready()
    return self.cue.velocity:length() < kReady
end

function Pool:shoot()
    if self:ready() and self.power > 0.03 then
        local speed = self.power * kMaxSpeed * self.cue.mass
        self.cue:applyImpulse(math.cos(self.aim) * speed, math.sin(self.aim) * speed)
        self.shots = self.shots + 1
    end
    self.power, self.charging, self.dragging = 0, false, false
end

function Pool:exit()
    Pool.super.exit(self)
    input.clearVirtual()
end

function Pool:update(dt)
    Pool.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer, cue = self.pointer, self.cue
    if pointer.pressed and not ui.usingPointer() then
        self.dragging = true
    end
    if self.dragging then
        local dx, dy = cue.x - pointer.worldX, cue.y - pointer.worldY
        self.aim = math.atan(dy, dx)
        self.power = math.min(1, math.sqrt(dx * dx + dy * dy) / 300)
        if pointer.released then
            self:shoot()
        end
    end

    local aimX, aimY = input.vector('aim')
    if aimX * aimX + aimY * aimY > 0.36 then
        self.aim = math.atan(aimY, aimX)
    end
    self.aim = self.aim + input.value('turn') * 1.2 * dt
    if input.down('shoot') then
        self.charging = true
        self.power = math.min(1, self.power + dt / kChargeTime)
    elseif self.charging then
        self:shoot()
    end
    self:status(string.format('Pocketed %d of 15, shots %d, aim %.0f degrees, power %.0f percent, awake balls %d, step %.2f ms', self.pocketed, self.shots, math.deg(self.aim) % 360, self.power * 100, self.world.awakeBodyCount, self:stepTime()))
end

function Pool:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Pool:draw(area)
    local halfWidth, halfHeight = kWidth / 2, kHeight / 2
    graphics2d.drawRect({-halfWidth - 60, -halfHeight - 60, kWidth + 120, kHeight + 120}, '#FF5D4037', {layer = -3})
    graphics2d.drawRect({-halfWidth, -halfHeight, kWidth, kHeight}, '#FF1B5E20', {layer = -2})
    for _, pocket in ipairs(self.pockets) do
        graphics2d.drawCircle(pocket.x, pocket.y, 32, '#FF0B0F14', {layer = -1})
    end
    parts.draw(self.rails)
    parts.drawAll(self.balls, {layer = 1})
    for _, ball in ipairs(self.balls) do
        if ball.valid and ball.data.stripe then
            graphics2d.drawRing(ball.x, ball.y, kRadius - 5, 5, '#FFF5F5F5', {layer = 2})
        end
    end
    if not self:ready() then
        return
    end

    -- A circle cast along the aim finds the first ball or cushion the cue ball would touch, where the guide draws its ghost.
    local cue = self.cue
    local dx, dy = math.cos(self.aim), math.sin(self.aim)
    local hit = self.world:castCircle(cue.x, cue.y, kRadius, dx * 1600, dy * 1600, {mask = kCushion | kBall})
    local travel = hit and hit.fraction * 1600 or 1600
    graphics2d.drawLine(cue.x, cue.y, cue.x + dx * travel, cue.y + dy * travel, 2, '#88FFFFFF', {layer = 3})
    graphics2d.drawRing(cue.x + dx * travel, cue.y + dy * travel, kRadius, 2, '#CCFFFFFF', {layer = 3})
    local back = kRadius + 12 + self.power * 120
    graphics2d.drawLine(cue.x - dx * back, cue.y - dy * back, cue.x - dx * (back + 420), cue.y - dy * (back + 420), 10, '#FFD7B98E', {layer = 4})
end

return Pool
