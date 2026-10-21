-- ECS: the world of `ecs.lua` with hundreds of balls whose behavior comes only from the components they hold and the systems that run over them: every ball moves and bounces, some fall with gravity, some seek the cursor, and some live for a while, fade and give way to new ones. A system switches off with its toggle, and the status line counts the entities, the components and what each system visited.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ComponentTest = require('categories.components.component-test')
local Ecs = require('categories.components.ecs')
local Test = require('harness.test')

local EcsTest = haylen.class('EcsTest', ComponentTest)

EcsTest.code = [[
local world = require('ecs').new()  -- One file, copied from samples/tests.
local entity = world:spawn({position = {x = 0, y = 0}, velocity = {x = 120, y = -40}, ball = {radius = 12}})
world:system('move', {'position', 'velocity'}, function(_, p, v, dt) p.x, p.y = p.x + v.x * dt, p.y + v.y * dt end)
world:add(entity, 'gravity', {strength = 900})  -- Gravity now acts on it.
world:update(dt)  -- Runs every system over the entities that hold what it needs.
world:destroy(entity)]]

-- An aging ball fades no further than this, so it stays visible until it ends.
EcsTest.faded = 0.35
EcsTest.colors = {{0.56, 0.69, 1}, {0.44, 0.86, 0.63}, {0.95, 0.7, 0.23}, {0.79, 0.63, 1}, {1, 0.54, 0.52}}

function EcsTest:enter()
    EcsTest.super.enter(self)
    self.world = Ecs.new()
    self.seed = 1
    self:addSystems()
    self:spawnMany(240)
    self:frame{
        hint = 'Each toggle switches a system on or off for every ball that has its component. The cursor is what the seekers chase.',
        code = EcsTest.code,
        play = true,
        view = ComponentTest.view,
        controls = {
            ui.toggle{id = 'gravity', text = 'System "gravity"', checked = true, onChange = function(event) self.world:findSystem('gravity').enabled = event.checked end},
            ui.toggle{id = 'seek', text = 'System "seek"', checked = true, onChange = function(event) self.world:findSystem('seek').enabled = event.checked end},
            ui.toggle{id = 'age', text = 'System "age"', checked = true, onChange = function(event) self.world:findSystem('age').enabled = event.checked end},
            ui.row{wrap = true, children = {
                ui.button{id = 'spawn', text = 'Spawn 200 balls', variant = 'primary', onClick = function() self:spawnMany(200) end},
                ui.button{id = 'calm', text = 'Take "velocity" from half', onClick = function() self:calm() end},
                ui.button{id = 'clear', text = 'Destroy every ball', onClick = function() self:clear() end},
            }},
        },
    }
end

function EcsTest:roll()
    self.seed = (self.seed * 1103515245 + 12345) % 2147483648
    return self.seed / 2147483648
end

