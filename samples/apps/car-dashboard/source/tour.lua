-- The automatic run of the headless platform: it drives for a while, warms the cabin, heats the seat of the passenger, goes through the apps of the dock, logs each step with what the dashboard shows and quits, so an unattended run checks every part of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local timer = require('haylen.timer')

local Tour = {}

Tour.interval = 2

function Tour.controls()
    return scene.top().controls
end

function Tour.report(name)
    local drive = scene.top().drive
    log.info(string.format('%s: %.0f km/h, %.0f m to the turn, battery %.1f %%, %.1f kWh/100 km.', name, drive.speed * 3.6, drive:distanceToTurn(), drive:charge() * 100, drive.consumption))
end

function Tour.steps()
    return {
        {'Drive', function() end},
        {'Warm the driver', function() Tour.controls():change(Tour.controls().zones[1], 1) end},
        {'Heat the passenger seat', function() Tour.controls():heatSeat(Tour.controls().zones[2]) end},
        {'Open the navigation', function() Tour.controls():select('navigation') end},
        {'Open the music', function() Tour.controls():select('music') end},
        {'Open the phone', function() Tour.controls():select('phone') end},
        {'Back home', function() Tour.controls():select('home') end},
        {'Keep driving', function() end},
    }
end

function Tour.start()
    local steps = Tour.steps()
    local index = 0
    timer.every(Tour.interval, function()
        index = index + 1
        if index > #steps then
            log.info('The tour finished.')
            haylen.quit()
            return
        end
        steps[index][2]()
        Tour.report(string.format('Step %d of %d, %s', index, #steps, steps[index][1]))
    end, {owner = Tour})
end

return Tour
