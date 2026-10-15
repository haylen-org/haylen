-- The automatic run of the headless platform: it searches a destination, compares the kinds of ride, requests one, follows the whole trip to the rating, opens the activity, a trip, the wallet and the account in both themes, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Rides = require('rides')
local Shell = require('shell')

local Tour = {}

Tour.interval = 1.5

function Tour.steps()
    return {
        {'Ride', function() end},
        {'Search a destination', function() Shell.open(require('screens.search')) end},
        {'Type a place', function() local page = scene.top() page.query = 'museum' page.gui:set('query', {value = 'museum'}) page:refresh() end},
        {'Preview the route', function() scene.top():pick(scene.top().shown[1].id) end},
        {'Pick a ride', function() local page = scene.top() page.gui:collection('kinds'):focus('comfort') page:pickKind('comfort') end},
        {'Request the ride', function() scene.top():request() end},
        {'Find a driver', function() end},
        {'Driver on the way', function() Rides.current.elapsed = Rides.stageStart(Rides.current, 2) + 2 end},
        {'Chat with the driver', function() local page = scene.top() page.gui:set('chatDialog', {open = true}) page:sendChat('I am at the pickup point.') end},
        {'Open the safety tools', function() local page = scene.top() page.gui:set('chatDialog', {open = false}) page.gui:set('safetyDialog', {open = true}) end},
        {'Ride to the destination', function() scene.top().gui:set('safetyDialog', {open = false}) Rides.current.elapsed = Rides.stageStart(Rides.current, 4) + 3 end},
        {'Arrive', function() Rides.current.elapsed = Rides.stageStart(Rides.current, 5) end},
        {'Rate the ride', function() local page = scene.top() page.stars, page.tip = 5, 2 page.gui:set('tip', {selected = '2'}) end},
        {'Finish the ride', function() scene.top():finish() end},
        {'Open the activity', function() Shell.showTab('activity') end},
        {'Open a trip', function() Shell.open(require('screens.trip-detail'), {trip = Rides.past[3].id}) end},
        {'Open the wallet', function() Shell.showTab('wallet') end},
        {'Add funds', function() local page = scene.top() page.gui:set('fundsDialog', {open = true}) end},
        {'Open the account', function() scene.top().gui:set('fundsDialog', {open = false}) Rides.addFunds(20) Shell.showTab('account') end},
        {'Turn on the day theme', function() ui.setTheme('rideDay') Shell.showTab('home') end},
        {'Preview in the day theme', function() Shell.open(require('screens.preview'), {to = 'work'}) end},
        {'Go back to the night', function() ui.setTheme('rideNight') Shell.showTab('home') end},
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
        log.info(string.format('Step %d of %d: %s.', index, #steps, steps[index][1]))
        steps[index][2]()
    end, {owner = Tour})
end

return Tour
