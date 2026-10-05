-- Time scale: the speed of one tween, of every tween with a tag and of the whole app, each on its own slider.
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local TimeScale = haylen.class('TimeScale', TweenTest)

local kLanes = {
    {label = 'Enemy 1', tag = 'enemies'},
    {label = 'Enemy 2', tag = 'enemies'},
    {label = 'HUD', tag = 'hud'},
    {label = 'Own scale', tag = ''},
    {label = 'Unscaled', tag = '', unscaled = true},
}
local kCode = [[
tween.setTimeScale('enemies', 0.25)  -- Every tween tagged `enemies`, even new ones.
handle.timeScale = 2  -- One tween.
haylen.setTimeScale(0.5)  -- The whole app, except tweens with `unscaled = true`.]]

local function slider(id, label, onChange)
    return ui.formField{label = label, ui.slider{id = id, min = 0, max = 3, value = 1, step = 0.05, showValue = true, onChange = onChange}}
end

function TimeScale:enter()
    self.boxes, self.handles = {}, {}
    for index, lane in ipairs(kLanes) do
        self.boxes[index] = {x = 0}
        self.handles[index] = tween.to(self.boxes[index], 2, {x = 1000}, {owner = self, ease = 'sineInOut', loopMode = 'yoyo', repeatCount = -1, tag = lane.tag, unscaled = lane.unscaled})
    end
    self:frame({
        hint = 'The enemies share a tag, the fourth tween has its own scale and the last one runs on real time.',
        code = kCode,
        controls = {
            slider('enemies', 'Tag enemies', function(event) tween.setTimeScale('enemies', event.value) end),
            slider('own', 'Fourth tween', function(event) self.handles[4].timeScale = event.value end),
            slider('app', 'Whole app', function(event) haylen.setTimeScale(event.value) end),
        },
        focus = 'enemies',
    })
end

function TimeScale:exit()
    TimeScale.super.exit(self)
    haylen.setTimeScale(1)
    tween.setTimeScale('enemies', 1)
end

function TimeScale:update(dt)
    TimeScale.super.update(self, dt)
    self:status(string.format('Time scale of the enemies %.2f, HUD %.2f, fourth tween %.2f, app %.2f', tween.timeScale('enemies'), tween.timeScale('hud'), self.handles[4].timeScale, haylen.timeScale()))
end

function TimeScale:draw(area)
    for index, lane in ipairs(TweenTest.lanes(area, #kLanes)) do
        local color = kLanes[index].tag == 'enemies' and Test.red or kLanes[index].unscaled and Test.green or Test.accent
        TweenTest.drawLane(lane, kLanes[index].label, self.boxes[index].x, color)
    end
end

return TimeScale
