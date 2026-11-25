-- The app of the canvas check of the web: its window in "haylen.json" is smaller than the page and not resizable, and it logs the size of its framebuffer whenever it changes, so `haylen test --target web` sees that the canvas fills the page and follows it.
local log = require('haylen.log')
local scene = require('haylen.scene')
local window = require('haylen.window')

log.info('The canvas check is ready.')

local width, height = 0, 0

scene.push({
    update = function()
        local currentWidth, currentHeight = window.framebufferSize()
        if currentWidth ~= width or currentHeight ~= height then
            width, height = currentWidth, currentHeight
            log.info(string.format('The canvas check draws %d by %d pixels.', width, height))
        end
    end,
})
