-- Stagger: the same hop and the same bar growth on a row of targets, each one starting a little after the one before, from the start, the end or the center.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local TweenTest = require('categories.tween.tween-test')

local Stagger = haylen.class('Stagger', TweenTest)

local kCount = 17
local kCode = [[
tween.stagger(dots, 0.05, function(dot, index)
    return tween.to(dot, 0.35, {lift = 1}, {ease = 'quadOut', loopMode = 'yoyo', repeatCount = 1})
end, {origin = 'center'})]]

function Stagger:enter()
    self.dots, self.bars = {}, {}
    for index = 1, kCount do
        self.dots[index] = {lift = 0}
        self.bars[index] = {fill = 0.1}
    end
    self.origin = 'start'
    self:frame({
        hint = 'Start the wave from each place. It plays again every few seconds from the last place picked.',
        code = kCode,
        controls = {
            ui.button{id = 'start', text = 'From the start', variant = 'primary', onClick = function() self:play('start') end},
            ui.button{id = 'end', text = 'From the end', onClick = function() self:play('end') end},
            ui.button{id = 'center', text = 'From the center', onClick = function() self:play('center') end},
        },
        focus = 'start',
    })
    self:play('start')
    timer.every(2.5, function() self:play(self.origin) end, {owner = self})
end

function Stagger:play(origin)
    self.origin = origin
    tween.killTag('wave')
    tween.stagger(self.dots, 0.05, function(dot)
        return tween.to(dot, 0.35, {lift = 1}, {ease = 'quadOut', loopMode = 'yoyo', repeatCount = 1})
    end, {origin = origin, owner = self, tag = 'wave'})
    tween.stagger(self.bars, 0.04, function(bar)
        return tween.fromTo(bar, 0.6, {fill = 0.1}, {fill = 1}, {ease = 'backOut'})
    end, {origin = origin, owner = self, tag = 'wave', delay = 0.2})
end

function Stagger:update(dt)
    Stagger.super.update(self, dt)
    self:status(string.format('Origin "%s", tweens playing %d', self.origin, tween.size()))
end

function Stagger:draw(area)
    local step = (area.width - 160) / (kCount - 1)
    for index = 1, kCount do
        local x = 80 + (index - 1) * step
        local dot, bar = self.dots[index], self.bars[index]
        local color = m.fromHsv(index / kCount * 0.7, 0.6, 1)
        graphics2d.drawCircle(x, area.height * 0.32 - dot.lift * 120, 22, color)
        local height = (area.height * 0.38) * bar.fill
        graphics2d.drawRect({x - 18, area.height - 40 - height, 36, height}, color)
    end
end

return Stagger
