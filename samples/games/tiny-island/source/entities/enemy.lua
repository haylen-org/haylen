-- A night raider. It follows A* paths around the water and cliffs, steers apart from the others, never enters the light of the fire, and fights the way its kind does.
local navigation2d = require('haylen.navigation2d')

local config = require('config')
local enemies = require('data.enemies')
local sound = require('systems.sound')
local unit = require('entities.unit')

local enemy = setmetatable({}, {__index = unit})
enemy.__index = enemy

local radii = {imp = 18, brute = 30, thrower = 22, bomber = 22}

local function distance(ax, ay, bx, by)
    local dx, dy = bx - ax, by - ay
    return math.sqrt(dx * dx + dy * dy)
end

-- Raiders grow tougher with every night, by the `growth` share of their health and damage.
function enemy.new(game, kindName, night, x, y)
    local kind = enemies[kindName]
    local strength = 1 + config.enemies.growth * (night - 1)
    local self = setmetatable({game = game, kindName = kindName, kind = kind, timers = {}}, enemy)
    local category = config.category
    unit.setup(self, game.island.world, kindName, x, y, radii[kindName], category.enemy, category.world | category.player | category.enemy | category.barrier | category.tree)
    self.health = kind.health * strength
    self.maxHealth = self.health
    self.power = kind.damage * strength
    self.agent = navigation2d.newAgent({x = x, y = y, maxSpeed = kind.speed, maxForce = kind.speed * 8, seed = math.random(1, 100000)})
    self.cooldown = 0.5 + math.random() * kind.cooldown
    self.pathTime = 0
    self.time = math.random() * 10
    return self
end

-- Picks where to go: the player when it is outside the light and close, or else the edge of the light next to this raider.
function enemy:goal()
    local player = self.game.player
    local fire = self.game.campfire
    local x, y = self:position()
    if player.alive then
        local px, py = player:position()
        if not fire:contains(px, py) and (not fire:lit() or distance(x, y, px, py) < config.enemies.aggroRange) then
            return px, py, player
        end
    end
    local dx, dy = x - fire.x, y - fire.y
    local length = math.max(1, math.sqrt(dx * dx + dy * dy))
    return fire.x + dx / length * (fire.radius + 70), fire.y + dy / length * (fire.radius + 70), nil
end

