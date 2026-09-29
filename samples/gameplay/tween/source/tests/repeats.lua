-- Repeat modes: three boxes loop three extra times with a delay between loops, restarting, going back and forth, or moving on from where each loop ended.
local haylen = require('haylen')
local input = require('haylen.input')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Repeats = haylen.class('Repeats', sample.Test)

local kModes = {'restart', 'yoyo', 'incremental'}
local kCode = [[
tween.to(a, 0.8, {x = 700}, {repeatCount = 3, repeatDelay = 0.3, loop = 'restart'})
tween.to(b, 0.8, {x = 700}, {repeatCount = 3, repeatDelay = 0.3, loop = 'yoyo'})
tween.by(c, 0.8, {x = 200}, {repeatCount = 3, repeatDelay = 0.3, loop = 'incremental', onLoop = function(loop) print(loop) end})]]

function Repeats:enter()
    self:frame({
        hint = 'Every box plays four times in all. Replay with the button, R or the west button.',
        code = kCode,
        controls = {
            ui.button{id = 'replay', text = 'Replay', variant = 'primary', onClick = function() self:play() end},
            ui.label{id = 'loops', text = ''},
        },
        focus = 'replay',
    })
    self:play()
end

function Repeats:play()
    if self.boxes then
        for _, box in ipairs(self.boxes) do
            tween.killTarget(box)
        end
    end
    self.boxes = {{x = 100}, {x = 100}, {x = 100}}
    self.loops = {0, 0, 0}
    for index, mode in ipairs(kModes) do
        local options = {owner = self, ease = 'quad_in_out', repeatCount = 3, repeatDelay = 0.3, loop = mode, onLoop = function(loop)
            self.loops[index] = loop
        end}
        if mode == 'incremental' then
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
    self:set('loops', {text = string.format('Loops begun\nrestart %d\nyoyo %d\nincremental %d', self.loops[1], self.loops[2], self.loops[3])})
    self:status(string.format('restart %.0f   yoyo %.0f   incremental %.0f', self.boxes[1].x, self.boxes[2].x, self.boxes[3].x))
end

function Repeats:draw(area)
    for index, lane in ipairs(sample.lanes(area, 3)) do
        sample.drawLane(lane, kModes[index], self.boxes[index].x, index == 3 and sample.green or sample.accent)
    end
end

return Repeats