-- Spawns a ball with the components every ball has, and a share of them with gravity, a seeker or a lifetime.
function EcsTest:spawnBall()
    local world = self.world
    local color = EcsTest.colors[math.floor(self:roll() * #EcsTest.colors) + 1]
    local entity = world:spawn({
        position = {x = (self:roll() - 0.5) * 1700, y = (self:roll() - 0.5) * 900},
        velocity = {x = (self:roll() - 0.5) * 500, y = (self:roll() - 0.5) * 500},
        ball = {radius = 8 + self:roll() * 14, color = {color[1], color[2], color[3], 1}},
    })
    local kind = self:roll()
    if kind < 0.3 then
        world:add(entity, 'gravity', {strength = 900})
    elseif kind < 0.5 then
        world:add(entity, 'seeker', {pull = 900})
    end
    if self:roll() < 0.4 then
        local life = 2 + self:roll() * 4
        world:add(entity, 'lifetime', {left = life, total = life})
    end
    return entity
end

function EcsTest:spawnMany(count)
    for _ = 1, count do
        self:spawnBall()
    end
end

function EcsTest:addSystems()
    local world = self.world

    -- The systems run in the order they are added: the forces first, then the motion, the walls and the age.
    world:system('gravity', {'velocity', 'gravity'}, function(_, velocity, gravity, dt)
        velocity.y = velocity.y + gravity.strength * dt
    end)

    world:system('seek', {'position', 'velocity', 'seeker'}, function(_, position, velocity, seeker, dt)
        local dx, dy = self.pointer.worldX - position.x, self.pointer.worldY - position.y
        local distance = math.sqrt(dx * dx + dy * dy) + 1
        velocity.x = velocity.x + dx / distance * seeker.pull * dt
        velocity.y = velocity.y + dy / distance * seeker.pull * dt
        velocity.x, velocity.y = velocity.x * (1 - dt * 0.8), velocity.y * (1 - dt * 0.8)
    end)

    world:system('move', {'position', 'velocity'}, function(_, position, velocity, dt)
        position.x, position.y = position.x + velocity.x * dt, position.y + velocity.y * dt
    end)

    world:system('bounce', {'position', 'velocity', 'ball'}, function(_, position, velocity, ball)
        local right, bottom = 940 - ball.radius, 520 - ball.radius
        if position.x < -right or position.x > right then
            position.x, velocity.x = math.max(-right, math.min(right, position.x)), -velocity.x * 0.9
        end
        if position.y < -bottom or position.y > bottom then
            position.y, velocity.y = math.max(-bottom, math.min(bottom, position.y)), -velocity.y * 0.85
        end
    end)

    world:system('age', {'ball', 'lifetime'}, function(entity, ball, lifetime, dt)
        lifetime.left = lifetime.left - dt
        ball.color[4] = math.max(EcsTest.faded, math.min(1, lifetime.left / lifetime.total * 2))
        if lifetime.left <= 0 then
            world:destroy(entity)
            self:spawnBall()
        end
    end)

    -- A ball draws as a disc with a darker rim and a soft highlight, in tables the painter fills in place.
    local rect, rim, shine = {0, 0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1, 0}
    local disc = {kind = 'ellipse', color = nil, borderWidth = 0, borderColor = rim, layer = 1}
    local glint = {kind = 'ellipse', color = shine, layer = 2}
    self.painter = world:system('draw', {'position', 'ball'}, function(_, position, ball)
        local radius, color = ball.radius, ball.color
        rim[1], rim[2], rim[3], rim[4] = color[1] * 0.55, color[2] * 0.55, color[3] * 0.6, color[4]
        rect[1], rect[2], rect[3], rect[4] = position.x - radius, position.y - radius, radius * 2, radius * 2
        disc.color, disc.borderWidth = color, math.max(2, radius * 0.18)
        graphics2d.drawShape(rect, disc)
        shine[4] = color[4] * 0.55
        rect[1], rect[2], rect[3], rect[4] = position.x - radius * 0.55, position.y - radius * 0.6, radius * 0.5, radius * 0.4
        graphics2d.drawShape(rect, glint)
    end)
    self.painter.enabled = false
end

-- Takes the velocity component from every other ball that moves, so those balls stop, since no system moves them any more.
function EcsTest:calm()
    local pool = self.world:pool('velocity')
    for slot = pool.count, 1, -2 do
        self.world:remove(pool.entities[slot], 'velocity')
    end
end

function EcsTest:clear()
    local pool = self.world:pool('ball')
    for slot = pool.count, 1, -1 do
        self.world:destroy(pool.entities[slot])
    end
end

function EcsTest:update(dt)
    EcsTest.super.update(self, dt)
    if not self.stage then
        return
    end

    local started = os.clock()
    self.world:update(dt)
    local spent = (os.clock() - started) * 1000
    local world = self.world
    self:status(string.format('Entities %d, of which %d have "velocity", %d "gravity", %d "seeker" and %d "lifetime", systems took %.2f ms', world.count, world:countOf('velocity'), world:countOf('gravity'), world:countOf('seeker'), world:countOf('lifetime'), spent))
end

function EcsTest:draw(area)
    graphics2d.drawShape({-954, -534, 1908, 1068}, {radius = 18, color = '#FF181D2A', borderWidth = 4, borderColor = '#FF4A5578', layer = -1})
    self.world:runSystem(self.painter, 0)
    graphics2d.drawRing(self.pointer.worldX, self.pointer.worldY, 26, 3, Test.warm, {layer = 2})
end

return EcsTest
