-- A slime or a mushroom that sprouts from the ground, walks up to the hero and bumps into it.
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local tween = require('haylen.tween')

local config = require('config')

local enemy = {}
enemy.__index = enemy

function enemy.new(quest, kindName, x)
    local kind = config.enemies[kindName]
    local self = setmetatable({quest = quest, kind = kind, art = quest.art[kindName], x = x, facing = -1, health = kind.health, time = math.random(), cooldown = kind.interval / 2, grow = 0, push = 0, flash = 0}, enemy)
    tween.to(self, 0.4, {grow = 1}, {ease = 'backOut', owner = quest.owner})
    quest.effects:burst('dust', x, quest.lane.ground, 10)
    return self
end

function enemy:bounds()
    local kind = self.kind
    return m.rect(self.x + self.push - kind.width / 2, self.quest.lane.ground - kind.height, kind.width, kind.height)
end

function enemy:update(dt, hero, lane)
    self.time = self.time + dt
    self.cooldown = math.max(0, self.cooldown - dt)
    self.x = m.clamp(self.x, lane.left, lane.right)

    local distance = hero.x - self.x
    self.facing = distance >= 0 and 1 or -1
    if hero.resting then
        return
    end
    if math.abs(distance) > config.hero.reach * 0.8 then
        self.x = self.x + self.facing * self.kind.speed * dt
    elseif self.cooldown == 0 then
        self.cooldown = self.kind.interval
        tween.punch(self, 0.3, self.facing * 12, {field = 'push', vibrato = 2, overwrite = true, owner = self.quest.owner})
        self.quest:enemyStrikes(self)
    end
end

-- Takes a hit, flashes white and bounces back, and returns `true` when it has no health left.
function enemy:hit(damage)
    self.health = self.health - damage
    self.flash = 1
    tween.to(self, 0.25, {flash = 0}, {overwrite = true, owner = self.quest.owner})
    tween.punch(self, 0.3, -self.facing * 16, {field = 'push', vibrato = 3, overwrite = true, owner = self.quest.owner})
    return self.health <= 0
end

function enemy:draw()
    local animation = self.flash > 0.5 and self.art.hurt or self.art.move
    local scale = config.pixel * self.grow
    graphics2d.draw(self.art.texture, self.x + self.push, self.quest.lane.ground, {
        source = animation:frame(animation:frameAt(self.time)),
        pivotY = 1,
        scaleX = scale,
        scaleY = scale,
        flipHorizontal = self.facing < 0,
        flash = {1, 1, 1, self.flash * 0.8},
        layer = config.layer.enemies,
    })

    -- Enemies that were hurt show a small health bar.
    if self.health < self.kind.health then
        local bounds = self:bounds()
        local width = bounds.width * self.health / self.kind.health
        graphics2d.drawRect({bounds.x, bounds.y - 10, bounds.width, 6}, '#C01A1C2C', {layer = config.layer.enemies})
        graphics2d.drawRect({bounds.x + 1, bounds.y - 9, math.max(0, width - 2), 4}, '#FFE5534B', {layer = config.layer.enemies})
    end
end

return enemy
