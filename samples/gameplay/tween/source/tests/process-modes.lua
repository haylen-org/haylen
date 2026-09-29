-- Process modes: tweens in every process mode next to a tween on real time, with the game pause and the time scale on the panel. The test itself runs in the always mode, so it keeps answering while the game is paused.
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local ProcessModes = haylen.class('ProcessModes', sample.Test)
ProcessModes.processMode = 'always'

local kLanes = {
    {label = 'pausable', options = {processMode = 'pausable'}},
    {label = 'whenPaused', options = {processMode = 'whenPaused'}},
    {label = 'always', options = {processMode = 'always'}},
    {label = 'disabled', options = {processMode = 'disabled'}},
    {label = 'inherit', options = {}},
    {label = 'unscaled', options = {processMode = 'always', unscaled = true}},
}
local kCode = [[
tween.to(box, 1.5, {x = 1000}, {processMode = 'whenPaused'})  -- 'pausable', 'whenPaused', 'always', 'disabled' or 'inherit'
tween.to(box, 1.5, {x = 1000}, {owner = scene})  -- inherit takes the mode of the owner, this scene runs 'always'
tween.to(box, 1.5, {x = 1000}, {unscaled = true})  -- real time, whatever haylen.setTimeScale says]]

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
        hint = 'Pause the game and change the time scale. Disabled never runs, and the inherit lane follows this scene.',
        code = kCode,
        controls = {
            ui.toggle{id = 'pause', text = 'Pause the game', onChange = function(event) haylen.setPaused(event.checked) end},
            ui.formField{label = 'Time scale', ui.slider{id = 'scale', min = 0, max = 2, value = 1, step = 0.05, showValue = true, onChange = function(event) haylen.setTimeScale(event.value) end}},
        },
        focus = 'pause',
    })
end

function ProcessModes:exit()
    haylen.setPaused(false)
    haylen.setTimeScale(1)
end

function ProcessModes:update(dt)
    ProcessModes.super.update(self, dt)
    self:status(string.format('paused %s   time scale %.2f', haylen.paused(), haylen.timeScale()))
end

function ProcessModes:draw(area)
    for index, lane in ipairs(sample.lanes(area, #kLanes)) do
        local color = haylen.paused() and sample.warm or sample.accent
        sample.drawLane(lane, kLanes[index].label, self.boxes[index].x, color)
    end
end

return ProcessModes
