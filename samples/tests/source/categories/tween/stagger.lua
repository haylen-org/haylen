-- Stagger: the same hop and the same bar growth on a row of targets, each one starting a little after the one before, from the start, the end or the center, with a mark under the target the wave starts from.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Stagger = haylen.class('Stagger', TweenTest)

local kCount = 17
local kIcons = {'gem', 'coin', 'star', 'heart'}
local kColors = {'#FF2F86F6', '#FFFFC93C', '#FF2FB3C4', '#FFF0473E'}
local kCode = [[
tween.stagger(gems, 0.05, function(gem, index)
    return tween.to(gem, 0.35, {lift = 1}, {ease = 'quadOut', loopMode = 'yoyo', repeatCount = 1})
end, {origin = 'center'})]]

function Stagger:enter()
    self.dots, self.bars, self.icons = {}, {}, {}
    for index = 1, kCount do
        self.dots[index] = {lift = 0}
        self.bars[index] = {fill = 0.1}
    end
    for index, name in ipairs(kIcons) do
        self.icons[index] = TweenTest.icon(name)
    end
    self.origin = 'start'
    self:frame({
        hint = 'Start the wave from each place. It plays again every few seconds from the last place picked, and the arrow marks where it starts.',
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
    for _, id in ipairs({'start', 'end', 'center'}) do
        self:set(id, {variant = id == origin and 'primary' or 'default'})
    end
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
    self:status(string.format('The wave starts from the %s, with %d tweens playing.', self.origin, tween.size()))
end

function Stagger:draw(area)
    local step = (area.width - 160) / (kCount - 1)
    local iconY = area.height * 0.3
    local barTop, barBottom = area.height * 0.46, area.height - 40
    local origin = self.origin == 'start' and 1 or (self.origin == 'end' and kCount or (kCount + 1) // 2)
    for index = 1, kCount do
        local x = 80 + (index - 1) * step
        local kind = (index - 1) % #kIcons + 1
        local dot, bar = self.dots[index], self.bars[index]
        local lift = dot.lift * 110
        graphics2d.drawShape({x - 22, iconY + 30, 44, 12}, {kind = 'ellipse', color = '#40000000'})
        graphics2d.draw(self.icons[kind], x, iconY - lift, {width = 54, height = 54, layer = 1})

        local height = (barBottom - barTop) * bar.fill
        graphics2d.drawShape({x - 18, barTop, 36, barBottom - barTop}, {radius = 18, color = TweenTest.groove, borderWidth = 2, borderColor = TweenTest.grooveEdge})
        graphics2d.drawShape({x - 14, barBottom - 4 - math.max(28, height - 8), 28, math.max(28, height - 8)}, {radius = 14, color = kColors[kind], borderWidth = 3, borderColor = '#FF1E1B33', layer = 1})
        if index == origin then
            graphics2d.drawPolygon({{x, iconY + 52}, {x - 12, iconY + 72}, {x + 12, iconY + 72}}, Test.warm)
        end
    end
    graphics2d.drawText(nil, 'Each hop and each bar starts a moment after the one beside it.', area.width / 2, 24, {size = 22, color = Test.muted, anchor = {0.5, 0}})
end

return Stagger
