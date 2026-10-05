-- Process modes: tweens in every process mode next to a tween on real time, with the game pause and the time scale on the panel. The test itself runs in the `always` mode, so it keeps answering while the game is paused.
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local ProcessModes = haylen.class('ProcessModes', TweenTest)
ProcessModes.processMode = 'always'

local kLanes = {
    {label = 'Mode\n"pausable"', options = {processMode = 'pausable'}},
    {label = 'Mode\n"whenPaused"', options = {processMode = 'whenPaused'}},
    {label = 'Mode\n"always"', options = {processMode = 'always'}},
    {label = 'Mode\n"disabled"', options = {processMode = 'disabled'}},
    {label = 'Mode\n"inherit"', options = {}},
    {label = 'Option\n"unscaled"', options = {processMode = 'always', unscaled = true}},
}
local kCode = [[
tween.to(box, 1.5, {x = 1000}, {processMode = 'whenPaused'})  -- One of `'pausable'`, `'whenPaused'`, `'always'`, `'disabled'` or `'inherit'`.
tween.to(box, 1.5, {x = 1000}, {owner = scene})  -- The mode `inherit` takes the mode of the owner, this scene runs `'always'`.
tween.to(box, 1.5, {x = 1000}, {unscaled = true})  -- Real time, whatever `haylen.setTimeScale` says.]]

function ProcessModes:enter()
    self.boxes = {}
    for index, lane in ipairs(kLanes) do
        self.boxes[index] = {x = 0}
        local options = {owner = self, ease = 'sineInOut', loopMode = 'yoyo', repeatCount = -1}
        for key, value in pairs(lane.options) do
            options[key] = value
        end
        tween.to(self.boxes[index], 1.5, {x = 1000}, options)
    end
    self:frame({
        hint = 'Pause the game and change the time scale. The "disabled" mode never runs, and the "inherit" lane follows this scene.',
        code = kCode,
        controls = {
            ui.toggle{id = 'pause', text = 'Pause the game', onChange = function(event) haylen.setPaused(event.checked) end},
            ui.formField{label = 'Time scale', ui.slider{id = 'scale', min = 0, max = 2, value = 1, step = 0.05, showValue = true, onChange = function(event) haylen.setTimeScale(event.value) end}},
        },
        focus = 'pause',
    })
end

function ProcessModes:exit()
    ProcessModes.super.exit(self)
    haylen.setPaused(false)
    haylen.setTimeScale(1)
end

function ProcessModes:update(dt)
    ProcessModes.super.update(self, dt)
    self:status(string.format('Game %s, time scale %.2f', haylen.paused() and 'paused' or 'running', haylen.timeScale()))
end

function ProcessModes:draw(area)
    for index, lane in ipairs(TweenTest.lanes(area, #kLanes)) do
        local color = haylen.paused() and Test.warm or Test.accent
        TweenTest.drawLane(lane, kLanes[index].label, self.boxes[index].x, color)
    end
end

return ProcessModes
