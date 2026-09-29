-- The hero walks along the lane, turns around at its ends and swings the sword at the nearest enemy within reach.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local timer = require('haylen.timer')
local tween = require('haylen.tween')

local config = require('config')

local hero = {}
hero.__index = hero

local greetings = {'Hey!', 'Onward!', 'Hi there!', 'Adventure!'}
local bodyWidth = 14 * config.pixel
local bodyHeight = 20 * config.pixel

function hero.new(quest, x)
    local self = setmetatable({quest = quest, x = x, y = quest.lane.ground, facing = 1, lift = 0, health = config.hero.health, cooldown = 0, resting = false}, hero)
    local art = quest.art.hero
    self.sprite = graphics2d.newSprite(art.texture, {scaleX = config.pixel, scaleY = config.pixel, layer = config.layer.hero})
    self.animator = animation2d.newAnimator()
    for _, name in ipairs({'idle', 'walk', 'attack', 'hurt'}) do
        self.animator:add(name, art[name])
    end
    self.animator.pivotY = 1

    -- The second frame of the swing is the one where the blade reaches forward.
    self.animator.onFrame = function(name, frame)
        if name == 'attack' and frame == 2 then
            quest:heroStrikes()
        end
    end
    self.animator:play('walk')
    return self
end

function hero:bounds()
    return m.rect(self.x - bodyWidth / 2, self.y + self.lift - bodyHeight, bodyWidth, bodyHeight)
end

function hero:attacking()
    return self.animator.current == 'attack' and not self.animator.finished
end

function hero:update(dt, lane)
    self.cooldown = math.max(0, self.cooldown - dt)
    self.y = lane.ground
    self.x = m.clamp(self.x, lane.left, lane.right)

    local target = self.quest:enemyInReach()
    if self.resting then
        self.animator:play('hurt')
    elseif target then
        self.facing = target.x >= self.x and 1 or -1
        if self.cooldown == 0 then
            self.cooldown = config.hero.attackInterval
            self.animator:play('attack', true)
        elseif not self:attacking() then
            self.animator:play('idle')
        end
    elseif not self:attacking() then
        self.x = self.x + self.facing * config.hero.speed * dt
        if self.x >= lane.right or self.x <= lane.left then
            self.x = m.clamp(self.x, lane.left, lane.right)
            self.facing = -self.facing
        end
        self.animator:play('walk')
    end

    -- The body stands 11 pixels into the 24 pixel frame, so the pivot follows it when the frame flips.
    self.animator.pivotX = self.facing > 0 and 11 / 24 or 13 / 24
    self.animator:update(dt)
    self.animator:apply(self.sprite)
end

function hero:hurt(damage)
    self.health = math.max(0, self.health - damage)
    self.sprite.flash = '#D0FF4040'
    tween.to(self.sprite, 0.3, {flash = '#00FF4040'}, {overwrite = true, owner = self.quest.owner})
    if self.health == 0 then
        self:rest()
    end
end

-- A defeated hero sits down for a moment and gets up again with full health.
function hero:rest()
    self.resting = true
    timer.after(config.hero.restTime, function()
        self.resting = false
        self:heal(config.hero.health)
    end, {owner = self.quest.owner})
end

function hero:heal(amount)
    local healed = math.min(config.hero.health, self.health + amount) - self.health
    self.health = self.health + healed
    self.quest.effects:burst('heal', self.x, self.y - 30, 16)
    self.quest.effects:text(self.x, self.y - 64, '+' .. healed, '#FFA7F070')
end

-- A click on the hero makes it hop and say hello.
function hero:cheer()
    if self.resting then
        self.quest.effects:text(self.x, self.y - 64, 'Zzz', '#FFC0D8EC')
        return
    end
    tween.to(self, 0.16, {lift = -24}, {ease = 'quad_out', repeatCount = 1, loop = 'yoyo', overwrite = true, owner = self.quest.owner})
    self.quest.effects:text(self.x, self.y - 70, greetings[math.random(#greetings)], '#FFFFFFFF')
end

function hero:draw()
    local sprite = self.sprite
    sprite.x = self.x
    sprite.y = self.y + self.lift
    sprite.flipX = self.facing < 0
    sprite.color = self.resting and '#99FFFFFF' or '#FFFFFFFF'
    sprite:draw()
end

return hero
