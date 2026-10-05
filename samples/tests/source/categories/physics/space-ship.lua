-- A ship in a world without gravity that turns, thrusts with `body:applyForce` and fires bullets, where everything wraps around the edges of the stage and rocks hit by bullets break into smaller rocks with `physics2d.fracture`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local SpaceShip = haylen.class('SpaceShip', PhysicsTest)

SpaceShip.actions = {
    {name = 'turn', type = 'axis', positive = {'key:d', 'key:right', 'axis:leftX+', 'button:dpadRight', 'virtual:turnRight'}, negative = {'key:a', 'key:left', 'axis:leftX-', 'button:dpadLeft', 'virtual:turnLeft'}},
    {name = 'thrust', type = 'button', bindings = {'key:w', 'key:up', 'axis:rightTrigger+', 'button:north', 'virtual:thrust'}},
    {name = 'fire', type = 'button', bindings = {'key:space', 'button:south', 'virtual:fire'}},
}

local kShip, kRock, kBullet = 2, 4, 8
local kHalfWidth, kHalfHeight = 830, 460
local kTurnSpeed = 4
local kThrust = 520
local kBulletSpeed = 900
local kBulletLife = 1.1
local kFireEvery = 0.18
local kLastLevel = 3
local kRockColors = {'#FF8D8F99', '#FFA1887F', '#FFB0BEC5'}

function SpaceShip:enter()
    self:frame{
        hint = 'Turn with A and D, the arrows or the left stick, thrust with W, the up arrow, the right trigger or the north button, and fire with Space or the south button. Everything wraps around the edges.',
        controls = {
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = SpaceShip.actions,
        overlay = {
            ui.touchButton{action = 'turnLeft', text = 'Left', size = 130, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}},
            ui.touchButton{action = 'turnRight', text = 'Right', size = 130, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 190}},
            ui.touchButton{action = 'thrust', text = 'Thrust', size = 130, touchOnly = true, anchor = 'bottomRight', margin = {0, 690, 110, 0}},
            ui.touchButton{action = 'fire', text = 'Fire', size = 150, touchOnly = true, anchor = 'bottomRight', margin = {0, 520, 110, 0}},
        },
    }
    self.random = m.random(57)
    self:build()
end

