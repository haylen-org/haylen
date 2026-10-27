-- The app of the wake lock check of the web: it keeps the screen on from its start, as its app.json asks, and lets the screen sleep once the space key is pressed, while `haylen.py test --platform web` watches the wake lock of the page.
local input = require('haylen.input')
local log = require('haylen.log')
local scene = require('haylen.scene')
local window = require('haylen.window')

log.info('The wake lock check is ready, and the app keeps the screen on: ' .. tostring(window.keepScreenOn()) .. '.')

scene.push({
    update = function()
        if input.keyPressed('space') and window.keepScreenOn() then
            window.setKeepScreenOn(false)
            log.info('The wake lock check lets the screen sleep.')
        end
    end,
})
