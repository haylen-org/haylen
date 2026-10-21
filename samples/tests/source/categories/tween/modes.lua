-- The functions `to`, `from`, `by` and `fromTo`: four icons that take their start and end values in the four ways, replayed together every few seconds.
local haylen = require('haylen')
local input = require('haylen.input')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Modes = haylen.class('Modes', TweenTest)

local kLanes = {
    {title = 'To', caption = 'From where it is to 900', icon = 'coin', color = Test.warm},
    {title = 'From', caption = 'From 900 back to its place', icon = 'gem', color = Test.accent},
    {title = 'By', caption = '250 past where it is', icon = 'star', color = Test.green},
    {title = 'From and to', caption = 'From 100 to 700 every time', icon = 'heart', color = Test.red},
}
local kReplay = 2.4
local kCode = [[
tween.to(a, 1.2, {x = 900})  -- From where it is to 900.
tween.from(b, 1.2, {x = 900})  -- From 900 back to where it is.
tween.by(c, 1.2, {x = 250})  -- 250 further than where it is.
tween.fromTo(d, 1.2, {x = 100}, {x = 700})  -- From 100 to 700, wherever it is.]]

function Modes:enter()
    self.boxes = {{x = 100}, {x = 100}, {x = 100}, {x = 500}}
    self.trails, self.icons = {}, {}
    for index, lane in ipairs(kLanes) do
        self.trails[index] = TweenTest.trail()
        self.icons[index] = TweenTest.icon(lane.icon)
    end
    self:frame({
        hint = 'The tweens replay every few seconds, or now with the button, R or the X button. The "by" icon moves on from where it stopped.',
        code = kCode,
        controls = {ui.button{id = 'replay', text = 'Replay', variant = 'primary', onClick = function() self:play() end}},
        focus = 'replay',
    })
    self:play()
    timer.every(kReplay, function() self:play() end, {owner = self})
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
    self.ends = {{100, 900}, {900, 100}, {c.x, c.x + 250}, {100, 700}}

    local options = {owner = self, ease = 'quadInOut', delay = 0.2}
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
    for index, box in ipairs(self.boxes) do
        self.trails[index]:push(box.x)
    end
    local a, b, c, d = table.unpack(self.boxes)
    self:status(string.format('The icons stand at %.0f, %.0f, %.0f and %.0f.', a.x, b.x, c.x, d.x))
end

function Modes:draw(area)
    for index, lane in ipairs(TweenTest.lanes(area, #kLanes)) do
        local spec = kLanes[index]
        TweenTest.drawLane(lane, {title = spec.title, caption = spec.caption, value = self.boxes[index].x, from = self.ends[index][1], to = self.ends[index][2], color = spec.color, icon = self.icons[index], trail = self.trails[index], ticks = {0, 250, 500, 750, 1000}})
    end
end

return Modes
