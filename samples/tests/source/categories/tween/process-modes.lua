-- Process modes: tweens in every process mode next to a tween on real time, with the game pause and the time scale on the panel. The test opens with the game paused, so every lane shows what its mode does, and it runs in the `always` mode itself, so it keeps answering while the game is paused.
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local ProcessModes = haylen.class('ProcessModes', TweenTest)
ProcessModes.processMode = 'always'

local kLanes = {
    {title = 'Pausable', caption = 'Stops while the game is paused', icon = 'coin', options = {processMode = 'pausable'}, runs = function(paused) return not paused end},
    {title = 'When paused', caption = 'Runs only while the game is paused', icon = 'gem', options = {processMode = 'whenPaused'}, runs = function(paused) return paused end},
    {title = 'Always', caption = 'Runs paused or not', icon = 'star', options = {processMode = 'always'}, runs = function() return true end},
    {title = 'Disabled', caption = 'Never runs', icon = 'shield', options = {processMode = 'disabled'}, runs = function() return false end},
    {title = 'Inherit', caption = 'Follows this scene, which always runs', icon = 'heart', options = {}, runs = function() return true end},
    {title = 'Unscaled', caption = 'Real time, whatever the time scale', icon = 'potion', options = {processMode = 'always', unscaled = true}, runs = function() return true end},
}
local kCode = [[
tween.to(box, 1.5, {x = 1000}, {processMode = 'whenPaused'})  -- Or "pausable", "always", "disabled" or "inherit".
tween.to(box, 1.5, {x = 1000}, {owner = scene})  -- The mode "inherit" takes the mode of the owner, and this scene runs "always".
tween.to(box, 1.5, {x = 1000}, {unscaled = true})  -- Real time, whatever "haylen.setTimeScale" says.
haylen.setPaused(true)]]

function ProcessModes:enter()
    self.boxes, self.trails, self.icons = {}, {}, {}
    for index, lane in ipairs(kLanes) do
        self.boxes[index] = {x = 0}
        self.trails[index] = TweenTest.trail()
        self.icons[index] = TweenTest.icon(lane.icon)
        local options = {owner = self, ease = 'sineInOut', loopMode = 'yoyo', repeatCount = -1}
        for key, value in pairs(lane.options) do
            options[key] = value
        end
        tween.to(self.boxes[index], 1.5, {x = 1000}, options)
    end
    haylen.setPaused(true)
    self:frame({
        hint = 'The game opens paused. Let it run and change the time scale: the "disabled" lane never moves, and the "inherit" lane follows this scene.',
        code = kCode,
        controls = {
            ui.toggle{id = 'pause', text = 'Pause the game', checked = true, onChange = function(event) haylen.setPaused(event.checked) end},
            ui.formField{label = 'Time scale', help = 'From 0 to 2 times the speed', ui.slider{id = 'scale', min = 0, max = 2, value = 1, step = 0.05, showValue = true, onChange = function(event) haylen.setTimeScale(event.value) end}},
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
    for index, box in ipairs(self.boxes) do
        self.trails[index]:push(box.x)
    end
    self:status(string.format('The game is %s at %.2f times the speed.', haylen.paused() and 'paused' or 'running', haylen.timeScale()))
end

function ProcessModes:draw(area)
    local paused = haylen.paused()
    for index, lane in ipairs(TweenTest.lanes(area, #kLanes)) do
        local spec = kLanes[index]
        local runs = spec.runs(paused)
        TweenTest.drawLane(lane, {title = spec.title, caption = spec.caption, value = self.boxes[index].x, color = runs and Test.green or Test.muted, icon = self.icons[index], trail = runs and self.trails[index] or nil, alpha = runs and 1 or 0.4, ticks = {0, 500, 1000}, size = 44})
    end
end

return ProcessModes
