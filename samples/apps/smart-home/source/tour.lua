-- The automatic run of the headless platform: it goes through the scenes, dims a room, warms the climate, unlocks a door, arms the alarm and lets the day run, logs each step with what the panel shows and quits, so an unattended run checks every part of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local timer = require('haylen.timer')

local Tour = {}

Tour.interval = 2

function Tour.home()
    return scene.top().home
end

function Tour.report(name)
    local home = Tour.home()
    log.info(string.format('%s: %s, %d lights on, %.1f° toward %.1f°, solar %.1f kW, home %.1f kW, battery %.0f %%.', name, require('text').clock(home.minute * 60), home:lightsOn(), home.climate.current, home.climate.target, home:solar(), home:consumption(), home.battery * 100))
end

function Tour.steps()
    return {
        {'Morning', function() end},
        {'Movie scene', function() Tour.home():applyScene('movie') end},
        {'Dim the kitchen', function() local room = Tour.home():room('kitchen') room.on, room.level = true, 35 scene.top().panel:showRoom(room) end},
        {'Warm the climate', function() Tour.home().climate.target = 23 end},
        {'Unlock the patio', function() Tour.home().locks[3].locked = false scene.top().panel:showControls() end},
        {'Away scene', function() Tour.home():applyScene('away') end},
        {'Night scene', function() Tour.home():applyScene('night') end},
        {'Morning scene', function() Tour.home():applyScene('morning') end},
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
