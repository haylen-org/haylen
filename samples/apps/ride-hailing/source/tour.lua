-- The automatic run of the headless platform: it searches a destination, compares the kinds of ride, requests one, follows the whole trip to the rating, opens the menu, the trips, a past trip, the wallet and the account, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Rides = require('rides')
local Shell = require('shell')

local Tour = {}

Tour.interval = 1.5

function Tour.steps()
    return {
        {'Map', function() end},
        {'Search a destination', function() Shell.open(require('screens.search')) end},
        {'Type a place', function() local page = Shell.top() page.query = 'museum' page.gui:set('query', {value = 'museum'}) page:refresh() end},
        {'Preview the route', function() Shell.top():pick(Shell.top().shown[1].id) end},
        {'Pick a ride', function() local page = Shell.top() page.gui:collection('kinds'):focus('roomy') page:pickKind('roomy') end},
        {'Request the ride', function() Shell.top():request() end},
        {'Find a driver', function() end},
        {'Driver on the way', function() Rides.current.elapsed = Rides.stageStart(Rides.current, 2) + 2 end},
        {'Message the driver', function() local page = Shell.top() page:openChat() page.chatSheet.screen:send('I am at the pickup point.') end},
        {'Open the safety tools', function() local page = Shell.top() page.chatSheet:dismiss() page:openSafety() end},
        {'Ride to the destination', function() Shell.top().safety:dismiss({result = 'close'}) Rides.current.elapsed = Rides.stageStart(Rides.current, 4) + 3 end},
        {'Arrive', function() Rides.current.elapsed = Rides.stageStart(Rides.current, 5) end},
        {'Rate the ride', function() local page = Shell.top() page.stars, page.tip = 5, 2 page.gui:set('tip', {selected = '2'}) end},
        {'Finish the ride', function() Shell.top():finish() end},
        {'Open the menu', function() Shell.openMenu() end},
        {'Open the trips', function() Shell.openPage(require('screens.activity')) end},
        {'Open a trip', function() Shell.open(require('screens.trip-detail'), {trip = Rides.past[3].id}) end},
        {'Open the wallet', function() Shell.openPage(require('screens.wallet')) end},
        {'Add funds', function() Shell.top():openFunds() end},
        {'Open the account', function() Shell.top().funds:dismiss({result = 20}) Shell.openPage(require('screens.account')) end},
        {'Back to the map', function() Shell.showHome() end},
        {'Ride to work', function() Shell.open(require('screens.preview'), {to = 'work'}) end},
        {'Back again', function() ui.goBack() end},
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
