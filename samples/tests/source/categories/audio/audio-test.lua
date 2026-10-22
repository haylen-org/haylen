-- The base of the audio tests: the sounds preload while the transition covers the screen, a bar draws a level such as a volume or a playback position, and a note says how the player lets the sound play while a browser keeps it locked.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')

-- The sounds module defines the preload group that `load` waits for.
require('categories.audio.sounds')

local AudioTest = haylen.class('AudioTest', Test)

AudioTest.idle = '#FF3A4258'
AudioTest.unlockHint = 'Click, tap or press a key to hear the sound'
AudioTest.noteStyle = {size = 20, color = Test.ink, anchor = {0, 0.5}, layer = 31}

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

-- Tells whether a browser holds the sound back until the first click, tap or key on the page.
function AudioTest.locked()
    return audio.outputState() == 'locked'
end

-- Draws a padlock `size` units tall centered on `x`, `y`, whose keyhole takes the color `hole`.
function AudioTest.padlock(x, y, size, color, hole, layer)
    graphics2d.drawArc(x, y - size * 0.12, size * 0.25, size * 0.11, math.pi, math.pi * 2, color, {layer = layer})
    graphics2d.drawShape({x - size * 0.36, y - size * 0.14, size * 0.72, size * 0.56}, {radius = size * 0.12, color = color, layer = layer})
    graphics2d.drawCircle(x, y + 0.12 * size, size * 0.08, hole, {layer = layer + 1})
end

-- While a browser keeps the sound locked, draws a pill with a padlock that says how the player lets it play, with its top right corner at `right`, `top`.
function AudioTest.lockNote(right, top)
    if not AudioTest.locked() then
        return
    end
    local width = graphics2d.measureText(nil, AudioTest.unlockHint, AudioTest.noteStyle) + 74
    local left = right - width
    graphics2d.drawShape({left, top, width, 44}, {radius = 22, color = '#F02A2618', borderWidth = 2, borderColor = Test.warm, layer = 30})
    AudioTest.padlock(left + 30, top + 21, 26, Test.warm, '#FF2A2618', 31)
    graphics2d.drawText(nil, AudioTest.unlockHint, left + 54, top + 22, AudioTest.noteStyle)
end

-- Writes a count with its noun, such as "1 voice" or "3 voices".
function AudioTest.count(count, one, many)
    return string.format('%d %s', count, count == 1 and one or many)
end

-- Writes seconds as minutes and seconds, such as "1:05".
function AudioTest.clock(seconds)
    seconds = math.max(0, math.floor(seconds))
    return string.format('%d:%02d', seconds // 60, seconds % 60)
end

return AudioTest
