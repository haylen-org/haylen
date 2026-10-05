-- The base of the tween tests: the replay action, R or the X button, and lanes where a box travels from 0 to 1000.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')

local TweenTest = haylen.class('TweenTest', Test)

TweenTest.replay = {name = 'replay', type = 'button', bindings = {'key:r', 'button:west'}}

-- Mounts the frame of the harness and loads the replay action with the `actions` of the options.
function TweenTest:frame(options)
    TweenTest.super.frame(self, options)
    local actions = {TweenTest.replay}
    for _, action in ipairs(options.actions or {}) do
        actions[#actions + 1] = action
    end
    self:loadActions({actions = actions})
end

-- Splits the area into `count` horizontal lanes and returns their rectangles, top to bottom, leaving room for labels on the left.
function TweenTest.lanes(area, count)
    local lanes = {}
    local height = area.height / count
    for index = 1, count do
        lanes[index] = {x = area.x + 240, y = area.y + (index - 1) * height, width = area.width - 300, height = height}
    end
    return lanes
end

-- Draws a lane with its label on the left and a square at `value`, where 0 and 1000 are the ends of the lane.
function TweenTest.drawLane(lane, label, value, color)
    local middle = lane.y + lane.height / 2
    graphics2d.drawText(nil, label, lane.x - 216, middle, {size = 28, color = Test.ink, anchor = {0, 0.5}})
    graphics2d.drawLine(lane.x, middle, lane.x + lane.width, middle, 2, Test.line)
    graphics2d.drawRect({lane.x - 2, middle - 12, 4, 24}, '#FF3A4058')
    graphics2d.drawRect({lane.x + lane.width - 2, middle - 12, 4, 24}, '#FF3A4058')
    local x = lane.x + lane.width * value / 1000
    graphics2d.drawRect({x - 22, middle - 22, 44, 44}, color or Test.accent, {layer = 1})
end

return TweenTest
