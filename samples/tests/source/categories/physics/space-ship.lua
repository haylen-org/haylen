-- A rocket ship in a world without gravity that turns, thrusts with `body:applyForce` and fires bullets, where everything wraps around the edges of what the camera shows and rocks hit by bullets break into smaller rocks with `physics2d.fracture`. The ship aims at the nearest rock and fires by itself until the player steers it.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local SpaceShip = haylen.class('SpaceShip', PhysicsTest)

SpaceShip.code = [[
local world = physics2d.newWorld({gravity = {0, 0}})
ship.angularVelocity = turn * 4
ship:applyForce(math.cos(ship.rotation) * ship.mass * 520, math.sin(ship.rotation) * ship.mass * 520)
local shot = world:createBody({x = noseX, y = noseY, vx = dx * 900, vy = dy * 900, bullet = true})
physics2d.fracture(rock, {pieces = 3, impact = {shot.x, shot.y}, minimumArea = 300})
if body.x > bounds.x + bounds.width + radius then body.x = bounds.x - radius end  -- Wraps around the edge.]]

SpaceShip.actions = {
    {name = 'turn', type = 'axis', positive = {'key:d', 'key:right', 'axis:leftX+', 'button:dpadRight', 'virtual:turnRight'}, negative = {'key:a', 'key:left', 'axis:leftX-', 'button:dpadLeft', 'virtual:turnLeft'}},
    {name = 'thrust', type = 'button', bindings = {'key:w', 'key:up', 'axis:rightTrigger+', 'button:north', 'virtual:thrust'}},
    {name = 'fire', type = 'button', bindings = {'key:space', 'button:south', 'virtual:fire'}},
}

SpaceShip.shipCategory, SpaceShip.rockCategory, SpaceShip.bulletCategory = 2, 4, 8
SpaceShip.turnSpeed = 4
SpaceShip.thrust = 520
SpaceShip.bulletSpeed = 900
SpaceShip.bulletLife = 1.1
SpaceShip.fireEvery = 0.18
SpaceShip.lastLevel = 3
-- The rocks of each level, from the big ones of a new wave to the smallest pieces, with their size and their stone.
SpaceShip.levels = {
    {radius = 70, material = 'slate'},
    {radius = 42, material = 'stone'},
    {radius = 24, material = Look.tint('stone', '#FFB58E6B', '#FF8A6448', '#FFD9B596')},
}
-- The craters of a rock, by their place and size as shares of its radius.
SpaceShip.craters = {{-0.3, -0.2, 0.24}, {0.32, 0.26, 0.17}, {0.12, -0.46, 0.12}}
SpaceShip.shipSize = {width = 56, height = 43, nozzle = 26}

function SpaceShip:enter()
    self:frame{
        code = SpaceShip.code,
        hint = 'Turn with A and D, the arrows or the left stick, thrust with W, the up arrow, the right trigger or the north button, and fire with Space or the south button. Everything wraps around the edges.',
        controls = {
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = SpaceShip.actions,
        overlay = {
            ui.touchButton{action = 'turnLeft', text = 'Left', size = 130, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}},
            ui.touchButton{action = 'turnRight', text = 'Right', size = 130, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 174}},
            ui.touchButton{action = 'thrust', text = 'Thrust', size = 130, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 174, 24, 0}},
            ui.touchButton{action = 'fire', text = 'Fire', size = 150, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 4, 24, 0}},
        },
    }
    self.random = m.random(57)
    self.picture = assets.texture('physics/ship.png', {filter = 'linear'})
    self.glow = Scenery.glow()
    self.dust = Burst('explosions/smoke-burst', {count = 6, scale = 0.4, layer = 30})
    self.edges = {x = -800, y = -380, width = 1600, height = 760}
    self.craterShape = {kind = 'ellipse', color = '#FFFFFFFF', layer = 1}
    self.craterRect = {0, 0, 0, 0}
    self:build()
end

function SpaceShip:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.rocks, self.bullets, self.impacts = {}, {}, {}
    self.score, self.bumps, self.cooldown, self.wave, self.seed, self.throttle = 0, 0, 0, 0, 0, 0
    self.autopilot = true

    self.ship = self.world:createBody({linearDamping = 0.3})
    self.ship:addPolygon({{26, 0}, {-16, -15}, {-9, 0}, {-16, 15}}, {category = SpaceShip.shipCategory, mask = SpaceShip.rockCategory})
    self.ship.data = {radius = 28}
    self:newWave()

    self.world.onContactBegin = function(a, b)
        self:contact(a, b)
        self:contact(b, a)
    end
