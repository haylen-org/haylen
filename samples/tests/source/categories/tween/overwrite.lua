-- Overwrite mode: a gem gets a new target every moment. With overwrite on, the new tween takes the fields over and the old one is killed. With it off, every tween keeps writing and they fight, which the lines to every target still pulling show.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Pointer = require('harness.pointer')
local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Overwrite = haylen.class('Overwrite', TweenTest)

local kTrail = 24
local kCode = [[
tween.to(ball, 1.6, {x = target.x, y = target.y}, {ease = 'sineInOut', overwrite = true})]]

function Overwrite:enter()
    self.ball = {x = 400, y = 300}
    self.targets = {}
    self.overwrite = true
    self.gem = TweenTest.icon('gem')
    self.trail = collections.newRingBuffer(kTrail)
    self.pointer = Pointer()
    self:frame({
        hint = 'Click or tap the stage to send the gem somewhere, or let the timer pick targets. Turn overwrite off to see the tweens fight.',
        code = kCode,
        controls = {
            ui.toggle{id = 'overwrite', text = 'Overwrite', checked = true, onChange = function(event) self.overwrite = event.checked end},
            ui.button{id = 'burst', text = 'Send three targets at once', onClick = function() self:burst() end},
            ui.label{id = 'pulling', text = ''},
        },
        focus = 'overwrite',
        actions = Pointer.actions,
    })
    timer.after(0.3, function() self:burst() end, {owner = self})
    timer.every(1.4, function() self:randomTarget() end, {owner = self})
end

function Overwrite:randomTarget()
    if self.area then
        self:send(100 + math.random() * (self.area.width - 200), 100 + math.random() * (self.area.height - 200))
    end
end

-- Three targets in quick succession, which overwrite turns into one move to the last and the fight into a pull between all three.
function Overwrite:burst()
    for index = 0, 2 do
        timer.after(index * 0.15, function() self:randomTarget() end, {owner = self})
    end
end

function Overwrite:send(x, y)
    local handle = tween.to(self.ball, 1.6, {x = x, y = y}, {owner = self, ease = 'sineInOut', overwrite = self.overwrite})
    table.insert(self.targets, {x = x, y = y, handle = handle})
end

function Overwrite:update(dt)
    Overwrite.super.update(self, dt)
    self.pointer:update(dt, self)
    if self.pointer.pressed then
        self:send(self.pointer.worldX, self.pointer.worldY)
    end
    for index = #self.targets, 1, -1 do
        if not self.targets[index].handle.alive then
            table.remove(self.targets, index)
        end
    end
    self.trail:push(m.vec2(self.ball.x, self.ball.y))
    local text = string.format('Tweens moving the gem: %d', #self.targets)
    if text ~= self.pullingText then
        self.pullingText = text
        self:set('pulling', {text = text})
    end
    self:status(self.overwrite and 'Overwrite is on, so only the newest tween moves the gem.' or 'Overwrite is off, so every tween still alive pulls the gem toward its target.')
end

function Overwrite:renderUi()
    self.pointer:draw()
end

function Overwrite:draw(area)
    local color = m.color(self.overwrite and Test.green or Test.red)
    for index, target in ipairs(self.targets) do
        local newest = index == #self.targets
        graphics2d.drawStroke({self.ball.x, self.ball.y, target.x, target.y}, {width = 3, color = color:withAlpha(newest and 0.8 or 0.45), cap = 'round', dashes = {12, 9}})
        graphics2d.drawShape({target.x - 22, target.y - 22, 44, 44}, {kind = 'ellipse', color = '#00000000', borderWidth = 5, borderColor = newest and Test.warm or '#FF8A7A55'})
        graphics2d.drawShape({target.x - 7, target.y - 7, 14, 14}, {kind = 'ellipse', color = newest and Test.warm or '#FF8A7A55'})
    end
    for index = 1, self.trail.size - 1 do
        local point = self.trail:get(index)
        local share = index / self.trail.size
        graphics2d.draw(self.gem, point.x, point.y, {width = 60 * share, height = 60 * share, color = m.color(1, 1, 1, share * 0.25), layer = 1})
    end
    graphics2d.drawShape({self.ball.x - 34, self.ball.y + 26, 68, 16}, {kind = 'ellipse', color = '#40000000', layer = 1})
    graphics2d.draw(self.gem, self.ball.x, self.ball.y, {width = 76, height = 76, layer = 2})
end

return Overwrite