function enemy:after(delay, action)
    self.timers[#self.timers + 1] = {time = delay, action = action}
end

local attacks = {}

-- The imp rakes with its claws, a small violet swipe in front of it.
function attacks.imp(self)
    sound.play('swing', self:position())
    self:after(0.12, function()
        local x, y = self:position()
        self.game.effects:animate('slash', x + self.facing * 40, y - 36, {rotation = self.facing < 0 and math.pi or 0, scale = 0.5, color = '#FFD08CFF', glow = true})
        self.game:strikePlayer(self, self.power, self.kind.range + 30)
    end)
end

-- The brute glows red while it winds up, which gives the player time to step away, then smashes the ground in front of it.
function attacks.brute(self)
    self.flashColor = 'FF3020'
    self.windup = self.kind.windup
    self:after(self.kind.windup, function()
        local x, y = self:position()
        local sx = x + self.facing * 70
        self.flashColor = nil
        self.game.effects:burst('dust', sx, y, 14)
        self.game.effects:animate('hit', sx, y - 20, {scale = 1.3, glow = true})
        self.game.camera:addTrauma(0.3)
        sound.play('hit', sx, y)
        self.game:smash(self, sx, y, self.kind.smash, self.power)
    end)
end

function attacks.thrower(self, target)
    sound.play('swing', self:position())
    self:after(self.kind.release, function()
        local x, y = self:position()
        local px, py = target:position()
        local dx, dy = px - x, py - y - 20
        local length = math.max(1, math.sqrt(dx * dx + dy * dy))
        self.game:shoot('javelin', self, x + self.facing * 30, y - 60, dx / length, dy / length, self.power)
    end)
end

function attacks.bomber(self, target)
    self:after(self.kind.release, function()
        local x, y = self:position()
        local px, py = target:position()
        sound.play('swing', x, y)
        self.game:shoot('bomb', self, x + self.facing * 24, y - 50, px, py, self.power)
    end)
end

function enemy:attack(target)
    local x = self:position()
    local px = target:position()
    self.cooldown = self.kind.cooldown
    self.facing = px > x and 1 or -1
    self:play('attack', true)
    attacks[self.kindName](self, target)
end

-- Dawn sends the survivors running from the fire until they burn away.
function enemy:ignite()
    self.burnTime = 1.2 + math.random() * 1.2
    self.timers = {}
end

function enemy:burn(dt)
    local x, y = self:position()
    self.burnTime = self.burnTime - dt
    local fire = self.game.campfire
    local dx, dy = x - fire.x, y - fire.y
    local length = math.max(1, math.sqrt(dx * dx + dy * dy))
    unit.move(self, dx / length * self.kind.speed * 1.3, dy / length * self.kind.speed * 1.3, dt)
    self.flash = 0.5 + 0.5 * math.sin(self.burnTime * 30)
    self.flashColor = 'FF8A3D'
    self:play('run')
    self.animator:update(dt)
    if math.random() < dt * 12 then
        self.game.effects:burst('embers', x, y - 50, 2)
    end
    if self.burnTime <= 0 then
        self.game:killEnemy(self, true)
    end
end

-- Counts down the delayed blows, which a raider that fell or burned never lands.
function enemy:runTimers(dt)
    for index = #self.timers, 1, -1 do
        local timer = self.timers[index]
        timer.time = timer.time - dt
        if timer.time <= 0 then
            table.remove(self.timers, index)
            timer.action()
            if not self.alive then
                return
            end
        end
    end
end

function enemy:update(dt, neighbours)
    if self.burnTime then
        self:burn(dt)
        return
    end
    self.time = self.time + dt
    self:runTimers(dt)
    if not self.alive then
        return
    end

    local x, y = self:position()
    local tx, ty, target = self:goal()
    local range = distance(x, y, tx, ty)
    self.cooldown = self.cooldown - dt
    if target and range <= self.kind.range and self.cooldown <= 0 and not unit.busy(self, 'attack') then
        self:attack(target)
    end

    self.pathTime = self.pathTime - dt
    if self.pathTime <= 0 then
        self.path = self.game.island:path(x, y, tx, ty)
        self.pathTime = 0.45 + math.random() * 0.3
    end
    local wx, wy = tx, ty
    if self.path and #self.path > 0 then
        if distance(x, y, self.path[1].x, self.path[1].y) < 48 and #self.path > 1 then
            table.remove(self.path, 1)
        end
        wx, wy = self.path[1].x, self.path[1].y
    end

    -- Imps weave from side to side on their way in, which makes them hard to hit.
    if self.kind.weave then
        local dx, dy = wx - x, wy - y
        local length = math.max(1, math.sqrt(dx * dx + dy * dy))
        local sway = math.sin(self.time * 6) * self.kind.weave
        wx, wy = wx - dy / length * sway, wy + dx / length * sway
    end

    self.agent.position = {x, y}
    local force
    if target and self.kind.keepDistance > 0 and range < self.kind.keepDistance then
        force = self.agent:flee({tx, ty})
    elseif target and range <= self.kind.range * 0.8 then
        force = self.agent:arrive({x, y}, 1)
    else
        force = self.agent:arrive({wx, wy}, 80)
    end
    force = force + self.agent:separation(neighbours, 80) * 1.6
    self.agent:apply(force, dt)
    local velocity = self.agent.velocity
    local attacking = unit.busy(self, 'attack')
    local slow = attacking and 0.15 or 1
    unit.move(self, velocity.x * slow, velocity.y * slow, dt)

    if target and attacking then
        self.facing = tx > x and 1 or -1
    end
    if self.windup then
        self.windup = self.windup - dt
        self.flash = 0.35 + 0.35 * math.sin(self.windup * 40)
        if self.windup <= 0 then
            self.windup = nil
        end
    end
    if not attacking and not unit.busy(self, 'hurt') then
        self:play(velocity:length() > 20 and 'run' or 'idle')
    end
    self:animate(dt)
end

-- Falls with the last animation of its kind and leaves the physics world.
function enemy:fall()
    self.timers = {}
    self.flashColor = nil
    self:play('death', true)
    unit.destroy(self)
end

return enemy
