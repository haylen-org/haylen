-- The base of the audio tests: the sounds preload while the transition covers the screen, and a bar draws a level such as a volume or a playback position.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')

-- The sounds module defines the preload group that `load` waits for.
require('categories.audio.sounds')

local AudioTest = haylen.class('AudioTest', Test)

AudioTest.idle = '#FF3A4258'

function AudioTest:load(context)
    context:preload('audio'):await()
end

-- Draws a horizontal pill filled to `value` from 0 to 1, whose fill starts as a dot as wide as the bar is tall, so both keep round ends.
function AudioTest.bar(x, y, width, height, value, color)
    local radius = height / 2
    graphics2d.drawShape({x, y, width, height}, {radius = radius, color = AudioTest.idle, layer = 1})
    if value > 0 then
        graphics2d.drawShape({x, y, math.max(height, width * math.min(1, value)), height}, {radius = radius, color = color or Test.accent, layer = 2})
    end
end

return AudioTest
