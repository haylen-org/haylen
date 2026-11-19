-- The app of the orientation check of the web: it takes landscape from "haylen.json", the key P locks portrait and the key A lets the screen turn freely, and it counts its frames once a second, so `haylen test --target web` sees it run on under the turn screen of the page.
local input = require('haylen.input')
local log = require('haylen.log')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local window = require('haylen.window')

log.info('The orientation check is ready.')

local frames = 0
timer.every(1, function()
    log.info(string.format('The orientation check drew %d frames.', frames))
end)

scene.push({
    update = function()
        frames = frames + 1
        if input.keyPressed('p') then
            window.lockOrientation('portrait')
            log.info('The orientation check locked portrait.')
        elseif input.keyPressed('a') then
            window.lockOrientation('any')
            log.info('The orientation check lets the screen turn.')
        end
    end,
})
