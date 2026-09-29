-- A coin that pops out of a defeated enemy, lands on the grass and then flies to the gold counter, at once when the player clicks it.
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local tween = require('haylen.tween')

local config = require('config')

local coin = {}
coin.__index = coin

local size = 10 * config.pixel

function coin.new(quest, x, y)
    local self = setmetatable({quest = quest, x = x, y = y, time = math.random(), flying = false, gone = false}, coin)
    local landing = {x + (math.random() - 0.5) * 100, quest.lane.ground}
    tween.jump(self, 0.55, landing, {
        power = 40 + math.random() * 40,
        owner = quest.owner,
        onComplete = function() self:fly(0.4 + math.random() * 0.4) end,
    })
    return self
end

function coin:bounds()
    return m.rect(self.x - size / 2, self.y - size, size, size)
end

-- Arcs up to the counter after the delay and hands the coin to the quest when it arrives.
function coin:fly(delay)
    self.flying = delay == 0
    local x, y = self.quest.hud:counter()
    local control = {(self.x + x) / 2, math.min(self.y, y) - 60}
    tween.bezier(self, 0.7, {control, {x, y + size / 2}}, {
        delay = delay,
        ease = 'sineIn',
        owner = self.quest.owner,
        onStart = function() self.flying = true end,
        onComplete = function() self.quest:collect(self) end,
    })
end

-- A click sends a coin that is still falling or resting to the counter at once.
function coin:grab()
    if self.flying then
        return
    end
    tween.killTarget(self)
    self.quest.effects:burst('sparks', self.x, self.y - size / 2, 8)
    self:fly(0)
end

function coin:update(dt)
    self.time = self.time + dt
end

function coin:draw()
    local spin = self.quest.art.coin.spin
    graphics2d.draw(self.quest.art.coin.texture, self.x, self.y, {source = spin:frame(spin:frameAt(self.time)), pivotY = 1, scaleX = config.pixel, scaleY = config.pixel, layer = config.layer.coins})
end

return coin
