-- The survivor the player controls. Attacking next to a tree chops it, and anywhere else it fights with the move of the chosen class.
local input = require('haylen.input')

local config = require('config')
local sound = require('systems.sound')
local unit = require('entities.unit')

local player = setmetatable({}, {__index = unit})
player.__index = player

local chopReach = 120
-- A raider this close is fought first, even with a tree in reach.
local threatReach = 160

function player.new(game, class, x, y)
    local self = setmetatable({game = game, class = class}, player)
    local category = config.category
    unit.setup(self, game.island.world, class.unit, 'blue', x, y, category.player, category.world | category.enemy | category.tree)
    self.health = class.health
    self.maxHealth = class.health
    self.wood = 0
    self.cooldown = 0
    self.specialCooldown = 0
    self.sprintTime = 0
    self.chargeTime = 0
    self.stepTime = 0
    self.aim = {x = 1, y = 0}
    return self
end

function player:attacking()
    local current = self.animator.current
    return current and current:find('^attack') ~= nil and not self.animator.finished
end

-- Faces the closest enemy within reach, so attacks and arrows go where the danger is.
function player:aimAt(reach)
    local x, y = self:position()
    local target = self.game:closestEnemy(x, y, reach)
    if target then
        local tx, ty = target:position()
        local dx, dy = tx - x, ty - y
        local length = math.max(1, math.sqrt(dx * dx + dy * dy))
        self.aim = {x = dx / length, y = dy / length}
        if math.abs(dx) > 1 then
            self.facing = dx > 0 and 1 or -1
        end
    end
    return target
end

function player:attack()
    local x, y = self:position()
    local tree = not self.game:closestEnemy(x, y, threatReach) and self.game:treeInReach(x, y, chopReach, self.facing)
    self.cooldown = self.class.cooldown
    sound.play('swing', x, y)

    if tree then
        self:play('attack', true)
        self.game:chop(tree, self.class.chop)
        return
    end

    self:aimAt(self.class.range + 200)
    if self.class.unit == 'lancer' then
        local clip = 'attack'
        if math.abs(self.aim.y) > 0.7 then
            clip = self.aim.y < 0 and 'attackUp' or 'attackDown'
        end
        self:play(clip, true)
    elseif self.class.unit == 'warrior' then
        self:play(math.random() < 0.5 and 'attack' or 'attack2', true)
    else
        self:play('attack', true)
    end

    if self.class.unit == 'archer' then
        self.game:shoot(self, x, y - 40, self.aim.x, self.aim.y, self.class.damage, 1)
    elseif self.class.unit == 'monk' then
        self.game.effects:burst('heal', x, y - 30, 16)
        self.game:strike(x, y, self.class.range, nil, self.class.damage, 260)
    else
        self.game:strike(x, y, self.class.range, self.aim, self.class.damage, self.class.unit == 'lancer' and 520 or 300)
    end
end

function player:special()
    local x, y = self:position()
    local kind = self.class.special
    self.specialCooldown = self.class.specialCooldown
    if kind == 'volley' then
        self:aimAt(self.class.range)
        self:play('attack', true)
        local angle = math.atan(self.aim.y, self.aim.x)
        for offset = -1, 1 do
            self.game:shoot(self, x, y - 40, math.cos(angle + offset * 0.18), math.sin(angle + offset * 0.18), self.class.damage, 2)
        end
    elseif kind == 'charge' then
        self:aimAt(self.class.range + 300)
        self.chargeTime = 0.28
        self.chargeHits = {}
        self.knockback = {x = self.aim.x * 1100, y = self.aim.y * 1100}
        self:play('attack', true)
        sound.play('swing', x, y)
    elseif kind == 'heal' then
        self.health = math.min(self.maxHealth, self.health + self.maxHealth * 0.35)
        self.game.effects:burst('heal', x, y - 40, 40)
        self.game.effects:number(x, y - 60, '+' .. math.floor(self.maxHealth * 0.35), '#FF8CFF7A')
        self:play('attack', true)
        sound.play('heal', x, y)
    elseif kind == 'sprint' then
        self.sprintTime = 2.5
        self.game.effects:burst('dust', x, y, 10)
    end
end

function player:update(dt)
    local x, y = self:position()
    self.cooldown = math.max(0, self.cooldown - dt)
    self.specialCooldown = math.max(0, self.specialCooldown - dt)
    self.sprintTime = math.max(0, self.sprintTime - dt)
    self.guarding = self.class.special == 'guard' and input.down('special')

    local mx, my = input.vector('move')
    local length = math.sqrt(mx * mx + my * my)
    if length > 0.2 then
        self.aim = {x = mx / length, y = my / length}
    end
    local speed = self.class.speed * (self.sprintTime > 0 and 1.6 or 1) * (self.guarding and 0.35 or 1) * (self:attacking() and 0.55 or 1)
    unit.move(self, mx * speed, my * speed, dt)

    -- A charging lancer hurts everything it runs through, once per charge.
    if self.chargeTime > 0 then
        self.chargeTime = self.chargeTime - dt
        self.game:strike(x, y, 90, nil, self.class.damage * 0.6, 640, self.chargeHits)
    end

    if input.pressed('attack') and self.cooldown <= 0 then
        self:attack()
    elseif self.class.special ~= 'guard' and input.pressed('special') and self.specialCooldown <= 0 then
        self:special()
    end

    -- Walking up to the fire feeds it, and the interact action reaches it from further away.
    local reach = input.down('interact') and config.fire.feedDistance * 2 or config.fire.feedDistance
    if self.wood > 0 and self.game:nearFire(x, y, reach) then
        self.game:deliverWood(self)
    end
    self.game:collectWood(self)

    local moving = length > 0.2
    if not self:attacking() then
        if self.guarding then
            self:play('guard')
        elseif self.class.unit == 'pawn' and self.wood > 0 then
            self:play(moving and 'runCarry' or 'idleCarry')
        else
            self:play(moving and 'run' or 'idle')
        end
    end

    if moving then
        self.stepTime = self.stepTime - dt
        if self.stepTime <= 0 then
            self.stepTime = 0.32
            sound.play('footstep', x, y)
            self.game.effects:burst('dust', x, y, 2)
        end
    end
    self:animate(dt)
end

-- Takes a hit, which a raised guard mostly blocks, and returns `true` when the player falls.
function player:damage(amount, fromX, fromY)
    local x, y = self:position()
    if self.guarding then
        amount = amount * 0.3
        sound.play('guard', x, y)
        self.game.effects:burst('sparks', x, y - 50, 8)
    else
        sound.play('playerHurt', x, y)
    end
    self:push(fromX, fromY, 380)
    self.game.effects:number(x, y - 70, tostring(math.floor(amount + 0.5)), '#FFFF7A6B')
    self.game.camera:addTrauma(0.35)
    return self:hurt(amount)
end

return player
