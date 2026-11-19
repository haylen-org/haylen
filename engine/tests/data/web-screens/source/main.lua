-- The app of the screens check of the web: it asks to see every screen once the space key is pressed, which the browser takes as an input of the person, and logs the answer while `haylen test --target web` sets what the browser answers.
local display = require('haylen.display')
local input = require('haylen.input')
local log = require('haylen.log')
local scene = require('haylen.scene')

local Check = {}

function Check:enter()
    log.info(string.format('The screens check is ready, and the app sees %d displays.', #display.list()))
end

function Check:update()
    if not input.keyPressed('space') then
        return
    end

    scene.spawn(self, function()
        local _, message, failure = display.requestEveryScreen():await()
        if failure then
            log.info(string.format('The screens check got the code "%s": %s', failure.code, message))
            return
        end

        log.info(string.format('The screens check sees every screen, %d displays.', #display.list()))
    end)
end

scene.push(Check)
