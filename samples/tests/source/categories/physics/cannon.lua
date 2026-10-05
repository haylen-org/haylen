-- A cannon aimed by angle and power that fires bodies with `bullet = true` at a tower, while every shot kicks its carriage back against a recoil spring and the hits on the tower are counted.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Cannon = haylen.class('Cannon', PhysicsTest)

Cannon.actions = {
    {name = 'adjust', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:adjust'}},
    {name = 'fire', type = 'button', bindings = {'key:space', 'button:south', 'virtual:fire'}},
}

local kTrunnion = {20, -30}
local kBarrel = 130
local kBulletRadius = 14
local kCooldown = 0.4
local kMaxBullets = 12
local kFlashLife = 0.15

function Cannon:enter()
    self:frame{
        hint = 'Raise and lower the barrel with W and S, the arrows or the left stick, change the power with A and D, and hold Space or the south button to fire. R or the X button builds the tower again.',
        controls = {
            ui.label{text = 'Angle in degrees', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'angle', value = 20, min = 5, max = 75, step = 1, showValue = true, decimals = 0, onChange = function(event) self.angle = event.value end},
            ui.label{text = 'Power in units per second', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'power', value = 1300, min = 800, max = 2200, step = 50, showValue = true, decimals = 0, onChange = function(event) self.power = event.value end},
            ui.button{id = 'fire', text = 'Fire', onClick = function() self:fire() end},
            ui.button{id = 'reset', text = 'Build the tower again', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = Cannon.actions,
        overlay = {
            ui.touchStick{action = 'adjust', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
            ui.touchButton{action = 'fire', text = 'Fire', size = 150, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self.angle, self.power = 20, 1300
    self:build()
end

function Cannon:build()
    self.world = physics2d.newWorld()
    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40)
    local stake = self.world:createBody({type = 'static', x = -760, y = 360})
    parts.box(stake, 16, 60)
    self.statics = {ground, stake}

    self.carriage = self.world:createBody({x = -560, y = 336})
    parts.box(self.carriage, 150, 36, {density = 6})
    parts.paint(self.carriage, '#FF8D6E63')
    self.wheels = {}
    for _, x in ipairs({-610, -510}) do
        local wheel = self.world:createBody({x = x, y = 360})
        parts.circle(wheel, 30, {density = 2, friction = 0.9, rollingResistance = 0.05})
        parts.paint(wheel, '#FF5D4037')
        self.world:createJoint('revolute', self.carriage, wheel, {ax = x, ay = 360})
        self.wheels[#self.wheels + 1] = wheel
    end
    self.world:createJoint('distance', stake, self.carriage, {ax = -760, ay = 350, bx = -635, by = 336, enableSpring = true, hertz = 1.2, dampingRatio = 0.5})

    self.blocks, self.bullets = {}, {}
    for row = 0, 6 do
        for column = 0, 2 do
            local block = self.world:createBody({x = 420 + column * 52, y = 365 - row * 50})
            parts.box(block, 50, 50, {density = 0.6})
            block.data.block = true
            self.blocks[#self.blocks + 1] = {body = block, x = block.x, y = block.y}
        end
    end
    self.hits, self.shots, self.cooldown, self.flash = 0, 0, 0, 0

    self.world.onHit = function(a, b, contact)
        for _, pair in ipairs({{a, b}, {b, a}}) do
            local bullet, other = pair[1], pair[2]
            if bullet.data.bullet and not bullet.data.scored and other.data.block then
                bullet.data.scored = true
                self.hits = self.hits + 1
            end
        end
    end
end

-- Returns the trunnion of the barrel and the direction it points, which turn with the carriage.
function Cannon:barrel()
    local carriage = self.carriage
    local cos, sin = math.cos(carriage.rotation), math.sin(carriage.rotation)
    local x = carriage.x + kTrunnion[1] * cos - kTrunnion[2] * sin
    local y = carriage.y + kTrunnion[1] * sin + kTrunnion[2] * cos
    local direction = carriage.rotation - math.rad(self.angle)
    return x, y, math.cos(direction), math.sin(direction)
end

-- Launches a bullet from the muzzle and pushes the carriage back at the trunnion with the momentum the bullet took.
function Cannon:fire()
    if self.cooldown > 0 then
        return
    end
    local x, y, dx, dy = self:barrel()
    local bullet = self.world:createBody({x = x + dx * kBarrel, y = y + dy * kBarrel, bullet = true})
    parts.circle(bullet, kBulletRadius, {density = 10, restitution = 0.1})
    parts.paint(bullet, '#FF37474F')
    bullet.data.bullet = true
    local velocity = self.carriage.velocity
    bullet.velocity = {velocity.x + dx * self.power, velocity.y + dy * self.power}
    self.carriage:applyImpulse(-dx * bullet.mass * self.power, -dy * bullet.mass * self.power, x, y)
    self.bullets[#self.bullets + 1] = bullet
    if #self.bullets > kMaxBullets then
        table.remove(self.bullets, 1):destroy()
    end
    self.shots, self.cooldown, self.flash = self.shots + 1, kCooldown, kFlashLife
end

function Cannon:knocked()
    local count = 0
    for _, block in ipairs(self.blocks) do
        if math.abs(block.body.x - block.x) > 25 or math.abs(block.body.y - block.y) > 25 then
            count = count + 1
        end
    end
    return count
end

function Cannon:exit()
    Cannon.super.exit(self)
    input.clearVirtual()
end

function Cannon:update(dt)
    Cannon.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local power, raise = input.vector('adjust')
    if power ~= 0 or raise ~= 0 then
        self.angle = m.clamp(self.angle - raise * 40 * dt, 5, 75)
        self.power = m.clamp(self.power + power * 600 * dt, 800, 2200)
        self:set('angle', {value = self.angle})
        self:set('power', {value = self.power})
    end
    if input.down('fire') then
        self:fire()
    end
    self.flash = math.max(0, self.flash - dt)
    self:status(string.format('Angle %.0f degrees, power %.0f, hits %d of %d shots, blocks knocked %d of %d, carriage speed %.0f, step %.2f ms', self.angle, self.power, self.hits, self.shots, self:knocked(), #self.blocks, self.carriage.velocity.x, self:stepTime()))
end

function Cannon:fixedUpdate(step)
    self.cooldown = math.max(0, self.cooldown - step)
    self:simulate(self.world, step)
end

function Cannon:draw(area)
    parts.drawAll(self.statics)
    for _, block in ipairs(self.blocks) do
        parts.draw(block.body)
    end
    parts.drawAll(self.bullets, {layer = 1})
    local x, y, dx, dy = self:barrel()
    graphics2d.drawLine(-760, 350, self.carriage.x - 75, self.carriage.y, 3, '#FFB0BEC5')
    graphics2d.drawLine(x - dx * 24, y - dy * 24, x + dx * kBarrel, y + dy * kBarrel, 34, '#FF455A64', {layer = 2})
    graphics2d.drawRing(x + dx * kBarrel, y + dy * kBarrel, 17, 6, '#FF263238', {layer = 2})
    parts.draw(self.carriage, {layer = 3})
    parts.drawAll(self.wheels, {layer = 4})
    graphics2d.drawCircle(x, y, 12, '#FF263238', {layer = 4})
    if self.flash > 0 then
        local fade = self.flash / kFlashLife
        graphics2d.drawCircle(x + dx * (kBarrel + 30), y + dy * (kBarrel + 30), 30 * fade + 10, m.color(1, 0.8, 0.3, fade), {layer = 5, blend = 'additive'})
    end
end

return Cannon
