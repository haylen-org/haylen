-- Timelines: a small cutscene built with `append`, `join`, a pause, callbacks, a label and inserts at a label and at a time, with a log of its steps and a bar of its time.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Timelines = haylen.class('Timelines', sample.Test)

local kLogLines = 9
local kCode = [[
tween.timeline({onStep = logStep, onComplete = logDone})
    :append(tween.to(door, 0.6, {open = 1})):append(tween.to(hero, 1.2, {x = 700})):join(tween.to(hero, 0.4, {alpha = 1}))
    :append(0.3):append(function() log('The hero waves') end):addLabel('fight'):append(tween.to(hero, 0.25, {hop = 1}, {loopMode = 'yoyo', repeatCount = 3}))
    :insert('fight', tween.to(sky, 1, {light = 0.3})):insert(1, function() log('The door creaks') end)]]

function Timelines:enter()
    self.lines = {}
    self:frame({
        hint = 'Restart, pause, reverse or jump to the "fight" label. Seeking never runs callbacks.',
        code = kCode,
        controls = {
            ui.button{id = 'restart', text = 'Restart', variant = 'primary', onClick = function() self:play() end},
            ui.row{gap = 12,
                ui.button{id = 'pause', text = 'Pause', grow = 1, onClick = function() self.line:pause() end},
                ui.button{id = 'resume', text = 'Resume', grow = 1, onClick = function() self.line:resume() end},
            },
            ui.row{gap = 12,
                ui.button{id = 'reverse', text = 'Reverse', grow = 1, onClick = function() self.line:reverse() end},
                ui.button{id = 'fight', text = 'Seek fight', grow = 1, onClick = function() self.line:seek('fight') end},
            },
            ui.sectionTitle{text = 'Log'},
            ui.label{id = 'log', text = '', font = 'caption'},
        },
        focus = 'restart',
    })
    self:play()
end

function Timelines:log(text)
    table.insert(self.lines, string.format('%.2f  %s', self.line.time, text))
    if #self.lines > kLogLines then
        table.remove(self.lines, 1)
    end
    self:set('log', {text = table.concat(self.lines, '\n')})
end

function Timelines:play()
    if self.line then
        self.line:kill()
    end
    self.door, self.hero, self.sky = {open = 0}, {x = 60, alpha = 0, hop = 0}, {light = 1}
    self.line = tween.timeline({owner = self, autoKill = false, onStep = function(step) self:log('Step ' .. step .. ' ended') end, onComplete = function() self:log('Complete') end})
    self.line:append(tween.to(self.door, 0.6, {open = 1}, {ease = 'quadOut'}))
        :append(tween.to(self.hero, 1.2, {x = 700}, {ease = 'sineInOut'}))
        :join(tween.to(self.hero, 0.4, {alpha = 1}))
        :append(0.3)
        :append(function() self:log('The hero waves') end)
        :addLabel('fight')
        :append(tween.to(self.hero, 0.25, {hop = 1}, {ease = 'quadOut', loopMode = 'yoyo', repeatCount = 3}))
        :insert('fight', tween.to(self.sky, 1, {light = 0.3}))
        :insert(1, function() self:log('The door creaks') end)
end

function Timelines:update(dt)
    Timelines.super.update(self, dt)
    if input.pressed('replay') then
        self:play()
    end
    local line = self.line
    self:status(string.format('time %.2f of %.2f   progress %.2f   %s%s', line.time, line.duration, line.progress, line.paused and 'paused' or line.completed and 'completed' or 'playing', line.reversed and ', reversed' or ''))
end

function Timelines:draw(area)
    local light = self.sky.light
    local ground = area.height - 150
    graphics2d.drawRect({0, 0, area.width, ground}, m.color(0.25 * light, 0.45 * light, 0.75 * light, 1))
    graphics2d.drawRect({0, ground, area.width, 50}, '#FF3A5A32')

    local doorX = 820
    graphics2d.drawRect({doorX - 10, ground - 210, 150, 210}, '#FF2A1E16')
    graphics2d.drawRect({doorX, ground - 200, 130 * (1 - self.door.open), 200}, '#FF8A5A30', {layer = 1})

    local hero = self.hero
    local color = m.color(sample.warm):withAlpha(hero.alpha)
    graphics2d.drawCircle(hero.x, ground - 40 - hero.hop * 80, 40, color, {layer = 2})

    -- The time bar under the scene, with a mark on the `fight` label.
    local bar = {40, area.height - 60, area.width - 80, 16}
    local line = self.line
    graphics2d.drawRect(bar, sample.line)
    graphics2d.drawRect({bar[1], bar[2], bar[3] * line.time / line.duration, bar[4]}, sample.accent, {layer = 1})
    local fight = bar[1] + bar[3] * 2.1 / line.duration
    graphics2d.drawRect({fight - 2, bar[2] - 12, 4, 40}, sample.warm, {layer = 2})
    graphics2d.drawText(nil, 'fight', fight + 8, bar[2] - 20, {size = 22, color = sample.warm})
end

return Timelines
