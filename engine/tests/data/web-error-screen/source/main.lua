-- The app of the error screen check of the web: it clears the canvas to magenta and stops with an error once the key E is pressed, while `haylen.py test --platform web` copies the report of the error screen and restarts the app from it.
local input = require('haylen.input')
local log = require('haylen.log')
local scene = require('haylen.scene')

log.info('The error screen check is ready.')

scene.push({
    update = function()
        if input.keyPressed('e') then
            error('The error screen check stops the app.')
        end
    end,
})
