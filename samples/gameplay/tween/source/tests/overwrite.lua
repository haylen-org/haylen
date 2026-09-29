-- Overwrite mode: a ball gets a new target every moment. With overwrite on, the new tween takes the fields over and the old one is killed. With it off, every tween keeps writing and they fight.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Overwrite = haylen.class('Overwrite', sample.Test)

local kCode = [[
tween.to(ball, 1.6, {x = target.x, y = target.y}, {ease = 'sineInOut', overwrite = true})]]

function Overwrite:enter()
    self.ball = {x = 400, y = 300}
    self.targets = {}
    self.overwrite = true
    self:frame({
        hint = 'Click or tap the stage to send the ball somewhere, or let the timer pick targets. Turn overwrite off to see the tweens fight.',
        code = kCode,
        controls = {ui.toggle{id = 'overwrite', text = 'Overwrite', checked = true, onChange = function(event) self.overwrite = event.checked end}},
        focus = 'overwrite',
    })
    timer.every(0.7, function() self:randomTarget() end, {owner = self})
end

function Overwrite:randomTarget()
    if self.area then
        self:send(80 + math.random() * (self.area.width - 160), 80 + math.random() * (self.area.height - 160))
    end
end

function Overwrite:send(x, y)
    local handle = tween.to(self.ball, 1.6, {x = x, y = y}, {owner = self, ease = 'sineInOut', overwrite = self.overwrite})
    table.insert(self.targets, {x = x, y = y, handle = handle})
end

function Overwrite:update(dt)
    Overwrite.super.update(self, dt)
    local x, y, pressed = sample.pointer()
    if pressed and self.area then
        local localX, localY = self:toStage(x, y)
        if self.area:contains({localX, localY}) then
            self:send(localX, localY)
        end
    end
    for index = #self.targets, 1, -1 do
        if not self.targets[index].handle.alive then
            table.remove(self.targets, index)
        end
    end
    self:status(string.format('overwrite %s   %d tweens move the ball', self.overwrite and 'on' or 'off', #self.targets))
end

function Overwrite:draw(area)
    for _, target in ipairs(self.targets) do
        graphics2d.drawLine(self.ball.x, self.ball.y, target.x, target.y, 2, '#664C7DFF')
        graphics2d.drawRing(target.x, target.y, 18, 3, sample.warm)
    end
    graphics2d.drawCircle(self.ball.x, self.ball.y, 34, self.overwrite and sample.green or sample.red, {layer = 1})
end

return Overwrite
