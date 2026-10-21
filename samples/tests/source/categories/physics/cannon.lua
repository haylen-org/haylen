-- A bronze cannon on a wooden carriage aimed by angle and power that fires iron balls with `bullet = true` at a tower of stone and crates, while every shot kicks the carriage back against a recoil spring and the hits on the tower are counted. The cannon fires once by itself when the test opens.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Cannon = haylen.class('Cannon', PhysicsTest)

Cannon.code = [[
local shot = world:createBody({x = muzzleX, y = muzzleY, bullet = true})
shot:addCircle(14, {density = 10, restitution = 0.1})
shot.velocity = {dx * power, dy * power}
carriage:applyImpulse(-dx * shot.mass * power, -dy * shot.mass * power, trunnionX, trunnionY)  -- The recoil.
world:createJoint('distance', stake, carriage, {ax = -760, ay = 340, bx = -650, by = 326, enableSpring = true, hertz = 1.2})]]

Cannon.actions = {
    {name = 'adjust', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:adjust'}},
    {name = 'fire', type = 'button', bindings = {'key:space', 'button:south', 'virtual:fire'}},
}

Cannon.ground = 390
Cannon.trunnion = {14, -36}
Cannon.barrelLength = 110
Cannon.bulletRadius = 14
Cannon.cooldown = 0.4
Cannon.maxBullets = 12
-- The barrel picture turns around its trunnion, this share of its width from its back, and reaches the muzzle this many pixels further.
Cannon.picture = {width = 320, height = 166, pivot = 0.425, muzzle = 180}
Cannon.wheel = Look.tint('darkWood', '#FFA86B3C', '#FF7A4724', '#FFD39A62')
Cannon.wheel.hub, Cannon.wheel.hubShade = '#FFC9D3E2', '#FF5A6E8A'

function Cannon:enter()
    self:frame{
        code = Cannon.code,
        hint = 'Raise and lower the barrel with W and S, the arrows or the left stick, change the power with A and D, and hold Space or the south button to fire. R or the X button builds the tower again.',
        controls = {
            ui.label{text = 'Angle of the barrel, in degrees', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'angle', value = 20, min = 5, max = 75, step = 1, showValue = true, decimals = 0, onChange = function(event) self.angle = event.value end},
            ui.label{text = 'Power of the shot', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'power', value = 1300, min = 800, max = 2200, step = 50, showValue = true, decimals = 0, onChange = function(event) self.power = event.value end},
            ui.button{id = 'fire', text = 'Fire', onClick = function() self:fire() end},
            ui.button{id = 'reset', text = 'Build the tower again', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = Cannon.actions,
        view = {1600, 680},
        overlay = {
            ui.touchStick{action = 'adjust', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
            ui.touchButton{action = 'fire', text = 'Fire', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.camera.position = {0, 80}
    self.angle, self.power = 20, 1300
    self.barrelTexture = assets.texture('physics/cannon.png', {filter = 'linear'})
    self.flash = Burst('combat/muzzle-flash', {count = 3, scale = 0.7, layer = 30})
    self.impacts = Burst('combat/bullet-impact', {count = 6, scale = 0.6, layer = 30})
    self:build()
    timer.after(0.8, function() self:fire() end, {owner = self})
end

function Cannon:build()
    self.world = physics2d.newWorld()
    local ground = self.world:createBody({type = 'static', x = 0, y = Cannon.ground + 30})
    ground:addBox(8000, 60)
    local stake = self.world:createBody({type = 'static', x = -760, y = Cannon.ground - 30})
    stake:addBox(18, 60)
    Look.dress(stake, 'darkWood')
    self.statics = {stake}

    self.carriage = self.world:createBody({x = -560, y = Cannon.ground - 64})
    self.carriage:addBox(170, 40, {density = 6})
    Look.dress(self.carriage, 'darkWood')
    self.wheels = {}
    for _, x in ipairs({-616, -504}) do
        local wheel = self.world:createBody({x = x, y = Cannon.ground - 34})
        wheel:addCircle(34, {density = 2, friction = 0.9, rollingResistance = 0.05})
        Look.dress(wheel, Cannon.wheel)
        self.world:createJoint('revolute', self.carriage, wheel, {ax = x, ay = Cannon.ground - 34})
        self.wheels[#self.wheels + 1] = wheel
    end
    self.world:createJoint('distance', stake, self.carriage, {ax = -760, ay = Cannon.ground - 50, bx = -645, by = Cannon.ground - 64, enableSpring = true, hertz = 1.2, dampingRatio = 0.5})

    self.blocks, self.bullets = {}, {}
    for row = 0, 6 do
        for column = 0, 2 do
            local block = self.world:createBody({x = 420 + column * 52, y = Cannon.ground - 25 - row * 50})
            block:addBox(50, 50, {density = 0.6})
            Look.dress(block, row < 3 and 'stone' or 'woodCrate')
            block.data.block = true
            self.blocks[#self.blocks + 1] = {body = block, x = block.x, y = block.y}
        end
    end
    self.hits, self.shots, self.reloading = 0, 0, 0

    self.world.onHit = function(a, b, contact)
        for _, pair in ipairs({{a, b}, {b, a}}) do
            local bullet, other = pair[1], pair[2]
            if bullet.data and bullet.data.bullet and not bullet.data.scored and other.data and other.data.block then
                bullet.data.scored = true
                self.hits = self.hits + 1
                self.impacts:fire(contact.x, contact.y)
            end
        end
    end
end

-- Returns the trunnion of the barrel and the direction it points, which turn with the carriage.
function Cannon:barrel()
    local carriage = self.carriage
    local cos, sin = math.cos(carriage.rotation), math.sin(carriage.rotation)
    local x = carriage.x + Cannon.trunnion[1] * cos - Cannon.trunnion[2] * sin
    local y = carriage.y + Cannon.trunnion[1] * sin + Cannon.trunnion[2] * cos
    local direction = carriage.rotation - math.rad(self.angle)
    return x, y, math.cos(direction), math.sin(direction)
end

-- Launches a ball from the muzzle and pushes the carriage back at the trunnion with the momentum the ball took.
function Cannon:fire()
    if self.reloading > 0 then
        return
    end
    local x, y, dx, dy = self:barrel()
    local length = Cannon.barrelLength
    local bullet = self.world:createBody({x = x + dx * length, y = y + dy * length, bullet = true})
    bullet:addCircle(Cannon.bulletRadius, {density = 10, restitution = 0.1})
    Look.dress(bullet, 'iron')
    bullet.data.bullet = true
    local velocity = self.carriage.velocity
    bullet.velocity = {velocity.x + dx * self.power, velocity.y + dy * self.power}
    self.carriage:applyImpulse(-dx * bullet.mass * self.power, -dy * bullet.mass * self.power, x, y)
    self.bullets[#self.bullets + 1] = bullet
    if #self.bullets > Cannon.maxBullets then
        table.remove(self.bullets, 1):destroy()
    end
    self.flash:fire(x + dx * (length + 10), y + dy * (length + 10), 1, math.atan(dy, dx))
    self.shots, self.reloading = self.shots + 1, Cannon.cooldown
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
    self.flash:update(dt)
    self.impacts:update(dt)
    self:status(string.format('The barrel points %d degrees up. %d of %d shots hit the tower and knocked %d of its %d blocks.', math.floor(self.angle + 0.5), self.hits, self.shots, self:knocked(), #self.blocks))
end

function Cannon:fixedUpdate(step)
    self.reloading = math.max(0, self.reloading - step)
    self:simulate(self.world, step)
end

function Cannon:draw(area)
    Scenery.sky(self.camera, Cannon.ground)
    Scenery.ground(Cannon.ground)
    Look.drawAll(self.statics)
    for _, block in ipairs(self.blocks) do
        Look.draw(block.body, {layer = 1})
    end
    Look.drawAll(self.bullets, {layer = 1})
    local carriage = self.carriage
    local cos, sin = math.cos(carriage.rotation), math.sin(carriage.rotation)
    Look.spring(-760, Cannon.ground - 50, carriage.x - 85 * cos, carriage.y - 85 * sin, 20, 6, '#FFA9BCD3', 1)
    Look.draw(carriage, {layer = 2})

    -- The barrel turns around its trunnion, drawn so its muzzle lies the length of the barrel ahead.
    local x, y, dx, dy = self:barrel()
    local picture = Cannon.picture
    local scale = Cannon.barrelLength / picture.muzzle
    graphics2d.draw(self.barrelTexture, x, y, {width = picture.width * scale, height = picture.height * scale, pivotX = picture.pivot, pivotY = 0.5, rotation = math.atan(dy, dx), layer = 3})
    Look.drawAll(self.wheels, {layer = 4})
    self.flash:draw()
    self.impacts:draw()
end

return Cannon
