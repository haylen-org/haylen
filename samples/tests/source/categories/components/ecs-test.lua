-- ECS: the world of `ecs.lua` with hundreds of balls whose behavior comes only from the components they hold and the systems that run over them: every ball moves and bounces, some fall with gravity, some seek the cursor, and some live for a while, with a ring of the time they have left, and give way to new ones. The color of a ball tells its components, as the legend in the arena says. A system switches off with its toggle, and the status line counts the entities and the components.
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

-- The color of a ball tells what moves it: blue balls only move and bounce, orange ones also fall with gravity and violet ones seek the cursor.
EcsTest.colors = {plain = {0.56, 0.69, 1}, gravity = {0.95, 0.66, 0.23}, seeker = {0.79, 0.6, 1}}
EcsTest.arena = {-920, -500, 1840, 1000}
EcsTest.legend = {
    {'plain', 'Moves and bounces'},
    {'gravity', 'Also falls with gravity'},
    {'seeker', 'Also seeks the cursor'},
    {'lifetime', 'Lives a while, its ring is the time left'},
}

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
    local roll = self:roll()
    local kind = roll < 0.3 and 'gravity' or roll < 0.5 and 'seeker' or 'plain'
    local entity = world:spawn({
        position = {x = (self:roll() - 0.5) * 1700, y = (self:roll() - 0.5) * 900},
        velocity = {x = (self:roll() - 0.5) * 500, y = (self:roll() - 0.5) * 500},
        ball = {radius = 9 + self:roll() * 14, color = EcsTest.colors[kind]},
    })
    if kind == 'gravity' then
        world:add(entity, 'gravity', {strength = 900})
    elseif kind == 'seeker' then
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
        local right, bottom = EcsTest.arena[3] / 2 - 6 - ball.radius, EcsTest.arena[4] / 2 - 6 - ball.radius
        if position.x < -right or position.x > right then
            position.x, velocity.x = math.max(-right, math.min(right, position.x)), -velocity.x * 0.9
        end
        if position.y < -bottom or position.y > bottom then
            position.y, velocity.y = math.max(-bottom, math.min(bottom, position.y)), -velocity.y * 0.85
        end
    end)

    world:system('age', {'lifetime'}, function(entity, lifetime, dt)
        lifetime.left = lifetime.left - dt
        if lifetime.left <= 0 then
            world:destroy(entity)
            self:spawnBall()
        end
    end)

    -- A ball draws as a disc with a darker rim and a soft highlight, in tables the painter fills in place, and a ball with a lifetime draws a white ring of the time it has left.
    local rect, rim = {0, 0, 0, 0}, {0, 0, 0, 1}
    local disc = {kind = 'ellipse', color = nil, borderWidth = 0, borderColor = rim, layer = 1}
    local glint = {kind = 'ellipse', color = '#8CFFFFFF', layer = 2}
    local ring = {layer = 2}
    self.painter = world:system('draw', {'position', 'ball'}, function(entity, position, ball)
        local radius, color = ball.radius, ball.color
        rim[1], rim[2], rim[3] = color[1] * 0.55, color[2] * 0.55, color[3] * 0.6
        rect[1], rect[2], rect[3], rect[4] = position.x - radius, position.y - radius, radius * 2, radius * 2
        disc.color, disc.borderWidth = color, math.max(2, radius * 0.18)
        graphics2d.drawShape(rect, disc)
        rect[1], rect[2], rect[3], rect[4] = position.x - radius * 0.55, position.y - radius * 0.6, radius * 0.5, radius * 0.4
        graphics2d.drawShape(rect, glint)
        local lifetime = world:get(entity, 'lifetime')
        if lifetime then
            graphics2d.drawRing(position.x, position.y, radius + 7, 3, '#40FFFFFF', ring)
            graphics2d.drawArc(position.x, position.y, radius + 7, 3, -math.pi / 2, -math.pi / 2 + math.pi * 2 * lifetime.left / lifetime.total, '#FFFFFFFF', ring)
        end
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
    self:status(string.format('The arena holds %d balls: %d move, %d fall with gravity, %d seek the cursor and %d live for a while. The systems took %.1f ms.', world.count, world:countOf('velocity'), world:countOf('gravity'), world:countOf('seeker'), world:countOf('lifetime'), spent))
end

function EcsTest:draw(area)
    graphics2d.drawShape(EcsTest.arena, {radius = 24, color = '#FF1E2536', borderWidth = 4, borderColor = '#FF4A5578', layer = -1})
    self.world:runSystem(self.painter, 0)
    graphics2d.drawRing(self.pointer.worldX, self.pointer.worldY, 26, 3, Test.warm, {layer = 2})
    self:drawLegend()
end

-- The legend sits on a plate in the top left corner of the arena, above the balls, with a ball of each color and a ball with its ring.
function EcsTest:drawLegend()
    local left, top = EcsTest.arena[1] + 24, EcsTest.arena[2] + 24
    graphics2d.drawShape({left, top, 740, 36 + #EcsTest.legend * 52}, {radius = 18, color = '#F5141824', borderWidth = 2, borderColor = '#FF2C3147', layer = 5})
    for index, entry in ipairs(EcsTest.legend) do
        local x, y = left + 44, top + 18 + (index - 0.5) * 52
        local color = EcsTest.colors[entry[1]] or EcsTest.colors.plain
        graphics2d.drawCircle(x, y, 14, color, {layer = 6})
        if entry[1] == 'lifetime' then
            graphics2d.drawArc(x, y, 21, 3, -math.pi / 2, math.pi * 0.9, '#FFFFFFFF', {layer = 6})
        end
        graphics2d.drawText(nil, entry[2], x + 42, y, {size = 36, color = Test.ink, anchor = {0, 0.5}, layer = 6})
    end
end

return EcsTest
