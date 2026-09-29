-- To, from, by and fromTo: four boxes that take their start and end values in the four ways, replayed together.
local haylen = require('haylen')
local input = require('haylen.input')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Modes = haylen.class('Modes', sample.Test)

local kLabels = {'to', 'from', 'by', 'fromTo'}
local kCode = [[
tween.to(a, 1.2, {x = 900})  -- from where it is to 900
tween.from(b, 1.2, {x = 900})  -- from 900 back to where it is
tween.by(c, 1.2, {x = 250})  -- 250 further than where it is
tween.fromTo(d, 1.2, {x = 100}, {x = 700})  -- from 100 to 700, wherever it is]]

function Modes:enter()
    self.boxes = {{x = 100}, {x = 100}, {x = 100}, {x = 500}}
    self:frame({
        hint = 'Replay with the button, R or the west button. The by box moves on from where it stopped.',
        code = kCode,
        controls = {ui.button{id = 'replay', text = 'Replay', variant = 'primary', onClick = function() self:play() end}},
        focus = 'replay',
    })
    self:play()
end

function Modes:play()
    local a, b, c, d = table.unpack(self.boxes)
    for _, box in ipairs(self.boxes) do
        tween.killTarget(box)
    end
    a.x, b.x = 100, 100
    if c.x > 700 then
        c.x = 100
    end

    local options = {owner = self, ease = 'quad_in_out', delay = 0.2}
    tween.to(a, 1.2, {x = 900}, options)
    tween.from(b, 1.2, {x = 900}, options)
    tween.by(c, 1.2, {x = 250}, options)
    tween.fromTo(d, 1.2, {x = 100}, {x = 700}, options)
end

function Modes:update(dt)
    Modes.super.update(self, dt)
    if input.pressed('replay') then
        self:play()
    end
    local a, b, c, d = table.unpack(self.boxes)
    self:status(string.format('a.x %6.1f   b.x %6.1f   c.x %6.1f   d.x %6.1f', a.x, b.x, c.x, d.x))
end

function Modes:draw(stage)
    for index, lane in ipairs(sample.lanes(stage, 4)) do
        sample.drawLane(lane, kLabels[index], self.boxes[index].x)
    end
end

return Modes
