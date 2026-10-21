-- Time scale: the speed of one tween, of every tween with a tag and of the whole app, each on its own slider, opened with the enemies slowed down and one tween sped up.
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local TimeScale = haylen.class('TimeScale', TweenTest)

local kLanes = {
    {title = 'Enemy 1', tag = 'enemies', icon = 'heart', color = Test.red},
    {title = 'Enemy 2', tag = 'enemies', icon = 'heart', color = Test.red},
    {title = 'HUD', tag = 'hud', icon = 'star', color = Test.warm},
    {title = 'Own scale', tag = '', icon = 'gem', color = Test.accent},
    {title = 'Real time', tag = '', icon = 'coin', color = Test.green, unscaled = true},
}
local kEnemies, kOwn = 0.25, 2
local kCode = [[
tween.setTimeScale('enemies', 0.25)  -- Every tween tagged "enemies", even new ones.
handle.timeScale = 2  -- One tween.
haylen.setTimeScale(0.5)  -- The whole app, except tweens with "unscaled = true".]]

local function slider(id, label, value, onChange)
    return ui.formField{label = label, help = 'From 0 to 3 times the speed', ui.slider{id = id, min = 0, max = 3, value = value, step = 0.05, showValue = true, onChange = onChange}}
end

function TimeScale:enter()
    self.boxes, self.handles, self.trails, self.icons = {}, {}, {}, {}
    for index, lane in ipairs(kLanes) do
        self.boxes[index] = {x = 0}
        self.trails[index] = TweenTest.trail()
        self.icons[index] = TweenTest.icon(lane.icon)
        self.handles[index] = tween.to(self.boxes[index], 2, {x = 1000}, {owner = self, ease = 'sineInOut', loopMode = 'yoyo', repeatCount = -1, tag = lane.tag, unscaled = lane.unscaled})
    end
    tween.setTimeScale('enemies', kEnemies)
    self.handles[4].timeScale = kOwn
    self:frame({
        hint = 'The enemies share a tag, the own scale belongs to one tween and the last one runs on real time, whatever the app scale says.',
        code = kCode,
        controls = {
            slider('enemies', 'Tag "enemies"', kEnemies, function(event) tween.setTimeScale('enemies', event.value) end),
            slider('own', 'Own scale', kOwn, function(event) self.handles[4].timeScale = event.value end),
            slider('app', 'Whole app', 1, function(event) haylen.setTimeScale(event.value) end),
        },
        focus = 'enemies',
    })
end

function TimeScale:exit()
    TimeScale.super.exit(self)
    haylen.setTimeScale(1)
    tween.setTimeScale('enemies', 1)
end

-- The speed each lane runs at, from its own scale, its tag and the app.
function TimeScale:speed(index)
    local lane = kLanes[index]
    if lane.unscaled then
        return 'Real time, whatever the app scale'
    end
    local app = haylen.timeScale()
    if lane.tag ~= '' then
        return string.format('Tag "%s" %.2f, app %.2f', lane.tag, tween.timeScale(lane.tag), app)
    end
    return string.format('Its own %.2f, app %.2f', self.handles[index].timeScale, app)
end

function TimeScale:update(dt)
    TimeScale.super.update(self, dt)
    for index, box in ipairs(self.boxes) do
        self.trails[index]:push(box.x)
    end
    self:status(string.format('The enemies run at %.2f, the HUD at %.2f, the own scale at %.2f and the app at %.2f times the speed.', tween.timeScale('enemies'), tween.timeScale('hud'), self.handles[4].timeScale, haylen.timeScale()))
end

function TimeScale:draw(area)
    for index, lane in ipairs(TweenTest.lanes(area, #kLanes)) do
        local spec = kLanes[index]
        TweenTest.drawLane(lane, {title = spec.title, caption = self:speed(index), value = self.boxes[index].x, color = spec.color, icon = self.icons[index], trail = self.trails[index], ticks = {0, 500, 1000}, size = 44})
    end
end

return TimeScale
