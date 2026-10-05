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

-- Draws a horizontal bar filled to `value` from 0 to 1.
function AudioTest.bar(x, y, width, height, value, color)
    graphics2d.drawRect({x, y, width, height}, AudioTest.idle, {layer = 1})
    graphics2d.drawRect({x, y, width * math.max(0, math.min(1, value)), height}, color or Test.accent, {layer = 2})
end

return AudioTest
