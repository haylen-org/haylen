-- The automatic run of the headless platform: it takes the app through every step of its load, one step per interval, logs the frame numbers of each step and quits, so an unattended run checks every path of the app.
local debugging = require('haylen.debug')
local haylen = require('haylen')
local log = require('haylen.log')
local timer = require('haylen.timer')

local Tour = {}

Tour.interval = 1

-- Runs the functions of `steps` in order, one per interval, while `owner` lives. Each step returns the text that describes the load it set.
function Tour.start(owner, steps)
    local index, described = 0, nil
    timer.every(Tour.interval, function()
        if index > 0 then
            local frame = debugging.stats().frame
            log.info(string.format('Step %d of %d, %s: average frame %.2f ms, one percent low %.2f ms.', index, #steps, described, frame.average, frame.onePercentLow))
        end
        index = index + 1
        if index > #steps then
            log.info('The tour finished.')
            haylen.quit()
            return
        end
        described = steps[index]()
    end, {owner = owner})
end

return Tour