function SpaceShip:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.rocks, self.bullets, self.impacts = {}, {}, {}
    self.score, self.bumps, self.cooldown, self.wave, self.seed, self.throttle = 0, 0, 0, 0, 0, 0

    self.ship = self.world:createBody({linearDamping = 0.3})
    parts.polygon(self.ship, {{26, 0}, {-16, -15}, {-9, 0}, {-16, 15}}, {category = kShip, mask = kRock})
    parts.paint(self.ship, '#FFECEFF1')
    self:newWave()

    self.world.onContactBegin = function(a, b)
        for _, pair in ipairs({{a, b}, {b, a}}) do
            local data = pair[1].data
            if data.bullet and not data.spent then
                data.spent = true
                self.impacts[#self.impacts + 1] = {bullet = pair[1], rock = pair[2]}
            elseif pair[1] == self.ship then
                self.bumps = self.bumps + 1
            end
        end
    end
end

-- A wave starts with big rocks around the edges, away from the ship.
function SpaceShip:newWave()
    self.wave = self.wave + 1
    for index = 1, 3 + self.wave do
        local angle = index / (3 + self.wave) * math.pi * 2
        self:rock(math.cos(angle) * 650, math.sin(angle) * 360, 70)
    end
end

function SpaceShip:rock(x, y, radius)
    local random = self.random
    local rock = self.world:createBody({x = x, y = y, vx = random:range(-90, 90), vy = random:range(-90, 90), angularVelocity = random:range(-1, 1)})
    local points = {}
    for corner = 0, 7 do
        local angle, reach = corner / 8 * math.pi * 2, radius * random:range(0.8, 1.1)
        points[corner + 1] = {math.cos(angle) * reach, math.sin(angle) * reach}
    end
    parts.polygon(rock, points, {category = kRock, mask = kRock | kShip | kBullet})
    self:addRock(rock, 1)
end

function SpaceShip:addRock(rock, level)
    parts.paint(rock, kRockColors[level])
    rock.data.level = level
    self.rocks[#self.rocks + 1] = rock
end

-- The pieces keep the motion of the rock, take a push away from the impact and carry the outlines of their shapes into the parts drawing.
function SpaceShip:breakRock(rock, x, y)
    local level = rock.data.level
    self.score = self.score + 1
    if level == kLastLevel then
        rock:destroy()
        return
    end
    self.seed = self.seed + 1
    for _, piece in ipairs(physics2d.fracture(rock, {pieces = 4 - level, impact = {x, y}, seed = self.seed, minimumArea = 300})) do
        for _, shape in ipairs(piece:shapes()) do
            local outline = {}
            for _, point in ipairs(shape.points) do
                outline[#outline + 1] = {point.x, point.y}
            end
            parts.outline(piece, outline)
        end
        local dx, dy = piece.worldCenter.x - x, piece.worldCenter.y - y
        local length = math.max(math.sqrt(dx * dx + dy * dy), 1)
        piece:applyImpulse(dx / length * piece.mass * 120, dy / length * piece.mass * 120)
        self:addRock(piece, level + 1)
    end
end

function SpaceShip:fire()
    local ship = self.ship
    local cos, sin = math.cos(ship.rotation), math.sin(ship.rotation)
    local velocity = ship.velocity
    local bullet = self.world:createBody({x = ship.x + cos * 30, y = ship.y + sin * 30, vx = velocity.x + cos * kBulletSpeed, vy = velocity.y + sin * kBulletSpeed, bullet = true})
    parts.circle(bullet, 4, {category = kBullet, mask = kRock})
    parts.paint(bullet, '#FFFFEB3B')
    bullet.data.bullet, bullet.data.life = true, kBulletLife
    self.bullets[#self.bullets + 1] = bullet
end

function SpaceShip:exit()
    SpaceShip.super.exit(self)
    input.clearVirtual()
end

function SpaceShip:update(dt)
    SpaceShip.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.ship.angularVelocity = input.value('turn') * kTurnSpeed
    self.throttle = input.value('thrust')
    self.cooldown = self.cooldown - dt
    if input.down('fire') and self.cooldown <= 0 then
        self.cooldown = kFireEvery
        self:fire()
    end
    local velocity = self.ship.velocity
    self:status(string.format('Wave %d, rocks %d, bullets %d, score %d, ship speed %.0f, bumps %d, step %.2f ms', self.wave, #self.rocks, #self.bullets, self.score, math.sqrt(velocity.x ^ 2 + velocity.y ^ 2), self.bumps, self:stepTime()))
end

-- Bodies that leave one edge come back at the other, keeping their motion.
function SpaceShip:wrap(body)
    local x, y = body.x, body.y
    if math.abs(x) > kHalfWidth then
        body.x = x - m.sign(x) * kHalfWidth * 2
    end
    if math.abs(y) > kHalfHeight then
        body.y = y - m.sign(y) * kHalfHeight * 2
    end
end

function SpaceShip:fixedUpdate(step)
    local ship = self.ship
    if self.throttle > 0 then
        local force = ship.mass * kThrust * self.throttle
        ship:applyForce(math.cos(ship.rotation) * force, math.sin(ship.rotation) * force)
    end
    self:simulate(self.world, step)

    for _, impact in ipairs(self.impacts) do
        if impact.rock.valid and impact.rock.data.level then
            self:breakRock(impact.rock, impact.bullet.x, impact.bullet.y)
        end
        impact.bullet.data.life = 0
    end
    self.impacts = {}
    local rocks = {}
    for _, rock in ipairs(self.rocks) do
        if rock.valid then
            self:wrap(rock)
            rocks[#rocks + 1] = rock
        end
    end
    self.rocks = rocks
    for index = #self.bullets, 1, -1 do
        local bullet = self.bullets[index]
        bullet.data.life = bullet.data.life - step
        if bullet.data.life <= 0 then
            table.remove(self.bullets, index):destroy()
        else
            self:wrap(bullet)
        end
    end
    self:wrap(ship)
    if #self.rocks == 0 then
        self:newWave()
    end
end

function SpaceShip:draw(area)
    parts.drawAll(self.rocks)
    parts.drawAll(self.bullets, {layer = 1})
    local ship = self.ship
    parts.draw(ship, {layer = 2})
    if self.throttle > 0 then
        local cos, sin = math.cos(ship.rotation), math.sin(ship.rotation)
        local length = 14 + self.throttle * self.random:range(14, 26)
        local x, y = ship.x - cos * 10, ship.y - sin * 10
        graphics2d.drawPolygon({{x - sin * 7, y + cos * 7}, {x + sin * 7, y - cos * 7}, {x - cos * length, y - sin * length}}, '#FFFFB74D', {layer = 1, blend = 'additive'})
    end
    graphics2d.drawRectOutline({-800, -430, 1600, 860}, 2, '#332C3147', {layer = -1})
end

return SpaceShip
