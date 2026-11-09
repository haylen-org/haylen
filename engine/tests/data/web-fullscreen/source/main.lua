-- The app of the fullscreen check of the web: the key F covers the screen, the key L leaves fullscreen and the key R asks to cover it and to leave it at once, while `haylen.py test --platform web` reads what the window reports when each promise settles.
local async = require('async')
local events = require('haylen.events')
local input = require('haylen.input')
local log = require('haylen.log')
local scene = require('haylen.scene')
local window = require('haylen.window')

log.info('The fullscreen check is ready.')

events.on('windowFullscreenChanged', function(info)
    log.info('The fullscreen check heard the window change to ' .. tostring(info.fullscreen) .. '.')
end)

-- Logs the outcome of a promise of the window together with the mode that the window reports at that moment.
local function report(name, promise)
    async.spawn(function()
        local _, message, failure = promise:await()
        if failure then
            log.info(string.format('The fullscreen check saw "%s" fail with "%s": %s', name, failure.code, message))
            return
        end
        log.info(string.format('The fullscreen check saw "%s" settle, and the window reports %s.', name, tostring(window.fullscreen())))
    end)
end

scene.push({
    update = function()
        if input.keyPressed('f') then
            report('enter', window.setFullscreen(true))
        elseif input.keyPressed('l') then
            report('leave', window.setFullscreen(false))
        elseif input.keyPressed('r') then
            report('replaced', window.setFullscreen(true))
            report('replacing', window.setFullscreen(false))
        end
    end,
})
