-- Repeat modes: three icons play four times with a delay between the plays, restarting, going back and forth, or moving on from where each play ended, replayed every few seconds.
local haylen = require('haylen')
local input = require('haylen.input')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Repeats = haylen.class('Repeats', TweenTest)

local kLanes = {
    {mode = 'restart', title = 'Restart', caption = 'Every play starts again at 100', icon = 'coin', color = Test.warm, marks = {100, 700}},
    {mode = 'yoyo', title = 'Yoyo', caption = 'Every other play runs backwards', icon = 'gem', color = Test.accent, marks = {100, 700}},
    {mode = 'incremental', title = 'Incremental', caption = 'Every play goes 200 further', icon = 'star', color = Test.green, marks = {100, 300, 500, 700, 900}},
}
local kPlays = 4
local kReplay = 5.5
local kCode = [[
tween.to(a, 0.8, {x = 700}, {repeatCount = 3, repeatDelay = 0.3, loopMode = 'restart'})
tween.to(b, 0.8, {x = 700}, {repeatCount = 3, repeatDelay = 0.3, loopMode = 'yoyo'})
tween.by(c, 0.8, {x = 200}, {repeatCount = 3, repeatDelay = 0.3, loopMode = 'incremental', onLoop = function(loop) print(loop) end})]]

function Repeats:enter()
    self.trails, self.icons = {}, {}
    for index, lane in ipairs(kLanes) do
        self.trails[index] = TweenTest.trail()
        self.icons[index] = TweenTest.icon(lane.icon)
    end
    self:frame({
        hint = 'Every icon plays four times in all, and the three replay every few seconds, or now with the button, R or the X button.',
        code = kCode,
        controls = {
            ui.button{id = 'replay', text = 'Replay', variant = 'primary', onClick = function() self:play() end},
            ui.label{id = 'loops', text = ''},
        },
        focus = 'replay',
    })
    self:play()
    timer.every(kReplay, function() self:play() end, {owner = self})
end

function Repeats:play()
    if self.boxes then
        for _, box in ipairs(self.boxes) do
            tween.killTarget(box)
        end
    end
    self.boxes = {{x = 100}, {x = 100}, {x = 100}}
    self.plays = {1, 1, 1}
    for index, lane in ipairs(kLanes) do
        local options = {owner = self, ease = 'quadInOut', repeatCount = kPlays - 1, repeatDelay = 0.3, loopMode = lane.mode, onLoop = function(loop)
            self.plays[index] = loop + 1
        end}
        if lane.mode == 'incremental' then
            tween.by(self.boxes[index], 0.8, {x = 200}, options)
        else
            tween.to(self.boxes[index], 0.8, {x = 700}, options)
        end
    end
end

function Repeats:update(dt)
    Repeats.super.update(self, dt)
    if input.pressed('replay') then
        self:play()
    end
    local lines = {'Plays begun'}
    for index, lane in ipairs(kLanes) do
        self.trails[index]:push(self.boxes[index].x)
        lines[#lines + 1] = string.format('%s, play %d of %d', lane.title, self.plays[index], kPlays)
    end
    local text = table.concat(lines, '\n')
    if text ~= self.loopText then
        self.loopText = text
        self:set('loops', {text = text})
    end
    self:status(string.format('The icons stand at %.0f, %.0f and %.0f.', self.boxes[1].x, self.boxes[2].x, self.boxes[3].x))
end

function Repeats:draw(area)
    for index, lane in ipairs(TweenTest.lanes(area, #kLanes)) do
        local spec = kLanes[index]
        TweenTest.drawLane(lane, {title = spec.title, caption = spec.caption, value = self.boxes[index].x, marks = spec.marks, from = spec.marks[1], to = spec.marks[#spec.marks], color = spec.color, icon = self.icons[index], trail = self.trails[index], ticks = {0, 250, 500, 750, 1000}})
    end
end

return Repeats
