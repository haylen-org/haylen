-- A night raider. It follows A* paths around the water and cliffs, steers apart from the others, and never enters the light of the fire.
local navigation2d = require('haylen.navigation2d')

local config = require('config')
local enemies = require('data.enemies')
local sound = require('systems.sound')
local unit = require('entities.unit')

local enemy = setmetatable({}, {__index = unit})
enemy.__index = enemy

local function distance(ax, ay, bx, by)
    local dx, dy = bx - ax, by - ay
    return math.sqrt(dx * dx + dy * dy)
end

function enemy.new(game, kindName, color, x, y)
    local kind = enemies.kinds[kindName]
    local strength = enemies.colors[color]
    local self = setmetatable({game = game, kind = kind, color = color}, enemy)
    local category = config.category
    unit.setup(self, game.island.world, kind.unit, color, x, y, category.enemy, category.world | category.player | category.enemy | category.barrier | category.tree)
    self.health = kind.health * strength.health
    self.maxHealth = self.health
    self.power = kind.damage * strength.damage
    self.agent = navigation2d.newAgent({x = x, y = y, maxSpeed = kind.speed, maxForce = kind.speed * 8, seed = math.random(1, 100000)})
    self.cooldown = math.random() * kind.cooldown
    self.pathTime = 0
    return self
end

-- Picks where to go: the player when it is outside the light and close, or else the edge of the light next to this enemy.
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

function enemy:attacking()
    return self.animator.current == 'attack' and not self.animator.finished
end

function enemy:attack(target)
    local x, y = self:position()
    local px, py = target:position()
    self.cooldown = self.kind.cooldown
    self.facing = px > x and 1 or -1
    self:play('attack', true)
    if self.kind.unit == 'archer' then
        local dx, dy = px - x, py - y
        local length = math.max(1, math.sqrt(dx * dx + dy * dy))
        self.game:shoot(self, x, y - 40, dx / length, dy / length, self.power, 1)
    else
        sound.play('swing', x, y)
        self.game:strikePlayer(self, self.power)
    end
end

-- Dawn sends the survivors running from the fire until they burn away.
function enemy:ignite()
    self.burnTime = 1.2 + math.random() * 1.2
end

function enemy:update(dt, neighbours)
    local x, y = self:position()
    if self.burnTime then
        self.burnTime = self.burnTime - dt
        local fire = self.game.campfire
        local dx, dy = x - fire.x, y - fire.y
        local length = math.max(1, math.sqrt(dx * dx + dy * dy))
        unit.move(self, dx / length * self.kind.speed * 1.3, dy / length * self.kind.speed * 1.3, dt)
        self.flash = 0.5 + 0.5 * math.sin(self.burnTime * 30)
        self.flashColor = 'FF8A3D'
        self:play('run')
        self.animator:update(dt)
        if self.burnTime <= 0 then
            self.game:killEnemy(self, true)
        end
        return
    end

    local tx, ty, target = self:goal()
    local range = distance(x, y, tx, ty)
    self.cooldown = self.cooldown - dt
    if target and range <= self.kind.range and self.cooldown <= 0 and not self:attacking() then
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
    local slow = self:attacking() and 0.2 or 1
    unit.move(self, velocity.x * slow, velocity.y * slow, dt)

    if target and self:attacking() then
        self.facing = tx > x and 1 or -1
    end
    if not self:attacking() then
        self:play(velocity:length() > 20 and 'run' or 'idle')
    end
    self:animate(dt)
end

return enemy
