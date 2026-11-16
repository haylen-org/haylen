-- The automatic run of the headless platform: it searches a destination, compares the kinds of ride, requests one, follows the whole trip to the rating with its details in a sheet over the map, opens the menu, the trips, a past trip, the wallet and the account, opens links of the scheme and of the web, and requests a second ride that it cancels after the reasons and the alert, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Rides = require('rides')
local Routes = require('routes')
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
        {'Open the details of the ride', function() Shell.top():openDetails() end},
        {'Pull the details up', function() Shell.top().details.detent = 2 end},
        {'Arrive', function() Rides.current.elapsed = Rides.stageStart(Rides.current, 5) end},
        {'Rate the ride', function() local page = Shell.top() page.stars, page.tip = 5, 2 page.gui:set('tip', {selected = '2'}) end},
        {'Finish the ride', function() Shell.top():finish() end},
        {'Open the menu', function() Shell.openMenu() end},
        {'Open the trips', function() Shell.openPage('trips') end},
        {'Open a trip', function() Shell.router:push(Routes.trip(Rides.past[3])) end},
        {'Open the wallet', function() Shell.openPage('wallet') end},
        {'Add funds', function() Shell.top():openFunds() end},
        {'Open the account', function() Shell.top().funds:dismiss({result = 20}) Shell.openPage('account') end},
        {'Back to the map', function() Shell.showHome() end},
        {'Open a link to a trip', function() Shell.router:open(Routes.link(Routes.trip(Rides.past[7]))) end},
        {'Open a web link to the wallet', function() Shell.router:open(Routes.webLink('wallet')) end},
        {'Open a link to a trip the rider never took', function() Shell.router:open(Routes.link('trip/99999')) end},
        {'Open a link to the map', function() Shell.router:open(Routes.link('map')) end},
        {'Ride to work', function() Shell.open(require('screens.preview'), {to = 'work'}) end},
        {'Request it', function() Shell.top():request() end},
        {'Ask to cancel', function() Shell.top():askCancel() end},
        {'Tell why', function() Shell.top().cancelSheet:dismiss({result = 'plans'}) end},
        {'Cancel the ride', function() Shell.top().cancelAlert:dismiss({result = 'cancel'}) end},
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
