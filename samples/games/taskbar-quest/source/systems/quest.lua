-- The rules of the quest: enemies appear ahead of the hero, both sides trade blows, defeated enemies drop coins and the gold buys a sharper sword and potions.
local m = require('haylen.math')

local coin = require('entities.coin')
local config = require('config')
local enemy = require('entities.enemy')
local hero = require('entities.hero')

local quest = {}
quest.__index = quest

local function sweep(list)
    for index = #list, 1, -1 do
        if list[index].gone then
            table.remove(list, index)
        end
    end
end

-- The owner is the scene, so every timer and tween of the quest ends with it.
function quest.new(owner, art, effects, hud, lane)
    local self = setmetatable({owner = owner, art = art, effects = effects, hud = hud, lane = lane, enemies = {}, coins = {}, gold = 0, defeated = 0, sword = 1, spawnTime = config.spawn.firstDelay}, quest)
    self.hero = hero.new(self, (lane.left + lane.right) / 2)
    return self
end

function quest:swordCost()
    return config.shop.sword * self.sword
end

function quest:canDrinkPotion()
    return self.gold >= config.shop.potion and self.hero.health < config.hero.health and not self.hero.resting
end

function quest:upgradeSword()
    local cost = self:swordCost()
    if self.gold < cost then
        return
    end
    self.gold = self.gold - cost
    self.sword = self.sword + 1
    self.effects:burst('sparks', self.hero.x, self.hero.y - 40, 20)
    self.effects:text(self.hero.x, self.hero.y - 70, 'Sword ' .. self.sword .. '!', '#FFFFCD75')
    self.hud:pulse('level')
end

function quest:drinkPotion()
    if not self:canDrinkPotion() then
        return
    end
    self.gold = self.gold - config.shop.potion
    self.hero:heal(config.shop.heal)
end

-- Returns the nearest living enemy the sword reaches, on either side of the hero.
function quest:enemyInReach()
    local nearest, best = nil, config.hero.reach
    for _, candidate in ipairs(self.enemies) do
        local distance = math.abs(candidate.x - self.hero.x)
        if not candidate.gone and distance <= best then
            nearest, best = candidate, distance
        end
    end
    return nearest
end

-- Returns what the player points at, as the thing and its kind: `enemy`, `coin` or `hero`.
function quest:targetAt(x, y)
    local point = {x, y}
    for _, candidate in ipairs(self.enemies) do
        if candidate:bounds():expanded(8):contains(point) then
            return candidate, 'enemy'
        end
    end
    for _, candidate in ipairs(self.coins) do
        if candidate:bounds():expanded(8):contains(point) then
            return candidate, 'coin'
        end
    end
    if self.hero:bounds():contains(point) then
        return self.hero, 'hero'
    end
    return nil
end

function quest:strikeAt(x, y)
    local target, kind = self:targetAt(x, y)
    if kind == 'enemy' then
        self.effects:burst('sparks', x, y, 10)
        self:damage(target, self.sword)
    elseif kind == 'coin' then
        target:grab()
    elseif kind == 'hero' then
        target:cheer()
    end
end

function quest:heroStrikes()
    local target = self:enemyInReach()
    if target == nil then
        return
    end
    self.effects:burst('sparks', (target.x + self.hero.x) / 2, self.lane.ground - 30, 8)
    self:damage(target, self.sword)
end

function quest:damage(target, amount)
    self.effects:text(target.x, self.lane.ground - target.kind.height - 12, '-' .. amount, '#FFFFFFFF')
    if not target:hit(amount) then
        return
    end
    target.gone = true
    self.defeated = self.defeated + 1
    self.effects:burst(target.kind.burst, target.x, self.lane.ground - 16, 24)
    for _ = 1, target.kind.coins do
        self.coins[#self.coins + 1] = coin.new(self, target.x, self.lane.ground - 20)
    end
end

function quest:enemyStrikes(attacker)
    local hero = self.hero
    hero:hurt(attacker.kind.damage)
    self.effects:text(hero.x, hero.y - 64, '-' .. attacker.kind.damage, '#FFFF6B64')
    if not hero.resting then
        return
    end

    -- The hero sits down and the enemies around vanish in a puff, so the next wave starts fresh.
    self.effects:text(hero.x, hero.y - 90, 'Ouch!', '#FFFFCD75')
    for _, other in ipairs(self.enemies) do
        other.gone = true
        self.effects:burst('dust', other.x, self.lane.ground - 12, 16)
    end
end

function quest:collect(arrived)
    self.gold = self.gold + 1
    self.hud:pulse('gold')
    arrived.gone = true
end

function quest:spawn(dt)
    local hero = self.hero
    self.spawnTime = self.spawnTime - dt
    if hero.resting or self.spawnTime > 0 or #self.enemies >= config.spawn.alive then
        return
    end

    local spawn = config.spawn
    self.spawnTime = spawn.minDelay + math.random() * (spawn.maxDelay - spawn.minDelay)
    local kind = (self.defeated >= spawn.mushroomFrom and math.random() < spawn.mushroomChance) and 'mushroom' or 'slime'
    local distance = spawn.minAhead + math.random() * (spawn.maxAhead - spawn.minAhead)
    local x = hero.x + hero.facing * distance
    if x < self.lane.left or x > self.lane.right then
        x = hero.x - hero.facing * distance
    end
    self.enemies[#self.enemies + 1] = enemy.new(self, kind, m.clamp(x, self.lane.left, self.lane.right))
end

function quest:update(dt, lane)
    self.lane = lane
    self:spawn(dt)
    self.hero:update(dt, lane)
    for _, other in ipairs(self.enemies) do
        if not other.gone then
            other:update(dt, self.hero, lane)
        end
    end
    for _, item in ipairs(self.coins) do
        item:update(dt)
    end
    sweep(self.enemies)
    sweep(self.coins)
end

-- Adds the hero, the enemies and the coins to the regions that keep the mouse.
function quest:collectRegions(regions)
    regions[#regions + 1] = self.hero:bounds()
    for _, other in ipairs(self.enemies) do
        regions[#regions + 1] = other:bounds():expanded(8)
    end
    for _, item in ipairs(self.coins) do
        regions[#regions + 1] = item:bounds():expanded(8)
    end
end

function quest:draw()
    for _, other in ipairs(self.enemies) do
        other:draw()
    end
    for _, item in ipairs(self.coins) do
        item:draw()
    end
    self.hero:draw()
end

return quest