end

function SpaceShip:contact(body, other)
    local data = body.data
    if data.bullet and not data.spent then
        data.spent = true
        self.impacts[#self.impacts + 1] = {bullet = body, rock = other}
    elseif body == self.ship then
        self.bumps = self.bumps + 1
    end
end

-- A wave starts with big rocks around the edges, away from the ship.
function SpaceShip:newWave()
    self.wave = self.wave + 1
    for index = 1, 3 + self.wave do
        local angle = index / (3 + self.wave) * math.pi * 2
        self:rock(math.cos(angle) * 620, math.sin(angle) * 300)
    end
end

function SpaceShip:rock(x, y)
    local random = self.random
    local radius = SpaceShip.levels[1].radius
    local rock = self.world:createBody({x = x, y = y, vx = random:range(-90, 90), vy = random:range(-90, 90), angularVelocity = random:range(-1, 1)})
    local points = {}
    for corner = 0, 7 do
        local angle, reach = corner / 8 * math.pi * 2, radius * random:range(0.8, 1.1)
        points[corner + 1] = {math.cos(angle) * reach, math.sin(angle) * reach}
    end
    rock:addPolygon(points, {category = SpaceShip.rockCategory, mask = SpaceShip.rockCategory | SpaceShip.shipCategory | SpaceShip.bulletCategory})
    self:addRock(rock, 1)
end

function SpaceShip:addRock(rock, level)
    local spec = SpaceShip.levels[level]
    Look.dress(rock, spec.material)
    rock.data.level, rock.data.radius = level, spec.radius
    self.rocks[#self.rocks + 1] = rock
end

-- The pieces keep the motion of the rock, take a push away from the impact and draw in the stone of the next level.
function SpaceShip:breakRock(rock, x, y)
    local level = rock.data.level
    self.score = self.score + 1
    self.dust:fire(x, y)
    if level == SpaceShip.lastLevel then
        rock:destroy()
        return
    end
    self.seed = self.seed + 1
    for _, piece in ipairs(physics2d.fracture(rock, {pieces = 4 - level, impact = {x, y}, seed = self.seed, minimumArea = 300})) do
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
    local speed = SpaceShip.bulletSpeed
    local bullet = self.world:createBody({x = ship.x + cos * 30, y = ship.y + sin * 30, vx = velocity.x + cos * speed, vy = velocity.y + sin * speed, bullet = true})
    bullet:addCircle(4, {category = SpaceShip.bulletCategory, mask = SpaceShip.rockCategory})
    bullet.data = {bullet = true, life = SpaceShip.bulletLife, radius = 4}
    self.bullets[#self.bullets + 1] = bullet
end

function SpaceShip:exit()
    SpaceShip.super.exit(self)
    input.clearVirtual()
end

-- The autopilot turns the ship toward the nearest rock and fires once the nose points at it.
function SpaceShip:aim()
    local ship, nearest, best = self.ship, nil, math.huge
    for _, rock in ipairs(self.rocks) do
        local distance = (rock.x - ship.x) ^ 2 + (rock.y - ship.y) ^ 2
        if distance < best then
            nearest, best = rock, distance
        end
    end
    if not nearest then
        return 0, false
    end
    local wanted = math.atan(nearest.y - ship.y, nearest.x - ship.x)
    local turn = (wanted - ship.rotation + math.pi) % (math.pi * 2) - math.pi
    return m.clamp(turn * 3, -1, 1), math.abs(turn) < 0.12
end

function SpaceShip:update(dt)
    SpaceShip.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.dust:update(dt)
    local turn, firing = input.value('turn'), input.down('fire')
    self.throttle = input.value('thrust')
    if turn ~= 0 or firing or self.throttle > 0 then
        self.autopilot = false
    end
    if self.autopilot then
        turn, firing = self:aim()
    end
    self.ship.angularVelocity = turn * SpaceShip.turnSpeed
    self.cooldown = self.cooldown - dt
    if firing and self.cooldown <= 0 then
        self.cooldown = SpaceShip.fireEvery * (self.autopilot and 2.5 or 1)
        self:fire()
    end
    local rocks = #self.rocks
    local pilot = self.autopilot and 'The ship aims and fires by itself until you steer it. ' or ''
    self:status(string.format('%sWave %d has %d %s left after %d hits, and the ship bumped into rocks %d times.', pilot, self.wave, rocks, rocks == 1 and 'rock' or 'rocks', self.score, self.bumps))
end

-- A body that leaves one edge of what the camera shows comes back at the other once it is out of sight, keeping its motion.
function SpaceShip:wrap(body)
    local edges, radius = self.edges, body.data.radius
    local left, right = edges.x - radius, edges.x + edges.width + radius
    local top, bottom = edges.y - radius, edges.y + edges.height + radius
    local x, y = body.x, body.y
    if x < left then
        body.x = right
    elseif x > right then
        body.x = left
    end
    if y < top then
        body.y = bottom
    elseif y > bottom then
        body.y = top
    end
end

function SpaceShip:fixedUpdate(step)
    local ship = self.ship
    if self.throttle > 0 then
        local force = ship.mass * SpaceShip.thrust * self.throttle
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
    for index = #self.rocks, 1, -1 do
        local rock = self.rocks[index]
        if rock.valid then
            self:wrap(rock)
        else
            table.remove(self.rocks, index)
        end
    end
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

-- Every rock carries a few craters that turn with it, as many as its size holds.
function SpaceShip:drawCraters(rock)
    local radius = rock.data.radius
    local cos, sin = math.cos(rock.rotation), math.sin(rock.rotation)
    local shape, rect = self.craterShape, self.craterRect
    local count = 4 - rock.data.level
    for index = 1, count do
        local crater = SpaceShip.craters[index]
        local cx, cy, size = crater[1] * radius * 0.6, crater[2] * radius * 0.6, crater[3] * radius
        local x, y = rock.x + cx * cos - cy * sin, rock.y + cx * sin + cy * cos
        rect[1], rect[2], rect[3], rect[4] = x - size, y - size, size * 2, size * 2
        shape.color = '#80202040'
        graphics2d.drawShape(rect, shape)
        rect[1], rect[2], rect[3], rect[4] = x - size * 0.7, y - size * 0.45, size * 1.6, size * 1.5
        shape.color = '#40FFFFFF'
        graphics2d.drawShape(rect, shape)
    end
end

function SpaceShip:draw(area)
    local bounds = graphics2d.canvasBounds()
    self.edges.x, self.edges.y, self.edges.width, self.edges.height = bounds.x, bounds.y, bounds.width, bounds.height
    Scenery.space()
    for _, rock in ipairs(self.rocks) do
        if rock.valid then
            Look.draw(rock, {layer = 1})
            self:drawCraters(rock)
        end
    end
    for _, bullet in ipairs(self.bullets) do
        graphics2d.draw(self.glow, bullet.x, bullet.y, {width = 34, height = 34, color = '#FFFFC93C', blend = 'additive', layer = 2})
        graphics2d.drawCircle(bullet.x, bullet.y, 4, '#FFFFF8DC', {layer = 2})
    end

    local ship, picture = self.ship, SpaceShip.shipSize
    local cos, sin = math.cos(ship.rotation), math.sin(ship.rotation)
    if self.throttle > 0 then
        local length = 14 + self.throttle * self.random:range(14, 26)
        local x, y = ship.x - cos * (picture.nozzle + length / 2), ship.y - sin * (picture.nozzle + length / 2)
        graphics2d.drawShape({x - length / 2, y - 7, length, 14}, {kind = 'ellipse', rotation = ship.rotation, color = '#E6FF9A3C', layer = 2})
        graphics2d.drawShape({x - length * 0.3, y - 4, length * 0.6, 8}, {kind = 'ellipse', rotation = ship.rotation, color = '#FFFFF0A0', layer = 2})
    end
    graphics2d.draw(self.picture, ship.x, ship.y, {width = picture.width, height = picture.height, rotation = ship.rotation, layer = 3})
    self.dust:draw()
    self:label(string.format('Wave %d', self.wave), bounds.x + 24, bounds.y + 24, {anchor = {0, 0}, size = 24})
    self:label(string.format('Hits %d', self.score), bounds.x + 24, bounds.y + 24 + 50 * Look.pixel, {anchor = {0, 0}, size = 24})
end

return SpaceShip
